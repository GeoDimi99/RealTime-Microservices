#!/bin/bash

# Complete benchmark execution script

set -e  # Exit on error

echo "============================================"
echo "   MATHEMATICAL BENCHMARK RUNNER"
echo "============================================"
echo ""

# Step 1: Build base image
echo "🔨 Step 1/6: Building base task-wrapper image..."
docker build --no-cache -t geodimi99/realtime-microservices:task-wrapper \
    -f services/task-wrapper/Dockerfile .
echo "✅ Base image built"
echo ""

# Step 2: Backup and replace manifest
echo "📝 Step 2/6: Setting up benchmark manifest..."
if [ -f task/manifest.yaml.backup ]; then
    echo "   Backup already exists, using it"
else
    cp task/manifest.yaml task/manifest.yaml.backup
    echo "   Created backup of original manifest"
fi
cp task/manifest-benchmark.yaml task/manifest.yaml
echo "✅ Benchmark manifest ready"
echo ""

# Step 3: Build task image
echo "🔨 Step 3/6: Building task image..."
python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task 
echo "✅ Task image built"
echo ""

# Step 4: Build services
echo "🔨 Step 4/6: Building deploy-manager and execution-manager..."
docker compose build deploy-manager execution-manager 
echo "✅ Services built"
echo ""

# Step 5: Cleanup and start services
echo "🧹 Step 5/6: Cleaning up and starting services..."
docker compose down 
docker stop task-service-sum 
docker rm task-service-sum 
docker compose up -d redis deploy-manager 
echo "   Waiting for container to be ready (15 seconds)..."
sleep 15
echo "✅ Services ready (1 container for 20 task executions)"
echo ""

# Step 6: Run benchmark
echo "🚀 Step 6/6: Executing benchmark (20 tasks, 1 container)..."
echo "   This will take approximately 25 seconds..."
echo ""

# Save full logs to temporary file for analysis, show filtered output to user
docker compose run --rm execution-manager 2>&1 | tee /tmp/benchmark_execution.log | grep -E "(unique task|SCHEDULER|STARTED|COMPLETED|EXECUTION TIME|T=)"

echo ""
echo "============================================"
echo "   BENCHMARK COMPLETED"
echo "============================================"
echo ""
echo "📊 Analyzing results..."
echo ""

# Run analysis
bash scripts/analyze_benchmark.sh

# Restore original manifest
echo ""
echo "🔄 Restoring original manifest..."
cp task/manifest.yaml.backup task/manifest.yaml
echo "✅ Original manifest restored"
