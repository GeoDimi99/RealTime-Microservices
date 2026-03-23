# Sistema di Misurazione End-to-End con Recap Automatico

## Implementazione Completa

Il sistema ora traccia automaticamente tutti i 4 timestamp e mostra un **recap dettagliato** nell'Execution Manager al completamento di ogni task.

## Flusso dei Timestamp

```
EXECUTION MANAGER                    TASK WRAPPER
      │                                   │
      │ T1: Before sending gRPC           │
      ├──────────────────────────────────>│
      │         (Request Latency)         │
      │                                   │ T2: Thread started
      │                                   │ (Task Execution)
      │                                   │ T3: Task completed
      │<──────────────────────────────────┤
      │         (Response Latency)        │
      │ T4: After receiving result        │
      │                                   │
      └─> RECAP STAMPATO                  │
```

## Output Esempio

```
[EXECUTION MANAGER] ⏱️ T1=9352952578.107 ms | Sending gRPC request for task 1 'sum'
[TASK WRAPPER] ⏱️ T2=9352952607.599 ms | Thread started on core 1
[TASK WRAPPER] ⏱️ T3=9352955598.433 ms (Δ=2990.834 ms from T2) | ✅ Task completed
[EXECUTION MANAGER] ⏱️ T4=9352955598.456 ms | Received result via gRPC

╔═══════════════════════════════════════════════════════════════╗
║           END-TO-END TIMING MEASUREMENT RECAP                 ║
║                   Task 1: sum                                 
╠═══════════════════════════════════════════════════════════════╣
║ T1 (start_request)    = 9352952578.107 ms                     ║
║ T2 (end_request)      = 9352952607.599 ms                     ║
║ T3 (start_result)     = 9352955598.433 ms                     ║
║ T4 (end_result)       = 9352955598.456 ms                     ║
╠═══════════════════════════════════════════════════════════════╣
║ METRICS:                                                      ║
║   Request Latency    =       29.492 ms  (T2 - T1)            ║
║   Task Execution     =     2990.834 ms  (T3 - T2)            ║
║   Response Latency   =        0.023 ms  (T4 - T3)            ║
║   Total End-to-End   =     3020.349 ms  (T4 - T1)            ║
║   Network Overhead   =       29.515 ms  (Req + Resp)         ║
╚═══════════════════════════════════════════════════════════════╝
```

## Metriche Calcolate Automaticamente

### 1. Request Latency (T2 - T1)
Tempo impiegato dalla richiesta per:
- Attraversare la rete Docker
- Deserializzare il protobuf
- Creare il thread nel Task Wrapper

**Valore tipico**: 20-30 ms (Docker bridge networking)

### 2. Task Execution (T3 - T2)
Tempo puro di esecuzione del workload:
- CPU operations
- I/O operations
- Nessun overhead di rete

**Valore tipico**: Dipende dal workload (es. 3000 ms per 1000 ops)

### 3. Response Latency (T4 - T3)
Tempo impiegato dalla risposta per:
- Serializzare il risultato
- Attraversare la rete Docker
- Arrivare all'Execution Manager

**Valore tipico**: 0.02-0.1 ms (molto veloce, solo serializzazione)

### 4. Total End-to-End (T4 - T1)
Tempo totale dalla richiesta alla ricezione del risultato.

**Formula**: Request Latency + Task Execution + Response Latency

### 5. Network Overhead (T2-T1 + T4-T3)
Overhead totale di rete e serializzazione, escludendo l'esecuzione del task.

**Valore tipico**: 30-40 ms per Docker bridge

## Modifiche Implementate

### 1. Protobuf (`proto/task_service.proto`)
- Aggiunto `client_timestamp_ms` a `TaskRequest`
- Aggiunto `t2_thread_start_ms` e `t3_task_complete_ms` a `TaskResponse`

### 2. Task Wrapper (`task/sum/app_task.h`)
- Salvato T2 nel context all'inizio del thread
- Salvato T3 nel context dopo completamento task
- Passati T2 e T3 tramite gRPC response

### 3. Task Wrapper (`services/task-wrapper/src/grpc_server.cpp`)
- Estratto T2 e T3 dal context
- Inviati nella risposta gRPC (STARTED e COMPLETED)

### 4. Execution Manager (`services/execution-manager/src/realtime_scheduler.c`)
- Salvato T1 prima di inviare richiesta
- Ricevuto T2 nella risposta STARTED
- Ricevuto T3 nella risposta COMPLETED
- Catturato T4 dopo ricezione risultato
- **Stampato recap automatico con tutte le metriche**

### 5. gRPC Client (`services/execution-manager/src/grpc_client.cpp`)
- Estratto T2 e T3 dalle risposte
- Passati al callback

### 6. Callback Signature (`services/execution-manager/include/grpc_client.h`)
- Aggiunto parametri `t2_thread_start_ms` e `t3_task_complete_ms`

## Build e Test

```bash
cd /home/khadas/RT-grpc/RealTime-Microservices

# Rebuild proto (necessario per i nuovi campi)
sudo docker compose build proto-builder

# Rebuild services
sudo docker compose build execution-manager task-service-sum

# Run test
sudo docker compose up execution-manager
```

## Vantaggi

✅ **Automatico**: Il recap viene stampato automaticamente per ogni task  
✅ **Completo**: Mostra tutti i 4 timestamp e tutte le metriche derivate  
✅ **Leggibile**: Formato tabellare chiaro e ben formattato  
✅ **Preciso**: Usa `CLOCK_MONOTONIC` per timestamp consistenti  
✅ **Distribuito**: Funziona correttamente tra container diversi  

## Note Tecniche

- Tutti i timestamp sono in millisecondi con 3 decimali di precisione
- I timestamp sono assoluti (da boot del sistema) per permettere confronti diretti
- Il recap viene stampato solo per task completati con successo
- In caso di errore o timeout, il recap non viene mostrato
