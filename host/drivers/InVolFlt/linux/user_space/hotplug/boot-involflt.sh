#!/bin/bash
#%stage: char
#%depends:
#%provides:
#%programs: /bin/inm_dmit
#%if: "involflt"
#%modules: involflt
#

grep -qi "inmage=0" /proc/cmdline && exit 1

modprobe --allow-unsupported-modules involflt in_initrd=yes
major=`grep involflt /proc/devices | awk '{print $1}'`
mknod /dev/involflt c $major 0

