#!/bin/bash

if [ $# -ne 2 ]; then
  echo "usage: $0 [RootPassword] [MountDirForDataDisks]"
  exit 1
fi

######### Start Global Variables############
DateStr=`date`
Date=`echo $DateStr  |  sed -e 's/ /_/g'`

LogFile="/FileSystemHealthCheck-$Date.log"

RootPassword=$1

# Base path for data disk mount points
MNTDIR=$2

# Initialize an associative array to store device names and their corresponding mount points
declare -A device_mount_map

LoginAsRoot()
{
  rootUser="root"

  if [ "$(whoami)" != "$rootUser" ]
  then
      echo "Logging in as $rootUser" >> $LogFile
      sudo su -s "$RootPassword" >> $LOGFILE 2>&1
      if [ $? -eq 0 ]; then
          echo "Successfully logged in as $rootUser" >> $LogFile
      else
          echo "Failed to login as $rootUser with exit status : $?" >> $LogFile
    exit 1
      fi
  fi

  me="$(whoami)"
  echo "Logged in as $me" >> $LogFile
}

GetDataDisks()
{
  # Find the disks matching the mount point directory
  while read -r line; do
    echo "df line entry : $line" >> $LogFile
    if [[ "$line" == *"$MNTDIR"* ]]; then
      disk=$(echo "$line" | awk '{print $1}')
      mount_point=$(echo "$line" | awk '{print $6}')
      echo "disk = $disk, mount point = $mount_point" >> $LogFile
      device_mount_map[$disk]=$mount_point
    fi
  done < <(df -h)
}

RunFSHealthCheck()
{
  # Check if any matching data disks are currently mounted, unmount them if necessary, run fsck on them and mount back the disks
  for device in "${!device_mount_map[@]}"; do
    mount_point="${device_mount_map[$device]}"

    echo "mount point : $mount_point, device : $device" >> $LogFile
    # UnMount the disks if they are mounted
    if grep -qs "$device" /proc/mounts; then
      echo "Unmounting $device..." >> $LogFile
      umount "$mount_point"
      exit_code=$?
      if [ $exit_code -ne 0 ]; then
        echo "Error: UnMount of $mount_point failed on $device with exit code $exit_code" >> $LogFile
        exit $exit_code
      fi
    fi

    # Run fsck on the disk
    echo "Running fsck on $device" >> $LogFile
    fsck -y "$device"
    exit_code=$?
    if [ $exit_code -ne 0 ]; then
      echo "File System Corruption detected on $device" >> $LogFile
      echo "Error: fsck failed on $device with exit code $exit_code" >> $LogFile
      exit $exit_code
    else
      echo "Success, file system is healthy" >> $LogFile
    fi

    # Mount the disks
    echo "Mounting $device to ${device_mount_map[$device]}..." >> $LogFile
    mount "$device" "${device_mount_map[$device]}" >> $LogFile
    exit_code=$?
    if [ $exit_code -ne 0 ]; then
      echo "Error: Mount of $mount_point failed on $device with exit code $exit_code" >> $LogFile
      exit $exit_code
    fi
  done
}

#######################
# MAIN ENTRY POINT
#######################

echo "Params :"
echo "$0 $@"

LoginAsRoot
GetDataDisks
RunFSHealthCheck

echo "Success"
exit 0