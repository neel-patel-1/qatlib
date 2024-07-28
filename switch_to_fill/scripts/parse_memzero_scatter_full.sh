#!/bin/bash

source configs/phys_core.sh

QUERY_SIZES=( 64    256  1024 4096 16384 65536 262144 1048576 )
[ -z "$CORE" ] && echo "CORE is not set" && exit 1

for q in ${QUERY_SIZES[@]}; do
  grep -v main memfill_and_gather_logs_full/core_${CORE}_querysize_${q}.log \
    | grep -v info \
    | grep -v RPS \
    | awk '/Kernel1/{printf("%s", " 0 0 ") } /Offload/{printf(" ");} {printf("%s ", $5);} /Kernel2/{printf("\n")  } /Wait/{printf("%s", "0 ");} /Post/{print " "}'
done