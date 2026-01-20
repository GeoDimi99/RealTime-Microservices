#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <string.h>
#include "task.h"

#define MAX_SCHEDULE_NAME 64
#define MAX_SCHEDULE_VERSION 16
#define MAX_TASKS_PER_SCHEDULE 32

/* Schedule Descriptor */
typedef struct {
    char schedule_name[MAX_SCHEDULE_NAME];
    char schedule_version[MAX_SCHEDULE_VERSION];
    task_t tasks[MAX_TASKS_PER_SCHEDULE];
    int num_tasks;
} schedule_t;

int init_schedule(schedule_t* sched, const char* name, const char* version); 
int add_task_to_schedule(schedule_t* sched, task_t task);

#endif // SCHEDULE_H
