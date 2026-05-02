#include "app_task.h"


/* --- Internal Workload Functions --- */

static void do_cpu_op() {
    double val = 10;
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
    json_object_set_int_member(obj, "total_ops", output->total_ops);

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
    const char *json_str = (const char *)arg;

    JsonParser *parser = json_parser_new();
    input_t parsed_input = { .total_ops = 100, .io_percentage = 0 };

    if (json_str && json_parser_load_from_data(parser, json_str, -1, NULL)) {
        JsonArray *array = json_node_get_array(json_parser_get_root(parser));
        if (array && json_array_get_length(array) > 0) {
            JsonObject *obj = json_array_get_object_element(array, 0);
            convert_json_to_input(obj, &parsed_input);
        }
    }
    g_object_unref(parser);
    input_t *input = &parsed_input;

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
    res->result    = 0;
    res->total_ops = cpu_ops;
    
    return (void*)res; 
}