#!/bin/bash

source configs/phys_core.sh
source configs/devid.sh

# sudo python3 scripts/accel_conf.py --load=configs/iaa-1n1d8e1w128q-s-n2.conf

QUERY_SIZES=( 64    256  1024 4096 16384 65536 262144 1048576 )
ITERATIONS=(  10   10  10  10  10   10    10     10  )
REQUESTS=(    100 100 100 100 100  100   100    100 )

[ -z "$CORE" ] && echo "CORE is not set" && exit 1
[ -z "$DEVID" ] && echo "DEVID is not set" && exit 1
NUM_ACCESSES=10

# Initialize memzero_and_scatter_logs_full
mkdir -p memzero_and_scatter_logs_full
for q in "${QUERY_SIZES[@]}"; do
    for i in "${CORES[@]}";
    do
        echo -n > memzero_and_scatter_logs_full/core_${i}_querysize_${q}.log
    done
done

qidx=0
for q in "${QUERY_SIZES[@]}"; do
    NUM_ACCESSES=$(( $q / 4 ))

    #iterations corresponding to current the query size
    iters=${ITERATIONS[$j]}
    taskset -c $CORE sudo ./memfill_and_gather \
        -q $q \
        -a $NUM_ACCESSES \
        -t ${REQUESTS[$qidx]} \
        -d $DEVID \
        -i $iters >> memzero_and_scatter_logs_full/core_${i}_querysize_${q}.log &
    wait
    qidx=$((qidx+1))
done