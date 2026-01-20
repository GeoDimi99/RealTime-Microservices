#include "execution_manager.h"

int init_execution_manager(execution_manager_t* svc, const char* name, const char* rx_q){
    if (!svc) return -1;

    /* Initialize context strings */
    strncpy(svc->execution_manager_name, name, MAX_TASK_NAME);
    strncpy(svc->rx_queue_name, rx_q, MAX_QUEUE_NAME);

    /* Setup RX Queue (Service Input) */
    /* Destroy the queue if it was left over from a previous crash. This ensures we start with an empty queue. */
    destroy_queue(svc->rx_queue_name);
    svc->rx_fd = create_queue(svc->rx_queue_name, O_RDONLY);
    
    log_message(LOG_INFO, svc->execution_manager_name , "RX Queue initialized: %s\n", svc->rx_queue_name);
    
    return 0;
}


/**
 * Assigns a schedule to the execution manager and initializes TX queues for all tasks.
 */
int set_execution_manager_schedule(execution_manager_t* svc, schedule_t* schedule)
{
    /* Validate input pointers */
    if (!svc || !schedule) 
        return -1;

    /* Copy the schedule into the execution manager context */
    svc->schedule = *schedule;

    /* Initialize TX queues for each task in the schedule */
    for (int i = 0; i < schedule->num_tasks; i++) {

        /* Set the queue name for this task */
        snprintf(svc->tx_queues_name[i], MAX_QUEUE_NAME, "/%s", schedule->tasks[i].task_name);

        /* Poll until the TX queue for this task is available */
        while (1) {
            svc->tx_fd[i] = open_queue(svc->tx_queues_name[i], O_WRONLY);

            /* Case A: Queue successfully opened */
            if (svc->tx_fd[i] != (mqd_t)-1) {
                log_message(LOG_INFO, svc->execution_manager_name, 
                            "TX Queue initialized: %s\n", svc->tx_queues_name[i]);
                break; /* Exit polling loop */
            }

            /* Case B: Queue not created yet, wait and retry */
            if (errno == ENOENT) {
                sleep(1);
            } 
            /* Case C: Critical failure (permissions, limits, etc.) */
            else {
                perror("Fatal error connecting to Execution Manager");
                return -1;
            }
        }
    }

    return 0; /* All TX queues successfully initialized */
}


/**
 * Deletes all task queues assigned to the execution manager.
 */
int delete_execution_manager_schedule(execution_manager_t* svc)
{
    /* Validate input pointer */
    if (!svc) 
        return -1;

    /* Clean up all TX queues */
    for (int i = 0; i < svc->schedule.num_tasks; i++) {
        close_queue(svc->tx_fd[i]);
        destroy_queue(svc->tx_queues_name[i]);
    }

    return 0; /* All TX queues successfully deleted */
}


/**
 * Closes the execution manager's RX queue and logs shutdown.
 * Does NOT delete any TX queues or schedules.
 */
void close_execution_manager(execution_manager_t* svc)
{
    if (svc) {
        log_message(LOG_INFO, svc->execution_manager_name, "Shutting down...\n");
        delete_execution_manager_schedule(svc);
        close_queue(svc->rx_fd);
    }
}
