#!/bin/bash

#This Will collect all the required files by running first command and next it will take the snapshot on running the next command.
#It will run first "EsxUtil -drdrill [-d <Directory Path>]" to get the required files,
#Next it will run "EsxUtil -physnapshot [-d <Directory Path>]" to take the physical snapshot.

INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
SCRIPTS_DIR=$INMAGE_DIR/scripts
LOGDIR=$INMAGE_DIR/failover_data
BINDIR=$INMAGE_DIR/bin

echo "Started Physical Snapshot Operation.."
touch $LOGDIR/EsxUtil_Linux_FxJob.txt

echo "Snapshot command : ./VCon_Physical_Snapshot.sh $*"

if [ -z "$1" ];then
	echo "First command to execute: $BINDIR/EsxUtil -drdrill"
	$BINDIR/EsxUtil -drdrill >> $LOGDIR/EsxUtil_Linux_FxJob.txt
else
	if [ -z "$4" ];then
		echo "First command to execute: $BINDIR/EsxUtil -drdrill -d "$1" -cxpath "$2" -planid "$3" "
		$BINDIR/EsxUtil -drdrill -d "$1" -cxpath "$2" -planid "$3" >> $LOGDIR/EsxUtil_Linux_FxJob.txt
	else
		echo "First command to execute: $BINDIR/EsxUtil -drdrill -d "$1" -cxpath "$2" -planid "$3" -op "$4" "
		$BINDIR/EsxUtil -drdrill -d "$1" -cxpath "$2" -planid "$3" -op "$4" >> $LOGDIR/EsxUtil_Linux_FxJob.txt
	fi
fi

if [ $? -ne 0 ]
then
	cat $LOGDIR/EsxUtil_Linux_FxJob.txt
	echo "Physical Snapshot operation Failed while running the first command."
	echo "Exiting abnormally......."
	exit 1
fi
		
touch $LOGDIR/EsxUtil_FxJob.txt

if [ -z "$1" ];then
	echo "Second Command to Execute: $BINDIR/EsxUtil -physnapshot"
	$BINDIR/EsxUtil -physnapshot >> $LOGDIR/EsxUtil_FxJob.txt
else
	if [ -z "$4" ];then
		echo "Second Command to Execute: $BINDIR/EsxUtil -physnapshot -d "$1" -cxpath "$2" -planid "$3" "
		$BINDIR/EsxUtil -physnapshot -d "$1" -cxpath "$2" -planid "$3" >> $LOGDIR/EsxUtil_FxJob.txt
	else
		echo "Second Command to Execute: $BINDIR/EsxUtil -physnapshot -d "$1" -cxpath "$2" -planid "$3" -op "$4" "
		$BINDIR/EsxUtil -physnapshot -d "$1" -cxpath "$2" -planid "$3" -op "$4" >> $LOGDIR/EsxUtil_FxJob.txt
	fi
fi

if [ $? -ne 0 ]
then
	cat $LOGDIR/EsxUtil_FxJob.txt
	echo "Physical Snapshot operation Failed while second command."
	echo "Exiting abnormally......."
	exit 1
fi

echo "Successfully completed the physical snapshot operation"
echo "Exiting gracefully...."
exit 0
