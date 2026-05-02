#!/bin/bash

for test_elem in  test_1_IO_0_perc test_1_IO_25_perc test_1_IO_50_perc test_1_IO_75_perc test_1_IO_100_perc test_2 test_3 test_4 test_5 test_6
do
    # Test Native Implementation (Baseline)
    cd /home/khadas/RT-microservices-native/tests
    echo "Start test $test_elem (Native)"
    cat test_io_ram/test_code/${test_elem}_main.c > ../services/execution-manager/src/main.c
    ./test-performance-native.sh $test_elem
    sleep 60


    Test Choreography with Process Engine Implementation 
    cd /home/khadas/RT-microservices-choreography-pe/tests
    echo "Start test $test_elem (Choreography-PE)"
    cat test_io_ram/test_code/${test_elem}_main.c > ../services/execution-manager/src/main.c 
    ./test-performance-choreography-pe.sh $test_elem
    sleep 60

done 