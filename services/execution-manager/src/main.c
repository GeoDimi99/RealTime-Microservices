#include <glib.h>
#include <signal.h> // For signals
#include <unistd.h> // For sleep()
#include <sched.h>
#include <sys/mman.h>

#include "schedule.h"
#include "execution_manager.h"
#include "app_task.h"




/* Gloabal flag for check the main loop */
volatile gboolean keep_running = TRUE;


/* Signal Handler for SIGINT (Ctrl+C) */
void int_handler(int dummy) {
    (void)dummy; 
    g_print("\n[INFO] Execution Manager: SIGINT received.\n");
    keep_running = FALSE;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;


    /* Lock memory */
    if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
        g_error("[ERROR] Execution Manager: mlockall failed: %m\n");
        return 1;
    }

    /* Registre the signal handler for a clean clousure */
    signal(SIGINT, int_handler);

    /* ------ Init Execution Manager ------ */
    int exit_code = 0;
    execution_manager_t *em = em_new(DEFAULT_EXECUTION_MANAGER_NAME);
    if (!em) {
        g_error("[ERROR] Execution Manager: em_new failed.");
        return 1;
    }
    
    schedule_t *sched = NULL; // Init to NULL

    g_print("=== Execution Manager Initialized ===\n");
    g_print("Click Ctrl+C for a clean exit.\n\n");

    /* Control for new schedule */
    gboolean is_set_new_schedule = TRUE;


    /* -------------- Main Loop Execution -------------- */
    //while (keep_running) {

    for(int i=0; i < 25 && keep_running; i++){     // For test 
        iteration = i;
        
        if (is_set_new_schedule){

            /* Set to false for the next iteration*/
            is_set_new_schedule = FALSE;

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

            input_t *func_input = g_new0(input_t, 1);
            func_input->total_ops = 4;
            func_input->io_percentage = 0;



            //void schedule_add_task(schedule_t *sched, guint16 id, const gchar *name, GThreadFunc task_exec, gint policy, gint8 priority, gint cpu_affinity, guint8 repetition, GSList *depends_on,  gint64 start_time, gint64 end_time, gpointer input);
            schedule_add_task(sched, 1, "stress_task_1", task_main, SCHED_FIFO, 1, 1, 1, NULL, 1000, 1100, func_input);
            schedule_add_task(sched, 2, "stress_task_2", task_main, SCHED_FIFO, 1, 1, 1, NULL, 1040, 1120, func_input);


            schedule_print(sched);
        } else {
            schedule_reset(sched);
        }

        /* Run the schedule */
        g_print("\n[INFO] Execution Manager: Start iteration %d, progress percentage %d %% \n",i, (i*100)/25);
        em_run_schedule(em, sched);
        

        //if (keep_running) {
            //g_print("\n[INFO] Execution Manager: Schedule Completed. Reboot in 5 seconds... (or push Ctrl+C for exit)...\n\n");
            
            /* Slee for 5 second, but check flag each second */
            //for (int i = 0; i < 5 && keep_running; i++) {
                //sleep(1);
            //}
        //}
    }

    g_print("\n[INFO] Execution Manager: Exit from the main loop. Cleanup ...\n");

    if (em) em_free(em);
    if (sched) schedule_free(sched);
    
    g_print("[INFO] Execution Manager: Cleanup completed.\n");
    return exit_code;
}