#!/bin/sh 
#=================================================================
# FILENAME
#    db2appwizard.sh
#
# DESCRIPTION
# application wizard for db2.
#    
# HISTORY
#     <7/12/2012>  Anusha Created.
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

PLATFORM=`uname | tr -d "\n"`
TMP_OUTPUT_FILE=/tmp/appwizard
TMP_DB_OUTPUT=/tmp/db_out.log
NULL=/dev/null

Usage()
{
    $ECHO
    $ECHO "usage: $0 <query|update|display>"
    $ECHO "query - to initialize the application settings"
    $ECHO "update - to update the existing settings"
    $ECHO "display - to display the existing settings"
    $ECHO
    return
}

Output()
{
    $ECHO $* >> $TMP_OUTPUT_FILE
    return
}

SetupLogs()
{
    dirName=`dirname $0`
    orgWd=`pwd`

    cd $dirName
    SCRIPT_DIR=`pwd`
    cd $orgWd

    LOG_FILE=$SCRIPT_DIR/db2out.log

    if [ -f $LOG_FILE ]; then
        size=`$DU -k $LOG_FILE | awk '{ print $1 }'`
        if [ $size -gt 5000 ]; then
            $CP -f $LOG_FILE $LOG_FILE.1
            > $LOG_FILE
        fi
    else
        > $LOG_FILE
        chmod a+w $LOG_FILE
    fi
}

log()
{
    $ECHO "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ $OPTION $*" >> $LOG_FILE
    return
}

Cleanup()
{
    $ECHO "Exiting application wizard."
    rm -rf $TMP_OUTPUT_FILE > $NULL
}

CleanupError()
{
    rm -rf $TMP_OUTPUT_FILE > $NULL
    exit 1
}

ReadAns()
{
    $ECHO $* "[Y/N]:"

    while :
    do
        read ANS
        if [ "$ANS" = "y" -o "$ANS" = "Y" -o "$ANS" = "n" -o "$ANS" = "N" ]; then
            break
        fi
        $ECHO "Please enter [Y/N]:"
    done

}

WriteToConfFile()
{
    TAG_NAME=$1
	TAG_VALUE=$2
	#echo "<$TAG_NAME>$TAG_VALUE</$TAG_NAME>" >> $APP_CONF_FILE
	echo "$TAG_NAME=$TAG_VALUE" >> $APP_CONF_FILE
	return
}

ParseArgs()
{
    if [ $# -ne 1 ]; then
        Usage
        exit 1
    fi

    OPTION=$1
    
    if [ $OPTION != "query" -a $OPTION != "update"  -a $OPTION != "display" ]; then
        Usage
        exit 1
    fi

    SetupLogs

    return
}

ReadOpt()
{
    options=$1
    $ECHO "Please select from $options:"

    while :
    do
        read OPT
        echo $options | grep -w $OPT > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            break
        fi
        $ECHO "Please select from $options:"
    done
}

QueryDb2Discovery()
{
  $ECHO
  ReadAns "Would you like to enable DB2 database application discovery on this host?"

  if [ "$ANS" = "n" -o "$ANS" = "N" ]; then
		WriteToConfFile "EnableDB2Discovery" "NO"
		exit 0
  fi
  WriteToConfFile "EnableDB2Discovery" "YES"
  
  DB2Users=`ps -ef | grep db2sysc | grep -v grep |awk '{print $1}'`
  for DB2User in $DB2Users
  do
	tempfile=/tmp/appconf
		
	DB2Group=`id $DB2User | awk '{print $2}' | awk -F"(" '{ print $2}' | awk -F")" '{ print $1}'`
	
	su - $DB2User -c "db2 list active databases" > $TMP_DB_OUTPUT
	if [ $? -ne 0 ];then
		log "No databases are present in the DB2Instance: $DB2User"
		continue
	fi
	
	NUM_DB=`grep "Database name" $TMP_DB_OUTPUT | wc -l `	
  
	# Get the Database Names  
	# Here each DB2 instance may have number of Databases.
	DB_NAMES=`grep "Database name" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'|sed 's/ //g'`
  
	i=1
	while [[ $i -le $NUM_DB ]]
	do
		DB_NAME=`echo $DB_NAMES | cut -d" " -f$i`		
			
		ReadAns "Would you like to enable discovery on this database : $DB2User:$DB_NAME ?"
				
		if [ "$ANS" = "y" -o "$ANS" = "Y" ];then
			WriteToConfFile "DB2Database" "YES:$DB2Group:$DB2User:$DB_NAME"			
		elif [ "$ANS" = "n" -o "$ANS" = "N" ];then				
			WriteToConfFile "DB2Database" "NO:$DB2Group:$DB2User:$DB_NAME"			
		fi	

	    ((i=i+1))		
    done   
  done
  
}

RegisterNewDb()
{
	DB2INSTANCES=`ps -ef | grep db2sysc | grep -v grep |awk '{print $1}'`
	
	for DB2_INS in $DB2INSTANCES
	do
		DB2Group=`id $DB2_INS | awk '{print $2}' | awk -F"(" '{ print $2}' | awk -F")" '{ print $1}'`
		su - $DB2_INS -c "db2 list active databases " > $TMP_DB_OUTPUT
		
		if [ $? -ne 0 ];then
			log "No databases are active in the DB2Instance: $DB2_INS"
			continue
		fi
		
		NUM_DB=`grep "Database name" $TMP_DB_OUTPUT | wc -l `
		
		# Get the Database Names  
		# Here each DB2 instance may have number of Databases.
		DB_NAMES=`grep "Database name" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'|sed 's/ //g'`
		
		i=1
		while [[ $i -le $NUM_DB ]]
		do
			DB_NAME=`echo $DB_NAMES | cut -d" " -f$i`		
			grep "$DB2_INS:$DB_NAME" $APP_CONF_FILE >> /dev/null
			if [ $? -ne 0 ];then
				$ECHO
				ReadAns "Would you like to enable the discovery for newly registered database '$DB2_INS:$DB_NAME'?"
				
				if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
					WriteToConfFile "DB2Database" "YES:$DB2Group:$DB2_INS:$DB_NAME"	
				elif [ "$ANS" = "n" -o "$ANS" = "N" ];then		
					WriteToConfFile "DB2Database" "NO:$DB2Group:$DB2_INS:$DB_NAME"	
				fi
			fi
			((i=i+1))
		done			
		rm -f $TMP_DB_OUTPUT
	done
	
	return
}

EnableRegisteredDb()
{
    dbName=$1
    tempfile1=/tmp/appwizconf.$$
		
	while read line
	do
		case $line in
		*DB2Database*)
			read line1 
			name=$line1
			flag=`echo $line|awk -F">" '{print $2}' | awk -F"</" '{print $1}'`
			db_name=`echo $name|awk -F">" '{print $2}' | awk -F"</" '{print $1}'`
			if [[ "$flag" = "NO" && $db_name = $dbName ]];then
				echo "<DB2Database>YES</DB2Database>" >>$tempfile1
			else
				echo $line >>$tempfile1
			fi
			echo $line1 >>$tempfile1
			;;
		*)
			echo $line >>$tempfile1
			;;
		esac

	done <$APP_CONF_FILE

    mv -f $tempfile1 $APP_CONF_FILE	

    return 
}

DisableRegisteredDb()
{
	dbName=$1
    tempfile1=/tmp/appwizconf.$$
		
	while read line
	do
		case $line in
		*DB2Database*)
			read line1 
			name=$line1
			flag=`echo $line|awk -F">" '{print $2}' | awk -F"</" '{print $1}'`
			db_name=`echo $name|awk -F">" '{print $2}' | awk -F"</" '{print $1}'`
			if [[ "$flag" = "YES" && $db_name = $dbName ]];then
				echo "<DB2Database>NO</DB2Database>" >>$tempfile1
			else
				echo $line >>$tempfile1
			fi
			echo $line1 >>$tempfile1
			;;
		*)
			echo $line >>$tempfile1
			;;
		esac

	done <$APP_CONF_FILE

    mv -f $tempfile1 $APP_CONF_FILE
	
    return 
}

UnregisterDb()
{
	dbName=$1
    tempfile1=/tmp/appwizconf.$$
	tmpfile=/tmp/out
	
	while read line
	do
		case $line in
		*DB2Database*)
				read line1 
				name=$line1
				db_name=`echo $name | awk -F">" '{print $2}' | awk -F"</" '{print $1}'`
				if [ $db_name = $dbName ];then
					log "Unregistering $db_name"
					read line2
					read line3
				else
					echo $line >>$tempfile1
					echo $line1 >>$tempfile1
				fi
			;;
			*)
				echo $line >>$tempfile1
			;;
		esac

	done <$APP_CONF_FILE
	rm -f $tmpfile
    mv -f $tempfile1 $APP_CONF_FILE	

    return 
}

UpdateConfig()
{
	if [ ! -f "$APP_CONF_FILE" ]; then
        $ECHO "$APP_CONF_FILE not found. Please initialize with 'query' option."
        return
    fi

    while :
    do
        clear
        $ECHO
        $ECHO
        $ECHO "1. Register new databases for discovery"
        $ECHO "2. Enable registered databases for discovery"
        $ECHO "3. Disable registered databases for discovery"
        $ECHO "4. Unregister databases"
        $ECHO "5. Exit"
        $ECHO

        ReadOpt "1 2 3 4 5 "
		
		case $OPT in
        1)
            RegisterNewDb
            $ECHO "Please press enter to continue"
            read cr
            ;;
		2)
			
			tempfile=/tmp/appconf	
			disabledDbNames=`grep DB2Database $APP_CONF_FILE | awk -F"=" '{print $2}'|grep NO`
			
			for entry in $disabledDbNames
			do
				dbname=`echo $entry | awk -F":" '{print $(NF-1),":",$NF}' | sed 's/ //g'`
			    ReadAns "Would you like to enable discovery on this database : $dbname ?"
				
				if [ "$ANS" = "y" -o "$ANS" = "Y" ];then
					data=`echo $entry | sed 's/NO/YES/g'`
					sed "s/$entry/$data/g" $APP_CONF_FILE > $tempfile					
					mv $tempfile $APP_CONF_FILE
				fi
			done
			rm -f $tempfile
            $ECHO "Please press enter to continue"
            read cr
            ;;            
		3)
			tempfile=/tmp/appconf	
			enabledDbNames=`grep DB2Database $APP_CONF_FILE | awk -F"=" '{print $2}'|grep YES`
			
			for entry in $enabledDbNames
			do
				dbname=`echo $entry | awk -F":" '{print $(NF-1),":",$NF}' | sed 's/ //g'`
			    ReadAns "Would you like to disable discovery on this database : $dbname ?"
				
				if [ "$ANS" = "y" -o "$ANS" = "Y" ];then
					data=`echo $entry | sed 's/YES/NO/g'`
					sed "s/$entry/$data/g" $APP_CONF_FILE > $tempfile					
					mv -f $tempfile $APP_CONF_FILE
				fi
			done
			rm -f $tempfile
			
            $ECHO "Please press enter to continue"
            read cr
            ;;
			
		4)
			tempfile=/tmp/appconf	
			registeredDbNames=`grep DB2Database $APP_CONF_FILE | awk -F"=" '{print $2}'`
			
			for entry in $registeredDbNames
			do
				dbname=`echo $entry | awk -F":" '{print $(NF-1),":",$NF}' | sed 's/ //g'`
				ReadAns "Would you like to unregister the database : $dbname ?"
				
				if [ "$ANS" = "y" -o "$ANS" = "Y" ];then
					grep -v $dbname $APP_CONF_FILE > $tempfile
					mv -f $tempfile $APP_CONF_FILE
				fi
			done
			rm -f $tempfile 
			
            $ECHO "Please press enter to continue"
            read cr
            ;;
		5)
            return
            ;;
        esac
    done
}

DisplayConfig()
{
	
	if [ ! -f "$APP_CONF_FILE" ]; then
        $ECHO
        $ECHO "Config file $APP_CONF_FILE not found"
        $ECHO
        return
    fi
	
	enabledDbNames=`grep DB2Database $APP_CONF_FILE | awk -F"=" '{print $2}'|grep YES |awk -F":" '{print $NF}'`
	disabledDbNames=`grep DB2Database $APP_CONF_FILE | awk -F"=" '{print $2}'|grep NO |awk -F":" '{print $NF}'`
	
	if [ ! -z "$enabledDbNames" ]; then
        $ECHO
        $ECHO "The following databases are currently registered and enabled for discovery"
        $ECHO
        $ECHO "$enabledDbNames"
        $ECHO
    fi

    if [ ! -z "$disabledDbNames" ]; then
        $ECHO
        $ECHO "The following databases are currently registered and disabled for discovery"
        $ECHO
        $ECHO "$disabledDbNames"
        $ECHO
    fi     
}
########### Main ##########

trap Cleanup 0

if [ "$PLATFORM" = "Linux" -o "$PLATFORM" = "AIX" ]; then
    trap CleanupError 1 2 3 15
else
    $ECHO "Error: Unsupported Operating System $PLATFORM."
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

APP_CONF_FILE=$INSTALLPATH/etc/db2appwizard.conf

while [ -f $TMP_OUTPUT_FILE ]
do
    log "Error: appwizard is already running."
    sleep 2
done

> $TMP_OUTPUT_FILE

$ECHO
$ECHO
$ECHO "Welcome to application wizard!"
$ECHO

if [ $OPTION = "query" ]; then

    if [ -f $APP_CONF_FILE ]; then
        $ECHO "WARNING: Reinitializing the configuration file will erase the existing settings."
        ReadAns "Do you want to continue?"
        if [ $ANS = "n" -o $ANS = "N" ]; then
            exit 0
        fi
    fi

    > $APP_CONF_FILE
	
	QueryDb2Discovery
	
elif [ $OPTION = "update" ]; then

    UpdateConfig

elif [ $OPTION = "display" ]; then

    DisplayConfig

fi

rm -f $TMP_OUTPUT_FILE &> $NULL

exit 0