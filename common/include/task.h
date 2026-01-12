#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_NAME_MAX        64
#define TASK_JSON_IN_MAX     512
#define TASK_JSON_OUT_MAX    512

#define TASK_PRIORITY_MIN 1
#define TASK_PRIORITY_MAX 99

/* Scheduling Policies */
typedef enum {
    SCHED_POLICY_OTHER = 0,
    SCHED_POLICY_FIFO  = 1,
    SCHED_POLICY_RR    = 2
} sched_policy_t;

/* Task struct */
typedef struct {
    char task_name[TASK_NAME_MAX];
    sched_policy_t policy;
    uint8_t priority;  /* 1–99 */
    char input[TASK_JSON_IN_MAX];
    char output[TASK_JSON_OUT_MAX];
} task_t;

#endif /* TASK_H */
