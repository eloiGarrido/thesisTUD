/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
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
 * This file is part of the Contiki operating system.
 *
 * $Id: staffettamac.h,v 1.5 2010/02/23 20:09:11 nifi Exp $
 */

/**
 * \file
 *         A simple power saving MAC protocol based on X-MAC [SenSys 2006]
 * \author
 *         Adam Dunkels <adam@sics.se>
 */



//FILEPATH = "/home/jester/cfs_file.txt"
//
//sim = mote.getSimulation();
//log.log(sim.getMotesCount());
//for (i=1; i < sim.getMotesCount(); i++) {
//   m = sim.getMote(i);
//   fs = m.getFilesystem();
//   success = fs.insertFile(FILEPATH);
//}
//
//TIMEOUT(600000);
//
//while (true) {
//  log.log(id + "|" + time + "|" + msg + "|\n");
//  YIELD();
//}



#ifndef __STAFFETTA_H__
#define __STAFFETTA_H__

#include "contiki.h"
#include "dev/watchdog.h"
#include "dev/cc2420_const.h"
#include "dev/leds.h"
#include "dev/spi.h"
#include "sys/ctimer.h"
#include "lib/random.h"
#include <stdio.h>
#include <legacymsp430.h>
#include <stdlib.h>


/*------------------------- OPTIONS --------------------------------------------------*/

//TODO implement all the options

// #define PAKETS_PER_NODE 	6
#define PAKETS_PER_NODE   3
#define WITH_CRC 		      1
#define IS_SINK 		      (node_id == 1)
//#define IS_SINK 		    (node_id < 4) // Mobile sink on flocklab
#define WITH_SELECT 		  0 // enable 3-way handshake (in case of multiple forwarders, initiator can choose)

#define WITH_GRADIENT 		1 // ensure that messages follows a gradient to the sink (number of wakeups)
#define BCP_GRADIENT		  0 // use the queue size as gradient (BCP)
#define ORW_GRADIENT		  1 // use the expected duty cycle as gradient (ORW)


#define FAST_FORWARD 		  1  // forward as soon as you can (not dummy messages)
#define BUDGET_PRECISION 	1 //use fixed point precision to compute the number of wakeups
#define BUDGET 			      750 // how much energy should we use (in ms * 10)
#define AVG_SIZE 		      5 // windows size for averaging the rendezvous time
#define AVG_EDC_SIZE		  10 //20 averaging size for orw's edc
#define WITH_RETX 		    0 // retransmit a beacon ack if we receive another beacon
#define USE_BACKOFF 		  1 // before sending listen to the channel for a certain period
#define SLEEP_BACKOFF 		0 // after the backoff, if we receive a beacon instead on starting a communication we go to sleep
#define RSSI_FILTER 		  0 // filter strobes with RSSI lower that a threshold
#define RSSI_THRESHOLD 		-90
#define WITH_SINK_DELAY 	1 // add a delay to the beacon ack of nodes that are not a sink (sink is always the first to answer to beacons)
// #define DATA_SIZE 		    100 // size of the packet queue
#define DATA_SIZE         50 // size of the packet queue

#define WITH_AGGREGATE		0
#define RENDEZ_TIME       10000

#define ENERGY_HARV       1 //Enable dynamic budget depending on Energy Harvesting capabilities
#define NEW_EDC           1
#define AGEING            0 //EDC vector ages depending on rendezvous value
// #define EDC_WITH_RV         0
#define STAFFETTA_ENERGEST  1
#define ELAPSED_TIME        1

#define ADAPTIVE_PACKET_CREATION  0
#define RANDOM_PACKET_CREATION    0
// #define SCALE_FACTOR 100
#define DYN_DC 		0 // Staffetta adaptative wakeups
#if NEW_EDC
#define MAX_EDC   100
#else
#define MAX_EDC   255
#endif /*NEW_EDC*/

#define PKTS_PER_NODE  3
#define WAKE_UP_PERIOD  10
#define WITH_COLLISION_AVOIDANCE 1
/*-------------------------- MACROS -------------------------------------------------*/
#define MIN(a, b) ((a) < (b)? (a) : (b))
#define MAX(a, b) ((a) > (b)? (a) : (b))
#define CSCHEDULE_POWERCYCLE(rtime) cschedule_powercycle((1ul * CLOCK_SECOND * (rtime)) / RTIMER_ARCH_SECOND)
#define GOTO_IDLE(rtime) ctimer_set(&idle_timeout_ctimer,(1ul * CLOCK_SECOND * (rtime)) / RTIMER_ARCH_SECOND,(void (*)(void *))goto_idle, NULL)
#define STOP_IDLE() ctimer_stop(&idle_timeout_ctimer)
#define SEND_BACKOFF(rtime) ctimer_set(&backoff_ctimer,(1ul * CLOCK_SECOND * (rtime)) / RTIMER_ARCH_SECOND,(void (*)(void *))send_packet, NULL)
#define STOP_BACKOFF() ctimer_stop(&backoff_ctimer)
#define PRINTF(...)
#define BUSYWAIT_UNTIL(cond, max_time)                                  \
  do {                                                                  \
    rtimer_clock_t t0;                                                  \
    t0 = RTIMER_NOW();                                                  \
    while(!(cond) && RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + (max_time)));   \
  } while(0)
/*------------------------- STATE --------------------------------------------------*/
enum mac_state{
disabled = 0,
enabled = 1,
idle = 1,
wait_to_send = 2,
wait_beacon_ack = 3,
sending_ack = 4,
beacon_sent = 5,
wait_select = 6,
select_received = 7
} ;

/*------------------------- PACKETS --------------------------------------------------*/
struct staffetta_hdr {
  uint8_t type;
  uint8_t dst;
  uint8_t data;
  uint8_t hop;
  uint8_t seq;
  uint8_t gradient;
  uint16_t wakeups;
};

#define RET_FAST_FORWARD    1
#define RET_NO_RX		        2
#define RET_EMPTY_QUEUE		  3
#define RET_ERRORS		      4
#define RET_WRONG_SELECT	  5
#define RET_FAIL_RX_BUFF	  6
#define RET_WRONG_TYPE		  7
#define RET_WRONG_CRC		    8
#define RET_WRONG_GRADIENT  9
#define RET_ALL_GOOD        10

#define TYPE_BEACON       	1
#define TYPE_BEACON_ACK   	2
#define TYPE_SELECT       	3

#define STAFFETTA_PKT_LEN 	7
#define FOOTER_LEN		2

#define PKT_LEN			0
#define PKT_TYPE		1
#define PKT_SRC			2
#define PKT_DST			3
#define PKT_SEQ			4
#define PKT_TTL			5
#define PKT_DATA		6
#define PKT_GRADIENT	7
#define PKT_RSSI		8
#define PKT_CRC			9 //last field + 2

#define FOOTER1_CRC_OK                0x80
#define FOOTER1_CORRELATION           0x7f

#define STAFFETTA_LEN_FIELD            packet[0]
#define STAFFETTA_HEADER_FIELD         packet[1]
#define STAFFETTA_DATA_FIELD           packet[2]
#define STAFFETTA_RELAY_CNT_FIELD      packet[packet_len_tmp - FOOTER_LEN]
#define STAFFETTA_RSSI_FIELD           packet[packet_len_tmp - 1]
#define STAFFETTA_CRC_FIELD            packet[packet_len_tmp]

/*------------------------- TIMINGS --------------------------------------------------*/


#define PERIOD 			      RTIMER_ARCH_SECOND 		// 1s
// #define STROBE_TIME 		  PERIOD				// 1s
#define STROBE_TIME       (RTIMER_ARCH_SECOND / 350) // 2.85ms
#define STROBE_WAIT_TIME	(RTIMER_ARCH_SECOND/700) 	// 2ms
#define ON_TIME 		      (RTIMER_ARCH_SECOND/300) 	// 3ms
#define OFF_TIME 		      (PERIOD-ON_TIME)		// 995ms
#define BACKOFF_TIME 		  (ON_TIME)			// 5ms


#define GUARD_TIME      (RTIMER_ARCH_SECOND / 800) // 1.25ms
#define CCA_TIME        (STROBE_WAIT_TIME * 3/2) // 2.15ms
#define WAIT_TIME       (GUARD_TIME * 5/4)
#define LISTEN_TIME     (RTIMER_ARCH_SECOND / 200) // 5ms
#define NO_TX_AFTER_RX  0
#define TX_AFTER_RX     1

#define OP_DURATION_LOW   (RTIMER_ARCH_SECOND / 200) // 5ms
#define OP_DURATION_MID   (RTIMER_ARCH_SECOND / 153) // 6.5ms
#define OP_DURATION_HIGH  (RTIMER_ARCH_SECOND / 125) // 8ms

//#define MAX_STROBE_SIZE 50
//#define DEFAULT_STROBE_WAIT_TIME DEFAULT_ON_TIME
//#define DEFAULT_STROBE_TIME DEFAULT_ON_TIME + DEFAULT_OFF_TIME
//#define SELECT_WAITING_TIME (DEFAULT_ON_TIME * 5)
//
//#if DEFAULT_PERIOD == 0
//#undef DEFAULT_PERIOD
//#define DEFAULT_PERIOD 1
//#endif

struct staffettamac_config {
  rtimer_clock_t on_time;
  rtimer_clock_t off_time;
  rtimer_clock_t strobe_time;
  rtimer_clock_t strobe_wait_time;
};

/*------------------------- FUNCTIONS --------------------------------------------------*/
int staffetta_main(void);
// uint32_t getWakeups(void);
void sink_listen(void);
void staffetta_print_stats(void);
void staffetta_add_data(uint8_t);
void staffetta_init(void);
void staffetta_get_energy_consumption(uint32_t * rxtx_time);
int staffetta_transmit();
int staffetta_listen(uint32_t timer_duration);
#endif /* __STAFFETTA_H__ */
