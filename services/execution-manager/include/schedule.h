#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <glib.h>
#include <mqueue.h>
#include "task_ipc.h"
#include <sched.h>

/* --- Utils Structures --- */

typedef struct {
    guint16 task_id;
    GString *task_name;
    gint policy;
    gint8 priority;
    guint8 repetition;
    GSList *depends_on;  /* Lista di ID (guint16 convertiti a puntatori) */
    GString *input_data;
    mqd_t task_queue;       
} activation_data_t;

typedef struct {
    guint16 task_id;
    GString *task_name;
    mqd_t task_queue;
} expiration_data_t;

typedef struct {
    gint64 timestamp;
    GSList *data_list; /* Lista di activation_data_t* o expiration_data_t* */
} timeline_entry_t;

typedef struct {
    GSList *output_list;   /* Lista di GString* */
    guint8 remaining_runs;
} task_result_t;

/* --- Schedule Main Structure --- */

typedef struct {
    GString *schedule_name;
    GString *schedule_version;
    GQueue *schedule_start_info;
    GQueue *schedule_end_info;
    GHashTable *schedule_results; /* Mappa: Task ID (guint16) -> task_result_t* */
    gint64 schedule_duration;
} schedule_t;

/* --- Function Prototypes --- */

schedule_t* schedule_new(const gchar *name, const gchar *version);
void schedule_free(schedule_t *sched);


void schedule_add_task(schedule_t *sched, 
                guint16 id, 
                const gchar *name, 
                gint policy, 
                gint8 priority, 
                guint8 repetition,
                GSList *depends_on,  
                gint64 start_time, 
                gint64 end_time, 
                const gchar *input);
                
void schedule_set_result(schedule_t *sched, guint16 id, const gchar *output);
gboolean schedule_is_task_completed(schedule_t *sched, guint16 id);
GSList *schedule_get_results(schedule_t *sched, guint16 id);
void schedule_reset(schedule_t *sched);

int compare_versions(const gchar *v1, const gchar *v2);
void print_schedule(schedule_t *sched);

#endif // SCHEDULE_H