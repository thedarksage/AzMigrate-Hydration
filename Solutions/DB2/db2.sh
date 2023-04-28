#!/bin/sh

    INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
    CONFIG_FILE=$INMAGE_DIR/etc/drscout.conf
    SCRIPTS_DIR=$INMAGE_DIR/scripts
    DISCOVERYXML=$INMAGE_DIR/failover_data/db2sample.xml
    HostName=`hostname`
    TEMP_FILE=/tmp/tempfile.$$
    PVDEVICES=/tmp/pvdevices.$$
    mntptlist=/tmp/mntptlist.$$
    NamesFromDb2=/tmp/names.$$
    ForDatabaseCatalog=$INMAGE_DIR/failover_data/db2catalog.conf
    usergiventag=0
    gotRootPv=0
    mkdir -p $INMAGE_DIR/failover_data/DB2
    DB2ConfFile=$INMAGE_DIR/failover_data/DB2
    i=0
    APP=DB2
    SourceHostId=`grep -i HostId $CONFIG_FILE|awk -F"=" '{ print $2 }'`

    programname=$0
    arg0=0

    usage()
    {
    echo "Usage is $programname -user <username> [-password <password>] -action <discovery/consistency/failover> -tag <tag name> -group <groupname> -Help <Help Message>  "
    echo "Where"
    echo "username = DB2 User Name with DBA Privileges"
    #echo "password = Password of DBA Privileges user"
    exit 1
    }

    if [ $# -eq 0 ]
    then usage
    exit 1
    fi

        args=$#
        count=1

    while [ $count -le $args ]
    do
        opt=$1

        opt=`echo $opt | sed "s/-//g"`

        case $opt in
        action )
                count=`expr $count + 1`
                shift 1
                Action=$1
                ;;
	dbname )
                count=`expr $count + 1`
                shift 1
		dbname=$1
		;;
        user )
                count=`expr $count + 1`
                shift 1
                user=$1
                ;;
	group )
                count=`expr $count + 1`
                shift 1
		group=$1
		;;
        unplanned )
                unplanned=1
                ;;
        tag )
                count=`expr $count + 1`
                shift 1
                tag=$1
		usergiventag=1
                ;;
        failover )
                 failover=1
                 ;;
        planned )
                planned=1
                 ;;
        password )
                count=`expr $count + 1`
                shift 1
                password=$1
                ;;
        Help )
		usage
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
        if [ $usergiventag -eq 0 ]
        then
                i=`expr $i + 1`
                tag1=`date +%Y%m%d%H%M%S`
                tag=`echo ${APP}_$tag1 `
        fi

Db2Discovery()
{
    su - $user -c "db2 <<END >$TEMP_FILE
        list active databases
	terminate
END" 

    grep -w " No data was returned by Database System Monitor" $TEMP_FILE>>/dev/null
    if [ $? -eq 0 ]
    then
	echo "No active databases are discovered on the host $HostName"
    else

        Totaldatabases=`grep -c "Database name" $TEMP_FILE`	
        DatabaseNames=`grep -w "Database name" $TEMP_FILE|awk -F"=" '{print $2}'|sed 's/ //g'`

	grep -w "Database name" $TEMP_FILE|awk -F"=" '{print $2}'|sed 's/ //g'>$NamesFromDb2
        grep -w "Database path" $TEMP_FILE|awk -F"=" '{print $2}'|sed 's/ //g'>MountPointsFromDb2

        for line in `cat MountPointsFromDb2`
        do
            cd `dirname $line`
            df .|tail -1|awk '{ print $NF}'>>$mntptlist
        done
        paste $NamesFromDb2 $mntptlist >$ForDatabaseCatalog
	
	cat $ForDatabaseCatalog|while read each
	do
		
	    	DBNAME=`echo $each|awk -F" " '{print $1}'| tr '[A-Z]' '[a-z]'`
		MOUNTPOINT=`echo $each|awk -F" " '{print $2}'`

		echo "APPLICATION = DB2">${DB2ConfFile}/DB2_$user.${DBNAME}.conf
		echo "MachineName = $HostName">>${DB2ConfFile}/DB2_$user.${DBNAME}.conf
		echo "SourceHostId = $SourceHostId">>${DB2ConfFile}/DB2_$user.${DBNAME}.conf
		echo "DatabaseName = $DBNAME" >> ${DB2ConfFile}/DB2_$user.${DBNAME}.conf

        	df -k|grep -v "Mounted"|awk -F" " '{print $7}'>mountpointlist

            	cat mountpointlist|grep -w $MOUNTPOINT>>/dev/null
	    	if [ $? -eq 0 ]
	        then
	        	$SCRIPTS_DIR/map_devices_aix.sh $MOUNTPOINT>$PVDEVICES
	    	else
	        	echo "WARNING: Mountpoint $line is not present"
	    	fi
		cat $PVDEVICES|while read line
		do
	    		echo $line|grep ":rootvg:">>/dev/null
	    		if [ $? -ne 0 ]
	    		then
				echo "PVDevices = $line">>${DB2ConfFile}/DB2_$user.${DBNAME}.conf
	    		else
				echo ""
	    		fi
		done
	
 	       	cat $PVDEVICES|awk -F":" '{print $3}'|sed 's/,/ /g'>listofpvs
        	cat $PVDEVICES|awk -F":" '{print $2}'|sed 's/,/ /g'>listofvgs
		cat $PVDEVICES|awk -F":" '{print $1}'|sed 's/,/ /g'>listoflvs
        	cat listofvgs|grep -w "rootvg">>/dev/null
        	if [ $? -eq 0 ]
        	then
	    		echo "ERROR: Found rootvg being used by the database.."
	    		echo "Cannot protect rootvg.. "
	    		rootpv=`lsvg -p rootvg|awk '{print $1}'|tail -1`
	    		gotRootPv=1
        	fi 
	
		echo "Database $DBNAME is using $MOUNTPOINT and"
        	echo "the following list of PVs should be protected.."
		if [ $gotRootPv -eq 1 ]
		then
	    		cat listofpvs|grep -v $rootpv
	    		pvlist=`cat listofpvs|grep -v $rootpv|sed 's/ /,/g'`
	    		echo "PVs = $pvlist">>${DB2ConfFile}/DB2_$user.${DBNAME}.conf
		else
	    		cat listofpvs
	    		pvlist=`cat listofpvs|sed 's/ /,/g'`
	    		echo "PVs = $pvlist">>${DB2ConfFile}/DB2_$user.${DBNAME}.conf
		fi
		echo "=================="
	
	done
    fi
    rm -f $TEMP_FILE
    rm -f $PVDEVICES
    rm -f $mntptlist
    rm -f $NamesFromDb2
}

Db2Freeze()
{
    if [ -z "$dbname" ];then
        echo "dbname is not specified in the arguments, please specify the dbname like -dbname<dbname>"
        exit 1
    fi
        
    echo "Keeping the database(s) in write suspend mode.."
    echo "================"
    for eachdatabase in `echo $dbname|sed 's/:/ /g'| tr '[A-Z]' '[a-z]'`
    do
        if [ ! -f "${DB2ConfFile}/DB2_$user.${eachdatabase}.conf" ];then
            echo "Config file for the database ${eachdatabase} -- ${DB2ConfFile}/DB2_$user.${eachdatabase}.conf is missing."
            echo "Please run discovery "
            exit 1
        else
            HostId=`grep "SourceHostId" ${DB2ConfFile}/DB2_$user.${eachdatabase}.conf |awk -F= '{print $2}'|sed 's/ //g'`
            if [ "$HostId" != "$SourceHostId" ];then
                echo "${DB2ConfFile}/DB2_$user.${eachdatabase}.conf is not of current host : $SourceHostId"
                echo "It is of Host : $HostId"
                echo "Please run discovery "
                exit 1
            fi
        fi

        su - $user -c "db2 <<END >$TEMP_FILE
           connect to $eachdatabase
           set write suspend for database
	   commit
	   disconnect $eachdatabase
END"
	grep -w "SQLSTATE=42705" $TEMP_FILE>>/dev/null
	if [ $? -eq 0 ]
	then
	    echo "WARNING: The database $eachdatabase is not active or could not be connected"
	    echo "Cannot freeze the database.."
	    exit 1
	fi
        grep -w " The SET WRITE command completed successfully." $TEMP_FILE>>/dev/null
        if [ $? -ne 0 ]
        then
	    echo "Failed to Freeze the database $eachdatabase.."
        cat $TEMP_FILE
	    exit 1
        else
	    echo "write suspend succeeded for database $eachdatabase.."
	fi 
	
    done
    rm -f $TEMP_FILE
}

Db2VacpTagGenerate()
{
    echo "================"
    echo "Generating tag for the database volumes.."
    rm -rf listofpvs listoflvs
    for eachdatabase in `echo $dbname|sed 's/:/ /g'| tr '[A-Z]' '[a-z]'`
    do
    if [ ! -f "${DB2ConfFile}/DB2_$user.${eachdatabase}.conf" ];then
        echo "Config file for the database ${eachdatabase} -- ${DB2ConfFile}/DB2_$user.${eachdatabase}.conf is missing."
        Db2UnFreeze
        exit 1
    fi
	cat ${DB2ConfFile}/DB2_$user.${eachdatabase}.conf | grep -w "PVDevices" |awk -F"=" '{print $2}' |sed 's/ //g'|awk -F":" '{print $3}'>>listofpvs
	cat ${DB2ConfFile}/DB2_$user.${eachdatabase}.conf | grep -w "PVDevices" |awk -F"=" '{print $2}' |sed 's/ //g'|awk -F":" '{print $1}'>>listoflvs
    done
 
    	list=`cat listofpvs`
    	lvlist=`cat listoflvs`
    	VACPCOMMANDOPTION=`echo $list $lvlist|awk 'BEGIN { print "-v" } { for(i=1;i<=NF;i++) printf("%s,",$i) }'`
    	$INMAGE_DIR/bin/vacp $VACPCOMMANDOPTION -t $tag 2>&1
	if [ $? -ne 0 ]
	then
	    echo "ERROR:Failed to issue Tag.."
        Db2UnFreeze
        exit 1
	fi
    echo "================"
}

Db2UnFreeze()
{
    for eachdatabase in `echo $dbname|sed 's/:/ /g'| tr '[A-Z]' '[a-z]'`
    do
        su - $user -c "db2 <<END >$TEMP_FILE
           connect to $eachdatabase
           set write resume for database
           commit
           disconnect $eachdatabase
END"

        grep -w " The SET WRITE command completed successfully." $TEMP_FILE>>/dev/null
        if [ $? -ne 0 ]
        then
            echo "Failed to UnFreeze the database $eachdatabase.."
            exit 1
        else
            echo "write resume succeeded for database $eachdatabase.."
        fi

    done
    echo "================"
    echo "Keeping the database(s) in write resume mode is done.."
    rm -f $TEMP_FILE
}

RollbackVolumes()
{
    if [ -z "$dbname" ];then
       echo "dbname is not specified in the arguments, please specify the dbname like -dbname<dbname>"
       exit 1
    fi

    rm -rf targetpv forrollback srctgtvols
    for eachdatabase in `echo $dbname|sed 's/:/ /g'| tr '[A-Z]' '[a-z]'`
    do
        echo "Determining the corresponding target partitions for $eachdatabase.."
        SourceHostId=`awk -F "=" '$1=="SourceHostId " { print $2 }' ${DB2ConfFile}/DB2_$user.${eachdatabase}.conf|sed 's/ //g'`
        ThisHostId=`grep -i hostid $CONFIG_FILE|awk -F"=" '{ print $2 }'|sed 's/ //g'`
        CXServerIP=`grep -i "^Hostname" $CONFIG_FILE|awk -F"=" '{ print $2 }'|sed 's/ //g'`
        CxPortNumber=`grep -i "^Port" $CONFIG_FILE|awk -F"=" '{ print $2 }'|sed 's/ //g'`
        PVList=`awk -F "=" '$1=="PVs " { print $2 }' ${DB2ConfFile}/DB2_$user.${eachdatabase}.conf`

        for SourcePartition in `echo $PVList|sed 's/,/ /g'`
        do
            echo "<discovery>" >$DISCOVERYXML
            echo "        <operation>3</operation>">>$DISCOVERYXML
            echo "        <src_hostid>$SourceHostId</src_hostid>">>$DISCOVERYXML
            echo "        <trg_hostid>$ThisHostId</trg_hostid>">>$DISCOVERYXML
            echo "        <src_pri_partition>$SourcePartition</src_pri_partition>">>$DISCOVERYXML
            echo "</discovery>">>$DISCOVERYXML

            $INMAGE_DIR/bin/cxcli "http://$CXServerIP:${CxPortNumber}/cli/discovery.php" $DISCOVERYXML>$TEMP_FILE
            targetpv=`grep "target_devicename" $TEMP_FILE|awk -F">" '{ print $2 }'|awk -F"<" '{print $1}'`
            tgtPvList=`echo "${tgtPvList} ${targetpv}"`
            echo  $targetpv>>forrollback
	    echo $SourcePartition $targetpv>>srctgtvols
        done
    done
    cat srctgtvols|sort|uniq>sortedfiles
    cat forrollback|sort|uniq>uniqvalues

    if [ "$tag" != "LATEST" ]; then
     cat uniqvalues|while read line
     do
        tagFound=0
        timeOutLoop=1
        while true
        do
           $INMAGE_DIR/bin/cdpcli --listevents --vol=$line | grep -w $tag >>/dev/null
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
       if [ $tagFound -ne 1 ]
       then
         echo "Tag did not reach target.Waited for 5 mins..exiting.."
         exit 255
       #Bug 26462 - DB2 failover got failed, when failover fx job triggered by using File system tag.   
       else
         fstype=`$INMAGE_DIR/bin/cdpcli --listevents --vol=$line | grep -w $tag|awk -F" " '{print $(NF-1)}'`         
       fi
     done
    fi

    cat uniqvalues|while read line
    do
       if [ "$tag" = "LATEST" ]; then
          #tgtPvList=`echo $tgtPvList | sed 's/^ //g' | sed 's/ $//g' | sed 's/ /;/g'`
          #$INMAGE_DIR/bin/cdpcli --rollback --rollbackpairs="$tgtPvList" --force=yes --recentappconsistentpoint --app=FS --deleteretentionlog=yes
          $INMAGE_DIR/bin/cdpcli --rollback --dest=$line --force=yes --recentappconsistentpoint --app=FS --deleteretentionlog=yes
          if [ $? -ne 0 ]
          then
            echo "Rolling back for: $line Failed."
            echo "Unable to roll back the volume...Can't proceed"
            echo "Exiting..."
            exit 1
          fi
       else
          #cat uniqvalues|while read line
          #do
          $INMAGE_DIR/bin/cdpcli --rollback --dest=$line --force=yes --event=$tag --app=$fstype --picklatest --deleteretentionlog=yes
          if [ $? -ne 0 ]
          then
             echo "Rolling back for: $line Failed."
             echo "Unable to roll back the volume...Can't proceed"
             echo "Exiting..."
             exit 1
          fi
       fi
    done
    
    rm -f $TEMP_FILE
}

MountVolumes()
{
    group=`id $user|awk -F" " '{ print $2}'|awk -F"(" '{print $2}'|awk -F")" '{print $1}'`
    echo "Mounting the Filesystems.."
    rm -rf listofpvs listofvgs forCreatingVg ForMount tgtpv
    for eachdatabase in `echo $dbname|sed 's/:/ /g'| tr '[A-Z]' '[a-z]'`
    do
        cat ${DB2ConfFile}/DB2_$user.${eachdatabase}.conf|grep -w "PVDevices"|awk -F"=" '{ print $2}'|sed 's/ //g'>forCreatingVg
  	    cat forCreatingVg|awk -F":" '{ print $3}'>srcpv
	    srcpvdevice=`cat srcpv`
	    echo "These are the $srcpvdevice srcpvdevice for the database $eachdatabase"
	    for srcpvdevice1 in `echo $srcpvdevice|sed 's/,/ /g'|tr '[A-Z]' '[a-z]'`
	    do	
	    	cat sortedfiles|grep -w $srcpvdevice1|awk -F" " '{ print $2}'>tgtpv3
            echo "listing out target device for the corresponding $srcpvdevice1"
            cat tgtpv3
            paste tgtpv3 >>tgtpv
	    	rm -rf tgtpv3
        done

	    echo "listing out target PVs for database $eachdatabase "
	    cat tgtpv
		while read line
		do 
			tgtpv1=${line}","${tgtpv1}
		done <tgtpv

		#echo $tgtpv1
		num=`echo $tgtpv1|wc -c`
		#echo $num
		num=`expr $num - 2`
		#echo $num
		echo $tgtpv1|cut -c 1-${num} >tgtpv2
        echo "printing the output of target pv's below for the database $eachdatabase"
        cat tgtpv2
        paste forCreatingVg tgtpv2 >>ForMount
        echo "Output of source details and corresponding target device "
	    cat ForMount
        rm -rf forCreatingVg tgtpv2 tgtpv
	    tgtpv1=""	
	    cat ForMount|while read line
        do
            echo $line|sed 's/ /:/g'>>forCreatingVg
        done
    done

    cat forCreatingVg|sort|uniq>SortedForVgCreation
    rm -rf ForMount forrollback
    cat SortedForVgCreation|while read line
    do
        vg=`echo $line|awk -F":" '{print $2}'`
        mp=`echo $line|awk -F":" '{print $4}'`
        pv=`echo $line|awk -F":" '{print $6}'`

        if [ ! -z $vg ]
        then
            lsvg -o | grep -w $vg > /dev/null
		    if [ $? -eq 0 ]
		    then
                echo "Volume Group $vg already present on host"
	  	    else		 
                echo "Calling the recreatevg function.."
                $SCRIPTS_DIR/discover_vg_aix.sh $vg $pv
                if [ $? -ne 0 ]
	            then
                    echo "Failed to recreatevg.."
		            exit 1
                fi
            fi
        else
            echo "Vg name not found : $line"
            exit 1
		fi

        df -k | awk '{print $NF}' | grep -w $mp > /dev/null
        if [ $? -eq 0 ]
        then
             echo "Mountpoint is already present.. "
        else
            tgtlvName=`lsvg -l $vg | grep "$mp$" | awk  '{print $1}'`
            if [ -z "$tgtlvName" ]; then
                echo "lv not found for mount point $mp on $vg"
                exit 1
            fi

            tgtmp=`lslv -l  $tgtlvName | grep $tgtlvName | awk -F":" '{ print $2 }'`
            if [ -z "$tgtmp" ]; then
                echo "mount point not found for lv $tgtlvName"
                exit 1
            fi

            if [ "$tgtmp" != "$mp" ]; then
                echo "chfs -m $mp $tgtmp"
                chfs -m $mp $tgtmp
                if [ $? -ne 0 ]; then
                    exit 1
                fi
            fi

            mkdir -p $mp
            mount $mp
            df -k | awk '{print $NF}' | grep -w $mp > /dev/null
            if [ $? -ne 0 ];then
                echo "Mounting of lv $tgtlvName on $mp failed.."
                exit 1
            fi
        fi

        chown -R $user:$group $mp
    done	
}

ConnectToDatabase()
{
    echo "Connecting to the database.."
    
    #Bug 24607 - SQL1005N  The database alias "dbtest44" already exists in either the local
    #database directory or system database directory. When Database is already cataloged then it should be considered as success.
    TMP_DB_OUTPUT=/tmp/dbcheck
    su - $user -c "db2 list db directory" > $TMP_DB_OUTPUT

    DB_NAMES=`grep "Database name" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'`
    DB_LOCS=`grep "Local database directory" $TMP_DB_OUTPUT | awk -F"=" '{print $2}'`
    NUM_DB=`grep "Database name" $TMP_DB_OUTPUT | wc -l `

    for eachdatabase in `echo $dbname|sed 's/:/ /g'| tr '[A-Z]' '[a-z]'`
    do
	MountPoint=`cat ${DB2ConfFile}/DB2_$user.${eachdatabase}.conf|grep -w "PVDevices" |awk -F"=" '{ print $2 }'|sed 's/ //g'|awk -F":" '{print $4}'` 
    #To check the status of the db2instance
    ps -ef|grep "db2sysc" |grep $user >>/dev/null
    if [ $? -ne 0 ];then
        echo "Db2 instance $user is not running"
        echo "starting the db2instance $user"
        TMP_FILE=/tmp/db2status.$$
        su - $user -c "db2start" >$TMP_FILE
      
        grep "DB2START processing was successful" $TMP_FILE >>/dev/null
        if [ $? -ne 0 ];then
            echo "Unable to start the db2 instance $user"
            cat $TMP_FILE
            rm -f $TMP_FILE
            exit 1
        else
            echo "db2instance $user is started successfully"
        fi
        rm -f $TMP_FILE
     fi

     TMP_FILE1=/tmp/db2out.$$

     i=1
     while [[ $i -le $NUM_DB ]]
     do
        DB_NAME=`echo $DB_NAMES | cut -d" " -f$i|tr '[A-Z]' '[a-z]'`
        DB_LOC=`echo $DB_LOCS | cut -d" " -f$i`

        if [[ "$DB_NAME" = "$eachdatabase" ]];then
           if [[ "$DB_LOC" = "$MountPoint" ]];then
               echo "$DB_NAME is already present on the target on same mount point $DB_LOC"
               su - $user -c "db2 <<END >$TMP_FILE1
                              uncatalog database $eachdatabase
               END"
               grep "The UNCATALOG DATABASE command completed successfully." $TMP_FILE1 >>/dev/null
               if [ $? -ne 0 ];then
                  echo "Failed to uncatalog the database $eachdatabase"
                  cat $TMP_FILE1
                  rm -f $TMP_FILE1
                  exit 1
               fi
          else
             echo "$DB_NAME is already present on the target on different mount point $DB_LOC"
             su - $user -c "db2 <<END >$TMP_FILE1
                            deactivate database $eachdatabase
                            drop database $eachdatabase
             END"
             grep "The DROP DATABASE command completed successfully" $TMP_FILE1 >>/dev/null
             if [ $? -ne 0 ];then
                 grep "SQL1031N" $TMP_FILE1 >>/dev/null
                 if [ $? -eq 0 ];then
                       echo "The database directory cannot be found on the indicated file system, uncatalog the database"
                       su - $user -c "db2 <<END >$TMP_FILE1
                                      uncatalog database $eachdatabase
                       END"
                       grep "The UNCATALOG DATABASE command completed successfully." $TMP_FILE1 >>/dev/null
                       if [ $? -ne 0 ];then
                           echo "Failed to cleanup the existing database"
                           cat $TMP_FILE1
                           rm -f $TMP_FILE1
                           exit 1
                       fi
                else
                     echo "Failed to drop the database $eachdatabase.."
                     echo "Database $eachdatabase already exists on the mount point $DB_LOC. Cleanup this database"
                     cat $TMP_FILE1
                     rm -f $TMP_FILE1
                     exit 1
                fi
             fi
           fi

       fi
      ((i=i+1))
    done

    rm -f $TMP_DB_OUTPUT

        su - $user -c "db2 <<END >$TMP_FILE1
            catalog database $eachdatabase on $MountPoint
	        restart db $eachdatabase write resume
            disconnect $eachdatabase
 	        activate database $eachdatabase
END"
        grep "The CATALOG DATABASE command completed successfully." $TMP_FILE1 >>/dev/null
        if [ $? -ne 0 ]
        then
            #Bug 24610 - SQL1005N  The database alias "dbtest44" already exists in either the local
            #database directory or system database directory. When Database is already cataloged then it should be considered as success.
            grep "SQL1005N" $TMP_FILE1 >>/dev/null
            if [ $? -eq 0 ];then
                echo "The database $eachdatabase is already cataloged continuing with further actions .."
            else
                echo "Failed to catalog the database $eachdatabase.."
                cat $TMP_FILE1
                rm -f $TMP_FILE1
                exit 1
            fi
        else
            echo "The catalog database command succeeded for database $eachdatabase.."
        fi

#        su - $user -c "db2 <<END >$TMP_FILE1
#	    restart db $eachdatabase write resume
#	    activate database $eachdatabase
#END"
        grep "The RESTART DATABASE command completed successfully." $TMP_FILE1 >>/dev/null
        if [ $? -ne 0 ]
        then
            echo "Failed to write resume the database $eachdatabase.."
            cat $TMP_FILE1
            rm -f $TMP_FILE1
            exit 1
        else
            echo "The write resume command succeeded for database $eachdatabase.."
        fi

        grep "The SQL DISCONNECT command completed successfully." $TMP_FILE1 >>/dev/null
        if [ $? -ne 0 ]
        then
           echo "Failed to disconnect the database $eachdatabase.."
           echo "Activation of $eachdatabase is not done"
           cat $TMP_FILE1
           rm -f $TMP_FILE1
           exit 1
       else
           echo "$eachdatabase is successfully disconnected.."
       fi
       
       grep "The ACTIVATE DATABASE command completed successfully." $TMP_FILE1 >>/dev/null
       if [ $? -ne 0 ]
       then
           echo "Failed to activate the database $eachdatabase.."
           cat $TMP_FILE1
           rm -f $TMP_FILE1
           exit 1
       else
           echo "Successfully activated the database $eachdatabase.."
       fi
       rm -f $TMP_FILE1

	
    done
    
}

    #Change the current working directory to installationpath/scripts
    cd $SCRIPTS_DIR 
    case $Action in
	discovery )
		Db2Discovery
		;;
	consistency )
		Db2Freeze
		Db2VacpTagGenerate
		Db2UnFreeze
		;;
	failover )
		RollbackVolumes
		MountVolumes
		ConnectToDatabase
		;;
    esac

