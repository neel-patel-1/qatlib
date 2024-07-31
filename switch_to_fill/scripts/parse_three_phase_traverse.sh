#!/bin/bash

source configs/phys_core.sh

QUERY_SIZES=( 64    256  1024 4096 16384 $((42 * 1024)) 65536 262144 1048576 )
[ -z "$CORE" ] && echo "CORE is not set" && exit 1

for q in ${QUERY_SIZES[@]}; do
  grep -v main traverse_logs/core_${CORE}_querysize_${q}.log \
    | grep -v info \
    | grep -v RPS \
    | awk "BEGIN{printf(\"${q} \");}\
      /Offload Mean/{printf(\"BlockingOffload %s \", \$5 );} \
      /Wait Mean/{printf(\"%s \", \$5);  } \
      /Post Mean/{printf(\"%s\n\", \$5);} \
      /OffloadSwitchToFill/{printf(\" SwitchToFill %s \", \$5);} \
      /YieldToResume/{printf(\"%s \", \$5);} \
      /PostProcessingSwitchToFill/{printf(\"%s\n\", \$5);} \
      "
    echo
done