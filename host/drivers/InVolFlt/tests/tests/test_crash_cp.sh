#!/bin/bash -e

. libflt.sh

# Private execs required for execution of test
testexe="cc_drain cc_verify write_pattern" 

blksz=4096
msz=5555555555
tagfile="$logdir/tags"
nif="eth0"
etime=""
interval="30"
endfile="$rundir/stop"

declare -A dl
declare -A dl_opts

rmexec()
{
    local host="$1"
    local cmd="$2"
    local opts="$3"

    if [ "$host" = "localhost" ]
    then
        $cmd 
        return $?
    else
        ssh $opts root@$host "$cmd"
        return $?
    fi
}

rmexec_background()
{
    local host="$1"
    local cmd="$2"
    local log="$3"
    
    if [ "$host" = "localhost" ]
    then
        fork_background "$cmd" "$log"
        return $?
    else
        if [ "$log" != "" ]
        then
            cmd="$cmd > $log 2>&1"
        fi
        rmexec "$host" "$cmd" "-f"
        return $?
    fi
}

prepare_remote_nodes()
{
    local i=""
    local host=""

    for host in $host_list
    do
        if [ "$host" = "localhost" ]
        then
            continue
        fi

        for i in $testexe
        do
            log "Copying `which $i` to $host:/usr/local/bin"
            scp `which $i` $host:/usr/local/bin > /dev/null 2>&1
        done
    done
}

cleanup_remote_nodes()
{
    local i=""
    local host=""

    for host in $host_list
    do
        if [ "$host" = "localhost" ]
        then
            continue
        fi

        for i in $testexe
        do
            # Remove binaries we had copied in prepare
            log "Removing $host:/usr/local/bin/$i"
            rmexec $host "rm -f /usr/local/bin/$i"
        done
    done
}

stop_replication()
{
    local host=""

    log "Stop replication"
    for host in $host_list
    do
        rmexec $host "killall cc_drain" > /dev/null 2>&1
        rmexec $host "pkill cc_drain" > /dev/null 2>&1
    done
}

stop_filtering()
{
    local host=""
    local disk=""

    log "Stop filtering"
    for host in $host_list
    do
        for disk in ${dl[$host]}
        do
            rmexec $host "$asrpath/bin/inm_dmit --op=stop_flt --src_vol=$disk" > /dev/null 2>&1
        done
    done

}

process_disk()
{
    local host=localhost
    local disk=$1

    echo "$disk" | grep -q : && {
        host="`echo "$1" | cut -f 1 -d ":"`"
        disk="`echo "$1" | cut -f 2 -d ":"`"

        if [ "$disk" = "" ]
        then
            return 1
        fi
    }

    # Append disk to per host disk list
    dl[$host]="${dl[$host]} $disk"
    # Append host to host list
    host_list="$host_list $host"
}

setup()
{
    local host=""
    local disk=""

    for host in $host_list
    do
        dl[$host]="`echo ${dl[$host]} | sed 's/ /\n/g' |\
                           sort | tr -s '\n' ' '`"

        # Prepare the opt string to pass to the commands
        dl_opts[$host]="-d `echo ${dl[$host]} | sed 's/ / -d /g'`"

        # Get individual disk size and determine smallest
        for disk in ${dl[$host]}
        do
            sz="`rmexec $host "blockdev --getsz $disk"`"
            sz=`expr $sz \* 512`
            log "$host:$disk -> $sz"
            if [ $sz -lt $msz ]
            then
                msz=$sz
            fi
        done

        # create log dirs
        rmexec "$host" "rm -rf $logdir"
        rmexec "$host" "mkdir -p $logdir"
    done
    
    > $tagfile
    rm -f "$endfile"

    log "Minimum Disk Size = $msz"

}

start_notify()
{
    local host=""

    log "Start Notify"
    for host in $host_list
    do
        rmexec_background "$host" "$asrpath/bin/inm_dmit --op=start_notify" "$logdir/rep.log" 
    done
}

stop_notify()
{
    local host=""

    log "Stop Notify"
    for host in $host_list
    do
        rmexec "$host" "pkill inm_dmit"
        rmexec "$host" "killall inm_dmit"
    done
}

start_replication()
{
    for host in $host_list
    do
        rmexec_background "$host" "cc_drain ${dl_opts[$host]} -b $logdir -l" "$logdir/rep.log" 
    done
    repid="$!"
}

start_io()
{
    set +x
    local host=""
    local offset=0
    local i=0

    while [ 1 -eq 1 ]
    do
        for host in $host_list
        do
            rmexec $host "write_pattern ${dl_opts[$host]} -s $blksz -i -l -o $offset -p $i >$logdir/write_pattern.log 2>&1"
            if [ $? -ne 0 ]; then
                write_output="`rmexec $host "cat $logdir/write_pattern.log"`"
                log "write failed on $host at $offset. $write_output."
                touch $endfile
                break
            fi
        done

        i=`expr $i + 1`
    
        offset=`expr $offset + $blksz`
        offset=`expr $offset % $msz`
        
        if [ $offset -eq 0 ]
        then
            log "Wrapped..."
        fi

        if [ -f "$endfile" ]
        then
            break
        fi
    done
    set -x
}

start_tagging()
{
    set +e
    set +x
    local i=1
    local host=""
    local tmpf="$logdir/tag"
    local tag=""
    local rhlist="`echo $host_list | sed 's/localhost//'`" # remote nodes list

    which ifconfig > /dev/null 2>&1 
    if [ $? -eq 0 ]; then
        ip="`ifconfig $nif | grep "inet addr" | tr -s " " | cut -f 3 -d " " | cut -f 2 -d ":"`"
    else
        ip="`ip addr show $nif | grep -w inet | tr -s " " | cut -f 3 -d " " | cut -f 1 -d "/"`"
    fi
    
    for host in $rhlist
    do
        cmd="$cmd,$host"
    done

    # Remove the leading comma
    cmd="`echo $cmd | sed 's/^,//'`"
    cmd="$asrpath/bin/vacp -systemlevel -cc -distributed -mn $ip -cn $cmd"
    log "TAG: \"$cmd\""

    while [ 1 -eq 1 ]
    do
        sleep $interval
        
        > $tmpf

        # Start vacp in background on remote nodes
        for host in $rhlist
        do 
            rmexec_background $host "$cmd" "${tmpf}.$i"
        done

        $cmd > ${tmpf}.$i
        if [ $? -eq 0 ]
        then
            # Should check for last tag
            tag="`grep DistributedCrashTag ${tmpf}.$i | cut -f 2 -d " "`"
            grep -q "Tags are issued on some of the devices only" ${tmpf}.$i
            if [ $? -eq 0 ]
            then
                echo "$i:1:$tag" >> $tagfile # cc_verify returns 1 for patial
            else
                echo "$i:0:$tag" >> $tagfile # cc_verify returns 0 for success
            fi
        else
            echo "$i:255:$tag" >> $tagfile # cc_verify returns -1(255) for failure
        fi
        
        i=`expr $i + 1`
        
        if [ -f $endfile ]
        then
            break
        fi
    done
}

start_timer()
{
    sleep $etime
    touch $endfile
}

verify_pattern()
{
    local i=0
    local master="$@"
    local prev=$1 
    local pat="n_n_n"
    shift

    n_1=0

    for i in $@
    do
        diff=`expr $prev - $i`
        if [ $diff -gt 1 -o $diff -lt 0 ]
        then
            log "Pattern: $master -> FAILED"
            return 1
        elif [ $diff -eq 1 ]
        then
            if [ $n_1 -eq 1 ]
            then
                log "Pattern: $master -> FAILED"
                return 1
            else
                prev=$i
                n_1=1
                pat=n_n_n-1
            fi
        else
            prev=$i
            if [ $n_1 -eq 1 ]
            then
                pat=n_n-1_n-1
            fi
        fi
    done

    log "Pattern: $master -> $pat"

    # Easier and bug free procedure would be to sort 
    # and compare against original list. Both should be same.
    # verify_pattern2 $master

    return 0
}

start_verify()
{
    set +e
    local tag=""
    local pattern=""
    local state=""
    local failed=1
    local host=""
    local patfile="$logdir/master_pat"
    local i=1

    sleep $interval
    > $patfile

    while [ 1 -eq 1 ]
    do
        pattern=""

        while [ 1 -eq 1 ]
        do
            if [ -f $endfile ]
            then
                break
            fi

            tag="`sed -n ${i}p $tagfile`"
            if [ "$tag" != "" ]
            then
                i=`expr $i + 1`
                state="`echo $tag | cut -f 2 -d ":"`"
                tag="`echo $tag | cut -f 3- -d ":"`"
                
                # If tag failed/partial, skip this iteration
                # TODO: Remove this
                if [ $state -ne 0 ]
                then
                    log "$i: Skipping as tag failed"
                    continue
                fi

                if [ "$tag" != "" ]
                then
                    break
                else
                    log "$i: Empty Tag .. Skipping"
                    continue
                fi
            fi

            sleep $interval
        done

        if [ -f $endfile ]
        then
            break
        fi

        master_pat=""
        log "Verifying $tag"
        for host in $host_list
        do
            pattern="`rmexec $host "cc_verify ${dl_opts[$host]} -l -t $tag -r $blksz -b $logdir 2>>$logdir/verify.log"`"
            # Return should match whatever tag process 
            # saved as state in tagfile
            if [ $? -eq $state ] 
            then
                log "Pass: $host"
                # skip the verify pattern for partial and failed case
                if [ $state -eq 0 ] 
                then
                    master_pat="$master_pat $pattern"
                fi
            else
                # In case of partial, return may not match
                # so don't consider it a failure
                if [ $state -ne 1 ]
                then
                    log "Fail: $host"
                    fail=`expr $fail + 1`
                    # On failure stop the tests
                    touch $endfile
                    break
                fi
            fi
        done

        if [ $fail -eq 0 ]
        then
            # Verify pattern for success/partial cases
            if [ $state -ne 255 ]
            then
                verify_pattern $master_pat
            else
                log "Skipping pattern verification for failed tag"
            fi
            if [ $? -eq 0 ]
            then
                pass=`expr $pass + 1`
                log "PASS->$pass: Master verification"
                echo "$master_pat" >> $patfile

                for host in $host_list
                do
                    rmexec $host "cc_verify ${dl_opts[$host]} -c -t $tag -r $blksz -b $logdir 2>>$logdir/verify.log"
                done
            else
                log "FAIL: Master pattern match"
                fail=`expr $fail + 1`
                touch $endfile
            fi
        fi

        if [ -f $endfile ]
        then
            break
        fi
    done
}

getoptions()
{
    while getopts ":b:d:i:n:s:t:" opt 
    do
        case $opt in
            b)  isnumber $OPTARG
                if [ $? -eq 0 ]
                then
                    blksz=$OPTARG
                else
                    log_err "$OPTARG is not a number" 
                fi
                ;;

            d)  process_disk "$OPTARG"
                ;;

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

            *)  log "Invalid option"
                return 1;
        esac
    done

    if [ "$host_list" = "" ]
    then
        log_err "Need to provide atleast one disk"
        return 1;
    fi
   
    # Unique names in host_list 
    host_list="`echo $host_list | sed 's/ /\n/g' | sort |\
                uniq | tr -s '\n' ' '`"

    return 0;
}

pass=0
fail=0

if [ -d "$logdir" ]
then
    echo "Saving older logs"
    tar cjf "${logdir}-`date`.tar.bz2" "$logdir"
    rm -rf "$logdir"
fi

init
check_dependency "$testexe"
getoptions $@
prepare_remote_nodes
setup
start_notify
start_replication 
start_io &
iopid="$!"
start_tagging &
tpid="$!"
if [ "$etime" != "" ]
then
    log "Runnign test for $etime secs"
	start_timer &
	tmpid="$!"
else
    log "Running tests infinitely"
fi
start_verify
# start_verify should end only when timer completes
# and populated endfile or a test fails
kill -9 $tmpid
wait $tpid
set +e
stop_replication
if [ $fail -eq 0 ]
then
    stop_filtering
fi
stop_notify
wait $iopid $tpid $vpid
log "RESULT: Passed = $pass Failed = $fail"
