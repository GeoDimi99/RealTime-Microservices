#!/bin/bash

# Script to analyze benchmark results and calculate statistics

echo "============================================"
echo "   BENCHMARK RESULTS ANALYSIS"
echo "============================================"
echo "📊 Extracting execution times from execution-manager logs..."
echo ""

# Extract execution times (in microseconds) from execution-manager logs
# Expected format: "   [EXECUTION TIME] 12345 µs (12.345 ms)"
# Read from temporary file saved during benchmark execution
if [ -f /tmp/benchmark_execution.log ]; then
    times=$(grep "\[EXECUTION TIME\]" /tmp/benchmark_execution.log | awk '{print $3}')
else
    # Fallback: try to read from docker logs (if container still exists)
    times=$(docker logs execution-manager 2>&1 | grep "\[EXECUTION TIME\]" | awk '{print $3}')
fi

if [ -z "$times" ]; then
    echo "❌ No timing data found in logs!"
    echo "Make sure the benchmark has been executed."
    exit 1
fi

# Count number of executions
count=$(echo "$times" | wc -l)

# Calculate statistics using awk
stats=$(echo "$times" | awk '
BEGIN {
    sum = 0
    min = 999999999
    max = 0
    count = 0
}
{
    val = $1
    sum += val
    count++
    if (val < min) min = val
    if (val > max) max = val
    values[count] = val
}
END {
    avg = sum / count
    
    # Calculate standard deviation
    sum_sq_diff = 0
    for (i = 1; i <= count; i++) {
        diff = values[i] - avg
        sum_sq_diff += diff * diff
    }
    stddev = sqrt(sum_sq_diff / count)
    
    printf "COUNT: %d\n", count
    printf "AVERAGE: %.2f\n", avg
    printf "MIN: %.2f\n", min
    printf "MAX: %.2f\n", max
    printf "STDDEV: %.2f\n", stddev
}
')

# Parse statistics
exec_count=$(echo "$stats" | grep "COUNT:" | cut -d' ' -f2)
avg_us=$(echo "$stats" | grep "AVERAGE:" | cut -d' ' -f2)
min_us=$(echo "$stats" | grep "MIN:" | cut -d' ' -f2)
max_us=$(echo "$stats" | grep "MAX:" | cut -d' ' -f2)
stddev_us=$(echo "$stats" | grep "STDDEV:" | cut -d' ' -f2)

# Convert to milliseconds
avg_ms=$(echo "scale=3; $avg_us / 1000" | bc)
min_ms=$(echo "scale=3; $min_us / 1000" | bc)
max_ms=$(echo "scale=3; $max_us / 1000" | bc)
stddev_ms=$(echo "scale=3; $stddev_us / 1000" | bc)

echo "=== EXECUTION STATISTICS ==="
echo ""
echo "Number of executions: $exec_count"
echo ""
echo "⏱️  Execution Time:"
echo "   Average:  ${avg_ms} ms (${avg_us} µs)"
echo "   Minimum:  ${min_ms} ms (${min_us} µs)"
echo "   Maximum:  ${max_ms} ms (${max_us} µs)"
echo "   Std Dev:  ${stddev_ms} ms (${stddev_us} µs)"
echo ""

# Calculate coefficient of variation (CV)
cv=$(echo "scale=2; ($stddev_us / $avg_us) * 100" | bc)
echo "📈 Coefficient of Variation: ${cv}%"
echo ""

# Show all individual times
echo "=== INDIVIDUAL EXECUTION TIMES ==="
echo "$times" | nl -w2 -s'. ' | awk '{printf "%s %s µs (%.3f ms)\n", $1, $2, $2/1000}'
echo ""

echo "============================================"
