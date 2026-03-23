# Sistema di Misurazione Tempi End-to-End

## Architettura

Il sistema traccia le performance di richieste tra due microservizi:
- **Execution Manager**: Invia richieste gRPC
- **Task Wrapper**: Elabora richieste in thread dedicati

## 4 Punti di Misurazione Temporale

Tutti i timestamp usano `clock_gettime(CLOCK_MONOTONIC)` per garantire coerenza.

### T1 - start_time_request
**Dove**: Execution Manager  
**Quando**: Immediatamente prima di inviare la richiesta gRPC  
**File**: `services/execution-manager/src/realtime_scheduler.c`  
**Funzione**: `grpc_task_thread()`  
**Output**: `[EXECUTION MANAGER] ⏱️ T1=<timestamp> ms | Sending gRPC request`

### T2 - end_time_request
**Dove**: Task Wrapper (thread dedicato)  
**Quando**: Prima istruzione significativa nel thread, prima di eseguire il task  
**File**: `task/sum/app_task.h`  
**Funzione**: `task_main()` (inizio)  
**Output**: `[TASK WRAPPER] ⏱️ T2=<timestamp> ms | Thread started on core X`

### T3 - start_time_result
**Dove**: Task Wrapper (stesso thread)  
**Quando**: Immediatamente dopo il completamento del task  
**File**: `task/sum/app_task.h`  
**Funzione**: `task_main()` (dopo workload)  
**Output**: `[TASK WRAPPER] ⏱️ T3=<timestamp> ms (Δ=<delta> ms from T2) | Task completed`

### T4 - end_time_result
**Dove**: Execution Manager  
**Quando**: Immediatamente dopo aver ricevuto il risultato tramite gRPC  
**File**: `services/execution-manager/src/realtime_scheduler.c`  
**Funzione**: `scheduler_task_callback()` (status="COMPLETED")  
**Output**: `[EXECUTION MANAGER] ⏱️ T4=<timestamp> ms | Received result via gRPC`

## Metriche Calcolabili

### Latenza di Rete/gRPC (Request)
```
Latency_Request = T2 - T1
```
Tempo impiegato dalla richiesta per attraversare la rete e creare il thread.

### Tempo di Esecuzione Task
```
Execution_Time = T3 - T2
```
Tempo puro di esecuzione del workload (CPU + I/O).

### Latenza di Rete/gRPC (Response)
```
Latency_Response = T4 - T3
```
Tempo impiegato dalla risposta per tornare all'Execution Manager.

### Tempo Totale End-to-End
```
Total_Time = T4 - T1
```
Tempo totale dalla richiesta alla ricezione del risultato.

### Overhead di Sistema
```
Overhead = Total_Time - Execution_Time = (T2-T1) + (T4-T3)
```
Overhead totale di rete, serializzazione, thread creation, ecc.

## Esempio di Output

```
[SCHEDULER] ⏰ T=10001 ms: Launching task 1 'sum' (deadline: 30000 ms)
[EXECUTION MANAGER] ⏱️ T1=9352952578.107 ms | Sending gRPC request for task 1 'sum'
[TASK WRAPPER] ⏱️ T2=9352952607.599 ms | Thread started on core 1 | Executing: 500 CPU ops, 500 I/O ops
[THREAD] Phase 1/2: CPU operations...
[THREAD] Phase 2/2: I/O operations...
[TASK WRAPPER] ⏱️ T3=9352955598.433 ms (Δ=2990.834 ms from T2) | ✅ Task completed on core 1
[EXECUTION MANAGER] ⏱️ T4=9352955598.456 ms | Received result via gRPC
```

### Analisi:
- **Latency_Request** = 9352952607.599 - 9352952578.107 = **29.492 ms**
- **Execution_Time** = 9352955598.433 - 9352952607.599 = **2990.834 ms**
- **Latency_Response** = 9352955598.456 - 9352955598.433 = **0.023 ms**
- **Total_Time** = 9352955598.456 - 9352952578.107 = **3020.349 ms**
- **Overhead** = 29.492 + 0.023 = **29.515 ms**

## Build e Test

Per applicare le modifiche:

```bash
cd /home/khadas/RT-grpc/RealTime-Microservices

# Rebuild services
sudo docker compose build execution-manager task-service-sum

# Run test
sudo docker compose up execution-manager
```

## Note Tecniche

- Tutti i timestamp sono in millisecondi con precisione di 3 decimali
- `CLOCK_MONOTONIC` garantisce che i timestamp non siano influenzati da aggiustamenti dell'orologio di sistema
- I timestamp assoluti permettono il confronto diretto tra container diversi
- La latenza di ~30ms per la richiesta è tipica per Docker bridge networking
