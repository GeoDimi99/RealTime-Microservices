#include <stdio.h>
#include <unistd.h>
#include "redis_reader.h"

int main(void)
{
    redisContext *ctx = redis_connect("redis", 6379);
    redis_wait_for_schedule(ctx);

    schedule_t s;

    while (1) {
        redis_read_schedule(ctx, &s);

        printf("\n=== SCHEDULE ===\n");
        printf("Name: %s\n", s.name);
        printf("Version: %s\n", s.version);
        printf("Tasks: %d\n", s.num_tasks);

        for (int i = 0; i < s.num_tasks; i++) {
            task_t *t = &s.tasks[i];
            printf("\nTask %d\n", i+1);
            printf("  Name     : %s\n", t->name);
            printf("  Policy   : %s\n", t->policy);
            printf("  Priority : %d\n", t->priority);
            printf("  Inputs   : %s\n", t->inputs);
            printf("  Outputs  : %s\n", t->outputs);
        }

        sleep(1);
    }
}
