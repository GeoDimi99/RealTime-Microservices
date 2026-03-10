#include <stdio.h>
#include <glib.h>
#include <signal.h> // For signals
#include <unistd.h> // For sleep()
#include "schedule.h"
#include "execution_manager.h"


/* Gloabal flag for check the main loop */
static volatile gboolean keep_running = TRUE;


/* Signal Handler for SIGINT (Ctrl+C) */
void int_handler(int dummy) {
    (void)dummy; 
    g_print("[INFO] Execution Manager: SIGINT received.\n");
    keep_running = FALSE;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv; 

    /* Registre the signal handler for a clean clousure */
    signal(SIGINT, int_handler);

    /* ------ Init Execution Manager ------ */
    g_print("=========== Execution Manager Init ========\n");
    int exit_code = 0;
    execution_manager_t *em = em_new(DEFAULT_EXECUTION_MANAGER_NAME);
    if (!em) {
        g_error("[ERROR] Execution Manager: em_new failed.");
        return 1;
    }
    
    schedule_t *sched = NULL; // Init to NULL
    gboolean is_set_new_schedule = TRUE;


    g_print("====== Execution Manager Initialized ======\n");
    g_print("==== Execution Manager Start Main Loop ====\n");
    g_print("Click Ctrl+C for a clean exit.\n\n");


    /* -------------- Main Loop Execution -------------- */
    while (keep_running) {
        
        
        if (is_set_new_schedule) {
            //is_set_new_schedule = FALSE;

            if (sched != NULL) {
                schedule_free(sched);
                sched = NULL;   // Avoid double-free at exit 
            }
        
            /* Create a schedule */
            gchar *schedule_name = "schedule";
            sched = schedule_new(schedule_name, "0.0.1");
            if (!sched) {
                g_error("[ERROR] Execution Manager (%s) : scheduler creation failed.", schedule_name);
            }

            schedule_add_task(sched, 1, "sum", SCHED_POLICY_FIFO, 10, 1, 1, NULL,
                            1 * 1000, 2 * 1000, "[{\"a\":10, \"b\":5}]");

            //schedule_add_task(sched, 2, "subtract", SCHED_POLICY_FIFO, 8, 1, 1, NULL,
            //                  1 * 1000, 7 * 1000, "[{\"a\":20, \"b\":8}]");

            //schedule_add_task(sched, 3, "multiply", SCHED_POLICY_FIFO, 6, 1, 1, NULL,
            //                  2 * 1000, 7 * 1000, "[{\"a\":4, \"b\":7}]");

            schedule_print(sched);
        }

        /* Run the schedule */
        em_run_schedule(em, sched);
        


        if (keep_running) {
            g_print("\n[INFO] Execution Manager: Schedule Completed. Reboot in 5 seconds... (or push Ctrl+C for exit)...\n\n");
            
            /* Slee for 5 second, but check flag each second */
            for (int i = 0; i < 5 && keep_running; i++) {
                sleep(1);
            }
        }

        // schedule_reset(sched); // DO NOT reset a schedule that is about to be freed. This causes memory corruption.
    }

    
    g_print("==== Execution Manager Exit Main Loop ====\n");
    g_print("[INFO] Execution Manager: Exit from the main loop. Cleanup ...\n");

    if (em) em_free(em);
    if (sched) schedule_free(sched);
    
    g_print("[INFO] Execution Manager: Cleanup completed.\n");
    g_print("==== Execution Manager Fish ====\n");
    return exit_code;
}