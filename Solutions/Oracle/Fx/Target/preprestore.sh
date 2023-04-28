#!/bin/ksh 
#####################This script is for copying the latest files from LATEST directory to  archive log directories###########
        programname=$0
        usage()
        {
                echo "oracle job error:" |tee -a $LOGDIR/oraclejob.log
                echo "Usage is $programname -s <ORACLE_SID> -h <oracle_home> -p <PFILEPATH> -c <Controlfile path> -u <ora-userid>" |tee -a $LOGDIR/oraclejob.log
                $INMAGEHOME/alert -f $LOGDIR/oraclejob.log
                echo  "Syntax Error...alert has been sent"
                rm -f  $LOGDIR/oraclejob.log
                exit 1

        }
##################### Intialization Parameters#################################################################################
	INMAGEHOME=`grep INSTALLATION_DIR /usr/local/.fx_version|cut -d"=" -f2`
        LOGDIR=$INMAGEHOME/loginfo
	SCRIPTS=$INMAGEHOME/scripts
	ASMUSED=0
####################Start of Main Program#############################################################################

        [ ! -d $LOGDIR ] && (mkdir -p $LOGDIR;chmod 777 $LOGDIR)
        [ $# -eq 0 ] && usage

        while getopts ":s:h:p:c:a:u:" opt;
        do
                case $opt in
                s ) ORACLE_SID=$OPTARG;;
		h ) ORACLE_HOME=$OPTARG;;
                p ) PFILE=$OPTARG;;
		c ) CTRFILE=$OPTARG;;
		u ) USER=$OPTARG;;
                \?) usage;;
                esac
        done
        shift $(($OPTIND - 1))
	grep "ASM=yes" $PFILE >>/dev/null
	if [ $? -eq 0 ]
	then
		echo "************************************"
		echo "You are trying to restore Database which is using ASM"
		echo "Please ensure ASM instance is up"
		echo "**************************************"
		ASMUSED=1	
		grep -v "ASM=yes" $PFILE >$LOGDIR/tempfile
		mv $LOGDIR/tempfile $PFILE
		grep -v ".control_files" $PFILE >$LOGDIR/tempfile
		mv $LOGDIR/tempfile $PFILE

	fi
	ADUMP=`grep "audit_file_dest" $PFILE|awk -F"'" '{ print $2 }'`
	BDUMP=`grep "background_dump_des" $PFILE|awk -F"'" '{ print $2 }'`
	CDUMP=`grep "core_dump_dest" $PFILE|awk -F"'" '{ print $2 }'`
	UDUMP=`grep "user_dump_dest" $PFILE|awk -F"'" '{ print $2 }'`
	RDEST=`grep "db_recovery_file_dest" $PFILE|awk -F"'" '{ print $2 }'`
	CTRLFILE=`grep "control_files" $PFILE|awk -F"'" '{ print $2 }'`
	if [ ! -z "$CTRLFILE" ]
	then
		CTRLDIR=`dirname $CTRLFILE>>/dev/null`
	fi
	ARCHDEST=`grep "log_archive_dest_1" $PFILE|awk -F"'" '{ print $2 }'|awk -F"=" '{ print $2 }'`
	echo "Preparing rmanres.sh script..."
	echo "#!/bin/ksh" >$SCRIPTS/rmanres.sh
	echo "ORACLE_HOME=$ORACLE_HOME" >>$SCRIPTS/rmanres.sh
	echo "ORACLE_SID=$ORACLE_SID">>$SCRIPTS/rmanres.sh
	echo "LD_LIBRARY_PATH=$ORACLE_HOME/lib" >>$SCRIPTS/rmanres.sh
	echo "TNS_ADMIN=$ORACLE_HOME/network/admin" >>$SCRIPTS/rmanres.sh
	echo "PATH=$PATH:$ORACLE_HOME/bin" >>$SCRIPTS/rmanres.sh
	echo "export ORACLE_HOME PATH LD_LIBRARY_PATH TNS_ADMIN ORACLE_SID" >>$SCRIPTS/rmanres.sh
	echo "echo "Copying init.ora file to $ORACLE_HOME/dbs directory..."" >>$SCRIPTS/rmanres.sh
	echo "cp $PFILE $ORACLE_HOME/dbs/init$ORACLE_SID.ora" >>$SCRIPTS/rmanres.sh
	echo "echo "Copying is done."">>$SCRIPTS/rmanres.sh
	echo "echo "Running rman restore command"">>$SCRIPTS/rmanres.sh
	echo "$ORACLE_HOME/bin/rman NOCATALOG << EOF" >>$SCRIPTS/rmanres.sh
	echo "connect target" >>$SCRIPTS/rmanres.sh
	echo "run {" >>$SCRIPTS/rmanres.sh
	echo "STARTUP NOMOUNT PFILE='$PFILE';" >>$SCRIPTS/rmanres.sh
	echo "RESTORE controlfile from '$CTRFILE';" >>$SCRIPTS/rmanres.sh
	echo "sql 'alter database mount standby database';" >>$SCRIPTS/rmanres.sh
	echo "restore database;" >>$SCRIPTS/rmanres.sh
	echo "}" >>$SCRIPTS/rmanres.sh
	echo "EOF" >>$SCRIPTS/rmanres.sh
	echo 'if [ $? -eq 0 ]' >>$SCRIPTS/rmanres.sh
	echo "then" >>$SCRIPTS/rmanres.sh
	echo "	echo "---------------------------------"">>$SCRIPTS/rmanres.sh
        echo "	echo "Rman restore successful...."">>$SCRIPTS/rmanres.sh
	echo "	echo "---------------------------------"">>$SCRIPTS/rmanres.sh
	echo "	exit 0" >>$SCRIPTS/rmanres.sh
	echo "else">>$SCRIPTS/rmanres.sh
	echo "	echo "---------------------------------"">>$SCRIPTS/rmanres.sh
	echo "	echo "Rman restore failed...."" >>$SCRIPTS/rmanres.sh
	echo "	echo "---------------------------------"">>$SCRIPTS/rmanres.sh
	echo "	exit 255">>$SCRIPTS/rmanres.sh
	echo "fi">>$SCRIPTS/rmanres.sh
	chmod +x $SCRIPTS/rmanres.sh
	echo "Preparing rmanres.sh script completed"
	echo "Preparing rmanresmain.sh script..."
	Group=`id $USER|awk '{ print $2 }'|awk -F'(' '{ print $2 }'|sed 's/)//g'`
	echo "#!/bin/ksh" >$SCRIPTS/rmanresmain.sh
	if [ ! -z "$ADUMP" ]
	then
		echo "mkdir -p $ADUMP >>/dev/null " >>$SCRIPTS/rmanresmain.sh
		echo "chown $USER:$Group $ADUMP   " >>$SCRIPTS/rmanresmain.sh
	fi
	if [ ! -z "$BDUMP" ]
	then
		echo "mkdir -p $BDUMP >>/dev/null " >>$SCRIPTS/rmanresmain.sh
		echo "chown $USER:$Group $BDUMP   " >>$SCRIPTS/rmanresmain.sh
	fi
	if [ ! -z "$CDUMP" ]
	then
		echo "mkdir -p $CDUMP >>/dev/null " >>$SCRIPTS/rmanresmain.sh
		echo "chown $USER:$Group $CDUMP	  " >>$SCRIPTS/rmanresmain.sh
	fi
	if [ ! -z "$UDUMP" ]
	then
		echo "mkdir -p $UDUMP >>/dev/null " >>$SCRIPTS/rmanresmain.sh
		echo "chown $USER:$Group $UDUMP   " >>$SCRIPTS/rmanresmain.sh
	fi
	echo $RDEST|grep "+" >>/dev/null
	if  [ $? -ne 0 ] && [ ! -z "$RDEST" ] 
	then
		echo "mkdir -p $RDEST >>/dev/null " >>$SCRIPTS/rmanresmain.sh
		echo "chown $USER:$Group $RDEST   " >>$SCRIPTS/rmanresmain.sh

	fi
	echo $CTRLDIR|grep "+"  >>/dev/null
	if  [ $? -ne 0 ] && [ ! -z "$CTRLDIR" ]
	then
		echo "mkdir -p $CTRLDIR >>/dev/null " >>$SCRIPTS/rmanresmain.sh
		echo "chown $USER:$Group $CTRLDIR   " >>$SCRIPTS/rmanresmain.sh
	fi
	echo $ARCHDEST |grep "+" >>/dev/null
	if  [ $? -ne 0 ] && [ ! -z "$ARCHDEST" ]
	then
		echo "mkdir -p $ARCHDEST >>/dev/null " >>$SCRIPTS/rmanresmain.sh
		echo "chown $USER:$Group $ARCHDEST   " >>$SCRIPTS/rmanresmain.sh
	fi
	echo "su - $USER -c $SCRIPTS/rmanres.sh|tee $LOGDIR/restoreoutput.txt" >>$SCRIPTS/rmanresmain.sh
	echo "if [ \$? -ne 0 ] " >>$SCRIPTS/rmanresmain.sh
	echo "then" >>$SCRIPTS/rmanresmain.sh
	echo "	   echo \"Please refer to $LOGDIR/restoreoutput.txt file\"" >>$SCRIPTS/rmanresmain.sh
	echo "           exit 255" >>$SCRIPTS/rmanresmain.sh
	echo "fi " >>$SCRIPTS/rmanresmain.sh
	if [ $ASMUSED -eq 1 ]
	then
		echo "x=\`grep control $LOGDIR/restoreoutput.txt|grep output | awk -F"=" '{ print \$2 }'\`" >>$SCRIPTS/rmanresmain.sh
		echo "y=\`echo \$x|sed 's/ /, /g'\`" >>$SCRIPTS/rmanresmain.sh
		echo "z=\`echo "'$y'"\`"  >>$SCRIPTS/rmanresmain.sh
		echo "echo *.control_files=\'\$z\' >>$ORACLE_HOME/dbs/init$ORACLE_SID.ora" >>$SCRIPTS/rmanresmain.sh
		echo "echo *.control_files=\'\$z\' >>$PFILE" >>$SCRIPTS/rmanresmain.sh
	fi
	echo "exit 0" >>$SCRIPTS/rmanresmain.sh
	echo "Preparing rmanresmain.sh script completed..."
	chmod +x $SCRIPTS/rmanresmain.sh
	$SCRIPTS/rmanresmain.sh
