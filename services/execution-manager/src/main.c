#include <stdio.h>
#include "redis_reader.h"

int main() {
    redisContext *ctx = redis_connect("redis", 6379);
    if (!ctx) return 1;

    /* Wait until schedule exists */
    redis_wait_for_schedule(ctx);

    schedule_t schedule;

    while (1) {
        if (redis_read_schedule(ctx, &schedule) == 0) {

            printf("\n=== SCHEDULE ===\n");
            printf("Name       : %s\n", schedule.name);
            printf("Version    : %s\n", schedule.version);
            printf("Description: %s\n", schedule.description);
            printf("Tasks      : %d\n", schedule.num_tasks);

            for (int i = 0; i < schedule.num_tasks; i++) {
                task_t *t = &schedule.tasks[i];
                printf("\nTask %d\n", i + 1);
                printf("  Name     : %s\n", t->name);
                printf("  Policy   : %s\n", t->policy);
                printf("  Priority : %d\n", t->priority);
                printf("  Inputs   : %s\n", t->inputs);
                printf("  Outputs  : %s\n", t->outputs);
            }
        }

        sleep(1);   // future-proof for version updates
    }

    redisFree(ctx);
    return 0;
}
