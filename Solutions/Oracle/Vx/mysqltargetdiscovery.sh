#!/bin/ksh
        ##########################Start of the program######################################
        INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
        CONFIG_DIR="$INMAGE_DIR/etc"
        CONFIG_FILE="$CONFIG_DIR/drscout.conf"
        SCRIPTS_DIR="$INMAGE_DIR/scripts"
        LOGDIR="$INMAGE_DIR/failover_data"
	MYSQLLOGDIR="$LOGDIR/mysql"
        DISCOVERYXML="$LOGDIR/sample.xml"

        TEMP_FILE="$LOGDIR/.files"
        TEMP_FILE1="$LOGDIR/.files1"
        TEMP_FILE2="$LOGDIR/.files2"
        TEMP_FILE3="$LOGDIR/.files3"
	rm -f $TEMP_FILE $TEMP_FILE1 $TEMP_FILE2 $TEMP_FILE3

	MYSQL_CONFIG_FILENAME="$MYSQLLOGDIR/mysql_default.txt"
	if [ ! -f "$MYSQL_CONFIG_FILENAME" ] 
	then
		echo "$MYSQL_CONFIG_FILENAME doesn't exist"
		echo "Exiting..."
		exit 1

	fi
	
	echo "Checking the retention log path.."
	SourceHostId=`grep "SourceHostId =" $MYSQL_CONFIG_FILENAME|awk -F"=" '{ print $2 }'`
        ThisHostId=`grep -i hostid $CONFIG_FILE|awk -F"=" '{ print $2 }'|sed 's/ //g'`
        CXServerIP=`grep -i "^Hostname" $CONFIG_FILE|awk -F"=" '{ print $2 }'|sed 's/ //g'`
        SourcePartitionsList=`awk -F "=" '$1=="PARTITIONS " { print $2 }' $MYSQL_CONFIG_FILENAME`
	####Looping for finding out corresponding target partitions and retention log path ###
	for SourcePartition in `echo $SourcePartitionsList`
	do
                   echo "<discovery>" >$DISCOVERYXML
                   echo "    <operation>3</operation>" >>$DISCOVERYXML
                   echo "    <src_hostid>$SourceHostId</src_hostid>" >>$DISCOVERYXML
                   echo "    <trg_hostid>$ThisHostId</trg_hostid>" >>$DISCOVERYXML
                   echo "    <src_pri_partition>$SourcePartition</src_pri_partition>" >>$DISCOVERYXML
                   echo "</discovery>" >>$DISCOVERYXML
                   $INMAGE_DIR/bin/cxcli "http://$CXServerIP/cli/discovery.php" $DISCOVERYXML >$TEMP_FILE1
		   grep -s -i "fail" $TEMP_FILE1 
		   if [ $? -eq 0 ]
		   then
			echo "Cxcli unable to get the replicatin pair details"
			echo "Failed.."
			exit 1

		   fi
		   TargetPartition=`grep "target_devicename" $TEMP_FILE1|awk -F">" '{ print $2 }'|awk -F"<" '{print $1}'`
		   TargetPartitionList="$TargetPartitionList $TargetPartition"
		   RetentionBaseDirlog=`grep "retention_log_path" $TEMP_FILE1|awk -F">" '{ print $2 }'|awk -F"<" '{print $1}'`
		   if [ ! -z "$RetentionBaseDirlog" ]
		   then
		   	Retentionlog="$RetentionBaseDirlog/$ThisHostId$TargetPartition/cdpv1.db"
		   	RetentionlogList="$RetentionlogList $Retentionlog"
	           else
			Retentionlog="NORETENTION"
		   	RetentionlogList="$RetentionlogList $Retentionlog"
	 	   fi

	done
		   grep -v "RetentionLog=" $MYSQL_CONFIG_FILENAME|grep -v "TargetPartition=" >$TEMP_FILE
		   mv $TEMP_FILE $MYSQL_CONFIG_FILENAME
		   echo "RetentionLog=$RetentionlogList" |tee -a $MYSQL_CONFIG_FILENAME
		   echo "TargetPartition=$TargetPartitionList"|tee -a $MYSQL_CONFIG_FILENAME
