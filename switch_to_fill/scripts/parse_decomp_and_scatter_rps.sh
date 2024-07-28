#!/bin/bash

QUERY_SIZES=( 64    256  1024 4096 16384 65536 262144 1048576 )
CORE=20
for i in ${QUERY_SIZES[@]}; do
    gp_rps=$(grep GPCore decomp_and_scatter_logs/core_${CORE}_querysize_${i}.log | \
      awk '{sum+=$5} END{print sum}' )
    sw_to_fill_rps=$(grep SwitchToFill decomp_and_scatter_logs/core_${CORE}_querysize_${i}.log | \
      awk '{sum+=$5} END{print sum}' )
    blocking_rps=$(grep Blocking decomp_and_scatter_logs/core_${CORE}_querysize_${i}.log | \
      awk '{sum+=$5} END{print sum}' )
    echo $i $gp_rps $blocking_rps $sw_to_fill_rps
done