#!/bin/bash


file=prefetch_overhead.log
make -j
sudo taskset -c 30  ./compress_crc   | tee $file

PATTERNS=(RANDOM LINEAR)
CONFIGS=(Blocking-NoPrefetch AxBuffer-NoPrefetch AxBuffer-Prefetched)
echo | tee parsed.txt
for pattern in ${PATTERNS[@]}; do
    echo $pattern | tee -a parsed.txt
    for config in ${CONFIGS[@]}; do
      for i in $(seq 1 3); do
        echo -n "$config " | tee -a parsed.txt
      done
    done
    echo | tee -a parsed.txt
    for config in ${CONFIGS[@]}; do
        grep $pattern $file | grep -e $config | awk '{printf("%s %s %s\n", $2, $6, $8); }' | tee ${config}_${pattern}.log
    done
    # paste Baseline_${pattern}.log AxOutput-LLC_${pattern}.log AxOutput-Prefetch_${pattern}.log | column -s $'\t' -t >> parsed.txt
    paste $(for config in ${CONFIGS[@]}; do echo ${config}_${pattern}.log; done) | column -s $'\t' -t >> parsed.txt
done