#!/bin/sh
#=================================================================
# FILENAME
#    vsnap_unexport.sh
#
# DESCRIPTION
# When run with -s pre option
#   > delete any existing iscsi device that are created by iscsi loop back
# When run with -s post option
#   > sanity check
#    
# HISTORY
#     <27/10/2011>  Vishnu     Created.
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================

PATH=/bin:/usr/bin:/usr/sbin:/sbin:$PATH

ECHO=/bin/echo
SED=/bin/sed
CP=/bin/cp
MOVE=/bin/mv
VXDISK=vxdisk
DU=/usr/bin/du

NULL=/dev/null

OSTYPE=`uname | tr -d "\n"`
HOSTNAME=`hostname`

TMP_CONF_FILE=/tmp/vsnap.conf.$$
TMP_OUTPUT_FILE=/tmp/vsnap.$$

OPTION=""

Usage()
{
    echo "Usage: $0 -s <Stage> -f <filename>"
    echo " "
    echo " where Stage can be any of the following:"
    echo " pre          -- To unexport vsnaps as iscsi luns"
    echo " post         -- To delete any existing vsnap config files"
    echo " "
    exit 1
}

Output()
{
    $ECHO $* >> $TMP_OUTPUT_FILE
    return
}

WriteToConfFile()
{
    $ECHO $* >> $TMP_CONF_FILE 
    return
}

log()
{
    $ECHO "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ $STAGE $*" >> $LOG_FILE
    return
}

SetupLogs()
{
    LOG_FILE=$SCRIPT_PATH/vsnap_export.log

    if [ -f $LOG_FILE ]; then
        size=`$DU -k $LOG_FILE | awk '{ print $1 }'`
        if [ $size -gt 5000 ]; then
            $CP -f $LOG_FILE $LOG_FILE.1
            > $LOG_FILE
        fi
    else
        > $LOG_FILE
        chmod a+rw $LOG_FILE
    fi

    return
}

Cleanup()
{
    rm -rf $TMP_CONF_FILE > $NULL
    rm -rf $TMP_OUTPUT_FILE > $NULL
    #rm -f $tempfile > $NULL
    rm -f $tempfile1 > $NULL
}

CleanupError()
{
    rm -rf $TMP_CONF_FILE > $NULL
    rm -rf $TMP_OUTPUT_FILE > $NULL
    #rm -f $tempfile > $NULL
    rm -f $tempfile1 > $NULL
    exit 1
}

DeleteConfFile()
{
    log "Deleting the existing vsnap config file $CONFIG_FILE"
    $ECHO "Deleting the existing vsnap config file $CONFIG_FILE"

    log "rm -f $CONFIG_FILE"

    rm -f $CONFIG_FILE

    log "Done."
    $ECHO "Done."

    return
}

CheckOsPackages()
{
    log "Checking OS packages and services for iscsi target and initiator modules..."
    $ECHO "Checking OS packages and services for iscsi target and initiator modules..."

    if [ "$OSTYPE" = "Linux" ]; then
        log "rpm -qa | grep iscsitarget > $NULL "
        rpm -qa | grep iscsitarget > $NULL
        if [ $? -ne 0 ]; then
            $ECHO "Error: RPM iscsitarget not found" 
            exit 1
        fi

        log "rpm -qa | grep iscsi-initiator-utils > $NULL"
        rpm -qa | grep iscsi-initiator-utils > $NULL
        if [ $? -ne 0 ]; then
            $ECHO "Error: RPM iscsi-initiator-utils not found" 
            exit 1
        fi

    elif [ "$OSTYPE" = "SunOS" ]; then

        RELEASE=`uname -r`
        if [ $RELEASE != "5.10" ]; then
            $ECHO "Error: $OSTYPE $RELEASE does not support iscsi target functionality"
            exit 1
        fi

        log "pkginfo | grep SUNWiscsitgtr > $NULL"
        pkginfo | grep SUNWiscsitgtr > $NULL
        if [ $? -ne 0 ]; then
            $ECHO "Error: package SUNWiscsitgtr not found." 
            exit 1
        fi

        log "modinfo | grep iscsi > $NULL"
        modinfo | grep iscsi > $NULL
        if [ $? -ne 0 ]; then
            $ECHO "Error: iscsi initiator module is not loaded." 
            exit 1
        fi

        log "svcs | grep iscsi | grep initiator >$NULL"
        svcs | grep iscsi | grep initiator >$NULL
        if [ $? -ne 0 ]; then
            $ECHO "Error: iscsi initiator service is not found." 
            exit 1
        else
            log "svcs | grep iscsi | grep initiator | grep online >$NULL"
            svcs | grep iscsi | grep initiator | grep online >$NULL
            if [ $? -ne 0 ]; then
                $ECHO "Error: iscsi initiator service is not online." 
                exit 1
            fi
        fi

        log "svcs | grep iscsitgt >$NULL"
        svcs | grep iscsitgt >$NULL
        if [ $? -ne 0 ]; then
            $ECHO "Error: iscsi target service is not found." 
            exit 1
        else
            log "svcs | grep iscsitgt | grep online >$NULL"
            svcs | grep iscsitgt | grep online >$NULL
            if [ $? -ne 0 ]; then
                $ECHO "Error: iscsi target service is not online." 
                exit 1
            fi
        fi


        log "iscsiadm list discovery | grep "Static:" | grep enabled > $NULL"
        iscsiadm list discovery | grep "Static:" | grep enabled > $NULL
        if [ $? -ne 0 ]; then
            $ECHO "Error: iscsi initiator static discover mode not enabled"
            $ECHO "Use 'iscsiadm modify discovery --static enable' to enable static discovery."
            exit 1
        fi
    fi

    log "Done."
    $ECHO "Done."

    return
}

CheckVsnapConfFile()
{
    log "Verifying the vsnap config file..."
    $ECHO "Verifying the vsnap config file..."

    grep VsnapDriveType=Virtual $CONFIG_FILE > $NULL
    if [ $? -ne 0 ]; then
        log "Config file $CONFIG_FILE doesn't have valid configuration paramenters."
        $ECHO "Config file $CONFIG_FILE doesn't have valid configuration paramenters."
        exit 1
    fi

    log "Done."
    $ECHO "Done."

    return
}

CleanupTargets()
{
    log "Cleanup the target devices..."
    $ECHO "Cleanup the target devices..."
    
    if [ $OSTYPE = "SunOS" ]; then
        log "devfsadm -C -c disk"
        devfsadm -C -c disk
    #elif [ "$OSTYPE" = "Linux" ]; then
    fi
    
    log "Done."
    $ECHO "Done."

    return
}

RemoveIscsiInitiatorDevice()
{
    log "Removing the iscsi target device $vsName from iscsi initiator view"
    $ECHO "Removing the iscsi target device $vsName from iscsi initiator view"

    iscsiadm list target | grep $vsName > $NULL
    if [ $? -ne 0 ]; then
        log "iscsi target device $vsName is already removed."
        $ECHO "iscsi target device $vsName is already removed."
        return
    fi
        
    iqnName=`iscsiadm list target | grep $vsName | grep  "Target:" | awk '{print $2}'`

    if [ ! -z "$iqnName" ]; then
        iscsiadm remove static-config $iqnName
    fi

    iscsiadm list target | grep $vsName > $NULL
    if [ $? -eq 0 ]; then
        log "Unable to remove $iqnName"
        $ECHO "Unable to remove $iqnName"
        return
    fi

    log "Successfully removed $iqnName"
    $ECHO "Successfully removed $iqnName"

    return
}

RemoveIscsiTargetDevice()
{
    log "Removing the iscsi target device $vsName from iscsi target"
    $ECHO "Removing the iscsi target device $vsName from iscsi target"

    iscsitadm list target $vsName | grep $vsName > $NULL
    if [ $? -ne 0 ]; then
        log "iscsi target device $vsName is already removed."
        $ECHO "iscsi target device $vsName is already removed."
        return
    fi
        
    iscsitadm delete target -u 0 $vsName

    iscsitadm list target $vsName | grep $vsName > $NULL
    if [ $? -ne 0 ]; then
        log "Unable to remove $vsName"
        $ECHO "Unable to remove $vsName"
        return
    fi

    log "Successfully removed $vsName"
    $ECHO "Successfully removed $vsName"

    return
}

UnexportiScsi()
{
    vsDev=$1
    vsName=`echo $vsDev | awk -F/ '{ print $NF }'`
    vsDev=${vsDev}s2

    RemoveIscsiInitiatorDevice

    RemoveIscsiTargetDevice

    CleanupTargets
}

UnexportVsnaps()
{
    CheckOsPackages

    CheckVsnapConfFile

    tempfile1=/tmp/vsnapexport.$$
    for tgtDeviceAndTag in `grep RecoveryTag= $CONFIG_FILE | sed 's/RecoveryTag=//g' | sed 's/,/ /g'`
    do
        tgtDevice=`echo $tgtDeviceAndTag | awk -F= '{print $1}'`
        tag=`echo $tgtDeviceAndTag | awk -F= '{print $2}'`

        log "$CDPCLI --vsnap --op=list --target=$tgtDevice --verbose >$tempfile1"
        $CDPCLI --vsnap --op=list --target=$tgtDevice --verbose >$tempfile1

        grep $tag $tempfile1 > $NULL
        if [ $? -ne 0 ]; then
            $ECHO "Error: Vsnap for Tag $tag not found on target device $tgtDevice"
            continue
        fi
        
        linNum=`grep -n $tag $tempfile1 | awk -F: '{print $1}'`
        linNum=`expr $linNum - 4`
        vsnapDeviceName=`cat $tempfile1 | head -$linNum | tail -1 | awk -F: '{ print $2 }'`

        UnexportiScsi $vsnapDeviceName

    done

    for tgtDeviceAndTag in `grep RecoveryTime= $CONFIG_FILE | sed 's/RecoveryTime=//g' | set 's/ /%/g' | sed 's/,/ /g'`
    do
        tgtDevice=`echo $tgtDeviceAndTime | awk -F= '{print $1}'`
        tagTime=`echo $tgtDeviceAndTime | awk -F= '{print $2}' | sed 's/:0*/:/g' | sed 's/::/:0:/g' | sed 's/:$/:0/g' | sed 's/%/ /g'`

        log "$CDPCLI --vsnap --op=list --target=$tgtDevice --verbose >$tempfile1"
        $CDPCLI --vsnap --op=list --target=$tgtDevice --verbose >$tempfile1

        grep $tag $tempfile1 > $NULL
        if [ $? -ne 0 ]; then
            $ECHO "Error: Vsnap for Tag time $tagTime not found on target device $tgtDevice"
            continue
        fi
        
        linNum=`grep -n $tagTime $tempfile1 | awk -F: '{print $1}'`
        linNum=`expr $linNum - 3`
        vsnapDeviceName=`cat $tempfile1 | head -$linNum | tail -1 | awk -F: '{ print $2 }'`

        UnexportiScsi $vsnapDeviceName

    done

    rm -f $tempfile1

    return
}

########### Main ##########

trap Cleanup 0

if [ "$OSTYPE" = "Linux" ]; then
    trap CleanupError SIGHUP SIGINT SIGQUIT SIGTERM
elif [ "$OSTYPE" = "SunOS" ]; then
    trap CleanupError 1 2 3 15
else
    exit 0
fi

### To check if the arguments are passed   ####
if [ $# -ne 4 ]
then
    Usage
fi

while getopts "s:f:" opt
do
    case $opt in
    s )
        STAGE=$OPTARG
        if [ "$STAGE" != "pre" -a "$STAGE" != "post" ]; then
            Usage
        fi
        ;;	 
    f )
        CONFIG_FILE=$OPTARG
        if [ ! -f $CONFIG_FILE ]; then
            Usage
        fi
        ;;
    \?) Usage ;;
    esac
done

dirName=`dirname $0`
orgWd=`pwd`

cd $dirName
SCRIPT_PATH=`pwd`
cd $orgWd

INSTALLPATH=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
if [ -z "$INSTALLPATH" ]; then
    $ECHO "Error: Agent installation not found."
    exit 1
fi

CDPCLI=$INSTALLPATH/bin/cdpcli

SetupLogs

if [ "$STAGE" = "pre" ]; then 
    UnexportVsnaps
elif [ "$STAGE" = "post" ]; then
    DeleteConfFile
fi

exit 0
