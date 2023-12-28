#!/bin/sh

if [ -f /etc/oracle-release ]; then
    if grep -q 'Oracle Linux Server release 6.*' /etc/oracle-release; then
        if [ `uname -m` = "x86_64" ]; then
            OS="OL6-64: `cat /etc/oracle-release`: 0"
        fi
    elif grep -q 'Oracle Linux Server release 7.*' /etc/oracle-release; then
        if [ `uname -m` = "x86_64" ]; then
            OS="OL7-64: `cat /etc/oracle-release`: 0"
        fi
    elif grep -q 'Oracle Linux Server release 8.*' /etc/oracle-release; then
        if [ `uname -m` = "x86_64" ]; then
            OS="OL8-64: `cat /etc/oracle-release`: 0"
        fi
    elif grep -q 'Oracle Linux Server release 9.*' /etc/oracle-release; then
        if [ `uname -m` = "x86_64" ]; then
            OS="OL9-64: `cat /etc/oracle-release`: 0"
        fi    
    fi

    if [ -z "$OS" ]; then
        OS="Unsupported: `cat /etc/oracle-release` `uname -m`: 1"
    fi
elif [ -f /etc/redhat-release ]; then
    if grep -q 'Red Hat Enterprise Linux Server release 5.*' /etc/redhat-release || \
        grep -q 'CentOS release 5.*' /etc/redhat-release; then
        VERSION=`sed "s/[^0-9]*//g" /etc/redhat-release`
        if [ `uname -m` = "x86_64" -a $VERSION -ge 52 -a $VERSION -le 511 ]; then
            OS="RHEL5-64: `cat /etc/redhat-release`: 0"
        fi
    elif grep -q 'Red Hat Enterprise Linux Server release 6.*' /etc/redhat-release || \
        grep -q 'Red Hat Enterprise Linux Workstation release 6.*' /etc/redhat-release || \
        grep -q 'CentOS Linux release 6.*' /etc/redhat-release ||
        grep -q 'CentOS release 6.*' /etc/redhat-release; then
        if [ `uname -m` = "x86_64" ]; then
            OS="RHEL6-64: `cat /etc/redhat-release`: 0"
        fi
    elif grep -q 'Red Hat Enterprise Linux Server release 7.*' /etc/redhat-release || \
        grep -q 'Red Hat Enterprise Linux Workstation release 7.*' /etc/redhat-release || \
        grep -q 'CentOS Linux release 7.*' /etc/redhat-release; then
        if [ `uname -m` = "x86_64" ]; then
            OS="RHEL7-64: `cat /etc/redhat-release`: 0"
        fi                
    elif grep -q 'Red Hat Enterprise Linux release 8.*' /etc/redhat-release || \
        grep -q 'Rocky Linux release 8.*' /etc/redhat-release || \
        grep -q 'CentOS Linux release 8.*' /etc/redhat-release; then
        if [ `uname -m` = "x86_64" ]; then
            OS="RHEL8-64: `cat /etc/redhat-release`: 0"
        fi
    elif grep -q 'Red Hat Enterprise Linux release 9.*' /etc/redhat-release || \
        grep -q 'Rocky Linux release 9.*' /etc/redhat-release || \
        grep -q 'CentOS Linux release 9.*' /etc/redhat-release; then
        if [ `uname -m` = "x86_64" ]; then
            OS="RHEL9-64: `cat /etc/redhat-release`: 0"
        fi
    fi

    if [ -z "$OS" ]; then
        OS="Unsupported: `cat /etc/redhat-release` `uname -m`: 1"
    fi
elif  ( [ -f /etc/SuSE-release ] && ( grep -q 'VERSION = 11' /etc/SuSE-release  ||  grep -q 'VERSION = 12' /etc/SuSE-release )) ; then
    if grep -q 'VERSION = 11' /etc/SuSE-release && grep -q 'PATCHLEVEL = 3' /etc/SuSE-release; then
        if [ `uname -m` = "x86_64" ]; then
            OS="SLES11-SP3-64: `cat /etc/SuSE-release`: 0"
        fi
    elif grep -q 'VERSION = 11' /etc/SuSE-release && grep -q 'PATCHLEVEL = 4' /etc/SuSE-release; then
        if [ `uname -m` = "x86_64" ]; then
            OS="SLES11-SP4-64: `cat /etc/SuSE-release`: 0"
        fi
    fi
    if grep -q 'VERSION = 12' /etc/SuSE-release; then
        if [ `uname -m` = "x86_64" ]; then
            OS="SLES12-64: `cat /etc/SuSE-release`: 0"
        fi
    fi

    if [ -z "$OS" ]; then
        OS="Unsupported: `cat /etc/SuSE-release` `uname -m`: 1"
    fi
elif [ -f /etc/os-release ] && grep -q 'SLES' /etc/os-release; then
    if grep -q 'VERSION="15' /etc/os-release; then
        if [ `uname -m` = "x86_64" ]; then
            OS="SLES15-64: `cat /etc/os-release | grep PRETTY | cut -d '"' -f 2`: 0"
        fi
    fi

    if [ -z "$OS" ]; then
        OS="Unsupported: `cat /etc/os-release` `uname -m`: 1"
    fi
elif [ -f /etc/lsb-release ] ; then
    if grep -q 'DISTRIB_RELEASE=14.04' /etc/lsb-release ; then
        if [ `uname -m` = "x86_64" ]; then
            OS="UBUNTU-14.04-64: `cat /etc/lsb-release`: 0"
        fi
    elif grep -q 'DISTRIB_RELEASE=16.04' /etc/lsb-release ; then
        if [ `uname -m` = "x86_64" ]; then
            OS="UBUNTU-16.04-64: `cat /etc/lsb-release`: 0"
        fi
    elif grep -q 'DISTRIB_RELEASE=18.04' /etc/lsb-release ; then
        if [ `uname -m` = "x86_64" ]; then
            OS="UBUNTU-18.04-64: `cat /etc/lsb-release`: 0"
        fi
    elif grep -q 'DISTRIB_RELEASE=20.04' /etc/lsb-release ; then
        if [ `uname -m` = "x86_64" ]; then
            OS="UBUNTU-20.04-64: `cat /etc/lsb-release`: 0"
        fi
    elif grep -q 'DISTRIB_RELEASE=22.04' /etc/lsb-release ; then
        if [ `uname -m` = "x86_64" ]; then
            OS="UBUNTU-22.04-64: `cat /etc/lsb-release`: 0"
        fi
    fi

    if [ -z "$OS" ]; then
        OS="Unsupported: `cat /etc/lsb-release` `uname -m`: 1"
    fi
elif [ -f /etc/debian_version ]; then
    if grep -q '^7.*' /etc/debian_version; then
        if [ `uname -m` = "x86_64" ]; then
            OS="DEBIAN7-64: `cat /etc/debian_version`: 0"
        fi
    elif grep -q '^8.*' /etc/debian_version; then
        if [ `uname -m` = "x86_64" ]; then
            OS="DEBIAN8-64: `cat /etc/debian_version`: 0"
        fi
    elif grep -q '^9.*' /etc/debian_version; then
        if [ `uname -m` = "x86_64" ]; then
            OS="DEBIAN9-64: `cat /etc/debian_version`: 0"
        fi
    elif grep -q '^10.*' /etc/debian_version; then
        if [ `uname -m` = "x86_64" ]; then
            OS="DEBIAN10-64: `cat /etc/debian_version`: 0"
        fi
    elif grep -q '^11.*' /etc/debian_version; then
        if [ `uname -m` = "x86_64" ]; then
            OS="DEBIAN11-64: `cat /etc/debian_version`: 0"
        fi
    fi

    if [ -z "$OS" ]; then
        OS="Unsupported: Debian `cat /etc/debian_version` `uname -m`: 1"
    fi
fi

if [ $# -gt 0 ]
then
    echo $OS
fi
