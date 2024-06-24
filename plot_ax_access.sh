#!/bin/bash


make -j

echo | tee log


# blocking ax payload access (strided) on a 4kb buffer
sudo taskset -c 30  stdbuf -o0 ./compress_crc -a -t 0 -s 4096 -i 1000 -b | tee -a  log

# blocking host payload access (strided) on a 4kb buffer
sudo taskset -c 30  stdbuf -o0 ./compress_crc -t 0 -s 4096 -i 1000 -b | tee -a  log