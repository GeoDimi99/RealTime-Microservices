#ifndef TASK_WRAPPER_MESSAGE_EVENT_H
#define TASK_WRAPPER_MESSAGE_EVENT_H


#include <glib.h>
#include <stdio.h>
#include <mqueue.h>

#include "task_ipc.h"
#include "task_wrapper.h"

typedef struct {
    gpointer tw;
    guint16 session_id;
    pthread_t thread_id;
} thread_cleanup_data_t;


/* Message Evnet Handlers */
gboolean handle_input_message(GIOChannel *source, GIOCondition condition, gpointer user_data);
gboolean handle_end_thread(gpointer data);

#endif