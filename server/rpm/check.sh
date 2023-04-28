#!/bin/sh

SCRIPT_DIR=`dirname $0`
SCRIPT_NAME=`basename $0`

cd $SCRIPT_DIR

CX_mode=${1:-install}

php_version=`php --version 2>/dev/null | grep ^PHP | cut -d" " -f2 | cut -d"." -f1,2`

if [ "$php_version" != "5.2" ]; then
        php_value=php
        php_rpm=${php_value}
else
        php_value=php52
        php_rpm=${php_value}
fi

CheckACL_Support() {
    HomePart=`cat /proc/mounts|grep '/home '| awk '{ print $1 }'`
    if [ -n "$HomePart" ]; then
        VERIFY1=`tune2fs -l $HomePart|grep -i acl`
        VERIFY2=`grep $HomePart /proc/mounts | grep -i acl`
        if [ -z "$VERIFY1" -a -z "$VERIFY2" ]; then
            if grep -q "^$HomePart" /etc/fstab ; then
                LineNo=$(grep -n "^$HomePart" /etc/fstab | awk -F: '{ print $1 }')
                sed -i -e $LineNo'{s/defaults.*/defaults,acl 1 2/g}' /etc/fstab
            else
                echo "$HomePart  /home   ext4    defaults,acl    1 2" >> /etc/fstab
            fi

            if mount -o remount,acl /home ; then
                echo -e "\n\tAble to remount /home file system with ACL support"
            else
                echo -e "\n\tUnable to remount /home file system with ACL support."
                echo -e "\n\tReboot CX server and retry. (OR) Please contact the Admin/Support."
                exit 1
            fi
        else
            echo -e "\n\t$HomePart already mounted with ACL support"
        fi
    fi
}

Check_Space_Requirements()
{
#######################################################
# Start running all the checks now (packages and space)
#######################################################

echo
echo "       Running other necessary checks before installation commences..."
echo

# if /home partition exists, then check for 2 GB free space on /home, otherwise check for 2 GB free space on /.
home_partition=`df -kP | awk '{print $6}' | grep /home`
if [ ! -z "$home_partition" ]; then
        echo -n "       Checking for space availability on '/home'..."
        spaceh=`df -kP /home | tail -1 |awk '{print $4}'`

        if [ $spaceh -ge 2097152 ]; then
                echo " pass"
        else
                echo " failed!"
                echo "       ---> Minimum required space on '/home' is 2 GB!"
                echo
        fi
else
        echo -n "       Checking for space availability on '/'..."
        spacer=`df -kP / | tail -1 |awk '{print $4}'`

        if [ $spacer -ge 2097152 ]; then
                echo " pass"
        else
                echo " failed!"
                echo "       ---> Minimum required space on '/' is 2 GB!"
                echo
        fi

fi

# Check for 250 MB free space on /var.
echo -n "       Checking for space availability on '/var' ..."
spaceh=`df -kP /var | tail -1 |awk '{print $4}'`

if [ $spaceh -ge 256000 ]; then
	echo " pass"
else
   	echo " failed!"
   	echo "       ---> Minimum required space on '/var' is 250 MB!"
   	echo
fi

# Check for 1 GB free space on /tmp.
echo -n "       Checking for space availability on '/tmp' ..."
spaceh=`df -kP /tmp | tail -1 |awk '{print $4}'`

if [ $spaceh -ge 1048576 ]; then
        echo " pass"
else
        echo " failed!"
        echo "       ---> Minimum required space on '/tmp' is 1GB!"
        echo
fi


which getenforce > /dev/null 2>&1

if [ $? -eq 0 ]; then
        echo -n "       Checking whether SELinux is disabled ..."
        if getenforce 2>&1 | grep -iq "disabled\|failed"
        then
           echo " pass"
        elif getenforce 2>&1 | grep -iq "enforcing\|permissive"
        then
           echo " failed!"
           echo "       ---> SELinux needs to be disabled for this product to work!"
           echo
        fi
fi

}

case $CX_mode in
install)

#######################################################
# Check whether we support our product for this OS
#######################################################

OS=`./OS_details.sh 1`

if [ "$OS" != "RHEL6-64" -a "$OS" != "RHEL6U4-64" ]; then
	echo
	echo "  ---> This product is not supported for this platform!"
	echo
	exit 1
fi

#######################################################
# Set all variables here - common as well as OS-based
#######################################################

RPM_ARCH=$(rpm -q --qf "%{ARCH}" rpm)
echo
echo "       RPM architecture for this platform is $RPM_ARCH"

if grep -q 'CentOS release 6.* (Final)' /etc/redhat-release ; then
    COMMON_PACKAGE_LIST="perl-DBI- perl-5.10 perl-libwww-perl- rrdtool-1 rrdtool-perl-1"
elif grep -q 'Red Hat Enterprise Linux Server release 6.* (Santiago)' /etc/redhat-release ; then
    COMMON_PACKAGE_LIST="perl-DBI- perl-5.10 perl-libwww-perl- rrdtool-1 rrdtool-perl-1"
else
    COMMON_PACKAGE_LIST="perl-DBI- perl-5.8 perl-libwww-perl-"
fi

PACKAGE_LIST="${php_rpm}-5 ${php_value}-mysql-5 perl-URI httpd-2 mysql-5 mysql-server-5 perl-DBD-MySQL- ${php_value}-pear-[0-9] ${php_value}-ldap"

#############################
# Loop over the packages now
#############################

# To print the list of the packages that are required.

echo
echo -n "       The list of required RPM packages is :" && echo

for pkg in $COMMON_PACKAGE_LIST $PACKAGE_LIST
do
        echo
        pkg_name=`echo $pkg | sed s/-$//`
        echo -n "       $pkg_name"
done

echo
echo

# Check not just for package name but also its RPM architecture
# This check had to be introduced because one can install 32-bit packages on 64-bit machines but this may lead to ldd failures
# Refer bug 6785 for more details

for pkg in $COMMON_PACKAGE_LIST $PACKAGE_LIST
do
	echo -n "       Checking for $RPM_ARCH package $pkg ..."	
	if ! rpm -qa | grep -i -q "^$pkg" ; then
				echo " failed!"
				RPM_NOT_FOUND=$?
				echo "       ---> $RPM_ARCH package $pkg needs to be present for this product to work!"
				echo
		else
		for rpmpkg in `rpm -qa | grep -i "^$pkg" | sort -u`
		do
			if rpm -q --qf '%{ARCH}\n' $rpmpkg | grep -q "${RPM_ARCH}\|noarch" ; then
				echo " pass"
			else
				echo "       ---> $RPM_ARCH package $pkg needs to be present for this product to work!"
				echo
			fi
		done
		fi
done

echo

if [ ! -z "$RPM_NOT_FOUND" ]; then
    echo "       All the required packages are NOT PRESENT on this system for this installation to succeed."
else
    echo "       All the required packages are present on this system for this installation to succeed!"
fi

Check_Space_Requirements


CheckACL_Support
;;

upgrade)

Check_Space_Requirements

CheckACL_Support
;;
esac

echo
