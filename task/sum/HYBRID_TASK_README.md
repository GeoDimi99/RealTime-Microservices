# Hybrid Task - CPU/IO Configurable Workload

## Overview

Il file `app_task_hybrid.h` implementa un task configurabile che permette di bilanciare tra operazioni CPU-intensive e IO-intensive, ideale per benchmark realistici e stress testing dello scheduler.

## Parametri di Input

```json
{
  "a": 10,
  "b": 5,
  "io_percentage": 50
}
```

### `io_percentage` (0-100)

Controlla il bilanciamento del workload:

| Valore | CPU | IO  | Tipo Workload |
|--------|-----|-----|---------------|
| 0      | 100%| 0%  | Pure CPU (calcoli matematici intensivi) |
| 10     | 90% | 10% | Mostly CPU con alcune operazioni I/O |
| 25     | 75% | 25% | CPU-dominant con I/O significativo |
| 50     | 50% | 50% | Bilanciato CPU/IO |
| 75     | 25% | 75% | IO-dominant con calcoli CPU |
| 90     | 10% | 90% | Mostly IO con pochi calcoli |
| 100    | 0%  | 100%| Pure IO (file operations, sync, memory) |

## Operazioni Implementate

### CPU-Intensive Operations
- Operazioni aritmetiche con sqrt, divisioni
- Calcoli trigonometrici: sin, cos, tan
- Operazioni su floating-point ad alta precisione
- Loop nested con operazioni matematiche complesse

### IO-Intensive Operations
- **File I/O**: Lettura/scrittura file temporanei in `/tmp`
- **Fsync**: Forzatura flush su disco (molto lento)
- **Stat operations**: Metadata filesystem
- **Memory allocation**: Allocazione/deallocazione MB di memoria
- **Page faults**: Touch memory per forzare operazioni kernel

### Memory-Intensive Operations (Hybrid)
- Allocazione blocchi 1MB
- Memory touch (force page faults)
- Potenziale swapping se memoria insufficiente

## Output

```json
{
  "result": 12345,
  "cpu_iterations": 8500000,
  "io_operations": 150
}
```

- **`result`**: Risultato computazione (a + b + hash)
- **`cpu_iterations`**: Numero iterazioni CPU eseguite
- **`io_operations`**: Numero operazioni I/O completate

## Esempi di Utilizzo

### Test 1: Pure CPU Benchmark (io_percentage=0)
```json
{"a": 10, "b": 5, "io_percentage": 0}
```
**Tempo atteso**: 2-5 secondi  
**Uso**: Benchmark performance CPU pura

### Test 2: Balanced Workload (io_percentage=50)
```json
{"a": 10, "b": 5, "io_percentage": 50}
```
**Tempo atteso**: 3-8 secondi  
**Uso**: Simulazione applicazione real-world

### Test 3: High IO Stress (io_percentage=90)
```json
{"a": 10, "b": 5, "io_percentage": 90}
```
**Tempo atteso**: 5-15 secondi  
**Uso**: Stress test I/O scheduler, disk performance

### Test 4: IO Saturation (io_percentage=100)
```json
{"a": 10, "b": 5, "io_percentage": 100}
```
**Tempo atteso**: 8-20 secondi  
**Uso**: Test pure I/O latency e throughput

## Integrazione con il Sistema

### Opzione 1: Sostituire app_task.h
```bash
cp task/sum/app_task_hybrid.h task/sum/app_task.h
```

### Opzione 2: Task Separato
Creare nuova directory:
```bash
cp -r task/sum task/sum-hybrid
cp task/sum/app_task_hybrid.h task/sum-hybrid/app_task.h
```

Aggiungere al manifest:
```yaml
tasks:
  - alias: "sum-hybrid"
    src: "sum-hybrid"
```

## Manifest per Benchmark Hybrid

### Esempio: Test Scalabilità CPU→IO

```yaml
schedule:
  name: "CPU to IO Scaling Test"
  tasks:
    - id: 1
      image: "sum-hybrid"
      inputs:
        a: {type: int, value: 10}
        b: {type: int, value: 5}
        io_percentage: {type: int, value: 0}
    
    - id: 2
      image: "sum-hybrid"
      inputs:
        a: {type: int, value: 10}
        b: {type: int, value: 5}
        io_percentage: {type: int, value: 25}
    
    - id: 3
      image: "sum-hybrid"
      inputs:
        a: {type: int, value: 10}
        b: {type: int, value: 5}
        io_percentage: {type: int, value: 50}
    
    - id: 4
      image: "sum-hybrid"
      inputs:
        a: {type: int, value: 10}
        b: {type: int, value: 5}
        io_percentage: {type: int, value: 75}
    
    - id: 5
      image: "sum-hybrid"
      inputs:
        a: {type: int, value: 10}
        b: {type: int, value: 5}
        io_percentage: {type: int, value: 100}
```

## Metriche di Performance

Lo scheduler mostrerà:
- **Execution time**: Tempo totale esecuzione
- **CPU iterations**: Quantità lavoro CPU
- **IO operations**: Quantità operazioni I/O

Analizzando questi dati puoi:
- Identificare colli di bottiglia
- Testare fairness dello scheduler con workload misti
- Valutare impatto I/O wait su real-time guarantees
- Ottimizzare configurazioni per carichi specifici

## Note Tecniche

### Performance Considerations
- Le operazioni I/O sono ~100x più lente delle operazioni CPU
- `fsync()` può causare latenze >100ms su HDD
- SSD vs HDD hanno performance molto diverse
- Lo swapping memory aumenta drasticamente i tempi I/O

### Compatibilità
- Richiede filesystem con supporto fsync (ext4, xfs, ecc.)
- Richiede accesso a `/tmp` con permessi scrittura
- Testato su Linux (potrebbe richiedere modifiche per altri OS)

### Cleanup
I file temporanei vengono rimossi automaticamente, ma in caso di crash potrebbero rimanere in `/tmp/benchmark_io_*.tmp`

Puoi pulirli manualmente:
```bash
rm -f /tmp/benchmark_io_*.tmp
```

## Casi d'Uso

### 1. Web Application Simulation
`io_percentage=30-40`: Mix CPU (business logic) + I/O (database, cache)

### 2. Data Processing Pipeline
`io_percentage=60-70`: Lettura file grandi, processing, scrittura risultati

### 3. Scientific Computing
`io_percentage=5-10`: Calcoli intensivi con checkpoint periodici su disco

### 4. Database Server
`io_percentage=70-90`: Queries, disk reads/writes, index operations

## Troubleshooting

### Task molto lenti con high IO%
- Controlla performance disco: `iostat -x 1`
- Verifica se il disco è saturo
- Considera usare tmpfs: `mount -t tmpfs -o size=1G tmpfs /tmp/benchmark`

### Out of Memory
- Riduci `io_percentage` sotto 80
- Aumenta RAM disponibile
- Monitora: `free -h` e `vmstat 1`

### File descriptor limits
Se ottieni errori "Too many open files":
```bash
ulimit -n 4096
```
