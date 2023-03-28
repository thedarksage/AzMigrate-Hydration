#!/bin/bash
if [ "$1" = "" ]; then
    src="`pwd`"
else
    src=$1
fi

echo "Source = $src"

dest=$src/sles_drivers
sym=$src/sles_drivers_dbg

if [ -f /etc/SuSE-release ]; then
    distro=sles12
else
    distro=sles15
fi

supported_kernels_file="/root/supported_kernels"
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

fetch_latest_headers=1

for kpat in azure default; do

    log "Building $kpat"

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
        kernel_list=(`grep $kpat $supported_kernels_file | grep ":$sp" | cut -f1 -d ":" | sed "s/-$kpat/.1/"`)
        
        log "`echo "${kernel_list[@]}"`"
        for ver in "${kernel_list[@]}"; do
            log "Building for kernel pkg $ver"
            ver="`echo $ver | sed 's/\.[0-9]*$//'`-$kpat"
            ver_sp=`grep ${ver} $supported_kernels_file | cut -d ':' -f2`
            if [ ${ver_sp} -ne ${sp} ]; then
                continue
            fi
            log "Building for kernel $ver and sp $sp"
			
			if [ $kpat = "default" -a $ver != "${base_kernel}-default" ]; then
                log "Skipping build for $ver"
                echo "${ver}:${sp}" >> $dest/supported_kernels
                continue
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

if [ -f "$dest/supported_kernels" ]; then
    uniq $dest/supported_kernels > $dest/supported_kernels.uniq
    mv -f $dest/supported_kernels.uniq $dest/supported_kernels
fi
exit $error
