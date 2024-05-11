#!/bin/bash

sudo ./compress_crc  | tee sw-hw-sw-chain.txt
grep -e 'BufferSize' -e 'CumulativeThr' -A1 -e '---' sw-hw-sw-chain.txt | grep -v -e '--' -e 'NumBuff' | awk '/Performance/{printf("%s%s,", $1,$2);} /BufferSize/{printf("%s,",$2);} /Cumulative/{printf("%s\n", $2);}'
./plot_latency_hists.sh