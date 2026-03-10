#include "app_task.h"
#include <fcntl.h>
#include <unistd.h>

#define CPU_INTENSITY 1000000 
#define IO_TMP_FILE "/tmp/rt_bench.bin"

/* --- Internal Workload Functions --- */

static void do_cpu_op() {
    volatile double val = 1.1;
    for (int i = 0; i < CPU_INTENSITY; i++) {
        val *= 1.1;
    }
}

static void do_io_op(int fd) {
    if (fd < 0) return;
    char buf[1024] = {0};
    if (write(fd, buf, sizeof(buf)) > 0) {
        fdatasync(fd); 
    }
}

/* --- JSON Parsing Logic --- */

int convert_json_to_input(JsonObject *obj, input_t* input) {
    g_return_val_if_fail(obj != NULL, -1);
    g_return_val_if_fail(input != NULL, -1);

    input->total_ops = json_object_has_member(obj, "total_ops") ? 
                       json_object_get_int_member(obj, "total_ops") : 100;

    input->io_percentage = json_object_has_member(obj, "io_percentage") ? 
                           json_object_get_int_member(obj, "io_percentage") : 0;

    return 0;
}

gchar* convert_output_to_json(const output_t* output) {
    JsonObject *obj = json_object_new();
    json_object_set_int_member(obj, "result", output->result);

    JsonNode *root = json_node_new(JSON_NODE_OBJECT);
    json_node_set_object(root, obj);

    JsonGenerator *gen = json_generator_new();
    json_generator_set_root(gen, root);
    gchar *res = json_generator_to_data(gen, NULL);

    g_object_unref(gen);
    json_node_free(root);
    return res;
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
    // We allocate the output_t on the heap so it persists after the thread joins
    output_t *res = g_new0(output_t, 1);
    res->result = 0; 
    
    return (void*)res; 
}