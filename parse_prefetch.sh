#!/bin/bash


file=prefetch_patterns.log
make -j
sudo taskset -c 30  ./compress_crc   | tee $file

PATTERNS=(RANDOM LINEAR)
CONFIGS=(HostBuffer-NoPrefetch HostBuffer-Prefetched)
echo | tee parsed.txt
for pattern in ${PATTERNS[@]}; do
  echo $pattern | tee -a parsed.txt
  echo -n "HostWSS FillerWSS," | tee -a parsed.txt
  for config in ${CONFIGS[@]}; do
    echo -n "$config-PrefetchCycles $config-PostProcessCycles" | tee -a parsed.txt
  done

  grep  -e HostBuffer-Prefetched $file  | grep -e $pattern | awk '{printf("%s %s\n", $2, $10); }' | tee f_sizes.log
  echo | tee -a parsed.txt
  for config in ${CONFIGS[@]}; do
      grep $pattern $file | grep -e $config | awk '{printf("%s %s\n", $8, $6); }' | tee ${config}_${pattern}.log
  done
  # paste Baseline_${pattern}.log AxOutput-LLC_${pattern}.log AxOutput-Prefetch_${pattern}.log | column -s $'\t' -t >> parsed.txt
  paste f_sizes.log $(for config in ${CONFIGS[@]}; do echo ${config}_${pattern}.log; done) | column -s $'\t' -t >> parsed.txt
done