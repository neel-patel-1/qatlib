#!/bin/bash

# sudo python3 scripts/accel_conf.py --load=configs/iaa-1n1d8e1w128q-s-n2.conf

source configs/phys_core.sh
source configs/devid.sh

QUERY_SIZES=( 256  64  1024 4096 16384 65536 262144 1048576 )
ITERATIONS=(  10   10  10  10  10   10    10     10  )
REQUESTS=(    100 100 100 100 100  10   10    10 )

[ -z "$CORE" ] && echo "CORE is not set" && exit 1
[ -z "$DEVID" ] && echo "DEVID is not set" && exit 1
NUM_ACCESSES=10

# Initialize decomp_and_scatter_logs
mkdir -p decomp_and_scatter_logs
for q in "${QUERY_SIZES[@]}"; do
    echo -n > decomp_and_scatter_logs/core_${CORE}_querysize_${q}.log
done

qidx=0
for q in "${QUERY_SIZES[@]}"; do
    NUM_ACCESSES=$(( $q / 4 ))

    #iterations corresponding to current the query size
    iters=${ITERATIONS[$j]}
    taskset -c $CORE sudo ./decomp_and_scatter \
        -d $DEVID \
        -q $q \
        -a $NUM_ACCESSES \
        -t ${REQUESTS[$qidx]} \
        -i $iters >> decomp_and_scatter_logs/core_${CORE}_querysize_${q}.log &
    wait
    qidx=$((qidx+1))
done