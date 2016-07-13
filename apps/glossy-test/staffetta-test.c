#include "staffetta.h"
#include "node-id.h"
#include "../../apps/energytrace/energytrace.h"
#include "../../core/dev/metric.h"

static uint8_t round_stats;
static int loop_stats;
uint32_t sleep_reference = 0;
static uint32_t time_counter = 0;
static uint32_t old_Tw = 0;

PROCESS(staffetta_test, "Staffetta test");
AUTOSTART_PROCESSES(&staffetta_test);

PROCESS(staffetta_print_stats_process, "Staffetta print stats");

PROCESS_THREAD(staffetta_print_stats_process, ev, data){
    PROCESS_BEGIN();

    static struct etimer et;

    round_stats = PAKETS_PER_NODE;
    loop_stats = node_id;
    static uint8_t counter = 0;
    static uint8_t data_counter = 0;
    static uint8_t gen_data;

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

    gen_data = random_rand()%12 + 12;
    etimer_set(&et,CLOCK_SECOND*1+(random_rand()%(CLOCK_SECOND*10)));
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    while(1) {
        staffetta_print_stats();
        counter = counter + 1;
        if (counter >= 2){
            printf("6|%d|%lu|%lu|%lu|%lu\n", node_energy_state, remaining_energy, harvesting_rate, acum_consumption, acum_harvest);
            acum_consumption = 0; // Reset acumulative values
            acum_harvest = 0;
            counter = 0;
        }
#if NEW_EDC
        data_counter++;
        if (data_counter >= gen_data) {
            staffetta_add_data(round_stats++);
            gen_data = random_rand()%12 + 12;
            data_counter = 0;
        }
#else //For the original ORW we generate packets periodically every 5 seconds to "maintain" the link quality
        staffetta_add_data(round_stats++);
#endif
        etimer_set(&et,CLOCK_SECOND*5);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    }
#endif /*ADAPTIVE_PACKET_CREATION*/
    PROCESS_END();

}

static uint32_t get_next_wakeup(uint32_t sleep_reference){
    uint32_t random_increment;
    uint32_t Tw;
    random_increment = random_rand()%1000; // Get randomized wakeup point in ms
    Tw = (1000 * time_counter) + random_increment + (node_duty_cycle*10) - (old_Tw);
    old_Tw = (1000 * time_counter) + random_increment;
    time_counter++;
    return (uint32_t) Tw;
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

        Tw = get_next_wakeup(sleep_t);
        // printf("22|%lu|%lu\n",Tw, sleep_t );
        Tw = (CLOCK_SECOND * Tw) / 1000;
        // etimer_set(&et,Tw);
        etimer_set(&et,Tw);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        // printf("second\n");
        //Perform a data exchange

#if ENERGY_HARV
        if (node_energy_state != NS_ZERO){
            timer_on = RTIMER_NOW();
            staffetta_result = staffetta_main();
            timer_off = RTIMER_NOW();
            printf("9|%lu|%lu\n",timer_on,timer_off); //Notify when a node goes to sleep
        } else { // Node did not have enough energy to operate
            printf("5\n");
        }
#else
        if (node_energy_state != NS_ZERO){
            timer_on = RTIMER_NOW();
            staffetta_result = staffetta_main();
            timer_off = RTIMER_NOW();
            printf("9|%lu|%lu\n",timer_on,timer_off); //Notify when a node goes to sleep
        } else { // Node did not have enough energy to operate
            printf("5\n");
        }
#endif /*ENERGY_HARV*/
    }

    PROCESS_END();
}
