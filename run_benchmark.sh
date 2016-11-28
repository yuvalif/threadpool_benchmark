#!/bin/bash
echo running $1 iterations for processors 1 to $2
echo ======================================================
NUMBER_OF_PROCS=1
while [  $NUMBER_OF_PROCS -le $2 ]; do
    echo -n $NUMBER_OF_PROCS ' '
    ./benchmark_pool $1 $NUMBER_OF_PROCS
    let NUMBER_OF_PROCS=$NUMBER_OF_PROCS+1
done
