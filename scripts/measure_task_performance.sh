#!/bin/bash

# Script to measure task performance with automatic analysis

echo "============================================"
echo "   TASK PERFORMANCE MEASUREMENT"
echo "============================================"
echo ""

# Check for manifest argument
MANIFEST="${1:-task/manifest-measure.yaml}"

if [ ! -f "$MANIFEST" ]; then
    echo "❌ Manifest file not found: $MANIFEST"
    echo "Usage: $0 [manifest_file]"
    exit 1
fi

echo "📋 Using manifest: $MANIFEST"
echo ""

# Step 1: Stop and cleanup
echo "🧹 Cleaning up previous containers..."
docker compose down -v > /dev/null 2>&1

# Step 2: Rebuild deploy-manager to load new manifest
echo "🔨 Rebuilding deploy-manager with new manifest..."
cp "$MANIFEST" task/manifest.yaml
docker compose build deploy-manager > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

# Step 3: Start services
echo "🚀 Starting services..."
docker compose up -d redis deploy-manager > /dev/null 2>&1

echo "⏳ Waiting for services to initialize (15 seconds)..."
sleep 15

# Step 4: Run benchmark
echo "▶️  Running benchmark..."
echo ""
docker compose run --rm execution-manager 2>&1 | tee /tmp/benchmark_execution.log

# Step 5: Analyze results
echo ""
echo "============================================"
echo ""
bash scripts/analyze_benchmark.sh

# Step 6: Show task details
echo ""
echo "============================================"
echo "   TASK CONFIGURATION"
echo "============================================"
echo ""
grep -A 5 "total_ops:" "$MANIFEST" | head -6

echo ""
echo "✅ Measurement complete!"
