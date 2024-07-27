#!/bin/bash

QUERY_SIZES=( 64    256  1024 4096 16384 65536 262144 1048576 )
CORE=20

for q in ${QUERY_SIZES[@]}; do
  grep -v main memfill_and_gather_logs/core_${CORE}_querysize_${q}.log \
    | grep -v info \
    | grep -v RPS \
    | awk '/Kernel1/{printf("%s", " 1.00001 ") } /Offload/{printf(" ");} {printf("%s ", $5);} /Kernel2/{printf("\n")  } /Post/{print " "}'
done