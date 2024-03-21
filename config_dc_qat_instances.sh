#!/bin/bash
[ "$( grep 'dc' /etc/sysconfig/qat )" = "" ] && sudo sed -i 's/ServicesEnabled=.*/ServicesEnabled=dc/' /etc/sysconfig/qat && sudo systemctl restart qat
