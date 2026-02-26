#ifndef TIMER_EVENT_H
#define TIMER_EVENT_H

#include <glib.h>
#include <mqueue.h>
#include "schedule.h"
#include "task_ipc.h"

/* Forward declaration */
typedef struct execution_manager_t execution_manager_t;

typedef struct {
    gpointer data;      /* GSList di activation_data_t */
    gint64 timestamp;   /* Per logging */
} start_context_t;

typedef struct {
    gpointer data;       /* GSList di expiration_data_t */
    GMainLoop *loop;     /* Riferimento per terminare il programma */
    gboolean is_last;    /* Flag per l'ultimo evento della timeline */
    gint64 timestamp;    /* Per logging */
    schedule_t *sched;
} deadline_context_t;

typedef struct {
    execution_manager_t *em;
    schedule_t *sched;
} result_context_t;

typedef struct {
    schedule_t *sched;
    GMainLoop *loop;
} completion_context_t;


/* Forward declarations */
gboolean handle_initialization(gpointer user_data);
gboolean handle_expiration(gpointer user_data);
gboolean handle_result_message(GIOChannel *source, GIOCondition condition, gpointer user_data);
gboolean check_completion(gpointer user_data);


#endif