#!/bin/bash

mntpath=$1
vx_version_file=`ls -l $mntpath/usr/local/.vx_version | awk -F'-> ' '{ print $2 }'`
if [ "$vx_version_file" = "" ]
then
    vx_version_file=/usr/local/.vx_version
fi
os_built_for=`grep OS_BUILT_FOR $mntpath/$vx_version_file | awk -F= '{ print $2 }'`

case $os_built_for in
    OL*)
        UNAME=Linux
    ;;
    RHEL*)
        UNAME=Linux
    ;;
    SUSE*)
        UNAME=Linux
    ;;
    SLES*)
        UNAME=Linux
    ;;
    DEBIAN*)
        UNAME=Linux
    ;;
    UBUNTU*)
        UNAME=Linux
    ;;
    XEN*)
        UNAME=Linux
    ;;
esac

is_6u6p()
{
	Base_version=`cat $1 |awk -F"\." '{print $1}' 2>/dev/null | awk '{print $NF}'`
	OS_update=`cat $1 | awk -F"\." '{print $2}' 2>/dev/null|awk '{print $1}'`

	if [  $(($Base_version)) -eq 6 ] && [ $(($OS_update)) -ge 6 ];then
        	return 0
	else
        	return 1
	fi
}

case $UNAME in
  Linux)
    if [ -f $mntpath/etc/enterprise-release ] && [ -f $mntpath/etc/redhat-release ] ; then
        if grep -q 'Enterprise Linux Enterprise Linux Server release 5 (Carthage)' $mntpath/etc/enterprise-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="OL5-32"
            else
                OS="OL5-64"
            fi
        elif grep -q 'Enterprise Linux Enterprise Linux Server release 5.1' $mntpath/etc/enterprise-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="OL5U1-32"
            else
                OS="OL5U1-64"
            fi
        elif grep -q 'Enterprise Linux Enterprise Linux Server release 5.2' $mntpath/etc/enterprise-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="OL5U2-32"
            else
                OS="OL5U2-64"
            fi
        elif grep -q 'Enterprise Linux Enterprise Linux Server release 5.2' $mntpath/etc/enterprise-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="OL5U2-32"
            else
                OS="OL5U2-64"
            fi
        elif grep -q 'Enterprise Linux Enterprise Linux Server release 5.3' $mntpath/etc/enterprise-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="OL5U3-32"
            else
                OS="OL5U3-64"
            fi
        elif grep -q 'Enterprise Linux Enterprise Linux Server release 5.4' $mntpath/etc/enterprise-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="OL5U4-32"
            else
                OS="OL5U4-64"
            fi
        elif grep -q 'Enterprise Linux Enterprise Linux Server release 5.5' $mntpath/etc/enterprise-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="OL5U5-32"
            else
                OS="OL5U5-64"
            fi
        elif grep -q 'Enterprise Linux Enterprise Linux Server release 5.6' $mntpath/etc/enterprise-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="OL5U6-32"
            else
                OS="OL5U6-64"
            fi
        elif grep -q 'Enterprise Linux Enterprise Linux Server release 5.7' $mntpath/etc/enterprise-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="OL5U7-32"
            else
                OS="OL5U7-64"
            fi
        elif grep -q 'Enterprise Linux Enterprise Linux Server release 5.8' $mntpath/etc/enterprise-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="OL5U8-32"
            else
                OS="OL5U8-64"
            fi
        elif grep -q 'Enterprise Linux Enterprise Linux Server release 5.9' $mntpath/etc/enterprise-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="OL5U9-32"
            else
                OS="OL5U9-64"
            fi
        elif grep -q 'Enterprise Linux Enterprise Linux Server release 5.10' $mntpath/etc/enterprise-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="OL5U10-32"
            else
                OS="OL5U10-64"
            fi
        fi
    elif [ -f $mntpath/etc/oracle-release ] && [ -f $mntpath/etc/redhat-release ] ; then
        if grep -q 'Oracle Linux Server release 6.0' $mntpath/etc/oracle-release ; then
            if `echo $os_built_for | grep -q "32"`; then 
                OS="OL6-32"
            else
                OS="OL6-64"
            fi
        elif grep -q 'Oracle Linux Server release 6.0' $mntpath/etc/oracle-release ; then
            if `echo $os_built_for | grep -q "32"`; then 
                 OS="OL6-32"
            else
                OS="OL6-64"
            fi
        elif grep -q 'Oracle Linux Server release 6.1' $mntpath/etc/oracle-release ; then
            if `echo $os_built_for | grep -q "32"`; then 
                 OS="OL6U1-32"
            else
                OS="OL6U1-64"
            fi
        elif grep -q 'Oracle Linux Server release 6.2' $mntpath/etc/oracle-release ; then
            if `echo $os_built_for | grep -q "32"`; then 
                 OS="OL6U2-32"
            else
                OS="OL6U2-64"
            fi
        elif grep -q 'Oracle Linux Server release 6.3' $mntpath/etc/oracle-release ; then
            if `echo $os_built_for | grep -q "32"`; then 
                 OS="OL6U3-32"
            else
                OS="OL6U3-64"
            fi
        elif grep -q 'Oracle Linux Server release 6.4' $mntpath/etc/oracle-release ; then
            if `echo $os_built_for | grep -q "32"`; then 
                 OS="OL6U4-32"
            else
                OS="OL6U4-64"
            fi
        elif grep -q 'Oracle Linux Server release 6.5' $mntpath/etc/oracle-release ; then
            if `echo $os_built_for | grep -q "32"`; then 
                 OS="OL6U5-32"
            else
                OS="OL6U5-64"
            fi
	# if OS is greater than 6.5 then using common string for all
        elif is_6u6p $mntpath/etc/oracle-release ; then
		
            if `echo $os_built_for | grep -q "32"`; then 
                 OS="OL6U6p-32"
            else
                OS="OL6U6p-64"
            fi
        elif grep -q 'Oracle Linux Server release 7' $mntpath/etc/oracle-release ; then
		
            if `echo $os_built_for | grep -q "32"`; then 
                OS="OL7-32"
            else
                OS="OL7-64"
            fi
        elif grep -q 'Oracle Linux Server release 8' $mntpath/etc/oracle-release ; then
            OS="OL8-64"
        fi
    elif [ -f $mntpath/etc/redhat-release ]; then
    if grep -q 'release 3' $mntpath/etc/redhat-release; then
                OS="RHEL3-32"
    elif grep -q 'release 4 (Nahant Update 3)' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 4.3 (Final)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL4U3-32"
            else
                OS="RHEL4U3-64"
            fi
    elif grep -q 'release 4 (Nahant Update 4)' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 4.4 (Final)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL4U4-32"
            else
                OS="RHEL4U4-64" 
            fi
    elif grep -q 'Enterprise Linux Enterprise Linux AS release 4 (October Update 4)' $mntpath/etc/redhat-release ; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="ELORCL4U4-32"
            #else
            #    OS="ELORCL4U4-64"
            fi
    elif grep -q 'release 4 (Nahant Update 5)' $mntpath/etc/redhat-release || \
            grep -q 'CentOS release 4.5 (Final)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL4U5-32"
            else
                OS="RHEL4U5-64"      
            fi

    elif grep -q 'release 4 (Nahant Update 6)' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 4.6 (Final)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL4U6-32"
            else
                OS="RHEL4U6-64"
            fi

    elif grep -q 'release 4 (Nahant Update 7)' $mntpath/etc/redhat-release || \
         grep -q 'release 4 (October Update 7)' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 4.7 (Final)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL4U7-32"
            else
                OS="RHEL4U7-64"
            fi
    elif grep -q 'release 4 (Nahant Update 8)' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 4.8 (Final)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL4U8-32"
            else
                OS="RHEL4U8-64"
            fi
    elif grep -q 'release 4 (Nahant Update 9)' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 4.9 (Final)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL4U9-32"
            else
                OS="RHEL4U9-64"
            fi
    elif grep -q 'release 5 (Tikanga)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL5-32"
            else
                OS="RHEL5-64"
            fi
    elif grep -q 'release 5.1 (Tikanga)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL5U1-32"
            else
                OS="RHEL5U1-64"
            fi
    elif grep -q 'CentOS release 5 (Final)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL5-32"
            else
                OS="RHEL5-64"
            fi
    elif grep -q 'release 5.1' $mntpath/etc/redhat-release || \
            grep -q 'CentOS release 5.1' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL5U2-32"
            else
                OS="RHEL5U2-64"
            fi
    elif grep -q 'release 5.2' $mntpath/etc/redhat-release || \
            grep -q 'CentOS release 5.2' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL5U2-32"
            else
                OS="RHEL5U2-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 5.3' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 5.3' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL5U3-32"
            else
                OS="RHEL5U3-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 5.4' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 5.4' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL5U4-32"
            else
                OS="RHEL5U4-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 5.5' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 5.5' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL5U5-32"
            else
                OS="RHEL5U5-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 5.6' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 5.6' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL5U6-32"
            else
                OS="RHEL5U6-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 5.7' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 5.7' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL5U7-32"
            else
                OS="RHEL5U7-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 5.8' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 5.8' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL5U8-32"
            else
                OS="RHEL5U8-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 5.9' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 5.9' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL5U9-32"
            else
                OS="RHEL5U9-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 5.10' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 5.10' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL5U10-32"
            else
                OS="RHEL5U10-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 6.0' $mntpath/etc/redhat-release || \
         grep -q 'CentOS Linux release 6.0' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 6.0' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL6-32"
            else
                OS="RHEL6-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 6.1' $mntpath/etc/redhat-release || \
         grep -q 'CentOS Linux release 6.1' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 6.1' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL6U1-32"
            else
                OS="RHEL6U1-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 6.2' $mntpath/etc/redhat-release || \
         grep -q 'CentOS Linux release 6.2' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 6.2' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL6U2-32"
            else
                OS="RHEL6U2-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 6.3' $mntpath/etc/redhat-release || \
         grep -q 'CentOS Linux release 6.3' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 6.3' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL6U3-32"
            else
                OS="RHEL6U3-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 6.4' $mntpath/etc/redhat-release || \
         grep -q 'CentOS Linux release 6.4' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 6.4' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL6U4-32"
            else
                OS="RHEL6U4-64"
            fi
    elif grep -q 'Red Hat Enterprise Linux Server release 6.5' $mntpath/etc/redhat-release || \
         grep -q 'CentOS Linux release 6.5' $mntpath/etc/redhat-release || \
         grep -q 'CentOS release 6.5' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL6U5-32"
            else
                OS="RHEL6U5-64"
            fi
    elif is_6u6p $mntpath/etc/redhat-release ; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL6U6p-32"
            else
                OS="RHEL6U6p-64"
            fi
	elif grep -q 'Red Hat Enterprise Linux Server release 7.* (Maipo)' $mntpath/etc/redhat-release || \
         grep -q 'CentOS Linux release 7.* (Core)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL7-32"
            else
                OS="RHEL7-64"
            fi
        elif grep -q 'Red Hat Enterprise Linux release 8.* (Ootpa)' $mntpath/etc/redhat-release || \
         grep -q 'CentOS Linux release 8.* (Core)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="RHEL8-32"
            else
                OS="RHEL8-64"
            fi
    elif grep -q 'XenServer release 4.1.0-7843p (xenenterprise)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="CITRIX-XEN-4.1.0-7843p-32"
            else
                OS="CITRIX-XEN-4.1.0-7843p-64"
            fi
    elif grep -q 'XenServer release 5.0.0-10918p (xenenterprise)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="CITRIX-XEN-5.0.0-10918p-32"
            else
                OS="CITRIX-XEN-5.0.0-10918p-64"
            fi
    elif grep -q 'XenServer release 4.0.1-4249p (xenenterprise)' $mntpath/etc/redhat-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="CITRIX-XEN-4.0.1-4249p-32"
            else
                OS="CITRIX-XEN-4.0.1-4249p-64"
            fi
    elif grep -q 'XenServer release 5.5.0-15119p (xenenterprise)' $mntpath/etc/redhat-release || \
         grep -q 'XenServer SDK release 5.5.0-15119p (xenenterprise)' $mntpath/etc/redhat-release ; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="CITRIX-XEN-5.5.0-15119p-32"
            else
                OS="CITRIX-XEN-5.5.0-15119p-64"
            fi
    elif grep -q 'XenServer release 5.5.0-25727p (xenenterprise)' $mntpath/etc/redhat-release || \
         grep -q 'XenServer SDK release 5.5.0-25727p (xenenterprise)' $mntpath/etc/redhat-release ; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="CITRIX-XEN-5.5.0-25727p-32"
            else
                OS="CITRIX-XEN-5.5.0-25727p-64"
            fi

    elif grep -q 'XenServer release 5.6.0-31188p (xenenterprise)' $mntpath/etc/redhat-release || \
         grep -q 'XenServer SDK release 5.6.0-31188p (xenenterprise)' $mntpath/etc/redhat-release ; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="CITRIX-XEN-5.6.0-31188p-32"
            else
                OS="CITRIX-XEN-5.6.0-31188p-64"
            fi
    fi
    elif [ -f $mntpath/etc/SuSE-release ] && grep -q 'VERSION = 10' $mntpath/etc/SuSE-release && ! grep -q PATCHLEVEL $mntpath/etc/SuSE-release; then
            # This one is for SLES10 or OpenSuSE10 series
            if grep -q "SUSE LINUX 10.0" $mntpath/etc/SuSE-release; then
                if `echo $os_built_for | grep -q "32"`; then
                    OS="OPENSUSE10-32"
                else
                    OS="OPENSUSE10-64"
                fi
            fi
    elif grep -q "SUSE Linux Enterprise Server 10" $mntpath/etc/SuSE-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="SLES10-32"
            else
                OS="SLES10-64"
            fi
    elif [ -f $mntpath/etc/SuSE-release ] && grep -q 'VERSION = 10' $mntpath/etc/SuSE-release && \
        grep -q "SUSE Linux Enterprise Server 10" $mntpath/etc/SuSE-release && grep -q 'PATCHLEVEL = 1' $mntpath/etc/SuSE-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="SLES10-SP1-32"
            else
                OS="SLES10-SP1-64"
            fi
    elif [ -f $mntpath/etc/SuSE-release ] && grep -q 'VERSION = 10' $mntpath/etc/SuSE-release && \
        grep -q "SUSE Linux Enterprise Server 10" $mntpath/etc/SuSE-release && grep -q 'PATCHLEVEL = 2' $mntpath/etc/SuSE-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="SLES10-SP2-32"
            else
                OS="SLES10-SP2-64"
            fi
    elif [ -f $mntpath/etc/SuSE-release ] && grep -q 'VERSION = 10' $mntpath/etc/SuSE-release && \
        grep -q "SUSE Linux Enterprise Server 10" $mntpath/etc/SuSE-release && grep -q 'PATCHLEVEL = 3' $mntpath/etc/SuSE-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="SLES10-SP3-32"
            else
                OS="SLES10-SP3-64"
            fi
    elif [ -f $mntpath/etc/SuSE-release ] && grep -q 'VERSION = 10' $mntpath/etc/SuSE-release && \
        grep -q "SUSE Linux Enterprise Server 10" $mntpath/etc/SuSE-release && grep -q 'PATCHLEVEL = 4' $mntpath/etc/SuSE-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="SLES10-SP4-32"
            else
                OS="SLES10-SP4-64"
            fi
    elif [ -f $mntpath/etc/SuSE-release ] && grep -q 'VERSION = 10' $mntpath/etc/SuSE-release && \
        grep -q 'PATCHLEVEL = 1' $mntpath/etc/SuSE-release; then
        # This one is for SLES10 SP1 or OpenSuSE10 SP1 series
        if grep -q "SUSE LINUX 10.0" $mntpath/etc/SuSE-release; then
            if `echo $os_built_for | grep -q "32"`; then
                OS="OPENSUSE10-SP1-32"
            else
                OS="OPENSUSE10-SP1-64"
            fi
        fi
    elif [ -f $mntpath/etc/SuSE-release ] && grep -q 'VERSION = 11' $mntpath/etc/SuSE-release && \
        grep -q "SUSE Linux Enterprise Server 11" $mntpath/etc/SuSE-release && \
        grep -q 'PATCHLEVEL = 0' $mntpath/etc/SuSE-release; then
            # This one is for SLES11 Base
            if grep -q "SUSE Linux Enterprise Server 11" $mntpath/etc/SuSE-release; then
                if `echo $os_built_for | grep -q "32"`; then
                    OS="SLES11-32"
                else
                    OS="SLES11-64"
                fi
            fi
    elif [ -f $mntpath/etc/SuSE-release ] && grep -q 'VERSION = 11' $mntpath/etc/SuSE-release && \
        grep -q "SUSE Linux Enterprise Server 11" $mntpath/etc/SuSE-release && \
        grep -q 'PATCHLEVEL = 1' $mntpath/etc/SuSE-release; then
            # This one is for SLES11 Base with SP1
            if grep -q "SUSE Linux Enterprise Server 11" $mntpath/etc/SuSE-release; then
                if `echo $os_built_for | grep -q "32"`; then
                    OS="SLES11-SP1-32"
                else
                    OS="SLES11-SP1-64"
                fi
            fi
    elif [ -f $mntpath/etc/SuSE-release ] && grep -q 'VERSION = 11' $mntpath/etc/SuSE-release && \
        grep -q "SUSE Linux Enterprise Server 11" $mntpath/etc/SuSE-release && \
        grep -q 'PATCHLEVEL = 2' $mntpath/etc/SuSE-release; then
            # This one is for SLES11 Base with SP2
            if grep -q "SUSE Linux Enterprise Server 11" $mntpath/etc/SuSE-release; then
                if `echo $os_built_for | grep -q "32"`; then
                    OS="SLES11-SP2-32"
                else
                    OS="SLES11-SP2-64"
                fi
            fi
    elif [ -f $mntpath/etc/SuSE-release ] && grep -q 'VERSION = 11' $mntpath/etc/SuSE-release && \
        grep -q "SUSE Linux Enterprise Server 11" $mntpath/etc/SuSE-release && \
        grep -q 'PATCHLEVEL = 3' $mntpath/etc/SuSE-release; then
            # This one is for SLES11 Base with SP3
            if grep -q "SUSE Linux Enterprise Server 11" $mntpath/etc/SuSE-release; then
                if `echo $os_built_for | grep -q "32"`; then
                    OS="SLES11-SP3-32"
                else
                    OS="SLES11-SP3-64"
                fi
            fi
	elif [ -f $mntpath/etc/SuSE-release ] && grep -q 'VERSION = 11' $mntpath/etc/SuSE-release && \
        grep -q "SUSE Linux Enterprise Server 11" $mntpath/etc/SuSE-release && \
        grep -q 'PATCHLEVEL = 4' $mntpath/etc/SuSE-release; then
            # This one is for SLES11 Base with SP4
            if grep -q "SUSE Linux Enterprise Server 11" $mntpath/etc/SuSE-release; then
                OS="SLES11-SP4-64"
            fi
    elif [ -f $mntpath/etc/SuSE-release ] && grep -q 'VERSION = 12' $mntpath/etc/SuSE-release; then
            # This one is for SLES12
            if grep -q "SUSE Linux Enterprise Server 12" $mntpath/etc/SuSE-release; then
                OS="SLES12-64"
            fi			
    elif [ -f $mntpath/etc/os-release ] && grep -q 'SLES' $mntpath/etc/os-release && \
            grep -q 'VERSION="15' $mntpath/etc/os-release; then
            # This one is for SLES15
            if grep -q "SUSE Linux Enterprise Server 15" $mntpath/etc/os-release; then
                OS="SLES15-64"
            fi
    elif [ -f $mntpath/etc/SuSE-release ]; then 
            # This one is for SLES9 series (base, SP2, SP3)
            if grep -q 'VERSION = 9' $mntpath/etc/SuSE-release; then
                if ! grep -q PATCHLEVEL $mntpath/etc/SuSE-release; then
                    if `echo $os_built_for | grep -q "32"`; then
                        OS="SLES9-32"
                    else
                        OS="SLES9-64"
                    fi
                elif grep -q 'PATCHLEVEL = 2' $mntpath/etc/SuSE-release; then
                    if `echo $os_built_for | grep -q "32"`; then
                        OS="SLES9-SP2-32"
                    else
                        OS="SLES9-SP2-64"
                    fi
                elif grep -q 'PATCHLEVEL = 3' $mntpath/etc/SuSE-release; then
                    if `echo $os_built_for | grep -q "32"`; then
                        OS="SLES9-SP3-32"
                    else
                        OS="SLES9-SP3-64"
                    fi
                fi
            fi
    elif [ -f $mntpath/etc/lsb-release ]; then
            if grep -q 'Ubuntu 8.04.2' $mntpath/etc/lsb-release || 
               grep -q 'Ubuntu 8.04.3' $mntpath/etc/lsb-release ; then
                    if `echo $os_built_for | grep -q "32"`; then
                       OS="UBUNTU8-32"
                    else
                       OS="UBUNTU8-64"
                    fi
            elif grep -q 'Ubuntu 10.04.1' $mntpath/etc/lsb-release ; then
                    if `echo $os_built_for | grep -q "32"`; then
                        OS="UBUNTU10-32"
                    else
                        OS="UBUNTU10-64"
                    fi
            elif grep -q 'DISTRIB_RELEASE=14.04' $mntpath/etc/lsb-release ; then
                if uname -a | grep -q x86_64; then
                    OS="UBUNTU-14.04-64"
                fi
            elif grep -q 'DISTRIB_RELEASE=16.04' $mntpath/etc/lsb-release ; then
                if uname -a | grep -q x86_64; then
                    OS="UBUNTU-16.04-64"
                fi
            elif grep -q 'DISTRIB_RELEASE=18.04' $mntpath/etc/lsb-release ; then
                if uname -a | grep -q x86_64; then
                    OS="UBUNTU-18.04-64"
                fi
            elif grep -q 'DISTRIB_RELEASE=20.04' $mntpath/etc/lsb-release ; then
                if uname -a | grep -q x86_64; then
                    OS="UBUNTU-20.04-64"
                fi
            elif grep -q 'DISTRIB_RELEASE=22.04' $mntpath/etc/lsb-release ; then
                if uname -a | grep -q x86_64; then
                    OS="UBUNTU-22.04-64"
                fi
            fi
    elif [ -f $mntpath/etc/debian_version ]; then
            if grep -q '4.0' $mntpath/etc/debian_version; then
                if `echo $os_built_for | grep -q "32"`; then
                    OS="DEBIAN-ETCH-32"
                else
                    OS="DEBIAN-ETCH-64"
                fi
            fi
    elif [ -f $mntpath/etc/gentoo-release ]; then
            if grep -q 'Gentoo Base System release 1.12.13' $mntpath/etc/gentoo-release; then
                if `echo $os_built_for | grep -q "32"`; then
                    OS="GENTOO-32"
                fi
            fi
    fi
    ;;
 #HP-UX)
#
#        # Itanium is identified by 'ia64' in the output of 'uname -a'
#    if [ `uname -r` = "B.11.11" ] && [ `uname -m` != "ia64" ]; then
#        OS="HP-UX-PA-RISC"
#    elif [ `uname -r` = "B.11.23" ] && [ `uname -m` = "ia64" ]; then
#        OS="HP-UX-Itanium-11iv2"
#    elif [ `uname -r` = "B.11.31" ] && [ `uname -m` = "ia64" ]; then
#        OS="HP-UX-Itanium-11iv3"
#    fi
#
#    ;;
#
# AIX)
#    case `uname -v` in
#      5)
#    if [ `uname -r` = "2" ]; then
#        OS="AIX52"
#   elif [ `uname -r` = "3" ]; then
#        OS="AIX53"
#    fi
#    ;;
#      6)
#    if [ `uname -r` = "1" ] ; then
#        OS="AIX61"
#    fi
#    ;;
#    esac
#    ;;
# SunOS)
#    if [ `uname -r` = "5.8" ]; then
#        case `uname -p` in
#            sparc*)
#                if [ `isainfo -b` = "64" ]; then
#                    OS="Solaris-5-8-Sparc"
#                fi
#            ;;
#        esac
#
#        elif [ `uname -r` = "5.9" ]; then
#        case `uname -p` in
#            sparc*)
#                if [ `isainfo -b` = "64" ]; then
#                    OS="Solaris-5-9-Sparc"
#                fi
#            ;;
#        esac
#
#    elif [ `uname -r` = "5.10" ]; then
#         case `uname -p` in
#                        i386|x86*)
#                if [ `isainfo -b` = "64" ]; then          
#                                    OS="Solaris-5-10-x86-64"
#                fi
#                        ;;
#                        sparc*)
#                if [ `isainfo -b` = "64" ]; then
#                                    OS="Solaris-5-10-Sparc"
#                fi
#                        ;;
#                 esac
#
#    elif [ `uname -r` = "5.11" ] && grep 'OpenSolaris' $mntpath/etc/release >/dev/null ; then
#                 case `uname -p` in
#                        i386|x86*)
#                                if [ `isainfo -b` = "64" ]; then
#                                        OS="OpenSolaris-5-11-x86-64"
#                                fi
#                        ;;
#            sparc*)
#                if [ `isainfo -b` = "64" ]; then
#                                    OS="OpenSolaris-5-11-Sparc"
#                fi
#                        ;;
#        esac
#    fi
#    ;;
esac

if [ $# -gt 0 ]
then
    echo $OS
fi
