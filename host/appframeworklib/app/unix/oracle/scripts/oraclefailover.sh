#!/bin/bash
##########################################################################################################################################################
#This Program  oraclefailover.sh used for failover of oracle application
#All rights Reserved solely by InMage Systems.
#Rev 1.0
##########################################################################################################################################################

Usage()
{
    echo "Usage: $0 -a Action -c Cluster -s SourceConfigurationFile -t RecoveryPointType -n Tagname -v ListOfVolumes -p PFilePath -g GeneratedPFilePath"
    echo " "
    echo " where Action can be any of the following:"
    echo " rollback            -- To rollback  all the volumes to a consistent tag"
    echo " dgstartup           -- To start asm/vxvm disk groups or Mount Filesystems"
    echo " databasestartup     -- To start database"
    echo " "
    exit 1
}

Unsupported()
{
    echo "Detected unsupported configurations. Failover failed."
    exit 1
}

CheckVXVMCluster()
{
    if [ -f /sbin/vxdctl ]; then
        VXDCTL=/sbin/vxdctl
    elif [ -f /usr/sbin/vxdctl ]; then
        VXDCTL=/usr/sbin/vxdctl
    else
        echo "Error: vxdctl is not found. Vxvm state could not be verified."
        log  "Error: vxdctl is not found. Vxvm state could not be verified."
        exit 1
    fi

    $VXDCTL mode | grep "mode: enabled" > /dev/null
    if [ $? -ne 0 ]; then
        echo "Error: vxconfigd is not enabled. Vxvm operations cannot be performed."
        log  "Error: vxconfigd is not enabled. Vxvm operations cannot be performed."
        exit 1
    fi

    isVxVMCluster=NO
    $VXDCTL -c mode | grep "cluster active" > /dev/null
    if [ $? -eq 0 ]; then
        isVxVMCluster=YES
        echo "Vxvm cluster is active."
        log  "Vxvm cluster is active."

        $VXDCTL -c mode | grep -w MASTER > /dev/null
        if [ $? -eq 0 ]; then
            isVxVMMaster=YES
        fi

    else
        echo "Vxvm cluster is not active."
        log  "Vxvm cluster is not active."
    fi

}


VXVMActions()
{
    echo "Performing VXVM related actions.."
    log "Performing VXVM related actions.."

    CheckVXVMCluster

    echo "Performing vxdisk scan.."
    log "Performing vxdisk scan.."

    log "vxdisk scandisks"
    echo "vxdisk scandisks"
    vxdisk scandisks

    tempfile1=/tmp/vxmp1.$$
    for volMountPoint in $SOURCE_MOUNTPOINTS
    do
        fsType=`echo $volMountPoint |$AWK -F":" '{ print $1 }'`
        DiskPath=`echo $volMountPoint |$AWK -F":" '{ print $2 }'`
        mountPoint=`echo $volMountPoint |$AWK -F":" '{ print $3 }'`
        echo "$mountPoint/:$DiskPath:$fsType" >> $tempfile1
    done

    for volMountPoint in `cat $tempfile1 | sort | uniq`
    do
        mountPoint=`echo $volMountPoint |$AWK -F":" '{ print $1 }'`
        DiskPath=`echo $volMountPoint |$AWK -F":" '{ print $2 }'`
        fsType=`echo $volMountPoint |$AWK -F":" '{ print $3 }'`

        DiskGroup=`echo $DiskPath | $AWK -F"/" '{ print $5 }'`
        vxdisk -o alldgs list | awk '{print $4}' >$tempfile1

        cat $tempfile1|grep -w $DiskGroup>>/dev/null
        if [ $? -eq 0 ]
        then 
            echo "Found Diskgroup $DiskGroup in vxdisk list"
            log "Found Diskgroup $DiskGroup in vxdisk list"
        else
            echo "Diskgroup $DiskGroup is not listed in 'vxdisk list'"
            log "Diskgroup $DiskGroup is not listed in 'vxdisk list'"
            echo "Cannot import diskgroup $DiskGroup."
            log "Cannot import diskgroup $DiskGroup."
            echo "Hence exiting.."
            exit 1
        fi 	

        vxdg list | grep -w  $DiskGroup | grep enabled
        if [ $? -eq 0 ]; then
            echo "disk group $DiskGroup already imported"
            log "disk group $DiskGroup already imported"
        else
            if [ "$isVxVMCluster" = "YES" ]; then
                if [ "$TARGET_IsCluster" = "YES" ]; then
                    if [ "$isVxVMMaster" = "YES" ]; then
                        log "vxdg -sC import $DiskGroup"
                        echo "vxdg -sC import $DiskGroup"
                        vxdg -sC import $DiskGroup
                        if [ $? -ne 0 ]; then
                            echo "Importing shared diskgroup $DiskGroup failed.."
                            log "Importing shared diskgroup $DiskGroup failed.."
                            exit 1
                        fi
                    else

                        echo "==============================================================================================="
                        echo "Diskgroup $DiskGroup must be imported from VXVM cluster master node."
                        echo "Please run the following commands on the master node."
                        echo "vxdg -sC import $DiskGroup"
                        echo "vxvol -g $DiskGroup startall"
                        echo "==============================================================================================="

                        log "Diskgroup $DiskGroup must be imported from VXVM cluster master node."
                        exit 1
                    fi
                else
                    log "vxdg -C import $DiskGroup"
                    echo "vxdg -C import $DiskGroup"
                    vxdg -C import $DiskGroup
                    if [ $? -ne 0 ]; then
                        echo "Importing diskgroup $DiskGroup failed.."
                        log "Importing diskgroup $DiskGroup failed.."
                        exit 1
                    fi
                fi
            else
                log "vxdg -C import $DiskGroup"
                echo "vxdg -C import $DiskGroup"
                vxdg -C import $DiskGroup
                if [ $? -ne 0 ]; then
                    echo "Importing diskgroup $DiskGroup failed.."
                    log "Importing diskgroup $DiskGroup failed.."
                    exit 1
                fi
            fi
        fi
        
        rm -f $tempfile1

        Vol=`echo $DiskPath | $AWK -F"/" '{ print $6 }'`
        vxprint -g $DiskGroup -v | grep -w $Vol | grep -i enabled
        if [ $? -eq 0 ]; then
            echo "Volume $Vol on $DiskGroup already started"
            log "Volume $Vol on $DiskGroup already started"
        else
            if [ "$isVxVMCluster" = "YES" ]; then
                if [ "$TARGET_IsCluster" = "YES" ]; then
                    if [ "$isVxVMMaster" = "YES" ]; then
                        log "vxvol -g $DiskGroup start $Vol"
                        echo "vxvol -g $DiskGroup start $Vol"
                        vxvol -g $DiskGroup start $Vol
                        if [ $? -ne 0 ]; then
                            echo "Starting volume $Vol failed.."
                            log "Starting volume $Vol failed.."
                            exit 1
                        fi
                    else
                        echo "==============================================================================================="
                        echo "Volumes on diskgroup $DiskGroup must be started from VXVM cluster master node."
                        echo "Please run the following commands on the master node."
                        echo "vxvol -g $DiskGroup startall"
                        echo "==============================================================================================="

                        log  "Volumes on diskgroup $DiskGroup must be started from VXVM cluster master node."
                        exit 1
                    fi
                else
                    log "vxvol -g $DiskGroup start $Vol"
                    echo "vxvol -g $DiskGroup start $Vol"
                    vxvol -g $DiskGroup start $Vol
                    if [ $? -ne 0 ]; then
                        echo "Starting volume $Vol failed.."
                        log "Starting volume $Vol failed.."
                        exit 1
                    fi
                fi
            else
                log "vxvol -g $DiskGroup start $Vol"
                echo "vxvol -g $DiskGroup start $Vol"
                vxvol -g $DiskGroup start $Vol
                if [ $? -ne 0 ];then
                    echo "Starting volume $Vol failed.."
                    log "Starting volume $Vol failed.."
                    exit 1
                fi
            fi
        fi

        # check if filesyste already mounted
        mount | grep $DiskPath > /dev/null
        if [ $? -eq 0 ]; then
            log " $DiskPath already mounted."
            echo " $DiskPath already mounted."
        else
            log "mkdir -p $mountPoint"
            echo "mkdir -p $mountPoint"
            mkdir -p $mountPoint

            RDisk=`echo $DiskPath|sed 's/dsk/rdsk/g'`
            
            if [ "$isVxVMCluster" = "YES" ]; then
                if [ "$TARGET_IsCluster" = "YES" ]; then
                    log "mount -$MOUNT_ARG $fsType -o cluster $DiskPath $mountPoint"
                    echo "mount -$MOUNT_ARG $fsType -o cluster $DiskPath $mountPoint"
                    mount -$MOUNT_ARG $fsType -o cluster $DiskPath $mountPoint
                else
                    log "mount -$MOUNT_ARG $fsType $DiskPath $mountPoint"
                    echo "mount -$MOUNT_ARG $fsType $DiskPath $mountPoint"
                    mount -$MOUNT_ARG $fsType $DiskPath $mountPoint
                fi
            else
                log "mount -$MOUNT_ARG $fsType $DiskPath $mountPoint"
                echo "mount -$MOUNT_ARG $fsType $DiskPath $mountPoint"
                mount -$MOUNT_ARG $fsType $DiskPath $mountPoint
            fi

            if [ $? -ne 0 ]
            then
                echo "Mounting the filesystem failed.."
                log "Mounting the filesystem failed.."

                echo "Running fsck on filesystem .."
                log "Running fsck on filesystem .."

                log "fsck -$MOUNT_ARG $fsType $RDisk"
                echo "fsck -$MOUNT_ARG $fsType $RDisk"
                fsck -$MOUNT_ARG $fsType $RDisk

                if [ "$isVxVMCluster" = "YES" ]; then
                    if [ "$TARGET_IsCluster" = "YES" ]; then
                        log "mount -$MOUNT_ARG $fsType -o cluster $DiskPath $mountPoint"
                        echo "mount -$MOUNT_ARG $fsType -o cluster $DiskPath $mountPoint"
                        mount -$MOUNT_ARG $fsType -o cluster $DiskPath $mountPoint
                    else
                        log "mount -$MOUNT_ARG $fsType $DiskPath $mountPoint"
                        echo "mount -$MOUNT_ARG $fsType $DiskPath $mountPoint"
                        mount -$MOUNT_ARG $fsType $DiskPath $mountPoint
                    fi
                else
                    log "mount -$MOUNT_ARG $fsType $DiskPath $mountPoint"
                    echo "mount -$MOUNT_ARG $fsType $DiskPath $mountPoint"
                    mount -$MOUNT_ARG $fsType $DiskPath $mountPoint
                fi

                if [ $? -ne 0 ]
                then
                    echo "Unable to mount filesytem after fsck also.."
                    log "Unable to mount filesytem after fsck also.."
                    echo "Exiting.."
                    exit 1
                fi
            fi	
        fi

        log "chown -R $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $mountPoint"
        echo "chown -R $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $mountPoint"
        chown -R $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $mountPoint

    done

    echo "Completed the vxvm specific actions."
    log "Completed the vxvm specific actions."
}

VXVMRawActions()
{
    echo "Performing VXVM related actions.."
    log "Performing VXVM related actions.."

    CheckVXVMCluster

    echo "Performing vxdisk scan.."
    log "Performing vxdisk scan.."

    echo "vxdisk scandisks"
    log  "vxdisk scandisks"
    vxdisk scandisks

    for DiskGroup in $SOURCE_VxvmDgName
    do
        vxdisk -o alldgs list | awk '{print $4}' >$tempfile1

        cat $tempfile1|grep -w $DiskGroup>>/dev/null
        if [ $? -eq 0 ]
        then 
            echo "Found diskgroup $DiskGroup in 'vxdisk list'"
            log "Found diskgroup $DiskGroup in 'vxdisk list'"
        else
            echo "Diskgroup $DiskGroup is not listed in 'vxdisk list'"
            log "Diskgroup $DiskGroup is not listed in 'vxdisk list'"
            echo "Cannot import diskgroup $DiskGroup"
            log "Cannot import diskgroup $DiskGroup"
    	    exit 1
        fi 	

        #XXX: vxdisk cleaerimport

        vxdg list | grep -w  $DiskGroup | grep enabled
        if [ $? -eq 0 ]; then
            echo "Diskgroup $DiskGroup already imported"
            log "Diskgroup $DiskGroup already imported"
        else
            if [ "$isVxVMCluster" = "YES" ]; then
                if [ "$TARGET_IsCluster" = "YES" ]; then
                    if [ "$isVxVMMaster" = "YES" ]; then
                        log "vxdg -sC import $DiskGroup"
                        echo "vxdg -sC import $DiskGroup"
                        vxdg -sC import $DiskGroup
                        if [ $? -ne 0 ]; then
                            echo "Importing shared diskgroup $DiskGroup failed."
                            log "Importing shared diskgroup $DiskGroup failed."
                            exit 1
                        fi
                    else

                        echo "============================================================================================"
                        echo "Diskgroup $DiskGroup must be imported from VXVM cluster master node."
                        echo "Please run the following commands on the master node."
                        echo "vxdg -sC import $DiskGroup"
                        echo "vxvol -g $DiskGroup startall"
                        echo "vxedit -g $DiskGroup set group=$TARGET_ORACLE_GROUP user=$TARGET_ORACLE_USER mode=660 volume_name"
                        echo "where volume_name is the volume belonging to diskgroup $DiskGroup."
                        echo "============================================================================================"

                        log "Diskgroup $DiskGroup must be imported from VXVM cluster master node."
                        exit 1
                    fi
                else
                    log "vxdg -C import $DiskGroup"
                    echo "vxdg -C import $DiskGroup"
                    vxdg -C import $DiskGroup
                    if [ $? -ne 0 ]; then
                        echo "Importing diskgroup $DiskGroup failed."
                        log "Importing diskgroup $DiskGroup failed."
                        exit 1
                    fi
                fi
            else
                log "vxdg -C import $DiskGroup"
                echo "vxdg -C import $DiskGroup"
                vxdg -C import $DiskGroup
                if [ $? -ne 0 ]
                then
                    echo "Importing diskgroup $DiskGroup failed."
                    log "Importing diskgroup $DiskGroup failed."
                    exit 1
                fi
            fi
        fi

        for rawVol in `ls -l /dev/vx/dsk/$DiskGroup | grep -v total | awk '{ print $NF }'`
        do
            vxprint -g $DiskGroup -v | grep -w $rawVol | grep -i enabled
            if [ $? -eq 0 ]; then
                echo "Volume $rawVol on $DiskGroup already started."
                log "Volume $rawVol on $DiskGroup already started."
            else
                if [ "$isVxVMCluster" = "YES" ]; then
                    if [ "$TARGET_IsCluster" = "YES" ]; then
                        if [ "$isVxVMMaster" = "YES" ]; then
                            log "vxvol -g $DiskGroup start $rawVol"
                            echo "vxvol -g $DiskGroup start $rawVol"
                            vxvol -g $DiskGroup start $rawVol
                            if [ $? -ne 0 ]
                            then
                                echo "Starting volume $rawVol failed."
                                log "Starting volume $rawVol failed."
                                exit 1
                            fi
                        else
                            echo "==============================================================================================="
                            echo "Volumes on diskgroup $DiskGroup must be started from VXVM cluster master node."
                            echo "Please run the following commands on the master node."
                            echo "vxvol -g $DiskGroup startall"
                            echo "vxedit -g $DiskGroup set group=$TARGET_ORACLE_GROUP user=$TARGET_ORACLE_USER mode=660 volume_name"
                            echo "where volume_name is the volume belonging to diskgroup $DiskGroup."
                            echo "==============================================================================================="

                            log "Volumes on diskgroup $DiskGroup must be started from VXVM cluster master node."
                            exit 1
                        fi
                    else
                        log "vxvol -g $DiskGroup start $rawVol"
                        echo "vxvol -g $DiskGroup start $rawVol"
                        vxvol -g $DiskGroup start $rawVol
                        if [ $? -ne 0 ]
                        then
                            echo "Starting volume $rawVol failed."
                            log "Starting volume $rawVol failed."
                            exit 1
                        fi
                    fi
                else
                    log  "vxvol -g $DiskGroup start $rawVol"
                    echo "vxvol -g $DiskGroup start $rawVol"
                    vxvol -g $DiskGroup start $rawVol
                    if [ $? -ne 0 ]
                    then
                        echo "Starting volume $rawVol failed."
                        log "Starting volume $rawVol failed."
                        exit 1
                    fi
                fi
            fi

            volUser=`ls -l /dev/vx/dsk/$DiskGroup/$rawVol | awk '{print $3}'`
            volGroup=`ls -l /dev/vx/dsk/$DiskGroup/$rawVol | awk '{print $4}'`

            if [ "$volUser" != "$TARGET_ORACLE_USER" -o "$volGroup" != "$TARGET_ORACLE_GROUP" ]; then
                if [ "$isVxVMCluster" = "YES" ]; then
                    if [ "$TARGET_IsCluster" = "YES" ]; then
                        if [ "$isVxVMMaster" = "YES" ]; then
                            log  "vxedit -g $DiskGroup set group=$TARGET_ORACLE_GROUP user=$TARGET_ORACLE_USER mode=660 $rawVol"
                            echo "vxedit -g $DiskGroup set group=$TARGET_ORACLE_GROUP user=$TARGET_ORACLE_USER mode=660 $rawVol"
                            vxedit -g $DiskGroup set group=$TARGET_ORACLE_GROUP user=$TARGET_ORACLE_USER mode=660 $rawVol
                            if [ $? -ne 0 ]; then
                                echo "Setting permissions on $rawVol failed.."
                                log "Setting permissions on $rawVol failed.."
                                exit 1
                            fi
                        else
                            echo "==============================================================================================="
                            echo "Volume permissions must be changed from VXVM cluster master node."
                            echo "Please run the following commands on the master node."
                            echo "vxedit -g $DiskGroup set group=$TARGET_ORACLE_GROUP user=$TARGET_ORACLE_USER mode=660 volume_name"
                            echo "where volume_name is the volume belonging to diskgroup $DiskGroup."
                            echo "==============================================================================================="

                            log "Volume permissions must be changed from VXVM cluster master node."
                            exit 1
                        fi
                    else
                        log  "vxedit -g $DiskGroup set group=$TARGET_ORACLE_GROUP user=$TARGET_ORACLE_USER mode=660 $rawVol"
                        echo "vxedit -g $DiskGroup set group=$TARGET_ORACLE_GROUP user=$TARGET_ORACLE_USER mode=660 $rawVol"
                        vxedit -g $DiskGroup set group=$TARGET_ORACLE_GROUP user=$TARGET_ORACLE_USER mode=660 $rawVol
                        if [ $? -ne 0 ]; then
                            echo "Setting permissions on $rawVol failed.."
                            log "Setting permissions on $rawVol failed.."
                            exit 1
                        fi
                    fi
                else
                    log  "vxedit -g $DiskGroup set group=$TARGET_ORACLE_GROUP user=$TARGET_ORACLE_USER mode=660 $rawVol"
                    echo "vxedit -g $DiskGroup set group=$TARGET_ORACLE_GROUP user=$TARGET_ORACLE_USER mode=660 $rawVol"
                    vxedit -g $DiskGroup set group=$TARGET_ORACLE_GROUP user=$TARGET_ORACLE_USER mode=660 $rawVol
                    if [ $? -ne 0 ]; then
                        echo "Setting permissions on $rawVol failed.."
                        log "Setting permissions on $rawVol failed.."
                        exit 1
                    fi
                fi
            fi
        done
    done

    echo "Completed vxvm specific actions."
    log "Completed vxvm specific actions."
}

CheckASMInstanceStatus()
{
    echo "Checking the status of ASM instance..."
    log "Checking the status of ASM instance..."
    ps -ef|grep -v "grep"|grep -w asm_pmon_${TARGET_ASM_SID}>>/dev/null
    if [ $? -ne 0 ]
    then
        echo "ASM instance is not running"
        log "ASM instance is not running"
        echo "Starting the ASM instance.."
        log "Starting the ASM instance.."
        StartASMInstance
    else
        echo "ASM instance $TARGET_ASM_SID is already running.."
        log "ASM instance $TARGET_ASM_SID is already running.."
    fi
}
	
StartASMInstance()
{
    ORACLE_HOME=$TARGET_ASM_HOME
    ORACLE_SID=$TARGET_ASM_SID
    ORACLE_USER=$TARGET_ORACLE_USER

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile2
        startup;
_EOF"

    ps -ef|grep -v "grep"|grep -w asm_pmon_${TARGET_ASM_SID}>>/dev/null
    if [ $? -ne 0 ]
    then
        echo "Unable to start the ASM instance.."
	log "Unable to start the ASM instance.."
        echo "Please bring up the ASM instance"
	log "Please bring up the ASM instance"
        exit 1
    else
        echo "Succesfully started the ASM instance.."
	log "Succesfully started the ASM instance.."
    fi
}

GetSourceDevice()
{
   TargetDevice=$1
   PairInfo=$INMAGE_DIR/etc/$SOURCE_DBNAME.PairInfo
   
   rm -f $tempfile6
   match=0
   while read line
   do
     case $line in
       "<TargetDevice>")
            read line1
            tgtdev=`echo $line1|awk -F">" '{print $2}'|awk -F"</" '{print $1}'`
            if [ "$TargetDevice" = "$tgtdev" ];then
               match=1
            fi
        ;;        
       "</TargetDevice>")
            match=0
        ;;
        *)
           if [ $match -eq 1 ];then
              echo $line >> $tempfile6
           fi
     esac
   done < $PairInfo 
}

GetTargetDevice()
{
   SourceDevice=$1
   PairInfo=$INMAGE_DIR/etc/$SOURCE_DBNAME.PairInfo
   
   rm -f $tempfile6
   while read line
   do
     case $line in 
       "<TargetDevice>")
           read line1
           tgtdev=`echo $line1|awk -F">" '{print $2}' | awk -F"</" '{print $1}'`
       ;;
      *source_devicename*)
           srcdev=`echo $line|awk -F">" '{print $2}'|awk -F"</" '{print $1}'`
           echo $srcdev|grep -w $SourceDevice >> /dev/null
           if [ $? -eq 0 ];then
              echo "<target_devicename>$tgtdev</target_devicename>" > $tempfile6
           fi
     esac
   done < $PairInfo
}

LvmActions()
{
    echo "Performing LVM related actions.."
    log "Performing LVM related actions.."
    rm -f $tempfile1 $tempfile2 $tempfile5

    CXServerIP=`grep -i "^Hostname" $INMAGE_DIR/etc/drscout.conf|$AWK -F"=" '{ print $2 }' | sed "s/ //g"`
    CXPort=`grep -i "^Port" $INMAGE_DIR/etc/drscout.conf|$AWK -F"=" '{ print $2 }' | sed "s/ //g"`
    TARGET_HostID=`grep -i "^HostId" $INMAGE_DIR/etc/drscout.conf|$AWK -F"=" '{ print $2 }' | sed "s/ //g"`
    log " CXServerIP=$CXServerIP CXPort=$CXPort TARGET_HostID=$TARGET_HostID"

    if [ ! -z "$SOURCE_VGS" ]; then
        echo "Source filesystems are on LVs."
        log "Source filesystems are on LVs."

        >TgtVgMapping
        >SrcTgtMapping

        for tgtVol in `echo $TARGET_DEVICES|sed 's/,/ /g'`
        do
            echo "Determining source devices for target device $tgtVol.."
            log  "Determining source devices for target device $tgtVol.."

            GetSourceDevice $tgtVol  
            #As in Prism we are going to have same source device name for two hostIds, not passing sourcehostId argument to GetSourceDevice  
            SourceDevice=`grep "source_devicename" $tempfile6|$AWK -F">" '{ print $2 }'|$AWK -F"<" '{print $1}'|uniq`
            if [ -z "$SourceDevice" ]; then
                echo "Failed to fetch Source device name for $tgtVol using cxcli."
                log "Failed to fetch Source device name for $tgtVol using cxcli."
                exit 1
            fi
            echo "$SourceDevice $tgtVol" >> SrcTgtMapping
        done

        for vginfo in $SOURCE_VGS
        do
            vgname=`echo $vginfo |$AWK -F":" '{ print $2 }'`

            tgtPvList=
            for SourceDisk in `echo $vginfo | $AWK -F: '{ for(i=3;i<=NF;i++) print $i }'`
            do
                TargetDevice=`grep -w ^$SourceDisk SrcTgtMapping | $AWK '{print $2}'` 
                if [ ! -b "$TargetDevice" ]; then
                    echo "Failed to fetch target device name for $SourceDisk in SrcTgtMapping."
                    log "Failed to fetch target device name for $SourceDisk in SrcTgtMapping."
                    exit 1
                fi
                pvName=`echo $TargetDevice | $AWK -F"/" '{ print $3 }'`
                tgtPvList=`echo "${tgtPvList} ${pvName}"`
            done

            echo "$vgname $tgtPvList" >> TgtVgMapping
            log "$vgname $tgtPvList"

        done
        rm -f $tempfile6
        return
    fi

    tempfile1=/tmp/lvmact.$$
    for volMountPoint in $SOURCE_MOUNTPOINTS
    do
        fsType=`echo $volMountPoint |$AWK -F":" '{ print $1 }'`
        DiskPath=`echo $volMountPoint |$AWK -F":" '{ print $2 }'`
        mountPoint=`echo $volMountPoint |$AWK -F":" '{ print $3 }'`
        echo "$mountPoint/:$DiskPath:$fsType" >> $tempfile1
    done

    for volMountPoint in `cat $tempfile1 | sort | uniq`
    do
        mountPoint=`echo $volMountPoint |$AWK -F":" '{ print $1 }'`
        DiskPath=`echo $volMountPoint |$AWK -F":" '{ print $2 }'`
        fsType=`echo $volMountPoint |$AWK -F":" '{ print $3 }'`

        echo "Determining corresponding target partitions.."
        log "Determining corresponding target partitions.."

        GetTargetDevice $DiskPath
        Target_partition=`grep "target_devicename" $tempfile6|$AWK -F">" '{ print $2 }'|$AWK -F"<" '{print $1}'`
        if [ -z "$Target_partition" ]; then
            echo "Failed to fetch target device name for $DiskPath using cxcli."
            log "Failed to fetch target device name for $DiskPath using cxcli."
            exit 1
        fi

        if [ "$fsType" = "ocfs2" ]; then
            echo "$MOUNT_ARG $fsType -o _netdev,datavolume,nointr $Target_partition $mountPoint" >> MountOnTarget
        else
            echo "$MOUNT_ARG $fsType $Target_partition $mountPoint" >> MountOnTarget
        fi
        echo "$mountPoint" >> ForPermissions
    done

    rm -f $tempfile1 > $NULL
    rm -f $tempfile6 
    return
}

ImportVgs()
{
    echo "Importing the VGs on target.."
    log "Importing the VGs on target.."

    vgImportError=0
    cat TgtVgMapping | uniq | while read vginfo
    do
        vgname=`echo $vginfo |$AWK '{ print $1 }'`

        if [ "$OSTYPE" = "AIX" ]; then
            lsvg -o | grep -w $vgname > /dev/null 
            if [ $? -eq 0 ]; then
                echo "Volume group $vgname already imported"
                log "Volume group $vgname already imported"
                continue
            fi
            echo "recreatevg -Y NA -y $vginfo"
            log "recreatevg -Y NA -y $vginfo"
            recreatevg -Y NA -y $vginfo
            if [ $? -ne 0 ]; then
                echo "Volume group $vgname import failed"
                log "Volume group $vgname import failed"
                vgImportError=1
                continue
            fi
        else
            echo "Not supported on $OSTYPE"
            log "Not supported on $OSTYPE"
            exit 1
        fi
    done
    
    #rm -f TgtVgMapping

    if [ $vgImportError -eq 1 ]; then
        exit 1
    fi
}

MountVolumes()
{
    echo "Mounting the filesystems on target.."
    log "Mounting the filesystems on target.."

    if [ ! -z "$SOURCE_VGS" ]; then
        echo "Source filesystems are on LVs."
        log "Source filesystems are on LVs."
        ImportVgs

        tempfile1=/tmp/mntvol.$$
        for volMountPoint in $SOURCE_MOUNTPOINTS
        do
            fsType=`echo $volMountPoint |$AWK -F":" '{ print $1 }'`
            DiskPath=`echo $volMountPoint |$AWK -F":" '{ print $2 }'`
            mountPoint=`echo $volMountPoint |$AWK -F":" '{ print $3 }'`
            echo "$mountPoint/:$DiskPath:$fsType" >> $tempfile1
        done

        for volMountPoint in `cat $tempfile1 | sort | uniq`
        do
            mountPoint=`echo $volMountPoint |$AWK -F":" '{ print $1 }'`
            mountPoint=`echo $mountPoint | sed 's/\/$//g'`
            DiskPath=`echo $volMountPoint |$AWK -F":" '{ print $2 }'`
            fsType=`echo $volMountPoint |$AWK -F":" '{ print $3 }'`

            df -k | awk '{print $NF}' | grep -w $mountPoint > /dev/null
            if [ $? -eq 0 ]; then
                echo "$mountPoint already mounted"
                log "$mountPoint already mounted"
                continue
            fi

            if [ "$OSTYPE" = "AIX" ]; then
                logVolume=`echo $volMountPoint |$AWK -F":" '{ print $4 }'`
            fi
            
            mkdir -p $mountPoint

            if [ "$OSTYPE" = "AIX" ]; then
                vgName=`grep "VolumeGroup=" $SOURCECONFIGFILE | grep -w $DiskPath | uniq |$AWK -F: '{print $2}'`
                if [ -z "$vgName" ]; then
                    echo "VolumeGroup not found for logical volume $DiskPath"
                    log "VolumeGroup not found for logical volume $DiskPath"
                    exit 1
                fi

                # this should never be 'grep -w $mountPoint'
                lvName=`lsvg -l $vgName | grep "$mountPoint$" | $AWK  '{print $1}'`
                if [ -z "$lvName" ]; then
                    echo "lv not found for mount point $mountPoint"
                    log  "lv not found for mount point $mountPoint"
                    exit 1
                fi

                uvMountPoint=`lslv -l  $lvName | grep $lvName | $AWK -F":" '{ print $2 }'`
                if [ -z "$uvMountPoint" ]; then
                    echo "mount point not found for lv $lvName"
                    log  "mount point not found for lv $lvName"
                    exit 1
                fi

                if [ "$uvMountPoint" != "$mountPoint" ]; then
                    echo "chfs -m $mountPoint $uvMountPoint"
                    log "chfs -m $mountPoint $uvMountPoint"
                    chfs -m $mountPoint $uvMountPoint
                    if [ $? -ne 0 ]; then
                        exit 1
                    fi
                fi

                mount $mountPoint
            else
                echo "mount -$MOUNT_ARG $fsType $DiskPath $mountPoint"
                log "mount -$MOUNT_ARG $fsType $DiskPath $mountPoint"
                mount -$MOUNT_ARG $fsType $DiskPath $mountPoint
            fi
            if [ $? -ne 0 ]; then
                log "Mount failed"
                echo "Mount failed"
                exit 1
            fi

            chown -R $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $mountPoint
        done

        rm -f $tempfile1 > $NULL

        echo "Succesfully Mounted the Volumes.."
        log "Succesfully Mounted the Volumes.."
        return
    fi

    cat ForPermissions|while read line
    do
        mkdir -p $line
    done

    cat MountOnTarget|while read line
    do
        log " mount -$line"
        mount -$line
    done	
    cat ForPermissions|while read line
    do
        chown -R $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $line
    done

    echo "Succesfully Mounted the Volumes.."
    log "Succesfully Mounted the Volumes.."

    rm -f MountOnTarget
    rm -f ForPermissions
}

MountZfsVolumes()
{
   echo "Mounting the zfs filesystems on target.."
   log "Mounting the zfs filesystems on target.." 

   tempfile1=/tmp/mntvol.$$
   for volMountPoint in $SOURCE_MOUNTPOINTS
   do
      fsType=`echo $volMountPoint |$AWK -F":" '{ print $1 }'`
      DiskPath=`echo $volMountPoint |$AWK -F":" '{ print $2 }'`
      mountPoint=`echo $volMountPoint |$AWK -F":" '{ print $3 }'`
      Disks=`echo $volMountPoint |$AWK -F":" '{ print $4 }'`
      echo "$mountPoint/:$DiskPath:$fsType:$Disks" >> $tempfile1
   done

   for volMountPoint in `cat $tempfile1 | sort | uniq`
   do
       mountPoint=`echo $volMountPoint |$AWK -F":" '{ print $1 }'`
       DiskPath=`echo $volMountPoint |$AWK -F":" '{ print $2 }'`
       fsType=`echo $volMountPoint |$AWK -F":" '{ print $3 }'`

       df -k | awk '{print $NF}' | grep -w $mountPoint > /dev/null
       if [ $? -eq 0 ]; then
           echo "$mountPoint already mounted"
           log "$mountPoint already mounted"
           continue
       fi

       mkdir -p $mountPoint
      
       PoolName=`echo $DiskPath | awk -F/ '{print $1}'`
       TgtPools=`zpool list|grep -v NAME |awk '{print $1}'` 
       for TgtPool in $TgtPools
       do
          if [ $TgtPool = $PoolName ];then
              log "$PoolName is already exists on target, destroying it"
              zpool destroy $TgtPool
          fi
          zpool status $TgtPool |grep 'c[0-9]'|awk '{print $1}' > /tmp/zpooldisks
          for tgtVol in `echo $TARGET_DEVICES|sed 's/,/ /g'`
          do
             grep $tgtVol /tmp/zpooldisks > /dev/null
             if [ $? -eq 0 ];then
                 echo "ERROR: $tgtVol is part of already active zpool $TgtPool on the target"
                 log "ERROR: $tgtVol is part of already active zpool $TgtPool on the target"
                 zpool destroy $TgtPool
             fi
          done
       done 

       log "Importing $PoolName"
       zpool import -f $PoolName 
       status=`zpool status $PoolName | grep state | awk -F":" '{print $2}' |sed 's/ //g'`
       if [[ "$status" = "ONLINE" ]];then
            echo "zpool is created successfully"
            log "zpool is created successfully"
       else
            error=`zpool status $PoolName`
            echo "Errors while importing zpool : $error"
            exit 1
       fi

    done 

}
ChangesInPfileClusterToCluster()
{

    echo "modifying pfile entries...Cluster to Cluster.."
    log "modifying pfile entries...Cluster to Cluster.."
    Totalnodes=$TARGET_CLUSTER_TOTALNODES
    log "Added the line *.cluster_database_instances=$Totalnodes in pfile.."
    rm -f $tempfile1

    grep -v "*.cluster_database_instances" $SRCGENPFILE >/tmp/tempfile1
    echo "*.cluster_database_instances=$Totalnodes" >>/tmp/tempfile1
    cp -f /tmp/tempfile1 $SRCGENPFILE

    chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP  $SRCGENPFILE

}

ChangesInPfileStandaloneToStandalone()
{

    echo "modifying pfile entries...Standalone to Standalone.."
    log "modifying pfile entries...Standalone to Standalone.."

    chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP  $SRCGENPFILE
}		

ChangesInPfileStandaloneToCluster()
{
    echo "modifying pfile entries...Standalone to Cluster.."
    log "modifying pfile entries...Standalone to Cluster.."
    Totalnodes=$TARGET_CLUSTER_TOTALNODES
    rm -f $tempfile1 $tempfile2

    if [ "$SOURCE_DBNAME" != "$SOURCE_ORACLE_SID" ]
    then
        grep -v "*.cluster_database=false" $SRCGENPFILE >/tmp/tempfile1
        echo "*.cluster_database=true">>/tmp/tempfile1 
        echo "*.cluster_database_instances=$Totalnodes">>/tmp/tempfile1
        cp -f /tmp/tempfile1 $SRCGENPFILE
        log "Added the line *.cluster_database_instances=$Totalnodes in pfile.."
        log "Added the line *.cluster_database=true in pfile"

    fi

    chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP  $SRCGENPFILE
}
	

CheckInstanceStatus()
{
    ORACLE_HOME=$TARGET_ORACLE_HOME
    ORACLE_SID=$SOURCE_ORACLE_SID
    ORACLE_USER=$TARGET_ORACLE_USER
    rm -f $tempfile1

    ps -ef|grep -v grep|grep -w ora_pmon_${ORACLE_SID} >>/dev/null
    if [ $? -eq 0 ]
    then
        echo "Instance $ORACLE_SID is already running.."
        log "Instance $ORACLE_SID is already running.."
        echo "Trying to shutdown the database.."
        log  "Trying to shutdown the database.."
        su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile1
        shutdown immediate;
_EOF"
        ps -ef|grep -v "grep"|grep -w ora_pmon_${ORACLE_SID} >>/dev/null
        if [ $? -eq 0 ]
        then 
            echo "Unable to shutdown the database.."
	    log "Unable to shutdown the database.."
            echo "Hence exiting..!"
	    log "Hence exiting..!"
            exit 1
        else
            echo "Database is shutdown succesfully.."
	    log "Database is shutdown succesfully.."
        fi
    fi
}


####Function:OracleRecover
####Action:Performs database recovery actions
####O/P:Brings up the Database 
###########################################################
OracleRecover()
{
    echo "Performing oracle recovery actions...."
    log "Performing oracle recovery actions...."
    ORACLE_HOME=$TARGET_ORACLE_HOME
    ORACLE_SID=$SOURCE_ORACLE_SID
    ORACLE_USER=$TARGET_ORACLE_USER

    rm -f $tempfile1 $tempfile2 $tempfile3

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile1
        select status from v\\\$instance;
_EOF"

    cat $tempfile1|grep "OPEN">> /dev/null
    if [ $? -eq 0 ]
    then
        cat $tempfile1
        echo "Database started and opened"
	log "Database started and opened"
    else
        su - $ORACLE_USER -c "ORACLE_HOME=$ORACLE_HOME;ORACLE_SID=$ORACLE_SID;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile2
                    select member from v\\\$logfile;
                    exit;
_EOF"


        su - $ORACLE_USER -c "ORACLE_HOME=$ORACLE_HOME;ORACLE_SID=$ORACLE_SID;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile3
                    recover database;
                    exit;
_EOF"


        cat $tempfile3|grep "Media recovery complete">> /dev/null
        if [ $? -ne 0 ]
        then
            echo "database recovery failed"
            log "database recovery failed"
            exit 1
        fi

        su - $ORACLE_USER -c "ORACLE_HOME=$ORACLE_HOME;ORACLE_SID=$ORACLE_SID;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile3
                    alter database open;
                    select status from v\\\$instance;
                    exit;
_EOF"

        grep "OPEN" $tempfile3 >>/dev/null
        if [ $? -eq 0 ]
        then
            echo "Database started,Recovered with redo logs and database opened"
            log "Database started,Recovered with redo logs and database opened"
        else
            cat $tempfile3
            echo "Unable to open database."
            log "Unable to open database."
            echo "Recovery process failed...Please check the database manually"
            log "Recovery process failed...Please check the database manually"
            exit 123
        fi
    fi
}


##############Defining the functions########################
####Function:ChangesInPfile
####Action:changes the contents of pfile in case of cluster to standalone failover and keeps back up of pfile for failback activities
####O/P:Modifies the pfile
###########################################################
ChangesInPfileClusterToStandalone()
{
    echo "modifying pfile entries...Cluster to Standalone.."
    log "modifying pfile entries...Cluster to Standalone.."
    rm -f $tempfile1
    log "Adding the line *.cluster_database=false in pfile"

    grep -v "*.cluster_database"  $SRCGENPFILE >/tmp/tempfile1
    echo "*.cluster_database=false">>/tmp/tempfile1
    cp -f /tmp/tempfile1 $SRCGENPFILE
    chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP  $SRCGENPFILE

    # cp -f /tmp/tempfile1 $TARGET_ORACLE_HOME/dbs/init${SOURCE_ORACLE_SID}.ora_pfile_failback
    # chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP  $TARGET_ORACLE_HOME/dbs/init${SOURCE_ORACLE_SID}.ora_pfile_failback
}
	
##############Defining the functions########################
####Function:CreateSpfile
####Action:Creates spfile from pfile in case of cluster to standalone failover
####O/P:spfile gets created and located in ORACLE_HOME/dbs directory
###########################################################
CreateSpfile()
{
    echo "Creating Spfile from pfile..."
    log "Creating Spfile from pfile..."
    ORACLE_HOME=$TARGET_ORACLE_HOME
    ORACLE_SID=$SOURCE_ORACLE_SID
    ORACLE_USER=$TARGET_ORACLE_USER

    SpfileLocation=`grep SPFile= $SOURCECONFIGFILE | $AWK -F"=" '{ print $2 }' | sed -e "s/'//g"`
    if [ ! -z "$SpfileLocation" ]; then
        if [ "$SOURCE_IsASM" = "YES" ]; then
            echo $SpfileLocation | grep "^+" > /dev/null
            if [ $? -eq 0 ]; then
                log "spfile location $SpfileLocation is in ASM diskgroup."
            else
                log "spfile location $SpfileLocation is not in ASM diskgroup."
                if [ ! -f $SpfileLocation ]; then
                    spfileDir=`dirname $SpfileLocation`
                    spfileName=`basename $SpfileLocation`
                    if [ ! -d $spfileDir ]; then
                        echo "spfile location $spfileDir not found."
                        log "spfile location $spfileDir not found."
                        SpfileLocation=$TARGET_ORACLE_HOME/dbs/$spfileName
                        echo "using $SpfileLocation as spfile location."
                        log "using $SpfileLocation as spfile location."
                    fi
                fi
            fi
        else
            if [ ! -f $SpfileLocation ]; then
                spfileDir=`dirname $SpfileLocation`
                spfileName=`basename $SpfileLocation`
                if [ ! -d $spfileDir ]; then
                    echo "spfile location $spfileDir not found."
                    log "spfile location $spfileDir not found."
                    SpfileLocation=$TARGET_ORACLE_HOME/dbs/$spfileName
                    echo "using $SpfileLocation as spfile location."
                    log "using $SpfileLocation as spfile location."
                fi
            fi
        fi

        rm -f $tempfile1

        SRCGENPFILE=`echo $SRCGENPFILE | sed 's/\/\//\//g'`
     
        su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile1
        create spfile='$SpfileLocation' from pfile='$SRCGENPFILE';
_EOF"

        log "create spfile='$SpfileLocation' from pfile='$SRCGENPFILE'"
        echo "create spfile='$SpfileLocation' from pfile='$SRCGENPFILE' "

        cat $tempfile1|grep "File created">>/dev/null
        if [ $? -ne 0 ]; then
            echo "Unable to create Spfile..Hence exiting..!"
            log "Unable to create Spfile..Hence exiting..!"
            exit 1
        fi

        if [ -f $SpfileLocation ]; then
            log "chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $SpfileLocation"
            chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $SpfileLocation
        fi
    fi

    PfileLocation=`grep PFileLocation= $SOURCECONFIGFILE | $AWK -F"=" '{ print $2 }' | sed -e "s/'//g"`
    if [ ! -z "$PfileLocation" ]; then
        pfileDir=`dirname $PfileLocation`
        pfileName=init${ORACLE_SID}.ora

        if [ -d $pfileDir ]; then
            if [ "$pfileDir" != "${TARGET_ORACLE_HOME}/dbs" ] ; then
                PfileLocation=$TARGET_ORACLE_HOME/dbs/$pfileName
                echo "using $PfileLocation as pfile location."
                log "using $PfileLocation as pfile location."
            else
                log "$PfileLocation is same as source pfile location."
            fi
        else
            echo "pfile location $pfileDir not found."
            log "pfile location $pfileDir not found."
            PfileLocation=$TARGET_ORACLE_HOME/dbs/$pfileName
            echo "using $PfileLocation as pfile location."
            log "using $PfileLocation as pfile location."
        fi

        log "cp -f $SRCPFILE $PfileLocation"
        echo "cp -f $SRCPFILE $PfileLocation"
        cp -f $SRCPFILE $PfileLocation

        log  "chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $PfileLocation"
        echo "chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $PfileLocation"
        chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $PfileLocation
    fi
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
        grep local_listener= $SOURCECONFIGFILE > $NULL
        if [ $? -eq 0 ]; then
            listenerName=`grep local_listener= $SOURCECONFIGFILE | awk -F= '{print $2}'`
            grep "^$listenerName" $TARGET_ORACLE_HOME/network/admin/tnsnames.ora > $NULL
            if [ $? -ne 0 ]; then
                log "WARNING: local listener $listenerName not found in $TARGET_ORACLE_HOME/network/admin/tnsnames.ora"
                $ECHO "WARNING: local listener $listenerName not found in $TARGET_ORACLE_HOME/network/admin/tnsnames.ora"
            fi
        fi

        grep remote_listener= $SOURCECONFIGFILE > $NULL
        if [ $? -eq 0 ]; then
            listenerName=`grep remote_listener= $SOURCECONFIGFILE | awk -F= '{print $2}'`
            grep "^$listenerName" $TARGET_ORACLE_HOME/network/admin/tnsnames.ora > $NULL
            if [ $? -ne 0 ]; then
                log "WARNING: remote listener $listenerName not found in $TARGET_ORACLE_HOME/network/admin/tnsnames.ora"
                $ECHO "WARNING: remote listener $listenerName not found in $TARGET_ORACLE_HOME/network/admin/tnsnames.ora"
            fi
        fi
    fi

    log "EXIT CheckListeners"
}
##############Defining the functions########################
####Function:OracleStartup
####Action:starts the database
####O/P:Database comes to open state..
###########################################################
OracleStartup()
{
    echo "Starting the database instance.."
    log "Starting the database instance.."
    rm -f $tempfile1
    ORACLE_HOME=$TARGET_ORACLE_HOME
    ORACLE_SID=$SOURCE_ORACLE_SID
    ORACLE_USER=$TARGET_ORACLE_USER

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile1
    startup;
_EOF"

    ps -ef|grep -v "grep"|grep -w ora_pmon_${ORACLE_SID}>>/dev/null
    if [ $? -ne 0 ]
    then
        echo "Unable to start the database.."
        log "Unable to start the database.."
        echo "hence exiting.."
        log "hence exiting.."
        cat $tempfile1
        log "`$tempfile1`"
        exit 1
    fi
}
	
##############Defining the functions########################
####Function:MountDiskgroup
####Action:Mounts the Diskgroup
####O/P:Diskgroup comes to mounted state
###########################################################
MountDiskgroup()
{
    echo "Mounting diskgroup..."
    log "Mounting diskgroup..."
    log "Diskgroup=$DiskGroupName"
    ORACLE_HOME=$TARGET_ASM_HOME
    ORACLE_SID=$TARGET_ASM_SID
    ORACLE_USER=$TARGET_ORACLE_USER

    ASMUSER=sysdba
    echo $ASM_VER | awk -F"." '{print $1}' | grep 11 > /dev/null
    if [ $? -eq 0 ]; then
        release=`echo $ASM_VER | awk -F"." '{print $2}'`
        if [ $release -gt 1 ]; then
            ASMUSER=sysasm
        fi
    fi

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as $ASMUSER' << _EOF >$tempfile2
        alter diskgroup $DiskGroupName mount;
_EOF"
    CheckDiskgroupStatus

}

##############Defining the functions########################
####Function:CheckDiskgroupStatus
####Action:Checks the status of the Diskgroup
####O/P:Outputs the status of the diskgroup..
###########################################################
CheckDiskgroupStatus()
{
    ORACLE_HOME=$TARGET_ASM_HOME
    ORACLE_SID=$TARGET_ASM_SID
    ORACLE_USER=$TARGET_ORACLE_USER 		

    echo "Checking the status of the diskgroups..."
    log "Checking the status of the diskgroups..."

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile3
        select name, state from v\\\$asm_diskgroup where name='$DiskGroupName';
_EOF"

    grep "$DiskGroupName" $tempfile3 >>/dev/null
    if [ $? -eq 0 ]; then
        grep "DISMOUNTED" $tempfile3 >>/dev/null
        if [ $? -eq 0 ]
        then
            echo "Unable to Mount Diskgroup"
            log "Unable to Mount Diskgroup"
            exit 1
        else
            echo "Mounted Diskgroup $DiskGroupName succesfully.."
            log "Mounted Diskgroup $DiskGroupName succesfully.."
        fi
    else
        echo "Unable to Mount Diskgroup. Diskgroup not listed."
        log "Unable to Mount Diskgroup. Diskgroup $DiskGroupName not listed."
        exit 1
    fi
}
	
##############Defining the functions########################
####Function:DismountDiskgroup
####Action:Dismounts the Diskgroup
####O/P:Diskgroup comes to dismounted state
###########################################################
DismountDiskgroup()
{
    echo "Dismounting the diskgroup..."
    log "Dismounting the diskgroup..."
    ORACLE_HOME=$TARGET_ASM_HOME
    ORACLE_SID=$TARGET_ASM_SID
    ORACLE_USER=$TARGET_ORACLE_USER

    ASMUSER=sysdba
    echo $ASM_VER | awk -F"." '{print $1}' | grep 11 > /dev/null
    if [ $? -eq 0 ]; then
        release=`echo $ASM_VER | awk -F"." '{print $2}'`
        if [ $release -gt 1 ]; then
            ASMUSER=sysasm
        fi
    fi

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as $ASMUSER' <<_EOF >$tempfile2
    alter diskgroup $DiskGroupName dismount;
_EOF"

    grep "Diskgroup altered" $tempfile2 >>/dev/null
    if [ $? -ne 0 ]
    then
        echo "Unable to Dismount $DiskGroupName  Diskgroup"
        log "Unable to Dismount $DiskGroupName Diskgroup"
        cat $tempfile2
        echo "cannot perform Failover"
        log "cannot perform Failover"
        exit 1
    fi
}

##############Defining the functions########################
####Function:ASMLibActions
####Action:Scans,lists,Mounts the diskgroups..
####O/P:Diskgroup comes to Mounted state
###########################################################
ASMLibActions()
{
    echo "Performing ASM Disk group related tasks..."
    log "Performing ASM Disk group related tasks..."
    if [ ! -f /etc/init.d/oracleasm ]
    then
        echo "Error: ASMLib is not found."
        log "Error: ASMLib is not found."
        echo "Cannot perform Failover"
        log "Cannot perform Failover"
        exit 1
    else
        /etc/init.d/oracleasm status | grep no > /dev/null
        if [ $? -eq 0 ]; then
            echo "Error: ASMLib is not online"
            log "Error: ASMLib is not online"
            exit 1
        fi
        ##Chaning the ownership of asm devices to oracle's user id so that oracle can access those disks
        /etc/init.d/oracleasm scandisks > /dev/null
        /etc/init.d/oracleasm listdisks>>temp

        grep ASMLibDiskLabels $SOURCECONFIGFILE > /dev/null
        if [ $? -ne 0 ]; then
            echo "Error: ASMLibDiskLabels not found"
            log "Error: ASMLibDiskLabels not found in config file "
            exit 1
        fi

        if [ -z "$SOURCE_ASMDiskLabels" ]; then
            echo "Source ASM Disk lables are not found."
            log "Source ASM Disk lables are not found."
            exit 1
        fi

        asmLibLabels=`echo $SOURCE_ASMDiskLabels | sed 's/,/ /g'`
        for label in $asmLibLabels
        do
            if [ ! -z "$label" ]; then
                grep $label temp > /dev/null
                if [ $? -ne 0 ]; then
                    echo "Error: ASMLibLabel $label not found"
                    log "Error: ASMLibLabel $label not found"
                    exit 1
                fi
                chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP /dev/oracleasm/disks/$label
            fi
        done
    fi
}

GetAsmDiskAlias()
{
    echo "Performing asm disk alias device tasks..."
    log "Performing asm disk alias device tasks..."

    rm -f $tempfile1 $tempfile2 $tempfile5

    CXServerIP=`grep -i "^Hostname" $INMAGE_DIR/etc/drscout.conf|$AWK -F"=" '{ print $2 }' | sed "s/ //g"`
    CXPort=`grep -i "^Port" $INMAGE_DIR/etc/drscout.conf|$AWK -F"=" '{ print $2 }' | sed "s/ //g"`
    TARGET_HostID=`grep -i "^HostId" $INMAGE_DIR/etc/drscout.conf|$AWK -F"=" '{ print $2 }' | sed "s/ //g"`
    log " CXServerIP=$CXServerIP CXPort=$CXPort TARGET_HostID=$TARGET_HostID"

    >TgtAsmAliasMapping
    >SrcTgtMapping

    for tgtVol in `echo $TARGET_DEVICES|sed 's/,/ /g'`
    do
        echo "Determining source devices for target device $tgtVol.."
        log  "Determining source devices for target device $tgtVol.."

        GetSourceDevice $tgtVol
        SourceDevice=`grep "source_devicename" $tempfile6|$AWK -F">" '{ print $2 }'|$AWK -F"<" '{print $1}'|uniq`
        if [ -z "$SourceDevice" ]; then
            echo "Failed to fetch Source device name for $tgtVol using cxcli."
            log "Failed to fetch Source device name for $tgtVol using cxcli."
            exit 1
        fi
        echo "$SourceDevice $tgtVol" >> SrcTgtMapping
    done

    for diskAlias in $SOURCE_DiskAliases
    do
        log "Source alias : $diskAlias"

        SourceDisk=`echo $diskAlias | $AWK -F":" '{print $1}'`
        Alias=`echo $diskAlias | $AWK -F":" '{print $2}'`

        TargetDevice=`grep -w $SourceDisk SrcTgtMapping | $AWK '{print $2}'` 

        log "Target Alias : $Alias $TargetDevice"

        echo "$Alias $TargetDevice" >> TgtAsmAliasMapping
    done


    rm -f $tempfile2 > $NULL

    return
}

ASMLibDeviceActions()
{
    echo "Performing asmlib device tasks..."
    log "Performing asmlib device tasks..."

    if [ "$OSTYPE" = "Linux" ]; then
        for device in `echo $TARGET_DEVICES|sed 's/,/ /g'`
        do
            case $device in
                */dev/mapper/*)
                    devname=`echo $device | awk -F/ '{print $NF}'`
					
		    multipath -ll | grep -w $devname > /dev/null
		    if [ $? -ne 0 ];then
                        continue
                    fi

                    $ORACLE_HOME/bin/kfed op=read dev=$device > /dev/null
                    if [ $? -eq 0 ];then
                        continue
                    fi

		    partition1=${device}_part1
                    partition=${device}p1

                    if [ -e $partition ]; then
                        if [ -b $partition ]; then
                            echo "partition $partition found."
                            log "partition $partition found."
                            continue
                        fi
                    elif [ -e $partition1 ];then
                        if [ -b $partition1 ]; then
                            echo "partition $partition1 found."
                            log "partition $partition1 found."
                            continue
                        fi
                    fi

                    KPARTX=/sbin/kpartx
                    if [ ! -f $KPARTX ] ; then
                        echo "$KPARTX : file not found."
                        log "$KPARTX : file not found."
                        exit 1
                    fi

                    echo "partition $partition or $partition1 not found."
                    log "partition $partition or $partition1 not found."

                    echo "$KPARTX -a $device"
                    log "$KPARTX -a $device"
                    $KPARTX -a $device
                    if [ $? -eq 0 ]; then
                        if [ -e $partition ]; then
                            if [ -b $partition ]; then
                                echo "partition $partition found."
                                log "partition $partition found."
                                continue
                            fi
                        elif [ -e $partition1 ];then
			      if [ -b $partition1 ]; then
				  echo "partition $partition1 found."
				  log "partition $partition1 found."
			 	  continue
			      fi
                        fi

                        echo "partition device not created by $KPARTX"
                        log "partition device not created by $KPARTX"
                        exit 1
                    else
                        echo "$KPARTX returned error. "
                        log "$KPARTX returned error. "
                        exit 1
                    fi
                    ;;
                *)
                    ;;
            esac
        done
    fi
}

ASMRawActions()
{
    echo "Performing raw device related tasks..."
    log "Performing raw device related tasks..."
    for device in `echo $TARGET_DEVICES|sed 's/,/ /g'`
    do
        if [ "$OSTYPE" = "SunOS" ]; then

            case $device in
                */dev/vx/dsk/*)
                    dir=`echo $device | awk -Fc '{print $1}'`
                    partition=`echo $device | awk -F/ '{print $NF}' | awk -Fs '{print $1}'`
                    allPartitions="${dir}${partition}s*"
                    rawPartitions=`echo $allPartitions | sed -e 's/dsk/rdsk/g'`
                    ;;
                */dev/vx/dmp/*)
                    dir=`echo $device | awk -Fc '{print $1}'`
                    partition=`echo $device | awk -F/ '{print $NF}' | awk -Fs '{print $1}'`
                    allPartitions="${dir}${partition}s*"
                    rawPartitions=`echo $allPartitions | sed -e 's/dmp/rdmp/g'`
                    ;;
                */dev/dsk/*)
                    partition=`echo $device | awk -F/ '{print $NF}' | awk -Fs '{print $1}'`
                    allPartitions="/dev/dsk/${partition}s*"
                    rawPartitions=`echo $allPartitions | sed -e 's/dsk/rdsk/g'`
                    ;;
                */dev/did/dsk/*)
                    partition=`echo $device | awk -F/ '{print $NF}' | awk -Fs '{print $1}'`
                    allPartitions="/dev/did/dsk/${partition}s*"
                    rawPartitions=`echo $allPartitions | sed -e 's/dsk/rdsk/g'`
                    ;;
                *)
                    echo "Error: unsupported raw device $device"
                    log "Error: unsupported raw device $device"
                    exit 1
                    ;;
            esac

            chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $allPartitions
            chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $rawPartitions

        elif [ "$OSTYPE" = "AIX" ]; then

            case $device in
                */dev/hdisk*)
                    rawDevice=`echo $device | sed -e 's/hdisk/rhdisk/g'`
                    ;;
                *)
                    echo "Error: unsupported raw device $device"
                    log "Error: unsupported raw device $device"
                    exit 1
                    ;;
            esac

            chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $rawDevice

        elif [ "$OSTYPE" = "Linux" ]; then

            log "Linux ASMraw device : $device"

            case $device in
            */dev/sd*)
                ;;
            */dev/mapper/*)
                ;;
            *)
                echo "Error: unsupported raw device $device"
                log "Error: unsupported raw device $device"
                ;;
            esac

            chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $device
        else

            echo "Error: Not supported on $OSTYPE"
            log "Error: Not supported on $OSTYPE"
            exit 1

        fi
    done

    for device in `cat TgtAsmAliasMapping | awk '{print $1}'`
    do
        chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $device
    done
}


ASMActions()
{
    ORACLE_HOME=$TARGET_ASM_HOME
    rm -f $tempfile1 $tempfile2 $tempfile3 $tempfile4
    ORACLE_SID=$TARGET_ASM_SID
    ORACLE_USER=$TARGET_ORACLE_USER

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

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile1
                    select name,state from v\\\$asm_diskgroup;
_EOF"

    diskGroups=`echo $SOURCE_ASMDGS|sed -e 's/,/ /g'`
    for diskgroup in $diskGroups
    do
        DiskGroupName=$diskgroup
        if [ ! -z "$DiskGroupName" ]
        then
            grep -w $DiskGroupName $tempfile1 >>/dev/null
            if [ $? -eq 0 ]
            then
                grep -w $DiskGroupName $tempfile1 | grep -w  "DISMOUNTED" >>/dev/null
                if [ $? -eq 0 ]; then
                    echo "Diskgroup $DiskGroupName not mounted"
                    log "Diskgroup $DiskGroupName not mounted"
                    MountDiskgroup
                else
                    echo "Diskgroup $DiskGroupName already mounted"
                    log "Diskgroup $DiskGroupName already mounted"
                fi
            else
                echo "Diskgroup is not present"
                log "Diskgroup $DiskGroupName is not present"
                echo "Mounting diskgroup $DiskGroupName"
                log "Mounting diskgroup $DiskGroupName"
                MountDiskgroup
            fi
        fi
    done 	
}

##############Defining the functions########################
####Function:RollbackVolumes
####Action:Rolls back to the consistent point specified
####O/P:the Volumes get mounted with the consistent data
###########################################################
RollbackVolumes()
{
    if [ "$recoveryPointType" = "TAG" ]; then

        devices=`echo $TARGET_DEVICES|sed 's/,/;/g'`
        echo "Rolling back devices: $devices"
        log "Rolling back devices: $devices selected TAG"

        log  "$INMAGE_DIR/bin/cdpcli --rollback --rollbackpairs="$devices" --event=$tag --picklatest --deleteretentionlog=yes"
        echo "$INMAGE_DIR/bin/cdpcli --rollback --rollbackpairs="$devices" --event=$tag --picklatest --deleteretentionlog=yes"
        $INMAGE_DIR/bin/cdpcli --rollback --rollbackpairs="$devices" --event=$tag --picklatest --deleteretentionlog=yes
        if [ $? -ne 0 ]
        then
            echo "Rollback for: $devices   Failed"
            log "Rollback for: $devices   Failed"
            echo "Unable to Rollback the volume...Can't proceed"
            log "Unable to Rollback the volume...Can't proceed"
            echo "Exiting..."
            log "Exiting..."
            exit 1
        else
            echo "Rollback for: $devices Done"
            log "Rollback for: $devices  Done"
        fi

    elif [ "$recoveryPointType" = "LATEST" ]; then

        devices=`echo $TARGET_DEVICES|sed 's/,/;/g'`
        echo "Rolling back for: $devices"
        log "Rolling back for: $devices  selected LATEST"
        log  "$INMAGE_DIR/bin/cdpcli --rollback --rollbackpairs="$devices" --force=yes --recentappconsistentpoint --app=FS --deleteretentionlog=yes"
        echo "$INMAGE_DIR/bin/cdpcli --rollback --rollbackpairs="$devices" --force=yes --recentappconsistentpoint --app=FS --deleteretentionlog=yes"
        $INMAGE_DIR/bin/cdpcli --rollback --rollbackpairs="$devices" --force=yes --recentappconsistentpoint --app=FS --deleteretentionlog=yes
        if [ $? -ne 0 ]
        then
            echo "Rolling back for: $TARGET_DEVICES Failed"
            log "Rolling back for: $TARGET_DEVICES Failed"
            echo "Unable to Roll back the volume...Can't proceed"
            log "Unable to Roll back the volume...Can't proceed"
            echo "Exiting..."
            log "Exiting..."
            exit 1
        else
            echo "Rolling back for: $TARGET_DEVICES Done"
            log  "Rolling back for: $TARGET_DEVICES Done"
        fi

    elif [ "$recoveryPointType" = "LATESTTIME" ]; then

        devices=`echo $TARGET_DEVICES|sed 's/,/;/g'`
        echo "Rolling back for: $devices"
        log "Rolling back for: $devices  selected LATESTTIME"
        log  "$INMAGE_DIR/bin/cdpcli --rollback --rollbackpairs="$devices" --recentcrashconsistentpoint --deleteretentionlog=yes"
        echo "$INMAGE_DIR/bin/cdpcli --rollback --rollbackpairs="$devices" --recentcrashconsistentpoint --deleteretentionlog=yes"
        $INMAGE_DIR/bin/cdpcli --rollback --rollbackpairs="$devices" --recentcrashconsistentpoint --deleteretentionlog=yes
        if [ $? -ne 0 ]
        then
            echo "Rolling back for: $TARGET_DEVICES      Failed."
            log "Rolling back for: $TARGET_DEVICES      Failed."
            echo "Unable to Roll back the volume...Can't proceed"
            log "Unable to Roll back the volume...Can't proceed"
            echo "Exiting..."
            exit 1
        else
            echo "Rolling back for: $TARGET_DEVICES     Done."
            log "Rolling back for: $TARGET_DEVICES     Done."
        fi

    elif [ "$recoveryPointType" = "CUSTOM" ]; then

        for device_tag in `echo $tag |sed 's/ /#/g' | sed 's/,/ /g'`
        do
            device=`echo $device_tag | awk -F: '{print $1}'`
            rptype=`echo $device_tag | awk -F: '{print $2}'`

            if [ ! -z "$device" ]; then

                if [ "$rptype" = "TAG" ]; then
                    devtag=`echo $device_tag | awk -F: '{ for(i=3;i<NF;i++){ if(i!=NF-1) { print $i":"} else print $i}}'`
                    devtag=`echo $devtag | sed 's/ //g'`
                    tagtype=`echo $device_tag | awk -F: '{print $NF}'`

                    echo "Rolling back $device to tag $devtag of type $tagtype"
                    log "Rolling back $device to tag $devtag of type $tagtype"

                    echo "$INMAGE_DIR/bin/cdpcli --rollback --dest=$device --force=yes --event=$devtag --picklatest --app=$tagtype --deleteretentionlog=yes"
                    log  "$INMAGE_DIR/bin/cdpcli --rollback --dest=$device --force=yes --event=$devtag --picklatest --app=$tagtype --deleteretentionlog=yes"
                    $INMAGE_DIR/bin/cdpcli --rollback --dest=$device --force=yes --event=$devtag --picklatest --app=$tagtype --deleteretentionlog=yes
                elif [ "$rptype" = "TIME" ]; then
                    devtag=`echo $device_tag | awk -F: '{ for(i=3;i<=NF;i++){ if(i!=NF) { print $i":"} else print $i}}'`
                    devtag=`echo $devtag | sed 's/ //g'`
                    devtag=`echo $devtag | sed 's/#/ /g'`

                    echo "Rolling back $device to time $devtag"
                    log "Rolling back $device to time $devtag"

                    echo "$INMAGE_DIR/bin/cdpcli --rollback --dest=$device --at=$devtag --deleteretentionlog=yes"
                    log  "$INMAGE_DIR/bin/cdpcli --rollback --dest=$device --at=$devtag --deleteretentionlog=yes"
                    $INMAGE_DIR/bin/cdpcli --rollback --dest=$device --at="$devtag" --deleteretentionlog=yes
                fi

            else
                echo "Rolling back failed for $device_tag as device name is missing."
                log "Rolling back failed for $device_tag as device name is missing."
                exit 1
            fi

            if [ $? -ne 0 ]
            then
                echo "Rolling back for: $device   Failed"
                log "Rolling back for: $device   Failed"
                echo "Unable to Roll back the volume...Can't proceed"
                log "Unable to Roll back the volume...Can't proceed"
                echo "Exiting..."
                log "Exiting..."
                exit 1
            else
                echo "Rolling back for: $device   Done"
                log "Rolling back for: $device   Done"
            fi
        done
    fi

}


##############Defining the functions########################
####Function:StopVxAgent
####Action:This function will stop vxagent
####O/P:"Stoping information of vxagent" command o/p on terminal
StopVxAgent()
{
    echo "Stopping Vx agent...."
    #/etc/init.d/vxagent stop
    $INMAGE_DIR/bin/stop
    echo " "
    echo "Vx agent stopped..."
}

##############Defining the functions########################
####Function:StartVxAgent
####Action:This function will stop vxagent
####O/P:"Startting information of vx agent " command o/p on terminal
StartVxAgent()
{
    echo "Starting Vx agent...."
    #/etc/init.d/vxagent start
    $INMAGE_DIR/bin/start
    echo " "
    echo "Vx agent started..."
}

##############Defining the functions########################
####Function:AddEntryInOratabFile
####Action:This function will add entries in oratab file
####O/P:Oratab file contains the database details.. 
AddEntryInOratabFile()
{
    echo "Checking $ORATABFILE file..."
    log "Checking $ORATABFILE file..."
    if [ -f $ORATABFILE ]
    then
        if [ "$TARGET_IsCluster" = "YES" ]; then
            grep -v "^#" $ORATABFILE | grep -v "^*:" | grep -w "$SOURCE_DBNAME" >>/dev/null
        else
            grep -v "^#" $ORATABFILE | grep -v "^*:" | grep -w "$SOURCE_ORACLE_SID" >>/dev/null
        fi
        if [ $? -ne 0 ]
        then
            echo "Adding entry in $ORATABFILE file"
            log "Adding entry in $ORATABFILE file"
            if [ "$TARGET_IsCluster" = "YES" ]; then
                echo "$SOURCE_DBNAME:$TARGET_ORACLE_HOME:N" >>$ORATABFILE
            else
                echo "$SOURCE_ORACLE_SID:$TARGET_ORACLE_HOME:N" >>$ORATABFILE
            fi
            echo "Entry added $ORATABFILE file"
            log "Entry added $ORATABFILE file"
        else
            if [ "$TARGET_IsCluster" = "YES" ]; then
                echo "$SOURCE_DBNAME already present in $ORATABFILE file"
            else
                echo "$SOURCE_ORACLE_SID already present in $ORATABFILE file"
            fi
        fi
    else
        echo "$ORATABFILE file doesn't exist."
        log "$ORATABFILE file doesn't exist."
        echo "Please create $ORATABFILE ..."
        log "Please create $ORATABFILE ..."
        exit 1
    fi
}

##############Defining the functions########################
####Function:CreatingDirectories
####Action:This function will create directories required to start database
####O/P:All the necessary directories will be created with ownership changes
CreatingDirectories()
{
    PFILE=$SRCGENPFILE
    if [ -f $PFILE ]
    then
        chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $PFILE
    else
        echo "Pfile ( $PFILE ) doesn't exist"
        log "Pfile ( $PFILE ) doesn't exist"
        echo "Can't proceed without pfile to start the database..."
        log "Can't proceed without pfile to start the database..."
        echo "Exiting.."
        exit 1
    fi

    ADUMP=`grep "audit_file_dest" $PFILE|$AWK -F"'" '{ print $2 }'`
    BDUMP=`grep "background_dump_des" $PFILE|$AWK -F"'" '{ print $2 }'`
    CDUMP=`grep "core_dump_dest" $PFILE|$AWK -F"'" '{ print $2 }'`
    UDUMP=`grep "user_dump_dest" $PFILE|$AWK -F"'" '{ print $2 }'`
    RDEST=`grep "db_recovery_file_dest" $PFILE|$AWK -F"'" '{ print $2 }'`

    #CTRLFILE=`grep "control_files" $PFILE|$AWK -F"'" '{ print $2 }'`
    #if [ ! -z "$CTRLFILE" ]
    #then
        #CTRLDIR=`dirname $CTRLFILE>>/dev/null`
    #fi

    ARCHDEST=`grep "log_archive_dest_1" $PFILE|$AWK -F"'" '{ print $2 }'|$AWK -F"=" '{ print $2 }'`
    if [ ! -z "$ADUMP" ]
    then
        mkdir -p $ADUMP 
        chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $ADUMP 
    fi
    if [ ! -z "$BDUMP" ]
    then
        mkdir -p $BDUMP 
        chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $BDUMP
    fi
    if [ ! -z "$CDUMP" ]
    then
        mkdir -p $CDUMP
        chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $CDUMP
    fi
    if [ ! -z "$UDUMP" ]
    then
        mkdir -p $UDUMP 
        chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $UDUMP
    fi

    echo $RDEST|grep "+" >>/dev/null
    if  [ $? -ne 0 ] && [ ! -z "$RDEST" ]
    then
        mkdir -p $RDEST 
        chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $RDEST
    fi

    #echo $CTRLDIR|grep "+"  >>/dev/null
    #if  [ $? -ne 0 ] && [ ! -z "$CTRLDIR" ]
    #then
        #mkdir -p $CTRLDIR 
        #chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $CRTLDIR
    #fi

    echo $ARCHDEST |grep "+" >>/dev/null
    if  [ $? -ne 0 ] && [ ! -z "$ARCHDEST" ]
    then
        mkdir -p $ARCHDEST
        chown $TARGET_ORACLE_USER:$TARGET_ORACLE_GROUP $ARCHDEST
    fi
}


EnableDiscovery()
{
    tempfile1=/tmp/appwizconf.$$

    if [ "$TARGET_IsCluster" = "YES" ]; then
        newCfgEntry="OracleDatabase=YES:$SOURCE_DBNAME:$TARGET_ORACLE_HOME"
    else
        newCfgEntry="OracleDatabase=YES:$SOURCE_ORACLE_SID:$TARGET_ORACLE_HOME"
    fi


    if [ "$TARGET_IsCluster" = "YES" ]; then
        grep OracleDatabase= $APP_CONF_FILE | grep -w $SOURCE_DBNAME > /dev/null
    else
        grep OracleDatabase= $APP_CONF_FILE | grep -w $SOURCE_ORACLE_SID > /dev/null
    fi

    if [ $? -eq 0 ]; then
        if [ "$TARGET_IsCluster" = "YES" ]; then
            cfgEntry=`grep OracleDatabase= $APP_CONF_FILE | grep -w $SOURCE_DBNAME`
        else
            cfgEntry=`grep OracleDatabase= $APP_CONF_FILE | grep -w $SOURCE_ORACLE_SID`
        fi
        grep -v $cfgEntry $APP_CONF_FILE > $tempfile1
        mv -f $tempfile1 $APP_CONF_FILE
    fi

    echo $newCfgEntry >> $APP_CONF_FILE
    return 
}


##############Defining the functions########################
####Function:CallingCorrespondingOS
####Action:Calling Corresponding Function based on the arguements
####O/P:Assigning variables with their corresponding values
###########################################################
CallingCorrespondingOS()
{	        
    OSTYPE=`uname -s`
    case $OSTYPE in
        HP-UX )
            df_command="bdf"
            FSTABFILE="/etc/fstab"
        ;;
        Linux )
            df_command="df"
            FSTABFILE="/etc/fstab"
            ORATABFILE="/etc/oratab"
            AWK="/usr/bin/awk"
            MOUNT_ARG="t"
        ;;
        SunOS )
            df_command="df"
            FSTABFILE="/etc/vfstab"
            ORATABFILE="/var/opt/oracle/oratab"
            AWK="/usr/xpg4/bin/awk"
            MOUNT_ARG="F" 
        ;;
        AIX )
            df_command="df"
            FSTABFILE="/etc/vfstab"
            ORATABFILE="/etc/oratab"
            AWK="/usr/bin/awk"
            MOUNT_ARG="v" 
        ;;
    esac
}

##############Defining the functions########################
####Function:ReadSourceConfigurationFile
####Action:This function Reads the values from the configuration file provided with 
####O/P:Assigning variables with their corresponding values
###########################################################
ReadSourceConfigurationFile()
{

    SOURCE_HOSTNAME=`grep "MachineName=" $SOURCECONFIGFILE|$AWK -F"=" '{print $2}'`
    if [ -z "$SOURCE_HOSTNAME" ]
    then 
        echo "Source HostName is Null in Configuration File"
        log "Source HostName is Null in Configuration File"
        echo "Cannot Perform Failover"
        exit 1
    fi

    SOURCE_HostID=`grep "HostID=" $SOURCECONFIGFILE|$AWK -F"=" '{print $2}'`
    if [ -z "$SOURCE_HostID" ]
    then
        echo "Source HostId is Null in Configuration File"
        log "Source HostId is Null in Configuration File"
        echo "Cannot Perform Failover"
        exit 1
    fi

    SOURCE_DBNAME=`grep "OracleDBName=" $SOURCECONFIGFILE|$AWK -F"=" '{print $2}'`
    if [ -z "$SOURCE_DBNAME" ]
    then
        echo "Database name is Null in Source Configuration File"
        log "Database name is Null in Source Configuration File"
        echo "Cannot Perform Failover"
        exit 1
    fi

    SOURCE_ORACLE_HOME=`grep "OracleHome=" $SOURCECONFIGFILE|$AWK -F"=" '{print $2}'`
    if [ -z "$SOURCE_ORACLE_HOME" ]
    then
        echo "ORACLE HOME is Null in Source Configuration File"
        log "ORACLE HOME is Null in Source Configuration File"
        echo "Cannot Perform Failover"
        exit 1
    fi

    SOURCE_ORACLE_SID=`grep "OracleSID" $SOURCECONFIGFILE|$AWK -F"=" '{print $2}'`
    if [ -z "$SOURCE_ORACLE_SID" ]
    then
        echo "ORACLE SID is Null in Source Configuration File"
        log "ORACLE SID is Null in Source Configuration File"
        echo "Cannot Perform Failover"
        exit 1
    fi

    SOURCE_ORACLE_VERSION=`grep "DBVersion=" $SOURCECONFIGFILE|$AWK -F"=" '{print $2}'`
    if [ -z "$SOURCE_ORACLE_VERSION" ]
    then
        echo "DBVersion is Null in Source Configuration File"
        log "DBVersion is Null in Source Configuration File"
        echo "Cannot Perform Failover"
        exit 1
    fi

    SOURCE_IsCluster=`grep "isCluster=" $SOURCECONFIGFILE|$AWK -F"=" '{print $2}'`
    if [  -z "$SOURCE_IsCluster" ]
    then
        echo "SOURCE_IsCluster is Null in Source Configuration File"
        log "SOURCE_IsCluster is Null in Source Configuration File"
        echo "Cannot Perform Failover"
        exit 1
    fi

    SOURCE_CLUSTER_NAME=`grep "ClusterName=" $SOURCECONFIGFILE|$AWK -F"=" '{print $2}'`
    SOURCE_CLUSTER_TOTALNODES=`grep "TotalClusterNodes=" $SOURCECONFIGFILE|$AWK -F"=" '{print $2}'`

    SOURCE_IsASM=`grep "isASM=" $SOURCECONFIGFILE| uniq | $AWK -F"isASM=" '{print $2}'`
    SOURCE_IsASMLib=`grep "isASMLib=" $SOURCECONFIGFILE| uniq |$AWK -F"=" '{print $2}'`
    SOURCE_IsASMRaw=`grep "isASMRaw=" $SOURCECONFIGFILE|uniq | $AWK -F"=" '{print $2}'`
    SOURCE_ASMDGS=`grep "DiskGroupName=" $SOURCECONFIGFILE|$AWK -F"=" '{print $2}'`
    SOURCE_ASMDiskLabels=`grep "ASMLibDiskLabels=" $SOURCECONFIGFILE|$AWK -F"=" '{print $2}'`

    SOURCE_IsVxvm=`grep "isVxvm=" $SOURCECONFIGFILE|uniq | $AWK -F"=" '{print $2}'`
    SOURCE_IsVxfs=`grep "isVxfs=" $SOURCECONFIGFILE|uniq | $AWK -F"=" '{print $2}'`
    SOURCE_IsVxvmRaw=`grep "isVxvmRaw=" $SOURCECONFIGFILE|uniq | $AWK -F"=" '{print $2}'`
    SOURCE_VxvmDgName=`grep "VxvmDgName=" $SOURCECONFIGFILE|uniq|$AWK -F"=" '{print $2}'`

    SOURCE_IsOcfs2=`grep "isOcfs2=" $SOURCECONFIGFILE|uniq | $AWK -F"=" '{print $2}'`
    SOURCE_IsZfs=`grep "isZfs=" $SOURCECONFIGFILE|uniq | $AWK -F"=" '{print $2}'`

    SOURCE_MOUNTPOINTS=`grep "MountPoint=" $SOURCECONFIGFILE| uniq |$AWK -F"=" '{print $2}'`
    SOURCE_VGS=`grep "VolumeGroup=" $SOURCECONFIGFILE | uniq |$AWK -F"=" '{print $2}'`
    SOURCE_DiskAliases=`grep "DiskAlias=" $SOURCECONFIGFILE | uniq |$AWK -F"=" '{print $2}'`

    #if [ "$SOURCE_IsCluster" = "YES" ]
    #then 
        #if [ "$SOURCE_CLUSTER_NAME" = "" -o "$SOURCE_CLUSTER_TOTALNODES" = "" ]
        #then 
            #echo " Cluster name nodes is not valid"
            #log " Cluster name nodes is not valid"
            #echo "Cannot perform Failover"
            #exit 1
        #fi
    #fi

    if [ "$SOURCE_IsASM" = "YES" ]
    then
        if [ -z "$SOURCE_ASMDGS" ]
        then
            echo "ASM DiskGroup Information is not provided"
            log "ASM DiskGroup Information is not provided"
            echo "Cannot Perform Failover"
            exit 1
        fi

        if [ "$SOURCE_IsASMRaw" = "YES" ]
        then
            SOURCE_DATABASE_TYPE=ASMRaw
        elif [ "$SOURCE_IsASMLib" = "YES" ]
        then
            SOURCE_DATABASE_TYPE=ASMLib
        else
            echo "Neither of ASMLib or ASMRaw is Yes"
            log "Neither of ASMLib or ASMRaw is Yes"
            echo "Cannot perform Failover"
            exit 1
        fi

    elif [ "$SOURCE_IsVxvm" = "YES" ]
    then
        if [ "$SOURCE_IsVxvmRaw" = "YES" ]
        then
            SOURCE_DATABASE_TYPE=VXVMRaw
        else
            if [ "$CLUSTER" = "yes" -o "$OSTYPE" = "AIX" ]; then
                SOURCE_DATABASE_TYPE=VXVM
            else
                SOURCE_DATABASE_TYPE=DATABASE_ON_LOCAL_FILE_SYSTEM
            fi
            if [ -z "$SOURCE_MOUNTPOINTS" ]
            then
                echo "Mount points are not found for Vxfs."
                log "Mount points are not found for Vxfs."
                echo "Cannot perform Failover"
                log "Cannot perform Failover"
                exit 1
            fi
        fi
    elif [ "$SOURCE_IsOcfs2" = "YES" ] ; then
        if [ "$SOURCE_IsCluster" = "YES" ]; then
            SOURCE_DATABASE_TYPE=DATABASE_ON_LOCAL_FILE_SYSTEM
        else
            SOURCE_DATABASE_TYPE=DATABASE_ON_LOCAL_FILE_SYSTEM
        fi

        if [ -z "$SOURCE_MOUNTPOINTS" ]
        then
            echo "Mount points are not found for Vxfs."
            log "Mount points are not found for Vxfs."
            echo "Cannot perform Failover"
            log "Cannot perform Failover"
            exit 1
        fi
    elif [ "$SOURCE_IsZfs" = "YES" ];then
          SOURCE_DATABASE_TYPE=DATABASE_ON_ZFS_FILE_SYSTEM
    else
        if [ "$SOURCE_IsCluster" = "YES" ]
        then
            echo "Source cluster is not based on either ASM or VCS."
            log "Source cluster is not based on either ASM or VCS."
            echo "Cannot perform Failover"
            log "Cannot perform Failover"
            exit 1
        else
            SOURCE_DATABASE_TYPE=DATABASE_ON_LOCAL_FILE_SYSTEM
        fi
    fi

    if [ "$SOURCE_IsCluster" = "YES" ]
    then
        echo "Identified the source database as.. CLUSTER_"$SOURCE_DATABASE_TYPE
        log "Identified the source database as.. CLUSTER_"$SOURCE_DATABASE_TYPE
    else
        echo "Identified the source database as.. STANDALONE_"$SOURCE_DATABASE_TYPE
        log "Identified the source database as.. STANDALONE_"$SOURCE_DATABASE_TYPE
    fi
}

CallReadSourceConfigFile()
{
    ReadSourceConfigurationFile
}

GetClusterInformation()
{
    CRSD_PATH=`ps -ef|grep crsd.bin|grep -v grep |awk '{ print $(NF-1) }' | uniq`

    if [ ! -z "${CRSD_PATH}" ]; then
        CRSD_DIRECTORY=`dirname ${CRSD_PATH}`
        export CRSD_DIRECTORY

        CLUSTER_CMD=`ps -ef|grep crsd.bin|grep -v grep |awk '{ print $(NF-1) }'| uniq | sed 's/crsd\.bin/cemutlo \-n/'`

        if [ -z "$CLUSTER_CMD" ]; then
            echo "Error: cluster command not found"
            log "Error: cluster command not found"
            exit 1 
        fi

        TARGET_CLUSTER_NAME=`$CLUSTER_CMD`
        if [ -z "$TARGET_CLUSTER_NAME" ]; then
            echo "Error: cluster name not found"
            log "Error: cluster name not found"
            exit 1 
        fi

        TARGET_IsCluster="YES"

        export CLUSTER_NAME
        log "Cluster Name=$CLUSTER_NAME"

        TARGET_CLUSTER_NODENAME=`${CRSD_DIRECTORY}/olsnodes -l`
        log "TARGET_CLUSTER_NODENAME=$TARGET_CLUSTER_NODENAME"

        TARGET_OTHER_CLUSTER_NODES=`${CRSD_DIRECTORY}/olsnodes | grep -v $TARGET_CLUSTER_NODENAME | tr -s '\n' ','`
        log "TARGET_OTHER_CLUSTER_NODES=$TARGET_OTHER_CLUSTER_NODES"

        TARGET_CLUSTER_TOTALNODES=`${CRSD_DIRECTORY}/olsnodes | wc -l`
        log "TARGET_CLUSTER_TOTALNODES=$TARGET_CLUSTER_TOTALNODES"

        TARGET_SID_POSTFIX=`${CRSD_DIRECTORY}/olsnodes -n -l | awk '{print $2}'`
    fi

    return 
}

GetAsmInformation()
{
    if [ -f $ORATABFILE ]
    then
        TARGET_ASM_HOME=`cat $ORATABFILE | grep -v "^#" | grep -v "^*:" | grep "+ASM"|$AWK -F":" '{ print $2 }'|head -1`
        log "TARGET_ASM_HOME=$TARGET_ASM_HOME"
    fi

    if [ -z "$TARGET_ASM_HOME" ]; then
        echo "Oracle home for ASM is not found."
        log "Oracle home for ASM is not found."
        exit 1
    fi

    if [ "$TARGET_IsCluster" = "YES" ]; then
        # TARGET_ASM_SID="+ASM$TARGET_SID_POSTFIX"
        TARGET_ASM_SID=`ps -ef | grep asm_pmon |  grep "+ASM" | grep -v grep | awk '{ print $NF}' |  sed -e 's/asm\_pmon\_//'`
        if [ -z "$TARGET_ASM_SID" ]; then
            echo "Oracle ASM is not online."
            log "Oracle ASM $TARGET_ASM_SID is not online."
            exit 1
        fi
    else
        TARGET_ASM_SID=`cat $ORATABFILE | grep -v "^#" | grep -v "^*:" | grep "+ASM"|$AWK -F":" '{ print $1 }'|head -1`
        if [ -z "$TARGET_ASM_SID" ]; then
            log "Oracle ASM entry not found in $ORATABFILE."
            TARGET_ASM_SID="+ASM"
        fi
    fi

}

GetTargetConfig()
{
    # get the oracle home compatible with SOURCE_ORACLE_VERSION
    # for each of the OracleDatabases in appwizard.conf check
    # if the DBVersion is same as the source, use that ORACLE_HOME

    isOracleHomeFound=0
    isRacSupported=0

    if [ "$OSTYPE" = "Linux" -o "$OSTYPE" = "AIX" ]; then
        inventory_loc=`grep "inventory_loc" /etc/oraInst.loc | awk -F= '{print $2}'`
    elif [ "$OSTYPE" = "SunOS" ]; then
         inventory_loc=`grep "inventory_loc" /var/opt/oracle/oraInst.loc | awk -F= '{print $2}'`
    fi

    oracleDbHomes=`grep -w "LOC=" $inventory_loc/ContentsXML/inventory.xml | grep -v "CRS=\"true\"" | awk '{print $3}' |awk -F= '{print $2}'|sed 's/"//g'`

    if [ ! -z "$oracleDbHomes" ]; then
        for oracleHome in $oracleDbHomes
        do
            ORACLE_HOME=`echo $oracleHome`
            export ORACLE_HOME
            TARGET_ORACLE_VERSION=`strings $ORACLE_HOME/bin/oracle | grep NLSRTL | awk '{print $3}'`

            if [ "$TARGET_ORACLE_VERSION" = "$SOURCE_ORACLE_VERSION" ]; then
                #if [ "$SOURCE_IsCluster" = "YES" ] ; then
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
                            echo "$ORACLE_HOME is not RAC enabled."
                            log "$ORACLE_HOME is not RAC enabled."
                        else
                            #isOracleHomeFound=1
                            #break
                            isRacSupported=1
                        fi
                    else
                        log "Error: File $NM not found. RAC compatibility could not be verified."
                        #exit 1
                    fi
                #fi

                isOracleHomeFound=1
                break

            fi
        done
    fi


    if [ $isOracleHomeFound -eq 0 ]; then
        echo "Error: Compatible ORACLE_HOME not found for version $SOURCE_ORACLE_VERSION." 
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
        echo "Error: Oracle user or group not valid." 
        exit 1
    fi

    # check if this node is part of a cluster
    TARGET_IsCluster="NO"
    if [ "$CLUSTER" = "yes" ]; then
        #GetClusterInformation
        if [ $isRacSupported -eq 1 ]; then
            TARGET_IsCluster="YES"
            TARGET_CLUSTER_TOTALNODES=`grep .instance_number= $SRCGENPFILE |wc -l|sed 's/ //g'`
        fi
    fi

    # get ASM related information
    if [ "$SOURCE_IsASM" = "YES" ]; then
        GetAsmInformation
    fi

}

log()
{
    echo "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ oraclefailover:$ACTION $*" >> $LOG_FILE
    return
}

########################################################################
#################    START OF PROGRAM   ################################
########################################################################

###########Calling Corresponding Function based on the OS #####	
CallingCorrespondingOS

INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version|cut -d'=' -f2`
tempfile1='/tmp/tempfile1'
tempfile2='/tmp/tempfile2'
tempfile3='/tmp/tempfile3'
tempfile4='/tmp/tempfile4'
tempfile5='/tmp/tempfile5'
tempfile6='tmp/tempfile.PairInfo'
DISCOVERYXML='/tmp/DISCOVERYXML'
NULL=/dev/null

dirName=`dirname $0`
orgWd=`pwd`
cd $dirName
SCRIPT_DIR=`pwd`
cd $orgWd

LOG_FILE=/var/log/oradisc.log
touch $LOG_FILE
chmod 640 $LOG_FILE

rm -f $tempfile1 $tempfile2 $tempfile3 $tempfile4

APP_CONF_FILE=$INMAGE_DIR/etc/appwizard.conf

### To check if the arguments are passed   ####
if [ $# -ne 16 ]
then
	Usage
fi

while getopts ":a:c:s:t:n:v:p:g:" opt
do
    case $opt in
    a )
        ACTION=$OPTARG
        if [ $ACTION != "rollback" -a $ACTION != "dgstartup" -a $ACTION != "databasestartup" ]
        then
            Usage
        fi
        ;;
    c )
        CLUSTER=$OPTARG
        if [ "$CLUSTER" != "yes" -a "$CLUSTER" != "no" ]
        then
            Usage
        fi
        ;;
    s ) 
        if [ -f $OPTARG ]
        then 
            SOURCECONFIGFILE=$OPTARG
        else
            echo "File $OPTARG doesnt exist"
            log "File $OPTARG doesnt exist"
            exit 1
        fi
    ;;
    p ) 
        if [ -f $OPTARG ]
        then
            SRCPFILE=$OPTARG
        else
            echo "File $OPTARG doesnt exist"
            log "File $OPTARG doesnt exist"
            exit 1
        fi
    ;;
    g ) 
        if [ -f $OPTARG ]
        then
            SRCGENPFILE=`echo $OPTARG | sed 's/\/\//\//g'`
        else
            echo "File $OPTARG doesnt exist"
            log "File $OPTARG doesnt exist"
            exit 1
        fi
    ;;
    t )
        recoveryPointType=$OPTARG
    ;;
    n )
        tag=$OPTARG
    ;;
    v )
        TARGET_DEVICES=$OPTARG
        rm -f $tempfile1
        for i in `echo $TARGET_DEVICES|sed 's/,/ /g'`
        do
            echo $i>>$tempfile1
        done
    ;;	 
    \?) Usage ;;
    esac
done

##calling necessary script
echo "Reading source config file..."
log "Reading source config file..."
CallReadSourceConfigFile	

echo "Probing Target configuration ..."
log "Probing Target configuration ..."
GetTargetConfig

if [ $ACTION = "rollback" ]
then
    if [ $SOURCE_DATABASE_TYPE = "DATABASE_ON_LOCAL_FILE_SYSTEM" ]
    then
        LvmActions
    elif [ $SOURCE_DATABASE_TYPE = "ASMRaw" ]
    then
        GetAsmDiskAlias 
    fi
    RollbackVolumes
elif [ $ACTION = "dgstartup" ]
then
    if [ $SOURCE_DATABASE_TYPE = "VXVM" ]
    then
        VXVMActions
    elif [ $SOURCE_DATABASE_TYPE = "VXVMRaw" ]
    then
        VXVMRawActions
    elif [ $SOURCE_DATABASE_TYPE = "ASMLib" ]
    then
        CheckASMInstanceStatus
        ASMLibDeviceActions 
        ASMLibActions
        ASMActions
    elif [ $SOURCE_DATABASE_TYPE = "ASMRaw" ]
    then
        CheckASMInstanceStatus
        ASMRawActions
        sleep 30
        ASMActions
    elif [ $SOURCE_DATABASE_TYPE = "DATABASE_ON_LOCAL_FILE_SYSTEM" ]
    then
        MountVolumes
    elif [ $SOURCE_DATABASE_TYPE = "DATABASE_ON_ZFS_FILE_SYSTEM" ]
    then
        MountZfsVolumes
    else
        Unsupported
    fi
else
    AddEntryInOratabFile
    CreatingDirectories
    if [ $SOURCE_IsCluster = "YES" ]
    then
        if [ $TARGET_IsCluster = "NO" ]
        then
           ChangesInPfileClusterToStandalone
        elif [ $TARGET_IsCluster = "YES" ]
        then
           ChangesInPfileClusterToCluster
        fi
    else
        if [ $TARGET_IsCluster = "YES" ]
        then
            if [ "$CLUSTER" = "yes" ]; then
               ChangesInPfileStandaloneToCluster
            else
                Unsupported
            fi
        elif [ $TARGET_IsCluster = "NO" ]
        then
           ChangesInPfileStandaloneToStandalone
        fi
    fi
    CheckListeners
    CheckInstanceStatus
    CreateSpfile
    OracleStartup
    OracleRecover
    #EnableDiscovery
fi

exit 0
