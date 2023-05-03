#!/bin/ksh 
        INMAGEHOME=`grep INSTALLATION_DIR /usr/local/.fx_version|cut -d"=" -f2`
        LOGDIR=$INMAGEHOME/loginfo
        SCRIPTS=$INMAGEHOME/scripts
	
	ORACLE_SID=$1
	ORACLE_HOME=$2
	export ORACLE_SID ORACLE_HOME
$ORACLE_HOME/bin/sqlplus -s '/as sysdba' <<END >$LOGDIR/dbsize_$ORACLE_SID.txt
	set head off;
	SELECT sum(bytes)/(1024) FROM  dba_data_files;
END


$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<EOF1 >$LOGDIR/pfileinfo_$ORACLE_SID.txt
  SELECT  DECODE(value, NULL, 'PFILE', 'SPFILE') "Init File Type"  
  FROM 	  sys.v_\$parameter 
  WHERE   name = 'spfile';
EOF1


grep -v "SPFILE" $LOGDIR/pfileinfo_$ORACLE_SID.txt|grep -v "grep"|grep PFILE >>/dev/null
x=$?
if [ $x -eq 0 ]
then
$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<END1 >>/dev/null
create spfile='/tmp/spfile.ora' from pfile;
create pfile='/tmp/pfile.ora' from spfile='/tmp/spfile.ora';
END1
fi

if [ $x -eq 1 ]
then
$ORACLE_HOME/bin/sqlplus  -S '/as sysdba' <<END >>/dev/null
create pfile='/tmp/pfile.ora' from spfile;
END
fi
###Running Query for verifying whehter Database is RAC or standlone#####
$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<END2 >$LOGDIR/Clusterinfo_${ORACLE_SID}.txt
select parallel from v\$instance;
END2
exit 0
