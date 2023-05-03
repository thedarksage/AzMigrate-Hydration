#!/usr/bin/sh

ORACLE_SID=hcprd
ARHIVEDIR=/hcprd/oraarch/hcprd

cd $ARCHIVEDIR
if [ -s /usr/local/InMage/Fx/loginfo/archives_applied_$ORACLE_SID.txt ]
then
	for i in `cat /usr/local/InMage/Fx/loginfo/archives_applied_$ORACLE_SID.txt`
	do
		if [ -f $i ]
		then
			cd $ARCHIVEDIR
			#rm -f `basename $i`
		fi
	done
fi
exit 0
