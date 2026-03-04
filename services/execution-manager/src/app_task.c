#include "app_task.h"

void print_input(input_t* input){
    g_return_if_fail(input != NULL);
    g_print("input_t { a = %d, b = %d }", input->a, input->b);
}


void* task_main(void* data){
    input_t* input = (input_t*)data;

    /* Do RT specific stuff here */
    //g_print("[INFO] ThreadCall &d: input { a = %d, b = %d }\n", getpid(), input->a, input->b);
    //print_input(input);
    output_t* output = g_new0(output_t, 1);
    output->result = input->a + input->b;
    return output;
}
