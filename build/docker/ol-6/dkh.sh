#!/usr/bin/env bash
set -x

declare -A KernelHeaders=( \
["2.6.32-100.28.5.el6.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL6/0/base/x86_64/getPackage/kernel-uek-devel-2.6.32-100.28.5.el6.x86_64.rpm" \
["2.6.32-100.34.1.el6uek.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL6/1/base/x86_64/getPackage/kernel-uek-devel-2.6.32-100.34.1.el6uek.x86_64.rpm" \
["2.6.32-131.0.15.el6.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL6/1/base/x86_64/getPackage/kernel-devel-2.6.32-131.0.15.el6.x86_64.rpm" \
["2.6.32-300.3.1.el6uek.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL6/2/base/x86_64/getPackage/kernel-uek-devel-2.6.32-300.3.1.el6uek.x86_64.rpm" \
["2.6.39-100.5.1.el6uek.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL6/UEK/latest/x86_64/getPackage/kernel-uek-devel-2.6.39-100.5.1.el6uek.x86_64.rpm" \
["2.6.39-200.24.1.el6uek.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL6/UEK/latest/x86_64/getPackage/kernel-uek-devel-2.6.39-200.24.1.el6uek.x86_64.rpm" \
["2.6.39-300.17.1.el6uek.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL6/UEK/latest/x86_64/getPackage/kernel-uek-devel-2.6.39-300.17.1.el6uek.x86_64.rpm" \
["2.6.39-400.17.1.el6uek.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL6/UEK/latest/x86_64/getPackage/kernel-uek-devel-2.6.39-400.17.1.el6uek.x86_64.rpm" \
["3.8.13-16.2.1.el6uek.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL6/UEKR3/latest/x86_64/getPackage/kernel-uek-devel-3.8.13-16.2.1.el6uek.x86_64.rpm" \
["4.1.12-32.1.2.el6uek.x86_64"]="https://yum.oracle.com/repo/OracleLinux/OL6/UEKR4/archive/x86_64/getPackage/kernel-uek-devel-4.1.12-32.1.2.el6uek.x86_64.rpm" )

declare -A KernelSrcHeaders=( \
["2.6.32-200.16.1.el6uek"]="http://oss.oracle.com/ol6/SRPMS-updates/kernel-uek-2.6.32-200.16.1.el6uek.src.rpm" \
["2.6.32-400.26.1.el6uek"]="http://oss.oracle.com/ol6/SRPMS-updates/kernel-uek-2.6.32-400.26.1.el6uek.src.rpm" )

mkdir -p /KernelHeaderDownload
cd /KernelHeaderDownload

for kernel in "${!KernelHeaders[@]}";
do 
    # Download and install kernel headers rpm.
    wget "${KernelHeaders[$kernel]}"
    rpm -ivh $(ls *.rpm | grep -i $kernel) --force
    if [ $? -ne 0 ]; then
        echo "Failed to install $kernel rpm."
        exit 1
    else
        echo "Installed $kernel rpm successfully."
    fi

    # Create softlink to build folder.
    mkdir -p /lib/modules/$kernel
    ln -nsf /usr/src/kernels/$kernel /lib/modules/$kernel/build
done

for kernel in "${!KernelSrcHeaders[@]}";
do 
    # Download and install kernel headers source rpm.
    cd /KernelHeaderDownload
    wget "${KernelSrcHeaders[$kernel]}"
    rpm -ivh $(ls *.rpm | grep -i $kernel) --force
    if [ $? -ne 0 ]; then
        echo "Failed to install $kernel source rpm."
        exit 1
    else
        echo "Installed $kernel source rpm successfully."
    fi

    # Build kernel headers rpm.
    rpmbuild -ba /root/rpmbuild/SPECS/kernel-uek.spec
    if [ $? -ne 0 ]; then
        echo "Failed to build $kernel rpm."
        exit 1
    else
        echo "Built $kernel rpm successfully."
    fi

    # Install kernel headers rpm.
    cd /root/rpmbuild/RPMS/x86_64
    rpm -ivh $(ls *.rpm | grep -i kernel-uek-devel-$kernel) --force
    if [ $? -ne 0 ]; then
        echo "Failed to install $kernel rpm."
        exit 1
    else
        echo "Installed $kernel rpm successfully."
    fi

    # Create softlink to build folder.
    mkdir -p /lib/modules/$kernel.x86_64
    ln -nsf /usr/src/kernels/$kernel.x86_64 /lib/modules/$kernel.x86_64/build
    rm -rf /root/rpmbuild
done

cd /
rm -rf /KernelHeaderDownload
yum clean all
