#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <map>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include "proto/task_service.grpc.pb.h"
#include "grpc_client.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using taskservice::TaskExecutor;
using taskservice::TaskRequest;
using taskservice::TaskResponse;

// ============================================
// CHANNEL POOL - Reuse gRPC channels
// ============================================
static std::map<std::string, std::shared_ptr<Channel>> g_channel_pool;
static std::mutex g_channel_pool_mutex;

// Get or create a channel for the given address
static std::shared_ptr<Channel> get_or_create_channel(const char* address) {
    std::lock_guard<std::mutex> lock(g_channel_pool_mutex);
    
    std::string addr_str(address);
    auto it = g_channel_pool.find(addr_str);
    
    if (it != g_channel_pool.end()) {
        // Channel exists, reuse it
        printf("[gRPC Channel Pool] Reusing existing channel for %s\n", address);
        return it->second;
    }
    
    // Create new channel with optimized settings
    printf("[gRPC Channel Pool] Creating new channel for %s\n", address);
    
    grpc::ChannelArguments args;
    // Enable keepalive to maintain connection
    args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 10000);  // 10 seconds
    args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 5000);  // 5 seconds
    args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
    // Increase max concurrent streams
    args.SetInt(GRPC_ARG_MAX_CONCURRENT_STREAMS, 100);
    // Optimize for low latency
    args.SetInt(GRPC_ARG_USE_LOCAL_SUBCHANNEL_POOL, 0);
    
    std::shared_ptr<Channel> channel = grpc::CreateCustomChannel(
        address,
        grpc::InsecureChannelCredentials(),
        args
    );
    
    g_channel_pool[addr_str] = channel;
    return channel;
}

// C interface for calling from C code
extern "C" {
    
// Execute a task via gRPC
// Returns 0 on success, -1 on error
int grpc_execute_task(const char* task_service_address,
                      unsigned int task_id,
                      const char* task_name,
                      const char* inputs_json,
                      int priority,
                      const char* policy,
                      char* result_json_out,
                      int result_json_max_len) {
    
    // Get or create a channel from the pool (reuses existing connections)
    std::shared_ptr<Channel> channel = get_or_create_channel(task_service_address);
    
    // Create stub
    std::unique_ptr<TaskExecutor::Stub> stub = TaskExecutor::NewStub(channel);
    
    // Prepare request
    TaskRequest request;
    request.set_task_id(task_id);
    request.set_task_name(task_name);
    request.set_inputs_json(inputs_json);
    request.set_priority(priority);
    request.set_policy(policy);
    
    // Container for response
    TaskResponse response;
    ClientContext context;
    
    // Set deadline (5 seconds from now)
    std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(5);
    context.set_deadline(deadline);
    // Don't wait for ready - channel should already be connected from pool
    // context.set_wait_for_ready(true);
    
    printf("[gRPC Client] Calling task '%s' at %s\n", task_name, task_service_address);
    printf("[gRPC Client] Inputs: %s\n", inputs_json);
    
    // Make the RPC call
    Status status = stub->ExecuteTask(&context, request, &response);
    
    if (!status.ok()) {
        std::cerr << "[gRPC Client] RPC failed: " << status.error_message() << std::endl;
        return -1;
    }
    
    printf("[gRPC Client] Task completed with status: %s\n", response.status().c_str());
    printf("[gRPC Client] Result: %s\n", response.result_json().c_str());
    
    // Copy result to output buffer
    if (result_json_out && result_json_max_len > 0) {
        snprintf(result_json_out, result_json_max_len, "%s", response.result_json().c_str());
    }
    
    if (response.status() == "COMPLETED") {
        return 0;
    } else {
        std::cerr << "[gRPC Client] Task failed: " << response.error_message() << std::endl;
        return -1;
    }
}

// Execute a task via gRPC asynchronously with streaming responses
int grpc_execute_task_async(const char* task_service_address,
                            unsigned int task_id,
                            const char* task_name,
                            const char* inputs_json,
                            int priority,
                            const char* policy,
                            double client_timestamp_ms,
                            grpc_task_callback_t callback,
                            void* user_data) {
    
    // Get or create a channel from the pool (reuses existing connections)
    std::shared_ptr<Channel> channel = get_or_create_channel(task_service_address);
    
    // Create stub
    std::unique_ptr<TaskExecutor::Stub> stub = TaskExecutor::NewStub(channel);
    
    // Prepare request
    TaskRequest request;
    request.set_task_id(task_id);
    request.set_task_name(task_name);
    request.set_inputs_json(inputs_json);
    request.set_priority(priority);
    request.set_policy(policy);
    request.set_client_timestamp_ms(client_timestamp_ms);
    
    // Client context
    ClientContext context;
    
    // Set deadline (configurable, 60 seconds default for long-running tasks)
    // The gRPC server will check context->IsCancelled() periodically
    // and abort the task thread if deadline expires
    std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(60);
    context.set_deadline(deadline);
    // Don't wait for ready - channel should already be connected from pool
    // context.set_wait_for_ready(true);
    
    printf("[gRPC Client Async] Calling task '%s' at %s\n", task_name, task_service_address);
    printf("[gRPC Client Async] Inputs: %s\n", inputs_json);
    
    // Make the streaming RPC call
    std::unique_ptr<grpc::ClientReader<TaskResponse>> reader = 
        stub->ExecuteTaskAsync(&context, request);
    
    // Read streaming responses
    TaskResponse response;
    int response_count = 0;
    
    while (reader->Read(&response)) {
        response_count++;
        
        std::string status = response.status();
        std::string result_json = response.result_json();
        std::string error_message = response.error_message();
        double t2_timestamp = response.t2_thread_start_ms();
        double t3_timestamp = response.t3_task_complete_ms();
        
        printf("[gRPC Client Async] Response #%d - Status: %s\n", response_count, status.c_str());
        
        // Call user callback
        if (callback) {
            callback(
                response.task_id(),
                status.c_str(),
                result_json.c_str(),
                error_message.c_str(),
                t2_timestamp,
                t3_timestamp,
                user_data
            );
        }
        
        // If error, stop reading
        if (status == "ERROR") {
            break;
        }
    }
    
    // Check final status
    Status grpc_status = reader->Finish();
    if (!grpc_status.ok()) {
        std::cerr << "[gRPC Client Async] RPC failed: " << grpc_status.error_message() << std::endl;
        // Notify scheduler of the error so it doesn't hang waiting for a completion event
        if (callback) {
            callback(task_id, "ERROR", "", grpc_status.error_message().c_str(), 0.0, 0.0, user_data);
        }
        return -1;
    }
    
    printf("[gRPC Client Async] Stream completed, received %d responses\n", response_count);
    return 0;
}

// Internal structure for cancellable call
struct GrpcCallContext {
    std::shared_ptr<Channel> channel;
    std::unique_ptr<TaskExecutor::Stub> stub;
    ClientContext context;
    std::unique_ptr<grpc::ClientReader<TaskResponse>> reader;
    bool cancelled;
    
    GrpcCallContext() : cancelled(false) {}
};

// Execute a task via gRPC with cancellation support
grpc_call_handle_t grpc_execute_task_async_cancellable(
                            const char* task_service_address,
                            unsigned int task_id,
                            const char* task_name,
                            const char* inputs_json,
                            int priority,
                            const char* policy,
                            double client_timestamp_ms,
                            grpc_task_callback_t callback,
                            void* user_data) {
    
    // Allocate call context
    GrpcCallContext* ctx = new GrpcCallContext();
    
    // Get or create channel from pool
    ctx->channel = get_or_create_channel(task_service_address);
    
    // Create stub
    ctx->stub = TaskExecutor::NewStub(ctx->channel);
    
    // Prepare request
    TaskRequest request;
    request.set_task_id(task_id);
    request.set_task_name(task_name);
    request.set_inputs_json(inputs_json);
    request.set_priority(priority);
    request.set_policy(policy);
    request.set_client_timestamp_ms(client_timestamp_ms);
    
    // Set deadline (60 seconds)
    std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(60);
    ctx->context.set_deadline(deadline);
    // Don't wait for ready - channel should already be connected from pool
    // ctx->context.set_wait_for_ready(true);
    
    printf("[gRPC Client Cancellable] Calling task '%s' at %s\n", task_name, task_service_address);
    
    // Make the streaming RPC call
    ctx->reader = ctx->stub->ExecuteTaskAsync(&ctx->context, request);
    
    // Read streaming responses
    TaskResponse response;
    int response_count = 0;
    
    while (ctx->reader->Read(&response)) {
        response_count++;
        
        std::string status = response.status();
        std::string result_json = response.result_json();
        std::string error_message = response.error_message();
        double t2_timestamp = response.t2_thread_start_ms();
        double t3_timestamp = response.t3_task_complete_ms();
        
        printf("[gRPC Client Cancellable] Response #%d - Status: %s\n", response_count, status.c_str());
        
        // Check if cancelled
        if (ctx->cancelled) {
            printf("[gRPC Client Cancellable] Call was cancelled\n");
            break;
        }
        
        // Call user callback
        if (callback) {
            callback(
                response.task_id(),
                status.c_str(),
                result_json.c_str(),
                error_message.c_str(),
                t2_timestamp,
                t3_timestamp,
                user_data
            );
        }
        
        // If error or cancelled status, stop reading
        if (status == "ERROR" || status == "CANCELLED") {
            break;
        }
    }
    
    // Check final status
    Status grpc_status = ctx->reader->Finish();
    if (!grpc_status.ok() && !ctx->cancelled) {
        std::cerr << "[gRPC Client Cancellable] RPC failed: " << grpc_status.error_message() << std::endl;
    }
    
    printf("[gRPC Client Cancellable] Stream completed, received %d responses\n", response_count);
    
    delete ctx;
    return nullptr;  // Context deleted after call completes
}

// Cancel an ongoing gRPC task call
void grpc_cancel_task(grpc_call_handle_t handle) {
    if (!handle) {
        printf("[gRPC Client] Cannot cancel NULL handle\n");
        return;
    }
    
    GrpcCallContext* ctx = static_cast<GrpcCallContext*>(handle);
    
    printf("[gRPC Client] Cancelling gRPC call...\n");
    ctx->cancelled = true;
    ctx->context.TryCancel();
}

} // extern "C"
