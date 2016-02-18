#include "staffetta.h"
#include "node-id.h"
#include "../../apps/energytrace/energytrace.h"
#include "../../core/dev/metric.h"

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

    //etimer_set(&et,random_rand()%(CLOCK_SECOND*5));
    etimer_set(&et,CLOCK_SECOND*25+(random_rand()%(CLOCK_SECOND*10)));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    while(1) {
        staffetta_print_stats();

        staffetta_add_data(round_stats++);
//        #if ENERGY_HARV
        printf("6|%d|%ld|%d\n", node_energy_state, remaining_energy, harvesting_rate);
//        #endif /*ENERGY_HARV*/
        etimer_set(&et,CLOCK_SECOND*10);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    }

    PROCESS_END();
}



PROCESS_THREAD(staffetta_test, ev, data){
    PROCESS_BEGIN();

    static struct etimer et;
    int staffetta_result;
    uint32_t wakeups,Tw;

    leds_init();
    leds_on(LEDS_GREEN);
    staffetta_init() ;
    random_init(node_id);
    watchdog_stop();
    leds_off(LEDS_GREEN);

//    #if ENERGY_HARV
    PROCESS_EXITHANDLER(energytrace_stop();)
    energytrace_start();
//    #endif /*WITH_ENERGY_HARV*/

    process_start(&staffetta_print_stats_process, NULL);
    while(1){
        //Get wakeups/period from Staffetta
        wakeups = getWakeups();
        //Compute Tw
        Tw = ((CLOCK_SECOND*(10*BUDGET_PRECISION))/wakeups);
        //Add some randomness
        etimer_set(&et,((Tw*3)/4) + (random_rand()%(Tw/2)));
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        //Perform a data exchange

        #if ENERGY_HARV
//        printf("6,%d,%ld,%d\n", node_energy_state, remaining_energy, harvesting_rate);
        if (node_energy_state != NS_ZERO){
            staffetta_result = staffetta_send_packet();
        }
        #else
        if (node_energy_state != NS_ZERO){
            staffetta_result = staffetta_send_packet();
        }
        #endif /*ENERGY_HARV*/

        //TODO compute histogram of staffetta results
//        leds_off(LEDS_RED);
    }

    PROCESS_END();
}

