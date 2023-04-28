#!/bin/sh -x
echo ##########################################################################
echo                        RESTORE  RECOVERY SCRIPTS
echo ##########################################################################
echo ##########################################################################
echo      These variable releated to Oracle needs to modified accoring to 
echo      installation of Oracle binaries and Database.
echo ##########################################################################

USER=sys
PASSWORD=
ORACLE_SID=orainmag
ORACLE_HOME=/home/oracle/10.2.0
ORA_ADMIN=$ORACLE_HOME/admin
ORA_DATA=/home/oracle/10.2.0/oradata/
BACKUP_DIR=/home/oracle/oradump/$ORACLE_SID
SCR_LOC=/usr/local/InMage/Fx
RMANBAT=$SCR_LOC/RMAN"$ORACLE_SID"res.rcv

export  ORACLE_HOME ORACLE_SID

echo ##########################################################################
echo                        Creation of Required directory structure
echo ##########################################################################

mkdir -p $ORA_ADMIN/$ORACLE_SID
mkdir -p $ORA_ADMIN/$ORACLE_SID/bdump
mkdir -p $ORA_ADMIN/$ORACLE_SID/udump
mkdir -p $ORA_ADMIN/$ORACLE_SID/cdump
mkdir -p $ORA_ADMIN/$ORACLE_SID/pfile

mkdir -p $ORA_DATA/$ORACLE_SID/create
mkdir -p $ORA_DATA/$ORACLE_SID/pfile
mkdir -p $ORA_DATA/$ORACLE_SID/archive


echo ##########################################################################
echo                      Creation of Restore RMAN script 
echo ##########################################################################

echo  > $RMANBAT


echo "run {"  >> $RMANBAT
echo STARTUP NOMOUNT PFILE="'"$BACKUP_DIR"/"$ORACLE_SID"."ORA"';" >> $RMANBAT
echo RESTORE controlfile from "'"$BACKUP_DIR"/"$ORACLE_SID"."CTL"';" >> $RMANBAT
echo sql "'"alter database mount standby database"';"  >> $RMANBAT
echo restore database";"  >> $RMANBAT
echo "}"  >> $RMANBAT
 

echo exit >> $RMANBAT


$ORACLE_HOME/bin/rman target sys/inmage cmdfile=$RMANBAT

 
echo ##########################################################################
echo			 Delete RMAN script file
echo ##########################################################################
