#!/bin/bash
#
# Copyright (c) 2005 InMage
# This file contains proprietary and confidential information and
# remains the unpublished property of InMage. Use, disclosure,
# or reproduction is prohibited except as permitted by express
# written license aggreement with InMage.
#
# File       : boottimemirroring.sh
#
# Boot time mirroring setup script for lun replication
#
# Description:
# Setup mirror from source Physical Target (PT) to destination Appliance
# Target (AT) for respective lun replication pair on CX.
# Mirroring is setup based on class of the source device used. Class could be
# SCSI, VxDMP, OS Native multipath, HDLM or PowerPath device. This ensures that
# we would always use device of only one class for setting up the mirroring
# AT device is always the SCSI device. 
# 
# Author - Prashant Kharche (prkharch@inmage.com)


# Persistent store directory
pstore=/etc/vxagent/involflt
inm_dmit=/etc/vxagent/bin/inm_dmit
get_scsid_cmd=/etc/vxagent/bin/inm_scsi_id
os=`uname -s`
script_path=/etc/init.d/
partition=""
inm_dbg_str=""
MirrorFilterDevType="5"
FILTER_DEV_HOST_VOLUME="2"
INVOLFLT_DRVNAME="involflt"
INVOLFLT_CTRLNODE="/dev/involflt"
src_drv_name=""
dst_drv_name=""
scanned_dir=""

rt_src_devices=""

declare -a DB_SCSI_ID
declare -a  DB_DEV
DB_SCSI_ID[0]=
DB_DEV[0]=
DB_COUNT=0
dev_list=""
opti_count=0
PATH=/bin:/sbin:/usr/sbin:/usr/bin:$PATH
export PATH

#scan existing dir
scan_dir() {
	dir=$1
	num=`echo "$scanned_dir"|awk -F, '{print NF}'`
	i=1
	while((i <= $num))
	do
		temp=""
		temp=`echo "$scanned_dir"|cut -d, -f$i`
		if [ $temp = $dir ];then
			return 0
		fi
		i=$(( $i + 1 ))
	done
	return 1
}

# Tokenize dev list and get devices for appropriate drviers
get_app_devs() {
	dev=$1
	drivers="$2"
	if [ "${drivers}" = "PT" ]
	then
		drivers="${src_drv_name}"
	elif [ "${drivers}" = "AT" ];then
		drivers="${dst_drv_name}"
	else
		exit 1
        fi
	num=`echo ${dev} | awk -F, '{ print NF }'`
	if [ $num -gt 1 ]
	then
		final_dev=""
		i=1
		while((i <= $num))
		do
			temp=""
			temp=`echo ${dev} | cut -d, -f$i`
			dev_maj=`get_device_major $temp`
			dev_driver=`get_driver_by_major $dev_maj`

			for x in $drivers
        		do
				if [ "${dev_driver}" = "${x}" ]
        	  		then
					if [ -z ${final_dev} ];then
						final_dev=${temp}
						break
					else
						final_dev=`echo ${final_dev},${temp}`
						break
					fi
				fi
			done
			i=$(($i + 1 ))
		done
	else
		temp=""
		temp=${dev}
		dev_maj=`get_device_major $temp`
		dev_driver=`get_driver_by_major $dev_maj`

		for x in $drivers
        	do
			if [ "${dev_driver}" = "${x}" ]
          		then
				if [ -z ${final_dev} ];then
					final_dev=${temp}
					break
				else
					final_dev=`echo ${final_dev},${temp}`
					break
				fi
			fi
		done
	fi
	echo ${final_dev}
}

# provide "space" separated inputs 
# e.g. -o add/find/print -d dev -s scsi_id -p dir -m driver
parent_fun() {
    declare -a parm
    num_parm=$#
    j=0
    for i in $*
    do
	parm[$j]=$i
	j=$(( $j + 1 ))
    done
    i=0
    while (( i < $num_parm))
    do
	case "${parm[$i]}" in
	    -o)
		i=$(( $i + 1 ))
		opt=${parm[$i]}
		;;
	    -d)
		i=$(( $i + 1 ))
		dev=${parm[$i]}
		;;
	    -s)
		i=$(( $i + 1 ))
		scsi_id=${parm[$i]}
		;;
	    -p)
		i=$(( $i + 1 ))
		directory=${parm[$i]}
		;;
	    -m)
		i=$(( $i + 1 ))
		drvr=${parm[$i]}
		;;
	    *)
		#echo "In correct input"
		#return 1
		;;
	esac
	i=$(( $i + 1 ))
    done
    comp_scsi_id=`echo "$scsi_id,$directory"`
    case "$opt" in
	add)
	    temp=0
	    while [ $temp -lt $DB_COUNT ]
	    do
	    	if [ `echo "${DB_SCSI_ID[$temp]}"` = $comp_scsi_id ];then
		   break
		fi
		temp=$(($temp + 1))
	    done
	    if [ $temp -lt $DB_COUNT ];then
		if [ ! `echo "${DB_DEV[$temp]}" | grep $dev` ];then
		    DB_DEV[$temp]=`echo "${DB_DEV[$temp]},$dev"`
		fi
	    else
		DB_DEV[$DB_COUNT]=${dev}
		DB_SCSI_ID[$DB_COUNT]=$comp_scsi_id
		DB_COUNT=$(( $DB_COUNT +1))
	    fi
	    ;;
	find)
	    temp=0
	    dev="NULL"
	    while [ $temp -lt $DB_COUNT ]
	    do
		if [ ${DB_SCSI_ID[$temp]} = $comp_scsi_id ];then
			dev=${DB_DEV[$temp]}
			break
		fi
		temp=$(($temp + 1))
	   done
	   if [ $temp -lt $DB_COUNT ]
	   then
		dev=`get_app_devs $dev $drvr`
	   fi
	   dev_list=$dev
	   ;;
	print_db1)
	   echo ""
	   echo "===============Cache DB($DB_COUNT)==============="
	   temp=0
	   echo "scsi_id,dev       dev_list"
	   while (( temp < $DB_COUNT))
	   do
		echo  "${DB_SCSI_ID[$temp]}       ${DB_DEV[$temp]}      "
		temp=$(( $temp + 1 ))
	   done
	    ;;
    esac
}

isainfo=
if [ -f /usr/bin/isalist ]; then
    isainfo=`isalist`
fi

# err logger for the script
inm_log() {
	inm_dbg_str="${inm_dbg_str} |"
	inm_date_str=`date +"%b %d %H:%M:%S"`
	inm_dbg_str="${inm_dbg_str} ${inm_date_str}"
	inm_dbg_str="${inm_dbg_str} $*"
}

# FUNCTION: Invokes inm_logger to write into kernel ring buffer
cleanup() {
	INM_LOGGER=/etc/init.d/inm_logger
	if [ -f $INM_LOGGER ] ; then
		$INM_LOGGER "inm_hotplug: " $inm_dbg_str
	else
		echo $inm_dbg_str
	fi
	inm_dbg_str=""
}

module_id()
{
    module="$1"
    case "${os}" in
    	SunOS)
            /usr/sbin/modinfo | awk 'BEGIN { n = "none"; }\
            {                                             \
                if ($6 == "'"$module"'") {                \
                    n = $1;                               \
                    exit;                                 \
                }                                         \
            }                                             \
            END { print n; }'
	    ;;
	Linux)
            cat /proc/devices  | grep "Block devices" -A 100  \
            | grep -v "Block devices" |                   \
            awk 'BEGIN { n = "none"; }                    \
            {                                             \
                if ($2 == "'"$module"'") {                \
                    n = $1;                               \
                    exit;                                 \
                }                                         \
            }                                             \
            END { print n; }'
	    ;;
	*)
            echo "OS is not supported"
            exit
            ;;
    esac
}

# Is a given module loaded?
is_module_loaded()
{
   module="$1"
   if [ `module_id "$module"` = 'none' ]; then
      echo no
   else
      echo yes
   fi
}

# FUNCTION: Create the filter driver node /dev/involflt if the driver is already loaded
CreateFiltDrvNode()
{
    drv_name="$1"
    drv_node="$2"
    case "${os}" in
    	SunOS)
            rm -f $drv_node 2>/dev/null
            major=0
            major=`grep $drv_name /etc/name_to_major | awk '{print $2}'`
            if [ ${major} ]
            then
                mknod $drv_node c ${major} 0
            else
                inm_log " Unable to create the filter device node ${drv_node}"
                cleanup
                return 1
            fi
    		;;
    	Linux)
    	    if lsmod | grep $drv_name > /dev/null 2>&1 && [ ! -c $drv_node ]; then
    		    local FILTDRVNODE_MAJ_NUM=`cat /proc/devices | grep $drv_name | grep -o "^[0-9]*"`
           	    mknod $drv_node c ${FILTDRVNODE_MAJ_NUM} 0
	    	fi
    		;;
    	*)
    		echo "OS is not supported"
    		return 1
    		;;
    esac
    return 0
}


#case "${os}" in
#	SunOS)
#		get_scsid_cmd="${script_path}prg"
#		;;
#	Linux)
#		get_scsid_cmd="${script_path}scsi_id.sh "
#		;;
#	*)
#		echo "OS is not supported"
#		exit
#		;;
#esac
# returns OK if $1 contains $2
strstr() {
  [ "${1#*$2*}" = "$1" ] && return 1
  return 0
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
                    major_num=`ls -lL $1 | awk -F' ' '{print $5}' | grep -o "[0-9]*"`
                fi
             ;;
             *)
             ;;
        esac
        echo "${major_num}"
    fi
}

# Get driver name by device class
get_driver_by_class()
{
    case "${os}" in
    	SunOS)
            case "$1" in
        	# SCSI class
            	2) 
            	case "$isainfo" in
        		*sparc*)
        			echo "sd ssd"
        			;;
        		*amd64*)
        			echo "sd"
        			;;
        		*)
        			echo "UnknownVendor"
                	;;
		esac
		;;
            	# Native multipath - MPxIO
            	4)
            	case "$isainfo" in
        		*sparc*)
        			echo "sd ssd"
        			;;
        		*amd64*)
        			echo "sd"
        			;;
        		*)
        			echo "UnknownVendor"
                	;;
		esac
		;;
            	#4) echo "mpxio" ;;
            	# EMC PowerPath
            	5) echo "emcp" ;;
            	# HDLM
            	6) echo "dlmfdrv" ;;
            	# DEVDID - Sun Cluster Global devices
            	7) echo "did" ;;
            	# DEVGLOBAL - Sun Cluster Global devices
            	7) echo "did" ;;
            	# VxDMP
            	9) echo "vxdmp" ;;
            	# ASM device -
            	16)
            	case "$isainfo" in
        		*sparc*)
        			echo "sd ssd"
        			;;
        		*amd64*)
        			echo "sd"
        			;;
        		*)
        			echo "UnknownVendor"
                	;;
		esac
		;;
        	# Unknown vendor
	           *)
            	    echo "UnknownVendor" ;;
            esac
            ;;
        Linux)
            case "$1" in
        	# SCSI class
            	2) echo "sd" ;;
            	# Native multipath - DM
            	3) echo "device-mapper" ;;
            	# EMC PowerPath
            	5) echo "power2" ;;
            	# HDLM
            	6) echo "dlmfdrv" ;;
            	# VxDMP
            	9) echo "VxDMP" ;;
            	# ASM device -
            	16) echo "sd ssd" ;;
        	# Unknown vendor
	           *)
            	    echo "UnknownVendor" ;;
            esac
	        ;;
        *)
            #echo "OS is not supported"
            exit
            ;;
    esac
}

get_drv_majors()
{
    module="$1"
    case "${os}" in
    	SunOS)
            /usr/sbin/modinfo | awk 'BEGIN { count=0; }       \
            {                                                 \
                if ($6 == "'"$module"'") {                    \
                    count++;                                  \
                    major_list[count]=$4                      \
                }                                             \
            }                                                 \
            END { for (x=1; x <= count; x++) printf "%s ",major_list[x] }'
	    ;;
	Linux)
            cat /proc/devices  | grep "Block devices" -A 100  \
            | grep -v "Block devices" |                       \
            awk 'BEGIN { count=0; }                           \
            {                                                 \
               #print "'"$module"'";                          \
               if ($2 == "'"$module"'") {                     \
                   #print $1 $2;                              \
                   count++;                                   \
                   #print $2;                                 \
                   major_list[count]=$1;                      \
               }                                              \
            }                                                 \
            END { for (x=1; x <= count; x++) printf "%s ",major_list[x] }'
	    ;;
	*)
            #echo "OS is not supported"
            exit
            ;;
    esac
}

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

# a function to find the device name based on scsi id
get_scsid()
{
	sdev=$1
	scsi_id=""
	scsi_id=`${get_scsid_cmd} $sdev 2> /dev/null | sed -e "s: ::"`
	#echo "get_scsid: sdev:${sdev}"
	#case "${os}" in
	#	SunOS)
	#		scsi_id=`${get_scsid_cmd} $sdev 2> /dev/null | grep "page 83" | awk -F= '{ print $2 }' | sed -e "s: ::"`
	#		if [ ! "${scsi_id}" ]
	#		then
	#			scsi_id=`$get_scsid_cmd $sdev 2> /dev/null | grep "page 80" | awk -F= '{ print $2 }' | sed -e "s: ::"`
	#		fi
	#		;;
	#	Linux)
	#		scsi_id=`$get_scsid_cmd $sdev 2> /dev/null`
	#		;;
	#	*)
	#		#echo "OS is not supported"
	#		exit
	#		;;
	#esac
	echo $scsi_id
}
# a function to find the device name based on scsi id
get_device_by_scsid()
{
	dir=$1
	scsiid=$2
	drivers="$3"
	devices=""
	prev_fulldisk_dev="***DEVICE_DOES_NOT_EXIST****"
	counted_dev=""
	fulldisk_dev=""

	if [ "${drivers}" = "PT" ]
	then
		drivers="${src_drv_name}"
	else
		if [ "${drivers}" = "AT" ]
		then
			drivers="${dst_drv_name}"
		fi
	fi
	#echo "get_device_by_scsid(): $dir $scsiid"
	for cur_dev in $dir*
	do
		if [ -f "${cur_dev}" ]
		then 
			continue
		fi
		case "${os}" in
			SunOS)
				nop=0
				;;
			Linux)
				if [ ! -b "${cur_dev}" ]
				then 
					continue
				fi
				cur_dev_maj=`get_device_major $cur_dev`
				cur_dev_driver=`get_driver_by_major $cur_dev_maj`
				if [ "${cur_dev_driver}" = "device-mapper" ]
				then
					echo ${cur_dev} | grep "dm-[0-9]" 2>&1 > /dev/null
					if [ $? -eq 0 ]; then
						continue
					fi
					echo ${cur_dev} | grep "/dev/root" 2>&1 > /dev/null
					if [ $? -eq 0 ]; then
						continue
					fi
					dmsetup status  $cur_dev | grep "multipath" 2>&1 > /dev/null 
					if [ $? -ne 0 ]; then
						continue	
					fi
				fi
				;;
			*)
				rt_src_devices=`echo "OS is not supported"`
				exit
				;;
		esac
		# Try to form partition name same as the one we got while setting up 
		# the mirror initially. fulldisk_dev forms the common string
		# For linux, remove digits at the end of cur_dev and then compare
		# For solaris, ignore if fulldisk_dev turns out to be sed -e "s/s[0-9]//g"
		case "${os}" in
			SunOS)
				cur_dev=`echo ${cur_dev} | sed -e "s: ::g"`
				fulldisk_dev=`echo ${cur_dev} | sed -e "s:[sp][0-9]*$::"`
				#echo "fulldisk_dev:$fulldisk_dev newstr1:$newstr1 $cur_dev $prev_fulldisk_dev"
				if [ "${fulldisk_dev}" = "${prev_fulldisk_dev}" ]
				then
					#echo "fulldisk_dev:$fulldisk_dev is the substring from cur_dev:$tpdev prev_fulldisk_dev:$prev_fulldisk_dev"
					continue
				fi
				if [ -b "${fulldisk_dev}${partition}" ] || [ -c "${fulldisk_dev}${partition}" ] 
				then
					cur_dev="${fulldisk_dev}${partition}"
				else
					continue
				fi
				;;
			Linux)
                                safe_pattern=$(printf "%s\n" "$prev_fulldisk_dev" | sed 's/[][\.*^$(){}?+|/]/\\&/g')
				temp_string=`echo "${cur_dev}" | sed -e "s:${safe_pattern}::g"`
				temp_string=`echo  "${temp_string}" | sed -e 's:[0-9]::g'`
				if [ -z "${temp_string}" ]; then
					#It is a partition of prev_fulldisk_dev, now skip it.
					continue
					#`echo "${cur_dev}" | grep "${prev_fulldisk_dev}" 2>&1 > /dev/null`
					#if [ $? -eq 0 ]
					#then
					#	#echo "$cur_dev is a partition. Skipping it"
					#	continue
					#fi
				fi

				if [ "${src_drv_name}" = "device-mapper" ]
				then
					fulldisk_dev=`echo ${cur_dev} | sed -e 's:[p][0-9]*$::'`
				else
					fulldisk_dev=`echo ${cur_dev} | sed -e 's:[0-9]*$::'`
				fi
				;;
			*)
				echo "OS is not supported"
				exit
				;;
		esac

	    # Check if current device class majors matches with the requested device class majors
		cur_dev_maj=`get_device_major $cur_dev`
		cur_dev_driver=`get_driver_by_major $cur_dev_maj`

		class_check="none"
		for x in $drivers
        do
            if [ "${cur_dev_driver}" = "${x}" ]
            then
                class_check="yes"
                break
            else
                class_check="no"
            fi
        done
        if [ $class_check = "none" -o $class_check = "no" ]
        then
            continue
        fi

		prev_fulldisk_dev="${fulldisk_dev}"

		scsiid_local=`get_scsid ${cur_dev}`	

		# Add the entry to the scsi id cache
		if [ ! -z ${scsiid_local} ];then
			parent_fun -o add -d ${cur_dev} -s ${scsiid_local} -p ${dir}
		else
			continue
		fi

		#echo "device formed in get_device_by_scsiid:$cur_dev scsiid:$scsiid local scsiid:$scsiid_local"
		if [ "${scsiid_local}" = "${scsiid}" ]
		then
			#echo "get_device_by_scsid - found device:$cur_dev with scsi_id:$scsiid "
			if [ "${devices}" ]
			then
				if [ `echo ${devices} | grep ${cur_dev}` ]
				then
					continue
				fi
				devices="${devices},${cur_dev}"
			else
				devices="${cur_dev}"
			fi
		fi
	done
	rt_src_devices=`echo "$devices"`
}

check_if_scsi_matches()
{
	src_dev_local=$1
	scsi_id_in=$2
	scsi_id_local=""
	i=1
	num=`echo $src_dev_local | awk -F, '{ print NF }'`
	if [ $num -eq 0 ] || [ $num -gt 50 ]; then
		echo 1
	fi
	while [ $i -lt $num ] || [ $i -eq $num ]
	do
		device_local=`echo $src_dev_local | cut -d, -f${i}`
		#echo "In SCSI ID" $scsi_id_in "Found device:" $device_local "SCSI ID:" `get_scsid $device_local`
		scsi_id_local=`get_scsid $device_local`
		if [ "${scsi_id_in}" = "${scsi_id_local}" ]
		then
			scsi_id_local=""
		else
			#echo "SCSI ID did not match in:$scsi_id_in device:$device_local scsi_id:$scsi_id_local"
			echo 1
			exit
		fi
		i=`expr $i + 1`
	done
	echo 0
	exit
}

# Main entry point to the script
inm_log "Executing boottime mirroring script"
cleanup
case "${os}" in
    SunOS)
        if /usr/sbin/modinfo | grep -i involflt > /dev/null 2>&1; then
            inm_log " ${INVOLFLT_DRVNAME} driver is loaded"
            cleanup
        else
            echo " ${INVOLFLT_DRVNAME} driver is not loaded"
            inm_log " ${INVOLFLT_DRVNAME} driver is not loaded"
            cleanup
            exit
        fi
    ;;
    Linux)
        if ! lsmod | grep -i involflt > /dev/null 2>&1; then
            inm_log " ${INVOLFLT_DRVNAME} driver is not loaded"
            cleanup
            echo " ${INVOLFLT_DRVNAME} driver is not loaded"
            exit
        fi
	/etc/vxagent/bin/at_lun_masking
    ;;
    *)
        echo "OS is not supported"
        exit
    ;;
esac
CreateFiltDrvNode $INVOLFLT_DRVNAME $INVOLFLT_CTRLNODE
#/etc/vxagent/bin/inm_dmit --op=set_verbosity --verbosity=3
for file in $pstore/*
do
	if [ "$file" != "$pstore/common" ]
	then
		directory=`echo $file | sed -e "s:[^/]*$::"`
		tgt_scsi_id=`echo $file | sed -e "s:[/].*[/]::"`
		is_already_protected=`/etc/vxagent/bin/inm_dmit --get_protected_volume_list | grep $tgt_scsi_id`
		if [ $? -eq 0 ]; then
			inm_log "SCSI ID:$tgt_scsi_id is already protected. Skipping now"
			cleanup
			continue
		fi
		if [ -f ${pstore}/$tgt_scsi_id/FilterDevType ] && \
			[ "`cat ${pstore}/${tgt_scsi_id}/FilterDevType`" = ${MirrorFilterDevType} ] && \
			[ "`cat ${pstore}/${tgt_scsi_id}/VolumeFilteringDisabled`" = "0" ]
		then
			#echo "tgt_scsi_id name $tgt_scsi_id"
			flt_dev_type=`cat ${pstore}/${tgt_scsi_id}/FilterDevType`
			#echo "Filter dev type $flt_dev_type"
			if [ -f ${pstore}/${tgt_scsi_id}/VolumeMirrorSourceList ] && \
				[ -f ${pstore}/${tgt_scsi_id}/VolumeMirrorDestinationList ]
			then
				src_status=""
				dst_status=""
				right_src_devices=""
				right_dst_devices=""
				src_drv_name=""
				dst_drv_name=""
				src_dev=`awk -F, '{ print $1 }' "${pstore}/${tgt_scsi_id}/VolumeMirrorSourceList"`
				dst_dev=`awk -F, '{ print $1 }' "${pstore}/${tgt_scsi_id}/VolumeMirrorDestinationList"`
				src_dev_list=`cat ${pstore}/${tgt_scsi_id}/VolumeMirrorSourceList`
				dst_dev_list=`cat ${pstore}/${tgt_scsi_id}/VolumeMirrorDestinationList`
				dst_scsi_id=`cat "${pstore}/${tgt_scsi_id}/VolumeMirrorDestinationScsiID"`
				src_dev_class=`cat "${pstore}/${tgt_scsi_id}/VolumeDeviceVendor"`
				#src_dev_class="1"
				src_drv_name=`get_driver_by_class $src_dev_class`
				echo "Source driver : $src_drv_name"

				driver_check="okay"
				for driver_name in $src_drv_name
				do
				    if [ "`is_module_loaded "$driver_name"`" != "yes" ]; then
				        driver_check="failure"
				        break
				    fi
				done
                if [ "${driver_check}" != "okay" ]; then
                    echo "Driver(s)[${src_drv_name}] for source disks:${src_dev} is not loaded"
                    inm_log "Driver(s)[${src_drv_name}] for source disks:${src_dev} is not loaded"
                    continue
                fi

				# For AT device (destination device), device class should be SCSI
				dst_dev_class="2" 
				dst_drv_name=`get_driver_by_class $dst_dev_class`
				echo "Destination drivers : $dst_drv_name"
				driver_check="okay"
				for driver_name in $dst_drv_name
				do
				    if [ "`is_module_loaded "$driver_name"`" != "yes" ]; then
				        driver_check="failure"
				        break
				    fi
				done
                if [ "${driver_check}" != "okay" ]; then
                    echo "Driver(s)[${dst_drv_name}] for destination disks:${dst_dev} is not loaded"
                    inm_log "Driver(s)[${dst_drv_name}] for destination disks:${dst_dev} is not loaded"
                    continue
                fi

				echo "persistent store src scsi id:$tgt_scsi_id dst scsi id:$dst_scsi_id src:$src_dev dst:$dst_dev"
				inm_log "persistent store src scsi id:$tgt_scsi_id dst scsi id:$dst_scsi_id src:$src_dev dst:$dst_dev"
				cleanup
				if [ "${os}" = "SunOS" ]
				then 
					src_dev=`echo $src_dev | sed -e "s:dmp:rdmp:g"`
					src_dev=`echo $src_dev | sed -e "s:dsk:rdsk:g"`
					dst_dev=`echo $dst_dev | sed -e "s:dmp:rdmp:g"`
					dst_dev=`echo $dst_dev | sed -e "s:dsk:rdsk:g"`
					src_dev_list=`echo $src_dev_list | sed -e "s:dmp:rdmp:g"`
					src_dev_list=`echo $src_dev_list | sed -e "s:dsk:rdsk:g"`
					dst_dev_list=`echo $dst_dev_list | sed -e "s:dmp:rdmp:g"`
					dst_dev_list=`echo $dst_dev_list | sed -e "s:dsk:rdsk:g"`
				fi

				src_status=`check_if_scsi_matches $src_dev_list  $tgt_scsi_id`
				if [ $src_status -eq 1 ]; then
					#echo "persistent store device name : $src_dev"
					tp_str=`echo ${src_dev} | sed -e "s:[sp][0-9]*$::"`
					partition=`echo ${src_dev} | sed -e "s:${tp_str}::"`
					#echo "src_dev scsi id for ${src_dev} does not match"
					inm_log "src_dev scsi id for ${src_dev} does not match"
					cleanup
					dir=`echo $src_dev | sed -e "s:[^/]*$::"`

					parent_fun -o find -s $tgt_scsi_id -p ${dir} -m PT 
					if [ ! $dev_list = "NULL" ]
					then
						right_src_devices=$dev_list
						opti_count=$(( $opti_count + 1 )) 
					else
						if [ ! -z $scanned_dir ];then
							scan_dir $dir
							if [ $? -eq 0 ];then
								continue
							fi
						fi
						get_device_by_scsid $dir $tgt_scsi_id PT
						right_src_devices=$rt_src_devices
						if [ -z $scanned_dir ];then
							scanned_dir="$dir"
						else
							scanned_dir="$scanned_dir,$dir"
						fi
					fi
				else
					right_src_devices=$src_dev_list;
				fi

				dst_status=`check_if_scsi_matches $dst_dev_list  $dst_scsi_id`
				if [ $dst_status = 1 ]; then
					tp_str=`echo ${dst_dev} | sed -e "s:[sp][0-9]*$::"`
					partition=`echo ${dst_dev} | sed -e "s:${tp_str}::"`
					#echo "dst_dev scsi id for ${dst_dev} does not match"
					inm_log "dst_dev scsi id for ${dst_dev} does not match"
					cleanup
					dir=`echo $dst_dev | sed -e "s:[^/]*$::"`

					parent_fun -o find -s $dst_scsi_id -p ${dir} -m AT
					if [ ! $dev_list = "NULL" ]
					then
						right_dst_devices=$dev_list
						opti_count=$(( $opti_count + 1 )) 
					else
						if [ ! -z $scanned_dir ];then
							scan_dir $dir
							if [ $? -eq 0 ];then
								continue
							fi
						fi
						get_device_by_scsid $dir $dst_scsi_id AT
						right_dst_devices=$rt_src_devices
						if [ -z $scanned_dir ];then
							scanned_dir="$dir"
						else
							scanned_dir="$scanned_dir,$dir"
						fi
					fi
				else
					right_dst_devices=$dst_dev_list
				fi

				#echo "Persistent store information:"
				inm_log "Persistent store information:"
				cleanup
				cat ${pstore}/${tgt_scsi_id}/FilterDevType | awk '{print $1}'
				cat ${pstore}/${tgt_scsi_id}/VolumeMirrorSourceList
				cat ${pstore}/${tgt_scsi_id}/VolumeMirrorDestinationList
				cat ${pstore}/${tgt_scsi_id}/VolumeMirrorDestinationScsiID
				if [ "${os}" = "SunOS" ]
				then 
					right_src_devices=`echo $right_src_devices | sed -e "s:rdmp:dmp:g"`
					right_src_devices=`echo $right_src_devices | sed -e "s:rdsk:dsk:g"`
					right_dst_devices=`echo $right_dst_devices | sed -e "s:rdmp:dmp:g"`
					right_dst_devices=`echo $right_dst_devices | sed -e "s:rdsk:dsk:g"`
					src_dev_list=`echo $src_dev_list | sed -e "s:rdmp:dmp:g"`
					src_dev_list=`echo $src_dev_list | sed -e "s:rdsk:dsk:g"`
					dst_dev_list=`echo $dst_dev_list | sed -e "s:rdmp:dmp:g"`
					dst_dev_list=`echo $dst_dev_list | sed -e "s:rdsk:dsk:g"`
				fi

				#echo "Getting source devices as $right_src_devices"
				inm_log "Getting source devices as $right_src_devices"
				cleanup
				#echo "Getting destination devices as $right_dst_devices"
				inm_log "Getting destination devices as $right_dst_devices"
				cleanup
				echo " "
				echo "execute boot time mirroring ioctl $inm_dmit --op=stack_mirror --src_list=${right_src_devices} --dst_list=${right_dst_devices}"
				inm_log "execute boot time mirroring ioctl $inm_dmit --op=stack_mirror --src_list=${right_src_devices} --dst_list=${right_dst_devices}"
				cleanup
				$inm_dmit --op=stack_mirror --src_list=${right_src_devices} --dst_list=${right_dst_devices}
			fi
		elif [ -f ${pstore}/$tgt_scsi_id/FilterDevType ] && \
                        [ "`cat ${pstore}/${tgt_scsi_id}/FilterDevType`" = ${FILTER_DEV_HOST_VOLUME} ] && \
                        [ "`cat ${pstore}/${tgt_scsi_id}/VolumeFilteringDisabled`" = "0" ]
		then
			#-----Host based protection --------
			echo "execute boot time stacking ioctl $inm_dmit  --op=boottime_stacking "
			inm_log "execute boot time stacking ioctl $inm_dmit  --op=boottime_stacking "
			cleanup
			$inm_dmit  --op=boottime_stacking
		fi
	fi
done
# Uncomment lines for debugging scsi_id cache optimization
#parent_fun -o print_db1
#echo ""
#echo " optimization count $opti_count"
#echo ""
exit 0

