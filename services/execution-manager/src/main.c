#include <stdbool.h>
#include "logger.h"
#include "execution_manager.h"
#include "task.h"
#include "schedule.h"
#include "task_ipc.h"

int main(int argc, char *argv[]){

    /* Execution Manager Initializzaztion */
    int exit_code = 0;

    execution_manager_t em;
    if (init_execution_manager(&em, DEFAULT_EXECUTION_MANAGER_NAME, DEFAULT_EM_QUEUE) != 0){
        exit_code = -1;
        log_message(LOG_ERROR, em.execution_manager_name, "Failed to initialize Execution Manager\n");
        goto cleanup_em;
    }

    /* Prepera datas */
    schedule_t sched;


    /* Wait for New Schedule */
    bool new_schedule = true;

    /* Start Execution Managment */
    ipc_msg_t in_msg, out_msg;
    while(1){


        /* Compare Schedule Version */

        
        if(new_schedule){
            /* Set new schedule */

            if (init_schedule(&sched, "Schedule", "0.1") != 0){
                exit_code = -1;
                log_message(LOG_ERROR, em.execution_manager_name, "Failed to initialize Schedule\n");
                goto cleanup;
            }

            task_t task_1;
            if (init_task(&task_1,"sum", SCHED_POLICY_FIFO, 3, "{\"a\": 10, \"b\": 12}", "{\"result\": 0}") != 0){
                exit_code = -1;
                log_message(LOG_ERROR, em.execution_manager_name, "Failed to initialize Task\n");
                goto cleanup;
            }

            if(add_task_to_schedule(&sched, task_1) != 0){
                exit_code = -1;
                log_message(LOG_ERROR, em.execution_manager_name, "Failed to add Task to Schedule\n");
                goto cleanup;
            }

            task_t task_2;
            if (init_task(&task_2,"multiply", SCHED_POLICY_FIFO, 2, "{\"a\": 10, \"b\": 2}", "{\"result\": 0}") != 0){
                exit_code = -1;
                log_message(LOG_ERROR, em.execution_manager_name, "Failed to add Task to Schedule\n");
                goto cleanup;
            }

            if(add_task_to_schedule(&sched, task_2) != 0){
                exit_code = -1;
                log_message(LOG_ERROR, em.execution_manager_name, "Failed to add Task to Schedule\n");
                goto cleanup;
            }

            task_t task_3;
            if (init_task(&task_3,"divide", SCHED_POLICY_FIFO, 1, "{\"a\": 10, \"b\": 5}", "{\"result\": 0}") != 0){
                exit_code = -1;
                log_message(LOG_ERROR, em.execution_manager_name, "Failed to add Task to Schedule\n");
                goto cleanup;
            }

            if(add_task_to_schedule(&sched, task_3) != 0){
                exit_code = -1;
                log_message(LOG_ERROR, em.execution_manager_name, "Failed to add Task to Schedule\n");
                goto cleanup;
            }


            if (set_execution_manager_schedule(&em, &sched) != 0){
                exit_code = -1;
                log_message(LOG_ERROR, em.execution_manager_name, "Failed to set Execution Manager Schedule\n");
                goto cleanup;
            }


            
            new_schedule = false;
        }

        /* Run The Schedule */
        for(int i = 0; i < em.schedule.num_tasks; i++){

            /* Craft the message REQUEST */
            out_msg =  (ipc_msg_t){0};
            out_msg.type = MSG_TASK_REQUEST;
            out_msg.data.task = em.schedule.tasks[i];

            /* Send the message REQUEST */
            log_message(LOG_INFO, em.execution_manager_name, "Sending Task Request to %s\n",em.schedule.tasks[i].task_name);
            if(send_message(em.tx_fd[i], &out_msg) != 0){
                exit_code = -1;
                log_message(LOG_ERROR, em.execution_manager_name, "Failed to send message to task queue\n");
                goto cleanup;
            }

            /* Receive the ACK {OK, ERROR} */
            bool ack_ok = false;
            while(!ack_ok){
                if (receive_message(em.rx_fd, &in_msg) > 0){
                    if (in_msg.type == MSG_TASK_ACK){
                        switch(in_msg.data.ack){
                            case ACK_OK:
                                log_message(LOG_INFO, em.execution_manager_name, "Received Task ACK with OK\n");
                                ack_ok = true;
                                break;    
                            case ACK_ERROR:
                                log_message(LOG_INFO, em.execution_manager_name, "Received Task ACK with ERROR\n");
                                break;
                            default:
                                log_message(LOG_INFO, em.execution_manager_name, "Received Unknown Task ACK\n");
                        }
                    } else {
                        log_message(LOG_WARN, em.execution_manager_name, "Received unknown message type %d", in_msg.type);
                    }
                    
            
                }
            }

            /* Receive the Task REsult  */
            bool result_received= false;
            while(!result_received){
                /* Craft GET STATUS message */
                out_msg =  (ipc_msg_t){0};
                out_msg.type = MSG_GET_STATUS;

                /* Send GET STATUS message */
                log_message(LOG_INFO, em.execution_manager_name, "Sending Status Request to %s\n",em.schedule.tasks[i].task_name);
                if(send_message(em.tx_fd[i], &out_msg) != 0){
                    exit_code = -1;
                    log_message(LOG_ERROR, em.execution_manager_name, "Failed to send message to task queue\n");
                    goto cleanup;
                }

                /* Receive Task Status */
                if (receive_message(em.rx_fd, &in_msg) > 0){
                    if (in_msg.type == MSG_TASK_STATUS){
                        switch(in_msg.data.status){
                            
                            case IDLE:
                                log_message(LOG_INFO, em.execution_manager_name, "Received Task Status IDLE\n");
                                result_received = true;
                                break;

                            case RUNNING:
                                log_message(LOG_INFO, em.execution_manager_name, "Received Task Status RUNNING\n");
                                break;

                            case COMPLETED:
                                log_message(LOG_INFO, em.execution_manager_name, "Received Task Status COMPLETED\n");

                                /* Craft GET RESULTS message */
                                out_msg =  (ipc_msg_t){0};
                                out_msg.type = MSG_GET_RESULTS;

                                /* Send GET RESULTS message */
                                log_message(LOG_INFO, em.execution_manager_name, "Sending Results Request to %s\n",em.schedule.tasks[i].task_name);
                                if(send_message(em.tx_fd[i], &out_msg) !=  0){
                                    exit_code = -1;
                                    log_message(LOG_ERROR, em.execution_manager_name, "Failed to send message to task queue\n");
                                    goto cleanup;  
                                }

                                /* Receive Task Results */
                                if (receive_message(em.rx_fd, &in_msg) > 0){
                                    if (in_msg.type == MSG_TASK_RESULT){
                                        log_message(LOG_INFO, em.execution_manager_name, "Received Task Results: %s\n", in_msg.data.result);
                                    }else{
                                        log_message(LOG_INFO, em.execution_manager_name, "Received Unknown Task Results\n");
                                    }

                                }
                                result_received = true;
                                break;                            
                            default:
                                log_message(LOG_INFO, em.execution_manager_name, "Received Unknown Task Status\n");
                        
                        }
                    } else {
                        log_message(LOG_WARN, em.execution_manager_name, "Received unknown message type %d", in_msg.type);
                    }
                }

                
            }
                    




        }

        sleep(1);
        
    }

cleanup:
cleanup_em:
    close_execution_manager(&em);

exit_program:
    return exit_code;

}