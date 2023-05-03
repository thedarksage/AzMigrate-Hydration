#!/bin/ksh

	##################### Intialization Parameters#################################################################################
        INMAGEHOME=/usr/local/InMage/Fx
        LOGDIR=$INMAGEHOME/loginfo
  	SCRIPTS="$INMAGEHOME/scripts"
	CHECK_DB_HOUR=7

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
	####################Start of Main Program######################################################################################
        [ ! -d $LOGDIR ] && mkdir -p $LOGDIR
        [ $# -eq 0 ] && usage

        while getopts ":s:d:h:p:u:" opt;
        do
                case $opt in
                s ) ORACLE_SID=$OPTARG;;
                d ) ARCHDIR=$OPTARG;;
                h ) ORACLE_HOME=$OPTARG;;
                p ) DELAYTIME=$OPTARG;;
		u ) ORAUSERID=$OPTARG;;
                \?) usage;;
                esac
        done
        shift $(($OPTIND - 1))
	ps -ef|grep -v grep|grep pmon >>/dev/null
	result=$?
	id $ORAUSERID >>/dev/null
	result1=$?
	

        if [ ! -d $ARCHDIR ] || [ ! -d $ORACLE_HOME ] || [ $DELAYTIME -lt 0 ] || [ $result -ne 0 ] || [ $result1 -ne 0 ]
        then
                echo "oracle job error:" |tee -a $LOGDIR/oraclejob.log
                echo "Either Applied directory or Archive log directory doesn exists on Cx" |tee -a $LOGDIR/oraclejob.log
                $INMAGEHOME/alert -f $LOGDIR/oraclejob.log
                echo  "alert has been sent..to you"
                rm -f  $LOGDIR/oraclejob.log

	fi

	#################################################################################################
	# This script for Recovering the DB.
	#################################################################################################
  	chown $ORAUSERID $ARCHDIR/*
  	su - $ORAUSERID -c "$SCRIPTS/recover.sh $ORACLE_SID $ARCHDIR $ORACLE_HOME $DELAYTIME"
  	grep "ORA-00278" $LOGDIR/recoveroutput_$ORACLE_SID.txt | cut -f2 -d"'"|awk -F"/" '{print $NF }' >> $LOGDIR/archives_applied_$ORACLE_SID.txt
  	grep "ORA-00289" $LOGDIR/recoveroutput_$ORACLE_SID.txt |tail -1|cut -f2 -d"'"|awk -F"/" '{print $NF }' > $LOGDIR/neededfile_$ORACLE_SID.txt


############
#The below code is for checking the DB status and sending
# alert email once in a day whether DB can be opened or not
############

	if [ ! -f $LOGDIR/oneday_target_$ORACLE_SID.txt ]
	then
		echo "target" >$LOGDIR/oneday_target_$ORACLE_SID.txt
	fi

# We will find for our target file with mtime>0, indicating that it is older
# than one deay. We will get one line if file is older than one daye
# Other wise zero lines are returned.
# Also makes sures that the hour to send the notification is during
# the CHECK_DB_HOUR parameter.

	cnt=`find $LOGDIR -name oneday_target_$ORACLE_SID.txt -mtime +0|wc -l`
	hr=`date +%H`

	
	if [ $cnt -gt 0 ] 
	then
	       su - $ORAUSERID -c "$SCRIPTS/checkdb_${ORACLE_SID}.sh $ORACLE_SID $ORACLE_HOME"
		if [ $? -eq 0 ]
		then
	 		
			echo "Validation of Oracle instance: $ORACLE_SID :SUCCESS"|tee  $LOGDIR/oraclejob.log
			echo " $ORACLE_SID was succesfully opened at `date` "|tee -a $LOGDIR/oraclejob.log
			$INMAGEHOME/alert -f $LOGDIR/oraclejob.log
			rm -f  $LOGDIR/oraclejob.log
		else

			echo "Validation of Oracle instance: $ORACLE_SID :FAILED"|tee  $LOGDIR/oraclejob.log
			echo "Standby database on target couldn't be opened for validation. ORACLE_SID=$ORACLE_SID ORACLE_HOME=$ORACLE_HOME. Please check..it" |tee -a $LOGDIR/oraclejob.log

			$INMAGEHOME/alert -f $LOGDIR/oraclejob.log
                        rm -f  $LOGDIR/oraclejob.log
		fi

		echo "target" >$LOGDIR/oneday_target_$ORACLE_SID.txt
	fi
