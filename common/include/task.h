#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <string.h>

#define MAX_TASK_NAME        64
#define MAX_TASK_JSON_IN     512
#define MAX_TASK_JSON_OUT    512

#define MIN_TASK_PRIORITY 0
#define MAX_TASK_PRIORITY 99

/* Scheduling Policies */
typedef enum {
    SCHED_POLICY_OTHER = 0,
    SCHED_POLICY_FIFO  = 1,
    SCHED_POLICY_RR    = 2
} sched_policy_t;

/* Task struct */
typedef struct {
    char task_name[MAX_TASK_NAME];
    sched_policy_t policy;
    uint8_t priority;  /* 0–99 */
    char input[MAX_TASK_JSON_IN];
    char output[MAX_TASK_JSON_OUT];
} task_t;

int init_task(task_t* task, const char* name, sched_policy_t policy, uint8_t priority, const char* input, const char* output);


#endif /* TASK_H */
