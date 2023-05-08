#!/bin/bash

#$1 -- Output folder
#$2 -- max size of file in MB
#$3 -- Total size of data in MB
#$4 -- Generation and calculate-checksum 0 / Remove old Checksum if exist and Calculate-checkum 1
#$5 -- Root User Password

DateStr=`date`
Date=`echo $DateStr  |  sed -e 's/ /_/g'`

LogFile="/ApplyLoad-$Date.log"

if [ $# -ne 5 ]
then
	echo ""
	echo "Usage ApplyLoad.sh <Directory name> <Max File Size> <Total Size in MB> <RootUserPassword>"
	echo "Example: Data Generation "
	echo "ApplyLoad.sh /home/test 2 100 0 RootUserPassword"
	echo ""
	echo "Example: Data Verification "
	echo "ApplyLoad.sh /home/test 2 100 1 RootUserPassword"
	echo "Incorrect Arguments Passed."
	echo "Failed"
exit 1
fi

MNTDIR=$1
MaxFileSize=$2
TotalFileSize=$3
VerifyFlag=$4
RootPassword=$5

LoginAsRoot()
{
    rootUser="root"

    if [ "$(whoami)" != "$rootUser" ]
    then
        echo "Logging in as $rootUser" >> $LogFile
        sudo su -s "$RootPassword" >> $LOGFILE 2>&1
        if [ $? -eq 0 ]; then
            echo "Successfully logged in as $rootUser" >> $LogFile
        else
            echo "Failed to login as $rootUser with exit status : $?" >> $LogFile
			exit 1
        fi
    fi

    me="$(whoami)"
    echo "Logged in as $me" >> $LogFile
}

ApplyChurn()
{
	DIRS=$(ls -1d ${MNTDIR}/disk* 2>/dev/null| sort --version-sort)
	echo "$DIRS"
	
	for device in $(echo $DIRS | tr "," "\n")
	do
		echo "For device $device"
		
		#Create the directory
		if [ $VerifyFlag -eq 1 ]
		then
		echo "Skipping Generating data" >> $LogFile
		else
			rm -rf $device/* >> $LogFile
			echo "Starting Generation"
			echo "$(date "+%b  %d %T")" >> $LogFile
			echo "" >> $LogFile
			echo "Data Generation" >> $LogFile
			echo "" >> $LogFile
			if [ ! -d $device ]
			then
			mkdir -p  $device
			echo "Creating directory" >> $LogFile
			echo"" >> $LogFile
			fi

			size=0
			totalSize=0

			while [ "$totalSize" -le $(($TotalFileSize*1024)) ]
			do
				RANDOM=`date '+%s'`
				size=$(($(($RANDOM*$RANDOM)) % $(($MaxFileSize*1024))))
				if [ $size -ne 0 ]
				then
				echo "Creating a file of size: $size" >> $LogFile
				dd if=/dev/urandom of=$device/$RANDOM bs=1024 count=$size >> $LogFile
				fi
				totalSize=$(($totalSize + $size))
			done
		fi

		#calculate the md5
		echo "Calculating the md5 and sha"
		rm -f $device/gen.md5
		rm -f $device/gen.sha
		if [[ `uname` == "SunOS" && `uname -r` == "5.10" ]]
		then
			digest -v -a md5 $device/[[:digit:]]* > $device/gen.md5
		else
			str=`date`
			Date=`echo $str |  sed -e 's/ /_/g'`
			cp $device/gen.md5.source $device/gen.md5.source-$Date >> $LogFile
			cp $device/gen.sha.source $device/gen.sha.source-$Date >> $LogFile
			rm -rf $device/gen.md5.source >> $LogFile
			rm -rf $device/gen.sha.source >> $LogFile

			for file in `find $device -type f -name "[[:digit:]]*"`
			do
				md5sum  $file >> $device/gen.md5.source
				sha1sum  $file >> $device/gen.sha.source
			done
		fi
	done
}

#######################
# MAIN ENTRY POINT
#######################

echo "Params :"
echo "$0 $@"

LoginAsRoot
ApplyChurn

echo "Success"
exit 0