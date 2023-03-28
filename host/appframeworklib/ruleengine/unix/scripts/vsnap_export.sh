#!/bin/sh
#=================================================================
# FILENAME
#    vsnap_export.sh
#
# DESCRIPTION
# When run with -s pre option
#   > delete any existing vsnap config files
# When run with -s post option
#   > read the vsnap config file
#   > get the vsnap names from cdpcli
#   > create the iscsi export devices
#   > scan the bus and check for devices
#
#    
# HISTORY
#     <21/10/2011>  Vishnu     Created.
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
    echo "Usage: $0 -s <Stage>"
    echo " "
    echo " where Stage can be any of the following:"
    echo " pre           -- To delete any existing vsnap config files"
    echo " post          -- To export vsnaps as iscsi luns"
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
    CheckOsPackages

    log "Deleting the existing vsnap config files $INSTALLPATH/Failover/..."
    $ECHO "Deleting the existing vsnap config files $INSTALLPATH/Failover/..."

    log "rm -f $INSTALLPATH/Failover/*vsnap_config.conf"

    rm -f $INSTALLPATH/Failover/*vsnap_config.conf

    log "Done."
    $ECHO "Done."
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
    log "Finding the currnet vsnap config file..."
    $ECHO "Finding the currnet vsnap config file..."

    numConfFiles=`ls -l $INSTALLPATH/Failover/*vsnap_config.conf | wc -l`

    if [ $numConfFiles -ne 1 ]; then
        $ECHO "Error : vsnap conf file not found. file count $numConfFiles."
        exit 1
    fi

    vsnapConfFile=`ls $INSTALLPATH/Failover/*vsnap_config.conf`

    log "Vsnap config file found at $vsnapConfFile."
    $ECHO "Vsnap config file found at $vsnapConfFile."

    log "Done."
    $ECHO "Done."
}


CreateFullDevice()
{
    bdev=$1

    log "Creating full device for block device $bdev"
    $ECHO "Creating full device for block device $bdev"

    log "$INSTALLPATH/scripts/iscisi_export/create_full_dev $bdev"

    $INSTALLPATH/scripts/iscisi_export/create_full_dev $bdev
    if [ $? -ne 0 ]; then
        $ECHO "Error: creation of full device for $bdev failed"
        exit 1
    fi

    log "Done."
    $ECHO "Done."

    return 
}

TargetExport()
{
    
    vsName=$1
    
    log "Exporting the vsnaps over iscsi target..."
    $ECHO "Exporting the vsnaps over iscsi target..."

    if [ $OSTYPE = "SunOS" ]; then
        log "iscsitadm create target -t raw -b /dev/vs/lun/rdsk/$vsName $vsName"
        iscsitadm create target -t raw -b /dev/vs/lun/rdsk/$vsName $vsName
        if [ $? -ne 0 ]; then
            $ECHO "Error: creation of iscsi target device for vsnap $vsName failed"
            exit 1
        fi

        log "iscsitadm list target | grep $vsName > $NULL"
        iscsitadm list target | grep $vsName > $NULL
        if [ $? -ne 0 ]; then
            $ECHO "Error: iscsi target for vsnap $vsName is not found"
            exit 1
        fi

    #elif [ "$OSTYPE" = "Linux" ]; then
            
    fi
    
    log "Done."
    $ECHO "Done."

    return
}

AddTargetForDiscovery()
{
    vsnapName=$1
    log "Configuring initiator to discover the targets for vsnap $vsnapName..."
    $ECHO "Configuring initiator to discover the targets for vsnap $vsnapName..."

    #targetServerName=`grep TargetServerName= $vsnapConfFile | awk -F= '{ print $2 }'`

    if [ $OSTYPE = "SunOS" ]; then
        targetServerName=`iscsiadm list initiator-node | grep "Initiator node alias:" | awk -F: '{ print $2 }'`
        
        targetIp=`grep -i $targetServerName /etc/hosts | head -1 |  awk '{ print $1 }'`

        iqn=`iscsitadm list target | grep $vsnapName | grep "iSCSI Name:" | awk '{print $NF}'`

        log " iscsiadm add static-config $iqn,$targetIp"
        iscsiadm add static-config $iqn,$targetIp
        if [ $? -ne 0 ]; then
            $ECHO "Error: iscsi initiator discovery for iscsi target exported vsnap $vsnapName could not be enabled"
            exit 1
        fi

        log "iscsiadm list target | grep $vsnapName > $NULL"
        iscsiadm list target | grep $vsnapName > $NULL
        if [ $? -ne 0 ]; then
            $ECHO "Error: The iscsi exported vsnap is not discovered at the iscsi initiator"
            exit 1
        fi    
    #elif [ "$OSTYPE" = "Linux" ]; then

    fi    

    log "Done."
    $ECHO "Done."
    
    return
}

DiscoverTargets()
{
    log "Discovering the target devices..."
    $ECHO "Discovering the target devices..."
    
    if [ $OSTYPE = "SunOS" ]; then
        log "devfsadm -c disk"
        devfsadm -c disk
    #elif [ "$OSTYPE" = "Linux" ]; then
    fi
    
    log "Done."
    $ECHO "Done."

    return
}

ExportiScsi()
{
    vsDev=$1
    name=`echo $vsDev | awk -F/ '{ print $NF }'`
    vsDev=${vsDev}s2

    log "Exporting vsnap $name over iscsi using the block device $vsDev"
    $ECHO "Exporting vsnap $name over iscsi using the block device $vsDev"

    if [ ! -b $vsDev ]; then
        $ECHO "Error: vsnap block device $vsDev not found"
        exit 1
    fi

    if [ $OSTYPE = "SunOS" ]; then
        CreateFullDevice $vsDev
    fi

    TargetExport $name

    AddTargetForDiscovery $name

    DiscoverTargets
}

CreateDeleteConfig()
{

    unexportConfFile=$INSTALLPATH/Failover/vsnap_config`date +'%Y%m%d%H%M%S'`.conf
    cp -f $vsnapConfFile $unexportConfFile

    $ECHO "#########################################################################"
    $ECHO " Please run the following command as 'root' user on `hostname` before deleting the vsnaps"
    $ECHO " $INSTALLPATH/scripts/vsnap_unexport.sh -s pre -f $unexportConfFile "
    $ECHO "#########################################################################"

}

ExportVsnaps()
{
    CheckOsPackages

    CheckVsnapConfFile

    tempfile1=/tmp/vsnapexport.$$
    for tgtDeviceAndTag in `grep RecoveryTag= $vsnapConfFile  | sed 's/RecoveryTag=//g' | sed 's/,/ /g'`
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

        ExportiScsi $vsnapDeviceName

    done

    for tgtDeviceAndTag in `grep RecoveryTime= $vsnapConfFile  | sed 's/RecoveryTime=//g' | set 's/ /%/g' | sed 's/,/ /g'`
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

        ExportiScsi $vsnapDeviceName

    done

    rm -f $tempfile1

    CreateDeleteConfig

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
if [ $# -ne 2 ]
then
    Usage
fi

while getopts "s:" opt
do
    case $opt in
    s )
        STAGE=$OPTARG
        if [ "$STAGE" != "pre" -a "$STAGE" != "post" ]
        then
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
    DeleteConfFile
elif [ "$STAGE" = "post" ]; then
    ExportVsnaps
fi

exit 0
