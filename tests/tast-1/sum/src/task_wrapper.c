#include "task_wrapper.h"
#include <errno.h>
#include <fcntl.h>

/* ---- Private Helper Functions ---- */

/**
 * task_session_list_free:
 * Function to free the GSList of pthread_t* stored in the hash table.
 * Used as a GDestroyNotify callback.
 */
static void task_session_list_free(gpointer data) {
    GSList *list = (GSList *)data;
    if (list) {
        g_slist_free_full(list, g_free);
    }
}

/* ---- Lifecycle Management ---- */

task_wrapper_t* task_wrapper_new(const gchar *task_name, const gchar *task_queue_name, const gchar *em_queue_name) {
    g_return_val_if_fail(task_name != NULL, NULL);
    g_return_val_if_fail(task_queue_name != NULL, NULL);
    g_return_val_if_fail(em_queue_name != NULL, NULL);

    task_wrapper_t *tw = g_new0(task_wrapper_t, 1);
    tw->task_name = g_string_new(task_name);

    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 10,           
        .mq_msgsize = sizeof(ipc_msg_t), 
        .mq_curmsgs = 0
    };

    /* Open the incoming task queue (Receiver) */
    tw->task_queue = mq_open(task_queue_name, O_RDONLY | O_CREAT | O_NONBLOCK, 0644, &attr);
    if (tw->task_queue == (mqd_t)-1) {
        g_error("[ERROR] %s: mq_open failed for %s", task_name, task_queue_name);
    }

    /* Setup TX Queue (Connection to Execution Manager) */
    /* Polling Loop: Wait until the Manager creates its queue */
    g_print("[INFO] %s: Waiting for Execution Manager queue '%s'...\n", task_name, em_queue_name);
    while (TRUE) {
        /* Attempt to open the queue in write-only mode. DO NOT create it. */
        tw->em_queue = mq_open(em_queue_name, O_WRONLY | O_NONBLOCK);

        if (tw->em_queue != (mqd_t)-1) {
            g_print("[INFO] %s: Connected to Execution Manager queue.\n", task_name);
            break; // Success
        }

        if (errno == ENOENT) {
            g_usleep(500000); // Wait 0.5 seconds and retry
        } else {
            // For other errors (e.g., permissions), it's a fatal issue.
            g_error("[ERROR] %s: mq_open failed for EM queue '%s': %s", task_name, em_queue_name, g_strerror(errno));
        }
    }

    /* Initialize the Session Hash Table: Session ID (guint16) -> GSList of pthread_t* */
    tw->task_session_table = g_hash_table_new_full(
        g_direct_hash, 
        g_direct_equal, 
        NULL, 
        task_session_list_free
    );

    return tw;
}

void task_wrapper_free(task_wrapper_t *tw) {
    if (!tw) return;

    g_string_free(tw->task_name, TRUE);
    if (tw->task_queue != (mqd_t)-1) mq_close(tw->task_queue);
    if (tw->em_queue != (mqd_t)-1) mq_close(tw->em_queue);

    /* This will recursively call task_session_list_free on all remaining sessions */
    g_hash_table_destroy(tw->task_session_table);

    g_free(tw);
}

/* ---- Session & Thread Management ---- */

void task_wrapper_add_thread(task_wrapper_t *tw, guint16 session_id, pthread_t thread_id) {
    g_return_if_fail(tw != NULL);

    gpointer key = GINT_TO_POINTER(session_id);
    GSList *list = g_hash_table_lookup(tw->task_session_table, key);
    
    pthread_t *p_tid = g_new(pthread_t, 1);
    *p_tid = thread_id;

    if (list != NULL) {
        /* We "steal" the pointer to update the list without triggering the destructor */
        g_hash_table_steal(tw->task_session_table, key);
        list = g_slist_prepend(list, p_tid);
    } else {
        list = g_slist_prepend(NULL, p_tid);
    }

    g_hash_table_insert(tw->task_session_table, key, list);
}



void task_wrapper_remove_thread(task_wrapper_t *tw, guint16 session_id, pthread_t thread_id) {
    g_return_if_fail(tw != NULL);

    gpointer key = GINT_TO_POINTER(session_id);
    GSList *list = g_hash_table_lookup(tw->task_session_table, key);

    if (!list) return; // Protezione: sessione già rimossa

    GSList *iterator = list;
    while (iterator != NULL) {
        pthread_t *stored_tid = (pthread_t *)iterator->data;
        if (pthread_equal(*stored_tid, thread_id)) {
            /* BUG FIX: The data pointed to by the list element ('stored_tid') was being freed
             * before the element was removed from the list. This is a use-after-free.
             * The fix is to remove the element from the list first, then free the data. */
            
            // 1. Steal the list from the hash table to modify it.
            g_hash_table_steal(tw->task_session_table, key);
            
            // 2. Remove the list element. g_slist_delete_link is O(1) since we have the iterator.
            list = g_slist_delete_link(list, iterator);
            
            // 3. If the list is not empty, put it back in the hash table.
            if (list != NULL) {
                g_hash_table_insert(tw->task_session_table, key, list);
            }
            
            // 4. Now that the data is no longer referenced by the list, it's safe to free it.
            g_free(stored_tid);
            
            return;
        }
        iterator = iterator->next;
    }
}


GSList* task_wrapper_get_threads(task_wrapper_t *tw, guint16 session_id) {
    g_return_val_if_fail(tw != NULL, NULL);
    g_return_val_if_fail(tw->task_session_table != NULL, NULL);

    /* Cerchiamo la lista originale nella tabella */
    GSList *original_list = g_hash_table_lookup(tw->task_session_table, GINT_TO_POINTER(session_id));

    if (original_list == NULL) {
        return NULL;
    }

    /* Restituiamo una copia superficiale (shallow copy) della lista.
     * I puntatori pthread_t* restano gli stessi, ma la struttura GSList è nuova. */
    return g_slist_copy(original_list);
}

void * task_wrapper_run_thread(void *arg){
    task_input_t *input_data = (task_input_t *)arg;
    task_wrapper_t *tw = input_data->tw;
    guint16 sid = input_data->session_id;
    pthread_t self = pthread_self();
    
    /* 1. Esecuzione del Task Reale */
    output_t *output = task_main(input_data->input);

    /* 2. Invio del risultato alla em_queue */
    if (output != NULL) {
        ipc_msg_t msg_out;
        memset(&msg_out, 0, sizeof(ipc_msg_t));

        msg_out.type = MSG_TASK_RESULT;
        msg_out.task_id = sid;

        // Convertiamo l'output_t in una stringa JSON
        // Nota: convert_output_to_json deve restituire una stringa allocata (gchar*)
        gchar *json_res = convert_output_to_json(output);

        if (json_res != NULL) {
            // Copiamo la stringa nel payload del messaggio (union data.result)
            g_strlcpy(msg_out.data.result, json_res, MAX_TASK_JSON_OUT);
            g_free(json_res);

            // Invio del messaggio sulla coda EM
            if (mq_send(tw->em_queue, (const char *)&msg_out, sizeof(ipc_msg_t), 0) == -1) {
                g_warning("[ERROR] mq_send result failed: %s", g_strerror(errno));
            } else {
                g_print("[INFO] Result sent to EM for Session ID: %u\n", sid);
            }
        }
    }

    /* 3. Preparazione dati per il cleanup nel thread principale */
    thread_cleanup_data_t *cleanup = g_new0(thread_cleanup_data_t, 1);
    cleanup->tw = tw;
    cleanup->session_id = sid;
    cleanup->thread_id = self;

    /* 4. Invia la richiesta di rimozione dalla tabella al GMainLoop */
    g_main_context_invoke(NULL, (GSourceFunc)handle_end_thread, cleanup);

    /* 5. Cleanup locale al thread */
    g_free(output);
    g_free(input_data->input);
    g_free(input_data);

    return NULL;
}
/* ---- JSON Parsing Operations ---- */

GSList* parse_input_list(const gchar *input_data) {
    g_return_val_if_fail(input_data != NULL, NULL);

    JsonParser *parser = json_parser_new();
    GError *error = NULL;

    if (!json_parser_load_from_data(parser, input_data, -1, &error)) {
        g_printerr("[ERROR] JSON Load: %s\n", error->message);
        g_error_free(error);
        g_object_unref(parser);
        return NULL;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_ARRAY(root)) {
        g_printerr("[ERROR] JSON root is not an array.\n");
        g_object_unref(parser);
        return NULL;
    }

    GSList *list = NULL;
    JsonArray *array = json_node_get_array(root);
    guint length = json_array_get_length(array);

    for (guint i = 0; i < length; i++) {
        JsonObject *obj = json_array_get_object_element(array, i);
        input_t *input_elem = g_new0(input_t, 1);
        
        if (convert_json_to_input(obj, input_elem) == -1) {
            g_free(input_elem);
            g_slist_free_full(list, g_free); 
            g_object_unref(parser);
            return NULL;
        }
        
        /* Prepending is O(1). We will reverse it at the end for O(N) total. */
        list = g_slist_prepend(list, input_elem);
    }

    g_object_unref(parser);
    return g_slist_reverse(list);
}

void free_input_list(GSList *list) {
    /* Assuming input_t is a flat structure, otherwise use a custom free wrapper */
    g_slist_free_full(list, g_free);
}