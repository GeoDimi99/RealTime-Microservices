/*
 * measure_exec.c
 *
 * Misura il tempo di esecuzione di task_main con:
 *   - Priorità SCHED_FIFO 50
 *   - total_ops = 1000
 *   - io_percentage = 50
 *   - Pinnato su core 1
 *
 * Compilare:
 *   gcc -O2 -o measure_exec measure_exec.c app_task.c \
 *       $(pkg-config --cflags --libs glib-2.0 json-glib-1.0) \
 *       -lpthread -lm
 *
 * Eseguire (serve root per SCHED_FIFO):
 *   sudo ./measure_exec
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <float.h>
#include <math.h>

#include "app_task.h"

/* ---------- Contesto passato al thread ---------- */
typedef struct {
    input_t         task_input;
    struct timespec  t_start;
    struct timespec  t_end;
} bench_ctx_t;

/* ---------- Wrapper: misura tempo di esecuzione di task_main ---------- */
static void *wrapper(void *arg)
{
    bench_ctx_t *ctx = (bench_ctx_t *)arg;

    clock_gettime(CLOCK_MONOTONIC, &ctx->t_start);

    void *ret = task_main(&ctx->task_input);

    clock_gettime(CLOCK_MONOTONIC, &ctx->t_end);

    if (ret) g_free(ret);
    return NULL;
}

/* ---------- Differenza in microsecondi ---------- */
static double diff_us(const struct timespec *a, const struct timespec *b)
{
    return (double)(b->tv_sec - a->tv_sec) * 1e6
         + (double)(b->tv_nsec - a->tv_nsec) / 1e3;
}

/* ---------- main ---------- */
int main(void)
{
    const int priority      = 50;
    const int total_ops     = 100;
    const int io_percentage = 75;
    const int target_core   = 1;
    const int iterations    = 25;

    printf("=== Misurazione tempo di esecuzione task_main ===\n");
    printf("Priorità (SCHED_FIFO) : %d\n", priority);
    printf("Core                  : %d\n", target_core);
    printf("total_ops             : %d\n", total_ops);
    printf("io_percentage         : %d%%\n", io_percentage);
    printf("Iterazioni            : %d\n\n", iterations);

    double *samples = calloc(iterations, sizeof(double));
    if (!samples) { perror("calloc"); return 1; }

    double sum = 0.0, min_val = DBL_MAX, max_val = 0.0;

    for (int i = 0; i < iterations; i++) {

        bench_ctx_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.task_input.total_ops      = total_ops;
        ctx.task_input.io_percentage  = io_percentage;

        /* Attributi thread: SCHED_FIFO con priorità 50 */
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

        struct sched_param sp;
        sp.sched_priority = priority;
        pthread_attr_setschedparam(&attr, &sp);

        /* Pinnare il thread sul core specificato */
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(target_core, &cpuset);
        pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);

        pthread_t tid;
        int rc = pthread_create(&tid, &attr, wrapper, &ctx);

        if (rc == EPERM) {
            fprintf(stderr,
                "ERRORE: permessi insufficienti per SCHED_FIFO.\n"
                "Eseguire con: sudo ./measure_exec\n");
            pthread_attr_destroy(&attr);
            free(samples);
            return 1;
        }
        if (rc != 0) {
            fprintf(stderr, "pthread_create fallita: %s\n", strerror(rc));
            pthread_attr_destroy(&attr);
            free(samples);
            return 1;
        }

        pthread_join(tid, NULL);
        pthread_attr_destroy(&attr);

        double elapsed_us = diff_us(&ctx.t_start, &ctx.t_end);
        samples[i] = elapsed_us;
        sum += elapsed_us;

        if (elapsed_us < min_val) min_val = elapsed_us;
        if (elapsed_us > max_val) max_val = elapsed_us;

        printf("  [%3d] exec time: %12.2f us  (%8.4f ms)\n",
               i + 1, elapsed_us, elapsed_us / 1000.0);
    }

    double mean = sum / iterations;

    double var = 0.0;
    for (int i = 0; i < iterations; i++) {
        double d = samples[i] - mean;
        var += d * d;
    }
    double stddev = (iterations > 1) ? sqrt(var / (iterations - 1)) : 0.0;

    printf("\n=== Riepilogo ===\n");
    printf("  Media    : %12.2f us  (%8.4f ms)\n", mean, mean / 1000.0);
    printf("  Min      : %12.2f us  (%8.4f ms)\n", min_val, min_val / 1000.0);
    printf("  Max      : %12.2f us  (%8.4f ms)\n", max_val, max_val / 1000.0);
    printf("  Std Dev  : %12.2f us  (%8.4f ms)\n", stddev, stddev / 1000.0);
    printf("  Campioni : %d\n", iterations);

    free(samples);
    return 0;
}