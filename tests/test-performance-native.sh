#!/bin/bash

# Set env variables
TEST_NAME=$1

TESTS_DIR=$(pwd)

cd ../services/execution-manager/
cmake --build build
cd $TESTS_DIR

mkdir $TEST_NAME || rm -rf $TEST_NAME && mkdir $TEST_NAME
cd $TEST_NAME
echo Qui $(pwd)

# Init the file table fields
echo "iteration,task_id,start_req,end_req,start_res,end_res,em_tw_ms,task_ms,tw_em_ms,total_ms" > $TEST_NAME.csv

# Run the program with performance sensors
../../services/execution-manager/build/execution-manager | tee output.log
grep "PERF_LOG:" output.log | sed 's/PERF_LOG://' >> $TEST_NAME.csv

python3 ../analyze_performance.py $TEST_NAME.csv
