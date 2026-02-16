#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <glib.h>
#include <stdlib.h>
#include "task.h"

/* Task Output Structures */
typedef struct{
    GString *output_data;       // Contains the effective output in json format (if the task is not void)
    guint8 remaining_runs;      // Contains the value of the repetition that lefts 
} task_result_t;

/* Schedule Structure */
typedef struct {
    GString *schedule_name;
    GString *schedule_version;
    guint16 length;
    GSList *tasks;
    GHashTable *results;

} schedule_t;

/* Schedule Plan Structure */


typedef struct {

} schedule_plan_t;

/* Schedule Constructors */
schedule_t* schedule_new(const gchar *name, const gchar *version);

/* Schedule Distructor */
void schedule_free(schedule_t *sched);

/* Schedule Getters */
GSList* schedule_get_tasks(schedule_t *sched);
GHashTable* schedule_get_results(schedule_t *sched);

/* Schedule Additional Operation */
void schedule_add_task(schedule_t *sched, task_t *task);

/* Schedule Utils Functions */
int compare_versions(const gchar *v1, const gchar *v2);


#endif // SCHEDULE_H
