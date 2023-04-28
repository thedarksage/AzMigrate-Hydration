#!/bin/ksh
############################################################################################################################################################
#This Program  application.sh used for issuing consistency tag on protected volumes for Oracle application
#All rights Reserved solely by InMage Systems.
#Rev 1.0
###########################################################################################################################################################
#################Start of the Program############################################
 	Usage_Oracle()
        {
                        echo "Usage is $programname -a oracle -h <HOME> -c <Component> -u <userid> [-t <Tagname>] -v <LunID1:LUNID2:LUNID3...> -Help"  |tee $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
                        echo "Where:"|tee -a $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
                        echo "HOME= path of the ORACLE_HOME"|tee -a $TEMP_FILE
                        echo "Component= Instance name(ORACLE_SID)"|tee -a $TEMP_FILE
                        echo "Tagname= Name of the Consistency Tag"|tee -a $TEMP_FILE
			echo "LunIDs = Lun Numbers "|tee -a $TEMP_FILE
                        echo "-Help = Help"|tee -a $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
                        exit 1

        }
 	Usage()
        {
		echo "Usage:  $programname -a <Application> -Help" | tee -a $TEMP_FILE
                echo "Application = Name of the application.eg. mysql oracle"|tee -a $TEMP_FILE
		exit 1
	}

CheckArgs()
{

	if [ "$APP" = "oracle" ]
	then
		if  [ "$USER" = "" ]
		then
			echo " ";
			echo "Please enter Username ";
			Usage_Oracle ;
		fi

		if  [ "$HOME" = "" ]
		then
			echo " ";
			echo "Please enter Oracle Home ";
			Usage_Oracle ;
		fi

		if  [ "$COMPONENT" = "" ]
		then
			echo " ";
			echo "Please enter Oracle SID ";
			Usage_Oracle ;
		fi
		if  [ "$LunsList" = "" ]
		then
			echo " ";
			echo "Please enter Luns used by Oracle ";
			Usage_Oracle ;
		fi
	fi


}


	INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.fx_version | cut -d'=' -f2`
	CONFIGFILE=$INMAGE_DIR/config.ini
	SCRIPTS_DIR=$INMAGE_DIR/fabric/scripts
	LOGDIR=$INMAGE_DIR/loginfo/fabric
	TEMP_FILE="$LOGDIR/.files"
	MOUNT_POINTS="$LOGDIR/.mountpoint"
	ERRORFILE=$LOGDIR/errorfile.txt
	ALERT=$INMAGE_DIR/bin/alert
	programname=$0
	WITHOUT_RETENTION=0
	
	###Create if scripts directory doesn't exist####
	[ ! -d $SCRIPTS_DIR ] && mkdir -p $SCRIPTS_DIR
	[ ! -d $LOGDIR ] && mkdir -p $LOGDIR


	#####using i variable for verifying whether all the required arguments are passed or not#####
	i=0	
	DISCOVER=0
	USER="";

	if [ $# -eq 0 ]
	then
		Usage_Oracle;

	fi 

	while getopts ":a:h:c:t:u:H:v:" opt 
	do
		case $opt in
		a ) APP=$OPTARG
		    i=`expr $i + 1`;;

		h ) HOME=$OPTARG
		    i=`expr $i + 1`;;

		c ) COMPONENT=$OPTARG
		    i=`expr $i + 1`;;
		t ) 	
		    tag=$OPTARG
		    i=`expr $i + 1`;;
		u ) 
		   if [ "$OPTARG" = "nplanned" ]
		   then
			unplanned=$OPTARG
		   else
		    	USER=$OPTARG
		  fi
		   ;;
		v ) LunsList=$OPTARG;;
			
		H ) 	
			case $APP in 
			"oracle" )
				Usage_Oracle
		   		;;
			\? )
				echo "Invalid application name \n";
				exit;
			esac
			;;
	       	\?) 
			Usage;;
		esac
	done

	shift $(($OPTIND - 1))

	#####Verification of number of arguements.If you increase no of arguements to this program.You need increase this value also ############
	if [ -z "$tag" ]
	then
		i=`expr $i + 1`
                tag1=`date +%Y%m%d%H%M%S`
                tag=`echo ${APP}_$tag1 `
	fi

	CheckArgs;

	
	case 	$APP in #######Calling corresponding script based on application on source for generating cosistency tags only############
		
		"oracle" )

					$SCRIPTS_DIR/oracle.sh oracle $HOME $COMPONENT $tag consistency $USER $LunsList
					code=$?
					exit $code;
				;;
				
		
		\? ) 		echo "You have Choosen wrong application.Plase choose the correct application name"
				exit 255;;

	esac


