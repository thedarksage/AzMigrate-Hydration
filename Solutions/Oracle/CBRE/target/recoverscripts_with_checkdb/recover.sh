#!/bin/sh
	. ~/.profile 1>>/dev/null 2>>/dev/null 2>>/dev/null 2>>/dev/null 2>>/dev/null
	id|grep -v grep|grep "uid=0" >>/dev/null
	if [ $? -eq 0 ]
	then
		echo "You can't run this script with root user id.\n exiting...."
		exit
	fi
	ORACLE_SID=$1
	ARCHDIR=$2
	ORACLE_HOME=$3
	DELAYTIME=$4
	TNS_ADMIN=$ORACLE_HOME/network/admin
	INMAGEHOME=`grep INSTALLATION_DIR /usr/local/.fx_version|cut -d"=" -f2`
	LOGDIR="$INMAGEHOME/loginfo"
	ERRORFILE=$LOGDIR/errorfile
	TEMPFILE="/tmp/xfile"
	LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$ORACLE_HOME/lib:$INMAGEHOME/lib:$INMAGEHOME/lib_alert
	PATH=$PATH:$ORACLE_HOME/bin:/usr/local/bin:/usr/ccs/bin:/usr/sbin
	export  ORACLE_HOME PATH LD_LIBRARY_PATH TNS_ADMIN ORACLE_SID


	if [ ! -d $LOGDIR ]
	then
		mkdir -p $LOGDIR
	fi

  	ps -ef|grep -v "grep"|grep pmon_$ORACLE_SID >>/dev/null
	if [ $? -ne  0 ]
	then
	echo "#########################"|tee -a $ERRORFILE
	echo "ERROR: $ORACLE_SID is down: " |tee $ERRORFILE
	echo "#########################"|tee -a $ERRORFILE

	echo "ERROR: Database instance $ORACLE_SID is down on target..Not able to proceed recovery.please bring up the database on target:" |tee -a $ERRORFILE
	echo "	Login as sysdba "|tee -a $ERRORFILE
	echo "	startup nomount;"|tee -a  $ERRORFILE
	echo "	alter database mount standby database;"|tee -a $ERRORFILE
	echo "	exit;"|tee -a $ERRORFILE
	echo "#########################"|tee -a $ERRORFILE
	$INMAGEHOME/alert -f $ERRORFILE >>/dev/null
	rm -f $ERRORFILE
	exit 1
	fi
	##########Checking whether database is in Mounted state or not##############
	sqlplus -S '/as sysdba'<<EOF >$TEMPFILE
	set head off
	select status from v\$instance;
	quit;
EOF
	grep -i "MOUNTED" $TEMPFILE >/dev/null
	
	if [ $? -ne 0 ]
	then
	rm -f $ERRORFILE >>/dev/null

	echo "ERROR:  Database instance $ORACLE_SID is not in Standby mode: " |tee $ERRORFILE
	echo "####################################"|tee -a $ERRORFILE
	echo "ERROR: $ORACLE_SID is not in Standby mode on target machine:" |tee -a $ERRORFILE
	echo "####################################"|tee -a $ERRORFILE
	echo "Please use the following steps to put the database in standby mode" |tee -a $ERRORFILE
	echo "	Login as sysdba "|tee -a $ERRORFILE
	echo "	shutdown immediate;" |tee -a $ERRORFILE
	echo "	startup nomount;"|tee -a  $ERRORFILE
	echo "	alter database mount standby database;"|tee -a $ERRORFILE
	echo "	exit;"|tee -a $ERRORFILE
	echo "####################################"|tee -a $ERRORFILE
	
	$INMAGEHOME/alert -f $ERRORFILE >>/dev/null
	rm -f $ERRORFILE

	exit 1
	fi

	

	##########Finding Archive log file format#######################
	sqlplus '/as sysdba'<<EOF >$TEMPFILE
	show parameters log_archive_format;
	quit;
EOF

	######### Getting sequence number Field ##############################
	fileformat=`grep "archive" $TEMPFILE|awk '{ print $3 }'`
	FieldNo=`echo $fileformat|cut -f 1 -d "."| awk -F"_"  '{ x=NF; for(i=1;i<=NF;i++) if ( $i == "%s") print i; }'`


	########Querying the database for Latest Archive Log info########################
	sqlplus -S '/as  sysdba'<<EOF >$TEMPFILE
		select max(SEQUENCE#) from v\$log_history;
	quit;
EOF

	oldarchiveno=`tail -2 $TEMPFILE|head -1`
	################################Finding the Maximum Archive log number#################################
	MaxArchiveNo=0
	cd $ARCHDIR
	for i in `ls`
	do
		temp=`echo $i|cut -f 1 -d "."|cut -f $FieldNo -d "_"`
		if [ $temp -gt $MaxArchiveNo ]
		then
			MaxArchiveNo=$temp
		fi
	done
	###########################################################################################################
	#                              Finding Holes In Archive Log List                                            #
	#############################################################################################################
	cd $ARCHDIR
	Archivetemp=`expr $oldarchiveno + 1`
	ls | cut -f 1 -d "."|cut -f $FieldNo -d "_" >$TEMPFILE
	while [ $Archivetemp -le $MaxArchiveNo ]
	do
        	grep $Archivetemp $TEMPFILE >>/dev/null
        	if [ $? -ne 0 ]
        	then
                	echo "Archive Log $Archivetemp Not found " >>$LOGDIR/archiveholes.log
        	fi
        	Archivetemp=`expr $Archivetemp + 1`
	done
	#################################################################################################################
	#                           Sending Email alert                                                                 #
	#################################################################################################################
  	if [ -s $LOGDIR/archiveholes.log ]
  	then
		if [ ! -f $LOGDIR/Alert_last_$ORACLE_SID.txt ]
		then
	          	echo "$ORACLE_SID archive log errors" >$TEMPFILE
        	  	cat $TEMPFILE $LOGDIR/archiveholes.log >$LOGDIR/archiveholes1.log
	  		$INMAGEHOME/alert -f $LOGDIR/archiveholes1.log
	  	  	rm -f $LOGDIR/archiveholes.log $LOGDIR/archiveholes1.log
			echo 1 > $LOGDIR/Alert_last_$ORACLE_SID.txt
		else
			counter=`cat $LOGDIR/Alert_last_$ORACLE_SID.txt`
			
			if [ $counter -ne 4 ]
			then
				
				counter=`expr $counter + 1`
				echo $counter > $LOGDIR/Alert_last_$ORACLE_SID.txt
			else
		          	echo "$ORACLE_SID archive log errors" >$TEMPFILE
	        	  	cat $TEMPFILE $LOGDIR/archiveholes.log >$LOGDIR/archiveholes1.log
		  		$INMAGEHOME/alert -f $LOGDIR/archiveholes1.log
	  		  	rm -f $LOGDIR/archiveholes.log $LOGDIR/archiveholes1.log
				echo 1 > $LOGDIR/Alert_last_$ORACLE_SID.txt
		
			
			fi
			

		fi
  	fi
	
	#################################################################################################################
	#                           Recovery of  DB                                                                     #
	#################################################################################################################
	
	echo " set echo off feedback off heading off newpage 0 space 0 pagesize 0 trimspool on " > $LOGDIR/recover_tmp.sql
	if [ $DELAYTIME -gt 0 ]; then
	echo "select 'recover standby database until time ' || '''' || to_char(sysdate - "$DELAYTIME"/24 , 'yyyy-mm-dd:hh24:mi:ss') || ''';' from dual;" >> $LOGDIR/recover_tmp.sql
	else
	echo "select 'recover standby database until time ' || '''' || to_char(sysdate , 'yyyy-mm-dd:hh24:mi:ss') || ''';' from dual;" >> $LOGDIR/recover_tmp.sql
	fi
	
	$ORACLE_HOME/bin/sqlplus -S "/as sysdba" < $LOGDIR/recover_tmp.sql > $LOGDIR/recover_tmp1.sql
	
	echo "set linesiz 325" > $LOGDIR/recover_$ORACLE_SID.sql
	cat $LOGDIR/recover_tmp1.sql >> $LOGDIR/recover_$ORACLE_SID.sql
	echo "" >> $LOGDIR/recover_$ORACLE_SID.sql
	echo "auto" >> $LOGDIR/recover_$ORACLE_SID.sql
	echo "exit" >> $LOGDIR/recover_$ORACLE_SID.sql
	
	if [ ! -f $LOGDIR/neededfile_$ORACLE_SID.txt ] ####For running First time#######
	then
        	$ORACLE_HOME/bin/sqlplus -S "/as sysdba" < $LOGDIR/recover_$ORACLE_SID.sql >$LOGDIR/recoveroutput_$ORACLE_SID.txt 
	else
		if [ ! -s $LOGDIR/neededfile_$ORACLE_SID.txt ]
		then
        	
		$ORACLE_HOME/bin/sqlplus -S "/as sysdba" < $LOGDIR/recover_$ORACLE_SID.sql >$LOGDIR/recoveroutput_$ORACLE_SID.txt 
		exit 0
		fi

        	filename=`cat $LOGDIR/neededfile_$ORACLE_SID.txt`
        	cd $ARCHDIR
        	ls|grep $filename >/dev/null
        	if [ $? -eq 0 ]
        	then
        	$ORACLE_HOME/bin/sqlplus -S "/as sysdba" < $LOGDIR/recover_$ORACLE_SID.sql >$LOGDIR/recoveroutput_$ORACLE_SID.txt 
        	fi
	fi
	exit 0
