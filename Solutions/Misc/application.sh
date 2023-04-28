#!/bin/sh
############################################################################################################################################################
#This Program  application.sh used for issuing consistency tag on protected volumes,failover of oracle/mysql 
#application and for changing the dns entry of the server.
#All rights Reserved solely by InMage Systems.
#Rev 1.0
###########################################################################################################################################################
#################Start of the Program############################################
 	Usage_Oracle()
        {
                        echo "Usage is $programname -a oracle -h <HOME> -c <Component> -u <userid>[-t <Tagname>] -discover -failover -planned -unplanned -P <prescript> -O <postscript> -Help"  |tee $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
                        echo "Where:"|tee -a $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
                        echo "HOME= path of the ORACLE_HOME"|tee -a $TEMP_FILE
                        echo "Component= Instance name(ORACLE_SID)"|tee -a $TEMP_FILE
                        echo "Tagname= Name of the Consistency Tag"|tee -a $TEMP_FILE
			echo "-planned Needed on Source machine to make Source ready for Failover"|tee -a $TEMP_FILE
			echo "-failover Needed on target machine to Failover application"|tee -a $TEMP_FILE
			echo "<prescript> and <postscript> will be run on Target machine while failover only and will be run as 'root'"|tee -a $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
                        echo "-Help = Help"|tee -a $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
			echo "Note:Only on target Machine You need to use -planned/unplanned and -failover combined options to failover the application"|tee -a $TEMP_FILE
                        exit 1

        }
 	Usage()
        {
		echo "Usage:  $programname -a <Application> -Help" | tee -a $TEMP_FILE
                echo "Application = Name of the application.eg. mysql oracle"|tee -a $TEMP_FILE
		exit 1
	}
 	Usage_MySQL()
        {
                        echo "Usage is $programname -a mysql -u <userid>[-t <Tagname>] -discover [-p password ] -failover -planned -unplanned [-noretention] -P <prescript> -O<postscript> -H"  |tee $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
                        echo "Where:"|tee -a $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
			echo "userid = Mysql Database user id"
			echo "-planned Needed on Source machine to make Source ready for Failover"
			echo "-unplanned Needed on target  machine to do Failover"
			echo "-noretention Needed on target  machine to do Failover without media retention"
			echo "-failover  Needed on target machine to Failover application"
			echo "<prescript> and <postscript> will be run on Target machine while failover only and will be run as 'root'"|tee -a $TEMP_FILE
                        echo "-Help Help"|tee -a $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
			echo "Note:Only on target Machine You need to use -planned/unplanned and -failover combined options to failover the application"|tee -a $TEMP_FILE
                        exit 1
	}

CheckArgs()
{

	# Make sure that app is either mysql or oracle
	if [  "$APP" != "mysql"   ]
	then
		if [ "$APP" != "oracle" ]
		then
			Usage ;
		fi
	
	fi

	if [ "$APP" = "mysql" ]
	then
		if  [ "$USER" = "" ]
		then
			echo " ";
			echo "Please enter Username ";
			Usage_MySQL ;
		fi
	fi

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
	fi


}


	INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
	SCRIPTS_DIR=$INMAGE_DIR/scripts
	LOGDIR=$INMAGE_DIR/failover_data
	TEMP_FILE="$LOGDIR/.files"
	MOUNT_POINTS="$LOGDIR/.mountpoint"
	ERRORFILE=$LOGDIR/errorfile.txt
	ALERT=$INMAGE_DIR/bin/alert
	programname=$0
	export WITHOUT_RETENTION=0

	#####using i variable for verifying whether all the required arguments are passed or not#####
	i=0	
	DISCOVER=0
	USER="";

	while getopts ":a:h:c:t:f:p:u:n:H:d:P:O:" opt 
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
		f ) 	
		    failover=$OPTARG;;
		p ) 	
		    if [ "$OPTARG" = "lanned" ]
		    then
		    	planned=$OPTARG
		    else
			PASSWORD=$OPTARG
		    fi
                     ;;
		u ) 
		   if [ "$OPTARG" = "nplanned" ]
		   then
			unplanned=$OPTARG
		   else
		    	USER=$OPTARG
		  fi
		   ;;
		n ) export WITHOUT_RETENTION=1;;
		d ) DISCOVER=1;;
		P )
		    PRESCRIPT=$OPTARG
		    export PRESCRIPT;;
		O ) 
		    POSTSCRIPT=$OPTARG
		    export POSTSCRIPT;;
		H ) 	
			case $APP in 
			"mysql" )
				Usage_MySQL
				;;
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
	if [ -z $tag ]
	then
		i=`expr $i + 1`
                tag1=`date +%Y%m%d%H%M%S`
                tag=`echo ${APP}_$tag1 `
	fi

	CheckArgs;

	
	case 	$APP in #######Calling corresponding script based on application on source for generating cosistency tags only############
		
		"oracle" )
				#If neither failover nor planned is chosen ( -z means no value) then just
				# issue consistency tag.
				if [ $DISCOVER -eq 1 ]
				then

					$SCRIPTS_DIR/oracle.sh oracle $HOME $COMPONENT $tag discover $USER
					code=$?
					exit $code;
				fi
				if [ -z $failover ] && [ -z $planned ]
				then

					 $SCRIPTS_DIR/oracle.sh oracle $HOME $COMPONENT $tag consistency $USER
					 code=$?
					 exit $code;

				fi
				
				if [ -z $failover ] && [ $planned ]
				then

					 $SCRIPTS_DIR/oracle.sh oracle $HOME $COMPONENT $tag showcommand $USER
					 code=$?
					 exit $code;
				fi
				
				if [  $failover ] && [ $planned ]
				then

					 $SCRIPTS_DIR/oracle.sh oracle $HOME $COMPONENT $tag plannedfailover $USER
					 code=$?
					 exit $code;

				fi

				if [  $failover ] && [ $unplanned ]
				then

					 $SCRIPTS_DIR/oracle.sh oracle $HOME $COMPONENT $tag unplannedfailover $USER
					 code=$?
					 exit $code;

				fi
				;;
		"mysql" )
				#If neither failover nor planned is chosen ( -z means no value) then just
				# issue consistency tag.
				if [ $DISCOVER -eq 1 ]
				then

					$SCRIPTS_DIR/mysql.sh mysql default $tag discover $USER $PASSWORD 
					code=$?
					exit $code;

				fi

				#If neither failover nor planned is chosen ( -z means no value) then just
				# issue consistency tag.
				if [ -z $failover ] && [ -z $planned ]
				then

					$SCRIPTS_DIR/mysql.sh mysql default $tag consistency $USER $PASSWORD
					code=$?
					exit $code;

				fi

				if [ -z $failover ] && [ $planned ]
				then

					 $SCRIPTS_DIR/mysql.sh mysql default $tag showcommand $USER $PASSWORD
					 code=$?
					 exit $code;

				fi

				if [  $failover ] && [ $planned ]
				then

					 $SCRIPTS_DIR/mysql.sh mysql default $tag plannedfailover $USER $PASSWORD
					 code=$?
					 exit $code;

				fi
				

				if [  $failover ] && [ $unplanned ]
				then
					$SCRIPTS_DIR/mysql.sh mysql default $tag unplannedfailover $USER $PASSWORD
					code=$?
					exit $code;

				fi
				;;
				
		
		\? ) 		echo "You have Choosen wrong application.Plase choose the correct application name"
				exit 255;;

	esac


