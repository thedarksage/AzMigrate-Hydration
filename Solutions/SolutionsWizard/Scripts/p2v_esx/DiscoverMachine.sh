#!/bin/bash
#TODO
#NEED TO ADD COPYRIGHT MESSAGE
_LOG_FILE_="LinuxP2V.log"

#redirect STDOUT and STDERR output to log file "LinuxP2V.log"
_DATE_TIME_SUFFIX_=`date +%d`_`date +%b`_`date +%Y`_`date +%H`_`date +%M`_`date +%S`
_TEMP_LOG_F_="LinuxP2V-$_DATE_TIME_SUFFIX_.log"
#exec 3>$_TEMP_LOG_F_
#exec 1>&3 #STDOUT
#exec 2>&3 #STDERR

_VERSION_="8.2"
_SCRIPTS_PATH_=$1
_OS_DETAILS_SH_F_="$_SCRIPTS_PATH_/OS_details.sh"
_CREATE_KERNEL_IMAGES_SH_F_="$_SCRIPTS_PATH_/CreateKernelImageFiles.sh"
_COLLECT_CONFIG_INFO_SH_F_="$_SCRIPTS_PATH_/CollectConfigInfo.sh"
_CONFIG_INFO_F_="$_SCRIPTS_PATH_/Configuration.info"
_Create_kernel_images_=$2
_azure_workflow=$3

#first We have to find the OS NAME.
_OS_NAME_=""
_HOSTNAME_=""

_SUCCESS_=0
_FAILED_=1

#functions#

function GetOS
{
	echo "Find OS."
        #check if the _OS_DETAILS_SH_F_ exists and executable permissions are assigned to it.
        if [ ! -f "$_OS_DETAILS_SH_F_" ] || [ ! -x "$_OS_DETAILS_SH_F_" ]; then
                echo "ERROR :: $_OS_DETAILS_SH_F_ file is missing."
                echo "ERROR :: 1. Cross check if above file exists and executable permissions are assigned."
                return $_FAILED_
        fi

        #Invoke _OS_DETAILS_SH_F_ to find OS.
        _OS_NAME_=`$_OS_DETAILS_SH_F_ "print"`
        if [ ! "${_OS_NAME_}" ]; then
                echo "ERROR :: Failed to Determine OS."
                return $_FAILED_
        fi
        echo "OS NAME=$_OS_NAME_"
	return $_SUCCESS_
}

function CreateKernelImages
{
	echo "Create Kernel Image files."
	if [ ! -f $_CREATE_KERNEL_IMAGES_SH_F_  ] || [ ! -x $_CREATE_KERNEL_IMAGES_SH_F_ ]; then
		echo "ERROR :: $_CREATE_KERNEL_IMAGES_SH_F_ file is either missing or having no permissions to execute."
		echo "ERROR :: 1. Check if above file exists and executable permissions are assigned."
		return $_FAILED_
	fi
	
	#Invoke _CREATE__KERNEL_IMAGES_SH_F_ to create/check kernel image files.
	$_CREATE_KERNEL_IMAGES_SH_F_ "$_OS_NAME_" "$_azure_workflow"
	if [ 0 -ne $? ]; then
		echo "ERROR :: Failed to verify initrd/initramfs files."
		return $_FAILED_
	fi

	echo "Successfully created/checked initrd/initramfs files."
	return $_SUCCESS_
}

function CollectConfigurationInformation
{
	echo "Collecting configuration information like fstab,device.map,menu.lst"
	if [ ! -f $_COLLECT_CONFIG_INFO_SH_F_ ] || [ ! -x $_COLLECT_CONFIG_INFO_SH_F_ ]; then
		echo "ERROR :: \"$_COLLECT_CONFIG_INFO_SH_F_\" file is either missing or having no permissions to execute."
		echo "ERROR :: 1. Check if above file exists and executable permissions are assigned."
		return $_FAILED_
	fi
	
	#Invoke _COLLECT_CONFIG_INFO_SH_F_ to collect information about fstab, menu.lst and device.map entries.
	$_COLLECT_CONFIG_INFO_SH_F_ $_SCRIPTS_PATH_ > "$_SCRIPTS_PATH_/Configuration.info"
	if [ 0 -ne $? ]; then
		echo "ERROR :: Failed to collect configuration Information."
		return $_FAILED_
	fi

	/etc/vxagent/bin/inm_dmit --fsync_file "$_CONFIG_INFO_F_"
	if [ 0 -ne $? ]; then
		echo "Failed to sync file ${_CONFIG_INFO_F_}"
	fi
	
	echo "Successfully collected configuration information."
	/bin/cp $_CONFIG_INFO_F_ /boot/
	/bin/cp $_CONFIG_INFO_F_ /etc/
	#rm -rf $_CONFIG_INFO_F_
	return $_SUCCESS_
}

function EXIT
{
	date
	echo "------- END -------"
	echo "*************************"
	3>&-
#	cat $_TEMP_LOG_F_>>$_LOG_FILE_
#	rm -rf $_TEMP_LOG_F_	
	exit $1
}

#END of Functions#

#MAIN#

echo "------- START -------"
date

_HOSTNAME_=`hostname`

echo "*****LinuxP2V-Source*****"
echo "Version=$_VERSION_"
echo "HostName=$_HOSTNAME_"
echo "Scripts Path = $_SCRIPTS_PATH_"

#call GetOS function to determine OS type of running machine.
GetOS
if [ 0 -ne $? ]; then
	EXIT $?
fi

#Create Initrd Modules
shopt -s nocasematch
if [[ "no" != "${_Create_kernel_images_}" ]]; then
	CreateKernelImages
	if [ 0 -ne $? ]; then
		EXIT $?
	fi
fi

CollectConfigurationInformation
if [ 0 -ne $? ];then
	EXIT $?
fi
	
EXIT $? 
