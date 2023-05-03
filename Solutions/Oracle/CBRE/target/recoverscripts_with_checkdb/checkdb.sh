#!/bin/sh

ORACLE_SID=$1
ORACLE_HOME=$2
INMAGEHOME=/usr/local/InMage/Fx
LOGDIR=$INMAGEHOME/loginfo
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$INMAGEHOME/lib
export LD_LIBRARY_PATH ORACLE_SID ORACLE_HOME

	$ORACLE_HOME/bin/sqlplus '/as sysdba' << _EOF > $LOGDIR/test_db.log
	select status from v\$instance;
_EOF
	grep -i "ORA-01034" $LOGDIR/test_db.log >>/dev/null
	if [ $? -eq 0 ]
	then
		echo "Database is down.."
		exit 1
	fi
	egrep "MOUNTED|STARTED" $LOGDIR/test_db.log >>/dev/null
	if [ $? -eq 0 ]
	then

		$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF > $LOGDIR/test_db.log
		alter database open read only;
		select count(*) from dual;
		
_EOF
		#########Putting the DB back to standby Mode########################

		echo "Performing Database shutdown/startup...."

		$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF >>/dev/null
		shutdown immediate;
		startup nomount;
		alter database mount standby database;
_EOF
	
		grep "ORA-" $LOGDIR/test_db.log >/dev/null
		if [ $? -eq 0 ]
		then
			rm -f $LOGDIR/test_db.log
			exit 1
	
		fi

		cnt=`grep -v "^$" $LOGDIR/test_db.log|tail -1`
		if [ $cnt -gt 0 ]
		then

			rm -f $LOGDIR/test_db.log
			exit 0
		fi
	
	fi

	rm -f $LOGDIR/test_db.log
