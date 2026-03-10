#!/bin/bash

# Script to measure build times for task images

echo "============================================"
echo "   IMAGE BUILD TIME MEASUREMENT"
echo "============================================"
echo ""

# Check if --no-cache flag is provided
NO_CACHE=""
BUILD_TYPE="with cache"
if [ "$1" == "--no-cache" ]; then
    NO_CACHE="--no-cache"
    BUILD_TYPE="WITHOUT cache"
fi

echo "📊 Measuring build time ($BUILD_TYPE)..."
echo ""

# Capture start time
START_TIME=$(date +%s.%N)

# Run the build
echo "🔨 Building task images..."
python3 sdk/image-builder/src/main.py -f task/manifest.yaml -c task $NO_CACHE

# Capture end time
END_TIME=$(date +%s.%N)

# Calculate elapsed time
ELAPSED=$(echo "$END_TIME - $START_TIME" | bc)

echo ""
echo "============================================"
echo "   BUILD TIME RESULTS"
echo "============================================"
echo ""
echo "Build type: $BUILD_TYPE"
echo "Total time: ${ELAPSED}s"
echo ""

# Convert to minutes and seconds if > 60s
if (( $(echo "$ELAPSED > 60" | bc -l) )); then
    MINUTES=$(echo "$ELAPSED / 60" | bc)
    SECONDS=$(echo "$ELAPSED % 60" | bc)
    echo "Formatted:  ${MINUTES}m ${SECONDS}s"
    echo ""
fi

# Save to log file
LOG_FILE="build_times.log"
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
echo "$TIMESTAMP | $BUILD_TYPE | ${ELAPSED}s" >> $LOG_FILE
echo "📝 Result saved to: $LOG_FILE"
echo ""
