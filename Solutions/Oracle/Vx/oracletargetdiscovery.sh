#!/bin/ksh

OS_Related_Params()
{
        ######Calling Corresponding Function based on the arguements#####################
	isAIX=0
        OS=`uname -s`
        case $OS in
             HP-UX ) df_command="bdf"
                     FSTABFILE="/etc/fstab";;
             Linux ) df_command="df"
                     i=1
                     for p in `df -k|grep -v "Mounted on"`
                     do
                         echo -n "$p " >> $DF_OUTPUT
                         i=`expr $i + 1`
                         x=`expr $i % 7` >>/dev/null
                         if [ $x  -eq 0 ]
                         then
                              echo "" >>  $DF_OUTPUT
                              i=1
                         fi
                     done

                     FSTABFILE="/etc/fstab"
                     ORATABFILE="/etc/oratab"
                     AWK="/usr/bin/awk"
                        ;;
             SunOS ) df_command="df"
                     FSTABFILE="/etc/vfstab"
                     ORATABFILE="/var/opt/oracle/oratab"
                     AWK="/usr/xpg4/bin/awk"
                        ;;
             AIX )  df_command="df"
                     AWK="/usr/bin/awk"
                     FSTABFILE="/etc/vfstab"
		     isAIX=1;;
        esac
}

        ##########################Start of the program######################################
        INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
        CONFIG_DIR="$INMAGE_DIR/etc"
        CONFIG_FILE="$CONFIG_DIR/drscout.conf"
        SCRIPTS_DIR="$INMAGE_DIR/scripts"
        LOGDIR="$INMAGE_DIR/failover_data"
	ORACLELOGDIR="$LOGDIR/oracle"
        DISCOVERYXML="$LOGDIR/sample.xml"
	DF_OUTPUT="$LOGDIR/.df_koutput.txt"

        TEMP_FILE="$LOGDIR/.files"
        TEMP_FILE1="$LOGDIR/.files1"
        TEMP_FILE2="$LOGDIR/.files2"
        TEMP_FILE3="$LOGDIR/.files3"
	rm -f $TEMP_FILE $TEMP_FILE1 $TEMP_FILE2 $TEMP_FILE3

#calling OS related parameter funtion##
	OS_Related_Params

	if [ $# -ne 1 ]
	then
		echo "syntax is:"
		echo "$0 <ORACLE_SID>"
		echo "Exiting..."
		exit 1
	fi
	ORACLE_SID=$1	
	ORACLE_CONFIG_FILENAME="$ORACLELOGDIR/oracle_${ORACLE_SID}.txt"
	ORACLE_CONFIG_UNPLANNED_FAILOVER_FILENAME="$ORACLELOGDIR/oracle_${ORACLE_SID}_unplanned.txt"
	if [ ! -f "$ORACLELOGDIR/oracle_${ORACLE_SID}.txt" ] 
	then
		echo "$ORACLELOGDIR/oracle_${ORACLE_SID}.txt doesn't exist"
		echo "Exiting..."
		exit 1

	fi
	
	echo "Checking the retention log path.."
	SourceHostId=`grep "SourceHostId =" $ORACLE_CONFIG_FILENAME|$AWK -F "=" '{ print $2 }'`
        ThisHostId=`grep -i hostid $CONFIG_FILE|$AWK -F"=" '{ print $2 }'|sed 's/ //g'`
        CXServerIP=`grep -i "^Hostname" $CONFIG_FILE|$AWK -F"=" '{ print $2 }'|sed 's/ //g'`
	CXPortNumber=`grep -i "^Port"  $CONFIG_FILE|$AWK -F"=" '{ print $2 }'|sed 's/ //g'`
        SourcePartitionsList=`$AWK -F "=" '$1=="PARTITIONS " { print $2 }' $ORACLE_CONFIG_FILENAME`
	if [ $isAIX -eq 1 ]
	then
	    SourcePartitionsList=`$AWK -F "=" '$1=="PVList " { print $2 }' $ORACLE_CONFIG_FILENAME`
	fi
	####Looping for finding out corresponding target partitions and retention log path ###
	for SourcePartition in `echo $SourcePartitionsList`
	do
                   echo "<discovery>" >$DISCOVERYXML
                   echo "    <operation>3</operation>" >>$DISCOVERYXML
                   echo "    <src_hostid>$SourceHostId</src_hostid>" >>$DISCOVERYXML
                   echo "    <trg_hostid>$ThisHostId</trg_hostid>" >>$DISCOVERYXML
                   echo "    <src_pri_partition>$SourcePartition</src_pri_partition>" >>$DISCOVERYXML
                   echo "</discovery>" >>$DISCOVERYXML
                   if [ "$SourceHostId" = "$ThisHostId" ]
                   then
                        echo "Both source and target machines are same...can't continue.."
                        echo "Please check $ORACLE_CONFIG_FILENAME"
                        exit 255

                   fi

                   $INMAGE_DIR/bin/cxcli "http://${CXServerIP}:${CXPortNumber}/cli/discovery.php" $DISCOVERYXML >$TEMP_FILE1
		   grep -s -i "replication_pair Failed =" $TEMP_FILE1 
		   if [ $? -eq 0 ]
		   then
			echo "Cxcli unable to get the replicatin pair details"
			echo "Failed.."
			exit 1

		   fi
		   TargetPartition=`grep "target_devicename" $TEMP_FILE1|$AWK -F">" '{ print $2 }'|$AWK -F"<" '{print $1}'`
		   TargetPartitionList="$TargetPartitionList $TargetPartition"
		   RetentionBaseDirlog=`grep "retention_log_path" $TEMP_FILE1|$AWK -F">" '{ print $2 }'|$AWK -F"<" '{print $1}'`
		   if [ ! -z "$RetentionBaseDirlog" ]
		   then
		   	Retentionlog="$RetentionBaseDirlog/$ThisHostId$TargetPartition/cdpv1.db"
		   	RetentionlogList="$RetentionlogList $Retentionlog"
	           else
			Retentionlog="NORETENTION"
		   	RetentionlogList="$RetentionlogList $Retentionlog"
	 	   fi

	done
		   grep -v "RetentionLog=" $ORACLE_CONFIG_FILENAME|grep -v "TargetPartition=" >$TEMP_FILE
		   mv $TEMP_FILE $ORACLE_CONFIG_FILENAME
		   echo "RetentionLog=$RetentionlogList" |tee $ORACLE_CONFIG_UNPLANNED_FAILOVER_FILENAME
		   echo "TargetPartition=$TargetPartitionList"|tee -a $ORACLE_CONFIG_UNPLANNED_FAILOVER_FILENAME
