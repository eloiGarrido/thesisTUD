/**
 * \file
 *         Header file for the metric file.
 * \author
 *         Eloi Garrido Barrabés
 */
#ifndef METRIC_H
#define METRIC_H
#include <stdio.h>
#include <legacymsp430.h>
#include <stdlib.h>


#define SCALE_FACTOR 100
#define HYSTERESIS 0
#define FIX_NODE_STATE 1
/*
 * NS_ENERGY_LOW needs 750uJ to operate at a rate of 1% over a period of 1 second
 * NS_ENERGY_MID needs 3750uJ to operate at a rate of 5% over a period of 1 second
 * NS_ENERGY_LOW needs 7500uJ to operate at a rate of 10% over a period of 1 second
 */
#define NS_ENERGY_LOW	75 		//750uJ * SCAKE_FACTOR (Floating Point)
#define NS_ENERGY_MID	375		//3750uJ * SCAKE_FACTOR (Floating Point)
#define NS_ENERGY_HIGH 	750		//7500uJ * SCAKE_FACTOR (Floating Point)

#define DC_ZERO 0
#define DC_LOW 	5 	// Duty Cycle 0.5% * 10
#define DC_MID	15	// Duty Cycle 1.5% * 10
#define DC_HIGH	35	// Duty Cycle 3.5% * 10

typedef enum{
	NS_ZERO,
	NS_LOW,
	NS_MID,
	NS_HIGH
} node_energy_state_t;

extern int node_duty_cycle;
extern node_energy_state_t node_energy_state;
extern uint32_t harvesting_rate;

void compute_node_duty_cycle(void);
uint8_t get_duty_cycle(void);
void compute_node_state(void);
node_energy_state_t get_node_state(void);
void compute_harvesting_rate(void);
uint32_t get_harvesting_rate(void);

#endif /* METRIC_H */