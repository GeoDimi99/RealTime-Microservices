#include <stdio.h>
#include <glib.h>
#include <signal.h> // Per la gestione dei segnali
#include <unistd.h> // Per sleep()
#include "schedule.h"
#include "execution_manager.h"

// Flag globale per controllare il ciclo principale
static volatile gboolean keep_running = TRUE;

// Gestore di segnali per SIGINT (Ctrl+C)
void int_handler(int dummy) {
    (void)dummy; // Parametro non utilizzato
    g_print("\n[SYSTEM] SIGINT ricevuto. Chiusura pulita dopo questo ciclo...\n");
    keep_running = FALSE;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv; // Parametri non utilizzati

    /* Registra il gestore di segnali per una chiusura pulita */
    signal(SIGINT, int_handler);

    /* ------ Init Execution Manager ------ */
    int exit_code = 0;
    execution_manager_t *em = em_new(DEFAULT_EXECUTION_MANAGER_NAME);
    if (!em) {
        g_error("[ERROR] Execution Manager: em_new failed.");
        return 1;
    }
    
    schedule_t *sched = NULL; // Inizializza a NULL

    g_print("=== Execution Manager Initialized ===\n");
    g_print("Premi Ctrl+C per uscire in modo pulito.\n\n");

    /* ------ Ciclo di Esecuzione Principale ------ */
    while (keep_running) {
        /* Libera la pianificazione precedente, se esiste */
        if (sched != NULL) {
            schedule_free(sched);
            sched = NULL; // Evita double-free all'uscita
        }

        /* Crea e popola una nuova pianificazione. */
        gchar *schedule_name = "schedule";
        sched = schedule_new(schedule_name, "0.0.1");
        if (!sched) {
            g_error("[ERROR] Execution Manager (%s) : scheduler creation failed.", schedule_name);
        }

        schedule_add_task(sched, 1, "sum", SCHED_POLICY_FIFO, 10, 1, NULL,
                          1 * 1000, 2 * 1000, "[{\"a\":10, \"b\":5}]");

        schedule_add_task(sched, 2, "subtract", SCHED_POLICY_FIFO, 8, 1, NULL,
                          1 * 1000, 7 * 1000, "[{\"a\":20, \"b\":8}]");

        schedule_add_task(sched, 3, "multiply", SCHED_POLICY_FIFO, 6, 1, NULL,
                          2 * 1000, 7 * 1000, "[{\"a\":4, \"b\":7}]");

        print_schedule(sched);

        /* Run the schedule */
        em_run_schedule(em, sched);

        if (keep_running) {
            g_print("\n[SYSTEM] Pianificazione completata. Riavvio in 5 secondi (o premi Ctrl+C per uscire)...\n\n");
            
            // Dormi per 5 secondi, ma controlla il flag ogni secondo
            for (int i = 0; i < 5 && keep_running; i++) {
                sleep(1);
            }
        }
    }

    g_print("\n[SYSTEM] Uscita dal ciclo principale. Pulizia delle risorse...\n");

    if (em) em_free(em);
    if (sched) schedule_free(sched);
    
    g_print("[SYSTEM] Pulizia completata. Uscita.\n");
    return exit_code;
}