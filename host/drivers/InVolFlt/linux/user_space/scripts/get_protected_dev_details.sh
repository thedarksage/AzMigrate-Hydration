#!/bin/bash
INMBINDIR='/etc/vxagent/bin/'
INMUSRDIR='/etc/vxagent/usr/'
VXPROTDEVMAPPATH=$INMUSRDIR/vxprotdevdetails.dat
VXPERSISTPLATFORMPATH=$INMUSRDIR/vxplatformtype.dat
INMDMIT=$INMBINDIR/inm_dmit
INMSCSIID=$INMBINDIR/inm_scsi_id
VMBUSDEVDIR="/sys/bus/vmbus/devices/"
IDE_CLASS_ID="{32412632-86cb-44a2-9b5c-50d1417354f5}"
BOOT_SCSI_CLASS_ID="{f8b3781a-1e82-4818-a1c3-63d806ec15bb}"
DATA_SCSI_CLASS_ID="{f8b3781b-1e82-4818-a1c3-63d806ec15bb}"
BOOT_DEV_ID="{00000000-0000-8899-0000-000000000000}"
TEMP_DEV_ID="{00000000-0001-8899-0000-000000000000}"
AZURE_ASSET_TAG="7783-7084-3265-9085-8269-3286-77"
ROOTDISKDEFAULTPERSNAME="RootDisk"
PLATFORMTYPE_VMWARE="VmWare"

EBADF=9
ENODEV=19
ENOTRECOVERABLE=131

gHypervisorType=""
gBootDevice=""
gTempDevice=""
gDataDiskHostId=
declare -a gDataDevices
declare -a gDevicesMap
gVxPlatformType=""
gCsType=""
gBekVolume=""
gAzureVmGen=""

Usage()
{
    echo "Usage: "
    echo "$0 -p <device>  - to return a persistent name for a device"
    echo "$0 -d <persistentname>  - to return a device for a persistent"
    echo " "
    exit 1
}

GetBootDevice()
{
    for vmbusdev in `ls $VMBUSDEVDIR`
    do

        deviceid=(`cat ${VMBUSDEVDIR}/${vmbusdev}/device_id | sed 's/[{}-]/ /g' | awk '{print $1,$2}'`)
        if [ "${deviceid[0]}" != "00000000" -o \
             "${deviceid[1]}" != "0000" ]; then
            continue
        fi

        if [ -d ${VMBUSDEVDIR}/${vmbusdev}/host*/target*/[0-9]*:*/block ]; then
            boot_dev=`ls ${VMBUSDEVDIR}/${vmbusdev}/host*/target*/[0-9]*:*/block/ 2> /dev/null `
        else
            boot_dev=`ls ${VMBUSDEVDIR}/${vmbusdev}/host*/target*/[0-9]*:*/| grep block | awk -F: '{ print $2}'  2> /dev/null `
        fi

        if [ ! -z "$boot_dev" ]; then
            gBootDevice="/dev/"${boot_dev}
            echo "Boot device: ${gBootDevice}"
            break
        fi
    done
}

GetTempDevice()
{
    for vmbusdev in `ls $VMBUSDEVDIR`
    do

        deviceid=(`cat ${VMBUSDEVDIR}/${vmbusdev}/device_id | sed 's/[{}-]/ /g' | awk '{print $1,$2}'`)
        if [ "${deviceid[0]}" != "00000000" -o \
             "${deviceid[1]}" != "0001" ]; then
            continue
        fi

        if [ -d ${VMBUSDEVDIR}/${vmbusdev}/host*/target*/[0-9]*:*/block ]; then
            temp_dev=`ls ${VMBUSDEVDIR}/${vmbusdev}/host*/target*/[0-9]*:*/block/ 2> /dev/null `
        else
            temp_dev=`ls ${VMBUSDEVDIR}/${vmbusdev}/host*/target*/[0-9]*:*/| grep block | awk -F: '{ print $2}'  2> /dev/null `
        fi
        if [ ! -z "$temp_dev" ]; then
            gTempDevice="/dev/"${temp_dev}
            echo "Temp device: ${gTempDevice}"
            break
        fi
    done
}

GetBekVolume()
{
    blkid | grep "LABEL=\"BEK VOLUME\"" > /dev/null 2>&1
    if [ $? -ne "0" ]; then
        return
    fi

    dev=`blkid | grep "LABEL=\"BEK VOLUME\"" | awk -F":" '{print $1}'`
    gBekVolume=`echo $dev | sed 's/[0-9]\+$//'`
}

GetBootTempDevicesOnGen2()
{

    numbootscsicontrollers=`grep ${BOOT_SCSI_CLASS_ID} /sys/bus/vmbus/devices/*/device_id | wc -l`
    numdatascsicontrollers=`grep ${DATA_SCSI_CLASS_ID} /sys/bus/vmbus/devices/*/device_id | wc -l`

    if [ $numbootscsicontrollers -ne 1 ]; then
        echo "Found ${numbootscsicontrollers} scsi controllers for boot disk"
        exit $ENOTRECOVERABLE
    fi

    if [ $numdatascsicontrollers -ne 1 ]; then
        echo "Found ${numdatascsicontrollers} scsi controllers for data disks"
        exit $ENOTRECOVERABLE
    fi

    bootscsicontroller=`grep ${BOOT_SCSI_CLASS_ID} /sys/bus/vmbus/devices/*/device_id | awk -F: '{ print $1}' | sed 's/device_id//g'`

    boot_dev=`ls ${bootscsicontroller}/host*/target*/[0-9]*:*0/block/ 2>/dev/null`
    if [ ! -z "$boot_dev" ]; then
        gBootDevice="/dev/"${boot_dev}
        echo "Boot device: ${gBootDevice}"
    fi

    temp_dev=`ls ${bootscsicontroller}/host*/target*/[0-9]*:*1/block/ 2> /dev/null`
    if [ ! -z "$temp_dev" ]; then
        gTempDevice="/dev/"${temp_dev}
        echo "Temp device: ${gTempDevice}"
    fi

}

DiscoverDevices()
{
    GetBootDevice
    GetTempDevice
    GetBekVolume

    if [  -z "$gBootDevice" ]; then
        GetBootTempDevicesOnGen2
    fi

    if [ -z "$gBootDevice" ]; then
        echo "Failed to find boot device"
        exit $ENOTRECOVERABLE
    fi

    if [ -z "$gTempDevice" ]; then
        echo "Failed to find temp device"
#       _v4 size machines do not have temp disk.
#       Do not exit when temp disks are not found.
#       exit $ENOTRECOVERABLE
    fi

    for hctl in `ls /sys/class/scsi_disk/`
    do
        if [ -d "/sys/class/scsi_disk/$hctl/device/block" ]; then
            dev=`ls /sys/class/scsi_disk/$hctl/device/block/  2> /dev/null`
        else
            dev=`ls /sys/class/scsi_disk/$hctl/device/ | grep block | awk -F: '{print $2}'  2> /dev/null`
        fi

        if [ -z "$dev" ]; then
            echo "No block device found at /sys/class/scsi_disk/$hctl/device/"
            continue
        fi

        device="/dev/"${dev} 

        if [ ! -z "$gBekVolume" -a "$device" = "$gBekVolume" ]; then
            continue
        fi

        host_id=`echo $hctl | cut -d: -f1`
        found_iscsi_disk=0
        if [ -d "/sys/class/iscsi_host" ]; then
            for host in `ls /sys/class/iscsi_host/`
            do
                if [ "${host}" = "host${host_id}" ]; then
                    found_iscsi_disk=1
                    break
                fi
            done
        fi

        if [ ${found_iscsi_disk} -eq "1" ]; then
            continue
        fi

        if [ "$device" != "${gBootDevice}" -a \
		"$device" != "${gTempDevice}" ]; then

            echo "Data disk device: $device at $hctl"
            lun_id=`echo $hctl | cut -d: -f4`
            if [ -z "${gDataDiskHostId}" ]; then
                gDataDiskHostId=$host_id
            fi

            if [ "$gDataDiskHostId" != "$host_id" ]; then
                echo "Found more than one host for data disks: $gDataDiskHostId $host_id $hctl $device"
                exit $ENOTRECOVERABLE
            fi

            PersistentDeviceName="DataDisk${lun_id}"
            gDataDevices[$lun_id]="${PersistentDeviceName}:${device}"
        fi
    done
}

GetRootDiskPersistentName()
{
    if [ -e $VXPROTDEVMAPPATH ]; then
        BOOTDEVPERSNAME=`grep m_id $VXPROTDEVMAPPATH | grep -v Data | awk -F: '{ print $3 }' | awk -F\" '{ print $2 }' | sed 's/\\\//g' | sed 's/\//_/g'`
    fi

    if [ -z "${BOOTDEVPERSNAME}" ]; then
        BOOTDEVPERSNAME="${ROOTDISKDEFAULTPERSNAME}"
    fi
}

GetDeviceMapFromPersistence()
{
    numdisk=0
    while read line
    do
        echo $line | grep m_id > /dev/null
        if [ $? -eq 0 ]; then
            PersistentDeviceName=`echo $line | sed 's/^\S*"m_id"/"m_id"/g' | awk -F: '{ print $2 }' | sed 's/[",]//g'`
            continue
        fi

        echo $line | grep m_deviceName > /dev/null
        if [ $? -eq 0 ]; then
            device=`echo $line | cut -d':' -f2 | sed 's/[",]//g'`

            if [ -z "${PersistentDeviceName}" ]; then
                echo "For device $device , no persistent name found in map file"
                exit $EBADF
            fi

            gDevicesMap[$numdisk]="${PersistentDeviceName}:${device}"
            numdisk=`expr $numdisk + 1`
            continue
        fi

    done < $VXPROTDEVMAPPATH
}


GetPersistentNameForDeviceNameOnVmWare()
{
    if [ ! -b "$deviceName" ]; then
       exit $ENODEV
    fi

    if [ "${gCsType}" != "CSPrime" ]; then
        persistent_name=`echo $deviceName | sed 's/\//_/g'`
        echo "Persistent Name: $persistent_name"
        exit 0
    fi

    GetDeviceMapFromPersistence

    for pnamdev in "${gDevicesMap[@]}"
    do
        persistent_name="${pnamdev%%:*}"
        device_name="${pnamdev##*:}"
        if [ "$device_name" == "$deviceName" ]; then
            echo "Persistent Name: $persistent_name"
            exit 0
        fi
    done

    echo "For Device $deviceName persistent name is not discovered."
    exit $EBADF
}

GetPersistentNameForDeviceName()
{
    if [ ! -b "$deviceName" ]; then
       exit $ENODEV
    fi

    if [ "${gVxPlatformType}" == "${PLATFORMTYPE_VMWARE}" -a "$gHypervisorType" != "Microsoft" ]; then
        GetPersistentNameForDeviceNameOnVmWare
    fi

    DiscoverDevices
    GetRootDiskPersistentName

    if [ "$deviceName" == "${gBootDevice}" ]; then
        persistent_name=$BOOTDEVPERSNAME
        echo "Persistent Name: $persistent_name"
        exit 0
    elif [ "$deviceName" == "${gTempDevice}" ]; then
        echo "Device $deviceName is Azure temp disk."
        exit $EBADF
    elif [ ! -z "$gBekVolume" -a "$deviceName" == "$gBekVolume" ]; then
        echo "Device $deviceName is Azure BEK volume."
        exit $EBADF
    else
        for pnamdev in "${gDataDevices[@]}"
        do
            persistent_name="${pnamdev%%:*}"
            device_name="${pnamdev##*:}"
            if [ "$device_name" == "$deviceName" ]; then
                echo "Persistent Name: $persistent_name"
                exit 0
            fi
        done

    fi

    echo "For Device $deviceName persistent name is not discovered."
    exit $EBADF
}

GetDeviceNameForPersistentNameOnVmWare()
{
    if [ "${gCsType}" != "CSPrime" ]; then
        echo $persistentDeviceName | grep "_dev_" > /dev/null
        if [ $? -ne 0 ]; then
            exit $EBADF
        fi

        device_name=`echo $persistentDeviceName | sed 's/_/\//g'`

        if [ ! -b "$device_name" ]; then
           exit $ENODEV
        fi

        echo "Device Name: $device_name"
        exit 0
    fi

    GetDeviceMapFromPersistence

    for pnamdev in "${gDevicesMap[@]}"
    do
        persistent_name="${pnamdev%%:*}"
        device_name="${pnamdev##*:}"
        if [ "$persistentDeviceName" == "$persistent_name" ]; then
            echo "Device Name: $device_name"
            exit 0
        fi
    done

    echo "For Device $deviceName persistent name is not discovered."
    exit $EBADF
}

GetDeviceNameForPersistentName()
{
    if [ "${gVxPlatformType}" == "${PLATFORMTYPE_VMWARE}" -a "$gHypervisorType" != "Microsoft" ]; then
        GetDeviceNameForPersistentNameOnVmWare
    fi

    DiscoverDevices
    GetRootDiskPersistentName

    if [ "$persistentDeviceName" == "$BOOTDEVPERSNAME" ]; then
        echo "Device Name: $gBootDevice"
        exit 0
    else

        echo $persistentDeviceName | grep "DataDisk" > /dev/null
        for pnamdev in "${gDataDevices[@]}"
        do
            persistent_name="${pnamdev%%:*}"
            device_name="${pnamdev##*:}"
            if [ "$persistentDeviceName" == "$persistent_name" ]; then
                echo "Device Name: $device_name"
	        exit 0
            fi
        done
    fi

    echo "Device for persistent name $persistentDeviceName not found."
    exit $EBADF
}

GetHypervisorType()
{
    # 1. check the dmidcode -t system output if dmidcode bin is available
    if [ -x /usr/sbin/dmidecode ]; then 
        HypervisorName=`/usr/sbin/dmidecode -t system | grep Manufacturer | awk -F: '{print $2}'`
        AssetTag=`/usr/sbin/dmidecode -t chassis | grep 'Asset Tag:' | awk '{print $3}'`
    elif [ -e /sys/class/dmi/id/sys_vendor ]; then
        HypervisorName=`cat /sys/class/dmi/id/sys_vendor`
        AssetTag=`cat /sys/devices/virtual/dmi/id/chassis_asset_tag`
    else
        echo "Hypervisor detection failed"
        exit $ENOTRECOVERABLE
    fi

    echo "Hypervisor name : $HypervisorName"

    echo $HypervisorName | grep -i Microsoft > /dev/null
    if [ $? -eq 0 ]; then

        if [ "${AssetTag}" == "${AZURE_ASSET_TAG}" ]; then
            gHypervisorType="Microsoft"
        else
            # this is a VM on the Hyper-V 
            gHypervisorType="Physical"
        fi

        return
    fi

    echo $HypervisorName | grep -i VMware > /dev/null
    if [ $? -eq 0 ]; then
        gHypervisorType="VMware"
        return
    fi

    # else it is physical
    gHypervisorType="Physical"
    return
}

GetVxPlatformType()
{
    if [ -e ${VXPERSISTPLATFORMPATH} ]; then
        gVxPlatformType=`grep -w VMPLATFORM ${VXPERSISTPLATFORMPATH} | awk -F= '{print $2}'`
    fi

    if [ -z "${gVxPlatformType}" ]; then
        echo "Failed to find the Vx persistent platform type file"
        exit $ENOTRECOVERABLE
    fi

    echo "Platform type: $gVxPlatformType"
}

GetCsType()
{
    if [ -e ${VXPERSISTPLATFORMPATH} ]; then
        gCsType=`grep -w CS_TYPE ${VXPERSISTPLATFORMPATH} | awk -F= '{print $2}'`
    fi

    if [ -z "${gVxPlatformType}" ]; then
        echo "Failed to find the Vx persistent platform type file"
        exit $ENOTRECOVERABLE
    fi

    echo "CS type: $gCsType"
}

#### MAIN ####

if [ $# -ne 2 ]
then
	Usage
fi

bGetPersistentName=0
bGetDeviceName=0

while getopts ":p:d:" opt
do
    case $opt in
    p )
	bGetPersistentName=1
	deviceName=$2
        ;;
    d )
	bGetDeviceName=1
	persistentDeviceName=$2
    	;;	 
    \?) Usage ;;
    esac
done

GetVxPlatformType
GetCsType

# discover the hyper-visor type
GetHypervisorType
echo "Hypervisor Type: $gHypervisorType"

if [ $bGetPersistentName -eq 1 ]; then
    GetPersistentNameForDeviceName
elif [ $bGetDeviceName -eq 1 ]; then
    GetDeviceNameForPersistentName
fi

exit 0
