#!/bin/bash

# Set env variables
METRICS_FILENAME=$1

# Init the file table fields
echo "iteration,task_id,start_req,end_req,start_res,end_res,em_tw_ms,task_ms,tw_em_ms,total_ms" > $METRICS_FILENAME

# Run the program with performance sensors
/home/vboxuser/projects/RT-microservices-native/services/execution-manager/build/execution-manager | tee output.log
grep "PERF_LOG:" output.log | sed 's/PERF_LOG://' >> $METRICS_FILENAME

python3 analyze_performance.py $METRICS_FILENAME
