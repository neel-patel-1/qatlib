#!/bin/bash

# sudo python3 scripts/accel_conf.py --load=configs/iaa-1n1d8e1w128q-s-n2.conf

RUN_TYPES=( 0 1 2 )
QUERY_SIZES=( 64 256 1024 4096 16384 $(( 64 * 1024 )) $(( 256 * 1024 )) $(( 1024 * 1024 )) )
ITERATIONS=( 1000 1000 1000 1000 100  10 10 10  )
REQUESTS=( 1000 1000 1000 1000 100 10 10 10 )

CORES=(0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 )

# Initialize logs
mkdir -p logs
for q in "${QUERY_SIZES[@]}"; do
    for i in "${CORES[@]}";
    do
        echo -n > logs/core_${i}_querysize_${q}.log
    done
done

qidx=0
for q in "${QUERY_SIZES[@]}"; do
    for j in "${RUN_TYPES[@]}"; do
        for i in "${CORES[@]}";
        do
            #iterations corresponding to current the query size
            iters=${ITERATIONS[$j]}
            taskset -c $i sudo ./decomp_and_hash_multi_threaded \
                -r $j \
                -t ${ITERATIONS[$qidx]} \
                -q $q \
                -t ${REQUESTS[$qidx]} \
                -s $(( 10 )) \
                -i $iters >> logs/core_${i}_querysize_${q}.log &
        done
        wait
    done
    qidx=$((qidx+1))
done