#!/bin/bash 

DBG=""

regenerate()
{
    mkinitrd -i initrd-$1 -k vmlinuz-$1
    return $?
}

regenerate_foreach()
{
    local retval=0
    local kernel=""

    for kernel in `ls /lib/modules`; do 
        echo $kernel | grep -q ^[1-9] || continue
        test -f /lib/modules/$kernel/modules.dep || continue
        test -f /boot/vmlinu*$kernel || continue
        regenerate $kernel || retval=1

        # As all the drivers needed to boot on Azure are included in the original initrd, have to remove the
        # ones created by the pre-script of consistency tags so that the hydration shouldn't do any renaming of
        # initrd rather use the original one for booting after FO/FB.
        if [ ${retval} -eq "0" ]; then
            rm -f /boot/initrd-$kernel-InMage* /boot/initrd-$kernel-INMAGE*
        fi

    done

    return $retval
}

regenerate_all()
{
    regenerate_foreach
    return $?
}

cleanup()
{
    if [ -f /lib/mkinitrd/boot/60-involflt.sh ]; then
        rm -f /lib/mkinitrd/boot/60-involflt.sh
    fi

    rm -f /lib/mkinitrd/bin/inm_dmit
    rm -f /lib/mkinitrd/scripts/boot-involflt.sh
    rm -f /lib/mkinitrd/scripts/setup-involflt.sh
    rm -f /lib/mkinitrd/boot/03-involflt.sh
    rm -f /lib/mkinitrd/setup/04-involflt.sh
    rm -f /boot/initrd-*-InMage.orig
}

regenerate_initrd()
{
    $DBG
    local vxdir=""

    cleanup

    (echo $2 | grep -qi "^vmware$") && {
       sed -i -e "/#%modules: involflt/c #%modules: involflt $V2A_MODULES" $INITRD_DIR/involflt/involflt_init.sh
    }

    cp $INITRD_DIR/involflt/involflt_init.sh "$sys_initrd_dir/scripts/boot-involflt.sh" || return 1
    cp $INITRD_DIR/involflt/setup_involflt.sh "$sys_initrd_dir/scripts/setup-involflt.sh" || return 1

    vxdir=$(echo $1 | sed 's/\//\\\//g')
    echo "Setting vxdir = $1 ($vxdir)"
    sed -i "s/_vxdir=\"\"/_vxdir=\"$vxdir\"/" $sys_initrd_dir/scripts/setup-involflt.sh

    ln -s $sys_initrd_dir/scripts/boot-involflt.sh $sys_initrd_dir/boot/03-involflt.sh
    ln -s $sys_initrd_dir/scripts/setup-involflt.sh $sys_initrd_dir/setup/04-involflt.sh

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
        # SLES requires additional params for modprobe
        set_param "_MODOPT" "--allow-unsupported-modules" || break
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
sys_initrd_dir="/lib/mkinitrd"

cmd=$1
shift

$cmd $@
