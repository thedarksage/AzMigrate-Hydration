#!/bin/bash

if [[ $# -ne 1 ]]; then
    echo "Insufficient number of args"
    exit 1
fi

INSTALL_DIR=$1
inm_dmit=$INSTALL_DIR/inm_dmit

sd_devices=`ls -1 /dev/sd*`
for i in $sd_devices; do
    op=`$inm_dmit --op=get_blk_mq_status --src_vol=$i | grep -i 'enabled'`
    ret=$?
    #echo "i: $i, ret: $ret"
    if [[ $ret == 0 ]]; then
        echo "blk_mq enabled"
        exit 1
    fi
done

echo "blk_mq disabled"
