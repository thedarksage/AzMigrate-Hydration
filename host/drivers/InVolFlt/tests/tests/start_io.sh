#!/bin/bash -e

_logstr=""
_disable_log=0
logfile="/io.log"
endfile="/stop"

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


fops()
{
    cat /etc/fstab > /xyz.2

    # make sure the new file has atleast one fs block more
    dd if=/dev/urandom of=/xyz.1 bs=4096 count=1
    echo "" >> /xyz.1

    while :
    do
        test -f $endfile && break
        cat /xyz.1 > /xyz
        cat /xyz.2 >> /xyz
        cp /xyz.2 /xyz
   
        sleep 90
    done
}

gtt()
{
    while :
    do
        test -d /GTT && rm -rf /GTT
        GenerateTestTree.sh /GTT 16 1024 0 >> $logfile 2>&1
	sync
        test -f $endfile && break
        sleep 90
    done
}

start_io()
{
    log "IO: Start GenTestTree"
    fork_background "gtt" "$logfile"
    log "IO: Start FOPS"
    fork_background "fops" "$logfile"
}

check_dependency GenerateTestTree.sh

>$logfile
rm -rf $endfile

log "Starting test"
start_io
log "Started IO in background, touch $endfile to stop it."
