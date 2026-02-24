#ifndef TASK_WRAPPER_H
#define TASK_WRAPPER_H

#include <stdint.h>
#include <unistd.h>       /* For sleep() */
#include <errno.h>        /* For errno, ENOENT */
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <mqueue.h>

#include "task.h"
#include "task_ipc.h"

#define DEFAULT_TASK_NAME "task_wrapper"



/* Task descriptor */
typedef struct {
    /* Static configuration */
    GString *task_name;
    mqd_t task_queue;
    mqd_t em_queue; 

} task_wrapper_t;

task_wrapper_t* task_wrapper_new(const gchar *task_name, const gchar *task_queue_name, const gchar *em_name);
void task_wrapper_free(task_wrapper_t *task);



#endif /* TASK_WRAPPER_H */
