#!/bin/bash
#=================================================================
# FILENAME
#    inmvalidator.sh
#
# DESCRIPTION
#    validates and prepares the target volumes
#    
# HISTORY
#     <06/01/2011>  Vishnu     Created.
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

OPTION=""

Usage()
{
    $ECHO "usage: $0 <targetreadiness> <configfilepath>"
    $ECHO "       $0 <preparetarget>   <configfilepath>"
    $ECHO "targetreadiness          - To verify target readiness"
    $ECHO "preparetarget            - To prepare for the target use"
    return
}

ParseArgs()
{
    if [ $# -ne 2 ]; then
        Usage
        exit 1
    fi

    if [ $1 != "preparetarget"  -a $1 != "targetreadiness" ]; then
        Usage
        exit 1
    fi

    OPTION=$1

    if [ -z $2 ]; then
        Usage
        exit 1
    fi

    if [ $OPTION = "preparetarget" ]; then
        OUTPUT_FILE=
        TGT_CONFIG_FILE=$2
    elif [ $OPTION = "targetreadiness" ]; then
        OUTPUT_FILE=
        TGT_CONFIG_FILE=$2
    fi

    SetupLogs

    return
}

Output()
{
    $ECHO $* >> $TMP_OUTPUT_FILE
    return
}

log()
{
    $ECHO "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ $OPTION $*" >> $LOG_FILE
    return
}

SetupLogs()
{
    LOG_FILE=/var/log/inmvalidator.log

    if [ -f $LOG_FILE ]; then
        size=`$DU -k $LOG_FILE | awk '{ print $1 }'`
        if [ $size -gt 5000 ]; then
            $CP -f $LOG_FILE $LOG_FILE.1
            > $LOG_FILE
        fi
    else
        > $LOG_FILE
    fi
}

Cleanup()
{
    rm -rf $TMP_CONF_FILE > $NULL
    rm -f $tempfile3 > $NULL
    rm -f $tempfile2 > $NULL
    rm -f $tempfile1 > $NULL
}

CleanupError()
{
    rm -rf $TMP_CONF_FILE > $NULL
    rm -f $tempfile3 > $NULL
    rm -f $tempfile2 > $NULL
    rm -f $tempfile1 > $NULL
    exit 1
}

PrepareTargetVolumes()
{
    if [ ! -f $TGT_CONFIG_FILE ]; then
        $ECHO "Error: config file $TGT_CONFIG_FILE not found."
        exit 1
    fi

    retStatus=0
    for line in  `grep DeviceName= $TGT_CONFIG_FILE`
    do
        deviceName=`echo $line | awk -F: '{ print $1 }' | awk -F= '{ print $2 }'`    
        fsType=`echo $line | awk -F: '{ print $2 }' | awk -F= '{ print $2 }'`    
        volType=`echo $line | awk -F: '{ print $3 }' | awk -F= '{ print $2 }'`
        vendor=`echo $line | awk -F: '{ print $4 }' | awk -F= '{ print $2 }'`
        mountPoint=`echo $line | awk -F: '{ print $5 }' | awk -F= '{ print $2 }'`
        diskGroup=`echo $line | awk -F: '{ print $6 }' | awk -F= '{ print $2 }'`
        shouldDestroy=`echo $line | awk -F: '{ print $7 }' | awk -F= '{ print $2 }'`

        if [ "$mountPoint" != "0" ]; then
            if [ -d "$mountPoint" ]; then
                if [ "$OSTYPE" = "Linux" ]; then
                    df -P $mountPoint | awk '{print $NF}' | grep $mountPoint > /dev/null
                elif [ "$OSTYPE" = "SunOS" ]; then
                    df -k $mountPoint | awk '{print $NF}' | grep $mountPoint > /dev/null
                elif [ "$OSTYPE" = "AIX" ]; then
                    df -k $mountPoint | awk '{print $NF}' | grep $mountPoint > /dev/null
                fi
                
                if [ $? -eq 0 ]; then
                    $ECHO "Unmounting mount point $mountPoint on device $deviceName."
                    umount $mountPoint
                    if [ $? -ne 0 ]; then
                        retStatus=1
                    fi
                fi
            fi
            if [ "$OSTYPE" = "AIX" ]; then
                if [ "$vendor" = "11" ]; then
                    $ECHO "rmfs $mountPoint"
                    rmfs $mountPoint
                    if [ $? -ne 0 ]; then
                        retStatus=1
                    fi
                fi
            fi
        fi

        # VXVM volume
        if [ "$shouldDestroy" = "1" -a "$volType" = "LogicalVolume" -a "$vendor" = "10" ]; then
            ls $deviceName > /dev/null
            if [ $? -eq 0 ]; then
                volName=`echo $deviceName | awk -F/ '{ print $NF }'`
                $ECHO "Stopping volume $deviceName."
                vxvol -g $diskGroup stop $volName
                if [ $? -ne 0 ]; then
                    retStatus=1
                fi
            fi
        # logical volume
        elif [ "$shouldDestroy" = "1" -a "$volType" = "LogicalVolume" -a "$vendor" = "11" ]; then
            ls $deviceName > /dev/null
            if [ $? -eq 0 ]; then
                $ECHO "Deleting volume $deviceName."
                if [ "$OSTYPE" = "Linux" ]; then
                    lvremove -f $deviceName
                elif [ "$OSTYPE" = "AIX" ]; then
                    echo $deviceName | awk -F/ '{print $3}' | xargs rmlv -f 
                fi
                if [ $? -ne 0 ]; then
                    retStatus=1
                fi
            fi
        fi
    done

    if [ $retStatus -eq 1 ]; then
        $ECHO "Error: prepare target volumes failed."
        exit 1
    fi

    updateVxDgList=1
    updateLvmDgList=1
    updateAsmDeviceList=1

    for line in  `grep DeviceName= $TGT_CONFIG_FILE`
    do

        if [ $updateVxDgList -eq 1 ]; then
            tempfile1=/tmp/vxdglist.$$
            if [ -f /sbin/vxdg ]; then
                /sbin/vxdg list > $tempfile1
            elif [ -f /usr/sbin/vxdg ]; then
                /usr/sbin/vxdg list > $tempfile1
            fi
            updateVxDgList=0
        fi

        if [ $updateLvmDgList -eq 1 ]; then
            tempfile2=/tmp/vgslist.$$
            if [ "$OSTYPE" = "Linux" ]; then
                if [ -f /usr/sbin/vgs ]; then
                    /usr/sbin/vgs --noheadings 2> /dev/null >$tempfile2
                elif [ -f /sbin/vgs ]; then
                    /sbin/vgs --noheadings 2> /dev/null >$tempfile2
                fi
            elif [ "$OSTYPE" = "AIX" ]; then
                if [ -f /usr/sbin/lsvg ]; then
                    lsvg >$tempfile2
                fi
            fi
            updateLvmDgList=0
        fi

        if [ $updateAsmDeviceList -eq 1 ]; then
            tempfile3=/tmp/asmlist.$$
            $INSTALLPATH/scripts/oracleappagent.sh displayappdisks $tempfile3
            updateAsmDeviceList=0
        fi

        deviceName=`echo $line | awk -F: '{ print $1 }' | awk -F= '{ print $2 }'`    
        fsType=`echo $line | awk -F: '{ print $2 }' | awk -F= '{ print $2 }'`    
        volType=`echo $line | awk -F: '{ print $3 }' | awk -F= '{ print $2 }'`
        vendor=`echo $line | awk -F: '{ print $4 }' | awk -F= '{ print $2 }'`
        mountPoint=`echo $line | awk -F: '{ print $5 }' | awk -F= '{ print $2 }'`
        diskGroup=`echo $line | awk -F: '{ print $6 }' | awk -F= '{ print $2 }'`
        shouldDestroy=`echo $line | awk -F: '{ print $7 }' | awk -F= '{ print $2 }'`

        # VXVM volume
        if [ "$shouldDestroy" = "1" -a "$volType" = "LogicalVolume" -a "$vendor" = "10" ]; then
            if [ -f "$tempfile1" ]; then
                grep -w $diskGroup $tempfile1 > /dev/null
                if [ $? -eq 0 ]; then
                    $ECHO "Destroying disk group $diskGroup on device $deviceName."
                    vxdg destroy $diskGroup
                    if [ $? -ne 0 ]; then
                        retStatus=1
                    fi
                    updateVxDgList=1
                fi
            fi
        # logical volume
        elif [ "$shouldDestroy" = "1" -a "$volType" = "LogicalVolume" -a "$vendor" = "11" ]; then
            if [ -f "$tempfile2" ]; then
                grep -w $diskGroup $tempfile2 > /dev/null
                if [ $? -eq 0 ]; then
                    $ECHO "Destroying disk group $diskGroup on device $deviceName."
                    if [ "$OSTYPE" = "Linux" ]; then
                        vgremove -f $diskGroup
                    elif [ "$OSTYPE" = "AIX" ]; then
                        varyoffvg $diskGroup 
                        exportvg $diskGroup
                    fi

                    if [ $? -ne 0 ]; then
                        retStatus=1
                    fi
                    updateLvmDgList=1
                fi
            fi
        elif [ "$volType" = "Disk" -a "$diskGroup" != "0" ]; then

            if [ -f "$tempfile1" ]; then
                grep -w $diskGroup $tempfile1 > /dev/null
                if [ $? -eq 0 ]; then
                    $ECHO "Destroying disk group $diskGroup on device $deviceName."
                    vxdg destroy $diskGroup
                    if [ $? -ne 0 ]; then
                        retStatus=1
                    fi
                    updateVxDgList=1
                fi
            fi

            if [ -f "$tempfile2" ]; then
                grep -w $diskGroup $tempfile2 > /dev/null
                if [ $? -eq 0 ]; then
                    $ECHO "Destroying disk group $diskGroup on device $deviceName."
                    if [ "$OSTYPE" = "Linux" ]; then
                        vgremove -f $diskGroup
                    elif [ "$OSTYPE" = "AIX" ]; then
                        varyoffvg $diskGroup 
                        exportvg $diskGroup
                    fi
                    if [ $? -ne 0 ]; then
                        retStatus=1
                    fi
                    updateLvmDgList=1
                fi
            fi
        fi
        if [ "$diskGroup" = "ASM" ]; then
            if [ -f "$tempfile3" ]; then
                for asmDeviceName in `cat $tempfile3 | grep -v "ASM Disks -"`
                do
                    if [ "$deviceName" = "$asmDeviceName" ]; then
                        $ECHO "Error: The target device $deviceName is being used by ASM and needs to be manually excluded from ASM. "
                        $ECHO "If the target device is part of a ASM diskgroup, the diskgroup should be dropped or the device $deviceName should be excluded from the ASM diskgroup. " 
                        $ECHO "The target device and/or it's corresponding raw device should not have Oracle database user as the file owner. Use chown to modify the file ownership to root user. "
                        ls -Ll $deviceName
                        retStatus=1
                    fi
                done
            fi
        fi
 
        if [ "$OSTYPE" = "AIX" ]; then
            if [ "$volType" = "Disk" ]; then
                hdisk=`echo $deviceName | awk -F/ '{print $NF}'`
                pvid=`lspv | grep $hdisk | awk '{print $2}'`
                if [ "$pvid" = "none" ]; then
                    $ECHO "The target device $deviceName doesn't have PVID assigned."
                    $ECHO "chdev -l $hdisk -a pv=yes"
                    chdev -l $hdisk -a pv=yes
                    if [ $? -ne 0 ]; then
                        $ECHO "ERROR: Assigning PVID to $hdisk failed."
                        retStatus=1
                    fi
                fi
            fi
        fi

    done

    if [ $retStatus -eq 1 ]; then
        $ECHO "Error: prepare target volumes failed."
        exit 1
    fi
}

VerifyTargetVolumes()
{
    if [ ! -f $TGT_CONFIG_FILE ]; then
        $ECHO "Error: config file $TGT_CONFIG_FILE not found."
        exit 1
    fi

    retStatus=0
    for line in  `grep DeviceName= $TGT_CONFIG_FILE`
    do
        deviceName=`echo $line | awk -F: '{ print $1 }' | awk -F= '{ print $2 }'`    
        fsType=`echo $line | awk -F: '{ print $2 }' | awk -F= '{ print $2 }'`    
        volType=`echo $line | awk -F: '{ print $3 }' | awk -F= '{ print $2 }'`
        vendor=`echo $line | awk -F: '{ print $4 }' | awk -F= '{ print $2 }'`
        mountPoint=`echo $line | awk -F: '{ print $5 }' | awk -F= '{ print $2 }'`
        diskGroup=`echo $line | awk -F: '{ print $6 }' | awk -F= '{ print $2 }'`
        shouldDestroy=`echo $line | awk -F: '{ print $7 }' | awk -F= '{ print $2 }'`

        if [ "$mountPoint" != "0" ]; then
            if [ -d "$mountPoint" ]; then
                if [ "$OSTYPE" = "Linux" ]; then
                    df -P $mountPoint | awk '{print $NF}' | grep $mountPoint > /dev/null
                elif [ "$OSTYPE" = "SunOS" ]; then
                    df -k $mountPoint | awk '{print $NF}' | grep $mountPoint > /dev/null
                elif [ "$OSTYPE" = "AIX" ]; then
                    df -k $mountPoint | awk '{print $NF}' | grep $mountPoint > /dev/null
                fi
                
                if [ $? -eq 0 ]; then
                    $ECHO "$mountPoint must be unmounted."
                    retStatus=1
                fi
            fi
        fi

        # VXVM volume
        if [ "$shouldDestroy" = "1" -a "$volType" = "LogicalVolume" -a "$vendor" = "10" ]; then
            ls $deviceName > /dev/null
            if [ $? -eq 0 ]; then
                $ECHO "Volume $deviceName must be destroyed."
                retStatus=1
            fi
        # linux/aix logical volume
        elif [ "$shouldDestroy" = "1" -a "$volType" = "LogicalVolume" -a "$vendor" = "11" ]; then
            ls $deviceName > /dev/null
            if [ $? -eq 0 ]; then
                $ECHO "Volume $deviceName must be destroyed."
                retStatus=1
            fi
        fi
    done

    tempfile1=/tmp/vxdglist.$$
    if [ -f /sbin/vxdg ]; then
        /sbin/vxdg list > $tempfile1
    elif [ -f /usr/sbin/vxdg ]; then
        /usr/sbin/vxdg list > $tempfile1
    fi

    tempfile2=/tmp/vgslist.$$
    if [ "$OSTYPE" = "Linux" ]; then
        if [ -f /usr/sbin/vgs ]; then
            /usr/sbin/vgs --noheadings 2> /dev/null >$tempfile2
        elif [ -f /sbin/vgs ]; then
            /sbin/vgs --noheadings 2> /dev/null >$tempfile2
        fi
    elif [ "$OSTYPE" = "AIX" ]; then
        if [ -f /usr/sbin/lsvg ]; then
            lsvg >$tempfile2
        fi
    fi

    tempfile3=/tmp/asmlist.$$
    $INSTALLPATH/scripts/oracleappagent.sh displayappdisks $tempfile3

    for line in  `grep DeviceName= $TGT_CONFIG_FILE`
    do
        deviceName=`echo $line | awk -F: '{ print $1 }' | awk -F= '{ print $2 }'`    
        fsType=`echo $line | awk -F: '{ print $2 }' | awk -F= '{ print $2 }'`    
        volType=`echo $line | awk -F: '{ print $3 }' | awk -F= '{ print $2 }'`
        vendor=`echo $line | awk -F: '{ print $4 }' | awk -F= '{ print $2 }'`
        mountPoint=`echo $line | awk -F: '{ print $5 }' | awk -F= '{ print $2 }'`
        diskGroup=`echo $line | awk -F: '{ print $6 }' | awk -F= '{ print $2 }'`
        shouldDestroy=`echo $line | awk -F: '{ print $7 }' | awk -F= '{ print $2 }'`

        # VXVM volume
        if [ "$shouldDestroy" = "1" -a "$volType" = "LogicalVolume" -a "$vendor" = "10" ]; then
            if [ -f "$tempfile1" ]; then
                grep -w $diskGroup $tempfile1 > /dev/null
                if [ $? -eq 0 ]; then
                    $ECHO "Diskgroup $diskGroup for $deviceName must be destroyed."
                    retStatus=1
                fi
            fi
        # logical volume
        elif [ "$shouldDestroy" = "1" -a "$volType" = "LogicalVolume" -a "$vendor" = "11" ]; then
            if [ -f "$tempfile2" ]; then
                grep -w $diskGroup $tempfile2 > /dev/null
                if [ $? -eq 0 ]; then
                    $ECHO "Diskgroup $diskGroup for $deviceName must be destroyed."
                    retStatus=1
                fi
            fi
        elif [ "$volType" = "Disk" -a "$diskGroup" != "0" ]; then
            if [ -f "$tempfile1" ]; then
                grep -w $diskGroup $tempfile1 > /dev/null
                if [ $? -eq 0 ]; then
                    $ECHO "Diskgroup $diskGroup on $deviceName must be destroyed."
                    retStatus=1
                fi
            fi

            if [ -f "$tempfile2" ]; then
                grep -w $diskGroup $tempfile2 > /dev/null
                if [ $? -eq 0 ]; then
                    $ECHO "Diskgroup $diskGroup on $deviceName must be destroyed."
                    retStatus=1
                fi
            fi
        elif [ "$diskGroup" = "ASM" ]; then
            if [ -f "$tempfile3" ]; then
                for asmDeviceName in `cat $tempfile3 | grep -v "ASM Disks -"`
                do
                    if [ "$deviceName" = "$asmDeviceName" ]; then
                        $ECHO "The target device $deviceName is being used by Oracle ASM and needs to be excluded from it."
                        retStatus=1
                    fi
                done
            fi
        fi

        if [ "$OSTYPE" = "AIX" ]; then
            if [ "$volType" = "Disk" ]; then
                hdisk=`echo $deviceName | awk -F/ '{print $NF}'`
                pvid=`lspv | grep $hdisk | awk '{print $2}'`
                if [ "$pvid" = "none" ]; then
                    $ECHO "ERROR: The target device $deviceName doesn't have PVID assigned."
                    retStatus=1
                fi
            fi
        fi
    done

    if [ $retStatus -eq 1 ]; then
        $ECHO "Target volumes check failed."
        exit 1
    fi
}

########### Main ##########

trap Cleanup 0

if [ "$OSTYPE" = "Linux" -o  "$OSTYPE" = "SunOS" -o "$OSTYPE" = "AIX" ]; then
    trap CleanupError 1 2 3 15
else
    $ECHO "Error: Unsupported Operating System $OSTYPE"
    exit 1
fi

if [ ! -f /usr/local/.vx_version ]; then
    $ECHO "Error: Agent installation not found."
    exit 1
fi

INSTALLPATH=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
if [ -z "$INSTALLPATH" ]; then
    $ECHO "Error: Agent installation not found."
    exit 1
fi

ParseArgs $*

if [ "$OPTION" = "preparetarget" ]; then

    PrepareTargetVolumes

elif [ "$OPTION" = "targetreadiness" ]; then

    VerifyTargetVolumes
fi

exit 0
