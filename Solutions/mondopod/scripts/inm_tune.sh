#!/bin/sh

PATH=/bin:/usr/bin:/usr/sbin:/sbin:$PATH

ECHO=/bin/echo
SED=/bin/sed
AWK=/usr/bin/awk
DU=/usr/bin/du
ITADM=/usr/sbin/itadm
STMFADM=/usr/sbin/stmfadm

NULL=/dev/null

PLATFORM=`uname | tr -d "\n"`
HOSTNAME=`hostname`
LU='lu'

SetupLogs()
{
    dirName=`dirname $0`
    orgWd=`pwd`

    cd $dirName
    SCRIPT_DIR=`pwd`
    cd $orgWd

    LOG_FILE=$SCRIPT_DIR/inm_tune.log

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

    INSTALLATION_DIR=`grep INSTALLATION_DIR /usr/local/.sms_version| cut -d'=' -f2`
    SMARTCTL=${INSTALLATION_DIR}/bin/smartctl

}

log()
{
    $ECHO "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ $*" >> $LOG_FILE
    return
}

ConvertLuns()
{
    for lu in `stmfadm list-lu  | awk '{print $3}'`
    do
        log "LU : $lu"
        data_file=`stmfadm list-lu -v $lu | grep 'Data File' | awk '{print $4}'`
        log "Data File : $data_file"
        echo $data_file | grep -w dsk > /dev/null
        if [ $? -eq 0 ]; then
            entry=`stmfadm list-view -vl $lu | grep "View Entry" | awk '{print $3}'`
            host_group=`stmfadm list-view -vl $lu | grep "Host group" | awk '{print $4}'`
            target_group=`stmfadm list-view -vl $lu | grep "Target Group" | awk '{print $4}'`
            lun=`stmfadm list-view -vl $lu | grep "LUN" | awk '{print $3}'`
            state=`stmfadm list-lu -v $lu | grep "Operational Status" | awk '{print $4}'`

            log "Entry : $entry - Host Group : $host_group - Target Group : $target_group - LUN : $lun - State : $state"

            if [ "$state" = "Online" ]; then
                log "stmfadm offline-lu $lu"
                stmfadm offline-lu $lu 
                if [ $? -ne 0 ]; then
                    log "Failed: stmfadm offline-lu $lu "
                    echo "Failed: stmfadm offline-lu $lu "
                    return
                fi
            fi

            log "stmfadm remove-view -l $lu $entry"
            stmfadm remove-view -l $lu $entry
            if [ $? -ne 0 ]; then
                log "Failed: stmfadm remove-view -l $lu $entry"
                echo "Failed: stmfadm remove-view -l $lu $entry"
                return
            fi

            log "stmfadm delete-lu $lu"
            stmfadm delete-lu $lu 
            if [ $? -ne 0 ]; then
                log "Failed: stmfadm delete-lu $lu "
                echo "Failed: stmfadm delete-lu $lu "
                return
            fi

            orgVol=`echo $data_file | awk -F/ '{print $5"/"$6}'`
            newVol=`echo $data_file | awk -F/ '{print $5"/new"$6}'`

            log "zfs rename $orgVol $newVol"
            zfs rename $orgVol $newVol
            if [ $? -ne 0 ]; then
                log "Failed: zfs rename $orgVol $newVol"
                echo "Failed: zfs rename $orgVol $newVol"
                return
            fi

            new_data_file=`echo /dev/zvol/rdsk/${newVol}`

            log "stmfadm import-lu $new_data_file"
            stmfadm import-lu $new_data_file
            if [ $? -ne 0 ]; then
                log "Failed: stmfadm import-lu $new_data_file"
                echo "Failed: stmfadm import-lu $new_data_file"
                return
            fi

            log "stmfadm modify-lu -p wcd=false $lu" 
            stmfadm modify-lu -p wcd=false $lu 
            if [ $? -ne 0 ]; then
                log "Failed: stmfadm modify-lu -p wcd=false $lu "
                echo "Failed: stmfadm modify-lu -p wcd=false $lu "
                return
            fi

            log "stmfadm add-view -n $lun -h $host_group -t $target_group $lu "
            stmfadm add-view -n $lun -h $host_group -t $target_group $lu 
            if [ $? -ne 0 ]; then
                log "Failed: stmfadm add-view -n $lun -h $host_group -t $target_group $lu "
                echo "Failed: stmfadm add-view -n $lun -h $host_group -t $target_group $lu "
                return
            fi
        fi
    done

    stmfadm list-lu -v

    return
}

# update param
UpdateParams()
{
    reboot_required=0

    grep -v "^\*" /etc/system |  grep zfs_arc_max > /dev/null
    if [ $? -ne 0 ]; then
        phyMem=`prtconf | grep "Memory size:" | awk '{print $3}'`
        qualifier=`prtconf | grep "Memory size:" | awk '{print $4}'`
        if [ "$qualifier" = "Megabytes" ]; then
            arcMax=`expr $phyMem \* 1024 \* 1024`
        elif [ "$qualifier" = "Gigabytes" ]; then
            arcMax=`expr $phyMem \* 1024 \* 1024 \* 1024`
        else
            echo "unknown memory size qualifier"
            exit 1
        fi

        # less 16GB
        arcMax=`expr $arcMax - 17179869184`

        echo "set zfs:zfs_arc_max=$arcMax" >> /etc/system
        reboot_required=1
    fi

    grep -v "^\*" /etc/system | grep l2arc_write_boost > /dev/null
    if [ $? -ne 0 ]; then
        echo "set zfs:l2arc_write_boost=33554432" >> /etc/system
    fi

        

    grep -v "^\*"  /kernel/drv/sd.conf | grep cache-nonvolatile > /dev/null
    if [ $? -ne 0 ]; then

        numIntels=`iostat -E | grep -i intel | awk '{print $4 "_" $5}' | uniq | wc -l | sed 's/ //g'`
        numSamsung=`iostat -E | grep -i samsung | awk '{print $4 "_" $5}' | uniq | wc -l | sed 's/ //g'`
        numCrucials=`iostat -E | grep -i crucial | awk '{print $4}' | uniq | wc -l | sed 's/ //g'`

        totalSsds=`expr $numIntels + $numSamsung + $numCrucials`

        echo "sd-config-list=\c" >> /kernel/drv/sd.conf

        numSet=0
        for disk in `iostat -E | grep -i intel | awk '{print $4 "_" $5}' | uniq` 
        do
            product=`echo $disk | sed 's/_/ /g'`
            echo "\"ATA     $product\",\"cache-nonvolatile:true\"\c" >> /kernel/drv/sd.conf

            numSet=`expr $numSet + 1`
            if [ $totalSsds -eq $numSet ]; then
                echo ";" >> /kernel/drv/sd.conf
            else
                echo "," >> /kernel/drv/sd.conf
                echo "               \c" >> /kernel/drv/sd.conf
            fi
        done

        for disk in `iostat -E | grep -i samsung | awk '{print $4 "_" $5}' | uniq` 
        do
            product=`echo $disk | sed 's/_/ /g'`
            echo "\"ATA     $product\",\"cache-nonvolatile:true\"\c" >> /kernel/drv/sd.conf

            numSet=`expr $numSet + 1`
            if [ $totalSsds -eq $numSet ]; then
                echo ";" >> /kernel/drv/sd.conf
            else
                echo "," >> /kernel/drv/sd.conf
                echo "               \c" >> /kernel/drv/sd.conf
            fi
        done

        for disk in `iostat -E | grep -i crucial | awk '{print $4}' | uniq`
        do
            echo "\"ATA     $disk\",\"cache-nonvolatile:true\"\c" >> /kernel/drv/sd.conf

            numSet=`expr $numSet + 1`
            if [ $totalSsds -eq $numSet ]; then
                echo ";" >> /kernel/drv/sd.conf
            else
                echo "," >> /kernel/drv/sd.conf
                echo "               \c" >> /kernel/drv/sd.conf
            fi
        done
    fi

    for disk in `iostat -en | awk '{print $NF}' | grep d0`
    do
        log `$SMARTCTL -d scsi -s wcache=on /dev/rdsk/${disk}`
    done

    grep -v "^\*" /etc/system | grep zfs_unmap_ignore_size > /dev/null   
    if [ $? -ne 0 ]; then   
        echo "set zfs:zfs_unmap_ignore_size=0" >> /etc/system   
        reboot_required=1
    fi

    grep -v "^\*" /etc/system | grep apic_panic_on_nmi > /dev/null   
    if [ $? -ne 0 ]; then   
        echo "set pcplusmp:apic_panic_on_nmi=1" >> /etc/system
        echo "set apix:apic_panic_on_nmi=1" >> /etc/system
        reboot_required=1
    fi

    
    for pool in `zpool list -H | grep -v ^rpool | awk '{print $1}'`
    do
        zfs set compression=gzip $pool
        zfs set recordsize=32k $pool
    done

    if [ $reboot_required -eq 1 ]; then
       echo ""
       echo "ATTENTION: Reboot is required for the changes to take effect." 
       echo ""
    fi
}

SetupLogs

# ConvertLuns

UpdateParams

exit 0
