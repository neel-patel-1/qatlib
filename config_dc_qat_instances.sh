#!/bin/bash
[ "$( grep 'sym;dc' /etc/sysconfig/qat )" = "" ] && sudo sed -i 's/ServicesEnabled=.*/ServicesEnabled=sym;dc/' /etc/sysconfig/qat && sudo systemctl restart qat
