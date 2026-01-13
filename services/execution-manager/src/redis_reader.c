#include "redis_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include "jsmn.h"

/* Connect to Redis */
redisContext* redis_connect(const char *host, int port) {
    redisContext *ctx = redisConnect(host, port);
    if (ctx == NULL || ctx->err) {
        if (ctx) {
            fprintf(stderr, "Redis connection error: %s\n", ctx->errstr);
            redisFree(ctx);
        } else {
            fprintf(stderr, "Redis connection allocation error\n");
        }
        return NULL;
    }
    return ctx;
}

/* Helper to parse JSON token */
static void json_copy_value(const char *json, jsmntok_t *t, char *dest, size_t maxlen) {
    int len = t->end - t->start;
    if ((size_t)len >= maxlen) len = maxlen - 1;
    strncpy(dest, json + t->start, len);
    dest[len] = '\0';
}

/* Wait for schedule to be uploaded */
int redis_wait_for_schedule(redisContext *ctx) {
    redisReply *reply = NULL;

    printf("Waiting for schedule to be uploaded...\n");

    while (1) {
        reply = redisCommand(ctx, "EXISTS schedule:meta");

        if (reply && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1) {
            freeReplyObject(reply);
            printf("Schedule found!\n");
            return 0;
        }

        if (reply)
            freeReplyObject(reply);

        sleep(1);
    }
}

/* Read schedule from Redis */
int redis_read_schedule(redisContext *ctx, schedule_t *schedule) {
    if (!ctx || !schedule) return -1;

    /* Read schedule meta */
    redisReply *reply = redisCommand(ctx, "GET schedule:meta");
    if (!reply || reply->type != REDIS_REPLY_STRING) {
        if (reply) freeReplyObject(reply);
        fprintf(stderr, "Failed to get schedule:meta\n");
        return -1;
    }

    jsmn_parser parser;
    jsmntok_t tokens[64];
    jsmn_init(&parser);
    int r = jsmn_parse(&parser, reply->str, strlen(reply->str), tokens, sizeof(tokens)/sizeof(tokens[0]));
    if (r < 1 || tokens[0].type != JSMN_OBJECT) {
        fprintf(stderr, "Failed to parse schedule meta JSON\n");
        freeReplyObject(reply);
        return -1;
    }

    for (int i = 1; i < r; i += 2) {
        jsmntok_t *key = &tokens[i];
        jsmntok_t *val = &tokens[i+1];
        char keybuf[64];
        json_copy_value(reply->str, key, keybuf, sizeof(keybuf));

        if (strcmp(keybuf, "name") == 0) {
            json_copy_value(reply->str, val, schedule->name, sizeof(schedule->name));
        } else if (strcmp(keybuf, "version") == 0) {
            json_copy_value(reply->str, val, schedule->version, sizeof(schedule->version));
        } else if (strcmp(keybuf, "description") == 0) {
            json_copy_value(reply->str, val, schedule->description, sizeof(schedule->description));
        } else if (strcmp(keybuf, "length") == 0) {
            char buf[16];
            json_copy_value(reply->str, val, buf, sizeof(buf));
            schedule->num_tasks = atoi(buf);
        }
    }
    freeReplyObject(reply);

    /* Read each task */
    for (int i = 0; i < schedule->num_tasks; i++) {
        char key[64];
        snprintf(key, sizeof(key), "schedule:task:%d", i+1);
        reply = redisCommand(ctx, "GET %s", key);
        if (!reply || reply->type != REDIS_REPLY_STRING) {
            fprintf(stderr, "Failed to get %s\n", key);
            if (reply) freeReplyObject(reply);
            continue;
        }

        jsmn_init(&parser);
        r = jsmn_parse(&parser, reply->str, strlen(reply->str), tokens, sizeof(tokens)/sizeof(tokens[0]));
        if (r < 1 || tokens[0].type != JSMN_OBJECT) {
            fprintf(stderr, "Failed to parse task JSON %s\n", key);
            freeReplyObject(reply);
            continue;
        }

        task_t *task = &schedule->tasks[i];

        for (int j = 1; j < r; j += 2) {
            jsmntok_t *keytok = &tokens[j];
            jsmntok_t *valtok = &tokens[j+1];
            char keybuf[64];
            json_copy_value(reply->str, keytok, keybuf, sizeof(keybuf));

            if (strcmp(keybuf, "name") == 0) {
                json_copy_value(reply->str, valtok, task->name, sizeof(task->name));
            } else if (strcmp(keybuf, "policy") == 0) {
                json_copy_value(reply->str, valtok, task->policy, sizeof(task->policy));
            } else if (strcmp(keybuf, "priority") == 0) {
                char buf[16];
                json_copy_value(reply->str, valtok, buf, sizeof(buf));
                task->priority = atoi(buf);
            } else if (strcmp(keybuf, "inputs") == 0) {
                json_copy_value(reply->str, valtok, task->inputs, sizeof(task->inputs));
            } else if (strcmp(keybuf, "outputs") == 0) {
                json_copy_value(reply->str, valtok, task->outputs, sizeof(task->outputs));
            }
        }
        freeReplyObject(reply);
    }

    return 0;
}
