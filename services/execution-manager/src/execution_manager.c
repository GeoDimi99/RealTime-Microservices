#include "execution_manager.h"

/* ----------------- Executor Manager Constructor/Distructors ----------------- */
execution_manager_t* em_new(const gchar *name){
    g_return_val_if_fail(name != NULL, NULL);

    execution_manager_t *em = g_new0(execution_manager_t, 1);
    em->em_name = g_string_new(name);

    GString *q_name = g_string_new(NULL);
    g_string_printf(q_name, "/%s_q", em->em_name->str);

    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 10,           
        .mq_msgsize = sizeof(ipc_msg_t), 
        .mq_curmsgs = 0
    };
    mqd_t qd = mq_open(q_name->str, O_RDONLY | O_CREAT | O_NONBLOCK, 0644, &attr);
    if (qd == (mqd_t)-1) {
            g_error("[ERROR] Execution Manager (%s) : mq_open failed.", q_name->str);
    }
    em->em_queue = qd;
    g_string_free(q_name, TRUE);

    return em;
}


void em_free(execution_manager_t *em){
    if (!em) return;

    g_string_free(em->em_name, TRUE);
    if (em->em_queue != (mqd_t)-1) {
        mq_close(em->em_queue);
        em->em_queue = (mqd_t)-1;
    }
    g_free(em);
}

/* ----------------- Exection Manager Activities ----------------- */
void em_run_schedule(execution_manager_t *em, schedule_t *sched) {
    g_return_if_fail(em != NULL);
    g_return_if_fail(sched != NULL);

    /* 0. Drain any stale messages from previous runs */
    ipc_msg_t stale_msg;
    g_print("[INFO] Execution Manager: Draining stale messages from the queue...\n");
    while (mq_receive(em->em_queue, (char *)&stale_msg, sizeof(ipc_msg_t), NULL) != -1) {
        g_print("[INFO] Execution Manager: Discarded stale message for Task ID %u\n", stale_msg.task_id);
    }

    // The loop ends when mq_receive returns -1. We expect errno to be EAGAIN because the queue is now empty.
    if (errno != EAGAIN) {
        g_printerr("[WARNING] Execution Manager: Unexpected error while draining queue");
    }

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    gint64 time_zero_us = g_get_monotonic_time();

    /* 1. Listen for incoming results from Task Wrappers */
    GIOChannel *channel = g_io_channel_unix_new(em->em_queue);
    g_io_channel_set_encoding(channel, NULL, NULL); // Binary messages
    g_io_channel_set_buffered(channel, FALSE);
 
    result_context_t *result_ctx = g_new0(result_context_t, 1);
    result_ctx->em = em;
    result_ctx->sched = sched;
 
    // Store the ID of the watch to remove it later
    guint result_watch_id = g_io_add_watch(channel, G_IO_IN, (GIOFunc)handle_result_message, result_ctx);
 

    /* 2. Plan the Deadlines */
    for (GList *l = sched->schedule_end_info->head; l != NULL; l = l->next) {
        timeline_entry_t *entry = (timeline_entry_t *)l->data;

        deadline_context_t *ctx = g_new0(deadline_context_t, 1);
        ctx->data = entry->data_list;
        ctx->loop = loop;
        ctx->timestamp = entry->timestamp;
        ctx->is_last = (l->next == NULL); // Check if it's the last node
        ctx->sched = sched;

        gint64 target_mono_us = time_zero_us + (entry->timestamp * 1000);

        GSource *source = g_timeout_source_new(0);
        g_source_set_ready_time(source, target_mono_us);
        g_source_set_callback(source, handle_expiration, ctx, NULL);
        g_source_attach(source, g_main_loop_get_context(loop));
        g_source_unref(source);
    }

    /* 3. Plan the Starts */
    for (GList *l = sched->schedule_start_info->head; l != NULL; l = l->next) {
        timeline_entry_t *entry = (timeline_entry_t *)l->data;

        start_context_t *ctx = g_new0(start_context_t, 1);
        ctx->data = entry->data_list;
        ctx->timestamp = entry->timestamp;

        gint64 target_mono_us = time_zero_us + (entry->timestamp * 1000);

        GSource *source = g_timeout_source_new(0);
        g_source_set_ready_time(source, target_mono_us);
        g_source_set_callback(source, handle_initialization, ctx, NULL);
        g_source_attach(source, g_main_loop_get_context(loop));
        g_source_unref(source);
    }

    g_print("[INFO] Execution Manager: Scheduler started. Waiting for events...\n");
    g_main_loop_run(loop);
    
    /* --- Cleanup of event sources --- */
    // This is crucial to prevent dangling pointers and race conditions in the next main loop iteration
    g_source_remove(result_watch_id);
    g_io_channel_unref(channel);
    g_free(result_ctx);

    g_main_loop_unref(loop);
    g_print("[INFO] Execution Manager: Scheduler terminated successfully.\n");
}


/* ----------------- Execution Manager Event Handlers ----------------- */
gboolean handle_initialization(gpointer user_data) {
    start_context_t *ctx = (start_context_t *)user_data;
    GSList *tasks = (GSList *)ctx->data;

    if (tasks == NULL) {
        g_print("[ERROR] Execution Manager: in handle_initialization No tasks\n");
        g_free(ctx);
        return G_SOURCE_REMOVE;
    }

    for (GSList *l = tasks; l != NULL; l = l->next) {
        activation_data_t *task = (activation_data_t *)l->data;
        ipc_msg_t msg;
        
        /* Clean the memory for avoid dirty messages */
        memset(&msg, 0, sizeof(ipc_msg_t));

        /* Measure the start_time_request (for the performance) */
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        glong start_time_request = ts.tv_sec * 1000000000L + ts.tv_nsec;


        /* Prepare the message */
        msg.task_id = task->task_id;
        msg.type = MSG_TASK_REQUEST;
        msg.start_time_request = start_time_request;
        msg.data.task_request.policy = task->policy;
        msg.data.task_request.priority = task->priority;
        msg.data.task_request.cpu_affinity = task->cpu_affinity;
        msg.data.task_request.repetition = task->repetition;

        if (task->input_data && task->input_data->str) {
            g_strlcpy(msg.data.task_request.input_data, 
                      task->input_data->str, 
                      MAX_TASK_JSON_IN);
        }

        /* Check if task->task_queue was open correctly */
        if (mq_send(task->task_queue, (const char *)&msg, sizeof(ipc_msg_t), 0) == -1) {
            g_printerr("[ERROR] Execution Manager: mq_send (REQUEST) failed");
        }
        
        g_print("[INFO] Execution Manager: Sent REQUEST for Task ID %u\n", task->task_id);
    }

    g_free(ctx); 
    return G_SOURCE_REMOVE;
}


/* Function for print and compute the performanc e*/
static void print_performance_metrics(guint16 task_id, glong start_req, glong end_req, glong start_res, glong end_res) {
    double q_em_tw = (end_req - start_req) / 1e6;
    double t_in_tw  = (start_res - end_req) / 1e6;
    double q_tw_em = (end_res - start_res) / 1e6;
    double total   = (end_res - start_req) / 1e6;

    g_print("[METRICS] Execution Manager: Task %u | EM->TW: %.3f ms | Task: %.3f ms | TW->EM: %.3f ms | Total: %.3f ms\n",
            task_id, q_em_tw, t_in_tw, q_tw_em, total);
}


gboolean handle_result_message(GIOChannel *source, GIOCondition condition, gpointer user_data) {
    result_context_t *ctx = (result_context_t *)user_data;
    execution_manager_t *em = ctx->em;
    ipc_msg_t msg;
    unsigned int priority;

    (void)source; // Non utilizzato

    if (condition & G_IO_IN) {
        /* Legge tutti i messaggi disponibili per svuotare la coda in un unico callback */
        while (mq_receive(em->em_queue, (char *)&msg, sizeof(ipc_msg_t), &priority) != -1) {
            
            if (msg.type == MSG_TASK_RESULT) {
                // 1. Ottieni il timestamp finale non appena il messaggio viene ricevuto.
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                glong end_time_result = ts.tv_sec * 1000000000L + ts.tv_nsec;

                // 2. Stampa le metriche di performance.
                print_performance_metrics(
                    msg.task_id,
                    msg.start_time_request,
                    msg.end_time_request,
                    msg.start_time_result,
                    end_time_result
                );

                g_print("[INFO] EM: Ricevuto risultato per Task ID %u: %s\n", msg.task_id, msg.data.result);
                
                // Qui puoi aggiungere la logica per marcare il task come completato nello schedule.
                // Esempio: schedule_set_result(ctx->sched, msg.task_id, msg.data.result);

            } else {
                g_warning("EM: Ricevuto tipo di messaggio inatteso %d.", msg.type);
            }
        }

        if (errno != EAGAIN) {
            g_printerr("[ERRORE] EM: mq_receive ha fallito con un errore inatteso: %s", g_strerror(errno));
        }
    }

    return TRUE; // Mantiene l'event source attivo
}


gboolean handle_expiration(gpointer user_data) {
    deadline_context_t *ctx = (deadline_context_t *)user_data;
    GSList *tasks = (GSList *)ctx->data;

    if (tasks == NULL) {
        g_print("[INFO] Execution Manager: No tasks to expire\n");
    } else {
        for (GSList *l = tasks; l != NULL; l = l->next) {
            expiration_data_t *exp = (expiration_data_t *)l->data;
            
            if (schedule_is_task_completed(ctx->sched, exp->task_id)) {
                g_print("[INFO] Execution Manager: Task %u already completed. No ABORT sent.\n", exp->task_id);
                continue; 
            }
            
            /* ---- Part only for test (disable the abort messages ) ----------- */
            g_print("[INFO] Execution Manager: Task %u NOT completed.\n", exp->task_id);
            continue;
            /* ---------------------------------------------------------------- */

            ipc_msg_t msg;
            memset(&msg, 0, sizeof(ipc_msg_t));

            msg.task_id = exp->task_id; 
            msg.type = MSG_TASK_ABORT;

    
            if (mq_send(exp->task_queue, (const char *)&msg, sizeof(ipc_msg_t), 0) == -1) {
                g_printerr("[ERROR] Execution Manager: mq_send (ABORT) failed");
            }

            g_print("[INFO] Execution Manager: Sent ABORT for Task ID %u\n", exp->task_id);
        }
    }

    if (ctx->is_last) {
        g_print("[INFO] Execution Manager: Final deadline reached. Quitting...\n");
        g_main_loop_quit(ctx->loop);
    }

    g_free(ctx); 
    return G_SOURCE_REMOVE;
}