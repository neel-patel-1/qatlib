#!/bin/bash
# sudo python3 scripts/accel_conf.py --load=configs/iaa-1n1d1e1w16q-s-n2.conf
taskset -c 1 sudo ./decomp_and_hash_multi_threaded