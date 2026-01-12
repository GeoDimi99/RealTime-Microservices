#ifndef TASK_IPC_H
#define TASK_IPC_H

#include <stdint.h>
#include <mqueue.h>
#include "task.h"

/* Queue names */
#define QUEUE_NAME_TASK "/task_queue"
#define QUEUE_NAME_EM   "/execution_manager_queue"

/* Message types */
typedef enum {
    MSG_TASK_REQUEST = 1,
    MSG_TASK_ACK,
    MSG_GET_STATUS,
    MSG_TASK_STATUS,
    MSG_GET_RESULTS,
    MSG_TASK_RESULT
} msg_type_t;

/* ACK status */
typedef enum {
    ACK_OK = 0,
    ACK_ERROR = 1
} ack_status_t;

/* Task-service states */
typedef enum {
    TS_IDLE = 0,
    TS_RUNNING,
    TS_COMPLETED
} task_service_state_t;

/* IPC message */
typedef struct {
    msg_type_t type;
    uint32_t   task_id;

    union {
        task_t task;                  /* For TASK_REQUEST */
        ack_status_t ack;              /* For TASK_ACK */
        task_service_state_t status;   /* For STATUS */
        char result[TASK_JSON_OUT_MAX];/* For TASK_RESULT */
    } data;
} ipc_msg_t;

/* POSIX MQ helpers */
mqd_t create_queue(const char *name, int flags);
int send_message(mqd_t qd, const ipc_msg_t *msg);
int receive_message(mqd_t qd, ipc_msg_t *msg);

#endif /* TASK_IPC_H */
