#!/bin/sh
##############Defining the functions########################
####Function:MySqlValidate
####Action:This function to Validate whether username/passwords are correct or not and also to check whether the specified user 
####O/P:It writes output on Screen as well as $TEMP_FILE(for further processing)
############################################################
	MySqlValidate()
	{
		mysql --user="$USER" --password="$PSW" --execute quit 1>>/dev/null 2>>/dev/null
		if test $? -ne 0
		then
			service mysqld status 1>>/dev/null 2>>/dev/null
			status=$?

			if [ $status -ne 0 ]
			then
				echo "ERROR: Mysqld service is not running" | tee -a $ERRORFILE
				echo "Please start mysqld..."|tee -a $ERRORFILE
				$ALERT -f $ERRORFILE
				exit -1
			fi
		

				echo "Either MySql Username or Password are wrong"|tee $ERRORFILE
				echo " "|tee -a $ERRORFILE
				echo "Please check the username and Password.Exiting.."|tee -a $ERRORFILE
				$ALERT -f $ERRORFILE
				exit -1
		else
			
			export USER
			mysql --user=$USER --password=$PSW mysql<<EOF >>$ERRORFILE
			select Lock_tables_priv from user where user="$USER"
EOF
			grep "Y" $ERRORFILE >>/dev/null

			if test $? -ne 0 
			then

				echo "$USER doesn't have previledge to Lock the tables."|tee $ERRORFILE
				echo "Please Grant permission to the user to Lock the tables"|tee -a $ERRORFILE
				echo "Exiting..."|tee -a $ERRORFILE
				exit -1
			else

				echo "DB Validation is successfull"	
				echo " "

			fi

		fi

	
	
	}
##############Defining the functions########################
####Function:MySqlDiscover
####Action:This function Disover the volumes(Filesystems) those are using by MySql instance.
####O/P:It writes output on Screen as well as $TEMP_FILE(for further processing)
############################################################
	MySqlDiscover()
	{

			MySqlValidate
			APPLOGDIR=$LOGDIR/$APP
			if [ ! -d $LOGDIR/$APP ]
			then
				mkdir $APPLOGDIR
			fi
			ps -ef|grep -v "grep"|grep mysqld >>/dev/null
			
			if [ $? -ne 0 ] 
			then
				echo "Couldnot find a running instance of mysqld" |tee $ERRORFILE
				echo "Please start mysql \n"|tee -a $ERRORFILE
				$ALERT -f $ERRORFILE
				exit -1
			fi
			mysqladmin --user="$USER" --password="$PSW" variables|grep -v "grep"|grep datadir >>/dev/null
			if test $? -ne 0 
			then
			
				echo "mysqladmin variables command failed..." |tee $ERRORFILE
				$ALERT -f $ERRORFILE
				exit -1
			
			else
		
				mysqladmin --user="$USER" --password="$PSW" variables |grep -w "datadir" |awk '{ print $4 }' >$TEMP_FILE
	
				cat $TEMP_FILE|while read x
				do
					cd $x 
					$df_command . |tail -1|awk '{ print $NF}' >>$MOUNT_POINTS
				done
				sort $MOUNT_POINTS|uniq >$TEMP_FILE
				echo "mysql is using following filesystems..."
				echo "Following file systems need to be protected using VX "
				echo " "
				cat $TEMP_FILE
			fi
	}
##############Defining the functions########################
####Function:MySqlFreezevacpUnFreeze
####Action:This function puts mysql in Read lock mode,Generates Consistency tag and Unlocks the tables
####
####O/P:Vacp Output
############################################################
	MySqlFreezevacpUnFreeze()
	{ 		
		
		MOUNT_DIR=`cat $TEMP_FILE`
		export DATA_DIR INMAGE_DIR tag
		echo "Puting the Mysql in read lock mode...."

        	mysql --user=$USER --password="$PSW" <<EOF
	
        	flush tables with read lock;
	
		system echo "Issuing consistency Tag on.."
		system echo "$MOUNT_DIR"
        	system $INMAGE_DIR/bin/vacp -v $MOUNT_DIR -t "$tag"
		system echo " "
		system echo "Issuing consistency Tag is done..."
        	unlock tables
EOF
		DATA_DIR=`mysqladmin --user="$USER" --password="$PSW" variables |grep -w "datadir" |awk '{ print $4 }'`
		BASE_DIR=`mysqladmin --user="$USER" --password="$PSW" variables |grep -w "basedir" |awk '{ print $4 }'`
		HOSTNAME=`hostname`
		echo "APPLICATION   = Mysql " > $APPLOGDIR/${APP}_${COMPONENT}.txt
		echo "COMPONENT     = default " >>$APPLOGDIR/${APP}_${COMPONENT}.txt
		echo "FS	    = $MOUNT_DIR">>$APPLOGDIR/${APP}_${COMPONENT}.txt
		echo "DATADIR 	    = $DATA_DIR">>$APPLOGDIR/${APP}_${COMPONENT}.txt
		echo "MOUNTDIR	    = $MOUNT_DIR">>$APPLOGDIR/${APP}_${COMPONENT}.txt
		echo "BASEDIR	    = $BASE_DIR">>$APPLOGDIR/${APP}_${COMPONENT}.txt
		echo "HOSTNAME=$HOSTNAME">>$APPLOGDIR/${APP}_${COMPONENT}.txt


	}

##############Defining the functions########################
####Function:ShowTargetCommand
####Action:This function displays the command that needs to be run on target machine to complete the failover.
####O/P:the command that needs to be run on target machine to complete the failover.
	ShowTargetCommand()
	{
			
			echo "Please run the following command on target host to complete failover..."
			echo " "
			echo "####################################################################################"
			echo " "
			echo "application.sh -a $APP  -u $USER -p $PSW -t $tag -planned -failover "
			echo " "
			echo "#####################################################################################"
			rm -f $TEMP_FILE $ERRORFILE $MOUNT_POINTS
		
	
	}
##############Defining the functions########################
####Function:MySqlStart
####Action:This function start the particular MySql instance
####O/P:"startup" command o/p on terminal
	MySqlStart()
	{
			echo "Starting  mysql services...."
			result=`service mysqld start`
			echo $result
	}		
##############Defining the functions########################
####Function:MySqlShutdown
####Action:This function stop the particular MySql instance
####O/P:"shutdown immediate" command o/p on terminal
	MySqlShutdown()
	{
			echo "Stoping  mysql services...."
			result=`service mysqld stop`
			echo $result
	}		

##############Defining the functions########################
####Function:StopVxAgent
####Action:This function will stop vxagent
####O/P:"Stoping information of vxagent" command o/p on terminal
        StopVxAgent()
        {

                echo "Stopping Vx agent...."
                /etc/init.d/vxagent stop
                echo " "
                echo "Vx agent stopped..."

        }

##############Defining the functions########################
####Function:StartVxAgent
####Action:This function will stop vxagent
####O/P:"Startting information of vx agent " command o/p on terminal
        StartVxAgent()
        {

                echo "Starting Vx agent...."
                /etc/init.d/vxagent start
                echo " "
                echo "Vx agent started..."

        }

##############Defining the functions########################
####Function:MySql Planned Failover
####Action:This function Rollbacks the volumes to particular constency point based the information available in $INMAGEDIR/failover_data/oracle/<oracle_{$ORACLE_SID}.txt file.It stops vx services before cdpcli and start vx services once cdpcli completes.
####O/P:cdpcli command output
	
        MySqlPlannedFailover()
	{



	        APPLOGDIR=$LOGDIR/$APP
		HOSTNAME=`hostname`
		if [ ! -f $APPLOGDIR/${APP}_${COMPONENT}.txt ]
		then
		
			echo "Unable to Determine the File systems required for $APP and $COMPONENT "|tee $ERRORFILE
			echo "Please check there is file in $APPLOGDIR directory"|tee -a $ERRORFILE
			echo "Failover failed..."|tee -a $ERRORFILE
			$ALERT -f $ERRORFILE
			rm -f $ERRORFILE
			exit -1

		fi

		HOSTNAME=`hostname`
		SOURCEHOST=`awk -F "=" '$1=="HOSTNAME" { print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
		if [ "$HOSTNAME" =  "$SOURCEHOST" ]
		then
		
			echo "Source and Target machines are same.Exiting...." |tee $ERRORFILE
			echo "Failover failed..."|tee -a $ERRORFILE
			$ALERT -f $ERRORFILE
			rm -f $ERRORFILE
			exit -1
		fi


		temp=`awk -F "=" '/FS/  { print $2  }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
		####taking filesystem names in seperate file###########
		rm -f $TEMP_FILE
		for mount_point in `echo $temp`
		do
			partition=`grep -w "$mount_point" $FSTABFILE|awk '{ print $1 }' |sed 's/#//g'`
			echo $mount_point $partition |tee -a $TEMP_FILE
		done

		###### changing datadir variable in my.cnf ##########

		DIR=`grep DATADIR $APPLOGDIR/mysql_default.txt | awk '{print $3}'`
		export DIR
		sed -i '/datadir/ c\ datadir='$DIR'' /etc/my.cnf

		#####Issuing cdpcli for Rolling back of volumes#################
		echo "Checking the Consistency Tag Status on target....."
		cat $TEMP_FILE|while read line
		do
			partition=`echo $line|awk '{print $2 }'`
			mountpoint=`echo $line|awk '{ print $1 }'`
			
			while true
			do
				$INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|grep $tag >>/dev/null
				if [ $? -ne 0 ]
					then
		
					echo "Waiting for $tag to Arrive on target.....Sleeping for 30 seconds..."
					sleep 30
				else
					break

				fi
			done
			
		done		

		####StopVxAgent is a function####
		StopVxAgent

		cat $TEMP_FILE|while read line
		do
			partition=`echo $line|awk '{print $2 }'`
			mountpoint=`echo $line|awk '{ print $1 }'`
			
				$INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --mountpoint=$mountpoint --force=yes --event=$tag 
				

		done
		

		####StartVxAgent is a function####
		StartVxAgent

	}

##############Defining the functions########################
####Function:MySql Unplanned Failover
####Action:This function Rollbacks the volumes to latest constency point based on eventnum.It stops vx services before cdpcli and start vx services once cdpcli completes.
####O/P:cdpcli command output
	
        MySqlUnplannedFailover()
	{


	        APPLOGDIR=$LOGDIR/$APP
		HOSTNAME=`hostname`
		if [ ! -f $APPLOGDIR/${APP}_${COMPONENT}.txt ]
		then
		
			echo "Unable to Determine the File systems required for $APP and $COMPONENT "|tee $ERRORFILE
			echo "Please check there is file in $APPLOGDIR directory"|tee -a $ERRORFILE
			echo "Failover failed..."|tee -a $ERRORFILE
			$ALERT -f $ERRORFILE
			rm -f $ERRORFILE
			exit -1

		fi

		HOSTNAME=`hostname`
		SOURCEHOST=`awk -F "=" '$1=="HOSTNAME "{ print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
		if [ "$HOSTNAME" =  "$SOURCEHOST" ]
		then
		
			echo "Source and Target machines are same.Exiting...." |tee $ERRORFILE
			echo "Failover failed..."|tee -a $ERRORFILE
			$ALERT -f $ERRORFILE
			rm -f $ERRORFILE
			exit -1
		fi


		temp=`awk -F "=" '/FS/  { print $2  }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
		####taking filesystem names in seperate file###########
		rm -f $TEMP_FILE
		for mount_point in `echo $temp`
		do
			partition=`grep -w "$mount_point" $FSTABFILE|awk '{ print $1 }' |sed 's/#//g'`
			echo $mount_point $partition |tee -a $TEMP_FILE
		done

		###### changing datadir variable in my.cnf ##########

		DIR=`grep DATADIR $APPLOGDIR/mysql_default.txt | awk '{print $3}'`
		export DIR
		sed -i '/datadir/ c\ datadir='$DIR'' /etc/my.cnf


		if [ $WITHOUT_RETENTION -ne 1 ]
		then

			#####Issuing cdpcli for Rolling back of volumes#################
			echo "Checking the Consistency Tag Status on target....."
			cat $TEMP_FILE|while read line
			do
				partition=`echo $line|awk '{print $2 }'`
				mountpoint=`echo $line|awk '{ print $1 }'`
				
				while true
				do
					$INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|awk '{print $1}' | grep -o "1" | uniq >>/dev/null
					if [ $? -ne 0 ]
					then
			
						echo "Waiting for $tag to Arrive on target.....Sleeping for 30 seconds..."
						sleep 30
					else
						break
	
					fi
				done
				
			done

		fi

		####StopVxAgent is a function####
		StopVxAgent

		cat $TEMP_FILE|while read line
		do
			partition=`echo $line|awk '{print $2 }'`
			mountpoint=`echo $line|awk '{ print $1 }'`

			if [ $WITHOUT_RETENTION -ne 1 ]; then			
				$INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --mountpoint=$mountpoint --force=yes --eventnum=1 
			else
				echo "Unhiding the volume $mountpoint on the partition $partition"
				partition=`grep -w "$mount_point" $FSTABFILE | tail -n1 | awk '{ print $1 }' |sed 's/#//g'`
				FILESYSTEM_TYPE=`grep -w "$mount_point" $FSTABFILE | tail -n1 | awk '{ print $3 }'`
				echo "$INMAGE_DIR/bin/cdpcli --unhide_rw $partition --mountpoint=$mountpoint --force=yes --filesystem=$FILESYSTEM_TYPE"
				$INMAGE_DIR/bin/cdpcli --unhide_rw $partition --mountpoint=$mountpoint --force=yes --filesystem=$FILESYSTEM_TYPE
			fi
		done

		
		####StartVxAgent is a function####
		StartVxAgent

	}

##############Defining the functions########################
####Function:IsMySqlDown
####Action:This function used to check whether MySql Service is UP/NOT
####O/P:
###Return Values: 0 in case of mysql serivce is down;1 in case of mysql service is up

	IsMySqlDown()
	{
	
		service mysqld status|grep -i "stopped" >>/dev/null
		if test $? -eq 0 
		then

			return 0
		else
			
			return 1

		fi
	
	
	
	}
##############Defining the functions########################
####Function:RunPreScript
####Action:This function used to run prescript which is passed to application.sh as arguement before starting actual failover job
####O/P:
        RunPreScript()
        {
                if [ $PRESCRIPT ]
                then
                        echo "Checking Prescript..."
                        if [ -f $PRESCRIPT ]
                        then
                                if [ -x $PRESCRIPT ]
                                then
                                        echo "Running prescript with Root user"
                                        $PRESCRIPT
                                else
                                        echo "Prescript does not have execute permission"
                                        chmod +x $PRESCRIPT
                                        echo "Changing execute permission"
                                        echo "Running prescript with Root user"
                                        $PRESCRIPT
                                fi

                        else
                                echo "Prescript does not exist...Ignoring prescript"
                        fi
                fi
        }

##############Defining the functions########################
####Function:RunPostScript
####Action:This function used to run post script which is passed to application.sh as arguement after finishing failover job
####O/P:

        RunPostScript()
        {
                if [ $POSTSCRIPT ]
                then
                        echo "Checking Post script..."
                        if [ -f $POSTSCRIPT ]
                        then
                                if [ -x $POSTSCRIPT ]
                                then
                                        echo "Running Postscript with Root user"
                                        $POSTSCRIPT
                                else
                                        echo "Postscript does not have execute permission"
                                        chmod +x $POSTSCRIPT
                                        echo "Changing execute permission"
                                        echo "Running postscript with Root user"
                                        $POSTSCRIPT
                                fi

                        else
                                echo "Post script does not exist...Ignoring POST script"
                        fi
                fi
        }




##############Defining the functions########################
	
	##########################Start of the program######################################
        INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
        SCRIPTS_DIR=$INMAGE_DIR/scripts
        LOGDIR=$INMAGE_DIR/failover_data
        TEMP_FILE="$LOGDIR/.files"
        MOUNT_POINTS="$LOGDIR/.mountpoint"
        ERRORFILE=$LOGDIR/errorfile.txt
        ALERT=$INMAGE_DIR/bin/alert



	#########Capturing Arguements those are sent by Application.sh##############################
        programname=$0
	APP=$1
	COMPONENT=$2
	tag=$3
	action=$4
	USER=$5
	PSW=$6
	rm -f $TEMP_FILE $MOUNT_POINTS $ERRORFILE
	MySqlPasswdFile="/root/.my.cnf"
	


	######Calling Corresponding Function based on the arguements#####################
	OS=`uname -s`
       	case $OS in
       	     HP-UX ) df_command="bdf"
               	     FSTABFILE="/etc/fstab";;
       	     Linux ) df_command="df"
               	     FSTABFILE="/etc/fstab"
		     MySqlConfigFile="/etc/my.cnf";;
       	     SunOS ) df_command="df"
               	     FSTABFILE="/etc/vfstab";;
             AIX ) df_command="df"
                     FSTABFILE="/etc/vfstab";;
       	esac
	########################Picking the password from configuration file#########
	echo "Refering to configuration files..."
	if [ -z "$PSW" -a -f $MySqlPasswdFile ]
	then
		CountofPasswordString=`grep -v "^#" $MySqlPasswdFile|grep -i "^password"|wc -l`
		if [ $CountofPasswordString -gt 1 ]
		then
			echo "Password exists more than once in my.cnf file"
			echo "please check it..."
			exit
		fi
		
		PSW=`grep -v "^#" $MySqlPasswdFile|grep -i "^password"|awk -F"=" '{ print $2 }'`

	fi

	case 	$action in
		"discover" )
             
				MySqlDiscover;;
		"consistency" )
             
			
				MySqlDiscover
				MySqlFreezevacpUnFreeze;;
		"showcommand" )
             
				MySqlDiscover
				MySqlFreezevacpUnFreeze
				ShowTargetCommand;;
			
		"plannedfailover" )
				RunPreScript
				IsMySqlDown
				if [ $? -ne 0 ]
				then
					MySqlShutdown
				fi
				MySqlPlannedFailover
				MySqlStart
				RunPostScript;;
		"unplannedfailover" )
				RunPreScript
				IsMySqlDown
				if [ $? -ne 0 ]
				then
					MySqlShutdown
				fi
				MySqlUnplannedFailover
				MySqlStart
				RunPostScript;;
				

	esac
	rm -f $TEMP_FILE $MOUNT_POINTS $ERRORFILE
