#include "app_task.h"

void print_input(input_t* input){
    g_return_if_fail(input != NULL);
    g_print("input_t { a = %d, b = %d }\n", input->a, input->b);
}


void* task_main(void* data){
    input_t* input = (input_t*)data;

    /* Do RT specific stuff here */
    //print_input(input);
    printf("\nHello World!\n");

    return NULL;
}
