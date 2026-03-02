#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <glib.h>

#include "task_wrapper.h"
#include "task_wrapper_message_event.h"





int main(int argc, char *argv[]) {
    int exit_code = 0;

    //const gchar *json_test = "[ {\"a\":2, \"b\":3}, {\"a\":3, \"b\":9} ]";
    //GSList *list = parse_input_list(json_test);

    //GSList *node;
    //for (node = list; node != NULL; node = node->next) {
        //input_t *elem = (input_t*) node->data;
        //print_input(elem);  // usa la funzione che hai definito
    //}
    //free_input_list(list);

    //return 0;
    //parse_and_print_input(json_test);
    //return 0;

    /* Memory Locking per Real-Time */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("mlockall failed");
        return -1;
    }

    /* Init Task Wrapper */
    char *env_task = getenv("TASK_NAME");
    char *env_queue = getenv("TASK_QUEUE_NAME");
    
    
    gchar *q_path = g_strdup_printf("/%s_q", env_queue ? env_queue : DEFAULT_TASK_QUEUE);

    
    task_wrapper_t *tw = task_wrapper_new(
        env_task ? env_task : DEFAULT_TASK_NAME, 
        q_path, 
        "/execution_manager_q"
    );
    g_free(q_path);

    /* 3. Setup GMainLoop e Sorgenti eventi */
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // Trasformiamo il descrittore mqd_t in un canale monitorabile da GLib
    GIOChannel *channel = g_io_channel_unix_new((gint)tw->task_queue);
    
    // Molto importante: mq_receive vuole un canale binario senza codifiche strane
    g_io_channel_set_encoding(channel, NULL, NULL);
    g_io_channel_set_buffered(channel, FALSE);

    // Colleghiamo la coda al loop. Passiamo 'tw' per averlo nella callback.
    g_io_add_watch(channel, G_IO_IN, (GIOFunc)handle_input_message, tw);

    g_print("[INFO] Task Wrapper (%s): It's ready. Waiting for messages ...\n", tw->task_name->str);

    /* Start Loop */
    g_main_loop_run(loop);

    /* Cleanup */
    g_io_channel_unref(channel);
    g_main_loop_unref(loop);
    task_wrapper_free(tw);

    return exit_code;
}