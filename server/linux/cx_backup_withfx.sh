#!/bin/sh

if [ -e /etc/SuSE-release ] && grep -qi "VERSION[ ]*=[ ]*10" /etc/SuSE-release ; then
    PHP="php5"
else
    PHP="php"
fi

$PHP /home/svsystems/admin/web/cx_config_backup.php "$1"
