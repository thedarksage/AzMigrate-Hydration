#!/bin/bash

scriptsdir=/root/LinuxCVT

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

if [[ $# -ne 1 ]]; then
    echo "Insufficient number of args"
    exit 1
fi

machine=$1

# Save yesterday logs for debugging
execute_cmd "cp -r /root/LinuxCVT /root/LinuxCVT_old" "ignorefail"
execute_cmd "/usr/local/ASR/uninstall.sh -Y" "ignorefail"
execute_cmd "rm -rf /root/LinuxCVT" "ignorefail"
execute_cmd "sshpass -p Password~1 scp -r -o StrictHostKeyChecking=no root@$machine:/root/workspace/host/drivers/InVolFlt/tests/LinuxCVT $scriptsdir"

execute_cmd "cd $scriptsdir"
execute_cmd "$scriptsdir/setupcvt.sh 9.11 > $scriptsdir/initop.txt 2>&1"

for TEST in $(cat $scriptsdir/testcases)
do
	echo "Starting $TEST test"
	$scriptsdir/startcvt.sh /dev/sdb /target_dev_sdc/target_files/ $TEST > $scriptsdir/startcvtop_$TEST.txt 2>&1
        ret=$?
        if [[ $ret != 0 ]]; then
		sleep 500
                # might have failed to take barrier, retry
                $scriptsdir/startcvt.sh /dev/sdb /target_dev_sdc/target_files/ $TEST > $scriptsdir/startcvtop_$TEST.txt 2>&1
        fi
	sleep 500
done
