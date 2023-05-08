pname="`basename $0 | cut -f 1 -d "."`"
. libflt.cfg

init()
{
    mkdir -p "$rundir_base"
    mkdir -p $rundir
    mkdir -p $logdir

    return 0
}

start_service()
{
    log "Starting service"
    fork_background "inm_dmit --op=service_start"
    _spid="$!"
    sleep 5
}

_logstr=""
_disable_log=0

disable_log()
{
    _disable_log=1
    sleep 10
}

enable_log()
{
    # This requires IPC
    if [ "$_disable_log" = "1" ]
    then
        echo -e "$_logstr" | tee -a $logfile
        _logstr=""
        _disable_log=0
    fi
}

log()
{
    if [ "$_disable_log" = "1" ]
    then
        _logstr="${_logstr}`date` ($$): $1\n"
    else
	if [ "$logfile" != "" ]
	then
            echo "`date`($$): $1" | tee -a $logfile
	else
            echo "`date` ($$): $1" 
	fi
    fi
}

log_err()
{
    log "ERROR: $1"
}

fork_background()
{
    if [ "$2" = "" ]
    then
        $1 &
    else
        $1 >> $2 2>&1 &
    fi

    sleep 2
    # test if still executing
    kill -0 $! && {
        return 0
    } || {
        log "Failed to execute \"$1\" in background"
        return 1
    }
}

isnumber()
{
    if [[ $1 =~ ^[0-9]+$ ]]
    then
        return 0
    else
        return 1
    fi
}

check_dependency()
{
    for i in $1
    do
        which $i > /dev/null 2>&1 || {
            log "$i not found in $PATH"
            return 1
        }
    done
    return 0
}

resync()
{
    log "Resyncing volumes"
    dd if="$1" of="$2" bs=1048576 > /dev/null 2>&1
}

stop_filtering()
{
    log "Stop filtering on $1"
    inm_dmit --op=clear_diffs --src_vol=$1
    inm_dmit --op=stop_flt --src_vol=$1
}

stop_service()
{
    log "Kill service"
    kill -9 $_spid > /dev/null 2>&1
}


get_fs_dev_list()
{
    local fs=""
    local fdev=""

    for fs in `blkid ${1}[1-9]* | grep -v swap | grep -v dos | grep -v LVM | cut -f 1 -d ":"`
    do
        fdev="$fs $fdev"
    done

    if [ "$fdev" = "" ]
    then
        # For dev names ending in digit, partitions start with 'p' prefix
        for fs in `blkid ${1}p[1-9]* | grep -v swap | grep -v dos | grep -v LVM | cut -f 1 -d ":"`
        do
            fdev="$fs $fdev"
        done
        # If there are still no partitions with FS return full disk
        if [ "$fdev" = "" ]
        then
            fdev="$1"
        fi
    fi

    echo $fdev
}

fstype()
{
    blkid -s TYPE | grep $1 | cut -f 2 -d "=" | sed "s/\"//g" | tr -d " "
}

fsck_fs()
{
    local fdev="$1"
    local fs=""
    local ret=1

    fs="`fstype $fdev`"

    case $fs in
        xfs*)
        xfs_repair -n $fdev
        ret=$?
        ;;

        ext*)  
        fsck -f -y $fdev
        ret=$?
        ;;        
        
        *) 
        echo "Unsupported FS - $fs"
        ret=$?
        ;;       
    esac
}

mount_fs()
{
    local fdev="$1"
    local mnt="$2"
    local fs=""
    local ret=1

    fs=`fstype $fdev`

    case $fs in
        xfs*)
        mount -o nouuid "$fdev" "$mnt"
        ret=$?
        ;;

        ext*)  
        mount "$fdev" "$mnt"
        ret=$?
        ;; 

        *) 
        echo "Unsupported FS - $fs"
        ret=$?
        ;;       
    esac
}

# $1 = src
# $2 = dest
# $3 = size (optional)
fast_copy()
{
    local size="$3"

    if [ "$size" = "" ]
    then
        if [ -b $src ]
        then
            #size=`fdisk -l $src | grep ^Disk | grep $src | cut -f 5 -d " "`
            size=`blockdev --getsize64 $src`
        elif [ -f $src ]
        then
            size=`ls -l $src | cut -f 5 -d " "`
        fi
    fi

    seq 0 64 $(( $size/16777216)) |\
    xargs -i -P 4 dd if=$1 of=$2 bs=16M count=64 iflag=direct,sync \
                    oflag=direct,sync skip={} seek={}
}

