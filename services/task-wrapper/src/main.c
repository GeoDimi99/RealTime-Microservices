#include <glib.h>
#include <stdlib.h>
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

    /* Set main process to real-time FIFO (higher than task threads) */
    struct sched_param rt_param;
    rt_param.sched_priority = 80;
    if (sched_setscheduler(0, SCHED_FIFO, &rt_param) == -1) {
        g_warning("[WARNING] Execution Manager: sched_setscheduler failed: %m (running without RT priority)");
    } else {
        g_print("[INFO] Execution Manager: Main process set to SCHED_FIFO priority 80\n");
    }

    /* Registre the signal handler for a clean clousure */
    signal(SIGINT, int_handler);

    /* ------ Init Execution Manager ------ */
    int exit_code = 0;
    char *env_task = getenv("TASK_NAME");
    
    gchar *em_name = g_strdup_printf("%s", env_task ? env_task : DEFAULT_EXECUTION_MANAGER_NAME);

    execution_manager_t *em = em_new(em_name);
    if (!em) {
        g_error("[ERROR] Execution Manager: em_new failed.");
        return 1;
    }
    
    schedule_t *sched = NULL; // Init to NULL

    g_print("[INFO] Execution Manager: Initialized (Click Ctrl+C for a clean exit)\n");

    /* Control for new schedule */
    em_wait_for_schedule(em);
    gboolean is_set_new_schedule = TRUE;


    /* -------------- Main Loop Execution -------------- */
    //while (keep_running) {

    gint max_iterations = em_get_iterations(em);
    g_print("[INFO] Execution Manager: Running %d iteration(s)\n", max_iterations);

    for(int i=0; i < max_iterations && keep_running; i++){
        iteration = i;
        
        if (is_set_new_schedule){

            /* Set to false for the next iteration*/
            is_set_new_schedule = FALSE;

            if (sched != NULL) {
                schedule_free(sched);
                sched = NULL;   // Avoid double-free at exit 
            }
             sched = em_read_schedule(em);


            schedule_print(sched);
        } else {
            schedule_reset(sched);
        }

        /* Run the schedule */
        g_print("\n[INFO] Execution Manager: Start iteration %d, progress percentage %d %% \n",i, (i*100)/max_iterations);
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