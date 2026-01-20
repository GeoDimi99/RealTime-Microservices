#include <stdio.h>
#include "task.h"

/**
 * Initializes a task structure with the provided parameters.
 */
int init_task(task_t* task,
              const char* name,
              sched_policy_t policy,
              uint8_t priority,
              const char* input,
              const char* output)
{
    /* Validate that the task pointer is not NULL */
    if (!task) 
        return -1;

    /* Validate that the priority is within allowed range */
    if (priority > MAX_TASK_PRIORITY) 
        return -1;

    /* Validate that the scheduling policy is one of the defined values */
    if (policy < SCHED_POLICY_OTHER || policy > SCHED_POLICY_RR) 
        return -1;

    /* Copy the task name, ensuring it fits in the fixed-size buffer */
    snprintf(task->task_name, MAX_TASK_NAME, "%s", name);

    /* Initialize scheduling parameters */
    task->policy   = policy;
    task->priority = priority;

    /* Copy the input buffer, truncated if too long */
    snprintf(task->input, MAX_TASK_JSON_IN, "%s", input);

    /* Copy the output buffer, truncated if too long */
    snprintf(task->output, MAX_TASK_JSON_OUT, "%s", output);

    /* Initialization successful */
    return 0;
}
