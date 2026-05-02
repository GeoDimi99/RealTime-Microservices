#ifndef EXECUTION_MANAGER_H
#define EXECUTION_MANAGER_H

#define _GNU_SOURCE
#include <glib.h>
#include <json-glib/json-glib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <hiredis/hiredis.h> 


#include <time.h>  // For performance measuring


#include "schedule.h"
#include "execution_manager.h"
#include "task_ipc.h"



#define DEFAULT_EXECUTION_MANAGER_NAME "execution_manager"

// For test propose 
extern gint iteration; 


/* Execution Manager Stucture */
typedef struct execution_manager_t{
    GString *em_name;               // Execution Manager Name
    gboolean is_leader;             // Execution Manager Role (is leader or not)
    void *zmq_ctx;                  // ZeroMQ Context (one per process)
    void *zmq_pull;                 // PULL socket: leader receives READY from workers
    void *zmq_push;                 // PUSH socket: worker sends READY to leader
    void *zmq_pub;                  // PUB socket: leader broadcasts SYNC to workers
    void *zmq_sub;                  // SUB socket: worker receives SYNC from leader
    redisContext* redis_client;     // Redis Client (for Schedule)
} execution_manager_t;



typedef struct {
    gpointer data;           // GList of activation_data_t
    gint64 timestamp;
    gint64 scheduled_time_ns; // T-zero + start_ms in nanoseconds
    schedule_t *sched;
} start_context_t;

typedef struct {
    gpointer data;       // GSList of expiration_data_t 
    GMainLoop *loop;     // Reference to end the process 
    gboolean is_last;    // Flag that indicat if is the last event 
    gint64 timestamp;    
    schedule_t *sched;
} deadline_context_t;


typedef struct {
    guint16 dep_task_id;    // ID of the task this one depends on
    gchar  *param;          // Input field name to overwrite with dep output
} dep_entry_t;

typedef struct {
    guint16 task_id;            // Task ID 
    gpointer data;              // Task input (JSON string)
    GThreadFunc thread_func;    // Task function
    /* Timestamps for measures the performance */
    glong scheduled_time_ns;    // T-zero + start_ms (theoretical activation)
    glong start_time_request;   // Actual activation time (handle_initialization)
    /*-----------------------------------------*/
    schedule_t *sched;          // Reference to the schedule for store the result
    GSList *depends_on;         // List of dep_entry_t* (ZMQ data dependencies)
    /* Pre-created ZMQ output channel (bound before thread start) */
    void *zmq_ctx;              // ZMQ context (created in handle_initialization)
    void *push_sock;            // PUSH socket already bound to task_out_<id>
} task_wrapper_input_t; 




/* Executor Manager Constructor/Distructors */
execution_manager_t* em_new(const gchar *name);
void em_free(execution_manager_t *em);
gint em_get_iterations(execution_manager_t *em);

/* Execution Manager Getters/Setters */
void em_set_leader(execution_manager_t *em, gboolean leader_flag);


/* Execution Manager Activities*/
void em_run_schedule(execution_manager_t *em, schedule_t *sched);
void em_wait_for_schedule(execution_manager_t *em);
schedule_t * em_read_schedule(execution_manager_t *em);

/* Exectuion Manager Usefull Functions  */
void* task_wrapper_func(void* data);

/* Execution Manager Event Handlers */
gboolean handle_initialization(gpointer user_data);
gboolean handle_expiration(gpointer user_data);

#endif // EXECUTION_MANAGER_H