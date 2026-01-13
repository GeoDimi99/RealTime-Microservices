#ifndef REDIS_READER_H
#define REDIS_READER_H

#include <hiredis/hiredis.h>

#define MAX_TASKS     32
#define MAX_JSON_LEN  1024

typedef struct {
    char name[64];
    char policy[16];
    int  priority;
    char inputs[MAX_JSON_LEN];   // JSON string
    char outputs[MAX_JSON_LEN];  // JSON string
} task_t;

typedef struct {
    char name[64];
    char version[16];
    char description[128];
    int  num_tasks;
    task_t tasks[MAX_TASKS];
} schedule_t;

redisContext* redis_connect(const char *host, int port);
int redis_wait_for_schedule(redisContext *ctx);
int redis_read_schedule(redisContext *ctx, schedule_t *schedule);

#endif
