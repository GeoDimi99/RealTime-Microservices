#include "redis_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---------- Redis ---------- */

redisContext* redis_connect(const char *host, int port)
{
    redisContext *ctx = redisConnect(host, port);
    if (!ctx || ctx->err) {
        fprintf(stderr, "Redis connection error\n");
        exit(1);
    }
    return ctx;
}

int redis_wait_for_schedule(redisContext *ctx)
{
    while (1) {
        redisReply *r = redisCommand(ctx, "EXISTS schedule");
        if (r && r->integer == 1) {
            freeReplyObject(r);
            return 0;
        }
        if (r) freeReplyObject(r);
        sleep(1);
    }
}

/* ---------- Helpers ---------- */

static const char* hget(redisReply *r, const char *key)
{
    for (size_t i = 0; i < r->elements; i += 2) {
        if (strcmp(r->element[i]->str, key) == 0)
            return r->element[i + 1]->str;
    }
    return NULL;
}

/* ---------- Main Reader ---------- */

int redis_read_schedule(redisContext *ctx, schedule_t *sched)
{
    redisReply *r = redisCommand(ctx, "HGETALL schedule");
    if (!r || r->type != REDIS_REPLY_ARRAY) return -1;

    const char *v;

    if ((v = hget(r, "name")))
        strncpy(sched->name, v, sizeof(sched->name));

    if ((v = hget(r, "version")))
        strncpy(sched->version, v, sizeof(sched->version));

    if ((v = hget(r, "description")))
        strncpy(sched->description, v, sizeof(sched->description));

    if ((v = hget(r, "length")))
        sched->num_tasks = atoi(v);

    freeReplyObject(r);

    /* ---------- Tasks ---------- */

    for (int i = 0; i < sched->num_tasks; i++) {
        char key[32];
        snprintf(key, sizeof(key), "scheduletask:%d", i + 1);

        r = redisCommand(ctx, "HGETALL %s", key);
        if (!r || r->type != REDIS_REPLY_ARRAY) continue;

        task_t *task = &sched->tasks[i];

        if ((v = hget(r, "name")))
            strncpy(task->name, v, sizeof(task->name));

        if ((v = hget(r, "policy")))
            strncpy(task->policy, v, sizeof(task->policy));

        if ((v = hget(r, "priority")))
            task->priority = atoi(v);

        if ((v = hget(r, "inputs")))
            strncpy(task->inputs, v, sizeof(task->inputs));

        if ((v = hget(r, "outputs")))
            strncpy(task->outputs, v, sizeof(task->outputs));

        freeReplyObject(r);
    }

    return 0;
}
