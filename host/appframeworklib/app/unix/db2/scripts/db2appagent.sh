#!/bin/bash
#=================================================================
# FILENAME
#    db2appagent.sh
#
# DESCRIPTION
#  1. discover : To discover the db2 databases currently active on the machine
#  2. preparetarget : To take the db2 databases or/and instances offline
#  3. VerifyTgtReadiness : To check the db2 instances on target are in sync with the source.
#    
# HISTORY
#     <5/12/2012>  Anusha T Created.
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
NULL=/dev/null

Cleanup()
{
    rm -rf $TMP_OUTPUT > $NULL	
}

CleanupError()
{
    rm -rf $TMP_OUTPUT > $NULL
    exit 1
}

SetupLogs()
{
  LOG_FILE=/var/log/db2out.log
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

Output()
{
    $ECHO $* >> $OUTPUT_FILE
    return
}

log()
{
  $ECHO "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ $OPTION $*" >> $LOG_FILE
  return
}

logXml()
{
  TAG_NAME=$1
  TAG_VALUE=$2
  CONF_FILE=$3
  
  ext=`echo $CONF_FILE|awk -F"." '{print $NF}'`
		
  if [ $ext = "conf" -o $ext = "xml" ];then
	echo "<$TAG_NAME>$TAG_VALUE</$TAG_NAME>" >> $CONF_FILE
  else
	echo "$TAG_NAME=$TAG_VALUE" >> $CONF_FILE
  fi
  
  return  
}

Usage()
{
    $ECHO "usage: $0 <discover> <output_filename>"
    $ECHO "       $0 <targetreadiness> <configfilepath>"
    $ECHO "       $0 <preparetarget>   <configfilepath>"
    $ECHO "discover                 - To discover DB2 database details, volumes, disks"
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
	
	if [ $1 != "discover"  -a  $1 != "preparetarget"  -a $1 != "targetreadiness" ]; then
        Usage
        exit 1
    fi
	
	OPTION=$1

    if [ -z "$2" ]; then
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
		ext=`echo $OUTPUT_FILE|awk -F"." '{print $NF}'`
		
		if [[ $ext = "conf" || $ext = "xml" ]];then
			isConfFile=1
		else
			isConfFile=0
		fi
    fi
    
	dirName=`dirname $0`
    orgWd=`pwd`

    cd $dirName
    SCRIPT_PATH=`pwd`
    cd $orgWd

    SetupLogs
	
    return
}

GetParentProcName()
{
    parent=`ps -p $$ -o ppid=`	
    ppname=`ps -p $parent -o comm=`

    echo $ppname | grep appservice > $NULL
    if [ $? -eq 0 ]; then
        ppname=appservice
    fi

    log "Parent id : $parent  name : $ppname"

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
   
    # Linux DMP
    elif [ "$deviceType" = "device-mapper" ]; then
        for device in `ls -l /dev/mapper/ | grep "^b" | awk '{print "/dev/mapper/"$NF}'`
        do
            stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
            if [ $? -eq 0 ]; then
                blockDevice=$device
                log "Linux DMP Device Bound to device $dev = $blockDevice"
            fi
        done
    else
        log "Error: unsupported device type $deviceType"
        exit 1
    fi

    if [ ! -z "$blockDevice" ]; then

        if [ "$option" = "FilterDevice" ] ; then        
			WriteToConfFile "FilterDevice" $blockDevice 
        else            
			echo "$blockDevice" >> $OUTPUT_FILE
        fi

    fi

    log "Exit GetBlockDeviceFromMajMinLinux..."

    return
}

#Check this once on AIX
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
                fstype=`mount | grep $volPath | grep -w $allmntvol| awk '{print $3}'`
				#logXml "MountPoint" "$fstype:$volPath:$mntPoint:$logVolume" $DB_CONF_FILE     
				WriteToConfFile "MountPoint" "$fstype:$volPath:$mntPoint:$logVolume"
            fi
        fi
    done
    
    return
}

GetVxvmVersion()
{
    DB_CONF_FILE=$1
	
	if [ "$OSTYPE" = "Linux" ]; then
        /bin/rpm -q VRTSvxvm > /dev/null
        if [ $? -eq 0 ]; then
            vxvmVersion=`/bin/rpm -q VRTSvxvm | awk -F"-" '{print $2}' | awk -F"." '{print $1"."$2}'`
        else
            log "Error: VXVM package not found"
            $ECHO "Error: VXVM package not found"
            exit 1
        fi
	#TODO Check how to find out version on AIX
    elif [ "$OSTYPE" = "AIX" ]; then
		lslpp -l VRTSvxvm > /dev/null
		if [ $? -eq 0 ]; then
			vxvmVersion=`lslpp -l VRTSvxvm`
		else
			log "Error: VXVM package not found"
            $ECHO "Error: VXVM package not found"
            exit 1
		fi
	else
	    $ECHO "Error: OS not supported."
        exit 1
    fi

    if [ -z "$vxvmVersion" ]; then
        log "Error: VXVM version not found"
        $ECHO "Error: VXVM version not found"
        exit 1
    fi

    #logXml "VxvmVersion" $vxvmVersion $DB_CONF_FILE
	WriteToConfFile "VxvmVersion" $vxvmVersion
	
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
            fi
        done
    # Linux DMP
    elif [ "$deviceType" = "device-mapper" ]; then
        for device in `ls -l /dev/mapper/ | grep "^b" | awk '{print "/dev/mapper/"$NF}'`
        do
            stat -c "%t,%T" $device | grep -i "$majHex,$minHex" > /dev/null
            if [ $? -eq 0 ]; then
                blockDevice=$device
                log "Linux DMP Device Bound to device $dev = $blockDevice"
            fi
        done
    else
        log "Error: unsupported device type $deviceType"
        exit 1
    fi

    if [ ! -z "$blockDevice" ]; then

        if [ "$option" = "FilterDevice" ] ; then
            echo "FilterDevice=$blockDevice" >> $MAJ_MIN_OUTPUT_FILE
        else
            echo "$blockDevice" >> $MAJ_MIN_OUTPUT_FILE
		fi

    fi

    log "Exit GetBlockDeviceFromMajMinLinux..."

    return
}

GetBlockDeviceFromMajMinAix()
{
    log "Exit GetBlockDeviceFromMajMinAix..."
    exit 1
}

GetFilterDevicesFromAlias()
{
	 log "ENTER GetFilterDevicesFromAlias... "

    dev=$1
    option=$2

    log "Device: $dev"

    maj=`ls -lL $dev  2>/dev/null| awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
    min=`ls -lL $dev  2>/dev/null| awk '{print $6}' | sed 's/,//' | sed 's/ //g'`

    if [ "$OSTYPE" = "Linux" ]; then
        GetBlockDeviceFromMajMinLinux $maj $min $option    
    elif [ "$OSTYPE" = "AIX" ]; then
        GetBlockDeviceFromMajMinAix $maj $min $option
    fi

    log "EXIT GetFilterDevicesFromAlias"

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
    else
	#TODO: Need to verify how to find the vxfs versin on AIX
        log "Error: OS not supported."
        $ECHO "Error: OS not supported."
        exit 1
    fi

    if [ -z "$vxfsVersion" ]; then
        log "Error: VXFS version not found"
        $ECHO "Error: VXFS version not found"
        exit 1
    fi

    WriteToDbConfFile "VxfsVersion" "$vxfsVersion"

    return
}

GetFilterDevicesFromFileSystem()
{
	log "ENTER GetFilterDevicesFromFileSystem..."
   
    dbInst=$1
	dbName=$2
	dbpath=$3
	option=$4
	DB_CONF_FILE=$5
	
	mount_point=`df $dbpath|tail -1|awk '{ print $NF}'`
		
    if [ "$OSTYPE" = "Linux" ]; then
		fstype=`df -PT $mount_point | grep -v Filesystem | awk '{ print $2 }'`
		#logXml "FsType" $fstype $DB_CONF_FILE
		WriteToConfFile "FsType" $fstype
	elif [ "$OSTYPE" = "AIX" ]; then
		fstype=`/usr/sysv/bin/df -n $mount_point | awk -F: '{ print $2 }' | sed 's/ //g'`
		#logXml "FsType" $fstype $DB_CONF_FILE
		WriteToConfFile "FsType" $fstype
	fi
	
	if [ "$option" = "FilterDevice" ]; then
		if [ "$fstype" = "vxfs" ];then
			WriteToDbConfFile "isVxfs" "YES"
			#Need to put this value in config file of each db.
			#echo "<isVxfs>YES</isVxfs>">>$DB2ConfFile/DB2_$dbInst.$dbName.xml
			echo "isVxfs=YES">>$DB2ConfFile/DB2_$dbInst.$dbName.conf
            GetVxfsVersion
		else
			#echo "<isVxfs>NO</isVxfs>">>$DB2ConfFile/DB2_$dbInst.$dbName.xml
			echo "isVxfs=NO">>$DB2ConfFile/DB2_$dbInst.$dbName.conf
        fi
    fi		
	
	levelCount=1
    if [ "$option" = "FilterDevice" ]; then
    	WriteToConfFile "MountPointWithLevel" "$mount_point(MountPoint),$levelCount" 
	fi
    levelCount=`expr $levelCount + 1`
	
	if [ "$OSTYPE" = "Linux" ]; then
        volume=`df -PT $mount_point | grep -v Filesystem | awk '{ print $1 }'`
		#logXml "Volume" $volume $DB_CONF_FILE			
		WriteToConfFile "Volume" $volume
	elif [ "$OSTYPE" = "AIX" ]; then
        volume=`df $mount_point | grep -v Filesystem | awk '{ print $1 }'`
        logVolume=`mount | grep -w $volume | awk -F= '{ print $2 }'`
		
		#logXml "Volume" $volume $DB_CONF_FILE		
		#logXml "logVolume" $logVolume $DB_CONF_FILE		
		WriteToConfFile "Volume" $volume
		WriteToConfFile "logVolume" $logVolume
	fi
	
	if [ "$option" = "FilterDevice" ]; then
        WriteToConfFile "VolumeNameWithLevel" "$volume(VolumeName),$levelCount" 	   
    fi
    levelCount=`expr $levelCount + 1`

	case $volume in
	*/dev/vx/dsk/*)

        if [ "$option" = "FilterDevice" ]; then
			#logXml "isVxvm" "YES" $DB_CONF_FILE
			WriteToConfFile "isVxvm" "YES"
			#echo "<isVxvm>YES</isVxvm>">>$DB2ConfFile/DB2_$dbInst.$dbName.xml
			echo "isVxvm=YES">>$DB2ConfFile/DB2_$dbInst.$dbName.conf
            GetVxvmVersion
        fi

        if [ "$option" = "FilterDevice" ]; then
			#logXml "MountPoint" "$fstype:$volume:$mount_point:$logVolume" $DB_CONF_FILE			
			WriteToConfFile "MountPoint" "$fstype:$volume:$mount_point:$logVolume" 
        fi

        diskgroup=`echo $volume |  awk -F/ '{print $5}'`
		WriteToConfFile "DiskGroup" $diskgroup
		
        if [ "$option" = "FilterDevice" ]; then            
			WriteToConfFile "DiskGroupNameWithLevel" "$diskgroup(DiskGroupName),$levelCount" 
        fi
        levelCount=`expr $levelCount + 1`

        for disk in `$VXDISK -g $diskgroup list | grep -v DEVICE | awk '{ print $3 }'`
        do
            if [ "$option" = "FilterDevice" ]; then                
				WriteToConfFile "DiskNameWithLevel" "$disk(DiskName),$levelCount" 
            fi
        done

        for disk in `$VXDISK -g $diskgroup list | grep -v DEVICE | awk '{ print $1 }'`
        do
            for path in `$VXDISK list $disk | grep state=enabled | awk '{print $1}'`
            do

				if [ "$option" = "FilterDevice" ]; then
                    if [ "$OSTYPE" = "Linux" ]; then                        
						WriteToConfFile "FilterDevice" "/dev/$path" 
                    fi
                else
                    if [ "$OSTYPE" = "Linux" ]; then                        
						$ECHO "/dev/$path" >> $OUTPUT_FILE
                    fi
                fi
            done
        done
        ;;
    */dev/dm-*)
        if [ "$option" = "FilterDevice" ]; then
            GetFilterDevicesFromAlias $volume "FilterDevice"

            DM_OUTPUT_FILE=/tmp/dmoutput.$$
            GetFilterDevicesFromAlias $volume $DM_OUTPUT_FILE ""
            mpDevice=`cat $DM_OUTPUT_FILE`
            rm -f $DM_OUTPUT_FILE
			
			#logXml "MountPoint" "$fstype:$mpDevice:$mount_point:$logVolume" $DB_CONF_FILE           
			WriteToConfFile "MountPoint" "$fstype:$mpDevice:$mount_point:$logVolume"
        fi
        ;;

	*) 

        if [ "$option" = "FilterDevice" ]; then
             WriteToConfFile "MountPoint" "$FSType:$volume:$mount_point:$logVolume"
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
		               WriteToConfFile "FilterDevice" "/dev/$pv"
                       found=1
                       break
                   else
                       $ECHO "/dev/$volume" >> $OUTPUT_FILE
                   fi
               fi
            done < $tempfile1

            rm -f $tempfile1

            if [ $found = 0 ]; then

	  	    lvname=`basename $volume`
	 	    vgname=`lslv $lvname|grep "VOLUME GROUP"|awk -F: '{print $NF}'|sed 's/ //g'`
	  
	 	    if [ "$option" = "FilterDevice" ]; then
			   GetAllMountPointsForVolumeGroup $vgname
            fi

            pvs=
		    if [ "$option" = "FilterDevice" ]; then
                WriteToConfFile "DiskGroupNameWithLevel" "$vgname(VolumeGroupName),$levelCount" 
                levelCount=`expr $levelCount + 1`
                      
                pvnames=
			    for pvname in ` lsvg -p $vgname | grep -v $vgname | grep -v DISTRIBUTION | awk '{ print $1 }'`
                do
                    WriteToConfFile "DiskNameWithLevel" "$pvname(DiskName),$levelCount" 
                    if [ -z "$pvnames" ];then
		               pvnames=$pvname
			        else
			           pvnames=$pvnames:$pvname
			        fi
		  	    done
                WriteToConfFile "DiskName" $pvnames 
		    fi
			
 		    for pvname in ` lsvg -p $vgname | grep -v $vgname | grep -v DISTRIBUTION | awk '{ print $1 }'`
            do
	 	       if [ "$option" = "FilterDevice" ]; then
                   if [ -z "$pvsinvg" ]; then
                       pvsinvg=/dev/$pvname
                       pvs=/dev/$pvname
                   else
                       pvsinvg=$pvsinvg:/dev/$pvname
                       pvs=$pvs,/dev/$pvname
                   fi
               else
                   $ECHO "/dev/$pvname" >> $OUTPUT_FILE
               fi
            done
		    WriteToConfFile "FilterDevice" $pvs
			
		    #logXml "VolumeGroup" "$volume:$vgname:$pvsinvg" $DB_CONF_FILE
		    WriteToConfFile "VolumeGroup" "$volume:$vgname:$pvsinvg"
	     fi
	  else
          if [ "$option" = "FilterDevice" ]; then                
	         WriteToConfFile "FilterDevice" $volume 				
          else                
	         $ECHO "$volume" >> $OUTPUT_FILE
          fi
      fi

      ;;
    esac
	
    log "EXIT GetFilterDevicesFromFileSystem"

    return
}

GetFilterDevicesFromVxvm()
{
	log "ENTER GetFilterDevicesFromVxvm..."
	
	dbInst=$1
	dbName=$2
	vxvmVol=$3
    VXVM_OUTPUT_FILE=$4
    option=$5
	DB_CONF_FILE=$6
	
	if [ "$option" = "FilterDevice" ] ; then
		#logXml "isVxvm" "YES" $DB_CONF_FILE
		WriteToConfFile "isVxvm" "YES"
        GetVxvmVersion $DB_CONF_FILE
    fi
	
	#TODO:Check this diskgroup in AIX
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
            fi
        done
    done

    log "EXIT GetFilterDevicesFromVxvm"

    return
}

GetFilterDevices()
{
	log "ENTER GetFilterDevices..."
    
	DB_INS=$1
	DB_NAME=$2
	DB_PATH=$3
	
	FILTER_CONF_FILE=$4
    
	case $DB_PATH in
	
	/dev/vx/dsk/*)
        GetFilterDevicesFromVxvm $DB_INS $DB_NAME $DB_PATH $FILTER_OUTPUT_FILE "FilterDevice" $FILTER_CONF_FILE
    ;;
    /dev/vx/rdsk/*)
        GetFilterDevicesFromVxvm $DB_INS $DB_NAME $DB_PATH $FILTER_OUTPUT_FILE "FilterDevice" $FILTER_CONF_FILE
	;;	
	*)	
		GetFilterDevicesFromFileSystem $DB_INS $DB_NAME $DB_PATH "FilterDevice" $FILTER_CONF_FILE
	;;
	esac	

    log "EXIT GetFilterDevices"

    return   
}

GetDatabaseName()
{
	DB_INS=$1
	DB_INSTALL_PATH=$2	
  
	# Get the version of the DB2 instance
	DB_VER=`su - $DB_INS -c "db2licm -v"` 
  
	log "DB Instance=$DB_INS"	
	log "DB Version=$DB_VER"
	
	if [ $isConfFile -eq 1 ]; then
		echo "<DatabaseInstance>" >> $OUTPUT_FILE	
	fi
	
	WriteToConfFile "DBInstance" $DB_INS 	
	WriteToConfFile "DBInstallationPath" $DB_INSTALL_PATH 
	WriteToConfFile "DBVersion" $DB_VER
	
	DB_VIEWS_FILE="/tmp/dbviews.$$"
	
	GetDbViews $DB_INS $DB_VER $DB_VIEWS_FILE 
	
	grep DBName $DB_VIEWS_FILE > $NULL
    if [ $? -ne 0 ]; then
        log "Error: db views does not have DBName"
		log "No DBs are active for the instance : $DB_INS"
		if [ $isConfFile -eq 1 ]; then
			echo "<Database>" >> $OUTPUT_FILE	
			echo "</Database>" >> $OUTPUT_FILE	
		fi
		rm -f $DB_VIEWS_FILE
		return
    fi
	
	NUM_DB=`grep "DBName=" $DB_VIEWS_FILE |wc -l`
	
	DB_NAMES=`grep "DBName=" $DB_VIEWS_FILE | awk -F= '{print $2}'`
	DB_LOCS=`grep "DBLocation=" $DB_VIEWS_FILE | awk -F= '{print $2}'`
	DB_VER=`grep "DBVersion=" $DB_VIEWS_FILE | awk -F= '{print $2}'`
	
	rm -f $DB_VIEWS_FILE
	
	i=1
	while [[ $i -le $NUM_DB ]]		
	#for (( i=1; i<=$NUM_DB; ++i ))
	do
		DB_NAME=`echo $DB_NAMES|cut -d" " -f$i`
		DB_LOC=`echo $DB_LOCS|cut -d" " -f$i`
			
		if [ $isConfFile -eq 1 ]; then
			echo "<Database>" >> $OUTPUT_FILE		
		fi
			
		log "DatabaseStart"		
		
		TEMP_FILE=/tmp/$DB_NAME
		
		su - $DB_INS -c "db2 <<END >$TEMP_FILE
			get db cfg for $DB_NAME 
END"
		
		LogMode=`grep "LOGARCHMETH1" $TEMP_FILE | awk -F"=" '{print $NF}'`						
		
		WriteToConfFile "DBName" $DB_NAME 
		WriteToConfFile "DBLocation" $DB_LOC 
		WriteToConfFile "ConfigFile" "$DB2ConfFile/DB2_$DB_INS.$DB_NAME.conf"
		WriteToConfFile	"RecoveryLogEnabled" $LogMode		
		
		#logXml "DBName" $DB_NAME $OUTPUT_FILE
		#logXml "DBLocation" $DB_LOC $OUTPUT_FILE
		
		#Call GetFilterDevices for each DB
		GetFilterDevices $DB_INS $DB_NAME $DB_LOC $OUTPUT_FILE
		
		log "DatabaseEnd"		
		
		if [ $isConfFile -eq 1 ]; then
			echo "</Database>" >> $OUTPUT_FILE		
		fi
		
		rm -f $TEMP_FILE
		((i=i+1))
	done	
	
	if [ $isConfFile -eq 1 ]; then
		echo "</DatabaseInstance>" >> $OUTPUT_FILE	
	fi
	
  return;
  
}

GetDbViews()
{
    log "ENTER GetDbViews..."
	log "Script Path: $SCRIPT_PATH"
	
	${SCRIPT_PATH}/db2discovery.sh $*
	
	if [ $? -ne 0 ]; then
        log "Error: DB2 discovery failed"
        exit 1
    fi
	
	log "EXIT GetDbViews"
    return	
}
	
DiscoverDatabases()
{
    log "Entering DiscoverDatabases..."
   
    if [ "$ppname" = "appservice" ]; then
		#This is to get the unregistered dbnames.
		APPWIZARD_CONF=$INSTALLPATH/etc/db2appwizard.conf
		if [ ! -f "$APPWIZARD_CONF" ]; then
			log "Error: File $INSTALLPATH/etc/db2appwizard.xml not found"
			log "Cannot retrieve the unregistered Dbs info"
		else
			for line in `grep DB2Database=NO $APPWIZARD_CONF`
			do
				dbName=`echo $line | cut -d: -f3`
				dbInstance=`echo $line | cut -d: -f2`
				WriteToConfFile "Unregister" "$dbInstance:$dbName"          				
			done
		fi
	fi   
	
   DB_INSTANCES=`ps -ef | grep db2sysc | grep -v grep | awk '{ print $1}'`
   
   for DB_INS in $DB_INSTANCES
   do
    	#Get the DB instance location
		DB_INS_LOC=`su - $DB_INS -c "db2path"`
		INSTALL_PATH=`su - $DB_INS -c "db2level" | grep "installed at" | awk '{print $NF}' | awk -F\" '{print $(NF-1)}'`
		# Instance and Usernames are one and the same.
		GetDatabaseName $DB_INS	$INSTALL_PATH 
		
	done
	
	log "EXIT DiscoverDatabases"	
		 
	return    
}

DB2ShutdownDatabase()
{
	log "ENTER DB2ShutdownDatabase..."
   
    TGT_DB_INST=$1
	TGT_DB_NAME=$2
	
	TMP_DB_OUTPUT=/tmp/dbcheck
    su - $TGT_DB_INST -c "db2 list db directory" > $TMP_DB_OUTPUT
	
	DB_NAMES=`grep "Database name" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'`
    DB_LOCS=`grep "Local database directory" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'`
    NUM_DB=`grep "Database name" $TMP_DB_OUTPUT | wc -l `

	ps -ef|grep "db2sysc" |grep $TGT_DB_INST >>/dev/null
    if [ $? -ne 0 ];then
        echo "Db2 instance $user is not running"
		log "Db2 instance $user is not running"
        echo "starting the db2instance $user"
		log "starting the db2instance $user"
        TMP_FILE=/tmp/db2status.$$
        su - $TGT_DB_INST -c "db2start" >$TMP_FILE

        grep "DB2START processing was successful" $TMP_FILE >>/dev/null
        if [ $? -ne 0 ];then
            echo "Unable to start the db2 instance $user"
			log "Unable to start the db2 instance $user"
            cat $TMP_FILE
            rm -f $TMP_FILE
            exit 1
        else
            echo "db2instance $user is started successfully"
			log "db2instance $user is started successfully"
        fi
        rm -f $TMP_FILE
    fi
	
	TMP_FILE1=/tmp/db2out.$$
	
	i=1
    while [[ $i -le $NUM_DB ]]
    do
        DB_NAME=`echo $DB_NAMES | cut -d" " -f$i`
        if [[ "$DB_NAME" = "$TGT_DB_NAME" ]];then
			echo "Database $TGT_DB_NAME is already existing on target machine, drop it"
			su - $TGT_DB_INST -c "db2 <<END >$TMP_FILE1
					deactivate database $TGT_DB_NAME
                    drop database $TGT_DB_NAME
END"
			grep "The DROP DATABASE command completed successfully" $TMP_FILE1 >>/dev/null
            if [ $? -ne 0 ];then
                grep "SQL1031N" $TMP_FILE1 >>/dev/null
                if [ $? -eq 0 ];then
                    echo "The database directory cannot be found on the indicated file system, uncatalog the database"
                    su - $TGT_DB_INST -c "db2 <<END >$TMP_FILE1
                        uncatalog database $TGT_DB_NAME
END"
                    grep "The UNCATALOG DATABASE command completed successfully." $TMP_FILE1 >>/dev/null
                    if [ $? -ne 0 ];then
                        echo "Failed to cleanup the existing database"
                        cat $TMP_FILE1
                        rm -f $TMP_FILE1
                        exit 1
					else
						echo "Uncatalog database is successful"
                    fi
                else
                    echo "Failed to drop the database $TGT_DB_NAME.."
                    echo "Database $TGT_DB_NAME already exists on target. Cleanup this database"
                    cat $TMP_FILE1
                    rm -f $TMP_FILE1
                    exit 1
                fi
			else
              echo " Database has been dropped successfully "
            fi
		fi
		((i=i+1))
	done	
	
	log "EXIT DB2ShutdownDatabase"
	return
}

#Done
TakeDatabaseOffline()
{
	log "ENTER TakeDatabaseOffline..."
   
	for TGT_CONFIG_FILE  in `echo $TGT_CONFIG_FILES | sed 's/:/ /g'`
	do
		log "Reading target config file : $TGT_CONFIG_FILE"
 		if [ ! -f "$TGT_CONFIG_FILE" ]; then
			log "Target config file $TGT_CONFIG_FILE not found."
			$ECHO "Error: Target config file $TGT_CONFIG_FILE not found."
			exit 1
		fi
		
		tgtext=`echo $TGT_CONFIG_FILE | awk -F"." '{print $NF}'`
		if [ $tgtext = "xml" ];then
			DB_INSTANCE=`grep "DBInstance" $TGT_CONFIG_FILE | awk -F">" '{print $2}'|awk -F"</" '{print $1}'`
			DBNAME=`grep "DBName" $TGT_CONFIG_FILE | awk -F">" '{print $2}'|awk -F"</" '{print $1}'`
			MOUNTPOINT=`grep "MountPoint" $TGT_CONFIG_FILE |awk -F">" '{print $2}'|awk -F"</" '{print $1}'|awk -F":" '{print $4}'`
		else
			DB_INSTANCE=`grep "DBInstance" $TGT_CONFIG_FILE | awk -F"=" '{print $2}'`
			DBNAME=`grep "DBName" $TGT_CONFIG_FILE | awk -F"=" '{print $2}'`
			MOUNTPOINT=`grep "MountPoint" $TGT_CONFIG_FILE |awk -F"=" '{print $2}'|awk -F":" '{print $4}'`
		fi
		
		echo "Shutting down $DBNAME of $DB_INSTANCE"		
		DB2ShutdownDatabase $DB_INSTANCE $DBNAME
    done
    for TGT_CONFIG_FILE in `echo $TGT_CONFIG_FILES | sed 's/:/ /g'`
    do
		tgtext=`echo $TGT_CONFIG_FILE | awk -F"." '{print $NF}'`
        if [ $tgtext = "xml" ];then		
			MOUNTPOINT=`grep "MountPoint" $TGT_CONFIG_FILE |awk -F">" '{print $2}'|awk -F"</" '{print $1}'|awk -F":" '{print $4}'`
        else
            MOUNTPOINT=`grep "MountPoint" $TGT_CONFIG_FILE |awk -F"=" '{print $2}'|awk -F":" '{print $4}'`
        fi

		mount | grep -w $MOUNTPOINT > $NULL
		if [ $? -eq 0 ]; then
			$ECHO "Unmounting $MOUNTPOINT"
			umount $MOUNTPOINT
			if [ $? -ne 0 ]; then
				echo "umount $MOUNTPOINT failed"
				exit 1 
			fi
		fi				
	done	
	
	log "EXIT TakeDatabaseOffline"			
}

#Done
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
    srcext=`echo $SRC_CONFIG_FILE | awk -F"." '{print $NF}'`
	
	if [ $srcext = "xml" ];then
		SOURCE_MOUNTPOINT=`grep 'MountPoint' $SRC_CONFIG_FILE | awk -F">" '{print $2}' |awk -F"</" '{print $1}'|awk -F":" '{print $NF}'` 
	else
		SOURCE_MOUNTPOINT=`grep 'MountPoint' $SRC_CONFIG_FILE | awk -F"=" '{print $2}'|awk -F":" '{print $NF}'`
	fi    

    if [ $OSTYPE = "Linux" ]; then
        mount |awk -F" " '{print $3 }'>$tempfile
	elif [ $OSTYPE = "AIX" ] ; then
        mount | awk '{print $3}' > $tempfile
    fi	

	flag=0
   
   	cat $tempfile|grep -w "$SOURCE_MOUNTPOINT" >>/dev/null
	if [ $? -eq 0 ]; then
        grep FileSystemType=$FsType $INSTALLPATH/etc/Db2TgtVolInfoFile.dat | grep "MountPoint=$mountpoint" > /dev/null
        if [ $? -ne 0 ]; then
            echo "MountPoint $mountpoint already present on node on device other than selected target device"
            log "MountPoint $mountpoint already present on node on device other than selected target device"
            flag=1 
        fi
    fi   
	
	rm -f $tempfile > $NULL    

    if [ $flag -ne 0 ];then
        exit 1
    fi

    log "EXIT checkMountPoint"

    return
}

#TODO check this function its only for AIX
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

    srcext=`echo $SRC_CONFIG_FILE | awk -F"." '{print $NF}'`
	
	if [ $srcext = "xml" ];then
		SOURCE_VGS=`grep "VolumeGroup" $SRC_CONFIG_FILE | awk -F">" '{print $2}' |awk -F"</" '{print $1}'` 
	else
		SOURCE_VGS=`grep "VolumeGroup=" $SRC_CONFIG_FILE | awk -F: '{print $2}'` 
	fi    
	
	#Check for VolumeGroup entry in $SRC_CONFIG_FILE
    

    flag=0
    for volGroup in $SOURCE_VGS
    do
        grep -w "$volGroup" $tempfile > /dev/null
        if [ $? -eq 0 ]; then
            flag=1 
            grep "DiskGroup=$volGroup" $INSTALLPATH/etc/DB2TgtVolInfoFile.dat > /dev/null
            if [ $? -ne 0 ]; then
                echo "VolumeGroup $volGroup already present on target node on device other than selected target device"
                log "VolumeGroup $volGroup already present on target node on device other than selected target device"
            fi
        fi
    done

    if [ $flag -ne 1 ];then
         for volGroup in $SOURCE_VGS
         do
             grep -w "$volGroup" $tempfile2 > /dev/null
             if [ $? -eq 0 ];then
                 echo "WARNING: VolumeGroup $volGroup already present on target node which is inactive"
                 log "WARNING: VolumeGroup $volGroup already present on target node which is inactive"
             fi
         done
    fi

    rm -f $tempfile > $NULL    
    rm -f $tempfile1 > $NULL
    rm -f $tempfile2 > $NULL

    if [ $flag -ne 0 ];then
        exit 1
    fi

    log "EXIT checkVolumeGroups"

    return
}

#Done
VerifyTgtReadiness()
{
	log "ENTER VerifyTgtReadiness..."
	
	# For each of the Db2Databases check if the DBVersion is same as the source
	if [ ! -f $SRC_CONFIG_FILE ]; then
        log "Error: src config file '$SRC_CONFIG_FILE' is not found"
        exit 1
	fi	
	
	srcext=`echo $SRC_CONFIG_FILE | awk -F"." '{print $NF}'`
	if [ $srcext = "xml" ];then
		SOURCE_DB_VERSION=`grep "DBVersion" $SRC_CONFIG_FILE|awk -F">" '{print $2}'|awk -F"<" '{print $1}'`
		SOURCE_DBINST=`grep "DBInstance" $SRC_CONFIG_FILE|awk -F">" '{print $2}'|awk -F"<" '{print $1}'|uniq`
		SOURCE_DATABASE=`grep "DBName" $SRC_CONFIG_FILE|awk -F">" '{print $2}'|awk -F"<" '{print $1}'|uniq`
    else
		SOURCE_DB_VERSION=`grep "DBVersion" $SRC_CONFIG_FILE| awk -F"=" '{print $2}'`
		SOURCE_DBINST=`grep "DBInstance" $SRC_CONFIG_FILE| awk -F"=" '{print $2}'`
		SOURCE_DATABASE=`grep "DBName" $SRC_CONFIG_FILE| awk -F"=" '{print $2}'`
	fi
	
    if [ -z "$SOURCE_DB_VERSION" ]
    then
        log "Error: DBVersion is Null in Source Configuration File"
        $ECHO "DBVersion is Null in Source Configuration File"
        exit 1
    fi	
	
	TARGET_DBINST=`ps -ef | grep db2sysc | grep -v grep | awk '{ print $1}'`
	numtgtdbinst=`echo $TARGET_DBINST|wc -w`	

    l_isInstanceRunning=0

	if [ ! -z "$TARGET_DBINST" ]; then		
		for DB2INS in $TARGET_DBINST
		do			
			if [ $SOURCE_DBINST != $DB2INS ];then
                continue
            else 
                l_isInstanceRunning=1
				log "Protected instance $SOURCE_DBINST is running on target"
				TARGET_DB_VERSION=`su - $DB2INS -c "db2licm -v"`
				if [ $SOURCE_DB_VERSION = $TARGET_DB_VERSION ]; then
					log "Source and target DB versions are same for DB2Instance: $DB2INS"					
				else
				   log "Source and Target DB versions are not matching "
				   log "Source DB version is $SOURCE_DB_VERSION"
				   log "Target DB version is $TARGET_DB_VERSION"
				fi
				
				TMP_FILE=/tmp/tgtdb.$$
				su - $DB2INS -c "db2 list db directory" >> $TMP_FILE
				if [ $? -ne 0 ];then
					log "No databases are present in the DB2Instance: $DB2INS"
					break
				fi
			
				NUM_DB=`grep "Database name" $TMP_FILE | wc -l `	
  
				# Get the Database Names  
				# Here each DB2 instance may have number of Databases.
				DB_NAMES=`grep "Database name" $TMP_FILE | awk -F"=" '{print $2}'`			
			
				i=1
				while [[ $i -le $NUM_DB ]]
				do
					DB_NAME=`echo $DB_NAMES | cut -d" " -f$i`						
					if [ $SOURCE_DATABASE = $DB_NAME ];then
						log " Protected DB : $SOURCE_DATABASE is present on the target machine under the instance : $DB2INS "
						isDbExists=1
					fi											
					((i=i+1))
				done
			fi		
						
			if [ $isDbExists -eq 1 ];then
				log " Protected DB : $SOURCE_DATABASE is already present on the target machine under instance : $DB2INS, drop the database"				
				echo " Protected DB : $SOURCE_DATABASE is already present on the target machine under instance : $DB2INS, drop the database"				
			fi
		done
	else
		log "No Instances are running on target machine......."
		exit 1
	fi
	
	if [ $srcext = "xml" ];then
		mntpt=`grep 'MountPoint' $SRC_CONFIG_FILE | awk -F">" '{print $2}' |awk -F"</" '{print $1}'| awk -F":" '{print $1}'` 
	else
		mntpt=`grep 'MountPoint' $SRC_CONFIG_FILE | awk -F"=" '{print $2}'|awk -F":" '{print $1}'`
	fi

    if [ $l_isInstanceRunning -eq 0 ];then
        log "ERROR: $SOURCE_DBINST is not running on the target ......."
        echo "ERROR: $SOURCE_DBINST is not running on the target ......."
        exit 1
    fi
	
	for eachmountpoint in $mntpt
	do
		if [ $eachmountpoint = "ufs" ];then
			log "Filesystem type is ufs"
			checkMountPoint ufs
		elif [ $eachmountpoint = "ext3" ];then
			log "Filesystem type is ext3"
			checkMountPoint ext3
		elif [ $eachmountpoint = "jfs2" ];then
			log "Filesystem type is jfs2"
			checkMountPoint jfs2 		 
		fi
	done
	
    #grep 'VolumeGroup' $SRC_CONFIG_FILE > $NULL
	if [ $srcext = "xml" ];then
		volumegroup=`grep 'MountPoint' $SRC_CONFIG_FILE | awk -F">" '{print $2}' |awk -F"</" '{print $1}'| awk -F":" '{print $3}'` 
	else
		volumegroup=`grep 'MountPoint' $SRC_CONFIG_FILE | awk -F"=" '{print $2}'|awk -F":" '{print $3}'`
	fi
	
    if [ ! -z $volumegroup ]; then
        log "checking for VolumeGroups..."
        checkVolumeGroups            
    fi

   	log "EXIT VerifyTgtReadiness"
	
}

GetNodeDetails()
{
    log "ENTER GetNodeDetails..."	
	
	HOSTNAME=`hostname`    
	WriteToConfFile "MachineName" $HOSTNAME

    hostId=`cat $INSTALLPATH/etc/drscout.conf | grep -i HostID | awk -F"=" '{print $2}' | sed "s/ //g"`
    if [ -z "$hostId" ] ; then
        log "Error: Host id not found in file $INSTALLPATH/etc/drscout.conf"
        exit 1 
    fi
    
	WriteToConfFile "HostID" $hostId

    log "EXIT GetNodeDetails"

    return
}

WriteToConfFile()
{
	TAG=$1
	VALUE=$2
	if [ $isConfFile -eq 1 ]; then
		echo "<$TAG>$VALUE</$TAG>" >> $OUTPUT_FILE
	else
		echo "$TAG=$VALUE" >> $OUTPUT_FILE
	fi	
}


######### Main ##########

trap Cleanup 0

# trap CleanupError SIGHUP SIGINT SIGQUIT SIGTERM -  SLES doesn't know the definitions
if [ "$OSTYPE" = "Linux" -o "$OSTYPE" = "AIX" ]; then
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

GetParentProcName

DB2ConfFile=$INSTALLPATH/etc

if [ $OPTION = "discover" ]; then	
	log "Db2DatabaseDiscoveryStart"
	GetNodeDetails 
	DiscoverDatabases 
	log "Db2DatabaseDiscoveryEnd"	

elif [ $OPTION = "preparetarget" ]; then

    TakeDatabaseOffline

elif [ $OPTION = "targetreadiness" ]; then
    
	VerifyTgtReadiness

fi

exit 0
