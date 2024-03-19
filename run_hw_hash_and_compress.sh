#!/bin/bash
HW_HASH_COMPRESS_EXE=./chaining_sample
[ "$( grep 'dcc' /etc/sysconfig/qat )" = "" ] && sudo sed -i 's/ServicesEnabled=.*/ServicesEnabled=dcc/' /etc/sysconfig/qat && sudo systemctl restart qat
$HW_HASH_COMPRESS_EXE  0 0
