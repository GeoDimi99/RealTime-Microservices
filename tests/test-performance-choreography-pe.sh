#!/bin/bash

# Set env variables
TEST_NAME=$1



docker build -t execution-manager -f ../services/execution-manager/Dockerfile .. 

# Tasks 
docker build  -t stress_task -f ../services/task-wrapper/Dockerfile ..


mkdir $TEST_NAME || rm -rf $TEST_NAME && mkdir $TEST_NAME
cd $TEST_NAME

# Init the file table fields
echo "iteration,task_id,start_req,end_req,start_res,end_res,em_tw_ms,task_ms,tw_em_ms,total_ms" > $TEST_NAME.csv

# Run the tasks
rm /dev/mqueue/*

# Run tasks
docker rm -f stress_task_1 stress_task_2 stress_task_3 stress_task_4
docker run -d --rm --ipc=host --tmpfs /tmp:size=64m,mode=1777 --cap-add=SYS_NICE --ulimit rtprio=99 -e TASK_NAME=stress_task_1 -e TASK_QUEUE_NAME=stress_task_1 --cap-add=IPC_LOCK --ulimit memlock=-1:-1 --cpuset-cpus="4,5" --name stress_task_1 stress_task:latest
docker run -d --rm --ipc=host --tmpfs /tmp:size=64m,mode=1777 --cap-add=SYS_NICE --ulimit rtprio=99 -e TASK_NAME=stress_task_2 -e TASK_QUEUE_NAME=stress_task_2 --cap-add=IPC_LOCK --ulimit memlock=-1:-1 --cpuset-cpus="4,5" --name stress_task_2 stress_task:latest
docker run -d --rm --ipc=host --tmpfs /tmp:size=64m,mode=1777 --cap-add=SYS_NICE --ulimit rtprio=99 -e TASK_NAME=stress_task_3 -e TASK_QUEUE_NAME=stress_task_3 --cap-add=IPC_LOCK --ulimit memlock=-1:-1 --cpuset-cpus="4,5" --name stress_task_3 stress_task:latest
docker run -d --rm --ipc=host --tmpfs /tmp:size=64m,mode=1777 --cap-add=SYS_NICE --ulimit rtprio=99 -e TASK_NAME=stress_task_4 -e TASK_QUEUE_NAME=stress_task_4 --cap-add=IPC_LOCK --ulimit memlock=-1:-1 --cpuset-cpus="4,5" --name stress_task_4 stress_task:latest


# Run the program with performance sensors
docker rm -f execution-manager
docker run --rm --ipc=host --cap-add=SYS_NICE --ulimit rtprio=99 --cap-add=IPC_LOCK --ulimit memlock=-1:-1 --cpuset-cpus="4" --name execution-manager execution-manager:latest | tee output.log
docker stop execution-manager stress_task_1 stress_task_2 stress_task_3 stress_task_4
grep "PERF_LOG:" output.log | sed 's/PERF_LOG://' >> $TEST_NAME.csv

python3 ../analyze_performance.py $TEST_NAME.csv
