/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
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
 */

/**
 * \file
 *         Energytrace: periodically print out energy consumption
 * \author
 *         Xin Wang
 */

#include "../../core/contiki.h"
#include "../../core/contiki-lib.h"

#include "energytrace.h"

#include "../../core/dev/dev/cc2420/cc2420.h"

#include "../../core/dev/metric.h"

#include <stdio.h>
#include <string.h>
#include "../../core/sys/compower.h"
#include "../../core/lib/random.h"
//#include "rimeaddr.h"
//#include "net/rime/rime.h"
// #ifdef MODEL_SOLAR
#include "math.h"
// #endif /*MODEL_SOLAR*/
#include "../../platform/sky/node-id.h"
#include "../../core/dev/staffetta.h"
/* ------------- coffee file system--------------------- */
#ifdef COFFEE_FILE_SYSTEM
#include "../../core/cfs/cfs.h"
#include "../../core/cfs/cfs-coffee.c"

// #include "/home/egarrido/staffetta_sensys2015/eh_staffetta/core/cfs/cfs.h"
#endif /*COFFEE_FILE_SYSTEM*/

struct energytrace_sniff_stats {
	struct energytrace_sniff_stats *next;
	uint32_t num_input, num_output;
	uint32_t input_txtime, input_rxtime;
	uint32_t output_txtime, output_rxtime;
#if UIP_CONF_IPV6
	uint16_t proto; /* includes proto + possibly flags */
#endif
	uint16_t channel;
	uint32_t last_input_txtime, last_input_rxtime;
	uint32_t last_output_txtime, last_output_rxtime;
};

#define TMOTE_ARCH_SECOND 8192
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * Energy harvesting related settings
 */
process_event_t node_activation_ev;
static havest_state_t harvest_state;

node_state_t node_state;
node_state_t node_state_old = NODE_INACTIVE;

node_dc_state_t node_dc_state = ZERO;

#if ADAPTIVE_PACKET_CREATION
uint8_t energy_change = 0;
#endif /*ADAPTIVE_PACKET_CREATION*/

#define PROB_SCALE_FACTOR 1000

/* Markov model */
#ifdef MODEL_MARKOV

/* prob markov_prob_active_to_inactive = 0.045: active -> inactive */
static int markov_prob_active_to_inactive = (int)(0.045 * PROB_SCALE_FACTOR);

/* prob markov_prob_inactive_to_active = 0.005: inactive -> active */
// static int markov_prob_inactive_to_active = 5;
static int markov_prob_inactive_to_active = (int)(0.005 * PROB_SCALE_FACTOR);

#endif /* MODEL_MARKOV */

/* Bernoulli model */
#ifdef MODEL_BERNOULLI

/* harvesting probability */
// static int bernoulli_harvesting_prob = (int)(0.5 * PROB_SCALE_FACTOR);

#endif /* MODEL_BERNOULLI */

/* the probability of consuming energy per second */
static int energy_consumes_prob = (int)(0.8 * PROB_SCALE_FACTOR);
static struct pt_sem /*mutex*/;

/**
 * CC2420
 * Manual http://www.ti.com/lit/ds/symlink/cc2420.pdf
 */

/**
 * RX current consumpiton(mA)
 * values are multiplied by 10 (should be 18.8 mA)
 */
static long rx_current_consumption = 188;

/**
 * Output voltage (V)
 * typical: 3.0, min: 2.1, max: 3.6 (from Page 13)
 */
static long voltage = 3;

/* Remaining energy (uJ) */
uint32_t remaining_energy = ENERGY_INITIAL;
uint32_t harvesting_rate_array[5] = {0,0,0,0,0};
uint8_t harvesting_array_index = 0;
uint32_t acum_consumption = 0;
uint32_t acum_harvest = 0;

#ifdef COFFEE_FILE_SYSTEM

#ifdef MODEL_MOVER
int fd_read_mover;
#endif

#ifdef MODEL_SOLAR
int fd_read_solar;
#endif
#endif /*COFFEE_FILE_SYSTEM*/


#if ADAPTIVE_PACKET_CREATION
#define hr_threshold 3
static uint16_t last_hr = 0;
static uint32_t time_hr = 0;
uint8_t context_trigger(void)
{
	uint16_t hr;
	uint32_t time_now, time_diff;

	time_now = RTIMER_NOW();
	time_diff = (time_now - time_hr) / TMOTE_ARCH_SECOND;

	hr = get_harvesting_rate();
	if ( (abs( hr - last_hr) > hr_threshold) && time_diff > 5 )
	{
		time_hr = time_now;
		last_hr = hr;
		return 1;
	}
	return 0;
}
#endif /*ADAPTIVE_PACKET_CREATION*/

// static uint32_t high_th = 1838;
// static uint32_t low_th = 1738;
/*---------------------------------------------------------------------------*/
// int bernoulli_solar_harvesting_prob = (int)()
/*---------------------------------------------------------------------------*/

/**
 * Transmit power levels
 * values are from Page 51
 */
// static uint8_t tx_levels[TX_LEVELS] =  {31, 27, 23, 19, 15, 11, 7, 3};
// static uint8_t tx_current_consumption[TX_LEVELS] = {174, 165, 152, 139, 125, 112, 99, 85};

//static int
//dir_test(void)
//{
//  struct cfs_dir dir;
//  struct cfs_dirent dirent;
//
//  /* Coffee provides a root directory only. */
//  if(cfs_opendir(&dir, "/") != 0) {
//    printf("failed to open the root directory\n");
//    return 0;
//  }
//
//  /* List all files and their file sizes. */
//  printf("Available files\n");
//  while(cfs_readdir(&dir, &dirent) == 0) {
//    printf("%s (%lu bytes)\n", dirent.name, (unsigned long)dirent.size);
//  }
//
//  cfs_closedir(&dir);
//
//  return 1;
//}


#ifdef MODEL_SOLAR
//double randn (uint32_t mu, uint32_t sigma)
//{
//	double U1, U2, W, mult, U1_t, U2_t; //double
//	static double X1, X2; //double
//	static uint8_t call = 0; //int
//    printf(">> 1\n");
//	if (call == 1)
//	{
//		call = !call;
//		return (mu + sigma * (uint32_t) X2);
//    }
//    printf(">> 2\n");
//	do
//	{
////	    printf("random %ld\n", (double) random_rand());
////	    printf("RAND_MAX %ld, %ld\n", (double) RAND_MAX, 2147483647);
//        U1_t = (double) random_rand();
//        U2_t = (double) random_rand();
//        U1 = ( U1_t / 2147483647);
//        U2 = ( U2_t/ 2147483647);
//        printf("U1: %ld, U2: %ld\n", U1, U2);
//        printf("U1: %ld, U2: %ld\n", U1, U2);
//
//		U1 = -1 + U1 * 2;
//		U2 = -1 + U2 * 2;
////		U1 = -1 + ((double) random_rand() / (double)RAND_MAX) * 2;
////		U2 = -1 + ((double) random_rand() / (double)RAND_MAX) * 2;
//		W = pow (U1, 2) + pow (U2, 2);
//		printf("U1: %ld, U2: %ld, W: %ld\n", U1, U2, W);
//	}
//	while (W >= 1 || W == 0);
//    printf(">> 3\n");
//
//	mult = sqrt ((-2 * logf(W)) / W);
//	X1 = U1 * mult;
//	X2 = U2 * mult;
//    printf(">> 4\n");
//
//	call = !call;
//	return (uint32_t)(mu + sigma * (double) X1);
//}

//uint32_t randn (uint32_t mu, uint32_t sigma)
//{
//    uint32_t return1, return2;
//	uint32_t U1, U2, W, mult, U1_t, U2_t; //double
//	uint32_t pow_U1, pow_U2;
//	static uint32_t X1, X2; //double
//	static uint8_t call = 0; //int
//
//	if (call == 1)
//	{
//		call = !call;
//		return2 = (uint32_t)mu + (uint32_t)sigma * X2;
//        printf("---------> Return2: %lu\n",return2);
//
//		return return2;
//    }
////    printf(">> 2\n");
//	do
//	{
//
//        U1_t = random_rand() * RANDOM_RAND_MAX;
//        U1_t = U1_t * 1000;
//        U2_t = random_rand()* RANDOM_RAND_MAX;
//        U2_t = U2_t * 1000;
//        U1 = ( U1_t / RANDOM_RAND_MAX );
//        U2 = ( U2_t / RANDOM_RAND_MAX );
//
////		printf(">> U1: %lu, U2: %lu\n", U1, U2);
//
//		U1 = -1000 + ( U1 * 2 );
//		U2 = -1000 + ( U2 * 2 );
////        printf(">>>> U1: %lu, U2: %lu\n", U1, U2);
//
////        pow_U1 = pow(U1,2);
//        pow_U1 = U1;
//        pow_U1 = pow_U1 * U1;
//
//        pow_U2 = U2;
//        pow_U2 = pow_U2 * U2;
////        pow_U2 = pow(U2,2);
////        pow_U2 = U2 * U2;
////        printf(">>>>>> pow_U1: %lu, pow_U2: %lu\n", pow_U1, pow_U2);
//
////        printf(">>>> U1: %lu, U2: %lu\n", U1, U2);
//        W = pow_U1 + pow_U2;
////		W = pow (U1, 2) + pow (U2, 2);
////		printf("U1: %lu, U2: %lu, W: %lu\n", U1, U2, W);
//	}while (W >= (uint32_t)1000 || W <= (uint32_t)0 || U1 > 1000);
////    printf("U2_t: %lu, RAND_MAX_t: %lu, U2: %lu\n", U2_t, RANDOM_RAND_MAX, U2);
////    printf(">> 3\n");
////    printf(">> U1_t: %lu, U2_t: %lu, W: %lu\n", U1_t, U2_t, W);
////    printf(">>>>>> pow_U1: %lu, pow_U2: %lu\n", pow_U1, pow_U2);
//
//
//	mult = sqrt ((2 * 1000 * logf(W)) / (W));
////    printf("U1: %lu, U2: %lu, W: %lu, mult: %lu\n", U1, U2, W, mult);
//	X1 = U1 * mult ;
//	X2 = U2 * mult ;
////    printf(">> 4\n");
//
//	call = !call;
////	printf("X1: %lu, X2: %lu\n",X1, X2);
//	return1 = (uint32_t)mu + (uint32_t)sigma * X1;
//	printf("---------> Return1: %lu\n",return1);
//	return return1;
//}


//
////uint32_t solar_energy_input (uint32_t time_solar, uint8_t scalling_factor)
//uint32_t solar_energy_input ( uint8_t scalling_factor)
//{
//
//	uint32_t result;
//	double normal_var;
//	int solar_mu = 200;
//	int solar_sigma = 40;
//
//    uint32_t time_solar = RTIMER_NOW();
//
////    printf(">>>> Before randn\n");
//	normal_var = randn(solar_mu, solar_sigma);
////    printf(">>>> After randn\n");
//
//	result = (uint32_t)( scalling_factor * normal_var * cos( time_solar / (70 * M_PI) ) * cos( time_solar / (100 * M_PI) ) );
//	printf(">> solar_energy_input -> %lu\n",result);
//	return result;
//}
#endif /*MODEL_SOLAR*/

#ifdef COFFEE_FILE_SYSTEM
#define MESSAGE_SIZE 30
void clean_message(char message[MESSAGE_SIZE])
{
  uint8_t idx;
  for (idx=0; idx<MESSAGE_SIZE; idx++){message[idx] = '\0';}
}


int read_lines(char message[MESSAGE_SIZE], uint16_t lines, uint8_t solarNoMover)
{
  char char_buffer[2] = "\0\0";
  char string_buffer[MESSAGE_SIZE];
  uint8_t idx=0;
  uint16_t lines_read = 0;
  int bytes_read = 0;
  int fd_read;

  clean_message(string_buffer);
  clean_message(message);

#ifdef MODEL_SOLAR
  if (solarNoMover == 1){fd_read = fd_read_solar;}
#endif

#ifdef MODEL_MOVER
  if (solarNoMover == 0){fd_read = fd_read_mover;}
#endif

  if(fd_read != -1) 
  {
    do
    {
      bytes_read = cfs_read(fd_read, char_buffer, sizeof(char));
      if (bytes_read > 0) 
      {
        string_buffer[idx] = char_buffer[0];
        idx += 1;
        if ( char_buffer[0] == '\n')
        {
          lines_read += 1;
        }
      }
      else
      {
        return 0;
      }
    }while (lines_read < lines);
    strncpy(message, string_buffer, idx);
  } 
  else 
  {
    printf("ERROR: read_lines.\n");
  }
  return bytes_read;
}


#endif /*COFFEE_FILE_SYSTEM*/


#ifdef MODEL_SOLAR
uint8_t init_solar_file()
{
	int rnd;
	rnd = random_rand() % 1500;
//    cfs_coffee_format();
	fd_read_solar = cfs_open("cfs_file.txt", CFS_READ);
  	if(fd_read_solar != -1)
	{
		cfs_seek(fd_read_solar, rnd, CFS_SEEK_SET);
		return 1;
	}
	else
	{
		return 0;
	}
}

uint32_t get_solar_energy(void)
{
  	int bytes_read = 0;
  	char message[MESSAGE_SIZE];
  	uint32_t result;
	bytes_read = read_lines( message, 1, 1 );
	if (bytes_read > 0)
	{
		result = atoi(message); //TODO add mult fator to deal with floating point
		return result;
	}
	else
	{
		cfs_seek(fd_read_solar, 0, CFS_SEEK_SET); //Return pointer to start of file
		bytes_read = read_lines( message, 1, 1 );
		if (bytes_read > 0)
		{
			result = atoi(message);
			return result;
		}
		else
		{
			return -1;
		}

	}
}
#endif


#ifdef MODEL_MOVER
uint8_t init_mover_file()
{
	fd_read_mover = cfs_open("energyShoeTrace.txt", CFS_READ);
  	if(fd_read_mover != -1)
	{
		return 1;
	} 
	else 
	{
		return 0;
	}
}

uint32_t get_mover_energy(void)
{
  	int bytes_read = 0;
  	char message[MESSAGE_SIZE];
  	uint32_t result;
	bytes_read = read_lines( message, 1 ,0 );
	if (bytes_read > 0)
	{
		result = atoi(message); //TODO add mult fator to deal with floating point
		return result;
	}
	else
	{
		cfs_seek(fd_read_mover, 0, CFS_SEEK_SET); //Return pointer to start of file
		bytes_read = read_lines( message, 1, 0 );
		if (bytes_read > 0)
		{
			result = atoi(message);
			return result;
		}
		else
		{
			return -1;
		}
	
	}
}
#endif
/**
 * TX current consumption (mA)
 * values are multiplied by 10 (e.g. 174 should be 17.4mA)
 */
inline long tx_current_consumption(uint8_t tx_level)
{
	if (tx_level > 27) {
		return 174;
	} else if (tx_level <= 27 && tx_level > 23) {
		return 165;
	} else if (tx_level <= 23 && tx_level > 19) {
		return 152;
	} else if (tx_level <= 19 && tx_level > 15) {
		return 139;
	} else if (tx_level <= 15 && tx_level > 11) {
		return 125;
	} else if (tx_level <= 11 && tx_level > 7) {
		return 112;
	} else if (tx_level <= 7 && tx_level > 3) {
		return 99;
	} else if (tx_level <= 3 && tx_level > 0) {
		return 85;
	}
	return 0;
}

PROCESS(energytrace_process, "Periodic energy output");
PROCESS(energytrace_process_print, "Periodic energy output print");
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(energytrace_process, ev, data)
{
	static struct etimer periodic;
	clock_time_t *period;
	static node_class_t node_class; //EGB
	uint32_t rxtx_time; 
	uint32_t energy_rxtx;
	uint32_t rd = 0;
    uint8_t tx_level;



	// remaining_energy = ENERGY_MAX_CAPACITY_SOLAR / 4;
	// remaining_energy 
	node_activation_ev = process_alloc_event();
	

	PROCESS_BEGIN();
#ifdef MODEL_MOVER
	PROCESS_EXITHANDLER(cfs_close(fd_read_mover);)
#endif /*MODEL_MOVER*/

#ifdef MODEL_SOLAR
	PROCESS_EXITHANDLER(cfs_close(fd_read_solar);)
#endif /*MODEL_SOLAR*/

#ifdef MODEL_MOVER

	if ( node_id % MOVER_PERCENTAGE == 0) {
		node_class = NODE_MOVER;
		if (init_mover_file() == 0)
		{
			printf(">> ERROR on mover file opening\n");
		}
	} else {
		node_class = NODE_SOLAR;
        if (init_solar_file() == 0)
		{
			printf(">> ERROR on solar file opening\n");
		}
	}
#else

	node_class = NODE_SOLAR;
#ifdef MODEL_SOLAR
    if (init_solar_file() == 0)
    {
        printf(">> ERROR on solar file opening\n");
    }
#endif /*MODEL_SOLAR*/

#endif /*MODEL_MOVER*/



	harvest_state = HARVEST_INACTIVE;
	node_state = NODE_ACTIVE;
	period = data;

	// random seed
//	random_init((unsigned short)(rimeaddr_node_addr.u8[0]));


	random_init((unsigned short) (clock_time()));
	int r = random_rand();
	if (period == NULL) {
		PROCESS_EXIT();
	}
	etimer_set(&periodic, *period);

	while (1) {
		clock_delay(10000);
		PROCESS_WAIT_UNTIL(etimer_expired(&periodic));
		etimer_reset(&periodic);

/*EGB---------------------------------------------------------------------------*/

		if (node_state == NODE_ACTIVE && remaining_energy < (uint32_t)ENERGY_LOWER_THRESHOLD) 
		{
			node_state = NODE_INACTIVE;
		} 
		else if (node_state == NODE_INACTIVE && remaining_energy > (uint32_t)ENERGY_UPPER_THRESHOLD) 
		{
			node_state = NODE_ACTIVE;
		}


		if (node_class == NODE_SOLAR)
		{
#ifdef MODEL_SOLAR
//            rtimer_clock_t t2;
//			t2 = RTIMER_NOW ();
//			rd = solar_energy_input(RTIMER_NOW () ,1); //TODO Validate

//			rd = solar_energy_input(1); //TODO Validate

            rd = get_solar_energy();

#else
#if FIXED_ENERGY_STEP
			    rd = ENERGY_HARVEST_STEP_SOLAR;
#else
				rd = random_rand() % 100;
				rd = rd * 2 * ENERGY_HARVEST_STEP_SOLAR;
				rd = rd / 100;
				rd = rd * 0.85;
#endif /*FIXED_ENERGY_STEP*/
#endif /*MODEL_SOLAR*/

			harvesting_rate_array[harvesting_array_index] = rd;
			harvesting_array_index++;
			if (harvesting_array_index > 4){ harvesting_array_index = 0;}
			
			// if ((uint32_t)ENERGY_MAX_CAPACITY_SOLAR - remaining_energy < (uint32_t)rd )
			if ( (remaining_energy + rd) > (uint32_t)ENERGY_MAX_CAPACITY_SOLAR )
			{
                acum_harvest += (uint32_t)ENERGY_MAX_CAPACITY_SOLAR - remaining_energy;
//				acum_harvest +=  ( remaining_energy + (uint32_t)rd ) - (uint32_t)ENERGY_MAX_CAPACITY_SOLAR;
				remaining_energy = (uint32_t)ENERGY_MAX_CAPACITY_SOLAR;
			}
			else
			{
				remaining_energy = remaining_energy + rd;
				acum_harvest += rd;
			}

		}
		else if (node_class == NODE_MOVER) // TODO Add stored mover values and test
		{
#ifdef MODEL_MOVER
			rd = get_mover_energy();
#else
			rd = random_rand() % 100;
			rd = rd * 2 * ENERGY_HARVEST_STEP_MOVER;
			rd = rd / 100;
#endif /*MODEL_MOVER*/

			harvesting_rate_array[harvesting_array_index] = rd;
			harvesting_array_index++;
			if (harvesting_array_index > 4){ harvesting_array_index = 0;}
			
			// if ((uint32_t)ENERGY_MAX_CAPACITY_MOVER - remaining_energy < (uint32_t)rd )
			if ( (remaining_energy + rd) > (uint32_t)ENERGY_MAX_CAPACITY_MOVER )
			{
//				acum_harvest += (uint32_t)ENERGY_MAX_CAPACITY_MOVER - (remaining_energy + (uint32_t)rd);
                acum_harvest +=  (uint32_t)ENERGY_MAX_CAPACITY_MOVER - remaining_energy;
				remaining_energy = (uint32_t)ENERGY_MAX_CAPACITY_MOVER;

			}
			else
			{
				remaining_energy = remaining_energy + rd;
				acum_harvest += rd;
			}
		}
		else
		{

		}

#ifdef MODEL_BERNOULLI

		harvest_state = HARVEST_ACTIVE;

/*EGB---------------------------------------------------------------------------*/
#endif /* MODEL_BERNOULLI */

		// int prob2 = random_rand() % PROB_SCALE_FACTOR;

		// if (prob2 <= energy_consumes_prob) {
#if STAFFETTA_ENERGEST
		rxtx_time = 0;
		energy_rxtx = 0;
		staffetta_get_energy_consumption(&rxtx_time);

		tx_level = cc2420_get_txpower();
        energy_rxtx = voltage * tx_current_consumption(tx_level) * rxtx_time / 1000 / 10;

		if (remaining_energy > energy_rxtx)
		{
		    acum_consumption += energy_rxtx;
			remaining_energy -= energy_rxtx;
		}
		else
		{
            acum_consumption += remaining_energy;
			remaining_energy = 0;
		}
#else
		if (remaining_energy > (uint32_t)ENERGY_CONSUMES_PER_MS)
		{
			remaining_energy = remaining_energy - (uint32_t)ENERGY_CONSUMES_PER_MS;
		}
		else
		{
			remaining_energy = 0;
		}
#endif /*STAFFETTA_ENERGEST*/
			


            compute_node_state();
            compute_node_duty_cycle();
            compute_harvesting_rate();


#if ADAPTIVE_PACKET_CREATION
		energy_change = context_trigger();
#endif /*ADAPTIVE_PACKET_CREATION*/
	}

	PROCESS_END();
}

//TODO Reimplement original Bernoulli harvest method



void
energytrace_print(char *str)
{
	static uint32_t last_cpu, last_lpm, last_transmit, last_listen;

	uint32_t cpu, lpm, transmit, listen;
	uint32_t all_cpu, all_lpm, all_transmit, all_listen;


	uint8_t tx_level;
	long tx_energy, rx_energy;
	static uint32_t all_tx_consumed, all_rx_consumed;

	static uint32_t seqno;

	energest_flush();

	all_cpu = energest_type_time(ENERGEST_TYPE_CPU);
	all_lpm = energest_type_time(ENERGEST_TYPE_LPM);
	all_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
	all_listen = energest_type_time(ENERGEST_TYPE_LISTEN);

	cpu = all_cpu - last_cpu;
	lpm = all_lpm - last_lpm;

	/* tx and rx  (in ticks) */
	if (remaining_energy > 0) {
		transmit = all_transmit - last_transmit;
		listen = all_listen - last_listen;
	} else {
		transmit = 0;
		listen = 0;
	}


	tx_level = cc2420_get_txpower();

	/* Energy (uJ) = v (V) * current (mA) * time (us) / 1000 */
	/* divide by 10 means scale down current value (eliminating the `dot') */
	long tx_time_us = 1000 * ((1000 * transmit) / TMOTE_ARCH_SECOND);
	long rx_time_us = 1000 * ((1000 * listen) / TMOTE_ARCH_SECOND);
	// long tx_time_us = 1000 * transmit / TMOTE_ARCH_SECOND;
	// long rx_time_us = 1000 * listen / TMOTE_ARCH_SECOND;
	tx_energy = voltage * tx_current_consumption(tx_level) * tx_time_us / 1000 / 10;
	rx_energy = voltage * rx_current_consumption * rx_time_us / 1000 / 10;
	
	// printf("tx_energy %ld, rx_energy %ld\n",tx_energy,rx_energy );
	if ( (uint32_t)tx_energy >= remaining_energy)
	{
		remaining_energy = 0;
	}
	else
	{
		remaining_energy = remaining_energy - (uint32_t)tx_energy;
	}

	if ( (uint32_t)rx_energy >= remaining_energy)
	{
		remaining_energy = 0;
	}
	else
	{
		remaining_energy = remaining_energy - (uint32_t)rx_energy;
	}


	// remaining_energy = MAX(remaining_energy - tx_energy, 0);
	// remaining_energy = MAX(remaining_energy - rx_energy, 0);

	all_tx_consumed += tx_energy;
	all_rx_consumed += rx_energy;
	// printf("DEBUG: tx_time_us: %lu, voltage: %lu, tx_current: %lu, tx_energy: %lu\n", tx_time_us, voltage, tx_current_consumption(tx_level), tx_energy);

#if SHOW_ENERGY_INFO
	// printf("INFO: energy, level: %d, remaining_energy(uJ): %u, transmit(us): %lu, listen(us): %lu, tx_energy(uJ): %lu, rx_energy(uJ): %lu, all_tx_consumed(uJ): %ld, all_rx_consumed(uJ): %lu,  harvest_state: %d\n",
	      //  tx_level,
	      //  remaining_energy,
	      //  tx_time_us,
	      //  rx_time_us,
	      //  tx_energy,
	      //  rx_energy,
	      //  all_tx_consumed,
	      //  all_rx_consumed,
	      //  harvest_state
	      // );

#endif

	last_cpu = energest_type_time(ENERGEST_TYPE_CPU);
	last_lpm = energest_type_time(ENERGEST_TYPE_LPM);
	last_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
	last_listen = energest_type_time(ENERGEST_TYPE_LISTEN);

	seqno++;
}

PROCESS_THREAD(energytrace_process_print, ev, data)
{
	static struct etimer periodic2;
	clock_time_t *period2;

	PROCESS_BEGIN();
	period2 = data;

	if (period2 == NULL) {
		PROCESS_EXIT();
	}
	etimer_set(&periodic2, *period2);

	while (1) {
		PROCESS_WAIT_UNTIL(etimer_expired(&periodic2));
		etimer_reset(&periodic2);
//		energytrace_print("");
	}
	PROCESS_END();
}

void
energytrace_start(void)
{
	clock_time_t period = CLOCK_SECOND / 100; //Adapt period to a smaller scale (1us) &EGB
	clock_time_t long_period = CLOCK_SECOND * 5;
	process_start(&energytrace_process, (void *)&period);
//	process_start(&energytrace_process_print, (void *)&long_period);
}

void
energytrace_stop(void)
{
	process_exit(&energytrace_process);
//	process_exit(&energytrace_process_print);
}

