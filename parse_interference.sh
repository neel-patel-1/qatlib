#!/bin/bash


file=precached_vs_axoutput_filler_if.log
make -j
sudo taskset -c 30 stdbuf -o0 ./compress_crc   | tee $file

PATTERNS=(RANDOM)
CONFIGS=(Blocking-ReuseDistance  Precached-ReuseDistance  AxOutput-ReuseDistance)
echo | tee parsed.txt
for pattern in ${PATTERNS[@]}; do
    echo $pattern | tee -a parsed.txt
    echo -n "post_proc_buf_size fbuf_size " | tee -a parsed.txt
    for config in ${CONFIGS[@]}; do
      echo -n "$config " | tee -a parsed.txt
    done
    echo | tee -a parsed.txt
    grep  -e Precached-ReuseDistance $file | awk '{printf("%s %s\n", $2, $10); }' | tee f_sizes.log
    for config in ${CONFIGS[@]}; do
        grep $config $file  | awk '{printf("%s\n", $4); }' | tee ${config}_${pattern}.log
    done
    # paste Baseline_${pattern}.log AxOutput-LLC_${pattern}.log AxOutput-Prefetch_${pattern}.log | column -s $'\t' -t >> parsed.txt
    paste f_sizes.log $(for config in ${CONFIGS[@]}; do echo ${config}_${pattern}.log; done) | column -s $'\t' -t >> parsed.txt
done