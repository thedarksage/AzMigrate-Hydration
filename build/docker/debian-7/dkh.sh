#!/bin/bash -x

# Max supported kernel version
KERNEL_VMAX=5.8
isdebian=1
if [ -f /etc/os-release ] && grep -q 'Ubuntu' /etc/os-release; then
    KERNEL_VMAX=5.4
    isdebian=0
else
    debian_version=`cat /etc/debian_version | cut -f1 -d "."`
fi

MAJOR_KERNEL_VMAX=`echo $KERNEL_VMAX | cut -f1 -d "."`
kmax="`echo $KERNEL_VMAX | cut -f 1-2 -d "." | sed "s/\.//g"`"

execute_cmd()
{
    cmd="$1"
    dont_exit_on_fail=0

    if [[ -n "$2" ]]; then
        dont_exit_on_fail=1
    fi

    printf "Executing command: $cmd\n"
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

should_skip()
{
    kernel_ver=$1

    major_ver=`echo ${kernel_ver} | cut -f1 -d "."`
    if [ ${major_ver} -gt ${MAJOR_KERNEL_VMAX} ]; then
        return 1
    fi

    if [ ${major_ver} -lt ${MAJOR_KERNEL_VMAX} ]; then
        return 0
    fi

    kver="`echo ${kernel_ver} | cut -f 1 -d "-" | cut -f 1-2 -d "." | sed "s/\.//g"`"
    if [ $kver -le $kmax ]; then
        return 0
    fi

    return 1
}

function check_and_install_new_kern_headers
{
    if [[ $isdebian != 0 ]]; then
        echo "Building for DEBIAN"
        if [ $debian_version = "9" -o $debian_version = "10" ]; then
            installed_versions=$(ls -1 /usr/src | grep "linux-headers" | grep "amd64" | cut -d"-" -f 3- | sort -u)
            aptcache_versions=$(sudo apt-cache search linux-headers-* | grep -v "meta-package" | grep -i "header files for" | egrep 'cloud-amd64|[0-9]-amd64' | awk '{print $1}' | cut -d"-" -f 3- | sort -u)
        else
            installed_versions=$(ls -1 /usr/src | grep "linux-headers" | grep "amd64" | cut -d"-" -f 3,4,5 | sort -u)
            aptcache_versions=$(sudo apt-cache search linux-headers-* | grep -i "header files for" | grep "common" | awk '{print $1}' | sed 's/linux-headers-//g' | cut -d"-" -f 1,2 | sort -u | sed 's/$/\-amd64/')
        fi
    else
        echo "Building for UBUNTU"
        installed_versions=$(ls -1 /usr/src | grep "linux-headers" | egrep 'generic|azure' | cut -d"-" -f 3,4,5 | sort)
        aptcache_versions=$(sudo apt-cache search linux-headers-* | grep -i "Linux kernel headers for version" | egrep 'generic|azure' | grep -v "azure-edge" | awk '{print $1}' | sed 's/linux-headers-//g' | sort -u)
    fi

    new_versions=$(comm -13 <(echo "$installed_versions") <(echo "$aptcache_versions"))
    dpkg_cmd="sudo dpkg --configure -a"

    printf "\nInstalled versions: $installed_versions\n\n\n"
    printf "\nAvailable versions: $aptcache_versions\n\n\n"
    printf "\nNew versions: $new_versions\n\n\n"

    if [[ -z "${new_versions// }" ]]; then
        printf "Not found new kernel versions to install\n"
        return 0
    fi

    printf "\nFound new kernel version(s):\n$new_versions\n"

    for i in $new_versions; do
        should_skip $i

        if [ $? != "0" ]
        then
            printf "Skipping $i\n"
        else
            execute_cmd "sudo dpkg --configure -a"
            if [ $debian_version = "9" ] && grep -q 'bpo' <<< $i; then
                execute_cmd "sudo apt-get install -y -t stretch-backports linux-headers-$i" "ignorefail"
            else
                execute_cmd "sudo apt-get install -y --force-yes linux-headers-$i" "ignorefail"
            fi
            # TODO: check /lib/modules for new version before ensuring
            printf "Successfully installed $i\n"
        fi
    done
}

# main code starts here
echo "Starting at $(date)"
sudo apt-get -o Acquire::Check-Valid-Until=false update
check_and_install_new_kern_headers
echo "Exiting at $(date)"
