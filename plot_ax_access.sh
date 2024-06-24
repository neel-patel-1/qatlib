#!/bin/bash


make -j

echo | tee log

size=16384

# blocking ax payload access (strided) on a 4kb buffer
sudo taskset -c 30  stdbuf -o0 ./compress_crc -a -t 0 -s $size -i 1000 -b | tee -a  log

# blocking host payload access (strided) on a 4kb buffer
sudo taskset -c 30  stdbuf -o0 ./compress_crc -t 0 -s $size -i 1000 -b | tee -a  log

# blocking ax payload access (random) on a 4kb buffer
sudo taskset -c 30  stdbuf -o0 ./compress_crc -a -t 1 -s $size -i 1000 -b | tee -a  log

# blocking host payload access (random) on a 4kb buffer
sudo taskset -c 30  stdbuf -o0 ./compress_crc -t 1 -s $size -i 1000 -b | tee -a  log

grep Cycles log | awk '{print $20}'