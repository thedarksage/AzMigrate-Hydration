#!/bin/bash
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
			ORACLE_SID=$COMPONENT
			ORACLE_HOME=$HOME
			export ORACLE_SID ORACLE_HOME
			ORACLE_USER=`ps -ef|grep -w ora_pmon_${ORACLE_SID}|grep -v grep|$AWK '{ print $1 }'`
			ps -ef|grep -v "grep"|grep -w ora_pmon_${COMPONENT} >>/dev/null
			if [ $? -ne 0 ] || [ "$ORACLE_USER" == "" ]
			then
				echo "Could not find a running instance with sid $ORACLE_SID" |tee $ERRORFILE
				echo "Please start oracle \n"|tee -a $ERRORFILE
				exit 255
			fi

			oraclehome=`$AWK -F':' -v oraclesid=$ORACLE_SID '$1==oraclesid { print $2 }' $ORATABFILE`
				
			#oraclehome=`grep -w $ORACLE_SID $ORATABFILE|$AWK -F':' '{ print $2 }'`
			if [ "$oraclehome" == "" ]
			then
				echo "Unable to find entry for $ORACLE_SID in $ORATABFILE"	
				echo "Ensure that you have entry in $ORATABFILE"
				echo "Exiting.."
				exit 255 

			fi

	  		oraclehome=`grep -w $ORACLE_SID $ORATABFILE|$AWK -F':' '{ print $2 }'`

			if [ "$oraclehome" != "$ORACLE_HOME" ]
			then
				echo "Refering to $ORATABFILE file..."
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
			ps -ef|grep -v grep |grep asmb_${ORACLE_SID} >>/dev/null 
			####Checking ASM instance for particular DB
			ASM_STATUS=$?
			if [ $ASM_STATUS -eq 0 ]
			then

				echo " "
				echo "ASM instance discovered for Database $ORACLE_SID"
				echo " "

				su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<END > $TEMP_FILE
				set head off;
				
				select member from v\\\$logfile
				union
				select file_name from dba_data_files
				union
				select name from v\\\$controlfile;
			
END" 
				
				grep "^+" $TEMP_FILE|$AWK -F"/" '{ print $1 }' >$MOUNT_POINTS
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
				su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<END > $TEMP_FILE
				set head off;
				@$LOGDIR/.sqlquery
END"
				>$MOUNT_POINTS ####clearing content in Tempfile
				case `uname` in
				Linux )	

				        if [ -f /etc/redhat-release ]
 					then	
						if [ ! -f /etc/sysconfig/rawdevices ]
						then
							echo "Unable to find associated physical devices for exiting Raw devices"
							echo "please check whehter /etc/sysconfig/rawdevices file exists or not"
							echo "Exiting..."
					                exit 255
						fi

						grep -v "^$" $TEMP_FILE|while read x
						do
							grep -v "^#" /etc/sysconfig/rawdevices|grep -w $x|$AWK '{print $2 }' >>$MOUNT_POINTS
						done
						cp $MOUNT_POINTS $TEMP_FILE
					fi
					
                                        
                                        if [ -f /etc/SuSE-release ]
					then	
						if [ ! -f /etc/raw ]
						then
							echo "Unable to find associated physical devices for exsiting Raw devices"
                                                        echo "please check whehter /etc/raw file exists or not"
                                                        echo "Exiting..."
                                                        exit 255        
						fi
				      
      
				                      
                                      		grep -v "^$" $TEMP_FILE|cut -d"/" -f4>$TEMP_FILE1
                                      		cat $TEMP_FILE1
				      		cat $TEMP_FILE1|while read x
                                      		do
                                      			grep -v "^#" /etc/raw|grep -w $x|awk -F":" '{ print $2 }' >>$MOUNT_POINTS
				
                                      		done
						awk '{ print "/dev/" $1 }' $MOUNT_POINTS >$TEMP_FILE
						cp $TEMP_FILE $MOUNT_POINTS
                                         fi  
                                  ;;
				
                                


				esac
				echo "$ORACLE_SID is using following devices...."
				sleep 2
				cat $TEMP_FILE
				
				


			else ####Else is for Normal instances without ASM(Mount point based)####
				su - $ORACLE_USER -c "ORACLE_SID=$COMPONENT;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<END > $TEMP_FILE
				set head off;
				
				select member from v\\\$logfile
				union
				select file_name from dba_data_files
				union
				select name from v\\\$controlfile;
			
END" 
				FILELIST=`cat $TEMP_FILE|grep -v "rows selected"`
				isAIX=0
				case $OS in 
				     Linux ) 
					egrep -v "^$|rows selected" $TEMP_FILE|while read x
					do
						cd `dirname $x` 
						$df_command -P . |tail -1 |$AWK '{ print $NF}' >>$MOUNT_POINTS
					done;;
				      SunOS )
					egrep -v "^$|rows selected" $TEMP_FILE|while read x
					do
						cd `dirname $x` 
						$df_command . |tail -1 |$AWK '{ print $1}' >>$MOUNT_POINTS
					done;;
					AIX )
						isAIX=1
					 egrep -v "^$|rows selected" $TEMP_FILE|while read x
                                        do
                                                cd `dirname $x`
                                                $df_command . |tail -1 |$AWK '{ print $NF}' >>$MOUNT_POINTS
                                        done;;	
				esac
				sort $MOUNT_POINTS|uniq >$TEMP_FILE
				echo "$ORACLE_SID using following filesystems..."
				echo " "
				cat $TEMP_FILE
				rm -f $PVDEVICES
				if [ $isAIX -eq 1 ]
				then
			            cat $TEMP_FILE|while read line
				    do
				    	$INMAGE_DIR/scripts/map_devices_aix.sh $line >>$PVDEVICES
			            done
					
				    cat $PVDEVICES|$AWK -F":" '{print $3 }'|sed 's/,/ /g'>listofpvs
				
				    ListOfPVs=`cat listofpvs`
				    cat $PVDEVICES|$AWK -F":" '{print $2 }'|sed 's/,/ /g'>listofvgs
				    ListOfVGs=`cat listofvgs`
				    rm -f listofvgs listofpvs
				    echo "Please protect following full devices in order to protect database..."
				    echo $ListOfPVs
				fi
			fi
	##########Writing Discovery information to files########
                        rm -f $TEMP_FILE1
                        ###Collecting information about the partition  names for corresponding mount points
                        cat $TEMP_FILE|while read x
                        do

                                $AWK -v var=$x '$NF==var { print $1 }' $DF_OUTPUT >>$TEMP_FILE1

                        done

                        PARTITIONS=`cat $TEMP_FILE1`
                        FSLIST=`cat $TEMP_FILE`

                        ###Determining SourceHostID######
                        SourceHostId=`grep -i hostid $CONFIG_FILE|$AWK -F"=" '{ print $2 }'`

                        MACHINENAME=`hostname`
                        echo "APPLICATION = ORACLE " > $APPLOGDIR/${APP}_${COMPONENT}.txt
                        echo "ORACLE_SID = $ORACLE_SID">> $APPLOGDIR/${APP}_${COMPONENT}.txt
                        echo "ORACLE_HOME = $ORACLE_HOME">> $APPLOGDIR/${APP}_${COMPONENT}.txt
                        echo "FS = "$FSLIST"" >> $APPLOGDIR/${APP}_${COMPONENT}.txt
                        echo "PARTITIONS = "$PARTITIONS"">> $APPLOGDIR/${APP}_${COMPONENT}.txt
                        echo "Data files = "$FILELIST"" >> $APPLOGDIR/${APP}_${COMPONENT}.txt
                        echo "SOURCENAME =$MACHINENAME">> $APPLOGDIR/${APP}_${COMPONENT}.txt
                        echo "SourceHostId =$SourceHostId">> $APPLOGDIR/${APP}_${COMPONENT}.txt
			if [ $isAIX -eq 1 ]
			then
  			    echo "PVList = $ListOfPVs">> $APPLOGDIR/${APP}_${COMPONENT}.txt
		            cat $PVDEVICES|while read line
        		    do
            			echo $line|grep ":rootvg:">>/dev/null
            			if [ $? -ne 0 ]
            			then
                		    echo "PVDevices = $line">>$APPLOGDIR/${APP}_${COMPONENT}.txt
            			else
                		    echo ""
            			fi
        		    done
			fi
                        if [ $ASM_STATUS -eq 0 ]
                        then
                                echo "ASM = TRUE" >> $APPLOGDIR/${APP}_${COMPONENT}.txt
                        else

                                echo "ASM = FALSE" >> $APPLOGDIR/${APP}_${COMPONENT}.txt
                        fi


	}
##############Defining the functions########################
####Function:OracleFreeze
####Action:This function puts database in Begin Backup mode &Writes the command o/p on screen
####O/P:alter database begin backup command output on screen
############################################################
	OracleFreeze()
	{ 		
		
			echo "Changing the Database $ORACLE_SID to  begin backup mode...."
			
			result=`su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF 
                			alter database begin backup;
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
                        echo "Generating VACP tag for the below file systems..."
                        mount_point=`cat $TEMP_FILE`
			echo "Issuing the Consistency tag for the below file systems..."
			VACPCOMMANDOPTION=`echo $mount_point |$AWK 'BEGIN { print "-v" } { for(i=1;i<=NF;i++) printf("%s,",$i) }'`
			if [ $isAIX -eq 1 ]
			then
			    VACPCOMMANDOPTION=`echo $mount_point $ListOfPVs|$AWK 'BEGIN { print "-v" } { for(i=1;i<=NF;i++) printf("%s,",$i) }'`
			fi
                        ########Issuing vacp tag for all file system with one vacp command
                        result=`$INMAGE_DIR/bin/vacp $VACPCOMMANDOPTION -t $tag 2>&1`
                        echo $result
			##Writing tag information to file
                        echo "TAG = $tag" >> $APPLOGDIR/${APP}_${COMPONENT}.txt

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
			result=`su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF 
                		alter database end backup
                		/
_EOF"`
			echo $result
			echo "" && echo "Reverting the Database $ORACLE_SID to normal mode..." && echo ""
	}		
##############Defining the functions########################
####Function:OracleStart
####Action:This function start the particular Oracle instance
####O/P:"startup" command o/p on terminal
	OracleStart()
	{
			result=`su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF
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
			ps -ef|grep -v "grep"|grep pmon_${ORACLE_SID} >>/dev/null
			if [ $? -eq 0 ]
			then
				result=`su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF
                			shutdown immediate
                			/
_EOF"`
				echo $result
			fi
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
####Function:GetTargetPartitionInfo
####Action:This function will get the information from CX for the source volumes and its corresponding target volumes information
####O/P:Prepartion of TEMP_FILE with mountpoints and volume information
	GetTargetPartitionInfo()
	{
                        ##Removing old temporary files##
                        rm -f $TEMP_FILE $TEMP_FILE1 $TEMP_FILE2
                        SourcePartitionsList=`$AWK -F "=" '$1=="PARTITIONS " { print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
                        SourceMountPointList=`$AWK -F "=" '$1=="FS " { print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
			if [ $AIX -eq 1 ]
			then
				SourcePartitionsList=`$AWK -F "=" '$1=="PVList " { print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
			fi

                        for Partition in `echo $SourcePartitionsList`
                        do

                                echo $Partition >>$TEMP_FILE1
                        done
                        for MountPoint in `echo $SourceMountPointList`
                        do
                                echo  $MountPoint >>$TEMP_FILE2
                        done
                        paste $TEMP_FILE1 $TEMP_FILE2 >$TEMP_FILE3

                        echo "Determining corresponding target volumes"
                        SourceHostId=`$AWK -F "=" '$1=="SourceHostId " { print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
                        ThisHostId=`grep -i hostid $CONFIG_FILE|$AWK -F"=" '{ print $2 }'|sed 's/ //g'`
                        CXServerIP=`grep -i "^Hostname" $CONFIG_FILE|$AWK -F"=" '{ print $2 }'|sed 's/ //g'`
			CxPortNumber=`grep -i "^Port" $CONFIG_FILE|$AWK -F"=" '{ print $2 }'|sed 's/ //g'`

                        for SourcePartition in `echo $SourcePartitionsList`
                        do
                                echo "<discovery>" >$DISCOVERYXML
                                echo "    <operation>3</operation>" >>$DISCOVERYXML
                                echo "    <src_hostid>$SourceHostId</src_hostid>" >>$DISCOVERYXML
                                echo "    <trg_hostid>$ThisHostId</trg_hostid>" >>$DISCOVERYXML
                                echo "    <src_pri_partition>$SourcePartition</src_pri_partition>" >>$DISCOVERYXML
                                echo "</discovery>" >>$DISCOVERYXML
                                $INMAGE_DIR/bin/cxcli "http://$CXServerIP:${CxPortNumber}/cli/discovery.php" $DISCOVERYXML >$TEMP_FILE1
                                partition=`grep "target_devicename" $TEMP_FILE1|$AWK -F">" '{ print $2 }'|$AWK -F"<" '{print $1}'`
                                mount_point=`grep -w $SourcePartition $TEMP_FILE3 | $AWK '{ print $2 }'`
				if [ $AIX -eq 1 ]
                        	then
					echo $partition |tee -a $TEMP_FILE
				else
					echo $mount_point $partition |tee -a $TEMP_FILE
				fi
                        done
	}
OraclePlannedFailoverForAix()
{

		ORACLE_SID=$COMPONENT
                ORACLE_HOME=$HOME
                export ORACLE_HOME ORACLE_SID
	
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
                SOURCEHOST=`$AWK -F "=" '$1=="HOSTNAME "{ print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
                if [ "$HOSTNAME" =  "$SOURCEHOST" ]
                then

                        echo "Source and Target machines are same.Exiting...." |tee $ERRORFILE
                        echo "Failover failed..."|tee -a $ERRORFILE
                        $ALERT -f $ERRORFILE
                        rm -f $ERRORFILE
                        exit 255
                fi
                        rm -f $TEMP_FILE
                        GetTargetPartitionInfo

                        #####Issuing cdpcli for Rolling back of volumes#################
                        echo "Checking the Consistency Tag Status on target....."
                        cat $TEMP_FILE|while read line
                        do
                                partition=`echo $line`
				tagFound=0
                                timeOutLoop=1
				while true
				do
                                        $INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|grep -w $tag >>/dev/null
                                        if [ $? -ne 0 ]
                                        then
                                                if [ $timeOutLoop  -lt 20 ]
                                                then
                                                        echo "Waiting for $tag to Arrive on target.....Sleeping for 30 seconds..."
                                                        sleep 30
                                                        timeOutLoop=`expr $timeOutLoop + 1`
                                                else
							break
						fi
                                        else
						tagFound=1
						break
                                        fi
				done
				if [ $tagFound -ne 1 ] 
				then
                	                echo "Tag did not reach target.Waited for 10 mins..exiting....."
                        	        exit 255

                        	fi
                        done
			####StopVxAgent is a function######
                        #StopVxAgent

                        cat $TEMP_FILE|while read line
                        do
                                partition=`echo $line`
                                $INMAGE_DIR/bin/cdpcli --rollback --dest=$partition  --force=yes --event=$tag --picklatest --deleteretentionlog=yes
				if [ $? -ne 0 ]
				then
					echo " No Common consistency point found ....existing"	
					exit 255
				fi
                        done
			cat $APPLOGDIR/${APP}_${COMPONENT}.txt|grep -w "PVDevices" | awk -F"=" '{print $2}'|sed 's/ //g'>ForCreatingVg
			

			while read line
			do
				tgtpv1=${line}","${tgtpv1}
			done <$TEMP_FILE
			#echo $tgtpv1
			num=`echo $tgtpv1|wc -c`
			#echo $num
			num=`expr $num - 2`
			#echo $num
			echo $tgtpv1|cut -c 1-${num} >tgtpv2
			echo "printing the target pv's"
			cat tgtpv2

			paste ForCreatingVg tgtpv2 >>ForMount

			rm -rf ForCreatingVg
			cat ForMount|while read line
			do
			   echo $line|sed 's/ /:/g'>>ForCreatingVg
			done
            cat ForCreatingVg|sort|uniq>SortedForVgCreation
            rm -rf ForMount forrollback tgtpv2
            cat SortedForVgCreation|while read line
            do
                lv=`echo $line|awk -F":" '{print $1}'`
                vg=`echo $line|awk -F":" '{print $2}'`
                logdev=`echo $line|awk -F":" '{print $5}'`
                pv=`echo $line|awk -F":" '{print $6}'`
                mp=`echo $line|awk -F":" '{print $4}'`

                echo lv $lv  vg $vg logdev $logdev pv  $pv mp  $mp
                if [ ! -z "$vg" ]
                then
                    echo "Calling the recreate vg function.."
                    lsvg -l $vg >/dev/null 2>&1
                    if [ $? -eq 0 ]
                    then
                        echo "Found the Volume Group $vg already present on host"
                    else
                        $SCRIPTS_DIR/discover_vg_aix.sh $vg $pv
                        if [ $? -ne 0 ]
                        then
                             echo "Failed to recreate vg.."
                             exit 1
                        fi
                    fi
                fi

                mkdir -p $mp>>/dev/null
                if [ $? -ne 0 ]
                then
                     echo "Mountpoint is already present.. "
                fi
                mount -V jfs2 -o log=$logdev $lv $mp>>/dev/null
                if [ $? -ne 0 ]
                then
                    echo "Failed to mount the filesystem.."
                    exit 1
                fi
		Group=`id $ORACLE_USER|$AWK '{ print $2 }'|$AWK -F'(' '{ print $2 }'|sed 's/)//g'`
                echo "user $ORACLE_USER group $Group mount point $mp"
		chown -R $ORACLE_USER:$Group $mp
            done

 }

###############Defining the functions########################
####Function:oracle planned failover
####Action:This function Rollbacks the volumes to particular constency point based the information available in $INMAGEDIR/failover_data/oracle/<oracle_{$ORACLE_SID}.txt file.It stops vx services before cdpcli and start vx services once cdpcli completes.
####O/P:vxagent stop,cdpcli ouput,vxagent start
	
        OraclePlannedFailover()
	{


		ORACLE_SID=$COMPONENT
		ORACLE_HOME=$HOME
		export ORACLE_HOME ORACLE_SID

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
		SOURCEHOST=`$AWK -F "=" '$1=="HOSTNAME "{ print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
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
			temp=`$AWK -F"= " '$1=="TargetPartition" {print $2 }' $APPLOGDIR/${APP}_${COMPONENT}_unplanned.txt`
			rm -f $TEMP_FILE
			for partition in `echo $temp`
			do

				echo $partition|tee -a $TEMP_FILE
			done

			echo "Checking the Consistency Tag Status on target....."
			cat $TEMP_FILE|while read line
			do
				partition=`echo $line|$AWK '{ print $1}'`
			        timeOutLoop=1
                                tagFound=0
				while true
				do
					cat $partition
					$INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|grep -w $tag >>/dev/null
					if [ $? -ne 0 ]
					then
						
                                                if [ $timeOutLoop  -lt 20 ]
                                                then
							echo "Waiting for $tag to Arrive on target.....Sleeping for 30 seconds..."
							sleep 30
						        timeOutLoop=`expr $timeOutLoop + 1`
						else
							break
                                                fi
					else
						tagFound=1
						break
					fi
				done
				
				if [ $tagFound -ne 1 ]
                        	then
                                	echo "Tag did not reach target.Waited for 10 mins..exiting.."
                                	exit 1
                        	fi

			done

			#StopVxAgent  ####To Stop Vxagent on targetbefore cdpcli command
			cat $TEMP_FILE|while read line
			do
				partition=`echo $line|$AWK '{print $1 }'`
			
				$INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --force=yes --event=$tag --picklatest --deleteretentionlog=yes
				

			done
			####StartVxAgent is a function#####
			#StartVxAgent


		else

			####taking filesystem names in seperate file###########
			rm -f $TEMP_FILE
			GetTargetPartitionInfo
	
			#####Issuing cdpcli for Rolling back of volumes#################
			echo "Checking the Consistency Tag Status on target....."
			cat $TEMP_FILE|while read line
			do
				mountpoint=`echo $line|$AWK '{ print $1 }'`
				partition=`echo $line|$AWK '{print $2 }'`
				timeOutLoop=1
				tagFound=0
				while true
				do
					$INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|grep -w $tag >>/dev/null
					if [ $? -ne 0 ]
					then
						if [ $timeOutLoop  -lt 10 ]
						then	
							echo "Waiting for $tag to Arrive on target.....Sleeping for 30 seconds..."
							sleep 30
							timeOutLoop=`expr $timeOutLoop + 1`
						else
							break
						fi
					else
						tagFound=1
						break
	
					fi
				done
			
			done		
			if [ $tagFound -ne 1 ]
			then
				echo "Tag did not reach target.Waited for 5 mins..exiting.."
				exit 1

			fi
			
			####StopVxAgent is a function######
			#StopVxAgent
			cat $TEMP_FILE|while read line
			do
				partition=`echo $line|$AWK '{print $2 }'`
				mountpoint=`echo $line|$AWK '{ print $1 }'`
			
				$INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --mountpoint=$mountpoint --force=yes --event=$tag --picklatest --deleteretentionlog=yes
				

			done
			
			####StartVxAgent is a function#####
			#StartVxAgent
		fi
		}


OracleUnplannedFailoverForAix()
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
                SOURCEHOST=`$AWK -F "=" '$1=="HOSTNAME "{ print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
                if [ "$HOSTNAME" =  "$SOURCEHOST" ]
                then

                        echo "Source and Target machines are same.Exiting...." |tee $ERRORFILE
                        echo "Failover failed..."|tee -a $ERRORFILE
                        $ALERT -f $ERRORFILE
                        rm -f $ERRORFILE
                        exit 255
                fi

                        temp=`$AWK -F "=" '$1=="FS " { print $2  }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
                        ####taking filesystem names in seperate file###########
                        rm -f $TEMP_FILE $TEMP_FILE1 $TEMP_FILE2 $TEMP_FILE3
                        #SourceMountPointList=`$AWK -F "=" '$1=="FS " { print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
                        TargetPartitionList=`$AWK -F "=" '$1=="TargetPartition" { print $2 }' $APPLOGDIR/${APP}_${COMPONENT}_unplanned.txt`
                        #RetentionLogList=`$AWK -F "=" '$1=="RetentionLog" { print $2 }' $APPLOGDIR/${APP}_${COMPONENT}_unplanned.txt`
                        #for SourceMountPoint in `echo $SourceMountPointList`
                        #do
                        #        echo $SourceMountPoint >>$TEMP_FILE1
                        #done
                        for Partition in `echo $TargetPartitionList`
                        do
                                echo $Partition >>$TEMP_FILE2
                        done
			echo "printing the target partitions .........."
			cat $TEMP_FILE2

                        #for Retention in `echo $RetentionLogList`
                        #do
                        #        echo $Retention >>$TEMP_FILE3
                        #done
                        #paste $TEMP_FILE1 $TEMP_FILE2 $TEMP_FILE3 > $TEMP_FILE
			paste $TEMP_FILE2 > $TEMP_FILE
                        echo "printing the target partitions .........."
                        cat $TEMP_FILE
			#####Issuing cdpcli for Rolling back of volumes#################
                        echo "Checking the Consistency Tag Status on target....."
                        cat $TEMP_FILE|while read line
                        do
                                #mountpoint=`echo $line|$AWK '{ print $1 }'`
                                partition=`echo $line|$AWK '{print $1 }'`
                                timeOutLoop=1
                                tagFound=0
                                while true
                                do
                                        if [ "$tag" = "LATEST" ]
                                        then
                                                $INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|$AWK '{print $1}' | grep -w "1" |uniq >>/dev/null
                                        else
                                                $INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|grep -w $tag >>/dev/null
                                        fi
                                        if [ $? -ne 0 ]
                                        then
                                                if [ $timeOutLoop  -lt 20 ]
                                                then
                                                        echo "Waiting for $tag to Arrive on target.....Sleeping for 30 seconds..."
                                                        sleep 30
                                                        timeOutLoop=`expr $timeOutLoop + 1`
                                                else
                                                        break
                                                fi
                                        else

                                                tagFound=1
                                                break

                                        fi

                                done

				if [ $tagFound -ne 1 ]
				then
				echo "Tag did not found........Waited for 10 Mins..Exiting....."
				exit 1
				fi
				
                        done


                                ####StopVxAgent is a function######
                                #StopVxAgent
				#echo "printing temp file output below"
				#cat $TEMP_FILE
                                cat $TEMP_FILE|while read line
                                do
                                        partition=`echo $line|$AWK '{print $1 }'`
                                        #mountpoint=`echo $line|$AWK '{ print $1 }'`
                                        if [ "$tag" = "LATEST" ]
                                        then

                                                $INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --force=yes --recentappconsistentpoint --app=FS  --deleteretentionlog=yes
                                        else

                                                $INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --force=yes --event=$tag --picklatest --deleteretentionlog=yes

                                        fi
                                done

                                ####StartVxAgent is a function#####
                                #StartVxAgent

				#############Checking for VG exist on target server if not creating VG and mounting the LV###################
			
				while read line
        	        	do
                	        	tgtpv1=${line}","${tgtpv1}
                        	done <$TEMP_FILE
                        	#echo $tgtpv1
                        	num=`echo $tgtpv1|wc -c`
                        	#echo $num
                        	num=`expr $num - 2`
                        	#echo $num
                        	echo $tgtpv1|cut -c 1-${num} >tgtpv2
                        	#echo "printing the target pv's"
                        	#cat tgtpv2

				#cat $TEMP_FILE|while read line
				#do
				#	echo $line|sed 's/ /:/g'>getpartitioninfo
				#	cat getpartitioninfo|$AWK -F":" '{print $2}' >>getinfo2
				#	cat getinfo2	
				#done
				
				#echo "Printing the get info output "
				#cat getinfo2	
				#rm -rf getpartitioninfo
				
				cat $APPLOGDIR/${APP}_${COMPONENT}.txt|grep -w "PVDevices" | awk -F"=" '{print $2}'|sed 's/ //g'>ForCreatingVg
                        	paste ForCreatingVg tgtpv2 >>ForMount
                        	#paste ForCreatingVg getinfo2 >>ForMount
				rm -rf ForCreatingVg tgtpv2 
				#rm -rf ForCreatingVg getinfo2
                        	cat ForMount|while read line
                        	do
                        	 	echo $line|sed 's/ /:/g'>>ForCreatingVg
                        	done
            			cat ForCreatingVg|sort|uniq>SortedForVgCreation
            			rm -rf ForMount forrollback
				cat SortedForVgCreation|while read line
            			do
                			lv=`echo $line|awk -F":" '{print $1}'`
                			vg=`echo $line|awk -F":" '{print $2}'`
                			logdev=`echo $line|awk -F":" '{print $5}'`
                			pv=`echo $line|awk -F":" '{print $6}'`
               				mp=`echo $line|awk -F":" '{print $4}'`

                			echo lv $lv  vg $vg logdev $logdev pv  $pv mp  $mp
                			if [ ! -z "$vg" ]
                			then
                    				echo "Calling the recreate vg function.."
                    				lsvg -l $vg >/dev/null 2>&1
                    				if [ $? -eq 0 ]
                    				then
                        				echo "Found the Volume Group $vg already present on host"
                    				else
                        				$SCRIPTS_DIR/discover_vg_aix.sh $vg $pv
                        				if [ $? -ne 0 ]
                        				then
                             					echo "Failed to recreate vg.."
                             					exit 1
                        				fi
                    				fi
                			fi

                			mkdir -p $mp>>/dev/null
                			if [ $? -ne 0 ]
                			then
                     				echo "Mountpoint is already present.. "
                			fi
                		
					mount -V jfs2 -o log=$logdev $lv $mp>>/dev/null
                			if [ $? -ne 0 ]
                			then
                    				echo "Failed to mount the filesystem.."
                    				exit 1
                			fi

                			Group=`id $ORACLE_USER|$AWK '{ print $2 }'|$AWK -F'(' '{ print $2 }'|sed 's/)//g'`
                			echo "user $ORACLE_USER group $Group mount point $mp"
                			chown -R $ORACLE_USER:$Group $mp
            			done


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
		SOURCEHOST=`$AWK -F "=" '$1=="HOSTNAME "{ print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
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
		if [ $ASM_STATUS -eq 0 ]   ###Enter into if ASM is enabled
		then
			temp=`$AWK -F "=" '$1=="FS " { print $2  }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
			rm -f $TEMP_FILE
			for partition in `echo $temp`
			do

				echo $partition|tee -a $TEMP_FILE
			done

			echo "Checking the Consistency Tag Status on target....."
			cat $TEMP_FILE|while read line
			do
				partition=`echo $line|$AWK '{ print $1}'`
				timeOutLoop=1
                                tagFound=0
			
				while true
				do
					if [ "$tag" = "LATEST" ]
					then
						$INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|$AWK '{print $1}' | grep -o "1" |uniq >>/dev/null
					else
						$INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|grep -w $tag >>/dev/null

					fi
					if [ $? -ne 0 ]
                                        then
                                                if [ $timeOutLoop  -lt 20 ]
                                                then
                                                        echo "Waiting for $tag to Arrive on target.....Sleeping for 30 seconds..."
                                                        sleep 30
                                                        timeOutLoop=`expr $timeOutLoop + 1`
						else
							break
                                                fi
                                        else
                                                tagFound=1
                                                break

                                        fi
				done
                        	if [ $tagFound -ne 1 ]
                        	then
                                	echo "Tag did not reach target.Waited for 10 mins..exiting.."
                                	exit 1

                        	fi
			done
			#StopVxAgent  ####To Stop Vxagent on targetbefore cdpcli command
			cat $TEMP_FILE|while read line
			do
				partition=`echo $line|$AWK '{print $1 }'`
				if [ "$tag" = "LATEST" ]
				then
			
					$INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --force=yes --eventnum=1  --deleteretentionlog=yes
				else
					$INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --force=yes --event=$tag  --picklatest --deleteretentionlog=yes
			
				fi
				

			done
			####StartVxAgent is a function#####
			#StartVxAgent


		else

			temp=`$AWK -F "=" '$1=="FS " { print $2  }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
                       	####taking filesystem names in seperate file###########
			rm -f $TEMP_FILE $TEMP_FILE1 $TEMP_FILE2 $TEMP_FILE3
                       	SourceMountPointList=`$AWK -F "=" '$1=="FS " { print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
			TargetPartitionList=`$AWK -F "=" '$1=="TargetPartition" { print $2 }' $APPLOGDIR/${APP}_${COMPONENT}_unplanned.txt`
			RetentionLogList=`$AWK -F "=" '$1=="RetentionLog" { print $2 }' $APPLOGDIR/${APP}_${COMPONENT}_unplanned.txt`
	                for SourceMountPoint in `echo $SourceMountPointList`
       		        do
                               	echo $SourceMountPoint >>$TEMP_FILE1
                       	done
	                for Partition in `echo $TargetPartitionList`
       		        do
                               	echo $Partition >>$TEMP_FILE2
                       	done
			for Retention in `echo $RetentionLogList`
			do
				echo $Retention >>$TEMP_FILE3
			done
			paste $TEMP_FILE1 $TEMP_FILE2 $TEMP_FILE3 > $TEMP_FILE
			#####Issuing cdpcli for Rolling back of volumes#################
			echo "Checking the Consistency Tag Status on target....."
			cat $TEMP_FILE|while read line
			do
				mountpoint=`echo $line|$AWK '{ print $1 }'`
				partition=`echo $line|$AWK '{print $2 }'`
				timeOutLoop=1
                                tagFound=0
				while true
				do
					if [ "$tag" = "LATEST" ]
					then
						$INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|$AWK '{print $1}' | grep -o "1" |uniq >>/dev/null
					else
						$INMAGE_DIR/bin/cdpcli --listevents --vol=$partition|grep -w $tag >>/dev/null


					fi
                                        if [ $? -ne 0 ]
                                        then
                                                if [ $timeOutLoop  -lt 10 ]
                                                then
                                                        echo "Waiting for $tag to Arrive on target.....Sleeping for 30 seconds..."
                                                        sleep 30
                                                        timeOutLoop=`expr $timeOutLoop + 1`
						else
							break
                                                fi
                                        else
                                                tagFound=1
                                                break

                                        fi
	
				done

			done		
                      if [ $tagFound -ne 1 ]
                        then
                                echo "Tag did not reach target.Waited for 5 mins..exiting.."
                                exit 1

                        fi

				####StopVxAgent is a function######
				#StopVxAgent
				cat $TEMP_FILE|while read line
				do
					partition=`echo $line|$AWK '{print $2 }'`
					mountpoint=`echo $line|$AWK '{ print $1 }'`
					if [ "$tag" = "LATEST" ]
					then
				
						$INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --mountpoint=$mountpoint --force=yes --eventnum=1  --deleteretentionlog=yes
					else

						$INMAGE_DIR/bin/cdpcli --rollback --dest=$partition --mountpoint=$mountpoint --force=yes --event=$tag --picklatest --deleteretentionlog=yes

					fi
				done
			
				####StartVxAgent is a function#####
				#StartVxAgent

		fi
		}

##############Defining the functions########################
####Function:OracleRecover
####Action:This function Recover the mounted database.
####O/P:"recover"command output
	OracleRecover()
	{
		echo "Starting Recovery of database...."
		chmod 777 $TEMP_FILE1
		su - $ORACLE_USER -c "ORACLE_HOME=$ORACLE_HOME;ORACLE_SID=$ORACLE_SID;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF >$TEMP_FILE1
                			startup mount;
					recover database;
					alter database open;
					exit;
_EOF"
		chmod 777 $TEMP_FILE $TEMP_FILE1
		su - $ORACLE_USER -c "ORACLE_HOME=$ORACLE_HOME;ORACLE_SID=$ORACLE_SID;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF >$TEMP_FILE
					select status from v\\\$instance;
					exit;
_EOF"
		grep "OPEN" $TEMP_FILE >>/dev/null
		if [ $? -eq 0 ]
		then

			cat $TEMP_FILE1
			echo "Database started and opened"
		        mv $LOGDIR/oracle/oracle_${ORACLE_SID}_unplanned.txt $LOGDIR/oracle/oracle_${ORACLE_SID}_unplanned_used.txt
			exit 0;
		else
			cat $TEMP_FILE1
			su - $ORACLE_USER -c "ORACLE_HOME=$ORACLE_HOME;ORACLE_SID=$ORACLE_SID;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF  >$TEMP_FILE1
					select member from v\\\$logfile;
					exit;
_EOF"
			for file in `cat $TEMP_FILE1`	
			do
			output=`su - $ORACLE_USER -c "ORACLE_HOME=$ORACLE_HOME;ORACLE_SID=$ORACLE_SID;export ORACLE_HOME ORACLE_SID;export file=$file;echo $file|$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF 
					recover database;
					exit;
_EOF"`
			done
			su - $ORACLE_USER -c "ORACLE_HOME=$ORACLE_HOME;ORACLE_SID=$ORACLE_SID;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/ as sysdba' << _EOF >$TEMP_FILE
					alter database open;
					select status from v\\\$instance;
					exit;
_EOF"
			grep OPEN $TEMP_FILE >>/dev/null
			if [ $? -eq 0 ]
			then
				echo "Database started,Recovered with redo logs and database opened"
				mv $LOGDIR/oracle/oracle_${ORACLE_SID}_unplanned.txt $LOGDIR/oracle/oracle_${ORACLE_SID}_unplanned_used.txt
				exit 0
			else
				cat $TEMP_FILE
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
			su  - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<EOF >$TEMP_FILE
@$TEMP_FILE1
/
EOF"
			echo "" && echo "Copying configuration files..." && echo ""
			grep -v "SPFILE" $TEMP_FILE|grep -v "grep"|grep PFILE 
			x=$?
			if [ $x -eq 0 ]
			then
				su  - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<EOF1 >>/dev/null
				create spfile='/tmp/spfile.ora' from pfile
				/
				create pfile='/tmp/pfile.ora' from spfile='/tmp/spfile.ora'
				/
EOF1"

			fi

			if [ $x -eq 1 ]
			then
				su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S ' /as sysdba' <<EOF2 >>/dev/null
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


		Group=`id $ORACLE_USER|$AWK '{ print $2 }'|$AWK -F'(' '{ print $2 }'|sed 's/)//g'`
		TempVariable=`$AWK -F'=' '/^FS/ {print $2 }' $APPLOGDIR/${APP}_${COMPONENT}.txt`
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
			ADUMP=`grep "audit_file_dest" $PFILE|$AWK -F"'" '{ print $2 }'`
			BDUMP=`grep "background_dump_des" $PFILE|$AWK -F"'" '{ print $2 }'`
			CDUMP=`grep "core_dump_dest" $PFILE|$AWK -F"'" '{ print $2 }'`
			UDUMP=`grep "user_dump_dest" $PFILE|$AWK -F"'" '{ print $2 }'`
			RDEST=`grep "db_recovery_file_dest" $PFILE|$AWK -F"'" '{ print $2 }'`
			CTRLFILE=`grep "control_files" $PFILE|$AWK -F"'" '{ print $2 }'`
			if [ ! -z "$CTRLFILE" ]
			then
				CTRLDIR=`dirname $CTRLFILE>>/dev/null`
			fi
			ARCHDEST=`grep "log_archive_dest_1" $PFILE|$AWK -F"'" '{ print $2 }'|$AWK -F"=" '{ print $2 }'`
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
		su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S ' /as sysdba' <<EOF2 >$TEMP_FILE2
				@$TEMP_FILE1
				/
EOF2"
		grep -w "ARCHIVELOG" $TEMP_FILE2 >>/dev/null
		if [ $? -ne 0 ]
		then
			echo "Archive Log not enabled for this Database.."
			echo "Change database to Archivelog modee,then try again..."
			echo  && echo "exiting..."
			exit 123
		else

			echo "Database is in archive log mode."
			echo "Continuing.." && echo ""

		fi
		


	}
	AddEntryInOratabFile()
	{


		echo "Checking $ORATABFILE file..."
		if [ -f $ORATABFILE ]
		then
			grep -w "$ORACLE_SID" $ORATABFILE >>/dev/null
			if [ $? -ne 0 ]
			then
				echo "Adding entry in $ORATABFILE file"
				echo "$ORACLE_SID:$ORACLE_HOME:N" >>$ORATABFILE
				echo "Entry added $ORATABFILE file"
			else

				echo "$ORACLE_SID already present in $ORATABFILE file"

			fi

		else
			echo "$ORATABFILE file doesnt exist."
			echo "Please create $ORATABFILE ..."
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
    if [ ! -f /usr/local/.vx_version ]; then
        exit 1
    fi
     
        INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2 | sed "s/ //g"`
	CONFIG_DIR="$INMAGE_DIR/etc"
	CONFIG_FILE="$CONFIG_DIR/drscout.conf"
        SCRIPTS_DIR=$INMAGE_DIR/scripts
        COMPONENT=$3
	LOGDIR=$INMAGE_DIR/failover_data
        TEMP_FILE="$LOGDIR/.files_$COMPONENT"
	TEMP_FILE1="$LOGDIR/.files1_$COMPONENT"
	TEMP_FILE2="$LOGDIR/.files2_$COMPONENT"
	TEMP_FILE3="$LOGDIR/.files3_$COMPONENT"
	TEMP_FILE4="$LOGDIR/.files3_$COMPONENT"
	DF_OUTPUT="$LOGDIR/.df_koutput.txt_$COMPONENT"
        MOUNT_POINTS="$LOGDIR/.mountpoint_$COMPONENT"
	PVDEVICES="$LOGDIR/pvdevices_$COMPONENT.$$"
        ERRORFILE=$LOGDIR/errorfile_$COMPONENT.txt
        ALERT=$INMAGE_DIR/bin/alert
	DISCOVERYXML="$LOGDIR/sample_$COMPONENT.xml"
        programname=$0
	#//Removing Old files
	rm -f $TEMP_FILE $MOUNT_POINTS $ERRORFILE $TEMP_FILE1 $TEMP_FILE2 $DF_OUTPUT $TEMP_FILE3

        > $TEMP_FILE
        chmod 666 $TEMP_FILE

        > $TEMP_FILE1
        chmod 666 $TEMP_FILE1

        > $TEMP_FILE2
        chmod 666 $TEMP_FILE2

        > $TEMP_FILE3
        chmod 666 $TEMP_FILE3

        > $TEMP_FILE4
        chmod 666 $TEMP_FILE4

        > $DF_OUTPUT
        chmod 666 $DF_OUTPUT

        > $MOUNT_POINTS
        chmod 666 $MOUNT_POINTS

        > $PVDEVICES
        chmod 666 $PVDEVICES

        > $ERRORFILE
        chmod 666 $ERRORFILE

	#########Capturing Arguements those are sent by Application.sh##############################
	APP=$1
	HOME=$2
	COMPONENT=$3
	tag=$4
	action=$5
	USER=$6
	ORACLE_HOME=$HOME
	ORACLE_SID=$COMPONENT
	ORACLE_USER=$USER
	export ORACLE_HOME ORACLE_SID ORACLE_USER

	######Calling Corresponding Function based on the arguements#####################
	OS=`uname -s`
	AIX=0
       	case $OS in
       	     HP-UX ) df_command="bdf"
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
		     ORATABFILE="/etc/oratab"
		     AWK="/usr/bin/awk"
			;;
       	     SunOS ) df_command="df"
	    	     df -k |grep -v "Mounted on" > $DF_OUTPUT
               	     FSTABFILE="/etc/vfstab"
		     ORATABFILE="/var/opt/oracle/oratab"
		     AWK="/usr/xpg4/bin/awk"
			;;
             AIX ) df_command="df"
		     i=1
                     for p in `df -k|grep -v "Mounted on"`
                     do
                     	 echo -n "$p " >> $DF_OUTPUT
                       	 i=`expr $i + 1`
                         x=`expr $i % 8` >>/dev/null
                         if [ $x  -eq 0 ]
                         then
                              echo "" >>  $DF_OUTPUT
                              i=1
                         fi
                     done
		   AWK="/usr/bin/awk"
		   ORATABFILE="/etc/oratab"	
                   FSTABFILE="/etc/vfstab"
		   AIX=1
		   ;;
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
				OracleShutdown
				ShowTargetCommand;;
			
		"plannedfailover" )
				RunPreScript
				OracleShutdown
				if [ $AIX -eq 1 ]
				then
					OraclePlannedFailoverForAix
				else
					OraclePlannedFailover
				fi
				CopyPfileOnTarget
				AddEntryInOratabFile
				OracleRecover
				RunPostScript;;
		"unplannedfailover" )
				RunPreScript
				OracleShutdown
				if [ $AIX -eq 1 ]
                                then
					OracleUnplannedFailoverForAix
				else
					OracleUnplannedFailover
				fi
				CopyPfileOnTarget
				AddEntryInOratabFile
				OracleRecover
				RunPostScript;;
		"discover" )
				OracleDiscover;;
				

	esac
	rm -f $TEMP_FILE $MOUNT_POINTS $ERRORFILE $TEMP_FILE1 $TEMP_FILE2 $DF_OUTPUT $TEMP_FILE3 $TEMP_FILE4
