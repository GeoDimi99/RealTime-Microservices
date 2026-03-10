#ifndef TASK_ENTRY_HYBRID_H
#define TASK_ENTRY_HYBRID_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>     /* For file I/O and sync operations */
#include <fcntl.h>      /* For file control */
#include <sys/stat.h>   /* For file stats */
#include <errno.h>
#include "jsmn.h"
#include "task_ipc.h"

/* --- Data Structures --- */

typedef struct {
    int a;
    int b;
    int io_percentage;  // 0-100: percentage of IO-intensive operations
} input_hybrid_t;

typedef struct {
    int result;
    int cpu_iterations;
    int io_operations;
} output_hybrid_t;

typedef struct {
    pthread_mutex_t lock;
    task_service_state_t status;
    input_hybrid_t input;
    output_hybrid_t output;
} task_context_hybrid_t;


/* --- Helper Functions --- */

static int json_eq_hybrid(const char *json, jsmntok_t *tok, const char *s) {
    if (tok->type == JSMN_STRING || tok->type == JSMN_PRIMITIVE) {
        if ((int)strlen(s) == tok->end - tok->start &&
            strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Perform CPU-intensive operations */
static volatile double perform_cpu_intensive_batch(int a, int b, int iterations) {
    volatile double result = 0.0;
    
    for (int i = 1; i < iterations; i++) {
        result += sqrt((double)a / (double)i);
        result -= sqrt((double)b / (double)(i + 1));
        result *= 1.00001;
        
        double angle = (double)i * 0.0001;
        result += sin(angle) * cos(angle);
        result -= tan(angle / 2.0);
        
        if (i % 10000 == 0) {
            result = sqrt(fabs(result) + 1.0);
        }
    }
    
    return result;
}

/* Perform IO-intensive operations */
static int perform_io_intensive_batch(int batch_id, int operations) {
    char filename[256];
    snprintf(filename, sizeof(filename), "/tmp/benchmark_io_%d_%d.tmp", getpid(), batch_id);
    
    int io_count = 0;
    
    for (int i = 0; i < operations; i++) {
        // Write operation
        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            char buffer[4096];
            memset(buffer, 'A' + (i % 26), sizeof(buffer));
            
            // Write multiple blocks
            for (int block = 0; block < 10; block++) {
                write(fd, buffer, sizeof(buffer));
            }
            
            // Force sync to disk (IO-intensive!)
            fsync(fd);
            close(fd);
            io_count++;
        }
        
        // Read operation
        fd = open(filename, O_RDONLY);
        if (fd >= 0) {
            char buffer[4096];
            
            // Read multiple blocks
            for (int block = 0; block < 10; block++) {
                read(fd, buffer, sizeof(buffer));
            }
            
            close(fd);
            io_count++;
        }
        
        // Additional IO: stat operations
        struct stat st;
        if (stat(filename, &st) == 0) {
            io_count++;
        }
    }
    
    // Cleanup
    unlink(filename);
    
    return io_count;
}

/* Perform memory-intensive operations (hybrid IO/CPU) */
static void perform_memory_intensive_batch(int iterations) {
    for (int i = 0; i < iterations; i++) {
        // Allocate and deallocate memory blocks
        size_t size = 1024 * 1024; // 1MB blocks
        void *ptr = malloc(size);
        if (ptr) {
            // Touch memory to force page faults (IO to disk if swapping)
            memset(ptr, i % 256, size);
            
            // Read back to ensure operation completes
            volatile char sum = 0;
            char *data = (char*)ptr;
            for (size_t j = 0; j < size; j += 4096) {
                sum += data[j];
            }
            
            free(ptr);
        }
    }
}


/* --- Task Functions --- */

/* Input Conversion: JSON String -> Struct */
int convert_input_hybrid(char* input_json, input_hybrid_t* input) {
    jsmn_parser p;
    jsmntok_t t[128];
    
    jsmn_init(&p);
    int r = jsmn_parse(&p, input_json, strlen(input_json), t, 128);
    
    if (r < 0 || r < 1 || t[0].type != JSMN_OBJECT) {
        return -1;
    }
    
    // Default values
    input->io_percentage = 0;
    
    for (int i = 1; i < r; i++) {
        if (json_eq_hybrid(input_json, &t[i], "a")) {
            char buf[32];
            int len = t[i+1].end - t[i+1].start;
            if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
            memcpy(buf, input_json + t[i+1].start, len);
            buf[len] = '\0';
            input->a = atoi(buf);
            i++;
        }
        else if (json_eq_hybrid(input_json, &t[i], "b")) {
            char buf[32];
            int len = t[i+1].end - t[i+1].start;
            if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
            memcpy(buf, input_json + t[i+1].start, len);
            buf[len] = '\0';
            input->b = atoi(buf);
            i++;
        }
        else if (json_eq_hybrid(input_json, &t[i], "io_percentage")) {
            char buf[32];
            int len = t[i+1].end - t[i+1].start;
            if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
            memcpy(buf, input_json + t[i+1].start, len);
            buf[len] = '\0';
            input->io_percentage = atoi(buf);
            
            // Clamp to 0-100 range
            if (input->io_percentage < 0) input->io_percentage = 0;
            if (input->io_percentage > 100) input->io_percentage = 100;
            
            i++;
        }
    }
    
    return 0;
}

/* Output Conversion: Struct -> JSON String */
int convert_output_hybrid(output_hybrid_t* output, char* output_json) {
    return sprintf(output_json, 
                   "{\"result\": %d, \"cpu_iterations\": %d, \"io_operations\": %d}", 
                   output->result, output->cpu_iterations, output->io_operations);
}


/* MAIN HYBRID TASK - Configurable CPU/IO workload */
void *task_main_hybrid(void *arg) {
    task_context_hybrid_t *ctx = (task_context_hybrid_t *)arg;
    
    pthread_mutex_lock(&ctx->lock);
    ctx->status = RUNNING;
    pthread_mutex_unlock(&ctx->lock);
    
    int a = ctx->input.a;
    int b = ctx->input.b;
    int io_pct = ctx->input.io_percentage;
    
    printf("[THREAD HYBRID] Processing: a=%d, b=%d, IO%%=%d\n", a, b, io_pct);
    printf("[THREAD HYBRID] Starting HYBRID computation (CPU/IO balanced)...\n");
    
    volatile double cpu_result = 0.0;
    int total_cpu_iterations = 0;
    int total_io_operations = 0;
    
    // Calculate workload distribution
    // Total workload units: 100
    int cpu_units = 100 - io_pct;
    int io_units = io_pct;
    
    printf("[THREAD HYBRID] Workload: %d%% CPU, %d%% IO\n", cpu_units, io_units);
    
    // Execute 10 phases, each with mixed CPU/IO based on percentage
    int total_phases = 10;
    
    for (int phase = 0; phase < total_phases; phase++) {
        printf("[THREAD HYBRID] Phase %d/%d\n", phase + 1, total_phases);
        
        // CPU portion of this phase
        if (cpu_units > 0) {
            int cpu_iterations = cpu_units * 100000; // Scale factor
            cpu_result += perform_cpu_intensive_batch(a, b, cpu_iterations);
            total_cpu_iterations += cpu_iterations;
        }
        
        // IO portion of this phase
        if (io_units > 0) {
            int io_operations = io_units / 10; // Scale factor (IO is slower)
            if (io_operations < 1) io_operations = 1;
            
            int io_done = perform_io_intensive_batch(phase, io_operations);
            total_io_operations += io_done;
        }
        
        // Memory-intensive operations (mix of both)
        if (phase % 3 == 0 && io_pct > 20) {
            perform_memory_intensive_batch(io_units / 20);
        }
    }
    
    // Additional CPU-intensive finalization if mostly CPU
    if (cpu_units >= 80) {
        printf("[THREAD HYBRID] Extra CPU phase (high CPU mode)...\n");
        for (int i = 0; i < 5; i++) {
            cpu_result += perform_cpu_intensive_batch(a, b, 1000000);
            total_cpu_iterations += 1000000;
        }
    }
    
    // Additional IO-intensive finalization if mostly IO
    if (io_units >= 80) {
        printf("[THREAD HYBRID] Extra IO phase (high IO mode)...\n");
        for (int i = 0; i < 3; i++) {
            int io_done = perform_io_intensive_batch(100 + i, 10);
            total_io_operations += io_done;
        }
    }
    
    // Calculate final result
    int res = a + b + (int)(fmod(fabs(cpu_result), 1000000.0));
    
    printf("[THREAD HYBRID] ✅ Hybrid computation completed\n");
    printf("[THREAD HYBRID] CPU iterations: %d, IO operations: %d\n", 
           total_cpu_iterations, total_io_operations);
    
    // Write output
    pthread_mutex_lock(&ctx->lock);
    ctx->output.result = res;
    ctx->output.cpu_iterations = total_cpu_iterations;
    ctx->output.io_operations = total_io_operations;
    ctx->status = COMPLETED;
    pthread_mutex_unlock(&ctx->lock);
    
    printf("[THREAD HYBRID] Job Done. Result: %d\n", res);
    
    return NULL;
}

#endif
