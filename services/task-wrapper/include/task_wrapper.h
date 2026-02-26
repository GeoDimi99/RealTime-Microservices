#ifndef TASK_WRAPPER_H
#define TASK_WRAPPER_H

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>    /* Obbligatorio per pthread_t */

#include <glib.h>
#include <json-glib/json-glib.h>
#include <mqueue.h>

#include "task.h"
#include "task_ipc.h"
#include "app_task.h"
#include "task_wrapper_message_event.h"



#define DEFAULT_TASK_NAME "task_wrapper"

/* Struttura principale del Task Wrapper */
typedef struct {
    GString *task_name;
    mqd_t task_queue;
    mqd_t em_queue;
    
    /* Mappa: Session ID (guint16) -> GSList di puntatori pthread_t* */
    GHashTable* task_session_table;

} task_wrapper_t;

typedef struct {
    guint16 session_id;
    task_wrapper_t *tw;
    input_t *input;
} task_input_t;



/* Costruttore e Distruttore */
task_wrapper_t* task_wrapper_new(const gchar *task_name, const gchar *task_queue_name, const gchar *em_name);
void task_wrapper_free(task_wrapper_t *tw);

/* Gestione Thread/Sessioni */

void task_wrapper_add_thread(task_wrapper_t *tw, guint16 session_id, pthread_t thread_id);
void task_wrapper_remove_thread(task_wrapper_t *tw, guint16 session_id, pthread_t thread_id);
GSList* task_wrapper_get_threads(task_wrapper_t *tw, guint16 session_id);


/* Operazioni di Parsing Input */
GSList* parse_input_list(const gchar *input_data);
void free_input_list(GSList *list);
void * task_wrapper_run_thread(void *arg);


#endif /* TASK_WRAPPER_H */