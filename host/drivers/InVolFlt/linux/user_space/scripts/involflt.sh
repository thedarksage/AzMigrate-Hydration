#!/bin/bash

DBG=""
vxdir="/etc/vxagent"
pdir="$vxdir/involflt"
bdir="$vxdir/bin"
hdir="$vxdir/hotplug_utils"
ldir="$vxdir/logs"
logfile=""
modprobe_opt=""

export PATH=$PATH:$bdir:$hdir
start_bin="modprobe basename mknod inm_dmit get_protected_dev_details.sh"
stop_bin="fsfreeze lsof inmshutnotify"
preshutdown_bin="inmshutnotify inm_dmit get_protected_dev_details.sh"

function log
{
    $DBG
    echo "$1" >> $logfile
}

function log_cmd
{
echo "CMD: $1" >> $logfile
echo "###################" >> $logfile
$1 >> $logfile 2>&1
return $?
}

function flt_mkctrl
{
    $DBG

    local major="`cat /proc/devices | grep involflt | cut -f 1 -d " "`"
    mknod /dev/involflt c $major 0
}

function flt_check_dependency
{
    $DBG

    local i=""

    for i in ${!1}
    do
        which $i > /dev/null 2>&1 || {
            log "$i not found in $PATH" 
            return 1
        }
    done
}

# Similar function also implemented in inm_dmit which also 
# needs to be updated in case of a change

function flt_resolve_pname
{
    $DBG

    basename "`readlink -f $pdir/$1`"
}

function flt_init
{
    $DBG

    if [ "$#" -eq "2" ]
    then
        mkdir -p $ldir
        logfile="$ldir/${2}"
    else
        if [ "$1" != "start" ]
        then
            mkdir -p $ldir
            logfile="$ldir/flt_${1}_$(date +%Y%m%d%H%M%S).log"
        else
            logfile="/dev/console"
        fi
    fi
    
    flt_check_dependency ${1}_bin

    if [ -f /etc/SuSE-release ]
    then
        modprobe_opt="--allow-unsupported-modules"
    fi

    if [ -f /etc/os-release ] && grep -q 'SLES' /etc/os-release; 
    then
        modprobe_opt="--allow-unsupported-modules"
    fi

}

function flt_preshutdown
{
    $DBG

    log "Preshutdown"

    local cur_pname=""
    local new_pname=""
    local d_pname=""

    /etc/vxagent/hotplug_utils/inmshutnotify --pre || {
        log "Preshutdown failed"
    }

    #log_cmd "get_protected_dev_details.sh -v"

    # By this time, agent should have been stopped. We can assume no new
    # protections and carry any upgrades required for existing disks
    for disk in `inm_dmit --get_protected_volume_list`
    do
        cur_pname="`inm_dmit --op=map_name --src_vol=$disk`"
        # if older driver does not support name mapping 
        # fallback to default naming convention
        if [ "$cur_pname" = "" ]              
        then                                 
                cur_pname="`echo $disk | sed "s/\//_/g"`"
        fi                                              
                                                                                            
        new_pname="`get_protected_dev_details.sh -p $disk | grep "Persistent Name"`" && {
            new_pname="`echo $new_pname | awk '{print $NF}'`"
            d_pname="$new_pname"

            log "$disk:$cur_pname:$new_pname:$d_pname"
            if [ "$cur_pname" != "$d_pname" ]
            then
                log "Upgrading pname $cur_pname ($d_pname)-> $new_pname"
                ln -fs "$cur_pname" "$pdir/$new_pname"
            fi
        } || {
            log "BUG: Persistent name not found for $disk: $?"
        }
    done
}

function flt_stop
{
    $DBG

    flt_mkctrl

    log "PATH=$PATH"
    log_cmd "df"
    log_cmd "ps -eaf"
    
    df | awk '/^\/dev/ { print $NF }' |\
    while read fs
    do 
        if [ $fs != / ]
        then 
            log "remounting $fs ro"
            mount -o remount,ro $fs
        fi
    done
    
    log_cmd "mount"
    
    fsfreeze -f /
    fsfreeze -u /
    
    /etc/vxagent/hotplug_utils/inmshutnotify 
    if [ $? -ne 0 ]
    then
        log "Shutdown notification failed"
    fi
   
    mount -n -o remount,ro /
    sleep 30

    return 0
}

function flt_start
{
    $DBG

    grep -q inmage=0 /proc/cmdline && return 0

    modprobe ${modprobe_opt} involflt
    flt_mkctrl

    # For initrd mode driver
    /etc/vxagent/bin/inm_dmit --op=init_driver_fully

    local pname=""
    local dname=""
    
    #log_cmd "get_protected_dev_details.sh -v"

    # Boottime stacking
    ls "$pdir" | grep -v common |\
    while read pname
    do
        pname="`basename $pname`"  
        get_protected_dev_details.sh -d $pname > /dev/null 2>&1 && {
            dname="`get_protected_dev_details.sh -d $pname | grep "Device Name" | awk '{print $NF}'`"
            d_pname="$pname"
            log "Stacking $pname ($d_pname) -> $dname"
            inm_dmit --op=stack --src_vol=$dname --pname=$d_pname
        } || {
            log "Not stacking $pname: $?"
        }
    done

    # Stop filtering the extra protected disks
    inm_dmit --get_protected_volume_list |\
    while read disk
    do
        pname=`inm_dmit --op=map_name --src_vol=$disk 2>/dev/null`
        issue_stop_filtering=0
        if [ ! -d "$pdir/$pname" ]; then
            issue_stop_filtering=1
        elif [ ! -f "$pdir/$pname/VolumeFilteringDisabled" ]; then
            issue_stop_filtering=1
        elif [ `cat $pdir/$pname/VolumeFilteringDisabled` -ne "0" ]; then
            issue_stop_filtering=1
        fi

        if [ $issue_stop_filtering -eq "1" ]; then
            inm_dmit --op=stop_flt --src_vol=$disk
        fi
    done

    return 0;
}

$DBG

flt_init $@

case $1 in
    start)          flt_start
                    ;;

    stop)           # Eventually move all stop scripts to call us
                    #flt_stop
                    ;;

    preshutdown)    flt_preshutdown
                    ;;

    *)              echo "Unsupported option $1"
                    ;;
esac

exit $?
