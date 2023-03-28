#!/bin/bash

scriptsdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
branch_type=$(git symbolic-ref HEAD | sed -e 's,.*/\(.*\),\1,')
src_location=`pwd`
echo "Source = $src_location"

pkgsdir=$src_location/ubuntu_drivers
sym=$src_location/drivers_dbg
supported_kernels_list="$src_location/supported_kernels"

builderrors=0
NR_PARALLEL_BUILDS_ALLOWED=20

# Max supported kernel version
KERNEL_VMAX=5.10
isdebian=1
if [ -f /etc/os-release ] && grep -q 'Ubuntu' /etc/os-release; then
    KERNEL_VMAX=5.15
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
#    if [ "$branch_type" = "develop" ]; then
#        return 0;
#    fi
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
        if [ $debian_version = "7" -o $debian_version = "8" ]; then
            installed_versions=$(ls -1 /usr/src | grep "linux-headers" | grep "amd64" | cut -d"-" -f 3,4,5 | sort -u)
            aptcache_versions=$(sudo apt-cache search linux-headers-* | grep -i "header files for" | grep "common" | awk '{print $1}' | sed 's/linux-headers-//g' | cut -d"-" -f 1,2 | sort -u | sed 's/$/\-amd64/')
        else
            installed_versions=$(ls -1 /usr/src | grep "linux-headers" | grep "amd64" | cut -d"-" -f 3- | sort -u)
            aptcache_versions=$(sudo apt-cache search linux-headers-* | grep -v "meta-package" | grep -i "header files for" | egrep 'cloud-amd64|[0-9]-amd64' | awk '{print $1}' | cut -d"-" -f 3- | sort -u)
        fi
    else
        echo "Building for UBUNTU"
        installed_versions=$(ls -1 /usr/src | grep "linux-headers" | egrep 'generic|azure' | cut -d"-" -f 3,4,5 | sort)
        aptcache_versions=$(sudo apt-cache search linux-headers-* | grep -i "Linux kernel headers for version" | egrep 'generic|azure' | egrep -v 'azure-edge|azure-cvm' | awk '{print $1}' | sed 's/linux-headers-//g' | sort -u)
    fi

    new_versions=$(comm -13 <(echo "$installed_versions") <(echo "$aptcache_versions"))

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
            execute_cmd "sudo dpkg --configure -a" "ignorefail"
            if [ $debian_version = "9" ] && grep -q 'bpo' <<< $i; then
                execute_cmd "sudo apt-get install -y -t stretch-backports linux-headers-$i" "ignorefail"
            elif [ $debian_version = "8" ] || [ $debian_version = "7" ]; then
                execute_cmd "sudo apt-get install -y --force-yes linux-headers-$i" "ignorefail"
            else
                execute_cmd "sudo apt-get install -y linux-headers-$i" "ignorefail"
            fi
            # TODO: check /lib/modules for new version before ensuring
            printf "Successfully installed $i\n"
        fi
    done
}

function build_for_all_kernels
{
    if [[ $isdebian != 0 ]]; then
        if [ $debian_version = "7" -o $debian_version = "8" ]; then
	    installed_versions=$(ls -1 /usr/src | grep "linux-headers" | grep "common" | cut -d"-" -f 3,4 | sort -u | sed 's/$/\-amd64/')	
        else
            installed_versions=$(ls -1 /usr/src | grep "linux-headers" | grep "amd64" | cut -d"-" -f 3- | sort -u)		
        fi
    else
        installed_versions=$(ls -1 /usr/src | grep "linux-headers" | egrep 'generic|azure' | egrep -v 'azure-edge|azure-cvm' | cut -d"-" -f 3,4,5 | sort)
    fi

    printf "\nStarting build for installed versions: $installed_versions\n\n\n"
    
    execute_cmd "mkdir -p $pkgsdir"
    execute_cmd "mkdir -p $sym"

    :>${supported_kernels_list}
    for kerver in $installed_versions; do
        should_skip ${kerver}
        if [ $? != "0" ]; then
            printf "Skipping build for ${kerver}\n"
            continue
        fi

        if test "$(jobs | wc -l)" -ge ${NR_PARALLEL_BUILDS_ALLOWED}; then
            wait -n
        fi

        echo "${kerver}" >> ${supported_kernels_list}
        printf "\nBuilding for kernel $kerver\n"
        {
            # if build fails for a kernel, do not kill, proceed building for other kernels
            # After completing build for all kernels will check and report build failure versions
            BLD_INVOLFLT=bld_involflt_${kerver}
            execute_cmd "make all -f involflt.mak KDIR=/lib/modules/$kerver/build BLD_INVOLFLT=${BLD_INVOLFLT}" "ignorefail"
            execute_cmd "strip -g ${BLD_INVOLFLT}/involflt.ko -o $pkgsdir/involflt.ko.$kerver" "ignorefail"
            execute_cmd "cp ${BLD_INVOLFLT}/involflt.ko $sym/involflt.ko.${kerver}.dbg" "ignorefail"
            execute_cmd "gzip $sym/involflt.ko.${kerver}.dbg" "ignorefail"
            execute_cmd "stat $pkgsdir/involflt.ko.$kerver" "ignorefail"
            execute_cmd "make clean -f involflt.mak KDIR=/lib/modules/$kerver/build BLD_INVOLFLT=${BLD_INVOLFLT}" "ignorefail"
        } &
    done
    wait
}

function validate_binaries_for_pkgmgmt
{
    declare build_failures

    if [[ $isdebian != 0 ]]; then
        if [ $debian_version = "7" -o $debian_version = "8" ]; then
            aptcache_versions=$(sudo apt-cache search linux-headers-* | grep -i "header files for" | grep "common" | awk '{print $1}' | sed 's/linux-headers-//g' | cut -d"-" -f 1,2 | sort -u | sed 's/$/\-amd64/')		
        else
	    aptcache_versions=$(sudo apt-cache search linux-headers-* | grep -v "meta-package" | grep -i "header files for" | egrep 'cloud-amd64|[0-9]-amd64' | awk '{print $1}' | cut -d"-" -f 3- | sort -u)
        fi
    else
        aptcache_versions=$(sudo apt-cache search linux-headers-* | grep -i "Linux kernel headers for version" | egrep 'generic|azure' | egrep -v 'azure-edge|azure-cvm' | awk '{print $1}' | sed 's/linux-headers-//g' | sort -u)
    fi

        for kernver in $aptcache_versions; do
            file="$pkgsdir/involflt.ko.$kernver"
            if [[ ! -e $file ]]; then
                should_skip ${kernver}
                if [ $? -eq "0" ]
                then
                    if [ ! -d "/lib/modules/${kernver}" ]; then
                        nobuild="$nobuild $kernver"
                    else
                        builderrors=1
                        build_failures="$build_failures Not found involflt.ko binary for kernel $kernver\n"
                    fi
                else
                    # Unsupported kernel
                    nobuild="$nobuild $kernver"
                fi
            fi
        done

    printf "\n$build_failures\n"
    printf "\nNot Built: $nobuild\n"
}

function print_help
{
    help="\tUsage:   $script_name <location of source code host folder> 
    \tExample: $script_name /BUILDS/INT_BRANCH/daily_unified_build/source/host/drivers/InVolFlt/linux" 

    printf "$help"
}

# main code starts here
echo "Starting at $(date)"

if [[ ! -d $src_location ]]; then
    printf "Directory $src_location doesn't exist\n\n"
    print_help
    exit 2
fi

execute_cmd "rm -rf $pkgsdir" "ignorefail"

if [ $isdebian = 1 ]; then
    sudo apt-get -o Acquire::Check-Valid-Until=false update
else
    sudo apt-get update
fi

if [[ $1 == "" ]]; then
    check_and_install_new_kern_headers
elif [[ $1 == "build" ]]; then
    build_for_all_kernels
    validate_binaries_for_pkgmgmt
elif [[ $1 == "all" ]]; then
    check_and_install_new_kern_headers
    build_for_all_kernels
    validate_binaries_for_pkgmgmt
fi

echo "Exiting at $(date)"

if [[ $builderrors == 0 ]]; then
    exit 0
else
    exit 1
fi
