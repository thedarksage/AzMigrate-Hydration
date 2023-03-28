#!/bin/sh -e

PREREQS=""

prereqs() { echo "$PREREQS"; }

case "$1" in
    prereqs)
    prereqs
    exit 0
    ;;
esac

. /usr/share/initramfs-tools/hook-functions

set -x
_vxdir=""
$_vxdir/bin/check_drivers.sh noinitrd

manual_add_modules involflt
cp $_vxdir/scripts/initrd/involflt_init_lib.sh.cfg $DESTDIR/bin/involflt_init_lib.sh
cp $_vxdir/scripts/initrd/involflt/involflt_init.sh $DESTDIR/bin/
cp $_vxdir/scripts/initrd/involflt/99_involflt.rules $DESTDIR/lib/udev/rules.d/
copy_exec $_vxdir/bin/inm_dmit bin
copy_exec `which sed` bin

set +x

