#!/bin/bash

# Set env variables
TEST_NAME=$1

# Cleanup 
docker compose down
docker rm -f stress_task_1 stress_task_2 stress_task_3 stress_task_4


# Tasks 
docker build  -t stress_task_1 -f ../services/task-wrapper/Dockerfile ..
docker build  -t stress_task_2 -f ../services/task-wrapper/Dockerfile ..
docker build  -t stress_task_3 -f ../services/task-wrapper/Dockerfile ..
docker build  -t stress_task_4 -f ../services/task-wrapper/Dockerfile ..
docker compose build


mkdir $TEST_NAME || rm -rf $TEST_NAME && mkdir $TEST_NAME
cd $TEST_NAME

# Init the file table fields
echo "iteration,task_id,start_req,end_req,start_res,end_res,em_tw_ms,task_ms,tw_em_ms,total_ms" > $TEST_NAME.csv

# Run the tasks
rm /dev/mqueue/*

# Run tasks
docker compose up -d

sleep 60

# Run the program with performance sensors

docker logs -f stress_task_1 | tee output.log
docker logs -f stress_task_2 | tee -a output.log
docker logs -f stress_task_3 | tee -a output.log
docker logs -f stress_task_4 | tee -a output.log
grep "PERF_LOG:" output.log | sed 's/PERF_LOG://' >> $TEST_NAME.csv

python3 ../analyze_performance.py $TEST_NAME.csv
