# Guida Integrazione Task Hybrid CPU/IO

## 📋 Panoramica

Hai ora **3 versioni** del task disponibili:

| File | Tipo | Tempo Esecuzione | Uso |
|------|------|------------------|-----|
| `app_task.h` (originale) | CPU semplice | ~10ms | Task lightweight |
| `app_task.h` (modificato) | CPU intensivo | 3-10s | Stress test CPU puro |
| `app_task_hybrid.h` | CPU/IO configurabile | 2-20s | Benchmark realistici |

## 🚀 Quick Start - Test Hybrid Task

### Passo 1: Backup e Switch

```bash
# Backup versione CPU-intensive corrente
cp task/sum/app_task.h task/sum/app_task_cpu_extreme.h

# Attiva versione hybrid
cp task/sum/app_task_hybrid.h task/sum/app_task.h
```

### Passo 2: Build e Deploy

```bash
# Build base image
docker build -t geodimi99/realtime-microservices:task-wrapper \
    -f services/task-wrapper/Dockerfile .

# Build task con hybrid
cp task/manifest-hybrid-benchmark.yaml task/manifest.yaml
python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task

# Build services
docker compose build deploy-manager execution-manager
```

### Passo 3: Run Benchmark Hybrid

```bash
# Cleanup e start
docker compose down
docker stop task-service-sum 2>/dev/null || true
docker rm task-service-sum 2>/dev/null || true
docker compose up -d redis deploy-manager

# Wait for ready
sleep 15

# Execute benchmark
docker compose run --rm execution-manager
```

## 📊 Risultati Attesi

### Output per ogni task

```
✅ [T=1234 ms] Task 1 'sum' COMPLETED
   [EXECUTION TIME] 3456789 µs (3456.789 ms)
   [RESULT] {"result": 15, "cpu_iterations": 9000000, "io_operations": 0}

✅ [T=5678 ms] Task 4 'sum' COMPLETED  
   [EXECUTION TIME] 8234567 µs (8234.567 ms)
   [RESULT] {"result": 15, "cpu_iterations": 5000000, "io_operations": 75}
```

### Analisi Performance

| Task ID | IO% | CPU Iter. | IO Ops | Tempo (ms) |
|---------|-----|-----------|--------|------------|
| 1       | 0   | ~9M       | 0      | ~3000      |
| 2       | 10  | ~8M       | ~15    | ~3500      |
| 3       | 25  | ~7.5M     | ~37    | ~5000      |
| 4       | 50  | ~5M       | ~75    | ~8000      |
| 5       | 75  | ~2.5M     | ~112   | ~12000     |
| 6       | 90  | ~1M       | ~135   | ~15000     |
| 7       | 100 | 0         | ~150   | ~18000     |

## 🔧 Configurazioni Avanzate

### Scenario 1: Web Application Simulation

Crea `task/manifest-webapp-sim.yaml`:

```yaml
schedule:
  name: "Web Application Workload"
  tasks:
    # API Endpoint (CPU + DB access)
    - id: 1
      image: "sum"
      start: 0
      deadline: 5
      priority: 100
      inputs:
        a: {type: int, value: 10}
        b: {type: int, value: 5}
        io_percentage: {type: int, value: 35}
    
    # Background Job (Heavy CPU)
    - id: 2
      image: "sum"
      start: 0
      deadline: 30
      priority: 50
      inputs:
        a: {type: int, value: 100}
        b: {type: int, value: 50}
        io_percentage: {type: int, value: 10}
    
    # Log Writer (High IO)
    - id: 3
      image: "sum"
      start: 1
      deadline: 10
      priority: 75
      inputs:
        a: {type: int, value: 5}
        b: {type: int, value: 2}
        io_percentage: {type: int, value: 80}
```

### Scenario 2: Data Pipeline

```yaml
schedule:
  name: "ETL Pipeline Simulation"
  tasks:
    # Extract (Read from disk)
    - id: 1
      image: "sum"
      start: 0
      deadline: 20
      inputs:
        io_percentage: {type: int, value: 70}
    
    # Transform (CPU heavy)
    - id: 2
      image: "sum"
      start: 5
      deadline: 30
      depends_on: [1]
      inputs:
        io_percentage: {type: int, value: 15}
    
    # Load (Write to disk)
    - id: 3
      image: "sum"
      start: 10
      deadline: 40
      depends_on: [2]
      inputs:
        io_percentage: {type: int, value: 85}
```

### Scenario 3: Concurrent Mixed Workload

```yaml
schedule:
  name: "Concurrent Mixed Stress Test"
  tasks:
    # 5 CPU-heavy tasks (parallel)
    - id: 1
      image: "sum"
      start: 0
      deadline: 15
      inputs: {io_percentage: {type: int, value: 5}}
    
    - id: 2
      image: "sum"
      start: 0
      deadline: 15
      inputs: {io_percentage: {type: int, value: 5}}
    
    # 3 IO-heavy tasks (parallel with CPU tasks)
    - id: 3
      image: "sum"
      start: 0
      deadline: 25
      inputs: {io_percentage: {type: int, value: 80}}
    
    - id: 4
      image: "sum"
      start: 0
      deadline: 25
      inputs: {io_percentage: {type: int, value: 75}}
    
    # 2 Balanced tasks
    - id: 5
      image: "sum"
      start: 1
      deadline: 20
      inputs: {io_percentage: {type: int, value: 50}}
```

## 📈 Analisi Risultati con Script

Crea `scripts/analyze_hybrid_benchmark.sh`:

```bash
#!/bin/bash

echo "=== HYBRID BENCHMARK ANALYSIS ==="
echo ""

# Extract execution times per IO percentage
for io_pct in 0 10 25 50 75 90 100; do
    echo "IO Percentage: $io_pct%"
    
    grep "io_percentage.*value: $io_pct" task/manifest.yaml -A 30 | \
    grep "\[EXECUTION TIME\]" | \
    awk '{print "  Time: " $3 " µs"}'
    
    echo ""
done

# Calculate average time vs IO%
echo "=== TIME vs IO% CORRELATION ==="
grep "\[EXECUTION TIME\]" /tmp/benchmark_execution.log | \
awk '{print $3}' | \
awk '{sum+=$1; count++; print "Task " count ": " $1 " µs"} 
     END {print "Average: " sum/count " µs"}'
```

## 🎯 Metriche da Monitorare

### Durante l'esecuzione

```bash
# Terminal 1: CPU usage
top -b -d 1 | grep task-service-sum

# Terminal 2: I/O activity
iostat -x 1 | grep -E "(sda|nvme)"

# Terminal 3: Disk I/O operations
watch -n 1 'ls -lh /tmp/benchmark_io_*.tmp 2>/dev/null | wc -l'

# Terminal 4: Memory usage
watch -n 1 'free -h'
```

### Post-esecuzione

```bash
# Verifica tempi esecuzione
docker logs execution-manager 2>&1 | grep "EXECUTION TIME"

# Analisi risultati
docker logs execution-manager 2>&1 | grep -E "(cpu_iterations|io_operations)"

# Timeline completo
docker logs execution-manager 2>&1 | grep "\[T="
```

## 🔄 Switching tra Versioni

### Versione CPU Estrema (per stress test CPU puro)

```bash
cp task/sum/app_task_cpu_extreme.h task/sum/app_task.h
# Rebuild necessario
```

### Versione Hybrid (per test realistici)

```bash
cp task/sum/app_task_hybrid.h task/sum/app_task.h
# Rebuild necessario
```

### Versione Originale (per test rapidi)

```bash
git checkout task/sum/app_task.h
# Rebuild necessario
```

## 🧪 Testing Progressivo

### Step 1: Test Funzionale

```bash
# Test singolo con io_percentage=0
cp task/manifest-benchmark.yaml task/manifest.yaml
# Edit: ridurre a 1 solo task con io_percentage=0
docker compose run --rm execution-manager
```

### Step 2: Test Scaling IO

```bash
# Usa manifest-hybrid-benchmark.yaml
cp task/manifest-hybrid-benchmark.yaml task/manifest.yaml
docker compose run --rm execution-manager
```

### Step 3: Test Concurrent

```bash
# Test con task paralleli mixed workload
# Modifica manifest con start=0 per multipli task
docker compose run --rm execution-manager
```

### Step 4: Stress Test

```bash
# 20+ task con mixed IO%
# Aumenta deadline per evitare timeout
docker compose run --rm execution-manager
```

## 📝 Tips & Best Practices

### Performance Optimization

1. **Usa SSD**: I test I/O sono 10x+ più veloci su SSD
2. **tmpfs per test veloci**:
   ```bash
   sudo mount -t tmpfs -o size=2G tmpfs /tmp
   ```
3. **Aumenta file descriptors**:
   ```bash
   ulimit -n 8192
   ```

### Deadline Configuration

Regola le deadline in base all'IO%:

```python
# Formula suggerita
deadline_seconds = 10 + (io_percentage * 0.5)

# Esempi:
# io_percentage=0  → deadline=10s
# io_percentage=50 → deadline=35s
# io_percentage=100→ deadline=60s
```

### Container Resources

Se i task sono troppo lenti, aumenta le risorse:

```yaml
# docker-compose.yml
services:
  execution-manager:
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 4G
```

## 🐛 Troubleshooting

### Task timeout troppo frequenti

**Problema**: Task non completano entro deadline

**Soluzione**:
```yaml
# Aumenta deadline nel manifest
deadline: 60  # Invece di 30
```

### Disco saturo

**Problema**: I/O operations molto lente

**Soluzione**:
```bash
# Check disk usage
df -h /tmp

# Cleanup
rm -f /tmp/benchmark_io_*.tmp

# Monitor I/O
iostat -x 1
```

### Out of Memory

**Problema**: Container crashed per OOM

**Soluzione**:
```bash
# Riduci io_percentage sotto 70
# Oppure aumenta memoria container
docker update --memory="2g" task-service-sum
```

### File descriptor limit

**Problema**: "Too many open files"

**Soluzione**:
```bash
# Aumenta limit
ulimit -n 4096

# Verifica
ulimit -n
```

## 🎓 Conclusioni

Hai ora un sistema completo per:

✅ **Testare performance** con workload realistici  
✅ **Benchmark scalabilità** CPU→IO  
✅ **Stress test** scheduler real-time  
✅ **Simulare** applicazioni reali  
✅ **Analizzare** impatto I/O su latency  

Per domande o miglioramenti, vedi `task/sum/HYBRID_TASK_README.md`
