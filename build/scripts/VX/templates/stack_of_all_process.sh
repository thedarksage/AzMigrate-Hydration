#!/bin/bash

# Check if an argument was passed
if [ $# -eq 0 ]; then
    echo "No argument provided. Using the current directory as LogDirectory."
    supportstore=$(pwd)
else
    supportstore="$1"
fi

# Construct the log file path based on the supportstore
logfile="$supportstore/process_stack.log"

echo "logfile: ${logfile}"

# Redirect all output to the log file
exec > "$logfile"
exec 2>&1

echo "Protected disks"
protected_disks=$(/etc/vxagent/bin/inm_dmit --get_protected_volume_list)
echo "$protected_disks"

echo "=============="
/etc/vxagent/bin/inm_dmit --get_volume_stat "$protected_disks"
echo "----------end----------"

echo "Process list is as follows"
ps -ef

for pid in $(ps -ef | awk '{print $2}')
do
    echo "PID: ${pid}"
    echo "Comm: $(cat /proc/${pid}/comm)"
    echo "============Start================"
    
    # Exception handling for reading stack information
    if [ -d "/proc/${pid}/task" ]; then
        for tid in $(ls /proc/${pid}/task)
        do
            echo "Tid: ${tid}"
            if [ -e "/proc/${pid}/task/${tid}/stack" ]; then
                cat "/proc/${pid}/task/${tid}/stack"
            else
                echo "Error reading stack for TID ${tid}"
            fi
        done
    else
        echo "Error reading task information for PID ${pid}"
    fi
    
    echo "------------End------------------"
done

echo "Done"

