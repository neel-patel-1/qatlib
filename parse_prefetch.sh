#!/bin/bash


file=prefetch_patterns.log
[ "$(sudo accel-config list | grep block | awk -F: '{print $2}' | tr -d , )" != 1 ] && echo "enable bof via \"sudo ./setup_dsa bof_swq_dev2.cfg\" " && exit -1

make -j
sudo taskset -c 30  ./compress_crc   | tee $file

PATTERNS=(RANDOM LINEAR)
CONFIGS=(Blocking-Poll Blocking-Umwait AxBuffer-NoPrefetch AxBuffer-Prefetched)
echo | tee parsed.txt
# for pattern in ${PATTERNS[@]}; do
echo "config pattern hostbufsize prefetch_overhead req_process_cycles" | tee -a parsed.txt
  grep Blocking-Umwait prefetch_patterns.log | awk '{printf("%s %s %s 0 0 %s \n", $1, $4, $2, $6);}' | tee -a parsed.txt
  grep Blocking-Poll prefetch_patterns.log | awk '{printf("%s %s %s 0 0 %s \n", $1, $4, $2, $6);}' | tee -a parsed.txt
  grep  AxBuffer-NoPrefetch prefetch_patterns.log | awk '{printf("%s %s %s %s %s 0 \n", $1,  $4, $2, $8, $6);}' | tee -a parsed.txt
  grep AxBuffer-Prefetched prefetch_patterns.log | awk '{printf("%s %s %s %s %s 0 \n", $1 , $4, $2, $8, $6);}' | tee -a parsed.txt
# done