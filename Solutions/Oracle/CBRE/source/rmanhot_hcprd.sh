#!/usr/bin/sh
echo ##########################################################################
echo                       HOT BACKUP USING ORACLE RMAN
echo ##########################################################################


ORACLE_SID=hcprd

CONNECT_USER=" /as sysdba"

echo ##########################################################################
echo      These variable releated to Oracle needs to modified accoring to
echo      installation of Oracle binaries and Database.
echo ##########################################################################



BACKUP_DIR=/hcprd/oraback/hcprd/oradump   

ORACLE_BASE=/hcprd/oracle/hcprd/
ORACLE_HOME=/hcprd/oracle/hcprd/ora10g
TEP=/tmp
TEMPDIR=/tmp
ORACLE_SID=hcprd
LD_LIBRARY_PATH=$ORACLE_HOME/lib
PATH=$PATH:$ORACLE_HOME/bin
TNS_ADMIN=$ORACLE_HOME/network/admin
PATH=$PATH:/usr/sbin



export ORACLE_BASE ORACLE_HOME PATH LD_LIBRARY_PATH TNS_ADMIN ORACLE_SID

echo ##########################################################################
echo                        Processing Oracle Dump
echo ##########################################################################


echo CREATE PFILE="'"$BACKUP_DIR"/"$ORACLE_SID"."ora"'" FROM SPFILE";" > /tmp/"$ORACLE_SID".sql
echo exit >> /tmp/"$ORACLE_SID".sql

$ORACLE_HOME/bin/sqlplus -s "/ as sysdba"  @/tmp/"$ORACLE_SID".sql



$ORACLE_HOME/bin/rman NOCATALOG << EOF
connect target
RUN {
ALLOCATE CHANNEL C1 TYPE DISK format '/hcqa/oraindex/hcqa/oradump/hcprd/ora_df%chr%t_s%chr%s_s%chr%p.BKP' maxopenfiles 3 rate 6M maxpiecesize 3G;
ALLOCATE CHANNEL C2 TYPE DISK format '/hcqa/oraarch/hcqa/oradump/hcprd/ora_df%chr%t_s%chr%s_s%chr%p.BKP' maxopenfiles 4 rate 8M maxpiecesize 4G;
ALLOCATE CHANNEL C3 TYPE DISK format '/hcqa/oradata/hcqa/oradump/hcprd/ora_df%chr%t_s%chr%s_s%chr%p.BKP' maxopenfiles 3 rate 6M maxpiecesize 3G;
ALLOCATE CHANNEL C4 TYPE DISK format '/hcprd/oraarch/hcprd/oradump/hcprd/ora_df%chr%t_s%chr%s_s%chr%p.BKP' maxopenfiles 2 rate 6M maxpiecesize 4G;
ALLOCATE CHANNEL C5 TYPE DISK format '/hcprd/oradata/hcprd/oradump/hcprd/ora_df%chr%t_s%chr%s_s%chr%p.BKP' maxopenfiles 2 rate 6M maxpiecesize 3G;
BACKUP INCREMENTAL LEVEL 0
(database FILESPERSET 3 ) ;
COPY CURRENT CONTROLFILE FOR STANDBY TO '/hcprd/oraback/hcprd/oradump/hcprd/hcprd.ctl';
RELEASE CHANNEL C1;
RELEASE CHANNEL C2;
RELEASE CHANNEL C3;
RELEASE CHANNEL C4;
RELEASE CHANNEL C5;
}
EOF

exit 0


