#!/bin/bash
SW_HASH_COMPRESS_EXE=./chaining_sample
[ "$( grep 'sym;dc' /etc/sysconfig/qat )" = "" ] && sudo sed -i 's/ServicesEnabled=.*/ServicesEnabled=sym;dc/' /etc/sysconfig/qat && sudo systemctl restart qat
$SW_HASH_COMPRESS_EXE  0 1 > sw_hash_comp.log
