#!/usr/bin/sh -x
ORACLE_SID=orainmag
cd /d06/oradata/$ORACLE_SID/arch
if [ -s /usr/local/InMage/Fx/loginfo/delete_$ORACLE_SID.txt ]
then
	for i in `cat /usr/local/InMage/Fx/loginfo/delete_$ORACLE_SID.txt`
	do
		if [ -f $i ]
		then
			cd /d06/oradata/orainmag/arch
			rm -f `basename $i`
		fi
	done
fi
exit 0
