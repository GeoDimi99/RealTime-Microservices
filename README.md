# RealTime Microservices

Sistema di scheduling real-time basato su microservizi Docker, con sincronizzazione inter-container via **ZeroMQ** (trasporto IPC su Unix domain socket).

---

## Architettura

```
┌─────────────────────────────────────────────────────────┐
│                        Host                             │
│                                                         │
│  ┌──────────────┐   ┌────────────────┐   ┌───────────┐ │
│  │ Image Builder │   │ Deploy Manager │   │   Redis   │ │
│  │   (SDK)       │──▶│  (Python)      │──▶│  (DB)     │ │
│  └──────────────┘   └───────┬────────┘   └─────▲─────┘ │
│                             │                  │        │
│               Lancia i container via Docker API │        │
│                             │                  │        │
│          ┌──────────────────┼──────────────┐   │        │
│          │                  │              │   │        │
│   ┌──────▼──────┐   ┌──────▼──────┐  ┌────▼───▼────┐   │
│   │  Worker 1   │   │  Worker 2   │  │   Leader    │   │
│   │ (task-wrap) │   │ (task-wrap) │  │ (task-wrap) │   │
│   └──────┬──────┘   └──────┬──────┘  └──────┬──────┘   │
│          │                 │                │           │
│          │    ZeroMQ IPC (ipc:///dev/shm/)   │           │
│          └────────────PUSH/PULL──────────────┘           │
│          └────────────PUB/SUB────────────────┘           │
└─────────────────────────────────────────────────────────┘
```

**Componenti principali:**

- **Image Builder (SDK)** — Genera le immagini Docker dei task a partire dal manifest
- **Deploy Manager** — Parsa il manifest, lancia i container, carica lo schedule su Redis
- **Task Wrapper (Execution Manager)** — Esegue i task real-time dentro ogni container, si sincronizza via ZeroMQ
- **Redis** — Database di configurazione per lo schedule

---

## Prerequisiti

- **Docker** >= 24.0
- **Docker Compose** >= 2.20
- **Python** >= 3.10 (solo per l'Image Builder SDK, eseguito sull'host)
- Un sistema Linux con supporto real-time (`CAP_SYS_NICE`, `CAP_IPC_LOCK`)

---

## Struttura del progetto

```
RealTime-Microservices/
├── common/                          # Librerie C condivise (task, IPC)
│   ├── include/
│   │   ├── task.h                   # Struttura task_t
│   │   └── task_ipc.h               # Protocollo IPC (ZeroMQ)
│   └── src/
│       └── task.c
├── services/
│   ├── deploy-manager/              # Servizio Python (orchestratore)
│   │   ├── Dockerfile
│   │   └── src/
│   └── task-wrapper/                # Servizio C (execution manager)
│       ├── Dockerfile
│       ├── CMakeLists.txt
│       ├── include/
│       └── src/
├── sdk/
│   └── image-builder/               # Tool per generare immagini Docker
├── tests/
│   └── test_0_code/                 # Esempio: manifest + sorgenti task
│       ├── manifest.yaml
│       ├── stress_task_1/
│       ├── stress_task_2/
│       ├── stress_task_3/
│       └── stress_task_4/
└── docker-compose.yml               # Avvia Redis + Deploy Manager
```

---

## Guida passo-passo

### 1. Clonare il repository

```bash
git clone <repository-url>
cd RealTime-Microservices
```

### 2. Creare il manifest

Il manifest definisce le immagini dei task e lo schedule. Un esempio è già presente in `tests/test_0_code/manifest.yaml`:

```yaml
version: "1.0"

images:
  base: "georgi99dimi/task-wrapper:latest"
  repo: "georgi99dimi/task-wrapper"
  tasks:
    - alias: "stress_task_1"
      src: "stress_task_1"
    - alias: "stress_task_2"
      src: "stress_task_2"

schedule:
  name: "Configurable CPU/IO Benchmark"
  description: "Benchmark with configurable CPU/IO ratio"
  iterations: 1
  tasks:
    - id: 1
      image: "stress_task_1"
      start: 1000          # ms dopo T-Zero
      deadline: 30000       # ms dopo T-Zero
      cpu_affinity: 1
      policy: "fifo"
      priority: 10
      depends_on: []
      inputs:
        total_ops: { type: int, value: 80 }
        io_percentage: { type: int, value: 10 }
      outputs:
        result: { type: int }
```

### 3. Costruire le immagini dei task (Image Builder SDK)

L'Image Builder genera un Dockerfile per ogni task e costruisce le immagini Docker localmente:

```bash
cd sdk/image-builder
python src/main.py -f ../../tests/test_0_code/manifest.yaml -c ../../tests/test_0_code/
```

Questo crea le immagini Docker `stress_task_1`, `stress_task_2`, ecc. nel registro locale.

### 4. Avviare i servizi base (Redis + Deploy Manager)

```bash
cd ../../  # Tornare alla root del progetto
docker compose up --build
```

Questo avvia:

1. **Redis** (porta 6379) — database di configurazione
2. **Deploy Manager** — che automaticamente:
   - Parsa il manifest da `/tmp/manifest.yaml` (copiato nel Dockerfile)
   - Lancia un container Docker per ogni task definito nello schedule
   - Carica lo schedule su Redis

### 5. Cosa succede automaticamente

Dopo `docker compose up`, il Deploy Manager:

1. **Parsa** `manifest.yaml` e crea l'oggetto Schedule
2. **Lancia** un container per ogni immagine (es. `stress_task_1`, `stress_task_2`, ...)
   - Ogni container esegue il **task-wrapper** (Execution Manager in C)
   - I container vengono avviati con `ipc_mode: host` per condividere `/dev/shm`
   - Capabilities `SYS_NICE` e `IPC_LOCK` abilitate per scheduling real-time
3. **Carica** lo schedule su Redis (chiave `schedule_data`)

I task-wrapper poi:

1. Si connettono a Redis e leggono lo schedule
2. Il **leader** (task con start time più basso) fa `bind` sui socket ZeroMQ
3. I **worker** fanno `connect` al leader
4. **Barriera di sincronizzazione**: i worker inviano `READY` (PUSH→PULL), il leader risponde con `SYNC` e il T-Zero (PUB→SUB)
5. Ogni container esegue i propri task secondo lo schedule con timer GLib

---

## Comunicazione ZeroMQ

La sincronizzazione tra container usa **ZeroMQ IPC** su Unix domain socket (`/dev/shm/`):

| Socket | Endpoint | Direzione |
|---|---|---|
| PULL | `ipc:///dev/shm/leader_pull.sock` | Worker → Leader (READY) |
| PUB | `ipc:///dev/shm/leader_pub.sock` | Leader → Workers (SYNC broadcast) |

I worker non espongono socket — fanno solo `connect` verso il leader.

---

## Comandi utili

```bash
# Avviare tutto
docker compose up --build

# Avviare in background
docker compose up --build -d

# Vedere i log di tutti i container (inclusi i task-wrapper)
docker compose logs -f
docker logs stress_task_1 -f
docker logs stress_task_2 -f

# Controllare lo schedule caricato su Redis
docker exec redis redis-cli HGETALL schedule_data

# Fermare tutto e rimuovere i container
docker compose down
docker rm -f stress_task_1 stress_task_2 stress_task_3 stress_task_4

# Ricostruire solo il task-wrapper
docker compose build deploy-manager

# Pulire le immagini Docker
docker image prune -f
```

---

## Configurazione Real-Time

Ogni container task-wrapper viene avviato con:

| Parametro | Valore | Scopo |
|---|---|---|
| `ipc_mode` | `host` | Condivisione `/dev/shm` per socket ZeroMQ IPC |
| `cap_add` | `SYS_NICE`, `IPC_LOCK` | Scheduling RT e memory locking |
| `ulimits.rtprio` | `99` | Priorità real-time massima |
| `ulimits.memlock` | `-1` (unlimited) | `mlockall()` senza limiti |

Il codice C usa:
- **`mlockall(MCL_CURRENT|MCL_FUTURE)`** — blocca la memoria in RAM
- **`SCHED_FIFO` / `SCHED_RR`** — politiche di scheduling real-time
- **`pthread_attr_setaffinity_np`** — binding dei thread a CPU specifiche

---

## Troubleshooting

| Problema | Soluzione |
|---|---|
| `zmq_bind failed` | Controllare che `/dev/shm` sia condiviso (`ipc_mode: host`) |
| `mlockall failed` | Verificare `cap_add: IPC_LOCK` e `ulimits.memlock: -1` |
| Worker non si connettono | Assicurarsi che il leader parta prima dei worker (è gestito dal Deploy Manager) |
| `schedule_data` non trovato in Redis | Verificare che il Deploy Manager sia partito correttamente: `docker logs deploy-manager` |
| Container task non trovato | Verificare che le immagini siano state costruite con l'Image Builder (Step 3) |

