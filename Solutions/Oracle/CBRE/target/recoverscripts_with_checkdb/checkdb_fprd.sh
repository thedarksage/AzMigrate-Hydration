#!/bin/sh

ORACLE_SID=$1
ORACLE_HOME=$2
INMAGEHOME=/usr/local/InMage/Fx
LOGDIR=$INMAGEHOME/loginfo
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$INMAGEHOME/lib
LOG_FILE=$LOGDIR/test_db.log
COUNT_FILE=$LOGDIR/test_db_counts.log
export LD_LIBRARY_PATH ORACLE_SID ORACLE_HOME

export LD_LIBRARY_PATH

	$ORACLE_HOME/bin/sqlplus '/as sysdba' << _EOF > $LOG_FILE
	select status from v\$instance;
_EOF
	grep -i "ORA-01034" $LOG_FILE >>/dev/null
	if [ $? -eq 0 ]
	then
		echo "Database is down.."
		exit 1
	fi
	egrep "MOUNTED|STARTED" $LOG_FILE >>/dev/null
	if [ $? -eq 0 ]
	then

		$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF > $LOG_FILE
		set head off
		alter database open read only;
_EOF

		$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF > $COUNT_FILE
		set head off
		SELECT COUNT(*) FROM sysadm.PS_JRNL_LN WHERE journal_date = (SELECT MAX(journal_date) FROM sysadm.PS_JRNL_LN) GROUP BY journal_ID;

_EOF
	
		#########Putting the DB back to standby Mode########################

		echo "Performing Database shutdown/startup...."

		$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF >>/dev/null
		shutdown immediate;
		startup nomount;
		alter database mount standby database;
_EOF
	
		grep "ORA-" $LOG_FILE >/dev/null
		if [ $? -eq 0 ]
		then
			rm -f $LOG_FILE $COUNT_FILE
			exit 1
	
		fi

		cnt=`grep -v "^$" $COUNT_FILE | tail -1`
		if [ $cnt -gt 0 ]
		then
			rm -f $LOG_FILE $COUNT_FILE
			exit 0
		fi
		
	
	fi

	rm -f $LOG_FILE
	rm -f $COUNT_FILE
	exit 0
