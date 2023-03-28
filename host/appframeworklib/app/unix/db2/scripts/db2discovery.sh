#!/bin/bash
#=================================================================
# FILENAME
#    db2discovery.sh
#
# DESCRIPTION
#	Discover the DB2 database info
#
# HISTORY
#     <5/12/2012>  Anusha T     Created.
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================

PATH=/bin:/usr/bin:/usr/sbin:/sbin:$PATH
ECHO=/bin/echo
SED=/bin/sed
OSTYPE=`uname -s`

Usage()
{
    $ECHO "usage: $0 <DB2_INS> <DB2_VER> <OUTFILE> "
    return
}

log()
{
    $ECHO "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ db2discovery $*" >> $LOG_FILE
    return
}

ParseArgs()
{
    if [ $# -ne 3 ]; then
        Usage
        exit 1
    fi

    dirName=`dirname $0`
    orgWd=`pwd`

    cd $dirName
    SCRIPT_DIR=`pwd`
    cd $orgWd

    LOG_FILE=/var/log/db2out.log

    DB2_INS=$1	
	DB2_VER=$2 
    OUTPUT_FILE=$3
	
    > $OUTPUT_FILE	
	ID=/usr/bin/id
	
	id $DB2_INS > /dev/null
    if [ $? -ne 0 ]; then
        $ECHO "Error: DB2 Instance $DB2_INS not found"
        log "Error: DB2 Instance $DB2_INS not found"
        exit 1
    fi
	
	if [ ! -f "$LOG_FILE" ]; then
        > $LOG_FILE
    fi
	
}

logXml()
{
  TAG_NAME=$1
  TAG_VALUE=$2
  CONF_FILE=$3
 
  ext=`echo $CONF_FILE|awk -F"." '{print $NF}'`
  if [[ $ext = "xml" ]];then
	echo "<$TAG_NAME>$TAG_VALUE</$TAG_NAME>">>$CONF_FILE
  else
    echo "$TAG_NAME=$TAG_VALUE">>$CONF_FILE
  fi
  
  return  
}

GetPVForAIX()
{
	mnt_pt=$1
	DB_CONF_FILE=$2
	
	lv_name=""
	vg=""
	pv_list=""
	
	device=`df | grep "$mnt_pt"$ | awk -F" " '{print $1}'`
	if [ "$device" == "" ]; then
		echo "Wrong mount point $mnt_pt"
		exit 1
	fi
	
	# Capture the output of mount command and find the log device for the mount point
	mount > /tmp/mount$$
	found=0
	while read line
	do
		first_field=`echo $line | awk -F" " '{print $1}'`
		second_field=`echo $line | awk -F" " '{print $2}'`
		if [ "$first_field" = "$device" ] || [ "$second_field" = "$device" ]; then
			found=1
			break;
		fi
	done < /tmp/mount$$

	rm -f /tmp/mount$$

	last_field=`echo $line | awk -F" " '{print $NF}'`

	device_name=`basename $device`
	i=1
	while true
	do
        param=`echo $last_field | cut -d"," -f${i}`

        echo $param | grep "log=" > /dev/null
        ret=$?
        if [ $ret -eq "0" ]; then
                break
        fi

        i=`expr $i + 1`
	done
	log_dev=`echo $param | cut -d"=" -f2`

	# Capture the list of PVs in a temporary file
	lspv > /tmp/lspv$$

	# Check whether the device is PV or not
	while read line
	do
		pv_name=`echo $line | awk -F" " '{print $1}'`
		if [ $pv_name == $device_name ]; then
			echo "::${device}:${mnt_pt}:${log_dev}"
			rm -f /tmp/lspv$$
			exit 0
		fi
	done < /tmp/lspv$$

	# It is an LV.
	lv_dev=$device
	lv_name=$device_name
	disk=`lslv -l $lv_name | tail -1 | awk -F" " '{print $1}'`

	while read line
	do
		pv_name=`echo $line | awk -F" " '{print $1}'`
		if [ $pv_name == $disk ]; then
			vg_name=`echo $line | awk -F" " '{print $3}'`
			break
		fi
	done < /tmp/lspv$$

	vg=$vg_name
	pv_list=/dev/${disk}

	# Get all other PVs associacted with this VG
	while read line
	do
		pv_name=`echo $line | awk -F" " '{print $1}'`
		if [ $pv_name == $disk ]; then
			continue
		fi
		vg_name=`echo $line | awk -F" " '{print $3}'`
		if [ $vg_name == $vg ]; then
			pv_list=`echo ${pv_list},/dev/${pv_name}`
		fi
	done < /tmp/lspv$$
	
	logXml "PVDevices" "${lv_dev}:${vg}:${pv_list}:${mnt_pt}:${log_dev}" $DB_CONF_FILE
	logXml "PVs" $pv_list $DB_CONF_FILE
	echo "${lv_dev}:${vg}:${pv_list}:${mnt_pt}:${log_dev}"
	rm -f /tmp/lspv$$
	return
}

GetPVForLinux()
{
	mnt_pt=$1
	DB_CONF_FILE=$2
	PVDISPLAY=/tmp/pvdisplay.$$
	LVDISPLAY=/tmp/lvdisplay.$$
	
	lv_name=""
	vg=""
	pv_list=""
	
	device=`df -P $mnt_pt | tail -1 | awk -F" " '{print $1}'`
	if [ "$device" == "" ]; then
		echo "Wrong mount point $mnt_pt"
		exit 1
	fi
	
	device_name=`basename $device`
		
	lv_dev=$device	
	lvdisplay $logicalvolume >$LVDISPLAY 2>&1
	vg=`grep "VG Name" $LVDISPLAY |awk '{print $NF}'`
	
	#vg=`lvdisplay $lv_dev | grep "VG Name" | awk -F" " '{print $NF}'`
	pvs >$PVDISPLAY 2>&1
	pvs=`cat $PVDISPLAY | grep $vg | awk '{print $1}'`
	for pv in $pvs
	do
		if [ -z "$pv_list" ];then
			pv_list=$pv
		else
			pv_list=`echo $pv_list,$pv`
		fi
	done
	
	logXml "PVDevices" "${lv_dev}:${vg}:${pv_list}:${mnt_pt}" $DB_CONF_FILE
	logXml "PVs" $pv_list $DB_CONF_FILE
	
	log "PVDevices : ${lv_dev}:${vg}:${pv_list}:${mnt_pt}"
	log "PVs : $pv_list"
	
	echo "PVDevices="${lv_dev}:${vg}:${pv_list}:${mnt_pt}"" >> $OUTPUT_FILE	
	echo "PVs=$pv_list" >> $OUTPUT_FILE
	
	rm -f $PVDISPLAY
	rm -f $LVDISPLAY
	
}

GetDbDetails()
{
  log "Database instance name : $DB2_INS"
  
  HostName=`hostname`	

  log "Application :DB2"
  log "MachienName : $HostName"
  log "SourceHostId : $hostId"
  log "DBInstance : $DB2_INS"
  log "DBVersion : $DB2_VER"	
		
  INSTALL_PATH=`su - $DB2_INS -c "db2level" | grep "installed at" | awk '{print $NF}' | awk -F\" '{print $(NF-1)}'`
  TMP_DB_OUTPUT=/tmp/test
  su - $DB2_INS -c "db2 list active databases" > $TMP_DB_OUTPUT 
  
  if [ $? -ne 0 ];then
	log "No databases are active for the DB2Instance: $DB2_INS"
	return
  fi  
  
  NUM_DB=`grep "Database name" $TMP_DB_OUTPUT | wc -l ` 
  
  log "Number of Active Databases on $DB2_INS : $NUM_DB"    
  
  # Get the Database Names  
  # Here each DB2 instance may have number of Databases.
  DB_NAMES=`grep "Database name" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'`
  DB_LOCS=`grep "Database path" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'`  
    
  while read line
  do
     case $line in
      *name*)
        DB_NAME=`echo $line | awk -F"=" '{print $NF}'|sed 's/ //g'`
        read line
        read line
        DB_LOC=`echo $line | awk -F"=" '{print $NF}'|sed 's/ //g'`
		mount_point=`df -P $DB_LOC | tail -1 |awk '{print $NF}'`
	
		LVDISPLAY=/tmp/lvdisplay.$DB_NAME	
	
		if [ "$OSTYPE" = "Linux" ];then
			logicalvolume=`df -P $mount_point | grep -v Filesystem | awk '{print $1}'`
			fstype=`df -PT $mount_point | grep -v Type | awk '{ print $2 }'`
			lvdisplay $logicalvolume >$LVDISPLAY 2>&1
			volumegroup=`grep "VG Name" $LVDISPLAY |awk '{print $NF}'`
			#volumegroup=`lvs $logicalvolume|tail -1|awk '{print $2}'`
		elif [ "$OSTYPE" = "AIX" ];then
			logicalvolume=`df -P $mount_point | grep -v Filesystem | awk '{print $1}'`
			#This is just for the sake of displaying lslv
			logical_volume=`df -P $DB_LOC | grep -v Filesystem | awk '{print $1}'|awk -F"/" '{print $NF}'`	
			fstype=`/usr/sysv/bin/df -n $mount_point | awk -F":" '{print $2}'`
			lslv $logical_volume >$LVDISPLAY
			volumegroup=`grep "VOLUME GROUP" $LVDISPLAY|awk -F":" '{print $NF}'|sed 's/ //g'`
		fi	
	
		TEMP_FILE=/tmp/$DB_NAME
		
            su - $DB2_INS -c "db2 <<END >$TEMP_FILE
				get db cfg for $DB_NAME 
		    END"
				
		LogMode=`grep "LOGARCHMETH1" $TEMP_FILE | awk -F"=" '{print $NF}'`					
	
		rm -f $TEMP_FILE
		
		#Filename is modified as two instances can have similar dbnames.
		DB_CONF_FILE=$DB2ConfFile/DB2_$DB2_INS.$DB_NAME.conf		
	
		rm -f $DB_CONF_FILE
		rm -f $LVDISPLAY
	
		hostId=`cat $INMAGE_DIR/etc/drscout.conf | grep -i HostID | awk -F"=" '{print $2}' | sed "s/ //g"`
	
		logXml "APPLICATION" "DB2" $DB_CONF_FILE
		logXml "MachineName" $HostName $DB_CONF_FILE
		logXml "SourceHostId" $hostId $DB_CONF_FILE
		logXml "DBInstance" $DB2_INS $DB_CONF_FILE		
		logXml "DBInstallationPath" $INSTALL_PATH $DB_CONF_FILE
		logXml "DBVersion" $DB2_VER $DB_CONF_FILE		
  		logXml "DBName" $DB_NAME $DB_CONF_FILE
		logXml "DBLocation" $mount_point $DB_CONF_FILE
		logXml "RecoveryLogEnabled" $LogMode $DB_CONF_FILE
		logXml "MountPoint" "$fstype:$logicalvolume:$volumegroup:$mount_point" $DB_CONF_FILE
		logXml "VolumeGroup" $volumegroup $DB_CONF_FILE
		
		log "Database name : $DB_NAME"
		log "Database Location : $mount_point"
		log "RecoveryLogMode : $LogMode" 
		log "MountPoint : $fstype:$logicalvolume:$volumegroup:$mount_point"
		log "VolumeGroup : $volumegroup"		
		
		echo "DBName=$DB_NAME" >> $OUTPUT_FILE 
		echo "DBLocation=$mount_point" >> $OUTPUT_FILE 
		echo "RecoveryLogMode=$LogMode" >> $OUTPUT_FILE 
		echo "MountPoint="$fstype:$logicalvolume:$volumegroup:$mount_point"" >> $OUTPUT_FILE 
		echo "VolumeGroup=$volumegroup" >> $OUTPUT_FILE 
		
		if [ $OSTYPE = "Linux" ];then
			GetPVForLinux $mount_point $DB_CONF_FILE 
		elif [ $OSTYPE = "AIX" ];then
			GetPVForAIX $mount_point $DB_CONF_FILE
		fi	
	esac	
  done < $TMP_DB_OUTPUT
  rm -f $TMP_DB_OUTPUT
  return
}

#########################################################################
#      Main
#########################################################################
ParseArgs $*

log "DB2 User: $DB2_INS"

INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
DB2ConfFile=$INMAGE_DIR/etc

if [ "${DB2_INS}" != "" ];then
    DB2_SID="${DB2_INS}"
    export DB2_SID
else
    $ECHO "The DB2 instance is not available."
    log "The DB2 instance is not available."
    exit 1
fi

GetDbDetails

exit 0
