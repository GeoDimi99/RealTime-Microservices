#include "execution_manager.h"




execution_manager_t* em_new(const gchar *name){
    g_return_val_if_fail(name != NULL, NULL);

    execution_manager_t *em = g_new0(execution_manager_t, 1);
    em->em_name = g_string_new(name);

    return em;
}


void em_free(execution_manager_t *em){
    if (!em) return;

    g_string_free(em->em_name, TRUE);
    g_free(em);
}



void em_run_schedule(execution_manager_t *em, schedule_t *sched) {
    g_return_if_fail(em != NULL);
    g_return_if_fail(sched != NULL);

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    gint64 time_zero_us = g_get_monotonic_time();

    /* 1. Plan the scheudle DEADLINES */
    for (GList *l = sched->schedule_end_info->head; l != NULL; l = l->next) {
        timeline_entry_t *entry = (timeline_entry_t *)l->data;

        /* CORREZIONE: Uso deadline_context_t invece di context_t */
        deadline_context_t *ctx = g_new0(deadline_context_t, 1);
        ctx->data = entry->data_list;
        ctx->loop = loop;
        ctx->timestamp = entry->timestamp;
        ctx->is_last = (l->next == NULL); // Se è l'ultimo nodo della GQueue
        ctx->sched = sched;

        gint64 target_mono_us = time_zero_us + (entry->timestamp * 1000);

        GSource *source = g_timeout_source_new(0);
        g_source_set_ready_time(source, target_mono_us);
        g_source_set_callback(source, handle_expiration, ctx, NULL);
        g_source_attach(source, g_main_loop_get_context(loop));
        g_source_unref(source);
    }

    /* 2. Plan the schedule STARTS */
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

    g_print("[INFO] Execution Manager: Scheduler started! Waiting for events...\n");
    g_main_loop_run(loop);
    
    g_main_loop_unref(loop);
    g_print("[INFO] Execution Manager: Scheduler terminated successfully.\n");
}



gboolean handle_initialization(gpointer user_data) {
    start_context_t *ctx = (start_context_t *)user_data;
    GSList *tasks = (GSList *)ctx->data;

    if (tasks == NULL) {
        g_print("[ERROR] Execution Manager: No tasks\n");
        g_free(ctx);
        return G_SOURCE_REMOVE;
    }

    for (GSList *l = tasks; l != NULL; l = l->next) {
        activation_data_t *task = (activation_data_t *)l->data;
        
        g_print("--------------------------------------------------\n");
        g_print("[INFO] Task ID: %u\n", task->task_id);
        g_print("[INFO] Task Name: %s\n", task->task_name ? task->task_name->str : "(null)");
        g_print("[INFO] Policy: %d | Priority: %d | Repetition: %u\n", 
                task->policy, (gint)task->priority, (guint)task->repetition);
        
        /* Set CPU Affinity core */
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(task->cpu_affinity, &set);                  // Set CPU 0
        sched_setaffinity(0, sizeof(cpu_set_t), &set);      // 0 is the calling process

        /* Setting scheduler policy and priority */
        struct sched_param param;
        param.sched_priority = task->priority;


        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);
        pthread_attr_setschedpolicy(&attr, task->policy);
        pthread_attr_setschedparam(&attr, &param);
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

        pthread_t thread;
        pthread_create(&thread, &attr, task->task_exec, task->input_data);
        //pthread_detach(thread);


        

        // Iterate through GSList of dependencies
        if (task->depends_on) {
            g_print("[INFO] Depends On (IDs): ");
            for (GSList *dep = task->depends_on; dep != NULL; dep = dep->next) {
                // Assuming the list stores integers cast to pointers, or pointers to guint
                // If they are pointers to uint: *(guint*)dep->data
                // If they are IDs stored directly in the pointer: GPOINTER_TO_UINT(dep->data)
                g_print("%u ", GPOINTER_TO_UINT(dep->data));
            }
            g_print("\n");
        } else {
            g_print("[INFO] Depends On: None\n");
        }
    }
    g_print("--------------------------------------------------\n");

    g_free(ctx); 
    return G_SOURCE_REMOVE;
}



gboolean handle_expiration(gpointer user_data) {
    deadline_context_t *ctx = (deadline_context_t *)user_data;
    GSList *tasks = (GSList *)ctx->data;

    if (tasks == NULL) {
        g_print("[INFO] Execution Manager: No tasks to expire\n");
    } else {
        for (GSList *l = tasks; l != NULL; l = l->next) {
            expiration_data_t *exp = (expiration_data_t *)l->data;
            
            /* Check if the task is jet completed*/
            if (schedule_is_task_completed(ctx->sched, exp->task_id)) {
                g_print("[INFO] Execution Manager: Task %u already completed. No ABORT sent.\n", exp->task_id);
                continue;
            }

            g_print("[INFO] Execution Manager: Sent ABORT for Task ID %u\n", exp->task_id);
        }
    }

    if (ctx->is_last) {
        g_print("[INFO] Execution Manager (handle_expiration): Final deadline reached. Quitting...\n");
        g_main_loop_quit(ctx->loop);
    }

    g_free(ctx); 
    return G_SOURCE_REMOVE;
}