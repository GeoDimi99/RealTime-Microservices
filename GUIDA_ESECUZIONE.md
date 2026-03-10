# Guida all'Esecuzione - RealTime Microservices

Questa guida contiene tutti i comandi necessari per eseguire il sistema di microservizi real-time.

---

## 📋 Prerequisiti

- Docker e Docker Compose installati
- Python 3.x installato
- Accesso al socket Docker (`/var/run/docker.sock`)

---

## 🚀 Esecuzione Completa del Sistema

### Metodo 1: Esecuzione Manuale Step-by-Step

#### 1. Build dell'immagine base Task-Wrapper
```bash
docker build -t geodimi99/realtime-microservices:task-wrapper \
    -f services/task-wrapper/Dockerfile .
```

#### 2. Build delle immagini dei task (sum, subtract)
```bash
python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task
```

**Build senza cache** (forza una build pulita):
```bash
python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task --no-cache
```

**Nota**: Il push automatico su DockerHub è stato disabilitato. Le immagini vengono solo costruite localmente.

#### 3. Build dell'Execution-Manager
```bash
docker compose build execution-manager
```

#### 4. Build del Deploy-Manager
```bash
docker compose build deploy-manager
```

#### 5. Avvio di Redis
```bash
docker compose up -d redis
```

#### 6. Deploy del sistema (crea i task-services)
```bash
docker compose up deploy-manager
```
Questo comando legge il manifest, crea i task-services e li salva in Redis.

#### 7. Esecuzione dell'Execution-Manager
```bash
docker compose up execution-manager
```
Questo comando esegue i task tramite chiamate gRPC ai task-services.

---

### Metodo 2: Esecuzione Automatica con Benchmark

Per eseguire un benchmark completo con 20 task:

```bash
bash scripts/run_benchmark.sh
```

Questo script esegue automaticamente:
1. Build dell'immagine base task-wrapper
2. Backup e sostituzione del manifest con quello di benchmark
3. Build delle immagini dei task
4. Build dei servizi (deploy-manager, execution-manager)
5. Pulizia e avvio dei servizi
6. Esecuzione del benchmark (20 task, 1 container)
7. Analisi dei risultati
8. Ripristino del manifest originale

---

## 🔍 Comandi di Verifica e Debug

### Verificare i container attivi
```bash
docker ps
```

### Verificare che i task-services siano in esecuzione
```bash
docker ps | grep task-service
```

### Vedere i logs di un servizio
```bash
# Logs del deploy-manager
docker logs deploy-manager

# Logs dell'execution-manager
docker logs execution-manager

# Logs di Redis
docker logs redis

# Logs di un task-service specifico (es. sum)
docker logs task-service-sum
```

### Vedere i logs in tempo reale
```bash
# Deploy-manager
docker compose logs -f deploy-manager

# Execution-manager
docker compose logs -f execution-manager
```

### Verificare la rete Docker
```bash
docker network inspect realtime-microservices_default
```

### Verificare i dati in Redis
```bash
# Accedere al container Redis
docker exec -it redis redis-cli

# All'interno di Redis
# Vedere tutte le chiavi
KEYS *

# Vedere lo schedule
GET schedule

# Vedere un task specifico
GET task:1
```

---

## 🧹 Comandi di Pulizia

### Fermare tutti i container
```bash
docker compose down
```

### Fermare e rimuovere un task-service specifico
```bash
docker stop task-service-sum
docker rm task-service-sum
```

### Rimuovere tutti i container, volumi e immagini
```bash
# Fermare tutto
docker compose down

# Rimuovere volumi
docker compose down -v

# Rimuovere anche le immagini
docker compose down --rmi all
```

### Pulizia completa del sistema Docker (ATTENZIONE: rimuove tutto)
```bash
# Rimuove tutti i container fermi
docker container prune -f

# Rimuove tutte le immagini non utilizzate
docker image prune -a -f

# Rimuove tutti i volumi non utilizzati
docker volume prune -f

# Rimuove tutte le reti non utilizzate
docker network prune -f
```

---

## 🔧 Comandi di Build

### Rebuild completo (senza cache)
```bash
# Build singolo servizio senza cache
docker compose build --no-cache execution-manager
docker compose build --no-cache deploy-manager

# Build di tutti i servizi senza cache
docker compose build --no-cache
```

### Build dell'immagine base senza cache
```bash
docker build --no-cache -t geodimi99/realtime-microservices:task-wrapper \
    -f services/task-wrapper/Dockerfile .
```

### Build delle task images senza cache
```bash
python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task --no-cache
```

---

## ⏱️ Misurazione Tempi di Build

### Metodo 1: Usando il comando `time`

Per misurare rapidamente il tempo di esecuzione:

```bash
# Build con cache
time python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task

# Build senza cache
time python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task --no-cache
```

Output tipico:
```
real    0m0.569s    # Tempo totale dall'inizio alla fine
user    0m0.402s    # Tempo CPU in user space
sys     0m0.070s    # Tempo CPU in kernel space
```

### Metodo 2: Script dedicato con logging

Per misurazioni più dettagliate con salvataggio automatico:

```bash
# Build con cache
bash scripts/measure_build_time.sh

# Build senza cache
bash scripts/measure_build_time.sh --no-cache
```

Lo script:
- ✅ Mostra il tempo totale in secondi
- ✅ Converte automaticamente in minuti:secondi se > 60s
- ✅ Salva i risultati in `build_times.log`

### Vedere lo storico delle misurazioni
```bash
cat build_times.log
```

Esempio di output:
```
2026-03-09 22:45:12 | with cache | 0.569s
2026-03-09 22:47:30 | WITHOUT cache | 45.823s
```

---

## 🚢 Push Manuale su DockerHub (Opzionale)

Il push automatico su DockerHub è stato disabilitato per evitare push accidentali. Se vuoi pubblicare le immagini manualmente:

### Push dell'immagine base
```bash
# Login su DockerHub (se necessario)
docker login

# Tag dell'immagine
docker tag geodimi99/realtime-microservices:task-wrapper geodimi99/realtime-microservices:task-wrapper

# Push dell'immagine
docker push geodimi99/realtime-microservices:task-wrapper
```

### Push di un'immagine task specifica
```bash
# Tag dell'immagine locale con il repository remoto
docker tag sum geodimi99/realtime-microservices:sum

# Push dell'immagine
docker push geodimi99/realtime-microservices:sum
```

### Push di tutte le immagini task
```bash
# Per ogni task (sum, subtract, etc.)
for task in sum subtract; do
    docker tag $task geodimi99/realtime-microservices:$task
    docker push geodimi99/realtime-microservices:$task
done
```

---

## 📊 Comandi di Analisi e Benchmark

### Eseguire solo l'analisi dei risultati (dopo un benchmark)
```bash
bash scripts/analyze_benchmark.sh
```

### Vedere i risultati del benchmark
```bash
cat /tmp/benchmark_execution.log
```

---

## 🛠️ Comandi Utili per lo Sviluppo

### Accedere a un container in esecuzione
```bash
# Accedere a Redis
docker exec -it redis bash

# Accedere a deploy-manager
docker exec -it deploy-manager bash

# Accedere a execution-manager
docker exec -it execution-manager bash
```

### Ispezionare un'immagine Docker
```bash
docker inspect geodimi99/realtime-microservices:task-wrapper
docker inspect geodimi99/realtime-microservices:sum
```

### Vedere lo spazio utilizzato da Docker
```bash
docker system df
```

### Eseguire un singolo servizio
```bash
# Eseguire solo il deploy-manager
docker compose run --rm deploy-manager

# Eseguire solo l'execution-manager
docker compose run --rm execution-manager
```

---

## 🔄 Workflow Completo Consigliato

Per un'esecuzione completa del sistema, segui questi passaggi:

```bash
# 1. Pulizia iniziale
docker compose down
docker stop task-service-sum 2>/dev/null || true
docker rm task-service-sum 2>/dev/null || true

# 2. Build immagine base
docker build -t geodimi99/realtime-microservices:task-wrapper \
    -f services/task-wrapper/Dockerfile .

# 3. Build task images
python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task

# 4. Build servizi
docker compose build

# 5. Avvio Redis
docker compose up -d redis

# 6. Deploy (salva schedule in Redis)
docker compose up deploy-manager

# 7. Esecuzione (esegue i task)
docker compose up execution-manager

# 8. Vedere i logs
docker compose logs
```

---

## 📝 Note Importanti

1. **Ordine di esecuzione**: È importante seguire l'ordine corretto:
   - Prima build dell'immagine base
   - Poi build delle task images
   - Poi build dei servizi
   - Infine avvio in ordine: Redis → Deploy-Manager → Execution-Manager

2. **Deploy-Manager**: Deve essere eseguito prima di Execution-Manager perché popola Redis con lo schedule e i task.

3. **Task-Services**: Vengono creati automaticamente dal Deploy-Manager basandosi sul manifest.

4. **gRPC**: La comunicazione tra Execution-Manager e Task-Services avviene tramite gRPC sulla porta 50051.

5. **Real-Time Scheduling**: L'Execution-Manager supporta scheduling real-time (SCHED_FIFO) grazie alla capability `SYS_NICE`.

6. **Push su DockerHub**: Il push automatico su DockerHub è stato **disabilitato** per evitare pubblicazioni accidentali. Le immagini vengono costruite solo localmente. Se hai bisogno di pubblicare le immagini, consulta la sezione "Push Manuale su DockerHub".

---

## ❗ Troubleshooting

### Errore: "protoc: command not found"
```bash
docker compose build --no-cache execution-manager
```

### Errore: "gRPC connection failed"
```bash
# Verifica che i task-services siano attivi
docker ps | grep task-service

# Verifica network
docker network inspect realtime-microservices_default
```

### I task non ricevono chiamate
```bash
# Verifica logs task-wrapper
docker logs task-service-sum
docker logs task-service-subtract
```

### Redis non si avvia
```bash
# Verifica i logs
docker logs redis

# Riavvia Redis
docker compose restart redis
```

### Porta già in uso
```bash
# Trova quale processo usa la porta 6379 (Redis)
sudo lsof -i :6379

# Oppure
sudo netstat -tulpn | grep 6379
```

---

## 📂 Struttura File Principali

- `task/manifest.yaml` - Definizione dei task e dello schedule
- `task/manifest-benchmark.yaml` - Manifest per il benchmark (20 task)
- `docker-compose.yml` - Configurazione dei servizi
- `proto/` - Definizioni protobuf per gRPC
- `services/deploy-manager/` - Servizio che crea i task-services
- `services/execution-manager/` - Servizio che esegue i task
- `services/task-wrapper/` - Template per i task-services
- `scripts/run_benchmark.sh` - Script per eseguire il benchmark
- `scripts/analyze_benchmark.sh` - Script per analizzare i risultati
- `sdk/image-builder/` - Tool per creare le immagini dei task

---

## 🎯 Output Atteso

Quando esegui correttamente il sistema, dovresti vedere:

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
[SUCCESS] Task 'sum' completed successfully
[RESULT] {"result": 15}

[gRPC Client] Calling task 'subtract' at task-service-subtract:50051
[SUCCESS] Task 'subtract' completed successfully
[RESULT] {"result": 5}

[gRPC] All tasks executed
```

---

Questa guida dovrebbe coprire tutti i comandi necessari per lavorare con il sistema RealTime Microservices! 🚀
