#include "execution_manager.h"
#include "app_task.h"
#include <unistd.h>

gint iteration = 0; 

/* ----------------- Execution Manager Helper Functions ----------------- */
const char* redis_get_hash_value(redisReply *r, const char *key) {
    for (size_t i = 0; i < r->elements; i += 2) {
        if (strcmp(r->element[i]->str, key) == 0) {
            return r->element[i+1]->str;
        }
    }
    return NULL;
}

GList* json_array_to_gstring_list(JsonArray *array) {
    if (!array) return NULL;

    GList *list = NULL;
    guint length = json_array_get_length(array);

    for (guint i = 0; i < length; i++) {
        const char *value = json_array_get_string_element(array, i);
        
        // Create a new GString object for each element
        GString *gs = g_string_new(value);
        
        // Append the GString pointer to the GList
        list = g_list_append(list, gs);
    }

    return list;
}

static void print_performance_metrics(guint16 task_id, glong scheduled_ns, glong activated_ns, glong exec_start_ns, glong result_ns) {
    double jitter_ms   = (activated_ns  - scheduled_ns)  / 1e6;
    double dep_wait_ms = (exec_start_ns - activated_ns)  / 1e6;
    double exec_ms     = (result_ns     - exec_start_ns) / 1e6;
    double total_ms    = (result_ns     - scheduled_ns)  / 1e6;

    g_print("PERF_LOG:%d,%u,%ld,%ld,%ld,%ld,%.3f,%.3f,%.3f,%.3f\n", iteration,
            task_id, scheduled_ns, activated_ns, exec_start_ns, result_ns,
            jitter_ms, dep_wait_ms, exec_ms, total_ms);
}


gint em_get_iterations(execution_manager_t *em) {
    redisReply *r = redisCommand(em->redis_client, "HGET schedule_data schedule:iterations");
    gint result = 1;
    if (r && r->type == REDIS_REPLY_STRING && r->str) {
        result = atoi(r->str);
        if (result <= 0) result = 1;
    }
    if (r) freeReplyObject(r);
    return result;
}

/* ----------------- Executor Manager Constructor/Distructors ----------------- */
execution_manager_t* em_new(const gchar *name){
    g_return_val_if_fail(name != NULL, NULL);

    execution_manager_t *em = g_new0(execution_manager_t, 1);
    em->em_name = g_string_new(name);

    /* Create ZeroMQ context (one per process) */
    em->zmq_ctx = zmq_ctx_new();
    if (!em->zmq_ctx) {
        g_error("[ERROR] Execution Manager: zmq_ctx_new failed.");
    }

    /* Initialize socket pointers to NULL (created in em_run_schedule based on role) */
    em->zmq_pull = NULL;
    em->zmq_push = NULL;
    em->zmq_pub = NULL;
    em->zmq_sub = NULL;

    /* Connect to Redis */
    redisContext *c = redisConnect("redis", 6379);
    if (c != NULL && c->err ){
        g_error("[ERROR] Execution Manager (redis): %s\n", c->errstr);
    }
    em->redis_client = c;

    return em;
}


void em_free(execution_manager_t *em){
    if (!em) return;

    g_string_free(em->em_name, TRUE);

    /* Close ZeroMQ sockets */
    if (em->zmq_pull) zmq_close(em->zmq_pull);
    if (em->zmq_push) zmq_close(em->zmq_push);
    if (em->zmq_pub)  zmq_close(em->zmq_pub);
    if (em->zmq_sub)  zmq_close(em->zmq_sub);
    if (em->zmq_ctx)  zmq_ctx_destroy(em->zmq_ctx);

    redisFree(em->redis_client);

    g_free(em);
}
/* ----------------- Executor Manager Getters/Setters ----------------- */
void em_set_leader(execution_manager_t *em, gboolean leader_flag){
    g_return_if_fail(em != NULL);

    em->is_leader = leader_flag;
    return; 
}


/* ----------------- Executor Manager Activities ----------------- */

void em_run_schedule(execution_manager_t *em, schedule_t *sched) {
    g_return_if_fail(em != NULL);
    g_return_if_fail(sched != NULL);

    ipc_msg_t msg;
    gint64 time_zero_us;

    if (em->is_leader) {
        /* --- LEADER LOGIC --- */

        /* Create PULL socket to receive READY from workers */
        if (!em->zmq_pull) {
            em->zmq_pull = zmq_socket(em->zmq_ctx, ZMQ_PULL);
            if (zmq_bind(em->zmq_pull, ZMQ_LEADER_PULL_ENDPOINT) == -1) {
                g_error("[ERROR] Leader: zmq_bind PULL failed: %s", zmq_strerror(errno));
            }
        }

        /* Create PUB socket to broadcast SYNC to workers */
        if (!em->zmq_pub) {
            em->zmq_pub = zmq_socket(em->zmq_ctx, ZMQ_PUB);
            if (zmq_bind(em->zmq_pub, ZMQ_LEADER_PUB_ENDPOINT) == -1) {
                g_error("[ERROR] Leader: zmq_bind PUB failed: %s", zmq_strerror(errno));
            }
            /* Small sleep to allow SUB sockets to connect (slow-joiner problem) */
            g_usleep(200000);
        }

        guint ready_count = 0;
        guint target_count = g_list_length(sched->schedule_images);

        g_print("[SYNC] Leader: Waiting for %d workers to be READY...\n", target_count);

        /* Blocking receive: no polling needed */
        while (ready_count < target_count) {
            if (zmq_recv(em->zmq_pull, &msg, sizeof(msg), 0) == -1) {
                g_printerr("[ERROR] Leader: zmq_recv PULL failed: %s\n", zmq_strerror(errno));
                continue;
            }
            if (msg.type == MSG_TASK_READY) {
                ready_count++;
                g_print("[SYNC] Leader: Worker %d/%d is ready.\n", ready_count, target_count);
            }
        }

        /* All ready! Set T-Zero to (Now + 500ms) to account for latency */
        time_zero_us = g_get_monotonic_time() + 500000;

        /* Broadcast T-Zero to all workers via PUB (single send) */
        msg.type = MSG_TASK_SYNC;
        msg.data.sync_time_us = time_zero_us;

        if (zmq_send(em->zmq_pub, &msg, sizeof(msg), 0) == -1) {
            g_printerr("[ERROR] Leader: zmq_send PUB failed: %s\n", zmq_strerror(errno));
        }
        g_print("[SYNC] Leader: Barrier released. Sync time sent.\n");

    } else {
        /* --- WORKER LOGIC --- */

        /* Create PUSH socket to send READY to leader */
        if (!em->zmq_push) {
            em->zmq_push = zmq_socket(em->zmq_ctx, ZMQ_PUSH);
            if (zmq_connect(em->zmq_push, ZMQ_LEADER_PULL_ENDPOINT) == -1) {
                g_error("[ERROR] Worker: zmq_connect PUSH failed: %s", zmq_strerror(errno));
            }
        }

        /* Create SUB socket to receive SYNC from leader */
        if (!em->zmq_sub) {
            em->zmq_sub = zmq_socket(em->zmq_ctx, ZMQ_SUB);
            zmq_setsockopt(em->zmq_sub, ZMQ_SUBSCRIBE, "", 0);  /* Subscribe to all messages */
            if (zmq_connect(em->zmq_sub, ZMQ_LEADER_PUB_ENDPOINT) == -1) {
                g_error("[ERROR] Worker: zmq_connect SUB failed: %s", zmq_strerror(errno));
            }
            /* Small sleep to let the SUB connection establish */
            g_usleep(100000);
        }

        /* Signal READY to leader via PUSH */
        msg.type = MSG_TASK_READY;
        msg.task_id = 0;

        g_print("[SYNC] Worker: Signaling READY to leader...\n");
        if (zmq_send(em->zmq_push, &msg, sizeof(msg), 0) == -1) {
            g_printerr("[ERROR] Worker: zmq_send PUSH failed: %s\n", zmq_strerror(errno));
        }

        /* Wait for SYNC from leader via SUB (blocking) */
        g_print("[SYNC] Worker: Waiting for SYNC signal from leader...\n");
        if (zmq_recv(em->zmq_sub, &msg, sizeof(msg), 0) == -1) {
            g_printerr("[ERROR] Worker: zmq_recv SUB failed: %s\n", zmq_strerror(errno));
        }
        time_zero_us = msg.data.sync_time_us;
        g_print("[SYNC] Worker: Received sync time. Starting timers...\n");
    }

    g_print("[SYNC] Final Synchronization Complete. T-Zero (Monotonic): %ld us\n", (long)time_zero_us);

    /* --- THE REST OF YOUR FUNCTION (Unchanged timers logic) --- */
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    

    /* 1. Plan the scheudle DEADLINES */
    for (GList *l = sched->schedule_end_info->head; l != NULL; l = l->next) {
        timeline_entry_t *entry = (timeline_entry_t *)l->data;

        /* CORREZIONE: Uso deadline_context_t invece di context_t */
        deadline_context_t *ctx = g_new0(deadline_context_t, 1);
        ctx->data = entry->data_list;
        ctx->loop = loop;
        ctx->timestamp = entry->timestamp;
        ctx->is_last = (l->next == NULL); // Se è l'ultimo nodo della GQueue
        ctx->sched = sched;

        gint64 target_mono_us = time_zero_us + (entry->timestamp * 1000);

        GSource *source = g_timeout_source_new(0);
        g_source_set_ready_time(source, target_mono_us);
        g_source_set_callback(source, handle_expiration, ctx, NULL);
        g_source_attach(source, g_main_loop_get_context(loop));
        g_source_unref(source);
    }

    /* 1.5 Pre-initialize ZMQ output channels (once; skip if already created) */
    for (GList *l = sched->schedule_start_info->head; l != NULL; l = l->next) {
        timeline_entry_t *entry = (timeline_entry_t *)l->data;
        for (GSList *tl = entry->data_list; tl != NULL; tl = tl->next) {
            activation_data_t *task = (activation_data_t *)tl->data;
            if (task->zmq_ctx == NULL) {
                char push_path[256];
                char push_addr[280];
                snprintf(push_path, sizeof(push_path), "/dev/shm/task_out_%u", task->task_id);
                snprintf(push_addr, sizeof(push_addr), "ipc://%s", push_path);
                task->zmq_ctx  = zmq_ctx_new();
                task->push_sock = zmq_socket(task->zmq_ctx, ZMQ_PUSH);
                int conflate = 1;
                zmq_setsockopt(task->push_sock, ZMQ_CONFLATE, &conflate, sizeof(conflate));
                unlink(push_path);
                zmq_bind(task->push_sock, push_addr);
                g_print("[SETUP] Task %u: ZMQ output channel ready at %s\n", task->task_id, push_addr);
            }
        }
    }

    /* 2. Plan the schedule STARTS */
    for (GList *l = sched->schedule_start_info->head; l != NULL; l = l->next) {
        timeline_entry_t *entry = (timeline_entry_t *)l->data;

        start_context_t *ctx = g_new0(start_context_t, 1);
        ctx->data = entry->data_list;
        ctx->timestamp = entry->timestamp;
        ctx->sched = sched;

        

        gint64 target_mono_us = time_zero_us + (entry->timestamp * 1000);
        ctx->scheduled_time_ns = target_mono_us * 1000LL;

        GSource *source = g_timeout_source_new(0);
        g_source_set_ready_time(source, target_mono_us);
        g_source_set_callback(source, handle_initialization, ctx, NULL);
        g_source_attach(source, g_main_loop_get_context(loop));
        g_source_unref(source);
    }

    g_print("[INFO] Execution Manager: Scheduler started! Waiting for events...\n");
    g_main_loop_run(loop);
    
    g_main_loop_unref(loop);
    g_print("[INFO] Execution Manager: Scheduler terminated successfully.\n");
}

void em_wait_for_schedule(execution_manager_t *em) {
    while (1) {
        // Updated to check for the new hash name
        redisReply *r = redisCommand(em->redis_client, "EXISTS schedule_data");
        if (r && r->integer == 1) {
            freeReplyObject(r);
            return;
        }
        if (r) freeReplyObject(r);
        g_print("[INFO] Waiting for schedule_data in Redis...\n");
        sleep(1);
    }
}



schedule_t* em_read_schedule(execution_manager_t *em) {
    // 1. Fetch the entire flat hash from Redis
    redisReply *r = redisCommand(em->redis_client, "HGETALL schedule_data");
    if (!r || r->type == REDIS_REPLY_ERROR || r->elements == 0) {
        if (r) freeReplyObject(r);
        return NULL;
    }

    const char *name = redis_get_hash_value(r, "schedule:name");
    const char *version = redis_get_hash_value(r, "schedule:version");
    const char *leader = redis_get_hash_value(r, "schedule:leader");
    const char *duration_str = redis_get_hash_value(r, "schedule:duration");
    const char *images_json = redis_get_hash_value(r, "schedule:images");

    // 2. Parse the list of image names
    JsonParser *parser = json_parser_new();
    if (!json_parser_load_from_data(parser, images_json, -1, NULL)) {
        g_warning("Failed to parse images JSON");
        freeReplyObject(r);
        return NULL; 
    }

    JsonArray *images_array = json_node_get_array(json_parser_get_root(parser));
    GList* images_list = json_array_to_gstring_list(images_array);

    GList *l, *next;

    for (l = images_list; l != NULL; l = next) {
        next = l->next; // Always capture next at the start of the loop
        GString *gs = (GString *)l->data;

        if (g_strcmp0(em->em_name->str, gs->str) == 0) {
            // Unlink the current node from the list
            images_list = g_list_remove_link(images_list, l);
            
            // Free the data (GString)
            g_string_free(gs, TRUE);
            
            // Free the specific list node structure
            g_list_free_1(l);

            break;
        }
    }
    

    if (!name || !version || !leader || !images_json) {
        freeReplyObject(r);
        return NULL;
    }

    // Set Leader Flag
    em_set_leader(em, g_strcmp0(em->em_name->str, leader) == 0);
    //g_print("[DEBUG] Execution Manager: is_leader %s\n", (g_strcmp0(em->em_name->str, leader) == 0) ? "TRUE" : "FALSE");

    // Create the schedule
    schedule_t *sched = schedule_new(name, version, leader, images_list, duration_str ? g_ascii_strtoll(duration_str, NULL, 10) : 0);

    // Task Loading into the schedule
    gchar *img_name = em->em_name->str;
    char len_key[128];
    snprintf(len_key, sizeof(len_key), "schedule:%s:length", img_name);
    const char *img_task_count_str = redis_get_hash_value(r, len_key);
    int img_task_count = img_task_count_str ? atoi(img_task_count_str) : 0;

    for (int j = 0; j < img_task_count; j++) {
        char task_key[128];
        snprintf(task_key, sizeof(task_key), "schedule:%s:%d", img_name, j);
        const char *task_json = redis_get_hash_value(r, task_key);

        if (task_json) {
            JsonParser *t_parser = json_parser_new();
            if (json_parser_load_from_data(t_parser, task_json, -1, NULL)) {
                JsonObject *t_obj = json_node_get_object(json_parser_get_root(t_parser));

                // --- FIELD MAPPING LOGIC ---

                // A. Numeric fields (Direct)
                guint16 t_id = (guint16)json_object_get_int_member(t_obj, "id");
                gint64 start_time = json_object_get_int_member(t_obj, "start");
                gint64 end_time = json_object_get_int_member(t_obj, "deadline");

                // B. Stringified Numeric fields ("1", "10") -> Use atoi()
                const char *cpu_str = json_object_get_string_member(t_obj, "cpu_affinity");
                const char *prio_str = json_object_get_string_member(t_obj, "priority");
                
                gint cpu = cpu_str ? atoi(cpu_str) : 0;
                gint8 priority = prio_str ? (gint8)atoi(prio_str) : 0;

                // C. Policy String ("fifo", "rr") -> Map to C constants
                const char *policy_name = json_object_get_string_member(t_obj, "policy");
                gint policy = SCHED_OTHER; // Default to 0
                if (policy_name != NULL) {
                    if (g_ascii_strcasecmp(policy_name, "fifo") == 0) {
                        policy = SCHED_FIFO;
                    } else if (g_ascii_strcasecmp(policy_name, "rr") == 0) {
                        policy = SCHED_RR;
                    }
                }

                // D. Inputs (Read as string to prevent JSON nested-node errors)
                const char *t_input = json_object_get_string_member(t_obj, "inputs");

                // E. Depends On: parse "[{\"id\":1,\"param\":\"total_ops\"}]"
                GSList *dep_list = NULL;
                const char *dep_json_str = json_object_get_string_member(t_obj, "depends_on");
                if (dep_json_str && strlen(dep_json_str) > 2) {
                    JsonParser *dep_parser = json_parser_new();
                    if (json_parser_load_from_data(dep_parser, dep_json_str, -1, NULL)) {
                        JsonNode *dep_root = json_parser_get_root(dep_parser);
                        if (JSON_NODE_HOLDS_ARRAY(dep_root)) {
                            JsonArray *dep_arr = json_node_get_array(dep_root);
                            for (guint di = 0; di < json_array_get_length(dep_arr); di++) {
                                JsonObject *dep_obj = json_array_get_object_element(dep_arr, di);
                                dep_entry_t *entry = g_new0(dep_entry_t, 1);
                                entry->dep_task_id = (guint16)json_object_get_int_member(dep_obj, "id");
                                const char *p = json_object_get_string_member(dep_obj, "param");
                                entry->param = g_strdup(p ? p : "result");
                                dep_list = g_slist_append(dep_list, entry);
                            }
                        }
                    }
                    g_object_unref(dep_parser);
                }

                schedule_add_task(
                    sched,
                    t_id,
                    img_name,
                    (GThreadFunc)task_main, 
                    policy,
                    priority,
                    cpu,
                    1, 
                    dep_list, 
                    start_time,
                    end_time,
                    g_strdup(t_input ? t_input : "{}")
                );
            }
            g_object_unref(t_parser);
        }
    }

    g_object_unref(parser);
    freeReplyObject(r);
    return sched;
}


/* ----------------- Executor Manager Usefull Functions ----------------- */

static gchar* merge_dep_output_into_input(const gchar *input_json, const gchar *dep_output_json, const gchar *param) {
    gchar *result = NULL;
    JsonParser *in_parser  = json_parser_new();
    JsonParser *out_parser = json_parser_new();

    if (!json_parser_load_from_data(in_parser,  input_json,      -1, NULL) ||
        !json_parser_load_from_data(out_parser, dep_output_json, -1, NULL)) {
        result = g_strdup(input_json);
        goto cleanup;
    }

    JsonNode   *in_root  = json_parser_get_root(in_parser);
    JsonObject *in_obj   = NULL;
    if (JSON_NODE_HOLDS_ARRAY(in_root)) {
        JsonArray *arr = json_node_get_array(in_root);
        if (json_array_get_length(arr) > 0)
            in_obj = json_array_get_object_element(arr, 0);
    } else if (JSON_NODE_HOLDS_OBJECT(in_root)) {
        in_obj = json_node_get_object(in_root);
    }

    JsonNode   *out_root = json_parser_get_root(out_parser);
    JsonObject *out_obj  = NULL;
    if (JSON_NODE_HOLDS_OBJECT(out_root))
        out_obj = json_node_get_object(out_root);

    if (in_obj && out_obj && json_object_has_member(out_obj, param)) {
        JsonNode *val = json_object_get_member(out_obj, param);
        json_object_set_member(in_obj, param, json_node_copy(val));
    }

    JsonGenerator *gen = json_generator_new();
    json_generator_set_root(gen, in_root);
    result = json_generator_to_data(gen, NULL);
    g_object_unref(gen);

cleanup:
    g_object_unref(in_parser);
    g_object_unref(out_parser);
    return result ? result : g_strdup(input_json);
}

void* task_wrapper_func(void* data){
    
    task_wrapper_input_t* tw_input = (task_wrapper_input_t*)data;
    
    /* Read the thread context arguments */
    guint16 task_id              = tw_input->task_id;
    GThreadFunc thread_func      = tw_input->thread_func;
    glong scheduled_time_ns      = tw_input->scheduled_time_ns;
    glong start_time_request     = tw_input->start_time_request;
    schedule_t* sched            = tw_input->sched;
    GSList *depends_on           = tw_input->depends_on;

    /* --- 1. Use pre-created PUSH socket (bound in handle_initialization) --- */
    void *zmq_ctx  = tw_input->zmq_ctx;
    void *push_sock = tw_input->push_sock;

    /* --- 2. Resolve dependencies: PULL output from each dep and merge into input --- */
    gchar *actual_input = g_strdup((const char *)tw_input->data);
    for (GSList *dep = depends_on; dep != NULL; dep = dep->next) {
        dep_entry_t *entry = (dep_entry_t *)dep->data;
        char dep_path[256];
        char dep_addr[280];
        snprintf(dep_path, sizeof(dep_path), "/dev/shm/task_out_%u", entry->dep_task_id);
        snprintf(dep_addr, sizeof(dep_addr), "ipc://%s", dep_path);

        /* Wait until the dependency's socket file exists (it binds at thread start) */
        int waited = 0;
        while (access(dep_path, F_OK) != 0 && waited < 30000) {
            usleep(1000);
            waited++;
        }

        void *pull_sock = zmq_socket(zmq_ctx, ZMQ_PULL);
        zmq_connect(pull_sock, dep_addr);

        char dep_buf[MAX_TASK_JSON_OUT] = {0};
        int rc = zmq_recv(pull_sock, dep_buf, sizeof(dep_buf) - 1, 0);
        zmq_close(pull_sock);

        if (rc > 0) {
            g_print("[DEP] Task %u received output from task %u for param '%s': %s\n",
                    task_id, entry->dep_task_id, entry->param, dep_buf);
            gchar *merged = merge_dep_output_into_input(actual_input, dep_buf, entry->param);
            g_free(actual_input);
            actual_input = merged;
        }
    }

    /* --- 3. Performance: mark end of queuing --- */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    glong end_time_request = ts.tv_sec * 1000000000L + ts.tv_nsec;

    g_print("[INFO] ThreadCall %u: start thread function.\n", task_id);

    /* --- 4. Run the task --- */
    gpointer res = thread_func(actual_input);

    /* --- 5. Measure result start --- */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    glong start_time_result = ts.tv_sec * 1000000000L + ts.tv_nsec;

    g_print("[INFO] ThreadCall %u: termination thread function. \n", task_id);

    /* --- 6. Serialize output --- */
    gchar *out_json = convert_output_to_json((output_t *)res);

    /* --- 7. Store result in schedule --- */
    schedule_set_result(sched, task_id, out_json);

    /* --- 8. Performance: end (before ZMQ close to avoid linger delay) --- */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    glong end_time_result = ts.tv_sec * 1000000000L + ts.tv_nsec;

    print_performance_metrics(task_id, scheduled_time_ns, start_time_request, end_time_request, end_time_result);

    /* --- 9. PUSH output to socket (persistent channel — do not close) --- */
    zmq_send(push_sock, out_json, strlen(out_json), 0);

    /* Cleanup */
    g_free(out_json);
    g_free(actual_input);
    g_free(res);
    g_free(tw_input);
    return NULL;
}

/* ----------------- Executor Manager Event Handlers ----------------- */

gboolean handle_initialization(gpointer user_data) {
    start_context_t *ctx = (start_context_t *)user_data;
    GSList *tasks = (GSList *)ctx->data;
    schedule_t* sched = ctx->sched;


    if (tasks == NULL) {
        g_print("[ERROR] Execution Manager: No tasks\n");
        g_free(ctx);
        return G_SOURCE_REMOVE;
    }

    for (GSList *l = tasks; l != NULL; l = l->next) {
        
        /* Read the current task information */
        activation_data_t *task = (activation_data_t *)l->data;

        /* Measure T2: ZMQ channel already exists — dep_wait_ms = pure thread startup */
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        glong start_time_request = ts.tv_sec * 1000000000L + ts.tv_nsec;

        /* Prepare the thread (wrapper) input */
        task_wrapper_input_t* tw_input = g_new0(task_wrapper_input_t, 1);
        tw_input->task_id = task->task_id;
        tw_input->data = task->input_data;
        tw_input->thread_func = task->task_exec;
        tw_input->scheduled_time_ns = ctx->scheduled_time_ns;
        tw_input->start_time_request = start_time_request;
        tw_input->sched = sched;
        tw_input->depends_on = task->depends_on;
        tw_input->zmq_ctx  = task->zmq_ctx;
        tw_input->push_sock = task->push_sock;


        
        /* Prepare the thread */
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        /* Set CPU Affinity core */
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(task->cpu_affinity, &set);
        gint affinity_err = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &set);
        if (affinity_err != 0) {
            g_warning("[WARNING] Execution Manager: Failed to set CPU affinity for Task ID %u. Error: %d (%s)", task->task_id, affinity_err, g_strerror(affinity_err));
        }

        /* Setting scheduler policy and priority */
        struct sched_param param;
        param.sched_priority = task->priority;

        pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);
        pthread_attr_setschedpolicy(&attr, task->policy);
        pthread_attr_setschedparam(&attr, &param);
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

        pthread_t thread;
        gint rc = pthread_create(&thread, &attr, task_wrapper_func, tw_input);
        pthread_attr_destroy(&attr); // Clean up attributes
        if (rc) {
            g_printerr("[ERROR] Execution Manager: pthread_create failed with code %d (%s) for Task ID %u\n", rc, g_strerror(rc), task->task_id);
            continue;
        }
        pthread_detach(thread);

        // Iterate through GSList of dependencies
        if (task->depends_on) {
            g_print("[INFO] Depends On (IDs): ");
            for (GSList *dep = task->depends_on; dep != NULL; dep = dep->next) {
                // Assuming the list stores integers cast to pointers, or pointers to guint
                // If they are pointers to uint: *(guint*)dep->data
                // If they are IDs stored directly in the pointer: GPOINTER_TO_UINT(dep->data)
                g_print("%u ", GPOINTER_TO_UINT(dep->data));
            }
            g_print("\n");
        } else {
            g_print("[INFO] Depends On: None\n");
        }
    }

    g_free(ctx); 
    return G_SOURCE_REMOVE;
}



gboolean handle_expiration(gpointer user_data) {
    deadline_context_t *ctx = (deadline_context_t *)user_data;
    GSList *tasks = (GSList *)ctx->data;

    if (tasks == NULL) {
        g_print("[INFO] Execution Manager: No tasks to expire\n");
    } else {
        for (GSList *l = tasks; l != NULL; l = l->next) {
            expiration_data_t *exp = (expiration_data_t *)l->data;
            
            /* Check if the task is jet completed*/
            if (schedule_is_task_completed(ctx->sched, exp->task_id)) {
                g_print("[INFO] Execution Manager: Task %u already completed.\n", exp->task_id);
                continue;
            }
            g_print("[INFO] Execution Manager: Sent ABORT for Task ID %u\n", exp->task_id);
        }
    }

    if (ctx->is_last) {
        g_print("[INFO] Execution Manager (handle_expiration): Final deadline reached. Quitting...\n");
        g_main_loop_quit(ctx->loop);
    }

    g_free(ctx); 
    return G_SOURCE_REMOVE;
}