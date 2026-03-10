#ifndef TASK_ENTRY_H
#define TASK_ENTRY_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>     /* Required for atoi */
#include <pthread.h>    /* Required for pthread_mutex_t */
#include <math.h>       /* Required for mathematical operations */
#include "jsmn.h"
#include "task_ipc.h"   /* Required for task_service_state_t (IDLE, RUNNING, COMPLETED) */

/* --- Data Structures --- */

typedef struct {
    int a;
    int b;
} input_t;

typedef struct {
    int result;
} output_t;

/* * Shared Context Structure.
 * This resides in the heap and is accessed by both Main and Thread.
 */
typedef struct {
    pthread_mutex_t lock;           // Protects access to status and output
    task_service_state_t status;    // Current state (IDLE, RUNNING, COMPLETED)
    input_t input;                  // Input data (Read-only for the thread)
    output_t output;                // Output data (Written by the thread)
} task_context_t;



/* --- Helper Function --- */

/*
 * Compares a JSMN token with a C string.
 * It handles both standard JSON strings (JSMN_STRING) 
 * and unquoted keys (JSMN_PRIMITIVE) if strict mode is off.
 */
static int json_eq(const char *json, jsmntok_t *tok, const char *s) {
    if (tok->type == JSMN_STRING || tok->type == JSMN_PRIMITIVE) {
        if ((int)strlen(s) == tok->end - tok->start &&
            strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
            return 1;
        }
    }
    return 0;
}

/* --- Task Functions --- */

/* * Input Conversion: JSON String -> Struct 
 */
int convert_input(char* input_json, input_t* input){
    
    /* Prepare parser and token array */
    jsmn_parser p;
    jsmntok_t t[128];   // We expect no more than 128 JSON tokens
    
    jsmn_init(&p);
    
    /* Parsing execution */
    int r = jsmn_parse(&p, input_json, strlen(input_json), t, 128); 

    if (r < 0) {
        // Handle error codes if necessary
        return -1;
    }

    /* Scanning the results */
    /* Assume the top-level element is an object */
    if (r < 1 || t[0].type != JSMN_OBJECT) {
        return -1;
    }

    /* Loop through tokens (skipping the root object at index 0) */
    for (int i = 1; i < r; i++) {
        
        /* Check for key "a" */
        if (json_eq(input_json, &t[i], "a")) {
            char buf[32];
            int len = t[i+1].end - t[i+1].start;
            
            // Safety check for buffer size
            if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
            
            // Copy value to buffer to null-terminate it for atoi
            memcpy(buf, input_json + t[i+1].start, len);
            buf[len] = '\0';
            
            input->a = atoi(buf);
            
            i++; // Skip the value token
        }
        /* Check for key "b" */
        else if (json_eq(input_json, &t[i], "b")) {
            char buf[32];
            int len = t[i+1].end - t[i+1].start;
            
            if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
            
            memcpy(buf, input_json + t[i+1].start, len);
            buf[len] = '\0';
            
            input->b = atoi(buf);
            
            i++; // Skip the value token
        }
    }
    
    return 0;
}

/* * Output Conversion: Struct -> JSON String
 */
int convert_output(output_t* output, char* output_json) {
    /* Note: Ensure 'output_json' buffer is large enough in the caller */
    return sprintf(output_json, "{\"result\": %d}", output->result);
}


/* * MAIN TASK (Worker Thread)
 * Receives the Context pointer, performs calculations, and writes the result.
 */
void *task_main(void *arg){
    task_context_t *ctx = (task_context_t *)arg;

    /* 1. Update state to RUNNING */
    pthread_mutex_lock(&ctx->lock);
    ctx->status = RUNNING; 
    pthread_mutex_unlock(&ctx->lock);

    printf("[THREAD] Processing: %d + %d\n", ctx->input.a, ctx->input.b);

    /* 2. Real-Time Logic (Calculations) */
    /* * Accessing ctx->input is thread-safe here because 
     * the Main thread does not modify it after creation.
     */
    
    printf("[THREAD] Starting EXTREMELY INTENSIVE computation for task...\n");

    // EXTREMELY INTENSIVE mathematical operations for extended benchmarking
    volatile double result = 0.0;
    volatile double matrix_acc = 0.0;
    volatile double sqrt_acc = 0.0;
    int a = ctx->input.a;
    int b = ctx->input.b;
    
    // Phase 1: Basic arithmetic operations with sqrt (20 million iterations)
    printf("[THREAD] Phase 1/7: Arithmetic operations with sqrt...\n");
    for (int i = 1; i < 20000000; i++) {
        result += (double)a / (double)i;
        result -= (double)b / (double)(i + 1);
        result *= 1.00001;
        sqrt_acc += sqrt((double)i);
        
        if (i % 100000 == 0) {
            result = result / 1.001 + sqrt(fabs(result) + 1.0);
        }
        if (i % 200000 == 0) {
            sqrt_acc = sqrt(sqrt_acc);
        }
    }
    
    // Phase 2: Trigonometric operations with sqrt (10 million iterations)
    printf("[THREAD] Phase 2/7: Trigonometric calculations with sqrt...\n");
    for (int i = 1; i < 10000000; i++) {
        double angle = (double)i * 0.00001;
        result += sin(angle) * cos(angle);
        result -= tan(angle / 2.0);
        sqrt_acc += sqrt(fabs(sin(angle)) + 1.0);
        
        if (i % 50000 == 0) {
            result += atan2(result, (double)a) * sqrt((double)b);
        }
    }
    
    // Phase 3: Logarithmic and exponential with sqrt (6 million iterations)
    printf("[THREAD] Phase 3/7: Logarithmic and exponential with sqrt...\n");
    for (int i = 1; i < 6000000; i++) {
        result += log((double)i + 1.0) * log10((double)i + 2.0);
        result -= exp((double)i / 1000000.0) / 1000000.0;
        sqrt_acc += sqrt(log((double)i + 1.0));
        
        if (i % 10000 == 0) {
            result = sqrt(fabs(result) + 1.0) * sqrt(sqrt_acc + 1.0);
        }
    }
    
    // Phase 4: Power and root operations with nested sqrt (4 million iterations)
    printf("[THREAD] Phase 4/7: Power and root calculations with nested sqrt...\n");
    for (int i = 1; i < 4000000; i++) {
        result += pow((double)a, 1.0 / (double)i);
        result -= pow((double)b, 1.0 / (double)(i + 1));
        result *= cbrt((double)i);
        sqrt_acc += sqrt(sqrt(fabs(result) + 1.0));
        
        if (i % 20000 == 0) {
            result = fmod(sqrt(fabs(result)), 1000000.0);
        }
    }
    
    // Phase 5: Nested loop operations with sqrt (3 million iterations)
    printf("[THREAD] Phase 5/7: Nested mathematical operations...\n");
    for (int i = 1; i < 3000000; i++) {
        for (int j = 1; j <= 5; j++) {
            result += sqrt((double)(i * j)) / (double)j;
            result -= log((double)(i + j)) / (double)(j + 1);
        }
        
        if (i % 50000 == 0) {
            result = sqrt(fabs(result) + 1.0);
            sqrt_acc += sqrt((double)i);
        }
    }
    
    // Phase 6: Complex trigonometric with sqrt (2 million iterations)
    printf("[THREAD] Phase 6/7: Complex trigonometric operations...\n");
    for (int i = 1; i < 2000000; i++) {
        double angle1 = (double)i * 0.0001;
        double angle2 = (double)i * 0.0002;
        
        result += sin(sqrt(angle1)) * cos(sqrt(angle2));
        result -= tan(angle1 / sqrt(2.0));
        sqrt_acc += sqrt(fabs(sin(angle1) * cos(angle2)));
        
        if (i % 25000 == 0) {
            result = sqrt(fabs(result) + 1.0) * sqrt(fabs(sqrt_acc) + 1.0);
        }
    }
    
    // Phase 7: Advanced matrix operations with sqrt (2 million iterations)
    printf("[THREAD] Phase 7/7: Advanced matrix simulation with sqrt...\n");
    for (int i = 0; i < 2000000; i++) {
        // Simulate 4x4 matrix multiplication accumulation with sqrt
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 4; col++) {
                double val1 = sin((double)(i + row)) * (double)a;
                double val2 = cos((double)(i + col)) * (double)b;
                matrix_acc += sqrt(fabs(val1 * val2));
            }
        }
        
        if (i % 10000 == 0) {
            matrix_acc = sqrt(fabs(matrix_acc));
        }
        if (i % 50000 == 0) {
            matrix_acc = matrix_acc / sqrt(1000.0);
        }
    }
    
    // Combine all results with sqrt operations
    result += matrix_acc + sqrt_acc;
    result = sqrt(fabs(result)) * sqrt(fabs(matrix_acc) + 1.0);
    int res = a + b + (int)(fmod(fabs(result), 1000000.0));
    
    printf("[THREAD] ✅ EXTREME computation completed with %d phases\n", 7);

    /* 3. Write Output protected by Mutex */
    pthread_mutex_lock(&ctx->lock);
    ctx->output.result = res;
    ctx->status = COMPLETED;
    pthread_mutex_unlock(&ctx->lock);

    printf("[THREAD] Job Done. Result: %d\n", res);

    return NULL;
}

#endif