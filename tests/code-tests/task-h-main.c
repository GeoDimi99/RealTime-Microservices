#include <stdio.h>
#include <glib.h>
#include "task.h"

/**
 * Helper function to print task details to the console.
 * It demonstrates how to retrieve data from GString and GSList.
 */
void print_task_info(const task_t *task) {
    if (!task) {
        printf("Task reference is NULL\n");
        return;
    }
    
    printf("--- Task Report: %s ---\n", task->task_name->str);
    printf("ID         : %u\n", task->task_id);
    printf("Policy     : %d\n", task->policy);
    printf("Priority   : %d (Range: %d to %d)\n", 
            task->priority, MIN_TASK_PRIORITY, MAX_TASK_PRIORITY);
    printf("Repetition : %u\n", task->repetition);
    printf("Timing     : Start at %ld, End at %ld\n", task->start_time, task->end_time);
    printf("Input JSON : %s\n", task->input->str);
    
    printf("Dependencies: ");
    if (!task->depends_on) {
        printf("None\n");
    } else {
        for (GSList *iter = task->depends_on; iter != NULL; iter = iter->next) {
            // Using GPOINTER_TO_UINT because IDs are stored directly in the pointer
            guint16 dep_id = (guint16) GPOINTER_TO_UINT(iter->data);
            printf("[%u] ", dep_id);
        }
        printf("\n");
    }
    printf("--------------------------------------\n\n");
}

int main(int argc, char *argv[]) {
    printf("========================================\n");
    printf("   TASK MANAGEMENT INTERFACE TESTER     \n");
    printf("========================================\n\n");

    /* TEST 1: Standard Creation */
    printf("[Test 1] Creating a standard valid task...\n");
    task_t *t1 = task_new(101, "ImageProcessor", SCHED_POLICY_FIFO, 10, 1, 0, 1000, "{\"res\": \"1080p\"}");
    if (t1) {
        printf("Success: t1 created.\n");
        print_task_info(t1);
    }

    /* TEST 2: Adding Dependencies */
    printf("[Test 2] Testing dependency injection (using GUINT_TO_POINTER)...\n");
    if (t1) {
        task_add_dependency(t1, 50);
        task_add_dependency(t1, 60);
        task_add_dependency(t1, 70);
        print_task_info(t1);
    }

    /* TEST 3: Edge Case - Null Input & Minimum Priority */
    printf("[Test 3] Creating task with NULL input and minimum priority...\n");
    task_t *t2 = task_new(202, "BackgroundService", SCHED_POLICY_OTHER, MIN_TASK_PRIORITY, 5, 10, 50, NULL);
    if (t2) {
        printf("Success: t2 created (Input defaulted to empty string).\n");
        print_task_info(t2);
    }

    /* TEST 4: Boundary Check - Invalid Priority */
    // GLib will trigger a CRITICAL warning for this test.
    printf("[Test 4] Validation: Testing priority upper bound (100)...\n");
    task_t *t_fail_pri = task_new(303, "InvalidTask", SCHED_POLICY_RR, 100, 1, 0, 100, "");
    if (!t_fail_pri) {
        printf("Confirmed: Constructor rejected priority > %d.\n\n", MAX_TASK_PRIORITY);
    }

    /* TEST 5: Boundary Check - Timing logic */
    printf("[Test 5] Validation: Testing start_time >= end_time...\n");
    task_t *t_fail_time = task_new(404, "LateTask", SCHED_POLICY_DEADLINE, 50, 1, 500, 499, "");
    if (!t_fail_time) {
        printf("Confirmed: Constructor rejected invalid time sequence.\n\n");
    }

    /* CLEANUP */
    printf("Terminating and cleaning up memory...\n");
    if (t1) task_free(t1);
    if (t2) task_free(t2);

    printf("Test Suite Finished Successfully.\n");
    return 0;
}