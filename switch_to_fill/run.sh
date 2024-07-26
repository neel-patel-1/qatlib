#!/bin/bash
# sudo python3 scripts/accel_conf.py --load=configs/iaa-1n1d1e1w16q-s-n2.conf

CORES=(0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23)
for i in "${CORES[@]}";
do
    taskset -c $i sudo ./decomp_and_hash_multi_threaded 32 &
done