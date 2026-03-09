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

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    gint64 time_zero_us = g_get_monotonic_time();

    /* 1. Listen for incoming results from Task Wrappers */
    GIOChannel *channel = g_io_channel_unix_new(em->em_queue);
    g_io_channel_set_encoding(channel, NULL, NULL); // Binary messages
    g_io_channel_set_buffered(channel, FALSE);

    result_context_t *result_ctx = g_new0(result_context_t, 1);
    result_ctx->em = em;
    result_ctx->sched = sched;

    g_io_add_watch(channel, G_IO_IN, (GIOFunc)handle_result_message, result_ctx);


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

        msg.task_id = task->task_id;
        msg.type = MSG_TASK_REQUEST;
        msg.data.task_request.policy = task->policy;
        msg.data.task_request.priority = task->priority;
        msg.data.task_request.repetition = task->repetition;

        if (task->input_data && task->input_data->str) {
            g_strlcpy(msg.data.task_request.input_data, 
                      task->input_data->str, 
                      MAX_TASK_JSON_IN);
        }

        /* Check if task->task_queue was open correctly */
        if (mq_send(task->task_queue, (const char *)&msg, sizeof(ipc_msg_t), 0) == -1) {
            perror("[ERROR] Execution Manager: mq_send (REQUEST) failed");
        }
        
        g_print("[INFO] Execution Manager: Sent REQUEST for Task ID %u\n", task->task_id);
    }

    g_free(ctx); 
    return G_SOURCE_REMOVE;
}

gboolean handle_result_message(GIOChannel *source, GIOCondition condition, gpointer data) {
    result_context_t *ctx = (result_context_t *)data;
    ipc_msg_t msg;
    unsigned int priority;

    if (condition & G_IO_IN) {
        ssize_t bytes_read = mq_receive(ctx->em->em_queue, (char *)&msg, sizeof(ipc_msg_t), &priority);

        if (bytes_read < 0) {
            if (errno != EAGAIN) {
                perror("[ERROR] Execution Manager: mq_receive (RESULT) failed");
            }
            return TRUE;
        }

        if (bytes_read > 0) {
            if (msg.type == MSG_TASK_RESULT) {
                g_print("[INFO] Execution Manager: Received RESULT for Task ID %u\n", msg.task_id);
                schedule_set_result(ctx->sched, msg.task_id, msg.data.result);
            } else {
                g_print("[WARNING] Execution Manager: Received unexpected message type %d\n", msg.type);
            }
        }
    }
    return TRUE;
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

            ipc_msg_t msg;
            memset(&msg, 0, sizeof(ipc_msg_t));

            msg.task_id = exp->task_id; 
            msg.type = MSG_TASK_ABORT;

    
            if (mq_send(exp->task_queue, (const char *)&msg, sizeof(ipc_msg_t), 0) == -1) {
                perror("[ERROR] Execution Manager: mq_send (ABORT) failed");
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