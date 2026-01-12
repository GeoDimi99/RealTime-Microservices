#include <stdio.h>
#include <string.h>

#include "task.h"
#include "logger.h"

int main(void) {
    task_t task;

    /* Fill task fields */
    strncpy(task.task_name, "test_task", TASK_NAME_MAX);
    task.policy = SCHED_POLICY_FIFO;
    task.priority = 50;

    strncpy(task.input, "{\"a\": 3, \"b\": 4}", TASK_JSON_IN_MAX);
    strncpy(task.output, "{\"sum\": 7, \"product\": 12}", TASK_JSON_OUT_MAX);

    /* Print task info */
    printf("Task name   : %s\n", task.task_name);
    printf("Policy      : %d\n", task.policy);
    printf("Priority    : %u\n", task.priority);
    printf("Input JSON  : %s\n", task.input);
    printf("Output JSON : %s\n", task.output);

    log_message(LOG_INFO, "__main__", "Task: %s, policy=%s, priority=%d", 
                "init_core", "fifo", 30);
    log_message(LOG_WARN, "task_service", "Task execution delayed!");
    log_message(LOG_ERROR, "rt_task", "Thread creation failed with code %d", -1);

    return 0;
}
