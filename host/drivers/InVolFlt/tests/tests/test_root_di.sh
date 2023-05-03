#!/bin/bash -e

tcstr="`readlink -m $0`"
if [ ! -f libflt.sh ]
then
    tcdir="`dirname $tcstr`"
    cd $tcdir
fi

. libflt.sh

# Private execs required for execution of test
testexe="inm_dmit GenerateTestTree.sh fio consistency_point killall filecmp kpartx" 

ntags=""                           # Default 10 min run
loop="0"
interval=180                        # Default tag every 3 min 

fifo="$rundir/fifo"
endfile="$rundir/stop"
mnt=""
fioconf=/tmp/fio.$$
cfifo="$rundir/cfifo"
tfifo="$rundir/tfifo"
twaitpid=""
tagnotify=""
disk_rename=0
persname=""

BOOT_SCSI_CLASS_ID="{f8b3781a-1e82-4818-a1c3-63d806ec15bb}"

GetBootDevicesOnGen2()
{

    numbootscsicontrollers=`grep ${BOOT_SCSI_CLASS_ID} /sys/bus/vmbus/devices/*/device_id | wc -l`

    if [ $numbootscsicontrollers -ne 1 ]; then
        echo "Found ${numbootscsicontrollers} scsi controllers for boot disk"
        return
    fi

    bootscsicontroller=`grep ${BOOT_SCSI_CLASS_ID} /sys/bus/vmbus/devices/*/device_id | awk -F: '{ print $1}' | sed 's/device_id//g'`

    boot_dev=`ls ${bootscsicontroller}/host*/target*/[0-9]*:*0/block/ 2>/dev/null`
    if [ ! -z "$boot_dev" ]; then
        touch /etc/vxagent/usr/vxprotdevdetails.dat
        disk_rename=1
        src="/dev/"${boot_dev}
        persname=RootDisk
    fi
}


find_root_disk()
{
    local VMBUSDEVDIR="/sys/bus/vmbus/devices/"

    # For hyper-v/azure VM, crete the file for persistent name info
    for vmbusdev in `ls $VMBUSDEVDIR`
    do
        deviceid=(`cat ${VMBUSDEVDIR}/${vmbusdev}/device_id | sed 's/[{}-]/ /g' | awk '{print $1,$2}'`)
        if [ "${deviceid[0]}" != "00000000" -o \
             "${deviceid[1]}" != "0000" ]; then
            continue
        fi
        
        touch /etc/vxagent/usr/vxprotdevdetails.dat
	disk_rename=1
	src="/dev/`ls ${VMBUSDEVDIR}/${vmbusdev}/host*/target*/[0-9]*:*/block/`"
        persname=RootDisk
        break
    done

    if [ -z "$persname" ]; then
        GetBootDevicesOnGen2
    fi
}

wait_for_tag_notification()
{
    read tagnotify < "$fifo"
    echo $tagnotify > "$tfifo"
}

get_tag_notification()
{
    log "TAG: Wait"
    read tagnotify < "$tfifo"
    log "TAG: $tagnotify Notified"
    sleep 5
    echo $tagnotify
}

get_rc_local()
{
    for rclocal in "/etc/rc.d/rc.local" "/etc/rc.local" "/etc/init.d/boot.local"
    do
        test -f $rclocal && break
    done

    chmod +x $rclocal
}

remove_rc_local_entry()
{
    get_rc_local

    grep -q "ASR_DI" $rclocal && {
        # Delete prev entry
        ln="`grep -n "ASR_DI" $rclocal | cut -f 1 -d ":"`"
        # Delete two more lines apart from "ASR_DI"
        (( lne = ln + 3 ))
        sed -i ${ln},${lne}d $rclocal
    }
}

add_rc_local_entry()
{
    local _tcstr=""

    get_rc_local

    ((loop--))
    _tcstr="bash -x -e $tcstr -t $tgt -i $interval -r -l $loop -n $ntags" 
    # For non disk rename, provide the disk name via cli
    if [ "$disk_rename" = "0" ]
    then
        _tcstr="$_tcstr -s $src"
    fi

    # Any line here should be deleted in remove_rc_local_entry()
    echo "#ASR_DI" >> $rclocal
    echo "sleep 10" >> $rclocal
    echo "export PATH=$PATH" >> $rclocal
    echo "$_tcstr > $tgt/reboot.log 2>&1 &" >> $rclocal
}

setup()
{
    log "Test Setup"
    # Prevent system from complaining about long hangs
    echo 0 > /proc/sys/kernel/hung_task_timeout_secs
    # Drop cache if any from previous runs
    echo 1 > /proc/sys/vm/drop_caches
    echo 1 > /proc/scsi/sg/allow_dio

    # Increare rge freeze timeout
    inm_dmit --set_attr FsFreezeTimeout 9999999999
    inm_dmit --set_attr VacpAppTagCommitTimeout 9999999999
    inm_dmit --set_attr VacpIObarrierTimeout 9999999999
    inm_dmit --set_attr TrackRecursiveWrites 1
    # StablePages support has a bug where the modprobe on machine restart gets stuck
    # inm_dmit --set_attr StablePages 1

    test -p $fifo && rm -f $fifo
    mkfifo $fifo
    test -p $cfifo && rm -f $cfifo
    mkfifo $cfifo
    test -p $tfifo && rm -f $tfifo
    mkfifo $tfifo

    mkdir -p $logdir
    mkdir -p $mnt

    if [ "$tccont" != "0" ]
    then
        export TERM="/dev/console"
    else
        local tgtd="`mount | grep "$tgt" | cut -f 1 -d " "`"
        if [ "$tgtd" != "" ]
        then
            if echo "$src" | grep -q $tgtd
            then
                log "Target cannot be part of source dev list"
                return 1
            fi
        else
            log "Target directory not a mount point"
            return 1    
        fi    
    fi
}

do_resync()
{
    log "Resync $src to $img"
    io_barrier
    insert_tag "Resync"
    tag_name="`get_tag_notification`"
    log "Start Resync"
    fast_copy $src $img 
    match_blocks
    remove_io_barrier
    commit_tag Y
}

start_replication()
{
    log "Starting Replication"

    fork_background "inm_dmit --op=replicate --version=v2 --src_vol=$src --pname=$persname --verbose
                    --tgt_vol=$img --tag_pending_notify=$fifo" "$replog" 
    repid="$!"

    # Wait for bitmap file to be created
    sleep 60
}

fops()
{
    if [ "$tccont" = "0" ]
    then
        cat /etc/fstab > /xyz.2

        # make sure the new file has atleast one fs block more
        dd if=/dev/urandom of=/xyz.1 bs=4096 count=1
        echo "" >> /xyz.1
    fi

    while :
    do
        cat /xyz.1 > /xyz
        cat /xyz.2 >> /xyz
        cp /xyz.2 /xyz
   
        if [ -f "$endfile" ]
        then
            break
        fi

        sleep 90
    done
}

fio_io()
{
    local rtime=0

    (( rtime = ntags * interval ))

cat > $fioconf << EOF
[fixed-rate-submit]
rw=randwrite
ioengine=sync
iodepth=4
fsync=1024
do_verify=0
rate=128k
bsrange=4k-128k
size=1024m
filesize=256m
directory=/
runtime=$rtime
fallocate=none
EOF

    fork_background "fio $fioconf" "/dev/null"
}


gtt()
{
    while :
    do
        test -d /GTT && rm -rf /GTT
        GenerateTestTree.sh /GTT 16 32 0 > $tgt/io.log 2>&1
	sync
        test -f $endfile && break
        sleep 90
    done
}

start_io()
{
    log "IO: Start FIO"
    fork_background "fio_io" "$logdir/io.log"
    log "IO: Start GenTestTree"
    fork_background "gtt" "$logdir/io.log"
    log "IO: Start FOPS"
    fork_background "fops" "$logdir/io.log"
}

fsck_image()
{
    local fs=""
    local loopd=""
    local fsckfail=0
   
    echo 1 > /proc/sys/vm/drop_caches
    fast_copy $img ${img}_dup
    #dd if=$img of=${img}_dup bs=16M iflag=direct,sync oflag=direct,sync

    log "Creating paritions from ${img}_dup" 
    # Create loop devices from root file
    kpartx -asv ${img}_dup
    
    # Find the corresponding loop dev created
    loopd="`losetup -a | grep ${img}_dup | cut -f 1 -d ":"`"
    loopd="`basename $loopd`"

    # run fsck on all partitions containing fs
    for fs in `get_fs_dev_list /dev/mapper/${loopd}`
    do
        log "Running fsck on $fs"
        mount_fs $fs $mnt
        umount $mnt
        if fsck_fs $fs >> $logfile 2>&1
        then
            log "$fs: fsck clean"
        else
            log "$fs: fsck reports errors"
            fsckfail=1
        fi
    done

    # remove the loop devices
    kpartx -d ${img}_dup

    return $fsckfail
}

match_blocks()
{
    local cp=""

    echo 1 > /proc/sys/vm/drop_caches
   
    log "CHECK: Comparing $src and $img"
    log "`filecmp $src $img`" | grep mismatch && { 
        cp="`inm_dmit --get_drv_stat | grep ^CP | cut -f 2 -d " "`"
        if [ "$cp" = "0" ]
    then
            log "BARRIER: Timedout"
            return 1
    else
            return 1
        fi
    }

    return 0
}

get_src_snap()
{
    echo $1
}


remove_src_snap()
{
    echo "Remove Snap $1"
}

verify_tag()
{
    local testfail=0

    #freeze_fs $src
    io_barrier
    insert_tag "Tag$1"
    if [ $? -eq 0 ]
    then
        # Wait for tag
        tag_name="`get_tag_notification`"
        
        # Match the blocks
        if match_blocks
        then
            log "Blocks Matched"
        else
            log "Blocks Mismatched"
            testfail=1
        fi

        # Remove the barrier so that io can continue
        remove_io_barrier

        # Commit tag and continue draining
        commit_tag $continue
    else
        # Thaw the device on failure
        remove_io_barrier
        log "TAG: Failed to issue tag Tag$1"
        return 1
    fi

    if [ "$testfail" = "0" ]
    then
        log "PASSED: Data Match"
        pass=`expr $pass + 1`
    else
        log "FAILED: Data mismatch"
        fail=`expr $fail + 1`
        return 1
    fi
}

thaw_volumes()
{
    local fs=""

    for fs in $frozenlist
    do
        consistency_point -t -d $fs
    done

    frozenlist=""
}

freeze_volumes()
{
    local vrc=0
    local fs=""

    for fs in `get_fs_dev_list $1`
    do
        consistency_point -f -m 6000000 -d "$fs" 
        vrc=$?
        if [ $vrc != 0 ]
        then
            break
        fi
        frozenlist="$fs $frozenlist"
        log "$fs frozen"
    done

    return $vrc
}

thaw_fs()
{
    local fs=""

    for fs in $frozenlist
    do
        fsfreeze -u $fs
    done

    frozenlist=""
}

freeze_fs()
{
    local vrc=0
    local fsdev=""
    local mnt=""

    for fsdev in `get_fs_dev_list $1`
    do
        mnt="`mount | grep $fsdev | cut -f 3 -d " "`"
        fsfreeze -f "$mnt"
        vrc=$?
        if [ $vrc != 0 ]
        then
            break
        fi
        frozenlist="$mnt $frozenlist"
        log "$fsdev -> $mnt frozen"
    done

    return $vrc
}

io_barrier()
{
    log "BARRIER: Create"

    while :
    do
        sync
        echo 1 > /proc/sys/vm/drop_caches

        if [ cp = "app" ]
            then
            freeze_volumes $src && break
        else
            consistency_point -b -m 9999999999 && break 
        fi
        # wait while WOS is not data mode
        sleep 10
    done

    # Wait for inflight IO to complete before inserting tag
    sleep 60
}

remove_io_barrier()
{
    local _ret=0;
    log "BARRIER: Remove"

    if [ cp = "app" ]
                then
        thaw_volumes
    else
        consistency_point -u > /dev/null || echo "BARRIER: Remove Failed"
        thaw_fs
                fi
    _ret=$?

    echo 1 > /proc/scsi/sg/allow_dio

    return $_ret
}

insert_tag()
{
    log "TAG: Insert $1"

    # To synchronize with drainer and not miss any pending i
    # tag notification, we start the wait before inserting the tag
    fork_background wait_for_tag_notification 
    twaitpid=$!

    if [ cp = "app" ]
                then
        consistency_point -p "$1" -d $src > /dev/null
    else
        consistency_point -p "$1" > /dev/null
                fi
}

commit_tag()
{
    log "TAG: Commit"
    consistency_point -c > /dev/null || echo "TAG: Commit Failed"

                    # Signal rep thread to continue draining
    echo $1 > $fifo
}

verify_data()
{
    local vrc=0
    local ln=0
    local lne=0
    local fs=""
    local nt=$ntags
    local continue=Y

    log "Tag every $interval seconds"
    sleep $interval &
    sleepid="$!"

    while [ $nt -gt 0 ]
    do
        wait $sleepid

        # Start timer thread for multiple tags
        if [ $nt -gt 1 ]
        then
            # start the timer for issuing next tag
            sleep $interval &
            sleepid="$!"
        else
            sleepid=""
            continue=N
        fi

        log "IO: `inm_dmit --get_volume_stat $src | grep ^Writes`"
        if verify_tag
        then
            log "Iteration $nt succedded"
        else
            log "Iteration $nt failed"
            break
        fi

        (( nt-- ))
    done

    if [ $loop -eq 0 ]
    then
        touch $endfile
    fi
}

wait_for_data_mode()
{
    while :
    do
        state="`inm_dmit --get_volume_stat $src | grep ^"Filtering Mode" | cut -f 2 -d ":" | cut -f 2 -d " "`"
        if [ "$state" = "Data/Data" ]
        then
            break
        fi
        sleep 10
    done
}

start_timer()
{
    sleep $ntags
    log "TIMER: Times up"
    touch "$endfile"
}

stop_replication()
{
    log "Stopping replication process"
    kill -9 $repid > /dev/null 2>&1
}

start_notify()
{
    local host=""

    log "Start Notify"
    fork_background "inm_dmit --op=start_notify" "$logfile"
}

stop_notify()
{
    local host=""

    log "Stop Notify"
    pkill inm_dmit
    killall inm_dmit
}


cleanup()
{
    echo 120 > /proc/sys/kernel/hung_task_timeout_secs
}

getoptions()
{
    while getopts ":aci:l:n:rs:t:" opt 
    do
        case $opt in
            a)  cp=app
                ;;

            c)  cp=crash
                ;;

            i)  isnumber $OPTARG
                if [ $? -eq 0 ]
                then
                    interval=$OPTARG
                else
                    log_err "$OPTARG is not a number" 
                fi
                ;;

            l)  isnumber $OPTARG
                if [ $? -eq 0 ]
                then
                    loop=$OPTARG
                else
                    log_err "$OPTARG is not a number" 
                fi
                ;;

            n)  isnumber $OPTARG
                if [ $? -eq 0 ]
                then
                    ntags=$OPTARG
                else
                    log_err "$OPTARG is not a number"
                fi
                ;;

            r)  tccont=1;
                remove_rc_local_entry
                ;;

            s)  if [ "$disk_rename" = "1" ]
                then
                    log_err "Root disk will be autodetermined"
                else
		    src="$OPTARG"
                if [ ! -b "$src" ]
                then
                    log_err "$src not a block device"
                    return 1;
                fi
                    persname=`echo $src | sed "s/\//_/g"`
                fi
                ;;

            t)  tgt="$OPTARG"
                if [ ! -d "$tgt" ]
                then
                    log_err "$tgt not a directory"
                    return 1;
                fi
                ;;

            *)  log "Invalid option"
                return 1;
        esac
    done

    if [ "$src" = "" -o "$tgt" = "" ]
    then
        log_err "Need to provide both source and target volume"
        return 1;
    fi
 
    if [ "$loop" = "0" ]
    then
        if [ "$ntags" = "" ]
        then
            log_err "Provide number of tags"
            return 1
        fi
    else
        if [ "$ntags" = "" ]
        then
            ntags=1
        fi
    fi

    if [ "$cp" = "" ]
    then
       cp="crash"
    fi

    img="$tgt/root.img"
 
    return 0;
}

iopid=""
repid=""
tpid=""

tccont=0
rootdev=0
fsckdlist=""
frozenlist=""

pass=0
fail=0

find_root_disk
getoptions $@
mnt=$tgt/$$
logdir=$tgt
rundir_base=$tgt
logfile="$tgt/$pname.log"
replog="$tgt/rep.log"

# main
check_dependency "$testexe"


init

log "Starting Loop - $loop"

setup
start_service
start_notify

if [ "$tccont" = "0" ]
then
    > $logfile
    > $replog

    if [ -f /etc/vxagent/usr/vxprotdevdetails.dat ]
    then
        log "Running test on Hyper-V/Azure"
    else
        log "Running test on VmWare/Physical"
    fi
    
    log "RootDisk = $src"

    if [ "$loop" = "0" ]
    then
        log "Running test for $ntags tags"
    else
        log "Running for $loop reboots"
    fi
    touch $img
    inm_dmit --op=start_flt --src_vol=$src --pname=$persname
    start_replication 
    do_resync
else
    log "Continue test"
    log "RootDisk = $src"

    start_replication 
    wait_for_data_mode
fi

start_io
verify_data
log "RESULT: Passed: $pass Failed: $fail"

which journalctl > /dev/null 2>&1 && {
	journalctl -xb | grep involflt > $tgt/dmesg${loop}.log
} || {
	dmesg > $tgt/dmesg${loop}.log
}

if [ $loop -eq 0 ]
then
    # Make sure all the logs hit the disk
    sleep 60
    sync

    # Dont care about errors in cleanup
    # Just make sure all things go through
    set +e
    stop_replication
    stop_filtering $src
    wait $repid 
    rm -f "$endfile"
    stop_notify
    stop_service
    cleanup
else
    log "Rebooting System"
    add_rc_local_entry
    # Wait for all the queued writes to be drained
    sleep 60
    reboot
fi

