#!/usr/bin/sh
echo ##########################################################################
echo                        RESTORE  RECOVERY SCRIPTS
echo ##########################################################################
echo ##########################################################################
echo      These variable releated to Oracle needs to modified accoring to
echo      installation of Oracle binaries and Database.
echo ##########################################################################



ORACLE_BASE=/hcprd/oracle/hcprd
ORACLE_HOME=/hcprd/oracle/hcprd/ora10g
ORACLE_SID=hcprd
ORA_ADMIN=/hcprd/oracle/hcprd/admin/hcprd
LD_LIBRARY_PATH=$ORACLE_HOME/lib
PATH=$PATH:$ORACLE_HOME/bin
TNS_ADMIN=$ORACLE_HOME/network/admin
export ORACLE_BASE ORACLE_HOME PATH LD_LIBRARY_PATH TNS_ADMIN ORACLE_SID


echo ##########################################################################
echo                        Creation of Required directory structure
echo ##########################################################################

mkdir -p $ORA_ADMIN
mkdir -p $ORA_ADMIN/bdump
mkdir -p $ORA_ADMIN/udump
mkdir -p $ORA_ADMIN/cdump
mkdir -p $ORA_ADMIN/adump
mkdir -p $ORA_ADMIN/pfile
mkdir -p $ORA_ADMIN/dpdump

$ORACLE_HOME/bin/rman NOCATALOG << EOF
connect target
run {
STARTUP NOMOUNT ;
RESTORE controlfile from '/hcprd/oraback/hcprd/oradump/hcprd/hcprd.ctl';
sql 'alter database mount standby database';
restore database;
}
EOF

exit 0

