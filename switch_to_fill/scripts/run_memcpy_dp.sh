#!/bin/bash

# sudo python3 scripts/accel_conf.py --load=configs/iaa-1n1d8e1w128q-s-n2.conf

QUERY_SIZES=( 64    256  1024 4096 16384 65536 262144 1048576 )
ITERATIONS=(  100   100  100  100  100   10    10     10  )
REQUESTS=(    10000 1000 1000 1000 1000  100   100    100 )

CORES=( 1 )

# Initialize memcpy_dp_logs
mkdir -p memcpy_dp_logs
for q in "${QUERY_SIZES[@]}"; do
    for i in "${CORES[@]}";
    do
        echo -n > memcpy_dp_logs/core_${i}_querysize_${q}.log
    done
done

qidx=0
for q in "${QUERY_SIZES[@]}"; do
    for i in "${CORES[@]}";
    do
        #iterations corresponding to current the query size
        iters=${ITERATIONS[$j]}
        taskset -c $i sudo ./mlp \
            -q $q \
            -t ${REQUESTS[$qidx]} \
            -i $iters >> memcpy_dp_logs/core_${i}_querysize_${q}.log &
    done
    wait
    qidx=$((qidx+1))
done