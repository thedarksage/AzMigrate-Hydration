#!/bin/bash

str=`date`
Date=`echo $str  |  sed -e 's/ /_/g'`

export LOGFILE=/LayoutCreation-$Date.log

osf=`uname -a`
echo "$osf" >> /uname.txt


grep "CENTOS6" /uname.txt
if [ $? -eq 0 ];then
(echo n; echo p; echo 1; echo 1; echo +5G; echo w) | fdisk /dev/sdc
(echo n; echo p; echo 1; echo 1; echo +6G; echo w) | fdisk /dev/sdd
(echo n; echo p; echo 1; echo 1; echo +7G; echo w) | fdisk /dev/sde
(echo n; echo p; echo 1; echo 1; echo +8G; echo w) | fdisk /dev/sdf
else
(echo n; echo p; echo 1; echo ; echo +5G; echo w) | fdisk /dev/sdc
(echo n; echo p; echo 1; echo ; echo +6G; echo w) | fdisk /dev/sdd
(echo n; echo p; echo 1; echo ; echo +7G; echo w) | fdisk /dev/sde
(echo n; echo p; echo 1; echo ; echo +8G; echo w) | fdisk /dev/sdf
fi

echo "Partition created.. listing below" >> $LOGFILE
fdisk -l >> $LOGFILE

sudo apt-get update >> $LOGFILE #&> /dev/null
sudo apt-get install lvm2 -y >> $LOGFILE # &> /dev/null
echo "lvm2 installed successfully." >> $LOGFILE

echo "Creating PVs" >> $LOGFILE

pvcreate /dev/sdc1 /dev/sdd1 /dev/sde1 /dev/sdf1

echo "Creating VG" >> $LOGFILE
vgcreate striped_vg /dev/sdc1 /dev/sdd1 /dev/sde1 /dev/sdf1

echo "Creating LVs " >> $LOGFILE
lvcreate -i4 -L3G -n lv3G striped_vg
lvcreate -i4 -L4G -n lv4G striped_vg

echo "FS Mounting on lvs" >> $LOGFILE
mkfs.ext3 /dev/striped_vg/lv3G
mkfs.ext4 /dev/striped_vg/lv4G
mkdir /mnt/lv3G
mkdir /mnt/lv4G

echo "mounting the lvs" >> $LOGFILE
mount /dev/mapper/striped_vg-lv3G /mnt/lv3G
mount /dev/mapper/striped_vg-lv4G /mnt/lv4G

echo "Checking if Lvs mounted successfully and are online...." >> $LOGFILE
lvdisplay >> /LVs.txt
grep "lv3G" /LVs.txt
if [ $? -eq 0 ]
then
echo "LV lv3G is online." >> $LOGFILE
	grep "lv4G" /LVs.txt
	if [ $? -eq 0 ]
	then
		echo "LV lv3G is online." >> $LOGFILE
	else
		echo "LV lv4G did not come online" >> $LOGFILE
		echo "Failed"
		exit 1
	fi
else
	echo "LV lv3G did not come online" >> $LOGFILE
	echo "Failed"
	exit 1
fi

echo "/dev/mapper/striped_vg-lv3G /mnt/lv3G   ext3    defaults,nofail        0 0" >> /etc/fstab
echo "/dev/mapper/striped_vg-lv4G /mnt/lv4G   ext4    defaults,nofail        0 0" >> /etc/fstab

echo "removing tty option" >> $LOGFILE
sed -i -e 's/Defaults    requiretty.*/ #Defaults    requiretty/g' /etc/sudoers
echo "removed tty successfully." >> $LOGFILE


grep "Debian" /uname.txt
if [ $? -eq 0 ];then
	apt-get update >> $LOGFILE  
	sudo apt-get install build-essential -y >> $LOGFILE # &> /dev/null
	echo "build-essential installed" >> $LOGFILE
	sleep 20
	cd /home/azureuser
	wget http://nodejs.org/dist/v4.8.7/node-v4.8.7.tar.gz >> $LOGFILE # &> /dev/null
	tar -xzf node-v4.8.7.tar.gz >> $LOGFILE # &> /dev/null
	cd node-v4.8.7/
	./configure >> $LOGFILE # &> /dev/null
	make >> $LOGFILE # &> /dev/null
	sudo make install >> $LOGFILE # &> /dev/null
	echo "npm installed" >> $LOGFILE
	sleep 20
	sudo npm install azure-cli -g >> $LOGFILE # &> /dev/null
	echo "azure-cli installed" >> $LOGFILE
	sudo apt-get install nodejs-legacy -y >> $LOGFILE # &> /dev/null
	echo "nodejs-legacy installed" >> $LOGFILE
	/usr/local/lib/node_modules/azure-cli/bin/azure >> $LOGFILE # &> /dev/null
else 
grep "Ubuntu" /uname.txt
if [ $? -eq 0 ];then
			sudo apt-get install nodejs-legacy -y >> $LOGFILE # &> /dev/null
			echo "nodejs-legacy installed" >> $LOGFILE 
			sleep 20
			sudo apt-get install npm -y>> $LOGFILE # &> /dev/null
			echo "npm installed" >> $LOGFILE
			sleep 20
			sudo npm install -g azure-cli >> $LOGFILE # &> /dev/null
			echo "azure-cli installed" >> $LOGFILE
			/usr/local/lib/node_modules/azure-cli/bin/azure >> $LOGFILE # &> /dev/null
	else
			curl --silent --location https://rpm.nodesource.com/setup_4.x | sudo bash - >> $LOGFILE # &> /dev/null
			echo "curl installed" >> $LOGFILE
			sleep 20
			sudo yum -y install nodejs >> $LOGFILE # &> /dev/null
			echo "nodeja installed" >> $LOGFILE
			sleep 20
			sudo npm install azure-cli -g >> $LOGFILE # &> /dev/null
			echo "azure-cli installed" >> $LOGFILE
			/usr/bin/azure >> $LOGFILE # &> /dev/null
	fi
fi

echo "Success"
exit 0