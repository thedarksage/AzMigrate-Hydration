#!/bin/bash

##+----------------------------------------------------------------------------------+
##            Copyright(c) Microsoft Corp. 2021
##+----------------------------------------------------------------------------------+
## File         :   InstallLinuxGuestAgent.sh
##
## Description  :   Script invoked by asr.installga service on Migrated/Failed-over VM.
##                  Executes steps to install guest agent on the VM.
##
## History      :   29-01-2021 (Shantanu Mishra) - Created
##
## Usage        :   bash InstallLinuxGuestAgent.sh <Linux_Distro_Name> <Folder_Path> <install_setuptools>
##+----------------------------------------------------------------------------------+

# Region: Global Variables

waagent_servicename="waagent"

# EndRegion

#Region: Functions

unset service_binary
function GetServiceBinaryName
{
    service_binary="systemctl"
    
    ret=$(systemctl)
    if [[ $? -ne 0 ]]; then    
		echo "systemctl is not present on the system."
		echo "Using service commands to run script."
		service_binary="service"
    fi
}

#$1: Operation to perform the service
#$2: Service Name
function RunServiceCommand
{
    if [[ $service_binary == "systemctl" ]]; then
        systemctl $1 $2
    else
        service $2 $1
    fi
}

unset pythonversion
# Check for the version of python
function CheckValidPythonVersion
{
    # Check for Major Version 3 for python.
    python3 --version
    if [[ $? -ne 0 ]] ; then
        echo "Python3 is not installed on the VM."
    else
        pythonversion=3
		return
    fi
    
    # Check for Major version 2 and Minor Version >= 6 for Python.
    python --version
    if [[ $? -ne 0 ]] ; then        
        echo "Failed to install Linux Guest Agent on the VM. "
        echo "Install python version 2.6+ to install Guest Agent. "
        exit 0
    else
        vermajor=$(python -c"import platform; major, minor, patch = platform.python_version_tuple(); print(major)")
        verminor=$(python -c"import platform; major, minor, patch = platform.python_version_tuple(); print(minor)")
        if [ $vermajor -eq 2 ] && [ $verminor -ge 6 ]; then
            pythonversion=2
			return
        else
            echo "Failed to install Linux Guest Agent on the VM. "
            echo "Install python version 2.6+ to install Guest Agent. "
            exit 0
        fi
    fi    

    exit 0
}

unset gaRunningState
function CheckGuestAgentRunningState
{
    gaRunningState=1
    RunServiceCommand stop $1
    RunServiceCommand start $1
    sleep 60
    checkAzureGAInstalled=$(RunServiceCommand status $waagent_servicename)

    if [[ $checkAzureGAInstalled == *"active"* ]] && [[ $checkAzureGAInstalled == *"running"* ]]; then
        echo "$1 Linux Guest Agent is already installed on the VM and is in running state. "
        gaRunningState=0
        return
    elif [[ $checkAzureGAInstalled == *"is running"* ]]; then
        gaRunningState=0
        echo "$1 Linux Guest Agent is already installed on the VM and is in running state. "
        return
    else
        gaRunningState=1
        return
    fi
}

# Get the guest agent service name
# The name is conventionally walinuxagent for Debian/Ubuntu, and waagent for other distributions. 
function GetWALinuxAgentServiceName
{
    if [[ -f '/usr/lib/systemd/system/walinuxagent.service' ]] || [[ -f '/lib/systemd/system/walinuxagent.service' ]]; then 
        waagent_servicename="walinuxagent"
    else
        waagent_servicename="waagent"
    fi
}

function ensure_pip_availability()
{
    if python3 -m pip --version; then
        echo "Pip is already installed."
    else
        echo "Pip is not installed. Attempting installation..."
        if python3 -m ensurepip --default-pip; then
            echo "Pip installation successful."
        else
            echo "Pip installation failed."
        fi
    fi
}

#EndRegion

# Formatting Logs
echo ""
echo ""
echo $(date)
echo ""
#End Formatting Logs

unset setup_tools_install
unset base_linuxga_dir
unset distro_module_install

linux_distro=$1
base_linuxga_dir="$2"
setup_tools_install="$3"
distro_module_install="$4"

GetServiceBinaryName
CheckValidPythonVersion
echo "Python version is $pythonversion"

# Check if ASR pushed guest agent binaries are present on the VM.
# Files maybe absent if there was a failure in downloading guest agent files from Github,
# Or if it's the second boot of the VM, before which ASR deletes the installation binaries.
if [[ ! -d "/$base_linuxga_dir/WALinuxAgentASR" ]]; then
    echo "Guest agent installation folder not found. "
    echo "This should be second boot of VM since migration to Azure, hence skipping guest agent installation. "
    exit 0
else
    echo "Guest agent installation binaries present on the VM. Installation will proceed. "
fi

GetWALinuxAgentServiceName
echo "Working Directory: $base_linuxga_dir"

 # Check if waagent folders exists
if [[ -f '/usr/sbin/waagent' ]]; then
    echo "Guest Agent is already installed on the virtual machine."

    # Try to bring the Guest Agent online. If it comes to active or running state, do not Install
    # If found otherwise (i.e. agent remains in dead or inactive state), remove the guest agent and reinstall.
    CheckGuestAgentRunningState $waagent_servicename
    
    if [[ $gaRunningState -eq 0 ]]; then
        echo "Guest agent is in running state."
        rm -f /etc/profile.d/ASRLinuxGAStartup.sh
        exit 0
    else
        # WALinuxAgent is not present in runnable state.
        # Delete the waagent folder and log files, and reinstall the agent.
        RunServiceCommand stop $waagent_servicename
        rm -rf /var/lib/waagent/
        rm -f /var/log/waagent.log
    fi
fi

if [[ $pythonversion -eq 3 ]]; then
    echo "Installing using python3"

    python3 /$base_linuxga_dir/PythonSetupPrereqs.py
    if [[ $setup_tools_install == *"install_setup_tools_true"* ]]; then
        # Try repository based installation first.
        if [[ $linux_distro == *"UBUNTU"* || $linux_distro == *"DEBIAN"* ]]; then 
            # For CentOS, Redhat python3, setuptools comes pre-installed.
            apt-get install python3-distutils python3-setuptools -y
        fi

        cd "/$base_linuxga_dir/setuptools-33.1.1"
        python3 setup.py install
    fi

    if [[ $distro_module_install == *"install_distro_module_true"* ]]; then
        ensure_pip_availability
        if python3 -m pip install distro; then
            echo "Distro package installed successfully."
        else
            echo "Installation of distro package failed."
        fi

    fi

    cd /$base_linuxga_dir/WALinuxAgentASR/WALinuxAgent-master
    python3 setup.py install --register-service --force

    # This is for backward compatibility for Guest agent installation. The service file may exist at 2 locations and 2 possible names.
    # There are some guest agent installation binaries which fail to replace python version with the appropriate python version (3) after installation.
    # Replace python with python3 for the python path. The space after ExecStart=/usr/bin/python ensures that 
    # if python3 is already added in the path, python string won't be replaced.
    cat bin/waagent | sed 's_#!/usr/bin/env python_#!/usr/bin/env python3_' > /usr/sbin/waagent
    if [ -f '/usr/lib/systemd/system/waagent.service' ]; then
        cat /usr/lib/systemd/system/waagent.service | sed -i 's_ExecStart=/usr/bin/python _ExecStart=/usr/bin/python3 _' /usr/lib/systemd/system/waagent.service
    fi
    if [ -f '/lib/systemd/system/waagent.service' ]; then 
        cat /lib/systemd/system/waagent.service | sed -i 's_ExecStart=/usr/bin/python _ExecStart=/usr/bin/python3 _' /lib/systemd/system/waagent.service
    fi
	if [ -f '/usr/lib/systemd/system/walinuxagent.service' ]; then
        cat /usr/lib/systemd/system/walinuxagent.service | sed -i 's_ExecStart=/usr/bin/python _ExecStart=/usr/bin/python3 _' /usr/lib/systemd/system/walinuxagent.service
    fi
    if [ -f '/lib/systemd/system/walinuxagent.service' ]; then 
        cat /lib/systemd/system/walinuxagent.service | sed -i 's_ExecStart=/usr/bin/python _ExecStart=/usr/bin/python3 _' /lib/systemd/system/walinuxagent.service
    fi
fi

if [[ $pythonversion -eq 2 ]]; then
     # [shamish]: To remove in next release.
    python /$base_linuxga_dir/PythonSetupPrereqs.py

    # Installing setuptools v33.1.1 using pushed package.
    # Using setuptools-33.1.1 as it support python 2.6+, which matches Linux guest agent support matrix.
    if [[ $setup_tools_install == *"install_setup_tools_true"* ]]; then
        cd "/$base_linuxga_dir/setuptools-33.1.1"
        python setup.py install
    fi

    cd /$base_linuxga_dir/WALinuxAgentASR/WALinuxAgent-master
    python setup.py install --register-service --force
fi

# Make waagent as executable file, reload system-daemon and restart waagent
chmod +x /usr/sbin/waagent
if [[ $service_binary != *"systemctl"* ]]; then
    chmod +x /etc/init.d/waagent
fi

# Refresh Guest agent binary name post installation
GetWALinuxAgentServiceName

systemctl daemon-reload # Ignorable error if systemctl is absent
CheckGuestAgentRunningState $waagent_servicename

if [[ $gaRunningState -eq 0 ]]; then
    echo "Guest Agent successfully installed."
	rm -f /etc/profile.d/ASRLinuxGAStartup.sh
else
    case $linux_distro in
    OL8*)
        CheckGuestAgentRunningState "waagent"
        dnf install -y python3-pyasn1 WALinuxAgent
        ;;
    CENTOS*|OL*)
        CheckGuestAgentRunningState "waagent"
        yum install -y python-pyasn1 WALinuxAgent
        ;;
    RHEL6*)
        subscription-manager repos --enable=rhel-6-server-extras-rpms
        CheckGuestAgentRunningState "waagent"
        yum install -y WALinuxAgent
        systemctl enable waagent.service
        ;;
    RHEL7*)
        subscription-manager repos --enable=rhel-7-server-extras-rpms
        CheckGuestAgentRunningState "waagent"
        yum install -y WALinuxAgent
        systemctl enable waagent.service
        ;;
    RHEL*)
        CheckGuestAgentRunningState "waagent"
        yum install -y WALinuxAgent
        systemctl enable waagent.service
        ;;
    UBUNTU*)
        CheckGuestAgentRunningState "walinuxagent"
        apt-get install -y cloud-init gdisk netplan.io walinuxagent
        apt install -y linux-azure linux-image-azure linux-headers-azure linux-tools-common linux-cloud-tools-common linux-tools-azure linux-cloud-tools-azure


# sudo reboot
        ;;
    DEBIAN*)
        CheckGuestAgentRunningState "walinuxagent" 
        apt-get install -y waagent
        ;;
    ROCKY*)
        CheckGuestAgentRunningState "waagent" 
        dnf install -y WALinuxAgent
        ;; 
    ALMA*)
        CheckGuestAgentRunningState "waagent" 
        dnf install -y WALinuxAgent
        ;;
    SLES*)
        CheckGuestAgentRunningState "waagent"
        
        if [ -f /etc/os-release ] && grep -q 'SLES' /etc/os-release; then
            if grep -q 'VERSION="12' /etc/os-release; then
                SUSEConnect -p sle-module-public-cloud/12/x86_64
            elif grep -q 'VERSION="15-SP1' /etc/os-release; then
                SUSEConnect -p sle-module-public-cloud/15.1/x86_64
            elif grep -q 'VERSION="15-SP2' /etc/os-release; then
                SUSEConnect -p sle-module-public-cloud/15.2/x86_64
            elif grep -q 'VERSION="15-SP3' /etc/os-release; then
                SUSEConnect -p sle-module-public-cloud/15.3/x86_64
            elif grep -q 'VERSION="15-SP4' /etc/os-release; then
                SUSEConnect -p sle-module-public-cloud/15.4/x86_64
            elif grep -q 'VERSION="15-SP5' /etc/os-release; then
                SUSEConnect -p sle-module-public-cloud/15.5/x86_64
            elif grep -q 'VERSION="15-SP6' /etc/os-release; then
                SUSEConnect -p sle-module-public-cloud/15.6/x86_64
            elif grep -q 'VERSION="15-SP7' /etc/os-release; then
                SUSEConnect -p sle-module-public-cloud/15.7/x86_64
            fi
        elif [ -f /etc/os-release ] && grep -q 'openSUSE Leap' /etc/os-release; then
            zypper ar -f https://download.opensuse.org/update/openSUSE-stable openSUSE_stable_Updates
            zypper ar -f https://download.opensuse.org/repositories/Cloud:/Tools/15.4 Cloud:Tools_15.4
            zypper ar -f https://download.opensuse.org/distribution/openSUSE-stable/repo/oss openSUSE_stable_OSS
        fi    
        
        zypper --gpg-auto-import-keys -n refresh
        zypper -n install python-azure-agent
        zypper -n install cloud-init
        systemctl enable waagent
        ;;
    esac
fi

# Remove the folder and file to make subsequent boots faster.
rm -f /$base_linuxga_dir/PythonSetupPrereqs.py
exit 0
