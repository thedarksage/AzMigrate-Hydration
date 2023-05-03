#!/bin/sh

#This  script is for monitor the RPOs of source database 
#and target database
#

programname=$0

	usage()
	{
		echo "oracle job error:" |tee $LOGDIR/oraclejob.log
		echo "Usage is $programname -s <ORACLE_SID> -d <Archive log directory path> -a <applied directory> -p <hours to alert (in hours)> " |tee -a $LOGDIR/oraclejob.log
		$INMAGEHOME/alert -f $LOGDIR/oraclejob.log
		echo  "Syntax Error...alert has been sent"
		rm -f  $LOGDIR/oraclejob.log
		exit 1

	}



##################### Intialization Parameters#################################################################################
	INMAGEHOME=/usr/local/InMage/Fx
	LOGDIR=$INMAGEHOME/loginfo
####################Start of Main Program#####################################################################################
	if [ $# -eq 0 ]
	then
		usage;

	fi

	while  getopts ":s:d:a:p:" opt;
	do
		case $opt in
		s ) ORACLE_SID=$OPTARG;;
		d ) ARCHDIR=$OPTARG;;
		p ) hrs_to_alert=$OPTARG;;
		a ) APPLIEDDIR=$OPTARG;;
		\?) usage;;
		esac
	done

	shift $(($OPTIND - 1))
#Read the time information of the lates archive logs(generated on source) and (applied on target) that are sent from source and target
#get_last_arch_info.sh generates this info on source and target.

src=`cat $LOGDIR/archivenumber_source_$ORACLE_SID.txt`
trt=`cat $LOGDIR/archivenumber_target_$ORACLE_SID.txt`


#Just in case when archivelog directory or applied dirs doesn't exist on CX
# Then send an alert and exit
	if [ ! -d $ARCHDIR ] || [ ! -d $APPLIEDDIR ] 
        then
                echo "oracle job error:" |tee -a $LOGDIR/oraclejob.log
                echo "Either Applied directory or Archive log directory doesn exists " |tee -a $LOGDIR/oraclejob.log
                $INMAGEHOME/alert -f $LOGDIR/oraclejob.log
                #echo  "alert has been sent..to you"
                rm -f  $LOGDIR/oraclejob.log
		exit 1

	fi



	
#Get latest applied file name of target
	trtfile=`cat $LOGDIR/archivetime_target_$ORACLE_SID.txt`

#Get the time of latest generated archive on source. 
#This will be the seconds since EPOCH time.
	srctime=`stat --format=%Y $LOGDIR/archivetime_source_$ORACLE_SID.txt`

#Check both in archdir and applieddir just in case if the applied
#arch is not moved yet to applied dir.
	if [ -f $ARCHDIR/$trtfile  ]
	then

#Get time stamp of the latest applied arch file.
		trttime=`stat --format=%Y $ARCHDIR/$trtfile`
	fi
	
	
#Check in applied dir
	if [ -f $APPLIEDDIR/$trtfile  ]
	then
		trttime=`stat --format=%Y $APPLIEDDIR/$trtfile`
	fi

	difftimeSeconds=`expr $srctime - $trttime`
	difftimeMinutes=`expr $difftimeSeconds / 60`
	delayHours=`expr $difftimeMinutes / 60`
	delayMin=`expr $difftimeMinutes % 60`

	if [  ! -f $LOGDIR/onedayold_$ORACLE_SID.txt ]
	then
		
		echo CX > $LOGDIR/onedayold_$ORACLE_SID.txt
	fi

#If the file is older than 24 hours, we get one row. If not 0 rows.
# This is a way of waiting for 24 hours to send positive notification.

	cnt=`find $LOGDIR -name onedayold_$ORACLE_SID.txt -mtime +0 |wc -l`

	if [ $cnt -eq 1 ]
	then
#Since cnt>1 it has been more than 24 hours.

#Reset the time of file to present time. Restting the clock/count for next
#24 hours.
	echo Cx >$LOGDIR/onedayold_$ORACLE_SID.txt
	fi

	if [ $delayHours -gt $hrs_to_alert ] 
	then
	echo "$ORACLE_SID: RPOs Exceeded" |tee $LOGDIR/oraclejob_$ORACLE_SID.txt
	echo "Source Sequence number is: $src"|tee -a $LOGDIR/oraclejob_$ORACLE_SID.txt
	echo "Target Sequence number is: $trt"|tee -a $LOGDIR/oraclejob_$ORACLE_SID.txt
	echo "Time gap between source and target in hrs:mins" |tee -a $LOGDIR/oraclejob_$ORACLE_SID.txt
	echo $delayHours":"$delayMin |tee -a $LOGDIR/oraclejob_$ORACLE_SID.txt
	$INMAGEHOME/alert -f $LOGDIR/oraclejob_$ORACLE_SID.txt
	rm -f $LOGDIR/oraclejob_$ORACLE_SID.txt
	fi

	if [ $cnt -eq 1 ] && [ $delayHours -lt $hrs_to_alert ]
	then
	echo "$ORACLE_SID: Source and Target are in sync" |tee $LOGDIR/oraclejob_$ORACLE_SID.txt
	echo "Source sequence number is: $src"|tee -a $LOGDIR/oraclejob_$ORACLE_SID.txt
	echo "Target sequence number is: $trt"|tee -a $LOGDIR/oraclejob_$ORACLE_SID.txt
	echo "Time gap between source and target in hrs:mins" |tee -a $LOGDIR/oraclejob_$ORACLE_SID.txt
	echo $delayHours":"$delayMin |tee -a $LOGDIR/oraclejob_$ORACLE_SID.txt
	$INMAGEHOME/alert -f $LOGDIR/oraclejob_$ORACLE_SID.txt
	rm -f $LOGDIR/oraclejob_$ORACLE_SID.txt
	fi
