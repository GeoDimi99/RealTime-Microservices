#include "schedule.h"

/**
 * Initializes a schedule structure.
 */
int init_schedule(schedule_t* sched, const char* name, const char* version)
{
    /* Validate input pointer */
    if (!sched) return -1;

    /* Initialize schedule identification fields */
    strncpy(sched->schedule_name, name, MAX_SCHEDULE_NAME);
    strncpy(sched->schedule_version, version, MAX_SCHEDULE_VERSION);

    /* Initialize internal state */
    sched->num_tasks = 0;

    return 0; /* Initialization successful */
}


/**
 * Adds a task to an existing schedule.
 */
int add_task_to_schedule(schedule_t* sched, task_t task)
{
    /* Validate input pointer */
    if (!sched) return -1;

    /* Check capacity limit */
    if (sched->num_tasks >= MAX_TASKS_PER_SCHEDULE)
        return -1;

    /* Add task to the schedule */
    sched->tasks[sched->num_tasks++] = task;

    return 0; /* Task successfully added */
}

