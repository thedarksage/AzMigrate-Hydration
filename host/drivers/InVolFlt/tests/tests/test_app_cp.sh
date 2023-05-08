#!/bin/bash -e

. libflt.sh

# Private execs required for execution of test
testexe="inm_dmit fio consistency_point GenerateTestTree.sh" 

etime="9999999999"                  
interval=180                        # Default tag every 3 min 

fifo="$rundir/fifo"
endfile="$rundir/stop"

mntdir=/mnt/$$
fioconf=/tmp/fio.$$
tccont=0

part_disk()
{
fdisk $src << EOF
o
n
p
1

+512M
n
p
2


w
quit
EOF
}

setup()
{
    
    inm_dmit --set_attr VacpAppTagCommitTimeout 60000000
    inm_dmit --get_attr VacpAppTagCommitTimeout
    inm_dmit --set_attr FsFreezeTimeout 60000000
    inm_dmit --get_attr FsFreezeTimeout
    rm -f $fifo
    mkfifo $fifo
    mkdir -p $logdir
    mkdir -p $mntdir
    log "SETUP: Preparing the disk"
    set +e
    dd if=/dev/zero of=$src bs=1M 
    blockdev --flushbufs $src
    set -e
    mkfs -t ext4 -F $src > /dev/null
    blockdev --flushbufs $src
    mount $src $mntdir
}

start_replication()
{
    log "Starting Replication"
    fork_background "inm_dmit --op=replicate --version=v2 --src_vol=$src --tgt_vol=$tgt --tag_pending_notify=$fifo" "/dev/null" 
    repid="$!"
}

copy_fstab()
{
    fname=$mntdir/fstab
    cat /etc/fstab > $fname
    
    cat $fname > $fname.org

    # make sure the new file has atleast one fs block more
    dd if=/dev/urandom of=$fname.new bs=4096 count=1
    echo "" >> $fname.new
    cat $fname >> $fname.new
    sleep 10

    while [ 1 -eq 1 ]
    do
        test -f $fname.int && rm $fname.int

        cat $fname.org > $fname.int
        rm $fname.org
        cat $fname.new > $fname.org
        rm $fname.new
        cat $fname.int > $fname.new
   
        if [ -f "$endfile" ]
        then
            break
        fi

        # do it once every tag
        sleep $interval
    done
}

start_io_fstab()
{
    log "Starting file ops"
    fork_background "copy_fstab" "$logdir/io.log"
}

gtt()
{
    gttdir=$mntdir/GTT
    while true
    do
        rm -rf $gttdir
        GenerateTestTree.sh $gttdir 1 128 0
        if [ -f $endfile ]
        then
            break
        fi    
        sleep 60
    done
}

start_io_gtt()
{
    log "Start GenTestTree"
    fork_background "gtt" "$logdir/io.log"
}

start_fio()
{
    log "IO: Starting IO to $mntdir"

cat > $fioconf << EOF
[fixed-rate-submit]
rw=randwrite
ioengine=libaio
iodepth=1
direct=1
do_verify=0
rate=500k
bsrange=4k-128k
size=104857600m
filesize=512m
directory=$mntdir
runtime=$etime
EOF

    fio $fioconf > /dev/null 2>&1
    touch $endfile
}

start_io_fio()
{
    log "Start fio"
    fork_background "start_fio" "$logdir/io.log"
    iopid=$!
}

start_io()
{
    start_io_gtt
    start_io_fstab
    start_io_fio
}

check_data()
{
    log "CHECK: Comparing $src and $tgt"

    # blockdev may have earlier incarnation in the 
    # page cache while FS has modified on disk
    echo 1 > /proc/sys/vm/drop_caches

    smd5="`md5sum $src | cut -f 1 -d " "`"

    # tmd5 should have md5 of prev compare
    if [ "$smd5" = "$tmd5" ]
    then
        log "CHECK: No change in src data"
        return 1
    fi

    tmd5="`md5sum $tgt | cut -f 1 -d " "`"
    if [ "$smd5" != "$tmd5" ]
    then
        log "CHECK: data mismatch $smd5 -> $tmd5"
        cmp -b --print-bytes $src $tgt
        return 1
    fi

    log "MD5: $smd5"
    return 0
}

start_verification()
{
    log "VER: Wait for next tag"
    read tag_name < $fifo
    log "VER: $tag_name"
        
    check_data
    if [ $? -eq 0 ]
    then
        #(( pass++ )) - returns value of pass and script fails
        pass=`expr $pass + 1`
    else
        log "VER: FAILED: Data mismatch"
        fail=`expr $fail + 1`
                    
        # kill the timer and stop the tests
        touch $endfile
    fi
}

start_tag_verify()
{
    local i=0
   
    log "Tag every $interval seconds"

    while [ 1 -eq 1 ]
    do
        consistency_point -f -d $src -m 2400000 > /dev/null
        if [ $? -eq 0 ]
        then 
            consistency_point -p "Tag$i" -d $src > /dev/null
            if [ $? -eq 0 ]
            then
                log "TAG: Issued Tag$i"
                i=`expr $i + 1`

                # start the timer for issuing next tag
                sleep $interval &
                sleepid="$!"

                start_verification

                # Thaw the fs and commit the tag
                log "VER: Commit tag"
                consistency_point -t -d $src > /dev/null
                consistency_point -c > /dev/null
    
                if [ -f $endfile ]
                then
                    # if endfile present, break and return
                    break
                else
                    # Signal rep thread to continue draining
                    echo "Y" > $fifo
                fi

                # wait for timer to issue next tag
                wait $sleepid
            else
                # Thaw the device on failure
                consistency_point -t -d $src > /dev/null
                log "TAG: Failed to issue tag Tag$i"
                sleep 60
            fi
        else
            log "TAG: Failed to freeze $src"
            sleep 60
        fi

        if [ -f $endfile ]
        then
            break;
        fi
    done
}

stop_replication()
{
    log "Stopping replication process"
    echo "N" > $fifo
    sleep 5
    kill -9 $repid > /dev/null 2>&1
}


cleanup()
{
    umount $mntdir
    rm -rf $mntdir
}

getoptions()
{
    while getopts ":i:n:s:t:" opt 
    do
        case $opt in
            i)  isnumber $OPTARG
                if [ $? -eq 0 ]
                then
                    interval=$OPTARG
                else
                    log_err "$OPTARG is not a number" 
                fi
                ;;

            n)  isnumber $OPTARG
                if [ $? -eq 0 ]
                then
                    etime=$OPTARG
                else
                    log_err "$OPTARG is not a number"
                fi
                ;;

            s)  src="$OPTARG"
                if [ ! -b "$src" ]
                then
                    log_err "$src not a block device"
                    return 1;
                fi
                ;;

            t)  tgt="$OPTARG"
                if [ ! -b "$tgt" ]
                then
                    log_err "$tgt not a block device"
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
    
    return 0;
}

repid=""
iopid=""

pass=0
fail=0

if [ -d "$logdir" ]
then
    echo "Saving older logs"
    tar cjf "${logdir}-`date`.tar.bz2" "$logdir" 
    rm -rf "$logdir"
fi

# main
init
check_dependency "$testexe"
getoptions $@
setup
resync $src $tgt
start_service
start_replication 

log "Runnign test for $etime secs"
start_io
sleep 60
start_tag_verify

# Dont care about errors in cleanup
# Just make sure all things go through
set +e
kill -9 $iopid
stop_replication
stop_filtering $src
wait $repid 
rm -f "$endfile"
stop_service
cleanup
log "RESULT: Passed: $pass Failed: $fail"
