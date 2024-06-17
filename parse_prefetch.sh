#!/bin/bash


file=prefetch_patterns.log
make -j
sudo taskset -c 30  ./compress_crc   | tee $file

PATTERNS=(RANDOM LINEAR)
CONFIGS=(HostBuffer-NoPrefetch HostBuffer-Prefetched)
echo | tee parsed.txt
# for pattern in ${PATTERNS[@]}; do
echo "config pattern hostbufsize prefetch_overhead req_process_cycles"
grep Blocking-NoPrefetch prefetch_patterns.log  | awk '{printf("%s %s %s 0 0 %s \n", $1 ,$4, $2, $6);}' | tee -a parsed.txt
grep Blocking-Prefetch prefetch_patterns.log  | awk '{printf("%s %s %s 0 0 %s \n", $1 ,$4, $2, $6);}' | tee -a parsed.txt
grep HostBuffer-NoPrefetch prefetch_patterns.log | awk '{printf("%s %s %s %s %s 0 \n", $1,  $4, $2, $8, $6);}' | tee -a parsed.txt
grep HostBuffer-Prefetched prefetch_patterns.log | awk '{printf("%s %s %s %s %s 0 \n", $1 , $4, $2, $8, $6);}' | tee -a parsed.txt
sort -k 2 -V -k 3 -k 1 parsed.txt