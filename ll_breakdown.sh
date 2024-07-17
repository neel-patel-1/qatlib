#!/bin/bash

sudo taskset -c 20,21 ./compress_crc   -i 100 -o 1 -t 432 -d | tee req_brkdown.log
awk '/offload_req/{print}' req_brkdown.log  | \
  awk '{subsum+=$6; kernsum+=$7; hashsum+=$11;} END{ print "offload_req_avg: " subsum/NR " kern_avg: " kernsum/NR " hash_avg: " hashsum/NR;}' \
  | tee accel_router.txt

sudo taskset -c 20,21 ./compress_crc  -s 160 -o 3 -i 1 -d -l 4 | tee cpu_req_brkdown.log
awk '/offload_req/{print}' cpu_req_brkdown.log  | \
  awk '{ kernsum+=$6; hashsum+=$10;} END{ print  "kern_avg: " kernsum/NR " hash_avg: " hashsum/NR;}' \
  | tee  cpu_router.txt

cat accel_router.txt cpu_router.txt