#ifndef TASK_IPC_H
#define TASK_IPC_H

#include <zmq.h>
#include <glib.h>
#include "task.h"

#define MAX_TASK_JSON_IN  1024
#define MAX_TASK_JSON_OUT 1024

/* ZeroMQ IPC endpoints (unix domain sockets on /dev/shm) */
#define ZMQ_LEADER_PULL_ENDPOINT  "ipc:///dev/shm/leader_pull.sock"
#define ZMQ_LEADER_PUB_ENDPOINT   "ipc:///dev/shm/leader_pub.sock"

/* --- Data Structures & Enums --- */

/* Message types identifier */
typedef enum {
    MSG_TASK_REQUEST = 0,
    MSG_TASK_RESULT,
    MSG_TASK_READY,
    MSG_TASK_SYNC,
    MSG_TASK_ABORT
} msg_type_t;

typedef struct {
    sched_policy_t policy;
    gint8 priority;
    gint cpu_affinity;
    guint8 repetition;
    gchar input_data[MAX_TASK_JSON_IN];
} task_request_t;

typedef gchar task_outcome_t;



/* Main IPC Message Structure */
typedef struct {
    msg_type_t type;
    guint16  task_id;
    /* Timestamps for measures the performance */
    glong start_time_request;
    glong end_time_request;
    glong start_time_result; 
    /*-----------------------------------------*/
    union {
        task_request_t task_request;                /* Payload for TASK_REQUEST */
        task_outcome_t result[MAX_TASK_JSON_OUT];    /* Payload for TASK_RESULT */
        gint64 sync_time_us;
    } data;
} ipc_msg_t;



#endif /* TASK_IPC_H */
