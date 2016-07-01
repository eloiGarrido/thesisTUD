/*
 * Copyright (c) 2014, TU Delft.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Marco Cattani <m.cattani@tudelft.nl>
 *
 */

/**
 * \file
 *         Staffetta core, source file.
 * \author
 *         Marco Cattani <m.cattani@tudelft.nl>
 */

#include "staffetta.h"
#include "../../platform/sky/node-id.h"
#include "../../core/dev/gpio.h"
#include "../../core/dev/metric.h"
#include "../../apps/energytrace/energytrace.h"

/*---------------------------VARIABLES------------------------------------------------*/
#define TMOTE_ARCH_SECOND 8192
static int debug;

// Timeout timers
static struct ctimer idle_timeout_ctimer;
static struct ctimer cpowercycle_ctimer;
static struct ctimer backoff_ctimer;
// Rendezvous
static uint32_t rendezvous_time,rendezvous_starting_time,rendezvous_sum;
static uint32_t rendezvous[AVG_SIZE];
static uint8_t rendezvous_idx;

#if ENERGY_HARV
// static uint32_t avg_rendezvous = NS_ENERGY_LOW / SCALE_FACTOR;
static uint32_t avg_rendezvous = 750;
#else
static uint32_t avg_rendezvous = BUDGET;
#endif /*ENERGY_HARV*/

#if ORW_GRADIENT
// Edc expected duty cycle
static uint32_t received_edc,edc_sum,edc_min;
static uint32_t edc[AVG_EDC_SIZE];
static uint32_t edc_id[AVG_EDC_SIZE];
static uint8_t edc_idx;
static uint32_t avg_edc;
#endif /*ORW_GRADIENT*/

#if AGEING
static uint32_t edc_age[AVG_EDC_SIZE];
static uint32_t edc_age_counter[AVG_EDC_SIZE];
#endif /*AGEING*/

// Data exchange
static uint8_t mySeq,data[DATA_SIZE],seq[DATA_SIZE],ttl[DATA_SIZE];
static uint8_t read_idx,write_idx,q_size;
static uint8_t unique_bitmap[10000/8] = {0};

#if WITH_AGGREGATE
static uint8_t aggregateValue;
#endif /*WITH_AGGREGATE*/

// Staffetta
static uint8_t fast_forward = 0;
static int p_retx = 50;
static enum mac_state current_state = disabled;
static struct pt pt;

static uint8_t strobe[STAFFETTA_PKT_LEN+3];
static uint8_t strobe_ack[STAFFETTA_PKT_LEN+3];
static uint8_t select[STAFFETTA_PKT_LEN+3];
static uint8_t strobe_LPL[STAFFETTA_PKT_LEN+3];
//uint16_t harvesting_rate;
//node_energy_state_t node_energy_state;
static uint8_t node_on;
/* --------------------------- RADIO FUNCTIONS ---------------------- */

static inline void radio_flush_tx(void) {
    FASTSPI_STROBE(CC2420_SFLUSHTX);
}

static inline uint8_t radio_status(void) {
    uint8_t status;
    FASTSPI_UPD_STATUS(status);
    return status;
}

static inline void radio_on(void) {
    FASTSPI_STROBE(CC2420_SRXON);
    while(!(radio_status() & (BV(CC2420_XOSC16M_STABLE))));
    ENERGEST_ON(ENERGEST_TYPE_LISTEN);
}

static inline void radio_off(void) {
#if ENERGEST_CONF_ON
    if (energest_current_mode[ENERGEST_TYPE_TRANSMIT]) {
		ENERGEST_OFF(ENERGEST_TYPE_TRANSMIT);
    }
    if (energest_current_mode[ENERGEST_TYPE_LISTEN]) {
		ENERGEST_OFF(ENERGEST_TYPE_LISTEN);
    }
#endif /* ENERGEST_CONF_ON */
    FASTSPI_STROBE(CC2420_SRFOFF);
}

static inline void radio_flush_rx(void) {
    uint8_t dummy;
    FASTSPI_READ_FIFO_BYTE(dummy);
    FASTSPI_STROBE(CC2420_SFLUSHRX);
    FASTSPI_STROBE(CC2420_SFLUSHRX);
}

/*--------------------------- DC FUNCTIONS ------------------------------------------------*/

static void powercycle_turn_radio_off(void) {
    if ((current_state == idle) && !(IS_SINK)){
		radio_off();
    }
}

static void powercycle_turn_radio_on(void) {
    if (current_state != disabled) {
		radio_on();
		rendezvous_time = 0;
		rendezvous_starting_time = RTIMER_NOW();
    }
}

// static void clear_radio() {
//     radio_flush_rx();
//     radio_flush_tx();
//     current_state = idle;
// }

static void stop_radio() {
    radio_flush_rx();
    radio_flush_tx();
    current_state = idle;
    powercycle_turn_radio_off();
    fast_forward = 0;
    STOP_IDLE(); // if we go to idle before the idle timer expire we remove the timer
}
 
static void goto_idle() {

    radio_flush_rx();
    radio_flush_tx();
    current_state = idle;
    // powercycle_turn_radio_off();
    fast_forward = 0;
    STOP_IDLE(); // if we go to idle before the idle timer expire we remove the timer
}


/*--------------------------- DATA FUNCTIONS ------------------------------------------------*/
static void set_bitmap(int idx){
    unique_bitmap[idx / 8] |= 1 << (idx % 8);
}

static void clear_bitmap (int idx){
    unique_bitmap[idx / 8] &= ~(1 << (idx % 8));
}

static uint8_t get_bitmap(int idx){
    return (unique_bitmap[idx / 8] >> (idx % 8)) & 1;
}

static int add_data(uint8_t _data, uint8_t _ttl, uint8_t _seq){
    int next_idx, _unique;
    _unique = (_data-1)*100 + _seq;
    if(get_bitmap(_unique)) return 0; // if the message is already in the queue, do not add it again
    set_bitmap(_unique);
    next_idx = (write_idx+1)%DATA_SIZE;
    if (next_idx == read_idx) return 0; // error if queue is full
    if (_data == 0) return 0; // do not add 0 data
    data[write_idx] = _data;
    ttl[write_idx] = _ttl;
    seq[write_idx] = _seq;
    write_idx = next_idx;
    q_size++;
    return 1;
}

static uint8_t read_data(){
    if (read_idx == write_idx) return 0;
    return data[read_idx];
}

static uint8_t read_seq(){
    if (read_idx == write_idx) return 0;
    return seq[read_idx];
}

static uint8_t read_ttl(){
    if (read_idx == write_idx) return 0;
    return ttl[read_idx];
}

static uint8_t pop_data(){
    uint8_t _data,_seq;
    int _unique;
    if (read_idx == write_idx) return 0; // error if queue is empty
    _data = data[read_idx];
    _seq = seq[read_idx];
    _unique = (_data-1)*100 + _seq;
    clear_bitmap(_unique);
    read_idx = (read_idx+1)%DATA_SIZE;
    q_size--;
    return _data;
}

static uint8_t delete_selected_data(int idx) {
	uint8_t move_finished = 0;
	uint8_t _data,_seq;
    int _unique;
	int idx_t;
	
	if (read_idx == write_idx) return 0; 
	//Clear data
    _data = data[idx];
    _seq = seq[idx];
    _unique = (_data-1)*100 + _seq;
    clear_bitmap(_unique);
    q_size--;
	//Move all data backwards
	move_finished = 0;
    idx_t = idx;
	while (move_finished != 1) {
		if ((idx_t+1)%DATA_SIZE == write_idx){
			if(write_idx == 0) {
				write_idx = DATA_SIZE-1;
			} else {
				write_idx--;
			}
			move_finished = 1;
		} else {
			data[(idx_t)%DATA_SIZE] = data[(idx_t+1)%DATA_SIZE];
			seq[(idx_t)%DATA_SIZE] = seq[(idx_t+1)%DATA_SIZE];
			ttl[(idx_t)%DATA_SIZE] = ttl[(idx_t+1)%DATA_SIZE];
		}
		idx_t++;
	}

}

static int find_data_idx(uint8_t _data, uint8_t _seq) {
	int i;
	int index = -1;
	int found = 0;
	if (read_idx == write_idx) return 0; // error if queue is empty
	i = read_idx;
	found = 0;
	while (found == 0){
		if (data[i%DATA_SIZE] == _data && seq[i%DATA_SIZE] == _seq) {
			index = i%DATA_SIZE;
			found = 1;
		}
		if (i%DATA_SIZE == write_idx){
			found = 1;
		}
		i++;
	}
	return index;
}

static uint8_t find_packet_with_same_source(uint8_t _data, uint8_t _seq) {
    int next_idx, _unique;
    int idx, counter=0;
    for (idx=0; idx<_seq;idx++) {
	    _unique = (_data-1)*100 + (idx);
    	if(get_bitmap(_unique)) {
    		counter++;
    	} 
    }
    if (counter >= PKTS_PER_NODE) {
    	return 0;
    } else {
    	return 1;
    }

}

#if AGEING
static inline uint32_t ageing_ratio(uint32_t rv_time) {
	if ( rv_time == 0 ){
		return 10*4;
	}else if ( rv_time > 0 && rv_time <= 2 ) {
		return 8*4;
	}else if ( rv_time > 2 && rv_time <= 4 ) {
		return 6*4;
	}else if ( rv_time > 4 && rv_time <= 6 ) {
		return 4*4;
	}else {
		return 2*4;
	}
}

static void age_edc(){
	int age_idx;
  int var;
	for ( age_idx = 0; age_idx < AVG_EDC_SIZE; age_idx++ ) {
		if ( edc_age_counter[age_idx] == 0 ) {
			edc_age_counter[age_idx] = ageing_ratio( edc_age[age_idx] );
      		var = edc[age_idx]++;
	     	if ( var < MAX_EDC ) {
	        	edc[age_idx] = var;
        	} else {
	        	edc[age_idx] = MAX_EDC;
	      	}
		} else {
			edc_age_counter[age_idx]--;
		}
	}
}
#endif /*AGEING*/

uint8_t find_worst_edc_entry () {
  uint8_t i;
  uint8_t idx = MAX_EDC;
  uint8_t w_entry = 0;

  for (i = 0; i < AVG_EDC_SIZE; i++) {
    if ( edc[i] > w_entry && edc[i] != 0 ) {
      w_entry = edc[i];
      idx = i;
    }
    if (w_entry == MAX_EDC) { break;}
  }
  return idx;
}

/*--------------------------- STAFFETTA FUNCTIONS ------------------------------------------------*/
int staffetta_transmit(uint32_t operation_duration) {
    rtimer_clock_t t0,t1,t2;
    int i,collisions,strobes,bytes_read;

   	strobe[PKT_LEN] = STAFFETTA_PKT_LEN+FOOTER_LEN;
    strobe[PKT_SRC] = node_id;
    strobe[PKT_DST] = 0;
    strobe[PKT_TYPE] = TYPE_BEACON;
    strobe[PKT_DATA] = read_data();
    strobe[PKT_TTL] = read_ttl();
    strobe[PKT_SEQ] = read_seq();
#if ORW_GRADIENT
    strobe[PKT_GRADIENT] = (uint8_t)(MIN(avg_edc,MAX_EDC)); // limit to 255
#else
    strobe[PKT_GRADIENT] = (uint8_t)25; // we limit the # of wakeups to 25
#endif /*ORW_GRADIENT*/
#if WITH_AGGREGATE
    strobe[PKT_GRADIENT] = aggregateValue;
#endif /*WITH_AGGREGATE*/

	// If the queue is empty, exit
    if(read_data()==0){
		goto_idle();
		return RET_EMPTY_QUEUE;
    }
    current_state = wait_beacon_ack;
    t0 = RTIMER_NOW();
    collisions = 0;
    for (strobes = 0; current_state == wait_beacon_ack && collisions == 0 && RTIMER_CLOCK_LT (RTIMER_NOW (), t0 + operation_duration); strobes++) {
		radio_flush_tx();
		FASTSPI_WRITE_FIFO(strobe, STAFFETTA_PKT_LEN+1);
		FASTSPI_STROBE(CC2420_STXON);
		//We wait until transmission has ended
		BUSYWAIT_UNTIL(!(radio_status() & BV(CC2420_TX_ACTIVE)), RTIMER_SECOND / 10);
		t1 = RTIMER_NOW ();
		while (current_state == wait_beacon_ack && RTIMER_CLOCK_LT (RTIMER_NOW(),t1 + STROBE_WAIT_TIME)) {
			if(FIFO_IS_1){
				t2 = RTIMER_NOW (); while(RTIMER_CLOCK_LT (RTIMER_NOW (), t2 + 3));
				FASTSPI_READ_FIFO_BYTE(strobe_ack[PKT_LEN]);
				bytes_read = 1;
				//check if the size is right
				if (strobe_ack[PKT_LEN]>=(STAFFETTA_PKT_LEN+3)) {
					radio_flush_rx();
					goto_idle();
					return RET_FAIL_RX_BUFF;
				}
				//debug = strobe_ack[PKT_LEN];
				while (bytes_read < 10) {
					//while (bytes_read < strobe_ack[PKT_LEN]+1) {
					t2 = RTIMER_NOW ();
					// wait until the FIFO pin is 1 (until one more byte is received)
					while (!FIFO_IS_1) {
						if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t2 + GUARD_TIME)) {
							radio_flush_rx();
							goto_idle();
							//printf("goto sleep after waiting for BEACON's byte %u from radio\n",bytes_read);
							return RET_FAIL_RX_BUFF;
						}
					};
					// read another byte from the RXFIFO
					FASTSPI_READ_FIFO_BYTE(strobe_ack[bytes_read]);
					bytes_read++;
				}
				//Check CRC
				if (strobe_ack[PKT_CRC] & FOOTER1_CRC_OK) {}
				else {
#if WITH_CRC
					//CRC wrong, send a select to a non-existing node
#if WITH_SELECT
					select[PKT_LEN] = STAFFETTA_PKT_LEN+FOOTER_LEN;
					select[PKT_SRC] = node_id;
					select[PKT_TYPE] = TYPE_SELECT;
					select[PKT_DATA] = 0;
					select[PKT_TTL] = 0;
					select[PKT_SEQ] = 0;
					select[PKT_GRADIENT] = 0;
					select[PKT_DST] = 255;
					radio_flush_tx();
					FASTSPI_WRITE_FIFO(select, STAFFETTA_PKT_LEN+1);
					FASTSPI_STROBE(CC2420_STXON);
					//We wait until transmission has ended
					BUSYWAIT_UNTIL(!(radio_status() & BV(CC2420_TX_ACTIVE)), RTIMER_SECOND / 10);
#endif /*WITH_SELECT*/
					radio_flush_rx();
					goto_idle();
					//Wrong CRC
					return RET_WRONG_CRC;
#endif /*WITH_CRC*/
				}
				//packet received, process it
				if (strobe_ack[PKT_TYPE] == TYPE_BEACON_ACK){
					if ((strobe_ack[PKT_DST] == node_id)&&(strobe_ack[PKT_DATA] == strobe[PKT_DATA] )) {
						current_state = beacon_sent;
						//beacon ack for us from strobe_ack[PKT_SRC] 
					}else{
						//beacon ack not for us. For strobe_ack[PKT_DST], from strobe_ack[PKT_SRC]
						collisions++;
					}
				} else {
				    printf("expected beacon ack, got type %d\n",strobe_ack[PKT_TYPE]);
					collisions++;
				}
			}
		}
    }
    //Message sent. Send a select packet and go to sleep
    if(current_state == beacon_sent && collisions == 0){
#if WITH_SELECT
		select[PKT_LEN] = STAFFETTA_PKT_LEN+FOOTER_LEN;
		select[PKT_SRC] = node_id;
		select[PKT_TYPE] = TYPE_SELECT;
		select[PKT_DATA] = 0;
		select[PKT_TTL] = 0;
		select[PKT_SEQ] = 0;
		select[PKT_GRADIENT] = 0;
		select[PKT_DST] = strobe_ack[PKT_SRC];
		radio_flush_tx();
		FASTSPI_WRITE_FIFO(select, STAFFETTA_PKT_LEN+1);
		FASTSPI_STROBE(CC2420_STXON);
		//We wait until transmission has ended
		BUSYWAIT_UNTIL(!(radio_status() & BV(CC2420_TX_ACTIVE)), RTIMER_SECOND / 10);
#endif /*WITH_SELECT*/

#if WITH_AGGREGATE
		aggregateValue = MAX(aggregateValue,strobe_ack[PKT_GRADIENT]);
#endif /*WITH_AGGREGATE*/
		//Message delivered. Remove from our queue
		pop_data();
    }
    // add the rendezvous measure to our average window
    if (collisions==0) {
		rendezvous_time = ((RTIMER_NOW() - rendezvous_starting_time) * 10000) / RTIMER_ARCH_SECOND ;
        if(rendezvous_time < RENDEZ_TIME) {
			rendezvous[rendezvous_idx] = rendezvous_time;
			rendezvous_idx = (rendezvous_idx+1)%AVG_SIZE;
		}
		rendezvous_sum = 0;
		for (i=0;i<AVG_SIZE;i++){
			rendezvous_sum += rendezvous[i];
		}
		avg_rendezvous = rendezvous_sum/AVG_SIZE;

#if ORW_GRADIENT
			// if the neighbor has a better EDC, add it to the average
#if NEW_EDC
	  	if ((rendezvous_time<RENDEZ_TIME) && (avg_edc > strobe_ack[PKT_GRADIENT]) && (node_id != strobe_ack[PKT_SRC])) {
	    	edc_idx = find_worst_edc_entry();
	     	edc[edc_idx] = strobe_ack[PKT_GRADIENT];
	     	edc_id[edc_idx] = strobe_ack[PKT_SRC];

#if AGEING
	    	edc_age[edc_idx] = rendezvous_time / (RENDEZ_TIME/10);
	    	if ( edc_age[edc_idx] == 0 ) {
				edc_age_counter[edc_idx] = 10;
	    	} else if ( edc_age[edc_idx] > 0 && edc_age[edc_idx] <= 2 ) {
				edc_age_counter[edc_idx] = 8;
	    	} else if ( edc_age[edc_idx] > 2 && edc_age[edc_idx] <= 4 ) {
	    		edc_age_counter[edc_idx] = 6;
	    	} else if ( edc_age[edc_idx] > 4 && edc_age[edc_idx] <= 6 ) {
				edc_age_counter[edc_idx] = 4;
	    	} else {
				edc_age_counter[edc_idx] = 2;
	    	}
#endif /*AGEING*/
			edc_idx = (edc_idx+1)%AVG_EDC_SIZE;
			edc_sum = 0;
			for (i=0;i<AVG_EDC_SIZE;i++) {
		  		edc_sum += edc[i];
			}
	  	}
  		avg_edc = MIN( (6 / node_energy_state) + (edc_sum / AVG_EDC_SIZE ), MAX_EDC);
#else
	 	if ( avg_edc > strobe_ack[PKT_GRADIENT] && rendezvous_time < RENDEZ_TIME ) {
	    	edc_idx = find_worst_edc_entry();
	    	if (edc_idx != MAX_EDC) {
	     		edc[edc_idx] = strobe_ack[PKT_GRADIENT];
	     		edc_id[edc_idx] = strobe_ack[PKT_SRC];
	     	}
			edc_sum = 0;
			for (i=0;i<AVG_EDC_SIZE;i++) {
			    edc_sum += edc[i];
			}
		}
		avg_edc = MIN( ((avg_rendezvous/100) + (edc_sum/AVG_EDC_SIZE)), MAX_EDC); //limit to 255
#endif /*NEW_EDC*/
#endif /*ORW_GRADIENT*/
		if (!IS_SINK) {
			// printf("2,%d,%ld\n",strobe_ack[PKT_SRC],num_wakeups);
		}
#if AGEING
		age_edc();
#endif /*AGEING*/
	}
	radio_flush_rx();
	goto_idle();
	// printf("18|%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",edc[0],edc[1],edc[2],edc[3],edc[4],edc[5],edc[6],edc[7],edc[8],edc[9] );
	// printf("19|%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",edc_id[0],edc_id[1],edc_id[2],edc_id[3],edc_id[4],edc_id[5],edc_id[6],edc_id[7],edc_id[8],edc_id[9] );
	// printf("8|%u|%u|%u|%u|%u|%lu\n",strobe_ack[PKT_SRC],strobe_ack[PKT_DST],strobe_ack[PKT_SEQ],strobe_ack[PKT_DATA],strobe_ack[PKT_GRADIENT], avg_rendezvous);
	return RET_FAST_FORWARD;
}

int staffetta_listen(uint32_t timer_duration) {
    rtimer_clock_t t0,t1,t2,t3;
    int i,collisions,strobes,bytes_read;
    uint32_t rand_backup_time;
    uint8_t channel_idle;
    int idx;
    //prepare strobe_ack packet
    strobe_ack[PKT_LEN] = STAFFETTA_PKT_LEN+FOOTER_LEN;
    strobe_ack[PKT_SRC] = node_id;
    strobe_ack[PKT_TYPE] = TYPE_BEACON_ACK;
   	strobe_ack[PKT_GRADIENT] = MAX_EDC;

    radio_flush_rx();
    radio_flush_tx(); 
    //start measuring
    rendezvous_time = 0;
    rendezvous_starting_time = RTIMER_NOW();
    //start backoff
    current_state = wait_to_send;
    t0 = RTIMER_NOW();
    //TODO Check goto_idle usages
    // Lets Start by listening the channel in case of incoming transmission
    while ( current_state == wait_to_send && RTIMER_CLOCK_LT (RTIMER_NOW(),t0 + timer_duration)) { // Keep listening for the whole timer duration
		if (FIFO_IS_1) {
			t2 = RTIMER_NOW ();
			while(RTIMER_CLOCK_LT (RTIMER_NOW (), t2 + 3));
			FASTSPI_READ_FIFO_BYTE(strobe[PKT_LEN]);
			bytes_read = 1;
			//check if the size is right
			if (strobe[PKT_LEN]>=(STAFFETTA_PKT_LEN+3)) {
				radio_flush_rx();
				goto_idle();
				return RET_FAIL_RX_BUFF;
			}
			while (bytes_read < 10) {
				t1 = RTIMER_NOW ();
				// wait until the FIFO pin is 1 (until one more byte is received)
				while (!FIFO_IS_1) {
					//TODO Check sum of all timers for proper scheduling
					if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t1 + GUARD_TIME)) {
						radio_flush_rx();
						goto_idle();
						return RET_FAIL_RX_BUFF;
					}
				};
				// read another byte from the RXFIFO
				FASTSPI_READ_FIFO_BYTE(strobe[bytes_read]);
				bytes_read++;
			}
			//Check CRC
			if (strobe[PKT_CRC] & FOOTER1_CRC_OK) {}
			else {
#if WITH_CRC
				// packet is corrupted. we send a beacon ack to a non-existing node as a NACK
				strobe_ack[PKT_DST] = 255;
				strobe_ack[PKT_DATA] = 0;
				strobe_ack[PKT_SEQ] = 0;
				strobe_ack[PKT_TTL] = 0;
				FASTSPI_WRITE_FIFO(strobe_ack, STAFFETTA_PKT_LEN+1);
				FASTSPI_STROBE(CC2420_STXON);
				BUSYWAIT_UNTIL(!(radio_status() & BV(CC2420_TX_ACTIVE)), RTIMER_SECOND / 10);
				// and go to sleep
				radio_flush_rx();
				goto_idle();
				//Wrong CRC
				return RET_WRONG_CRC;
#endif /*WITH_CRC*/
			}
			//PRINTF("rx: %u %u %u %u %u %u %u %u\n",strobe[0],strobe[1],strobe[2],strobe[3],strobe[4],strobe[5],strobe[6],strobe[7]);
			//strobe received, process it

#if ORW_GRADIENT
			if(strobe[PKT_GRADIENT] < avg_edc){
				radio_flush_rx();
				goto_idle();
				//sender is closer to the sink than me
				return RET_WRONG_GRADIENT;
			}
#endif /*ORW_GRADIENT*/
			if (strobe[PKT_TYPE] == TYPE_BEACON){
				current_state = sending_ack;
			}else{
				radio_flush_rx();
				goto_idle();
				//expected beacon, got type strobe[PKT_TYPE]
				return RET_WRONG_TYPE;
			}
		}
    }

    //send beacon ack and wait to be selected
    if(current_state==sending_ack){
		strobe_ack[PKT_DST] = strobe[PKT_SRC];
		strobe_ack[PKT_DATA] = strobe[PKT_DATA];
		strobe_ack[PKT_SEQ] = strobe[PKT_SEQ];
		strobe_ack[PKT_TTL] = strobe[PKT_TTL];
        strobe_ack[PKT_SRC] = node_id;
#if ORW_GRADIENT
		strobe_ack[PKT_GRADIENT] = (uint8_t)(MIN(avg_edc,MAX_EDC));
#endif /*ORW_GRADIENT*/
#if WITH_AGGREGATE
		aggregateValue = MAX(aggregateValue,strobe[PKT_GRADIENT]);
		strobe_ack[PKT_GRADIENT] = aggregateValue;
#endif /*WITH_AGGREGATE*/
#if WITH_COLLISION_AVOIDANCE
		// Strobe prepared, init backoff period, let's listen to the channel
	    rand_backup_time = random_rand()%GUARD_TIME;
	    // Lets Start by listening the channel in case of incoming transmission
	    channel_idle = 1;
	    t3 = RTIMER_NOW();
	    while ( channel_idle == 1 && RTIMER_CLOCK_LT (RTIMER_NOW(),t3 + rand_backup_time)) {
			if (FIFO_IS_1) {
				//TODO channel_idle = 0 if we drop the packet because some other node is sending it
				t2 = RTIMER_NOW ();
				while(RTIMER_CLOCK_LT (RTIMER_NOW (), t2 + 3));
				FASTSPI_READ_FIFO_BYTE(strobe_LPL[PKT_LEN]);
				bytes_read = 1;
				//check if the size is right
				if (strobe_LPL[PKT_LEN]>=(STAFFETTA_PKT_LEN+3)) {
					radio_flush_rx();
					goto_idle();
					return RET_FAIL_RX_BUFF;
				}
				while (bytes_read < 10) {
					t1 = RTIMER_NOW ();
					// wait until the FIFO pin is 1 (until one more byte is received)
					while (!FIFO_IS_1) {
						if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t1 + GUARD_TIME)) {
							radio_flush_rx();
							goto_idle();
							return RET_FAIL_RX_BUFF;
						}
					};
					// read another byte from the RXFIFO
					FASTSPI_READ_FIFO_BYTE(strobe_LPL[bytes_read]);
					bytes_read++;
				}
				//strobe received, process it
				if ( strobe_LPL[PKT_DST] == strobe_ack[PKT_DST] && strobe_LPL[PKT_DATA] == strobe_ack[PKT_DATA] && strobe_LPL[PKT_SEQ] == strobe_ack[PKT_SEQ] ) {
					// Is the same packet, drop it and go to sleep
					channel_idle = 0;

				}
			}
	    }
#else
	    channel_idle = 1;
#endif /*WITH_COLLISION_AVOIDANCE*/
	    if (channel_idle) {
			// Channel is idle, we can transmit
			FASTSPI_WRITE_FIFO(strobe_ack, STAFFETTA_PKT_LEN+1);
			FASTSPI_STROBE(CC2420_STXON);
			//We wait until transmission has ended
			BUSYWAIT_UNTIL(!(radio_status() & BV(CC2420_TX_ACTIVE)), RTIMER_SECOND / 10);
			//wait for the select packet
#if WITH_SELECT
			current_state = wait_select;
			radio_flush_rx();
			t1 = RTIMER_NOW ();
			while (current_state == wait_select && RTIMER_CLOCK_LT (RTIMER_NOW(),t1 + STROBE_WAIT_TIME)) {
				if (FIFO_IS_1) {
					t2 = RTIMER_NOW (); while(RTIMER_CLOCK_LT (RTIMER_NOW (), t2 + 3));
					FASTSPI_READ_FIFO_BYTE(select[PKT_LEN]);
					bytes_read = 1;
					//check if the size is right
					if (select[PKT_LEN]>=(STAFFETTA_PKT_LEN+3)) {
						radio_flush_rx();
						goto_idle();
						//printf("goto sleep after waiting for SELECT. Wrong packet length\n");
						return RET_FAIL_RX_BUFF;
					}
					while (bytes_read < 10) {
						t2 = RTIMER_NOW ();
						// wait until the FIFO pin is 1 (until one more byte is received)
						while (!FIFO_IS_1) {
							if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t2 + GUARD_TIME)) {
								radio_flush_rx();
								goto_idle();
								//goto sleep after waiting for SELECT's byte bytes_read from radio
								return RET_FAIL_RX_BUFF;
							}
						};
						// read another byte from the RXFIFO
						FASTSPI_READ_FIFO_BYTE(select[bytes_read]);
						bytes_read++;
					}
					//Check CRC
					if (select[PKT_CRC] & FOOTER1_CRC_OK) {}
					else {
#if WITH_CRC
						radio_flush_rx();
						goto_idle();
						//PRINTF("Wrong CRC\n");
						return RET_WRONG_CRC;
#endif /*WITH_CRC*/
					}
					//change state to idle to signal that a message was received
					current_state = select_received;
				}
			}
			//Save received data
			if((current_state==select_received)&&(select[PKT_DST]!=node_id)){
				//if we received a select and it is not for us, trash the packet.
			}else{
				//otherwise save the packet
				if (strobe[PKT_SEQ]!= 0 && find_packet_with_same_source(strobe[PKT_DATA], strobe[PKT_SEQ]) == 0){
					idx = find_data_idx(strobe[PKT_DATA], strobe[PKT_SEQ]);
					if(idx!=-1){delete_selected_data(idx);}
					// printf("20|%d|%d\n",strobe[PKT_DATA], strobe[PKT_SEQ]-1 );
				}
				add_data(strobe[PKT_DATA], strobe[PKT_TTL]+1, strobe[PKT_SEQ]);
			}
#else 
			if(channel_idle){
				if (strobe[PKT_SEQ]!= 0 && find_packet_with_same_source(strobe[PKT_DATA], strobe[PKT_SEQ]) == 0){
					idx = find_data_idx(strobe[PKT_DATA], strobe[PKT_SEQ]);
					if(idx!=-1)delete_selected_data(idx);
					// printf("20|%d|%d\n",strobe[PKT_DATA], strobe[PKT_SEQ]-1 );
				}
				add_data(strobe[PKT_DATA], strobe[PKT_TTL]+1, strobe[PKT_SEQ]);
			}
#endif /*WITH_SELECT*/
			//Give time to the radio to finish sending the data
			t2 = RTIMER_NOW (); while(RTIMER_CLOCK_LT (RTIMER_NOW (), t2 + RTIMER_ARCH_SECOND/100));
			//Fast-forward
			radio_flush_rx();
			radio_flush_tx();
			//we fast forward only if we successfully received a select message (state = idle)
#if WITH_SELECT
			if(current_state != select_received){
				goto_idle();
				return RET_WRONG_SELECT;
			}	
#endif
		} else {
			radio_flush_rx();
			radio_flush_tx();
		}
#if !FAST_FORWARD
		// goto_idle(); // We don't want to go to sleep after receiving one packet, we want to use all our listening time
		// printf("8|%u|%u|%u|%u|%u\n",strobe[PKT_SRC],strobe[PKT_DST],strobe[PKT_SEQ],strobe[PKT_DATA],strobe[PKT_GRADIENT]);
		return RET_NO_RX;
#endif /*!FAST_FORWARD*/
      	// printf("8|%u|%u|%u|%u|%u|%lu\n",strobe[PKT_SRC],strobe[PKT_DST],strobe[PKT_SEQ],strobe[PKT_DATA],strobe[PKT_GRADIENT], avg_rendezvous);
    }
}


int staffetta_cca() {
    rtimer_clock_t t0,t1,t2,t3;
    int i,collisions,strobes,bytes_read;
    uint32_t rand_backup_time;
    uint8_t channel_idle, wait_channel;
    int idx;
    //prepare strobe_ack packet
    strobe_ack[PKT_LEN] = STAFFETTA_PKT_LEN+FOOTER_LEN;
    strobe_ack[PKT_SRC] = node_id;
    strobe_ack[PKT_TYPE] = TYPE_BEACON_ACK;
   	strobe_ack[PKT_GRADIENT] = MAX_EDC;
    radio_flush_rx();
    radio_flush_tx(); 
    //start measuring
    rendezvous_time = 0;
    wait_channel = 0;
    rendezvous_starting_time = RTIMER_NOW();
    //start backoff
    current_state = wait_to_send;
    t0 = RTIMER_NOW();
    // Lets Start by listening the channel in case of incoming transmission
    while ( current_state == wait_to_send && RTIMER_CLOCK_LT (RTIMER_NOW(),t0 + CCA_TIME)) { // Keep listening for the whole timer duration
		wait_channel = 0;
		if (FIFO_IS_1) {
			t2 = RTIMER_NOW ();
			while(RTIMER_CLOCK_LT (RTIMER_NOW (), t2 + 3));
			FASTSPI_READ_FIFO_BYTE(strobe[PKT_LEN]);
			bytes_read = 1;
			//check if the size is right
			if (strobe[PKT_LEN]>=(STAFFETTA_PKT_LEN+3)) {
				radio_flush_rx();
				goto_idle();
				wait_channel = 1;
				goto WAIT_LOOP;
				// return RET_FAIL_RX_BUFF;
			}
			while (bytes_read < 10) {
				t1 = RTIMER_NOW ();
				// wait until the FIFO pin is 1 (until one more byte is received)
				while (!FIFO_IS_1) {
					//TODO Check sum of all timers for proper scheduling
					if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t1 + GUARD_TIME)) {
						radio_flush_rx();
						goto_idle();
						wait_channel = 1;
						goto WAIT_LOOP;
						// return RET_FAIL_RX_BUFF;
					}
				};
				// read another byte from the RXFIFO
				FASTSPI_READ_FIFO_BYTE(strobe[bytes_read]);
				bytes_read++;
			}
			//Check CRC
			if (strobe[PKT_CRC] & FOOTER1_CRC_OK) {}
			else {
#if WITH_CRC
				// packet is corrupted. we send a beacon ack to a non-existing node as a NACK
				strobe_ack[PKT_DST] = 255;
				strobe_ack[PKT_DATA] = 0;
				strobe_ack[PKT_SEQ] = 0;
				strobe_ack[PKT_TTL] = 0;
				FASTSPI_WRITE_FIFO(strobe_ack, STAFFETTA_PKT_LEN+1);
				FASTSPI_STROBE(CC2420_STXON);
				BUSYWAIT_UNTIL(!(radio_status() & BV(CC2420_TX_ACTIVE)), RTIMER_SECOND / 10);
				// and go to sleep
				radio_flush_rx();
				goto_idle();
				wait_channel = 1;
				goto WAIT_LOOP;
				//Wrong CRC
				// return RET_WRONG_CRC;
#endif /*WITH_CRC*/
			}
			//PRINTF("rx: %u %u %u %u %u %u %u %u\n",strobe[0],strobe[1],strobe[2],strobe[3],strobe[4],strobe[5],strobe[6],strobe[7]);
			//strobe received, process it

#if ORW_GRADIENT
			if(strobe[PKT_GRADIENT] < avg_edc){
				radio_flush_rx();
				goto_idle();
				wait_channel = 1;
				goto WAIT_LOOP;
				//sender is closer to the sink than me
				// return RET_WRONG_GRADIENT;
			}
#endif /*ORW_GRADIENT*/
			if (strobe[PKT_TYPE] == TYPE_BEACON){
				current_state = sending_ack;
			}else{
				radio_flush_rx();
				goto_idle();
				wait_channel = 1;
				goto WAIT_LOOP;
				//expected beacon, got type strobe[PKT_TYPE]
				// return RET_WRONG_TYPE;
			}
		}
		// Busy wait while channel is used or GUARD duration
		
		WAIT_LOOP:if(wait_channel){
			t3 = RTIMER_NOW();
			while (  FIFO_IS_1 && RTIMER_CLOCK_LT (RTIMER_NOW(),t3 + WAIT_TIME) ) {} 
			radio_flush_rx();
			wait_channel = 0;
		}
    }

    //send beacon ack and wait to be selected
    if(current_state==sending_ack){
		strobe_ack[PKT_DST] = strobe[PKT_SRC];
		strobe_ack[PKT_DATA] = strobe[PKT_DATA];
		strobe_ack[PKT_SEQ] = strobe[PKT_SEQ];
		strobe_ack[PKT_TTL] = strobe[PKT_TTL];
        strobe_ack[PKT_SRC] = node_id;
#if ORW_GRADIENT
		strobe_ack[PKT_GRADIENT] = (uint8_t)(MIN(avg_edc,MAX_EDC));
#endif /*ORW_GRADIENT*/
#if WITH_AGGREGATE
		aggregateValue = MAX(aggregateValue,strobe[PKT_GRADIENT]);
		strobe_ack[PKT_GRADIENT] = aggregateValue;
#endif /*WITH_AGGREGATE*/
#if WITH_COLLISION_AVOIDANCE
		// Strobe prepared, init backoff period, let's listen to the channel
		strobe_LPL[PKT_DST] = 255;
		strobe_LPL[PKT_DATA] = 255;
		strobe_LPL[PKT_SEQ] = 255;
		strobe_LPL[PKT_SRC] = 255;

	    rand_backup_time = random_rand()%GUARD_TIME;
	    // Lets Start by listening the channel in case of incoming transmission
	    channel_idle = 1;
	    t3 = RTIMER_NOW();
	    while (channel_idle == 1 && RTIMER_CLOCK_LT (RTIMER_NOW(),t3 + rand_backup_time)) {
			if (FIFO_IS_1) {
				t2 = RTIMER_NOW ();
				while(RTIMER_CLOCK_LT (RTIMER_NOW (), t2 + 3));
				FASTSPI_READ_FIFO_BYTE(strobe_LPL[PKT_LEN]);
				bytes_read = 1;
				//check if the size is right
				if (strobe_LPL[PKT_LEN]>=(STAFFETTA_PKT_LEN+3)) {
					radio_flush_rx();
					goto_idle();
					return RET_FAIL_RX_BUFF;
				}
				while (bytes_read < 10) {
					t1 = RTIMER_NOW ();
					// wait until the FIFO pin is 1 (until one more byte is received)
					while (!FIFO_IS_1) {
						if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t1 + GUARD_TIME)) {
							radio_flush_rx();
							goto_idle();
							return RET_FAIL_RX_BUFF;
						}
					};
					// read another byte from the RXFIFO
					FASTSPI_READ_FIFO_BYTE(strobe_LPL[bytes_read]);
					bytes_read++;
				}
				//strobe received, process it
				if ( strobe_LPL[PKT_DST] == strobe_ack[PKT_DST] && strobe_LPL[PKT_DATA] == strobe_ack[PKT_DATA] && strobe_LPL[PKT_SEQ] == strobe_ack[PKT_SEQ] ) {
					// Is the same packet, drop it and go to sleep
					channel_idle = 0;

				}
			}
	    }
#else
	    channel_idle = 1;
#endif /*WITH_COLLISION_AVOIDANCE*/
	    if (channel_idle) {
			// Channel is idle, we can transmit
			FASTSPI_WRITE_FIFO(strobe_ack, STAFFETTA_PKT_LEN+1);
			FASTSPI_STROBE(CC2420_STXON);
			//We wait until transmission has ended
			BUSYWAIT_UNTIL(!(radio_status() & BV(CC2420_TX_ACTIVE)), RTIMER_SECOND / 10);
			//wait for the select packet
#if WITH_SELECT
			current_state = wait_select;
			radio_flush_rx();
			t1 = RTIMER_NOW ();
			while (current_state == wait_select && RTIMER_CLOCK_LT (RTIMER_NOW(),t1 + STROBE_WAIT_TIME)) {
				if (FIFO_IS_1) {
					t2 = RTIMER_NOW (); while(RTIMER_CLOCK_LT (RTIMER_NOW (), t2 + 3));
					FASTSPI_READ_FIFO_BYTE(select[PKT_LEN]);
					bytes_read = 1;
					//check if the size is right
					if (select[PKT_LEN]>=(STAFFETTA_PKT_LEN+3)) {
						radio_flush_rx();
						goto_idle();
						//printf("goto sleep after waiting for SELECT. Wrong packet length\n");
						return RET_FAIL_RX_BUFF;
					}
					while (bytes_read < 10) {
						t2 = RTIMER_NOW ();
						// wait until the FIFO pin is 1 (until one more byte is received)
						while (!FIFO_IS_1) {
							if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t2 + GUARD_TIME)) {
								radio_flush_rx();
								goto_idle();
								//goto sleep after waiting for SELECT's byte bytes_read from radio
								return RET_FAIL_RX_BUFF;
							}
						};
						// read another byte from the RXFIFO
						FASTSPI_READ_FIFO_BYTE(select[bytes_read]);
						bytes_read++;
					}
					//Check CRC
					if (select[PKT_CRC] & FOOTER1_CRC_OK) {}
					else {
#if WITH_CRC
						radio_flush_rx();
						goto_idle();
						//PRINTF("Wrong CRC\n");
						return RET_WRONG_CRC;
#endif /*WITH_CRC*/
					}
					//change state to idle to signal that a message was received
					current_state = select_received;
				}
			}
			//Save received data
			if((current_state==select_received)&&(select[PKT_DST]!=node_id)){
				//if we received a select and it is not for us, trash the packet.
			}else{
				//otherwise save the packet
				if (strobe[PKT_SEQ]!= 0 && find_packet_with_same_source(strobe[PKT_DATA], strobe[PKT_SEQ]) == 0){
					idx = find_data_idx(strobe[PKT_DATA], strobe[PKT_SEQ]);
					if(idx!=-1)delete_selected_data(idx);
					// printf("20|%d|%d\n",strobe[PKT_DATA], strobe[PKT_SEQ]-1 );
				}
				add_data(strobe[PKT_DATA], strobe[PKT_TTL]+1, strobe[PKT_SEQ]);
			}
#else 
			if(channel_idle){
				if (strobe[PKT_SEQ]!= 0 && find_packet_with_same_source(strobe[PKT_DATA], strobe[PKT_SEQ]) == 0){
					idx = find_data_idx(strobe[PKT_DATA], strobe[PKT_SEQ]);
					if(idx!=-1)delete_selected_data(idx);
					// printf("20|%d|%d\n",strobe[PKT_DATA], strobe[PKT_SEQ]-1 );
				}
				add_data(strobe[PKT_DATA], strobe[PKT_TTL]+1, strobe[PKT_SEQ]);
			}
#endif /*WITH_SELECT*/
			//Give time to the radio to finish sending the data
			t2 = RTIMER_NOW (); while(RTIMER_CLOCK_LT (RTIMER_NOW (), t2 + RTIMER_ARCH_SECOND/100));
			//Fast-forward
			radio_flush_rx();
			radio_flush_tx();
			//we fast forward only if we successfully received a select message (state = idle)
#if WITH_SELECT
			if(current_state != select_received){
				goto_idle();
				return RET_WRONG_SELECT;
			}	
#endif
		} else {
			radio_flush_rx();
			radio_flush_tx();
		}
#if !FAST_FORWARD
		// goto_idle(); // We don't want to go to sleep after receiving one packet, we want to use all our listening time
		// printf("8|%u|%u|%u|%u|%u\n",strobe[PKT_SRC],strobe[PKT_DST],strobe[PKT_SEQ],strobe[PKT_DATA],strobe[PKT_GRADIENT]);
		return RET_NO_RX;
#endif /*!FAST_FORWARD*/
      	// printf("8|%u|%u|%u|%u|%u|%lu\n",strobe[PKT_SRC],strobe[PKT_DST],strobe[PKT_SEQ],strobe[PKT_DATA],strobe[PKT_GRADIENT], avg_rendezvous);
    }
}


static uint32_t get_operation_duration(uint32_t * t_ext) {
	uint32_t operation_time = 0;
	uint32_t timer_extension = 0;

	timer_extension = get_op_extension(); 
	operation_time = MAX(OP_DURATION_MIN, timer_extension * (RTIMER_ARCH_SECOND / 1000));
#if DYN_DC
	t_ext = MIN(10 , timer_extension);
	return operation_time;
#else
	t_ext = 10;
	return OP_DURATION_MIN;
#endif
} 

int staffetta_main(uint32_t * t_op) {
	int return_value = RET_NO_RX;
	rtimer_clock_t t_operation;
	uint32_t operation_duration;
	uint8_t cca = 0;

    //turn radio on
    // Get operation duration depending on NODE_ENERGY_STATE

    operation_duration = get_operation_duration(&t_op);

    // Wake up, start the timer and CCA
    radio_on();
    t_operation = RTIMER_NOW();
    while (operation_duration != -1 && RTIMER_CLOCK_LT (RTIMER_NOW(),t_operation + operation_duration)) {
    	if (cca == 0) {
    		return_value = staffetta_cca();
    		cca = 1;
    	} else {
    		if (read_data() == 0) {
    			return_value = staffetta_listen(operation_duration); // Queue is empty, we can start listening
    		} else {
    			return_value = staffetta_transmit(operation_duration); // We have packets in the queue, LET'S TRANSMIT
    		}
			cca = 0;	
    	}
    }
	stop_radio();
	return return_value;    
} /*END_WHILE*/


/* SINK FUNCTIONS */
void sink_busy_wait(void) {
   // printf("Sink busy loop\n");
    while (1);
    // printf("Sink end busy loop\n");
}

void sink_listen(void) {
    rtimer_clock_t t1,t2;
    uint8_t strobe[STAFFETTA_PKT_LEN+3];
    uint8_t strobe_ack[STAFFETTA_PKT_LEN+3];
    uint8_t select[STAFFETTA_PKT_LEN+3];
    int i,collisions,strobes,bytes_read;
    //prepare strobe_ack packet
    strobe_ack[PKT_LEN] = STAFFETTA_PKT_LEN+FOOTER_LEN;
    strobe_ack[PKT_SRC] = node_id;
    strobe_ack[PKT_TYPE] = TYPE_BEACON_ACK;
    strobe_ack[PKT_GRADIENT] = 0; // we limit the # of wakeups to 25
    //turn radio on
    radio_on();
    radio_flush_rx();
    radio_flush_tx();
    watchdog_stop();
    current_state = idle;
    while (1) {
		//there is a message in the buffer
		if(FIFO_IS_1){
			t2 = RTIMER_NOW (); while(RTIMER_CLOCK_LT (RTIMER_NOW (), t2 + 3));
			FASTSPI_READ_FIFO_BYTE(strobe[PKT_LEN]); //Read received packet
			bytes_read = 1;

			if (strobe[PKT_LEN]>=(STAFFETTA_PKT_LEN+3)) {
				radio_flush_rx();
				current_state=idle;
				//sink got a too long beacon
				continue;
			}

			debug = strobe[PKT_LEN];
			while (bytes_read < 10) {
				t1 = RTIMER_NOW ();
				// wait until the FIFO pin is 1 (until one more byte is received)
				while (!FIFO_IS_1) {
					if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t1 + RTIMER_SECOND / 200)) {
						radio_flush_rx();
						current_state=idle;
						//sink goto sleep after waiting for BEACON's byte [bytes_read] from radio
						continue;
					}
				};
				// read another byte from the RXFIFO
				FASTSPI_READ_FIFO_BYTE(strobe[bytes_read]);
				bytes_read++;
			}

#if WITH_FLOCKLAB_SINK
			if ((!(P2IN & BV(7)))) {
				//we are not selected as sink in flocklab
				gpio_off(GPIO_GREEN);
				gpio_on(GPIO_RED);
				radio_flush_rx();
				current_state=idle;
				continue;
			} else {
				gpio_on(GPIO_GREEN);
				gpio_off(GPIO_RED);
			}
#endif /*WITH_FLOCKLAB_SINK*/
			//Check CRC
			if (strobe[PKT_CRC] & FOOTER1_CRC_OK) {}
			else {
#if WITH_CRC
				//CRC wrong, send an ack to a non-existing node (NACK)
				strobe_ack[PKT_DST] = 255;
				strobe_ack[PKT_DATA] = 0;
				strobe_ack[PKT_SEQ] = 0;
				strobe_ack[PKT_TTL] = 0;

				FASTSPI_WRITE_FIFO(strobe_ack, STAFFETTA_PKT_LEN+1);
				FASTSPI_STROBE(CC2420_STXON);
				BUSYWAIT_UNTIL(!(radio_status() & BV(CC2420_TX_ACTIVE)), RTIMER_SECOND / 10);
				radio_flush_rx();
				current_state=idle;
				//Wrong CRC
				continue;
#endif /*WITH_CRC*/
			}
			//PRINTF("sink beacon: %u %u %u %u %u %u %u %u\n",strobe[0],strobe[1],strobe[2],strobe[3],strobe[4],strobe[5],strobe[6],strobe[7]);
			//strobe received, process it
			if (strobe[PKT_TYPE] == TYPE_BEACON) {
				current_state = sending_ack;
			} else {
				radio_flush_rx();
				current_state=idle;
				continue;
			}
		}
        // we received a beacon
        if (current_state==sending_ack) {
            strobe_ack[PKT_DST] = strobe[PKT_SRC];
            strobe_ack[PKT_DATA] = strobe[PKT_DATA];
            strobe_ack[PKT_SEQ] = strobe[PKT_SEQ];
            strobe_ack[PKT_TTL] = strobe[PKT_TTL];
            strobe_ack[PKT_SRC] = 1;
#if ORW_GRADIENT
            strobe_ack[PKT_GRADIENT] = 0;
#endif /*ORW_GRADIENT*/

#if WITH_AGGREGATE
            aggregateValue = MAX(aggregateValue,strobe[PKT_GRADIENT]);
            strobe_ack[PKT_GRADIENT] = aggregateValue;
#endif /*WITH_AGGREGATE*/
            FASTSPI_WRITE_FIFO(strobe_ack, STAFFETTA_PKT_LEN+1);
            FASTSPI_STROBE(CC2420_STXON);
            //We wait until transmission has ended
            BUSYWAIT_UNTIL(!(radio_status() & BV(CC2420_TX_ACTIVE)), RTIMER_SECOND / 10);
            radio_flush_rx();
            current_state=idle;
            //SINK output
            printf("7|%u|%u|%u|%u|%u\n", strobe[PKT_SRC], strobe[PKT_DST], strobe[PKT_SEQ], strobe[PKT_DATA],strobe[PKT_GRADIENT]);

#if WITH_AGGREGATE
            //printf("A %u\n",aggregateValue);
#endif /*WITH_AGGREGATE*/
        }
	}
}

void staffetta_print_stats(void){
#if STAFFETTA_ENERGEST
	printf("14|%ld\n",avg_edc);
#else
    uint32_t on_time,elapsed_time;
    on_time = ((energest_type_time(ENERGEST_TYPE_TRANSMIT)+energest_type_time(ENERGEST_TYPE_LISTEN)) * 1000) / RTIMER_ARCH_SECOND;
    elapsed_time = clock_time() * 1000 / CLOCK_SECOND;
    if (!(IS_SINK)){
#if ORW_GRADIENT
  		printf("15|%ld|%d|%ld\n",(on_time*1000)/elapsed_time, q_size, avg_edc );
#else
		printf("3|%ld|%d\n",(on_time*1000)/elapsed_time,q_size);
#endif /*ORW_GRADIENT*/
    }
#endif /*STAFFETTA_ENERGEST*/
}

#if STAFFETTA_ENERGEST
void staffetta_get_energy_consumption(uint32_t *rxtx_time) {
#if ELAPSED_TIME
    uint32_t on_time,elapsed_time;
    on_time = ( (energest_type_time(ENERGEST_TYPE_TRANSMIT) + energest_type_time(ENERGEST_TYPE_LISTEN) ) * 1000) / RTIMER_ARCH_SECOND;
    elapsed_time = clock_time() * 1000 / CLOCK_SECOND;
    *rxtx_time = (on_time*1000) / elapsed_time;
    if (!(IS_SINK)){
  		printf("16|%ld|%d\n",(on_time*1000)/elapsed_time, q_size);
	}

#else
    static uint32_t last_rxtx;
    uint32_t all_rxtx, rxtx_time_t;
    all_rxtx = energest_type_time(ENERGEST_TYPE_TRANSMIT) + energest_type_time(ENERGEST_TYPE_LISTEN);
    rxtx_time_t = all_rxtx - last_rxtx;
    *rxtx_time = 1000 * ( (rxtx_time_t * 1000) / TMOTE_ARCH_SECOND);
    if (!(IS_SINK)){
      	printf("16|%ld\n",*rxtx_time);
    }
    last_rxtx = energest_type_time(ENERGEST_TYPE_LISTEN) + energest_type_time(ENERGEST_TYPE_TRANSMIT);

#endif /*ELAPSED_TIME*/

}
#endif /*STAFFETTA_ENERGEST*/

void staffetta_add_data(uint8_t _seq){
	int idx;
	if (_seq != 0 && find_packet_with_same_source(node_id, _seq) == 0){
		idx = find_data_idx(node_id, _seq);
		delete_selected_data(idx);
		// printf("20|%d|%d\n",node_id, _seq-1 );
	}
    if (add_data(node_id,0,_seq) == 1 ){
    	printf("4|%d\n",_seq);
	}
}

void staffetta_init(void) {
    int i;
	random_init( (unsigned short) (clock_time()) );
#if WITH_FLOCKLAB_SINK
    gpio_init();
#endif /*WITH_FLOCKLAB_SINK*/

    current_state = idle;
    //Clear average buffer
#if ENERGY_HARV
// for (i=0;i<AVG_SIZE;i++) rendezvous[i]= (NS_ENERGY_LOW / SCALE_FACTOR);
for (i=0;i<AVG_SIZE;i++) rendezvous[i] = 750;
// rendezvous_sum = (NS_ENERGY_LOW / SCALE_FACTOR) * AVG_SIZE;
    rendezvous_sum = 750 * AVG_SIZE;
#else
    for (i=0;i<AVG_SIZE;i++) rendezvous[i]=BUDGET;
    rendezvous_sum = BUDGET*AVG_SIZE;
#endif /*ENERGY_HARV*/

    rendezvous_idx = 0;

#if ORW_GRADIENT
    for (i=0;i<AVG_EDC_SIZE;i++){
        edc[i]=MAX_EDC;
    }
// #if NEW_EDC
    for (i=0;i<AVG_EDC_SIZE;i++) edc_id[i]=MAX_EDC;
// #endif

    edc_min = MAX_EDC;
    edc_idx = 0;
    avg_edc = MAX_EDC;
    edc_sum = MAX_EDC*AVG_EDC_SIZE;
#endif /*ORW_GRADIENT*/

    //Init message vars
    for (i=0;i<DATA_SIZE;i++) {
        data[i]=0;
        seq[i]=0;
        ttl[i]=0;
    }
    read_idx = 0;
    write_idx = 0;
    q_size = 0;
    //Add some messages to the queue
    for(i=0;i<PAKETS_PER_NODE;i++) staffetta_add_data(i);//add_data(node_id,0,i);

#if WITH_AGGREGATE
    aggregateValue = node_id;
#endif /*WITH_AGGREGATE*/
    //If the node is a sink, start listening indefinitely
    if (IS_SINK){
		sink_listen();
	}
}
