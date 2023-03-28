#!/bin/bash
#$1 -- Output folder
#$2 -- max size of file in MB
#$3 -- Total size of data in MB
#$4 -- Generation 0 and Verification 1

if [[ $#  -ne 4 ]]
then
echo""
echo "Usage GenerateTestTree.sh <Directory name> <Max File Size> <Total Size in MB>"
echo "Example: Data Generation "
echo "GenerateTestTree.sh /home/test 2 100 0 "
echo ""
echo "Example: Data Verification "
echo "GenerateTestTree.sh /home/test 2 100 1 "
echo""
exit 1
fi

#Create the directory
if [[ $4 -eq 0 ]]
then
	echo "$(date "+%b  %d %T")" >> /opt/JobTimingUpdate.txt
	echo ""
	echo "Data Generation"
	echo ""
	if [[ ! -d $1 ]]
	then
  	mkdir -p  $1
  	echo "Creating directory"
  	echo""
	fi

	size=0
	totalSize=0

	while [ "$totalSize" -le $(($3*1024)) ]
	do
		let size=$(($(($RANDOM*$RANDOM)) % $(($2*1024))))
		if [[ $size -ne 0 ]]
		then
		echo "Creating a file of size: $size"
		dd if=/dev/urandom of=$1/$RANDOM bs=1024 count=$size &> /dev/null
		fi
		let "totalSize += size"
	done
	

	#calculate the md5
	echo "Calculating the md5"
	rm -f $1/gen.md5
	rm -f $1/gen.sha
	if [[ `uname` == "SunOS" && `uname -r` == "5.10" ]]
	then
		digest -v -a md5 $1/[[:digit:]]* > $1/gen.md5
		#digest -v -a sha1 $1/[[:digit:]]* > $1/gen.sha
	else
		for file in `find $1 -type f -name "[[:digit:]]*"`
		do
			if [ `uname` == "SunOS" ]
			then
				cksum $file >> $1/gen.ck
	        	else		
				md5sum  $file >> $1/gen.md5
				sha1sum  $file >> $1/gen.sha
			fi
		done
	fi
		#for file in `find $1 -type f -name "[[:digit:]]*"`
		#do
		#		md5sum -b $file >> $1/gen.md5.target
		#		sha1sum -b $file >> $1/gen.sha.target
		#done

elif [[ $4 -eq 1 ]]
then
	resmd5="null"	
	ressha="null"
	resck="null"
	echo "Data Verification of $1 in progress"
#	echo "---------------------------------------"
	if [[ `uname` == "SunOS" && `uname -r` == "5.10" ]]
	then
		digest -v -a md5 $1/[[:digit:]]* > $1/ver.md5
		#digest -v -a sha1 $1/[[:digit:]]* > $1/ver.sha
		diff $1/gen.md5 $1/ver.md5
		resmd5=$?
		#diff $1/gen.sha $1/ver.sha
		#ressha=$?
	else
		if [ `uname` == "SunOS" ]
                then
	        	for file in `find $1 -type f -name "[[:digit:]]*"`
                	do
				cksum $file >> $1/ver.ck
			done
			diff $1/gen.ck $1/ver.ck
			resck=$?	
		else
			md5sum --status -c $1/gen.md5
			resmd5=$?
			sha1sum --status -c $1/gen.sha
			ressha=$?
		fi
	fi
	if [ $resmd5 != "null" ]
	then 
	echo ""
	echo "Data Verification using md5 algorithm"
	if [[ $resmd5 -eq 0 ]]
	then
	echo "Data Verification Successful using md5 algorithm"
	else
	echo "Data Verification Failed using md5 algorithm"
	fi
	fi
	if [ $ressha != "null" ]
        then
	echo ""
	echo "Data Verification using sha1 algorithm"
	if [[ $ressha -eq 0 ]]
	then
	echo "Data Verification Successful using sha1 algorithm"
	else
	echo "Data Verification Failed using sha1 algorithm"
	fi
	echo""
	fi
        if [ $resck != "null" ]
        then
        if [[ $resck -eq 0 ]]
        then
	echo ""
        echo "Data Verification Successful using CRC algorithm"
        else
        echo "Data Verification Failed using CRC algorithm"
        fi
        fi
	echo "-----------xxxxxxxxxxxxxxxxxxxx-------------"	
	echo ""
fi
