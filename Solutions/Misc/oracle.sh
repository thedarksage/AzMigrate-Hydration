#!/bin/sh 
##############Defining the functions########################
####Function:OracleDiscover
####Action:This function Disover the volumes(Filesystems) those are using by Oracle instance.
####O/P:It writes output on Screen as well as $TEMP_FILE(for further processing)
############################################################
	OracleDiscover()
	{

			APPLOGDIR=$LOGDIR/$APP
			if [ ! -d $LOGDIR/$APP ]
			then
				mkdir $APPLOGDIR
			fi
			export ORACLE_SID=$COMPONENT
			export ORACLE_HOME=$HOME
			ORACLE_USER=`ps -ef | grep -w "ora_pmon_${ORACLE_SID}"| grep -v grep |  awk '{ print $1}'`
			ps -ef|grep -v "grep"|grep pmon_$COMPONENT >>/dev/null
			
			if [ $? -ne 0 ] || [ $ORACLE_USER == "" ]
			then
				echo "Could not find a running instance with sid $ORACLE_SID" |tee $ERRORFILE
				echo "Please start oracle \n"|tee -a $ERRORFILE
				$ALERT -f $ERRORFILE
				exit 255
			fi
			oraclehome=`grep -w $ORACLE_SID /etc/oratab|awk -F':' '{ print $2 }'`

			if [ "$oraclehome" != "$ORACLE_HOME" ]
			then
				echo "Refering to /etc/oratab file..."
				sleep 5
				echo "You specified wrong ORACLE HOME" && echo "Please specify correct values"
				echo "Exiting"
				exit 254
			
			fi

			echo "ORACLE_HOME = $ORACLE_HOME";
			echo "ORACLE_SID = $ORACLE_SID";
			echo "ORACLE_USER = $ORACLE_USER";
		
			echo "Verifying Database......";
			sleep 10;
			ps -ef|grep -v grep |grep asmb_${ORACLE_SID} >>/dev/null ####Checking ASM instance for particular DB
			ASM_STATUS=$?
			if [ $ASM_STATUS -eq 0 ]
			then

				echo " "
				echo "ASM instance discovered for Database $ORACLE_SID"
				echo " "

				su - $ORACLE_USER -c "export ORACLE_SID=$ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<END > $TEMP_FILE
				set head off;
				
				select member from v\\\$logfile
				union
				select file_name from dba_data_files
				union
				select name from v\\\$controlfile;
			
END" 
				
				grep "^+" $TEMP_FILE|awk -F"/" '{ print $1 }' >$MOUNT_POINTS
				sort $MOUNT_POINTS|uniq >$TEMP_FILE
				if [ -s $TEMP_FILE ]
				then
					echo "$ORACLE_SID is using following Diskgroup(s) from ASM"
					cat $TEMP_FILE
					count=`wc -l $TEMP_FILE|cut -f1 -d" "`
					if [ $count -gt 1 ]
					then
						echo "$ORACLE_SID using more than one diskgroup"
						echo "More than 1 disk group not supported...."
						echo "exiting.."
						exit 255;

					fi
				else
	
					echo "Couldn't find associated Diskgroup for $ORACLE_SID"
					echo "Failed...Exiting abruptly...."
					exit 255
				fi
				echo "Finding associated Raw volumes in the disk groups"
				sleep 5
				DGGROUP=`cat $TEMP_FILE|cut -d"+" -f2`

				echo "select path from v\$asm_disk where group_number=(select group_number from v\$asm_diskgroup where name='$DGGROUP');" >$LOGDIR/.sqlquery
				su - $ORACLE_USER -c "export ORACLE_SID=$ORACLE_SID;export ORACLE_HOME=$ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<END > $TEMP_FILE
				set head off;
				@$LOGDIR/.sqlquery
END"
				>$MOUNT_POINTS ####clearing content in Tempfile
				case `uname` in
				Linux )	

					if [ ! -f /etc/sysconfig/rawdevices ]
					then
						echo "Unable to find associated physical devices for exiting Raw devices"
						echo "please check whehter /etc/sysconfig/rawdevices file exists or not"
						echo "Exiting..."
						exit 255
					fi

					grep -v "^$" $TEMP_FILE|while read x
					do
						grep -v "^#" /etc/sysconfig/rawdevices|grep $x|awk '{print $2 }' >>$MOUNT_POINTS
					done
					cp $MOUNT_POINTS $TEMP_FILE
					;;

				esac
				echo "$ORACLE_SID is using following devices...."
				sleep 2
				cat $TEMP_FILE
				
				


			else ####Else is for Normal instances....
				su - $ORACLE_USER -c "export ORACLE_SID=$COMPONENT;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<END > $TEMP_FILE
				set head off;
				
				select member from v\\\$logfile
				union
				select file_name from dba_data_files
				union
				select name from v\\\$controlfile;
			
END" 
				FILELIST=`cat $TEMP_FILE|grep -v "rows selected"`
		
				egrep -v "^$|rows selected" $TEMP_FILE|while read x
				do
					cd `dirname $x` 
					$df_command . |tail -1|awk '{ print $NF}' >>$MOUNT_POINTS
				done
				sort $MOUNT_POINTS|uniq >$TEMP_FILE
				echo "$ORACLE_SID using following filesystems..."
				echo " "
				cat $TEMP_FILE
			fi
	}
##############Defining the functions########################
####Function:OracleFreeze
####Action:This function puts database in Begin Backup mode &Writes the command o/p on screen
####O/P:alter database begin backup command output on screen
############################################################
	OracleFreeze()
	{ 		
		
			echo "Puting the Database $ORACLE_SID in begin backup mode...."
			
			result=`su - $ORACLE_USER -c "export ORACLE_SID=$ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF 
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
			echo "" && echo "Tag:$tag" && echo ""
			echo "Generating VACP tag for the below file systems..."
			mount_point=`cat $TEMP_FILE` 
			echo "Issuing the Consistency tag for the below file systems..."
			VACPCOMMANDOPTION=`echo $mount_point|awk 'BEGIN { print "-v" } { for(i=1;i<=NF;i++) printf("%s,",$i) }'`
			
			########Issuing vacp tag for all file system with one vacp command
			result=`$INMAGE_DIR/bin/vacp $VACPCOMMANDOPTION -t $tag 2>&1`
			echo $result
			
			###Taking Filesystem list in FSLIST variable#####################
			FSLIST=`cat $TEMP_FILE`
			MACHINENAME=`hostname`			
			echo "APPLICATION = ORACLE " > $APPLOGDIR/${APP}_${COMPONENT}.txt
			echo "ORACLE_SID = $ORACLE_SID">> $APPLOGDIR/${APP}_${COMPONENT}.txt
			echo "ORACLE_HOME = $ORACLE_HOME">> $APPLOGDIR/${APP}_${COMPONENT}.txt
			echo "FS = "$FSLIST"" >> $APPLOGDIR/${APP}_${COMPONENT}.txt
			echo "Data files = "$FILELIST"" >> $APPLOGDIR/${APP}_${COMPONENT}.txt
			echo "TAG = $tag" >> $APPLOGDIR/${APP}_${COMPONENT}.txt
			echo "SOURCENAME =$MACHINENAME">> $APPLOGDIR/${APP}_${COMPONENT}.txt
			if [ $ASM_STATUS -eq 0 ]
			then
				echo "ASM = TRUE" >> $APPLOGDIR/${APP}_${COMPONENT}.txt
			else
				
				echo "ASM = FALSE" >> $APPLOGDIR/${APP}_${COMPONENT}.txt
			fi
	}
##############Defining the functions########################
####Function:ShowTargetCommand
####Action:This function displays the command that needs to be run on target machine to complete the failover.
####O/P:the command that needs to be run on target machine to complete the failover.
	ShowTargetCommand()
	{
			
			echo "Please run the following command on target host to complete failover..."
			echo "####################################################################################"
			echo " "
			echo "application.sh -a $APP -h $HOME -c $COMPONENT -u $ORACLE_USER -t $tag -planned -failover "
			echo " "
			echo "#####################################################################################"
			rm -f $TEMP_FILE $ERRORFILE $MOUNT_POINTS
		
	
	}
##############Defining the functions########################
####Function:OracleUnFreeze
####Action:This function puts database in End Backup mode &Writes the command o/p on screen
####O/P:alter database end backup command output on screen
	OracleUnFreeze()
	{
			result=`su - $ORACLE_USER -c "export ORACLE_SID=$ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF 
                		alter database end backup
                		/
_EOF"`
			echo $result
			echo "" && echo "Puting the Database $ORACLE_SID in end backup mode is done..." && echo ""
	}		
##############Defining the functions########################
####Function:OracleStart
####Action:This function start the particular Oracle instance
####O/P:"startup" command o/p on terminal
	OracleStart()
	{
			result=`su - $ORACLE_USER -c "export ORACLE_SID=$ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF >>/dev/null
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
			result=`su - $ORACLE_USER -c "export ORACLE_SID=$ORACLE_SID;export ORACLE_HOME=$ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF >>/dev/null
                		shutdown abort	
                		/
_EOF"`
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
####Function:oracle planned failover
####Action:This function Rollbacks the volumes to particular constency point based the information available in $INMAGEDIR/failover_data/oracle/<oracle_{$ORACLE_SID}.txt file.It stops vx services before cdpcli and start vx services once cdpcli completes.
####O/P:vxagent stop,cdpcli ouput,vxagent start
	
        OraclePlannedFailover()
	{


		export ORACLE_SID=$COMPONENT
		export ORACLE_HOME=$HOME

	        APPLOGDIR=$LOGDIR/$APP
		if [ ! -f $APPLOGDIR/${APP}_${COMPONENT}.txt ]
		then
		
			echo "Unable to Determine the File systems required for $APP and $COMPONENT "|tee $ERRORFILE
			echo "Please check there is file in $APPLOGDIR directory"|tee -a $ERRORFILE
			echo "Failover failed..."|tee -a $ERRORFILE
			$ALERT -f $ERRORFILE
			rm -f $ERRORFILE
			exit 255

		fi


		HOSTNAME=`hostname`
		SOURCEHOST=`awk -F "=" '$1=="HOSTNAME "{ print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
		if [ "$HOSTNAME" =  "$SOURCEHOST" ]
		then
		
			echo "Source and Target machines are same.Exiting...." |tee $ERRORFILE
			echo "Failover failed..."|tee -a $ERRORFILE
			$ALERT -f $ERRORFILE
			rm -f $ERRORFILE
			exit 255
		fi
		grep "ASM = TRUE" $APPLOGDIR/${APP}_${COMPONENT}.txt >>/dev/null
		ASM_STATUS=$?
		if [ $ASM_STATUS -eq 0 ]
		then
			temp=`awk -F "=" '$1=="FS " { print $2  }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
			rm -f $TEMP_FILE
			for partition in `echo $temp`
			do

				echo $partition|tee -a $TEMP_FILE
			done

			echo "Checking the Consistency Tag Status on target....."
			cat $TEMP_FILE|while read line
			do
				partition=`echo $line|awk '{ print $1}'`
			
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
			StopVxAgent  ####To Stop Vxagent on targetbefore cdpcli command
			cat $TEMP_FILE|while read line
			do
				partition=`echo $line|awk '{print $1 }'`
			
				$INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --force=yes --event=$tag 
				

			done
			####StartVxAgent is a function#####
			StartVxAgent


		else

			temp=`awk -F "=" '$1=="FS " { print $2  }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
			####taking filesystem names in seperate file###########
			rm -f $TEMP_FILE
			for mount_point in `echo $temp`
			do
				partition=`grep -w "$mount_point" $FSTABFILE|awk '{ print $1 }' |sed 's/#//g'`
				echo $mount_point $partition |tee -a $TEMP_FILE
			done
	
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
			####StopVxAgent is a function######
			StopVxAgent
			cat $TEMP_FILE|while read line
			do
				partition=`echo $line|awk '{print $2 }'`
				mountpoint=`echo $line|awk '{ print $1 }'`
			
				$INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --mountpoint=$mountpoint --force=yes --event=$tag 
				

			done
			
			####StartVxAgent is a function#####
			StartVxAgent
		fi
		}

##############Defining the functions########################
####Function:oracle unplanned failover
####Action:This function Rollbacks the volumes to particular constency point based the information available in $INMAGEDIR/failover_data/oracle/<oracle_{$ORACLE_SID}.txt file.It stops vx services before cdpcli and start vx services once cdpcli completes.
####O/P:vxagent stop,cdpcli ouput,vxagent start
	
        OracleUnplannedFailover()
	{


		export ORACLE_SID=$COMPONENT
		export ORACLE_HOME=$HOME

	        APPLOGDIR=$LOGDIR/$APP
		if [ ! -f $APPLOGDIR/${APP}_${COMPONENT}.txt ]
		then
		
			echo "Unable to Determine the File systems required for $APP and $COMPONENT "|tee $ERRORFILE
			echo "Please check there is file in $APPLOGDIR directory"|tee -a $ERRORFILE
			echo "Failover failed..."|tee -a $ERRORFILE
			$ALERT -f $ERRORFILE
			rm -f $ERRORFILE
			exit 255

		fi


		HOSTNAME=`hostname`
		SOURCEHOST=`awk -F "=" '$1=="HOSTNAME "{ print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
		if [ "$HOSTNAME" =  "$SOURCEHOST" ]
		then
		
			echo "Source and Target machines are same.Exiting...." |tee $ERRORFILE
			echo "Failover failed..."|tee -a $ERRORFILE
			$ALERT -f $ERRORFILE
			rm -f $ERRORFILE
			exit 255
		fi
		grep "ASM = TRUE" $APPLOGDIR/${APP}_${COMPONENT}.txt >>/dev/null
		ASM_STATUS=$?
		if [ $ASM_STATUS -eq 0 ]
		then
			temp=`awk -F "=" '$1=="FS " { print $2  }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
			rm -f $TEMP_FILE
			for partition in `echo $temp`
			do

				echo $partition|tee -a $TEMP_FILE
			done

			echo "Checking the Consistency Tag Status on target....."
			cat $TEMP_FILE|while read line
			do
				partition=`echo $line|awk '{ print $1}'`
			
				while true
				do
					#$INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|grep $tag >>/dev/null
					$INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|awk '{print $1}' | grep -o "1" |uniq >>/dev/null
					if [ $? -ne 0 ]
					then
		
						echo "Waiting for $tag to Arrive on target.....Sleeping for 30 seconds..."
						sleep 30
					else
						break
					fi
				done
			done
			StopVxAgent  ####To Stop Vxagent on targetbefore cdpcli command
			cat $TEMP_FILE|while read line
			do
				partition=`echo $line|awk '{print $1 }'`
			
				$INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --force=yes --eventnum=1 
				

			done
			####StartVxAgent is a function#####
			StartVxAgent


		else

			temp=`awk -F "=" '$1=="FS " { print $2  }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
			####taking filesystem names in seperate file###########
			rm -f $TEMP_FILE
			for mount_point in `echo $temp`
			do
				partition=`grep -w "$mount_point" $FSTABFILE|awk '{ print $1 }' |sed 's/#//g'`
				echo $mount_point $partition |tee -a $TEMP_FILE
			done
	
			#####Issuing cdpcli for Rolling back of volumes#################
			echo "Checking the Consistency Tag Status on target....."
			cat $TEMP_FILE|while read line
			do
				partition=`echo $line|awk '{print $2 }'`
				mountpoint=`echo $line|awk '{ print $1 }'`
				
				while true
				do
					#$INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|grep $tag >>/dev/null
					$INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|awk '{print $1}' | grep -o "1" |uniq >>/dev/null
					if [ $? -ne 0 ]
					then
		
						echo "Waiting for $tag to Arrive on target.....Sleeping for 30 seconds..."
						sleep 30
					else
						break
	
					fi
				done
			
			done		
			####StopVxAgent is a function######
			StopVxAgent
			cat $TEMP_FILE|while read line
			do
				partition=`echo $line|awk '{print $2 }'`
				mountpoint=`echo $line|awk '{ print $1 }'`
			
				$INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --mountpoint=$mountpoint --force=yes --eventnum=1 
				

			done
			
			####StartVxAgent is a function#####
			StartVxAgent
		fi
		}

##############Defining the functions########################
####Function:OracleRecover
####Action:This function Recover the mounted database.
####O/P:"recover"command output
	OracleRecover()
	{
		echo "Starting Recovery of database...."
		su - $ORACLE_USER -c "export ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID=$ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF >$TEMP_FILE1
                			startup;
					recover database;
					alter database open;
					exit;
_EOF"
		chmod 777 $TEMP_FILE $TEMP_FILE1
		su - $ORACLE_USER -c "export ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID=$ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF >$TEMP_FILE
					select status from v\\\$instance;
					exit;
_EOF"
		grep "OPEN" $TEMP_FILE >>/dev/null
		if [ $? -eq 0 ]
		then

			cat $TEMP_FILE1
			echo "Database started and opened"
			exit 0;
		else
			su - $ORACLE_USER -c "export ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID=$ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF  >$TEMP_FILE1
					select member from v\\\$logfile;
					exit;
_EOF"
			for file in `cat $TEMP_FILE1`	
			do
			output=`su - $ORACLE_USER -c "export ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID=$ORACLE_SID;export file=$file;echo $file|$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF 
					recover database;
					exit;
_EOF"`
			done
			su - $ORACLE_USER -c "export ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID=$ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF >$TEMP_FILE
					alter database open;
					select status from v\\\$instance;
					exit;
_EOF"
			grep OPEN $TEMP_FILE >>/dev/null
			if [ $? -eq 0 ]
			then
				echo "Database started,Recovered with redo logs and database opened"
				exit 0
			else
				echo "Unable to open database."
				echo "Recovery process failed...Please check the database manually"
				exit 123
			fi
			
		fi
		
	}
##############Defining the functions########################
####Function:CopyPfileOnSource
####Action:This function is for copying pfile to InMage to directory and will be using on the target machine
####O/P:"Copying configuration information..."command output
	CopyPfileOnSource()
	{
	
	
	echo "SELECT  DECODE(value, NULL, 'PFILE', 'SPFILE') \"Init File Type\"  FROM sys.v_\$parameter WHERE   name = 'spfile' " >$TEMP_FILE1
			su  - $ORACLE_USER -c "export ORACLE_SID=$ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<EOF >$TEMP_FILE
@$TEMP_FILE1
/
EOF"
			echo "" && echo "Copying configuration files..." && echo ""
			grep -v "SPFILE" $TEMP_FILE|grep -v "grep"|grep PFILE 
			x=$?
			if [ $x -eq 0 ]
			then
				su  - $ORACLE_USER -c "export ORACLE_SID=$ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<EOF1 >>/dev/null
				create spfile='/tmp/spfile.ora' from pfile
				/
				create pfile='/tmp/pfile.ora' from spfile='/tmp/spfile.ora'
				/
EOF1"

			fi

			if [ $x -eq 1 ]
			then
				su - $ORACLE_USER -c "export ORACLE_SID=$ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S ' /as sysdba' <<EOF2 >>/dev/null
				create pfile='/tmp/pfile.ora' from spfile
				/
EOF2"
				
			fi
			cp -p /tmp/pfile.ora $APPLOGDIR/${APP}_init${ORACLE_SID}.ora
			rm -f /tmp/pfile.ora

	
	
	}
	
##############Defining the functions########################
####Function:CopyPfileOnTarget
####Action:This function is for copying pfile from InMage to $ORACLE_HOME/dbs
####O/P:"Copying configuration information..."command output
	CopyPfileOnTarget()
	{


		Group=`id $ORACLE_USER|awk '{ print $2 }'|awk -F'(' '{ print $2 }'|sed 's/)//g'`
		TempVariable=`awk -F'=' '/^FS/ {print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
		for i in $TempVariable
		do
			echo "Changing the permissions for $i"
			chown -R $ORACLE_USER:$Group $i
		
		done
		if [ ! -f $ORACLE_HOME/dbs/init${ORACLE_SID}.ora ]
		then
			echo "There is no init${ORACLE_SID}.ora file in ORACLE_HOME/dbs directory"
			echo "Copying it..."
			if [ ! -f $APPLOGDIR/${APP}_init${ORACLE_SID}.ora ]
			then
				echo "There is no ${APP}_init${ORACLE_SID}.ora in  $APPLOGDIR directory"
				echo "Please replicate it from source machine..."
				return 1
	
			fi
			cp -p $APPLOGDIR/${APP}_init${ORACLE_SID}.ora $ORACLE_HOME/dbs/init${ORACLE_SID}.ora
		fi
			chown $ORACLE_USER:$Group $ORACLE_HOME/dbs/init${ORACLE_SID}.ora
			PFILE=$ORACLE_HOME/dbs/init${ORACLE_SID}.ora
			ADUMP=`grep "audit_file_dest" $PFILE|awk -F"'" '{ print $2 }'`
			BDUMP=`grep "background_dump_des" $PFILE|awk -F"'" '{ print $2 }'`
			CDUMP=`grep "core_dump_dest" $PFILE|awk -F"'" '{ print $2 }'`
			UDUMP=`grep "user_dump_dest" $PFILE|awk -F"'" '{ print $2 }'`
			RDEST=`grep "db_recovery_file_dest" $PFILE|awk -F"'" '{ print $2 }'`
			CTRLFILE=`grep "control_files" $PFILE|awk -F"'" '{ print $2 }'`
			if [ ! -z "$CTRLFILE" ]
			then
				CTRLDIR=`dirname $CTRLFILE>>/dev/null`
			fi
			ARCHDEST=`grep "log_archive_dest_1" $PFILE|awk -F"'" '{ print $2 }'|awk -F"=" '{ print $2 }'`
			rm -f $TEMP_FILE
			if [ ! -z "$ADUMP" ]
			then
				echo "mkdir -p $ADUMP >>/dev/null " >$TEMP_FILE
				echo "chown $ORACLE_USER:$Group $ADUMP" >>$TEMP_FILE
			fi
			if [ ! -z "$BDUMP" ]
			then
				echo "mkdir -p $BDUMP >>/dev/null " >>$TEMP_FILE
				echo "chown $ORACLE_USER:$Group $BDUMP" >>$TEMP_FILE
			fi
			if [ ! -z "$CDUMP" ]
			then
				echo "mkdir -p $CDUMP >>/dev/null " >>$TEMP_FILE
				echo "chown $ORACLE_USER:$Group $CDUMP" >>$TEMP_FILE
			fi
			if [ ! -z "$UDUMP" ]
			then
				echo "mkdir -p $UDUMP >>/dev/null " >>$TEMP_FILE 
				echo "chown $ORACLE_USER:$Group $UDUMP" >>$TEMP_FILE
			fi

			echo $RDEST|grep "+" >>/dev/null
			if  [ $? -ne 0 ] && [ ! -z "$RDEST" ] 
			then
				echo "mkdir -p $RDEST >>/dev/null " >>$TEMP_FILE
				echo "chown $ORACLE_USER:$Group $RDEST" >>$TEMP_FILE
			fi
			echo $CTRLDIR|grep "+"  >>/dev/null
			if  [ $? -ne 0 ] && [ ! -z "$CTRLDIR" ]
			then
				echo "mkdir -p $CTRLDIR >>/dev/null " >>$TEMP_FILE
				echo "chown $ORACLE_USER:$Group $CRTLDIR" >>$TEMP_FILE
			fi
			echo $ARCHDEST |grep "+" >>/dev/null
			if  [ $? -ne 0 ] && [ ! -z "$ARCHDEST" ]
			then
				echo "mkdir -p $ARCHDEST >>/dev/null " >>$TEMP_FILE
				echo "chown $ORACLE_USER:$Group $ARCHDEST ">>$TEMP_FILE
			fi
			if [ -s $TEMP_FILE ]
			then
				echo "Creating necessary directories..."
				sh $TEMP_FILE

			fi
			
	}
	OracleDBArchiveLogModeCheck()
	{
		echo "" && echo "Checking archivelog mode..."
		echo "select log_mode from v\$database" >$TEMP_FILE1
		su - $ORACLE_USER -c "export ORACLE_SID=$ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S ' /as sysdba' <<EOF2 >$TEMP_FILE2
				@$TEMP_FILE1
				/
EOF2"
		grep -w "ARCHIVELOG" $TEMP_FILE2 >>/dev/null
		if [ $? -ne 0 ]
		then
			echo "Archive Log not enabled for this Database.."
			echo "Put the database in begin backup mode, Then try ..."
			echo  && echo "exiting..."
			exit 123
		else

			echo "Database is in archive log mode."
			echo "Continuing.." && echo ""

		fi
		


	}
	AddEntryInOratabFile()
	{


		echo "Checking /etc/oratab file..."
		if [ -f /etc/oratab ]
		then
			grep -w "$ORACLE_SID" /etc/oratab >>/dev/null
			if [ $? -ne 0 ]
			then
				echo "Adding entry in /etc/oratab file"
				echo "$ORACLE_SID:$ORACLE_HOME:N" >>/etc/oratab
				echo "Entry added /etc/oratab file"
			else

				echo "$ORACLE_SID already present in /etc/oratab file"

			fi

		else
			echo "/etc/oratab file doesn't exist."
			echo "Please create /etc/oratab ..."
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
####Action:This function used to run postscript which is passed to application.sh as arguement after finshing the failover job
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


	##########################Start of the program######################################
        INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
        SCRIPTS_DIR=$INMAGE_DIR/scripts
        LOGDIR=$INMAGE_DIR/failover_data
        TEMP_FILE="$LOGDIR/.files"
	TEMP_FILE1="$LOGDIR/.files1"
	TEMP_FILE2="$LOGDIR/.files2"
        MOUNT_POINTS="$LOGDIR/.mountpoint"
        ERRORFILE=$LOGDIR/errorfile.txt
        ALERT=$INMAGE_DIR/bin/alert
        programname=$0
	#########Capturing Arguements those are sent by Application.sh##############################
	APP=$1
	HOME=$2
	COMPONENT=$3
	tag=$4
	action=$5
	USER=$6
	rm -f $TEMP_FILE $MOUNT_POINTS $ERRORFILE $TEMP_FILE1 $TEMP_FILE2
	export ORACLE_HOME=$HOME
	export ORACLE_SID=$COMPONENT
	export ORACLE_USER=$USER

	######Calling Corresponding Function based on the arguements#####################
	OS=`uname -s`
       	case $OS in
       	     HP-UX ) df_command="bdf"
               	     FSTABFILE="/etc/fstab";;
       	     Linux ) df_command="df"
               	     FSTABFILE="/etc/fstab";;
       	     SunOS ) df_command="df"
               	     FSTABFILE="/etc/vfstab";;
             AIX ) df_command="df"
                     FSTABFILE="/etc/vfstab";;
       	esac

	case 	$action in
		"consistency" )
             
				OracleDiscover
				OracleDBArchiveLogModeCheck
				OracleFreeze
				OracleVacpTagGenerate	
				CopyPfileOnSource
				OracleUnFreeze;;
		"showcommand" )
             
				OracleDiscover
				OracleDBArchiveLogModeCheck
				OracleFreeze
				OracleVacpTagGenerate	
				CopyPfileOnSource
				OracleUnFreeze
				ShowTargetCommand;;
			
		"plannedfailover" )
				RunPreScript
				OracleShutdown
				OraclePlannedFailover
				CopyPfileOnTarget
				AddEntryInOratabFile
				OracleRecover
				RunPostScript;;
		"unplannedfailover" )
				RunPreScript
				OracleShutdown
				OracleUnplannedFailover
				CopyPfileOnTarget
				AddEntryInOratabFile
				OracleRecover
				RunPostScript;;
		"discover" )
				OracleDiscover;;
				

	esac
	rm -f $TEMP_FILE $MOUNT_POINTS $ERRORFILE $TEMP_FILE1 $TEMP_FILE2
