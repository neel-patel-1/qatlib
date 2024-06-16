#!/bin/bash

file=linear_prefetch_overhead.log

grep  Baseline $file | awk '{printf("%s %s\n", $2, $6); }' | tee baseline_rand.log
grep AxOutput-LLC $file | awk '{printf( "%s  \n", $8 + $6 ); }' | tee ctx_switch.log
grep AxOutput-Prefetch $file | awk '{printf( "%s %s \n", $8 , $6 ); }' | tee preftch_random.log
paste baseline_rand.log ctx_switch.log preftch_random.log | column -s $'\t' -t
