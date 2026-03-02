#ifndef APP_TASK_H
#define APP_TASK_H

#include <stdlib.h>
#include <glib.h>
#include <json-glib/json-glib.h>

/* --- Data Structures --- */

typedef struct {
    int a;
    int b;
} input_t;

typedef struct{
    int result;
} output_t;




void print_input(input_t* input);

/* Input/Output Task Functions */
int convert_json_to_input(JsonObject *obj, input_t* input);
gchar* convert_output_to_json(const output_t *output);

/* Main Task */
output_t *task_main(input_t *arg);



#endif