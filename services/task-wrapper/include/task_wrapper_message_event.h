#include <glib.h>
#include <stdio.h>
#include <mqueue.h>
#include "task_ipc.h"
#include "task_wrapper.h"

gboolean on_mqueue_message(GIOChannel *source, GIOCondition condition, gpointer user_data);