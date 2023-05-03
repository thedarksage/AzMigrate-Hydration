#!/bin/sh

KERN_LIST="4.18.0-80.el8.x86_64 4.18.0-147.el8.x86_64 4.18.0-305.el8.x86_64 4.18.0-348.el8.x86_64 4.18.0-425.3.1.el8.x86_64 4.18.0-425.10.1.el8_7.x86_64 5.4.17-2011.1.2.el8uek.x86_64 5.15.0-0.30.19.el8uek.x86_64"
bin_path=""
noerror=no

for kernel in ${KERN_LIST}
do
    echo "Building driver for $kernel"
    bin_path=$PATH
    if [ ${kernel} = "5.15.0-0.30.19.el8uek.x86_64" ]; then
        PATH=/opt/rh/gcc-toolset-11/root/usr/bin:$PATH
        noerror="yes"
    fi

    mkdir -p drivers_dbg && make -f involflt.mak KDIR=/lib/modules/${kernel}/build clean && make -j4 -f involflt.mak KDIR=/lib/modules/${kernel}/build noerror=${noerror} && mv bld_involflt/involflt.ko drivers_dbg/involflt.ko.${kernel}.dbg && strip -g drivers_dbg/involflt.ko.${kernel}.dbg -o involflt.ko.${kernel}

    ret=$?
    PATH=${bin_path}
    noerror=no
    if [ $ret -ne "0" ]; then
        break
    fi
done

exit $ret
