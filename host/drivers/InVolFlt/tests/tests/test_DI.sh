#!/bin/bash -xe

. libflt.sh

testexe="inm_dmit md5cksum vacp"   # Private execs required for execution of test
numio=1048576                      # Defaults to 4GB changes
fifo="$rundir/fifo"

setup()
{
    rm -f $fifo
    mkfifo $fifo
    mkdir -p $logdir
}

start_replication()
{
    log "Starting Replication"
    fork_background "inm_dmit --op=replicate --version=v2 --src_vol=$src --tgt_vol=$tgt --tag_notify=$fifo" "$logdir/rep.log" 
    repid="$!"
}

start_io()
{
    log "Starting IO on $src"
    fork_background "md5cksum -d $src -i 4096 -w -r -x $numio -p 4096" "$logdir/io.log"
    iopid="$!"
}

issue_tag()
{
    log "Issuing Tag"
    vacp -systemlevel -cc || {
        log "Failed to issue tags"
    }
}

wait_for_tag()
{
    log "Waiting for tag to be drained"
    read tag_name < $fifo
    log "$tag_name drained"
}

stop_replication()
{
    log "Stopping replication process"
    echo "N" > $fifo
    sleep 5
}

check_data()
{
    log "Comparing $src and $tgt"
    cmp -b --print-bytes $src $tgt
    return $?
}

getoptions()
{
    while getopts ":n:s:t:" opt 
    do
        case $opt in
            n)  isnumber $OPTARG
                if [ $? -eq 0 ]
                then
                    numio=$OPTARG
                else
                    log "$OPTARG is not a number"
                fi
                ;;

            s)  src="$OPTARG"
                if [ ! -b "$src" ]
                then
                    log "$src not a block device"
                    return 1;
                fi
                ;;

            t)  tgt="$OPTARG"
                if [ ! -b "$tgt" ]
                then
                    log "$tgt not a block device"
                    return 1;
                fi
                ;;

            *)  log "Invalid option"
                return 1;
        esac
    done

    if [ "$src" = "" -o "$tgt" = "" ]
    then
        log "Need to provide both source and target volume"
        return 1;
    fi
    
    return 0;
}

iopid=""
repid=""

# main
check_dependency "$testexe"
getoptions $@
init
setup
resync $src $tgt
start_service
start_replication 
start_io
wait $iopid
issue_tag
wait_for_tag
check_data && {
    log "PASSED: Data match between $src and $tgt"
} || {
    log "FAILED: Data Mismatch between $src and $tgt"
}
stop_replication
stop_filtering
stop_service
