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
uint16_t harvesting_rate_array[5] = {0,0,0,0,0};
uint8_t harvesting_array_index = 0;
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

	uint32_t rd = 0;

	node_class = NODE_SOLAR;
	// remaining_energy = ENERGY_MAX_CAPACITY_SOLAR / 4;
	// remaining_energy 
	node_activation_ev = process_alloc_event();
	


	PROCESS_BEGIN();

	harvest_state = HARVEST_INACTIVE;
	node_state = NODE_ACTIVE;
	period = data;

	// random seed
//	random_init((unsigned short)(rimeaddr_node_addr.u8[0]));
	int r = rand();
	random_init((unsigned short) (r));
	if (period == NULL) {
		PROCESS_EXIT();
	}
	etimer_set(&periodic, *period);

	while (1) {
		clock_delay(10000);
		PROCESS_WAIT_UNTIL(etimer_expired(&periodic));
		etimer_reset(&periodic);

/*EGB---------------------------------------------------------------------------*/

		if (node_state == NODE_ACTIVE && remaining_energy < (uint32_t)ENERGY_LOWER_THRESHOLD) {
			node_state = NODE_INACTIVE;
			//Notify main process with node state change
			process_post(PROCESS_BROADCAST,PROCESS_EVENT_MSG, "INACTIVE");
		} else if (node_state == NODE_INACTIVE && remaining_energy > (uint32_t)ENERGY_UPPER_THRESHOLD) {
			node_state = NODE_ACTIVE;
			//Notify main process with node state change
			process_post(PROCESS_BROADCAST,PROCESS_EVENT_MSG, "ACTIVE");
		}


		if (node_class == NODE_SOLAR)
		{
			rd = random_rand() % 100;
			rd = rd * 2 * ENERGY_HARVEST_STEP_SOLAR;
			rd = rd / 100;
			// printf("rd %lu\n",rd );
			// printf("remaining_energy + rd: %lu\n", remaining_energy + rd);
			harvesting_rate_array[harvesting_array_index] = (uint16_t)rd;
			harvesting_array_index++;
			if (harvesting_array_index > 4){ harvesting_array_index = 0;}
			
			if ((uint32_t)ENERGY_MAX_CAPACITY_SOLAR - remaining_energy < (uint32_t)rd )
			{
				remaining_energy = (uint32_t)ENERGY_MAX_CAPACITY_SOLAR;
			}
			else
			{
				remaining_energy = remaining_energy + (uint32_t)rd;
			}

		}
		else if (node_class == NODE_MOVER) // TODO Add adapted code from SOLAR into MOVER
		{
			rd = random_rand() % 100;
			rd = rd * 2 * ENERGY_HARVEST_STEP_MOVER;
			rd = rd / 100;
		}
		else
		{
			printf("ERROR! Node Class not defined\n");
		}

		#ifdef MODEL_BERNOULLI

		harvest_state = HARVEST_ACTIVE;

/*EGB---------------------------------------------------------------------------*/
		#endif /* MODEL_BERNOULLI */

		// int prob2 = random_rand() % PROB_SCALE_FACTOR;

		// if (prob2 <= energy_consumes_prob) {
			if (remaining_energy > (uint32_t)ENERGY_CONSUMES_PER_MS)
			{
				remaining_energy = remaining_energy - (uint32_t)ENERGY_CONSUMES_PER_MS;
			}
			else
			{
				remaining_energy = 0;
			}

            compute_node_state();
            compute_node_duty_cycle();
            compute_harvesting_rate();
	}

	PROCESS_END();
}



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
	// tx_energy = voltage * tx_current_consumption(tx_level) * tx_time_us / 1000 / 10;
	// rx_energy = voltage * rx_current_consumption * rx_time_us / 1000 / 10;
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
		energytrace_print("");
	}
	PROCESS_END();
}

void
energytrace_start(void)
{
	clock_time_t period = CLOCK_SECOND / 1000; //Addapt period to a smaller scale (1us) &EGB
	clock_time_t long_period = CLOCK_SECOND * 5;
	process_start(&energytrace_process, (void *)&period);
	process_start(&energytrace_process_print, (void *)&long_period);
}

void
energytrace_stop(void)
{
	process_exit(&energytrace_process);
	process_exit(&energytrace_process_print);
}

