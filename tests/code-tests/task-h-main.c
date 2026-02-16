#include <stdio.h>
#include <glib.h>
#include "task.h"

/**
 * Helper function to print task details to the console.
 */
void print_task_info(const task_t *task) {
    if (!task) {
        printf("Task is NULL\n");
        return;
    }
    
    printf("--- Task Report ---\n");
    printf("Task ID    : %u\n", task->task_id);
    printf("Name       : %s\n", task->task_name->str);
    printf("Policy     : %d\n", task->policy);
    printf("Priority   : %d\n", task->priority);
    printf("Repetition : %u\n", task->repetition);
    printf("Time Frame : [%ld to %ld]\n", task->start_time, task->end_time);
    printf("Input Data : %s\n", task->input->str);
    
    printf("Depends On : ");
    if (!task->depends_on) {
        printf("None\n");
    } else {
        for (GSList *iter = task->depends_on; iter != NULL; iter = iter->next) {
            // Dereferencing the guint16 pointer stored in the list
            guint16 *id = (guint16 *)iter->data;
            printf("%u ", *id);
        }
        printf("\n");
    }
    printf("-------------------\n\n");
}

int main(int argc, char *argv[]) {
    printf("=== Task Management System Test Suite ===\n\n");

    /* * TEST 1: Successful Task Creation 
     */
    printf("[Test 1] Creating a valid task...\n");
    task_t *t1 = task_new(101, "DataCompressor", SCHED_POLICY_FIFO, 15, 1, 0, 5000, "{\"codec\": \"lz4\"}");
    if (t1) {
        printf("Success: Task t1 created.\n");
        print_task_info(t1);
    }

    /* * TEST 2: Dependency Logic 
     */
    printf("[Test 2] Adding dependencies to t1...\n");
    task_add_dependency(t1, 10);
    task_add_dependency(t1, 20);
    print_task_info(t1);

    /* * TEST 3: Null Input Handling 
     */
    printf("[Test 3] Creating task with NULL input (defaulting to empty string)...\n");
    task_t *t2 = task_new(102, "LogCleaner", SCHED_POLICY_OTHER, -5, 2, 100, 200, NULL);
    if (t2) {
        printf("Success: Task t2 created with null-safe input handling.\n");
        print_task_info(t2);
    }

    /* * TEST 4: Boundary Validation (Priority) 
     * Note: GLib will print a 'CRITICAL' message to the terminal here.
     */
    printf("[Test 4] Validation check: Priority out of bounds (120)...\n");
    task_t *t_invalid_pri = task_new(999, "BadPriority", SCHED_POLICY_RR, 120, 1, 0, 100, "test");
    if (!t_invalid_pri) {
        printf("Confirmed: Constructor correctly rejected invalid priority.\n\n");
    }

    /* * TEST 5: Boundary Validation (Timing) 
     */
    printf("[Test 5] Validation check: Start time after End time...\n");
    task_t *t_invalid_time = task_new(888, "BadTime", SCHED_POLICY_DEADLINE, 50, 1, 500, 200, "");
    if (!t_invalid_time) {
        printf("Confirmed: Constructor correctly rejected invalid time frame.\n\n");
    }

    /* * CLEANUP 
     */
    printf("Cleaning up memory...\n");
    if (t1) task_free(t1);
    if (t2) task_free(t2);

    printf("Test Suite Finished.\n");

    return 0;
}