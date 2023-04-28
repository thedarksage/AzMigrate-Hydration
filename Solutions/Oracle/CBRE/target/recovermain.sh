  #!/bin/sh -x
  ########################################################################
  ##### Modify the script location and Archive log location	#########
  ##### as per setup.						#########
  #### First argument for number of hours delay.    		#########
  #### If no argument is specified it will be uptodate.           #########
  ##### If the log deletion process was automated using FX process ########
  ##### Modify the loginfo directory based on the configuration    ########
  ##### ###################################################################
  HOME="/usr/local/InMage/Fx"
  LOGDIR="$HOME/loginfo"
  scr_loc="$HOME/scripts"
  EMAILLIST="srikanth.chouta@inmage.net srinivasrao.koganti@inmage.net ganeshvishnu.nalawade@inmage.net"
  ###########The below parameters have to be changed based on environment#######################
  ORACLE_HOME=/home/oracle/10.2.0
  ORACLE_SID=orainmag
  archdir="/home/oracle/oradata/archive"
  
  chown  oracle:dba $archdir/*
  if [ $# -ne 1 ]
  then
	  echo "Command format is : Recovermain.sh <n> WHERE <n> is time in hours...Exiting..."
	  exit 1
  fi
  delaytime=$1
  su - oracle -c ""$scr_loc"/recover.sh $delaytime" 
  grep "ORA-00278" $LOGDIR/recoveroutput_$ORACLE_SID.txt | cut -f2 -d"'"|awk -F"/" '{print $NF }' >> $LOGDIR/archives_applied_$ORACLE_SID.txt
  grep "ORA-00289" $LOGDIR/recoveroutput_$ORACLE_SID.txt |tail -1|cut -f2 -d"'"|awk -F"/" '{print $NF }' > $LOGDIR/neededfile_$ORACLE_SID.txt
