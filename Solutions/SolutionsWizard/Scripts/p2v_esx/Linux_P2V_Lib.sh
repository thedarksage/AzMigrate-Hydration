#!/bin/bash

#GENERIC FUNCTIONS WHICH WILL USED FOR LINUX_P2V PROJECT.
_UUID_=""
_LABEL_=""
_disk_name_="" #contains Target device Name, 
_base_diskname_=""
_dev_name_="" #Contains device name for a UUID or Label. Either it could be a partition or entire device.
_real_partition_name_="" #It contains partition name corresponding to sym-link.
_lv_dev_name_="" #Contains alias for an LV.

#FINDS UUID INFORMATION FROM FSTAB-BLKID INFORMATION
function Find_UUID_FSTABINFO
{
	_UUID_=""
	_partition_name_=$1
	_uuid_=`sed -n  "/^[=]* FSTAB UUID Information - START [=]*/,/^[=]* FSTAB UUID Information - END [=]*/p" "$_INPUT_CONFIG_FILE_" | grep "$_partition_name_" | head -1`
	if [ "${_uuid_}" ]; then
		_uuid_=`echo $_uuid_ | awk -F"UUID=" '{ print $2 }' | awk '{ print $1 }' | sed -e "s/\"//g"`
		_UUID_=$_uuid_
	fi
	return $_SUCCESS_	
}


#FINDS UUID INFORMATION FROM BLKID INFORMATION
function Find_UUID_BLKID
{
	_UUID_=""
	_partition_name_=$1
	_uuid_=`sed -n  "/^[=]* BLKID Information - START [=]*/,/^[=]* BLKID Information - END [=]*/p" $_INPUT_CONFIG_FILE_ | grep "$_partition_name_"`
        if [ "${_uuid_}" ]; then
                _uuid_=`echo $_uuid_ | awk -F"UUID=" '{ print $2 }' | awk '{ print $1 }' | sed -e "s/\"//g"`
                _UUID_=$_uuid_
        fi
        if [ -z $_UUID_ ]; then
        	return $_FAILED_
        fi
        return $_SUCCESS_
}

#FINDS LABEL INFORMATION FROM BLKID INFORMATION
function Find_LABEL_BLKID
{
	_partition_name_=$1
	_label_=`sed -n  "/^[=]* BLKID Information - START [=]*/,/^[=]* BLKID Information - END [=]*/p" $_INPUT_CONFIG_FILE_ | grep "$_partition_name_"`
        if [ "${_label_}" ]; then
                _label_=`echo $_label_ | awk -F"LABEL=" '{ print $2 }' | awk '{ print $1 }' | sed -e "s/\"//g"`
                _LABEL_=$_label_
        fi
        if [ -z $_LABEL_ ]; then
        	return $_FAILED_
        fi
        return $_SUCCESS_
}

function Get_Partition_Name_BY_ID
{
	_partition_name_=$1
	_real_partition_name_=""
	echo "Finding disk for partition : $_by_id_ using by-id information"

	_temp_partition_name_=`echo $_partition_name_ | sed 's:\/dev\/disk\/by-id\/::g'`
	echo "temp partition name = $_temp_partition_name_"
        _real_partition_name_=`sed -n  "/^[=]* BY-ID Information - START [=]*/,/^[=]* BY-ID Information - END [=]*/p" $_INPUT_CONFIG_FILE_ | egrep -w "$_temp_partition_name_ "`
	echo "real partition name = $_real_partition_name_"
	_real_partition_name_=`echo  $_real_partition_name_ | awk -F"-> " '{ print $2 }'| sed -e "s:\.\.\/::g"`
	if [ ! "${_real_partition_name_}" ]; then
		echo "ERROR :: Failed to find sym link for partition $_partition_name_."
		return $_FAILED_
	fi
	_real_partition_name_="/dev/$_real_partition_name_"
	echo "Found sym link for partition $_real_partition_name_ , sym link = $_partition_name_"
	return 	$_SUCCESS_
}

function Get_Partition_Name_BY_UUID
{
	_partition_name_=$1
	_real_partition_name_=""
	echo "Finding disk for partition : $_by_uuid_ using by-uuid information"
	
	#NOW WE HAVE TO REMOVE /DEV/DISK/BY-UUID fROM PARTITION NAME PASSED.
	_temp_partition_name_=`echo $_partition_name_ | sed 's:\/dev\/disk\/by-uuid\/::g'`
        
	 _real_partition_name_=`sed -n  "/^[=]* BY-UUID Information - START [=]*/,/^[=]* BY-UUID Information - END [=]*/p" $_INPUT_CONFIG_FILE_ | egrep -w "$_temp_partition_name_ "`
	
	_real_partition_name_=`echo  $_real_partition_name_ | awk -F"-> " '{ print $2 }'| sed -e "s:\.\.\/::g"`
        if [ ! "${_real_partition_name_}" ]; then
                echo "ERROR :: Failed to find sym link for partition $_partition_name_."
                return $_FAILED_
        fi
	_real_partition_name_="/dev/$_real_partition_name_"
        echo "Found sym link for partition $_partition_name_ , sym link = $_real_partition_name_."
        return  $_SUCCESS_
}

function Get_partition_Name_BY_LABEL
{
	_partition_name_=$1
	_real_partition_name_=""
	echo "Finding disk for partition : $_by_label_ using by-label information"

        _temp_partition_name_=`echo $_partition_name_ | sed 's:\/dev\/disk\/by-label\/::g'`

	_real_partition_name_=`sed -n  "/^[=]* BY-LABEL Information - START [=]*/,/^[=]* BY-LABEL Information - END [=]*/p" $_INPUT_CONFIG_FILE_ | egrep -w "$_temp_partition_name_ "`

	_real_partition_name_=`echo  $_real_partition_name_ | awk -F"-> " '{ print $2 }'| sed -e "s:\.\.\/::g"`
        if [ ! "${_real_partition_name_}" ]; then
                echo "ERROR :: Failed to find sym link for partition $_partition_name_."
                return $_FAILED_
        fi
	_real_partition_name_="/dev/$_real_partition_name_"
        echo "Found sym link for partition $_partition_name_ , sym link = $_real_partition_name_."
        return  $_SUCCESS_
}
function Get_Partition_Name_BY_PATH
{
	_partition_name_=$1
	_real_partition_name_=""
	echo "Finding disk for partition : $_by_path_ using by-path information"

        _temp_partition_name_=`echo $_partition_name_ | sed 's:\/dev\/disk\/by-path\/::g'`

	_real_partition_name_=`sed -n  "/^[=]* BY-PATH Information - START [=]*/,/^[=]* BY-PATH Information - END [=]*/p" $_INPUT_CONFIG_FILE_ | egrep -w "$_temp_partition_name_ "`

	_real_partition_name_=`echo  $_real_partition_name_ | awk -F"-> " '{ print $2 }'| sed -e "s:\.\.\/::g"`
        if [ ! "${_real_partition_name_}" ]; then
                echo "ERROR :: Failed to find sym link for partition $_partition_name_."
                return $_FAILED_
        fi
	_real_partition_name_="/dev/$_real_partition_name_"
        echo "Found sym link for partition $_partition_name_ , sym link = $_real_partition_name_."
        return  $_SUCCESS_
}

function Deref_SymLink
{
	_partition_name_=$1

	_tmp_disk_by_var=`echo $_partition_name_ | grep "by-id"`
	if [ $? -eq 0 ]; then
		Get_Partition_Name_BY_ID "$_partition_name_"
		
	else
		_tmp_disk_by_var=`echo $_partition_name_ | grep "by-label"`
		if [ $? -eq 0 ]; then
			Get_Partition_Name_BY_LABEL "$_partition_name_"
		else
			_tmp_disk_by_var=`echo $_partition_name_ | grep "by-path"`
			if [ $? -eq 0 ]; then
				Get_Partition_Name_BY_PATH "$_partition_name_"
			else
				_tmp_disk_by_var=`echo $_partition_name_ | grep "by-uuid"`
				if [ $? -eq 0 ]; then
					Get_Partition_Name_BY_UUID "$_partition_name_"
				else
					echo "partition sent is a real partition."
					return $_FAILED_
				fi
			fi
		fi
	fi
	return $_SUCCESS_
}

#IT FINDS UUID. IT JUST MAKES BEST POSSIBLE COMBINATIONS TO FIND UUID FOR A DEVICE PASSED.
function Find_UUID
{
	_UUID_="" #CLEAR LAST ITERATION UUID
	_partition_name_=$1

	#FIRST CHECK IT"S UUID IN FSTAB -BLKID MAPPING TABLE
	Find_UUID_FSTABINFO "$_partition_name_"
	
	#IF NOT FOUND ABOVE,CHECK IN BLKID TABLE
	if [ -z "${_UUID_}" ]; then
		Find_UUID_BLKID "$_partition_name_"
	fi

	if [ -z "${_UUID_}" ]; then
		#FIRST WE HAVE TO DEREF SYMLINK
		Deref_SymLink "$_partition_name_"		
		if [ 0 -eq $? ]; then
			echo "Found partition name : $_real_partition_name_"
			Find_UUID_FSTABINFO "$_real_partition_name_"
			if [ ! "${_UUID_}"  ]; then
				Find_UUID_BLKID "$_real_partition_name_"
			fi
		fi
	fi

	if [ -z "${_UUID_}" ]; then
		echo "ERROR :: Failed to find UUID for partition $_partition_name_."
		return $_FAILED_
	fi
	#WE NEED TO CHOP OFF QUOTES TO UUID.
	echo "UUID = $_UUID_."
	echo "Found UUID for partition $_partition_name_, UUID = $_UUID_."
	return $_SUCCESS_
}

#THIS FUNCTION IS USED TO DETERMINE SOURCE DISK NAME ON TARGET SIDE.
function GetTargetDiskName
{
	_disk_num_=$1
	_VX_INSTALL_PATH_=$2
	_disk_name_=""
	echo "Generating disk name for disk at number $_disk_num_"
	#I need to check this Path.Whether it invokes or not.Else better it could be to pass the _VX_INSTALL_PATH_ as second input.
	_disk_name_=`$_VX_INSTALL_PATH_/bin/generate_device_name $_disk_num_`
	echo "Disk Name = $_disk_name_"
	if [ ! -z "${_disk_name_}" ]; then
		_disk_name_="/dev/$_disk_name_"
	fi
	return $_SUCCESS_ 
}

# MapDeviceHierarchyToBaseDevice takes two input arguments
# $1 - Inpout file
# $2 - Input device name
# Returns a device name 
function MapDeviceHierarchyToBaseDevice
{
	_base_diskname_=""
	dev_hierarchy=$2
	if [ ! -f $1 ]; then
		#echo "MapDeviceHierarchyToBaseDevice - input file $1 does not exists"
		return $diskname;
	fi

	exec 5<$1
	while read -u 5 -r i
	do
		echo $i |grep "DiskName.*\/dev\/" > /dev/null 2>&1
		if [ $? -eq 0 ]; then
			temp_diskname=`echo $i | awk -F'Value=' '{ print $2 }'`
			temp_diskname=`echo $temp_diskname | sed -e 's:["]*::g' | sed -e 's:/>::g'`
			if [ "${temp_diskname}" = "${dev_hierarchy}" ]; then
				_base_diskname_=$temp_diskname
				break
			fi
			#echo "Found disk name : $diskname"
		fi
		
		echo $i | grep "Parameter Name=\"Name\" Value=\"$dev_hierarchy\"" > /dev/null 2>&1
		if [ $? -eq 0 ]; then
			echo "Device hierarchy name $dev_hierarchy belongs to base device $diskname"
			_base_diskname_=$temp_diskname
			break
		fi
	done
	5<&-
	return $_SUCCESS_
}

#FINDS DEVICE NAME WITH UUID INFORMATION FROM BLKID INFORMATION
# $1 - Input UUID
# Returns a device name
function Find_Device_From_UUID
{
	_dev_name_=""
	_dev_uuid_=$1

	_uuid_=`sed -n  "/^[=]* BLKID Information - START [=]*/,/^[=]* BLKID Information - END [=]*/p" $_INPUT_CONFIG_FILE_ | grep "$_dev_uuid_"`
        if [ "${_uuid_}" ]; then
                _dev_name_=`echo $_uuid_ | awk -F':' '{print $1 }'`
        fi

	return $_SUCCESS_
}

#FINDS DEVICE NAME WITH LABEL INFORMATION FROM BLKID INFORMATION
# $1 - Input LABEL
# Returns a device name
function Find_Device_From_LABEL
{
	_dev_name_=""
	_dev_label=$1

	_label_=`sed -n  "/^[=]* BLKID Information - START [=]*/,/^[=]* BLKID Information - END [=]*/p" $_INPUT_CONFIG_FILE_ | grep "$_dev_label"`
        if [ "${_uuid_}" ]; then
                _dev_name_=`echo $_label_ | awk -F':' '{print $1 }'`
        fi

	return $_SUCCESS_
}

# FIND IF A GIVEN DEVICE IS A LOGICAL VOLUME
# $1 - Input block device
# Returns 0 or 1
function IsDevice_LV
{
	_lv_dev_name_=""
	_is_dev_lv_=$1
	for _dev_in_dmsetup in `sed -n  "/^[=]* DMESETUP TABLE Information - START [=]*/,/^[=]* DMSETUP TABLE Information - END [=]*/p" $_INPUT_CONFIG_FILE_ | awk -F':' '{ print $1 }' | grep -v "="`
	do
		is_multipath=`echo $_dev_in_dmsetup | awk '{ print $4 }'`
		if [ "${is_multipath}" = "multipath" ]; then
			continue
		fi
		_temp_vg=`echo $_dev_in_dmsetup | awk -F'-' '{ print $1 }'`	
		if [ 0 -ne $? ]; then
			echo "Device \"${_lv_dev_name_}\" is not a logical volume."
			return $_FAILED_
		fi
		
		_temp_lv=`echo $_dev_in_dmsetup | awk -F'-' '{ print $2 }'`	
		if [ 0 -ne $? ]; then
			echo "Device \"${_lv_dev_name_}\" is not a logical volume."
			return $_FAILED_
		fi
		
		_temp_dev_name1="/dev/$_temp_vg/$_temp_lv"
		_temp_dev_name2="/dev/mapper/$_temp_vg-$_temp_lv"

		if [ "${_temp_dev_name1}" = "${_is_dev_lv_}" ]; then
			_lv_dev_name_=$_temp_dev_name2
			break
		fi
		if [ "${_temp_dev_name2}" = "${_is_dev_lv_}" ]; then
			_lv_dev_name_=$_temp_dev_name1
			break
		fi
	done
	#Mean to say Device is not an LV.
	return $_FAILED_;
}

# FIND IF A GIVEN DEVICE IS A MULTIPATH DEVICE
# $1 - Input block device
# Returns 0 or 1
function IsDevice_Multipath
{
	_dev_name_=$1
	for _dev_in_dmsetup in `sed -n  "/^[=]* DMESETUP TABLE Information - START [=]*/,/^[=]* DMSETUP TABLE Information - END [=]*/p" $_INPUT_CONFIG_FILE_ | awk -F':' '{ print $1 }' | grep -v "="`
	do
		is_multipath=`echo $_dev_in_dmsetup | awk '{ print $4 }'`
		if [ "${is_multipath}" = "multipath" ]; then
			multipath_dev=`echo $_dev_in_dmsetup | awk '{ print $1 }'`
			# Form a multipath device - TODO other naming schemes
			multipath_dev="/dev/mapper/$multipath_dev"
			if [ "${multipath_dev}" = "${_dev_name_}" ]; then
				return $_SUCCESS_
			fi
		fi
	done
	return $_FAILED_;
}
# FIND IF A GIVEN DEVICE IS PROTECTED OR NOT
# $1 - Input block device
# Returns 0 or 1
function GetProtectedDeviceEntry
{
	_dev_name_=$1
	_protected_disklist="$_VX_INSTALL_PATH_/failover_data/$_MACHINE_UUID_.PROTECTED_DISKS_LIST"
	entry=`cat $_protected_disklist | grep -n "$_dev_name_" | awk -F":" '{print $1}'`
	echo "Entry = $entry"
	if [ -z "${entry}" ]; then
		return 0
	fi
	return $entry
}

function IsDiskProtected
{
	_dev_name_=$1
	echo "checking whether device \"$_dev_name_\" is protected or not."
	_protected_disklist_="$_VX_INSTALL_PATH_/failover_data/$_MACHINE_UUID_.PROTECTED_DISKS_LIST"
	_isProtected_=`cat $_protected_disklist_ | grep "$_dev_name_"`
	echo "is protected = $_isProtected_"
	if [ -z "${_isProtected_}" ]; then
		return $_FAILED_
	fi
	return $_SUCCESS_
}

function COPY
{
	_source_=$1
	_target_=$2

	echo "Copying file \"$_source_\" to \"$_target_\"."
	/bin/cp -f $_source_ $_target_
	if [ 0 -ne $? ]; then
		return $?
	fi
	return $_SUCCESS_
}
	
function MOVE
{
	_source_=$1
	_target_=$2

	echo "Moving file \"$_source_\" to \"$_target_\"."
	/bin/mv -f $_source_ $_target_
	if [ 0 -ne $? ]; then
		return $?
	fi
	return $_SUCCESS_
}
