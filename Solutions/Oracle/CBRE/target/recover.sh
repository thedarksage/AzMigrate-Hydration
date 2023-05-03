#!/usr/bin/sh -x
echo ##########################################################################
echo                          RECOVERY SCRIPTS
echo ##########################################################################
echo ##########################################################################
echo      These variable releated to Oracle needs to modified accoring to
echo      installation of Oracle binaries and Database.
echo ##########################################################################


ORACLE_HOME=/home/oracle/10.2.0
ORACLE_SID=orainmag
LD_LIBRARY_PATH=$ORACLE_HOME/lib
TNS_ADMIN=$ORACLE_HOME/network/admin
HOME=/usr/local/InMage/Fx
LD_LIBRARY_PATH=$HOME/lib
LOGDIR="$HOME/loginfo"
scr_loc="$HOME/scripts"
TEMPFILE="/tmp/xfile"
PATH=$PATH:$ORACLE_HOME/bin:/usr/local/bin:/usr/ccs/bin:/usr/sbin
export  ORACLE_HOME PATH LD_LIBRARY_PATH TNS_ADMIN ORACLE_SID
EMAILLIST="srikanth.chouta@inmage.net srinivasrao.koganti@inmage.net ganeshvishnu.nalawade@inmage.net"

if [ ! -d $LOGDIR ]
then
	mkdir -p $LOGDIR
fi
##########Finding Archive log file format#######################
sqlplus '/as sysdba'<<EOF >$TEMPFILE
show parameters log_archive_format;
quit;
EOF

######### Getting sequence number Field ##############################
fileformat=`grep "archive" $TEMPFILE|awk '{ print $3 }'`
FieldNo=`echo $fileformat|awk -F"_"  '{ x=NF; for(i=1;i<=NF;i++) if ( $i == "%s") print i; }'`


########Querying the database for Latest Archive Log info########################
sqlplus -S '/as  sysdba'<<EOF >$TEMPFILE
select max(SEQUENCE#) from v\$log_history;
quit;
EOF

oldarchiveno=`tail -2 $TEMPFILE|head -1`
#########Finding the Archive log Destination Directory##################
sqlplus -S '/as sysdba' <<EOF >$TEMPFILE
archive log list;
quit;
EOF

Archivelogdir=`grep "Archive destination" $TEMPFILE |awk '{print $3 }'`
################################Finding the Maximum Archive log number#################################
MaxArchiveNo=0
cd $Archivelogdir
for i in `ls`
do
	temp=`echo $i|cut -f $FieldNo -d "_"`
	if [ $temp -gt $MaxArchiveNo ]
	then
		MaxArchiveNo=$temp
	fi
done
#############################################################################################################
#                              Finding Holes In Archive Log List                                            #
#############################################################################################################
cd $Archivelogdir
Archivetemp=`expr $oldarchiveno + 1`
ls | cut -f $FieldNo -d "_" >$TEMPFILE
while [ $Archivetemp -le $MaxArchiveNo ]
do
        grep $Archivetemp $TEMPFILE >>/dev/null
        if [ $? -ne 0 ]
        then
                echo "Archive Log $Archivetemp Not found " >>$LOGDIR/archiveholes.log
        fi
        Archivetemp=`expr $Archivetemp + 1`
done

#################################################################################################################
#                           Sending Email alert                                                                 #
#################################################################################################################
  if [ -s $LOGDIR/archiveholes.log ]
  then
          echo "$ORACLE_SID archive log errors" >$TEMPFILE
          cat $TEMPFILE $LOGDIR/archiveholes.log >$LOGDIR/archiveholes1.log
	  $HOME/alert -f $LOGDIR/archiveholes1.log
  	  rm -f $LOGDIR/archiveholes.log $LOGDIR/archiveholes1.log
  fi

#################################################################################################################
#                           Recovery of  DB                                                                     #
#################################################################################################################

echo " set echo off feedback off heading off newpage 0 space 0 pagesize 0 trimspool on " > $LOGDIR/recover_tmp.sql
if [ $1 -gt 0 ]; then
echo "select 'recover standby database until time ' || '''' || to_char(sysdate - "$1"/24 , 'yyyy-mm-dd:hh24:mi:ss') || ''';' from dual;" >> $LOGDIR/recover_tmp.sql
else
echo "select 'recover standby database until time ' || '''' || to_char(sysdate , 'yyyy-mm-dd:hh24:mi:ss') || ''';' from dual;" >> $LOGDIR/recover_tmp.sql
fi

$ORACLE_HOME/bin/sqlplus -S "/as sysdba" < $LOGDIR/recover_tmp.sql > $LOGDIR/recover_tmp1.sql

echo "set linesiz 325" > $LOGDIR/recover_$ORACLE_SID.sql
cat $LOGDIR/recover_tmp1.sql >> $LOGDIR/recover_$ORACLE_SID.sql
echo "" >> $LOGDIR/recover_$ORACLE_SID.sql
echo "auto" >> $LOGDIR/recover_$ORACLE_SID.sql
echo "exit" >> $LOGDIR/recover_$ORACLE_SID.sql

if [ ! -f $LOGDIR/neededfile_$ORACLE_SID.txt ] ####For running First time#######
then
        $ORACLE_HOME/bin/sqlplus -S "/as sysdba" < $LOGDIR/recover_$ORACLE_SID.sql >$LOGDIR/recoveroutput_$ORACLE_SID.txt 
else
        filename=`cat $LOGDIR/neededfile_$ORACLE_SID.txt`
        cd $Archivelogdir
        ls|grep $filename >/dev/null
        if [ $? -eq 0 ]
        then
        $ORACLE_HOME/bin/sqlplus -S "/as sysdba" < $LOGDIR/recover_$ORACLE_SID.sql >$LOGDIR/recoveroutput_$ORACLE_SID.txt 
        fi
fi
exit 0
