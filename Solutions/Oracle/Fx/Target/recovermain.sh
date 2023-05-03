#!/bin/ksh 
	#####################This script is for copying the latest files from LATEST directory to  archive log directories###########
        programname=$0
        usage()
        {
                echo "oracle job error:" |tee -a $LOGDIR/oraclejob.log
                echo "Usage is $programname -s <ORACLE_SID> -d <Archive log directory path> -h <ORACLE_HOME> -p <delaytime> -u <orauser>" |tee -a $LOGDIR/oraclejob.log
                $INMAGEHOME/alert -f $LOGDIR/oraclejob.log
                echo  "Syntax Error...alert has been sent"
                rm -f  $LOGDIR/oraclejob.log
		exit

        }
	##################### Intialization Parameters#################################################################################
        INMAGEHOME=`grep INSTALLATION_DIR /usr/local/.fx_version|cut -d"=" -f2`
        LOGDIR=$INMAGEHOME/loginfo
  	SCRIPTS="$INMAGEHOME/scripts"
        OSTYPE=`uname`
        case $OSTYPE in
        AIX) LIBPATH=/lib:$LIBPATH:$INMAGEHOME/lib
             export LIBPATH;;
        esac
	####################Start of Main Program######################################################################################
	args=0
        [ ! -d $LOGDIR ] && mkdir -p $LOGDIR
        [ $# -eq 0 ] && usage

        while getopts ":s:d:h:p:u:" opt;
        do
                case $opt in
                s ) 
		    args=`expr $args + 1`
		    ORACLE_SID=$OPTARG;;
                d ) 
		    args=`expr $args + 1`
		    ARCHDIR=$OPTARG;;
                h ) 
		    args=`expr $args + 1`
		    ORACLE_HOME=$OPTARG;;
                p ) 
		    args=`expr $args + 1`
		    DELAYTIME=$OPTARG;;
		u ) 
		    args=`expr $args + 1`
		    ORAUSERID=$OPTARG;;
                \?) usage;;
                esac
        done
        shift $(($OPTIND - 1))
	if [ $args -ne 5 ]
	then
		usage

	fi
	ps -ef|grep -v grep|grep pmon >>/dev/null
	result=$?
	id $ORAUSERID >>/dev/null
	result1=$?

        if [ ! -d $ARCHDIR ] || [ ! -d $ORACLE_HOME ] || [ $DELAYTIME -lt 0 ] || [ $result -ne 0 ] || [ $result1 -ne 0 ]
        then
                echo "oracle job error:" |tee -a $LOGDIR/oraclejob.log
                echo "Either Applied directory or Archive log directory doesn exists on `hostname`" |tee -a $LOGDIR/oraclejob.log
                $INMAGEHOME/alert -f $LOGDIR/oraclejob.log
                echo  "alert has been sent..to you"
                rm -f  $LOGDIR/oraclejob.log

	fi

	#################################################################################################
	# This script for Recovering the DB.
	#################################################################################################
  	chown $ORAUSERID $ARCHDIR/*
  	su - $ORAUSERID -c "$SCRIPTS/recover.sh $ORACLE_SID $ARCHDIR $ORACLE_HOME $DELAYTIME"
	result=$?
	echo $result
	if [ $result -ne 0 ]
	then
		exit 255

	else
  	grep "ORA-00278" $LOGDIR/recoveroutput_$ORACLE_SID.txt | cut -f2 -d"'"|awk -F"/" '{print $NF }' >> $LOGDIR/archives_applied_$ORACLE_SID.txt
  	grep "ORA-00289" $LOGDIR/recoveroutput_$ORACLE_SID.txt |tail -1|cut -f2 -d"'"|awk -F"/" '{print $NF }' > $LOGDIR/neededfile_$ORACLE_SID.txt
	exit 0
	fi
