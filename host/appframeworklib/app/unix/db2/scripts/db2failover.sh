#!/bin/bash
#=================================================================
# FILENAME
#    db2failover.sh
#
# DESCRIPTION
#	1. failover: To perform the failover 
#   2. rollback: To rollback the volumes on target machine
#   3. dgstartup:  To start the disk groups
#   4. databasestartup: To start the database
#
# HISTORY
#     <5/12/2012>  Anusha T   Created.
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================

TEMP_FILE=/tmp/tempfile.$$

Usage()
{
	echo "Usage: $0 -a Action -t RecoveryPointType -n Tagname -v ListOfVolumes "
    echo " "
    echo " where Action can be any of the following:"
	echo " failover            -- All the three operations should be performed for failover"
    echo " rollback            -- To rollback  all the volumes to a consistent tag"
    echo " dgstartup           -- To start disk groups or Mount Filesystems"
    echo " databasestartup     -- To start database"
    echo " "
	echo "Note: Providing instance name along with dbname is mandatory as dbnames can be same under different instances."
	echo "failover should be initiated for only one instance at a time and can be for multiple dbs"
    exit 1
}

Unsupported()
{
    echo "Detected unsupported configurations. Failover failed."
    exit 1
}

log()
{
    echo "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ db2failover:$ACTION $*" >> $LOG_FILE
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
        Linux )
            df_command="df"
            FSTABFILE="/etc/fstab"
            AWK="/usr/bin/awk"
            MOUNT_ARG="t"
        ;;
        AIX )
            df_command="df"
            FSTABFILE="/etc/vfstab"
            AWK="/usr/bin/awk"
            MOUNT_ARG="v" 
        ;;
    esac
}

CallReadSourceConfigFile()
{
	#ReadSourceConfigurationFile	
	SOURCE_HostID=`grep "SourceHostId" $SOURCECONFIGFILE| awk -F"=" '{print $2}'`
	if [ -z "$SOURCE_HostID" ]
    then
        echo "Source HostId is Null in Configuration File"
        log "Source HostId is Null in Configuration File"
        echo "Cannot Perform Failover"
        exit 1
    fi
	
	SOURCE_DBINST=`grep "DBInstance" $SOURCECONFIGFILE | awk -F"=" '{print $2}'`
	if [ -z "$SOURCE_DBINST" ]
	then
		echo "Source Instance is Null in Configuration File"
		log "Source Instance is Null in Configuration File"
		echo "Cannot Perform Failover"
		exit 1
	fi
	
	SOURCE_DBNAME=`grep "DBName" $SOURCECONFIGFILE |awk -F"=" '{print $2}'`
    if [ -z "$SOURCE_DBNAME" ]
    then
        echo "Database name is Null in Source Configuration File"
        log "Database name is Null in Source Configuration File"
        echo "Cannot Perform Failover"
        exit 1
    fi

    SOURCE_DB_LOC=`grep "DBLocation" $SOURCECONFIGFILE |awk -F"=" '{print $2}'`
    if [ -z "$SOURCE_DB_LOC" ]
    then
        echo "DBLocation is Null in Source Configuration File"
        log "DBLocation is Null in Source Configuration File"
        echo "Cannot Perform Failover"
        exit 1
    fi
	
	SOURCE_DB_VERSION=`grep "DBVersion" $SOURCECONFIGFILE |awk -F"=" '{print $2}'`
    if [ -z "$SOURCE_DB_VERSION" ]
    then
        echo "DBVersion is Null in Source Configuration File"
        log "DBVersion is Null in Source Configuration File"
        echo "Cannot Perform Failover"
        exit 1
    fi
	
	SOURCE_MOUNTPOINT=`grep "MountPoint" $SOURCECONFIGFILE | awk -F"=" '{print $2}'`    
	SOURCE_VG=`grep "VolumeGroup" $SOURCECONFIGFILE |uniq|awk -F"=" '{print $2}'`
	SOURCE_DISKS=`grep "PVs" $SOURCECONFIGFILE | uniq |awk -F"=" '{print $2}'`
	SOURCE_LV=`grep "MountPoint" $SOURCECONFIGFILE | uniq |awk -F"=" '{print $2}'|awk -F":" '{print $2}'`
	#Check this once
	SOURCE_DATABASE_TYPE=DATABASE_ON_LOCAL_FILE_SYSTEM
}

ReadTargetDbs()
{
	TMP_DB_OUTPUT=/tmp/test	
	
	TARGET_DBINST=`ps -ef | grep db2sysc | grep -v grep | awk '{ print $1}'`
	numtgtdbinst=`echo $TARGET_DBINST|wc -w`	

	if [ ! -z "$TARGET_DBINST" ]; then
		tgtcount=1
		for DB2INS in $TARGET_DBINST
		do
			log "$DB2INS"
			TARGET_DB_VERSION=`su - $DB2INS -c "db2licm -v"`
			log "Target DB Version = $TARGET_DB_VERSION"
			srccount=1
			
			if [[ $SOURCE_DB_VERSION = $TARGET_DB_VERSION ]]; then
				log "Source and target DB versions are same for DB2Instance: $db2Inst"
			else
				log "Source and target DB versions are different for DB2Instance: $db2Inst"
			fi	
			
			# "db2 list db directory" is used as, some db's may be in write suspend or inactive state.
			su - $DB2INS -c "db2 list db directory" > $TMP_DB_OUTPUT 
			TARGET_DBNAME=`grep "Database name" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'`
			rm -f $TMP_DB_OUTPUT
		done
	fi
	
}

GetSourceDevice()
{
    TargetDevice=$1
    PairInfo=$INMAGE_DIR/etc/$SOURCE_DBINST.$SOURCE_DBNAME.PairInfo
    
    match=0
    while read line
    do
        case $line in
          "<TargetDevice>")
              read line1
              tgtdev=`echo $line1|awk -F">" '{print $2}'|awk -F"</" '{print $1}'`
              if [[ $TargetDevice = $tgtdev ]];then
                  match=1
              fi
          ;;
          "</TargetDevice>")
             match=0
          ;;
          *)
            if [ $match -eq 1 ];then
               echo $line > $tempfile2
            fi
        esac
    done < $PairInfo 
}

GetTargetDevice()
{
    SourceDevice=$1
    PairInfo=$INMAGE_DIR/etc/$SOURCE_DBINST.$SOURCE_DBNAME.PairInfo

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
                 echo "<target_devicename>$tgtdev</target_devicename>" > $tempfile2
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
	
	echo "SourceVgs=$SOURCE_VG"
    if [ ! -z "$SOURCE_VG" ]; then
        echo "Source filesystems are on LVs."
        log "Source filesystems are on LVs."

        >TgtVgMapping
        >SrcTgtMapping

        for tgtVol in `echo $TARGET_DEVICES|sed 's/,/ /g'`
        do
            echo "Determining source devices for target device $tgtVol.."
            log  "Determining source devices for target device $tgtVol.."

			GetSourceDevice $tgtVol 
            SourceDevice=`grep "source_devicename" $tempfile2|$AWK -F">" '{ print $2 }'|$AWK -F"<" '{print $1}'`
			echo "SourceDevice=$SourceDevice"
            if [ -z "$SourceDevice" ]; then
                echo "Failed to fetch Source device name for $tgtVol through ."
                log "Failed to fetch Source device name for $tgtVol using cxcli."
                exit 1
            fi
            echo "$SourceDevice $tgtVol" >> SrcTgtMapping
        done
		
		vgname=$SOURCE_VG
		lvname=$SOURCE_LV

        tgtPvList=
		for SourceDisk in `echo $SOURCE_DISKS | sed "s/,/ /g"`
        do
            TargetDevice=`grep -w ^$SourceDisk SrcTgtMapping | $AWK '{print $2}'` 
			if [ ! -b "$TargetDevice"  ];then
                echo "Failed to fetch target device name for $SourceDisk in SrcTgtMapping."
                log "Failed to fetch target device name for $SourceDisk in SrcTgtMapping."
                exit 1
            fi

			log "TargetDevice=$TargetDevice"
			pvName=`echo $TargetDevice | $AWK -F"/" '{ print $3 }'`
			log "pvName=$pvName"
			tgtPvList=`echo "${tgtPvList} ${pvName}"`
       done

       echo "$vgname $tgtPvList" >> TgtVgMapping
       log "$vgname $tgtPvList"

       return
    fi

    volMountPoint=$SOURCE_MOUNTPOINT
	
	fsType=`echo $volMountPoint |$AWK -F":" '{ print $1 }'`
    DiskPath=`echo $volMountPoint |$AWK -F":" '{ print $2 }'`
    mountPoint=`echo $volMountPoint |$AWK -F":" '{ print $4 }'`
   
	echo "Determining corresponding target partitions.."
    log "Determining corresponding target partitions.."

	GetTargetDevice $DiskPath
    Target_partition=`grep "target_devicename" $tempfile2|$AWK -F">" '{ print $2 }'|$AWK -F"<" '{print $1}'`
    if [ -z "$Target_partition" ]; then
        echo "Failed to fetch target device name for $DiskPath using cxcli."
        log "Failed to fetch target device name for $DiskPath using cxcli."
        exit 1
    fi

    echo "$MOUNT_ARG $fsType $Target_partition $mountPoint" >> MountOnTarget
    echo "$mountPoint" >> ForPermissions
    
	return
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

    if [ ! -z "$SOURCE_VG" ]; then
        echo "Source filesystems are on LVs."
        log "Source filesystems are on LVs."
        ImportVgs 
					
		volMountPoint=$SOURCE_MOUNTPOINT
        fsType=`echo $volMountPoint |$AWK -F":" '{ print $1 }'`
        DiskPath=`echo $volMountPoint |$AWK -F":" '{ print $2 }'`
        mountPoint=`echo $volMountPoint |$AWK -F":" '{ print $4 }'`
        
        df -k | awk '{print $NF}' | grep -w $mountPoint > /dev/null
        if [ $? -eq 0 ]; then
            echo "$mountPoint already mounted"
            log "$mountPoint already mounted"       
        fi

        if [ "$OSTYPE" = "AIX" ]; then
            logVolume=`echo $volMountPoint |$AWK -F":" '{ print $4 }'`
        fi
            
        mkdir -p $mountPoint

        if [ "$OSTYPE" = "AIX" ]; then
				
            vgName=$SOURCE_VG
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
			TargetDiskPath=`cat SrcTgtMapping | awk -F" " '{print $2}'`				
			echo "mount -$MOUNT_ARG $fsType $TargetDiskPath $mountPoint"
            log "mount -$MOUNT_ARG $fsType $TargetDiskPath $mountPoint"
            mount -$MOUNT_ARG $fsType $TargetDiskPath $mountPoint
        fi
        
		if [ $? -ne 0 ]; then
            log "Mount failed"
            echo "Mount failed"
            exit 1
        fi
            TARGET_DBINST=`ps -ef | grep db2sysc | grep -v grep | awk '{ print $1}'`
			for db2_user in $TARGET_DBINST
			do 
				if [[ $db2_user = $SOURCE_DBINST ]]; then
						chown -R $db2_user $mountPoint			
				fi
			done
			
	    echo "Succesfully Mounted the Volumes.."
        log "Succesfully Mounted the Volumes.."
        return
    fi  	
}

ConnectToDatabase()
{
    echo "Connecting to the database $SOURCE_DBNAME.."	
	
    db2_user=$SOURCE_DBINST
	
    #Bug 24607 - SQL1005N  The database alias "dbtest44" already exists in either the local
    #database directory or system database directory. When Database is already cataloged then it should be considered as success.
    TMP_DB_OUTPUT=/tmp/dbcheck
    su - $db2_user -c "db2 list db directory" > $TMP_DB_OUTPUT

    DB_NAMES=`grep "Database name" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'`
    DB_LOCS=`grep "Local database directory" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'`
    NUM_DB=`grep "Database name" $TMP_DB_OUTPUT | wc -l `
	
    #To check the status of the db2instance
    ps -ef|grep "db2sysc" |grep $db2_user >>/dev/null
    if [ $? -ne 0 ];then
        echo "Db2 instance $db2_user is not running"
        echo "starting the db2instance $db2_user"
        TMP_FILE=/tmp/db2status.$$
        su - $db2_user -c "db2start" >$TMP_FILE
      
        grep "DB2START processing was successful" $TMP_FILE >>/dev/null
        if [ $? -ne 0 ];then
            echo "Unable to start the db2 instance $db2_user"
            cat $TMP_FILE
            rm -f $TMP_FILE
            exit 1
        else
            echo "db2instance $user is started successfully"
        fi
        rm -f $TMP_FILE
    fi	
  	
	dbname=$SOURCE_DBNAME
	
	MountPoint=`cat ${DB2ConfFile}/DB2_$db2_user.$dbname.conf|grep -w "MountPoint" | awk -F"=" '{print $2}' | sed 's/ //g'|awk -F":" '{print $4}'` 

	TMP_FILE1=/tmp/db2out.$$
	
	i=1
	while [[ $i -le $NUM_DB ]]
	do
			DB_NAME=`echo $DB_NAMES | cut -d" " -f$i`
			DB_LOC=`echo $DB_LOCS | cut -d" " -f$i`

			if [[ "$DB_NAME" = "$dbname" ]];then
				if [[ "$DB_LOC" = "$MountPoint" ]];then
					echo "$DB_NAME is already present on the target on same mount point $DB_LOC"
					su - $db2_user -c "db2 <<END >$TMP_FILE1
						uncatalog database $dbname
					END"
					grep "The UNCATALOG DATABASE command completed successfully." $TMP_FILE1 >>/dev/null
					if [ $? -ne 0 ];then
						echo "Failed to uncatalog the database $dbname"
						cat $TMP_FILE1
						rm -f $TMP_FILE1
						exit 1
					fi	
				else
					echo "$DB_NAME is already present on the target on different mount point $DB_LOC"
					su - $db2_user -c "db2 <<END >$TMP_FILE1
						deactivate database $dbname
						drop database $dbname
					END"
					grep "The DROP DATABASE command completed successfully" $TMP_FILE1 >>/dev/null
					if [ $? -ne 0 ];then
						grep "SQL1031N" $TMP_FILE1 >>/dev/null
						if [ $? -eq 0 ];then
							echo "The database directory cannot be found on the indicated file system, uncatalog the database"
							su - $user -c "db2 <<END >$TMP_FILE1
								uncatalog database $dbname
							END"
							grep "The UNCATALOG DATABASE command completed successfully." $TMP_FILE1 >>/dev/null
							if [ $? -ne 0 ];then
								echo "Failed to cleanup the existing database"
								cat $TMP_FILE1
								rm -f $TMP_FILE1
								exit 1
							fi
						else
							echo "Failed to drop the database $dbname.."
							echo "Database $dbname already exists on the mount point $DB_LOC. Cleanup this database"
							cat $TMP_FILE1
							rm -f $TMP_FILE1
							exit 1
						fi
					fi
				fi
			fi
			((i=i+1))
		done
		rm -f $TMP_DB_OUTPUT

		su - $db2_user -c "db2 <<END >$TMP_FILE1					
			catalog database $dbname on $MountPoint
		END"
		grep "The CATALOG DATABASE command completed successfully." $TMP_FILE1 >>/dev/null
		if [ $? -ne 0 ]
		then
			echo "Failed to catalog the database $dbname.."
			cat $TMP_FILE1
			rm -f $TMP_FILE1
			exit 1
		else
			echo "The catalog database command succeeded for database $dbname.."
		fi		
	
		RecoveryLog=`cat ${DB2ConfFile}/DB2_$db2_user.$dbname.conf|grep -w "RecoveryLogEnabled" | awk -F"=" '{print $2}' |sed 's/ //g'` 
	
		if [[ $RecoveryLog = "LOGRETAIN" ]];then
			su - $db2_user -c "db2inidb $dbname as snapshot"
			su - $db2_user -c "db2 <<END >$TMP_FILE1			
				backup database $dbname
			END"
			grep "Backup successful" $TMP_FILE1 >>/dev/null
			if [ $? -ne 0 ];then
				echo "Backup is not successful"
				cat $TMP_FILE1
				rm -f $TMP_FILE1
				exit 1
			fi	
		fi	
	
		su - $db2_user -c "db2 <<END >$TMP_FILE1
			restart db $dbname write resume
			disconnect $dbname
			activate database $dbname
		END"
		grep "The RESTART DATABASE command completed successfully." $TMP_FILE1 >>/dev/null
		if [ $? -ne 0 ]
		then
			echo "Failed to write resume the database $dbname.."
			cat $TMP_FILE1
			rm -f $TMP_FILE1
			exit 1
		else
			echo "The write resume command succeeded for database $dbname.."
		fi
		
		grep "The ACTIVATE DATABASE command completed successfully." $TMP_FILE1 >>/dev/null
		if [ $? -ne 0 ]
		then
			echo "Failed to activate the database $dbname.."
			cat $TMP_FILE1
			rm -f $TMP_FILE1
			exit 1
		else
			echo "Successfully activated the database $dbname.."
		fi	
		rm -f $TMP_FILE1	

}

################# Main ################################

###########Calling Corresponding Function based on the OS #####	
CallingCorrespondingOS

INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version|cut -d'=' -f2`
tempfile1='/tmp/tempfile1'
tempfile2='/tmp/tempfile2'
tempfile3='/tmp/tempfile3'
tempfile4='/tmp/tempfile4'
tempfile5='/tmp/tempfile5'
DISCOVERYXML='/tmp/DISCOVERYXML'
NULL=/dev/null

dirName=`dirname $0`
orgWd=`pwd`
cd $dirName
SCRIPT_DIR=`pwd`
cd $orgWd

LOG_FILE=/var/log/db2out.log

rm -f $tempfile1 $tempfile2 $tempfile3 $tempfile4


#mkdir -p $INMAGE_DIR/failover_data/DB2
#DB2ConfFile=$INMAGE_DIR/failover_data/DB2
DB2ConfFile=$INMAGE_DIR/db2_failover_data

### To check if the arguments are passed   ####
if [ $# -ne 10 ]
then
	Usage
fi

while getopts ":a:i:d:t:n:v:s:" opt
do
    case $opt in
    a ) 
		ACTION=$OPTARG
        if [ $ACTION != "failover" -a $ACTION != "rollback" -a $ACTION != "dgstartup" -a $ACTION != "databasestartup" ]
        then
            Usage
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
	s)
		if [ -f $OPTARG ]
        then 
            SOURCECONFIGFILE=$OPTARG
        else
            echo "File $OPTARG doesnt exist"
            log "File $OPTARG doesnt exist"
            exit 1
        fi
    ;;
    \?) Usage ;;
    esac
done


	echo "Reading source config file ..."
	log "Reading source config file ..."
	CallReadSourceConfigFile	

	echo "Reading Target DB2 instances..."
	log "Reading Target DB2 instances..."
	ReadTargetDbs

	if [ $ACTION = "failover" ]
	then
		if [ $SOURCE_DATABASE_TYPE = "DATABASE_ON_LOCAL_FILE_SYSTEM" ]
		then
			LvmActions
		fi
		RollbackVolumes
	
		if [ $SOURCE_DATABASE_TYPE = "DATABASE_ON_LOCAL_FILE_SYSTEM" ]
		then
			MountVolumes
		else
			Unsupported
		fi
		ConnectToDatabase
	
	elif [ $ACTION = "rollback" ]
	then
		if [ $SOURCE_DATABASE_TYPE = "DATABASE_ON_LOCAL_FILE_SYSTEM" ]
		then
			LvmActions
		fi
		RollbackVolumes
	elif [ $ACTION = "dgstartup" ]
	then
		if [ $SOURCE_DATABASE_TYPE = "DATABASE_ON_LOCAL_FILE_SYSTEM" ]
		then
			MountVolumes
		else
			Unsupported
		fi
	elif [ $ACTION = "databasestartup" ]
	then
		ConnectToDatabase
	fi
#done
exit 0

