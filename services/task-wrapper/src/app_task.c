#include "app_task.h"
#include <math.h>


/* --- Internal Workload Functions --- */

static void do_cpu_op() {
    double val = 10;
    /* Set squrt as cpu */
    
    
    for (int i = 0; i < CPU_INTENSITY; i++) {
        sqrt(val);
        val += 1;
    }
    
}

static void do_io_op(int fd) {
    if (fd < 0) return;
    char buf[1024] = {0};
    if (write(fd, buf, sizeof(buf)) > 0) {
        fdatasync(fd); 
    }
}


/* --- Standardized Main Task Logic --- */

void* task_main(void* arg) {
    // Cast the generic pointer back to our known input type
    input_t *input = (input_t *)arg;
    if (input == NULL) return NULL;

    // 1. Thread Setup
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    // 2. Workload Distribution
    int io_ops = (input->total_ops * input->io_percentage) / 100;
    int cpu_ops = input->total_ops - io_ops;
    int core = sched_getcpu();

    g_print("[THREAD] Core %d | Executing: %d CPU ops, %d I/O ops\n", 
             core, cpu_ops, io_ops);

    // 3. Execution Phase: CPU
    for (int i = 0; i < cpu_ops; i++) {
        do_cpu_op();
        if (i % 50 == 0) pthread_testcancel(); 
    }

    // 4. Execution Phase: I/O
    int fd = open(IO_TMP_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    for (int i = 0; i < io_ops; i++) {
        do_io_op(fd);
        if (i % 50 == 0) pthread_testcancel();
    }
    if (fd >= 0) close(fd);

    // 5. Return Output
    output_t *res = g_new0(output_t, 1);
    res->result    = 0;
    res->total_ops = cpu_ops;
    
    return (void*)res; 
}

gchar* convert_output_to_json(const output_t *output) {
    return g_strdup_printf("{\"result\":%d,\"total_ops\":%d}",
                           output ? output->result : 0,
                           output ? output->total_ops : 0);
}