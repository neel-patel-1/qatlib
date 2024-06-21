#!/bin/bash


make -j

POST_SIZES=( $(( 48 * 1024 )) $(( 2 * 1024 * 1024 )) )
PATS=( 0 1 )

output=fig4.log
iter=100
echo -n > $output
for pat in ${PATS[@]}; do
  for size in "${POST_SIZES[@]}"; do
    # sudo taskset -c 30  stdbuf -o0 ./compress_crc  -s $size  -b -t 0 | tee -a $output
    # sudo taskset -c 30  stdbuf -o0 ./compress_crc  -s $size  -t 0 | tee -a $output
    # sudo taskset -c 30  stdbuf -o0 ./compress_crc  -s $size  -p -t 0 | tee -a $output

    sudo taskset -c 30  stdbuf -o0 ./compress_crc  -s $size  -a -t ${pat} -i ${iter} | tee -a $output
    sudo taskset -c 30  stdbuf -o0 ./compress_crc  -s $size  -a -p -t ${pat} -i ${iter} | tee -a $output
    sudo taskset -c 30  stdbuf -o0 ./compress_crc  -s $size  -a -b -t ${pat} -i ${iter} | tee -a $output
  done
done