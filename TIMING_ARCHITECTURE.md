# Architettura Corretta della Misurazione dei Tempi

## Principio Fondamentale

**I timestamp T2 e T3 devono essere misurati nel TASK WRAPPER (grpc_server.cpp), NON in app_task.h**

### Motivazione

`app_task.h` deve rimanere **generico e riutilizzabile** per qualsiasi tipo di task. Aggiungere logica di misurazione dei tempi lo renderebbe specifico per questo sistema di benchmarking.

## Architettura Corretta

```
┌─────────────────────────────────────────────────────────────────┐
│                    EXECUTION MANAGER                            │
│                                                                 │
│  ⏱️ T1: clock_gettime() prima di inviare gRPC                  │
│      └─> Salvato in task->t1_start_request                     │
│                                                                 │
│  📤 Invia richiesta gRPC con T1 nel payload                     │
│                          │                                      │
└──────────────────────────┼──────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                      TASK WRAPPER                               │
│                  (grpc_server.cpp)                              │
│                                                                 │
│  📥 Riceve richiesta con T1                                     │
│                                                                 │
│  pthread_create(&task_thread, &attr, task_main, ctx)           │
│                                                                 │
│  ⏱️ T2: clock_gettime() DOPO pthread_create()                  │
│      └─> Thread è stato creato e schedulato                    │
│      └─> Inviato in risposta STARTED                           │
│                                                                 │
│  ┌────────────────────────────────────────┐                    │
│  │         THREAD (task_main)             │                    │
│  │                                        │                    │
│  │  • Nessuna misurazione di tempo       │                    │
│  │  • Solo logica del task                │                    │
│  │  • Rimane generico                     │                    │
│  │                                        │                    │
│  └────────────────────────────────────────┘                    │
│                                                                 │
│  pthread_join(task_thread, NULL)                               │
│                                                                 │
│  ⏱️ T3: clock_gettime() DOPO pthread_join()                    │
│      └─> Thread è terminato                                    │
│      └─> Inviato in risposta COMPLETED                         │
│                                                                 │
│  📤 Invia risultato con T3                                      │
│                          │                                      │
└──────────────────────────┼──────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                    EXECUTION MANAGER                            │
│                                                                 │
│  📥 Riceve T2 (in risposta STARTED)                             │
│      └─> Salvato in task->t2_end_request                       │
│                                                                 │
│  📥 Riceve T3 (in risposta COMPLETED)                           │
│      └─> Salvato in task->t3_start_result                      │
│                                                                 │
│  ⏱️ T4: clock_gettime() dopo ricezione risultato               │
│      └─> Salvato in task->t4_end_result                        │
│                                                                 │
│  📊 STAMPA RECAP AUTOMATICO                                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Vantaggi di Questa Architettura

### ✅ Separazione delle Responsabilità
- **app_task.h**: Solo logica del task (generico)
- **grpc_server.cpp**: Gestione thread + misurazione tempi (specifico)

### ✅ Riusabilità
`app_task.h` può essere usato in qualsiasi contesto senza modifiche:
- Benchmark con timing
- Produzione senza overhead
- Test unitari
- Altri sistemi di deployment

### ✅ Precisione
- **T2**: Misurato subito dopo `pthread_create()` = tempo reale di creazione thread
- **T3**: Misurato subito dopo `pthread_join()` = tempo reale di completamento thread

### ✅ Overhead Minimo
Il thread esegue solo la logica del task, senza chiamate a `clock_gettime()` che potrebbero influenzare le performance.

## Implementazione

### File Modificati

#### 1. `task/sum/app_task.h` (GENERICO)
```c
typedef struct {
    pthread_mutex_t lock;
    task_service_state_t status;
    input_t input;
    output_t output;
    // ❌ NO timing fields
} task_context_t;

void* task_main(void* arg) {
    // ❌ NO clock_gettime() calls
    // Solo logica del task
}
```

#### 2. `services/task-wrapper/src/grpc_server.cpp` (SPECIFICO)
```cpp
// Crea thread
pthread_create(&task_thread, &attr, task_main, ctx);

// ⏱️ T2: Subito dopo creazione
struct timespec t2;
clock_gettime(CLOCK_MONOTONIC, &t2);
double t2_ms = t2.tv_sec * 1000.0 + t2.tv_nsec / 1000000.0;

// Invia ACK con T2
ack_response.set_t2_thread_start_ms(t2_ms);

// Attendi completamento
pthread_join(task_thread, NULL);

// ⏱️ T3: Subito dopo join
struct timespec t3;
clock_gettime(CLOCK_MONOTONIC, &t3);
double t3_ms = t3.tv_sec * 1000.0 + t3.tv_nsec / 1000000.0;

// Invia risultato con T3
result_response.set_t3_task_complete_ms(t3_ms);
```

## Confronto con Implementazione Precedente (SBAGLIATA)

### ❌ Implementazione Sbagliata
```c
// In app_task.h
void* task_main(void* arg) {
    // T2 misurato QUI
    clock_gettime(CLOCK_MONOTONIC, &t2);
    ctx->t2_thread_start_ms = t2_ms;
    
    // ... logica task ...
    
    // T3 misurato QUI
    clock_gettime(CLOCK_MONOTONIC, &t3);
    ctx->t3_task_complete_ms = t3_ms;
}
```

**Problemi:**
- ❌ app_task.h non è più generico
- ❌ Overhead di `clock_gettime()` nel thread
- ❌ Accoppiamento con sistema di timing
- ❌ Non riutilizzabile

### ✅ Implementazione Corretta
```cpp
// In grpc_server.cpp
pthread_create(&task_thread, &attr, task_main, ctx);
clock_gettime(CLOCK_MONOTONIC, &t2);  // T2

pthread_join(task_thread, NULL);
clock_gettime(CLOCK_MONOTONIC, &t3);  // T3
```

**Vantaggi:**
- ✅ app_task.h rimane generico
- ✅ Zero overhead nel thread
- ✅ Disaccoppiamento completo
- ✅ Completamente riutilizzabile

## Metriche Calcolate

Con questa architettura, le metriche sono ancora più precise:

- **Request Latency (T2-T1)**: Include tempo di rete + deserializzazione + `pthread_create()`
- **Task Execution (T3-T2)**: Tempo puro del thread (da creazione a terminazione)
- **Response Latency (T4-T3)**: Include serializzazione + tempo di rete
- **Total End-to-End (T4-T1)**: Tempo totale

## Build e Test

```bash
cd /home/khadas/RT-grpc/RealTime-Microservices

# Rebuild (app_task.h è cambiato)
sudo docker compose build task-service-sum execution-manager

# Test
sudo docker compose up execution-manager
```

## Conclusione

Questa architettura segue il principio di **separazione delle responsabilità**:
- Il **task** (app_task.h) si occupa solo della logica applicativa
- Il **wrapper** (grpc_server.cpp) si occupa di threading, comunicazione e benchmarking

Questo rende il sistema modulare, riutilizzabile e mantenibile.
