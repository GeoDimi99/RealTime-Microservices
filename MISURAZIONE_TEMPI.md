# Guida alla Misurazione dei Tempi di Build

Questo documento spiega come misurare i tempi di esecuzione del processo di build delle immagini Docker.

---

## 🚀 Comandi Rapidi

### Misurazione con `time` (metodo veloce)

```bash
# Build con cache
time python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task

# Build senza cache
time python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task --no-cache
```

### Misurazione con script dedicato (con logging)

```bash
# Build con cache
bash scripts/measure_build_time.sh

# Build senza cache
bash scripts/measure_build_time.sh --no-cache
```

---

## 📊 Interpretazione dei Risultati

### Output del comando `time`

Quando usi `time`, otterrai tre metriche:

```
real    0m0.569s
user    0m0.402s
sys     0m0.070s
```

- **real**: Tempo totale "wall clock" dall'inizio alla fine
  - Include tutto: CPU, I/O, attese, ecc.
  - È il tempo che percepisci come utente
  
- **user**: Tempo CPU speso in user mode
  - Esecuzione del codice dell'applicazione
  - Calcoli, elaborazioni
  
- **sys**: Tempo CPU speso in kernel mode
  - System calls, I/O, gestione memoria
  - Operazioni a livello di sistema operativo

### Output dello script `measure_build_time.sh`

Lo script mostra:

```
============================================
   BUILD TIME RESULTS
============================================

Build type: with cache
Total time: 0.631344772s

📝 Result saved to: build_times.log
```

E salva automaticamente i risultati in `build_times.log`:

```
2026-03-09 22:52:33 | with cache | 0.631344772s
2026-03-09 22:55:10 | WITHOUT cache | 45.823s
```

---

## 🔍 Scenari di Misurazione

### 1. Confronto Cache vs No-Cache

Per confrontare l'impatto della cache:

```bash
echo "=== Build CON cache ===" && \
time python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task

echo ""
echo "=== Build SENZA cache ===" && \
time python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task --no-cache
```

### 2. Test di Performance Ripetuti

Per eseguire 5 test consecutivi con cache:

```bash
for i in {1..5}; do
    echo "=== Test $i ===" 
    bash scripts/measure_build_time.sh
    sleep 2
done

# Vedi tutti i risultati
cat build_times.log
```

### 3. Test dopo Modifica del Codice

```bash
# 1. Misura tempo iniziale
bash scripts/measure_build_time.sh

# 2. Modifica il codice in task/sum/app_task.h

# 3. Misura tempo dopo la modifica
bash scripts/measure_build_time.sh

# 4. Confronta i tempi
cat build_times.log
```

---

## 📈 Tempi Attesi

### Con Cache (build incrementale)
- **2-3 task**: ~0.5-1.0 secondi
- **20 task** (benchmark): ~2-5 secondi

Veloce perché Docker riusa i layer già costruiti.

### Senza Cache (build completa)
- **2-3 task**: ~30-60 secondi
- **20 task** (benchmark): ~5-10 minuti

Lento perché ricompila tutto da zero:
- Installazione dipendenze
- Compilazione protobuf
- Compilazione C/C++
- Build con CMake

---

## 🛠️ Troubleshooting

### I tempi sembrano troppo lunghi?

1. **Controlla se stai usando la cache**:
   ```bash
   # Dovresti vedere "Using cache" nell'output
   python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task
   ```

2. **Verifica le risorse Docker**:
   ```bash
   docker info | grep -E "CPUs|Total Memory"
   ```

3. **Controlla il carico del sistema**:
   ```bash
   htop  # o top
   ```

### Lo script di misurazione non funziona?

1. **Verifica che sia eseguibile**:
   ```bash
   chmod +x scripts/measure_build_time.sh
   ```

2. **Verifica che `bc` sia installato** (necessario per i calcoli):
   ```bash
   which bc || sudo apt install bc
   ```

---

## 📝 Log File

Il file `build_times.log` viene creato nella root del progetto e contiene lo storico di tutte le misurazioni.

### Vedere le ultime 10 misurazioni
```bash
tail -10 build_times.log
```

### Filtrare solo build senza cache
```bash
grep "WITHOUT cache" build_times.log
```

### Calcolare tempo medio
```bash
# Media di tutte le build con cache
grep "with cache" build_times.log | awk -F'|' '{print $3}' | \
    awk '{sum+=$1; count++} END {print "Media:", sum/count, "s"}'
```

### Pulire il log
```bash
rm build_times.log
```

---

## 🎯 Best Practices

1. **Per sviluppo normale**: Usa la cache
   ```bash
   python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task
   ```

2. **Per release/deploy**: Usa `--no-cache`
   ```bash
   python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task --no-cache
   ```

3. **Per misurazioni accurate**: 
   - Esegui almeno 3 test consecutivi
   - Scarta il primo (warm-up)
   - Fai la media degli altri

4. **Per benchmark**: Usa lo script dedicato che salva tutto
   ```bash
   bash scripts/measure_build_time.sh
   ```

---

## 🔗 Riferimenti

- Documentazione completa: `GUIDA_ESECUZIONE.md`
- Script di benchmark: `scripts/run_benchmark.sh`
- Script di analisi: `scripts/analyze_benchmark.sh`
