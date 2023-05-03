#!/bin/sh

#This script sends the latest archive log info to CX. 
#Same script is used both on source and target.
#By taking the time stamps from source and target we compare them
#on CX by sending time stamps throug FX job in the form of the file.

INMAGEHOME=/usr/local/InMage/Fx
LOGDIR=$INMAGEHOME/loginfo
ORACLE_SID=$1
ORACLE_HOME=$2
ARCHIVE_LOG_DEST=$3
CLIENT=$4
export ORACLE_SID ORACLE_HOME

echo "Running select command to get archive log number..."
$ORACLE_HOME/bin/sqlplus  -s '/ as sysdba' << EOF  |tee /tmp/.max_file_num
set head off;
select max(sequence#) from v\$log_history;
EOF

max_seq=`grep -v "^$" /tmp/.max_file_num`
echo $max_seq >$LOGDIR/archivenumber_${CLIENT}_$ORACLE_SID.txt

#Since the same scirpt is used on source and target, we are taking
#the type of client as argument. It can be either source or target.
if [ $CLIENT = "source" ]
then
#Get the file name of the latest archive log
        filename=`ls -ld $ARCHIVE_LOG_DEST/*|grep $max_seq|awk ' {print $9 }'`

#Take the time of the latest generated archive log with max(sequnce)
#and set it to the text file so that it will be sent to CX for time comparison
        touch -r $filename  $LOGDIR/archivetime_${CLIENT}_$ORACLE_SID.txt
fi

if [ $CLIENT = "target" ]
then

#archives_applied_sid.txt has all the applied archive log numbers.
#Grep for the latest archive number which is in $max_seq.
#Do uniq just in case to rule out duplicate entries that can happen
#due to partial applies tha can happen because of time based recovery.
#Do Head  -1 just in case. This may not be required really.

	filename=`grep $max_seq $LOGDIR/archives_applied_$ORACLE_SID.txt|uniq|head -1`
#Send only the file name to CX. CX has the time stamp of it already.
	
	tail -1 $LOGDIR/archives_applied_$ORACLE_SID.txt > $LOGDIR/archivetime_${CLIENT}_$ORACLE_SID.txt

fi
