#!/bin/bash

OS_BUILT_FOR=""

# Detect the OS from agent version file: /usr/local/.vx_version
FindOSType() 
{
	OS_BUILT_FOR=`grep OS_BUILT_FOR /usr/local/.vx_version | awk -F= '{ print $2 }'`
}

ModifyWaagentConfig()
{
    # Set: Swap configuration
	sed -i --follow-symlinks  "/ResourceDisk.EnableSwap[[:space:]]*=[[:space:]]*/c ResourceDisk.EnableSwap=y"  /etc/waagent.conf
    sed -i --follow-symlinks  "/ResourceDisk.SwapSizeMB[[:space:]]*=[[:space:]]*/c ResourceDisk.SwapSizeMB=32768"  /etc/waagent.conf
	
	# Set: do not delete password
	sed -i --follow-symlinks  "/Provisioning.DeleteRootPassword[[:space:]]*=[[:space:]]*/c Provisioning.DeleteRootPassword=n" /etc/waagent.conf
	
	# Set: do not regenerate ssh keys
	sed -i --follow-symlinks  "/Provisioning.RegenerateSshHostKeyPair[[:space:]]*=[[:space:]]*/c Provisioning.RegenerateSshHostKeyPair=n" /etc/waagent.conf
}

InstallWALinuxAgent()
{
    # Download WALinuxAgent file
	echo "Downloading waagent ..."
	
	_waagent_file_link="https://raw.githubusercontent.com/Azure/WALinuxAgent/2.0/waagent"
	_waagent_download_file="/etc/vxagent/waagent"
	
	if [ -f "$_waagent_download_file" ]; then
	    echo "A file with name $_waagent_download_file already exist. Deleting it ..."
		/bin/rm -f "$_waagent_download_file"
	fi
	
	wget -O /etc/vxagent/waagent "$_waagent_file_link"
	retcode=$?
	if [ $retcode -ne 0 ] ; then
        Trace_Error "Error downloading file from: $_waagent_file_link. Error $retcode"
        return 1
    fi
	
	# Install WALinuxAgent
	echo "Installing waagent ..."
	/bin/cp -f /etc/vxagent/waagent /usr/sbin/waagent
	chmod 755 /usr/sbin/waagent
	/usr/sbin/waagent -install -verbose
	
	# Modify config options
	echo "Modifying waagent configuration ..."
	ModifyWaagentConfig
	
	# Start waagent
	echo "Starting the waagent daemon ..."
	service waagent start
	
	return 0
}

UpdateGrub_Rhel7()
{
	echo "### Grub options BEGIN ###"
	cat /etc/default/grub
	echo "### Grub options END ###"
	
	echo "Updating the grub ..."
	grub2_efi_path="/boot/efi/efi/redhat/grub.cfg"
	if [[ -f "$grub2_efi_path" ]]; then
		grub2-mkconfig -o "$grub2_efi_path"
	else
		grub2-mkconfig -o /boot/grub2/grub.cfg
	fi
}

UpdateGrub_Ubuntu14()
{
	echo "### Grub options BEGIN ###"
	cat /etc/default/grub
	echo "### Grub options END ###"
	
	echo "Updating the grub ..."
	update-grub
}

UpdateGrub()
{
	case $OS_BUILT_FOR in
	RHEL7* | SLES12* | OL7* | RHEL8* | SLES15* | OL8* | RHEL9* | OL9*)
	    UpdateGrub_Rhel7
	;;
	UBUNTU*)
	    UpdateGrub_Ubuntu14
	;;
	DEBIAN*)
	    UpdateGrub_Ubuntu14
	;;
	*)
	    echo "Un-supported OS $OS_BUILT_FOR for grub update"
	;;
	esac
}

main()
{
    FindOSType
	
    UpdateGrub
	
#	InstallWALinuxAgent
}

echo "Prepare For Azure Start Time: $(date +"%a %b %d %Y %T")"
main
echo "Prepare For Azure End Time: $(date +"%a %b %d %Y %T")"
