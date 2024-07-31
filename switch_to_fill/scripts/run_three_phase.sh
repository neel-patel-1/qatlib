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

# Initialize three_phase_logs
mkdir -p three_phase_logs

pre_bytes=$(( 42 * 1024 ))
host_bytes=$(( 42 * 1024 ))
ax_bytes=$(( 0 ))

# Host Only
echo -n "" > three_phase_logs/core_${CORE}_querysize_${pre_bytes}_prebytes_${host_bytes}_axbytes_${ax_bytes}.log
taskset -c $CORE sudo ./three_phase \
    -d $DSA_DEV_ID \
    -q ${pre_bytes} \
    -s ${ax_bytes} \
    -j ${host_bytes} \
    -t 100 \
    -i 100 >> three_phase_logs/core_${CORE}_querysize_${pre_bytes}_prebytes_${host_bytes}_axbytes_${ax_bytes}.log

pre_bytes=$(( 42 * 1024 ))
host_bytes=$(( 24 * 1024 ))
ax_bytes=$(( 24 * 1024 ))

# 50/50 Only
echo -n "" > three_phase_logs/core_${CORE}_querysize_${pre_bytes}_prebytes_${host_bytes}_axbytes_${ax_bytes}.log
taskset -c $CORE sudo ./three_phase \
    -d $DSA_DEV_ID \
    -q ${pre_bytes} \
    -s ${ax_bytes} \
    -j ${host_bytes} \
    -t 100 \
    -i 100 >> three_phase_logs/core_${CORE}_querysize_${pre_bytes}_prebytes_${host_bytes}_axbytes_${ax_bytes}.log

# Accelerator Only
ax_bytes=$(( 42 * 1024 ))
host_bytes=$(( 0 ))

echo -n "" > three_phase_logs/core_${CORE}_querysize_${pre_bytes}_prebytes_${host_bytes}_axbytes_${ax_bytes}.log
taskset -c $CORE sudo ./three_phase \
    -d $DSA_DEV_ID \
    -q ${pre_bytes} \
    -j ${host_bytes} \
    -s ${ax_bytes} \
    -t 100 \
    -i 100 >> three_phase_logs/core_${CORE}_querysize_${pre_bytes}_prebytes_${host_bytes}_axbytes_${ax_bytes}.log