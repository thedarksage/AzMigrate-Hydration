#!/bin/bash

if [ $# -ne 2 ]; then
  echo "usage: $0 [RootPassword] [MountDirForDataDisks]"
  exit 1
fi

######### Start Global Variables############

RootPassword = $1

# Base path for data disk mount points
MNTDIR=$2

# adding defaults, plus no fail for safety as mount_options in fstab
MOUNT_OPTIONS="defaults,nofail"

LOGFILE="/var/log/initdisks.log"
######### End Global Variables############

TraceLogMessage()
{
    QUIET_MODE=FALSE
    EXIT=FALSE

    if [ $# -gt 0 ]
    then
        if [ "X$1" = "X-q" ]
        then
            QUIET_MODE=TRUE
			shift
        fi
		if [ "X$1" = "X-e" ]
        then
			EXIT=TRUE
            shift
        fi
    fi

    if [ $# -gt 0 ]
    then
        DATE_TIME=`date '+%m/%d/%Y %H:%M:%S'`

        if [ "${QUIET_MODE}" = "TRUE" ]
        then
            echo "${DATE_TIME} : $*" >> ${LOGFILE}
        else
            echo -e "$@"
            echo "${DATE_TIME} : $@ " >> ${LOGFILE} 2>&1
        fi

		if [ "${EXIT}" = "TRUE" ]
		then
			echo "Failed"
			exit 1
		fi
    fi
}

LoginAsRoot()
{
    rootUser="root"
    echo -e "$RootPassword\n$RootPassword" | passwd "$rootUser"

    if [ "$(whoami)" != "$rootUser" ]
    then
        TraceLogMessage "Logging in as $rootUser"
        sudo su -s "$RootPassword" >> $LOGFILE 2>&1
        if [ $? -eq 0 ]; then
            TraceLogMessage "Successfully logged in as $rootUser"
        else
            TraceLogMessage -e "Failed to login as $rootUser with exit status : $?"
        fi
    fi

    me="$(whoami)"
    TraceLogMessage "Logged in as $me"
}

# Checks if the disk is already having partitions or not
IsPartitioned() {
    OUTPUT=$(partx -s ${1} 2>&1)
    egrep "partition table does not contains usable partitions|failed to read partition table" <<< "${OUTPUT}" >/dev/null 2>&1
    if [ ${?} -eq 0 ]; then
		#UnPartitioned
        return 1
    else
		#Partitiooned
        return 0
    fi    
}

# Checks if the disk is already having filesystem or if the disk is of type (CD)ROM
HasFileSystem() {
	#DEVICE will be like /dev/sdb1
    DEVICE=${1}
    OUTPUT=$(file -L -s ${DEVICE})
	
	egrep "filesystem|DOS" <<< "${OUTPUT}" > /dev/null 2>&1
    return ${?}
}

# Looks for unpartitioned disks and returns
# the disks for partitioning
# and formatting if it does not have a sd?1 entry or
# if it does have an sd?1 entry and does not contain a filesystem
ScanForNewDisks() {
    declare -a RET
    DEVS=($(ls -1 /dev/sd*|egrep -v "[0-9]$"))
    for DEV in "${DEVS[@]}";
    do
        IsPartitioned "${DEV}"
        if [ ${?} -eq 0 ];
        then
            HasFileSystem "${DEV}1"
            if [ ${?} -ne 0 ];
            then
                RET+=" ${DEV}"
            fi
        else
            RET+=" ${DEV}"
        fi
    done
    echo "${RET}"
}

# Adds entries to fstab
AddToFstab() {
    UUID=${1}
    MOUNTPOINT=${2}
    grep "${UUID}" /etc/fstab >/dev/null 2>&1
    if [ ${?} -eq 0 ];
    then
        TraceLogMessage "Not adding ${UUID} to fstab again (it's already there!)"
    else
        LINE="UUID=\"${UUID}\"\t${MOUNTPOINT}\text4\t${MOUNT_OPTIONS}\t1 2"
        echo -e "${LINE}" >> /etc/fstab
    fi
}

# This function creates one (1) primary partition on the
# disk, using all available space
DoPartition() {
    _disk=${1}
    _type=${2}
    if [ -z "${_type}" ]; then
        # default to Linux partition type (ie, ext3/ext4/xfs)
        _type=83
    fi
    (echo n; echo p; echo 1; echo ; echo ; echo ${_type}; echo w) | fdisk "${_disk}"

    #
    # Use the bash-specific $PIPESTATUS to ensure we get the correct exit code
    # from fdisk and not from echo
    if [ ${PIPESTATUS[1]} -ne 0 ];
    then
        TraceLogMessage "An error occurred partitioning ${_disk}. Exiting" >&2
        exit 2
    fi
}

ScanPartitionFormat()
{
    TraceLogMessage "Begin scanning and formatting data disks"

    DISKS=($(ScanForNewDisks))

	if [ "${#DISKS}" -eq 0 ];
	then
	    TraceLogMessage "No unpartitioned disks without filesystems detected"
	    return
	fi
	TraceLogMessage "Disks are ${DISKS[@]}"

    IDX=0
	for DISK in "${DISKS[@]}";
	do
	    TraceLogMessage "Working on ${DISK}"

		IsPartitioned ${DISK}
	    if [ ${?} -ne 0 ];
	    then
	        TraceLogMessage "${DISK} is not partitioned, partitioning"
	        DoPartition ${DISK}
	    fi
		
	    PARTITION=$(fdisk -l ${DISK}|grep -A 1 Device|tail -n 1|awk '{print $1}')
	    HasFileSystem ${PARTITION}
	    if [ ${?} -ne 0 ];
	    then
	        TraceLogMessage "Creating filesystem on ${PARTITION}."
	        mkfs -j -t ext4 ${PARTITION}
	    fi
		
		[ -d "${MNTDIR}" ] || mkdir -p "${MNTDIR}"
		
	    MOUNTPOINT=${MNTDIR}"/disk"${IDX}
		TraceLogMessage "Next mount point appears to be ${MOUNTPOINT}"
	    
		[ -d "${MOUNTPOINT}" ] || mkdir -p "${MOUNTPOINT}"
	    
		read UUID FS_TYPE < <(blkid -u filesystem ${PARTITION}|awk -F "[= ]" '{print $3" "$5}'|tr -d "\"")
	    AddToFstab "${UUID}" "${MOUNTPOINT}"
	    
		TraceLogMessage "Mounting disk ${PARTITION} on ${MOUNTPOINT}"
	    mount "${MOUNTPOINT}"
		
		IDX=$(( ${IDX} + 1 ))
		TraceLogMessage "Successfully partitioned, formatted and added mount point for ${DISK}"
	done
}

#######################
# MAIN ENTRY POINT
#######################

TraceLogMessage "Params :"
TraceLogMessage "$0 $@"

LoginAsRoot
DISKS=$(ScanForNewDisks)
ScanPartitionFormat

TraceLogMessage "Initializing disks completed!!"
exit 0