# ✅ Timing Measurements - Exact Match with Message Queue Implementation

## 🎯 Obiettivo

Modificare il codice gRPC per misurare T2 e T3 **esattamente come** l'implementazione con message queue, cioè **DENTRO il thread worker**.

## 📊 Confronto Implementazioni

### Message Queue (Originale)

```c
// Handler principale riceve messaggio
mq_receive(task_queue, &msg, ...);
↓
// Crea thread worker
pthread_create(&tid, &attr, task_wrapper_run_thread, t_in);
    ↓
    // DENTRO IL THREAD WORKER
    void * task_wrapper_run_thread(void *arg) {
        glong start_time_request = input_data->start_time_request;  // T1
        
        // ⏱️ T2 - MISURATO DENTRO IL THREAD
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        glong end_time_request = ts.tv_sec * 1000000000L + ts.tv_nsec;
        
        // Esegue task
        gpointer res = task_main(input_data->input);
        
        // ⏱️ T3 - MISURATO DENTRO IL THREAD
        clock_gettime(CLOCK_MONOTONIC, &ts);
        glong start_time_result = ts.tv_sec * 1000000000L + ts.tv_nsec;
        
        // Invia risultato con T1, T2, T3
        msg_out.start_time_request = start_time_request;
        msg_out.end_time_request = end_time_request;
        msg_out.start_time_result = start_time_result;
        mq_send(tw->em_queue, &msg_out, ...);
    }
```

### gRPC (Modificato - ESATTAMENTE UGUALE)

```cpp
// Server riceve richiesta
Status ExecuteTaskAsync(...) {
    double client_t1_ms = request->client_timestamp_ms();  // T1
    
    // Parsing, allocazione...
    task_context_t* ctx = malloc(...);
    convert_input(...);
    
    // Crea thread worker con WRAPPER
    pthread_create(&task_thread, &attr, task_main_wrapper, ctx);
        ↓
        // DENTRO IL THREAD WORKER (task_main_wrapper)
        void *task_main_wrapper(void *arg) {
            task_context_t *ctx = (task_context_t *)arg;
            
            // ⏱️ T2 - MISURATO DENTRO IL THREAD
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            ctx->t2_thread_entry_ms = ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
            
            // Esegue task
            task_main(arg);
            
            // ⏱️ T3 - MISURATO DENTRO IL THREAD
            clock_gettime(CLOCK_MONOTONIC, &ts);
            ctx->t3_task_complete_ms = ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
            
            return NULL;
        }
    
    // Invia STARTED
    TaskResponse ack;
    ack.set_status("STARTED");
    writer->Write(ack);
    
    // Aspetta completamento
    pthread_join(task_thread, NULL);
    
    // Legge T2 e T3 dal context (misurati dentro il thread!)
    double t2_ms = ctx->t2_thread_entry_ms;
    double t3_ms = ctx->t3_task_complete_ms;
    
    // Invia COMPLETED con T2 e T3
    TaskResponse result;
    result.set_t2_thread_start_ms(t2_ms);
    result.set_t3_task_complete_ms(t3_ms);
    writer->Write(result);
}
```

## 🔧 Modifiche Implementate

### 1. Aggiunto Campi Timestamp al Context

**File**: `services/task-wrapper/include/app_task.h`

```c
typedef struct {
    pthread_mutex_t lock;
    task_service_state_t status;
    input_t input;
    output_t output;
    
    // ✅ NUOVO: Timing measurements (measured INSIDE the worker thread)
    double t2_thread_entry_ms;      // T2: When thread starts
    double t3_task_complete_ms;     // T3: When task completes
} task_context_t;
```

### 2. Creato Thread Wrapper

**File**: `services/task-wrapper/include/app_task.h`

```c
void *task_main_wrapper(void *arg) {
    task_context_t *ctx = (task_context_t *)arg;
    
    // ⏱️ T2 - Measure INSIDE thread (like message queue)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ctx->t2_thread_entry_ms = ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
    
    // Execute actual task
    task_main(arg);
    
    // ⏱️ T3 - Measure INSIDE thread after completion
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ctx->t3_task_complete_ms = ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
    
    return NULL;
}
```

### 3. Modificato gRPC Server

**File**: `services/task-wrapper/src/grpc_server.cpp`

**Prima** ❌:
```cpp
// T2 misurato PRIMA di pthread_create
struct timespec t2;
clock_gettime(CLOCK_MONOTONIC, &t2);
double t2_ms = ...;

pthread_create(&task_thread, &attr, task_main, ctx);
pthread_join(task_thread, NULL);

// T3 misurato DOPO pthread_join
struct timespec t3;
clock_gettime(CLOCK_MONOTONIC, &t3);
double t3_ms = ...;
```

**Dopo** ✅:
```cpp
// NON misuriamo T2 qui!
pthread_create(&task_thread, &attr, task_main_wrapper, ctx);
                                     ^^^^^^^^^^^^^^^^^^
                                     Usa wrapper che misura T2 e T3 dentro

// Invia STARTED
TaskResponse ack;
ack.set_status("STARTED");
writer->Write(ack);

pthread_join(task_thread, NULL);

// Leggi T2 e T3 dal context (misurati dentro il thread!)
double t2_ms = ctx->t2_thread_entry_ms;
double t3_ms = ctx->t3_task_complete_ms;

// Invia COMPLETED con T2 e T3
result.set_t2_thread_start_ms(t2_ms);
result.set_t3_task_complete_ms(t3_ms);
```

## 📊 Cosa Misura Ogni Timestamp Ora

### T1 (Client)
**Dove**: `realtime_scheduler.c:205`
```c
clock_gettime(CLOCK_MONOTONIC, &t1);
double t1_ms = t1.tv_sec * 1000.0 + t1.tv_nsec / 1000000.0;
```
**Quando**: PRIMA di inviare richiesta gRPC
**Cosa**: Inizio invio richiesta

### T2 (Server - DENTRO IL THREAD)
**Dove**: `app_task.h:169` (task_main_wrapper)
```c
clock_gettime(CLOCK_MONOTONIC, &ts);
ctx->t2_thread_entry_ms = ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
```
**Quando**: Primo statement del thread worker
**Cosa**: Thread inizia esecuzione (IDENTICO a message queue!)

### T3 (Server - DENTRO IL THREAD)
**Dove**: `app_task.h:179` (task_main_wrapper)
```c
clock_gettime(CLOCK_MONOTONIC, &ts);
ctx->t3_task_complete_ms = ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
```
**Quando**: Subito dopo `task_main()` ritorna
**Cosa**: Task completa esecuzione (IDENTICO a message queue!)

### T4 (Client)
**Dove**: `realtime_scheduler.c:121`
```c
clock_gettime(CLOCK_MONOTONIC, &t4);
double t4_ms = t4.tv_sec * 1000.0 + t4.tv_nsec / 1000000.0;
```
**Quando**: DOPO aver ricevuto risposta gRPC
**Cosa**: Fine ricezione risultato

## ✅ Verifica Equivalenza

| Aspetto | Message Queue | gRPC (Nuovo) | Match? |
|---------|---------------|--------------|--------|
| T1 misurato | Prima mq_send | Prima grpc_send | ✅ SÌ |
| T2 misurato | DENTRO thread worker | DENTRO thread worker | ✅ SÌ |
| T3 misurato | DENTRO thread worker | DENTRO thread worker | ✅ SÌ |
| T4 misurato | Dopo mq_receive | Dopo grpc_receive | ✅ SÌ |
| T2-T1 include | IPC/Network | gRPC/Network | ✅ SÌ (semantica uguale) |
| T3-T2 include | **Solo task_main** | **Solo task_main** | ✅ SÌ (IDENTICO!) |
| T4-T3 include | IPC/Network | gRPC/Network | ✅ SÌ (semantica uguale) |

## 🎯 Metriche Corrette Ora

### Request Latency (T2 - T1)
**Cosa include**:
- Serializzazione protobuf (client)
- Trasmissione TCP/HTTP2
- Ricezione server
- Deserializzazione protobuf
- Parsing JSON
- Allocazione memoria
- Setup thread attributes
- **pthread_create (creazione thread)**
- **Scheduling del thread fino al primo statement**

**Nota**: Questo ora include l'overhead di creazione thread, che è corretto perché rappresenta il tempo fino a quando il thread **inizia effettivamente** a eseguire.

### Task Execution (T3 - T2)
**Cosa include**:
- ✅ **SOLO l'esecuzione di `task_main()`**
- ✅ **NIENTE ALTRO!**

Questo è **IDENTICO** al message queue!

### Response Latency (T4 - T3)
**Cosa include**:
- Serializzazione output
- Trasmissione TCP/HTTP2
- Ricezione client
- Deserializzazione protobuf

## 📝 Output di Esempio

```
[gRPC Server Async] Request received for task: sum (ID: 1) | Client T1=9360521078.725 ms
[THREAD WRAPPER] ⏱️ T2=9360521105.538 ms | Thread started (measured inside thread)
[THREAD] Processing: 10 + 5
[THREAD] Job Done. Result: 2
[THREAD WRAPPER] ⏱️ T3=9360524106.650 ms | Task completed (3001.112 ms execution)
[TASK WRAPPER] ⏱️ T2=9360521105.538 ms (measured in thread) | T3=9360524106.650 ms | Execution: 3001.112 ms
[EXECUTION MANAGER] ⏱️ T2=9360521105.538 ms | Server received request
[EXECUTION MANAGER] ⏱️ T3=9360524106.650 ms | Task completed (server measured)
[EXECUTION MANAGER] ⏱️ T4=9360524107.406 ms | Result received by client

╔═══════════════════════════════════════════════════════════════╗
║ METRICS:                                                      ║
║   Request Latency    =       26.813 ms  (T2 - T1)            ║
║   Task Execution     =     3001.112 ms  (T3 - T2) ← PURO!    ║
║   Response Latency   =        0.756 ms  (T4 - T3)            ║
╚═══════════════════════════════════════════════════════════════╝
```

## ✅ Conclusione

Il codice gRPC ora misura T2 e T3 **esattamente come** l'implementazione con message queue:

- ✅ T2 misurato DENTRO il thread worker al primo statement
- ✅ T3 misurato DENTRO il thread worker dopo task_main()
- ✅ T3-T2 rappresenta **SOLO** il tempo di esecuzione del task
- ✅ Semantica identica al message queue

**Il sistema ora è perfettamente comparabile!**
