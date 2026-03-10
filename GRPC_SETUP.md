# gRPC Implementation Guide

## Modifiche Apportate

### Nuovi File Creati:
1. **proto/task_service.proto** - Definizione servizio gRPC
2. **services/task-wrapper/src/grpc_server.cpp** - Server gRPC per task-wrapper
3. **services/execution-manager/src/grpc_client.cpp** - Client gRPC per execution-manager
4. **services/execution-manager/include/grpc_client.h** - Header client gRPC

### File Modificati:
1. **services/task-wrapper/Dockerfile** - Aggiunte dipendenze gRPC
2. **services/task-wrapper/CMakeLists.txt** - Supporto C++ e gRPC
3. **services/execution-manager/Dockerfile** - Aggiunte dipendenze gRPC
4. **services/execution-manager/CMakeLists.txt** - Supporto C++ e gRPC
5. **services/execution-manager/src/main.c** - Lettura Redis + chiamate gRPC

### Codice Mantenuto Intatto:
- ✅ services/task-wrapper/include/app_task.h (logica task)
- ✅ services/execution-manager/src/schedule.c (gestione schedule)
- ✅ common/ (tutto invariato)

---

## Comandi per Testare

### 1. Build Base Image Task-Wrapper
```bash
docker build -t geodimi99/realtime-microservices:task-wrapper \
    -f services/task-wrapper/Dockerfile .
```

### 2. Build Task Images (sum, subtract)
```bash
python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task
```

### 3. Build Execution-Manager
```bash
docker compose build execution-manager
```

### 4. Deploy Sistema (crea task-services)
```bash
docker compose up deploy-manager
```

### 5. Run Execution-Manager
```bash
docker compose up execution-manager
```

---

## Flusso Esecuzione

```
Execution-Manager
    │
    ├─> Legge schedule da Redis
    │
    ├─> Per ogni task:
    │   │
    │   ├─> gRPC call → task-service-sum:50051
    │   │                  │
    │   │                  ├─> Riceve TaskRequest
    │   │                  ├─> Esegue task_main() (C)
    │   │                  └─> Restituisce TaskResponse
    │   │
    │   └─> Riceve risultato e lo stampa
    │
    └─> Esegue test suite standalone
```

---

## Output Atteso

```
[Redis] Reading schedule from Redis...

=== REDIS DATA ===
Schedule: Demo Schedule (v0.0.1)
Number of tasks: 2

Tasks:
  1. Task: sum
     Policy: fifo, Priority: 50, Start: 3 ms
     Inputs: {"a": {"type": "int", "value": 10}, "b": {"type": "int", "value": 5}}

  2. Task: subtract
     Policy: fifo, Priority: 50, Start: 10 ms
     Inputs: {"a": {"type": "int", "value": 10}, "b": {"type": "int", "value": 5}}

==================

[gRPC] Executing tasks via gRPC...

[gRPC Client] Calling task 'sum' at task-service-sum:50051
[gRPC Client] Inputs: {"a": {"type": "int", "value": 10}, "b": {"type": "int", "value": 5}}
[gRPC Server] Received task: sum (ID: 1)
[gRPC Server] Task completed
[SUCCESS] Task 'sum' completed successfully
[RESULT] {"result": 2}

[gRPC Client] Calling task 'subtract' at task-service-subtract:50051
[SUCCESS] Task 'subtract' completed successfully
[RESULT] {"result": 5}

[gRPC] All tasks executed
```

---

## Troubleshooting

### Errore: "protoc: command not found"
```bash
# Reinstalla dependencies
docker compose build --no-cache execution-manager
```

### Errore: "gRPC connection failed"
```bash
# Verifica che i task-services siano attivi
docker ps | grep task-service

# Verifica network
docker network inspect realtime-microservices_default
```

### Task non ricevono chiamate
```bash
# Verifica logs task-wrapper
docker logs task-service-sum
```
