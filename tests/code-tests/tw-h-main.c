#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <glib.h>
#include <mqueue.h>
#include "task_wrapper.h"

/* * * Mock Thread Function
 * Represents the worker thread logic.
 * Using g_print because it's more robust in multi-threaded GLib apps.
 */
void* worker_thread_simulation(void* arg) {
    // Correct way to cast pointer back to integer on 64-bit systems
    guint16 session_id = (guint16)(guintptr)arg;

    g_print("[Worker] Thread started for Session ID: %u\n", session_id);
    
    // Simulate some work (0.5 seconds)
    g_usleep(500000);
    
    g_print("[Worker] Thread finishing for Session ID: %u\n", session_id);
    return NULL;
}

int main(int argc, char *argv[]) {
    g_print("=== Task Wrapper System Test ===\n");

    /* 1. Configuration */
    const gchar *task_name = "MathProcessor";
    const gchar *in_queue_name = "/multiply_in_q";
    const gchar *em_queue_name = "/em_out_q";

    g_print("[Test 1] Cleaning and Initializing Queues...\n");
    mq_unlink(in_queue_name);
    mq_unlink(em_queue_name);

    /* 2. Constructor Test */
    task_wrapper_t *tw = task_wrapper_new(task_name, in_queue_name, em_queue_name);
    if (!tw) {
        g_printerr("Failed to create task wrapper.\n");
        return EXIT_FAILURE;
    }
    g_print("Wrapper '%s' initialized. Queues opened.\n", tw->task_name->str);

    /* 3. JSON Parsing Test */
    g_print("\n[Test 2] Parsing Input JSON...\n");
    const gchar *mock_json = "[{\"a\": 10, \"b\": 20}, {\"a\": 5, \"b\": 7}]";
    
    GSList *inputs = parse_input_list(mock_json);
    if (!inputs) {
        g_printerr("JSON Parsing failed!\n");
    } else {
        g_print("Parsed %u input elements:\n", g_slist_length(inputs));
        for (GSList *l = inputs; l != NULL; l = l->next) {
            print_input((input_t*)l->data);
        }
    }
    g_print("JSON Parsing Test Finished.\n");

    /* 4. Session & Thread Management Test */
    g_print("\n[Test 3] Testing Session Table and Threads...\n");
    
    guint16 session_id = 42;
    pthread_t thread1, thread2;

    g_print("  -> Creating worker threads...\n");
    // Passing session_id as a pointer-sized integer
    if (pthread_create(&thread1, NULL, worker_thread_simulation, (void*)(guintptr)session_id) != 0) {
        perror("pthread_create 1 failed");
        return EXIT_FAILURE;
    }

    if (pthread_create(&thread2, NULL, worker_thread_simulation, (void*)(guintptr)session_id) != 0) {
        perror("pthread_create 2 failed");
        return EXIT_FAILURE;
    }

    g_print("  -> Registering threads in hash table...\n");
    task_wrapper_add_thread(tw, session_id, thread1);
    task_wrapper_add_thread(tw, session_id, thread2);

    // Give the threads a moment to start and print their first message
    g_usleep(100000); 

    GSList *active_threads = g_hash_table_lookup(tw->task_session_table, GINT_TO_POINTER(session_id));
    g_print("  -> Session %u registered with %u active threads.\n", session_id, g_slist_length(active_threads));

    g_print("  -> Simulating removal of thread 1...\n");
    task_wrapper_remove_thread(tw, session_id, thread1);
    
    active_threads = g_hash_table_lookup(tw->task_session_table, GINT_TO_POINTER(session_id));
    g_print("  -> Session %u now has %u active threads.\n", session_id, g_slist_length(active_threads));

    /* 5. Cleanup */
    g_print("\n[Test 4] Cleaning up...\n");
    
    // Wait for threads to finish properly
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    g_print("  -> Threads joined.\n");

    free_input_list(inputs);
    g_print("  -> Input list freed.\n");

    task_wrapper_free(tw);
    g_print("  -> Wrapper freed.\n");
    
    mq_unlink(in_queue_name);
    mq_unlink(em_queue_name);

    g_print("=== Test Completed Successfully ===\n");

    return EXIT_SUCCESS;
}