#!/bin/bash

installdir=`grep "INSTALLATION_DIR=" /usr/local/.vx_version| awk -F "=" '{ print $2 }'`

CURRENTTIME=$(date +"%y-%m-%d-%H-%M-%S-%N")
CLEANUPLOGFILE="${installdir}/failover_data/linuxcleanup_$CURRENTTIME.log"
MULTIPATHDEVICEFILE="/tmp/multipathdevices"
STANDARDDEVICEFILE="/tmp/standarddevices"

###Removing temporary files created in preivous run###
rm -f $MULTIPATHDEVICEFILE
rm -f $STANDARDDEVICEFILE

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
	echo "lsscsi output :" >> $CLEANUPLOGFILE
	echo "---------------" >> $CLEANUPLOGFILE
	lsscsi >> $CLEANUPLOGFILE
	echo "================" >> $CLEANUPLOGFILE
	echo "Multipath output :" >> $CLEANUPLOGFILE
	echo "------------------" >> $CLEANUPLOGFILE
	multipath -l >> $CLEANUPLOGFILE
	echo "=================" >> $CLEANUPLOGFILE
	echo "Test Volume info collector output :" >> $CLEANUPLOGFILE
	echo "-----------------------------------" >> $CLEANUPLOGFILE
	/usr/local/ASR/Vx/bin/AzureRcmCli --testvolumeinfocollector >> $CLEANUPLOGFILE
	echo "===================================" >> $CLEANUPLOGFILE
}

# Updating more log for debugging (current state of the disks) before running the actuall business logic
if is_log_enabled; then
	date >> $CLEANUPLOGFILE
    echo "Before running the disk clean up :" >> $CLEANUPLOGFILE
	update_debug_script_log	
fi

if [ $# -gt 0 ]
then
        ###This portion of script will be calling before detaching dummy disks###
        for i in `echo $*`
        do
                device=$(echo "$i" | awk '{print tolower($0)}')
                echo ${device} >>$MULTIPATHDEVICEFILE
                multipath -l|grep -A 3 -w ${device}| tail -1 |awk '{ print $2 }' >>$STANDARDDEVICEFILE
        done
		
		echo "Multipath devices received for clean up: " >> $CLEANUPLOGFILE
		cat $MULTIPATHDEVICEFILE >> $CLEANUPLOGFILE
		
		echo "H:C:T:L vaules to perform remove single device for cleanup: " >> $CLEANUPLOGFILE
		cat $STANDARDDEVICEFILE >> $CLEANUPLOGFILE
		
        ####Removing any failed persistent entries from multipath control######
        cat $MULTIPATHDEVICEFILE |while read  line
        do
		        echo "Running dmsetup remove on : $line" >> $CLEANUPLOGFILE
                dmsetup remove $line
                if [ $? -eq 0 ]
                then
					echo "$line is removed successfully from multipath control" |& tee -a $CLEANUPLOGFILE
                fi
        done

        sleep 2

        ####Removing any failed persistent entries from OS control######
        cat $STANDARDDEVICEFILE |while read x
        do
		        echo "Running remove-single-device on device : $x" >> $CLEANUPLOGFILE
                echo "scsi remove-single-device $x" > /proc/scsi/scsi 2>>$CLEANUPLOGFILE
			    echo "$x is removed successfully from O.S control"
                sleep 1
        done

        sleep 2
		
else
        ###This portion of script will be calling before adding dummy disks###
        multipath -l|grep -B 4 "failed undef"|grep "Virtual disk"|awk '{ print $1 }' >>$MULTIPATHDEVICEFILE
        multipath -l|grep "failed undef"|awk '{ print $2 }' >>$STANDARDDEVICEFILE
		
		echo "Failed Multipath devices list: " >> $CLEANUPLOGFILE
		cat $MULTIPATHDEVICEFILE >> $CLEANUPLOGFILE
		
		echo "H:C:T:L vaules of failed multipath devices: " >> $CLEANUPLOGFILE
		cat $STANDARDDEVICEFILE >> $CLEANUPLOGFILE
		
        ####Removing any failed persistent entries from multipath control######
        cat $MULTIPATHDEVICEFILE |while read  line
        do
		        echo "Running dmsetup remove on : $line" >> $CLEANUPLOGFILE
                dmsetup remove $line |& tee -a $CLEANUPLOGFILE
        done

        sleep 2

        ####Removing any failed persistent entries from OS control######
        cat $STANDARDDEVICEFILE |while read x
        do
		        echo "Running remove-single-device on device : $x " >>$CLEANUPLOGFILE
                echo "scsi remove-single-device $x" > /proc/scsi/scsi 2>>$CLEANUPLOGFILE
                sleep 1
        done

        sleep 2
		
	    ##Bug:29952. Replacing "?" with "*" to support controller number more than 9
		echo "Running host scan in all controllers to get new devices." >> $CLEANUPLOGFILE
        chmod +wx  -R /sys/class/scsi_host/host*/scan
        for i in `echo /sys/class/scsi_host/host*/scan`
        do
		        echo "$i" >> $CLEANUPLOGFILE
                echo "- - -" > $i
        done

		sleep 20
		
		echo "Running lscsi, for disk type..." >> $CLEANUPLOGFILE 
        lsscsi|awk '$2=="disk" { print $0 }' >/tmp/testfile
		
		echo "lsscsi output: " >> $CLEANUPLOGFILE 
		cat /tmp/testfile >> $CLEANUPLOGFILE
		
		echo "@Size,Controller details:" >> $CLEANUPLOGFILE

        cat /tmp/testfile|while read i
        do
                control=`echo $i|awk '{ print $1 }'|sed 's/\[//g'|awk -F':' '{ print $1 ":" $3 }'`
                disk=`echo $i|awk '{ print $NF }'`
                size=`fdisk -s $disk`
                echo -n @${size},${control} |& tee -a $CLEANUPLOGFILE
        done
fi

# Updating more log for debugging (current state of the disks) after running the actuall business logic

if is_log_enabled; then
    echo "After running the disk clean up : " >> $CLEANUPLOGFILE
	update_debug_script_log
	date >> $CLEANUPLOGFILE
fi
