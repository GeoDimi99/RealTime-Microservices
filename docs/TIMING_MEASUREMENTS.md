# Misurazione Tempi End-to-End

## 📊 Schema dei Timestamp

```
CLIENT (Execution Manager)          NETWORK          SERVER (Task Wrapper)
─────────────────────────────────────────────────────────────────────────

T1: Send gRPC Request ──────────────────────────────────────────────────┐
    clock_gettime()                                                     │
    [realtime_scheduler.c:205]                                          │
                                                                         │ Request
                                                                         │ Latency
                                                                         │ (Network +
                                                                         │  gRPC)
                                                                         │
                                    ┌───────────────────────────────────┘
                                    │
                                    └──────────> T2: Request Received
                                                    clock_gettime()
                                                    [grpc_server.cpp:147]
                                                    
                                                    ┌─ Parse input
                                                    ├─ Create thread
                                                    ├─ Set RT policy
                                                    └─ Send ACK
                                                    
                                                    [Task Execution]
                                                    pthread_join()
                                                    
                                                    T3: Task Completed
                                                    clock_gettime()
                                                    [grpc_server.cpp:297]
                                                    
                                    ┌───────────────────────────────────┐
                                    │                                   │ Response
                                    │                                   │ Latency
                                    │                                   │ (Network +
                                    │                                   │  gRPC)
T4: Result Received ◄───────────────┘                                   │
    clock_gettime()                                                     │
    [realtime_scheduler.c:121]      ◄───────────────────────────────────┘
```

## ⏱️ Definizione dei Timestamp

### T1 - Client Send Request
**Dove**: `services/execution-manager/src/realtime_scheduler.c:205`
```c
struct timespec t1;
clock_gettime(CLOCK_MONOTONIC, &t1);
double t1_ms = t1.tv_sec * 1000.0 + t1.tv_nsec / 1000000.0;
```
**Cosa misura**: Momento in cui il client (Execution Manager) invia la richiesta gRPC.

### T2 - Server Receive Request
**Dove**: `services/task-wrapper/src/grpc_server.cpp:147`
```cpp
struct timespec t2;
clock_gettime(CLOCK_MONOTONIC, &t2);
double t2_ms = t2.tv_sec * 1000.0 + t2.tv_nsec / 1000000.0;
```
**Cosa misura**: Momento in cui il server (Task Wrapper) riceve la richiesta.
**Importante**: Misurato IMMEDIATAMENTE all'inizio di `ExecuteTaskAsync()`, prima di qualsiasi processing.

### T3 - Server Task Complete
**Dove**: `services/task-wrapper/src/grpc_server.cpp:297`
```cpp
pthread_join(task_thread, NULL);  // Wait for task to complete

struct timespec t3;
clock_gettime(CLOCK_MONOTONIC, &t3);
double t3_ms = t3.tv_sec * 1000.0 + t3.tv_nsec / 1000000.0;
```
**Cosa misura**: Momento in cui il task completa l'esecuzione.
**Importante**: Misurato IMMEDIATAMENTE dopo `pthread_join()`, che ritorna non appena il thread termina.

### T4 - Client Receive Result
**Dove**: `services/execution-manager/src/realtime_scheduler.c:121`
```c
struct timespec t4;
clock_gettime(CLOCK_MONOTONIC, &t4);
double t4_ms = t4.tv_sec * 1000.0 + t4.tv_nsec / 1000000.0;
```
**Cosa misura**: Momento in cui il client riceve il risultato dal server.

## 📈 Metriche Derivate

### Request Latency (T2 - T1)
**Cosa misura**: Latenza di rete + overhead gRPC per inviare la richiesta.
**Include**:
- Serializzazione protobuf (client)
- Trasmissione TCP/HTTP2
- Deserializzazione protobuf (server)
- Overhead gRPC

**Valore atteso**:
- **Prima delle ottimizzazioni**: 20-100ms (creazione canale)
- **Dopo le ottimizzazioni**: 0.5-5ms (riuso canale)

### Task Execution Time (T3 - T2)
**Cosa misura**: Tempo effettivo di esecuzione del task.
**Include**:
- Parsing input JSON
- Creazione thread
- Setup RT scheduling
- Esecuzione task (`task_main()`)
- Join del thread

**Valore atteso**: Dipende dal task (tipicamente 1-100ms)

### Response Latency (T4 - T3)
**Cosa misura**: Latenza di rete + overhead gRPC per ricevere il risultato.
**Include**:
- Serializzazione protobuf (server)
- Trasmissione TCP/HTTP2
- Deserializzazione protobuf (client)
- Overhead gRPC

**Valore atteso**:
- **Prima delle ottimizzazioni**: 5-20ms
- **Dopo le ottimizzazioni**: 0.5-2ms (riuso canale)

### Total End-to-End (T4 - T1)
**Cosa misura**: Tempo totale dalla richiesta alla ricezione del risultato.
**Formula**: `(T2-T1) + (T3-T2) + (T4-T3)`

### Network Overhead (Request + Response Latency)
**Cosa misura**: Overhead totale di rete e gRPC.
**Formula**: `(T2-T1) + (T4-T3)`

**Valore atteso**:
- **Prima delle ottimizzazioni**: 25-120ms
- **Dopo le ottimizzazioni**: 1-7ms

## ✅ Correzioni Implementate

### 1. T2 Misurato Correttamente
**Prima** ❌:
```cpp
// Misurato DOPO pthread_create e setup
struct timespec t2;
clock_gettime(CLOCK_MONOTONIC, &t2);
```
Includeva tempo di creazione thread (~1-5ms)

**Dopo** ✅:
```cpp
// Misurato IMMEDIATAMENTE all'inizio della funzione
Status ExecuteTaskAsync(...) {
    struct timespec t2;
    clock_gettime(CLOCK_MONOTONIC, &t2);
    // ... resto del codice
}
```

### 2. T3 Misurato con Precisione
**Prima** ❌:
```cpp
// Polling ogni 100ms
struct timespec timeout = {0, 100000000};
int join_ret = pthread_timedjoin_np(task_thread, NULL, &timeout);
if (join_ret == 0) {
    clock_gettime(CLOCK_MONOTONIC, &t3);  // Impreciso!
}
```
Poteva aggiungere fino a 100ms di errore!

**Dopo** ✅:
```cpp
// Join bloccante - timing preciso
pthread_join(task_thread, NULL);
struct timespec t3;
clock_gettime(CLOCK_MONOTONIC, &t3);  // Preciso al nanosecondo!
```

### 3. Nomi Chiari nei Log
**Prima** ❌:
```
[gRPC Server Async] ⏱️ T1: Task received
```
Confondeva T1 client con timestamp server

**Dopo** ✅:
```
[gRPC Server Async] ⏱️ T1 (client)=X.XXX ms | T2 (server recv)=Y.YYY ms
```

## 🔬 Precisione delle Misurazioni

### Clock Source
Tutti i timestamp usano `CLOCK_MONOTONIC`:
- Non influenzato da cambiamenti di sistema (NTP, etc.)
- Monotonicamente crescente
- Risoluzione tipica: 1 nanosecondo
- Overhead di `clock_gettime()`: ~20-50ns

### Sincronizzazione Client-Server
**Importante**: T1 e T4 sono misurati sul client, T2 e T3 sul server.
- Usano lo stesso clock (`CLOCK_MONOTONIC`) ma su macchine diverse
- In Docker con host networking, i clock sono sincronizzati
- Le latenze (T2-T1) e (T4-T3) sono accurate

### Fonti di Errore

#### Minimizzate ✅
- Overhead `clock_gettime()`: ~20-50ns (trascurabile)
- Overhead `pthread_join()`: ~1-10µs (molto basso)
- Serializzazione protobuf: inclusa nelle latenze

#### Eliminate ✅
- ❌ Polling 100ms: rimosso, ora usa join bloccante
- ❌ Misurazione dopo processing: ora misurato immediatamente
- ❌ Creazione canale: ora riusa canali esistenti

## 📝 Output di Esempio

```
[EXECUTION MANAGER] ⏱️ T1=1234.567 ms | Sending gRPC request for task 1 'sum'
[gRPC Server Async] ⏱️ T1 (client)=1234.567 ms | T2 (server recv)=1235.123 ms | Request Latency=0.556 ms
[gRPC Server Async] ACK sent with T2=1235.123 ms
[TASK WRAPPER] ⏱️ T3=1237.890 ms | Task execution time: 2.767 ms (T3-T2)
[EXECUTION MANAGER] ⏱️ T3=1237.890 ms | Task completed (server measured)
[EXECUTION MANAGER] ⏱️ T4=1238.234 ms | Result received by client

╔═══════════════════════════════════════════════════════════════╗
║           END-TO-END TIMING MEASUREMENT RECAP                 ║
║                   Task 1: sum                                 
╠═══════════════════════════════════════════════════════════════╣
║ T1 (start_request)    =     1234.567 ms                        ║
║ T2 (end_request)      =     1235.123 ms                        ║
║ T3 (start_result)     =     1237.890 ms                        ║
║ T4 (end_result)       =     1238.234 ms                        ║
╠═══════════════════════════════════════════════════════════════╣
║ METRICS:                                                      ║
║   Request Latency    =        0.556 ms  (T2 - T1)            ║  ← gRPC send
║   Task Execution     =        2.767 ms  (T3 - T2)            ║  ← Task work
║   Response Latency   =        0.344 ms  (T4 - T3)            ║  ← gRPC recv
║   Total End-to-End   =        3.667 ms  (T4 - T1)            ║  ← Total
║   Network Overhead   =        0.900 ms  (Req + Resp)         ║  ← gRPC total
╚═══════════════════════════════════════════════════════════════╝
```

## 🎯 Interpretazione dei Risultati

### Request Latency (T2-T1)
- **< 1ms**: Eccellente (channel pooling attivo)
- **1-5ms**: Buono (riuso canale con qualche overhead)
- **> 10ms**: Problema (possibile creazione nuovo canale)

### Response Latency (T4-T3)
- **< 1ms**: Eccellente
- **1-2ms**: Buono
- **> 5ms**: Problema (possibile congestione rete)

### Network Overhead Totale
- **< 2ms**: Eccellente (ottimizzazioni attive)
- **2-7ms**: Buono
- **> 10ms**: Problema (verificare channel pooling)

### Task Execution (T3-T2)
Dipende dal task specifico. Confrontare con esecuzioni precedenti dello stesso task.
