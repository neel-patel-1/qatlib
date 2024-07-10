#!/bin/bash


SIZES=( $(( 1 * 1024)) $(( 2 * 1024)) $(( 4 * 1024)) )
SIZES+=( $(( 8 * 1024)) $(( 16 * 1024)) )
SIZES=( $((32 * 1024)) $(( 64 * 1024)) $(( 128 * 1024)) )
SIZES+=( $(( 256 * 1024)) $(( 512 * 1024)) )
SIZES+=( $(( 1024 * 1024)) $(( 2 * 1024 * 1024)) )

echo | tee log
for size in "${SIZES[@]}"; do
  sudo ./compress_crc  -y -i 100 -s $size -f $size | tee -a log
  sudo ./compress_crc  -i 100 -s $size -f $size | tee -a log
done