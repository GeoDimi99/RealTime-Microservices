#include <stdio.h>
#include "task_entry.h"

int main() {
    task_main();
    return 0;
}

/********* 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "task_ipc.h"

mqd_t em_queue, task_queue;


void* run_task(void* arg) {
    task_t *task = (task_t*)arg;

    printf("[Task Service] Running task '%s' with priority %u\n", task->task_name, task->priority);
    // Simulate computation
    sleep(1);

    // Send ACK
    ipc_msg_t ack;
    ack.type = MSG_TASK_ACK;
    ack.task_id = 0; // could use task ID
    ack.status = ACK_OK;
    snprintf(ack.payload, MAX_MSG_SIZE, "Task '%s' completed successfully", task->task_name);

    send_message(em_queue, &ack);

    free(task);
    return NULL;
}

int main() {
    em_queue   = create_queue(QUEUE_NAME_EM);
    task_queue = create_queue(QUEUE_NAME_TASK);

    printf("[Task Service] Waiting for messages...\n");

    while (1) {
        ipc_msg_t msg;
        receive_message(task_queue, &msg);

        switch (msg.type) {
            case MSG_TASK_REQUEST: {
                task_t *task = malloc(sizeof(task_t));
                memcpy(task, msg.payload, sizeof(task_t));

                pthread_t thread;
                pthread_create(&thread, NULL, run_task, task);
                pthread_detach(thread);
                break;
            }

            case MSG_GET_STATUS: {
                ipc_msg_t status_msg;
                status_msg.type    = MSG_TASK_STATUS;
                status_msg.task_id = msg.task_id;
                status_msg.status  = ACK_OK;
                snprintf(status_msg.payload, MAX_MSG_SIZE, "Task %u status: running", msg.task_id);
                send_message(em_queue, &status_msg);
                break;
            }

            case MSG_GET_RESULTS: {
                ipc_msg_t result_msg;
                result_msg.type    = MSG_TASK_RESULT;
                result_msg.task_id = msg.task_id;
                result_msg.status  = ACK_OK;
                snprintf(result_msg.payload, MAX_MSG_SIZE, "{\"result\": 42}");
                send_message(em_queue, &result_msg);
                break;
            }

            default:
                printf("[Task Service] Unknown message type %d\n", msg.type);
        }
    }

    close_queue(em_queue, QUEUE_NAME_EM);
    close_queue(task_queue, QUEUE_NAME_TASK);
    return 0;
}

***************/

/* 
#include <stdio.h>
#include <string.h>

#include "task.h"
#include "logger.h"

int main(void) {
    task_t task;

    
    strncpy(task.task_name, "test_task", TASK_NAME_MAX);
    task.policy = SCHED_POLICY_FIFO;
    task.priority = 50;

    strncpy(task.input, "{\"a\": 3, \"b\": 4}", TASK_JSON_IN_MAX);
    strncpy(task.output, "{\"sum\": 7, \"product\": 12}", TASK_JSON_OUT_MAX);

    
    printf("Task name   : %s\n", task.task_name);
    printf("Policy      : %d\n", task.policy);
    printf("Priority    : %u\n", task.priority);
    printf("Input JSON  : %s\n", task.input);
    printf("Output JSON : %s\n", task.output);

    l 
    log_message(LOG_WARN, "task_service", "Task execution delayed!");
    log_message(LOG_ERROR, "rt_task", "Thread creation failed with code %d", -1);

    return 0;
}

  */