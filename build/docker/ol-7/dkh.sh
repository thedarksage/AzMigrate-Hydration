#!/usr/bin/env bash

declare -A KernelHeaders=( \
["3.10.0-123.el7.x86_64"]="https://buildlogs.centos.org/c7-updates/kernel/3.10.0-123.el7/20140630120647/kernel-devel-3.10.0-123.el7.x86_64.rpm" \
["3.10.0-514.el7.x86_64"]="https://buildlogs.centos.org/c7.1611.01/kernel/20161117160457/3.10.0-514.el7.x86_64/kernel-devel-3.10.0-514.el7.x86_64.rpm" \
["3.10.0-693.el7.x86_64"]="https://buildlogs.centos.org/c7.1708.00/kernel/20170822030048/3.10.0-693.el7.x86_64/kernel-devel-3.10.0-693.el7.x86_64.rpm" \
["3.8.13-35.3.1.el7uek.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL7/UEKR3/x86_64/getPackage/kernel-uek-devel-3.8.13-35.3.1.el7uek.x86_64.rpm" \
["4.1.12-61.1.18.el7uek.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL7/UEKR4/archive/x86_64/getPackage/kernel-uek-devel-4.1.12-61.1.18.el7uek.x86_64.rpm" \
["4.14.35-1818.0.9.el7uek.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL7/UEKR5/archive/x86_64/getPackage/kernel-uek-devel-4.14.35-1818.0.9.el7uek.x86_64.rpm" \
["4.14.35-1902.0.18.el7uek.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL7/UEKR5/archive/x86_64/getPackage/kernel-uek-devel-4.14.35-1902.0.18.el7uek.x86_64.rpm" \
["5.4.17-2011.1.2.el7uek.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL7/UEKR6/x86_64/getPackage/kernel-uek-devel-5.4.17-2011.1.2.el7uek.x86_64.rpm")

mkdir -p /KernelHeaderDownload
cd /KernelHeaderDownload

wget https://yum.oracle.com/repo/OracleLinux/OL7/developer_UEKR5/x86_64/getPackage/libdtrace-ctf-1.1.0-1.el7.x86_64.rpm
rpm -ivh libdtrace-ctf-1.1.0-1.el7.x86_64.rpm --force

for kernel in "${!KernelHeaders[@]}";
do 
    wget --no-check-certificate "${KernelHeaders[$kernel]}"
    rpm -ivh $(ls *.rpm | grep -i $kernel) --force
    mkdir -p /lib/modules/$kernel
    ln -nsf /usr/src/kernels/$kernel /lib/modules/$kernel/build
done

cd /
rm -rf /KernelHeaderDownload
yum clean all
