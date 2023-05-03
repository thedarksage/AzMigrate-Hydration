#!/bin/sh

dirName=`dirname $0`
orgWd=`pwd`

cd $dirName
SCRIPT_PATH=`pwd`
cd $orgWd

{
for disk in `paste -d= <(iostat -x | awk '{print $1}') <(iostat -xn | awk '{print $NF}') | grep ^sd`
do
        echo "-----------------------------------------------"
        echo "Disk : $disk"
        sdname=`echo $disk | awk -F= '{print $1}'`
        ctname=`echo $disk | awk -F= '{print $2}'`
        sdnum=`echo $sdname | sed 's/sd//g'`
        sdnumhex=`echo "ibase=10;obase=16;$sdnum" | bc`

        sdptr=`echo "*sd_state::softstate $sdnumhex" | mdb -k`
        echo "${sdptr}::print struct sd_lun" |mdb -kw | egrep "cache|block"
        echo "-----------------------------------------------"
done
} > ${SCRIPT_PATH}/sd_params.log

echo "Log is available at - ${SCRIPT_PATH}/sd_params.log"
