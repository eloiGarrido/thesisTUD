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
//#include "metric.h"
#include "../../core/dev/metric.h"
#include "../../apps/energytrace/energytrace.h"


/*---------------------------VARIABLES------------------------------------------------*/

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
static uint32_t avg_rendezvous = NS_ENERGY_LOW;
#else
static uint32_t avg_rendezvous = BUDGET;
#endif /*ENERGY_HARV*/

#if ORW_GRADIENT
// Edc expected duty cycle
static uint32_t received_edc,edc_sum,edc_min;
static uint32_t edc[AVG_EDC_SIZE];
static uint8_t edc_idx;
static uint32_t avg_edc;
#endif /*ORW_GRADIENT*/

#if AGEING
static uint32_t edc_age[AVG_EDC_SIZE];
static uint32_t edc_age_counter[AVG_EDC_SIZE];
#endif /*AGEING*/

//EGB support variables
#if NEW_EDC
int index;
//uint8_t is_present = 0;
//uint8_t num_neighbors = 0;
uint32_t edc_id[AVG_EDC_SIZE];
#endif /*NEW_EDC*/

// Data exchange
static uint8_t mySeq,data[DATA_SIZE],seq[DATA_SIZE],ttl[DATA_SIZE];
static uint8_t read_idx,write_idx,q_size;
static uint8_t unique_bitmap[10000/8] = {0};

#if WITH_AGGREGATE
static uint8_t aggregateValue;
#endif /*WITH_AGGREGATE*/

// Staffetta
static uint32_t num_wakeups = 10;
//static uint8_t measuring = 0;
//static uint8_t gradient = 40;
static uint8_t fast_forward = 0;
static int p_retx = 50;
static enum mac_state current_state = disabled;

static struct pt pt;

//uint16_t harvesting_rate;
//node_energy_state_t node_energy_state;
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
		leds_off(LEDS_BLUE);
    }
}

static void powercycle_turn_radio_on(void) {
    if (current_state != disabled) {
		radio_on();
		leds_on(LEDS_BLUE);
		rendezvous_time = 0;
		rendezvous_starting_time = RTIMER_NOW();
    }
}

static void goto_idle() {

    radio_flush_rx();
    radio_flush_tx();
    current_state = idle;
    powercycle_turn_radio_off();
    leds_off(LEDS_RED);
    leds_off(LEDS_GREEN);
    fast_forward = 0;
    STOP_IDLE(); // if we go to idle before the idle timer expire we remove the timer

}

/*--------------------------- DATA FUNCTIONS ------------------------------------------------*/

uint32_t getWakeups(){
    return num_wakeups;
}

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
    // error if queue is full
    if (next_idx == read_idx) return 0;
    // do not add 0 data
    if (_data == 0) return 0;
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

#if AGEING
static inline uint32_t ageing_ratio(uint32_t rv_time) {

	if ( rv_time == 0 ){
		return 10;
	}else if ( rv_time > 0 && rv_time <= 2 ) {
		return 8;
	}else if ( rv_time > 2 && rv_time <= 4 ) {
		return 6;
	}else if ( rv_time > 4 && rv_time <= 6 ) {
		return 4;
	}else {
		return 2;
	}
}

static void age_edc(){
	int age_idx;
	for ( age_idx = 0; age_idx < AVG_EDC_SIZE; age_idx++ ) {
		if ( edc_age_counter[age_idx] == 0 ){
			edc_age_counter[age_idx] = ageing_ratio( edc_age[age_idx] );
			// edc[age_idx]++;
			edc[age_idx] = MIN(edc[age_idx]++, MAX_EDC);
		}else{
			edc_age_counter[age_idx]--;
		} 
	}
}
#endif /*AGEING*/

/*--------------------------- STAFFETTA FUNCTIONS ------------------------------------------------*/

int staffetta_send_packet(void) {
    rtimer_clock_t t0,t1,t2;
    uint8_t strobe[STAFFETTA_PKT_LEN+3];
    uint8_t strobe_ack[STAFFETTA_PKT_LEN+3];
    uint8_t select[STAFFETTA_PKT_LEN+3];
    uint8_t footer[2];
    int i,collisions,strobes,bytes_read;

    int is_present = 0;
    //prepare strobe_ack packet
    strobe_ack[PKT_LEN] = STAFFETTA_PKT_LEN+FOOTER_LEN;
    strobe_ack[PKT_SRC] = node_id;
    strobe_ack[PKT_TYPE] = TYPE_BEACON_ACK;
    // strobe_ack[PKT_GRADIENT] = (uint8_t)100;
   	strobe_ack[PKT_GRADIENT] = MAX_EDC;
    //turn radio on
    radio_on();
    radio_flush_rx();
    radio_flush_tx();
    //start measuring
    rendezvous_time = 0;
    rendezvous_starting_time = RTIMER_NOW();


    //start backoff
    current_state = wait_to_send;
    leds_on(LEDS_GREEN);
    t0 = RTIMER_NOW();
    while (current_state == wait_to_send && RTIMER_CLOCK_LT (RTIMER_NOW(),t0 + BACKOFF_TIME)) {
		if(FIFO_IS_1){
			//leds_off(LEDS_RED);
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
				//while (bytes_read < strobe[PKT_LEN]+1) {
				t1 = RTIMER_NOW ();
				// wait until the FIFO pin is 1 (until one more byte is received)
				while (!FIFO_IS_1) {
					if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t1 + RTIMER_ARCH_SECOND/200)) {
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
				leds_off(LEDS_GREEN);
				radio_flush_rx();
				goto_idle();
				//PRINTF("Wrong CRC\n");
				return RET_WRONG_CRC;
#endif /*WITH_CRC*/
			}
			//PRINTF("rx: %u %u %u %u %u %u %u %u\n",strobe[0],strobe[1],strobe[2],strobe[3],strobe[4],strobe[5],strobe[6],strobe[7]);
			//strobe received, process it

#if WITH_GRADIENT
				//#if BCP_GRADIENT
				//		if(strobe[PKT_GRADIENT] < q_size){
#if ORW_GRADIENT
				if(strobe[PKT_GRADIENT] < avg_edc){
				//#else
				//if(strobe[PKT_GRADIENT] > num_wakeups){
				#endif
					leds_off(LEDS_GREEN);
					radio_flush_rx();
					goto_idle();
					//PRINTF("sender is closer to the sink than me\n");
					return RET_WRONG_GRADIENT;
				}
#endif /*WITH_GRADIENT*/

				if (strobe[PKT_TYPE] == TYPE_BEACON){
					current_state = sending_ack;
				}else{
					leds_off(LEDS_GREEN);
					radio_flush_rx();
					goto_idle();
					//printf("expected beacon, got type %d\n",strobe[PKT_TYPE]);
					return RET_WRONG_TYPE;
				}
			}
	    }
	    //send beacon ack and wait to be selected
	    if(current_state==sending_ack){
			//	leds_off(LEDS_GREEN);
			//	leds_on(LEDS_BLUE);
			strobe_ack[PKT_DST] = strobe[PKT_SRC];
			strobe_ack[PKT_DATA] = strobe[PKT_DATA];
			strobe_ack[PKT_SEQ] = strobe[PKT_SEQ];
			strobe_ack[PKT_TTL] = strobe[PKT_TTL];
            strobe_ack[PKT_SRC] = node_id;
#if ORW_GRADIENT
			strobe_ack[PKT_GRADIENT] = (uint8_t)(MIN(avg_edc,MAX_EDC)); // limit to 255
#endif /*ORW_GRADIENT*/

#if WITH_AGGREGATE
			aggregateValue = MAX(aggregateValue,strobe[PKT_GRADIENT]);
			strobe_ack[PKT_GRADIENT] = aggregateValue;
#endif /*WITH_AGGREGATE*/

			FASTSPI_WRITE_FIFO(strobe_ack, STAFFETTA_PKT_LEN+1);
			FASTSPI_STROBE(CC2420_STXON);
			//We wait until transmission has ended
			BUSYWAIT_UNTIL(!(radio_status() & BV(CC2420_TX_ACTIVE)), RTIMER_SECOND / 10);
			//wait for the select packet
			current_state = wait_select;
			radio_flush_rx();
			t1 = RTIMER_NOW ();
			while (current_state == wait_select && RTIMER_CLOCK_LT (RTIMER_NOW(),t1 + STROBE_WAIT_TIME)) {
				if(FIFO_IS_1){
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

					//PRINTF("len %u\n",strobe[PKT_LEN]);
					while (bytes_read < 10) {
						//while (bytes_read < select[PKT_LEN]+1) {
						t2 = RTIMER_NOW ();
						// wait until the FIFO pin is 1 (until one more byte is received)
						while (!FIFO_IS_1) {
							if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t2 + RTIMER_ARCH_SECOND/200)) {
								radio_flush_rx();
								goto_idle();
								//printf("goto sleep after waiting for SELECT's byte %u from radio\n",bytes_read);
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
						leds_off(LEDS_GREEN);
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
				//printf("select not for us\n");
			}else{
				//otherwise save the packet
				add_data(strobe[PKT_DATA], strobe[PKT_TTL]+1, strobe[PKT_SEQ]);

			}
			//Give time to the radio to finish sending the data
			t2 = RTIMER_NOW (); while(RTIMER_CLOCK_LT (RTIMER_NOW (), t2 + RTIMER_ARCH_SECOND/1000));
            
			leds_off(LEDS_GREEN);
			//Fast-forward
			radio_flush_rx();
			radio_flush_tx();
			//we fast forward only if we successfully received a select message (state = idle)
			if(current_state != select_received){
				goto_idle();
				return RET_WRONG_SELECT;
			}
#if !FAST_FORWARD
			goto_idle();
			printf("8|%u|%u|%u|%u|%u\n",strobe[PKT_SRC],strobe[PKT_DST],strobe[PKT_SEQ],strobe[PKT_DATA],strobe[PKT_GRADIENT]);
			return RET_NO_RX;
#endif /*!FAST_FORWARD*/
	    }
	    leds_off(LEDS_GREEN);
	    leds_on(LEDS_RED);
	    //No message from backoff or backoff with fast-forward. LET'S TRANSMIT!

	    //prepare strobe packet
	    strobe[PKT_LEN] = STAFFETTA_PKT_LEN+FOOTER_LEN;
	    strobe[PKT_SRC] = node_id;
	    strobe[PKT_DST] = 0;
	    strobe[PKT_TYPE] = TYPE_BEACON;
	    strobe[PKT_DATA] = read_data();
	    strobe[PKT_TTL] = read_ttl();
	    strobe[PKT_SEQ] = read_seq();
		//#if BCP_GRADIENT
	    //strobe[PKT_GRADIENT] = (uint8_t)(MIN(q_size,255)); // we limit the queue size to 255
#if ORW_GRADIENT
	    strobe[PKT_GRADIENT] = (uint8_t)(MIN(avg_edc,MAX_EDC)); // limit to 255
#else
	    strobe[PKT_GRADIENT] = (uint8_t)(MIN(num_wakeups,25)); // we limit the # of wakeups to 25
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
	    for (strobes = 0; current_state == wait_beacon_ack && collisions == 0 && RTIMER_CLOCK_LT (RTIMER_NOW (), t0 + STROBE_TIME); strobes++) {
			radio_flush_tx();
			//TODO add energest TX
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
						//printf("goto sleep after waiting for SELECT. Wrong packet length\n");
						return RET_FAIL_RX_BUFF;
					}

					//debug = strobe_ack[PKT_LEN];
					while (bytes_read < 10) {
						//while (bytes_read < strobe_ack[PKT_LEN]+1) {
						t2 = RTIMER_NOW ();
						// wait until the FIFO pin is 1 (until one more byte is received)
						while (!FIFO_IS_1) {
							if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t2 + RTIMER_ARCH_SECOND/200)) {
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
						//t2 = RTIMER_NOW ();while(RTIMER_CLOCK_LT(RTIMER_NOW(),t2+32)); //give time to the radio to send a message (1ms) TODO: add this time to .h file
#endif /*WITH_SELECT*/

						radio_flush_rx();
						goto_idle();
						//PRINTF("Wrong CRC\n");
						return RET_WRONG_CRC;
#endif /*WITH_CRC*/
					}
					//packet received, process it
					if (strobe_ack[PKT_TYPE] == TYPE_BEACON_ACK){
						if ((strobe_ack[PKT_DST] == node_id)&&(strobe_ack[PKT_DATA] == strobe[PKT_DATA] )) {
							current_state = beacon_sent;
							//radio_flush_tx();
							//PRINTF("beacon ack for us from %d\n", strobe_ack[PKT_SRC]);
						}else{
							//printf("beacon ack not for us. For %d, from %d\n", strobe_ack[PKT_DST],strobe_ack[PKT_SRC]);
							collisions++;
						}
					}else{
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
			//t2 = RTIMER_NOW ();while(RTIMER_CLOCK_LT(RTIMER_NOW(),t2+32)); //give time to the radio to send a message (1ms) TODO: add this time to .h file
#endif /*WITH_SELECT*/

#if WITH_AGGREGATE
			aggregateValue = MAX(aggregateValue,strobe_ack[PKT_GRADIENT]);
#endif /*WITH_AGGREGATE*/
			//Message delivered. Remove from our queue
			pop_data();
	    }
	    //turn off the radio
	    goto_idle();
	    leds_off(LEDS_RED);
	    // add the rendezvous measure to our average window
	    if (collisions==0) {
			rendezvous_time = ((RTIMER_NOW() - rendezvous_starting_time) * 10000) / RTIMER_ARCH_SECOND ;
            if(rendezvous_time<RENDEZ_TIME) {
				rendezvous[rendezvous_idx] = rendezvous_time;
				rendezvous_idx = (rendezvous_idx+1)%AVG_SIZE;
			}
			// TODO make a running average
			rendezvous_sum = 0;
			for (i=0;i<AVG_SIZE;i++){
				rendezvous_sum += rendezvous[i];
			}
			avg_rendezvous = rendezvous_sum/AVG_SIZE;

#if ORW_GRADIENT
			// if the neighbor has a better EDC, add it to the average
#if NEW_EDC
            if( (rendezvous_time<RENDEZ_TIME) && (avg_edc > strobe_ack[PKT_GRADIENT]) && (node_id != strobe_ack[PKT_SRC])){
                edc[edc_idx] = strobe_ack[PKT_GRADIENT];
                edc_id[edc_idx] = strobe_ack[PKT_SRC];

#if AGEING //TODO Check that ageing works
		    	edc_age[edc_idx] = rendezvous_time / (RENDEZ_TIME/10);
		    	if ( edc_age[edc_idx] == 0 ){
	    			edc_age_counter[edc_idx] = 10;
		    	}else if ( edc_age[edc_idx] > 0 && edc_age[edc_idx] <= 2 ) {
	    			edc_age_counter[edc_idx] = 8;
		    	}else if ( edc_age[edc_idx] > 2 && edc_age[edc_idx] <= 4 ) {
		    		edc_age_counter[edc_idx] = 6;
		    	}else if ( edc_age[edc_idx] > 4 && edc_age[edc_idx] <= 6 ) {
	    			edc_age_counter[edc_idx] = 4;
		    	}else {
	    			edc_age_counter[edc_idx] = 2;
		    	}
#endif /*AGEING*/

                edc_idx = (edc_idx+1)%AVG_EDC_SIZE;
                edc_sum = 0;
                for (i=0;i<AVG_EDC_SIZE;i++){
                    edc_sum += edc[i];
                }
            }
// #if EDC_WITH_RV
           	// avg_edc = MIN( ( (6 / node_energy_state) + (rendezvous_time/100) + (edc_sum/AVG_EDC_SIZE)), MAX_EDC); //limit to 255
// #else
           	avg_edc = MIN( (6 / node_energy_state) + (edc_sum / AVG_EDC_SIZE ), MAX_EDC);
// #endif /*EDC_WITH_RV*/            
#else
            if((rendezvous_time<RENDEZ_TIME) && (avg_edc > strobe_ack[PKT_GRADIENT])){

				edc[edc_idx] = strobe_ack[PKT_GRADIENT];
				edc_idx = (edc_idx+1)%AVG_EDC_SIZE;
				edc_sum = 0;
				for (i=0;i<AVG_EDC_SIZE;i++){
				    edc_sum += edc[i];
				}
			}
			avg_edc = MIN( ((rendezvous_time/100)+(edc_sum/AVG_EDC_SIZE)),MAX_EDC); //limit to 255
#endif /*NEW_EDC*/
#endif /*ORW_GRADIENT*/

#if DYN_DC
#if ENERGY_HARV
            switch (node_energy_state)
            {
                case NS_LOW:
                    num_wakeups = MAX(1,(NS_ENERGY_LOW*10)/avg_rendezvous);
                    break;
                case NS_MID:
                    num_wakeups = MAX(1,(NS_ENERGY_MID*10)/avg_rendezvous);
                    break;
                case NS_HIGH:
                    num_wakeups = MAX(1,(NS_ENERGY_HIGH*10)/avg_rendezvous);
                    break;
            }
#else	
			num_wakeups = MAX(1,(BUDGET*10)/avg_rendezvous);
#endif /*ENERGY_HARV*/
#else
			num_wakeups = 10;
#endif /*DYN_DC*/

			if (!IS_SINK) {
				// printf("2,%d,%ld\n",strobe_ack[PKT_SRC],num_wakeups);
			}
			
#if AGEING
			age_edc();
#endif /*AGEING*/
	    }

	    radio_flush_rx();
	    goto_idle();
	    // printf("12|%lu\n", rendezvous_time);
	    printf("13|%u\n", node_energy_state);

	    
	    return RET_FAST_FORWARD;
	}

	void sink_busy_wait(void) {
	   // printf("Sink busy loop\n");
	    while (1);
	    // printf("Sink end busy loop\n");
	}

	void sink_listen(void) {
	    rtimer_clock_t t0,t1,t2;
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
				leds_on(LEDS_GREEN);
				t2 = RTIMER_NOW (); while(RTIMER_CLOCK_LT (RTIMER_NOW (), t2 + 3));
				FASTSPI_READ_FIFO_BYTE(strobe[PKT_LEN]); //Read received packet
				bytes_read = 1;

				if (strobe[PKT_LEN]>=(STAFFETTA_PKT_LEN+3)) {
					leds_off(LEDS_GREEN);
					radio_flush_rx();
					current_state=idle;
					//printf("sink got a too long beacon\n");
					continue;
				}

				debug = strobe[PKT_LEN];
				while (bytes_read < 10) {
					//while (bytes_read < strobe[PKT_LEN]+1) {
					t1 = RTIMER_NOW ();
					// wait until the FIFO pin is 1 (until one more byte is received)
					while (!FIFO_IS_1) {
						if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t1 + RTIMER_ARCH_SECOND/200)) {
							leds_off(LEDS_GREEN);
							radio_flush_rx();
							current_state=idle;
							//printf("sink goto sleep after waiting for BEACON's byte %u from radio\n",bytes_read);
							continue;
						}
					};
					// read another byte from the RXFIFO
					FASTSPI_READ_FIFO_BYTE(strobe[bytes_read]);
					bytes_read++;
				}

#if WITH_FLOCKLAB_SINK
				if((!(P2IN & BV(7)))){
					//we are not selected as sink in flocklab
					gpio_off(GPIO_GREEN);
					gpio_on(GPIO_RED);
					radio_flush_rx();
					current_state=idle;
					continue;
				}else{
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

					leds_off(LEDS_GREEN);
					radio_flush_rx();
					current_state=idle;
					//PRINTF("Wrong CRC\n");
					continue;
#endif /*WITH_CRC*/
				}
				//PRINTF("sink beacon: %u %u %u %u %u %u %u %u\n",strobe[0],strobe[1],strobe[2],strobe[3],strobe[4],strobe[5],strobe[6],strobe[7]);
				//strobe received, process it
				if (strobe[PKT_TYPE] == TYPE_BEACON){
					current_state = sending_ack;
				}
				else {
					leds_off(LEDS_GREEN);
					radio_flush_rx();
					current_state=idle;
					continue;
				}
			}
            // we received a beacon
            if(current_state==sending_ack){
                leds_off(LEDS_GREEN);
                leds_on(LEDS_BLUE);
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
                //t2 = RTIMER_NOW (); while(RTIMER_CLOCK_LT (RTIMER_NOW (), t2 + RTIMER_ARCH_SECOND/500)); //give time to the radio to send a message (1ms) TODO: add this time to .h file

                leds_off(LEDS_BLUE);
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
			printf("3|%ld\n",(on_time*1000)/elapsed_time);
			printf("14|%ld\n",avg_edc);
			printf("2|%ld\n",num_wakeups);
#else
			printf("3|%ld|%d\n",(on_time*1000)/elapsed_time,q_size);
#endif /*ORW_GRADIENT*/
	    }
#endif /*STAFFETTA_ENERGEST*/
	    //printf("id: %d\n",node_id);
	}

#if STAFFETTA_ENERGEST
    void staffetta_get_energy_consumption(uint32_t *rxtx_time)
    {
 	    uint32_t on_time,elapsed_time;
	    uint32_t on_time_t;

	    on_time_t = ((energest_type_time(ENERGEST_TYPE_TRANSMIT)+energest_type_time(ENERGEST_TYPE_LISTEN)) * 1000);
	    on_time = on_time_t / RTIMER_ARCH_SECOND;
	    elapsed_time = clock_time() * 1000 / CLOCK_SECOND;
	    *rxtx_time = (on_time*1000) / elapsed_time;
	    if (!(IS_SINK)){
			printf("3|%ld\n",(on_time*1000)/elapsed_time);
			// printf("3|%ld|%ld\n", on_time);

			printf("2|%ld\n",num_wakeups);
		}
    }
#endif /*STAFFETTA_ENERGEST*/
	void staffetta_add_data(uint8_t _seq){

	    if (add_data(node_id,0,_seq) == 1 ){
	        printf("4|%d\n",_seq);
	    }
	}

	void staffetta_init(void) {
	    int i;

#if WITH_FLOCKLAB_SINK
	    gpio_init();
#endif /*WITH_FLOCKLAB_SINK*/

	    current_state = idle;
	    //Clear average buffer
#if ENERGY_HARV
        for (i=0;i<AVG_SIZE;i++) rendezvous[i]=NS_ENERGY_LOW;
        rendezvous_sum = NS_ENERGY_LOW*AVG_SIZE;
#else
	    for (i=0;i<AVG_SIZE;i++) rendezvous[i]=BUDGET;
	    rendezvous_sum = BUDGET*AVG_SIZE;
#endif /*ENERGY_HARV*/

        rendezvous_idx = 0;

#if ORW_GRADIENT
	    for (i=0;i<AVG_EDC_SIZE;i++){
	        edc[i]=MAX_EDC;
        }
#if NEW_EDC
	    for (i=0;i<AVG_EDC_SIZE;i++) edc_id[i]=MAX_EDC;
#endif

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

