#!/bin/bash


make -j
bufsize=$(( 32 * 1024 ))

# an accelerator can be hinted to inject data into llc
sudo taskset -c 30  stdbuf -o0 ./compress_crc -a -j -s $bufsize -i 1000 -b

# or it can take a hint to go into the dram
sudo taskset -c 30  stdbuf -o0 ./compress_crc -a -s $bufsize -i 1000 -b

# we can also prefetch into different levels of chierarchy
sudo taskset -c 30  stdbuf -o0 ./compress_crc -a -c -l 0 -s $bufsize -i 1000 -b -p 1

# we can also prefetch into different levels of chierarchy
sudo taskset -c 30  stdbuf -o0 ./compress_crc -a -c -l 1 -s $bufsize -i 1000 -b -p 1

