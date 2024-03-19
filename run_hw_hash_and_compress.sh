#!/bin/bash
sudo make samples -j
HW_HASH_COMPRESS_EXE=./chaining_sample
[ "$( grep 'dcc' /etc/sysconfig/qat )" = "" ] && sudo sed -i 's/ServicesEnabled=.*/ServicesEnabled=dcc/' /etc/sysconfig/qat && sudo systemctl restart qat
taskset -c 19 $HW_HASH_COMPRESS_EXE  0 0 > hw_hash_comp.log
