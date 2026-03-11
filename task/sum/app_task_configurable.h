#ifndef APP_TASK_CONFIGURABLE_H
#define APP_TASK_CONFIGURABLE_H

#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include "jsmn.h"

/* --- Configuration --- */
#define CPU_INTENSITY 1000000 
#define IO_TMP_FILE "/tmp/rt_bench.bin"
#define MAX_JSON_TOKENS 128

/* --- Data Structures --- */

typedef struct {
    int total_ops;     // Total number of operations to perform
    int io_percentage; // % of total_ops that are I/O (0-100)
} input_t;

typedef struct {
    int result;        // Status code (e.g., 0 for success)
} output_t;

/* --- Helper: JSON Parsing with jsmn --- */

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
    if (tok->type == JSMN_STRING && 
        (int)strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

static int parse_int(const char *json, jsmntok_t *tok) {
    char buf[32];
    int len = tok->end - tok->start;
    if (len >= 32) len = 31;
    strncpy(buf, json + tok->start, len);
    buf[len] = '\0';
    return atoi(buf);
}

/* --- JSON Conversion Functions --- */

static int convert_input(const char *json_str, input_t *input) {
    if (!json_str || !input) return -1;
    
    // Initialize defaults
    input->total_ops = 100;
    input->io_percentage = 0;
    
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKENS];
    
    jsmn_init(&parser);
    int num_tokens = jsmn_parse(&parser, json_str, strlen(json_str), 
                                 tokens, MAX_JSON_TOKENS);
    
    if (num_tokens < 1 || tokens[0].type != JSMN_OBJECT) {
        return -1;
    }
    
    // Parse JSON object
    for (int i = 1; i < num_tokens; i++) {
        if (jsoneq(json_str, &tokens[i], "total_ops") == 0) {
            input->total_ops = parse_int(json_str, &tokens[i + 1]);
            i++;
        } else if (jsoneq(json_str, &tokens[i], "io_percentage") == 0) {
            input->io_percentage = parse_int(json_str, &tokens[i + 1]);
            i++;
        }
    }
    
    // Validate
    if (input->io_percentage < 0) input->io_percentage = 0;
    if (input->io_percentage > 100) input->io_percentage = 100;
    if (input->total_ops < 1) input->total_ops = 1;
    
    return 0;
}

static int convert_output(const output_t *output, char *json_out) {
    if (!output || !json_out) return -1;
    
    snprintf(json_out, 1024, "{\"result\": %d}", output->result);
    return 0;
}

/* --- Internal Workload Functions --- */

static void do_cpu_op(void) {
    volatile double val = 1.1;
    for (int i = 0; i < CPU_INTENSITY; i++) {
        val *= 1.1;
    }
}

static void do_io_op(int fd) {
    if (fd < 0) return;
    
    char buf[1024] = {0};
    // Write data
    if (write(fd, buf, sizeof(buf)) > 0) {
        // Sync to disk (real I/O operation)
        fdatasync(fd); 
    }
}

/* --- Main Task Logic --- */

void* task_main(void* arg) {
    // Cast the generic pointer back to our known input type
    input_t *input = (input_t *)arg;
    if (input == NULL) {
        fprintf(stderr, "[TASK ERROR] NULL input argument\n");
        return NULL;
    }

    // 1. Thread Setup
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    // 2. Workload Distribution
    int io_ops = (input->total_ops * input->io_percentage) / 100;
    int cpu_ops = input->total_ops - io_ops;
    int core = sched_getcpu();

    printf("[THREAD] Core %d | Executing: %d CPU ops, %d I/O ops (total %d, io_pct %d%%)\n", 
           core, cpu_ops, io_ops, input->total_ops, input->io_percentage);

    // 3. Execution Phase: CPU Operations
    printf("[THREAD] Phase 1/2: CPU operations...\n");
    for (int i = 0; i < cpu_ops; i++) {
        do_cpu_op();
        
        // Check for cancellation periodically
        if (i % 50 == 0) {
            pthread_testcancel(); 
        }
    }

    // 4. Execution Phase: I/O Operations
    printf("[THREAD] Phase 2/2: I/O operations...\n");
    int fd = -1;
    if (io_ops > 0) {
        fd = open(IO_TMP_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("[THREAD] Warning: Failed to open I/O file");
        }
    }
    
    for (int i = 0; i < io_ops; i++) {
        do_io_op(fd);
        
        // Check for cancellation periodically
        if (i % 50 == 0) {
            pthread_testcancel();
        }
    }
    
    if (fd >= 0) {
        close(fd);
        // Cleanup temp file
        unlink(IO_TMP_FILE);
    }

    // 5. Return Output
    printf("[THREAD] ✅ Task completed successfully on core %d\n", core);
    
    // We allocate the output_t on the heap so it persists after the thread joins
    output_t *res = (output_t *)malloc(sizeof(output_t));
    if (!res) {
        fprintf(stderr, "[TASK ERROR] Failed to allocate output\n");
        return NULL;
    }
    
    res->result = 0;  // Success
    
    return (void*)res; 
}

#endif /* APP_TASK_CONFIGURABLE_H */
