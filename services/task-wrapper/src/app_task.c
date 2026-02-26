#include "app_task.h"

void print_input(input_t* input){
    g_return_if_fail(input != NULL);
    g_print("input_t { a = %d, b = %d }\n", input->a, input->b);
}




/* --- Task Functions --- */

int convert_json_to_input(JsonObject *obj, input_t* input){
    g_return_val_if_fail(obj != NULL, -1);
    g_return_val_if_fail(input != NULL, -1);

    input->a = json_object_get_int_member(obj, "a");
    input->b = json_object_get_int_member(obj, "b");
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







/* * MAIN TASK (Worker Thread)
 * Receives the Context pointer, performs calculations, and writes the result.
 */
#define _GNU_SOURCE
#include <sched.h> // Necessario per sched_getcpu()

output_t *task_main(input_t *arg) {
    // Imposta la cancellazione come DIFFERITA
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    print_input(arg);
    
    int cpu = sched_getcpu();
    output_t *output = g_new0(output_t, 1);
    output->result = arg->a * arg->b;
    
    g_print("[THREAD] Running on Core %d | Calculation: %d * %d = %d\n", 
             cpu, arg->a, arg->b, output->result);
    
    // Punto di cancellazione sicuro: se arriva un cancel, il thread muore qui
    // invece che durante la g_print
    pthread_testcancel(); 
    
    g_usleep(500000); 
    
    return output;
}
