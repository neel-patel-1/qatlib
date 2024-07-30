#!/bin/bash

# sudo python3 scripts/accel_conf.py --load=configs/iaa-1n1d8e1w128q-s-n2.conf
source configs/phys_core.sh
source configs/devid.sh

QUERY_SIZES=( 64    256  1024 4096 16384 65536 262144 1048576 )
ITERATIONS=(  10   10  10  10  10   10    10     10  )
REQUESTS=(    100 100 100 100 100  100   100    100 )

[ -z "$CORE" ] && echo "CORE is not set" && exit 1
[ -z "$DEVID" ] && echo "DEVID is not set" && exit 1

# Initialize memcpy_dp_logs
mkdir -p memcpy_dp_logs
for q in "${QUERY_SIZES[@]}"; do
    echo -n > memcpy_dp_logs/core_${CORE}_querysize_${q}.log
done

qidx=0
for q in "${QUERY_SIZES[@]}"; do

    #iterations corresponding to current the query size
    iters=${ITERATIONS[$j]}
    taskset -c $CORE sudo ./mlp \
        -d $DEVID \
        -q $q \
        -t ${REQUESTS[$qidx]} \
        -i $iters >> memcpy_dp_logs/core_${CORE}_querysize_${q}.log &
    wait
    qidx=$((qidx+1))
done