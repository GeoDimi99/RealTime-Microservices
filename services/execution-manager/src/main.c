#include <stdio.h>
#include <glib.h>
#include "schedule.h"
#include "execution_manager.h"

int main(int argc, char *argv[]) {

    /* ------ Init Execution Manager ------ */
    int exit_code = 0;
    execution_manager_t *em = em_new(DEFAULT_EXECUTION_MANAGER_NAME);

    
    schedule_t *sched = schedule_new("empty_scheduler", NULL);
    if(!sched){
        g_error("[ERROR] Execution Manager (empty_scheduler) : empty_scheduler creation failed.");
    }
    print_schedule(sched);




    /* ------ Wait for schedule ------ */
    gboolean is_set_new_schedule = TRUE;


    /* ------ Stat Execution Manager ------ */
    while(TRUE){

        /* Get the schedule version from cache */

        /* Compare versions and set is_set_new_schedule */

        if(is_set_new_schedule){
            is_set_new_schedule = FALSE;
            schedule_free(sched);

            /* Read the new schedule */
            gchar * schedule_name = "schedule";
            sched = schedule_new(schedule_name, "0.0.1");
            if(!sched){
                g_error("[ERROR] Execution Manager (%s) : scheduler creation failed.", schedule_name);
            }

            schedule_add_task(sched, 1, "sum",
                  SCHED_POLICY_FIFO,
                  10, 1,
                  NULL,
                  1*1000, 2*1000,
                  "{\"a\":10, \"b\":5}");

            schedule_add_task(sched, 2, "subtract",
                  SCHED_POLICY_FIFO,
                  8, 1,
                  NULL,
                  1*1000, 7*1000,
                  "{\"a\":20, \"b\":8}");

            schedule_add_task(sched, 3, "multiply",
                  SCHED_POLICY_FIFO,
                  6, 1,
                  NULL,
                  2*1000, 7*1000,
                  "{\"a\":4, \"b\":7}");


            print_schedule(sched);
            //goto CLEANUP;

        }

        /* Run the schedule */
        em_run_schedule(sched);
        //goto CLEANUP;

    }



CLEANUP:
    em_free(em);
    schedule_free(sched);
    return exit_code;
}
