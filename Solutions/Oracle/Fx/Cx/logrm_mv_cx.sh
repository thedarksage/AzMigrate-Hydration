#!/bin/ksh
#####################This script is for copying the latest files from LATEST directory to  archive log directories###########
	programname=$0
	usage()
	{
		echo "oracle job error:" |tee -a $LOGDIR/oraclejob.log
		echo "Usage is $programname -s <ORACLE_SID> -l <Latest directory path> -d <Archive log directory path>" |tee -a $LOGDIR/oraclejob.log
		$INMAGEHOME/alert -f $LOGDIR/oraclejob.log
		echo  "Syntax Error...alert has been sent"
		rm -f  $LOGDIR/oraclejob.log
		exit 1

	}
##################### Intialization Parameters#################################################################################
	INMAGEHOME=`grep INSTALLATION_DIR /usr/local/.fx_version|cut -d"=" -f2`
	LOGDIR=$INMAGEHOME/loginfo
####################Start of Main Program#############################################################################	
	args=0
	[ ! -d $LOGDIR ] && mkdir -p $LOGDIR
	[ $# -eq 0 ] && usage

	while getopts ":s:d:l:" opt; 
	do
		case $opt in
		s ) 	args=`expr $args + 1`
			ORACLE_SID=$OPTARG;;
		d ) 
			args=`expr $args + 1`
			ARCHDIR=$OPTARG;;
		l ) 
			args=`expr $args + 1`
			LATEST=$OPTARG;;
	        \?) usage;;
		esac
	done
	shift $(($OPTIND - 1))

	if [ $args -ne 3 ]
	then
		usage
	
	fi
	if [ ! -d $LATEST ] || [ ! -d $ARCHDIR ]
	then
		echo "oracle job error:" |tee -a $LOGDIR/oraclejob.log
		echo "Either Latest directory or Archive log directory doesn't exists... " |tee -a $LOGDIR/oraclejob.log
		$INMAGEHOME/alert -f $LOGDIR/oraclejob.log
		echo  "alert has been sent.."
		rm -f  $LOGDIR/oraclejob.log
		exit 255
	fi
		
	cd $LATEST
	cnt=`ls -lrt $ARCHDIR|grep "^-"|wc -l`
	if [ $cnt -eq 0  ]
	then
		cp -p -f $LATEST/* $ARCHDIR/
	fi
	LATESTFILE=`ls -ldrt $ARCHDIR/* |tail -1|awk '{ print $9 }'`
	find . -newer $LATESTFILE -type f -exec cp -p -f {} $ARCHDIR \;
	exit 0
