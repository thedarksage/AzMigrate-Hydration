#!/bin/bash

str=`date`
Date=`echo $str  |  sed -e 's/ /_/g'`

export LOGFILE=/MountDataDisk-$Date.log

osf=`mount -a`
echo "$osf" >> /MountDataDisk-$Date.log

echo "Checking if Lvs mounted successfully and are online...." >> $LOGFILE
lvdisplay >> /LVs-$Date.log
grep "lv3G" /LVs-$Date.log
if [ $? -eq 0 ]
then
echo "LV lv3G is online." >> $LOGFILE
	grep "lv4G" /LVs-$Date.log
	if [ $? -eq 0 ]
	then
		echo "LV lv3G is online." >> $LOGFILE
	else
		echo "LV lv4G did not come online" >> $LOGFILE
		echo "Failed"
		exit 1
	fi
else
	echo "LV lv3G did not come online" >> $LOGFILE
	echo "Failed"
	exit 1
fi

echo "Success"
exit 0