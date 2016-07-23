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

/*
 * \file
 *          Metric: Contain the functions that allow the duty cycle and metric computations and support processes.
 * \author
 *          Eloi Garrido BarrabÃ©s
 */
#include "contiki.h"
#include "metric.h"
#include "../apps/energytrace/energytrace.h"
#include <stdio.h>
/*---------------------------------------------------------------------------*/
/* DEFINES */
/*---------------------------------------------------------------------------*/
/* VARIABLES AND CONSTANTS */
uint32_t node_duty_cycle;
uint8_t node_gradient = 20;
node_energy_state_t node_energy_state = NS_ZERO;
uint32_t harvesting_rate = 0;
#if LOW_ENERGY
int B = 95;
static int phi[3] = {1, 1, -B_goal};
#else
int B = 100;
static int phi[3] = {100, 1, -B_goal};
#endif

static int theta[3] = {2,-1,1};
static int rho = 1, u = 1, u_avg = 1;
static uint32_t cons_array[10];
static uint32_t harv_array[10];
static uint8_t array_idx = 0;
/*---------------------------------------------------------------------------*/
/* FUNCTIONS */
void compute_node_duty_cycle(void){
    node_duty_cycle = rho;
}

uint32_t get_duty_cycle(void){
#if ADAPTIVE_DC
    int val_t, val2_t;
    int phi_t[3];
    int mu = 1;
    uint32_t energy_needed;

    B = (remaining_energy * 100) / ENERGY_MAX_CAPACITY_SOLAR;

    // -------------------------------- //
    // theta = theta + ((mu / (sum(phi * phi)))* phi * (B - sum(phi * theta)))
    // -------------------------------- //
    // printf("OPERATION|energy:%lu|max:%lu|operation:%lu\n",remaining_energy * 100, ENERGY_MAX_CAPACITY_SOLAR,(remaining_energy * 100) / ENERGY_MAX_CAPACITY_SOLAR );
    // printf("OPERATION|B:%d|phi:%d|phi:%d|phi:%d|energy:%lu\n",B, phi[0], phi[1], phi[2],remaining_energy);
    val_t = (1000 * mu / (phi[0] * phi[0] + phi[1] * phi[1] + phi[2] * phi[2]));
    phi_t[0] = val_t * phi[0]; 
    phi_t[1] = val_t * phi[1]; 
    phi_t[2] = val_t * phi[2]; 
    // printf("OPERATION|val_t:%d|phi_t:%d|phi_t:%d|phi_t:%d|op:%d\n",val_t, phi_t[0], phi_t[1], phi_t[2],(phi[0] * phi[0] + phi[1] * phi[1] + phi[2] * phi[2]));
    val2_t = B - (phi[0] * theta[0] + phi[1] * theta[1] + phi[2] * theta[2]);
    phi_t[0] = (phi_t[0] * val2_t) / 100; // Remove x1000 of mu
    phi_t[1] = (phi_t[1] * val2_t) / 100;
    phi_t[2] = (phi_t[2] * val2_t) / 100;
    // printf("OPERATION|val2_t:%d|phi_t:%d|phi_t:%d|phi_t:%d\n",val2_t, phi_t[0], phi_t[1], phi_t[2]);
    theta[0] = theta[0] + phi_t[0];
    theta[1] = theta[1] + phi_t[1];
    theta[2] = theta[2] + phi_t[2];
    // printf("OPERATION|theta:%d|theta:%d|theta:%d\n", theta[0], theta[1], theta[2]);
    // -------------------------------- //
    // u = (B_goal - theta[0]*B + theta[2]*B_goal) / theta[1]
    // -------------------------------- //
    u = (B_goal - theta[0]*B + theta[2]*B_goal) / theta[1];
    u = MIN(DC_MAX, u);
    u = MAX(DC_0, u);
    // -------------------------------- //
    // phi = np.array([B, u, -B_goal])
    // -------------------------------- //
    phi[0] = B;
    phi[1] = u;
    phi[2] = -B_goal;
    // printf("OPERATION|phi:%d|phi:%d|phi:%d\n",phi[0], phi[1], phi[2]);
    // -------------------------------- //
    // u_avg = Alpha*u + (1-Alpha)*u_avg
    // -------------------------------- //    
    u_avg = (ALPHA*u + (100-ALPHA)*u_avg) / 100;
    // -------------------------------- //
    // rho = Beta*u + (1-Beta)*u_avg
    // -------------------------------- // 
    rho = (BETA*u + (100-BETA)*u_avg) / 100;
    // printf("OPERATION|u:%d|u_avg:%d|rho:%lu\n",u, u_avg, (uint32_t) rho);
    
    // Check that we have enough energy to operate at rho DC
    energy_needed = rho * 10 * 7 + rho * 5; // rho * 7.5 * SCALE_FACTOR 
    while (energy_needed > remaining_energy && rho > 1) {
        rho--;
        energy_needed = rho * 10 * 7 + rho * 5; // rho * 7.5 * SCALE_FACTOR 
    }
    if (remaining_energy < 75) rho = 0;
    return rho;
#else
    return 1;
#endif
}
void compute_harvest_gradient(void) {
    uint32_t avg_cons, avg_harv, hr_grad;
    int i = 0;
    cons_array[array_idx] = acum_consumption;
    harv_array[array_idx] = acum_harvest;
    
    node_gradient = 20 / rho;
    avg_cons = 0;
    avg_harv = 0;
    for (i=0; i<10;i++) {
        avg_cons += cons_array[i];
        avg_harv += harv_array[i];
    }
    avg_cons = avg_cons / 10;
    avg_harv = avg_harv / 10;

    if (avg_harv == 0) {
        hr_grad = (uint32_t)MAX_HR_GRAD;
    } else {
        hr_grad = avg_cons / avg_harv;
    }

    // Discretize hr_grad value into LOW, MID and HIGH
    if (remaining_energy >= (uint32_t)NS_ENERGY_LOW) {
        if (hr_grad > 5) {
            node_energy_state = NS_LOW;
        } else if (hr_grad > 2) {
            node_energy_state = NS_MID;
        } else {
            node_energy_state = NS_HIGH;
        }
    } else {
        node_energy_state = NS_ZERO;
    }
    array_idx = (array_idx + 1) % 10;
    // printf("avg_cons:%lu,avg_harv:%lu\n",avg_cons,avg_harv);
    // printf("hr_grad:%lu|node_energy_state:%u\n",hr_grad, node_energy_state );

}


void compute_node_state(void){
#if FIX_NODE_STATE
    node_energy_state = NS_MID;
#else
#if ADAPTIVE_DC
    if (remaining_energy > (uint32_t)NS_ENERGY_LOW) {
        node_gradient = 20 / rho;
        if (remaining_energy > (uint32_t)NS_ENERGY_HIGH) {
            node_energy_state = NS_HIGH;
        } else if (remaining_energy > (uint32_t)NS_ENERGY_MID){
            node_energy_state = NS_MID;
        } else {
            node_energy_state = NS_LOW;
        } 
    } else {
        node_energy_state = NS_ZERO;
    }

#else
#if HYSTERESIS
    switch (node_energy_state)
    {
        case NS_HIGH:
            if ( remaining_energy > ( (uint32_t)NS_ENERGY_HIGH - 100 ) ){ node_energy_state = NS_HIGH; }
            else if ( remaining_energy > ( (uint32_t)NS_ENERGY_MID - 100) ){ node_energy_state = NS_MID; }
            else if ( remaining_energy > (uint32_t)NS_ENERGY_LOW){ node_energy_state = NS_LOW; }
            else { node_energy_state = NS_ZERO; }
            break;

        case NS_MID:
            if ( remaining_energy > ( (uint32_t)NS_ENERGY_HIGH + 100 )) { node_energy_state = NS_HIGH; }
            else if ( remaining_energy > ( (uint32_t)NS_ENERGY_MID - 100)) { node_energy_state = NS_MID; }
            else if ( remaining_energy > (uint32_t)NS_ENERGY_LOW) { node_energy_state = NS_LOW; }
            else { node_energy_state = NS_ZERO; }
            break;

        case NS_LOW:
            if ( remaining_energy > ( (uint32_t)NS_ENERGY_HIGH + 100 ) ){ node_energy_state = NS_HIGH; }
            else if ( remaining_energy > ( (uint32_t)NS_ENERGY_MID + 100) ){ node_energy_state = NS_MID; }
            else if ( remaining_energy > (uint32_t)NS_ENERGY_LOW){ node_energy_state = NS_LOW; }
            else { node_energy_state = NS_ZERO; }
            break;

        default:
            if (remaining_energy > (uint32_t)NS_ENERGY_HIGH) {
                node_energy_state = NS_HIGH;
            } else if (remaining_energy > (uint32_t)NS_ENERGY_MID){
                node_energy_state = NS_MID;
            } else if ( (uint32_t)remaining_energy > (uint32_t)NS_ENERGY_LOW){
                node_energy_state = NS_LOW;
            } else {
                node_energy_state = NS_ZERO;
            }
            break;
    }
#else
    if (remaining_energy > (uint32_t)NS_ENERGY_HIGH) {
        node_energy_state = NS_HIGH;
    } else if (remaining_energy > (uint32_t)NS_ENERGY_MID){
        node_energy_state = NS_MID;
    } else if ( (uint32_t)remaining_energy > (uint32_t)NS_ENERGY_LOW){
        node_energy_state = NS_LOW;
    } else {
        node_energy_state = NS_ZERO;
    }
#endif /*HYSTERESIS*/
#endif /*ADAPTIVE_DC*/
#endif /*FIX_NODE_STATE*/
}

node_energy_state_t get_node_state(void){
    return node_energy_state;
}

void compute_harvesting_rate(void){
    int indx;
    uint32_t acum = 0;
    for (indx = 0; indx < 10; indx++){
        acum += harvesting_rate_array[indx];
    }
    if (acum < 10){ harvesting_rate = 0;}
    else{harvesting_rate = acum / 10;}

}

uint32_t get_harvesting_rate(void){
    return harvesting_rate;
}
