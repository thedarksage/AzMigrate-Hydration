#!/usr/bin/sh
echo ##########################################################################
echo                        RESTORE  RECOVERY SCRIPTS
echo ##########################################################################

ORACLE_SID=paprd01
CONNECT_USER="/as sysdba"

echo ##########################################################################
echo      These variable releated to Oracle needs to modified accoring to
echo      installation of Oracle binaries and Database.
echo ##########################################################################



ORACLE_HOME=/paprd/oracle/paprd01/ora9i
TEP=/tmp
TEMPDIR=/tmp
ORACLE_SID=paprd01
LD_LIBRARY_PATH=$ORACLE_HOME/lib
PATH=$PATH:$ORACLE_HOME/bin
TNS_ADMIN=$ORACLE_HOME/network/admin

#PATH=$ORACLE_HOME/bin:$ORACLE_BASE/local/bin:$PATH

export  ORACLE_HOME PATH LD_LIBRARY_PATH TNS_ADMIN ORACLE_SID

ORA_ADMIN=/paprd/oracle/paprd01/admin/paprd01

echo ##########################################################################
echo                        Creation of Required directory structure
echo ##########################################################################

mkdir -p $ORA_ADMIN
mkdir -p $ORA_ADMIN/bdump
mkdir -p $ORA_ADMIN/udump
mkdir -p $ORA_ADMIN/cdump
mkdir -p $ORA_ADMIN/adump
mkdir -p $ORA_ADMIN/pfile
mkdir -p $ORA_ADMIN/create

$ORACLE_HOME/bin/rman NOCATALOG << EOF
connect target
run {
STARTUP NOMOUNT PFILE='/paprd/oraback/paprd01/oradump/paprd01.ora';
RESTORE controlfile from '/paprd/oraback/paprd01/oradump/paprd/paprd.ctl';
sql 'alter database mount standby database';
restore database;
}
EOF

exit 0

