#include "staffetta.h"
#include "node-id.h"
#include "../../apps/energytrace/energytrace.h"
#include "../../core/dev/metric.h"

// #define ADAPTIVE_PACKET_CREATION 1

//uint16_t harvesting_rate;
//node_energy_state_t node_energy_state;
static uint8_t round_stats;
static int loop_stats;

PROCESS(staffetta_test, "Staffetta test");
AUTOSTART_PROCESSES(&staffetta_test);

PROCESS(staffetta_print_stats_process, "Staffetta print stats");

PROCESS_THREAD(staffetta_print_stats_process, ev, data){
    PROCESS_BEGIN();

    static struct etimer et;

    round_stats = PAKETS_PER_NODE;
    loop_stats = node_id;
    static uint8_t counter = 0;
    //etimer_set(&et,random_rand()%(CLOCK_SECOND*5));
#if ADAPTIVE_PACKET_CREATION
    static uint8_t counter = 0;
    etimer_set(&et,CLOCK_SECOND*25+(random_rand()%(CLOCK_SECOND*10)));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    while(1) {
        if(energy_change == 1)
        {
            staffetta_print_stats();
            staffetta_add_data(round_stats++);
            energy_change = 0;
        }
        etimer_set(&et,CLOCK_SECOND*1);
        counter = counter + 1;
        if (counter >= 15){
            printf("6|%d|%lu|%lu|%lu|%lu\n", node_energy_state, remaining_energy, harvesting_rate, acum_consumption, acum_harvest);
            acum_consumption = 0; // Reset acumulative values
            acum_harvest = 0;
            counter = 0;
        }
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    }
#elif RANDOM_PACKET_CREATION
    #define RAND_PACKET_CREATION 50

    etimer_set(&et,CLOCK_SECOND*25+(random_rand()%(CLOCK_SECOND*10)));
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    while(1) {
        staffetta_print_stats();
        staffetta_add_data(round_stats++);

        etimer_set(&et,CLOCK_SECOND * 1 + (random_rand()%(CLOCK_SECOND*60)));
        printf("6|%d|%lu|%d|%lu|%lu\n", node_energy_state, remaining_energy, harvesting_rate, acum_consumption, acum_harvest);
        acum_consumption = 0; // Reset acumulative values
        acum_harvest = 0;
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    }
#else
    // etimer_set(&et,CLOCK_SECOND*25+(random_rand()%(CLOCK_SECOND*10)));
    etimer_set(&et,CLOCK_SECOND*1+(random_rand()%(CLOCK_SECOND*10)));
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    while(1) {
        staffetta_print_stats();
        counter = counter + 1;
        if (counter >= 25){
            staffetta_add_data(round_stats++);
            printf("6|%d|%lu|%lu|%lu|%lu\n", node_energy_state, remaining_energy, harvesting_rate, acum_consumption, acum_harvest);
            acum_consumption = 0; // Reset acumulative values
            acum_harvest = 0;
            counter = 0;
        }
        // staffetta_add_data(round_stats++);

        etimer_set(&et,CLOCK_SECOND*1);



        // etimer_set(&et,CLOCK_SECOND*10);
        // // printf("6|%d|%lu|%lu|%lu|%lu\n", node_energy_state, remaining_energy, harvesting_rate, acum_consumption, acum_harvest);
        // acum_consumption = 0; // Reset acumulative values
        // acum_harvest = 0;
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    }
#endif /*ADAPTIVE_PACKET_CREATION*/
    PROCESS_END();

}



PROCESS_THREAD(staffetta_test, ev, data){
    PROCESS_BEGIN();

    static struct etimer et;
    int staffetta_result;
    uint32_t Tw;

    // leds_init();
    staffetta_init();
    random_init(node_id);
    watchdog_stop();
    uint32_t timer_on, timer_off;
    //#if ENERGY_HARV
    PROCESS_EXITHANDLER(energytrace_stop();)
    energytrace_start();
    //#endif /*WITH_ENERGY_HARV*/

    process_start(&staffetta_print_stats_process, NULL);
    while(1){
        //Compute Tw
#if NEW_EDC
        if (node_energy_state == NS_LOW){
            Tw = (CLOCK_SECOND / WAKE_UP_PERIOD);
        } else if (node_energy_state == NS_MID) {
            Tw = (CLOCK_SECOND / (WAKE_UP_PERIOD + 5) );
        } else if (node_energy_state == NS_HIGH) {
            Tw = (CLOCK_SECOND / (WAKE_UP_PERIOD + 10));
        } else {
            Tw = (CLOCK_SECOND / WAKE_UP_PERIOD);
        }
#else
        Tw = (CLOCK_SECOND / WAKE_UP_PERIOD);
#endif
        //Add some randomness
        // etimer_set(&et,((Tw*3)/4) + (random_rand()%(Tw/2)));
        etimer_set(&et,((Tw)/4) + (random_rand()%(Tw*3/4)));

        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        //Perform a data exchange

#if ENERGY_HARV
        if (node_energy_state != NS_ZERO){
            timer_on = RTIMER_NOW();
            staffetta_result = staffetta_send_packet();
            timer_off = RTIMER_NOW();
            // TODO Mod timers from ticks to seconds

            printf("9|%lu|%lu\n",timer_on,timer_off); //Notify when a node goes to sleep
        } else { // Node did not have enough energy to operate
            printf("5\n");
        }
#else
        if (node_energy_state != NS_ZERO){
            timer_on = RTIMER_NOW();
            staffetta_result = staffetta_send_packet();
            timer_off = RTIMER_NOW();

            // printf("9|%lu|%lu\n",timer_on,timer_off); //Notify when a node goes to sleep

        } else { // Node did not have enough energy to operate
            // printf("5\n");
        }
#endif /*ENERGY_HARV*/
    }

    PROCESS_END();
}
