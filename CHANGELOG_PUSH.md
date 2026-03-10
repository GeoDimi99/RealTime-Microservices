# Changelog - Rimozione Push Automatico su DockerHub

## Data Modifica
9 Marzo 2026

## Modifiche Apportate

### 1. File Modificati

#### `sdk/image-builder/src/main.py`
- **Righe 6, 97-110**: Commentata la fase di pubblicazione automatica su DockerHub
- **Motivo**: Evitare push accidentali durante il processo di build
- **Impatto**: Le immagini vengono ora costruite **solo localmente**

### 2. Comportamento Precedente
Quando eseguivi:
```bash
python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task
```

Lo script eseguiva automaticamente:
1. ✅ Build delle immagini task (sum, subtract, etc.)
2. ❌ **Push automatico su DockerHub** (RIMOSSO)

### 3. Comportamento Attuale
Ora lo script esegue solo:
1. ✅ Build delle immagini task localmente
2. ⏸️ Nessun push automatico

### 4. Come Fare il Push Manualmente (se necessario)

Se hai bisogno di pubblicare le immagini su DockerHub, puoi farlo manualmente:

```bash
# Login su DockerHub
docker login

# Tag e push dell'immagine base
docker push geodimi99/realtime-microservices:task-wrapper

# Tag e push delle immagini task
docker tag sum geodimi99/realtime-microservices:sum
docker push geodimi99/realtime-microservices:sum

docker tag subtract geodimi99/realtime-microservices:subtract
docker push geodimi99/realtime-microservices:subtract
```

Oppure usa un loop per tutte le immagini:
```bash
for task in sum subtract; do
    docker tag $task geodimi99/realtime-microservices:$task
    docker push geodimi99/realtime-microservices:$task
done
```

### 5. File Aggiornati
- ✅ `sdk/image-builder/src/main.py` - Push automatico disabilitato
- ✅ `GUIDA_ESECUZIONE.md` - Aggiunta sezione "Push Manuale su DockerHub"
- ✅ `GUIDA_ESECUZIONE.md` - Aggiunta nota nelle "Note Importanti"

### 6. Nessun Impatto su
- ✅ Build delle immagini (continua a funzionare)
- ✅ Esecuzione locale del sistema
- ✅ Docker Compose
- ✅ Deploy Manager
- ✅ Execution Manager

### 7. Vantaggi
- ✅ Nessun push accidentale durante lo sviluppo
- ✅ Maggiore controllo sulle pubblicazioni
- ✅ Risparmio di tempo durante i test locali
- ✅ Nessun consumo di banda per push non necessari

---

## Note Aggiuntive

- Il file `.env` con le credenziali DockerHub (`DOCKER_USER`, `DOCKER_TOKEN`) non è più utilizzato dall'image-builder
- Per riabilitare il push automatico, basta decommentare le righe 6, 97-110 in `sdk/image-builder/src/main.py`
- Le immagini costruite localmente sono completamente funzionali e pronte all'uso
