#!/bin/sh
       ##################### Intialization Parameters#################################################################################
        INMAGEHOME=`grep INSTALLATION_DIR /usr/local/.fx_version|cut -d"=" -f2`
        LOGDIR=$INMAGEHOME/loginfo
        SCRIPTS="$INMAGEHOME/scripts"
	
        #####################This script is for copying the latest files from LATEST directory to  archive log directories###########
        programname=$0
        usage()
        {
                echo "oracle job error:" |tee -a $LOGDIR/oraclejob.log
                echo "Usage is $programname -s <ORACLE_SID> -d <Archive log directory path> -h <ORACLE_HOME> -c <source/target> -u <orauser>" |tee -a $LOGDIR/oraclejob.log

                $INMAGEHOME/alert -f $LOGDIR/oraclejob.log
                echo  "Syntax Error...alert has been sent"
                rm -f  $LOGDIR/oraclejob.log
                exit

        }
        ####################Start of Main Program######################################################################################
        [ $# -eq 0 ] && usage

        while getopts ":s:d:h:c:u:" opt;
        do
                case $opt in
                s ) ORACLE_SID=$OPTARG;;
                d ) ARCHDIR=$OPTARG;;
                h ) ORACLE_HOME=$OPTARG;;
                c ) CLIENT=$OPTARG;;
                u ) ORAUSERID=$OPTARG;;
                \?) usage;;
                esac
        done

su - $ORAUSERID -c "$SCRIPTS/get_last_arch_info.sh $ORACLE_SID $ORACLE_HOME $ARCHDIR $CLIENT"
