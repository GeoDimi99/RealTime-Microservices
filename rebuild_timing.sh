#!/bin/bash
# Script per ricostruire i servizi dopo le modifiche ai timestamp

echo "🔨 Rebuilding services with timestamp improvements..."

# Rebuild proto files
echo "📦 Regenerating protobuf files..."
cd /home/khadas/RT-grpc/RealTime-Microservices
sudo docker compose build --no-cache proto-builder 2>/dev/null || true

# Rebuild execution-manager
echo "🔧 Rebuilding execution-manager..."
sudo docker compose build execution-manager

# Rebuild task-service-sum
echo "🔧 Rebuilding task-service-sum..."
sudo docker compose build task-service-sum

echo "✅ Rebuild complete!"
echo ""
echo "To test, run:"
echo "  sudo docker compose up execution-manager"
