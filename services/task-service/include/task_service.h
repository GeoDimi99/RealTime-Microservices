#ifndef TASK_SERVICE_H
#define TASK_SERVICE_H

#include <stdint.h>
#include <pthread.h>

/* Possible states */
typedef enum { IDLE, RUNNING, COMPLETED, FAULT } task_state_t;

/* Task descriptor */
typedef struct {
    pthread_t thread;
    pthread_attr_t attr;
    task_state_t state;
    int priority;
} task_service_t;


#endif /* TASK_SERVICE_H */
