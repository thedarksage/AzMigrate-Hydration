#! /bin/bash
psnap=$1
log_dir=/vpl.log
source /root/.bashrc

echo "------------Calling aws_recover.sh $$" 2>&1 >> $log_dir
date 2>&1 >> $log_dir
echo "" 2>&1 >> $log_dir
echo "Exiting SCRIPT " 2>&1 >> $log_dir
exit 0
