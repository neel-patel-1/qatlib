#!/bin/bash

# sudo python3 scripts/accel_conf.py --load=configs/iaa-1n1d8e1w128q-s-n2.conf

source configs/phys_core.sh
source configs/devid.sh

# QUERY_SIZES=( 256  64  1024 4096 16384 65536 262144 1048576 )
# ITERATIONS=(  10   10  10  10  10   10    10     10  )
# REQUESTS=(    100 100 100 100 100  10   10    10 )
QUERY_SIZES=( $((42 * 1024)) )
ITERATIONS=(  100  )
REQUESTS=(    100 )

[ -z "$CORE" ] && echo "CORE is not set" && exit 1
[ -z "$DEVID" ] && echo "DEVID is not set" && exit 1
NUM_ACCESSES=10

# Initialize traverse_logs
mkdir -p traverse_logs
for q in "${QUERY_SIZES[@]}"; do
    echo -n > traverse_logs/core_${CORE}_querysize_${q}.log
done

qidx=0
for q in "${QUERY_SIZES[@]}"; do
    taskset -c $CORE sudo ./traverse \
        -d $DSA_DEV_ID \
        -q $q \
        -t ${REQUESTS[$qidx]} \
        -i ${ITERATIONS[$qidx]} >> traverse_logs/core_${CORE}_querysize_${q}.log
    qidx=$((qidx+1))
done