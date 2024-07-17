#!/bin/bash

num_nodes=100
node_size=256

[ $(( num_nodes * node_size )) -gt $(( 2 * 1024 * 1024)) ] && echo "num_nodes * node_size > 2MB" && exit 1

sudo taskset -c 20,21 ./compress_crc  -s $(( $num_nodes * $node_size )) -t 0 -d -l 4 -i 100 -o 1 -t 432 -d | tee req_brkdown.log
awk '/offload_req/{print}' req_brkdown.log  | \
  awk '{subsum+=$6; kernsum+=$7; hashsum+=$11;} END{ print "offload_req_avg: " subsum/NR " kern_avg: " kernsum/NR " iterate: " hashsum/NR;}' \
  | tee accel_posting_list.txt

sudo taskset -c 20,21 ./compress_crc  -s $(( $num_nodes * $node_size )) -o 3 -d -l 4 -i 100 | tee cpu_req_brkdown.log
awk '/offload_req/{print}' cpu_req_brkdown.log  | \
  awk '{ kernsum+=$6; hashsum+=$10;} END{ print  "kern_avg: " kernsum/NR " iterate: " hashsum/NR;}' \
  | tee  cpu_posting_list.txt

sudo taskset -c 20,21 ./compress_crc  -s $(( $num_nodes * $node_size )) -t 0 -d -l 4 -i 100 -o 4 -t 432 -d | tee llc_merge.log
awk '/offload_req/{print}' ax_merge.log  | \
  awk '{ kernsum+=$6; hashsum+=$10;} END{ print  "kern_avg: " kernsum/NR " iterate: " hashsum/NR;}' \
  | tee  llc_merge.txt

echo "num_nodes: $num_nodes"
cat accel_posting_list.txt cpu_posting_list.txt ax_merge.txt