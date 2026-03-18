#!/bin/bash

# Set env variables
TEST_NAME=$1

mkdir $TEST_NAME
cd $TEST_NAME

# Init the file table fields
echo "iteration,task_id,start_req,end_req,start_res,end_res,em_tw_ms,task_ms,tw_em_ms,total_ms" > $TEST_NAME.csv

# Run the tasks
rm /dev/mqueue/*
docker run --rm -d --ipc=host --cap-add=SYS_NICE --ulimit rtprio=99 -e TASK_NAME=stress_task -e TASK_QUEUE_NAME=stress_task --cap-add=IPC_LOCK --ulimit memlock=-1:-1 --name stress_task stress_task:latest

# Run the program with performance sensors
docker run --rm --ipc=host --name execution-manager execution-manager:latest | tee output.log
docker stop $(docker ps -aq)
grep "PERF_LOG:" output.log | sed 's/PERF_LOG://' >> $TEST_NAME.csv

python3 ../analyze_performance.py $TEST_NAME.csv
