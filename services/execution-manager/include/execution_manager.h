#ifndef EXECUTION_MANAGER_H
#define EXECUTION_MANAGER_H

#include <glib.h>
#include "schedule.h"
#include "execution_manager_timer_event.h"


#define DEFAULT_EXECUTION_MANAGER_NAME "execution_manager"


typedef struct {
    /* Static configuration */
    GString *em_name;
    mqd_t em_queue;

} execution_manager_t;

execution_manager_t* em_new(const gchar *name);
void em_free(execution_manager_t *em);

/*  */

void em_run_schedule(schedule_t *sched);

#endif // EXECUTION_MANAGER_H