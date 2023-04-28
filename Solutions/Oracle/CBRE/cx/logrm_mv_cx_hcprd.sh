#!/bin/sh

ORACLE_SID=hcprd
ARCHDIR="/hcprd/oraarch/$ORACLE_SID"
LATEST="/hcprd/oraarch/latest"


cd $LATEST
if [ ! -d /usr/local/InMage/Fx/loginfo ]
then
	mkdir -p /usr/local/InMage/Fx/loginfo
fi

find . -name '*.arc'  > /usr/local/InMage/Fx/loginfo/delete_$ORACLE_SID.txt

LATESTFILE=`ls -lrt $ARCHDIR/*.arc |tail -1|awk '{ print $9 }'`
find . -newer $LATESTFILE -type f -exec cp -p -f {} $ARCHDIR \;

exit 0
