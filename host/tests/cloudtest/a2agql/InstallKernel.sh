#!/bin/bash

if [ $# -ne 4 ]; then
  echo "usage: $0 [Distro] [RequiredKernel] [RootPassword] [Validate]"
  exit 1
fi

Distro=$1
Validate=$2
RootPassword=$3
RequiredKernel=$4

LOGFILE="/var/log/installkernel.log"
trace_log_message()
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

trace_log_message "all params :"
trace_log_message "$0 $@"

# Currently Installed Kernel Version
CurrentKernel=$(uname -r)

trace_log_message "Current Kernel Version : $CurrentKernel, Required Kernel Version : $RequiredKernel"

# System Architecture
SystemArch=$(uname -i)
trace_log_message -q "SystemArch : $SystemArch"

RequiredKernelVer=$RequiredKernel
if [[ "$Distro" == "SLES" ]]; then
    RequiredKernelVer="${RequiredKernel}-azure"
fi
trace_log_message -q "RequiredKernelVer : $RequiredKernelVer"

if [ "$Validate" = "true" ]; then
	if [ "$CurrentKernel" = "$RequiredKernelVer" ]
	then
		trace_log_message "Kernel Version Validation is done successfully !"
		exit 0
	else
		trace_log_message -e "Kernel Version Validation Failed"
	fi
fi

# Check if System already has latest kernel installed
if [ "$CurrentKernel" = "$RequiredKernelVer" ]
then
    trace_log_message "VM is Already Updated to Required Kernel Version!"
    trace_log_message "Skipping the next steps!!!"
    sleep 2s
    exit 0
fi

rootUser="root"
echo -e "$RootPassword\n$RootPassword" | passwd "$rootUser"

if [ "$(whoami)" != "$rootUser" ]
then
	trace_log_message "Logging in as $rootUser"
    sudo su -s "$RootPassword" >> $LOGFILE 2>&1
	if [ $? -eq 0 ]; then
        trace_log_message "Successfully logged in as $rootUser"
	else
		trace_log_message -e "Failed to login as $rootUser with exit status : $?"
    fi
fi

me="$(whoami)"
trace_log_message "Logged in as $me"
trace_log_message -q "Attempting to install $RequiredKernel on $Distro"
if [[ "$Distro" == "SLES" ]]; then
	trace_log_message "Running zypper se -s 'kernel*' cmd"
	zypper se -s 'kernel*' >> $LOGFILE 2>&1
	if [ $? -eq 0 ]; then
        trace_log_message "Able to fetch installed kernels"
	else
		trace_log_message -e "Failed to fetch installed kernels list with exit status : $?"
	fi
	
	installcmd="zypper in -y kernel-azure-$RequiredKernel"
	trace_log_message "Executing cmd : $installcmd"
	$installcmd >> $LOGFILE 2>&1
	if [ $? -eq 0 ]; then
        trace_log_message "Successfully installed $RequiredKernel"
	else
		trace_log_message -e "$RequiredKernel kernel installation failed with exit status : $?"
	fi
	grubFile="/boot/grub2/grub.cfg"
elif [[ ("$Distro" == "UBUNTU" || "$Distro" == "DEBIAN") ]]; then
	cmd="apt update"
	trace_log_message "Running cmd : $cmd"
	$cmd >> $LOGFILE 2>&1
	if [ $? -eq 0 ]; then
        trace_log_message "Successfully ran apt update command"
	else
		trace_log_message "Failed to run cmd with exit status : $?"
	fi

	installCmd="apt install -y linux-image-$RequiredKernel"
	trace_log_message "Executing cmd : $installCmd"
	$installCmd >> $LOGFILE 2>&1
	if [ $? -eq 0 ]; then
        trace_log_message "Successfully installed $RequiredKernel"
	else
		trace_log_message -e "$RequiredKernel kernel installation failed with exit status : $?"
	fi
	grubFile="/boot/grub/grub.cfg"
else
	trace_log_message -e "Unsupported $Distro is found!!"
fi

minVer=$(echo -e $RequiredKernel"\n"$CurrentKernel|sort -V|head -n 1)

trace_log_message "minVer : $minVer"
if [ "$minVer" = "$RequiredKernel" ];then
	trace_log_message "Installed Version is higher than Expected, need grub default entry update."
	trace_log_message "grubFile : $grubFile"
	if [[ -f "$grubFile" ]]; then
		trace_log_message "$grubFile exists."
	else
		trace_log_message -e "$grubFile doesn't exists."
	fi
				
	readarray -t my_array < <( awk -F\' '$1=="menuentry " || $1=="submenu " {print i++ " : " $2}; /\tmenuentry / {print i-1">"j++ " : " $2};' $grubFile)
	if [ $? -eq 0 ]; then
		trace_log_message "Able to get menuentries from $grubFile"
	else
		trace_log_message -e "Failed to get menuentries from $grubFile"
	fi
	printf '%s\n' "${my_array[@]}"
	bootEntry=$(printf '%s\n' "${my_array[@]}" | grep ${RequiredKernel} | grep -v recovery | sed 's/ //g' | awk -F: '{print $1}')
	if [ $? -eq 0 ]; then
		trace_log_message "Able to fetch menuentry $bootEntry of $RequiredKernel from $grubFile"
	else
		trace_log_message -e "Failed to fetch menuentry of $RequiredKernel from $grubFile"
	fi
	
	if [ ! -z "$bootEntry" ]
		then
		defKernel=`grep GRUB_DEFAULT /etc/default/grub`
		trace_log_message "GRUB_DEFAULT value: ${defKernel}"

		trace_log_message -q "Taking backup of /etc/default/grub file"
		cp /etc/default/grub /etc/default/grub_org
		if [ $? -eq 0 ]; then
			trace_log_message "Successfully copied grub file"
		else
			trace_log_message -e "Failed to copy /etc/default/grub file"
		fi
		
		trace_log_message "Modifying GRUB_DEFAULT value to $bootEntry in /etc/default/grub"
		sudo sed -i  's/GRUB_DEFAULT=0/GRUB_DEFAULT="'"${bootEntry}"'"/g' /etc/default/grub >> $LOGFILE 2>&1
		if [ $? -eq 0 ]; then
			trace_log_message "Successfully updated GRUB_DEFAULT entry"
		else
			trace_log_message -e "Failed to update GRUB_DEFAULT entry"
		fi
		
		defKernel=`grep GRUB_DEFAULT /etc/default/grub`
		trace_log_message "GRUB_DEFAULT value after editing: $defKernel"
	else
		trace_log_message -e "Failed to fetch menuentry from $grubFile"
	fi
else
    trace_log_message "Required version is greater than installed version, so grub default entry update is not required"
fi

if [[ "$Distro" == "UBUNTU" ]]; then
	sudo update-grub >> $LOGFILE 2>&1
	if [ $? -eq 0 ]; then
		trace_log_message "Successfully executed update-grub command"
	else
		trace_log_message -e "Failed to run update-grub command"
	fi
elif [[ "$Distro" == "SLES" ]]; then
	grub2-mkconfig -o /boot/grub2/grub.cfg >> $LOGFILE 2>&1
	if [ $? -eq 0 ]; then
		trace_log_message "Successfully executed grub2-mkconfig -o /boot/grub2/grub.cfg command"
	else
		trace_log_message -e "Failed to run grub2-mkconfig -o /boot/grub2/grub.cfg command"
	fi
fi

exit 0
