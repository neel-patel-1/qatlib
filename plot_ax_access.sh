#!/bin/bash


make -j

PATS=( 0 1 )
BUFSIZES=( $((32 * 1024)) $((1024 * 1024)) )

echo | tee log

for bufsize in ${BUFSIZES[@]}; do
  for pat in ${PATS[@]}; do
    # an accelerator can be hinted to inject data into llc
    sudo taskset -c 30  stdbuf -o0 ./compress_crc -a -t $pat -j -s $bufsize -i 1000 -b | tee -a  log

    # or it can take a hint to go into the dram
    sudo taskset -c 30  stdbuf -o0 ./compress_crc -a -t $pat -s $bufsize -i 1000 -b | tee -a  log

    # we can also prefetch into different levels of chierarchy
    sudo taskset -c 30  stdbuf -o0 ./compress_crc -a -t $pat -c -l 0 -s $bufsize -i 1000 -b -p 1 | tee -a  log

    # we can also prefetch into different levels of chierarchy
    sudo taskset -c 30  stdbuf -o0 ./compress_crc -a -t $pat -c -l 1 -s $bufsize -i 1000 -b -p 1 | tee -a  log
  done
done