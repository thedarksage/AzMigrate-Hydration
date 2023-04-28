#!/bin/ksh 

##############Defining the functions########################
####Function:OracleFreeze
####Action:This function puts database in Begin Backup mode &Writes the command o/p on screen
####O/P:alter database begin backup command output on screen
############################################################
	OracleFreeze()
	{ 		
		
			echo "Puting the Database $ORACLE_SID in begin backup mode...."
			
			result=`su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF 
                			alter database begin backup
                			/
_EOF"`
			
			echo $result
	}

##############Defining the functions########################
####Function:OracleVacpTagGenerate
####Action:This function Generates consistency tags on volumes those are used by Oracle instance and writes config information in $INMAGEDIR/failover_data/oracle directory
####O/P:vacp command o/p & config information to $INMAGEDIR/failover_data/oracle directory
############################################################

	OracleVacpTagGenerate()
	{
			###Finding Cx server IP address###
			CXServerIP=`grep "SVServer" $CONFIG_FILE|head -1|$AWK -F"=" '{ print $2 }'`
			echo "Issuing the Consistency tag for the below file systems..."
			echo "$LunsList"
			echo " "
			echo "Running below command.."
			LunsListTemp=`echo $LunsList|sed 's/:/,/g'`
			LunsList=$LunsListTemp
                        echo $INMAGE_DIR/vacp -remote -serverdevice $LunsList -t $tag -serverip $CXServerIP 2>&1 
                        ########Issuing vacp tag for all file system with one vacp command
                        result=`$INMAGE_DIR/vacp -remote -serverdevice $LunsList -t $tag -serverip $CXServerIP 2>&1`
				
                        echo $result

	}
##############Defining the functions########################
####Function:OracleUnFreeze
####Action:This function puts database in End Backup mode &Writes the command o/p on screen
####O/P:alter database end backup command output on screen
	OracleUnFreeze()
	{
			result=`su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF 
                		alter database end backup
                		/
_EOF"`
			echo "" && echo "Puting the Database $ORACLE_SID in end backup mode is done..." && echo ""
	}		
##############Defining the functions########################
####Function:OracleStart
####Action:This function start the particular Oracle instance
####O/P:"startup" command o/p on terminal
	OracleStart()
	{
			result=`su - $ORACLE_USER -c "export ORACLE_SID=$ORACLE_SID;export ORACLE_HOME=$ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF
                		startup	
                		/
_EOF"`
			echo $result
	}		
##############Defining the functions########################
####Function:OracleShutdown
####Action:This function stop the particular Oracle instance
####O/P:"shutdown abort" command o/p on terminal
	OracleShutdown()
	{
			result=`su - $ORACLE_USER -c "export ORACLE_SID=$ORACLE_SID;export ORACLE_HOME=$ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF
                		shutdown immediate
                		/
_EOF"`
			echo $result
	}		
		
	OracleDBArchiveLogModeCheck()
	{
		echo "" && echo "Checking archivelog mode..."
		echo "select log_mode from v\$database" >$TEMP_FILE1

		su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S ' /as sysdba' <<EOF2 >$TEMP_FILE2
			@$TEMP_FILE1
			/
EOF2"
		grep "NOARCHIVELOG" $TEMP_FILE2 >>/dev/null
		if [ $? -eq 0 ]
		then
			echo "Archive Log not enabled for this Database.."
			echo "Put the database in ARCHIVELOG mode, Then try ..."
			echo  && echo "exiting..."
			exit 123
		else

			echo "Database is in archive log mode."
			echo "Continuing.." && echo ""

		fi
		


	}


	##########################Start of the program######################################
        INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.fx_version | cut -d'=' -f2`
	CONFIG_DIR="$INMAGE_DIR"
	CONFIG_FILE="$CONFIG_DIR/config.ini"
        SCRIPTS_DIR=$INMAGE_DIR/fabric/scripts
        LOGDIR=$INMAGE_DIR/loginfo/fabric
        TEMP_FILE="$LOGDIR/.files"
	TEMP_FILE1="$LOGDIR/.files1"
	TEMP_FILE2="$LOGDIR/.files2"
	TEMP_FILE3="$LOGDIR/.files3"
	DF_OUTPUT="$LOGDIR/.df_koutput.txt"
        MOUNT_POINTS="$LOGDIR/.mountpoint"
        ERRORFILE=$LOGDIR/errorfile.txt
        ALERT=$INMAGE_DIR/bin/alert
	DISCOVERYXML="$LOGDIR/sample.xml"
        programname=$0
	#########Capturing Arguements those are sent by Application.sh##############################
	APP=$1
	HOME=$2
	COMPONENT=$3
	tag=$4
	action=$5
	USER=$6
	LunsList=$7
	rm -f $TEMP_FILE $MOUNT_POINTS $ERRORFILE $TEMP_FILE1 $TEMP_FILE2
	ORACLE_HOME=$HOME
	ORACLE_SID=$COMPONENT
	ORACLE_USER=$USER
	export ORACLE_HOME ORACLE_SID ORACLE_USER

	######Calling Corresponding Function based on the arguements#####################
	OS=`uname -s`
       	case $OS in
       	     HP-UX ) df_command="bdf"
		     AWK=awk
               	     FSTABFILE="/etc/fstab";;

       	     Linux ) df_command="df"
		     i=1
                     for p in `df -k|grep -v "Mounted on"`
                     do
                     	 echo -n "$p " >> $DF_OUTPUT
                       	 i=`expr $i + 1`
                         x=`expr $i % 7` >>/dev/null
                         if [ $x  -eq 0 ]
                         then
                              echo "" >>  $DF_OUTPUT
                              i=1
                         fi
                     done

               	     FSTABFILE="/etc/fstab"
		     AWK=awk;;

       	     SunOS ) df_command="df"
               	     FSTABFILE="/etc/vfstab"
		     AWK=/usr/xpg4/bin/awk;;

             AIX )  df_command="df"
                     FSTABFILE="/etc/vfstab"
		     AWK=awk;;
       	esac

	case 	$action in
		"consistency" )
				OracleDBArchiveLogModeCheck
				OracleFreeze
				OracleVacpTagGenerate
				OracleUnFreeze;;
	esac
	rm -f $TEMP_FILE $MOUNT_POINTS $ERRORFILE $TEMP_FILE1 $TEMP_FILE2 $DF_OUTPUT
