#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>

#include "task_wrapper_message_event.h"

/* Nota: set_thread_affinity non è più strettamente necessaria se usiamo gli attributi 
   al momento della creazione, ma la teniamo come helper se servisse spostarli a runtime. */
static void set_thread_affinity(pthread_t tid, int core_id) {
    cpu_set_t cpuset; 
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    if (pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("[ERROR] pthread_setaffinity_np");
    } else {
        g_print("[INFO] Thread %lu pinned to Core %d\n", (unsigned long)tid, core_id);
    }
}

/* Message Event Handlers */
gboolean handle_input_message(GIOChannel *source, GIOCondition condition, gpointer user_data) {
    task_wrapper_t *tw = (task_wrapper_t *)user_data;
    ipc_msg_t msg;
    unsigned int priority;

    if (condition & G_IO_IN) {
        ssize_t bytes_read = mq_receive(tw->task_queue, (char *)&msg, sizeof(ipc_msg_t), &priority);

        if (bytes_read >= 0) {
            g_print("[INFO] Message for Session ID: %u\n", msg.task_id);

            switch (msg.type) {
                case MSG_TASK_REQUEST: {
                    g_print("  Action: Starting threads from JSON payload...\n");
                    
                    GSList *inputs = parse_input_list(msg.data.task_request.input_data);
                    if (!inputs) break;

                    /* PREPARAZIONE ATTRIBUTI PER CORE AFFINITY */
                    pthread_attr_t attr;
                    cpu_set_t cpuset;
                    pthread_attr_init(&attr);
                    CPU_ZERO(&cpuset);
                    CPU_SET(1, &cpuset); // Vincoliamo al Core 1
                    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);

                    for (GSList *l = inputs; l != NULL; l = l->next) {
                        task_input_t *t_in = g_new0(task_input_t, 1);
                        t_in->session_id = msg.task_id;
                        t_in->tw = tw;
                        t_in->input = (input_t *)l->data;

                        pthread_t tid;
                        /* 3. Crea il thread direttamente con l'affinità impostata */
                        if (pthread_create(&tid, &attr, task_wrapper_run_thread, t_in) == 0) {
                            g_print("[INFO] Thread %lu created directly on Core 1\n", (unsigned long)tid);
                            
                            task_wrapper_add_thread(tw, t_in->session_id, tid);
                            pthread_detach(tid);
                        } else {
                            perror("[ERROR] pthread_create failed");
                            g_free(t_in->input);
                            g_free(t_in);
                        }
                    }
                    
                    pthread_attr_destroy(&attr);
                    /* The ownership of the data within the 'inputs' list (the input_t structs)
                     * has been transferred to the newly created threads. The threads are now
                     * responsible for freeing that memory. We only need to free the list structure itself. */
                    g_slist_free(inputs);
                    break;
                }

                case MSG_TASK_ABORT: {
                    g_print("  Action: FORCED ABORT for Session ID: %u\n", msg.task_id);
                    
                    GSList *threads = task_wrapper_get_threads(tw, msg.task_id);
                    if (!threads) {
                        g_print("  [WARN] No active threads found for session %u\n", msg.task_id);
                        break;
                    }

                    for (GSList *l = threads; l != NULL; l = l->next) {
                        pthread_t *tid_ptr = (pthread_t *)l->data;
                        g_print("  [KILL] Cancelling thread %lu\n", (unsigned long)*tid_ptr);
                        
                        pthread_cancel(*tid_ptr);
                        task_wrapper_remove_thread(tw, msg.task_id, *tid_ptr);
                    }
                    
                    g_slist_free(threads);
                    break;
                }

                default:
                    break;
            }
        }
    }
    return TRUE;
}

gboolean handle_end_thread(gpointer data) {
    thread_cleanup_data_t *cleanup = (thread_cleanup_data_t *)data;
    task_wrapper_remove_thread(cleanup->tw, cleanup->session_id, cleanup->thread_id);
    g_free(cleanup);
    return G_SOURCE_REMOVE;
}