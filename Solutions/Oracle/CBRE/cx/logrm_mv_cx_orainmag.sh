#!/bin/sh -x

ORACLE_SID=orainmag

cd /d06/oradata/orainmag/latest
if [ ! -d /usr/local/InMage/Fx/loginfo ]
then
	mkdir -p /usr/local/InMage/Fx/loginfo
fi

find . -name '*.arc'  > /usr/local/InMage/Fx/loginfo/delete_$ORACLE_SID.txt

mv *.arc /d06/oradata/orainmag/arch

exit 0
