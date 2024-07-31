#!/bin/bash

source configs/phys_core.sh

[ -z "$CORE" ] && echo "CORE is not set" && exit 1
pre_bytes=$(( 42 * 1024 ))
host_bytes=$(( 42 * 1024 ))
ax_bytes=$(( 0 ))
grep -v main three_phase_logs/core_${CORE}_querysize_${pre_bytes}_prebytes_${host_bytes}_axbytes_${ax_bytes}.log \
  | grep -v info \
  | grep -v RPS \
  | awk "\
    /Offload Mean/{printf(\"%s \", \$5 );} \
    /Wait Mean/{printf(\"%s \", \$5);  } \
    /Post Mean/{printf(\"%s <-BlockingOffload HostBytes ${host_bytes} AxBytes ${ax_bytes} \n\", \$5);} \
    /OffloadSwitchToFill/{printf(\"%s \", \$5);} \
    /YieldToResume/{printf(\"%s \", \$5);} \
    /PostProcessingSwitchToFill/{printf(\"%s <-SwitchToFill HostBytes ${host_bytes} AxBytes ${ax_bytes} \n\", \$5);} \
    "

host_bytes=$(( 24 * 1024 ))
ax_bytes=$(( 24 * 1024 ))

grep -v main three_phase_logs/core_${CORE}_querysize_${pre_bytes}_prebytes_${host_bytes}_axbytes_${ax_bytes}.log \
  | grep -v info \
  | grep -v RPS \
  | awk "\
    /Offload Mean/{printf(\"%s \", \$5 );} \
    /Wait Mean/{printf(\"%s \", \$5);  } \
    /Post Mean/{printf(\"%s <-BlockingOffload HostBytes ${host_bytes} AxBytes ${ax_bytes} \n\", \$5);} \
    /OffloadSwitchToFill/{printf(\"%s \", \$5);} \
    /YieldToResume/{printf(\"%s \", \$5);} \
    /PostProcessingSwitchToFill/{printf(\"%s <-SwitchToFill HostBytes ${host_bytes} AxBytes ${ax_bytes} \n\", \$5);} \
    "

host_bytes=$(( 0 ))
ax_bytes=$(( 42 * 1024 ))

grep -v main three_phase_logs/core_${CORE}_querysize_${pre_bytes}_prebytes_${host_bytes}_axbytes_${ax_bytes}.log \
  | grep -v info \
  | grep -v RPS \
  | awk "\
    /Offload Mean/{printf(\"%s \", \$5 );} \
    /Wait Mean/{printf(\"%s \", \$5);  } \
    /Post Mean/{printf(\"%s <-BlockingOffload HostBytes ${host_bytes} AxBytes ${ax_bytes} \n\", \$5);} \
    /OffloadSwitchToFill/{printf(\"%s \", \$5);} \
    /YieldToResume/{printf(\"%s \", \$5);} \
    /PostProcessingSwitchToFill/{printf(\"%s <-SwitchToFill HostBytes ${host_bytes} AxBytes ${ax_bytes} \n\", \$5);} \
    "