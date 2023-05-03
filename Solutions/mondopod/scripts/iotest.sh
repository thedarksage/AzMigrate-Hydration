#!/bin/sh

Cleanup()
{
    echo "Complete."
}

CleanupError()
{
    recvdSignal=1
    echo "Exiting..."
}


WRITE=0
NUM_REGIONS=1

dirName=`dirname $0`
orgWd=`pwd`

cd $dirName
SCRIPT_PATH=`pwd`
cd $orgWd

recvdSignal=0
trap Cleanup 0

trap CleanupError 1 2 3 15


echo "This utility is to write/read from the disks. Do you want to continue[Y/y]?"
read ANS

if [ "$ANS" != "Y" -o "$ANS" != "y" ]; then
    exit
fi

echo "Log is available at - ${SCRIPT_PATH}/iotest.log"


{
inuseDisks=`zpool status | grep d0 | grep "^c*" | awk '{print $1}'`

echo
echo "In-use disks : $inuseDisks"
echo

for disk in `paste -d= <(iostat -x | awk '{print $1}') <(iostat -xn | awk '{print $NF}') | grep ^sd`
do
    if [ $recvdSignal -eq 1 ]; then
        echo "exiting.."
        break
    fi
    sdname=`echo $disk | awk -F= '{print $1}'`
    ctname=`echo $disk | awk -F= '{print $2}'`
    cap=`iostat -En $ctname | grep Size: | awk '{print $3}' | sed 's/<//g'`
    VenMod=`iostat -En $ctname | grep Vendor:`


    echo "-----------------------------------------------"
    echo "Disk : $sdname - $ctname - Capacity : $cap"
    echo "$VenMod"

    jump=`expr $cap / $NUM_REGIONS`
    jump=`expr $jump / 131584`
    region=0
    while [ $region -lt $NUM_REGIONS ]
    do

        if [ $recvdSignal -eq 1 ]; then
            echo "exiting."
            break
        fi

        seek=`expr $jump \* $region`
        region=`expr $region + 1`

        echo "dd if=/dev/rdsk/${ctname}s0 of=/dev/null bs=128k iseek=${seek} count=8k"
        time dd if=/dev/rdsk/${ctname}s0 of=/dev/null bs=128k iseek=${seek} count=8k
        if [ $? -ne 0 ]; then
            break
        fi
        echo
        echo

        if [ $WRITE -eq 1 ]; then
            echo $inuseDisks | grep $ctname > /dev/null
            if [ $? -eq 0 ]; then
                echo "Skipping writes to $ctname"
                continue
            fi

            echo "dd of=/dev/rdsk/${ctname}s0 if=/dev/zero bs=128k oseek=${seek} count=8k"
            time dd of=/dev/rdsk/${ctname}s0 if=/dev/zero bs=128k oseek=${seek} count=8k
            if [ $? -ne 0 ]; then
                break
            fi
            echo
            echo
        fi
    done
done
} > ${SCRIPT_PATH}/iotest.log 2>&1 


