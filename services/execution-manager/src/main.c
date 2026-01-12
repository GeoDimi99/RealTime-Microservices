#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "task_ipc.h"

mqd_t em_queue, task_queue;

void send_task_request(uint32_t task_id, task_t *task) {
    ipc_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type    = MSG_TASK_REQUEST;
    msg.task_id = task_id;
    msg.status  = ACK_OK;

    memcpy(msg.payload, task, sizeof(task_t));
    send_message(task_queue, &msg);
}

void get_status(uint32_t task_id) {
    ipc_msg_t msg = {0};
    msg.type    = MSG_GET_STATUS;
    msg.task_id = task_id;
    send_message(task_queue, &msg);
}

void get_results(uint32_t task_id) {
    ipc_msg_t msg = {0};
    msg.type    = MSG_GET_RESULTS;
    msg.task_id = task_id;
    send_message(task_queue, &msg);
}

int main() {
    em_queue   = create_queue(QUEUE_NAME_EM);
    task_queue = create_queue(QUEUE_NAME_TASK);

    // Example: send a task
    task_t task;
    strncpy(task.task_name, "compute_sum", TASK_NAME_MAX);
    task.policy   = SCHED_POLICY_FIFO;
    task.priority = 50;
    snprintf(task.input, TASK_JSON_IN_MAX, "{\"a\": 3, \"b\": 4}");
    snprintf(task.output, TASK_JSON_OUT_MAX, "{}");

    printf("[Execution Manager] Sending task request...\n");
    send_task_request(1, &task);

    while (1) {
        ipc_msg_t msg;
        receive_message(em_queue, &msg);

        switch (msg.type) {
            case MSG_TASK_ACK:
                printf("[Execution Manager] ACK: %s\n", msg.payload);
                break;
            case MSG_TASK_STATUS:
                printf("[Execution Manager] STATUS: %s\n", msg.payload);
                break;
            case MSG_TASK_RESULT:
                printf("[Execution Manager] RESULT: %s\n", msg.payload);
                break;
            default:
                printf("[Execution Manager] Unknown message type %d\n", msg.type);
        }
    }

    close_queue(em_queue, QUEUE_NAME_EM);
    close_queue(task_queue, QUEUE_NAME_TASK);
    return 0;
}
