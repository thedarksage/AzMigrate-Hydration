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
			ps -ef|grep -v grep |grep asmb_${ORACLE_SID} >>/dev/null ####Checking ASM instance for particular DB
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
						$df_command . |tail -1 |$AWK '{ print $NF}' >>$MOUNT_POINTS
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
                        if [ $ASM_STATUS -eq 0 ]
                        then
                                echo "ASM = TRUE" >> $APPLOGDIR/${APP}_${COMPONENT}.txt
                        else

                                echo "ASM = FALSE" >> $APPLOGDIR/${APP}_${COMPONENT}.txt
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
			VACPCOMMANDOPTION=`echo $volumes |$AWK 'BEGIN { print "-v" } { for(i=1;i<=NF;i++) printf("%s,",$i) }'`
                        ########Issuing vacp tag for all file system with one vacp command
			LD_LIBRARY_PATH=$INMAGE_DIR/tools/lib:$LD_LIBRARY_PATH
			export LD_LIBRARY_PATH
                            result=`$INMAGE_DIR/tools/bin/vacp $VACPCOMMANDOPTION -t $tag -remote -serverip $serverip -serverport $serverport -serverdevice $serverdevice  2>&1`
                        echo $result
			##Writing tag information to file
                        echo "TAG = $tag" >> $APPLOGDIR/${APP}_${COMPONENT}.txt

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
	CheckLicense()
	{
                rm -rf $TEMP_FILE1        
		rm -rf $ISCXLICENSEDXML

		echo "<licenseType>" >$ISCXLICENSEDXML
		echo "        <applicationEnabled>Y</applicationEnabled>">>$ISCXLICENSEDXML
		echo "</licenseType>">>$ISCXLICENSEDXML
		
		CXServerIP=`grep -w "SVServer" $CONFIG_FILE|$AWK -F"=" '{ print $2 }'|sed 's/ //g'`
                CxPortNumber=`grep -w "SVServerPort" $CONFIG_FILE|$AWK -F"=" '{ print $2 }'|sed 's/ //g'`
	
		LD_LIBRARY_PATH=$INMAGE_DIR/tools/lib:$LD_LIBRARY_PATH	
		export LD_LIBRARY_PATH
		$INMAGE_DIR/tools/bin/cxcli "http://$CXServerIP:${CxPortNumber}/get_app_license.php" $ISCXLICENSEDXML>$TEMP_FILE1 	
		grep -w "Non Application" $TEMP_FILE1 >>/dev/null
		if [ $? -eq 0 ]
		then
		    echo " "
		    echo "ERROR : The required license is not available for providing Application Consistency for Oracle!"
		    exit 1
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


	##########################Start of the program######################################

     
        INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.fx_version | cut -d'=' -f2`
	#CONFIG_DIR="$INMAGE_DIR/etc"
	CONFIG_FILE="$INMAGE_DIR/config.ini"
        SCRIPTS_DIR=$INMAGE_DIR/tools/scripts
        COMPONENT=$3
	LOGDIR=$INMAGE_DIR/tools/failover_data
        TEMP_FILE="$LOGDIR/.files_$COMPONENT"
	TEMP_FILE1="$LOGDIR/.files1_$COMPONENT"
	TEMP_FILE2="$LOGDIR/.files2_$COMPONENT"
	TEMP_FILE3="$LOGDIR/.files3_$COMPONENT"
	DF_OUTPUT="$LOGDIR/.df_koutput.txt_$COMPONENT"
        MOUNT_POINTS="$LOGDIR/.mountpoint_$COMPONENT"
        ERRORFILE=$LOGDIR/errorfile_$COMPONENT.txt
        ALERT=$INMAGE_DIR/bin/alert
	DISCOVERYXML="$LOGDIR/sample_$COMPONENT.xml"
	ISCXLICENSEDXML="$LOGDIR/get_license_$COMPONENT.xml"
        programname=$0
	#//Removing Old files
	rm -f $TEMP_FILE $MOUNT_POINTS $ERRORFILE $TEMP_FILE1 $TEMP_FILE2 $DF_OUTPUT $TEMP_FILE3
	#########Capturing Arguements those are sent by Application.sh##############################
	flag=0
	onlyvacp=0
	if [ $# -eq 6 ]
	then
	    flag=$1
	    serverport=$2
	    serverdevice=$3
	    serverip=$4
	    volumes=$5
	    tag=$6
	    action=onlyvacp
	fi
        if [ $# -eq 11 ]
        then
	    APP=$1
	    HOME=$2
	    COMPONENT=$3
	    tag=$4
	    action=$5
	    USER=$6
	    flag=$7
	    serverport=$8
	    serverdevice=$9
	    serverip=${10}
	    volumes=${11}

        ORACLE_HOME=$HOME
        ORACLE_SID=$COMPONENT
        ORACLE_USER=$USER
        export ORACLE_HOME ORACLE_SID ORACLE_USER

	fi


	######Calling Corresponding Function based on the arguements#####################
	OS=`uname -s`
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
		   ;;
       	esac

	case 	$action in
		"consistency" )
				OracleDBArchiveLogModeCheck
				if [ $flag -eq 1 ]
				then
				    CheckLicense
				fi
				OracleFreeze
				OracleVacpTagGenerate	
				CopyPfileOnSource
				OracleUnFreeze;;
			
		"discover" )
				OracleDiscover;;
		"onlyvacp" )
	    			OracleVacpTagGenerate
				;;

	esac
	rm -f $TEMP_FILE $MOUNT_POINTS $ERRORFILE $TEMP_FILE1 $TEMP_FILE2 $DF_OUTPUT $TEMP_FILE3
