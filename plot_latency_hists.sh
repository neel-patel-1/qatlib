#!/bin/bash

FILES=( `ls -1 *bufsize*` )

for file in  ${FILES[@]}; do
  # Create a gnuplot script
  echo "set terminal png
  set output '$( echo ${file} | sed 's/\.txt//g').png'
  set style data histogram
  set style histogram cluster gap 1
  set style fill solid
  set boxwidth 0.9
  set xtic rotate by -45 scale 0
  plot '${file}' using 1:xtic(1) with histogram" > plot.gp

  # Run gnuplot
  gnuplot plot.gp
done