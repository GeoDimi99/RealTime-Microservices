#define _GNU_SOURCE
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>

#include "task.h"
#include "task_ipc.h"
#include "task_service.h"
#include "logger.h"
#include "task_entry.h"



/* * Global pointer to the current task context.
 * We use this to maintain state between different IPC messages.
 */
static task_context_t * task_context = NULL;


int main(int argc, char *argv[]){
    (void)argc;
    (void)argv;
    /* Task Service Initialization */
    int exit_code = 0;
    

    /* Memory Locking: Essential for Real-Time determinism */
    if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
        perror("mlockall failed");
        exit_code = -1;
        goto exit_program;
    }

    /* Set Task and Queues Names */
    const char* env_task_name = getenv("TASK_NAME");
    char task_name[MAX_TASK_NAME];
    snprintf(task_name, MAX_TASK_NAME, "%s", env_task_name != NULL ? env_task_name : DEFAULT_TASK_NAME);

    const char* env_task_queue_name = getenv("TASK_QUEUE_NAME");
    char task_queue_name[MAX_QUEUE_NAME];
    snprintf(task_queue_name, MAX_QUEUE_NAME, "/%s", env_task_queue_name != NULL ? env_task_queue_name : DEFAULT_TASK_QUEUE);

    log_message(LOG_INFO, task_name, "Starting Task Service...\n");


    task_service_t svc;
    if (init_task_service(&svc, task_name, task_queue_name, DEFAULT_EM_QUEUE) != 0){
        exit_code = -1;
        log_message(LOG_ERROR, svc.task_name, "Failed to initialize Task Service\n");
        goto cleanup_task_service;
    }

    task_context = (task_context_t *)malloc(sizeof(task_context_t));
    if (!task_context){
        exit_code = -1;
        log_message(LOG_ERROR, svc.task_name, "Failed to allocate task context\n");
        goto cleanup_task_service;
    }

    memset(task_context, 0, sizeof(task_context_t));
    pthread_mutex_init(&task_context->lock, NULL);
    task_context->status = IDLE;


    ipc_msg_t in_msg, out_msg;


    /* Main loop: Task Service waits for commands and manages them */
    while(1){
        if(receive_message(svc.rx_fd, &in_msg) > 0){
            switch(in_msg.type){

                /* --- HANDLE NEW TASK REQUEST --- */
                case MSG_TASK_REQUEST:
                    log_message(LOG_INFO, svc.task_name, "Received Task Request\n");
                    if(task_context->status != IDLE){
                        log_message(LOG_INFO, svc.task_name, "Cannot recive request, not IDLE state");
                        
                        /* Send Error ACK */
                        out_msg.type = MSG_TASK_ACK;
                        out_msg.data.ack = ACK_ERROR;
                        send_message(svc.tx_fd, &out_msg);
                        continue;
                    }

                    /* Task Logic */
                    /* Parse Inputs */
                    log_message(LOG_INFO, svc.task_name, "Received JSON: %s\n", in_msg.data.task.input);
                    if (convert_input(in_msg.data.task.input, &task_context->input) != 0) {
                        log_message(LOG_ERROR, svc.task_name, "Error parsing input JSON\n");
                        
                        /* Send Error ACK */
                        out_msg.type = MSG_TASK_ACK;
                        out_msg.data.ack = ACK_ERROR;
                        send_message(svc.tx_fd, &out_msg);
                        continue;
                    }

                    /* Prepare Thread Attributes */
                    pthread_attr_t attr;
                    struct sched_param param;
                    pthread_t thread;
                    int ret;

                    pthread_attr_init(&attr);

                    /* Set Stack Size (Avoid page faults) */
                    pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + 0x4000);

                    /* CPU Affinity */
                    cpu_set_t cpuset;
                    CPU_ZERO(&cpuset);
                    CPU_SET(2, &cpuset);
                    ret = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
                    if (ret != 0) {
                        log_message(LOG_WARN, svc.task_name, "Affinity failed: %s\n", strerror(ret));
                    } 


                    /* Set Scheduling Policy */
                    int policy = SCHED_OTHER; // Default
                    if (in_msg.data.task.policy == SCHED_POLICY_FIFO)    policy = SCHED_FIFO;
                    else if (in_msg.data.task.policy == SCHED_POLICY_RR) policy = SCHED_RR;
                    
                    if (pthread_attr_setschedpolicy(&attr, policy) != 0) {
                        log_message(LOG_WARN, svc.task_name, "Failed to set policy (Need root?)\n");

                        /* Send Error ACK */
                        out_msg.type = MSG_TASK_ACK;
                        out_msg.data.ack = ACK_ERROR;
                        send_message(svc.tx_fd, &out_msg);
                        continue;
                    }
                    
                    /* Set Priority */
                    param.sched_priority = in_msg.data.task.priority;
                    pthread_attr_setschedparam(&attr, &param);

                    /* Inherit Scheduler */
                    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

                    /* Create Thread */
                    ret = pthread_create(&thread, &attr, task_main, task_context);
                    if (ret != 0) {
                        log_message(LOG_WARN, svc.task_name, "Failed to create thread: %s\n", strerror(ret));
                        
                        /* Send Error ACK */
                        out_msg.type = MSG_TASK_ACK;
                        out_msg.data.ack = ACK_ERROR;
                        send_message(svc.tx_fd, &out_msg);
                        continue;
                        
                    }

                    pthread_detach(thread);
                    pthread_attr_destroy(&attr);

                    /* Send ACK */
                    out_msg.type = MSG_TASK_ACK;
                    out_msg.data.ack = ACK_OK;
                    if (send_message(svc.tx_fd, &out_msg) != 0) {
                        log_message(LOG_ERROR, svc.task_name, "Failed to send ACK: %s\n", strerror(errno));
                    }
                    break;


                /* --- HANDLE STATUS REQUEST --- */
                case MSG_GET_STATUS:
                    log_message(LOG_INFO, svc.task_name, "Received Status Request\n");

                    /* Status Logic */
                    out_msg.type = MSG_TASK_STATUS;

                    pthread_mutex_lock(&task_context->lock);
                    out_msg.data.status = task_context->status;
                    pthread_mutex_unlock(&task_context->lock);
                    
                    send_message(svc.tx_fd, &out_msg);

                    break;

                /* --- HANDLE RESULTS REQUEST --- */
                case MSG_GET_RESULTS:
                    log_message(LOG_INFO, svc.task_name, "Received Results Request\n");
                    
                    /* Results Logic */
                    out_msg.type = MSG_TASK_RESULT;

                    pthread_mutex_lock(&task_context->lock);
                    convert_output(&task_context->output, out_msg.data.result);
                    task_context->status = IDLE;
                    pthread_mutex_unlock(&task_context->lock);
                    
                    send_message(svc.tx_fd, &out_msg);
                    break;
                    
                default:
                    log_message(LOG_WARN, svc.task_name, "Received unknown message type %d", in_msg.type);

            }
        }
    }

    
cleanup_task_service:
    close_task_service(&svc);

exit_program:
    return exit_code;
}
