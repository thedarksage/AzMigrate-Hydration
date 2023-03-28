#!/bin/bash
#$1 -- Output folder
#$2 -- max size of file in MB
#$3 -- Total size of data in MB
#$4 -- Generation and calculate-checksum 0 / Remove old Checksum if exist and Calculate-checkum 1

str=`date`
Date=`echo $str  |  sed -e 's/ /_/g'`

LogFile="/GenerateData-$Date.log"

if [ $#  -ne 4 ]
then
echo"" >> $LogFile
echo "Usage GenerateTestTree.sh <Directory name> <Max File Size> <Total Size in MB>" >> $LogFile
echo "Example: Data Generation " >> $LogFile
echo "GenerateTestTree.sh /home/test 2 100 0 " >> $LogFile
echo "" >> $LogFile
echo "Example: Data Verification " >> $LogFile
echo "GenerateTestTree.sh /home/test 2 100 1 " >> $LogFile
echo "Provided Less number of argument." >> $LogFile
echo "Failed"
exit 1
fi

	rm -rf /ValidateTestTree-Logs.txt >> $LogFile # &> /dev/null
	rm -rf /Result_DIValidation.txt >> $LogFile # &> /dev/null
	rm -rf /Output_md5.log >> $LogFile # &> /dev/null
	rm -rf /Output_sha.log >> $LogFile # &> /dev/null
	
devices=$1
echo "$devices"
for device in $(echo $devices | tr "," "\n")
#for device in "${devicelist[@]}"
do
   echo "For device $device"
	

	#Create the directory
	if [ $4 -eq 1 ]
	then
	echo "Skipping Generating data" >> $LogFile
	else
		rm -rf $device/* >> $LogFile # &> /dev/null
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

		while [ "$totalSize" -le $(($3*1024)) ]
		do
			RANDOM=`date '+%s'`
			size=$(($(($RANDOM*$RANDOM)) % $(($2*1024))))
			if [ $size -ne 0 ]
			then
			echo "Creating a file of size: $size" >> $LogFile
			dd if=/dev/urandom of=$device/$RANDOM bs=1024 count=$size >> $LogFile # &> /dev/null
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
			#digest -v -a sha1 $1/[[:digit:]]* > $1/gen.sha
		else
			   str=`date`
			   Date=`echo $str |  sed -e 's/ /_/g'`
			   cp $device/gen.md5.source $device/gen.md5.source-$Date >> $LogFile # &> /dev/null
			   cp $device/gen.sha.source $device/gen.sha.source-$Date >> $LogFile # &> /dev/null
			   rm -rf $device/gen.md5.source >> $LogFile # &> /dev/null
			   rm -rf $device/gen.sha.source >> $LogFile # &> /dev/null

			for file in `find $device -type f -name "[[:digit:]]*"`
			do
				md5sum  $file >> $device/gen.md5.source
				sha1sum  $file >> $device/gen.sha.source
			done
		fi
done
echo "Success"
