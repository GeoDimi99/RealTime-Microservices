# Spacecraft Schedule Manifest

## 📋 Overview

Ho creato il file `manifest-spacecraft.yaml` basato sui dati dell'Excel fornito.

## 🛰️ Task Identificati

Dal Gantt chart nell'Excel, ho identificato questi task:

| ID | Task Name      | Priority | Start (ms) | Deadline (ms) | Note |
|----|---------------|----------|------------|---------------|------|
| 1  | SATCM         | 136      | 39         | 54            | Satellite Attitude Control Manager |
| 2  | SWAPF         | 137      | 23         | 102           | Software Application Framework |
| 3  | SSMAPF        | 138      | 46         | 102           | System Software Management Application Framework |
| 4  | AOCN          | 141      | 70         | 117           | Attitude and Orbit Control Navigation |
| 5  | AOCSCM_POST   | 142      | 70         | 102           | AOC System Control Manager Post-processing |
| 6  | CMG (1st)     | 143      | 15         | 31            | Control Moment Gyroscope - First execution |
| 7  | CMG (2nd)     | 143      | 62         | 93            | Control Moment Gyroscope - Second execution |
| 8  | AOCN_CMG (1st)| 134      | 15         | 31            | AOC Navigation CMG - First execution |
| 9  | AOCN_CMG (2nd)| 134      | 62         | 93            | AOC Navigation CMG - Second execution |
| 10 | ORB           | 136      | 62         | 93            | Orbit Determination |
| 11 | MHSTR         | 139      | 39         | 93            | Mission Health and Status Reporting |
| 12 | SFDIR         | 139      | 101        | 109           | Safe Mode Director |

## ⚠️ Importante: Placeholder Tasks

**ATTENZIONE**: Attualmente tutti i task usano `"sum"` come sorgente placeholder.

Per avere un sistema funzionante, devi:

### Opzione 1: Usare task esistenti (per testing)
Lascia tutto come è. Tutti i task useranno l'implementazione di `sum`, quindi funzionerà ma tutti i task faranno la stessa operazione.

### Opzione 2: Creare implementazioni specifiche (consigliato)

Per ogni task, crea una cartella in `task/` con il file `app_task.h`:

```bash
# Esempio per SATCM
mkdir -p task/satcm
cp task/sum/app_task.h task/satcm/app_task.h
# Modifica task/satcm/app_task.h con la logica specifica
```

Poi aggiorna il manifest cambiando `src`:
```yaml
- alias: "satcm"
  src: "satcm"  # invece di "sum"
```

## 🔧 Come Usare il Manifest

### 1. Test con placeholder (rapido)
```bash
# Build delle immagini (userà sum come base per tutti)
python3 sdk/image-builder/src/main.py -f task/manifest-spacecraft.yaml -c task

# Deploy
docker compose up -d redis deploy-manager

# Esecuzione
docker compose up execution-manager
```

### 2. Personalizzazione completa

#### Step 1: Crea le implementazioni dei task

Per ogni task (satcm, swapf, ecc.), crea:

```bash
mkdir -p task/satcm
```

Crea il file `task/satcm/app_task.h`:
```c
#ifndef APP_TASK_H
#define APP_TASK_H

#include <stdio.h>

// Task-specific logic for SATCM
static inline void task_main() {
    printf("[SATCM] Satellite Attitude Control Manager executing...\n");
    
    // Implementa la logica specifica del task SATCM
    // Es: calcoli per controllo assetto, lettura sensori, ecc.
    
    printf("[SATCM] Task completed\n");
}

#endif
```

Ripeti per tutti i task.

#### Step 2: Aggiorna il manifest

Modifica `manifest-spacecraft.yaml`:
```yaml
tasks:
  - alias: "satcm"
    src: "satcm"  # ora punta alla cartella specifica
  
  - alias: "swapf"
    src: "swapf"
  
  # ... e così via
```

#### Step 3: Build e deploy

```bash
# Build delle immagini
python3 sdk/image-builder/src/main.py -f task/manifest-spacecraft.yaml -c task

# Deploy e esecuzione
docker compose up -d redis deploy-manager
docker compose up execution-manager
```

## 📊 Caratteristiche dello Schedule

- **Timeline**: 0-117 ms
- **Task totali**: 12 (alcuni task si ripetono)
- **Priorità**: Range da 134 (AOCN_CMG, più alta) a 143 (CMG, più bassa)
- **Task ricorrenti**: CMG e AOCN_CMG si eseguono due volte

## 🎯 Policy di Scheduling

Attualmente tutti i task usano policy `"fifo"`. Puoi cambiarla in:
- `"fifo"` - First In First Out
- `"edf"` - Earliest Deadline First (richiederebbe supporto nell'execution-manager)
- Altri... (da implementare)

## 📝 Note sulla Priorità

Nell'Excel, priorità **più basse** = **più importanti**:
- 134 = massima priorità (AOCN_CMG)
- 143 = minima priorità (CMG)

Questo è tipico nei sistemi real-time.

## 🚀 Next Steps

1. ✅ Manifest creato basato su Excel
2. ⏳ Decidere: usare placeholder o creare implementazioni?
3. ⏳ Se necessario: creare le cartelle task specifiche
4. ⏳ Build e test dello schedule
5. ⏳ Validazione tempi di esecuzione

## 📚 Riferimenti

- Manifest completo: `task/manifest-spacecraft.yaml`
- Esempio task: `task/sum/app_task.h`
- Guida esecuzione: `GUIDA_ESECUZIONE.md`
