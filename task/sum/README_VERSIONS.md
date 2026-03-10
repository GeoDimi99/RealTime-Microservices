# Task Versions Summary

Questo progetto ha ora **3 versioni** del task `sum` con complessità crescente.

## 📁 File Disponibili

| File | Descrizione | Complessità | Tempo Esecuzione |
|------|-------------|-------------|------------------|
| `app_task.h` (corrente) | CPU intensivo estremo | ⭐⭐⭐⭐⭐⭐⭐ | 3-10 secondi |
| `app_task_hybrid.h` | CPU/IO configurabile | ⭐⭐⭐⭐⭐ (variabile) | 2-20 secondi |
| *originale git* | CPU semplice | ⭐ | ~10 millisecondi |

## 🔧 Versione Corrente: CPU Intensivo Estremo

**File**: `app_task.h` (modificato oggi)

### Caratteristiche
- **7 fasi** computazionali intensive
- **~62 milioni** di iterazioni totali
- Operazioni **sqrt ovunque** (nested, combined)
- Loop nested (Phase 5: 3M × 5)
- Matrici 4×4 (Phase 7: 2M × 16 ops)

### Operazioni Incluse
```c
- sqrt() multipli e nested
- sin(), cos(), tan(), atan2()
- log(), log10(), exp()
- pow(), cbrt(), fmod()
- Arithmetic intensive (divisioni, moltiplicazioni)
```

### Input
```json
{"a": 10, "b": 5}
```

### Output
```json
{"result": 15}
```

### Performance
- **Tempo**: 3-10 secondi (dipende da CPU)
- **Uso CPU**: 100% (single core)
- **Uso I/O**: 0%

---

## 🎛️ Versione Hybrid: CPU/IO Configurabile

**File**: `app_task_hybrid.h` (nuovo!)

### Caratteristiche
- **Parametro configurabile**: `io_percentage` (0-100)
- **Workload bilanciato**: CPU ↔ I/O
- **10 fasi** con mix CPU/IO
- **Metriche dettagliate** in output

### Input
```json
{
  "a": 10,
  "b": 5,
  "io_percentage": 50
}
```

### Output
```json
{
  "result": 15,
  "cpu_iterations": 5000000,
  "io_operations": 75
}
```

### Operazioni CPU (quando io_percentage < 100)
- Batch di operazioni matematiche intensive
- sqrt, sin, cos, tan
- Scaling automatico basato su percentage

### Operazioni I/O (quando io_percentage > 0)
- **File I/O**: Write + Read file temporanei (4KB × 10 blocchi)
- **fsync()**: Force flush to disk (molto lento!)
- **stat()**: Filesystem metadata operations
- **Memory I/O**: Allocazione/deallocazione 1MB blocks con touch

### Modalità Speciali

#### High CPU Mode (io_percentage ≤ 20)
- Extra CPU phase: 5 × 1M iterazioni
- Minimal I/O overhead

#### Balanced Mode (io_percentage = 40-60)
- 10 fasi mixed
- CPU e I/O alternati

#### High I/O Mode (io_percentage ≥ 80)
- Extra I/O phase: 3 × 10 operazioni
- Memory intensive operations

### Performance per io_percentage

| io_percentage | Tempo Atteso | CPU Iter | IO Ops | Uso Principale |
|---------------|--------------|----------|--------|----------------|
| 0             | 2-3s         | ~9M      | 0      | CPU 100% |
| 25            | 4-6s         | ~7M      | ~40    | CPU 75%, I/O 25% |
| 50            | 6-10s        | ~5M      | ~75    | Balanced |
| 75            | 10-15s       | ~2.5M    | ~110   | I/O 75%, CPU 25% |
| 100           | 15-20s       | 0        | ~150   | I/O 100% |

*Nota: Tempi su SSD. Su HDD possono raddoppiare per io_percentage ≥ 50.*

---

## 🔄 Come Switchare tra Versioni

### Attivare Versione CPU Estrema

```bash
# Se hai modificato app_task.h, è già attiva!
# Altrimenti:
git checkout task/sum/app_task.h

# Poi le modifiche di oggi per renderla estrema:
# (Le modifiche sono già nel file corrente)
```

### Attivare Versione Hybrid

```bash
# Backup corrente
cp task/sum/app_task.h task/sum/app_task_cpu_extreme_backup.h

# Switch to hybrid
cp task/sum/app_task_hybrid.h task/sum/app_task.h

# Rebuild necessario!
python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task
```

### Tornare a Versione Originale

```bash
git checkout task/sum/app_task.h
# Rebuild necessario
```

---

## 📊 Benchmark Disponibili

### 1. Benchmark CPU Puro

**Manifest**: `task/manifest-benchmark.yaml`

- 20 task identici
- Solo CPU intensive
- Test throughput e consistency

**Run**:
```bash
bash scripts/run_benchmark.sh
```

### 2. Benchmark Hybrid Scaling

**Manifest**: `task/manifest-hybrid-benchmark.yaml`

- 10 task con io_percentage crescente (0→100)
- Test scalabilità e performance vs I/O
- Include test concorrenza

**Setup**:
```bash
cp task/sum/app_task_hybrid.h task/sum/app_task.h
cp task/manifest-hybrid-benchmark.yaml task/manifest.yaml
# Build e run come normale benchmark
```

**Analisi**:
```bash
bash scripts/analyze_hybrid_benchmark.sh
```

---

## 💡 Casi d'Uso

### CPU Estrema (app_task.h corrente)
✅ Stress test CPU puro  
✅ Benchmark performance matematiche  
✅ Test thermal throttling  
✅ Comparazione CPU diverse  

### Hybrid (app_task_hybrid.h)
✅ Simulazione applicazioni reali  
✅ Test scheduler con I/O wait  
✅ Benchmark storage performance  
✅ Test fairness con workload misti  
✅ Ottimizzazione deadline in base a workload  

---

## 📝 Strutture Dati

### CPU Estrema

```c
typedef struct {
    int a;
    int b;
} input_t;

typedef struct {
    int result;
} output_t;
```

### Hybrid

```c
typedef struct {
    int a;
    int b;
    int io_percentage;  // 0-100
} input_hybrid_t;

typedef struct {
    int result;
    int cpu_iterations;
    int io_operations;
} output_hybrid_t;
```

---

## 🚀 Quick Reference

```bash
# Versione CPU Estrema
FILE: app_task.h (corrente)
INPUT: {"a": 10, "b": 5}
TEMPO: 3-10s
USO: Stress test CPU

# Versione Hybrid  
FILE: app_task_hybrid.h
INPUT: {"a": 10, "b": 5, "io_percentage": 50}
TEMPO: 2-20s (varia con io_percentage)
USO: Test realistici, I/O benchmarking

# Versione Originale
FILE: git history
INPUT: {"a": 10, "b": 5}
TEMPO: ~10ms
USO: Test rapidi, development
```

---

## 📚 Documentazione Completa

- **Guida Integrazione**: `HYBRID_INTEGRATION_GUIDE.md`
- **Dettagli Hybrid**: `task/sum/HYBRID_TASK_README.md`
- **Script Analisi**: `scripts/analyze_hybrid_benchmark.sh`
- **Manifest Examples**: `task/manifest-*.yaml`

---

**Creato**: Mar 10, 2026  
**Versione**: 1.0  
**Autore**: Sistema di benchmarking RealTime-Microservices
