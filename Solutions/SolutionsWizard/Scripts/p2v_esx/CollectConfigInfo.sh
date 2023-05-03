#!/bin/bash
#
# Copyright (c) 2012 InMage
# This file contains proprietary and confidential information and
# remains the unpublished property of InMage. Use, disclosure,
# or reproduction is prohibited except as permitted by express
# written license aggreement with InMage.
#
# File       : collect_source_info.sh
#
# Revision - history 
# Initial version - prkharch
# Source information - prkharch
#
# Description:
# Collect source machine system device information

#exec 4>"Configuration.info"
_SCRIPTS_PATH_=$1
LOGS_DIR="/etc/vxagent/logs"
machine_manufacturer=""
machine_product_type=""
os=`uname -s`
os_rel=`uname -r`
case "${os}" in
    SunOS)
        rootdevice=`mount | grep "/ on "  | awk '{print $3}'`
        bootdevice="Not applicable"
    ;;
    Linux)
        rootdevice=`mount | grep "on / "  | awk '{print $1}'`
        bootdevice=`mount | grep " on /boot "  | awk '{print $1}'`
	if [ ! ${bootdevice} ];then
		bootdevice=$rootdevice
	fi
    ;;
    *)
        echo "Device info identification: OS is not supported"
        exit
    ;;
esac
get_driver_by_major()
{
    major="$1"
    case "${os}" in
        SunOS)
            /usr/sbin/modinfo | awk 'BEGIN { drv=""; }        \
            {                                                 \
                if ($4 == "'"$major"'") {                     \
                    drv=$6;                                   \
                    exit;                                     \
                }                                             \
            }                                                 \
            END { print drv; }'
            ;;
        Linux)
            cat /proc/devices  | grep "Block devices" -A 100  \
            | grep -v "Block devices" |                       \
            awk 'BEGIN { drv=""; }                            \
            {                                                 \
               if ($1 == "'"$major"'") {                      \
                    drv = $2;                                 \
                    exit;                                     \
               }                                              \
            }                                                 \
            END { print drv; }'
            ;;
        *)
            #echo "OS is not supported"
            exit
            ;;
    esac
}

get_device_major()
{
    if [ -b "$1" -o -c "$1" ]
    then
        major_num=`/usr/bin/file $1 | awk -F'(' '{print $2}' |      \
                           awk -F'/' '{print $1}'`
        #echo "Getting major num:(${major_num})"
        case "${os}" in
                Linux)
                if [ ! $major_num ]
                then
                    major_num=`ls -l $1 | awk -F' ' '{print $5}' | grep -o "[0-9]*"`
                fi
             ;;
             *)
             ;;
        esac
        echo "${major_num}"
    fi
}

rootdev_maj=`get_device_major $rootdevice`
rootdev_type=`get_driver_by_major $rootdev_maj`

echo "#$%^& InMage Scout - Source machine configuration details #$%^&"
echo 
echo 

#For root on logical volumes, scan disks for volume group
case "${os}" in
    SunOS)
    ;;
    Linux)
	if [ "${rootdev_type}" == "device-mapper" ]
	then
		#echo "Root device : $rootdevice is a logical volume major:$rootdev_maj type:$rootdev_type"
		rootvgname=`lvdisplay $rootdevice 2> /dev/null | grep "VG Name" | awk '{print $3}'`
		rootpvname=`vgdisplay $rootvgname -v 2> /dev/null | grep "Physical volumes" -A 2 | grep "PV Name"`
		rootpvname=`echo $rootpvname | grep "PV Name" | awk '{print $3}'`
	fi
    ;;
    *)
        echo "Device info identification: OS is not supported"
        exit
    ;;
esac
# Collect source machine information and device scsi ids
case "${os}" in
    SunOS)
    ;;
    Linux)
	if [ -f /usr/sbin/dmidecode ]
	then
		if [ ! -d $LOGS_DIR ]

		then
			mkdir $LOGS_DIR
		fi
		/usr/sbin/dmidecode > $LOGS_DIR/dmidecode.log
		if [ $? -ne 0 ]
		then
		machine_product_type=`/usr/sbin/dmidecode | grep "System Information" -A 2| grep "Product Name" | cut -d":" -f2 | sed -e "s/^ //g"`
		machine_manufacturer=`/usr/sbin/dmidecode | grep "System Information" -A 2| grep "Manufacturer" | cut -d":" -f2 | sed -e "s/^ //g"`
		else
		machine_product_type=`cat $LOGS_DIR/dmidecode.log | grep "System Information" -A 2| grep "Product Name" | cut -d":" -f2 | sed -e "s/^ //g"`
		machine_manufacturer=`cat $LOGS_DIR/dmidecode.log | grep "System Information" -A 2| grep "Manufacturer" | cut -d":" -f2 | sed -e "s/^ //g"`
		fi
	fi
	if [ "${rootdev_type}" == "device-mapper" ]
	then
		#echo "Root device : $rootdevice is a logical volume major:$rootdev_maj type:$rootdev_type"
		rootvgname=`lvdisplay $rootdevice 2> /dev/null | grep "VG Name" | awk '{print $3}'`
		rootpvname=`vgdisplay $rootvgname -v 2> /dev/null | grep "Physical volumes" -A 2 | grep "PV Name"`
		rootpvname=`echo $rootpvname | grep "PV Name" | awk '{print $3}'`
	fi
	>$LOGS_DIR/scsi_id.log
	for i in `echo "/dev /dev/disk/by-id /dev/disk/by-uuid /dev/disk/by-label /dev/mapper /dev/cciss"`
	do
		if [ ! -d "${i}" ]; then
			continue
		fi
		for j in `ls $i/*`
		do
			echo "${j}" | grep "/dev/fd" 2>&1 > /dev/null
			if [ $? -eq 0 ]; then
				continue
			fi
			echo "${j}" | grep "/dev/fb" 2>&1 > /dev/null
			if [ $? -eq 0 ]; then
				continue
			fi
			grep "${j}" $LOGS_DIR/scsi_id.log 2>&1 > /dev/null
			if [ $? -ne 0 ]
			then
				if [ -b "${j}" ]
				then
					tempdev_maj=`get_device_major $j`
					tempdev_type=`get_driver_by_major $tempdev_maj`
					temp_scsi_id=`/etc/vxagent/bin/inm_scsi_id $j`
					#echo "${temp_scsi_id}:${j}:MODULE-TYPE|$tempdev_type"
					echo "${temp_scsi_id}:${j}:MODULE-TYPE|$tempdev_type" >> $LOGS_DIR/scsi_id.log
				fi
			fi
		done
	done
    ;;
    *)
        echo "Device info identification: OS is not supported"
        exit
    ;;
esac
machine_arch=`uname -m`
. "$_SCRIPTS_PATH_/OS_details.sh"
echo "Operating System                       : $os"
echo "Operating System Kernel Release        : $os_rel"
echo "Distribution details                   : $OS"
echo "Machine architecture                   : $machine_arch"
echo "Root device                            : $rootdevice"
echo "Root device  type                      : $rootdev_type"
echo "Machine Manufacturer                   : $machine_manufacturer"
echo "Machine Product Type                   : $machine_product_type"
if [ "${rootvgname}" ]
then
	echo "Root volume group name                 : $rootvgname"
	echo "Root volume group PV name              : $rootpvname"
fi

echo "Boot device                            : $bootdevice"
echo "Distribution release information       :"
case "${os}" in
    SunOS)
        cat /etc/release
    ;;
    Linux)
	    cat /etc/[SuSEredhatlsb]*-release
    ;;
    *)
        echo "Distribution idenfication :OS is not supported"
        exit
    ;;
esac
echo "============================== Filesystem table - START ================================"
case "${os}" in
    SunOS)
        cat /etc/vfstab
    ;;
    Linux)
	cat /etc/fstab
    ;;
    *)
        echo "Distribution idenfication :OS is not supported"
        exit
    ;;
esac
echo "============================== Filesystem table - END ================================"
echo "============================== GRUB Information - START ================================"
case "${os}" in
    SunOS)
        cat /boot/grub/menu.lst
    ;;
    Linux)
	if [ -f "/boot/grub/menu.lst" ]; then
		cat /boot/grub/menu.lst
	elif [ -f "/boot/grub/grub.cfg" ]; then
		cat /boot/grub/grub.cfg
	fi
    ;;
    *)
        echo "Distribution idenfication :OS is not supported"
        exit
    ;;
esac
echo "============================== GRUB Information - END ================================"
echo "============================== Devicemap Information - START ==========================="
case "${os}" in
    SunOS)
    ;;
    Linux)
	if [ -f "/boot/grub/device.map" ]; then
	    cat /boot/grub/device.map
	fi
    ;;
    *)
        echo "Distribution idenfication :OS is not supported"
        exit
    ;;
esac
echo "============================== Devicemap Information - END ==========================="
echo "============================== FSTAB UUID Information - START ==============================="
case "${os}" in 
    SunOS)
    ;;
    Linux)
	cat /etc/fstab | while read _line_
        do
                _dev_partition_=`echo $_line_  | awk '{print $1}'`
                _mount_point_=`echo $_line_ | egrep "^[ ]*UUID|[ ]*LABEL" | awk '{print $2}'`
                if [ "${_mount_point_}" ]; then
                        _dev_partition_=`mount | grep " on ${_mount_point_} " | awk '{print $1}'`
                fi
                if [ -b "$_dev_partition_" ]; then
                        _temp_blkid_=`blkid $_dev_partition_`
                        echo $_temp_blkid_
                fi
        done
    ;;
    *)
	echo "Distribution identification :OS is not supported"
	exit
    ;;
esac
echo "============================== FSTAB UUID Information - END ==============================="
echo "============================== BLKID Information - START ==============================="
case "${os}" in 
    SunOS)
    ;;
    Linux)
    rm -rf /tmp/blkid.txt
    blkid | \
    while read -r line
    do
        initial_entry=`echo $line | cut -d " " -f 1`
        initial_entry=${initial_entry%?}
        final_entry=`readlink -f ${initial_entry/:/\:}`
        echo "${line/$initial_entry/$final_entry}" >> /tmp/blkid.txt
    done
    sort /tmp/blkid.txt | uniq
    ;;
    *)
	echo "Distribution identification :OS is not supported"
	exit
    ;;
esac
echo "============================== BLKID Information - END ==============================="
case "${os}" in 
    SunOS)
    ;;
    Linux)
	_formats_=`ls /dev/disk/`
	for _format_ in ${_formats_}
	do 
		_format_cap_=`echo $_format_ | tr '[a-z]' '[A-Z]'`
		echo "============================== $_format_cap_ Information - START ==============================="
		ls -l /dev/disk/$_format_
		echo "============================== $_format_cap_ Information - END ==============================="
	done
    ;;
    *)
	echo "Distribution identification :OS is not supported"
	exit
    ;;
esac
echo "============================== DMESETUP TABLE Information - START ==============================="
case "${os}" in 
    SunOS)
    ;;
    Linux)
	if [ -f /sbin/dmsetup ]; then
		/sbin/dmsetup table 2> /dev/null
	fi
    ;;
    *)
	echo "Distribution identification :OS is not supported"
	exit
    ;;
esac
echo "============================== DMSETUP TABLE Information - END ==============================="
echo "============================== LogicalVolume Information - START ==============================="
case "${os}" in 
    SunOS)
    ;;
    Linux)
	if [ -f /sbin/lvdisplay ]; then
		/sbin/lvdisplay 2> /dev/null | grep -i "[lv][vg] Name"
	fi
    ;;
    *)
	echo "Distribution identification :OS is not supported"
	exit
    ;;
esac
echo "============================== LogicalVolume Information - END ==============================="
echo "============================== LogicalVolume PV Information - START ==============================="
case "${os}" in 
    SunOS)
    ;;
    Linux)
	if [ -f /sbin/pvdisplay ]; then
		/sbin/pvdisplay 2> /dev/null | grep -i "[lv][vg] Name"
	fi
    ;;
    *)
	echo "Distribution identification :OS is not supported"
	exit
    ;;
esac
echo "============================== LogicalVolume PV Information - END ==============================="
echo "============================== LSSCSI Information - START ==============================="
case "${os}" in 
    SunOS)
    ;;
    Linux)
	if [ -f /usr/bin/lsscsi ];then
		/usr/bin/lsscsi
	fi
    ;;
    *)
	echo "Distribution identification :OS is not supported"
	exit
    ;;
esac
echo "============================== LSSCSI Information - END ==============================="

