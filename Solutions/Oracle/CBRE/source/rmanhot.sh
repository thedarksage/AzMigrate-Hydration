#!/bin/sh -x
echo ##########################################################################
echo                       HOT BACKUP USING ORACLE RMAN 
echo ##########################################################################
echo ##########################################################################
echo      These variable releated to Oracle needs to modified accoring to 
echo      installation of Oracle binaries and Database.
echo ##########################################################################

USER=
PASSWORD=
ORACLE_HOME=/home/oracle/10.2.0
ORACLE_SID=orainmag
BACKUP_DIR=/home/oracle/oradump/"$ORACLE_SID"
SCR_LOC=/home/oracle/scripts
RMANBAT=$SCR_LOC/RMAN"$ORACLE_SID"HOT.rcv

echo ##########################################################################
echo                        Processing for $ORACLE_SID
echo ##########################################################################
 
mkdir -p $BACKUP_DIR

echo CREATE PFILE="'"$BACKUP_DIR"/"$ORACLE_SID"."ORA"'" FROM SPFILE";" > "$SCR_LOC"/"$ORACLE_SID".sql
echo exit >> "$SCR_LOC"/"$ORACLE_SID".sql

$ORACLE_HOME/bin/sqlplus -s """$USER"/"$PASSWORD as sysdba""" @"$SCR_LOC"/"$ORACLE_SID".sql

echo  > $RMANBAT

echo ##########################################################################
echo                        CONNECTING TO RMAN CONFIGURATION
echo ##########################################################################


echo RUN "{" >> $RMANBAT
echo ALLOCATE CHANNEL C1 TYPE DISK";" >> $RMANBAT
echo BACKUP INCREMENTAL LEVEL 0 >> $RMANBAT
echo "("database format "'"$BACKUP_DIR"/"ora_df%chr%t_s%chr%s_s%chr%p.BKP"'"")" ";" >> $RMANBAT
echo COPY CURRENT CONTROLFILE FOR STANDBY TO "'"$BACKUP_DIR"/"$ORACLE_SID"."CTL"'"";" >> $RMANBAT
echo RELEASE CHANNEL C1";" >> $RMANBAT
echo "}" >> $RMANBAT



echo ##########################################################################
echo                        CALL THE BATCH FILE RMANBAT.BAT
echo ##########################################################################

$ORACLE_HOME/bin/rman  TARGET sys/inmage cmdfile=$RMANBAT

echo ##########################################################################

exit 0
