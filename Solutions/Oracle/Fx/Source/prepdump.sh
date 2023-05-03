#!/bin/ksh 
 	########################################################################################################################
 	#This script is for getting oracle information and preparing for RMAN dump backup					#
 	#Copy Rights reserverd by InMage systems										#
 	#########################################################################################################################
	INMAGEHOME=`grep INSTALLATION_DIR /usr/local/.fx_version|cut -d"=" -f2`
 	LOGDIR=$INMAGEHOME/loginfo
 	SCRIPTS=$INMAGEHOME/scripts
	DF_OUTPUT="$LOGDIR/df_koutput.txt"

	
 	if [ ! -d $LOGDIR ]
 	then
      		mkdir -p $LOGDIR	
 	fi
  	getparameter()
  	{
  		echo "Please Ensure database is up and running..."
  		echo "Enter the Oracle SID: \c"
  		read orasid
  		echo "Enter Oracle Home: \c"
  		read orahome
  		ps -ef|grep -v grep|grep $orasid >>/dev/null
  		if [ $? -ne 0 ]
  		then
      			echo "$orasid either not configured or not running....please check: & ReRun it again..."
      			exit 0
  		fi 
  		echo "Enter the unix userid of Oracle[oracle]:\c"
		read orauser
		if [ -z "$orauser"  ]
		then
 			orauser=oracle
			echo "Assuming userid as oracle..."
		fi
  		id $orauser >>/etc/null
  		if [  $? -ne 0 ]
  		then
         		echo "----------------------\n$orauser not found...Please enter correct user id\n--------------------------"
    	 		getparameter 
  		fi
  		su - $orauser -c  "$SCRIPTS/getDBsize.sh $orasid $orahome" >>/dev/null
  	}

        ######################################END OF GET PARAMETER FUNCTION#############################################################################
        
        select_filesys()
	{
       
  		case `uname`  in
  		HP-UX  )  
          			i=1
				for p in `bdf|grep -v "Mounted on"`
				do
					echo "$p \c" >> $LOGDIR/df_koutput.txt
					i=`expr $i + 1`
					x=`expr $i % 7` >>/dev/null
					if [ $x  -eq 0 ]
					then
						echo "\n" >> $LOGDIR/df_koutput.txt
						i=1
					fi
				done
                		AWK=awk;;
	
  		Linux )
          			i=1
				for p in `df -k|grep -v "Mounted on"`
				do
					echo -n "$p " >> $LOGDIR/df_koutput.txt
					i=`expr $i + 1`
					x=`expr $i % 7` >>/dev/null
					if [ $x  -eq 0 ]
					then
						echo "" >> $LOGDIR/df_koutput.txt
						i=1
					fi
				done
               			#df -k |grep -v "Mounted on" > $LOGDIR/df_koutput.txt
				AWK=awk;;
  		SunOS )
               	        	df -k |grep -v "Mounted on" > $LOGDIR/df_koutput.txt
				AWK=/usr/xpg4/bin/awk;;

                AIX )
                                i=1
                                for p in `df -kP|grep -v "Mounted on"`
                                do
                                        echo  "$p \c" >> $LOGDIR/df_koutput.txt
                                        i=`expr $i + 1`
                                        x=`expr $i % 7` >>/dev/null
                                        if [ $x  -eq 0 ]
                                        then
                                                echo "" >> $LOGDIR/df_koutput.txt
                                                i=1
                                        fi
                                done

                                AWK=awk;;

 		esac



                grep -v "^$" $LOGDIR/df_koutput.txt >$LOGDIR/testfile
                mv $LOGDIR/testfile $LOGDIR/df_koutput.txt
 
        	sizedb=`grep -v "^$" $LOGDIR/dbsize_$orasid.txt|tail -1`
  		i=1
		NoOfFileSystems=`wc -l $LOGDIR/df_koutput.txt|awk '{ print $1 }'`
		while [ $i -le $NoOfFileSystems ]
		do

			filesys[$i]=`$AWK -v field=$i 'NR==field { print $6 }' $LOGDIR/df_koutput.txt`
			sizes[$i]=`$AWK -v field=$i 'NR==field { print $4 }' $LOGDIR/df_koutput.txt`
			i=`expr $i + 1`

		done
  		p=1 
  		echo "FileSytem  Free Space(inKBs)"
 		echo " ---------------------"
  		while [ $p -le $NoOfFileSystems ]
  		do
  			echo "$p)  ${filesys[$p]}   ${sizes[$p]}"
  			p=`expr $p + 1` 
  		done
		echo "Size of DB is: $sizedb (KBs)"
  		echo "Please select one ore more file system numbers(space separated):\c"
  		read option
  		size=$sizedb
		
	}
        ######################################START OF DISK INFO#########################################################################################
  	getdiskinfo()
  	{
		select_filesys
  		for opt in `echo $option`
  		do
       	    		newsize=`expr $size - ${sizes[$opt]}`
            		size=$newsize
			if [ $newsize -gt 0 ]
        		then
				if [ "${filesys[$opt]}" = "/" ]
				then
					echo /$orasid"   " ${sizes[$opt]}>>$LOGDIR/$orasid.txt
				else
           				echo ${filesys[$opt]}/$orasid"   " ${sizes[$opt]}>>$LOGDIR/$orasid.txt
				fi
        		else
				if [ "${filesys[$opt]}" = "/" ]
                                then
                                        echo /$orasid"   " ${sizes[$opt]}>>$LOGDIR/$orasid.txt
				else

           				echo ${filesys[$opt]}/$orasid"   "${sizes[$opt]} >>$LOGDIR/$orasid.txt
				fi
           		break
        		fi        
  		done
    		cat $LOGDIR/$orasid.txt|awk '{print $1 }'|while read x
 		do
  			mkdir -p $x
  			chown $orauser $x
		done
 	 
  		if [ $newsize -gt 0 ]
  		then
  			echo "File systems you selected cannot accommodate backup of $sizedb(KB)"
  			echo "Please select again\n"
        		return 1
  		else 
			return 0
		
  		fi
		
	}
	
	prepare_rman_script()
        {  	
	
  	##########Preparing rmanhotmain.sh##################################################
	 	echo "su - $orauser -c $SCRIPTS/rmanhot.sh" >$SCRIPTS/rmanhotmain.sh
                echo 'EXITCODE=$?' >>$SCRIPTS/rmanhotmain.sh
  		chmod +x $SCRIPTS/rmanhotmain.sh
 	######################################Preparing RMANHOT##############################
 		 
		pass="test"
  		#echo "Enter \"sys\" password:(For initiating  RMAN backup)"
		#stty -echo
  		#read pass
		#stty echo
		awk ' { printf("%s %d %d\n",$1,$2,($2-($2*0.05)) ) }' $LOGDIR/$orasid.txt     >$LOGDIR/myfile
		mv $LOGDIR/myfile $LOGDIR/$orasid.txt
		BACKUP_DIR=`awk '{ print $1 }'  $LOGDIR/$orasid.txt|head -1`
		echo "Copying init.ora file to $BACKUP_DIR directory..."
		sleep 5
  		cp /tmp/pfile.ora $BACKUP_DIR/$orasid.ora
		echo "Copying is done..."
		sleep 2
		echo "Preparing necessary scripts..."
		sleep 5
  		RMANBAT=$LOGDIR/rman$orasid.sql
		echo "connect target sys/$pass">$RMANBAT
  		echo RUN "{" >> $RMANBAT
		$AWK -v q="'" '{ printf("ALLOCATE CHANNEL C%d TYPE DISK format=%c%s%c  kbytes=%d;\n",NR,q,$1"/%u.%p",q,$3) }' $LOGDIR/$orasid.txt>>$RMANBAT
  		echo "BACKUP INCREMENTAL LEVEL 0 database;" >> $RMANBAT
  		echo COPY CURRENT CONTROLFILE FOR STANDBY TO "'"$BACKUP_DIR"/"$orasid"."CTL"'"";" >> $RMANBAT
		awk '{ printf(" RELEASE CHANNEL C%d;\n",NR) }' $LOGDIR/$orasid.txt >> $RMANBAT
  		echo "}" >> $RMANBAT
		chown $orauser $RMANBAT
		chmod 700 $RMANBAT	
		echo "umask 022" >$SCRIPTS/rmanhot.sh
  		echo "ORACLE_HOME=$orahome" >>$SCRIPTS/rmanhot.sh
  		echo "ORACLE_SID=$orasid" >>$SCRIPTS/rmanhot.sh
  		echo "export ORACLE_HOME ORACLE_SID" >>$SCRIPTS/rmanhot.sh
		echo "echo "Taking RMAN backup of database "" >>$SCRIPTS/rmanhot.sh
  		echo "$orahome/bin/rman cmdfile = $RMANBAT >/tmp/rmanoutput" >>$SCRIPTS/rmanhot.sh
		echo 'if [ $? -eq 0 ]' >>$SCRIPTS/rmanhot.sh
		echo "then">>$SCRIPTS/rmanhot.sh
		echo "echo "RMAN backup successful"" >>$SCRIPTS/rmanhot.sh
		echo "rm -f $RMANBAT" >>$SCRIPTS/rmanhot.sh
		echo "exit 0" >>$SCRIPTS/rmanhot.sh
		echo "else">>$SCRIPTS/rmanhot.sh
		echo "echo "RMAN backup failed"">>$SCRIPTS/rmanhot.sh
		echo "exit 255">>$SCRIPTS/rmanhot.sh
		echo "fi" >>$SCRIPTS/rmanhot.sh

    		cat $LOGDIR/$orasid.txt|awk '{print $1 }'|while read x
 		do
  			echo "chmod 777 $x/*" >> $SCRIPTS/rmanhotmain.sh
			echo "chmod 777 $x" >> $SCRIPTS/rmanhotmain.sh
		done
		echo 'exit $EXITCODE'>> $SCRIPTS/rmanhotmain.sh
  		chmod +x $SCRIPTS/rmanhot.sh
		echo "scripts have been prepared"
 	 
	
  	}
	IsASMUsed()
	{

		ps -ef|grep -v grep|grep $orasid|grep "asmb_$orasid" >>/dev/null
		return $?
		

	}
	IsRACInstance()
	{
		if [ -f $LOGDIR/Clusterinfo_$orasid.txt ]
		then
			str=`grep -v "^$" $LOGDIR/Clusterinfo_$orasid.txt|tail -1`
			[ $str = "YES" ] && return 0
			[ $str = "NO" ] && return 1

		else
			
			echo "Failed to Run following command on the database instance $orasid...."
			echo "select parallel from v\$instance.."
			echo "proceeding further.."
			return 1

		fi

	}
	ASM_RAC_Modifications()
	{

		if [ $ASMUSED -eq 1 -a $RACUSED -eq 0 ]
		then
			echo "ASM=yes">>$BACKUP_DIR/$orasid.ora
			
		fi
		if [ $RACUSED -eq 1 ]
		then

			x=`grep ".remote_listener" $BACKUP_DIR/$orasid.ora`
			grep -v "$x" $BACKUP_DIR/$orasid.ora >$LOGDIR/tmpfile
			echo "Is Remote database is on RAC (y/n):"
			read ans
			while [ "$ans" != "y" -a "$ans" != "n" -a "$ans" != "Y" -a "$ans" != "N" ]
			do

				echo "Is Remote database is on RAC (y/n):"
				read ans

			done
			if [ "$ans" = "n" -o "$ans" = "N" ]
			then

				sed 's/.cluster_database=true/.cluster_database=false/' $LOGDIR/tmpfile >$BACKUP_DIR/$orasid.ora
			else
				cp -f $LOGDIR/tmpfile $BACKUP_DIR/$orasid.ora	
			fi
			if [ $ASMUSED -eq 1 ]
			then
			
				echo "ASM=yes">>$BACKUP_DIR/$orasid.ora
			fi

		fi




	}
	OSdetails()
	{
		rm -f $DF_OUTPUT
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
		
                     		FSTABFILE="/etc/fstab";;
             		SunOS ) df_command="df"
                     		FSTABFILE="/etc/vfstab";;
             		AIX ) df_command="df"
                     		FSTABFILE="/etc/vfstab";;
        	esac
	}
	FindFsName()
	{
	rm -f $LOGDIR/df_koutput1.txt

	case `uname`  in
                HP-UX  )
                                i=1
                                for p in `bdf $Directory|grep -v "Mounted on"`
                                do
                                        echo "$p \c" >> $LOGDIR/df_koutput1.txt
                                        i=`expr $i + 1`
                                        x=`expr $i % 7` >>/dev/null
                                        if [ $x  -eq 0 ]
                                        then
                                                echo "\n" >> $LOGDIR/df_koutput1.txt
                                                i=1
                                        fi
                                done
                                AWK=awk;;

                Linux )
                                i=1
                                for p in `df -k $Directory|grep -v "Mounted on"`
                                do
                                        echo -n "$p " >> $LOGDIR/df_koutput1.txt
                                        i=`expr $i + 1`
                                        x=`expr $i % 7` >>/dev/null
                                        if [ $x  -eq 0 ]
                                        then
                                                echo "" >> $LOGDIR/df_koutput1.txt
                                                i=1
                                        fi
                                done
                                #df -k |grep -v "Mounted on" > $LOGDIR/df_koutput1.txt
                                AWK=awk;;
                SunOS )
                                df -k $Directory|grep -v "Mounted on" > $LOGDIR/df_koutput1.txt
                                AWK=/usr/xpg4/bin/awk;;

                AIX )
                                i=1
                                for p in `df -kP $Directory|grep -v "Mounted on"`
                                do
                                        echo  "$p \c" >> $LOGDIR/df_koutput1.txt
                                        i=`expr $i + 1`
                                        x=`expr $i % 7` >>/dev/null
                                        if [ $x  -eq 0 ]
                                        then
                                                echo "" >> $LOGDIR/df_koutput1.txt
                                                i=1
                                        fi
                                done

                                AWK=awk;;

                esac
		FsName=`grep -v "Mounted on" $LOGDIR/df_koutput1.txt|$AWK '{ print $NF }' `

	}

	#####################MAIN Programme/Start#############################
  	clear
	OSdetails
	set -A  filesys
	set -A sizes
	rm -f /tmp/spfile.ora
	rm -f /tmp/pfile.ora
  	rm -f $LOGDIR/df_koutput.txt 
	rm -f $LOGDIR/Clusterinfo_$ORACLE_SID.txt
	ASMUSED=0
	RACUSED=0
	echo "---------------------------------------------------------------------------------------------"
	echo "                 WELCOME TO RMAN Dump Prepare program					   "
	echo "---------------------------------------------------------------------------------------------"
	echo "Before you Start please ensure you have following parameters ready with You"
	echo "1.Oracle sid"
	echo "2.Oracle Home"
	echo "3.Oracle Userid"
	echo "4.List of file systems where you want to take the dump of Database"
	echo "Do you want to proceed(y/Y):\c"
	read ans
	if [ "$ans" = 'y' ] || [ "$ans" = "Y" ]
	then
		getparameter      ###Get Parameter function for getting values of ORACLE_SID,ORACLE_HOME and other info
		IsASMUsed         #Check Whether ASM being used for the instance or not
		if [ $? -eq 0 ]
		then
			
			ASMUSED=1
		fi
		IsRACInstance
		if [ $? -eq 0 ]
		then
			RACUSED=1

		fi
		if [ $ASMUSED -eq 0 -a $RACUSED -eq 0 ] ###For stand lone instance
		then

			echo "$orasid ditected on your system..." 
			

		fi
		
		if [ $ASMUSED -eq 1 -a $RACUSED -eq 1 ] ###Incase of ASM+RAC being used for instance
		then

			echo "Real Application Cluster and ASM ditected for $orasid..."
			echo "Press any key to continue...."
			read key


		fi
		
		if [ $ASMUSED -eq 0 -a $RACUSED -eq 1 ] ###Incase of Only RAC  being used for instance
		then

			echo "Real Application Cluster without ASM ditected for $orasid..."
			echo "Press any key to continue...."
			read key

		fi
		if [ $ASMUSED -eq 1 -a $RACUSED -eq 0 ] ###Incase of Only ASM being used for instance #if both not used,then its is standlone instance
		then

			echo "ASM ditected with instace for $orasid..."
			echo "Press any key to continue...."
			read key

		fi
		
  		rm -f $LOGDIR/$orasid.txt
		echo "Hi " >> /dev/null
		
 		getdiskinfo 
		while [[ $? -ne 0 ]]
		do
		
        		rm -f $LOGDIR/df_koutput.txt
                	rm -f $LOGDIR/$orasid.txt
 			getdiskinfo 
 		done
		echo "RMAN dump will be taken in following directory(ies)(default)"
		awk '{ print $1 }' $LOGDIR/$orasid.txt
		echo "Do you want enter custom directory ?(y/n):\c"
		read ans
		if [ "$ans" = 'y' ] || [ "$ans" = 'Y' ]
		then
			rmdir `awk '{ print $1 }' $LOGDIR/$orasid.txt`
			echo "Please enter absolute path:\c"
			read Directory
			if [ ! -d $Directory ]
			then
				echo "$Directory doesn't exist"
				echo "Do you want to create it?(y/n)\c"
				read ans
				if [ "$ans" = 'y' ] || [ "$ans" = 'n' ]
				then
					mkdir -p $Directory
					chown $orauser $Directory
				fi

			fi
			echo "Directory name is $Directory"
			echo "Please confirm (y/n):\c"
			read ans
			if [ "$ans" = 'y' ] || [ "$ans" = 'Y' ]
			then
				cd $Directory
				FindFsName	##this funtion will assign Mount point for the custom directory chosen by user
				#FsName=`df -k .|grep -v "Mounted on"|$AWK '{ print $NF }' `
				FreeSpace=`grep $FsName $LOGDIR/df_koutput.txt|head -1|awk '{ print $4 }'`
				echo "$Directory    $FreeSpace" > $LOGDIR/$orasid.txt
			fi
		fi
  		prepare_rman_script
		ASM_RAC_Modifications
		echo "$orasid Relication Setup Assistance....." |tee -a $LOGDIR/testfile
		echo "Please start the replication as follows:"|tee -a $LOGDIR/testfile
		echo "-------------------------------------------------------------------------------"|tee -a $LOGDIR/testfile
		echo " "|tee -a $LOGDIR/testfile
		echo "Prescript for First is:/usr/local/InMage/Fx/scripts/rmanhotmain.sh"|tee -a $LOGDIR/testfile
		echo " "|tee -a $LOGDIR/testfile
		awk  '{ if(NR==1) printf("\t\tjob\t\tSource Server\t\t\tTarget Server\n") }' $LOGDIR/$orasid.txt|tee -a $LOGDIR/testfile
		awk '{ printf("\t\t%d\t\t%s\t\t\t%s\t\n",NR-1,$1,$1) }' $LOGDIR/$orasid.txt|tee -a $LOGDIR/testfile
		echo "\nPostscript  for Last job is:\n/usr/local/InMage/Fx/scripts/preprestore.sh -u $orauser -h $orahome -p $BACKUP_DIR/$orasid.ora -c $BACKUP_DIR/$orasid.CTL -s $orasid"|tee -a $LOGDIR/testfile
		echo " "|tee -a $LOGDIR/testfile
  		rm -f $LOGDIR/df_koutput.txt
  		rm -f $LOGDIR/df_koutput1.txt
		rm -f $LOGDIR/testfile
		rm -f $LOGDIR/Clusterinfo_$orasid.txt
		
	else
		echo "Exiting..."
		exit 0
	fi
