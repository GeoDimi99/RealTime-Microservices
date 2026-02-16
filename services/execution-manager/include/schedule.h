#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <glib.h>
#include <stdlib.h>
#include "task.h"

/* Utils Structures */

typedef struct {
    guint16 task_id;
    GString *task_name;
    sched_policy_t policy;
    gint8 priority;
    guint8 repetition;
    GSList *depends_on; 
    GString *input_data;        
} activation_data_t;

typedef struct {
    gint64 start_time;
    GSList *activation_data;
} start_entry_t;

typedef struct {
    guint16 task_id;
    GString *task_name;
} expiration_data_t;


typedef struct {
    gint64 end_time;
    GSList *expiration_data;
} end_entry_t;

typedef struct{
    GString *output_data;
    guint8 remaining_runs;    
} task_result_t;

/* Utils Structures */



/* Schedule Structure */
typedef struct {
    GString *schedule_name;
    GString *schedule_version;
    GSList *schedule_start_info;
    GHashTable *schedule_end_info;
    GArray *schedule_results;
} schedule_t;


/* Schedule Constructors */
schedule_t* schedule_new(const gchar *name, const gchar *version);

/* Schedule Distructor */
void schedule_free(schedule_t *sched);

/* Schedule Getters */


/* Schedule Additional Operation */
void schedule_add_task(schedule_t *sched, task_t *task);

/* Schedule Utils Functions */
int compare_versions(const gchar *v1, const gchar *v2);


#endif // SCHEDULE_H
