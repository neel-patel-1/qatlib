#!/bin/bash


make -j
# sudo perf stat -e L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,L1-dcache-store-misses taskset -c 30 ./compress_crc block | tee block.log
sudo taskset -c 30 ./compress_crc block | tee block.log
# # sudo perf stat -e L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,L1-dcache-store-misses taskset -c 30 ./compress_crc prefetch | tee prefetch.log
sudo taskset -c 30 ./compress_crc prefetch | tee prefetch.log

echo -n "Block "
grep RequestOnCPU block.log  | awk '{print $2}'
echo -n "Prefetch "
grep RequestOnCPU prefetch.log  | awk '{print $2}'