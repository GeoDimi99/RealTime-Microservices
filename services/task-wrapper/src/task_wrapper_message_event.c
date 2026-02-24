#include "task_wrapper_message_event.h"

gboolean on_mqueue_message(GIOChannel *source, GIOCondition condition, gpointer user_data) {
    task_wrapper_t *tw = (task_wrapper_t *)user_data;
    ipc_msg_t msg;
    unsigned int priority;

    if (condition & G_IO_IN) {
        // Leggiamo il messaggio dalla coda POSIX
        // Usiamo sizeof(ipc_msg_t) come definito in task_wrapper_c (mq_msgsize)
        ssize_t bytes_read = mq_receive(tw->task_queue, (char *)&msg, sizeof(ipc_msg_t), &priority);

        if (bytes_read >= 0) {
            g_print("[INFO] Task Wrapper (on_mqueue_message): Received message with Task ID: %u\n", msg.task_id);

            switch (msg.type) {
                case MSG_TASK_REQUEST:
                    g_print("  Type: REQUEST\n");
                    g_print("  Policy: %d, Priority: %d, Repetitions: %u\n", 
                            msg.data.task_request.policy, 
                            msg.data.task_request.priority, 
                            msg.data.task_request.repetition);
                    g_print("  Payload: %s\n", msg.data.task_request.input_data);
                    
                    // TODO: Qui chiameresti la logica effettiva del tuo task
                    break;

                case MSG_TASK_ABORT:
                    g_print("  Type: ABORT\n");
                    break;

                default:
                    g_print("  Type: UNKNOWN (%d)\n", msg.type);
                    break;
            }
        } else {
            if (errno != EAGAIN) {
                perror("[ERROR] mq_receive\n");
            }
        }
    }

    // Ritorniamo TRUE per mantenere la callback attiva nel loop
    return TRUE;
}