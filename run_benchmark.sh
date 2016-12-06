#!/bin/bash

if [ "$#" -ne 2 ]; then
        echo "Usage: run_benchmark.sh <interation> <max processors>"
        exit
    fi

echo ======================================================
echo running $1 iterations for processors 1 to $2
echo ======================================================
NUMBER_OF_PROCS=1
while [  $NUMBER_OF_PROCS -le $2 ]; do
    echo -n $NUMBER_OF_PROCS ' '
    ./benchmark_pool $1 $NUMBER_OF_PROCS | awk 'BEGIN { ORS=" " } {print $8}'
    echo ''
    let NUMBER_OF_PROCS=$NUMBER_OF_PROCS+1
done
