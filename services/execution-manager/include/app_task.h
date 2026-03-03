#ifndef APP_TASK_H
#define APP_TASK_H

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
//#include <json-glib/json-glib.h>

/* --- Data Structures --- */

typedef struct {
    int a;
    int b;
} input_t;

typedef struct{
    int result;
} output_t;




//void print_input(input_t* input);

void* task_main(void* data);



#endif