#!/bin/bash

QUERY_SIZES=( 64 256 1024 4096 16384 $(( 64 * 1024 )) $(( 256 * 1024 )) $(( 1024 * 1024 )) )

for i in ${QUERY_SIZES[@]}; do
    gp_rps=$(grep GPCore logs/core_*_querysize_${i}.log | \
      awk '{sum+=$5} END{print sum}' )
    sw_to_fill_rps=$(grep SwitchToFill logs/core_*_querysize_${i}.log | \
      awk '{sum+=$5} END{print sum}' )
    blocking_rps=$(grep Blocking logs/core_*_querysize_${i}.log | \
      awk '{sum+=$5} END{print sum}' )
    echo $i $gp_rps $blocking_rps $sw_to_fill_rps
done