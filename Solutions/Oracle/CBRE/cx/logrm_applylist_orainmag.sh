#!/bin/sh -x
ORACLE_SID=orainmag

cd /d06/oradata/orainmag/arch
if [ -s /usr/local/InMage/Fx/loginfo/archives_applied_$ORACLE_SID.txt ]
then
	for i in `cat /usr/local/InMage/Fx/loginfo/archives_applied_$ORACLE_SID.txt`
	do
		if [ -f $i ]
		then
			mv $i /d06/oradata/orainmag/applied
		fi
	done
fi
exit 0
