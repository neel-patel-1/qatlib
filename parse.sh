#!/bin/bash


file=prefetch_overhead${pattern}.log
make -j
sudo taskset -c 30  ./compress_crc   | tee $file


pattern=LINEAR
grep $pattern $file | grep -e Baseline | awk '{printf("%s %s\n", $2, $6); }' | tee baseline_rand${pattern}.log
grep $pattern $file | grep -e AxOutput-LLC | awk '{printf( "%s  \n", $8 + $6 ); }' | tee ctx_switch${pattern}.log
grep $pattern $file | grep -e AxOutput-Prefetch | awk '{printf( "%s %s \n", $8 , $6 ); }' | tee preftch_random${pattern}.log
paste baseline_rand${pattern}.log ctx_switch${pattern}.log preftch_random${pattern}.log | column -s $'\t' -t > parsed.txt

pattern=RANDOM
grep $pattern $file | grep -e Baseline | awk '{printf("%s %s\n", $2, $6); }' | tee baseline_rand${pattern}.log
grep $pattern $file | grep -e AxOutput-LLC | awk '{printf( "%s  \n", $8 + $6 ); }' | tee ctx_switch${pattern}.log
grep $pattern $file | grep -e AxOutput-Prefetch | awk '{printf( "%s %s \n", $8 , $6 ); }' | tee preftch_random${pattern}.log
paste baseline_rand${pattern}.log ctx_switch${pattern}.log preftch_random${pattern}.log | column -s $'\t' -t >> parsed.txt
