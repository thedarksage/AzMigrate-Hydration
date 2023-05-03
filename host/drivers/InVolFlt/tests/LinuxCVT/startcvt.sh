#!/bin/bash

if [[ $# -ne 3 ]]; then
    echo "Insufficient number of args"
    exit 1
fi

script_name=`basename "$0"`
source_dev=$1
target_dir=$2
subtestname=$3

workingdir=$PWD
cvt_logs_dir="$workingdir/cvt_logs"
cvt_log_file="$cvt_logs_dir/cvtlog_$subtestname.txt"
cvt_op_file="$cvt_logs_dir/cvt_$subtestname.txt"

execute_cmd()
{
    cmd="$1"
    dont_exit_on_fail=0

    if [[ -n "$2" ]]; then
        dont_exit_on_fail=1
    fi

    echo "Executing command: $cmd"
    #op=$($cmd)
    eval $cmd
    funret=$?
    if [[ $funret != 0 ]]; then
        printf 'command: %s failed with return value %d\n' "$cmd" "$funret"
        if [[ $dont_exit_on_fail == 0 ]]; then
            exit 1;
        fi
    fi
}

lsmodop=$(lsmod | grep "involflt")
if [[ -z "${lsmodop// }" ]]; then
    echo "Driver not loaded, exiting"
    exit 1
fi

if [[ ! -e $target_dir ]]; then
    echo "Target directory $target_dir doesn't exist"
    exit 1
fi

if [[ ! -e $source_dev ]]; then
    echo "Source dev $source_dev doesn't exist"
    exit 1
fi

sourcedevsize=`blockdev --getsize64 /dev/sdb`
if [[ $sourcedevsize > 1073741824 ]]; then
    echo "Source dev $source_dev size is $sourcedevsize"
    echo "CVT works on source dev of size <= 1GB"
    exit 1
fi

execute_cmd "mkdir -p $cvt_logs_dir"

if [ -f /etc/redhat-release ] && grep -q 'CentOS release 5.11 (Final)' /etc/redhat-release; then
    execute_cmd "time $workingdir/indskflt_ct_rhel5 --tc=ditest --loggerPath=$cvt_logs_dir --pair[ -type=d-f -sd=$source_dev -td=$target_dir/target_file.tgt -subtest=$subtestname -log=$cvt_log_file ] > $cvt_op_file 2>&1" "ignorefail"
else
    execute_cmd "time $workingdir/indskflt_ct --tc=ditest --loggerPath=$cvt_logs_dir --pair[ -type=d-f -sd=$source_dev -td=$target_dir/target_file.tgt -subtest=$subtestname -log=$cvt_log_file ] > $cvt_op_file 2>&1" "ignorefail"
fi

#inm_shut_path=/usr/local/ASR/Vx/bin/inmshutnotify
#execute_cmd "$inm_shut_path"

grepop=$(cat $cvt_op_file | grep -i "DI Test Passed")
if [[ -z "${grepop// }" ]]; then
	echo "CVT test failed"
	exit 1
else
	echo "CVT test passed"
	exit 0
fi

