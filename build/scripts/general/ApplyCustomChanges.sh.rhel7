#!/bin/bash
#####################################################################################
#Purpose: Used to apply custom configuration changes on CentOS/RHEL 6.6 Master Target
######################################################################################


DO()
{    
    eval "$@ 2>> $LOG_FILE | tee -a $LOG_FILE"
}

DATE_TIME_SUFFIX=`date +%d_%b_%Y_%H:%M:%S`
LOG_FILE=/var/log/appliedmtcustomchanges.log


echo -e "\t$DATE_TIME_SUFFIX" >> $LOG_FILE
echo -e "\t####################\n" >> $LOG_FILE

if [ -e /etc/.appliedmtcustomchanges ]; then
    DO 'echo'
    DO 'echo "--------------------------------------------------------------------------------"'
    DO 'echo "ApplyCustomChanges.sh script was already invoked. Please re-try the installation"'
    DO 'echo "--------------------------------------------------------------------------------"'
    exit 0
fi

DO 'echo -e "\tCustom Configuration\n"'
DO 'echo -e "\t####################\n"'

DO 'echo "WARNING: This script is intended to be executed only on Master Target servers. "'
DO 'echo "         If this server is not a Master Target, please do not continue. "'
DO 'echo'
while [ "X$Choice" = "X" ]
do
    DO 'echo -n "Are you sure you want to continue? [Y|N] [default N] : "'
    read Choice
    if [ -z "$Choice" -o "$Choice" = "N" -o "$Choice" = "n" ]; then
        Choice="N" ; exit 1
    elif [ "$Choice" = "Y" -o "$Choice" = "y" ];then
        Choice="Y" 
    else
        Choice=""
        DO 'echo -e "Invalid answer. Please retry.\n"'
    fi
done

# Make sure only root can run our script
DO 'echo -n "Checking for root privileges ... "'
[[ $EUID -ne 0 ]] && DO 'echo "FAILED"' && DO 'echo "This script must be run as root"' && exit 1
DO 'echo "OK"'

# Make sure OS is RedHat/CentOS 7.4(64bit)
DO 'echo -n "Checking for RHEL/CentOS 7.4(64bit) ... "'
if grep -q 'CentOS Linux release 7.4' /etc/redhat-release || grep -q 'Red Hat Enterprise Linux Server release 7.4' /etc/redhat-release ; then
	if uname -a | grep -q x86_64; then
		OSCheck=0
	else
        OSCheck=1
    fi
else
    OSCheck=1
fi
[[ $OSCheck -ne 0 ]] && { 
    DO 'echo "FAILED"' 
    exit 1
} || DO 'echo "OK"'

# Make sure required packages are available.
DO 'echo "Checking for required software packages"'
REQ_PKG_LIST="device-mapper-multipath lsscsi kpartx python2-pyasn1 lvm2 iprutils"

rm -f /var/log/missing-packages.log /dev/null 2>&1
for pkgName in `echo $REQ_PKG_LIST`
do
    DO echo -n \"Checking for $pkgName ... \"
    pkg=`rpm -qa $pkgName`
    if [ "$pkg" != "" ]; then
        DO echo "$pkg"
        PKG_MISSING=0
    else
        PKG_MISSING=1
        DO echo "Not Installed"
        echo "package $pkgName is not installed." >>/var/log/missing-packages.log
    fi
done
[[ $PKG_MISSING -ne 0 ]] && DO 'echo "FAILED"' || DO 'echo "OK"'
if [ -s /var/log/missing-packages.log ]; then
    DO 'echo'
    DO 'cat /var/log/missing-packages.log'
	DO 'echo "Command to install package: yum install package-name"'
    DO 'echo "Please install missing packages and re-try. Aborting..."'
    exit 1
fi

# Set core file size to unlimited
DO 'echo -n "Set core file size to unlimited ... "'
sed -i -e '/ulimit -c unlimited/d' /etc/bashrc
echo "ulimit -c unlimited" >> /etc/bashrc
[[ $? -ne 0 ]] && DO 'echo "FAILED"'|| DO 'echo "OK"'


# multipath.conf file creation
DO 'echo -n "Creating multipath.conf file ... "'
cat > /etc/multipath.conf << EOF
defaults {
        user_friendly_names no
		find_multipaths no
}

blacklist {
#For AT Lun masking
        device {
                vendor  "InMage"
        }
devnode "^cciss!c[0-9]d[0-9]*"
}
EOF
[[ $? -ne 0 ]] && DO 'echo "FAILED"'|| DO 'echo "OK"'

# Creating default multipath.conf file
DO 'echo -n "Configuring multipath to blacklist cciss disk ... "'
sed -i -e '/^blacklist[[:space:]]*{/ a devnode "^cciss!c[0-9]d[0-9]*"' /etc/multipath.conf
[[ $? -ne 0 ]] && DO 'echo "FAILED"' || DO 'echo "OK"'

# Black listing root device and enabled multipath
DO 'echo -n "Blacklist root disk and enable multipath... "'
BootDevice=`mount | grep "on / " | awk '{print $1}'`
BootDevice=`echo ${BootDevice} | sed -e "s:[sp]*[0-9]*$::"`
WWID=`/lib/udev/scsi_id --whitelisted --device=$BootDevice`
cat > /usr/sbin/blacklistrootdev.sh << EOF
[[ -n "$WWID" ]] && sed -i -e "/^blacklist.*{/ a wwid $WWID" /etc/multipath.conf || exit 1
if [ -f /.blacklistrootdev ] ; then
    sed -i "/^wwid/d" /etc/multipath.conf ; sed -i -e "/^blacklist.*{/ a wwid $WWID" /etc/multipath.conf
fi
EOF
chmod 755 /usr/sbin/blacklistrootdev.sh
[[ -f /etc/multipath.conf ]] && /usr/sbin/blacklistrootdev.sh > /var/log/blacklistrootdev.log 2>&1
touch /.blacklistrootdev > /dev/null 2>&1
[[ $? -ne 0 ]] && DO 'echo "FAILED"' || DO 'echo "OK"'

# Creating dummy file to restrict multiple execution
touch /etc/.appliedmtcustomchanges >/dev/null 2>&1

DO 'echo -e "*** NOTE: REBOOT IS REQUIRED FOR CUSTOM CHANGES TO TAKE EFFECT.***\n"'
DO 'echo -e "\n\t********** COMPLETED ************"'
DO 'echo -e "\t##################################\n"'

