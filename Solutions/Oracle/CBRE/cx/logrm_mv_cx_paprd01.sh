#!/bin/sh

ORACLE_SID=paprd01
ARCHDIR="/paprd/oraarch/$ORACLE_SID"
LATEST="/paprd/oraarch/latest"


cd $LATEST
if [ ! -d /usr/local/InMage/Fx/loginfo ]
then
	mkdir -p /usr/local/InMage/Fx/loginfo
fi

find . -name '*.dbf'  > /usr/local/InMage/Fx/loginfo/delete_$ORACLE_SID.txt

LATESTFILE=`ls -lrt $ARCHDIR/*.dbf |tail -1|awk '{ print $9 }'`
echo $LATESTFILE
find . -newer $LATESTFILE -type f -exec cp -p -f {} $ARCHDIR \;

exit 0
