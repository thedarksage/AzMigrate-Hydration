#!/bin/bash -x

# File: vmware-configure-network.sh
#
# Description: 
#    This script has the functionality to make network changes to failback/failovre VM comming to VMWare environment.
#        
# TODO: Currently this script makes network changes to configure DHCP for the NICs
#       When Static IP requirement comes then the logic should be added to this file.
#
# Usage: 
#   vmware-configure-network.sh <VX-Install-Path> <Recovering-VM-Root-Mountpoint>
#

#
# Variable initialization section.
#
export VX_PATH=""
export ROOT_MNT_PATH=""
export AZURE_ORG="-AZURE-ORG"
export INMAGE_ORG="-INMAGE-ORG"
export _INMAGE_ORG_="-INMAGE_ORG"
export _INMAGE_MOD_="-INMAGE_MOD"

_SUCCESS_=0
_FAILED_=1

#
# Functions defition section
#

function Usage()
{
    echo "vmware-configure-network.sh <Recovering-VM-Root-Mountpoint>"
}
function Trace()
{
    echo $1
}
function DirExist()
{
    if [ -d $1 ]; then
        return $_SUCCESS_
    else
        Trace "Error: $1 does not exist"
        return $_FAILED_
    fi
}

function FileExist()
{
    if [ -f $1 ]; then
        return $_SUCCESS_
    else
        Trace "Error: $1 does not exist"
        return $_FAILED_
    fi

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

function Restore_INITRD_RAMFS_Files
{
    #FIRST LIST ALL PATTERNS WHICH ARE IN THE FORM OF ".INMAGE_ORG"
    _boot_path_=$1
    _orig_initrd_ramfs_files_=`find $_boot_path_ -name "init*${_INMAGE_ORG_}"`
    
    echo "initrd files = $_orig_initrd_ramfs_files_"
    for _orig_initrd_ramfs_file_ in ${_orig_initrd_ramfs_files_}
    do
        #WE HAVE TO USE SAFE PATTERN
        if [ ! -f "${_orig_initrd_ramfs_file_}" ]; then
            echo "ERROR :: Failed to find file \"${_orig_initrd_ramfs_file_}\". Failed to change it back to original file."
            continue
        fi
        
        safe_pattern=$(printf "%s\n" "${_INMAGE_ORG_}" | sed 's/[][\.*^$(){}?+|/]/\\&/g')
        _initrd_ramfs_file_=`echo $_orig_initrd_ramfs_file_ | sed "s/${safe_pattern}$//g"`
        echo "Initrd file = $_initrd_ramfs_file_"
        
        #TAKE COPY OF THIS FILE
        COPY "$_initrd_ramfs_file_" "${_initrd_ramfs_file_}${_INMAGE_MOD_}"
        if [ 0 -ne $? ]; then
		    echo "Could not backup current initrd image"
            #return $_FAILED_
        fi				
        MOVE "$_orig_initrd_ramfs_file_" "$_initrd_ramfs_file_"
		if [ 0 -ne $? ]; then
		    echo "Could not rename original initrd image"
            return $_FAILED_
        fi
    done

	return $_SUCCESS_
}

function GetVxInstallPath()
{
    if [ -f /usr/local/.vx_version ]; then
        VX_PATH=`grep INSTALLATION_DIR /usr/local/.vx_version | awk -F'=' '{print $2 }'`
    else
        if [ -d /usr/local/ASR/Vx ]; then
            $VX_PATH=/usr/local/ASR/Vx
        else
            echo ""
            echo "InMage Scout product is not installed on the system. Exiting!"
            echo ""
            return $_FAILED_
        fi
    fi

    Trace "VX Install Path: $VX_PATH"
    return $_SUCCESS
}

function ConfigureNetwork()
{
    if [ $# -ne 1 ]; then
        Trace "Argument Error:: ConfigureNetwork: os name argument is missing"
        return $_FAILED_
    else
        os_name=$1
    fi

    #
    # Common functionality
    # ====================
    
    # set marking file for boot-up script
    touch $ROOT_MNT_PATH/etc/vxagent/setazureip

    # Remove persistent network rules
    # TODO: Investigate for static IP, below rules files may need modifications instead of removing it.
    /bin/rm -f $ROOT_MNT_PATH/etc/udev/rules.d/*-persistent-net.rules
    /bin/rm -f $ROOT_MNT_PATH/lib/udev/rules.d/*-persistent-net-generator.rules


    #
    # OS Specific functionality
    # =========================
    case $os_name in
        RHEL6*)
            hostname=`grep HOSTNAME  $ROOT_MNT_PATH/etc/sysconfig/network | awk -F= '{ print $2 }'`
            hostname=`echo $hostname | sed -e 's: ::g'`
            
            if [ ! ${hostname} ]; then
                hostname=localhost
            fi
            
            # Uninstall Azure Linux Agent
            /bin/rpm --root $ROOT_MNT_PATH -e --nodeps WALinuxAgent

            /bin/cp -f $ROOT_MNT_PATH/etc/sysconfig/network $ROOT_MNT_PATH/etc/vxagent/logs/network"${AZURE_ORG}"
            /bin/cp -f $VX_PATH/scripts/azure/network $ROOT_MNT_PATH/etc/sysconfig/network
            chown --reference=$ROOT_MNT_PATH/etc/vxagent/logs/network"${AZURE_ORG}" $ROOT_MNT_PATH/etc/sysconfig/network
            chmod --reference=$ROOT_MNT_PATH/etc/vxagent/logs/network"${AZURE_ORG}" $ROOT_MNT_PATH/etc/sysconfig/network
            sed -i --follow-symlinks  "/HOSTNAME*=*/c HOSTNAME=${hostname}"  $ROOT_MNT_PATH/etc/sysconfig/network
            echo "GATEWAYDEV=eth0" >> $ROOT_MNT_PATH/etc/sysconfig/network
            ;;
        OL6*)
            hostname=`grep HOSTNAME  $ROOT_MNT_PATH/etc/sysconfig/network | awk -F= '{ print $2 }'`
            hostname=`echo $hostname | sed -e 's: ::g'`
            
            if [ ! ${hostname} ]; then
                hostname=localhost
            fi

            # Uninstall Azure Linux Agent
            /bin/rpm --root $ROOT_MNT_PATH -e --nodeps WALinuxAgent

            /bin/cp -f $ROOT_MNT_PATH/etc/sysconfig/network $ROOT_MNT_PATH/etc/vxagent/logs/network"${AZURE_ORG}"
            /bin/cp -f $VX_PATH/scripts/azure/network $ROOT_MNT_PATH/etc/sysconfig/network
            chown --reference=$ROOT_MNT_PATH/etc/vxagent/logs/network"${AZURE_ORG}" $ROOT_MNT_PATH/etc/sysconfig/network
            chmod --reference=$ROOT_MNT_PATH/etc/vxagent/logs/network"${AZURE_ORG}" $ROOT_MNT_PATH/etc/sysconfig/network
            sed -i --follow-symlinks  "/HOSTNAME*=*/c HOSTNAME=${hostname}"  $ROOT_MNT_PATH/etc/sysconfig/network
            ;;
        SLES11*)
            /bin/mv -f $ROOT_MNT_PATH/etc/resolv.conf $ROOT_MNT_PATH/etc/resolv.conf"${AZURE_ORG}"
            /bin/touch  $ROOT_MNT_PATH/etc/resolv.conf
            chown --reference=$ROOT_MNT_PATH/etc/resolv.conf"${AZURE_ORG}" $ROOT_MNT_PATH/etc/resolv.conf
            chmod --reference=$ROOT_MNT_PATH/etc/resolv.conf"${AZURE_ORG}" $ROOT_MNT_PATH/etc/resolv.conf
            /bin/mv -f $ROOT_MNT_PATH/etc/sysconfig/network/ifroute-eth0 $ROOT_MNT_PATH/etc/sysconfig/network/ifroute-eth0"${AZURE_ORG}"
            /bin/mv -f $ROOT_MNT_PATH/etc/sysconfig/network/routes $ROOT_MNT_PATH/etc/sysconfig/network/routes"${AZURE_ORG}"
            ;;
		RHEL7* | OL7*)
		    # TODO: This is a temporary solution to restore initrd image for RHEL7/CentOS7
			#       This locgic should be handled in PrepareTarget.sh file while processing
			#       boot partition configuration files.
		    echo "Restoring initrd images ..."
		    Restore_INITRD_RAMFS_Files "$ROOT_MNT_PATH/boot"
			if [ 0 -ne $? ]; then
				echo "Error: Could not restore initrd image"
				return $_FAILED_
			fi
            ;;
        *)
            echo "OS Type: $os_name. Network configuration is not performed for this OS type."
            return $_SUCCESS_;;
    esac

    return $_SUCCESS_
}

function StartWork()
{
    GetVxInstallPath
    if [ $? -ne $_SUCCESS_ ]; then
        exit 1
    fi    

    FileExist $VX_PATH/scripts/azure/OS_details_target.sh
    if [ $? -ne $_SUCCESS_ ]; then
        exit 1
    fi

    os_name=`$VX_PATH/scripts/azure/OS_details_target.sh $ROOT_MNT_PATH $VX_PATH`
    if [ $? -ne $_SUCCESS_ ]; then
        Trace "Error: Could not determine source OS name"
        exit 1
    fi

    ConfigureNetwork $os_name
    if [ $? -ne $_SUCCESS_ ]; then
        exit 1
    fi
}

#
# Commandline arguments validation
#
if [ $# -ne 1 ]; then
    echo "Invalid no of arguments: $#"
    Usage
    exit 1
else
    DirExist $1
    if [ $? -ne $_SUCCESS_ ]; then
        exit 1
    fi
    ROOT_MNT_PATH=$1
fi

Trace "Root mount-path: $ROOT_MNT_PATH"


#
# Start work
#
StartWork
