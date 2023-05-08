#!/bin/sh

KERN_LIST="5.15.0-0.30.19.el9uek.x86_64 5.14.0-70.13.1.0.3.el9_0.x86_64 5.14.0-162.6.1.el9_1.x86_64"
noerror=no

for kernel in ${KERN_LIST}
do
    echo "Building driver for $kernel"
    if [ ${kernel} = "5.15.0-0.30.19.el9uek.x86_64" ]; then
        noerror="yes"
    fi

    mkdir -p drivers_dbg && make -f involflt.mak KDIR=/lib/modules/${kernel}/build clean && make -j4 -f involflt.mak KDIR=/lib/modules/${kernel}/build noerror=${noerror} && mv bld_involflt/involflt.ko drivers_dbg/involflt.ko.${kernel}.dbg && strip -g drivers_dbg/involflt.ko.${kernel}.dbg -o involflt.ko.${kernel}

    ret=$?
    noerror=no
    if [ $ret -ne "0" ]; then
        break
    fi
done

exit $ret
