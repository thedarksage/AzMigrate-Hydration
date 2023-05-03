#!/bin/bash
#
# Copyright (c) 2012 InMage
# This file contains proprietary and confidential information and
# remains the unpublished property of InMage. Use, disclosure,
# or reproduction is prohibited except as permitted by express
# written license aggreement with InMage.
#
# File       : vCon service script
#
# Revision - history 
#
#
#
# Description:
# vCon service script to accomplish tasks for vCon workflow
#

PATH=/bin:/sbin:/usr/sbin:/usr/bin:$PATH
export PATH
RETVAL=1
vcon_action_file="/etc/vxagent/vcon/p2v_actions"
DATE_STRING=`date +'%b_%d_%H_%M_%S'`
CDROM_DEVICE="/dev/cdrom"
DVDROM="/dev/dvd"
DVDROM1="/dev/dvd1"
DVDROM2="/dev/sr0"
VMWARETOOLS_DEVICE=""
TEMP_DIR="/tmp/install_vmware_tools_vcon_${DATE_STRING}"
VMWARE_MNT_PATH="/tmp/install_vmware_tools_vcon_${DATE_STRING}/mount-path"
VMWARE_INSTALL_SCRIPT=""
VMWARE_UNINSTALL_SCRIPT="vmware-uninstall-tools.pl"
VMWARE_INSTALL_SCRIPT_OPTIONS="--default"
OS=""
_WORKFLOW_=""
_VMWARETOOLS_INSTALL_=""
_INITRD_=""
_VMWARETOOLS_UNINSTALL_=""
_NEW_GUID_=""
vx_platform_file_path="/etc/vxagent/usr/vxplatformtype.dat"
VXPLATFORMTYPE=""
CSTYPE=""

vx_dir=""
if [ -f /usr/local/.vx_version ]; then
    vx_dir=`grep INSTALLATION_DIR /usr/local/.vx_version | awk -F'=' '{print $2 }'`
else
    if [ -d /usr/local/ASR/Vx ]; then
        vx_dir=/usr/local/ASR/Vx
    else
        echo "Microsoft Azure Site Recovery product is not installed on the system. Exiting."
        return 1
    fi
fi

function ReadInputFile
{	
	if [ ! -f $vcon_action_file ] || [ ! -r $vcon_action_file  ]; then
		echo "ERROR :: Could not find file \"$vcon_action_file\"."
		echo "ERROR :: Boot up script actions will not executed."
		return $_FAILED_
	fi
	
	_WORKFLOW_=`grep workflow $vcon_action_file | awk -F= '{ print $2 }'`
	_WORKFLOW_=`echo $_WORKFLOW_ | sed -e 's: ::g'`
	_VMWARETOOLS_INSTALL_=`grep -w install_vmware_tools $vcon_action_file | awk -F= '{ print $2 }'`
	_VMWARETOOLS_INSTALL_=`echo $_VMWARETOOLS_INSTALL_ | sed -e 's: ::g'`
	_VMWARETOOLS_UNINSTALL_=`grep uninstall_vmware_tools $vcon_action_file | awk -F= '{ print $2 }'`
	_VMWARETOOLS_UNINSTALL_=`echo $_VMWARETOOLS_UNINSTALL_ | sed -e 's: ::g'`
	_INITRD_=`grep create_initrd_images $vcon_action_file | awk -F= '{ print $2 }'`
	_NEW_GUID_=`grep new_guid $vcon_action_file | awk -F= '{ print $2 }'`
	
	echo "WORKFLOW              = $_WORKFLOW_"
	echo "VMWARETOOLS INSTALL   = $_VMWARETOOLS_INSTALL_"
	echo "INITRD                = $_INITRD_"
	echo "VMWARETOOLS UNINSTALL = $_VMWARETOOLS_UNINSTALL_"
	echo "NEW GUID              = $_NEW_GUID_"
	return $_SUCCESS_
}

function ReadVxPlatformTypeFile
{
	if [ ! -f $vx_platform_file_path ] || [ ! -r $vx_platform_file_path ] || [ ! -s $vx_platform_file_path ]; then
		echo "ERROR :: Could not find file \"$vx_platform_file_path\"."
		echo "ERROR :: Boot up script actions will not executed."
		return $_FAILED_
	fi

	VXPLATFORMTYPE=`grep  VMPLATFORM $vx_platform_file_path | awk -F= '{ print $2 }'`
	CSTYPE=`grep CS_TYPE $vx_platform_file_path | awk -F= '{ print $2 }'`

	echo "VXPLATFORMTYPE	= $VXPLATFORMTYPE"
	echo "CSTYPE		= $CSTYPE"
	return $_SUCCESS_
}

function install_vmware_tools_vcon
{
    retval=1
    is_rpm_file=0
    is_targz_file=0
    install_filename=""

    if [ -f /usr/bin/vmware-config-tools.pl ]; then
        echo "VMware Tools are already installed."
        retval=0
        return $retval
    fi

    # Mount cdrom
    mkdir -p $TEMP_DIR
    if [ $? -ne 0 ]; then
        echo "Failed to create directory ${TEMP_DIR}"
        return $retval
    fi
    mkdir -p ${VMWARE_MNT_PATH}
    mount -t iso9660 -o ro ${VMWARETOOLS_DEVICE} ${VMWARE_MNT_PATH} 2>&1
    if [ $? -eq 0 ]; then
        echo "Mount ${VMWARETOOLS_DEVICE} to ${VMWARE_MNT_PATH} succeeded"
    else
        echo "Mount ${VMWARETOOLS_DEVICE} to ${VMWARE_MNT_PATH} failed"
        return $retval
    fi
    
    ls -l ${VMWARE_MNT_PATH} |  grep -i "VMwareTools"| grep ".rpm$"
    if [ $? -eq 0 ]; then
        is_rpm_file=1
    fi
    ls -l ${VMWARE_MNT_PATH} |  grep -i "VMwareTools"| grep ".tar.gz$"
    if [ $? -eq 0 ]; then
        is_targz_file=1
    fi
    
    if [ $is_targz_file -eq 1 ]; then   
        install_filename=`ls ${VMWARE_MNT_PATH}/VMwareTools*tar.gz`
        cp $install_filename ${TEMP_DIR}
        install_filename=`ls ${TEMP_DIR}/VMwareTools*tar.gz`
        #untar it.
        tar zxf "${install_filename}" -C "${TEMP_DIR}"  2>&1 > /dev/null
        if [ 0 -ne $? ]; then
            echo "Failed to untar file ${install_filename} under path ${TEMP_DIR}."
            umount ${VMWARE_MNT_PATH} 2>&1
            return $retval
        fi
        VMWARE_INSTALL_SCRIPT="${TEMP_DIR}/vmware-tools-distrib/vmware-install.pl"
    else
        if [ $is_rpm_file -eq 1 ]; then   
            install_filename=`ls ${VMWARE_MNT_PATH}/VMwareTools*rpm`
            # install rpm
            rpm -Uvh $install_filename 2>&1 /dev/null
            if [ 0 -ne $? ]; then
                echo "Failed to install rpm ${install_filename} under path ${TEMP_DIR}."
                umount ${VMWARE_MNT_PATH} 2>&1
                return $retval
            fi
            VMWARE_INSTALL_SCRIPT="vmware-config-tools.pl" 
        fi
    fi
    # Install VMWare tools
    ${VMWARE_INSTALL_SCRIPT} ${VMWARE_INSTALL_SCRIPT_OPTIONS} 2>&1
    if [ 0 -ne $? ]; then
        echo  "Failed to install VMware tools."
        umount ${VMWARE_MNT_PATH} 2>&1
        return $retval
    fi
    #umount
    umount ${VMWARE_MNT_PATH} 2>&1
    if [ 0 -eq $? ]; then
        rm -rf ${TEMP_DIR}
    fi    

    # Sucess now set return value
    retval=0

    return $retval
}

function UpdateHostID
{
    #Update HostId value in drScout.conf file for VX agent.
    currentHostIdValue=`cat ${vx_dir}/etc/drscout.conf | grep "HostId"`
    currentHostIdValue=$(printf "%s\n" "${currentHostIdValue}" | sed 's/[][\.*^$(){}?+|/]/\\&/g')
    echo "VX: Old HostId Value : ${currentHostIdValue}, New HostId : ${_NEW_GUID_}."
    sed -i "s/${currentHostIdValue}/HostId=${_NEW_GUID_}/" "${vx_dir}/etc/drscout.conf"
    #restart vxagent. service name vxagent.
    #service vxagent restart

    return 0
}

function findostype
{
	os_built_for=`grep OS_BUILT_FOR /usr/local/.vx_version | awk -F= '{ print $2 }'`

	case $os_built_for in
   	    RHEL7*)
            OS=RHEL7
        ;;
    	UBUNTU-14*)
            OS=UBUNTU14
    	;;
		DEBIAN*)
            OS=DEBIAN
        ;;
		SLES12*)
            OS=SLES12
        ;;
        OL7*)
            OS=OL7
        ;;
	esac
 } 

function uninstall_vmwtools
{
    echo "Skipping vmware tools uninstall"
    return 0

    #echo "uninstalling VMware tools."
    #${VMWARE_UNINSTALL_SCRIPT}  2>&1
    #return $?
}

# Main entry point 

#Reads the p2v_actions file.
ReadInputFile

#Read the platform type
ReadVxPlatformTypeFile

# Create the initrd images to incorporate the protection of Full Disk that
# root filesystem. Calling the script inm_mkinitrd_sles.sh will do this job.
#
# To triiger this action, the action "create_mkinitrd_images=1" should be
# present in the p2v_actions file.

if [ "$_INITRD_" = "1" ]; then
    sed -i "s:create_initrd_images*=*1:create_initrd_images=0:" ${vcon_action_file}
fi

case "${_WORKFLOW_}" in

    p2v)
        # Find the OS for which the agents built for.
        findostype
        
        # For UBUNTU-14, RHEL7, SLES12 do not install/uninstall vmware tools. 
        # Note: For UBUNTU-14 the open-vm-tools are recomended.
        if [ "$OS" = "UBUNTU14" ] || [ "$OS" = "RHEL7" ] || [ "$OS" = "DEBIAN" ] || [ "$OS" = "SLES12" ] || [ "$OS" = "OL7" ]; then
             echo "For $OS, installing/uninstalling vmware tools will not be done by this workflow."
			 
             echo "Reseting the install/uninstall options in p2v_actions file."
             sed -i "s:uninstall_vmware_tools*=*1:uninstall_vmware_tools=0:" ${vcon_action_file} 
             sed -i "s:vmware_tools_install*=*1:vmware_tools_install=0:" ${vcon_action_file}
			 
        # Un-Install VMWare tool if uninstall_vmware_tools=1 in p2v_actions file
        elif [ "1" = "$_VMWARETOOLS_UNINSTALL_" ]; then
            uninstall_vmwtools
            if [ 0 -ne $? ]; then
                 echo  "Failed to uninstall VMware tools."
                 RETVAL=1
            else
                 sed -i "s:uninstall_vmware_tools*=*1:uninstall_vmware_tools=0:" ${vcon_action_file}
            fi
        # Install VMWare tool if vmware_tools_install=1 in p2v_actions file
        elif [ "1" = "$_VMWARETOOLS_INSTALL_" ]; then
            # install vmware tools on recovered machine here
            if [ ! -b ${CDROM_DEVICE} ]; then
                echo "Device ${CDROM_DEVICE} is not a block device"
                if [ ! -b ${DVDROM} ]; then
                    echo "Device ${DVDROM} is not a block device"
                    if [ ! -b ${DVDROM1} ]; then
                        echo "Device ${DVDROM1} is not a block device"
                        if [ ! -b ${DVDROM2} ]; then
                            echo "Device ${DVDROM2} is not a block device"
                        else
                            VMWARETOOLS_DEVICE="${DVDROM2}"
                        fi
                    else
                        VMWARETOOLS_DEVICE="${DVDROM1}"
                    fi
                else
                    VMWARETOOLS_DEVICE="${DVDROM}"
                fi
            else
                VMWARETOOLS_DEVICE="${CDROM_DEVICE}"
            fi

            install_vmware_tools_vcon
            if [ $? -ne 0 ]; then
                VMWARETOOLS_DEVICE=""
                for temp_device in `ls /dev/hd*`
                do
                   ide_device=`echo $temp_device | sed -e "s:/dev/::"`
                   ide_device_removable=`cat /sys/block/$ide_device/removable`
                   if [ "${ide_device_removable}" = "1" ]; then
                       VMWARETOOLS_DEVICE=$temp_device
                   fi
                done
                if [ ! -z "${VMWARETOOLS_DEVICE}" ]; then
                    install_vmware_tools_vcon
                    if [ $? -eq 0 ]; then
                        echo "VMWare Tools installation succeeded"
                        sed -i "s:vmware_tools_install*=*1:vmware_tools_install=0:" ${vcon_action_file}
                    else
                        echo "VMWare Tools installation failed"
                    fi
                fi
            else
                echo "VMWare Tools installation succeeded"
                sed -i "s:vmware_tools_install*=*1:vmware_tools_install=0:" ${vcon_action_file}
            fi
        fi

        # Reset HostId with give new hostId from p2v_actions file
        if [ "${VXPLATFORMTYPE}" == "VmWare" ] && [ "${CSTYPE}" != "CSPrime" ] && [ ! -z ${_NEW_GUID_} ]; then
            echo "Found new host GUID value. Going to update HostId value in drScout.conf and Config.ini"
            UpdateHostID
            if [ $? -ne 0 ]; then
                echo "failed to update host id."
            else
                sed -i "s:new_guid*=*${_NEW_GUID_}:new_guid=:" ${vcon_action_file}
            fi
        fi
        RETVAL=0
        ;;

    v2v)
        RETVAL=0
        ;;

    v2p)
    #we have to uninstall VMware tools.
        RETVAL=0
    echo "Uninstall VMware Tools = $_VMWARETOOLS_UNINSTALL_"
    if [ 1 -eq $_VMWARETOOLS_UNINSTALL_ ]; then
        echo "uninstalling VMware tools."
            ${VMWARE_UNINSTALL_SCRIPT}  2>&1
        if [ 0 -ne $? ]; then
             echo  "Failed to uninstall VMware tools."
                 RETVAL=1
        else
                     sed -i "s:uninstall_vmware_tools*=*1:uninstall_vmware_tools=0:" ${vcon_action_file}
        fi
    fi
        ;;

    *)
        echo "Invalid workflow:$_WORKFLOW_ found"
        exit $RETVAL
        ;;
esac

