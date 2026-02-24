#include "task_wrapper.h"


task_wrapper_t* task_wrapper_new(const gchar *task_name, const gchar *task_queue_name, const gchar *em_queue_name){
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
    tw->task_queue = mq_open(task_queue_name, O_RDONLY | O_CREAT | O_NONBLOCK, 0644, &attr);
    if (tw->task_queue == (mqd_t)-1) {
            g_error("[ERROR] %s (%s) : mq_open failed.", task_name, task_queue_name);
    }

    tw->em_queue = mq_open(em_queue_name, O_WRONLY | O_CREAT | O_NONBLOCK, 0644, &attr);
    if (tw->em_queue == (mqd_t)-1) {
            g_error("[ERROR] Execution Manager (%s) : mq_open failed.", em_queue_name);
    }; 

    return tw;
}

/* To implement the free*/
void task_wrapper_free(task_wrapper_t *tw) {
    if (tw == NULL) return;

    if (tw->task_queue != (mqd_t)-1) {
        mq_close(tw->task_queue);
    }
    if (tw->em_queue != (mqd_t)-1) {
        mq_close(tw->em_queue);
    }

    if (tw->task_name) {
        g_string_free(tw->task_name, TRUE);
    }

    g_free(tw);
    
    g_print("[INFO] Task wrapper rimosso correttamente.\n");
}