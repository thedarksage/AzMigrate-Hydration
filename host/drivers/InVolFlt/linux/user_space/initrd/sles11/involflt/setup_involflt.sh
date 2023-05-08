#!/bin/bash
#
#%stage: boot
#%programs: /bin/sed
#
set -x
_vxdir=""

$_vxdir/bin/check_drivers.sh noinitrd
cp $_vxdir/scripts/initrd/involflt_init_lib.sh.cfg $tmp_mnt/bin/involflt_init_lib.sh
cp $_vxdir/scripts/initrd/involflt/involflt_init.sh $tmp_mnt/bin/
cp $_vxdir/scripts/initrd/involflt/99_involflt.rules $tmp_mnt/etc/udev/rules.d/
cp $_vxdir/bin/inm_dmit $tmp_mnt/bin/

set +x

