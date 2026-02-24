#include <stdio.h>
#include <glib.h>
#include "schedule.h"

/**
 * Helper to inspect the internal state of the schedule.
 * It iterates through the sorted GQueues for both Start and End timelines.
 */
void print_schedule_report(schedule_t *sched) {
    if (!sched) return;

    printf("\n--- SCHEDULE REPORT: %s (v%s) ---\n", 
           sched->schedule_name->str, sched->schedule_version->str);
    printf("Total Duration: %ld ms\n", (long)sched->schedule_duration);

    // 1. Inspect Activation Timeline (Start Priority Queue)
    printf("\n[Activation Timeline (Sorted GQueue)]:\n");
    for (GList *l = sched->schedule_start_info->head; l != NULL; l = l->next) {
        timeline_entry_t *entry = (timeline_entry_t *)l->data;
        printf("  Time %ld ms: ", (long)entry->timestamp);
        for (GSList *s = entry->data_list; s != NULL; s = s->next) {
            activation_data_t *act = (activation_data_t *)s->data;
            printf("[%s (ID:%u, Prio:%d)] ", act->task_name->str, act->task_id, act->priority);
        }
        printf("\n");
    }

    // 2. Inspect Expiration Timeline (End Priority Queue)
    printf("\n[Expiration Timeline (Sorted GQueue)]:\n");
    for (GList *l = sched->schedule_end_info->head; l != NULL; l = l->next) {
        timeline_entry_t *entry = (timeline_entry_t *)l->data;
        printf("  Time %ld ms: ", (long)entry->timestamp);
        for (GSList *s = entry->data_list; s != NULL; s = s->next) {
            expiration_data_t *exp = (expiration_data_t *)s->data;
            printf("[%s (ID:%u)] ", exp->task_name->str, exp->task_id);
        }
        printf("\n");
    }

    // 3. Inspect Results Storage (GArray)
    printf("\n[Results Storage (GArray)]:\n");
    for (guint i = 0; i < sched->schedule_results->len; i++) {
        task_result_t *res = &g_array_index(sched->schedule_results, task_result_t, i);
        if (res->output_data != NULL) {
            printf("  Task ID %u: Remaining Runs: %u, JSON: %s\n", 
                   i, res->remaining_runs, res->output_data->str);
        }
    }
    printf("--------------------------------------------\n\n");
}

int main(int argc, char *argv[]) {
    printf("============================================\n");
    printf("   DUAL PRIORITY QUEUE ARCHITECTURE TESTER  \n");
    printf("============================================\n\n");

    /* TEST 1: Version Comparison Logic */
    printf("[Test 1] Testing Versioning Logic...\n");
    printf("  Comparing 1.2.0 vs 1.10.2: %d (Expected: -1)\n", compare_versions("1.2.0", "1.10.2"));
    printf("  Comparing 2.1.0 vs 2.1.0:  %d (Expected: 0)\n\n", compare_versions("2.1.0", "2.1.0"));

    /* TEST 2: Schedule Constructor */
    printf("[Test 2] Initializing new Schedule...\n");
    schedule_t *my_sched = schedule_new("Power_Grid_Optimizer", "1.0.4");
    if (my_sched) {
        printf("  Success: '%s' created.\n", my_sched->schedule_name->str);
    }

    /* TEST 3: Adding Tasks (Grouping Logic) */
    printf("[Test 3] Adding tasks (Grouping at T=100)...\n");
    // Task A and B share the same start time
    schedule_add_task(my_sched, 1, "Freq_Monitor", 0, 10, 5, NULL, 100, 200, "{\"unit\":\"Hz\"}");
    schedule_add_task(my_sched, 2, "Load_Balancer", 0, 8, 3, NULL, 100, 200, "{\"mode\":\"auto\"}");
    
    // Task C starts later
    schedule_add_task(my_sched, 5, "Database_Sync", 1, 5, 1, NULL, 300, 500, NULL);

    /* TEST 4: Out-of-Order Insertion (Priority Check) */
    printf("[Test 4] Inserting task with earlier time T=50 (Sorting Check)...\n");
    // This task is added LAST but should appear FIRST in the timeline
    schedule_add_task(my_sched, 0, "Hardware_Init", 0, 15, 1, NULL, 50, 80, "{\"init\":true}");

    /* TEST 5: Boundary Check - Duplicate Start Time at Tail */
    printf("[Test 5] Adding task at T=300 to existing group...\n");
    schedule_add_task(my_sched, 10, "Maintenance_Log", 2, 0, 1, NULL, 300, 450, "");

    /* TEST 6: Sparse ID in GArray */
    printf("[Test 6] Adding task with high ID 25...\n");
    schedule_add_task(my_sched, 25, "Emergency_Shutdown", 0, 20, 1, NULL, 600, 700, "{\"crit\":true}");

    /* FINAL REPORT */
    print_schedule_report(my_sched);

    /* TEST 7: Manual Check for Earliest Expiration */
    printf("[Test 7] Checking earliest expiration entry...\n");
    GList *head_end = my_sched->schedule_end_info->head;
    if (head_end) {
        timeline_entry_t *e = (timeline_entry_t *)head_end->data;
        printf("  Earliest expiration found at T = %ld (Expected: 80)\n", (long)e->timestamp);
    }

    /* CLEANUP */
    printf("\nPerforming Deep Memory Cleanup...\n");
    schedule_free(my_sched);

    printf("Test Suite Finished Successfully.\n");
    return 0;
}