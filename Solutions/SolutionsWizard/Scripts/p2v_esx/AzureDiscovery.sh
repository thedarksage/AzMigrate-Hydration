#!/bin/bash

check_new_initrd_reqd()
{
    create_initrd="no"

    case $OS in
        SLES12* | OL7*) echo "Will create initrd for $OS"
                        create_initrd="yes"
                        ;;

        *)              echo "Not generating initrd for $OS"
                        ;;
    esac
}

#wrapper script for Azure discovery.
#we need to handle return code in this case.
_SUCCESS_=0
_FAILURE_=1

_SCRIPTS_PATH_=""
OS=""
vx_dir=""
if [ -f /usr/local/.vx_version ]; then
    vx_dir=`grep INSTALLATION_DIR /usr/local/.vx_version | awk -F'=' '{print $2 }'`
else
    if [ -d /usr/local/InMage/Vx ]; then
        vx_dir=/usr/local/InMage/Vx
    else
        echo ""
        echo "InMage Scout product is not installed on the system. Exiting!"  >> "${_SCRIPTS_PATH_}/LinuxP2V.log" 2>&1
        echo ""
        exit ${_FAILURE_}
    fi
fi

_SCRIPTS_PATH_="$vx_dir/scripts/vCon/"
echo "PWD = ${_SCRIPTS_PATH_}"
_EXIT_CODE_=${_SUCCESS_}

OS=`${_SCRIPTS_PATH_}/OS_details.sh "print"`

create_initrd="no"
check_new_initrd_reqd

if [ "no" = "$create_initrd" ]; then
	"${_SCRIPTS_PATH_}/DiscoverMachine.sh" ${_SCRIPTS_PATH_} no >> "${_SCRIPTS_PATH_}/LinuxP2V.log" 2>&1
	RET=$?
else
	"${_SCRIPTS_PATH_}/DiscoverMachine.sh" ${_SCRIPTS_PATH_} yes "azure" >> "${_SCRIPTS_PATH_}/LinuxP2V.log" 2>&1
	RET=$?
fi
if [ ${RET} -ne ${_SUCCESS_} ]; then
	_EXIT_CODE_=$_FAILURE_
fi
echo "OS =$OS"
exit ${_EXIT_CODE_}


