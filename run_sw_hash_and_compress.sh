#!/bin/bash
SW_HASH_COMPRESS_EXE=./chaining_sample
sudo make samples -j
[ "$( grep 'sym;dc' /etc/sysconfig/qat )" = "" ] && sudo sed -i 's/ServicesEnabled=.*/ServicesEnabled=sym;dc/' /etc/sysconfig/qat && sudo systemctl restart qat
taskset -c 19 $SW_HASH_COMPRESS_EXE  0 1 > sw_hash_comp.log
