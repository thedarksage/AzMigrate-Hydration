#!/bin/bash -e

. libflt.sh

# Private execs required for execution of test
testexe="inm_dmit fio consistency_point" 

fifo="$rundir/fifo"
endfile="$rundir/stop"

setup()
{
    rm -f "$endfile"
    inm_dmit --set_attr FsFreezeTimeout 60000000
    rm -f $fifo
    mkfifo $fifo
    mkdir -p $logdir
    log "SETUP: Preparing the disk"
    set +e
    dd if=/dev/zero of=$src bs=1M 
    blockdev --flushbufs $src
    set -e
}

start_replication()
{
    log "Starting Replication"
    fork_background "inm_dmit --op=replicate --version=v2 --src_vol=$src --tgt_vol=$tgt --tag_pending_notify=$fifo" "/dev/null" 
    repid="$!"
}

start_io()
{
    log "IO: Starting IO to $src"
    time writev $src
    touch $endfile
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
    fi
}

start_tag_verify()
{
    local i=0
   
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

                start_verification

                # Thaw the fs and commit the tag
                log "VER: Commit tag"
                consistency_point -t -d $src > /dev/null
                consistency_point -c > /dev/null
    
                echo "Y" > $fifo

                if [ -f $endfile ]
                then
                    # if endfile present, break and return
                    break
                fi
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
    sleep 5
    kill -9 $repid > /dev/null 2>&1
}


getoptions()
{
    while getopts ":i:n:s:t:" opt 
    do
        case $opt in
            n)  isnumber $OPTARG
                if [ $? -eq 0 ]
                then
                    ntags=$OPTARG
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
tmpid=""

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
while [ $ntags -gt 0 ]
do
	start_io
	start_tag_verify
	(( ntags-- ))
done
stop_replication
stop_filtering $src
rm -f "$endfile"
stop_service
log "RESULT: Passed: $pass Failed: $fail"
