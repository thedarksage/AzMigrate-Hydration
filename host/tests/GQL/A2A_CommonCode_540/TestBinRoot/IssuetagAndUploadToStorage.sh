#!/bin/bash

#Command: IssuetagAndUploadToStorage.sh <tagname> <Storage Account Name> <Storage Account Key> <Container Name>
#Example: ./IssuetagAndUploadToStorage.sh MyCustomTag Str1 Key1 cntr1

if [ $#  -ne 4 ]
then
echo""
echo "Usage IssuetagAndUploadToStorage.sh <tag name> <Storage Account Name> <Storage Account Key> <Container Name>"
echo "Example: Tag generation and upload to storage account str1 and key Key1 and container container1"
echo "IssuetagAndUploadToStorage.sh MyCustomTag str1 key1 container1"
echo""
exit 1
fi



str=`date`
Date=`echo $str  |  sed -e 's/ /_/g'`

mkdir /IssuedTag
mkdir /IssuedTag/TimeStamp

cp /IssuedTag/issuedtaglog.txt /IssuedTag/issuedtaglog.bkp-$Date
cp /IssuedTag/TimeStamp/taginserttimestamp.txt /IssuedTag/taginserttimestamp.bkp-$Date
rm -rf /IssuedTag/issuedtaglog.txt
rm -rf /IssuedTag/TimeStamp/taginserttimestamp.txt

export LOGFILE=/IssuedTag/IssueTagAndUploadToStorage-$Date.log

osf=`uname -a`
echo "$osf" >> /uname.txt
grep "Debian" /uname.txt
if [ $? -eq 0 ];then
export AZURECLI=/usr/local/lib/node_modules/azure-cli/bin/azure
else
grep "Ubuntu" /uname.txt
if [ $? -eq 0 ];then
        export  AZURECLI=/usr/local/lib/node_modules/azure-cli/bin/azure
        else
        export  AZURECLI=/usr/bin/azure
        fi
fi


touch /IssuedTag/IssueTagAndUploadToStorage-$Date.log


Upload_File ()
{
    if [ $# -ne 3 ] ; then
		echo "Upload-File argument error: storagename ,key and container name is missing" >> $LOGFILE
		echo "Failed"
		return 1
    fi

	# curl --silent --location https://rpm.nodesource.com/setup_4.x | sudo bash -
	# echo "curl installed" >> $LOGFILE
	# sleep 20

	# sudo yum -y install nodejs
	# echo "nodejs installed" >> $LOGFILE
	# sleep 20

	# sudo npm install azure-cli -g
	# echo "azure-cli installed" >> $LOGFILE
	# sleep 20

    export AZURE_STORAGE_ACCOUNT="$1"
    export AZURE_STORAGE_ACCESS_KEY="$2"
    CONTAINER_NAME="$3"
    SOURCE_FOLDER=/IssuedTag/TimeStamp/*

    #creating container
    # $AZURECLI storage container create $CONTAINER_NAME
    # if [ $? -ne 0 ] ; then
        # echo "Azurecli util failed to create container" >> $LOGFILE
        # return 1
    # else
        # echo "Azurecli util created the container" >> $LOGFILE
     # fi

    #upload files to storage
    for file in $SOURCE_FOLDER
    do
        $AZURECLI storage blob upload -q $file $CONTAINER_NAME
        if [ $? -ne 0 ] ; then
           echo "Azurecli util failed to upload files into container" >> $LOGFILE
           return 1
        else
            echo "Azurecli util uploaded $file into container" >> $LOGFILE
        fi
    done

    return 0;
}


retrycount=1

        while [ "$retrycount" -le 3 ]
        do
			touch /IssuedTag/issuedtaglog.txt
			/usr/local/ASR/Vx/bin/vacp -systemlevel -t $1 >> /IssuedTag/issuedtaglog.txt
			if [ $? -ne 0 ];  then
				echo "tag did not issue successfully, on count:$retrycount. will re-try after 2 minutes..." >> $LOGFILE
				cp /IssuedTag/issuedtaglog.txt /IssuedTag/issuedtaglog-Count-$retrycount-$Date
				rm -rf /IssuedTag/issuedtaglog.txt
				sleep 120
			else
				#touch /IssuedTag/TimeStamp/taginserttimestamp.txt
				grep "TagInsertTime:" /IssuedTag/issuedtaglog.txt | cut -f2 -d ":" | awk '{$1=$1;print}' >> /IssuedTag/TimeStamp/taginserttimestamp.txt
				echo "tag $1 issued successfully. " >> $LOGFILE
				Upload_File $2 $3 $4
				if [ $? -ne 0 ] ; then
					exit 1
				else
					echo "SUCCESS"
					exit 0
				fi
				break
			fi

		((++retrycount))
        done
echo "retrycount:$retrycount" >> $LOGFILE
if [ "$retrycount" -eq 4 ]
then
echo "error:tag did not issued successfuly after 3 retry" >> $LOGFILE
echo "Failed"
exit 1
fi
