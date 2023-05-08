#!/bin/bash

# Assumption is that the script needs sudo permissions to run the commands
# Sciipt returns 0 if no FS corruption found, returns the fsck error code otherwise

DEVICE=$1
OPT_FORCE=$2

is_mounted()
{
	MNT=""
	MNT=`sudo lsblk $DEVICE | sed -n 2p | awk '{ print $7 }'`
	echo $MNT
	if [ -n "$MNT" ];
	then		
		echo "Device is MOUNTED"
		return 1
	else
		echo "Device is NOT MOUNTED"
		return 0
	fi		
}

if [ "$#" -lt "1" ] 
then
    echo "Usage: fsck.sh <DEVICE NAME> [-f] where DEVICE_NAME is the block device name such as /dev/sdb and -f is the force flag to force the unmount"
    exit 0
fi

SUB='/dev'
if [[ "$DEVICE" != "$SUB"* ]]; then
	echo "Invalid block device argument"
	exit 1	
fi

if [ "$#" -eq "2" -a "$OPT_FORCE" != "-f" ] 
then
	echo "Usage: fsck.sh <DEVICE NAME> [-f] where DEVICE_NAME is the block device name such as /dev/sdb and -f is the force flag to force the unmount"
	exit 0
fi

is_mounted
retval=$?
if [ "$retval" == 1 ]
then
	if [ -z "$OPT_FORCE" ]; then
		echo "Mounted and force not specified...exiting"
		exit 1
	elif [ "$OPT_FORCE" != "-f" ]; then
		echo "Mounted and incorrect force option specified. Use -f to force check...exiting"
		exit 1
	else
		echo "Mounted and correct force specified..doing unmount"
		sudo umount $DEVICE
		echo $?
	fi
fi

# at this point the device must be unmounted
#Replay the log by remounting the file system
#sudo mkdir /tmp/fsck_mount
#sudo mount $DEVICE /tmp/fsck_mount
#retval=$?
#if [ "$retval" != 0 ]
#then
#	echo "Mount errors..will proceed with fsck"
#else
#	sudo umount $DEVICE
#	retval=$?
#	if [ "$retval" != 0 ]
#	then
#		echo "Failed to unmount filesystem"
#		sudo rmdir /tmp/fsck_mount
#		exit 1
#	fi
#fi
#sudo rmdir /tmp/fsck_mount

# at this point the device must be unmounted
sudo fsck -n $DEVICE
retval=$?
if [ "$retval" == 0 ]
then
	echo "Filesystem is clean"
	exit 0
else
	echo "Filesystem is corrupted"
	exit $retval
fi

exit 0


