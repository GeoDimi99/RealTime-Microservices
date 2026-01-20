#ifndef EXECUTION_MANAGER_H
#define EXECUTION_MANAGER_H

#include <string.h>
#include "logger.h"
#include "task_ipc.h"
#include "schedule.h"


#define MAX_EXEXECUTION_MANAGER_NAME 64
#define DEFAULT_EXECUTION_MANAGER_NAME "execution-manager"

/* Execution Manager Descriptor */
typedef struct {
    /* Static configuration */
    char execution_manager_name[MAX_EXEXECUTION_MANAGER_NAME];
    char rx_queue_name[MAX_QUEUE_NAME];     // Listening queue (EM Queue)

    /* Runtime state */
    schedule_t schedule;                                            // Current schedule
    char tx_queues_name[MAX_TASKS_PER_SCHEDULE][MAX_QUEUE_NAME];    // Sending queues (Tasks Queues)
    mqd_t rx_fd;                                                    // Input queue descriptor
    mqd_t tx_fd[MAX_TASKS_PER_SCHEDULE];                            // Output queues descriptors  

} execution_manager_t;


int init_execution_manager(execution_manager_t* svc, const char* name, const char* rx_q);
int set_execution_manager_schedule(execution_manager_t* svc, schedule_t* schedule);
int delete_execution_manager_schedule(execution_manager_t* svc);
void close_execution_manager(execution_manager_t* svc);


#endif // EXECUTION_MANAGER_H
