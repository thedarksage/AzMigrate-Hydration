#!/bin/sh

REMOTE_LOG_LEVEL="ERROR"
LOG_LEVELS=

PATH=/bin:/usr/bin:/usr/sbin:/sbin:$PATH

ECHO=/bin/echo
SED=/bin/sed
AWK=/usr/bin/awk
DU=/usr/bin/du

NULL=/dev/null

PLATFORM=`uname | tr -d "\n"`
HOSTNAME=`hostname`

TEMPFILE1=/tmp/monitor.$$
SSD_DISKS=

SetupLogs()
{
    dirName=`dirname $0`
    orgWd=`pwd`

    cd $dirName
    SCRIPT_DIR=`pwd`
    cd $orgWd

    LOG_FILE=$SCRIPT_DIR/inm_monitor.log

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

    INM_DISK_DAT=$SCRIPT_DIR/inm_disk.dat
    FMADM_ALERT_LOG=$SCRIPT_DIR/inm_alert.dat

    INSTALLATION_DIR=`grep INSTALLATION_DIR /usr/local/.sms_version| cut -d'=' -f2`
    SMARTCTL=$INSTALLATION_DIR/bin/smartctl

    case $REMOTE_LOG_LEVEL in
    DEBUG)
        LOG_LEVELS="DEBUG ERROR ALERT"
        ;;
    ERROR)
        LOG_LEVELS="ERROR ALERT"
        ;;
    ALERT)
        LOG_LEVELS="ALERT"
        ;;
    esac

    log "REMOTE_LOG_LEVEL: $REMOTE_LOG_LEVEL - LOG_LEVELS: $LOG_LEVELS"
}


log()
{
    $ECHO "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ $*" >> $LOG_FILE
    return
}

Cleanup()
{
    log "Exiting alert monitor."
    rm -rf $TMP_OUTPUT_FILE > $NULL
}

CleanupError()
{
    rm -rf $TMP_OUTPUT_FILE > $NULL
    exit 1
}

StorageLogger()
{
    level=$1
    message=$2
    echo $LOG_LEVELS | grep -w $level > $NULL
    if [ $? -eq 0 ]; then
        date=`date +(%y-%m-%d %H:%M:%S)`
        /usr/bin/php -f /var/apache2/2.2/htdocs/Storagelogger.php "${date}:"  Monitor $level "Mondopod: $message" >/dev/null
    fi

    log "$message"
}

GetZpoolStatus()
{
    for pool in `zpool list -H | awk '{print $1}'`
    do
        poolStatus=`zpool list -H $pool | awk '{print $7}'`
        log "pool $pool is $poolStatus"
        
        case $poolStatus in
        FAULTED)
            StorageLogger ALERT "Pool $pool faulted." 
            ;;
        UNAVAIL)
            StorageLogger ALERT "Pool $pool unavailable." 
            ;;
        DEGRADED)
            StorageLogger ERROR "Pool $pool has degraded." 
            ;;
        ONLINE)
            StorageLogger DEBUG "Pool $pool online." 
            ;;
        esac
    done
}
    
GetDiskStatus()
{
    for pool in `zpool list -H | awk '{print $1}'`
    do
        zpool status $pool > $TEMPFILE1
        for disk in `grep c[0-9]t $TEMPFILE1 | awk '{print $1}'`
        do
            diskStatus=`grep $disk $TEMPFILE1 | awk '{print $2}'`

            case $diskStatus in
            FAULTED)
                StorageLogger ALERT "Disk $disk faulted." 
                ;;
            UNAVAIL)
                StorageLogger ALERT "Disk $disk unavailable." 
                ;;
            DEGRADED)
                StorageLogger ERROR "Disk $disk degraded." 
                ;;
            ONLINE)
                StorageLogger DEBUG "Disk $disk online." 
                ;;
            AVAIL)
                StorageLogger DEBUG "Hot spare disk $disk available." 
                ;;
            INUSE)
                StorageLogger DEBUG "Hot spare disk $disk inuse." 
                ;;
            esac

            $SMARTCTL -i -d scsi /dev/rdsk/${disk} | grep "Rotation Rate:" | grep "Solid State Device" > /dev/null
            if [ $? -eq 0 ]; then
                SSD_DISKS="$SSD_DISKS $disk"
                log "SSD_DISKS: $SSD_DISKS"
            fi

        done
    done

    rm -f $TEMPFILE1

    log "EXIT: GetDiskStatus"
}

GetSsdDiskStatus()
{
    for disk in $SSD_DISKS
    do
        availSpace=`$SMARTCTL -a -d sat /dev/rdsk/${disk} | grep Available_Reservd_Space | tail -1 | awk '{print $4}'`
        wearIndicator=`$SMARTCTL -a -d sat /dev/rdsk/${disk} | grep Media_Wearout_Indicator | awk '{print $4}'`

        StorageLogger DEBUG "SSD disk $disk : Available_Reservd_Space $availSpace : Media_Wearout_Indicator $wearIndicator"

        if [ ! -z "$availSpace" ]; then
            if [ $availSpace -lt 10 ]; then
                StorageLogger ALERT "SSD disk $disk : Available_Reservd_Space $availSpace "
            fi
        fi

        if [ ! -z "$wearIndicator" ]; then
            if [ $wearIndicator -lt 10 ]; then
                StorageLogger ALERT "SSD disk $disk : Media_Wearout_Indicator $wearIndicator"
            fi
        fi
    done

    return
}

GetZvolStatus()
{
    return
}

GetNicStatus()
{
    return
}

GetFmadmFaults()
{
    log "ENTER: GetFmadmFaults"
    if [ ! -f $FMADM_ALERT_LOG ]; then
        >$FMADM_ALERT_LOG
    fi
    FMDUMP=/tmp/fmdump.$$
    fmdump >$FMDUMP
    for uid in `grep Diagnosed $FMDUMP | awk '{print $4}'`
    do 
        grep $uid $FMDUMP | grep Resolved >/dev/null
        if [ $? -ne 0 ]; then  
            grep $uid $FMDUMP | grep Diagnosed | egrep "USB" >/dev/null
            if [ $? -eq 0 ]; then  
                log "Skip alert for $uid" 
                continue
            fi
            grep $uid $FMADM_ALERT_LOG > /dev/null
            if [ $? -ne 0 ]; then
                log "Require attention for $uid" 
                message=`fmdump -v -u $uid | grep FRU:`
                StorageLogger ALERT "The faulty FRU requires attention : $message"
                echo $uid >> $FMADM_ALERT_LOG
            else
                log "Alert has already been sen for $uid"
            fi
        else
            grep $uid $FMADM_ALERT_LOG > /dev/null
            if [ $? -eq 0 ]; then
                log "Resolved $uid. Removing from alert log" 
                TMP_FMADM_ALER_LOG=/tmp/inm_alert.dat.$$
                grep -v $uid $FMADM_ALERT_LOG > $TMP_FMADM_ALER_LOG
                mv -f $TMP_FMADM_ALER_LOG $FMADM_ALERT_LOG
            fi
        fi
    done

    rm -f $FMDUMP

}

################# MAIN ####################

trap Cleanup 0

if [ "$PLATFORM" = "SunOS" ]; then
    trap CleanupError 1 2 3 15
else
    $ECHO "Error: Unsupported Operating System $PLATFORM."
    exit 1
fi

TMP_OUTPUT_FILE=/tmp/inm_alert.dat
while [ -f $TMP_OUTPUT_FILE ]
do
    log "Error: already running."
    sleep 2
done

> $TMP_OUTPUT_FILE


SetupLogs

GetZpoolStatus

GetDiskStatus

GetSsdDiskStatus

GetZvolStatus

GetNicStatus

GetFmadmFaults

rm -f $TMP_OUTPUT_FILE >$NULL

exit 0
