#!/bin/sh

# Create log file
#LOGTIME=`date "+%Y%m%d%H%M%S"`
#LOG="UninstallAgent"$LOGTIME

LOGDIR=$1
LOG="/var/log/UninstallAgent.log"
touch $LOG

echo "Uninstalling Agent" >> $LOG
sh /usr/local/ASR/uninstall.sh -Y >> $LOG
retval=$?
if [ "$retval" == 0 ]
then
     echo "Success"
else
     echo "fail"
fi

