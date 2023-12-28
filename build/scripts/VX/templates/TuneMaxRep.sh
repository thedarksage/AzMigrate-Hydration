#!/bin/sh

echo
echo "========================================" >> /var/log/TuneMaxRep.log
echo `date` >> /var/log/TuneMaxRep.log

HomeLun=`mount |grep -w "/home" |awk '{print $1}'`
echo "HomeLun : ${HomeLun}" >> /var/log/TuneMaxRep.log
if [ -n "${HomeLun}" ];then
        blockdev --setra 65536 $HomeLun #32MB  readahead.
	mount -o noatime,nodiratime,remount,rw /home
fi

echo "cat dirty_background_ratio, default is 10" >> /var/log/TuneMaxRep.log
cat /proc/sys/vm/dirty_background_ratio >> /var/log/TuneMaxRep.log

echo "cat dirty_ratio, default is 40" >> /var/log/TuneMaxRep.log
cat /proc/sys/vm/dirty_ratio >> /var/log/TuneMaxRep.log

echo "cat dirty_writeback_centisecs, default is 499" >> /var/log/TuneMaxRep.log
cat /proc/sys/vm/dirty_writeback_centisecs >> /var/log/TuneMaxRep.log

echo "cat dirty_expire_centisecs, default is 2999" >> /var/log/TuneMaxRep.log
cat /proc/sys/vm/dirty_expire_centisecs >> /var/log/TuneMaxRep.log

echo "Tuning Linux VM parameters..." >> /var/log/TuneMaxRep.log
echo "setting dirty_background_ratio to 100" >> /var/log/TuneMaxRep.log
echo 100 > /proc/sys/vm/dirty_background_ratio 

echo "setting dirty_ratio to 99 on Source" >> /var/log/TuneMaxRep.log
echo 99 > /proc/sys/vm/dirty_ratio

echo "setting dirty_writeback_centisecs to 499" >> /var/log/TuneMaxRep.log
echo 499 > /proc/sys/vm/dirty_writeback_centisecs

echo "setting dirty_expire_centisecs to 48000" >> /var/log/TuneMaxRep.log
echo 48000 > /proc/sys/vm/dirty_expire_centisecs

echo "========================================" >> /var/log/TuneMaxRep.log
