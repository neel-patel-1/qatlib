#!/bin/bash

# sudo python3 scripts/accel_conf.py --load=configs/iaa-1n1d8e1w128q-s-n2.conf

source configs/phys_core.sh
source configs/devid.sh

QUERY_SIZES=( 64    256  1024 4096 16384 65536 262144 1048576 )
ITERATIONS=(  10   10  10  10  10   10    10     10  )
REQUESTS=(    100 100 100 100 100  100   100    100 )

[ -z "$CORE" ] && echo "CORE is not set" && exit 1
[ -z "$DEVID" ] && echo "DEVID is not set" && exit 1

# Initialize memfill_and_gather_logs
mkdir -p memfill_and_gather_logs
for q in "${QUERY_SIZES[@]}"; do
    echo -n > memfill_and_gather_logs/core_${CORE}_querysize_${q}.log
done

qidx=0
for q in "${QUERY_SIZES[@]}"; do
    NUM_ACCESSES=10
    floats=$(( $q / 32 ))
    [ $floats -lt $NUM_ACCESSES ] && NUM_ACCESSES=$floats

    #iterations corresponding to current the query size
    iters=${ITERATIONS[$j]}
    taskset -c $CORE sudo ./memfill_and_gather \
        -d $DEVID \
        -q $q \
        -a $NUM_ACCESSES \
        -t ${REQUESTS[$qidx]} \
        -i $iters >> memfill_and_gather_logs/core_${CORE}_querysize_${q}.log &
    wait
    qidx=$((qidx+1))
done