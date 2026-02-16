#include <stdio.h>
#include <glib.h>
#include "schedule.h"

/**
 * Helper to inspect the internal state of the schedule.
 * It demonstrates how to iterate through the GQueue (start info)
 * and the GHashTable (end info).
 */
void print_schedule_report(schedule_t *sched) {
    if (!sched) return;

    printf("\n--- SCHEDULE REPORT: %s (v%s) ---\n", 
           sched->schedule_name->str, sched->schedule_version->str);
    printf("Total Duration: %ld units\n", sched->schedule_duration);

    // 1. Inspect Activation Queue (schedule_start_info)
    printf("\n[Activation Timeline (GQueue)]:\n");
    GList *q_iter;
    for (q_iter = sched->schedule_start_info->head; q_iter != NULL; q_iter = q_iter->next) {
        start_entry_t *entry = (start_entry_t *)q_iter->data;
        printf("  Time %ld: ", entry->start_time);
        
        for (GSList *s_iter = entry->activation_data; s_iter != NULL; s_iter = s_iter->next) {
            activation_data_t *act = (activation_data_t *)s_iter->data;
            printf("[%s (ID:%u)] ", act->task_name->str, act->task_id);
        }
        printf("\n");
    }

    // 2. Inspect Results Array (schedule_results)
    printf("\n[Results Storage (GArray)]:\n");
    for (guint i = 0; i < sched->schedule_results->len; i++) {
        task_result_t *res = &g_array_index(sched->schedule_results, task_result_t, i);
        // Only print slots that were actually initialized (remaining_runs > 0)
        if (res->remaining_runs > 0) {
            printf("  Index [%u]: Remaining Runs: %u, JSON: %s\n", 
                   i, res->remaining_runs, res->output_data->str);
        }
    }
    printf("--------------------------------------------\n\n");
}

int main(int argc, char *argv[]) {
    printf("============================================\n");
    printf("   SCHEDULE INTERFACE ARCHITECTURE TESTER   \n");
    printf("============================================\n\n");

    /* TEST 1: Version Comparison Logic */
    printf("[Test 1] Testing Versioning Logic...\n");
    const gchar *v1 = "1.2.0";
    const gchar *v2 = "1.10.2";
    int cmp = compare_versions(v1, v2);
    printf("  Comparing %s vs %s: %s\n\n", v1, v2, (cmp < 0) ? "v1 is older" : "v1 is newer");

    /* TEST 2: Schedule Constructor */
    printf("[Test 2] Initializing new Schedule...\n");
    schedule_t *my_sched = schedule_new("Industrial_Control_Plan", "2.5.1");
    if (my_sched) {
        printf("  Success: '%s' created.\n", my_sched->schedule_name->str);
    }

    /* TEST 3: Adding Tasks (Checking Queue grouping) */
    printf("[Test 3] Adding tasks to schedule...\n");
    
    // Task A: Starts at 100
    schedule_add_task(my_sched, 1, "Sensor_Read", SCHED_POLICY_FIFO, 10, 5, NULL, 100, 200, "{\"dev\": \"temp\"}");
    
    // Task B: Also starts at 100 (Testing grouping logic in GQueue)
    schedule_add_task(my_sched, 2, "Voltage_Check", SCHED_POLICY_FIFO, 8, 3, NULL, 100, 150, "{\"dev\": \"volt\"}");
    
    // Task C: Starts at 300 (New start_entry_t)
    schedule_add_task(my_sched, 5, "Data_Sync", SCHED_POLICY_RR, 5, 1, NULL, 300, 500, NULL);

    print_schedule_report(my_sched);

    /* TEST 4: Expiration Mapping (GHashTable) */
    printf("[Test 4] Testing Expiration Mapping (Lookup by end_time)...\n");
    gint64 lookup_time = 150;
    GSList *expiring_tasks = g_hash_table_lookup(my_sched->schedule_end_info, &lookup_time);
    if (expiring_tasks) {
        printf("  Tasks expiring at %ld: ", lookup_time);
        for (GSList *it = expiring_tasks; it != NULL; it = it->next) {
            expiration_data_t *e = (expiration_data_t *)it->data;
            printf("[%s (ID:%u)] ", e->task_name->str, e->task_id);
        }
        printf("\n");
    } else {
        printf("  No tasks found expiring at %ld.\n", lookup_time);
    }

    /* TEST 5: Boundary Check - Duplicate Start Time at Tail */
    printf("\n[Test 5] Adding another task at time 300 (checking Tail-Peek optimization)...\n");
    schedule_add_task(my_sched, 10, "Log_Cleanup", SCHED_POLICY_OTHER, 0, 1, NULL, 300, 400, "");
    
    /* TEST 6: Sparse ID in GArray */
    printf("[Test 6] Adding task with high ID (Sparse Array Check)...\n");
    schedule_add_task(my_sched, 20, "Emergency_Stop", SCHED_POLICY_FIFO, 12, 1, NULL, 600, 700, "");
    
    print_schedule_report(my_sched);

    /* CLEANUP */
    printf("Terminating and performing Deep Memory Cleanup...\n");
    // This will trigger all the _free callbacks: 
    // expiration_list_free, start_entry_free, task_result_free, etc.
    schedule_free(my_sched);

    printf("Test Suite Finished Successfully.\n");
    return 0;
}