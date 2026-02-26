#include "execution_manager_timer_event.h"
#include "schedule.h"
#include "execution_manager.h"
#include <stdio.h>
#include <errno.h>

gboolean handle_initialization(gpointer user_data) {
    start_context_t *ctx = (start_context_t *)user_data;
    GSList *tasks = (GSList *)ctx->data;

    if (tasks == NULL) {
        g_print("[ERROR] Execution Manager (handle_initialization): No tasks\n");
        g_free(ctx);
        return G_SOURCE_REMOVE;
    }

    for (GSList *l = tasks; l != NULL; l = l->next) {
        activation_data_t *task = (activation_data_t *)l->data;
        ipc_msg_t msg;
        
        // Pulizia memoria del messaggio per evitare dati sporchi
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

        // Assicurati che task->task_queue sia stato aperto correttamente in precedenza
        if (mq_send(task->task_queue, (const char *)&msg, sizeof(ipc_msg_t), 0) == -1) {
            perror("[ERROR] mq_send (REQUEST) failed");
        }
        
        g_print("[INFO] Execution Manager (handle_initialization): Sent REQUEST for Task ID %u\n", task->task_id);
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
                perror("[ERROR] mq_receive (RESULT) failed");
            }
            return TRUE;
        }

        if (bytes_read > 0) {
            if (msg.type == MSG_TASK_RESULT) {
                g_print("[INFO] Execution Manager (handle_result_message): Received RESULT for Task ID %u\n", msg.task_id);
                schedule_set_result(ctx->sched, msg.task_id, msg.data.result);
            } else {
                g_print("[WARN] Execution Manager (handle_result_message): Received unexpected message type %d\n", msg.type);
            }
        }
    }
    return TRUE;
}

gboolean handle_expiration(gpointer user_data) {
    deadline_context_t *ctx = (deadline_context_t *)user_data;
    GSList *tasks = (GSList *)ctx->data;

    if (tasks == NULL) {
        g_print("[INFO] Execution Manager (handle_expiration): No tasks to expire\n");
    } else {
        for (GSList *l = tasks; l != NULL; l = l->next) {
            expiration_data_t *exp = (expiration_data_t *)l->data;
            
            // Controlla se il task è già stato completato
            if (schedule_is_task_completed(ctx->sched, exp->task_id)) {
                g_print("[INFO] Execution Manager (handle_expiration): Task %u already completed. No ABORT sent.\n", exp->task_id);
                continue; // Salta all'iterazione successiva
            }

            ipc_msg_t msg;
            memset(&msg, 0, sizeof(ipc_msg_t));

            msg.task_id = exp->task_id; // FIX: Usato exp invece di task
            msg.type = MSG_TASK_ABORT;

            /* ATTENZIONE: exp deve avere il campo task_queue. 
               Se non ce l'ha, questo mq_send darà errore di compilazione.
            */
            if (mq_send(exp->task_queue, (const char *)&msg, sizeof(ipc_msg_t), 0) == -1) {
                perror("[ERROR] mq_send (ABORT) failed");
            }

            g_print("[INFO] Execution Manager (handle_expiration): Sent ABORT for Task ID %u\n", exp->task_id);
        }
    }

    if (ctx->is_last) {
        g_print("[INFO] Execution Manager (handle_expiration): Final deadline reached. Quitting...\n");
        g_main_loop_quit(ctx->loop);
    }

    g_free(ctx); 
    return G_SOURCE_REMOVE;
}