#!/bin/bash

make -j
sudo ./compress_crc  | tee single-log.txt
echo "Configuration,BufferSize,Throughput(Offloads/sec)"
grep -A4 -e Stream single-log.txt |  awk '/Stream/{printf("%s,", $1);} /BufferSize/{printf("%s,",$2);} /OffloadsPerSec/{printf("%s\n", $2);}'

# echo "Configuration,BufferSize,Throughput(Offloads/sec)"
# grep -e 'BufferSize' -e 'CumulativeThr' -A1 -e '---' sw-hw-sw-chain.txt | grep -v -e '--' -e 'NumBuff' | awk '/Performance/{printf("%s%s,", $1,$2);} /BufferSize/{printf("%s,",$2);} /Cumulative/{printf("%s\n", $2);}'
#./plot_latency_hists.sh
