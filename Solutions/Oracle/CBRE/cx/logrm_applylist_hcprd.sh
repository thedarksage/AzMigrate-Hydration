#!/bin/sh
ORACLE_SID=hcprd
ARCHPATH="/hcprd/oraarch/$ORACLE_SID"
APPLIED_PATH=/hcprd/oraarch/applied
#Default number of days archives are keps.
#Script also takes the number of days as first argument.
days_to_keep=7

echo $#

#If number of days to keep is passed as argument then use it
if [ $# -gt 0 ]
then
        days_to_keep=$1
fi

cd $ARCHPATH
if [ -s /usr/local/InMage/Fx/loginfo/archives_applied_$ORACLE_SID.txt ]
then
        for i in `cat /usr/local/InMage/Fx/loginfo/archives_applied_$ORACLE_SID.txt`
        do
                if [ -f $i ]
                then
                mv $i $APPLIED_PATH
                fi
        done
fi
find $APPLIED_PATH -name '*.arc' -mtime +$days_to_keep -exec rm -f {} \;


exit 0
