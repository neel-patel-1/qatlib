#!/bin/bash

# sudo python3 scripts/accel_conf.py --load=configs/iaa-1n1d8e1w128q-s-n2.conf

QUERY_SIZES=( 64    256  1024 4096 16384 65536 262144 1048576 )
ITERATIONS=(  100   100  100  100  100   10    10     10  )
REQUESTS=(    10000 1000 1000 1000 1000  100   100    100 )

CORES=( 20 )
NUM_ACCESSES=10

# Initialize decomp_and_scatter_logs
mkdir -p decomp_and_scatter_logs
for q in "${QUERY_SIZES[@]}"; do
    for i in "${CORES[@]}";
    do
        echo -n > decomp_and_scatter_logs/core_${i}_querysize_${q}.log
    done
done

qidx=0
for q in "${QUERY_SIZES[@]}"; do
    NUM_ACCESSES=$(( $q / 4 ))

    for i in "${CORES[@]}";
    do
        #iterations corresponding to current the query size
        iters=${ITERATIONS[$j]}
        taskset -c $i sudo ./memfill_and_gather \
            -q $q \
            -a $NUM_ACCESSES \
            -t ${REQUESTS[$qidx]} \
            -i $iters >> decomp_and_scatter_logs/core_${i}_querysize_${q}.log &
    done
    wait
    qidx=$((qidx+1))
done