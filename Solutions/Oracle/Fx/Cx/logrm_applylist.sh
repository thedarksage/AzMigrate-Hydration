#!/bin/ksh
#####################This script is for copying the latest files from LATEST directory to  archive log directories###########
	programname=$0
	usage()
	{
		echo "oracle job error:" |tee -a $LOGDIR/oraclejob.log
		echo "Usage is $programname -s <ORACLE_SID> -d <Archive log directory path> -a <applied directory> -p <day_to_keep archive logs (in days)> " |tee -a $LOGDIR/oraclejob.log
		$INMAGEHOME/alert -f $LOGDIR/oraclejob.log
		echo  "Syntax Error...alert has been sent"
		rm -f  $LOGDIR/oraclejob.log
		exit 1

	}
##################### Intialization Parameters#################################################################################
	INMAGEHOME=`grep INSTALLATION_DIR /usr/local/.fx_version|cut -d"=" -f2`
	LOGDIR=$INMAGEHOME/loginfo
####################Start of Main Program#####################################################################################	
	args=0
	[ ! -d $LOGDIR ] && mkdir -p $LOGDIR
	[ $# -eq 0 ] && usage

	while getopts ":s:d:a:p:" opt; 
	do
		case $opt in
		s ) 
			args=`expr $args + 1`
			ORACLE_SID=$OPTARG;;
		d ) 
			args=`expr $args + 1`
			ARCHDIR=$OPTARG;;
		a ) 
			args=`expr $args + 1`
			APPLIED=$OPTARG;;
		p ) 
			args=`expr $args + 1`
			days_to_keep=$OPTARG;;
	        \?) usage;;
		esac
	done
	shift $(($OPTIND - 1))
	
	if [ $args -ne 4 ]
	then
		usage

	fi

	if [ ! -d $APPLIED ] || [ ! -d $ARCHDIR ]
	then
		echo "oracle job error:" |tee -a $LOGDIR/oraclejob.log
		echo "Either Applied directory or Archive log directory does not exists..on `hostname` for $ORACLE_SID"|tee -a $LOGDIR/oraclejob.log
		$INMAGEHOME/alert -f $LOGDIR/oraclejob.log
		echo  "alert has been sent..to you"
		rm -f  $LOGDIR/oraclejob.log
		
	fi
		
	
	cd $ARCHDIR
	if [ -s $LOGDIR/archives_applied_$ORACLE_SID.txt ]
	then
        	for i in `cat $LOGDIR/archives_applied_$ORACLE_SID.txt`
        	do
                	if [ -f $i ]
                	then
                	mv $i $APPLIED
                	fi
        	done
	fi
	find $APPLIED -name '*' -type f -mtime +$days_to_keep -exec rm -f {} \;
	exit 0
