#include <iostream>
#include <memory>
#include <string>
#include <pthread.h>
#include <sched.h>
#include <limits.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>
#include <grpcpp/grpcpp.h>
#include "proto/task_service.grpc.pb.h"

// Include C headers
extern "C" {
    #include "../include/app_task.h"
    #include "../include/logger.h"
}

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;
using taskservice::TaskExecutor;
using taskservice::TaskRequest;
using taskservice::TaskResponse;

// Implementation of TaskExecutor service
class TaskExecutorServiceImpl final : public TaskExecutor::Service {
    Status ExecuteTask(ServerContext* context, 
                      const TaskRequest* request,
                      TaskResponse* response) override {
        
        printf("[gRPC Server] Received task: %s (ID: %u)\n", 
               request->task_name().c_str(), request->task_id());
        
        // Allocate task context
        task_context_t* ctx = (task_context_t*)malloc(sizeof(task_context_t));
        if (!ctx) {
            response->set_status("ERROR");
            response->set_error_message("Memory allocation failed");
            return Status::OK;
        }
        
        pthread_mutex_init(&ctx->lock, NULL);
        ctx->status = IDLE;
        
        // Convert JSON input to input_t structure
        std::string inputs_json = request->inputs_json();
        if (convert_input((char*)inputs_json.c_str(), &ctx->input) != 0) {
            response->set_status("ERROR");
            response->set_error_message("Failed to parse inputs");
            pthread_mutex_destroy(&ctx->lock);
            free(ctx);
            return Status::OK;
        }
        
        printf("[gRPC Server] Input parsed successfully\n");
        
        // Prepare thread attributes for real-time execution
        pthread_attr_t attr;
        struct sched_param param;
        pthread_t task_thread;
        
        pthread_attr_init(&attr);
        
        // Set stack size (avoid page faults)
        pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + 0x4000);
        
        // Set CPU affinity (CPU 2)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(2, &cpuset);
        int ret = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
        if (ret != 0) {
            printf("[gRPC Server] Warning: CPU affinity failed\n");
        }
        
        // Set scheduling policy based on request
        int policy = SCHED_OTHER; // Default
        std::string policy_str = request->policy();
        if (policy_str == "fifo") {
            policy = SCHED_FIFO;
        } else if (policy_str == "rr") {
            policy = SCHED_RR;
        }
        
        if (pthread_attr_setschedpolicy(&attr, policy) != 0) {
            printf("[gRPC Server] Warning: Failed to set RT policy (need root privileges)\n");
        }
        
        // Set priority
        param.sched_priority = request->priority();
        pthread_attr_setschedparam(&attr, &param);
        
        // Use explicit scheduler inheritance
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        
        // Create real-time thread
        if (pthread_create(&task_thread, &attr, task_main, ctx) != 0) {
            response->set_status("ERROR");
            response->set_error_message("Failed to create task thread");
            pthread_attr_destroy(&attr);
            pthread_mutex_destroy(&ctx->lock);
            free(ctx);
            return Status::OK;
        }
        
        pthread_attr_destroy(&attr);
        
        // Wait for task completion
        pthread_join(task_thread, NULL);
        
        printf("[gRPC Server] Task completed\n");
        
        // Convert output to JSON
        char result_json[MAX_TASK_JSON_OUT];
        if (convert_output(&ctx->output, result_json) < 0) {
            response->set_status("ERROR");
            response->set_error_message("Failed to serialize output");
            pthread_mutex_destroy(&ctx->lock);
            free(ctx);
            return Status::OK;
        }
        
        // Set response
        response->set_task_id(request->task_id());
        response->set_status("COMPLETED");
        response->set_result_json(result_json);
        
        printf("[gRPC Server] Result: %s\n", result_json);
        
        // Cleanup
        pthread_mutex_destroy(&ctx->lock);
        free(ctx);
        
        return Status::OK;
    }
    
    // Asynchronous task execution with streaming responses
    Status ExecuteTaskAsync(ServerContext* context,
                           const TaskRequest* request,
                           ServerWriter<TaskResponse>* writer) override {
        
        printf("[gRPC Server Async] Received task: %s (ID: %u)\n", 
               request->task_name().c_str(), request->task_id());
        
        // Allocate task context
        task_context_t* ctx = (task_context_t*)malloc(sizeof(task_context_t));
        if (!ctx) {
            TaskResponse error_response;
            error_response.set_task_id(request->task_id());
            error_response.set_status("ERROR");
            error_response.set_error_message("Memory allocation failed");
            writer->Write(error_response);
            return Status::OK;
        }
        
        pthread_mutex_init(&ctx->lock, NULL);
        ctx->status = IDLE;
        
        // Convert JSON input to input_t structure
        std::string inputs_json = request->inputs_json();
        if (convert_input((char*)inputs_json.c_str(), &ctx->input) != 0) {
            TaskResponse error_response;
            error_response.set_task_id(request->task_id());
            error_response.set_status("ERROR");
            error_response.set_error_message("Failed to parse inputs");
            writer->Write(error_response);
            pthread_mutex_destroy(&ctx->lock);
            free(ctx);
            return Status::OK;
        }
        
        printf("[gRPC Server Async] Input parsed successfully\n");
        
        // Prepare thread attributes for real-time execution
        pthread_attr_t attr;
        struct sched_param param;
        pthread_t task_thread;
        bool use_rt = true;
        
        pthread_attr_init(&attr);
        
        // Set stack size (avoid page faults)
        int stack_ret = pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + 0x4000);
        printf("[gRPC Server Async] Set stack size: %s\n", stack_ret == 0 ? "OK" : "FAILED");
        
        // NOTE: CPU affinity will be set AFTER thread creation
        // Setting it in pthread_attr causes EINVAL with SCHED_FIFO
        
        // Set scheduling policy based on request
        int policy = SCHED_OTHER;
        std::string policy_str = request->policy();
        int priority_val = request->priority();
        
        printf("[gRPC Server Async] Requested policy='%s', priority=%d\n", policy_str.c_str(), priority_val);
        
        if (policy_str == "fifo") {
            policy = SCHED_FIFO;
        } else if (policy_str == "rr") {
            policy = SCHED_RR;
        }
        
        if (policy != SCHED_OTHER) {
            printf("[gRPC Server Async] Setting SCHED_FIFO with priority %d\n", priority_val);
            
            // IMPORTANT: Use PTHREAD_EXPLICIT_SCHED to not inherit from parent
            int sched_ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
            printf("[gRPC Server Async] Set inherit sched: %s (ret=%d)\n", 
                   sched_ret == 0 ? "OK" : "FAILED", sched_ret);
            
            int policy_ret = pthread_attr_setschedpolicy(&attr, policy);
            printf("[gRPC Server Async] Set policy SCHED_FIFO: %s (ret=%d)\n",
                   policy_ret == 0 ? "OK" : "FAILED", policy_ret);
            
            if (policy_ret != 0) {
                printf("[gRPC Server Async] Warning: Failed to set RT policy, using SCHED_OTHER\n");
                policy = SCHED_OTHER;
                use_rt = false;
            } else {
                // Set priority only if RT policy succeeded
                param.sched_priority = priority_val;
                int param_ret = pthread_attr_setschedparam(&attr, &param);
                printf("[gRPC Server Async] Set priority %d: %s (ret=%d)\n",
                       priority_val, param_ret == 0 ? "OK" : "FAILED", param_ret);
                
                if (param_ret == 0) {
                    printf("[gRPC Server Async] RT attributes configured successfully\n");
                } else {
                    use_rt = false;
                }
            }
        }
        
        // Create thread with RT attributes (or fallback to default if RT failed)
        int ret = pthread_create(&task_thread, &attr, task_main, ctx);
        pthread_attr_destroy(&attr);
        
        if (ret != 0) {
            // RT failed, try again with no attributes
            printf("[gRPC Server Async] Thread creation with RT failed (errno=%d), trying without RT...\n", ret);
            use_rt = false;  // ← FIX: aggiorna flag!
            ret = pthread_create(&task_thread, NULL, task_main, ctx);
        }
        
        if (ret != 0) {
            TaskResponse error_response;
            error_response.set_task_id(request->task_id());
            error_response.set_status("ERROR");
            error_response.set_error_message("Failed to create task thread");
            writer->Write(error_response);
            pthread_mutex_destroy(&ctx->lock);
            free(ctx);
            return Status::OK;
        }
        
        printf("[gRPC Server Async] Thread created successfully (RT=%s)\n", use_rt ? "yes" : "no");
        
        // Set CPU affinity on the created thread (must be done after creation when using SCHED_FIFO)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(2, &cpuset);  // Pin to CPU 2
        int affinity_ret = pthread_setaffinity_np(task_thread, sizeof(cpu_set_t), &cpuset);
        if (affinity_ret == 0) {
            printf("[gRPC Server Async] CPU affinity set to CPU 2\n");
        } else {
            printf("[gRPC Server Async] Warning: Failed to set CPU affinity (errno=%d)\n", affinity_ret);
        }
        
        // ✅ SEND IMMEDIATE ACK - Task started!
        TaskResponse ack_response;
        ack_response.set_task_id(request->task_id());
        ack_response.set_status("STARTED");
        ack_response.set_result_json("");
        writer->Write(ack_response);
        
        printf("[gRPC Server Async] ACK sent, task started\n");
        
        // Wait for task completion with cancellation support
        // Check every 100ms if client has cancelled
        bool task_completed = false;
        bool task_cancelled = false;
        
        while (!task_completed && !task_cancelled) {
            // Check if client has cancelled the request
            if (context->IsCancelled()) {
                printf("[gRPC Server Async] ⚠️ Client cancelled request! Aborting task thread...\n");
                
                // Cancel the pthread (sends SIGCANCEL)
                pthread_cancel(task_thread);
                
                // Wait a short time for thread to cleanup
                struct timespec timeout = {0, 100000000};  // 100ms
                nanosleep(&timeout, NULL);
                
                // Send CANCELLED response
                TaskResponse cancel_response;
                cancel_response.set_task_id(request->task_id());
                cancel_response.set_status("CANCELLED");
                cancel_response.set_error_message("Task aborted due to timeout or cancellation");
                writer->Write(cancel_response);
                
                task_cancelled = true;
                
                printf("[gRPC Server Async] Task thread cancelled\n");
                break;
            }
            
            // Check if task thread has completed (non-blocking check)
            struct timespec timeout = {0, 100000000};  // 100ms
            int join_ret = pthread_timedjoin_np(task_thread, NULL, &timeout);
            
            if (join_ret == 0) {
                // Thread completed successfully
                task_completed = true;
                printf("[gRPC Server Async] Task completed normally\n");
            } else if (join_ret == ETIMEDOUT) {
                // Thread still running, continue checking
                continue;
            } else {
                // Some error occurred
                printf("[gRPC Server Async] Warning: pthread_timedjoin_np returned %d\n", join_ret);
                break;
            }
        }
        
        // If task was cancelled, cleanup and return
        if (task_cancelled) {
            pthread_mutex_destroy(&ctx->lock);
            free(ctx);
            return Status::OK;
        }
        
        // Convert output to JSON
        char result_json[MAX_TASK_JSON_OUT];
        if (convert_output(&ctx->output, result_json) < 0) {
            TaskResponse error_response;
            error_response.set_task_id(request->task_id());
            error_response.set_status("ERROR");
            error_response.set_error_message("Failed to serialize output");
            writer->Write(error_response);
            pthread_mutex_destroy(&ctx->lock);
            free(ctx);
            return Status::OK;
        }
        
        // ✅ SEND RESULT - Task completed!
        TaskResponse result_response;
        result_response.set_task_id(request->task_id());
        result_response.set_status("COMPLETED");
        result_response.set_result_json(result_json);
        writer->Write(result_response);
        
        printf("[gRPC Server Async] Result sent: %s\n", result_json);
        
        // Cleanup
        pthread_mutex_destroy(&ctx->lock);
        free(ctx);
        
        return Status::OK;
    }
};

void RunServer(const std::string& server_address) {
    TaskExecutorServiceImpl service;
    
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[gRPC Server] Listening on " << server_address << std::endl;
    
    server->Wait();
}

int main(int argc, char** argv) {
    // Memory locking for real-time determinism
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        std::cerr << "[gRPC Server] Warning: mlockall failed (need root privileges)" << std::endl;
    }
    
    // Get task name from environment or default to port 50051
    const char* task_name = getenv("TASK_NAME");
    std::string server_address = "0.0.0.0:50051";
    
    if (task_name) {
        std::cout << "[gRPC Server] Starting server for task: " << task_name << std::endl;
    }
    
    RunServer(server_address);
    
    return 0;
}
