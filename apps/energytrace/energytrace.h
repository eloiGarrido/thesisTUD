/**
 * \file
 *         Header file for the energytrace application (based on powertrace)
 * \author
 *         Xin Wang
 */

#ifndef ENERGYTRACE_H
#define ENERGYTRACE_H

#include "../../core/sys/clock.h"

#define SHOW_ENERGY_INFO 	1
#define ENERGY_SIZE 		10
#define STAFFETTA_ENERGEST 	1
#define COFFEE_FILE_SYSTEM
#define SCALE_FACTOR 100
// #define FIXED_ENERGY_STEP  	1

/* choose energy harvesting model */
#define MODEL_BERNOULLI
// #define MODEL_MARKOV
#define MODEL_SOLAR
// #define MODEL_MOVER

#define RANDOM_RAND_MAX 65535U
// #define RAND_MAX 2147483647
#define PI 3.14159265

#define SOLAR_MU 10
#define SOLAR_SIGMA 2
#define MOVER_PERCENTAGE 3 // 1/3 Nodes are Movers
/*---------------------------------------------------------------------------*/
#define CPU_CURRENT 1800
#define LPM_CURRENT 54
#define RX_CURRENT 20000
#define TX_CURRENT 17700
#define TMOTE_VOLTAGE 3
// #define SCALE_FACTOR 1000
/*---------------------------------------------------------------------------*/
#define ENERGY_UPPER_THRESHOLD 1500
#define ENERGY_LOWER_THRESHOLD 600
// #define ENERGY_UPPER_THRESHOLD 1838
// #define ENERGY_LOWER_THRESHOLD 1738
/* initial energy (uJ) */

// #define ENERGY_INITIAL 1838*SCALE_FACTOR
#define ENERGY_INITIAL 18380


/* maximum enrgy (uJ) */
#define ENERGY_MAX_CAPACITY 3675*SCALE_FACTOR

/* the energy (uJ) can be harvested per second */
#define ENERGY_HARVEST_STEP 919*SCALE_FACTOR

/* the energy (uJ) which is consumed per second (to simulate node activity) */
#define ENERGY_CONSUMES_PER_SECOND 500*SCALE_FACTOR
/*---------------------------------------------------------------------------*/
// #define ENERGY_MAX_CAPACITY_SOLAR  54450 	//0.5 * 100mF * 3.3v^2 	-> in uJ

// #define ENERGY_MAX_CAPACITY_SOLAR  5445*SCALE_FACTOR
#define ENERGY_MAX_CAPACITY_SOLAR  544500
// #define ENERGY_MAX_CAPACITY_MOVER  850*SCALE_FACTOR	//0.5 * 1mF * 3.3v^2	-> in uJ
#define ENERGY_MAX_CAPACITY_MOVER  85000	//0.5 * 1mF * 3.3v^2	-> in uJ

#define ENERGY_HARVEST_STEP_SOLAR 	100*SCALE_FACTOR		//Average solar harvest amount
#define ENERGY_HARVEST_STEP_MOVER	70*SCALE_FACTOR		//Average mover harvest amount

#define ENERGY_CONSUMES_PER_MS 		85*SCALE_FACTOR		//Energy consumed per in uJ


/*---------------------------------------------------------------------------*/
void energytrace_start(void);
void energytrace_stop(void);

typedef enum {
  POWERTRACE_ON,
  POWERTRACE_OFF
} powertrace_onoff_t;

typedef enum {
    HARVEST_INACTIVE,
    HARVEST_ACTIVE
} havest_state_t;

typedef enum {
    NODE_INACTIVE,
    NODE_ACTIVE
} node_state_t;

typedef enum {
	NODE_SOLAR,
	NODE_MOVER
} node_class_t;

typedef enum {
	ZERO,
	LOW,
	MID,
	HIGH
} node_dc_state_t;

void energytrace_print(char *str);

extern uint32_t acum_consumption;
extern uint32_t acum_harvest;
extern uint32_t remaining_energy;
extern uint32_t harvesting_rate_array[10];
extern uint8_t  energy_change;
/* node state */
extern node_state_t node_state;
extern node_state_t node_state_old;
extern process_event_t node_activation_ev;

#endif /* ENERGYTRACE_H */
