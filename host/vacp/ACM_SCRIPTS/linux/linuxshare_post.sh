#!/bin/sh 

if [ -e ApplicationData/nfsshares ]; then
	cp -f ApplicationData/nfsshares /etc/exports
else
	echo "File Not Found: ApplicationData/nfsshares"
fi

if [ -e ApplicationData/smbshares ]; then
	cp -f ApplicationData/smbshares /etc/samba/smb.conf
else
	echo "File Not Found: ApplicationData/smbshares"
fi

if [ -e /etc/redhat-release ]; then
	service smb restart
	service nfs restart
elif [ -e /etc/SuSE-release ]; then
	/etc/init.d/nfs restart
	/etc/init.d/smb restart
fi
