#!/bin/bash
# Downloads SLES12/15 delta kernel headers and creates zip file with kernel headers, kernel headers list.
if [ "$1" != "" ]; then
    kernelheadersfile=$1
else
	kernelheadersfile=""
fi

src="`pwd`"
dest=$src/sles_drivers
target_dest=$src/dkh
kernelheadersfile_base=`basename ${kernelheadersfile}`
DATE_FOLDER=`date +%d_%b_%Y | awk '{print $1}'`
SCP_SERVER="10.150.24.13"
FTP_PATH="/DailyBuilds/Daily_Builds/SLESKernelHeaders"

log()
{
    local DATE_TIME=`date '+%m/%d/%Y %H:%M:%S'`
    echo "${DATE_TIME}: $*" | tee -a $dest/kernelheadersdownload.log
}

error=0

test -d $dest && rm -rf $dest
mkdir -p $dest/kernelheaders/
> $dest/supported_kernels
> $dest/kernelheadersdownload.log

cd $src

if [ -z $kernelheadersfile ] || [ ! -f "$kernelheadersfile" ]; then
   echo "Kernel headers file not available. Please pass kernel headers file to download delta kernel headers."
   exit 1
fi

MakeDir_Remote()
{
    remotedirpath=$1
    # retry the remote command for 5 mins
    starttime=`date +%s`
    while :
    do
        ssh $SCP_SERVER "mkdir -p $remotedirpath"
        if [ $? -eq 0 ]; then
            break
        fi

        currtime=`date +%s`
        elapsed=`expr $currtime - $starttime`
        if [ $elapsed -gt 300 ]; then
            echo "Retries of mkdir failed"
            return 1
        fi

        sleep 5
        echo "retrying mkdir command"
    done
}

Copy_To_Remote()
{
    localfilespath=$1
    remotedirpath=$2

    for localfilepath in `ls $localfilespath`
    do
        # retry the remote command for 5 mins
        starttime=`date +%s`
        while :
        do
            scp $localfilepath $SCP_SERVER:$remotedirpath
            if [ $? -eq 0 ]; then
                break
            fi

            currtime=`date +%s`
            elapsed=`expr $currtime - $starttime`
            if [ $elapsed -gt 300 ]; then
                echo "Retries of scp failed"
                return 1
            fi

            sleep 5
            echo "retrying scp command"
        done
    done
}

downloadKernelHeadersForOtherSP()
{
    local sp_ver="$1"
    log "Started downloading ${sp_ver} kernel headers..."
    local SPBldBoxIP="BLD-SLES15${sp_ver}-64"

    ssh $SPBldBoxIP "rm -rf $target_dest"
    ssh $SPBldBoxIP "mkdir -p $target_dest"
    scp host/drivers/InVolFlt/linux/user_space/build/sles_dkh.sh $SPBldBoxIP:$target_dest
	scp $kernelheadersfile $SPBldBoxIP:$target_dest
    ssh -f $SPBldBoxIP "cd $target_dest && \
			./sles_dkh.sh $kernelheadersfile_base && \
			touch ${sp_ver}KernelHeaderDownloadCompleted"
}

copyKernelHeadersFromOtherSP()
{
    local sp_ver="$1"
    log "Started copying ${sp_ver} kernel headers..."
    local SPBldBoxIP="BLD-SLES15${sp_ver}-64"

    scp ${SPBldBoxIP}:$target_dest/${sp_ver}KernelHeaderDownloadCompleted $dest
    if [ ! -f "$dest/${sp_ver}KernelHeaderDownloadCompleted" ]; then
		log "Sleeping for 10 minutes as the file ${sp_ver}KernelHeaderDownloadCompleted doesn't exist"
        sleep 600
        scp ${SPBldBoxIP}:$target_dest/${sp_ver}KernelHeaderDownloadCompleted $dest
    fi

    scp ${SPBldBoxIP}:$target_dest/sles_drivers/kernelheadersdownload.log $dest/kernelheadersdownload.log.${sp_ver}
    if [ -f "$dest/kernelheadersdownload.log.${sp_ver}" ]; then
        log "`cat $dest/kernelheadersdownload.log.${sp_ver}`"
        if grep -q "FAIL" $dest/kernelheadersdownload.log.${sp_ver} || \
		grep -q "no package" $dest/kernelheadersdownload.log.${sp_ver}; then
			log "Failed to download kernel headers on $SPBldBoxIP"
			exit 1
        fi
    fi
    
    if [ ! -f "$dest/${sp_ver}KernelHeaderDownloadCompleted" ]; then
		log "Download aborted or not completed yet on $SPBldBoxIP"
        exit 1
    fi

    scp ${SPBldBoxIP}:$target_dest/sles_drivers/kernelheaders/supported_kernels $dest/supported_kernels.${sp_ver}
    scp ${SPBldBoxIP}:$target_dest/sles_drivers/kernelheaders/*.rpm $dest/kernelheaders
    cat $dest/supported_kernels.${sp_ver} >> $dest/kernelheaders/supported_kernels
    log "Ended copying $sp_ver kernel headers successfully..."
}

# Download kernel headers from other SLES15 SP machines
if [ ! -f /etc/SuSE-release ]; then
    patch_level=`grep -o -- -SP[0-9] /etc/os-release | grep -o [0-9]`
    if [ ${patch_level} -eq 1 ]; then
		for ver in {2..4}
        do
			downloadKernelHeadersForOtherSP "SP$ver"
        done
    fi
fi

kernel_list_file=/tmp/suse_$$.ker

zypper -n refresh 2>&1 | grep "Permission to access .* denied"
if [ $? -eq 0 ];then
    log "zypper refresh failed. FAILED to download kernel headers."
    exit 1
fi

for kpat in azure default; do
    log "Downloading $kpat kernel headers"
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

    SP12_Base=('3.12.28-4' '3.12.49-11' '4.4.21-69' '4.4.73-5' '4.12.14-94.41' '4.12.14-120')
    SP15_Base=('4.12.14-23' '4.12.14-195' '5.3.18-22' '5.3.18-57' '5.14.21-150400.22')
	
    if [ -f /etc/SuSE-release ]; then
        SP_Base=("${SP12_Base[@]}")
    else
        SP_Base=("${SP15_Base[@]}")
    fi
    for sp in "${!SP_Base[@]}"; do
        base_kernel=${SP_Base[$sp]}
        rel=SP$sp
        log "Downloading for $rel"
        kernel_list=(`cat $kernel_list_file | grep $rel | tr -s " " | cut -f 7 -d " " | sort| uniq`)
        
		log "`echo "${kernel_list[@]}"`"
        for ver in "${kernel_list[@]}"; do
            verbup="`echo $ver | sed 's/\.[0-9]*$//'`"
            ver="`echo $ver | sed 's/\.[0-9]*$//'`-$kpat"
            log "Downloading for kernel $ver and sp $sp"
			
			if [ -f $kernelheadersfile ]; then
				echo "File : $kernelheadersfile found. Downloading delta kernel headers."
				ver_sp=`grep ${ver} $kernelheadersfile | cut -d ':' -f1`
				if [ ${ver_sp} = ${ver} ]; then
					echo "Skipping download - ${ver_sp}"
					continue
				fi
			fi
			
			if [ $kpat = "default" -a $ver != "${base_kernel}-default" ]; then
                log "Skipping download for $ver"
                echo "${ver}:${sp}" >> $dest/supported_kernels
                continue
            fi

        (
            echo "${ver}:${sp}" >> $dest/supported_kernels
			flock -e 200
            zypper --pkg-cache-dir $dest download kernel-$kpat-devel-$verbup >> $dest/kernelheadersdownload.log 2>&1
			if [ $kpat = "default" ]; then
				zypper --pkg-cache-dir $dest download kernel-devel-$verbup >> $dest/kernelheadersdownload.log 2>&1
			else
				zypper --pkg-cache-dir $dest download kernel-devel-azure-$verbup >> $dest/kernelheadersdownload.log 2>&1
			fi
			find $dest -name "*$verbup*.rpm" | xargs cp -t $dest/kernelheaders/
        ) 200>/tmp/driver.lock
        done
    done
done

if [ -f "$dest/supported_kernels" ]; then
	uniq $dest/supported_kernels > $dest/supported_kernels.uniq
    mv -f $dest/supported_kernels.uniq $dest/kernelheaders/supported_kernels
fi

# Copy kernel headers from other SLES15 SP build box
if [ ! -f /etc/SuSE-release ]; then
    patch_level=`grep -o -- -SP[0-9] /etc/os-release | grep -o [0-9]`
    if [ ${patch_level} -eq 1 ]; then
        for ver in {2..4}
        do
            copyKernelHeadersFromOtherSP "SP$ver"
        done
    fi
fi

if [ -s $dest/kernelheaders/supported_kernels ]; then
    if [ ! -f /etc/SuSE-release ]; then
        patch_level=`grep -o -- -SP[0-9] /etc/os-release | grep -o [0-9]`
        if [ ${patch_level} -eq 1 ]; then
	      zip SLES15KernelHeaders.zip $dest/kernelheaders/*
          MakeDir_Remote "${FTP_PATH}/${DATE_FOLDER}"
          Copy_To_Remote SLES15KernelHeaders.zip ${FTP_PATH}/${DATE_FOLDER}/
        fi
    else
		zip SLES12KernelHeaders.zip $dest/kernelheaders/*
		MakeDir_Remote "${FTP_PATH}/${DATE_FOLDER}"
		Copy_To_Remote SLES12KernelHeaders.zip ${FTP_PATH}/${DATE_FOLDER}/
    fi
else
    log "New kernel headers are not available. skipping zip creation."
fi

if [ ! -f /etc/SuSE-release ]; then
    patch_level=`grep -o -- -SP[0-9] /etc/os-release | grep -o [0-9]`
    if [ ${patch_level} -eq 1 ]; then
        Copy_To_Remote $dest/kernelheadersdownload.log ${FTP_PATH}/${DATE_FOLDER}/kernelheadersdownload_sles15.log
    fi
else
    Copy_To_Remote $dest/kernelheadersdownload.log ${FTP_PATH}/${DATE_FOLDER}/kernelheadersdownload_sles12.log
fi

exit $error
