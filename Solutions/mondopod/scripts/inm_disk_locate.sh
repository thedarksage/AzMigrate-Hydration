#!/bin/sh
#=================================================================
# FILENAME
#    inm_disk_locate.sh
#
# DESCRIPTION
#    To locate the disks 
#    
# HISTORY
#     <12/17/2013>  Vishnu     Created.
#
# REVISION : $Revision: 1.3 $ 
#
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================

PATH=/bin:/usr/bin:/usr/sbin:/sbin:$PATH

ECHO=/bin/echo
DU=/usr/bin/du
Diff=

SetupLogs()
{
   dirName=`dirname $0`
   orgWd=`pwd`

   cd $dirName
   SCRIPT_DIR=`pwd`
   cd $orgWd

   LOG_FILE=$SCRIPT_DIR/inm_disk_locate.log

   if [ -f $LOG_FILE ]; then
       size=`$DU -k $LOG_FILE | awk '{ print $1 }'`
       if [ $size -gt 5000 ]; then
           $CP -f $LOG_FILE $LOG_FILE.1
           > $LOG_FILE
       fi
   else
       > $LOG_FILE
       chmod a+w $LOG_FILE
   fi
}

log()
{
   $ECHO "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ $*" >> $LOG_FILE
   return
}

GetAllSlots()
{
    log " Enter GetAllSlots"

    for disk in `iostat -iEn | grep c[0-9]t | awk '{print $1}'`
    do
        slot=`prtconf -v /dev/rdsk/${disk} | egrep "name=|value=" | sed 'N;s/\n/ /' | grep -w attached-port-pm | awk '{print $4}' | awk -F= '{print $2}' | sed "s/'//g"`
        encl=`prtconf -v /dev/rdsk/${disk} | egrep "name=|value=" | sed 'N;s/\n/ /' | grep -w attached-port | grep -v pm | awk '{print $4}' | awk -F= '{print $2}' | sed "s/'//g"`

        num0s=`echo $slot | wc -c | sed 's/ //g'`
        bitPos=`echo $slot | sed 's/0//g'`

        case $bitPos in
        8)
            loc=4
            ;;
        4)
            loc=3
            ;;
        2)
            loc=2
            ;;
        1)
            loc=1
            ;;
        esac

       Slot=`expr $num0s - $Diff`
       Slot=`expr $Slot \* 4`
       Slot=`expr $Slot + $loc`

        echo "$encl $Slot $disk"
        log "$encl $Slot $disk"
    done

    log " Exit GetAllSlots"
}

GetSlot()
{
    log "Enter GetSlot "

    disk=$1
    Diff=$2 

    dd if=/dev/rdsk/${disk} of=/dev/null count=1 > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        log "Device /dev/rdsk/${disk} not accessible."
        return
    fi

    slot=`prtconf -v /dev/rdsk/${disk} | egrep "name=|value=" | sed 'N;s/\n/ /' | grep -w attached-port-pm | awk '{print $4}' | awk -F= '{print $2}' | sed "s/'//g"`
    encl=`prtconf -v /dev/rdsk/${disk} | egrep "name=|value=" | sed 'N;s/\n/ /' | grep -w attached-port | grep -v pm | awk '{print $4}' | awk -F= '{print $2}' | sed "s/'//g"`

    num0s=`echo $slot | wc -c | sed 's/ //g'`
    bitPos=`echo $slot | sed 's/0//g'`

    case $bitPos in
    8)
        loc=4
        ;;
    4)
        loc=3
        ;;
    2)
        loc=2
        ;;
    1)
        loc=1
        ;;
    esac

   Slot=`expr $num0s - $Diff`
   Slot=`expr $Slot \* 4`
   Slot=`expr $Slot + $loc`

   echo "${encl:1} $Slot $disk"
   log "${encl:1} $Slot $disk"

   log "Exit GetSlot "

}

GetDiff()
{
    log "Enter GetDiff"

    Diffs=
    typeset -A DiffCount

    for slot_disk in `diskinfo | grep disk | awk -F/ '{print $5,$6}' | awk '{print $1"_"$3}'`
    do

        disk=`echo $slot_disk | awk -F_ '{print $3}'`
        dislot=`echo $slot_disk | awk -F_ '{print $2}' | sed 's/^0//g'`
        log "diskinfo device: $disk - slot: $dislot"
        if [ ! -c "/dev/rdsk/${disk}" ]; then
            log "Device /dev/rdsk/${disk} not found."
            continue
        fi

        dd if=/dev/rdsk/${disk} of=/dev/null count=1 > /dev/null 2>&1
        if [ $? -ne 0 ]; then
            log "Device /dev/rdsk/${disk} not accessible."
            continue
        fi

        pcslot=`prtconf -v /dev/rdsk/${disk} | egrep "name=|value=" | sed 'N;s/\n/ /' | grep -w attached-port-pm | awk '{print $4}' | awk -F= '{print $2}' | sed "s/'//g"`
        log "prtconf slot - $pcslot"

        chknum0s=`echo $pcslot | wc -c | sed 's/ //g'`
        chkbitPos=`echo $pcslot | sed 's/0//g'`

        log "$chknum0s - $chkbitPos"

        case $chkbitPos in
        8)
            chkloc=4
            ;;
        4)
            chkloc=3
            ;;
        2)
            chkloc=2
            ;;
        1)
            chkloc=1
            ;;
       esac 

       chkSlot=`expr $dislot - $chkloc`
       chkDiff=`expr $chkSlot / 4`
       chkDiff=`expr $chknum0s - $chkDiff`

        log "$chkSlot - $chkDiff"
   
        if [ -z "${DiffCount[$chkDiff]}" ]; then
            DiffCount[$chkDiff]=1
            Diffs="$Diffs $chkDiff"
        else
            DiffCount[$chkDiff]=`expr ${DiffCount[$chkDiff]} + 1`
        fi

    done

    for diff in $Diffs
    do
        log "For diff $diff - count is ${DiffCount[$diff]}"
        if [ -z "$Diff" ]; then
            Diff=$diff
        else
            if [ ${DiffCount[$diff]} -gt ${DiffCount[$Diff]} ]; then
                Diff=$diff
            fi
        fi
    done

    if [ -z "$Diff" ]; then
        log "Error: diff could not be found"
        exit 1
    fi

    log "using diff as $Diff"

    log "Exit GetDiff"
}

Usage()
{
    $ECHO "usage: $0 <Disk>"
    return
}

if [ $# -ne 1 ];then
    Usage
    exit 1
fi

SetupLogs

GetDiff

GetSlot $1 $Diff

exit 0
