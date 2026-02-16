#include "schedule.h"

/* ---- Utils Functions ---- */

gboolean is_version_valid(const gchar *version) {
    /*
    * is_version_valid control if the version verify the pattern x.x.x where x is a number
    */
    if (version == NULL) return FALSE;

    static GRegex *regex = NULL;
    if (G_UNLIKELY (regex == NULL)) {
        regex = g_regex_new("^([0-9]+)\\.([0-9]+)\\.([0-9]+)$", G_REGEX_OPTIMIZE, 0, NULL);
    }

    return g_regex_match(regex, version, 0, NULL);
}

void task_result_free(task_result_t *res) {
    /* 
    * task_result_free free the memory of an object task_result_t 
    * It's passed as GDestroyNotify in GHashTable
    */
    if (res) {
        if (res->output_data) {
            g_string_free(res->output_data, TRUE);
        }
        g_free(res);
    }
}


/* ---- Schedule Constructors ---- */

schedule_t* schedule_new(const gchar *name, const gchar *version){
    /* Validate input */
    g_return_val_if_fail(name != NULL, NULL);
    g_return_val_if_fail(version == NULL || is_version_valid(version), NULL);

    /* Struct memory allocation */
    schedule_t *sched = g_new0(schedule_t, 1);

    /* Setup struct fields */
    sched->schedule_name = g_string_new(name);
    if(version == NULL) sched->schedule_version = g_string_new("0.0.0"); else sched->schedule_version = g_string_new(version);
    sched->tasks = NULL;

    sched->results = g_hash_table_new_full(
        g_direct_hash, 
        g_direct_equal, 
        NULL, 
        (GDestroyNotify)task_result_free
    );

    /* Return the created schedule */
    return sched;
}



/* ---- Schedule Distructor ---- */
void schedule_free(schedule_t *sched) {
    if (!sched) return;

    g_string_free(sched->schedule_name, TRUE);
    g_string_free(sched->schedule_version, TRUE);

    /* Free all the tasks in the list */
    g_slist_free_full(sched->tasks, (GDestroyNotify)task_free);

    /* Free the hash table */
    g_hash_table_destroy(sched->results);

    g_free(sched);
}

/* ---- Schedule Getters ---- */
GSList* schedule_get_tasks(schedule_t *sched) {
    g_return_val_if_fail(sched != NULL, NULL);
    return sched->tasks;
}

GHashTable* schedule_get_results(schedule_t *sched) {
    g_return_val_if_fail(sched != NULL, NULL);
    return sched->results;
}

/* ---- Schedule Additional Operation ---- */
void schedule_add_task(schedule_t *sched, task_t *task){
    /* Validate input */
    g_return_if_fail(sched != NULL);
    g_return_if_fail(task != NULL);

    /* Add task to the list */
    sched->tasks = g_slist_append(sched->tasks, task);
    sched->length++;

    /* Initialize automatically an empty result for this task in the hash table */
    task_result_t *res = g_new0(task_result_t, 1);
    res->output_data = g_string_new("{}"); // JSON vuoto di default
    res->remaining_runs = task->repetition;

    /* We use GUINT_TO_POINTER for memorise the numeric id as key */
    g_hash_table_insert(sched->results, GUINT_TO_POINTER(task->task_id), res);
}

/* ---- Schedule Utils Functions ---- */

int compare_versions(const gchar *v1, const gchar *v2){
    /*
    * compare_versions returns:
    *    0 if v1 == v2
    *   -1 if v1 < v2
    *    1 if v1 > v2
    *   -2 if not valid version string
    */

    g_return_val_if_fail(is_version_valid(v1), -2);
    g_return_val_if_fail(is_version_valid(v2), -2);

    gchar **parts1 = g_strsplit(v1, ".", 3);
    gchar **parts2 = g_strsplit(v2, ".", 3);
    int result = 0;

    for (int i = 0; i < 3; i++) {
        int n1 = atoi(parts1[i]);
        int n2 = atoi(parts2[i]);

        if (n1 > n2) {
            result = 1;
            break;
        } else if (n1 < n2) {
            result = -1;
            break;
        }
    }

    g_strfreev(parts1);
    g_strfreev(parts2);
    return result;
}
