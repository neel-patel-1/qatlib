#!/bin/bash


make -j
sudo taskset -c 30  stdbuf -o0 ./compress_crc  -s $(( 48 * 1024 ))  -b -t 0 | tee 48kb_linear_blocking
sudo taskset -c 30  stdbuf -o0 ./compress_crc  -s $(( 48 * 1024 ))  -t 0 | tee 48kb_linear
sudo taskset -c 30  stdbuf -o0 ./compress_crc  -s $(( 48 * 1024 ))  -p -t 0 | tee 48kb_linear_prefetch
