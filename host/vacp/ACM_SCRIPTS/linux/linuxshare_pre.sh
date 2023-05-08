#!/bin/sh 

if [ -e /etc/exports ]; then
	cp -f /etc/exports ApplicationData/nfsshares
	echo "Successfully copied nfs share information"
else 
	echo "File Not Found: /etc/exports"
fi

if [ -e /etc/samba/smb.conf ]; then
	cp -f /etc/samba/smb.conf  ApplicationData/smbshares
	echo "Successfully copied smb share information"
else
	echo "File Not Found: /etc/samba/smb.conf"
fi


