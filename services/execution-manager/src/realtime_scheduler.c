#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <hiredis/hiredis.h>
#include "grpc_client.h"

#define MAX_TASKS 100
#define MAX_EPOLL_EVENTS 20

// Task status
typedef enum {
    TASK_STATUS_PENDING,
    TASK_STATUS_RUNNING,
    TASK_STATUS_COMPLETED,
    TASK_STATUS_TIMEOUT,
    TASK_STATUS_ERROR
} task_status_t;

// Event types for epoll
typedef enum {
    EVENT_TYPE_START_TIMER,
    EVENT_TYPE_TIMEOUT_TIMER,
    EVENT_TYPE_TASK_COMPLETION
} event_type_t;

// Scheduled task structure
typedef struct {
    int task_id;
    char task_name[128];
    char service_address[256];
    char inputs_json[4096];
    int priority;
    char policy[32];
    
    // Timing (in milliseconds)
    uint64_t start_time_ms;    // Relative to schedule start
    uint64_t deadline_ms;      // Absolute from schedule start
    
    // Execution timing (for benchmarking)
    struct timespec execution_start_time;  // Timestamp when task execution started
    
    // Timers
    int start_timer_fd;
    int timeout_timer_fd;
    
    // State
    task_status_t status;
    pthread_t grpc_thread;
    pthread_mutex_t lock;
    
    // gRPC handle for cancellation
    grpc_call_handle_t grpc_handle;
    
} scheduled_task_t;

// Event data for epoll
typedef struct {
    event_type_t type;
    scheduled_task_t *task;
} epoll_event_data_t;

// Global state
static struct timespec g_schedule_start;
static int g_completion_eventfd = -1;
static scheduled_task_t g_tasks[MAX_TASKS];
static int g_num_tasks = 0;

// Helper: Get elapsed time in milliseconds since schedule start
static uint64_t get_elapsed_ms() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    uint64_t start_ms = g_schedule_start.tv_sec * 1000ULL + g_schedule_start.tv_nsec / 1000000ULL;
    uint64_t now_ms = now.tv_sec * 1000ULL + now.tv_nsec / 1000000ULL;
    
    return now_ms - start_ms;
}

// Helper: Convert milliseconds to timespec
static void ms_to_timespec(uint64_t ms, struct timespec *ts) {
    ts->tv_sec = ms / 1000;
    ts->tv_nsec = (ms % 1000) * 1000000UL;
}

// Callback wrapper that notifies event loop when task completes
static void scheduler_task_callback(unsigned int task_id,
                                   const char* status,
                                   const char* result_json,
                                   const char* error_message,
                                   void* user_data) {
    scheduled_task_t *task = (scheduled_task_t *)user_data;
    
    if (strcmp(status, "STARTED") == 0) {
        // Record execution start time for benchmarking
        clock_gettime(CLOCK_MONOTONIC, &task->execution_start_time);
        printf("✅ [T=%lu ms] Task %d '%s' STARTED\n", 
               get_elapsed_ms(), task->task_id, task->task_name);
    } else if (strcmp(status, "COMPLETED") == 0) {
        // Calculate execution time
        struct timespec end_time;
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        
        long long elapsed_ns = (end_time.tv_sec - task->execution_start_time.tv_sec) * 1000000000LL + 
                               (end_time.tv_nsec - task->execution_start_time.tv_nsec);
        long long elapsed_us = elapsed_ns / 1000;
        double elapsed_ms = elapsed_us / 1000.0;
        
        printf("🎉 [T=%lu ms] Task %d '%s' COMPLETED\n", 
               get_elapsed_ms(), task->task_id, task->task_name);
        printf("   [EXECUTION TIME] %lld µs (%.3f ms)\n", elapsed_us, elapsed_ms);
        printf("   [RESULT] %s\n", result_json);
        
        pthread_mutex_lock(&task->lock);
        if (task->status == TASK_STATUS_RUNNING) {
            task->status = TASK_STATUS_COMPLETED;
        }
        pthread_mutex_unlock(&task->lock);
        
        // Notify event loop
        uint64_t notify = task->task_id;
        write(g_completion_eventfd, &notify, sizeof(notify));
        
    } else if (strcmp(status, "ERROR") == 0) {
        printf("❌ [T=%lu ms] Task %d '%s' ERROR: %s\n", 
               get_elapsed_ms(), task->task_id, task->task_name, error_message);
        
        pthread_mutex_lock(&task->lock);
        task->status = TASK_STATUS_ERROR;
        pthread_mutex_unlock(&task->lock);
        
        // Notify event loop
        uint64_t notify = task->task_id;
        write(g_completion_eventfd, &notify, sizeof(notify));
        
    } else if (strcmp(status, "CANCELLED") == 0) {
        printf("⚠️ [T=%lu ms] Task %d '%s' CANCELLED\n",
               get_elapsed_ms(), task->task_id, task->task_name);
    }
}

// Thread function for gRPC call
typedef struct {
    scheduled_task_t *task;
} grpc_thread_args_t;

static void *grpc_task_thread(void *arg) {
    grpc_thread_args_t *args = (grpc_thread_args_t *)arg;
    scheduled_task_t *task = args->task;
    
    printf("[GRPC THREAD %d] Starting...\n", task->task_id);
    
    // Note: grpc_execute_task_async is blocking, but runs in separate thread
    // so main scheduler thread remains free
    // Cancellation is handled by gRPC server checking context->IsCancelled()
    // which happens when deadline expires
    grpc_execute_task_async(
        task->service_address,
        task->task_id,
        task->task_name,
        task->inputs_json,
        task->priority,
        task->policy,
        scheduler_task_callback,
        (void*)task
    );
    
    printf("[GRPC THREAD %d] Completed\n", task->task_id);
    
    free(args);
    return NULL;
}

// Launch a task at its scheduled time
static void launch_task(scheduled_task_t *task) {
    printf("[SCHEDULER] ⏰ T=%lu ms: Launching task %d '%s' (deadline: %lu ms)\n",
           get_elapsed_ms(), task->task_id, task->task_name, task->deadline_ms);
    
    pthread_mutex_lock(&task->lock);
    task->status = TASK_STATUS_RUNNING;
    pthread_mutex_unlock(&task->lock);
    
    // Create thread for gRPC call
    grpc_thread_args_t *args = malloc(sizeof(grpc_thread_args_t));
    args->task = task;
    
    pthread_create(&task->grpc_thread, NULL, grpc_task_thread, args);
    pthread_detach(task->grpc_thread);
    
    // Arm timeout timer if deadline is set
    if (task->deadline_ms > 0 && task->timeout_timer_fd >= 0) {
        uint64_t now_ms = get_elapsed_ms();
        
        if (task->deadline_ms > now_ms) {
            uint64_t timeout_ms = task->deadline_ms - now_ms;
            
            struct itimerspec timeout_spec = {0};
            ms_to_timespec(timeout_ms, &timeout_spec.it_value);
            
            timerfd_settime(task->timeout_timer_fd, 0, &timeout_spec, NULL);
            
            printf("[SCHEDULER] 🕐 Timeout armed for task %d at T=%lu ms (in %lu ms)\n",
                   task->task_id, task->deadline_ms, timeout_ms);
        }
    }
}

// Abort a task that exceeded its deadline
static void abort_task_timeout(scheduled_task_t *task) {
    printf("[SCHEDULER] ⚠️ T=%lu ms: TIMEOUT! Aborting task %d '%s'\n",
           get_elapsed_ms(), task->task_id, task->task_name);
    
    pthread_mutex_lock(&task->lock);
    
    if (task->status == TASK_STATUS_RUNNING) {
        task->status = TASK_STATUS_TIMEOUT;
        
        // Cancel gRPC call (if cancellation was implemented)
        if (task->grpc_handle) {
            printf("[SCHEDULER] 🔪 Cancelling gRPC call for task %d\n", task->task_id);
            grpc_cancel_task(task->grpc_handle);
        }
        
        printf("[SCHEDULER] ❌ Task %d aborted due to timeout\n", task->task_id);
    } else {
        printf("[SCHEDULER] Task %d already completed, timeout ignored\n", task->task_id);
    }
    
    pthread_mutex_unlock(&task->lock);
}

// Handle task completion
static void handle_task_completion(scheduled_task_t *task) {
    printf("[SCHEDULER] ✅ T=%lu ms: Task %d '%s' completed with status %d\n",
           get_elapsed_ms(), task->task_id, task->task_name, task->status);
    
    pthread_mutex_lock(&task->lock);
    
    if (task->status == TASK_STATUS_COMPLETED) {
        // Disarm timeout timer
        if (task->timeout_timer_fd >= 0) {
            struct itimerspec disarm = {0};
            timerfd_settime(task->timeout_timer_fd, 0, &disarm, NULL);
            printf("[SCHEDULER] 🔕 Timeout disarmed for task %d\n", task->task_id);
        }
    }
    
    pthread_mutex_unlock(&task->lock);
}

// Main function to execute schedule with event loop
void execute_schedule_with_event_loop(redisContext *redis, int num_tasks) {
    printf("\n============================================\n");
    printf("   REAL-TIME EVENT-DRIVEN SCHEDULER\n");
    printf("============================================\n\n");
    
    // Initialize global state
    clock_gettime(CLOCK_MONOTONIC, &g_schedule_start);
    g_num_tasks = num_tasks;
    
    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        return;
    }
    
    // Create eventfd for task completion notifications
    g_completion_eventfd = eventfd(0, EFD_NONBLOCK);
    
    epoll_event_data_t *completion_event_data = malloc(sizeof(epoll_event_data_t));
    completion_event_data->type = EVENT_TYPE_TASK_COMPLETION;
    completion_event_data->task = NULL;
    
    struct epoll_event completion_ev = {
        .events = EPOLLIN,
        .data.ptr = completion_event_data
    };
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, g_completion_eventfd, &completion_ev);
    
    printf("[SCHEDULER] 🚀 Schedule started at T=0\n");
    
    // Load tasks from Redis and setup timers
    for (int i = 0; i < num_tasks; i++) {
        scheduled_task_t *task = &g_tasks[i];
        task->task_id = i + 1;
        
        // Read task from Redis
        char key[64];
        snprintf(key, sizeof(key), "scheduletask:%d", task->task_id);
        
        redisReply *reply = redisCommand(redis, "HGETALL %s", key);
        if (!reply || reply->type != REDIS_REPLY_ARRAY) {
            printf("[SCHEDULER] Failed to read task %d from Redis\n", task->task_id);
            if (reply) freeReplyObject(reply);
            continue;
        }
        
        // Parse task data
        char *task_name = NULL;
        char *task_policy = NULL;
        int task_priority = 50;
        int task_start = 0;
        int task_deadline = 0;
        char *task_inputs = NULL;
        
        for (size_t j = 0; j < reply->elements; j += 2) {
            char *field = reply->element[j]->str;
            char *value = reply->element[j+1]->str;
            
            if (strcmp(field, "name") == 0) {
                task_name = value;
            } else if (strcmp(field, "policy") == 0) {
                task_policy = value;
            } else if (strcmp(field, "priority") == 0) {
                task_priority = atoi(value);
            } else if (strcmp(field, "start") == 0) {
                task_start = atoi(value);
            } else if (strcmp(field, "deadline") == 0) {
                task_deadline = atoi(value);
            } else if (strcmp(field, "inputs") == 0) {
                task_inputs = value;
            }
        }
        
        if (!task_name || !task_inputs) {
            printf("[SCHEDULER] Missing data for task %d\n", task->task_id);
            freeReplyObject(reply);
            continue;
        }
        
        // Convert input JSON from nested to simple format
        // From: {"a": {"type": "int", "value": 10}} 
        // To: {"a": 10}
        char simple_inputs[4096] = "{";
        const char* search_ptr = task_inputs;
        int first_field = 1;
        
        while (1) {
            const char* quote1 = strchr(search_ptr, '\"');
            if (!quote1) break;
            quote1++;
            
            const char* quote2 = strchr(quote1, '\"');
            if (!quote2) break;
            
            char key[64];
            size_t key_len = quote2 - quote1;
            if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
            memcpy(key, quote1, key_len);
            key[key_len] = '\0';
            
            if (strcmp(key, "type") == 0 || strcmp(key, "value") == 0) {
                search_ptr = quote2 + 1;
                continue;
            }
            
            const char* value_str = strstr(quote2, "\"value\"");
            if (!value_str || (value_str - quote2) > 100) {
                search_ptr = quote2 + 1;
                continue;
            }
            
            const char* num_start = value_str + 7;
            while (*num_start && (*num_start == ' ' || *num_start == ':')) num_start++;
            
            char num[32];
            int ni = 0;
            while (*num_start && ((*num_start >= '0' && *num_start <= '9') || *num_start == '-' || *num_start == '.')) {
                if (ni < sizeof(num) - 1) num[ni++] = *num_start;
                num_start++;
            }
            num[ni] = '\0';
            
            if (ni > 0) {
                if (!first_field) strcat(simple_inputs, ", ");
                strcat(simple_inputs, "\"");
                strcat(simple_inputs, key);
                strcat(simple_inputs, "\": ");
                strcat(simple_inputs, num);
                first_field = 0;
            }
            
            search_ptr = num_start;
        }
        strcat(simple_inputs, "}");
        
        // Fill task structure
        strncpy(task->task_name, task_name, sizeof(task->task_name) - 1);
        snprintf(task->service_address, sizeof(task->service_address),
                 "task-service-%s:50051", task_name);
        strncpy(task->inputs_json, simple_inputs, sizeof(task->inputs_json) - 1);
        task->priority = task_priority;
        strncpy(task->policy, task_policy ? task_policy : "fifo", sizeof(task->policy) - 1);
        task->start_time_ms = task_start * 1000ULL;  // Convert seconds to ms
        task->deadline_ms = task_deadline * 1000ULL;  // Convert seconds to ms
        task->status = TASK_STATUS_PENDING;
        pthread_mutex_init(&task->lock, NULL);
        task->grpc_handle = NULL;
        
        printf("[SCHEDULER] DEBUG - Task %d: start_time=%d s, deadline=%d s (parsed from Redis)\n",
               task->task_id, task_start, task_deadline);
        
        freeReplyObject(reply);
        
        // Create start timer
        task->start_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        if (task->start_timer_fd < 0) {
            perror("timerfd_create (start)");
            continue;
        }
        
        struct itimerspec start_spec = {0};
        ms_to_timespec(task->start_time_ms, &start_spec.it_value);
        timerfd_settime(task->start_timer_fd, 0, &start_spec, NULL);
        
        epoll_event_data_t *start_data = malloc(sizeof(epoll_event_data_t));
        start_data->type = EVENT_TYPE_START_TIMER;
        start_data->task = task;
        
        struct epoll_event start_ev = {
            .events = EPOLLIN,
            .data.ptr = start_data
        };
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, task->start_timer_fd, &start_ev);
        
        // Create timeout timer (disarmed)
        task->timeout_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        if (task->timeout_timer_fd < 0) {
            perror("timerfd_create (timeout)");
            continue;
        }
        
        epoll_event_data_t *timeout_data = malloc(sizeof(epoll_event_data_t));
        timeout_data->type = EVENT_TYPE_TIMEOUT_TIMER;
        timeout_data->task = task;
        
        struct epoll_event timeout_ev = {
            .events = EPOLLIN,
            .data.ptr = timeout_data
        };
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, task->timeout_timer_fd, &timeout_ev);
        
        printf("[SCHEDULER] 📋 Task %d '%s': start=%lu ms, deadline=%lu ms\n",
               task->task_id, task->task_name, task->start_time_ms, task->deadline_ms);
    }
    
    printf("\n[SCHEDULER] 🔄 Entering event loop (NO SLEEP - event-driven!)\n\n");
    
    // Event loop
    int tasks_pending = num_tasks;
    
    while (tasks_pending > 0) {
        struct epoll_event events[MAX_EPOLL_EVENTS];
        
        // Wait for events (blocks here but is event-driven!)
        int n = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);
        
        if (n < 0) {
            perror("epoll_wait");
            break;
        }
        
        // Process all ready events
        for (int i = 0; i < n; i++) {
            epoll_event_data_t *data = (epoll_event_data_t *)events[i].data.ptr;
            
            switch (data->type) {
                case EVENT_TYPE_START_TIMER: {
                    // Start timer expired - launch task
                    uint64_t expirations;
                    read(data->task->start_timer_fd, &expirations, sizeof(expirations));
                    
                    launch_task(data->task);
                    
                    // Remove start timer from epoll (one-shot)
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, data->task->start_timer_fd, NULL);
                    close(data->task->start_timer_fd);
                    data->task->start_timer_fd = -1;
                    
                    break;
                }
                
                case EVENT_TYPE_TIMEOUT_TIMER: {
                    // Timeout timer expired - abort task
                    uint64_t expirations;
                    read(data->task->timeout_timer_fd, &expirations, sizeof(expirations));
                    
                    abort_task_timeout(data->task);
                    
                    tasks_pending--;
                    printf("[SCHEDULER] %d tasks remaining\n", tasks_pending);
                    
                    break;
                }
                
                case EVENT_TYPE_TASK_COMPLETION: {
                    // Task completed - read notification
                    uint64_t task_id;
                    read(g_completion_eventfd, &task_id, sizeof(task_id));
                    
                    if (task_id > 0 && task_id <= num_tasks) {
                        scheduled_task_t *task = &g_tasks[task_id - 1];
                        handle_task_completion(task);
                        
                        tasks_pending--;
                        printf("[SCHEDULER] %d tasks remaining\n", tasks_pending);
                    }
                    
                    break;
                }
            }
        }
    }
    
    printf("\n[SCHEDULER] 🏁 All tasks completed!\n");
    printf("[SCHEDULER] Total execution time: %lu ms\n\n", get_elapsed_ms());
    
    // Cleanup
    for (int i = 0; i < num_tasks; i++) {
        scheduled_task_t *task = &g_tasks[i];
        
        if (task->start_timer_fd >= 0) close(task->start_timer_fd);
        if (task->timeout_timer_fd >= 0) close(task->timeout_timer_fd);
        pthread_mutex_destroy(&task->lock);
    }
    
    close(g_completion_eventfd);
    close(epoll_fd);
}
