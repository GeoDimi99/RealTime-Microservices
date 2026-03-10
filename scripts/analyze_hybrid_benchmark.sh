#!/bin/bash

# Script to analyze hybrid CPU/IO benchmark results

echo "============================================"
echo "   HYBRID BENCHMARK ANALYSIS"
echo "============================================"
echo ""

# Check if log file exists
if [ ! -f /tmp/benchmark_execution.log ]; then
    echo "❌ No log file found at /tmp/benchmark_execution.log"
    echo "Run the benchmark first with: docker compose run --rm execution-manager 2>&1 | tee /tmp/benchmark_execution.log"
    exit 1
fi

echo "📊 Extracting results from execution logs..."
echo ""

# Extract task completion data
results=$(grep -B 5 "EXECUTION TIME" /tmp/benchmark_execution.log | \
         grep -E "(Task [0-9]+ 'sum'|EXECUTION TIME|RESULT)")

if [ -z "$results" ]; then
    echo "❌ No execution data found in logs!"
    exit 1
fi

echo "=== TASK EXECUTION SUMMARY ==="
echo ""

# Parse each task
task_count=0
declare -a task_ids
declare -a exec_times
declare -a cpu_iters
declare -a io_ops

while IFS= read -r line; do
    if [[ $line =~ Task\ ([0-9]+) ]]; then
        current_task="${BASH_REMATCH[1]}"
    fi
    
    if [[ $line =~ EXECUTION\ TIME.*\ ([0-9]+)\ µs ]]; then
        exec_time="${BASH_REMATCH[1]}"
    fi
    
    if [[ $line =~ \"cpu_iterations\":\ ([0-9]+) ]]; then
        cpu_iter="${BASH_REMATCH[1]}"
    fi
    
    if [[ $line =~ \"io_operations\":\ ([0-9]+) ]]; then
        io_op="${BASH_REMATCH[1]}"
        
        # We have all data for this task
        task_ids+=("$current_task")
        exec_times+=("$exec_time")
        cpu_iters+=("$cpu_iter")
        io_ops+=("$io_op")
        
        task_count=$((task_count + 1))
    fi
done <<< "$results"

# Display results table
printf "%-8s | %-15s | %-15s | %-12s | %-10s\n" "Task ID" "Time (ms)" "Time (µs)" "CPU Iter" "IO Ops"
echo "---------|-----------------|-----------------|--------------|------------"

for i in "${!task_ids[@]}"; do
    task_id="${task_ids[$i]}"
    time_us="${exec_times[$i]}"
    time_ms=$(echo "scale=3; $time_us / 1000" | bc)
    cpu="${cpu_iters[$i]}"
    io="${io_ops[$i]}"
    
    printf "%-8s | %-15s | %-15s | %-12s | %-10s\n" \
           "$task_id" "$time_ms" "$time_us" "$cpu" "$io"
done

echo ""
echo "=== STATISTICS ==="
echo ""

# Calculate total execution time
if [ ${#exec_times[@]} -gt 0 ]; then
    total_time=0
    min_time=${exec_times[0]}
    max_time=${exec_times[0]}
    
    for time in "${exec_times[@]}"; do
        total_time=$((total_time + time))
        if [ $time -lt $min_time ]; then
            min_time=$time
        fi
        if [ $time -gt $max_time ]; then
            max_time=$time
        fi
    done
    
    avg_time=$((total_time / ${#exec_times[@]}))
    
    avg_time_ms=$(echo "scale=3; $avg_time / 1000" | bc)
    min_time_ms=$(echo "scale=3; $min_time / 1000" | bc)
    max_time_ms=$(echo "scale=3; $max_time / 1000" | bc)
    
    echo "Number of tasks: ${#exec_times[@]}"
    echo ""
    echo "Execution Time:"
    echo "  Average: $avg_time_ms ms ($avg_time µs)"
    echo "  Minimum: $min_time_ms ms ($min_time µs)"
    echo "  Maximum: $max_time_ms ms ($max_time µs)"
    echo ""
fi

# Calculate CPU iterations statistics
if [ ${#cpu_iters[@]} -gt 0 ]; then
    total_cpu=0
    for cpu in "${cpu_iters[@]}"; do
        total_cpu=$((total_cpu + cpu))
    done
    avg_cpu=$((total_cpu / ${#cpu_iters[@]}))
    
    echo "CPU Operations:"
    echo "  Total iterations: $total_cpu"
    echo "  Average per task: $avg_cpu"
    echo ""
fi

# Calculate IO operations statistics
if [ ${#io_ops[@]} -gt 0 ]; then
    total_io=0
    for io in "${io_ops[@]}"; do
        total_io=$((total_io + io))
    done
    avg_io=$((total_io / ${#io_ops[@]}))
    
    echo "I/O Operations:"
    echo "  Total operations: $total_io"
    echo "  Average per task: $avg_io"
    echo ""
fi

# Calculate IO percentage inference (based on results)
echo "=== WORKLOAD ANALYSIS ==="
echo ""

for i in "${!task_ids[@]}"; do
    task_id="${task_ids[$i]}"
    cpu="${cpu_iters[$i]}"
    io="${io_ops[$i]}"
    
    # Estimate IO percentage based on operations
    # (This is approximate - actual percentage comes from input)
    if [ $cpu -eq 0 ] && [ $io -gt 0 ]; then
        est_io_pct=100
    elif [ $io -eq 0 ]; then
        est_io_pct=0
    else
        # Rough estimate: normalize based on typical values
        # Pure CPU: ~10M iterations
        # Pure IO: ~150 operations
        cpu_normalized=$(echo "scale=2; $cpu / 100000" | bc)
        io_normalized=$(echo "scale=2; $io * 100" | bc)
        
        total=$(echo "$cpu_normalized + $io_normalized" | bc)
        if (( $(echo "$total > 0" | bc -l) )); then
            est_io_pct=$(echo "scale=0; ($io_normalized / $total) * 100" | bc)
        else
            est_io_pct=0
        fi
    fi
    
    echo "Task $task_id: ~${est_io_pct}% IO workload (estimated)"
done

echo ""
echo "=== PERFORMANCE METRICS ==="
echo ""

# Calculate throughput (operations per second)
for i in "${!task_ids[@]}"; do
    task_id="${task_ids[$i]}"
    time_us="${exec_times[$i]}"
    cpu="${cpu_iters[$i]}"
    io="${io_ops[$i]}"
    
    time_sec=$(echo "scale=6; $time_us / 1000000" | bc)
    
    if (( $(echo "$time_sec > 0" | bc -l) )); then
        cpu_throughput=$(echo "scale=0; $cpu / $time_sec" | bc)
        io_throughput=$(echo "scale=2; $io / $time_sec" | bc)
        
        echo "Task $task_id:"
        echo "  CPU Throughput: $cpu_throughput iter/sec"
        echo "  I/O Throughput: $io_throughput ops/sec"
        echo ""
    fi
done

# Time vs IO correlation (if we have multiple tasks)
if [ ${#task_ids[@]} -ge 3 ]; then
    echo "=== TIME vs IO CORRELATION ==="
    echo ""
    echo "Analyzing trend: execution time vs I/O operations..."
    echo ""
    
    # Simple correlation: sort by IO ops and show times
    for i in "${!task_ids[@]}"; do
        echo "${io_ops[$i]} ${exec_times[$i]}"
    done | sort -n | while read io time; do
        time_ms=$(echo "scale=3; $time / 1000" | bc)
        echo "  $io I/O ops → $time_ms ms"
    done
    
    echo ""
fi

echo "============================================"
echo ""

# Optional: Create visualization data file
output_file="/tmp/hybrid_benchmark_data.csv"
echo "task_id,time_us,time_ms,cpu_iterations,io_operations" > "$output_file"

for i in "${!task_ids[@]}"; do
    task_id="${task_ids[$i]}"
    time_us="${exec_times[$i]}"
    time_ms=$(echo "scale=3; $time_us / 1000" | bc)
    cpu="${cpu_iters[$i]}"
    io="${io_ops[$i]}"
    
    echo "$task_id,$time_us,$time_ms,$cpu,$io" >> "$output_file"
done

echo "📊 Data exported to: $output_file"
echo "   Use this file for plotting or further analysis"
echo ""

# Suggest plotting command if gnuplot is available
if command -v gnuplot &> /dev/null; then
    echo "💡 Generate plot with:"
    echo "   gnuplot -e \"set terminal png; set output 'hybrid_benchmark.png'; set xlabel 'I/O Operations'; set ylabel 'Time (ms)'; plot '$output_file' using 5:3 with linespoints title 'Time vs IO'\""
    echo ""
fi

echo "✅ Analysis complete!"
