#!/bin/bash

installdir=`grep "INSTALLATION_DIR=" /usr/local/.vx_version| awk -F "=" '{ print $2 }'`

CURRENTTIME=$(date +"%y-%m-%d-%H-%M-%S-%N")
DEVICEREFRESHLOGFILE="${installdir}/failover_data/devicerefresh_$CURRENTTIME.log"

function is_log_enabled
{
	if [ -f "${installdir}/failover_data/logenabled" ]
	then
		return 0
	else
		return 0
	fi
}

function update_debug_script_log
{
	echo "lsscsi output :" >> $DEVICEREFRESHLOGFILE
	echo "---------------" >> $DEVICEREFRESHLOGFILE
	lsscsi >> $DEVICEREFRESHLOGFILE
	echo "================" >> $DEVICEREFRESHLOGFILE
	echo "Multipath output :" >> $DEVICEREFRESHLOGFILE
	echo "------------------" >> $DEVICEREFRESHLOGFILE
	multipath -l >> $DEVICEREFRESHLOGFILE
	echo "=================" >> $DEVICEREFRESHLOGFILE
	echo "Test Volume info collector output :" >> $DEVICEREFRESHLOGFILE
	echo "-----------------------------------" >> $DEVICEREFRESHLOGFILE
	/usr/local/ASR/Vx/bin/AzureRcmCli --testvolumeinfocollector  >> $DEVICEREFRESHLOGFILE
	echo "===================================" >> $DEVICEREFRESHLOGFILE
}

# Updating more log for debugging (current state of the disks) before running the actuall business logic
if is_log_enabled; then
	date >> $DEVICEREFRESHLOGFILE
    echo "Before running the disk clean up :" >> $DEVICEREFRESHLOGFILE
	update_debug_script_log	
fi

# store arguments in a special array 
args=("$@") 
# get number of elements 
ELEMENTS=${#args[@]} 

date 

# echo each element in array
# for loop
for (( i=0;i<$ELEMENTS;i++)); do
    echo "refreshing device ${args[${i}]}" |& tee -a $DEVICEREFRESHLOGFILE
    echo "scsi remove-single-device ${args[${i}]}" > /proc/scsi/scsi 2>>$DEVICEREFRESHLOGFILE
    echo "scsi add-single-device ${args[${i}]}" > /proc/scsi/scsi 2>>$DEVICEREFRESHLOGFILE
done

# scan all hosts before adding it as device
echo "Running host scan in all controllers to get new devices." >> $DEVICEREFRESHLOGFILE
chmod +wx  -R /sys/class/scsi_host/host*/scan
for i in `echo /sys/class/scsi_host/host*/scan`
do
    echo "$i" >> $DEVICEREFRESHLOGFILE
    echo "- - -" > $i
done
sleep 20

multipath -r |& tee -a $DEVICEREFRESHLOGFILE

# Updating more log for debugging (current state of the disks) after running the actuall business logic
if is_log_enabled; then
    echo "After running the disk clean up : " >> $DEVICEREFRESHLOGFILE
	update_debug_script_log
	date >> $DEVICEREFRESHLOGFILE
fi
