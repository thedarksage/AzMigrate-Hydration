#!/bin/bash
if [ "$1" = "" ]; then
    src="`pwd`"
else
    src=$1
fi

echo "Source = $src"

dest=$src/sles_drivers
sym=$src/sles_drivers_dbg
branch=`git symbolic-ref --short HEAD`
branch_type=`echo $branch | sed -e 's,.*/\(.*\),\1,'`
supported_kernels_file="/root/supported_kernels_backup/supported_kernels_${branch_type}"
log()
{
    local DATE_TIME=`date '+%m/%d/%Y %H:%M:%S'`
    echo "${DATE_TIME}: $*" | tee -a $dest/drv_build.log
}

greaterOrEqualToVersion()
{
    ver1=`echo $1 | sed 's/[a-z]//g' | tr - .`
    ver2=`echo $2 | sed 's/[a-z]//g' | tr - .`
    for i in {1..5}; do
        val1=`echo $ver1 | cut -f"$i" -d"."`
        val2=`echo $ver2 | cut -f"$i" -d"."`
        if [ $val1 -lt $val2 ]; then
            return 1
        fi
        if [ $val1 -gt $val2 ]; then
            return 0
        fi
    done
    return 0
}

error=0

test -d $dest && rm -rf $dest
mkdir -p $dest
mkdir -p $sym
> $dest/supported_kernels
> $dest/drv_build.log
> $dest/pkg_install.log

cd $src

triggerDriverBuildForOtherSP()
{
    local sp_ver="$1"
    log "Started building ${sp_ver} drivers..."
    local SPBldBoxIP="BLD-SLES15${sp_ver}-64"

    ssh $SPBldBoxIP "rm -f /root/${branch_type}/version.h"
    ssh $SPBldBoxIP "mkdir -p /root/${branch_type}"
    scp ../../../common/version.h $SPBldBoxIP:/root/${branch_type}/
    ssh -f $SPBldBoxIP "rm -rf /BUILDS/${branch_type} && \
                        mkdir -p /BUILDS/${branch_type}/daily_unified_build/source && \
                        cd /BUILDS/${branch_type}/daily_unified_build/source && \
                        git clone msazure@vs-ssh.visualstudio.com:v3/msazure/One/InMage-Azure-SiteRecovery && \
                        cd InMage-Azure-SiteRecovery && \
                        git checkout ${branch} && \
                        cp /root/${branch_type}/version.h /BUILDS/${branch_type}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/common/ && \
                        cd host/drivers/InVolFlt/linux/ && \
                        ./user_space/build/sles.sh && \
                        touch /BUILDS/${branch_type}/${sp_ver}DriverBuildCompleted"

}

copyDriverBuildFromOtherSP()
{
    local sp_ver="$1"
    log "Started copying ${sp_ver} drivers..."
    local SPBldBoxIP="BLD-SLES15${sp_ver}-64"

    scp ${SPBldBoxIP}:/BUILDS/${branch_type}/${sp_ver}DriverBuildCompleted .
    if [ ! -f "${sp_ver}DriverBuildCompleted" ]; then
        log "Sleeping for 10 minutes as the file ${sp_ver}DriverBuildCompleted doesn't exist"
        sleep 600
        scp ${SPBldBoxIP}:/BUILDS/${branch_type}/${sp_ver}DriverBuildCompleted .
    fi

    scp ${SPBldBoxIP}:$dest/drv_build.log drv_build.log.${sp_ver}
    ret=1
    if [ -f "drv_build.log.${sp_ver}" ]; then
        log "`cat drv_build.log.${sp_ver}`"
        grep -q "FAIL" drv_build.log.${sp_ver}
        ret=$?
        rm -f drv_build.log.${sp_ver}
        if [ $ret -eq 0 ]; then
            log "Failed to generate the drivers on $SPBldBoxIP"
            log "Ended building $sp_ver drivers with failures..."
            exit 1
        fi
    fi

    if [ ! -f "${sp_ver}DriverBuildCompleted" ]; then
        log "Build aborted or not completed yet on $SPBldBoxIP"
        log "Ended building $sp_ver drivers with failures..."
        exit 1
    fi

    scp ${SPBldBoxIP}:$dest/supported_kernels $dest/supported_kernels.${sp_ver}
    scp ${SPBldBoxIP}:$dest/involflt.ko.* $dest/
    scp ${SPBldBoxIP}:$sym/involflt.ko.*.dbg.gz $sym/
    cat $dest/supported_kernels.${sp_ver} >> $dest/supported_kernels
    log "Ended copying $sp_ver drivers successfully..."

}

# Build drivers from other SLES15 SP build box
if [ ! -f /etc/SuSE-release ]; then
    patch_level=`grep -o -- -SP[0-9] /etc/os-release | grep -o [0-9]`
    if [ ${patch_level} -eq 1 ]; then
        for ver in {2..4}
        do
            triggerDriverBuildForOtherSP "SP$ver"
        done
    fi
fi

kernel_list_file=/tmp/suse_$$.ker

fetch_latest_headers=1
zypper -n refresh 2>&1 | grep "Permission to access .* denied"
if [ $? -eq 0 ];then
    log "zypper refresh failed, building headers from already installed kernel"
    if [ ! -f $supported_kernels_file ]; then
        log "file : $supported_kernels_file not found, backup file required to fetch sp version"
        exit 1
    fi
    fetch_latest_headers=0
fi

for kpat in azure default; do

    log "Building $kpat"
    if [ $fetch_latest_headers -eq 1 ]; then
        (
            flock -e 200
            zypper refresh
            zypper se -s "kernel-$kpat-devel*" | grep kernel-$kpat-devel > $kernel_list_file
        ) 200>/tmp/driver.lock
        log "`cat $kernel_list_file`"

        if [ -f /etc/SuSE-release ]; then
            sed -i 's/SLES12-Pool/SLES12-SP0-Pool/g' $kernel_list_file
            sed -i 's/SLES12-Updates/SLES12-SP0-Updates/g' $kernel_list_file
        else
            sed -i 's/SLE-Module-Basesystem15-Pool/SP0-Pool/g' $kernel_list_file
            sed -i 's/SLE-Module-Basesystem15-Updates/SP0-Updates/g' $kernel_list_file
            sed -i 's/SLE-Module-Public-Cloud15-Updates/Cloud-SP0-Updates/g' $kernel_list_file
        fi
    fi

    SP12_Base=('3.12.28-4' '3.12.49-11' '4.4.21-69' '4.4.73-5' '4.12.14-94.41' '4.12.14-120')
    SP15_Base=('4.12.14-23' '4.12.14-195' '5.3.18-22' '5.3.18-57' '5.14.21-150400.22')
    Make_Jobs=4
    if [ -f /etc/SuSE-release ]; then
        Make_Jobs=1
        SP_Base=("${SP12_Base[@]}")
    else
        SP_Base=("${SP15_Base[@]}")
    fi
    for sp in "${!SP_Base[@]}"; do
        base_kernel=${SP_Base[$sp]}

        rel=SP$sp
        log "Building for $rel"
        if [ $fetch_latest_headers -eq 1 ]; then
            kernel_list=(`cat $kernel_list_file | grep $rel | tr -s " " | cut -f 7 -d " " | sort| uniq`)
        else
            kernel_list=(`grep $kpat $supported_kernels_file | grep ":$sp" | cut -f1 -d ":" | sed "s/-$kpat/.1/"`)
        fi
        log "`echo "${kernel_list[@]}"`"
        for ver in "${kernel_list[@]}"; do
            log "Building for kernel pkg $ver"
            verbup=$ver
            ver="`echo $ver | sed 's/\.[0-9]*$//'`-$kpat"
            if [ $fetch_latest_headers -eq 0 ]; then
                ver_sp=`grep ${ver} $supported_kernels_file | cut -d ':' -f2`
                if [ ${ver_sp} -ne ${sp} ]; then
                    continue
                fi
            fi
            log "Building for kernel $ver and sp $sp"

            if [ $kpat = "default" -a $ver != "${base_kernel}-default" ]; then
                log "Skipping build for $ver"
                echo "${ver}:${sp}" >> $dest/supported_kernels
                continue
            fi

            if [ $fetch_latest_headers -eq 1 ]; then
                (
                    flock -e 200
                    zypper install --oldpackage -y kernel-$kpat-devel-$verbup >> $dest/pkg_install.log 2>&1
                ) 200>/tmp/driver.lock
            fi

            GCC_VER=""
            # Building kernels >= 4.12.14-16.31-azure in SLES12 with gcc-7
            if [ -f /etc/SuSE-release ] && [ "$kpat" = "azure" ]; then
                greaterOrEqualToVersion $ver "4.12.14-16.31-azure"
                if [ $? -eq 0 ]; then
                    GCC_VER="CC=gcc-7"
                    log "Building $ver with $GCC_VER parameter"
                fi
            fi

            log "Build Dir = $ver"
            make -j $Make_Jobs -f involflt.mak KDIR=/lib/modules/${ver}/build $GCC_VER PATCH_LEVEL=$sp && {
                echo "${ver}:${sp}" >> $dest/supported_kernels
                log "PASS: $ver"
                mv bld_involflt/involflt.ko bld_involflt/involflt.ko.dbg
                strip -g bld_involflt/involflt.ko.dbg -o bld_involflt/involflt.ko
                if [ $kpat = "default" ]; then
                    cp bld_involflt/involflt.ko $dest/involflt.ko.SP${sp}
                    mv bld_involflt/involflt.ko.dbg $sym/involflt.ko.SP${sp}.dbg
                    gzip -f $sym/involflt.ko.SP${sp}.dbg
                else
                    cp bld_involflt/involflt.ko $dest/involflt.ko.$ver
                    mv bld_involflt/involflt.ko.dbg $sym/involflt.ko.$ver.dbg
                    gzip -f $sym/involflt.ko.$ver.dbg
                fi
            } || {
                log "FAIL: $ver"
                error=1
            }
        done
    done
done
rm -f $kernel_list_file

if [ -f "$dest/supported_kernels" ]; then
    uniq $dest/supported_kernels > $dest/supported_kernels.uniq
    mv -f $dest/supported_kernels.uniq $dest/supported_kernels
fi

if [ $error -eq 0 ]; then
    cp -f $dest/supported_kernels $supported_kernels_file
fi

# Copy drivers from other SLES15 SP build box
if [ ! -f /etc/SuSE-release ]; then
    patch_level=`grep -o -- -SP[0-9] /etc/os-release | grep -o [0-9]`
    if [ ${patch_level} -eq 1 ]; then
        for ver in {2..4}
        do
            copyDriverBuildFromOtherSP "SP$ver"
        done
    fi
fi
exit $error
