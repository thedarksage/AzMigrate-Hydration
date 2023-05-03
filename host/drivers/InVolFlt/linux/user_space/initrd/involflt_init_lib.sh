PATH=$PATH:/sbin:/bin:/usr/bin:/usr/sbin

_VMBUSDEVDIR="/sys/bus/vmbus/devices"

log()
{
    echo "involflt: $1" > /dev/kmsg
}

log_cmd()
{
    local line=""

    log "COMMAND: $1"

    $1 |\
    while read line; do
        log "$line"
    done
}

fltls()
{
    # Debian initrd ls displays "ls -l" output by default.
    ls "$1" | sed "/\.$/d" | sed "s/\(.*\) -> .*/\1/" | sed "s/.* //"
}

patmatch()
{
    $DBG
    local pat="$1"
    local file="$2"
    local out=""

    out="`cat $file | sed -n  "/\<$pat\>/p"`"
    if [ -z "$out" ]; then
        return 1
    else
        return 0
    fi
}

mkctrlnode()
{
    $DBG
    local _major=`cat /proc/devices | sed -n '/involflt/p' | sed 's/involflt//g'`
    mknod /dev/involflt c $_major 0
    return $?
}

stack_disk()
{
    $DBG

    _pname="`echo /dev/$1 | sed 's/\//_/g'`"

    inm_dmit --op=stack --src_vol=/dev/$1 --pname=$_pname
    return $?
}

is_disk()
{
    $DBG
    if [ ! -b /dev/$1 ]; then
        return 1
    fi

    if [ -f /sys/block/$1/device/type ]; then
        # TYPE_DISK == 0
        patmatch 0 /sys/block/$1/device/type && return 0
    fi

    return 1
}

stack_all_disks()
{
    $DBG
    for _disk in `$_LS /sys/block`; do
        is_disk $_disk && stack_disk $_disk
    done 
}

load_driver()
{
    $DBG
    modprobe ${_MODOPT} involflt in_initrd=yes || return 1
    mkctrlnode
    return 0
}

debug_shell()
{
    patmatch inmdbg=1 /proc/cmdline && /bin/sh || return 0
}

#main
patmatch inmdbg=1 /proc/cmdline && {
    DBG="set -x"
    exec 2>&1
}

$DBG
patmatch inmage=0 /proc/cmdline && return 1

_LS="ls"
_MODOPT=""

test -z "$DBG" || set +x
