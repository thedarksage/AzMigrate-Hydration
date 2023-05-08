#!/bin/bash
#=================================================================
# FILENAME
#    oracleappagent.sh
#
# DESCRIPTION
# 1. find the oralce user name
# 2. get the cluster information - local node, other nodes
# 3. find all the installed dbs
# 4. for each installed db, get the db instance (only if it running)
# 5. for each active db instance, login to db to get the db information
# 6. find the filterable devices from the oracle used devices
# 7. format the output to appagent
#    
# HISTORY
#     <28/06/2010>  Vishnu     Created.
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

TMP_CONF_FILE=/tmp/oraclesrcinfo.conf.$$
TMP_OUTPUT_FILE=/tmp/oraoutput.$$

DB_CONF_FILE=""
DB_NAMES=
NUM_DBS=0
OPTION=""



Usage()
{
    $ECHO "usage: $0 <discover | displayappdisks | displaycrsdisks> <output_filename>"
    $ECHO "       $0 <targetreadiness> <configfilepath>"
    $ECHO "       $0 <preparetarget>   <configfilepath>"
    $ECHO "discover                 - To discover Oracle database details, volumes, disks"
    $ECHO "displayappdisks          - To display list of Disks used by ASM"
    $ECHO "displaycrsdisks          - To display list of OCR/VOTE disks"
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

    if [ $1 != "discover"  -a  $1 != "displayappdisks"  -a $1 != "displaycrsdisks" -a $1 != "preparetarget"  -a $1 != "targetreadiness" ]; then
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
        TGT_CONFIG_FILES=$2
    elif [ $OPTION = "targetreadiness" ]; then
        OUTPUT_FILE=
        SRC_CONFIG_FILE=$2
    else
        > $2
        OUTPUT_FILE=$2
    fi

    dirName=`dirname $0`
    orgWd=`pwd`

    cd $dirName
    SCRIPT_PATH=`pwd`
    cd $orgWd

    SetupLogs

    return
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

WriteToDbConfFile()
{
    $ECHO $* >> $TMP_DB_CONF_FILE 
    return
}

log()
{
    $ECHO "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ $OPTION $*" >> $LOG_FILE
    return
}

SetupLogs()
{
    LOG_FILE=/var/log/oradisc.log

    if [ -f $LOG_FILE ]; then
        size=`$DU -k $LOG_FILE | awk '{ print $1 }'`
        if [ $size -gt 5000 ]; then
            $CP -f $LOG_FILE $LOG_FILE.1
            > $LOG_FILE
        fi
    else
        > $LOG_FILE
        chmod 640 $LOG_FILE
    fi
}

Cleanup()
{
    rm -f $TMP_CONF_FILE > $NULL
    rm -f $DB_VIEWS_FILE > $NULL
    rm -f $TMP_DB_CONF_FILE > $NULL
    rm -f $TMP_OUTPUT_FILE > $NULL
    rm -f $tempfile > $NULL
    rm -f $tempfile1 > $NULL
    rm -f $tempfile2 > $NULL
    rm -f $tempfile3 > $NULL
}

CleanupError()
{
    rm -f $TMP_CONF_FILE > $NULL
    rm -f $DB_VIEWS_FILE > $NULL
    rm -f $TMP_DB_CONF_FILE > $NULL
    rm -f $TMP_OUTPUT_FILE > $NULL
    rm -f $tempfile > $NULL
    rm -f $tempfile1 > $NULL
    rm -f $tempfile2 > $NULL
    rm -f $tempfile3 > $NULL
    exit 1
}

GetDbViews()
{
    log "ENTER GetDbViews..."

    # $ECHO $* 

    #AGENTPATH=`ps -ef | grep "appservice"| grep bin | grep -v grep | awk '{print $NF}'`
    # $ECHO "AGENTPATH: $AGENTPATH"
    #AGENTDIRECTORY=`dirname ${AGENTPATH}`

    log "Script Path: $SCRIPT_PATH"

    echo $DB_VER | awk -F"." '{print $1}' | grep 9 > /dev/null

    if [ $? -eq 0 ]; then
        ${SCRIPT_PATH}/oraclediscovery.sh $*
    else
        su - $1 -c "${SCRIPT_PATH}/oracleappdiscovery.sh $*"
    fi

    if [ $? -ne 0 ]; then
        log "Error: oracle discovery failed"
        exit 1
    fi

    GetFilterDevices $4 $5

    log "EXIT GetDbViews"
    return
}

GetBlockDeviceFromMajMinLinux()
{
    log "ENTER GetBlockDeviceFromMajMinLinux..."

    maj=$1
    min=$2
    MAJ_MIN_OUTPUT_FILE=$3
    option=$4

    majHex=`echo "ibase=10;obase=16;$maj" | bc`
    minHex=`echo "ibase=10;obase=16;$min" | bc`

    flag=0
    while read line
    do
        if [ $flag -eq 0 ]; then
            if [ "$line" = "Block devices:" ]; then
                flag=1
            fi
        else
            tMaj=`echo $line | awk '{print $1}' | sed 's/ //g'`
            if [ $maj -eq $tMaj ]; then
                log "Found $maj in $line"
                deviceType=`echo $line | awk '{print $2}' | sed 's/ //g'`
                break
            fi
        fi
    done < /proc/devices

    blockDevice=

    # Linux sd
    if [ "$deviceType" = "sd" ]; then
    	for device in `ls -l /dev | grep "^b" | awk '{print "/dev/"$NF}'`
        do
            stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
            if [ $? -eq 0 ]; then
                blockDevice=$device
                log "sd Device Bound to device $dev = $blockDevice"
                break
            fi
        done
    # VXVM
    elif [ "$deviceType" = "VxDMP" ]; then 
        for device in `ls -l /dev/vx/dmp/ | grep "^b" | awk '{print "/dev/vx/dmp/"$NF}'`
        do
            stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
            if [ $? -eq 0 ]; then
                blockDevice=$device
                log "VxDMP Device Bound to device $dev = $blockDevice"
                break
            fi
        done
    # Linux DMP
    elif [ "$deviceType" = "device-mapper" ]; then
        for device in `ls -lL /dev/mapper/ | grep "^b" | awk '{print "/dev/mapper/"$NF}'`
        do
            if [ -L "$device" ]; then
                stat_device=`ls -l $device | awk -F/ '{print "/dev/"$NF}'`
            else
                stat_device=$device
            fi
            stat -c "%t,%T" $stat_device | grep -i "$majHex,$minHex" > /dev/null
            if [ $? -eq 0 ]; then
                blockDevice=$device
                log "Linux DMP Device Bound to device $dev = $blockDevice"
                break
            fi
        done
    else
        log "Error: unsupported device type $deviceType"
        exit 1
    fi

    if [ ! -z "$blockDevice" ]; then

        if [ "$option" = "FilterDevice" ] ; then
            echo "FilterDevice=$blockDevice" >> $MAJ_MIN_OUTPUT_FILE
            if [ "$blockDevice" != "$dev" ]; then
                WriteToDbConfFile "DiskAlias=$blockDevice:$dev"
            fi
        else
            echo "$blockDevice" >> $MAJ_MIN_OUTPUT_FILE
        fi

    fi

    log "Exit GetBlockDeviceFromMajMinLinux..."

    return
}

GetBlockDeviceFromMajMinSol()
{
    log "ENTER GetBlockDeviceFromMajMinSol..."

    maj=$1
    min=$2
    MAJ_MIN_OUTPUT_FILE=$3
    option=$4

    deviceType=`grep -w $maj /etc/name_to_major | awk '{print $1}' | sed 's/ //g'`
    blockDevice=

    # ssd or sd
    if [ "$deviceType" = "ssd" -o "$deviceType" = "sd" ]; then
        dirToSearch=/dev/dsk/*
    elif [ "$deviceType" = "vxdmp" ]; then 
        dirToSearch=/dev/vx/dmp/*
    else
        log "Error: unsupported device type $deviceType"
        exit 1
    fi  

    for device in `ls -lL $dirToSearch 2>/dev/null | gerp "^b" | awk '{print $NF}'`
    do
        tmaj=`ls -lL $device | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
        tmin=`ls -lL $device | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`
        if [ $tmaj -eq $maj -a $tmin -eq $min ]; then
            blockDevice=$device
            log "Device Bound to device $device = $blockDevice"
            break
        fi
    done

    if [ ! -z "$blockDevice" ]; then

        if [ "$option" = "FilterDevice" ] ; then
            echo "FilterDevice=$blockDevice" >> $MAJ_MIN_OUTPUT_FILE
            if [ "$blockDevice" != "$dev" ]; then
                WriteToDbConfFile "DiskAlias=$blockDevice:$dev"
            fi
        else
            echo "$blockDevice" >> $MAJ_MIN_OUTPUT_FILE
        fi

    fi

    log "Exit GetBlockDeviceFromMajMinSol..."

    return
}

GetBlockDeviceFromMajMinAix()
{
    log "ENTER GetBlockDeviceFromMajMinAix..."

    maj=$1
    min=$2
    MAJ_MIN_OUTPUT_FILE=$3
    option=$4

    blockDevice=

    dirToSearch=/dev/hdisk*

    for device in `ls -lL $dirToSearch 2>/dev/null | grep "^b" | awk '{print $NF}'`
    do
        tmaj=`ls -lL $device | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
        tmin=`ls -lL $device | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`
        if [ $tmaj -eq $maj -a $tmin -eq $min ]; then
            blockDevice=$device
            log "Device Bound to device $device = $blockDevice"
            break
        fi
    done

    if [ ! -z "$blockDevice" ]; then

        if [ "$option" = "FilterDevice" ] ; then
            echo "FilterDevice=$blockDevice" >> $MAJ_MIN_OUTPUT_FILE
            if [ "$blockDevice" != "$dev" ]; then
                WriteToDbConfFile "DiskAlias=$blockDevice:$dev"
            fi
        else
            echo "$blockDevice" >> $MAJ_MIN_OUTPUT_FILE
        fi

    fi

    log "Exit GetBlockDeviceFromMajMinAix..."

    return

}

GetFilterDevicesFromAlias()
{
    log "ENTER GetFilterDevicesFromAlias... "

    dev=$1
    ALIAS_OUTPUT_FILE=$2
    option=$3

    log "Device: $dev"

    maj=`ls -lL $dev | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
    min=`ls -lL $dev | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`

    if [ "$OSTYPE" = "Linux" ]; then
        GetBlockDeviceFromMajMinLinux $maj $min $ALIAS_OUTPUT_FILE $option
    elif [ "$OSTYPE" = "SunOS" ]; then
        GetBlockDeviceFromMajMinSol $maj $min $ALIAS_OUTPUT_FILE $option
    elif [ "$OSTYPE" = "AIX" ]; then
        GetBlockDeviceFromMajMinAix $maj $min $ALIAS_OUTPUT_FILE $option
    fi

    log "EXIT GetFilterDevicesFromAlias"

    return
}
GetFilterDevicesFromRaw()
{
    log "ENTER GetFilterDevicesFromRaw... "

    dev=$1
    RAW_OUTPUT_FILE=$2
    option=$3

    log "Raw Device: $dev"

    if [ "$OSTYPE" = "Linux" ]; then
        maj=`raw -q $dev | awk '{print $5}' | sed 's/,//' | sed 's/ //g' `
        min=`raw -q $dev | awk '{print $7}' | sed 's/,//' | sed 's/ //g' `

        GetBlockDeviceFromMajMinLinux $maj $min $RAW_OUTPUT_FILE $option
    elif [ "$OSTYPE" = "SunOS" ]; then
        blockDevice=`echo $dev | sed -e 's/rdsk/dsk/g'`
        if [ -b $blockDevice ]; then
            if [ "$option" = "FilterDevice" ] ; then
                $ECHO "FilterDevice=$blockDevice" >> $RAW_OUTPUT_FILE
            else
                $ECHO "$blockDevice" >> $RAW_OUTPUT_FILE
            fi
        fi
    elif [ "$OSTYPE" = "AIX" ]; then
        blockDevice=`echo $dev | sed -e 's/rhdisk/hdisk/g'`
        if [ -b $blockDevice ]; then
            if [ "$option" = "FilterDevice" ] ; then
                $ECHO "FilterDevice=$blockDevice" >> $RAW_OUTPUT_FILE
            else
                $ECHO "$blockDevice" >> $RAW_OUTPUT_FILE
            fi
        fi
    fi

    log "EXIT GetFilterDevicesFromRaw"

    return
}

GetFilterDevicesFromAsmlib()
{
    log "ENTER GetFilterDevicesFromAsmlib..."

    dev=$1
    ASMLIB_OUTPUT_FILE=$2 
    option=$3

    log "GetFilterDevicesFromAsmlib: device - `ls -l $dev`"

    maj=`ls -l $dev | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
    min=`ls -l $dev | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`

    GetBlockDeviceFromMajMinLinux $maj $min $ASMLIB_OUTPUT_FILE $option

    log "EXIT GetFilterDevicesFromAsmlib"

    return
}

GetFilterDevicesFromVxvmRaw()
{
    log "ENTER GetFilterDevicesFromVxvmRaw..."

    vxvmVol=$1
    VXVM_OUTPUT_FILE=$2
    option=$3

    WriteToDbConfFile "isVxvm=YES"
    GetVxvmVersion
    WriteToDbConfFile "isVxvmRaw=YES"

    if [ "$option" = "FilterDevice" ] ; then
        $ECHO "DirNameWithLevel=$vxvmVol(VolumeName),1" >> $VXVM_OUTPUT_FILE
    fi

    diskgroup=`echo $vxvmVol |  awk -F/ '{print $5}'`
    if [ "$option" = "FilterDevice" ] ; then
        $ECHO "DiskGroupNameWithLevel=$diskgroup(DiskGroupName),2" >> $VXVM_OUTPUT_FILE
    fi

    WriteToDbConfFile "VxvmDgName=$diskgroup"
    for disk in `$VXDISK -g $diskgroup list | grep -v DEVICE | awk '{ print $3 }'`
    do
        if [ "$option" = "FilterDevice" ] ; then
            $ECHO "DiskNameWithLevel=$disk(DiskName),3" >> $VXVM_OUTPUT_FILE
        fi
    done

    for disk in `$VXDISK -g $diskgroup list | grep -v DEVICE | awk '{ print $1 }'`
    do
        for path in `$VXDISK list $disk | grep state=enabled | awk '{print $1}'`
        do
            if [ "$OSTYPE" = "Linux" ]; then
                if [ "$option" = "FilterDevice" ] ; then
                    $ECHO "FilterDevice=/dev/$path" >> $VXVM_OUTPUT_FILE
                else
                    $ECHO "/dev/$path" >> $VXVM_OUTPUT_FILE
                fi
            elif [ "$OSTYPE" = "SunOS" ]; then
                if [ "$option" = "FilterDevice" ] ; then
                    $ECHO "FilterDevice=/dev/dsk/$path" >> $VXVM_OUTPUT_FILE
                else
                    $ECHO "/dev/dsk/$path" >> $VXVM_OUTPUT_FILE
                fi
            fi
        done
    done

    log "EXIT GetFilterDevicesFromVxvmRaw"

    return
}

GetFilterDevicesFromVxvm()
{
    log "ENTER GetFilterDevicesFromVxvm..."

    vxvmVol=$1
    VXVM_OUTPUT_FILE=$2
    option=$3

    if [ "$option" = "FilterDevice" ] ; then
        WriteToDbConfFile "isVxvm=YES"
        GetVxvmVersion
    fi

    diskgroup=`echo $vxvmVol |  awk -F/ '{print $5}'`
    for disk in `$VXDISK -g $diskgroup list | grep -v DEVICE | awk '{ print $1 }'`
    do
        for path in `$VXDISK list $disk | grep state=enabled | awk '{print $1}'`
        do
            if [ "$OSTYPE" = "Linux" ]; then
                if [ "$option" = "FilterDevice" ] ; then
                    $ECHO "FilterDevice=/dev/$path" >> $VXVM_OUTPUT_FILE
                else
                    $ECHO "/dev/$path" >> $VXVM_OUTPUT_FILE
                fi
            elif [ "$OSTYPE" = "SunOS" ]; then
                if [ "$option" = "FilterDevice" ] ; then
                    $ECHO "FilterDevice=/dev/dsk/$path" >> $VXVM_OUTPUT_FILE
                else
                    $ECHO "/dev/dsk/$path" >> $VXVM_OUTPUT_FILE
                fi
            fi
        done
    done

    log "EXIT GetFilterDevicesFromVxvm"

    return
}

GetFilterDevicesFromLvm()
{
    log "ENTER GetFilterDevicesFromLvm..."

    dir=$1
    LVM_OUTPUT_FILE=$2
    option=$3

    if [ "$OSTYPE" = "Linux" ]; then
        mount_points=`df -PT $dir | grep -v Filesystem | awk '{ print $NF }' | uniq`

        for line in $mount_points
        do
            fstype=`df -PT $line | grep -v Filesystem | awk '{ print $2 }'`

            GetFilterDevicesFromFileSystem $line $LVM_OUTPUT_FILE $fstype $option
        done

    elif [ "$OSTYPE" = "SunOS" ]; then

        mount_dir=`df -n $dir | awk -F: '{print $1}' | sed 's/ //g'`
        for line in `grep $mount_dir /etc/mnttab | awk '{print $1":"$2":"$3}'`
        do
            resource=`echo $line | awk -F: '{print $1}'`
            mount_point=`echo $line | awk -F: '{print $2}'`
            if [ -b "$resource" -a "$mount_point" = "$mount_dir" ]; then
                fstype=`echo $line | awk -F: '{print $3}'`

                GetFilterDevicesFromFileSystem $mount_point $LVM_OUTPUT_FILE $fstype $option
            else
               fstype=`echo $line | awk -F: '{print $3}'`

               if [ "$fstype" = "zfs" ];then
                   isPool=0
                   isZvol=0
                   zpool list|grep -v NAME|awk '{print $1}' > /tmp/pools
                   grep -w $resource /tmp/pools > /dev/null
                   if [ $? -eq 0 ];then
                       isPool=1
                   else
                       zfs list |grep -v NAME|awk '{print $1}' > /tmp/zfsvols
                       grep -w $resource /tmp/zfsvols > /dev/null
                       if [ $? -eq 0 ];then
                           isZvol=1
                       fi
                   fi
                   if [ $isPool -eq 1 -o $isZvol -eq 1 ];then
                       fstype=`echo $line | awk -F: '{print $3}'`
                       GetFilterDevicesFromZfsFileSystem $mount_point $LVM_OUTPUT_FILE $fstype $option
                   fi
               fi
            fi
        done

    elif [ "$OSTYPE" = "AIX" ]; then

        mount_points=`df -P $dir | grep -v Filesystem | awk '{ print $NF }' | uniq`

        for line in $mount_points
        do
            fstype=`/usr/sysv/bin/df -n $line | awk -F: '{ print $2 }' | sed 's/ //g'`

            GetFilterDevicesFromFileSystem $line $LVM_OUTPUT_FILE $fstype $option
        done
    fi

    log "EXIT GetFilterDevicesFromLvm"

    return
}

GetAllMountPointsForVxDiskGroup()
{
    vxdiskgroup=$1

    for allmntvol in `vxprint -g $vxdiskgroup -v | grep "^v" | awk '{print $2}'`
    do
        volPath=/dev/vx/dsk/${vxdiskgroup}/${allmntvol}
        mount | grep $volPath > /dev/null
        if [ $? -eq 0 ]; then
            if [ "$OSTYPE" = "Linux" ]; then
                mntPoint=`mount | grep $volPath | grep -w $vxdiskgroup | grep -w $allmntvol| awk '{ print $3}'`
            elif [ "$OSTYPE" = "SunOS" ]; then
                mntPoint=`mount | grep $volPath | grep -w $vxdiskgroup | grep -w $allmntvol| awk '{ print $1}'`
            elif [ "$OSTYPE" = "AIX" ]; then
                mntPoint=`df -P | grep $volPath | grep -w $vxdiskgroup | grep -w $allmntvol | awk '{print $NF}'`
            fi

            if [ ! -z "$mntPoint" ]; then

                if [ "$OSTYPE" = "Linux" ]; then
                    ampfstype=`df -PT $mntPoint | grep -v Filesystem | awk '{ print $2 }'`
                elif [ "$OSTYPE" = "SunOS" ]; then
                    ampfstype=`df -n $volPath | awk -F: '{ print $2 }' | sed 's/ //g'`
                elif [ "$OSTYPE" = "AIX" ]; then
                    ampfstype=`/usr/sysv/bin/df -n $mntPoint | awk -F: '{ print $2 }' | sed 's/ //g'`
                fi

                WriteToDbConfFile "MountPoint=$ampfstype:$volPath:$mntPoint"
            fi
        fi
    done

}

GetAllMountPointsForVolumeGroup()
{
    volgroupname=$1

    for allmntvol in `lsvg -l $volgroupname | grep -v $volgroupname | grep -v "LV NAME" | awk  '{print $1}'`
    do
        volPath=/dev/${allmntvol}
        mount |awk '{print $1}'| grep -w $volPath > /dev/null
        if [ $? -eq 0 ]; then
            mntPoint=`mount | grep -w $volPath | grep -w $allmntvol| awk '{ print $2}'`
            logVolume=`mount | grep -w $volPath | grep -w $allmntvol| awk -F= '{ print $2}'`

            if [ ! -z "$mntPoint" ]; then
                ampfstype=`mount | grep $volPath | grep -w $allmntvol| awk '{print $3}'`
                WriteToDbConfFile "MountPoint=$ampfstype:$volPath:$mntPoint:$logVolume"
            fi
        fi
    done
    
    return
}

GetFilterDevicesFromZfsFileSystem()
{
    log "ENTER GetFilterDevicesFromZfsFileSystem..."

    mount_point=$1
    EXT_OUTPUT_FILE=$2
    FSType=$3
    option=$4
 
    if [ "$option" = "FilterDevice" ]; then
        if [ "$FSType" = "zfs" ]; then
            WriteToDbConfFile "isZfs=YES"
        fi
    fi

    levelCount=1
    if [ "$option" = "FilterDevice" ]; then
        $ECHO "MountPointWithLevel=$mount_point(MountPoint),$levelCount" >> $EXT_OUTPUT_FILE
    fi
    levelCount=`expr $levelCount + 1`

    if [ "$OSTYPE" = "SunOS" ]; then
        for entry in `df -F $FSType | grep $mount_point | awk -F: '{print $1}' | sed 's/[()]/:/g' | sed 's/ //g'`
        do
           if [ $FSType = "zfs" ];then
               entmp=`echo $entry | awk -F: '{print $1}'`
               entzvol=`echo $entry | awk -F: '{print $2}'`
               if [ ! -z "$entzvol" -a "$entmp" = "$mount_point" ]; then
                   volume=$entzvol
                   break
               fi
           fi
        done

        if [ "$option" = "FilterDevice" ]; then
            $ECHO "VolumeNameWithLevel=$volume(VolumeName),$levelCount" >> $EXT_OUTPUT_FILE
        fi
        levelCount=`expr $levelCount + 1`

        entzpool=`echo $volume | awk -F/ '{print $1}'`

        zpool status $entzpool |awk '{print $1}' > /tmp/out

        #TODO: Need to find optimized way of doing this
        #removing log, cache and spare disks if exists.
        sed -n '/spares/q;p' /tmp/out > /tmp/out1
        sed -n '/cache/q;p' /tmp/out1 > /tmp/out
        sed -n '/logs/q;p' /tmp/out > /tmp/out1

        mv /tmp/out1 /tmp/out

        disks=
        for disk in `cat /tmp/out | grep 'c[0-9]'`
        do
           if [ -z "$disks" ]; then
               disks="/dev/dsk/$disk"
           else
               disks="$disks,/dev/dsk/$disk"
           fi

           if [ "$option" = "FilterDevice" ]; then
               $ECHO "FilterDevice=/dev/dsk/$disk" >> $EXT_OUTPUT_FILE
           else
               $ECHO "/dev/dsk/$disk" >> $EXT_OUTPUT_FILE
           fi
        done

        if [ "$option" = "FilterDevice" ]; then
            WriteToDbConfFile "MountPoint=$FSType:$volume:$mount_point:$disks:$logVolume"
        fi
    fi
}

GetFilterDevicesFromFileSystem()
{
    log "ENTER GetFilterDevicesFromFileSystem..."

    mount_point=$1
    EXT_OUTPUT_FILE=$2
    FSType=$3
    option=$4

    if [ "$option" = "FilterDevice" ]; then
        if [ "$FSType" = "vxfs" ]; then
            WriteToDbConfFile "isVxfs=YES"
            GetVxfsVersion
        elif [ "$FSType" = "ocfs2" ]; then
            WriteToDbConfFile "isOcfs2=YES"
        fi
    fi

    levelCount=1
    if [ "$option" = "FilterDevice" ]; then
        $ECHO "MountPointWithLevel=$mount_point(MountPoint),$levelCount" >> $EXT_OUTPUT_FILE
    fi
    levelCount=`expr $levelCount + 1`


    if [ "$OSTYPE" = "Linux" ]; then
        volume=`df -PT $mount_point | grep -v Filesystem | awk '{ print $1 }'`
    elif [ "$OSTYPE" = "SunOS" ]; then
        for entry in `df -F $FSType | grep $mount_point | awk -F: '{print $1}' | sed 's/[()]/:/g' | sed 's/ //g'`
        do
            entmp=`echo $entry | awk -F: '{print $1}'`
            entvol=`echo $entry | awk -F: '{print $2}'`
            if [ -b "$entvol" -a "$entmp" = "$mount_point" ]; then
                volume=$entvol
                break
            fi
        done
    elif [ "$OSTYPE" = "AIX" ]; then
        volume=`df $mount_point | grep -v Filesystem | awk '{ print $1 }'`
        if [ "$FSType" != "vxfs" ]; then
            logVolume=`mount | grep -w $volume | awk -F= '{ print $2 }'`
        fi
    fi

    case $volume in
    */dev/vx/dsk/*)

        if [ "$option" = "FilterDevice" ]; then
            $ECHO "VolumeNameWithLevel=$volume(VolumeName),$levelCount" >> $EXT_OUTPUT_FILE
        fi
        levelCount=`expr $levelCount + 1`

        if [ "$option" = "FilterDevice" ]; then
            if [ "$FSType" = "vxfs" ]; then
                 WriteToDbConfFile "MountPoint=$FSType:$volume:$mount_point"
            else
                 WriteToDbConfFile "MountPoint=$FSType:$volume:$mount_point:$logVolume"
            fi
        fi

        if [ "$option" = "FilterDevice" ]; then
            WriteToDbConfFile "isVxvm=YES"
            GetVxvmVersion
        fi

        if [ "$option" = "FilterDevice" ]; then
            if [ "$isClus" = "0" -a "$OSTYPE" != "AIX" ]; then
                $ECHO "FilterDevice=$volume" >> $EXT_OUTPUT_FILE
                return
            fi
        fi

        diskgroup=`echo $volume |  awk -F/ '{print $5}'`
        if [ "$option" = "FilterDevice" ]; then
            if [ "$isClus" = "1" -o "$OSTYPE" = "AIX" ]; then
                GetAllMountPointsForVxDiskGroup $diskgroup
            fi
        fi

        if [ "$option" = "FilterDevice" ]; then
            $ECHO "DiskGroupNameWithLevel=$diskgroup(DiskGroupName),$levelCount" >> $EXT_OUTPUT_FILE
        fi
        levelCount=`expr $levelCount + 1`

        for disk in `$VXDISK -g $diskgroup list | grep -v DEVICE | awk '{ print $3 }'`
        do
            if [ "$option" = "FilterDevice" ]; then
                $ECHO "DiskNameWithLevel=$disk(DiskName),$levelCount" >> $EXT_OUTPUT_FILE
            fi
        done

        for disk in `$VXDISK -g $diskgroup list | grep -v DEVICE | awk '{ print $1 }'`
        do
            for path in `$VXDISK list $disk | grep state=enabled | awk '{print $1}'`
            do
                if [ "$option" = "FilterDevice" ]; then
                    if [ "$OSTYPE" = "Linux" ]; then
                        $ECHO "FilterDevice=/dev/$path" >> $EXT_OUTPUT_FILE
                    elif [ "$OSTYPE" = "SunOS" ]; then
                        $ECHO "FilterDevice=/dev/dsk/$path" >> $EXT_OUTPUT_FILE
                    elif [ "$OSTYPE" = "AIX" ]; then
                        $ECHO "FilterDevice=/dev/$path" >> $EXT_OUTPUT_FILE
                    fi
                else
                    if [ "$OSTYPE" = "Linux" ]; then
                        $ECHO "/dev/$path" >> $EXT_OUTPUT_FILE
                    elif [ "$OSTYPE" = "SunOS" ]; then
                        $ECHO "/dev/dsk/$path" >> $EXT_OUTPUT_FILE
                    elif [ "$OSTYPE" = "AIX" ]; then
                        $ECHO "/dev/$path" >> $EXT_OUTPUT_FILE
                    fi
                fi
            done
        done
        ;;
    */dev/dm-*)

        DM_OUTPUT_FILE=/tmp/dmoutput.$$
        aliasoption=$option
        GetFilterDevicesFromAlias $volume $DM_OUTPUT_FILE ""
        aliasvolume=`head -n 1 -q $DM_OUTPUT_FILE`
        option=$aliasoption

        rm -f $DM_OUTPUT_FILE
        if [ ! -b "$aliasvolume" ]; then
            log "Failed to find alias for $volume. GetFilterDevicesFromAlias returned $aliasvolume."
            exit 1
        fi

        if [ "$option" = "FilterDevice" ]; then
            $ECHO "VolumeNameWithLevel=$aliasvolume(VolumeName),$levelCount" >> $EXT_OUTPUT_FILE
        fi
        levelCount=`expr $levelCount + 1`

        if [ "$option" = "FilterDevice" ]; then
            WriteToDbConfFile "MountPoint=$FSType:$aliasvolume:$mount_point:$logVolume"
        fi

        if [ "$option" = "FilterDevice" ]; then
            $ECHO "FilterDevice=$aliasvolume" >> $EXT_OUTPUT_FILE
        else
            $ECHO "$volume" >> $EXT_OUTPUT_FILE
        fi

        ;;
    *)
        if [ "$option" = "FilterDevice" ]; then
            $ECHO "VolumeNameWithLevel=$volume(VolumeName),$levelCount" >> $EXT_OUTPUT_FILE
        fi
        levelCount=`expr $levelCount + 1`

        if [ "$option" = "FilterDevice" ]; then
            WriteToDbConfFile "MountPoint=$FSType:$volume:$mount_point:$logVolume"
        fi

        if [ "$OSTYPE" = "AIX" ]; then

            tempfile1=/tmp/aixpvs.$$
            lspv > $tempfile1

            found=0
            while read line
            do
                pv=`echo $line | awk '{ print $1 }'`
                if [ "$pv" = "$volume" ]; then
                    if [ "$option" = "FilterDevice" ]; then
                        $ECHO "FilterDevice=/dev/$volume" >> $EXT_OUTPUT_FILE
                        found=1
                        break
                    else
                        $ECHO "/dev/$volume" >> $EXT_OUTPUT_FILE
                    fi
                fi
            done < $tempfile1

            rm -f $tempfile1

            if [ $found = 0 ]; then
                lvname=`basename $volume`
                tempfile1=/tmp/aixlvs.$$

                pvsinvg=
                vgname=`lslv $lvname | grep "VOLUME GROUP:" | awk -F: '{ print $3 }' | sed 's/ //g'`


                if [ "$option" = "FilterDevice" ]; then
                        GetAllMountPointsForVolumeGroup $vgname
                fi

                if [ "$option" = "FilterDevice" ]; then
                    $ECHO "DiskGroupNameWithLevel=$vgname(VolumeGroupName),$levelCount" >> $EXT_OUTPUT_FILE
                    levelCount=`expr $levelCount + 1`

                    for pvname in ` lsvg -p $vgname | grep -v $vgname | grep -v DISTRIBUTION | awk '{ print $1 }'`
                    do
                        $ECHO "DiskNameWithLevel=$pvname(DiskName),$levelCount" >> $EXT_OUTPUT_FILE
                    done
                fi

                for pvname in ` lsvg -p $vgname | grep -v $vgname | grep -v DISTRIBUTION | awk '{ print $1 }'`
                do
                    if [ "$option" = "FilterDevice" ]; then
                        $ECHO "FilterDevice=/dev/$pvname" >> $EXT_OUTPUT_FILE
                        if [ -z "$pvsinvg" ]; then
                            pvsinvg=/dev/$pvname
                        else
                            pvsinvg=$pvsinvg:/dev/$pvname
                        fi
                    else
                        $ECHO "/dev/$pvname" >> $EXT_OUTPUT_FILE
                    fi
                done

                WriteToDbConfFile "VolumeGroup=$volume:$vgname:$pvsinvg"
            fi
        else
            if [ "$option" = "FilterDevice" ]; then
                $ECHO "FilterDevice=$volume" >> $EXT_OUTPUT_FILE
            else
                $ECHO "$volume" >> $EXT_OUTPUT_FILE
            fi
        fi
        ;;
    esac

    log "EXIT GetFilterDevicesFromFileSystem"

    return
}

GetFilterDevices()
{

    log "ENTER GetFilterDevices..."

    FILTER_OUTPUT_FILE=$1
    TMP_FILTER_FILE=$FILTER_OUTPUT_FILE.tmp
    FILTER_CONF_FILE=$2
    
    isAsm=0   
    isClus=0

    while read line
    do

        echo $line >> $TMP_FILTER_FILE

        case $line in
        *usingAsmDg*)
            asmFlag=`echo $line | cut -d= -f2`
            if [ $asmFlag = "TRUE" ]; then
                isAsm=1   
            fi
            ;;
        *isCluster*)
            clusFlag=`echo $line | cut -d= -f2`
            if [ $clusFlag = "YES" ]; then
                isClus=1
            fi
            ;;
        *dbFileLoc*)
            dbFileLoc=$line
            if [ $isAsm -ne 1 ]; then
                # skip the dbName
                read line
                echo $line >> $TMP_FILTER_FILE

                dbFileDirs=`echo $dbFileLoc | cut -d= -f2`
                for dbFileDir in `echo $dbFileDirs`
                do
                    case $dbFileDir in
                    */dev/vx/dsk/*)
                        GetFilterDevicesFromVxvmRaw $dbFileDir $TMP_FILTER_FILE "FilterDevice"
                    ;;
                    */dev/vx/rdsk/*)
                        GetFilterDevicesFromVxvmRaw $dbFileDir $TMP_FILTER_FILE "FilterDevice"
                    ;;
                    *)
                        GetFilterDevicesFromLvm $dbFileDir $TMP_FILTER_FILE "FilterDevice"
                    ;;
                    esac
                done
            fi
            ;;
        *DiskPath*)
            diskPathLevel=`echo $line | cut -d= -f2`
            diskPath=`echo $diskPathLevel | cut -d, -f1`
            if [ $isAsm -ne 1 ]; then
                echo "Error: found diskPath for non-ASM"
                exit 1 
            fi

            case $diskPath in
            /dev/oracleasm/disks/*)
                GetFilterDevicesFromAsmlib $diskPath $TMP_FILTER_FILE "FilterDevice"
                ;;
            /dev/raw/raw*)
                echo "Error: unsupported device $diskPath"
                log "Error: unsupported device $diskPath"
                exit 1
                #GetFilterDevicesFromRaw $diskPath $TMP_FILTER_FILE "FilterDevice"
                ;;
            /dev/vx/dsk/*)
                GetFilterDevicesFromVxvm $diskPath $TMP_FILTER_FILE "FilterDevice"
                ;;
            /dev/vx/rdsk/*)
                GetFilterDevicesFromVxvm $diskPath $TMP_FILTER_FILE "FilterDevice"
                ;;
            /dev/rdsk/*)
                GetFilterDevicesFromRaw $diskPath $TMP_FILTER_FILE "FilterDevice"
                ;;
            /dev/did/rdsk/*)
                GetFilterDevicesFromRaw $diskPath $TMP_FILTER_FILE "FilterDevice"
                ;;
            /dev/rhdisk*)
                GetFilterDevicesFromRaw $diskPath $TMP_FILTER_FILE "FilterDevice"
                ;;
            /dev/mapper/*)
                #echo "FilterDevice=$diskPath" >> $TMP_FILTER_FILE
                GetFilterDevicesFromAlias $diskPath $TMP_FILTER_FILE "FilterDevice"
                ;;
            *)
                GetFilterDevicesFromAlias $diskPath $TMP_FILTER_FILE "FilterDevice"
                ;;
            esac
            ;;
        *)
            ;;
        esac

    done < $FILTER_OUTPUT_FILE

    $MOVE -f $TMP_FILTER_FILE $FILTER_OUTPUT_FILE

    log "EXIT GetFilterDevices"

    return
}
    
GetAsmDevices()
{
    log "ENTER GetAsmDevices..."

    if [ "${OSTYPE}" = "SunOS" ];then
        ORATAB="/var/opt/oracle/oratab"
    else
        ORATAB="/etc/oratab"
    fi

    if [ ! -f $ORATAB ]; then
        log "Error: File $ORATAB not found"
        exit 0
    fi

    numDbs=`cat $ORATAB | grep -v "^#" | grep -v "^$" | grep -v "^*:" | grep "+ASM" | wc -l`

    if [ $numDbs -eq 0 ]; then
        log "No ASM installed"
        return
    fi


    line=`cat $ORATAB | grep -v "^#" | grep -v "^*:" | grep -v "^$" | grep "+ASM" | awk -F# '{ print $1 }'`
    DB_NAME=`echo $line | cut -d: -f1`
    DB_LOC=`echo $line | cut -d: -f2`

    ORACLE_USER=`ps -ef | grep asm_pmon | grep -i $DB_NAME | grep -v grep |awk '{ print $1 }'`
    if [ -z "$ORACLE_USER" ]; then
        log "Error: Oracle user not found..."
        exit 1 
    fi

    DB_INS=`ps -ef | grep asm_pmon |  grep -i $DB_NAME | grep -v grep | awk '{ print $NF}' |  sed -e 's/asm\_pmon\_//'`
    if [ ! -z "$DB_INS" ]; then
 
        tempfile1=/tmp/asmdisctemp.$$
        ORACLE_SID=$DB_INS
        ORACLE_HOME=$DB_LOC

        su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile1
                        select status from v\\\$instance;
_EOF"
        grep "STARTED" $tempfile1>> /dev/null
        if [ $? -eq 0 ]
        then
            log "Database started and opened"

            su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile1
                        select path from v\\\$asm_disk;
_EOF"
        fi

        Output "ASM Disks -"
        tempfile2=/tmp/tasmdisctemp.$$
        egrep -v "^$|PATH|--|rows selected" $tempfile1 > $tempfile2

        TEMP_ASM_OUTPUT_FILE=/tmp/asmdevlib.$$
        >$TEMP_ASM_OUTPUT_FILE

        while read line
        do
            case $line in
            /dev/raw/raw*)
                GetFilterDevicesFromRaw $line $TEMP_ASM_OUTPUT_FILE ""
                ;;
            /dev/rdsk/*)
                GetFilterDevicesFromRaw $line $TEMP_ASM_OUTPUT_FILE ""
                ;;
            /dev/did/rdsk/*)
                GetFilterDevicesFromRaw $line $TEMP_ASM_OUTPUT_FILE ""
                ;;
            /dev/oracleasm/disks/*)
                asmlibdev=$line
                GetFilterDevicesFromAsmlib $asmlibdev $TEMP_ASM_OUTPUT_FILE ""
                ;;
            *ORCL*)
                asmlable=`echo $line | cut -d: -f2`
                asmlibdev=/dev/oracleasm/disks/$asmlable
                GetFilterDevicesFromAsmlib $asmlibdev $TEMP_ASM_OUTPUT_FILE ""
                ;;
            /dev/vx/dsk/*)
                GetFilterDevicesFromVxvm $line $TEMP_ASM_OUTPUT_FILE ""
                ;;
            /dev/vx/rdsk/*)
                GetFilterDevicesFromVxvm $line $TEMP_ASM_OUTPUT_FILE ""
                ;;
            /dev/rhdisk*)
                GetFilterDevicesFromRaw $line $TEMP_ASM_OUTPUT_FILE ""
                ;;
            *)
                GetFilterDevicesFromAlias $line $TEMP_ASM_OUTPUT_FILE ""
                ;;
            esac
        done < $tempfile2

        cat $TEMP_ASM_OUTPUT_FILE | sort | uniq >> $TMP_OUTPUT_FILE

        rm -rf $TEMP_ASM_OUTPUT_FILE > /dev/null

        rm -rf $tempfile1 > /dev/null
        rm -rf $tempfile2 > /dev/null
    fi

    log "EXIT GetAsmDevices"

    return
}

GetOcrVoteDevices()
{
    CRSD_PATH=`ps -ef|grep crsd.bin|grep -v grep |awk '{ print $(NF-1) }' | uniq`

    if [ ! -z "${CRSD_PATH}" ]; then
        log "Cluster service detected"
        CRSD_DIRECTORY=`dirname ${CRSD_PATH}`
        export CRSD_DIRECTORY

        #VOTE_CMD=`ps -ef|grep crsd.bin|grep -v grep |awk '{ print $(NF-1) }'| uniq | sed 's/crsd\.bin/crsctl/'`
        #OCR_CMD=`ps -ef|grep crsd.bin|grep -v grep |awk '{ print $(NF-1) }'| uniq | sed 's/crsd\.bin/ocrcheck/'`
        VOTE_CMD=$CRSD_DIRECTORY/crsctl
        OCR_CMD=$CRSD_DIRECTORY/ocrcheck

        if [ ! -f "$VOTE_CMD" ]; then
            log "Error: crsctl command not found"
            return
        fi

        if [ ! -f "$OCR_CMD" ]; then
            log "Error: ocrcheck command not found"
            return
        fi

        TEMP_VOTE_OUTPUT_FILE=/tmp/votedevlist.$$
        Output "VoteDeviceListStart"
        for line in `$VOTE_CMD query css votedisk | grep -v Located |  awk '{print $3}'`
        do
            usingFileSystem=1
            file $line | grep "block special" > /dev/null
            if [ $? -eq 0 ]; then
                usingFileSystem=0
            fi
            file $line | grep "character special" > /dev/null
            if [ $? -eq 0 ]; then
                usingFileSystem=0
            fi

            if [ $usingFileSystem -eq 1 ]; then

                GetFilterDevicesFromLvm $line $TEMP_VOTE_OUTPUT_FILE ""
            else
                case $line in
                */dev/raw/raw*)
                    GetFilterDevicesFromRaw $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                */dev/rdsk/*)
                    GetFilterDevicesFromRaw $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                */dev/did/rdsk/*)
                    GetFilterDevicesFromRaw $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                */dev/oracleasm/disks/*)
                    asmlibdev=$line
                    GetFilterDevicesFromAsmlib $asmlibdev $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                *ORCL*)
                    asmlable=`echo $line | cut -d: -f2`
                    asmlibdev=/dev/oracleasm/disks/$asmlable
                    GetFilterDevicesFromAsmlib $asmlibdev $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                */dev/vx/dsk/*)
                    GetFilterDevicesFromVxvm $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                */dev/vx/rdsk/*)
                    GetFilterDevicesFromVxvm $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                /dev/rhdisk*)
                    GetFilterDevicesFromRaw $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                *)
                    GetFilterDevicesFromAlias $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                esac
            fi

            cat $TEMP_VOTE_OUTPUT_FILE | sort | uniq >> $TMP_OUTPUT_FILE

        done
        Output "VoteDeviceListEnd"

        Output "OcrDeviceListStart"
        TEMP_OCR_OUTPUT_FILE=/tmp/ocrdevlist.$$
        for line in `$OCR_CMD | grep "Device/File Name" | awk -F: '{print $2}'`
        do
            usingFileSystem=1
            file $line | grep "block special" > /dev/null
            if [ $? -eq 0 ]; then
                usingFileSystem=0
            fi
            file $line | grep "character special" > /dev/null
            if [ $? -eq 0 ]; then
                usingFileSystem=0
            fi

            if [ $usingFileSystem -eq 1 ]; then

                GetFilterDevicesFromLvm $line $TEMP_VOTE_OUTPUT_FILE ""
            else
                case $line in
                */dev/raw/raw*)
                    GetFilterDevicesFromRaw $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                */dev/rdsk/*)
                    GetFilterDevicesFromRaw $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                */dev/did/rdsk/*)
                    GetFilterDevicesFromRaw $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                */dev/oracleasm/disks/*)
                    asmlibdev=$line
                    GetFilterDevicesFromAsmlib $asmlibdev $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                *ORCL*)
                    asmlable=`echo $line | cut -d: -f2`
                    asmlibdev=/dev/oracleasm/disks/$asmlable
                    GetFilterDevicesFromAsmlib $asmlibdev $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                */dev/vx/dsk/*)
                    GetFilterDevicesFromVxvm $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                */dev/vx/rdsk/*)
                    GetFilterDevicesFromVxvm $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                /dev/rhdisk*)
                    GetFilterDevicesFromRaw $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                *)
                    GetFilterDevicesFromAlias $line $TEMP_VOTE_OUTPUT_FILE ""
                    ;;
                esac
            fi

            cat $TEMP_VOTE_OUTPUT_FILE | sort | uniq >> $TMP_OUTPUT_FILE
        done

        Output "OcrDeviceListEnd"

    else
        log "Cluster service not detected"
    fi

    return
}

GetDatabaseName()
{
    ORACLE_SID=$1
    ORACLE_HOME=$2
    ORACLE_USER=$3
    DB_NAME=

    tempfile=/tmp/dbstate.$$

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' << _EOF >$tempfile
            select status from v\\\$instance;
_EOF"

    grep "OPEN" $tempfile>> /dev/null
    if [ $? -eq 0 ]
    then
        log "Database started and opened"
    else
        log "Database not started"
        rm -f $tempfile
        return
    fi

    rm -f $tempfile

    gverfile=/tmp/dbname.$$

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF > $gverfile
                   select version from v\\\$instance;
_EOF"

    grep "VERSION" $gverfile > /dev/null
    if [ $? -eq 0 ]; then
        DB_VER=`grep -v "^$" $gverfile | grep -v "VERSION" | grep -v -- "--"`
    else
        log "Database version not found for SID $ORACLE_SID"
        $ECHO "Database version not found for SID $ORACLE_SID"
        rm -f $gverfile
        exit 1
    fi

    rm -f $gverfile

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' << _EOF > $tempfile
     select parallel from v\\\$instance;
_EOF"

    grep "PAR" $tempfile > /dev/null
    if [ $? -eq 0 ]; then
        IS_CLUSTER=`grep -v "^$" $tempfile | grep -v "PAR" | grep -v -- "--"`
    else
        log "IS_CLUSTER not found for SID $ORACLE_SID"
        $ECHO "IS_CLUSTER not found for SID $ORACLE_SID"
        rm -f $tempfile
        exit 1
    fi

    rm -f $tempfile

    echo $DB_VER | awk -F"." '{print $1}' | grep 9 > /dev/null
    if [ $? -eq 0 ]; then
        DB_NAME=$ORACLE_SID
        if [ "$ppname" = "appservice" -a "$IS_CLUSTER" = "YES" ]; then
            DB_NAME=
        fi
        return
    fi

    gdbnfile=/tmp/dbname.$$
    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF > $gdbnfile
                   select db_unique_name from v\\\$database;
_EOF"

    grep "DB_UNIQUE_NAME" $gdbnfile > /dev/null
    if [ $? -eq 0 ]; then
        DB_NAME=`grep -v "^$" $gdbnfile | grep -v "DB_UNIQUE_NAME" | grep -v -- "--"`
    else
        log "Database name not found for SID $ORACLE_SID"
        $ECHO "Database name not found for SID $ORACLE_SID"
        rm -f $gdbnfile
        exit 1
    fi

    if [ -z "$DB_NAME" ]; then
        log "Database name not found for SID $ORACLE_SID"
        $ECHO "Database name not found for SID $ORACLE_SID"
        rm -f $gdbnfile
        exit 1
    fi

    rm -f $gdbnfile
    return
}

DiscoverDatabases()
{
    log "ENTER DiscoverDatabases..."

    if [ "$ppname" = "appservice" ]; then
        APPWIZARD_CONF=$INSTALLPATH/etc/appwizard.conf
        Output "DatabaseListStart"
        if [ ! -f "$APPWIZARD_CONF" ]; then
            log " File $INSTALLPATH/etc/appwizard.conf not found"
        else
            for line in `grep OracleDatabase=NO $APPWIZARD_CONF`
            do
               DB_NAME=`echo $line | cut -d: -f2`
               Output "DatabaseStart"
               Output "Unregister=$DB_NAME"
               Output "DatabaseEnd"
            done
        fi
    fi

    dbIns=`ps -ef | grep ora_pmon | grep -v grep | awk '{ print $NF}' |  sed -e 's/ora\_pmon\_//'`
    for DB_INS in $dbIns
    do
        insPid=`ps -ef | grep -w ora_pmon_${DB_INS} | grep -v grep | awk '{print $2}'`

        if [ "$OSTYPE" = "Linux" ]; then
            cat /proc/${insPid}/environ | grep ORACLE_HOME > $NULL
            if [ $? -ne 0 ]; then
                log "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                $ECHO "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                exit 1
            fi
            DB_LOC=`cat /proc/${insPid}/environ | tr '\0' '\n' | grep ORACLE_HOME | awk -F'=' '{print $2}'`
        elif [ "${OSTYPE}" = "SunOS" ];then
            pargs -e $insPid | grep ORACLE_HOME > $NULL
            if [ $? -ne 0 ]; then
                log "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                $ECHO "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                exit 1
            fi
            DB_LOC=`pargs -e $insPid | grep ORACLE_HOME | awk -F= '{print $2}'`
        elif [ "${OSTYPE}" = "AIX" ];then
            ps ewww $insPid | grep -v PID | grep ORACLE_HOME > $NULL
            if [ $? -ne 0 ]; then
                log "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                $ECHO "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                exit 1
            fi
            DB_LOC=`ps ewww $insPid | grep -v PID | awk '{for(i=2;i<=NF;i++) print $i}' | grep ORACLE_HOME | awk -F= '{print $2}'`
        fi

        ORACLE_USER=`ps -ef | grep ora_pmon | grep $DB_INS | grep -v grep |awk '{ print $1 }'`

        GetDatabaseName $DB_INS $DB_LOC $ORACLE_USER
        
        log "DB Name=$DB_NAME"
        log "DB Instance=$DB_INS"

        if [ ! -z "$DB_NAME" ]; then

            DB_VIEWS_FILE="/tmp/$DB_NAME.$$"
            DB_CONF_FILE="$INSTALLPATH/etc/$DB_NAME.conf"

            Output "DatabaseStart"
            Output "ConfigFile=$DB_CONF_FILE"

            log "OracleUser=$ORACLE_USER"
            log "OracleHome=$DB_LOC"
            log "OracleSID=$DB_INS"


            TMP_DB_CONF_FILE=$TMP_CONF_FILE.$DB_INS
            rm -f /tmp/oracledb_pfile.ora

            GetDbViews $ORACLE_USER $DB_INS $DB_LOC $DB_VIEWS_FILE $TMP_DB_CONF_FILE

            grep DBName $DB_VIEWS_FILE > $NULL
            if [ $? -ne 0 ]; then
                log "Error: db views does not have DBName"
                exit 1
            fi

            pFileLocation="$DB_LOC/dbs/init$DB_INS.ora"
            GeneratedPfile="$INSTALLPATH/etc/init$DB_INS.ora_pfile"

            spfile=`grep "SPFile=" $TMP_DB_CONF_FILE | awk -F"=" '{print $2}'`
            if [ -z "$spfile" ]; then
                if [ ! -f $pFileLocation ] ; then 
                    log "Error: neither pfile nor spfile found"
                    exit 1
                fi

                grep -i "SPFILE=" $pFileLocation > $NULL
                if [ $? -eq 0 ]; then
                    log "Error: pfile $pFileLocation has spfile location, but spfile not found from database"
                    exit 1   
                fi
                cp -f $pFileLocation $GeneratedPfile 
            else
                if [ ! -f $pFileLocation ] ; then 
                    log "Warning: pfile not found"
                    pFileLocation=$INSTALLPATH/etc/init$DB_INS.ora
                    cp -f "/tmp/oracledb_pfile.ora" $pFileLocation
                fi

                mv -f "/tmp/oracledb_pfile.ora" $GeneratedPfile 
            fi

            Output "PFileLocation=$pFileLocation"

            grep FilterDevice $DB_VIEWS_FILE > $NULL
            if [ $? -ne 0 ]; then
                log "Error: db views does not have FilterDevice"
                exit 1
            fi

            cat $TMP_CONF_FILE > $DB_CONF_FILE
            $ECHO "OracleHome=$DB_LOC"  >> $DB_CONF_FILE
            $ECHO "OracleSID=$DB_INS" >> $DB_CONF_FILE
            $ECHO "OracleUser=$ORACLE_USER" >> $DB_CONF_FILE

            $ECHO "OracleGroup=$ORACLE_GROUP" >> $DB_CONF_FILE
            $ECHO "PFileLocation=$pFileLocation" >> $DB_CONF_FILE
            cat $TMP_DB_CONF_FILE | sort | uniq >> $DB_CONF_FILE
    
            grep local_listener= $GeneratedPfile > $NULL
            if [ $? -eq 0 ]; then
                listenerName=`grep local_listener= $GeneratedPfile | awk -F= '{print $2}' | sed -e "s/'//g"`
                $ECHO "local_listener=$listenerName" >> $DB_CONF_FILE
            fi

            grep remote_listener= $GeneratedPfile > $NULL
            if [ $? -eq 0 ]; then
                listenerName=`grep remote_listener= $GeneratedPfile | awk -F= '{print $2}' | sed -e "s/'//g"`
                $ECHO "remote_listener=$listenerName" >> $DB_CONF_FILE
            fi

            Output "GeneratedPfile=$GeneratedPfile"

            cat $DB_VIEWS_FILE >> $TMP_OUTPUT_FILE
            Output "DatabaseEnd"

            rm -rf $DB_VIEWS_FILE > $NULL
            rm -rf $TMP_DB_CONF_FILE > $NULL
        fi
    done 

    Output "DatabaseListEnd"

    log "EXIT DiscoverDatabases"

    return
}

GetNodeDetails()
{
    log "ENTER GetNodeDetails..."

    WriteToConfFile "MachineName=$HOSTNAME"

    hostId=`cat $INSTALLPATH/etc/drscout.conf | grep -i HostID | awk -F"=" '{print $2}' | sed "s/ //g"`
    if [ -z "$hostId" ] ; then
        log "Error: Host id not found in file $INSTALLPATH/etc/drscout.conf"
        exit 1 
    fi

    WriteToConfFile "HostID=$hostId"

    log "EXIT GetNodeDetails"

    return
}

GetClusterInformation()
{
    log "ENTER GetClusterInformation..."

    Output "ClusterInfoStart"
    CRSD_PATH=`ps -ef|grep crsd.bin|grep -v grep |awk '{ print $(NF-1) }' | uniq`

    if [ ! -z "${CRSD_PATH}" ]; then
        log "Cluster service detected"
        CRSD_DIRECTORY=`dirname ${CRSD_PATH}`
        export CRSD_DIRECTORY

        CLUSTER_CMD=`ps -ef|grep crsd.bin|grep -v grep |awk '{ print $(NF-1) }'| uniq | sed 's/crsd\.bin/cemutlo \-n/'`

        if [ -z "$CLUSTER_CMD" ]; then
            log "Error: cluster command not found"
            exit 1 
        fi

        CLUSTER_NAME=`$CLUSTER_CMD`
        if [ -z "$CLUSTER_NAME" ]; then
            log "Error: cluster name not found"
            exit 1 
        fi

        export CLUSTER_NAME
        Output "ClusterName=$CLUSTER_NAME"
        WriteToConfFile "ClusterName=$CLUSTER_NAME"

        ${CRSD_DIRECTORY}/olsnodes > $NULL
        if [ $? -ne 0 ]; then
            log "Error: Failed to get cluster nodes"
            exit 1
        fi

        NODENAME=`${CRSD_DIRECTORY}/olsnodes -l`
        Output "NodeName=$NODENAME"

        OTHER_CLUSTER_NODES=`${CRSD_DIRECTORY}/olsnodes | grep -v $NODENAME | tr -s '\n' ','`
        Output "OtherClusterNodes=$OTHER_CLUSTER_NODES"

        TOTAL_CLUSTER_NODES=`${CRSD_DIRECTORY}/olsnodes | wc -l | sed 's/ //g'`
        WriteToConfFile "TotalClusterNodes=$TOTAL_CLUSTER_NODES"
    else
        log "Cluster service not detected"
    fi

    Output "ClusterInfoEnd"

    log "EXIT GetClusterInformation"

    return
}

OracleShutdownLocal()
{
    log "ENTER OracleShutdownLocal..."

    log "Shutting down Oracle database instance $ORACLE_SID on local host"
    $ECHO "Shutting down Oracle database instance $ORACLE_SID on local host"

    result=`su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF
                			shutdown immediate;
_EOF"`    

    log "Result from OracleShutdownLocal: $result"

    echo $result | grep "ORACLE instance shut down." > $NULL
    if [ $? -ne 0 ]; then
        $ECHO "Oracle instance $ORACLE_SID shutdown failed"
        log "Oracle instance $ORACLE_SID shutdown failed"
        exit 1
    fi

    log "Oracle database instance $ORACLE_SID is shutdown on local host"
    $ECHO "Oracle database instance $ORACLE_SID is shutdown on local host"

    log "EXIT OracleShutdownLocal"
    return
}

OracleShutdownCluster()
{
    RESULT_FILE=/tmp/kc8j2c.log.$$
    export RESULT_FILE

    su - $ORACLE_USER -c "ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME;PATH=$ORACLE_HOME/bin:$PATH; export PATH;srvctl status database -d $DB_NAME > $RESULT_FILE"
    outputCount=`cat $RESULT_FILE | grep "not running" | wc -l`
    if [ $numNodes -eq $outputCount ]; then
        log "Oracle database $DB_NAME offline on all $outputCount cluster nodes."
        $ECHO "Oracle database $DB_NAME offline on all $outputCount cluster nodes."
        rm -f $RESULT_FILE
        return
    else
        remNodes=`expr $numNodes - $outputCount`
        log "Oracle database $DB_NAME needs to be shutdown on $remNodes other cluster nodes."
        $ECHO "Oracle database $DB_NAME needs to be shutdown on $remNodes other cluster nodes."
    fi

    su - $ORACLE_USER -c "ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME;PATH=$ORACLE_HOME/bin:$PATH; export PATH;srvctl stop database -d $DB_NAME > $RESULT_FILE"

    cat $RESULT_FILE

    su - $ORACLE_USER -c "ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME;PATH=$ORACLE_HOME/bin:$PATH; export PATH;srvctl status database -d $DB_NAME > $RESULT_FILE"
    outputCount=`cat $RESULT_FILE | grep "not running" | wc -l`
    if [ $numNodes -ne $outputCount ]; then
        remNodes=`expr $numNodes - $outputCount`
        log "Oracle database $DB_NAME shutdown failed on $remNodes out of $numNodes cluster nodes."
        $ECHO "Oracle database $DB_NAME shutdown failed on $remNodes out of $numNodes cluster nodes."
        $ECHO
        cat $RESULT_FILE
        $ECHO
        rm -f $RESULT_FILE
        exit 1
    fi

    $ECHO "Oracle db shutdown succeeded on $outputCount cluster nodes."
    rm -f $RESULT_FILE
    return
}


AsmDgDismount()
{
    log "ENTER AsmDgDismount..."

    diskGroupName=$1

    ORACLE_SID=`ps -ef | grep asm_pmon |  grep "+ASM" | grep -v grep | awk '{ print $NF}' |  sed -e 's/asm\_pmon\_//'`
    if [ -z "$ORACLE_SID" ]; then
        log "ASM is not online"
        $ECHO "ASM is not online"
        return
    fi

    ORACLE_USER=`ps -ef | grep asm_pmon | grep "+ASM" | grep -v grep |awk '{ print $1 }'`
    if [ -z "$ORACLE_USER" ]; then
        log "oracle user not found"
        $ECHO "oracle user not found"
        exit 1
    fi

    ORACLE_HOME=`cat $ORATAB | grep "+ASM" | cut -d: -f2`
    if [ -z "$ORACLE_HOME" ]; then
        log "oracle home not found"
        $ECHO "oracle home not found"
        exit 1
    fi

    asmverfile=/tmp/asmname.$$

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF > $asmverfile
                   select version from v\\\$instance;
_EOF"

    grep "VERSION" $asmverfile > /dev/null
    if [ $? -eq 0 ]; then
        ASM_VER=`grep -v "^$" $asmverfile | grep -v "VERSION" | grep -v -- "--"`
    else
        log "ASM version not found for SID $ORACLE_SID"
        $ECHO "ASM version not found for SID $ORACLE_SID"
        rm -f $asmverfile
        exit 1
    fi

    rm -f $asmverfile

    tempfile=/tmp/kl3cls0.$$

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF > $tempfile
                            select name, state from v\\\$asm_diskgroup;
_EOF" 

    cat $tempfile | grep -w $diskGroupName > $NULL
    if [ $? -ne 0 ]; then
        log "ASM diskgroup $diskGroupName not present"
        $ECHO "ASM diskgroup $diskGroupName not present"
        rm -f $tempfile > $NULL
        return
    fi

    cat $tempfile | grep -w $diskGroupName | grep -w MOUNTED > $NULL
    if [ $? -ne 0 ]; then
        log "ASM diskgroup $diskGroupName not mounted"
        $ECHO "ASM diskgroup $diskGroupName not mounted"
        rm -f $tempfile > $NULL
        return
    fi

    ASMUSER=sysdba
    echo $ASM_VER | awk -F"." '{print $1}' | grep 11 > /dev/null
    if [ $? -eq 0 ]; then
        release=`echo $ASM_VER | awk -F"." '{print $2}'`
        if [ $release -gt 1 ]; then
            ASMUSER=sysasm
        fi
    fi

    result=`su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as $ASMUSER' << _EOF
                            alter diskgroup $diskGroupName dismount;
_EOF"`    

    log "Result from AsmDgDismount: $result"

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF > $tempfile
                            select state from v\\\$asm_diskgroup where name='$diskGroupName';
_EOF"

    cat $tempfile | grep "DISMOUNTED" > $NULL
    if [ $? -ne 0 ]; then
        log "ASM diskgroup $diskGroupName not dismounted"
        $ECHO "Error: ASM diskgroup $diskGroupName not dismounted"
        exit 1
    fi

    if [ $isCluster = "YES" ]; then
        su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF > $tempfile
                                select state from gv\\\$asm_diskgroup where name='$diskGroupName';
_EOF"

        cat $tempfile | grep -w "MOUNTED" > $NULL
        if [ $? -eq 0 ]; then
            log "ASM diskgroup $diskGroupName not dismounted on some of the nodes"
            $ECHO "Error: ASM diskgroup $diskGroupName not dismounted on some of the nodes"
            rm -f $tempfile > $NULL
            exit 1
        fi

    fi

    $ECHO " ASM diskgroup $diskGroupName dismounted"
    rm -f $tempfile > $NULL

    log "EXIT AsmDgDismount"
}

ChangeOwnerForASMDevices()
{
    log "ENTER ChangeOwnerForASMDevices..."

    diskGroupName=$1

    ORACLE_SID=`ps -ef | grep asm_pmon |  grep "+ASM" | grep -v grep | awk '{ print $NF}' |  sed -e 's/asm\_pmon\_//'`
    if [ -z "$ORACLE_SID" ]; then
        log "ASM is not online"
        $ECHO "ASM is not online"
        return
    fi

    ORACLE_USER=`ps -ef | grep asm_pmon | grep "+ASM" | grep -v grep |awk '{ print $1 }'`
    if [ -z "$ORACLE_USER" ]; then
        log "oracle user not found"
        $ECHO "oracle user not found"
        exit 1
    fi

    ORACLE_HOME=`cat $ORATAB | grep "+ASM" | cut -d: -f2`
    if [ -z "$ORACLE_HOME" ]; then
        log "oracle home not found"
        $ECHO "oracle home not found"
        exit 1
    fi

    tempfile=/tmp/JeNmw1k.$$

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF > $tempfile
                            select name, state from v\\\$asm_diskgroup;
_EOF" 

    cat $tempfile | grep -w $diskGroupName > $NULL
    if [ $? -ne 0 ]; then
        log "ASM diskgroup $diskGroupName not present"
        $ECHO "ASM diskgroup $diskGroupName not present"
        rm -f $tempfile > $NULL
        return
    fi

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF > $tempfile
                            select group_number from v\\\$asm_diskgroup where name='$diskGroupName';
_EOF"
    groupNumber=`cat $tempfile | grep -v GROUP_NUMBER | grep -v "\-\-" | grep -v "^$" | grep -v "rows selected"`

    if [ -z "$groupNumber" ]; then
        log "ASM diskgroup $diskGroupName not present"
        $ECHO "ASM diskgroup $diskGroupName not present"
        rm -f $tempfile > $NULL
        exit 1 
    fi

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile
                        select path from v\\\$asm_disk where group_number=$groupNumber;
_EOF"

    while read line
    do
        case $line in
        */dev/raw/raw*)
            chown root:root $line
            chmod 660 $line
            ;;
        */dev/rdsk/*)
            chown root:sys $line
            chmod 660 $line
            blockDevice=`echo $line | sed -e 's/rdsk/dsk/g'`
            chown root:sys $blockDevice
            chmod 660 $blockDevice
            ;;
        */dev/did/rdsk/*)
            chown root:sys $line
            chmod 660 $line
            blockDevice=`echo $line | sed -e 's/rdsk/dsk/g'`
            chown root:sys $blockDevice
            chmod 660 $blockDevice
            ;;
        *ORCL*)
            asmlable=`echo $line | cut -d: -f2`
            asmlibdev=/dev/oracleasm/disks/$asmlable
            chown root:root $asmlibdev
            chmod 660 $asmlibdev
            ;;
        */dev/vx/rdsk/*)
            chown root:sys $line
            chmod 660 $line
            blockDevice=`echo $line | sed -e 's/rdsk/dsk/g'`
            chown root:sys $blockDevice
            chmod 660 $blockDevice
            ;;
        */dev/vx/dsk/*)
            chown root:sys $line
            chmod 660 $line
            rawDevice=`echo $line | sed -e 's/dsk/rdsk/g'`
            chown root:sys $rawDevice
            chmod 660 $rawDevice
            ;;
        */dev/rhdisk*)
            chown root:system $line
            chmod 600 $line
            ;;
        */dev/*)
            chown root:root $line
            chmod 660 $line
            ;;
        esac
    done < $tempfile

    rm -f $tempfile > /dev/null

    log "EXIT ChangeOwnerForASMDevices"

    return
}

TakeDatabaseOffline()
{
    log "ENTER TakeDatabaseOffline..."

    if [ "${OSTYPE}" = "SunOS" ];then
        ORATAB="/var/opt/oracle/oratab"
    else
        ORATAB="/etc/oratab"
    fi


    for TGT_CONFIG_FILE  in `echo $TGT_CONFIG_FILES | sed 's/:/ /g'`
    do
 
    if [ ! -f "$TGT_CONFIG_FILE" ]; then
        log "Target config file $TGT_CONFIG_FILE not found."
        $ECHO "Error: Target config file $TGT_CONFIG_FILE not found."
        exit 1
    fi

    dbIns=`ps -ef | grep ora_pmon |  grep -v grep | awk '{ print $NF}' |  sed -e 's/ora\_pmon\_//'`
    for DB_INS in $dbIns
    do
        insPid=`ps -ef | grep -w ora_pmon_${DB_INS} | grep -v grep | awk '{print $2}'`

        if [ "$OSTYPE" = "Linux" ]; then
            cat /proc/${insPid}/environ | grep ORACLE_HOME > $NULL
            if [ $? -ne 0 ]; then
                log "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                $ECHO "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                exit 1
            fi
            ORACLE_HOME=`cat /proc/${insPid}/environ | tr '\0' '\n' | grep ORACLE_HOME | awk -F'=' '{print $2}'`
        elif [ "${OSTYPE}" = "SunOS" ];then
            pargs -e $insPid | grep ORACLE_HOME > $NULL
            if [ $? -ne 0 ]; then
                log "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                $ECHO "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                exit 1
            fi
            ORACLE_HOME=`pargs -e $insPid | grep ORACLE_HOME | awk -F= '{print $2}'`
        elif [ "${OSTYPE}" = "AIX" ];then
            ps ewww $insPid | grep -v PID | grep ORACLE_HOME > $NULL
            if [ $? -ne 0 ]; then
                log "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                $ECHO "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                exit 1
            fi
            ORACLE_HOME=`ps ewww $insPid | grep -v PID | awk '{for(i=2;i<=NF;i++) print $i}' | grep ORACLE_HOME | awk -F= '{print $2}'`
        fi

        ORACLE_USER=`ps -ef | grep ora_pmon | grep $DB_INS | grep -v grep |awk '{ print $1 }'`

        GetDatabaseName $DB_INS $ORACLE_HOME $ORACLE_USER
        
        log "DB Name=$DB_NAME"
        log "DB Instance=$DB_INS"

        if [ ! -z "$DB_NAME" ]; then
            ORACLE_SID=$DB_INS
            grep "OracleDBName=$DB_NAME" $TGT_CONFIG_FILE > $NULL
            if [ $? -eq 0 ]; then
                OracleShutdownLocal
            fi

        fi
    done
    isZfs=`grep 'isZfs=' $TGT_CONFIG_FILE | awk -F= '{print $2}'`

    if [ "$isZfs" = "YES" ];then
        TgtMountPoints=`grep "MountPoint=" $TGT_CONFIG_FILE | uniq |awk -F"=" '{print $2}'`
        for volMountPoint in $TgtMountPoints
        do
           fsType=`echo $volMountPoint |awk -F":" '{ print $1 }'`
           DiskPath=`echo $volMountPoint |awk -F":" '{ print $2 }'`
           MountPoint=`echo $volMountPoint |awk -F":" '{ print $3 }'`
           if [ "$fsType" = "zfs" ];then
               mount | grep -w $MountPoint > $NULL
               if [ $? -eq 0 ]; then
                   $ECHO "Unmounting $MountPoint"
                   umount $MountPoint
                   if [ $? -ne 0 ]; then
                       echo "umount $MountPoint failed"
                   fi
               fi

               TgtDevices=`cat $INSTALLPATH/etc/OracleTgtVolInfoFile.dat|grep "VolumeType=Disk"|awk -F: '{print $1}'|awk -F= '{print $2}'|awk -F/ '{print $NF}'`
               PoolName=`echo $DiskPath |awk -F/ '{print $1}'`
               TgtZpools=`zpool list |grep -v NAME|awk '{print $1}'`
               for TgtPool in $TgtZpools
               do
                  if [ "$PoolName" = "$TgtPool" ];then
                      log " Found $PoolName is active on target machine, destroying it"
                      zpool destroy $PoolName 
                      if [ $? -eq 0 ];then
                          log "Successfully destroyed the pool $PoolName"
                      else
                         #TODO Check whether it is possible to capture the error output
                         log "Unable to destroy the pool $PoolName"
                         exit 1
                      fi
                  fi
                  zpool status $TgtPool |grep 'c[0-9]'|awk '{print $1}' > /tmp/zpooldisks
                  for tgtVol in $TgtDevices
                  do
                     grep $tgtVol /tmp/zpooldisks > /dev/null
                     if [ $? -eq 0 ];then
                         echo "$tgtVol is part of already active zpool $TgtPool on the target, destroying it"
                         log "$tgtVol is part of already active zpool $TgtPool on the target, destroying it"
                         zpool destroy $TgtPool
                     fi
                  done
               done
           fi
        done
    fi

    done

    for TGT_CONFIG_FILE  in `echo $TGT_CONFIG_FILES | sed 's/:/ /g'`
    do
    isCluster=`grep 'isCluster=' $TGT_CONFIG_FILE | awk -F= '{print $2}'`
    numNodes=`grep 'TotalDbNodes=' $TGT_CONFIG_FILE | awk -F= '{print $2}'`
    isASM=`grep isASM= $TGT_CONFIG_FILE | cut -d= -f2`
    isASMRaw=`grep isASMRaw= $TGT_CONFIG_FILE | cut -d= -f2`
    
    if [ "$isASM" = "YES" ]; then
        
        dgNames=`grep DiskGroupName $TGT_CONFIG_FILE | cut -d= -f2`
        for dgName in `echo $dgNames | sed 's/,/ /g'`
        do
            $ECHO
            $ECHO "Unmount ASM disk group $dgName"
            AsmDgDismount $dgName

            if [ "$isASMRaw" = "YES" ]; then
                ChangeOwnerForASMDevices $dgName
            fi
        done
    fi

    isASMLib=`grep isASMLib= $TGT_CONFIG_FILE | cut -d= -f2`
    
    if [ "$isASMLib" = "YES" ]; then
        diskLabels=`grep ASMLibDiskLabels= $TGT_CONFIG_FILE | cut -d= -f2`
        for label in `echo $diskLabels | sed 's/,/ /g'`
        do
            /etc/init.d/oracleasm listdisks | grep $label > $NULL
            if [ $? -eq 0 ]; then
                $ECHO
                $ECHO "Deleting ASMLib disk label $label"
                /etc/init.d/oracleasm deletedisk $label
                if [ $? -ne 0 ]; then
                    log "Error: ASMLib deletedisk failed"
                    $ECHO "Error: ASMLib deletedisk failed"
                    exit 1
                fi
            fi
        done
    fi

    if [ "$isASMRaw" = "YES" ]; then
        paths=`grep ASMRawDiskPaths= $TGT_CONFIG_FILE | cut -d= -f2`
        for path in `echo $paths | sed 's/,/ /g'`
        do
           if [ "$OSTYPE" = "SunOS" ];then
            	chown root:sys $path
           else
           	chown root:root $path
           fi
           chmod 660 $path
        done
    fi

    MOUNTPOINTS=`grep "MountPoint=" $TGT_CONFIG_FILE |awk -F"=" '{print $2}'`

    vmpfile1=/tmp/vmpfile1.$$
    for volMountPoint in $MOUNTPOINTS
    do
        mountPoint=`echo $volMountPoint |awk -F":" '{ print $3 }'`
        echo $mountPoint >> $vmpfile1
    done

    if [ -f $vmpfile1 ]; then
        for mountPoint in `cat $vmpfile1 | sort -r` 
        do
            mount | grep -w $mountPoint > $NULL
            if [ $? -eq 0 ]; then
                $ECHO "Unmounting $mountPoint"
                umount $mountPoint
                if [ $? -ne 0 ]; then
                    echo "umount $mountPoint failed"
                    exit 1 
                fi
            fi
        done
        rm -f $vmpfile1
    fi

    status=0
    isVxvm=`grep "isVxvm=" $TGT_CONFIG_FILE|uniq | awk -F"=" '{print $2}'`

    if [ "$isVxvm" = "YES" ]; then

        $ECHO "Performing VXVM related actions.."

        if [ -f /sbin/vxdctl ]; then
            VXDCTL=/sbin/vxdctl
        elif [ -f /usr/sbin/vxdctl ]; then
            VXDCTL=/usr/sbin/vxdctl
        else
            log "Error: vxdctl is not found. Vxvm state could not be verified."
            $ECHO "vxdctl is not found. Vxvm state could not be verified."
            exit 1
        fi

        $VXDCTL mode | grep "mode: enabled" > /dev/null
        if [ $? -ne 0 ]; then
            $ECHO "Error: vxconfigd is not enabled. Vxvm operations cannot be performed."
            log  "Error: vxconfigd is not enabled. Vxvm operations cannot be performed."
            exit 1
        fi

        isVxVMCluster=NO
        $VXDCTL -c mode | grep "cluster active" > /dev/null
        if [ $? -eq 0 ]; then
            isVxVMCluster=YES
            $ECHO "Vxvm cluster is active."
            log  "Vxvm cluster is active."

            $VXDCTL -c mode | grep -w MASTER > /dev/null
            if [ $? -eq 0 ]; then
                isVxVMMaster=YES
            fi

        else
            $ECHO "Vxvm cluster is not active."
            log  "Vxvm cluster is not active."
        fi

        isVxfs=`grep "isVxfs=" $TGT_CONFIG_FILE |uniq | awk -F"=" '{print $2}'`
	
        if [ "$isCluster" = "YES" -o "$OSTYPE" = "AIX" ]; then
        if [ "$isVxfs" = "YES" ]; then

            for volMountPoint in $MOUNTPOINTS
            do
                DiskPath=`echo $volMountPoint |awk -F":" '{ print $2 }'`
                DiskGroup=`echo $DiskPath | awk -F"/" '{ print $5 }'`

                if [ -f /sbin/vxdg ]; then
                    VXDG=/sbin/vxdg
                elif [ -f /usr/sbin/vxdg ]; then
                    VXDG=/usr/sbin/vxdg
                else
                    log "Error: vxdg is not found. Vxvm diskgroup could not be verified."
                    $ECHO "vxdg is not found. Vxvm diskgroup could not be verified."
                    exit 1
                fi

                $VXDG list | grep -w $DiskGroup > $NULL 
                if [ $? -eq 0 ]
                then 
                    $ECHO "Found Diskgroup $DiskGroup."
                else
                    $ECHO "Diskgroup $DiskGroup is not present."
                    continue
                fi

                if [ "$isVxVMCluster" = "YES" ]; then
                    if [ "$isVxVMMaster" = "YES" ]; then
                        $VXDG destroy $DiskGroup
                        if [ $? -ne 0 ]
                        then
                            $ECHO "vxdg destroy $DiskGroup failed."
                            exit 1
                        fi
                    else
                        $ECHO "Volumes on $DiskGroup should be stopped from master."
                        status=1
                    fi
                else
                    $VXDG destroy $DiskGroup
                    if [ $? -ne 0 ]
                    then
                        $ECHO "vxdg destroy $DiskGroup failed."
                        exit 1
                    fi
                fi
            done
        fi
        fi

        isVxvmRaw=`grep "isVxvmRaw=" $TGT_CONFIG_FILE |uniq | awk -F"=" '{print $2}'`

        if [ "$isVxvmRaw" = "YES" ]; then

            vxvmDgName=`grep "VxvmDgName=" $TGT_CONFIG_FILE|awk -F"=" '{print $2}'`
            for DiskGroup in $vxvmDgName
            do

                if [ -f /sbin/vxdg ]; then
                    VXDG=/sbin/vxdg
                elif [ -f /usr/sbin/vxdg ]; then
                    VXDG=/usr/sbin/vxdg
                else
                    log "Error: vxdg is not found. Vxvm diskgroup could not be verified."
                    $ECHO "vxdg is not found. Vxvm diskgroup could not be verified."
                    exit 1
                fi

                $VXDG list | grep -w $DiskGroup > $NULL 
                if [ $? -eq 0 ]
                then 
                    $ECHO "Found Diskgroup $DiskGroup."
                else
                    $ECHO "Diskgroup $DiskGroup is not present."
                    continue
                fi

                if [ "$isVxVMCluster" = "YES" ]; then
                    if [ "$isVxVMMaster" = "YES" ]; then
                        $VXDG destroy $DiskGroup
                        if [ $? -ne 0 ]
                        then
                            $ECHO "Vxdg destroy $DiskGroup failed."
                            exit 1
                        fi
                    else
                        $ECHO "Volumes on $DiskGroup should be stopped from master."
                        status=1
                    fi
                else
                    $VXDG destroy $DiskGroup
                    if [ $? -ne 0 ]
                    then
                        $ECHO "Vxdg destroy $DiskGroup failed."
                        exit 1
                    fi
                fi
            done
        fi

        $ECHO "Completed the vxvm specific actions."
    fi
    done
    
    if [ $status -eq 1 ]; then
        exit 1
    fi

    log "EXIT TakeDatabaseOffline"
}

CheckAsmOnline()
{
    log "ENTER CheckAsmOnline..."

    ps -ef | grep -v grep | grep asm_pmon > $NULL

    if [ $? -ne 0 ]; then
        log "Error: ASM instance is not online"
        $ECHO "ASM instance is not online"
        exit 1
    fi

    log "EXIT CheckAsmOnline"
    return
}

CheckAsmDiskNames()
{
   log "ENTER CheckAsmDiskNames..."

   tempfile1=/tmp/asmdisknames1.$$
   asmdisknameflag=0

   if [ ! -f $INSTALLPATH/etc/OracleTgtVolInfoFile.dat ]; then
       log "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
       $ECHO "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
       exit 1
   fi

   su - $TARGET_ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile1
                       select name from v\\\$asm_disk; 
_EOF"

   tempfile2=/tmp/asmdisknames2.$$
   grep -v "VolumeType=LogicalVolume" $INSTALLPATH/etc/OracleTgtVolInfoFile.dat | awk -F: '{print $1}'  | awk -F= '{print $2}' > $tempfile2

   diskNames=`grep "ASMDiskNames=" $SRC_CONFIG_FILE | awk -F"=" '{ print $2 }' | sed 's/,/ /g'`

   for diskName in $diskNames
   do
       grep -w $diskName $tempfile1 >/dev/null
       if [ $? -eq 0 ]
       then
           log "Source ASM disk name $diskName found on target"

           tempfile3=/tmp/asmdisknames3.$$
           su - $TARGET_ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile3
                               select path from v\\\$asm_disk where name=$diskName; 
_EOF"
           diskPath=`cat $tempfile3 | grep -v PATH | grep -v "\-\-" | grep -v "^$" | grep -v "rows selected"`
           grep -w $diskPath $tempfile2 > /dev/null
           if [ $? -ne 0 ] ; then
               log "Error: ASM Disk name $diskName already present on target server disk $diskPath which is not a replication target disk."
               $ECHO "Error: ASM Disk name $diskName already present on target server disk $diskPath which is not a replication target disk."
               asmdisknameflag=1
           fi
       fi
   done
             
   if [ $asmdisknameflag -eq 1 ]
   then
        log "`cat $tempfile1 $tempfile2 $tempfile3`"
        rm -f $tempfile1 $tempfile2 $tempfile3
        exit 1
   fi
                                             
   rm -f $tempfile1 $tempfile2 $tempfile3
   log "EXIT CheckAsmDiskNames"

   return
}

CheckAsmLibInstall()
{
    log "ENTER CheckAsmLibInstall..."

    if [ -f /etc/init.d/oracleasm ]; then
        /etc/init.d/oracleasm status | grep no > $NULL
        if [ $? -eq 0 ]; then
            log "Error: ASMLib is not online"
            exit 1
        fi
    else
        log "Error: ASMLib is not found"
        exit 1
    fi

    log "EXIT CheckAsmLibInstall"
    return
}

CheckASMLibLabels()
{
    log "ENTER CheckASMLibLabels..."

    tempfile1=/tmp/asmliblabel1.$$
    asmlabflag=0

    if [ ! -f $INSTALLPATH/etc/OracleTgtVolInfoFile.dat ]; then
        log "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
        $ECHO "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
        exit 1
    fi

    tempfile2=/tmp/asmliblabel2.$$
    grep -v "VolumeType=LogicalVolume" $INSTALLPATH/etc/OracleTgtVolInfoFile.dat | awk -F: '{print $1}'  | awk -F= '{print $2}' > $tempfile2

    Labels=`grep "ASMLibDiskLabels" $SRC_CONFIG_FILE | awk -F"=" '{ print $2 }' | sed 's/,/ /g'`
    /etc/init.d/oracleasm listdisks>$tempfile1

    for label in $Labels
    do
        grep -w $label $tempfile1 >/dev/null
        if [ $? -eq 0 ]
        then
            dev=/dev/oracleasm/disks/$label
            maj=`ls -l $dev | awk '{print $5}' | sed 's/,//'| sed 's/ //g'`
            min=`ls -l $dev | awk '{print $6}' | sed 's/,//'| sed 's/ //g'`

            majHex=`echo "ibase=10;obase=16;$maj" | bc`
            minHex=`echo "ibase=10;obase=16;$min" | bc`

            flag=0
            while read line
            do
                if [ $flag -eq 0 ]; then
                    if [ "$line" = "Block devices:" ]; then
                        flag=1
                    fi
                else
                    tMaj=`echo $line | awk '{print $1}' | sed 's/ //g'`
                    if [ $maj -eq $tMaj ]; then
                        log "Found $maj in $line"
                        deviceType=`echo $line | awk '{print $2}' | sed 's/ //g'`
                        break
                    fi
                fi
            done < /proc/devices

            blockDevice=

            # Linux sd
            if [ "$deviceType" = "sd" ]; then
                for device in `ls -l /dev | grep "^b" | awk '{print "/dev/"$NF}'`
                do
                    stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                    if [ $? -eq 0 ]; then
                        blockDevice=$device
                        log "sd Device Bound to device $dev = $blockDevice"
                        break
                    fi
                done
            # VXVM
            elif [ "$deviceType" = "VxDMP" ]; then 
                for device in `ls -l /dev/vx/dmp/ | grep "^b" | awk '{print "/dev/vx/dmp/"$NF}'`
                do
                    stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                    if [ $? -eq 0 ]; then
                        blockDevice=$device
                        log "VxDMP Device Bound to device $dev = $blockDevice"
                        break
                    fi
                done
            # Linux DMP
            elif [ "$deviceType" = "device-mapper" ]; then
                for device in `ls -lL /dev/mapper/ | grep "^b" | awk '{print "/dev/mapper/"$NF}'`
                do
                    if [ -L "$device" ]; then
                        stat_device=`ls -l $device | awk -F/ '{print "/dev/"$NF}'`
                    else
                        stat_device=$device
                    fi

                    stat -c "%t,%T" $stat_device | grep -i "$majHex,$minHex" > /dev/null
                    if [ $? -eq 0 ]; then
                        blockDevice=$device
                        log "Linux DMP Device Bound to device $dev = $blockDevice"
                        break
                    fi
                done
            fi

            if [ ! -z "$blockDevice" ]; then
                grep -w $blockDevice $tempfile2 > /dev/null
                if [ $? -ne 0 ] ; then
                    log "Error: ASM lable $label already present on target disk $blockDevice"
                    $ECHO "Error: ASM lable $label already present on target disk $blockDevice"
                    asmlabflag=1
                fi
            fi
        fi
    done

    rm -f $tempfile1

    if [ $asmlabflag -eq 1 ]
    then
        exit 1
    fi
  
    log "EXIT CheckASMLibLabels..."

    return
}

CheckAsmRawAliases()
{
    log "ENTER CheckAsmRawAliases.."

    asmaliasflag=0

    if [ ! -f $INSTALLPATH/etc/OracleTgtVolInfoFile.dat ]; then
        log "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
        $ECHO "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
        exit 1
    fi

    tempfile2=/tmp/asmrawalias2.$$
    grep -v "VolumeType=LogicalVolume" $INSTALLPATH/etc/OracleTgtVolInfoFile.dat | awk -F: '{print $1}'  | awk -F= '{print $2}' > $tempfile2

    diskAliases=`grep "DiskAlias=" $SRC_CONFIG_FILE | awk -F"=" '{ print $2 }'`

    for diskalias in $diskAliases
    do
       srcDisk=`echo $diskalias | awk -F: '{ print $1 }'`
       alias=`echo $diskalias | awk -F: '{ print $2 }'`

       if [ ! -e "$alias" ]; then
           log "Disk alias $alias doesn't exist on the target server. On the source server this is mapped to $srcDisk."
           $ECHO "Disk alias $alias doesn't exist on the target server. On the source server this is mapped to $srcDisk. Please create a similar mapping on target server."
           asmaliasflag=1
           continue
       fi

       if [ "$OSTYPE" = "Linux" ]; then
           tgtDisk=`ls -lL $alias 2>/dev/null | awk '{print $NF}'`
           if [ -b "$tgtDisk" ]; then
               maj=`ls -lL $tgtDisk | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
               min=`ls -lL $tgtDisk | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`

               majHex=`echo "ibase=10;obase=16;$maj" | bc`
               minHex=`echo "ibase=10;obase=16;$min" | bc`

               flag=0
               while read line
               do
                  if [ $flag -eq 0 ]; then
                      if [ "$line" = "Block devices:" ]; then
                          flag=1
                      fi
                  else
                      tMaj=`echo $line | awk '{print $1}' | sed 's/ //g'`
                      if [ $maj -eq $tMaj ]; then
                          log "Found $maj in $line"
                          deviceType=`echo $line | awk '{print $2}' | sed 's/ //g'`
                          break
                      fi
                  fi
               done < /proc/devices

               # Linux sd
               if [ "$deviceType" = "sd" ]; then
                   for device in `ls -l /dev | grep "^b" | awk '{print "/dev/"$NF}'`
                   do
                      stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                      if [ $? -eq 0 ]; then
                          blockDevice=$device
                          log "sd Device Bound to device $dev = $blockDevice"
                          break
                      fi
                   done
                                                                                                                                                                                       # VXVM
               elif [ "$deviceType" = "VxDMP" ]; then 
                    for device in `ls -l /dev/vx/dmp/ | grep "^b" | awk '{print "/dev/vx/dmp/"$NF}'`
                    do
                       stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                       if [ $? -eq 0 ]; then
                           blockDevice=$device
                           log "VxDMP Device Bound to device $dev = $blockDevice"
                           break
                       fi
                    done

               # Linux DMP
               elif [ "$deviceType" = "device-mapper" ]; then
                    for device in `ls -lL /dev/mapper/ | grep "^b" | awk '{print "/dev/mapper/"$NF}'`
                    do
                       if [ -L "$device" ]; then
                           stat_device=`ls -l $device | awk -F/ '{print "/dev/"$NF}'`
                       else
                           stat_device=$device
                       fi

                       stat -c "%t,%T" $stat_device | grep -i "$majHex,$minHex" > /dev/null
                                                                                                                                                                                               if [ $? -eq 0 ]; then
                           blockDevice=$device
                           log "Linux DMP Device Bound to device $dev = $blockDevice"
                           break
                       fi
                    done
               fi

               tgtDisk=$blockDevice     
           fi

       elif [ "$OSTYPE" = "SunOS" ]; then
             maj=`ls -lL $alias | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
             min=`ls -lL $alias | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`

             deviceType=`grep -w $maj /etc/name_to_major | awk '{print $1}' | sed 's/ //g'`
             blockDevice=

             # ssd or sd
             if [ "$deviceType" = "ssd" -o "$deviceType" = "sd" ]; then
                 dirToSearch=/dev/dsk/*
             elif [ "$deviceType" = "vxdmp" ]; then 
                 dirToSearch=/dev/vx/dmp/*
             else
                 log "Error: unsupported device type $deviceType"
                 dirToSearch=/unsupporteddir
             fi  

             for device in `ls -lL $dirToSearch 2>/dev/null | grep "^b" | awk '{print $NF}'`
             do
                 tmaj=`ls -lL $device | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
                 tmin=`ls -lL $device | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`
                 if [ $tmaj -eq $maj -a $tmin -eq $min ]; then
                     blockDevice=$device
                     log "Device Bound to device $device = $blockDevice"
                     break
                 fi
             done

             tgtDisk=$blockDevice     

       elif [ "$OSTYPE" = "AIX" ]; then
             maj=`ls -lL $alias | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
             min=`ls -lL $alias | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`

             blockDevice=

             dirToSearch=/dev/hdisk*

             for device in `ls -lL $dirToSearch 2>/dev/null | grep "^b" | awk '{print $NF}'`
             do
                tmaj=`ls -lL $device | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
                tmin=`ls -lL $device | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`
                if [ $tmaj -eq $maj -a $tmin -eq $min ]; then
                    blockDevice=$device
                    log "Device Bound to device $device = $blockDevice"
                    break
                fi
             done

             tgtDisk=$blockDevice     
       fi

       if [ ! -e "$tgtDisk" ]; then
           log "A disk target alias $alias doesn't exist on the target server. On the source server this is mapped to $srcDisk."
           $ECHO "A disk target alias $alias doesn't exist on the target server. On the source server this is mapped to $srcDisk. Please create a similar mapping on target server."
           asmaliasflag=1
           continue
       fi

       if [ ! -b $tgtDisk ]; then
           log "A disk target $tgtDisk for alias $alias is not a block device."
           $ECHO "A disk target $tgtDisk for alias $alias is not a block device."
           asmaliasflag=1
           continue
       fi

       maj=`ls -lL $tgtDisk | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
       min=`ls -lL $tgtDisk | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`

       blockDevice=

       if [ "$OSTYPE" = "Linux" ]; then
           majHex=`echo "ibase=10;obase=16;$maj" | bc`
           minHex=`echo "ibase=10;obase=16;$min" | bc`

           flag=0
           while read line
           do
              if [ $flag -eq 0 ]; then
                  if [ "$line" = "Block devices:" ]; then
                      flag=1
                  fi
              else
                  tMaj=`echo $line | awk '{print $1}' | sed 's/ //g'`
                  if [ $maj -eq $tMaj ]; then
                      log "Found $maj in $line"
                      deviceType=`echo $line | awk '{print $2}' | sed 's/ //g'`
                      break
                  fi
              fi
           done < /proc/devices

           # Linux sd
           if [ "$deviceType" = "sd" ]; then
               for device in `ls -l /dev | grep "^b" | awk '{print "/dev/"$NF}'`
               do
                  stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                  if [ $? -eq 0 ]; then
                      blockDevice=$device
                      log "sd Device Bound to device $dev = $blockDevice"
                      break
                  fi
               done
           # VXVM
           elif [ "$deviceType" = "VxDMP" ]; then 
                 for device in `ls -l /dev/vx/dmp/ | grep "^b" | awk '{print "/dev/vx/dmp/"$NF}'`
                 do
                    stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                    if [ $? -eq 0 ]; then
                        blockDevice=$device
                        log "VxDMP Device Bound to device $dev = $blockDevice"
                        break
                    fi
                 done
           # Linux DMP
           elif [ "$deviceType" = "device-mapper" ]; then
                 for device in `ls -lL /dev/mapper/ | grep "^b" | awk '{print "/dev/mapper/"$NF}'`
                 do
                    if [ -L "$device" ]; then
                        stat_device=`ls -l $device | awk -F/ '{print "/dev/"$NF}'`
                    else
                        stat_device=$device
                    fi

                    stat -c "%t,%T" $stat_device | grep -i "$majHex,$minHex" > /dev/null
                    if [ $? -eq 0 ]; then
                        blockDevice=$device
                        log "Linux DMP Device Bound to device $dev = $blockDevice"
                        break
                    fi
                 done
           fi

       elif [ "$OSTYPE" = "SunOS" ]; then
             deviceType=`grep -w $maj /etc/name_to_major | awk '{print $1}' | sed 's/ //g'`
             # ssd or sd
             if [ "$deviceType" = "ssd" -o "$deviceType" = "sd" ]; then
                 dirToSearch=/dev/dsk/*
             elif [ "$deviceType" = "vxdmp" ]; then 
                 dirToSearch=/dev/vx/dmp/*
             else
                 log "Error: unsupported device type $deviceType"
                 asmaliasflag=1
                 continue
             fi  

             for device in `ls -lL $dirToSearch 2>/dev/null | grep "^b" | awk '{print $NF}'`
             do
                tmaj=`ls -lL $device | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
                tmin=`ls -lL $device | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`
                if [ $tmaj -eq $maj -a $tmin -eq $min ]; then
                    blockDevice=$device
                    log "Device Bound to device $device = $blockDevice"
                    break
                fi
             done

       elif [ "$OSTYPE" = "AIX" ]; then
             dirToSearch=/dev/hdisk*
             for device in `ls -lL $dirToSearch 2>/dev/null | grep "^b" | awk '{print $NF}'`
             do
                tmaj=`ls -lL $device | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
                tmin=`ls -lL $device | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`
                if [ $tmaj -eq $maj -a $tmin -eq $min ]; then
                    blockDevice=$device
                    log "Device Bound to device $device = $blockDevice"
                    break
                fi
             done
       fi

       if [ ! -z "$blockDevice" ]; then
           grep -w $blockDevice $tempfile2 > /dev/null
           if [ $? -ne 0 ] ; then
               log "Error: Disk alias $dev already present on target server disk $blockDevice which is not a replication target disk."
               $ECHO "Error: Disk alias $dev already present on target server disk $blockDevice which is not a replication target disk."
               asmaliasflag=1
           fi
       fi
    done

    rm -f $tempfile2

    if [ $asmaliasflag -eq 1 ]
    then
        exit 1
    fi
                                                                                                                                 
    log "EXIT CheckAsmRawAliases"

    return
}

checkASMDGOnline()
{
    log "ENTER checkASMDGOnline..."

    if [ ! -f $INSTALLPATH/etc/OracleTgtVolInfoFile.dat ]; then
        log "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
        $ECHO "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
        exit 1
    fi

    tempfile1=/tmp/asmdgonline1.$$
    grep -v "VolumeType=LogicalVolume" $INSTALLPATH/etc/OracleTgtVolInfoFile.dat | awk -F: '{print $1}'  | awk -F= '{print $2}' > $tempfile1

    if [ "${OSTYPE}" = "SunOS" ];then
        ORATAB="/var/opt/oracle/oratab"
    else
        ORATAB="/etc/oratab"
    fi

    if [ ! -f $ORATAB ]; then
        log "Error: File $ORATAB not found"
        exit 0
    fi
 
    tempfile2=/tmp/asmdgonline2.$$
    tempfile3=/tmp/asmdgonline3.$$
    asmdgflag=0
    Diskgroups=`grep "DiskGroupName" $SRC_CONFIG_FILE | awk -F"=" '{ print $2 }' | sed 's/,/ /g'`
    ORACLE_SID=`ps -ef | grep asm_pmon |  grep "+ASM" | grep -v grep | awk '{ print $NF}' |  sed -e 's/asm\_pmon\_//'`
    ORACLE_HOME=`cat $ORATAB | grep -v "^#" | grep -v "^$" | grep -v "^*:" | grep "+ASM" | awk -F":" '{print $2}'`
    su - $TARGET_ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile2
                        select name from v\\\$asm_diskgroup; 
_EOF"
	    
    for diskgroup in $Diskgroups
    do
        grep -w $diskgroup $tempfile2 >>/dev/null
        if [ $? -ne 0 ]; then
            log "Source ASM diskgroup $diskgroup not present on target"
        else

            su - $TARGET_ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF > $tempfile3
                                    select group_number from v\\\$asm_diskgroup where name='$diskgroup';
_EOF"
            groupNumber=`cat $tempfile3 | grep -v GROUP_NUMBER | grep -v "\-\-" | grep -v "^$" | grep -v "rows selected"`

            if [ -z "$groupNumber" ]; then
                log "Error: ASM diskgroup $diskgroup doesnt have a group number"
                continue
            fi

            log "Souce ASM diskgroup $diskgroup found on target."

            su - $TARGET_ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile3
                                select path from v\\\$asm_disk where group_number=$groupNumber;
_EOF"

            for asmdiskpath in `cat $tempfile3 | grep -v PATH | grep -v "\-\-" | grep -v "^$" | grep -v "rows selected"`
            do
                blockDevice=
                case $asmdiskpath in
                */dev/raw/raw*)
                    maj=`raw -q $asmdiskpath | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
                    min=`raw -q $asmdiskpath | awk '{print $7}' | sed 's/,//' | sed 's/ //g'`

                    majHex=`echo "ibase=10;obase=16;$maj" | bc`
                    minHex=`echo "ibase=10;obase=16;$min" | bc`

                    flag=0
                    while read line
                    do
                        if [ $flag -eq 0 ]; then
                            if [ "$line" = "Block devices:" ]; then
                                flag=1
                            fi
                        else
                            tMaj=`echo $line | awk '{print $1}' | sed 's/ //g'`
                            if [ $maj -eq $tMaj ]; then
                                log "Found $maj in $line"
                                deviceType=`echo $line | awk '{print $2}' | sed 's/ //g'`
                                break
                            fi
                        fi
                    done < /proc/devices

                    # Linux sd
                    if [ "$deviceType" = "sd" ]; then
                        for device in `ls -l /dev | grep "^b" | awk '{print "/dev/"$NF}'`
                        do
                            stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                            if [ $? -eq 0 ]; then
                                blockDevice=$device
                                log "sd Device Bound to device $dev = $blockDevice"
                                break
                            fi
                        done
                    # VXVM
                    elif [ "$deviceType" = "VxDMP" ]; then 
                        for device in `ls -l /dev/vx/dmp/ | grep "^b" | awk '{print "/dev/vx/dmp/"$NF}'`
                        do
                            stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                            if [ $? -eq 0 ]; then
                                blockDevice=$device
                                log "VxDMP Device Bound to device $dev = $blockDevice"
                                break
                            fi
                        done
                    # Linux DMP
                    elif [ "$deviceType" = "device-mapper" ]; then
                        for device in `ls -lL /dev/mapper/ | grep "^b" | awk '{print "/dev/mapper/"$NF}'`
                        do
                           if [ -L "$device" ]; then
                               stat_device=`ls -l $device | awk -F/ '{print "/dev/"$NF}'`
                           else
                               stat_device=$device
                           fi

                           stat -c "%t,%T" $stat_device | grep -i "$majHex,$minHex" > /dev/null
                           if [ $? -eq 0 ]; then
                               blockDevice=$device
                               log "Linux DMP Device Bound to device $dev = $blockDevice"
                               break
                           fi
                        done
                    fi

                    ;;
                */dev/rdsk/*)
                    blockDevice=`echo $asmdiskpath | sed -e 's/rdsk/dsk/g'`
                    ;;
                */dev/did/rdsk/*)
                    blockDevice=`echo $asmdiskpath | sed -e 's/rdsk/dsk/g'`
                    ;;
                *ORCL*)
                    asmlable=`echo $asmdiskpath | cut -d: -f2`
                    asmlibdev=/dev/oracleasm/disks/$asmlable

                    maj=`ls -l $asmlibdev | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
                    min=`ls -l $asmlibdev | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`

                    majHex=`echo "ibase=10;obase=16;$maj" | bc`
                    minHex=`echo "ibase=10;obase=16;$min" | bc`

                    flag=0
                    while read line
                    do
                        if [ $flag -eq 0 ]; then
                            if [ "$line" = "Block devices:" ]; then
                                flag=1
                            fi
                        else
                            tMaj=`echo $line | awk '{print $1}' | sed 's/ //g'`
                            if [ $maj -eq $tMaj ]; then
                                log "Found $maj in $line"
                                deviceType=`echo $line | awk '{print $2}' | sed 's/ //g'`
                                break
                            fi
                        fi
                    done < /proc/devices

                    # Linux sd
                    if [ "$deviceType" = "sd" ]; then
                        for device in `ls -l /dev | grep "^b" | awk '{print "/dev/"$NF}'`
                        do
                            stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                            if [ $? -eq 0 ]; then
                                blockDevice=$device
                                log "sd Device Bound to device $dev = $blockDevice"
                                break
                            fi
                        done
                    # VXVM
                    elif [ "$deviceType" = "VxDMP" ]; then 
                        for device in `ls -l /dev/vx/dmp/ | grep "^b" | awk '{print "/dev/vx/dmp/"$NF}'`
                        do
                            stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                            if [ $? -eq 0 ]; then
                                blockDevice=$device
                                log "VxDMP Device Bound to device $dev = $blockDevice"
                                break
                            fi
                        done
                    # Linux DMP
                    elif [ "$deviceType" = "device-mapper" ]; then
                        for device in `ls -lL /dev/mapper/ | grep "^b" | awk '{print "/dev/mapper/"$NF}'`
                        do
                            if [ -L "$device" ]; then
                                stat_device=`ls -l $device | awk -F/ '{print "/dev/"$NF}'`
                            else
                                stat_device=$device
                            fi

                            stat -c "%t,%T" $stat_device | grep -i "$majHex,$minHex" > /dev/null
                            if [ $? -eq 0 ]; then
                                blockDevice=$device
                                log "Linux DMP Device Bound to device $dev = $blockDevice"
                                break
                            fi
                        done
                    fi
                    ;;
                */dev/oracleasm/disks/*)

                    asmlibdev=$asmdiskpath

                    maj=`ls -l $asmlibdev | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
                    min=`ls -l $asmlibdev | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`

                    majHex=`echo "ibase=10;obase=16;$maj" | bc`
                    minHex=`echo "ibase=10;obase=16;$min" | bc`

                    flag=0
                    while read line
                    do
                        if [ $flag -eq 0 ]; then
                            if [ "$line" = "Block devices:" ]; then
                                flag=1
                            fi
                        else
                            tMaj=`echo $line | awk '{print $1}' | sed 's/ //g'`
                            if [ $maj -eq $tMaj ]; then
                                log "Found $maj in $line"
                                deviceType=`echo $line | awk '{print $2}' | sed 's/ //g'`
                                break
                            fi
                        fi
                    done < /proc/devices

                    # Linux sd
                    if [ "$deviceType" = "sd" ]; then
                        for device in `ls -l /dev | grep "^b" | awk '{print "/dev/"$NF}'`
                        do
                            stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                            if [ $? -eq 0 ]; then
                                blockDevice=$device
                                log "sd Device Bound to device $dev = $blockDevice"
                                break
                            fi
                        done
                    # VXVM
                    elif [ "$deviceType" = "VxDMP" ]; then 
                        for device in `ls -l /dev/vx/dmp/ | grep "^b" | awk '{print "/dev/vx/dmp/"$NF}'`
                        do
                            stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                            if [ $? -eq 0 ]; then
                                blockDevice=$device
                                log "VxDMP Device Bound to device $dev = $blockDevice"
                                break
                            fi
                        done
                    # Linux DMP
                    elif [ "$deviceType" = "device-mapper" ]; then
                        for device in `ls -lL /dev/mapper/ | grep "^b" | awk '{print "/dev/mapper/"$NF}'`
                        do
                            if [ -L "$device" ]; then
                                stat_device=`ls -l $device | awk -F/ '{print "/dev/"$NF}'`
                            else
                                stat_device=$device
                            fi
                            stat -c "%t,%T" $stat_device | grep -i "$majHex,$minHex" > /dev/null
                            if [ $? -eq 0 ]; then
                                blockDevice=$device
                                log "Linux DMP Device Bound to device $dev = $blockDevice"
                                break
                            fi
                        done
                    fi
                    ;;
                */dev/vx/rdsk/*)
                    blockDevice=`echo $asmdiskpath | sed -e 's/rdsk/dsk/g'`
                    ;;
                */dev/rhdisk*)
                    blockDevice=`echo $asmdiskpath | sed -e 's/rhdisk/hdisk/g'`
                    ;;
                */dev/vx/dsk/*)
                    blockDevice=$asmdiskpath
                    ;;
                */dev/*)
                    if [ ! -h $asmdiskpath ] ; then
                        blockDevice=`ls -lL $asmdiskpath | awk '{ print $NF }'`
                    else
                        asmrawdev=$asmdiskpath

                        maj=`ls -lL $asmrawdev | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
                        min=`ls -lL $asmrawdev | awk '{print $6}' | sed 's/,//' | sed 's/ //g'`

                        majHex=`echo "ibase=10;obase=16;$maj" | bc`
                        minHex=`echo "ibase=10;obase=16;$min" | bc`

                        flag=0
                        while read line
                        do
                           if [ $flag -eq 0 ]; then
                               if [ "$line" = "Block devices:" ]; then
                                   flag=1
                               fi
                           else
                               tMaj=`echo $line | awk '{print $1}' | sed 's/ //g'`
                               if [ $maj -eq $tMaj ]; then
                                   log "Found $maj in $line"
                                   deviceType=`echo $line | awk '{print $2}' | sed 's/ //g'`
                                   break
                               fi
                           fi
                        done < /proc/devices

                        # Linux sd
                        if [ "$deviceType" = "sd" ]; then
                            for device in `ls -l /dev | grep "^b" | awk '{print "/dev/"$NF}'`
                            do
                                stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                                if [ $? -eq 0 ]; then
                                    blockDevice=$device
                                    log "sd Device Bound to device $dev = $blockDevice"
                                    break
                                fi
                            done
                        # VXVM
                        elif [ "$deviceType" = "VxDMP" ]; then 
                              for device in `ls -l /dev/vx/dmp/ | grep "^b" | awk '{print "/dev/vx/dmp/"$NF}'`
                              do
                                  stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
                                  if [ $? -eq 0 ]; then
                                       blockDevice=$device
                                       log "VxDMP Device Bound to device $dev = $blockDevice"
                                       break;
                                  fi
                              done
                        # Linux DMP
                        elif [ "$deviceType" = "device-mapper" ]; then
                              for device in `ls -lL /dev/mapper/ | grep "^b" | awk '{print "/dev/mapper/"$NF}'`
                              do
                                 if [ -L "$device" ]; then
                                     stat_device=`ls -l $device | awk -F/ '{print "/dev/"$NF}'`
                                 else
                                     stat_device=$device
                                 fi

                                 stat -c "%t,%T" $stat_device | grep -i "$majHex,$minHex" > /dev/null
                                                                                                                                                                                                         if [ $? -eq 0 ]; then
                                     blockDevice=$device
                                     log "Linux DMP Device Bound to device $dev = $blockDevice"
                                     break;
                                 fi
                              done
                        fi
                    fi
                    ;;
                *)
                    log "Error: Unknown device type $asmdiskpath."
                    $ECHO "Erro: Unknown device type $asmdiskpath."
                    asmdgflag=1
                    ;;
                esac

                if [ ! -z "$blockDevice" ]; then
                    grep -w $blockDevice $tempfile1 > /dev/null
                    if [ $? -eq 0 ] ; then
                        log "Source ASM diskgroup $diskgroup is on $blockDevice which is a target device."
                    else
                        log "Error: Source ASM diskgroup $diskgroup present on $blockDevice which is not a target device."
                        $ECHO "Error: Source ASM diskgroup $diskgroup present on $blockDevice which is not a target device."
                        log "The diskgroup need to be dropped manually."
                        $ECHO "The diskgroup need to be dropped manually."
                        asmdgflag=1
                    fi
                else
                    log "Error: A block device for $asmdiskpath in $diskgroup is not present on target server."
                    $ECHO "Error: A block device for $asmdiskpath in $diskgroup is not present on target server."
                    asmdgflag=1
                fi
            done
        fi
    done

    rm -f $tempfile1
    rm -f $tempfile2
    rm -f $tempfile3

    if [ $asmdgflag -eq 1 ]
    then
        exit 1
    fi
    log "EXIT checkASMDGOnline..."

    return
}


CheckVxvmRawDGOnline()
{
    log "ENTER CheckVxvmRawDGOnline..."

    tempfile1=/tmp/vxvmrawdgonline.$$
    flag=0

    if [ ! -f $INSTALLPATH/etc/OracleTgtVolInfoFile.dat ]; then
        log "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
        $ECHO "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
        exit 1
    fi

    tempfile2=/tmp/oratgtvol.$$
    grep "VolumeType=Disk" $INSTALLPATH/etc/OracleTgtVolInfoFile.dat | awk -F: '{print $1}'  | awk -F= '{print $2}' > $tempfile2

    Diskgroups=`grep "VxvmDgName" $SRC_CONFIG_FILE | awk -F"=" '{ print $2 }'`
    vxdisk -o alldgs list>$tempfile1
    for diskgroup in $Diskgroups
    do
        grep -w $diskgroup $tempfile1 >>/dev/null
        if  [ $? -eq 0 ]
        then
            for diskdev in `grep -w $diskgroup $tempfile1 | awk '{print $1}'`
            do
                disks=`vxdisk list $diskdev | grep state=enabled | awk '{print $1}'`

                diskFound=0
                for disk in $disks
                do
                    if [ "$OSTYPE" = "Linux" ]; then
                        diskPath=/dev/$disk
                    elif [ "$OSTYPE" = "SunOS" ]; then
                        diskPath=/dev/dsk/$disk
                    fi

                    grep "$diskPath" $tempfile2 > /dev/null
                    if [ $? -eq 0 ] ; then
                        diskFound=1
                    fi
                done

                if [ $diskFound -eq 0 ] ; then
                    log "Error: VXVM Diskgroup $diskgroup already present on $diskPath"
                    $ECHO "Error: VXVM Diskgroup $diskgroup already present on $diskPath"
                    flag=1
                fi
            done
        fi
    done

    rm -f $tempfile1
    rm -f $tempfile2

    log "EXIT CheckVxvmRawDGOnline..."

    if [ $flag -eq 1 ]
    then
        exit 1
    fi

    return
}

CheckVxvmDGOnline()
{
    log "ENTER CheckVxvmDGOnline.."

    tempfile1=/tmp/vxvmdgonline.$$
    flag=0

    if [ ! -f $INSTALLPATH/etc/OracleTgtVolInfoFile.dat ]; then
        log "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
        $ECHO "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
        exit 1
    fi

    tempfile2=/tmp/oratgtvol.$$
    grep "VolumeType=Disk" $INSTALLPATH/etc/OracleTgtVolInfoFile.dat | awk -F: '{print $1}'  | awk -F= '{print $2}' > $tempfile2

    Diskgroups=`grep 'MountPoint=vxfs' $SRC_CONFIG_FILE | awk -F":" '{ print $2 }' | awk -F/ '{print $5}'`
    vxdisk -o alldgs list>$tempfile1
    for diskgroup in $Diskgroups
    do
        grep -w $diskgroup $tempfile1 > /dev/null
        if [ $? -eq 0 ]
        then
            for diskdev in `grep -w $diskgroup $tempfile1 | awk '{print $1}'`
            do
                disks=`vxdisk list $diskdev | grep state=enabled | awk '{print $1}'`

                diskFound=0
                for disk in $disks
                do
                    if [ "$OSTYPE" = "Linux" ]; then
                        diskPath=/dev/$disk
                    elif [ "$OSTYPE" = "SunOS" ]; then
                        diskPath=/dev/dsk/$disk
                    fi

                    grep "$diskPath" $tempfile2 > /dev/null
                    if [ $? -eq 0 ] ; then
                        diskFound=1
                    fi
                done

                if [ $diskFound -eq 0 ] ; then
                    log "Error: VXVM Diskgroup $diskgroup already present on $diskPath"
                    $ECHO "Error: VXVM Diskgroup $diskgroup already present on $diskPath"
                    flag=1
                fi
            done
        fi
    done

    rm -f $tempfile1
    rm -f $tempfile2

    log "EXIT CheckVxvmDGOnline..."

    if [ $flag -eq 1 ]
    then
        exit 1
    fi

    return
}

GetVxfsVersion()
{
    if [ "$OSTYPE" = "Linux" ]; then
        /bin/rpm -q VRTSvxfs > /dev/null
        if [ $? -eq 0 ]; then
            vxfsVersion=`/bin/rpm -q VRTSvxfs | awk -F"-" '{print $2}' | awk -F"." '{print $1"."$2}'`
        else
            log "Error: VXFS package not found"
            $ECHO "Error: VXFS package not found"
            exit 1
        fi
    elif [ "$OSTYPE" = "SunOS" ]; then
        /usr/bin/pkginfo -l VRTSvxfs > /dev/null
        if [ $? -eq 0 ]; then
            vxfsVersion=`pkginfo -l VRTSvxfs | grep VERSION | awk -F: '{print $2}' | awk -F"," '{print $1}' | sed 's/ //g'`
        else
            log "Error: VXFS package not found"
            $ECHO "Error: VXFS package not found"
            exit 1
        fi
    elif [ "$OSTYPE" = "AIX" ]; then
        /usr/bin/lslpp -l VRTSvxfs > /dev/null
        if [ $? -eq 0 ]; then
            vxfsVersion=`/usr/bin/lslpp -l VRTSvxfs | grep VRTSvxfs | awk '{print $2}' | head -1 | sed 's/ //g'`
        else
            log "Error: VXFS package not found"
            $ECHO "Error: VXFS package not found"
            exit 1
        fi
    fi

    if [ -z "$vxfsVersion" ]; then
        log "Error: VXFS version not found"
        $ECHO "Error: VXFS version not found"
        exit 1
    fi

    WriteToDbConfFile "VxfsVersion=$vxfsVersion"

    return
}

GetVxvmVersion()
{
    if [ "$OSTYPE" = "Linux" ]; then
        /bin/rpm -q VRTSvxvm > /dev/null
        if [ $? -eq 0 ]; then
            vxvmVersion=`/bin/rpm -q VRTSvxvm | awk -F"-" '{print $2}' | awk -F"." '{print $1"."$2}'`
        else
            log "Error: VXVM package not found"
            $ECHO "Error: VXVM package not found"
            exit 1
        fi
    elif [ "$OSTYPE" = "SunOS" ]; then
        /usr/bin/pkginfo -l VRTSvxvm > /dev/null
        if [ $? -eq 0 ]; then
            vxvmVersion=`pkginfo -l VRTSvxvm | grep VERSION | awk -F: '{print $2}' | awk -F"," '{print $1}' | sed 's/ //g'`
        else
            log "Error: VXVM package not found"
            $ECHO "Error: VXVM package not found"
            exit 1
        fi
    elif [ "$OSTYPE" = "AIX" ]; then
        /usr/bin/lslpp -l VRTSvxvm > /dev/null
        if [ $? -eq 0 ]; then
            vxvmVersion=`/usr/bin/lslpp -l VRTSvxvm | grep VRTSvxvm | awk '{print $2}' | head -1 | sed 's/ //g'`
        else
            log "Error: VXVM package not found"
            $ECHO "Error: VXVM package not found"
            exit 1
        fi
    fi


    if [ -z "$vxvmVersion" ]; then
        log "Error: VXVM version not found"
        $ECHO "Error: VXVM version not found"
        exit 1
    fi

    WriteToDbConfFile "VxvmVersion=$vxvmVersion"

    return
}

CheckVxDiskStatus()
{
    log "ENTER CheckVxDiskStatus..."

    if [ ! -f $INSTALLPATH/etc/OracleTgtVolInfoFile.dat ]; then
        log "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
        $ECHO "Error: $INSTALLPATH/etc/OracleTgtVolInfoFile.dat not found "
        exit 1
    fi

    tempfile3=/tmp/vxdisksofdg

    grep "VolumeType=Disk" $INSTALLPATH/etc/OracleTgtVolInfoFile.dat|awk -F: '{print $1}'  | awk -F= '{print $2}' > $tempfile3
    for volume in `cat $tempfile3`
    do
        vol=`basename $volume`
        vxdisk list | grep -w "^$vol" | grep "udid_mismatch" > /dev/null
        if [ $? -eq 0 ];then
            log "Disk $vol has udid_mismatch. Please clear that flag"
            $ECHO "Disk $vol has udid_mismatch. Please clear that flag"
            exit 1
        fi

        status=`vxdisk list | grep -w "^$vol" | awk '{print $5 $6}'`
        if [ "$status" = "online" -o "$status" = "onlineshared" ];then
            continue
        elif [ "$status" = "error" -o "$status" = "onlineinvalid" ];then
              log "Disk $vol has errors or has invalid configuration "
              $ECHO "Disk $vol has errors or has invalid configuration "
              exit 1
        elif [ "$status" = "offline" ];then
              log "Disk $vol is not initialized "
              $ECHO "Disk $vol is not initialized "
              exit 1
        fi
    done

    log "EXIT CheckVxDiskStatus..."
}

CheckVxfsInstall()
{
    log "ENTER CheckVxfsInstall..."

    version=$1

    if [ "$OSTYPE" = "Linux" ]; then
        /bin/rpm -q VRTSvxfs > /dev/null
        if [ $? -eq 0 ]; then
            tgtVersion=`/bin/rpm -q VRTSvxfs | awk -F"-" '{print $2}' | awk -F"." '{print $1"."$2}'`
        else
            log "Error: VXFS package not found"
            $ECHO "Error: VXFS package not found"
            exit 1
        fi
    elif [ "$OSTYPE" = "SunOS" ]; then
        /usr/bin/pkginfo -l VRTSvxfs > /dev/null
        if [ $? -eq 0 ]; then
            tgtVersion=`pkginfo -l VRTSvxfs | grep VERSION | awk -F: '{print $2}' | awk -F"," '{print $1}' | sed 's/ //g'`
        else
            log "Error: VXFS package not found"
            $ECHO "Error: VXFS package not found"
            exit 1
        fi
    elif [ "$OSTYPE" = "AIX" ]; then
        /usr/bin/lslpp -l VRTSvxfs > /dev/null
        if [ $? -eq 0 ]; then
            tgtVersion=`/usr/bin/lslpp -l VRTSvxfs | grep VRTSvxfs | awk '{print $2}' | head -1 | sed 's/ //g'`
        else
            log "Error: VXFS package not found"
            $ECHO "Error: VXFS package not found"
            exit 1
        fi
    fi


    if [ -z "$tgtVersion" ]; then
        log "Error: VXFS version not found"
        $ECHO "Error: VXFS version not found"
        exit 1
    fi

    if [ "$version" = "$tgtVersion" ]; then
        $ECHO "Vxfs version $tgtVersion found."
    else
        log "Error: Vxfs version $tgtVersion is not same as version $version on source."
        $ECHO "Error: Vxfs version $tgtVersion is not same as version $version on source."
        exit 1
    fi

    CheckVxDiskStatus

    log "EXIT CheckVxfsInstall"

    return
}

CheckVxvmInstall()
{
    version=$1
    log "ENTER CheckVxvmInstall..."

    if [ "$OSTYPE" = "Linux" ]; then
        /bin/rpm -q VRTSvxvm > /dev/null
        if [ $? -eq 0 ]; then
            tgtVersion=`/bin/rpm -q VRTSvxvm | awk -F"-" '{print $2}' | awk -F"." '{print $1"."$2}'`
        else
            log "Error: VXVM package not found"
            $ECHO "Error: VXVM package not found"
            exit 1
        fi
    elif [ "$OSTYPE" = "SunOS" ]; then
        /usr/bin/pkginfo -l VRTSvxvm > /dev/null
        if [ $? -eq 0 ]; then
            tgtVersion=`pkginfo -l VRTSvxvm | grep VERSION | awk -F: '{print $2}' | awk -F"," '{print $1}' | sed 's/ //g'`
        else
            log "Error: VXVM package not found"
            $ECHO "Error: VXVM package not found"
            exit 1
        fi
    elif [ "$OSTYPE" = "AIX" ]; then
        /usr/bin/lslpp -l VRTSvxvm > /dev/null
        if [ $? -eq 0 ]; then
            tgtVersion=`/usr/bin/lslpp -l VRTSvxvm | grep VRTSvxvm | awk '{print $2}' | head -1 | sed 's/ //g'`
        else
            log "Error: VXVM package not found"
            $ECHO "Error: VXVM package not found"
            exit 1
        fi
    fi


    if [ -z "$tgtVersion" ]; then
        log "Error: VXVM version not found"
        $ECHO "Error: VXVM version not found"
        exit 1
    fi

    if [ "$version" = "$tgtVersion" ]; then
        $ECHO "Vxvm version $tgtVersion found."
    else
        log "Error: Vxvm version $tgtVersion is not same as version $version on source."
        $ECHO "Error: Vxvm version $tgtVersion is not same as version $version on source."
        exit 1
    fi

    log "EXIT CheckVxvmInstall"

    return
}

checkZfsFileSystem()
{
   log "ENTER checkZfsFileSystem..."
   FsType=$1

   if [ ! -f $SRC_CONFIG_FILE ]; then
       log "Error: src config file '$SRC_CONFIG_FILE' is not found"
       exit 1
   fi

   tempfile=/tmp/tgtmntptlist.$$
   SOURCE_MOUNTPOINTS=`grep MountPoint=$FsType $SRC_CONFIG_FILE| awk -F"=" '{print $2 }'` 

   for volMountpoint in $SOURCE_MOUNTPOINTS 
   do
       SourceZfs=`echo $volMountpoint | awk -F: '{print $2}'`
       SourceMountPoint=`echo $volMountpoint | awk -F: '{print $3}'`
       SourcePoolName=`echo $SourceZfs | awk -F/ '{print $1}'`

       echo "SourceZfs=$SourceZfs;SourceMountPoint=$SourceMountPoint;SourcePoolName=$SourcePoolName"

       TgtDevices=`cat $INSTALLPATH/etc/OracleTgtVolInfoFile.dat|grep "VolumeType=Disk"|awk -F: '{print $1}'|awk -F= '{print $2}'|awk -F/ '{print $NF}'`

       TgtZpools=`zpool list |grep -v NAME|awk '{print $1}'`

       for TgtPool in $TgtZpools
       do
          zpool status $TgtPool |grep 'c[0-9]'|awk '{print $1}' > /tmp/zpooldisks
          if [ "$SourcePoolName" = "$TgtPool" ];then
              for Disk in $TgtDevices
              do
                 grep $Disk /tmp/zpooldisks > /dev/null
                 if [ $? -ne 0 ];then
                     echo "ERROR: $TgtPool already exists on the Target Node on other than Target Disks"
                     echo "Destroy it"
                     log "ERROR: $TgtPool already exists on the Target Node on other than Target Disks"
                     exit 1
                 else
                     echo "WARNING: $TgtPool already exists on the Target Node on Target Disks"
                     log "WARNING: $TgtPool already exists on the Target Node on Target Disks"
                 fi
             done
         else
            for Disk in $TgtDevices
            do
                grep $Disk /tmp/zpooldisks > /dev/null
                if [ $? -eq 0 ];then
                     echo "ERROR: $Disk is part of already active zpool $TgtPool on target"
                     log "ERROR: $Disk is part of already active zpool $TgtPool on target"
                     exit 1
                fi
            done
         fi
      done
      
      TgtZfsFileSystems=`zfs list|grep -v NAME|awk '{print $1","$NF}'`
      for eachFs in $TgtZfsFileSystems
      do
         TargetZfs=`echo $eachFs|awk -F"," '{print $1}'`
         MountPoint=`echo $eachFs|awk -F"," '{print $2}'`

         if [ "$SourceZfs" = "$TargetZfs" ];then

             if [ "$MountPoint" = "$SourceMountPoint" ];then
                 echo "WARNING: $MountPoint is already exists on the Target Node on the File System same as of the Source Fs"
                 log "WARNING: $MountPoint is already exists on the Target Node on the File System same as of the Source Fs"
             else
                 echo "ERROR: $MountPoint is already exists on the Target Node on other File System not same as of the Source Fs"
                 log "ERROR: $MountPoint is already exists on the Target Node on other File System not same as of the Source Fs"
                 exit 1
             fi
         fi
      done
   done

   log "EXIT checkZfsFileSystem"

   return
}

checkMountPoint()
{
    log "ENTER checkMountPoint.."

    flag=0
    FsType=$1
    if [ ! -f $SRC_CONFIG_FILE ]; then
        log "Error: src config file '$SRC_CONFIG_FILE' is not found"
        exit 1
    fi

    tempfile=/tmp/tgtmntptlist.$$
    SOURCE_MOUNTPOINTS=`grep MountPoint=$FsType $SRC_CONFIG_FILE| awk -F":" '{print $NF }'` 

    if [ $OSTYPE = "Linux" ]; then
        mount |awk -F" " '{print $3 }'>$tempfile
    elif [ $OSTYPE = "SunOS" ]; then
        mount |awk -F" " '{print $1 }'>$tempfile
    elif [ $OSTYPE = "AIX" ] ; then
        mount | awk '{print $3}' > $tempfile
    fi	

    flag=0
    for mountpoint in $SOURCE_MOUNTPOINTS 
    do
        cat $tempfile|grep -w "$mountpoint" >>/dev/null
        if [ $? -eq 0 ]; then
            grep FileSystemType=$FsType $INSTALLPATH/etc/OracleTgtVolInfoFile.dat | grep "MountPoint=$mountpoint" > /dev/null
            if [ $? -ne 0 ]; then
                echo "MountPoint $mountpoint already present on node on device other than selected target device"
                log "MountPoint $mountpoint already present on node on device other than selected target device"
                flag=1 
            fi
        fi
    done

    rm -f $tempfile > $NULL    

    if [ $flag -ne 0 ];then
        exit 1
    fi

    log "EXIT checkMountPoint"

    return
}

checkVolumeGroups()
{
    log "ENTER checkVolumeGroups.."
    
    tempfile=/tmp/tgtvglist.$$
    tempfile1=/tmp/tgtvglistall.$$
    tempfile2=/tmp/tgtinactivevg.$$

    if [ $OSTYPE = "AIX" ] ; then
        lsvg -o > $tempfile
        lsvg > $tempfile1
        grep -v -f $tempfile $tempfile1 > $tempfile2
    else
        log "Error: checkVolumeGroups not supported on $OSTYPE"
        rm -f $tempfile > $NULL    
        return
    fi

    SOURCE_VGS=`grep "VolumeGroup=" $SRC_CONFIG_FILE | awk -F: '{print $2}'` 

    flag=0
    for volGroup in $SOURCE_VGS
    do
        grep -w "$volGroup" $tempfile > /dev/null
        if [ $? -eq 0 ]; then
            grep "DiskGroup=$volGroup" $INSTALLPATH/etc/OracleTgtVolInfoFile.dat > /dev/null
            if [ $? -ne 0 ]; then
                echo "VolumeGroup $volGroup already present on target node on device other than selected target device"
                log "VolumeGroup $volGroup already present on target node on device other than selected target device"
                flag=1 
            fi
        fi

        grep -w "$volGroup" $tempfile2 > /dev/null
        if [ $? -eq 0 ];then
            echo "An inactive volumeGroup $volGroup already present on target node."
            log "An inactive volumeGroup $volGroup already present on target node."
            flag=1 
        fi
    done

    rm -f $tempfile > $NULL    
    rm -f $tempfile1 > $NULL    
    rm -f $tempfile2 > $NULL  

    if [ $flag -ne 0 ];then
        exit 1
    fi

    log "EXIT checkVolumeGroups"

    return
}

checkLogicalVolumes()
{
    log "ENTER checkLogicalVolumes.."
    
    if [ $OSTYPE != "AIX" ] ; then
        log "Error: checkLogicalVolumes not supported on $OSTYPE"
        rm -f $tempfile > $NULL    
        return
    fi	

    SOURCE_LVS=`grep "VolumeGroup=" $SRC_CONFIG_FILE | awk -F: '{print $1}'` 

    for volName in $SOURCE_LVS
    do
        if [ -b $volName ]; then
            grep "DeviceName=$volName" $INSTALLPATH/etc/OracleTgtVolInfoFile.dat > /dev/null
            if [ $? -ne 0 ]; then
                echo "Volume $volName already present on target node on device other than selected target device"
                log "Volume $volName already present on target node on device other than selected target device"
                flag=1 
            fi
        fi
    done

    rm -f $tempfile > $NULL    

    if [ $flag -ne 0 ];then
        exit 1
    fi

    log "EXIT checkLogicalVolumes"

    return
}

CheckListeners()
{
    log "ENTER CheckListeners..."

    ps -ef |grep -v grep | grep tnslsnr > $NULL
    if [ $? -ne 0 ]; then
        log "ERROR: listener is not running"
        $ECHO "ERROR: listener is not running"

        if [ ! -f $TARGET_ORACLE_HOME/network/admin/listener.ora ]; then
            log "ERROR: listener.ora is not found in $TARGET_ORACLE_HOME/network/admin"
            $ECHO "ERROR: listener.ora is not found in $TARGET_ORACLE_HOME/network/admin"
        fi

        exit 1
    fi

    if [ ! -f $TARGET_ORACLE_HOME/network/admin/sqlnet.ora ]; then
        log "WARNING: sqlnet.ora is not found in $TARGET_ORACLE_HOME/network/admin"
        $ECHO "WARNING: sqlnet.ora is not found in $TARGET_ORACLE_HOME/network/admin"
    fi

    if [ ! -f $TARGET_ORACLE_HOME/network/admin/tnsnames.ora ]; then
        log "WARNING: tnsnames.ora is not found in $TARGET_ORACLE_HOME/network/admin"
        $ECHO "WARNING: tnsnames.ora is not found in $TARGET_ORACLE_HOME/network/admin"
    else
        grep local_listener= $SRC_CONFIG_FILE > $NULL
        if [ $? -eq 0 ]; then
            listenerName=`grep local_listener= $SRC_CONFIG_FILE | awk -F= '{print $2}'`
            grep "^$listenerName" $TARGET_ORACLE_HOME/network/admin/tnsnames.ora > $NULL
            if [ $? -ne 0 ]; then
                log "WARNING: local listener $listenerName not found in $TARGET_ORACLE_HOME/network/admin/tnsnames.ora"
                $ECHO "WARNING: local listener $listenerName not found in $TARGET_ORACLE_HOME/network/admin/tnsnames.ora"
            fi
        fi

        grep remote_listener= $SRC_CONFIG_FILE > $NULL
        if [ $? -eq 0 ]; then
            listenerName=`grep remote_listener= $SRC_CONFIG_FILE | awk -F= '{print $2}'`
            grep "^$listenerName" $TARGET_ORACLE_HOME/network/admin/tnsnames.ora > $NULL
            if [ $? -ne 0 ]; then
                log "WARNING: remote listener $listenerName not found in $TARGET_ORACLE_HOME/network/admin/tnsnames.ora"
                $ECHO "WARNING: remote listener $listenerName not found in $TARGET_ORACLE_HOME/network/admin/tnsnames.ora"
            fi
        fi
    fi

    log "EXIT CheckListeners"
}

VerifyTgtConfig()
{
    log "ENTER VerifyTgtConfig..."

    if [ ! -f $SRC_CONFIG_FILE ]; then
        log "Error: src config file '$SRC_CONFIG_FILE' is not found"
        exit 1
    fi

    SOURCE_ORACLE_VERSION=`grep "DBVersion=" $SRC_CONFIG_FILE| awk -F"=" '{print $2}'`
    if [ -z "$SOURCE_ORACLE_VERSION" ]
    then
        log "Error: DBVersion is Null in Source Configuration File"
        $ECHO "DBVersion is Null in Source Configuration File"
        exit 1
    fi

    # get the oracle home compatible with SOURCE_ORACLE_VERSION
    # for each of the OracleDatabases in appwizard.conf check
    # if the DBVersion is same as the source, use that ORACLE_HOME

    isOracleHomeFound=0

    if [ "$OSTYPE" = "Linux" -o "$OSTYPE" = "AIX" ]; then
        inventory_loc=`grep "inventory_loc" /etc/oraInst.loc | awk -F= '{print $2}'`
    elif [ "$OSTYPE" = "SunOS" ]; then
        inventory_loc=`grep "inventory_loc" /var/opt/oracle/oraInst.loc | awk -F= '{print $2}'`
    fi

    oracleHomes=`grep -w "LOC=" $inventory_loc/ContentsXML/inventory.xml | grep -v "CRS=\"true\"" |  awk '{print $3}' |awk -F= '{print $2}'|sed 's/"//g'`

    if [ ! -z "$oracleHomes" ]; then

        for oracleHome in $oracleHomes
        do
            ORACLE_HOME=`echo $oracleHome`
            export ORACLE_HOME
            TARGET_ORACLE_VERSION=`strings $ORACLE_HOME/bin/oracle | grep NLSRTL | awk '{print $3}'`

            if [ "$TARGET_ORACLE_VERSION" = "$SOURCE_ORACLE_VERSION" ]; then
                grep 'isCluster=YES' $SRC_CONFIG_FILE  > $NULL
                if [ $? -eq 0 ]; then
                    if [ "$OSTYPE" = "Linux" -o "$OSTYPE" = "AIX" ]; then
                        NM=/usr/bin/nm
                    elif [ "$OSTYPE" = "SunOS" ]; then
                        NM=/usr/ccs/bin/nm
                    fi

                    if [ -f "$NM" ]; then
                        if [ "$OSTYPE" = "AIX" ]; then
                            racSymCount=`$NM -X 64 -r $ORACLE_HOME/rdbms/lib/libknlopt.a | grep -c kcsm.o`
                        else
                            racSymCount=`$NM -r $ORACLE_HOME/rdbms/lib/libknlopt.a | grep -c kcsm.o`
                        fi
                        if [ "$racSymCount" = "0" ]; then
                            log "$ORACLE_HOME is not RAC enabled."
                            $ECHO "$ORACLE_HOME is not RAC enabled."
                        #else
                            #isOracleHomeFound=1
                            #break
                        fi
                    else
                        log "Error: File $NM not found. Oracle RAC compatibility could not be verified."
                        #exit 1
                    fi
                fi

                isOracleHomeFound=1
                break

            fi
        done
    else
        grep 'isCluster=YES' $SRC_CONFIG_FILE  > $NULL
        if [ $? -eq 0 ]; then
            log "Error: ORACLE_HOME not found for source Oracle RAC database version $SOURCE_ORACLE_VERSION." 
            $ECHO "Error: ORACLE_HOME not found for source Oracle RAC database version $SOURCE_ORACLE_VERSION." 
        else
            log "Error: ORACLE_HOME not found for source Oracle database version $SOURCE_ORACLE_VERSION." 
            $ECHO "Error: ORACLE_HOME not found for source Oracle database version $SOURCE_ORACLE_VERSION." 
        fi
        exit 1
    fi

    if [ $isOracleHomeFound -eq 0 ]; then
        grep 'isCluster=YES' $SRC_CONFIG_FILE  > $NULL
        if [ $? -eq 0 ]; then
            log "Error: Compatible ORACLE_HOME not found for source Oracle RAC database version $SOURCE_ORACLE_VERSION." 
            $ECHO "Error: Compatible ORACLE_HOME not found for source Oracle RAC database version $SOURCE_ORACLE_VERSION." 
        else
            log "Error: Compatible ORACLE_HOME not found for source Oracle database version $SOURCE_ORACLE_VERSION." 
            $ECHO "Error: Compatible ORACLE_HOME not found for source Oracle database version $SOURCE_ORACLE_VERSION." 
        fi
        exit 1
    fi
    
    TARGET_ORACLE_HOME=$ORACLE_HOME

    if [ -f "$TARGET_ORACLE_HOME/oraInst.loc" ]; then
         TARGET_ORACLE_USER=`ls -l $TARGET_ORACLE_HOME/oraInst.loc | awk '{print $3}'`
         if [ ! -z "$TARGET_ORACLE_USER" ]; then
             TARGET_ORACLE_GROUP=`id $TARGET_ORACLE_USER | awk '{print $2}' | awk -F"(" '{ print $2}' | awk -F")" '{ print $1}'`
         fi
    fi

    if [ -z "$TARGET_ORACLE_USER" -o -z "$TARGET_ORACLE_GROUP" ]; then
        log "Error: Oracle user or group not valid." 
        $ECHO "Error: Oracle user or group not valid." 
        exit 1
    fi

    log "Found TARGET_ORACLE_HOME $TARGET_ORACLE_HOME TARGET_ORACLE_USER $TARGET_ORACLE_USER TARGET_ORACLE_GROUP $TARGET_ORACLE_GROUP"
    $ECHO "Found TARGET_ORACLE_HOME $TARGET_ORACLE_HOME TARGET_ORACLE_USER $TARGET_ORACLE_USER TARGET_ORACLE_GROUP $TARGET_ORACLE_GROUP"

    grep 'isASM=YES' $SRC_CONFIG_FILE  > $NULL
    if [ $? -eq 0 ]; then

        CheckAsmOnline

        CheckAsmDiskNames

        grep 'isASMLib=YES' $SRC_CONFIG_FILE  > $NULL
        if [ $? -eq 0 ]; then
            CheckAsmLibInstall
            CheckASMLibLabels
        fi

        grep 'isASMRaw=YES' $SRC_CONFIG_FILE  > $NULL
        if [ $? -eq 0 ]; then
            log "Asm is using raw volumes"
            CheckAsmRawAliases
        fi
	
        checkASMDGOnline
    fi

    grep 'isASM=NO' $SRC_CONFIG_FILE  > $NULL
    if [ $? -eq 0 ]; then

        grep 'MountPoint=zfs' $SRC_CONFIG_FILE  > $NULL
        if [ $? -eq 0 ]; then
            log "Filesystem type is zfs "
            checkZfsFileSystem zfs 
        fi
        grep 'MountPoint=ufs' $SRC_CONFIG_FILE  > $NULL
        if [ $? -eq 0 ]; then
            log "Filesystem type is ufs"
            checkMountPoint ufs
        fi

        grep 'MountPoint=ext3' $SRC_CONFIG_FILE  > $NULL
        if [ $? -eq 0 ]; then
            log "Filesystem type is ext3"
            checkMountPoint ext3
        fi

        grep 'MountPoint=vxfs' $SRC_CONFIG_FILE  > $NULL
        if [ $? -eq 0 ]; then
            vxfsVersion=`grep 'VxfsVersion=' $SRC_CONFIG_FILE | uniq | awk -F= '{print $2}'`
            CheckVxfsInstall $vxfsVersion
            grep 'isCluster=YES' $SRC_CONFIG_FILE  > $NULL
            if [ $? -eq 0 ]; then
                CheckVxvmDGOnline
            fi
            checkMountPoint vxfs
        fi

        grep 'MountPoint=jfs2' $SRC_CONFIG_FILE  > $NULL
        if [ $? -eq 0 ]; then
            log "Filesystem type is jfs2"
            checkMountPoint jfs2 
        fi

        grep 'VolumeGroup=' $SRC_CONFIG_FILE > $NULL
        if [ $? -eq 0 ]; then
            log "checking for VolumeGroups..."
            checkVolumeGroups
            #checkLogicalVolumes
        fi

        grep 'isVxvm=YES' $SRC_CONFIG_FILE  > $NULL
        if [ $? -eq 0 ]; then
            vxvmVersion=`grep 'VxvmVersion=' $SRC_CONFIG_FILE | uniq | awk -F= '{print $2}'`
            CheckVxvmInstall $vxvmVersion
            grep "isVxvmRaw=YES" $SRC_CONFIG_FILE >>/dev/null
            if [ $? -eq 0 ]
            then
                CheckVxvmRawDGOnline
            fi
        fi
    fi

    CheckListeners

    log "EXIT VerifyTgtConfig"
}

GetParentProcName()
{
    parent=`ps -p $$ -o ppid=`
    ppname=`ps -p $parent -o args=`

    echo $ppname | egrep 'inm_tmp_script|appservice' > $NULL
    if [ $? -eq 0 ]; then
        ppname=appservice
    fi

    #$ECHO "Parent id : $parent  name : $ppname"
    log "Parent id : $parent  name : $ppname"

    return
}

########### Main ##########

trap Cleanup 0

# trap CleanupError SIGHUP SIGINT SIGQUIT SIGTERM -  SLES doesn't know the definitions
if [ "$OSTYPE" = "Linux" -o "$OSTYPE" = "SunOS" -o "$OSTYPE" = "AIX" ]; then
    trap CleanupError 1 2 3 15
else
    exit 0
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

> $TMP_OUTPUT_FILE

GetParentProcName

if [ $OPTION = "discover" ]; then

    Output "OracleDatabaseDiscoveryStart"
    GetNodeDetails
    GetClusterInformation
    DiscoverDatabases
    Output "OracleDatabaseDiscoveryEnd"

    rm -rf $TMP_CONF_FILE > $NULL
    cat $TMP_OUTPUT_FILE > $OUTPUT_FILE

elif [ $OPTION = "displayappdisks" ]; then

    GetAsmDevices
    cat $TMP_OUTPUT_FILE > $OUTPUT_FILE

elif [ $OPTION = "displaycrsdisks" ]; then

    Output "AppDeviceListStart"
    GetOcrVoteDevices
    Output "AppDeviceListEnd"
    cat $TMP_OUTPUT_FILE > $OUTPUT_FILE

elif [ $OPTION = "preparetarget" ]; then

    TakeDatabaseOffline

elif [ $OPTION = "targetreadiness" ]; then

    VerifyTgtConfig

fi

rm -rf $TMP_OUTPUT_FILE > $NULL

exit 0
