#!/bin/bash

if [[ $# -ne 1 ]]; then
    echo "Insufficient number of args"
    exit 1
fi

mobility_version=$1

workingdir=$PWD

execute_cmd()
{
    cmd="$1"
    dont_exit_on_fail=0

    if [[ -n "$2" ]]; then
        dont_exit_on_fail=1
    fi

    echo "Executing command: $cmd"
    #op=$($cmd)
    eval $cmd
    funret=$?
    if [[ $funret != 0 ]]; then
        printf 'command: %s failed with return value %d\n' "$cmd" "$funret"
        if [[ $dont_exit_on_fail == 0 ]]; then
            exit 1;
        fi
    fi
}

echo "Checking platform distro"

centosplus=0
if [ -f /etc/enterprise-release ] && [ -f /etc/redhat-release ] ; then
	if grep -q 'Enterprise Linux Enterprise Linux Server release 5.* (Carthage)' /etc/enterprise-release; then 
		platform_name="OL5-64"
        fi
elif [ -f /etc/oracle-release ] && [ -f /etc/redhat-release ] ; then
	if grep -q 'Oracle Linux Server release 6.*' /etc/oracle-release; then
	    platform_name="OL6-64"
	else
	    echo "Not found platform info"
	    exit 1
	fi
elif [ -f /etc/redhat-release ]; then
	if grep -q 'Red Hat Enterprise Linux Server release 6.* (Santiago)' /etc/redhat-release || \
		grep -q 'CentOS Linux release 6.* (Final)' /etc/redhat-release || \
		grep -q 'CentOS release 6.* (Final)' /etc/redhat-release; then
		platform_name="RHEL6-64"
	elif grep -q 'Red Hat Enterprise Linux Server release 7.* (Maipo)' /etc/redhat-release || \
		grep -q 'CentOS Linux release 7.* (Core)' /etc/redhat-release; then
		platform_name="RHEL7-64"
	elif grep -q 'CentOS release 5.11 (Final)' /etc/redhat-release; then
		platform_name="RHEL5-64"
	else
        echo "Not found platform info"
        exit 1
	fi
elif [ -f /etc/SuSE-release ] && grep -q 'VERSION = 11' /etc/SuSE-release && grep -q "SUSE Linux Enterprise Server 11" /etc/SuSE-release && grep -q 'PATCHLEVEL = 3' /etc/SuSE-release; then
	platform_name="SLES11-SP3-64"
elif [ -f /etc/SuSE-release ] && grep -q 'VERSION = 11' /etc/SuSE-release && grep -q "SUSE Linux Enterprise Server 11" /etc/SuSE-release && grep -q 'PATCHLEVEL = 4' /etc/SuSE-release; then
	platform_name="SLES11-SP4-64"	
elif [ -f /etc/os-release ] && grep -q 'Ubuntu 14.04' /etc/os-release; then
    platform_name="UBUNTU-14.04-64"
elif [ -f /etc/os-release ] && grep -q 'Ubuntu 16.04' /etc/os-release; then
    platform_name="UBUNTU-16.04-64"
elif [ -f /etc/os-release ] && grep -q 'Debian GNU/Linux 8 (jessie)' /etc/os-release; then
    platform_name="DEBIAN8-64"
elif [ -f /etc/os-release ] && grep -q 'Debian GNU/Linux 7 (wheezy)' /etc/os-release; then
    platform_name="DEBIAN7-64"
else
    echo "Not found platform info"
    exit 1
fi

echo "Platform: $platform_name"

#Add IP to /etc/hosts, install cmd expects it
if [ -f /etc/redhat-release ] && 		\
    grep -q 'CentOS Linux release 7.* (Core)' /etc/redhat-release; then
	IPs=$(ifconfig | grep "inet" | grep "broadcast" | sed -e 's/^[ \t]*//' | cut -d" " -f 2)
	loopback=$(ifconfig | grep "inet" | grep "127.0.0" | sed -e 's/^[ \t]*//' | cut -d" " -f 2)
else
	IPs=$(ifconfig | grep "inet addr" | grep "Bcast" | cut -d":" -f 2 | cut -d" " -f 1)
	loopback=$(ifconfig | grep "inet addr" | cut -d":" -f 2 | cut -d" " -f 1 | grep "127.0.0")
fi
machinename=$(hostname)
longmachinename=$machinename.fareast.corp.microsoft.com

printf "$loopback localhost\n" > /etc/hosts
for ip in $IPs; do
    echo "Adding IP to /etc/hosts: $ip"
    printf "$ip $longmachinename $machinename\n" >> /etc/hosts
done

execute_cmd "cat /etc/hosts"

lsmodop=$(lsmod | grep "involflt")
if [[ ! -z "${lsmodop// }" ]]; then
    execute_cmd "rmmod involflt" "ignorefail"
    lsmodop=$(lsmod | grep "involflt")
    if [[ ! -z "${lsmodop// }" ]]; then
        echo "involflt driver already loaded on this machine, unable to proceed, exiting"
        exit 1
    fi
fi
execute_cmd "rm -rf /dev/involflt" "ignorefail"

#mntpath="/root/dailybuilds_mnt"
mntpath="/DailyBuilds/Daily_Builds"

#pkg path example:
#//10.150.24.60/builds/Daily_Builds/9.8/HOST/27_Feb_2017/UnifiedAgent_Builds/release/Microsoft-ASR_UA_9.8.0.0_RHEL6-64_DIT_10Feb2017_release.tar.gz
# or
#//10.150.24.60/builds/Daily_Builds/9.8/HOST/27_Feb_2017/UnifiedAgent_Builds/release/Microsoft-ASR_UA_9.8.0.0_RHEL6-64_GA_10Feb2017_release.tar.gz
# or
# sshpass -p Password~1 scp guestuser@10.150.24.66:/DailyBuilds/Daily_Builds/9.10/HOST/30_May_2017/UnifiedAgent_Builds/release/Microsoft-ASR_UA_9.10.0.0_RHEL6-64_* .

installdump_dir="$workingdir/workarea"
#dailybuilds_repo="//10.150.24.60/builds/Daily_Builds"
dailybuild_date=$(date +%d_%b_%Y)

pkgname="Microsoft-ASR_UA_$mobility_version.0.0_$platform_name*"
pkgpath="$mntpath/$mobility_version/HOST/$dailybuild_date/UnifiedAgent_Builds/release"

#execute_cmd "mkdir -p $mntpath" "ignorefail"

#if [[ ! -e $pkgpath ]]; then
#    mntcmd="mount -t cifs $dailybuilds_repo $mntpath -o username=administrator,password=Password~1"
#    execute_cmd "$mntcmd"
#fi

#pkg=`cd $pkgpath && ls $pkgname`

execute_cmd "rm -rf $installdump_dir" "ignorefail"
execute_cmd "mkdir $installdump_dir"
#execute_cmd "cp $pkgpath/$pkg $installdump_dir"
execute_cmd "sshpass -p Password~1 scp -o StrictHostKeyChecking=no guestuser@10.150.24.66:$pkgpath/$pkgname $installdump_dir"
pkg=`cd $installdump_dir && ls $pkgname`
execute_cmd "cd $installdump_dir && tar -xovf $installdump_dir/$pkg"
execute_cmd "cd $installdump_dir && ./install -q -d /usr/local/ASR -r MS -v VmWare"
execute_cmd "cd $installdump_dir && /usr/local/ASR/Vx/bin/UnifiedAgentConfigurator.sh -i 10.150.3.201 -P $workingdir/passpharse" "ignorefail"
execute_cmd "cd $workingdir"

inm_dmit_path="/usr/local/ASR/Vx/bin/inm_dmit"

execute_cmd "ls $inm_dmit_path"

lsmodop=$(lsmod | grep "involflt")
if [[ -z "${lsmodop// }" ]]; then
    echo "Failed to load involflt driver"
    exit 1
fi

execute_cmd "$inm_dmit_path --set_attr VacpIObarrierTimeout 60000"
execute_cmd "$inm_dmit_path --get_attr VacpIObarrierTimeout"
execute_cmd "$inm_dmit_path --set_attr DirtyBlockHighWaterMarkServiceRunning 30000"
execute_cmd "$inm_dmit_path --get_attr DirtyBlockHighWaterMarkServiceRunning"

echo  "CVT setup completed"

