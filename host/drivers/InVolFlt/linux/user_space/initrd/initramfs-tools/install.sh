#!/bin/bash 

DBG=""

regenerate()
{
    update-initramfs -k $1 -u -v
    return $?
}

regenerate_all()
{
    update-initramfs -k all -u -v
    return $?
}

cleanup()
{
    rm -f $sys_initrd_dir/involflt
}

regenerate_initrd()
{
    $DBG
    local vxdir=""

    cleanup

    cp $INITRD_DIR/involflt/setup_involflt.sh "$sys_initrd_dir/involflt" || return 1

    vxdir=$(echo $1 | sed 's/\//\\\//g')
    echo "Setting vxdir = $1 ($vxdir)"
    sed -i "s/_vxdir=\"\"/_vxdir=\"$vxdir\"/" $sys_initrd_dir/involflt

    regenerate_all
    return $?
}
 
install()
{
    $DBG
    local ret=0

    if [ ! -x "$1/scripts/initrd/involflt_install_lib.sh" ]; then
        exit "Incorrect install directory - $install_dir"
        exit 1
    fi
 
    . $1/scripts/initrd/involflt_install_lib.sh  $1

    for ret in 1; do
        install_init $2  || break
        is_distro "Debian" && {
            set_param "_LS" "fltls" || break
        }
        regenerate_initrd $@ || break
        ret=0
    done

    install_cleanup

    return $ret
}

uninstall()
{
    $DBG

    cleanup
    regenerate_all 
}

$DBG
sys_initrd_dir="/etc/initramfs-tools/hooks"

cmd=$1
shift

$cmd $@
