# Configurable CPU/IO Task

## 📋 Overview

Task configurabile che permette di controllare il **bilanciamento tra operazioni CPU-intensive e I/O-intensive** tramite parametri di input.

## 🎯 Caratteristiche

- **CPU Operations**: Operazioni matematiche intensive (1M iterazioni per operazione)
- **I/O Operations**: Write + fdatasync su file temporaneo
- **Configurabile**: Parametri `total_ops` e `io_percentage`
- **Cancellabile**: Supporta `pthread_cancel` ogni 50 operazioni
- **Monitorabile**: Log dettagliato su core CPU utilizzato

## 📥 Input Parameters

```json
{
  "total_ops": 100,       // Numero totale di operazioni (default: 100)
  "io_percentage": 0      // Percentuale di I/O (0-100, default: 0)
}
```

### Esempi:

**Pure CPU** (100% CPU):
```json
{"total_ops": 100, "io_percentage": 0}
```

**Balanced** (50% CPU, 50% I/O):
```json
{"total_ops": 100, "io_percentage": 50}
```

**I/O Heavy** (20% CPU, 80% I/O):
```json
{"total_ops": 100, "io_percentage": 80}
```

**High Volume**:
```json
{"total_ops": 1000, "io_percentage": 30}
```
→ 700 CPU ops + 300 I/O ops

## 📤 Output

```json
{
  "result": 0    // 0 = success
}
```

## 🔧 Installazione

### 1. Attiva il Task

```bash
cd /home/vboxuser/projects/ORCHESTRATORE/RealTime-Microservices

# Backup del task corrente
cp task/sum/app_task.h task/sum/app_task_backup.h

# Attiva la versione configurabile
cp task/sum/app_task_configurable.h task/sum/app_task.h
```

### 2. Crea Manifest di Test

Crea `task/manifest-configurable.yaml`:

```yaml
version: "1.0"
schedule:
  name: "Configurable CPU/IO Benchmark"
  description: "Test task with varying CPU/IO ratios"
  
  tasks:
    - id: 1
      image: "sum"
      start: 1
      deadline: 20
      policy: "fifo"
      priority: 50
      depends_on: []
      inputs:
        total_ops:
          type: int
          value: 100
        io_percentage:
          type: int
          value: 0
      outputs:
        result:
          type: int
    
    - id: 2
      image: "sum"
      start: 10
      deadline: 30
      policy: "fifo"
      priority: 50
      depends_on: []
      inputs:
        total_ops:
          type: int
          value: 100
        io_percentage:
          type: int
          value: 25
      outputs:
        result:
          type: int
    
    - id: 3
      image: "sum"
      start: 20
      deadline: 40
      policy: "fifo"
      priority: 50
      depends_on: []
      inputs:
        total_ops:
          type: int
          value: 100
        io_percentage:
          type: int
          value: 50
      outputs:
        result:
          type: int
    
    - id: 4
      image: "sum"
      start: 30
      deadline: 50
      policy: "fifo"
      priority: 50
      depends_on: []
      inputs:
        total_ops:
          type: int
          value: 100
        io_percentage:
          type: int
          value: 75
      outputs:
        result:
          type: int
    
    - id: 5
      image: "sum"
      start: 40
      deadline: 60
      policy: "fifo"
      priority: 50
      depends_on: []
      inputs:
        total_ops:
          type: int
          value: 100
        io_percentage:
          type: int
          value: 100
      outputs:
        result:
          type: int
```

### 3. Build e Deploy

```bash
# Copia manifest
cp task/manifest-configurable.yaml task/manifest.yaml

# Build base image (se non già fatto)
docker build -t geodimi99/realtime-microservices:task-wrapper \
    -f services/task-wrapper/Dockerfile .

# Build task image
python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task

# Build services
docker compose build
```

### 4. Esegui Test

```bash
# Cleanup
docker compose down
docker stop task-service-sum 2>/dev/null || true
docker rm task-service-sum 2>/dev/null || true

# Start services
docker compose up -d redis deploy-manager
sleep 15

# Run test
docker compose run --rm execution-manager 2>&1 | tee /tmp/configurable_test.log
```

## 📊 Output Atteso

```
[THREAD] Core 2 | Executing: 100 CPU ops, 0 I/O ops (total 100, io_pct 0%)
[THREAD] Phase 1/2: CPU operations...
[THREAD] Phase 2/2: I/O operations...
[THREAD] ✅ Task completed successfully on core 2

🎉 [T=5234 ms] Task 1 'sum' COMPLETED
   [EXECUTION TIME] 3456789 µs (3456.789 ms)
```

Con `io_percentage: 50`:
```
[THREAD] Core 2 | Executing: 50 CPU ops, 50 I/O ops (total 100, io_pct 50%)
[THREAD] Phase 1/2: CPU operations...
[THREAD] Phase 2/2: I/O operations...
[THREAD] ✅ Task completed successfully on core 2

🎉 [T=8123 ms] Task 2 'sum' COMPLETED
   [EXECUTION TIME] 6789123 µs (6789.123 ms)
```

## ⚙️ Parametri Performance

### CPU Operations
- **Intensità**: 1M iterazioni floating point per operazione
- **Tempo stimato**: ~30-40 ms per operazione (dipende da CPU)
- **Scalabilità**: Lineare con `total_ops`

### I/O Operations
- **Operazione**: Write 1KB + fdatasync
- **Tempo stimato**: ~50-100 ms per operazione (dipende da storage)
- **File temporaneo**: `/tmp/rt_bench.bin` (eliminato a fine task)

### Esempi Timing

| total_ops | io_percentage | CPU ops | I/O ops | Tempo stimato |
|-----------|---------------|---------|---------|---------------|
| 100       | 0%            | 100     | 0       | ~3-4 sec      |
| 100       | 25%           | 75      | 25      | ~4-5 sec      |
| 100       | 50%           | 50      | 50      | ~5-7 sec      |
| 100       | 75%           | 25      | 75      | ~6-9 sec      |
| 100       | 100%          | 0       | 100     | ~8-12 sec     |
| 1000      | 50%           | 500     | 500     | ~50-70 sec    |

## 🔍 Differenze con Hybrid Task

| Aspetto | Configurable Task | Hybrid Task |
|---------|-------------------|-------------|
| **Parametri** | `total_ops`, `io_percentage` | `a`, `b`, `io_percentage` |
| **CPU Ops** | Floating point loop | Sqrt, trig, log, pow |
| **I/O Ops** | Write + fdatasync | Write + fsync + stat + memory |
| **Complessità** | Media (~30ms/op) | Molto alta (~60ms/op) |
| **Uso** | Benchmark flessibile | Stress test estremo |

## 🎓 Casi d'Uso

### Test 1: Scalabilità CPU
```yaml
# Task con solo CPU, volume variabile
total_ops: [10, 50, 100, 500, 1000]
io_percentage: 0
```

### Test 2: Scalabilità I/O
```yaml
# Task con solo I/O, volume variabile
total_ops: [10, 50, 100, 500]
io_percentage: 100
```

### Test 3: Ratio Sweep
```yaml
# Stesso volume, ratio variabile
total_ops: 100
io_percentage: [0, 10, 20, ..., 90, 100]
```

### Test 4: Concorrenza
```yaml
# 10 task paralleli con ratio diversi
# Task 1-10 start allo stesso secondo
io_percentage: distribuito 0-100
```

## 🐛 Troubleshooting

### "Failed to open I/O file"
Il container non ha permessi su `/tmp`. Modifica `IO_TMP_FILE` o monta volume.

### I/O molto lento
Storage lento o filesystem sync pesante. Considera SSD o RAM disk.

### Task cancellato
Timeout troppo stretto. Aumenta `deadline` nel manifest.

## 📝 Note Tecniche

- **Thread Safety**: Task usa solo risorse locali
- **Cleanup**: File temporaneo eliminato automaticamente
- **Cancellation Points**: Ogni 50 operazioni
- **JSON Parser**: Usa `jsmn` (già nel progetto)
- **No Dependencies**: Solo librerie standard C

## ✅ Vantaggi

1. **Semplice**: 2 parametri invece di 10+
2. **Flessibile**: Controlla esattamente CPU vs I/O
3. **Prevedibile**: Timing calcolabile
4. **Compatibile**: Usa infrastruttura esistente
5. **Leggero**: ~200 righe di codice

---

**Autore**: Real-Time Microservices Project  
**Versione**: 1.0  
**Data**: 2026-03-10
