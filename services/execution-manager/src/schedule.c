#include "schedule.h"

/* ---- Utils Functions ---- */

gboolean is_version_valid(const gchar *version) {
    if (version == NULL) return FALSE;

    static GRegex *regex = NULL;
    if (G_UNLIKELY (regex == NULL)) {
        regex = g_regex_new("^([0-9]+)\\.([0-9]+)\\.([0-9]+)$", G_REGEX_OPTIMIZE, 0, NULL);
    }

    return g_regex_match(regex, version, 0, NULL);
}

static void expiration_data_free(expiration_data_t *data) {
    if (data != NULL) {
        if (data->task_name != NULL) {
            g_string_free(data->task_name, TRUE);
        }
        g_free(data);
    }
}

void expiration_list_free(gpointer data) {
    GSList *list = (GSList *)data;
    g_slist_free_full(list, (GDestroyNotify)expiration_data_free);
}

void task_result_free(gpointer data) {
    task_result_t *res = (task_result_t *)data;
    if (res->output_data != NULL) {
        g_string_free(res->output_data, TRUE);
        res->output_data = NULL;
    }
}

static void activation_data_free(gpointer data) {

    activation_data_t *act = (activation_data_t *)data;
    if (act) {
        g_string_free(act->task_name, TRUE);
        g_string_free(act->input_data, TRUE);
        g_slist_free(act->depends_on); 
        g_free(act);
    }
}

static void start_entry_free(gpointer data) {
    start_entry_t *entry = (start_entry_t *)data;
    if (entry) {
        g_slist_free_full(entry->activation_data, activation_data_free);
        g_free(entry);
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
    
    const gchar *v = (version == NULL) ? "0.0.0" : version;
    sched->schedule_version = g_string_new(v);

    sched->schedule_duration = 0;
    
    sched->schedule_start_info = g_queue_new();
    sched->schedule_end_info = g_hash_table_new_full(
        g_int64_hash, 
        g_int64_equal, 
        g_free, 
        (GDestroyNotify)expiration_list_free
    );
    sched->schedule_results = g_array_new(FALSE, TRUE, sizeof(task_result_t));
    g_array_set_clear_func(sched->schedule_results, (GDestroyNotify)task_result_free);

    /* Return the created schedule */
    return sched;
}



/* ---- Schedule Distructor ---- */
void schedule_free(schedule_t *sched) {
    if (!sched) return;

    g_string_free(sched->schedule_name, TRUE);
    g_string_free(sched->schedule_version, TRUE);

    g_queue_free_full(sched->schedule_start_info, (GDestroyNotify) start_entry_free);
    g_hash_table_destroy(sched->schedule_end_info);
    g_array_unref(sched->schedule_results);

    g_free(sched);
}

/* ---- Schedule Getters ---- */




/* ---- Schedule Additional Operation ---- */
void schedule_add_task(schedule_t *sched, 
                guint16 id, 
                const gchar *name, 
                sched_policy_t policy, 
                gint8 priority, 
                guint8 repetition, 
                gint64 start_time, 
                gint64 end_time, 
                const gchar *input);

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
