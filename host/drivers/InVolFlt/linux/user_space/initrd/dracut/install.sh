#!/bin/bash 

DBG="set -x"

$DBG
dracut_dir="/usr/lib/dracut/modules.d"
if [ ! -d $dracut_dir ]; then
    dracut_dir="/usr/share/dracut/modules.d"
fi


regenerate()
{
    local opt=""
    local ifile=""

    dracut -h | grep -q -- "--kver" && opt="--kver"

    is_distro OL6 && {
        # opt is blank for OL6
        opt="/boot/initrd.asr"
        echo "Creating image at $opt"
        > "$opt"
    }

    dracut -f "$opt" $1 || {
        echo "Cannot generate initrd for $ker"
        return 1
    }

    is_distro OL6 && {
        ifile=$(dracut '' $1 2>&1 | sed -n 's/.*\(\/boot\/initr.*\.img\).*/\1/p')
        echo "Copying $opt to $ifile"
        mv "$opt" "$ifile"
    }

    return 0
}

regenerate_foreach()
{
    local retval=0
    local kernel=""

    for kernel in `ls /lib/modules`; do 
        echo $kernel | grep -q ^[1-9] || continue
        test -f /lib/modules/$kernel/modules.dep || continue
        test -f /boot/vmlinuz-$kernel || continue
        regenerate $kernel || retval=1
    done

    return $retval
}

regenerate_all()
{
	# RH7 dracut help message is buggy and does not list "--regenerate-all"
	# option. RH7 and SLES12 support "--kver" option and "--regenerate-all"
	# option and list it in their help. RH6 does not support both options.
    dracut -h | grep -q -- "--kver" && {
        dracut -f --regenerate-all
        return $?
    }

    # RHEL6 does not have "--regenerate-all" option
    regenerate_foreach
    return $?
}

regenerate_initrd_dracut()
{
    $DBG
    local vxdir=""

    rm -rf "$dracut_dir"
    mkdir -p "$dracut_dir"

    cp $INITRD_DIR/95involflt/* "$dracut_dir/"
    cp $INITRD_DIR/involflt_init_lib.sh.cfg "$dracut_dir/involflt_init_lib.sh"
    cp $INSTALL_DIR/bin/inm_dmit "$dracut_dir/"

    vxdir=$(echo $1 | sed 's/\//\\\//g')
    echo "Setting vxdir = $1 ($vxdir)"
    sed -i "s/_vxdir=\"\"/_vxdir=\"$vxdir\"/" $dracut_dir/installkernel

    is_distro "OL8" || is_distro "OL9" || is_distro "RHEL8" || is_distro "RHEL9" || is_distro "RHEL7" || is_distro "SLES15" && {
        if [ "$2" = "vmware" ]; then
            sed -i "s/instmods involflt/instmods involflt $V2A_MODULES/" \
                $dracut_dir/installkernel
        fi
    }

	regenerate_all
    return $?
}
 
install()
{
    $DBG
    ret=0

    if [ ! -x "$1/scripts/initrd/involflt_install_lib.sh" ]; then
        exit "Incorrect install directory - $install_dir"
        exit 1
    fi
 
    . $1/scripts/initrd/involflt_install_lib.sh  $1

    for ret in 1; do
        install_init $2  || break
        # SLES 12 requires additional params for modprobe
        is_distro "SLES" && {
            set_param "_MODOPT" "--allow-unsupported-modules" || break
        }
        regenerate_initrd_dracut $@ || break
        ret=0
    done

    install_cleanup

    return $ret
}

uninstall()
{
    $DBG

    INST_DIR=`cat /usr/local/.vx_version | grep INSTALLATION_DIR | cut -d"=" -f 2`
    if [ ! -x "$INST_DIR/scripts/initrd/involflt_install_lib.sh" ]; then
        exit "Incorrect install directory - $INST_DIR"
        exit 1
    fi

    . $INST_DIR/scripts/initrd/involflt_install_lib.sh  $INST_DIR

    rm -rf "$dracut_dir"
    regenerate_all
}

if [ ! -d $dracut_dir ]; then
    exit 1
fi

dracut_dir="$dracut_dir/95involflt"

cmd=$1
shift

$cmd $@
