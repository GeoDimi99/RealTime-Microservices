#ifndef REDIS_READER_H
#define REDIS_READER_H

#include <hiredis/hiredis.h>

#define MAX_TASKS 32
#define MAX_JSON_LEN 1024

typedef struct {
    char name[64];
    char policy[16];
    int priority;
    char inputs[MAX_JSON_LEN];
    char outputs[MAX_JSON_LEN];
} task_t;

typedef struct {
    char name[64];
    char version[16];
    char description[128];
    int num_tasks;
    task_t tasks[MAX_TASKS];
} schedule_t;

/* Initialize Redis connection */
redisContext* redis_connect(const char *host, int port);

/* Wait until there is a loaded schedule */
int redis_wait_for_schedule(redisContext *ctx);

/* Read schedule from Redis into schedule_t */
int redis_read_schedule(redisContext *ctx, schedule_t *schedule);

#endif
