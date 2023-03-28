present() {
        ls "$@" >/dev/null 2>&1
}
str=`date` 
Date=`echo $str  |  sed -e 's/ /_/g'`
	 
devices=$1
#echo "$devices"
#devicelist=($(echo "$devices" | tr ',' '\n'))
allDeviceVerificationSucceeded=1

cp ValidateTestTree-Logs.txt ValidateTestTree-Logs-$Date &> /dev/null
rm -rf /ValidateTestTree-Logs.txt &> /dev/null
	
	#for device in "${devicelist[@]}"
	for device in $(echo $devices | tr "," "\n")
	do
		if [ $allDeviceVerificationSucceeded -eq 1 ]; then
		 rm -rf /Result_DIValidation.txt &> /dev/null
		 rm -rf /Output_md5.log &> /dev/null
		 rm -rf /Output_sha.log &> /dev/nul
		 cp $device/gen.sha.target $device/gen.sha.target-$Date			
		 cp $device/gen.md5.target $device/gen.md5.target-$Date
		 rm -rf $device/gen.md5.target
		 rm -rf $device/gen.sha.target
			for file in `find $device -type f -name "[[:digit:]]*"`
							do
							md5sum  $file >> $device/gen.md5.target
							sha1sum  $file >> $device/gen.sha.target
							done
					if  present $device/gen.md5.target &&  present $device/gen.sha.target ; then
							echo "All required files are present" >> /ValidateTestTree-Logs.txt
									echo "--- Comparing source and target files ---" >> /ValidateTestTree-Logs.txt
									cat $device/gen.md5.source | uniq | while read LINE; do if `grep -q "$LINE" $device/gen.md5.target` ; then echo "$LINE matching"; else echo "$LINE not-matching"; fi; done  >> /Output_md5.log
									grep -q "not-matching" /Output_md5.log
																	if [ $? -eq 1 ]
																	then
																	echo "INFO: Data Verification Successful for $device using md5 algorithm" >> /ValidateTestTree-Logs.txt
																	else
																	echo "ERROR: Data Verification Failed for $device md5 algorithm" >> /ValidateTestTree-Logs.txt
																	fi
									cat $device/gen.sha.source | uniq | while read LINE; do if `grep -q "$LINE" $device/gen.sha.target` ; then echo "$LINE matching"; else echo "$LINE not-matching"; fi; done  >> /Output_sha.log
									grep -q "not-matching" /Output_sha.log
																	if [ $? -eq 1 ]
																	then
																	echo "INFO: Data Verification Successful for $device using sha1 algorithm" >> /ValidateTestTree-Logs.txt
																	else
																	echo "ERROR: Data Verification Failed for $device sha1 algorithm" >> /ValidateTestTree-Logs.txt
																	fi
					grep -q "ERROR" /ValidateTestTree-Logs.txt
									if [ $? -eq 1 ]
									then													
										#echo "Pass"
										echo "Pass" > /Result_DIValidation.txt	
										echo "allDeviceVerificationSucceeded=$allDeviceVerificationSucceeded for device $device" >> /ValidateTestTree-Logs.txt
									else
										#echo "Failed"
										echo "Failed" > /Result_DIValidation.txt
										allDeviceVerificationSucceeded=0
										echo "allDeviceVerificationSucceeded=$allDeviceVerificationSucceeded for device $device"
										exit 1
									fi
					else
									   echo "INFO: Files (gen.md5.target and gen.sha.target) are not found." >> /ValidateTestTree-Logs.txt
						   echo "--------------------------------------------------------------" >> /ValidateTestTree-Logs.txt
									exit 1
					fi
		else
		echo "Failed"
			exit 1
		fi
	done			
echo "Success"
echo "-----------------------------------------------------------------" 
exit 0
