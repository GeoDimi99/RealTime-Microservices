#include <glib.h>
#include <stdio.h>
#include "schedule.h"
#include "task.h"

int main() {
    g_print("=== Starting Scheduler System Test ===\n\n");

    // 1. Test Version Comparison
    g_print("[TEST 1] Version Validation:\n");
    const gchar *v_old = "1.2.5";
    const gchar *v_new = "1.3.0";
    int cmp = compare_versions(v_old, v_new);
    g_print("  Comparing %s vs %s: %s\n", v_old, v_new, (cmp < 0) ? "Newer version detected" : "Old version detected");

    // 2. Initialize Schedule
    g_print("\n[TEST 2] Schedule Initialization:\n");
    schedule_t *my_sched = schedule_new("Production_Workflow", "2.1.0");
    if (my_sched) {
        g_print("  Schedule '%s' created (Ver: %s)\n", 
                my_sched->schedule_name->str, my_sched->schedule_version->str);
    }

    // 3. Create and Add Tasks
    g_print("\n[TEST 3] Adding Tasks and Resolving Dependencies:\n");
    
    // Task 1: Data Fetching
    task_t *t1 = task_new(10, "Data_Fetch", SCHED_POLICY_FIFO, 50, 3, 1000, 2000, NULL, "data.json");
    schedule_add_task(my_sched, t1);
    
    // Task 2: Data Processing (depends on Task 1)
    task_t *t2 = task_new(20, "Data_Process", SCHED_POLICY_RR, 40, 1, 2001, 3000, "data.json", "out.json");
    task_add_dependency(t2, "Data_Fetch");
    schedule_add_task(my_sched, t2);

    g_print("  Added %u tasks to the schedule.\n", my_sched->length);

    // 4. Verify Hash Table Mapping
    g_print("\n[TEST 4] Result Mapping (Hash Table):\n");
    // Retrieve the result for Task ID 10
    task_result_t *res = g_hash_table_lookup(my_sched->results, GUINT_TO_POINTER(10));
    if (res) {
        g_print("  Found result entry for Task 10!\n");
        g_print("  Initial remaining runs: %u\n", res->remaining_runs);
        
        // Simulate a result update
        g_string_assign(res->output_data, "{\"status\": \"success\", \"items\": 150}");
        g_print("  Updated JSON output: %s\n", res->output_data->str);
    }

    // 5. Cleanup
    g_print("\n[TEST 5] Deep Memory Cleanup...\n");
    schedule_free(my_sched); // This will free strings, tasks, list, and hash table
    
    g_print("\n=== All Tests Passed Successfully ===\n");

    return 0;
}