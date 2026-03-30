#!/usr/bin/env bash
set -euo pipefail

# Colori per output leggibile
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

step() {
  echo -e "\n${GREEN}▶ [$1/$TOTAL] $2${NC}\n"
}

fail() {
  echo -e "\n${RED}✖ Errore allo step $1: $2${NC}\n" >&2
  exit 1
}

TOTAL=7

# 1 — Build immagine task-wrapper
step 1 "docker build task-wrapper"
docker build --no-cache -t geodimi99/realtime-microservices:task-wrapper \
  -f services/task-wrapper/Dockerfile . \
  || fail 1 "docker build task-wrapper"

# 2 — Build immagine task via SDK image-builder
step 2 "python3 image-builder (task manifest)"
python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task --no-cache \
  || fail 2 "python3 image-builder"

# 3 — Build deploy-manager
step 3 "docker compose build deploy-manager"
docker compose build deploy-manager \
  || fail 3 "docker compose build deploy-manager"

# 4 — Build execution-manager
step 4 "docker compose build execution-manager"
docker compose build execution-manager \
  || fail 4 "docker compose build execution-manager"

# 5 — Avvia Redis in background
step 5 "docker compose up -d redis"
docker compose up -d redis \
  || fail 5 "docker compose up redis"

# 6 — Avvia deploy-manager (foreground, poi Ctrl+C o attendi uscita)
step 6 "docker compose up deploy-manager"
docker compose up deploy-manager \
  || fail 6 "docker compose up deploy-manager"

# 7 — Avvia execution-manager (output salvato su file di log)
LOG_FILE="execution_manager_$(date +%Y%m%d_%H%M%S).log"
step 7 "docker compose up execution-manager  →  log: $LOG_FILE"
docker compose up execution-manager 2>&1 | tee "$LOG_FILE" \
  || fail 7 "docker compose up execution-manager"

echo -e "\n${GREEN}✔ Tutti gli step completati con successo.${NC}"
echo -e "${GREEN}📄 Log salvato in: $LOG_FILE${NC}"
