#!/bin/bash
#=================================================================
# FILENAME
#    db2appagent.sh
#
# DESCRIPTION
# 1. If consitency tag should be added on DB Volumes of all db instances then, at <DB Names> we can give "all"
# 2. If consitency tag should be added on DB Volumes of a particular instance then, at <DB Names> we can give <DB Instance Name>
#    In this case all the Databases under DB Instance will be considered while issuing tag.
# 3. If consitency tag should be added on DB Volumes of a particular DB then, at <DB Names> we can give <DB name>
#    
# HISTORY
#     <10/12/2012>  Anusha Created.
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================

PLATFORM=`uname | tr -d "\n"`
NULL=/dev/null

TEMP_FILE=/tmp/tempfile.$$

Usage()
{
    echo ""
    echo "usage: $0 <action> <DBInstance> <DBNames> <TagName> <Volumes>"
    echo ""
    echo "where action can be one of the follwing:"
    echo "consistency           - To issue consistency tag on DB volumes"
    echo "                        Note: only to be used with appservice"
    echo "validate              - To validate if the tag can be issued on the database"
    echo "                        Note: In Place of Volumes source file needs to be given"
	echo "freeze                - To freeze the DBs specified"
	echo "                        all to freze all DBs"	
	echo "unfreeze              - To unfreeze the DBs specified"
	echo "                        all to unfreze all DBs"
	echo "failover				- To perform failover "
	echo ""
	echo "Note: At a time one Db Instance is only accepted. "
	echo "If multiple DBNames are to be given as input, give them as colon seperated"
	echo "If action should be performed on all dbs in one instance , then give "ALL" in place of DBNames"
    return
}

log()
{
    echo "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ db2_consistency:$ACTION $*" >> $LOG_FILE
    return
}

Cleanup()
{
    rm -f $tempfile1 > $NULL
}

CleanupError()
{
	rm -f $tempfile1 > $NULL
	exit 1
}

logXml()
{
  TAG_NAME=$1
  TAG_VALUE=$2
  CONFIG_FILE=$3
  
  echo "<$TAG_NAME>$TAG_VALUE</$TAG_NAME>" >> $CONFIG_FILE
  return  
}

ParseArgs()
{
    if [ $# -ne 5 ]; then
        if [ $# -ne 4 ];then
          Usage
          exit 1
        fi
    fi

    if [ $1 != "consistency"  -a $1 != "failover" -a $1 != "validate"  -a $1 != "freeze" -a $1 != "unfreeze" ]; then
        Usage
        exit 1
    fi

    ACTION=$1
    DB_Instance=$2
    isInst=0
    #This in the case where dbname is given along with instance name
    NUMDB=`echo $DB_Instance |awk -F":" '{print NF}'`
        if [ $NUMDB -eq 1 ];then
		DB_INST=$DB_Instance
		isInst=1
	else	
		DB_NAMES=
		i=1
		while [[ $i -le $NUMDB ]]
		do
			DB_INST=`echo $DB_Instance | cut -d":" -f$i`			
			((i=i+1))
			DB=`echo $DB_Instance | cut -d":" -f$i`
			echo $DB
			if [ -z $DB_NAMES ];then
				DB_NAMES=$DB
			else
				DB_NAMES=$DB_NAMES:$DB
			fi
			((i=i+1))
		done			
	fi	
	if [ $isInst -eq 1 ];then
		DB_NAMES=$3
		TAG=$4

		if [ $1 = "validate" ]; then
			SRC_CONF_FILE=$5
		else
			VOLUMES=$5
		fi
	else
		TAG=$3
		if [ $1 = "validate" ]; then
			SRC_CONF_FILE=$4
		else
			VOLUMES=$4
		fi
	fi

    INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
	
    if [ -z "$INMAGE_DIR" ]; then
        exit 1
    fi

    dirName=`dirname $0`
    orgWd=`pwd`

    cd $dirName
    SCRIPT_DIR=`pwd`
    cd $orgWd

    LOG_FILE=/var/log/db2out.log

    isDbFound=0

    return
}

GetDbInfo()
{
	DB_CONF_FILES=
	
	if [ `echo $DB_INST | tr '[A-Z]' '[a-z]'` = "all" ];then
		DB_INSTANCES=`ps -ef | grep db2sysc | grep -v grep | awk '{ print $1}'`
		for DB_INS in $DB_INSTANCES
		do
			#Get the DB instance location
			DB_INS_LOC=`su - $DB_INS -c "db2path"`
			TMP_DB_OUTPUT=/tmp/out
		
			# Get the version of the DB2 instance
			DB_VER=`su - $DB_INS -c "db2licm -v"` 
  			su - $DB_INS -c "db2 list active databases" > $TMP_DB_OUTPUT
		
			if [ $? -ne 0 ];then
				log "No databases are active in the DB2Instance: $DB_INS"
				continue
			fi
			NUM_DB=`grep "Database name" $TMP_DB_OUTPUT | wc -l `		
		
			# Here each DB2 instance may have number of Databases.
			DB_NAME=`grep "Database name" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'`
			DB_LOC=`grep "Database path" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'`
  			
			i=1
			while [[ $i -le $NUM_DB ]]			
			do			    
				db_name=`echo $DB_NAME|cut -d" " -f$i`
				db_loc=`echo $DB_LOC|cut -d" " -f$i`
				mount_point=`df -P $db_loc | grep -v "Filesystem" | awk '{print $1}'`
		
				if [[ `echo $DB_NAMES | tr '[A-Z]' '[a-z]'|sed 's/:/ /g'` = "all" ]]; then
					log "Reading info of $db_name"
					isDbFound=1
					
					DB_CONF_FILE="$INMAGE_DIR/etc/$DB_INS.$db_name.xml"				
			
					rm -f $DB_CONF_FILE
					
					log "DB Instance=$DB_INS"
					log "DB Instance Location=$DB_INST_LOC"
		
					log "DB Name=$db_name"
					log "DB Location=$DB_LOC"			
					
					logXml "DBUser" $DB_INS $DB_CONF_FILE
					logXml "DBName" $db_name $DB_CONF_FILE
					logXml "DBLocation" $db_loc $DB_CONF_FILE
					logXml "MountPoint" $mount_point $DB_CONF_FILE
					
					if [ -z "$DB_CONF_FILES" ]; then
						DB_CONF_FILES=$DB_CONF_FILE
					else
						DB_CONF_FILES=$DB_CONF_FILES:$DB_CONF_FILE
					fi		
				else				
					for eachdatabase in `echo $DB_NAMES|sed 's/:/ /g'`
					do
						if [ $eachdatabase = $db_name ]; then
							log "Reading info of $eachdatabase"
					
							isDbFound=1
				
							DB_CONF_FILE="$INMAGE_DIR/etc/$DB_INS.$db_name.xml"				
				
							rm -rf $DB_CONF_FILE
					
							log "DB Instance=$DB_INS"
							log "DB Instance Location=$DB_INST_LOC"
			
							log "DB Name=$db_name"
							log "DB Location=$DB_LOC"			
				
							logXml "DBUser" $DB_INS $DB_CONF_FILE
							logXml "DBName" $db_name $DB_CONF_FILE
							logXml "DBLocation" $db_loc $DB_CONF_FILE
							logXml "MountPoint" $mount_point $DB_CONF_FILE
				
							if [ -z "$DB_CONF_FILES" ]; then
								DB_CONF_FILES=$DB_CONF_FILE
							else
								DB_CONF_FILES=$DB_CONF_FILES:$DB_CONF_FILE
							fi										
						fi	
						if [ $isDbFound -eq 0 ];then
							log "Database $eachdatabase is not in active state"
							echo "Database $eachdatabase is not in active state"
						fi
					done
				fi
				((i=i+1))
			done  
		rm -f $TMP_DB_OUTPUT		
		done
	else
		#Get the DB instance location
		DB_INS_LOC=`su - $DB_INST -c "db2path"`
		TMP_DB_OUTPUT=/tmp/out
		
		# Get the version of the DB2 instance
		DB_VER=`su - $DB_INST -c "db2licm -v"` 
  		su - $DB_INST -c "db2 list active databases" > $TMP_DB_OUTPUT
		
		if [ $? -ne 0 ];then
			log "No databases are active in the DB2Instance: $DB_INST"
			continue
		fi
		NUM_DB=`grep "Database name" $TMP_DB_OUTPUT | wc -l `		
		
		# Here each DB2 instance may have number of Databases.
		DB_NAME=`grep "Database name" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'`
		DB_LOC=`grep "Database path" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'`
  		
		i=1
		while [[ $i -le $NUM_DB ]]
		do
			db_name=`echo $DB_NAME|cut -d" " -f$i`
			db_loc=`echo $DB_LOC|cut -d" " -f$i`
			mount_point=`df -P $db_loc | grep -v "Filesystem" | awk '{print $1}'`
			if [[ `echo $DB_NAMES | tr '[A-Z]' '[a-z]'|sed 's/:/ /g'` = "all" ]]; then
				log "Reading info of $db_name"
				isDbFound=1
				DB_CONF_FILE="$INMAGE_DIR/etc/$DB_INST.$db_name.xml"				
			
				rm -f $DB_CONF_FILE
					
				log "DB Instance=$DB_INST"
				log "DB Instance Location=$DB_INST_LOC"
		
				log "DB Name=$db_name"
				log "DB Location=$DB_LOC"			
					
				logXml "DBUser" $DB_INST $DB_CONF_FILE
				logXml "DBName" $db_name $DB_CONF_FILE
				logXml "DBLocation" $db_loc $DB_CONF_FILE
				logXml "MountPoint" $mount_point $DB_CONF_FILE
				
				if [ -z "$DB_CONF_FILES" ]; then
					DB_CONF_FILES=$DB_CONF_FILE
				else
					DB_CONF_FILES=$DB_CONF_FILES:$DB_CONF_FILE
				fi		
			else				
				for eachdatabase in `echo $DB_NAMES|tr '[a-z]' '[A-Z]'|sed 's/:/ /g'`
				do
					if [ $eachdatabase = $db_name ]; then
						log "Reading info of $eachdatabase"
					
						isDbFound=1
				
						DB_CONF_FILE="$INMAGE_DIR/etc/$DB_INST.$db_name.xml"				
				
						rm -rf $DB_CONF_FILE
				
						log "DB Instance=$DB_INST"
						log "DB Instance Location=$DB_INST_LOC"
			
						log "DB Name=$db_name"
						log "DB Location=$DB_LOC"			
				
						logXml "DBUser" $DB_INST $DB_CONF_FILE
						logXml "DBName" $db_name $DB_CONF_FILE
						logXml "DBLocation" $db_loc $DB_CONF_FILE
						logXml "MountPoint" $mount_point $DB_CONF_FILE
				
						if [ -z "$DB_CONF_FILES" ]; then
							DB_CONF_FILES=$DB_CONF_FILE
						else
							DB_CONF_FILES=$DB_CONF_FILES:$DB_CONF_FILE
						fi			
					fi			
				done
			fi
			((i=i+1))
		done  
	fi		
	
}

DB2Freeze()
{
    if [ -z "$DB_CONF_FILES" ]; then
        echo "No active DB2 databases found to suspend."
        if [ "$ACTION" = "freeze" ];then
            exit 2
        else
            return
        fi
    fi

	echo "Keeping the database(s) in write suspend mode.."
    echo "================"
	
    freezeStatus=0
    frozenDbs=
	for DB_CONF_FILE in `echo $DB_CONF_FILES | sed 's/:/ /g'`
	do
		DB_NAME=`grep DBName $DB_CONF_FILE | awk -F">" '{ print $2 }'|awk -F"</" '{print $1}'`
		DB_LOC=`grep DBLocation $DB_CONF_FILE | awk -F">" '{ print $2 }'|awk -F"</" '{print $1}'`
		DB_USER=`grep DBUser $DB_CONF_FILE | awk -F">" '{ print $2 }'|awk -F"</" '{print $1}'`
		
		su - $DB_USER -c "db2 <<END >$TEMP_FILE
           connect to $DB_NAME
           set write suspend for database
		commit
		disconnect $DB_NAME
END" 
		
		
		grep -w "SQLSTATE=42705" $TEMP_FILE>>/dev/null
		if [ $? -eq 0 ]
		then
			echo "WARNING: The database $DB_NAME is not active or could not be connected"
			echo "Cannot freeze the database.."
			cat $TEMP_FILE
			rm -f $TEMP_FILE
            freezeStatus=1
			break
		fi
		grep -w " The SET WRITE command completed successfully." $TEMP_FILE>>/dev/null
		if [ $? -ne 0 ]
		then
			echo "Failed to Freeze the database $DB_NAME.."
			cat $TEMP_FILE
			rm -f $TEMP_FILE
            freezeStatus=1
			break
		else
			echo "write suspend succeeded for database $DB_USER.$DB_NAME.."
		fi 	
		
		rm -f $TEMP_FILE

        if [ -z "$frozenDbs" ]; then
            frozenDbs="${DB_CONF_FILE}"
        else
            frozenDbs="${frozenDbs}:${DB_CONF_FILE}"
        fi
    done	

    if [ $freezeStatus -eq 1 ]; then
        DB_CONF_FILES=${frozenDbs}
        DB2UnFreeze
        exit 1
    fi
}

DB2UnFreeze()
{
		
	if [ `echo $DB_INST | tr '[A-Z]' '[a-z]'` = "all" ];then
		dbIns=`ps -ef | grep db2sysc | grep -v grep | awk '{ print $1}'`
	
		for DB_INS in $dbIns
		do
			log "Unfreezing db2 instance: $DB_INS"
			TMP_LISTDB=/tmp/listdb
			TMP_FILE=/tmp/tmpinst_$DB_INS
			TMP_FILE1=/tmp/dbstate_$DB_INS
				
			if [[ `echo $DB_NAMES|tr '[A-Z]' '[a-z]'|sed 's/:/ /g'` = "all" ]];then
				
				su - $DB_INS -c "db2 list db directory" > $TMP_LISTDB		
				if [ $? -ne 0 ];then
					log "No databases are present in the DB2Instance: $DB_INS"
					continue
				fi
		
				NUM_DB=`grep "Database name" $TMP_LISTDB | wc -l `	
				DB_NAME=`grep "Database name" $TMP_LISTDB | awk -F"=" '{print $2}'`
				
				i=1
				while [[ $i -le $NUM_DB ]]
				do
					db_name=`echo $DB_NAME|cut -d" " -f$i`
					log "Unfreezing db2 database: $db_name"
					
					#Before putting the db in write resume state, check whether db is in write suspended state or not.
					su - $DB_INS -c "db2 <<END >$TMP_FILE1
						connect to $db_name
						list tablespaces					
						disconnect $db_name
END"
						
					grep -w "Write Suspended" $TMP_FILE1 >> /dev/null
					if [ $? -eq 0 ];then
						su - $DB_INS -c "db2 <<END >$TMP_FILE
						connect to $db_name
						set write resume for database
						commit
						disconnect $db_name
END"
					else						
						echo "$DB_INS.$db_name is not in write suspend mode, no need to resume it"					
                        ((i=i+1))
						continue
					fi
				
					grep -w " The SET WRITE command completed successfully." $TMP_FILE>>/dev/null
					if [ $? -ne 0 ]
					then
						echo "Failed to UnFreeze the database $db_name.."
						cat $TMP_FILE
						rm -f $TMP_FILE
						rm -f $TMP_FILE1
						exit 1
					else
						echo "write resume succeeded for database $DB_INS.$db_name.."
					fi
				 ((i=i+1))
				done
			else
				for db_name in `echo $DB_NAMES|sed 's/:/ /g'`
				do
					#Before putting the db in write resume state, check whether db is in write suspended state or not.
					su - $DB_INS -c "db2 <<END >$TMP_FILE1
						connect to $db_name
						list tablespaces					
						disconnect $db_name
END"
						
					grep -w "Write Suspended" $TMP_FILE1 >> /dev/null
					if [ $? -eq 0 ];then
						su - $DB_INS -c "db2 <<END >$TMP_FILE
						connect to $db_name
						set write resume for database
						commit
						disconnect $db_name
END"
					else						
						echo "$DB_INS.$db_name is not in write suspend mode, no need to resume it"					
						continue
					fi
				
					grep -w " The SET WRITE command completed successfully." $TMP_FILE>>/dev/null
					if [ $? -ne 0 ]
					then
						echo "Failed to UnFreeze the database $db_name.."
						cat $TMP_FILE
						rm -f $TMP_FILE
						rm -f $TMP_FILE1
						exit 1
					else
						echo "write resume succeeded for database $DB_INS.$db_name.."
					fi
				done
			fi
			rm -f $TMP_LISTDB
			rm -f $TMP_FILE
			rm -f $TMP_FILE1
		done
	else
		TMP_LISTDB=/tmp/listdb
		TMP_FILE=/tmp/tmpinst_$DB_INST
		TMP_FILE1=/tmp/dbstate_$DB_INST
		
		if [[ `echo $DB_NAMES|tr '[A-Z]' '[a-z]'|sed 's/:/ /g'` = "all" ]];then
				
			su - $DB_INST -c "db2 list db directory" > $TMP_LISTDB		
			if [ $? -ne 0 ];then
				log "No databases are present in the DB2Instance: $DB_INST"
				continue
			fi
		
			NUM_DB=`grep "Database name" $TMP_LISTDB | wc -l `	
			DB_NAME=`grep "Database name" $TMP_LISTDB | awk -F"=" '{print $2}'`
				
			i=1
			while [[ $i -le $NUM_DB ]]
			do
				db_name=`echo $DB_NAME|cut -d" " -f$i`
				log "Unfreezing db2 database: $db_name"
					
				#Before putting the db in write resume state, check whether db is in write suspended state or not.
				su - $DB_INST -c "db2 <<END >$TMP_FILE1
					connect to $db_name
					list tablespaces					
					disconnect $db_name
END"
						
				grep -w "Write Suspended" $TMP_FILE1 >> /dev/null
				if [ $? -eq 0 ];then
					su - $DB_INST -c "db2 <<END >$TMP_FILE
						connect to $db_name
						set write resume for database
						commit
						disconnect $db_name
END"
				else						
					echo "$DB_INST.$db_name is not in write suspend mode, no need to resume it"					
                    ((i=i+1))
					continue
				fi
				
				grep -w " The SET WRITE command completed successfully." $TMP_FILE>>/dev/null
				if [ $? -ne 0 ]
				then
					echo "Failed to UnFreeze the database $db_name.."
					cat $TMP_FILE
					rm -f $TMP_FILE
					rm -f $TMP_FILE1
					exit 1
				else
					echo "write resume succeeded for database $DB_INST.$db_name.."
				fi
				((i=i+1))
			done
		else
			for db_name in `echo $DB_NAMES|sed 's/:/ /g'`
			do
				#Before putting the db in write resume state, check whether db is in write suspended state or not.
				su - $DB_INST -c "db2 <<END >$TMP_FILE1
					connect to $db_name
					list tablespaces					
					disconnect $db_name
END"
					
				grep -w "Write Suspended" $TMP_FILE1 >> /dev/null
				if [ $? -eq 0 ];then
					su - $DB_INST -c "db2 <<END >$TMP_FILE
					connect to $db_name
					set write resume for database
					commit
					disconnect $db_name
END"
				else						
					echo "$DB_INST.$db_name is not in write suspend mode, no need to resume it"					
					continue
				fi
				
				grep -w " The SET WRITE command completed successfully." $TMP_FILE>>/dev/null
				if [ $? -ne 0 ]
				then
					echo "Failed to UnFreeze the database $db_name.."
					cat $TMP_FILE
					rm -f $TMP_FILE
					rm -f $TMP_FILE1
					exit 1
				else
					echo "write resume succeeded for database $DB_INST.$db_name.."
				fi
			done
		fi
		rm -f $TMP_LISTDB
		rm -f $TMP_FILE
		rm -f $TMP_FILE1
	fi				
		
    echo "================"
    echo "Keeping the database(s) in write resume mode is done.."
}

ValidateVacp()
{
	isVxfs=`grep isVxfs $SRC_CONF_FILE | awk -F">" '{print $2}'|awk -F"</" '{print $1}'`
	#grep "isVxfs=YES" $SRC_CONF_FILE >/dev/null
	
	if [ "$isVxfs" = "YES" ]
	then
        $INMAGE_DIR/bin/vacp | grep -w "vxfs" >/dev/null
        if [ $? -ne 0 ]
        then
            echo "ERROR: $INMAGE_DIR/bin/vacp cannot freeze vxfs filesystem to issue tags."
            DB2UnFreeze
			exit 1
	   fi
	fi
}

DB2VacpTagGenerate()
{
	volumes=
	DB_TAG=	
	for DB_CONF_FILE in `echo $DB_CONF_FILES | sed 's/:/ /g'`
    do
		echo $DB_CONF_FILE
		for volume in `grep "MountPoint" $DB_CONF_FILE| awk -F">" '{print $2}'|awk -F"</" '{print $1}'`	
        do
            echo $volume
			if [ -z "$volumes" ]; then
                volumes="$volume"
            else
                volumes="$volume,$volumes"
            fi
        done
		
		DB_NAME=`grep "DBName" $DB_CONF_FILE |awk -F">" '{print $2}'|awk -F"</" '{print $1}'`
		if [ -z "$DB_TAG" ]; then
            DB_TAG="$DB_NAME"
        else
            DB_TAG="${DB_NAME}_${DB_TAG}"
        fi
	done
	
	echo $volumes
	echo $VOLUMES
	echo $DB_TAG
	
	TAG_NAME=$TAG

	if [ "$ACTION" != "failover" ]; then
		if [ ! -z "$DB_TAG" ]; then
			TAG_NAME="${TAG}_${DB_TAG}"
		fi 
	fi
	
    tempfile1=/tmp/vacpout.$$
	
	echo $TAG_NAME
	
	if [ "$ACTION" != "failover" -a -z "$DB_TAG" ]; then
        $INMAGE_DIR/bin/vacp -v "${volumes},${VOLUMES}" >$tempfile1 2>&1
    else
        $INMAGE_DIR/bin/vacp -t $TAG_NAME -v "${volumes},${VOLUMES}" >$tempfile1 2>&1
    fi
    if [ $? -ne 0 ]
    then
        result=`cat $tempfile1`
        log "Result from DB2VacpTagGenerate: $result"
        echo "$result"
        echo "Vacp failed to issue tags.."
        DB2UnFreeze
        exit 1
    fi

    result=`cat $tempfile1`
    log "Result from DB2VacpTagGenerate: $result"
    echo "$result"

    rm -f $tempfile1	
}

################   MAIN   #############

trap Cleanup 0

# trap CleanupError SIGHUP SIGINT SIGQUIT SIGTERM -  SLES doesn't know the definitions
if [ "$PLATFORM" = "Linux" -o "$PLATFORM" = "AIX" ]; then
    trap CleanupError 1 2 3 15
else
    exit 0
fi

date
echo "Script $0 $* spawned on source host `hostname`"

ParseArgs $*

if [ $ACTION != "unfreeze" ];then
	GetDbInfo
fi

if [ "$ACTION" = "validate" ]; then
    # no need to issue a tag
	DB2Freeze
    ValidateVacp
	DB2UnFreeze
elif [ "$ACTION" = "freeze" ];then
	DB2Freeze
elif [ "$ACTION" = "unfreeze" ];then
	DB2UnFreeze
else
	DB2Freeze
    DB2VacpTagGenerate
	DB2UnFreeze
fi


if [ $isDbFound -eq 0 -a "$ACTION" = "consistency" ]; then
    echo "protected database not online" 
    exit 1
fi

date
exit 0
