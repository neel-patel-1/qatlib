#!/bin/bash
log=default_affin_single_dev.log
grep -e threads -e ratio -e Throughput -e Direction $log  #|\
#  sed -E 's/\s\s+/,/g'  |\
#  awk -F ',' '{ for (i=1; i<NF; i++) {a[NR][i] = (a[NR][i]? a[NR][i] FS $i: $i)}} END{ for (i=1; i<=NR; i++){ print a[i][1] }  }'
# https://stackoverflow.com/questions/8257865/can-field-separator-in-awk-encompass-multiple-characters
# https://stackoverflow.com/questions/58315066/bash-shell-scripting-transpose-rows-and-columns
# https://unix.stackexchange.com/questions/399197/awk-f-delimit-by-space-greater-than-1