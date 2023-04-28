#!/bin/sh
#=================================================================
# FILENAME
#    inm_zvol_export.sh
#
# DESCRIPTION
# Creates and export a zvol over iSCSI
#    
# HISTORY
#     <10/07/2013>  Vishnu     Created.
#
# REVISION : $Revision: 1.7 $
#
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================


PATH=/bin:/usr/bin:/usr/sbin:/sbin:$PATH

ECHO=/bin/echo
SED=/bin/sed
AWK=/usr/bin/awk
AWK1=/usr/xpg4/bin/awk
DU=/usr/bin/du
ITADM=/usr/sbin/itadm
STMFADM=/usr/sbin/stmfadm

NULL=/dev/null

PLATFORM=`uname | tr -d "\n"`
HOSTNAME=`hostname`
LU='lu'

Output()
{
    $ECHO $* >> $TMP_OUTPUT_FILE
    return
}

SetupLogs()
{
    dirName=`dirname $0`
    orgWd=`pwd`

    cd $dirName
    SCRIPT_DIR=`pwd`
    cd $orgWd

    LOG_FILE=$SCRIPT_DIR/inm_lun_export.log

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

Cleanup()
{
    $ECHO "Exiting zvol wizard."
    rm -rf $TMP_OUTPUT_FILE > $NULL
}

CleanupError()
{
    rm -rf $TMP_OUTPUT_FILE > $NULL
    exit 1
}

WriteToConfFile()
{
    $ECHO $* >> $APP_CONF_FILE 
    return
}

ReadAns()
{
    if [ ! -z "$PROMPT" ]; then
        if [ "$PROMPT" = "-y" -o "$PROMPT" = "-Y" ]; then
            ANS=Y
            return
        fi
    fi
    $ECHO $* "[Y/N]:"

    while :
    do
        read ANS
        if [ "$ANS" = "y" -o "$ANS" = "Y" -o "$ANS" = "n" -o "$ANS" = "N" ]; then
            break
        fi
        $ECHO "Please enter [Y/N]:"
    done

}

ReadOpt()
{
    options=$1
    $ECHO "Please select from $options:"

    while :
    do
        read OPT
        echo $options | grep -w $OPT > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            break
        fi
        $ECHO "Please select from $options:"
    done
}

Run()
{
    if [ $TEST -eq 1 ]; then
        log "$*"
        return 0
    fi

    $*
}


UpdateConfig()
{
    while :
    do
        clear
        $ECHO
        $ECHO
        $ECHO "Welcome to InMage Storage Management wizard!"
        $ECHO
        $ECHO
        $ECHO "1. Create a new zvol and export over iSCSI to Compute Node"
        $ECHO "2. Delete a zvol exported to Compute Node"
        $ECHO "3. Display existing zvols and their export status"
        $ECHO "9. Exit"
        $ECHO

        ReadOpt "1 2 3 9"

        case $OPT in
        1)
            CreateLu
            $ECHO "Please press enter to continue"
            read cr
            ;;
        2)
            DeleteLu
            $ECHO "Please press enter to continue"
            read cr
            ;;
        3)
            DisplayLu
            $ECHO "Please press enter to continue"
            read cr
            ;;
        9)
            return
            ;;
        esac
    done
}

CreateTpg()
{
    tpgName="StorageNodeTargetPortalGroup"

    GetIScsiIp
    
    cn_tpgs=
    NUM_FREE_PORTS=0
    typeset -A IpTpgs 
    for ip in $ISCSIPORTS
    do
        for tpg in `$ITADM list-tpg | grep -v "TARGET PORTAL GROUP" | awk '{print $1}'`
        do
            $ITADM list-tpg -v $tpg  | grep $ip >$NULL
            if [ $? -eq 0 ]; then
                log "TPG $tpg exists for IP $ip"
                echo $cn_tpgs | grep $tpg > $NULL
                if [ $? -ne 0 ]; then
                    cn_tpgs="$cn_tpgs $tpg"
                fi
                IpTpgs[$ip]=$tpg
            fi
        done

        if [ -z "${IpTpgs[$ip]}" ]; then
            log "TPG doesn't exist for IP $ip"
            NUM_FREE_PORTS=`expr $NUM_FREE_PORTS + 1`
        fi
    done

    log "NUM_FREE_PORTS: $NUM_FREE_PORTS - NUM_ISCSIPORTS :$NUM_ISCSIPORTS"

    if [ $NUM_FREE_PORTS -ne 0 -a $NUM_FREE_PORTS -lt $NUM_ISCSIPORTS ]; then
        log "Error: Not all ISCSI ports in TPGs. Requires manual intervention to continue."
        $ECHO "Error: Not all ISCSI ports in TPGs. Requires manual intervention to continue."
        exit 1 
    elif [ $NUM_FREE_PORTS -eq 0 ]; then
       return 
    else
        log "$ITADM create-tpg $tpgName $ISCSIPORTS"
        $ITADM create-tpg $tpgName $ISCSIPORTS
        if [ $? -ne 0 ]; then
            log "Error: Creation of TPG failed."
            exit 1
        fi
        cn_tpgs=$tpgName
    fi
}


CreateTarget()
{
    CreateTpg

    for tpg in $cn_tpgs
    do
        target_tpg_mapping_found=0
        for targetport in `$ITADM list-target | grep -v TARGET | $AWK '{print $1}'`
        do
            targetTpg=`$ITADM list-target -v $targetport | grep  tpg-tags: | awk '{print $2}'`
            if [ "$targetTpg" = "$tpg" ];then
                log "target $targetport has a tpg $targetTpg"
                target_tpg_mapping_found=1
                cn_targetport=$targetport
                break
            fi
        done

        if [ $target_tpg_mapping_found -eq 0 ]; then
            tpgs=`echo $cn_tpgs | sed 's/ /,/g'`
            log "$ITADM create-target -t $tpgs"
            $ITADM create-target -t $tpgs > /tmp/inm_targetname.$$
            if [ $? -ne 0 ];then
                log "Error: target creation failed"
                exit 1
            fi
            cn_targetport=`grep "successfully created" /tmp/inm_targetname.$$ | awk '{print $2}'`
            rm -f /tmp/inm_targetname.$$
            break
        fi
    done

    CreateTg
}

CreateTg()
{
    cn_tg=
    for tg in `$STMFADM list-tg | awk -F: '{print $2}'`
    do
        $STMFADM list-tg  -v $tg | grep $cn_targetport > $NULL
        if [ $? -eq 0 ]; then
            log "Target group already found"
            cn_tg=$tg
            return
        fi
    done

    if [ -z "$cn_tg" ]; then
        cn_tg="StorageNodeTargetGroup"
        $STMFADM offline-target $cn_targetport
        if [ $? -ne 0 ];then
            log "Error: target offline failed for $cn_targetport"
            $ECHO "Error: Failed to create target mapping"
            exit 1
        fi

        $STMFADM create-tg $cn_tg
        if [ $? -ne 0 ];then
            log "Error: target group creation failed"
            $ECHO "Error: Failed to create target mapping"
            exit 1
        fi

        $STMFADM add-tg-member -g $cn_tg $cn_targetport
        if [ $? -ne 0 ];then
            log "Error: target addition to tg failed"
            $ECHO "Error: Failed to create target mapping"
            exit 1
        fi

        $STMFADM online-target $cn_targetport
        if [ $? -ne 0 ];then
            log "Error: target online failed"
            $ECHO "Error: Failed to create target mapping"
            exit 1
        fi
    fi
}

CreateHg()
{
    iqn=$1
    cn_name=
    cn_hg=
    for hg in `$STMFADM list-hg | awk -F: '{print $2}'`
    do
        $STMFADM list-hg  -v $hg | grep Member: | grep $iqn > $NULL
        if [ $? -eq 0 ]; then
            cn_hg=$hg
            cn_name=`echo $cn_hg| $AWK -FH '{print $1}'`
            log "Found host group : $cn_hg for $cn_name"
        fi
    done

    if [ -z "$cn_name" ]; then
        ReadAns "Would you like the host group name be auto generated?"

        if [ "$ANS" = "n" -o "$ANS" = "N" ]; then
            while :
            do
                $ECHO
                $ECHO "Please enter host group name prefix (in lowercase):"
                read ANS
                if [ ! -z "$ANS" ]; then
                    cn_name=`echo $ANS | awk '{print tolower($0)}'`
                    log "Using name : $cn_name"
                fi
            done
        else
            NodeNumber=`$STMFADM list-hg | $AWK -F: '{print $2}' | grep ComputeNode | sort | tail -1 | $AWK -FH '{print $1}' | sed 's/[a-zA-Z]*//g'`
            if [ -z "$NodeNumber" ]; then
                NodeNumber=1
            else
                NodeNumber=`expr $NodeNumber + 1`
            fi
            cn_name="ComputeNode${NodeNumber}"
            log "Using name : $cn_name"
        fi
    fi

    if [ -z "$cn_hg" ]; then
        cn_hg=${cn_name}HostGroup
        log "CreateHg : $cn_name - $iqn - $cn_hg"

        $STMFADM create-hg $cn_hg
        if [ $? -ne 0 ];then
            $ECHO "Error: Failed to create host group."
            log "Error: $STMFADM create-hg $cn_hg"
            exit 1
        fi

        $STMFADM add-hg-member -g $cn_hg $iqn
        if [ $? -ne 0 ];then
            $ECHO "Error: Failed to create host group."
            log "Error: $STMFADM add-hg-member -g $cn_hg $iqn" 
            exit 1
        fi
    fi
}

GetZVolName()
{
    LuNumber=`zfs list -H -t volume -s name  | awk '{print $1}' | grep ^${cn_storage} | awk -F/ '{print $2}' | grep -w "${LU}[0-9]*" | sed 's/[a-z]*//g' | sort -n | tail -1`
    if [ -z "$LuNumber" ]; then
        LuNumber=1
    else
        LuNumber=`expr $LuNumber + 1`
    fi

    cn_zvol="${LU}${LuNumber}"
}

GetLun()
{
    max_lun=-1
    for lu in `stmfadm list-lu | awk '{print $3}'`
    do
        stmfadm list-view -l $lu | grep -w $cn_hg > $NULL
        if [ $? -eq 0 ]; then
            for entry in `stmfadm list-view -l $lu | grep 'View Entry' | awk '{print $3}'`
            do
                stmfadm list-view -l $lu $entry | grep -w $cn_hg > $NULL
                if [ $? -eq 0 ]; then
                    lun=`stmfadm list-view -l $lu $entry | grep -w LUN | awk '{print $3}'`
                    if [ $lun -gt $max_lun ]; then
                        max_lun=$lun
                    fi
                fi
            done
        fi
    done
    
    cn_lun=`expr $max_lun + 1`
}

CreateLu()
{
    numStoragePools=`zpool list -H | grep -v ^rpool | awk '{print $1}' | wc -l | sed 's/ //g'`
    if [ $numStoragePools -gt 1 ]; then
        while :
        do
            $ECHO
            $ECHO "Please select the storage pool on which the LU to be created : "
            zpool list -H | grep -v ^rpool | awk '{print $1}'
            $ECHO
            $ECHO "Please enter storage pool name :"
            read ANS
            if [ ! -z "$ANS" ]; then
                cn_storage=$ANS
                zpool list $cn_storage > /dev/null 2>&1
                if [ $? -ne 0 ]; then
                    $ECHO "Error: storage pool $cn_storage not found."
                    log "Error: storage pool $cn_storage not found."
                else
                    break
                fi
            fi
        done
    else
        cn_storage=`zpool list -H | grep -v ^rpool | awk '{print $1}'`
    fi

    if [ -z "$cn_storage" ]; then
        $ECHO "Error: No storage pool found. Please create a pool using inm_pool_create.sh."
        log "Error: No storage pool found. Please create a pool using inm_pool_create.sh."
        return
    fi

    GetZVolName

    freeSpace=`zfs list $cn_storage | grep -w $cn_storage | awk '{print $3}'`
   
    echo
    $ECHO "Available space in storage pool $cn_storage : $freeSpace" 
    echo

    while :
    do
        $ECHO "Please enter the LU size in MB/GB/TB (ex: 1T): "
        read ANS
        if [ ! -z "$ANS" ]; then
            # TODO : check the free space in pool aginst the requested
            log "zfs create -b 4k -V $ANS ${cn_storage}/${cn_zvol}"
            zfs create -b 4k -V $ANS ${cn_storage}/${cn_zvol}
            if [ $? -ne 0 ];then
                log "Error: zvol creation failed"
                return
            fi

            log "sbdadm create-lu /dev/zvol/rdsk/${cn_storage}/${cn_zvol}"
            sbdadm create-lu /dev/zvol/rdsk/${cn_storage}/${cn_zvol}
            if [ $? -ne 0 ];then
                log "Error: LU creation failed"
                return
            fi

            cn_lu=`sbdadm list-lu | grep -w ${cn_storage}/$cn_zvol | awk '{print $1}'`

            stmfadm modify-lu -p wcd=false $cn_lu

            break
        fi
    done

    $ECHO
    $ECHO "Please enter the iSCSI IQN(s) (space separated) to which the LU be exported: "
    read ANS
    for cn_iqn in `echo $ANS`
    do
        if [ ! -z "$cn_iqn" ]; then
            CreateHg $cn_iqn
            GetLun $cn_hg

            log "stmfadm add-view -n $cn_lun  -t $cn_tg -h $cn_hg $cn_lu"
            stmfadm add-view -n $cn_lun  -t $cn_tg -h $cn_hg $cn_lu
        fi
    done
}

DeleteLu()
{
    DisplayLu

    $ECHO "Please enter one or more LUs (space separated) to be deleted:  "
    read ANS
    for lu in `echo $ANS`
    do
        if [ ! -z "$lu" ]; then
            zvol=`sbdadm list-lu  | grep -wi $lu | awk '{print $3}'`
            if [ -z "$zvol" ]; then
                $ECHO "LU $lu not found"
                continue
            fi
            sbdadm delete-lu $lu
            if [ $? -ne 0 ]; then
                log "Error: deletion of $lu for zvol $zvol failed"
            fi
            zvol=`echo $zvol | $AWK -F/ '{ print $5 "/" $6}'`
            zfs destroy $zvol
            if [ $? -ne 0 ]; then
                log "Error: deletion of zvol $zvol failed"
            fi
        fi
    done
}

DisplayLu()
{

    for lu in `stmfadm list-lu | awk '{print $3}'`
    do
        lunSize=`sbdadm list-lu | grep -i $lu | awk '{print $2}'`
        lunSizeKb=`expr $lunSize / 1024`
        lunSizeMb=`expr $lunSizeKb / 1024`
        lunSizeGb=`expr $lunSizeMb / 1024`
        lunSizeTb=`expr $lunSizeGb / 1024`

        if [ $lunSizeTb -ne 0 ]; then
            lunSize="${lunSizeTb}TB"
        elif [ $lunSizeGb -ne 0 ]; then
            lunSize="${lunSizeGb}GB"
        elif [ $lunSizeMb -ne 0 ]; then
            lunSize="${lunSizeMb}MB"
        elif [ $lunSizeKb -ne 0 ]; then
            lunSize="${lunSizeKb}KB"
        fi

        echo "LU : $lu  -  Size : $lunSize  -  Volume : `sbdadm list-lu | grep -i $lu | sed 's/dev//g' | sed 's/zvol//g' | sed 's/rdsk//g' | sed 's/\// /g' | awk '{ print $3 "/" $4}'` "

        echo

        stmfadm list-view -vl $lu >$NULL 2>&1
        if [ $? -eq 0 ]; then
            stmfadm list-view -vl $lu | egrep "Host group|Target Group|Lun"
            echo
            echo
        fi
    done

}

GetIScsiIp()
{
    NUM_ISCSIPORTS=0
    ISCSIPORTS=
    for dev in `dladm show-phys | grep Ethernet | grep 10000 | awk '{print $1}'`
    do
        ipaddr=`ipadm show-addr -o addr ${dev}/san | grep -iv addr | awk -F/ '{print $1}'`
        if [ ! -z "$ipaddr" ]; then
            ISCSIPORTS="$ISCSIPORTS $ipaddr"
            NUM_ISCSIPORTS=`expr $NUM_ISCSIPORTS + 1`
        fi
    done

    if [ -z "$ISCSIPORTS" ]; then
        for dev in `dladm show-phys | grep Ethernet | grep 1000 | awk '{print $1}'`
        do
            ipaddr=`ipadm show-addr -o addr ${dev}/san | grep -iv addr | awk -F/ '{print $1}'`
            if [ ! -z "$ipaddr" ]; then
                ISCSIPORTS="$ISCSIPORTS $ipaddr"
                NUM_ISCSIPORTS=`expr $NUM_ISCSIPORTS + 1`
            fi
        done
    fi
    
    log "iscsi ports : $ISCSIPORTS"
}

########### Main ##########

trap Cleanup 0

if [ "$PLATFORM" = "SunOS" ]; then
    trap CleanupError 1 2 3 15
else
    $ECHO "Error: Unsupported Operating System $PLATFORM."
    exit 1
fi

SetupLogs

svcs svc:/network/iscsi/target:default | grep -w online > $NULL
if [ $? -ne 0 ]; then
    svcadm enable -r svc:/network/iscsi/target:default
fi

TMP_OUTPUT_FILE=/tmp/inm_zvol_export.dat
while [ -f $TMP_OUTPUT_FILE ]
do
    log "Error: already running."
    sleep 2
done

> $TMP_OUTPUT_FILE

CreateTarget

UpdateConfig

rm -f $TMP_OUTPUT_FILE >$NULL


exit 0
