#!/bin/bash
############################################################################################################################################################
#This Program  application.sh used for issuing consistency tag on protected volumes,failover of oracle/mysql 
#application and for changing the dns entry of the server.
#All rights Reserved solely by InMage Systems.
#Rev 1.0
###########################################################################################################################################################
#################Start of the Program############################################
 	Usage_Oracle()
        {
                        echo "Usage is $programname -a oracle -h <HOME> -c <Component> -u <userid>[-t <Tagname>] -remote -serverip <serverip> -serverport <serverport> -csip <csip> -serverdevice <serverdevicelist> -v <list of volumes> -Help"  |tee $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
                        echo "Where:"|tee -a $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
                        echo "HOME= path of the ORACLE_HOME"|tee -a $TEMP_FILE
                        echo "Component= Instance name(ORACLE_SID)"|tee -a $TEMP_FILE
                        echo "Tagname= Name of the Consistency Tag"|tee -a $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
                        echo "-Help = Help"|tee -a $TEMP_FILE
			echo " "|tee -a $TEMP_FILE
                        exit 1

        }
 	Usage()
        {
		echo "Usage:  $programname -a <Application> -Help" | tee -a $TEMP_FILE
                echo "Application = Name of the application.eg. oracle"|tee -a $TEMP_FILE
		exit 1
	}

CheckArgs()
{

	# Make sure that app is either mysql or oracle
	if [ "$APP" != "oracle" ]
	then
		if [ $onlyvacp -eq 0 ]
		then
		    Usage ;
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


	INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.fx_version | cut -d'=' -f2`
	SCRIPTS_DIR=$INMAGE_DIR/tools/scripts
	LOGDIR=$INMAGE_DIR/tools/failover_data
	TEMP_FILE="$LOGDIR/.files"
	MOUNT_POINTS="$LOGDIR/.mountpoint"
	ERRORFILE=$LOGDIR/errorfile.txt
	ALERT=$SCRIPTS_DIR/bin/alert
	programname=$0
	onlyvacp=0
	WITHOUT_RETENTION=0

	#####using i variable for verifying whether all the required arguments are passed or not#####
	i=0	
	DISCOVER=0
	CONSISTENCY=0
	remote=0
	USER="";

	args=$#
	count=1

	while [ $count -le $args ]
	do
            opt=$1

            opt=`echo $opt | sed "s/-//g"`

        case $opt in
        a )
                count=`expr $count + 1`
                shift 1
                value=$1
		APP=$value
                #echo "value for app = $value"
                ;;

        h )
                count=`expr $count + 1`
                shift 1
                value=$1
		HOME=$value
                #echo "value for server = $value"
                ;;

        discover )
                DISCOVER=1 
                ;;
	c )
                count=`expr $count + 1`
                shift 1
                value=$1
		COMPONENT=$value
                #echo "value for instance = $value"
		;;
	csip ) 
                count=`expr $count + 1`
                shift 1
                csip=$1
                #echo "value for csip = $value"
		;;		
	u ) 
                count=`expr $count + 1`
                shift 1
                value=$1
		USER=$value
                #echo "value for user = $value"
		;;
	unplanned )
		unplanned=1
		;;
	t ) 
                count=`expr $count + 1`
                shift 1
                tag=$1
                #echo "value for tag = $value"
	        ;;
	failover )
                 failover=1
		 ;;
	planned )
		planned=1
		 ;;
	p ) 
                count=`expr $count + 1`
                shift 1
                value=$1
                #echo "value for password = $value"
		;;
	n ) 
		WITHOUT_RETENTION=1	
		;;
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
	P ) 
                count=`expr $count + 1`
                shift 1
                value=$1
		export $value	     
		;;
	O ) 
                count=`expr $count + 1`
                shift 1
                value=$1
		export $value
		;;
	v ) 
                count=`expr $count + 1`
                shift 1
                value=$1
		volumes=`echo $value|sed 's/:/,/g'`
		;;
	remote )
	  	remote=1	
 		;;
	noapp )
		onlyvacp=1
		;;
	serverport )
                count=`expr $count + 1`
                shift 1
                serverport=$1
		#echo "value for serverport = $value"
		 ;;
	serverip )
                count=`expr $count + 1`
                shift 1
                serverip=$1
                #echo "value for serverip = $value "
		;;
	serverdevice ) 
                count=`expr $count + 1`
                shift 1
		value=$1
		serverdevice=`echo $value|sed 's/:/,/g'`
		;;
	rac )
		DISCOVER=0;
		;;
        * )
                echo "Wrong usage"
                ;;
        esac

        count=`expr $count + 1`
        shift 1
done


	shift `expr $OPTIND - 1`

	#####Verification of number of arguements.If you increase no of arguements to this program.You need increase this value also ############
	if [ -z $tag ]
	then
		i=`expr $i + 1`
                tag1=`date +%Y%m%d%H%M%S`
                tag=`echo ${APP}_$tag1 `
	fi

	CheckArgs;

	
 #######Calling corresponding script based on application on source for generating cosistency tags only############
		
	if [ $onlyvacp -eq 0 ]
	then
	    $SCRIPTS_DIR/oracle.sh oracle $HOME $COMPONENT $tag consistency $USER $remote $serverport $serverdevice $serverip $volumes
					 code=$?
					 exit $code;
	else
	    $SCRIPTS_DIR/oracle.sh $remote $serverport $serverdevice $serverip $volumes $tag
	fi
				
