/**
 * \file
 *         Header file for the metric file.
 * \author
 *         Eloi Garrido Barrabés
 */

#ifndef METRIC_H
#define METRIC_H

// #define NS_ENERGY_LOW	4100 	//4.1mJ * 1000 (Floating Point)
// #define NS_ENERGY_MID	10971	//10.971mJ * 1000 (Floating Point)
// #define NS_ENERGY_HIGH	24610	//24.61mJ * 1000 (Floating Point)

#define NS_ENERGY_LOW	410 	//4.1mJ * 1000 (Floating Point)
#define NS_ENERGY_MID	1097	//10.971mJ * 1000 (Floating Point)
#define NS_ENERGY_HIGH	2461	//24.61mJ * 1000 (Floating Point)

// #define NS_EH_LOW	410 	// 0.41 mW/s * 1000 (FP)
// #define NS_EH_MID	1098 	// 0.41 mW/s * 1000 (FP)
// #define NS_EH_HIGH	2461 	// 0.41 mW/s * 1000 (FP)

#define NS_EH_LOW	41 	// 0.41 mW/s * 1000 (FP)
#define NS_EH_MID	110 	// 0.41 mW/s * 1000 (FP)
#define NS_EH_HIGH	246 	// 0.41 mW/s * 1000 (FP)

#define DC_ZERO 0
#define DC_LOW 	5 	// Duty Cycle 0.5% * 10
#define DC_MID	15	// Duty Cycle 1.5% * 10
#define DC_HIGH	35	// Duty Cycle 3.5% * 10

#define PERIOD 10	//Period in seconds

typedef enum{
	NS_ZERO,
	NS_LOW,
	NS_MID,
	NS_HIGH
} node_energy_state_t;

extern int node_duty_cycle;
extern node_energy_state_t node_energy_state;
extern uint16_t harvesting_rate;

void compute_node_duty_cycle(void);
uint8_t get_duty_cycle(void);
void compute_node_state(void);
node_energy_state_t get_node_state(void);
void compute_harvesting_rate(void);
uint16_t get_harvesting_rate(void);

#endif /* METRIC_H */