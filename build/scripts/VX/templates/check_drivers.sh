#! /bin/sh

PATH=/bin:/sbin:/usr/sbin:/usr/bin:$PATH
export PATH

# Get the following details during installation
##### Configuration #####
ACTL_KERNEL_VER="SET_ACTL_KERNEL_VER"
S_FILE_SUFFIX="SET_S_FILE_SUFFIX"
SEP="SET_SEP"
KERNEL_VER_SUFFIX="SET_KERNEL_VER_SUFFIX"
DEPLOY_DIR="SET_DEPLOY_DIR"
OS="SET_OS"
DRIVERS="involflt"
##### Configuration #####

regenerate_list=""

echo >> /var/log/InMage_drivers.log
echo `date` >> /var/log/InMage_drivers.log
echo "----------------------------" >> /var/log/InMage_drivers.log

is_supported_rhel8_kernel()
{
    CURR_KERNEL=$1
    EXTRA_VER1=$2
    EXTRA_VER2=$3

    ret=1
    extra_minor1=`echo ${CURR_KERNEL} | awk -F"." '{print $4}'`
    extra_minor2=`echo ${CURR_KERNEL} | awk -F"." '{print $5}'`
    if [ ! -z $extra_minor1 -a $extra_minor1 -eq $extra_minor1 ] 2> /dev/null; then
        if [ $extra_minor1 -gt $EXTRA_VER1 ]; then
            ret=0
        elif [ $extra_minor1 -eq $EXTRA_VER1 ]; then
            if [ ! -z $extra_minor2 -a $extra_minor2 -eq $extra_minor2 ] 2> /dev/null; then
                if [ $extra_minor2 -ge $EXTRA_VER2 ]; then
                    ret=0
                fi
            fi
        fi
    fi

    return $ret
}

copy_rhel8_drivers()
{
    local _suffix="$2"
    local _drv_dir="$3"

    RHEL8_KMV_V0="80"
    RHEL8_KMV_V1="147"
    RHEL8_KMV_V2="193"
    RHEL8_KMV_V3="240"
    RHEL8_KMV_V4="305"
    RHEL8_KMV_V5="348"

    VER_DIR=$(dirname $(dirname $(dirname $k_dir)))
    K_VER=${VER_DIR##*/}

    KERNEL_MINOR_VERSION=`echo "$1" | cut -d"-" -f2 | cut -d"." -f1`
    KERNEL_COPY_VERSION=""
    ret=1
    case $KERNEL_MINOR_VERSION in
        $RHEL8_KMV_V0)
            KERNEL_COPY_VERSION=$RHEL8_KMV_V0
        ;;
        $RHEL8_KMV_V1|$RHEL8_KMV_V2|$RHEL8_KMV_V3)
            KERNEL_COPY_VERSION=$RHEL8_KMV_V1
        ;;
        $RHEL8_KMV_V4)
            is_supported_rhel8_kernel $K_VER 30 1
            if [ $? -eq "0" ]; then
                KERNEL_COPY_VERSION=$RHEL8_KMV_V4
            fi
        ;;
        $RHEL8_KMV_V5)
            is_supported_rhel8_kernel $K_VER 5 1
            if [ $? -eq "0" ]; then
                KERNEL_COPY_VERSION=$RHEL8_KMV_V5
            fi
        ;;
        *)
            KERNEL_COPY_VERSION=$RHEL8_KMV_V5
        ;;
    esac
    if [ -z $KERNEL_COPY_VERSION ]; then
        echo "Not copying involflt driver to kernel $1" >> /var/log/InMage_drivers.log
    else
        echo "Copying ${_drv_dir}/involflt.ko.${S_FILE_SUFFIX}-${KERNEL_COPY_VERSION}*${SEP}${_suffix} to ${k_dir}" >> /var/log/InMage_drivers.log
        cp -f ${_drv_dir}/involflt.ko.${S_FILE_SUFFIX}-${KERNEL_COPY_VERSION}*${SEP}${_suffix} ${k_dir}/involflt.ko
        ret=$?
    fi

    return $ret
}

copy_rhel7_drivers()
{
    local _suffix="$2"
    local _drv_dir="$3"

    RHEL7_KMV_BASE="123"
    RHEL7_KMV_U3="514"
    RHEL7_KMV_U4="693"
    
    KERNEL_MINOR_VERSION=`echo ${1} | cut -d"-" -f2 | cut -d"." -f1`
    if [ $KERNEL_MINOR_VERSION -lt "$RHEL7_KMV_U4" ]; then
        if [ $KERNEL_MINOR_VERSION -lt "$RHEL7_KMV_U3" ]; then
            cp -f ${_drv_dir}/involflt.ko.${S_FILE_SUFFIX}-${RHEL7_KMV_BASE}*${SEP}${_suffix} ${k_dir}/involflt.ko
        else
            cp -f ${_drv_dir}/involflt.ko.${S_FILE_SUFFIX}-${RHEL7_KMV_U3}*${SEP}${_suffix} ${k_dir}/involflt.ko
        fi
    else
        cp -f ${_drv_dir}/involflt.ko.${S_FILE_SUFFIX}-${RHEL7_KMV_U4}*${SEP}${_suffix} ${k_dir}/involflt.ko
    fi

    return $?
}

is_sles_kernel_supported()
{
    local ker_ver=$1
    local deploy_dir=$2
    local supported_kernels="${deploy_dir}/bin/drivers/supported_kernels"
    local ret=1

    if [ -f "${supported_kernels}" ]; then
        grep -q "^${ker_ver}:" ${supported_kernels}
        if [ $? -eq "0" ]; then
            ret=0
            ker=`grep "^${ker_ver}:" ${supported_kernels} | awk -F":" '{print $1}'`
            patch_level=`grep "^${ker_ver}:" ${supported_kernels} | awk -F":" '{print $2}'`
            echo "Fetched kernel : $ker and patch level: $patch_level from supported kernels file" >> /var/log/InMage_drivers.log
        fi
    fi

    echo ${ker_ver} | grep -q "\-azure$"
    if [ $? -eq "0" ]; then
        echo "Validating for azure kernel" >> /var/log/InMage_drivers.log
        if [ ${ret} -eq "0" ]; then
            echo ${ker_ver}
            echo "$ker_ver found in supported kernels list" >> /var/log/InMage_drivers.log
            return 0
        fi

        return 1
    fi

    if [ ${ret} -eq "0" ]; then
        echo "SP${patch_level}"
        return 0
    fi

    if [ -f /etc/SuSE-release ] ; then
        patch_level=`grep "PATCHLEVEL = " /etc/SuSE-release | awk '{print $3}'`
        echo "Fetched patch level: $patch_level from SuSE-release file." >> /var/log/InMage_drivers.log
    else
        patch_level=`grep "VERSION=" /etc/os-release | grep -o SP[0-9] | grep -o [0-9]`
        echo "Fetched patch level: $patch_level from os-release file." >> /var/log/InMage_drivers.log
        if [ -z "${patch_level}" ]; then
            echo "No patch level info obtained, dumping os-release file." >> /var/log/InMage_drivers.log
            cat /etc/os-release >> /var/log/InMage_drivers.log
            patch_level=`grep "VERSION=" /etc/os-release | grep -o "SP[0-9]" | grep -o "[0-9]"`
            echo "Fetched patch level: $patch_level from os-release file." >> /var/log/InMage_drivers.log
        fi
        if [ -z "${patch_level}" ]; then
            local os_ver=`grep "VERSION=" /etc/os-release`
            local sp_ver=`echo $os_ver | grep -o "SP[0-9]"`
            local sp_num_ver=`echo $sp_ver | grep -o "[0-9]"`
            echo "os version = $os_ver, sp version = $sp_ver, sp_num version=$sp_num_ver" >> /var/log/InMage_drivers.log
            patch_level=`grep "VERSION=" /etc/os-release | awk -F '[-"]' '{print $3}' | tr -d 'SP'`
            echo "Fetched patch level: $patch_level from os-release file using awk." >> /var/log/InMage_drivers.log
        fi
        if [ -z "${patch_level}" ]; then
            echo "No patch level info obtained from os-release file, marking it as SP0" >> /var/log/InMage_drivers.log
            patch_level=0
        fi
        echo "Final patch level = $patch_level" >> /var/log/InMage_drivers.log
    fi
    if [ ! -z "${patch_level}" ]; then
        echo "SP${patch_level}"
        return 0
    fi

    return 1
}

echo "OS is $OS" >> /var/log/InMage_drivers.log
if echo $OS | grep -q 'RHEL\|SLES11' ; then
    for driver in $DRIVERS
    do
        case $driver in
            involflt) driver_dir="kernel/drivers/char" ;;            
        esac

        for suffix in $KERNEL_VER_SUFFIX
        do
            for TOP_DIR in `echo /lib/modules/${ACTL_KERNEL_VER}*${SEP}${suffix}`
            do
                base_kdir=`basename $TOP_DIR`
                test -f /boot/vmlinuz-$base_kdir || continue
                k_dir=$TOP_DIR/$driver_dir
                if [ -d ${k_dir} ]; then
                    echo >> /var/log/InMage_drivers.log
                    echo "Kernel Directory : ${k_dir}" >> /var/log/InMage_drivers.log
                    if [ -f ${k_dir}/${driver}.ko ]; then
                        echo "${driver}.ko already exists in ${k_dir}" >> /var/log/InMage_drivers.log
                        RET_VAL=256
                    else
                        if [ "$OS" = "RHEL7-64" ]; then
                            copy_rhel7_drivers "$k_dir" "$suffix" "${DEPLOY_DIR}/bin"
                        elif [ "$OS" = "RHEL8-64" ]; then
                            copy_rhel8_drivers "$k_dir" "$suffix" "${DEPLOY_DIR}/bin"
                        else
                            cp -f ${DEPLOY_DIR}/bin/${driver}.ko.${S_FILE_SUFFIX}*${SEP}${suffix} ${k_dir}/${driver}.ko
                        fi
                        RET_VAL=$?
                    fi
                    if [ "${RET_VAL}" = "0" ]; then
                        echo "Installed ${driver}.ko successfully under ${k_dir}" >> /var/log/InMage_drivers.log
                        VER_DIR=$(dirname $(dirname $(dirname $k_dir)))
                        K_VER=${VER_DIR##*/}
                        echo "Running depmod for kernel $K_VER ..." >> /var/log/InMage_drivers.log
                        depmod $K_VER
                        if [ $? -eq 0 ]; then
                            echo "depmod successful for above kernel..." >> /var/log/InMage_drivers.log
                            regenerate_list="$K_VER $regenerate_list" 
                        else
                            echo "depmod did not succeed for kernel $K_VER ..." >> /var/log/InMage_drivers.log
                            echo "This may affect loading of drivers when you boot this machine using the kernel $K_VER!" >> /var/log/InMage_drivers.log
                        fi
                    elif [ "${RET_VAL}" = "256" ]; then
                        :
                    else
                        echo "Could not install ${driver}.ko under ${k_dir}" >> /var/log/InMage_drivers.log
                    fi
                fi
            done
        done
    done
elif [ "${OS}" = "OL7-64" -o "${OS}" = "OL8-64" ]; then
	# RH compatible modules
    for TOP_DIR in `echo /lib/modules/${ACTL_KERNEL_VER}*${SEP}${KERNEL_VER_SUFFIX}`
    do
        base_kdir=`basename $TOP_DIR`
        test -f /boot/vmlinuz-$base_kdir || continue
        k_dir=$TOP_DIR/kernel/drivers/char
        test -f $k_dir/involflt.ko && continue
        if [ "$OS" = "OL7-64" ]; then
            copy_rhel7_drivers "$k_dir" "$KERNEL_VER_SUFFIX" "${DEPLOY_DIR}/bin/drivers"
        else
            copy_rhel8_drivers "$k_dir" "$KERNEL_VER_SUFFIX" "${DEPLOY_DIR}/bin/drivers"
        fi
        RET_VAL=$?
        if [ "${RET_VAL}" = "0" ]; then
            echo "Installed involflt.ko successfully under ${k_dir}" >> /var/log/InMage_drivers.log
            VER_DIR=$(dirname $(dirname $(dirname $k_dir)))
            K_VER=${VER_DIR##*/}
            echo "Running depmod for kernel $K_VER ..." >> /var/log/InMage_drivers.log
            depmod $K_VER
            if [ $? -eq 0 ]; then
                echo "depmod successful for above kernel..." >> /var/log/InMage_drivers.log
                regenerate_list="$K_VER $regenerate_list" 
            else
                echo "depmod did not succeed for kernel $K_VER ..." >> /var/log/InMage_drivers.log
                echo "This may affect loading of drivers when you boot this machine using the kernel $K_VER!" >> /var/log/InMage_drivers.log
            fi
        else
            echo "Could not install involflt.ko under ${k_dir}" >> /var/log/InMage_drivers.log
        fi
    done

    # UEK kernels
    for TOP_DIR in `find /lib/modules/*uek.x86_64 -maxdepth 0 -type d 2>/dev/null`; do
        base_kdir=`basename $TOP_DIR`
        test -f /boot/vmlinuz-$base_kdir || continue
        k_dir=$TOP_DIR/kernel/drivers/char
        test -f $k_dir/involflt.ko && continue
        kernel="`echo $k_dir | cut -f 4 -d "/" | cut -f 1 -d "-"`"
        if [ "$kernel" = "4.14.35" ]; then
            echo $k_dir | grep -q "\-18" && kernel="${kernel}-1818" || kernel="${kernel}-1902"
        fi
        echo "Copying involflt.ko.${kernel}*uek.x86_64" >> /var/log/InMage_drivers.log
        cp -f ${DEPLOY_DIR}/bin/drivers/involflt.ko.${kernel}*uek.x86_64 ${k_dir}/involflt.ko
        RET_VAL=$?
        if [ "${RET_VAL}" = "0" ]; then
            echo "Installed involflt.ko successfully under ${k_dir}" >> /var/log/InMage_drivers.log
            VER_DIR=$(dirname $(dirname $(dirname $k_dir)))
            K_VER=${VER_DIR##*/}
            echo "Running depmod for kernel $K_VER ..." >> /var/log/InMage_drivers.log
            depmod $K_VER
            if [ $? -eq 0 ]; then
                echo "depmod successful for above kernel..." >> /var/log/InMage_drivers.log
                regenerate_list="$K_VER $regenerate_list" 
            else
                echo "depmod did not succeed for kernel $K_VER ..." >> /var/log/InMage_drivers.log
                echo "This may affect loading of drivers when you boot this machine using the kernel $K_VER!" >> /var/log/InMage_drivers.log
            fi
        else
            echo "Could not install involflt.ko under ${k_dir}" >> /var/log/InMage_drivers.log
        fi
    done
elif [ "${OS}" = "OL6-32" -o "${OS}" = "OL6-64" ]; then
    echo "OS is OL6-32 or OL6-64. Copying involflt.ko to new kernels which are installed after agent installation." >> /var/log/InMage_drivers.log
    # Copy involflt.ko to RHEL standard kernels matching 2.6.32-*el6.i686 2.6.32-*el6.x86_64
    # Copy involflt.ko to uek kernels matching 2.6.32-100*el6uek.i686, 2.6.32-200*el6uek.i686, 2.6.32-300*el6uek.i686 and 2.6.32-400.*el6uek.i686
    # Copy involflt.ko to uek2 kernels matching 2.6.39-100*el6uek.i686, 2.6.39-200*el6uek.i686, 2.6.39-300*el6uek.i686 and 2.6.39-400.*el6uek.i686
    # Copy involflt.ko to uek kernels matching 2.6.32-100*el6.x86_64, 2.6.32-100*el6uek.x86_64, 2.6.32-200*el6uek.x86_64, 2.6.32-300*el6uek.x86_64 and 2.6.32-400*el6uek.x86_64
    # Copy involflt.ko to uek2 kernels matching 2.6.39-100*el6uek.x86_64, 2.6.39-200*el6uek.x86_64, 2.6.39-300*el6uek.x86_64 and 2.6.39-400*el6uek.x86_64

    for kernel in 2.6.32-131.0.15.el6.i686 2.6.32-131.0.15.el6.x86_64 \
                  2.6.32-100.34.1.el6uek.i686 2.6.32-200.16.1.el6uek.i686 2.6.32-300.3.1.el6uek.i686 2.6.32-400.26.1.el6uek.i686 \
                  2.6.39-100.5.1.el6uek.i686 2.6.39-200.24.1.el6uek.i686 2.6.39-300.17.1.el6uek.i686 2.6.39-400.17.1.el6uek.i686 \
                  2.6.32-100.28.5.el6.x86_64 2.6.32-100.34.1.el6uek.x86_64 2.6.32-200.16.1.el6uek.x86_64 2.6.32-300.3.1.el6uek.x86_64 2.6.32-400.26.1.el6uek.x86_64 \
                  2.6.39-100.5.1.el6uek.x86_64 2.6.39-200.24.1.el6uek.x86_64 2.6.39-300.17.1.el6uek.x86_64 2.6.39-400.17.1.el6uek.x86_64 3.8.13-16.2.1.el6uek.x86_64 \
                  4.1.12-32.1.2.el6uek.x86_64
    do
        # Based on kernel version decide kernel and suffix
        if [ "${kernel}" = "2.6.32-131.0.15.el6.i686" ]; then
            KER=2.6.32
            SUF=el6.i686
        elif [ "${kernel}" = "2.6.32-131.0.15.el6.x86_64" ]; then
            KER=2.6.32
            SUF=el6.x86_64
        elif [ "${kernel}" = "2.6.32-100.34.1.el6uek.i686" ]; then
            KER=2.6.32-100
            SUF=el6uek.i686
        elif [ "${kernel}" = "2.6.32-200.16.1.el6uek.i686" ]; then
            KER=2.6.32-200
            SUF=el6uek.i686
        elif [ "${kernel}" = "2.6.32-300.3.1.el6uek.i686" ]; then
            KER=2.6.32-300
            SUF=el6uek.i686
        elif [ "${kernel}" = "2.6.32-400.26.1.el6uek.i686" ]; then
            KER=2.6.32-400
            SUF=el6uek.i686
        elif [ "${kernel}" = "2.6.39-100.5.1.el6uek.i686" ]; then
            KER=2.6.39-100
            SUF=el6uek.i686
        elif [ "${kernel}" = "2.6.39-200.24.1.el6uek.i686" ]; then
            KER=2.6.39-200
            SUF=el6uek.i686
        elif [ "${kernel}" = "2.6.39-300.17.1.el6uek.i686" ]; then
            KER=2.6.39-300
            SUF=el6uek.i686
        elif [ "${kernel}" = "2.6.39-400.17.1.el6uek.i686" ]; then
            KER=2.6.39-400
            SUF=el6uek.i686
        elif [ "${kernel}" = "2.6.32-100.28.5.el6.x86_64" ]; then
            KER=2.6.32-100
            SUF=el6.x86_64
        elif [ "${kernel}" = "2.6.32-100.34.1.el6uek.x86_64" ]; then
            KER=2.6.32-100
            SUF=el6uek.x86_64
        elif [ "${kernel}" = "2.6.32-200.16.1.el6uek.x86_64" ]; then
            KER=2.6.32-200
            SUF=el6uek.x86_64
        elif [ "${kernel}" = "2.6.32-300.3.1.el6uek.x86_64" ]; then
            KER=2.6.32-300
            SUF=el6uek.x86_64
        elif [ "${kernel}" = "2.6.32-400.26.1.el6uek.x86_64" ]; then
            KER=2.6.32-400
            SUF=el6uek.x86_64
        elif [ "${kernel}" = "2.6.39-100.5.1.el6uek.x86_64" ]; then
            KER=2.6.39-100
            SUF=el6uek.x86_64
        elif [ "${kernel}" = "2.6.39-200.24.1.el6uek.x86_64" ]; then
            KER=2.6.39-200
            SUF=el6uek.x86_64
        elif [ "${kernel}" = "2.6.39-300.17.1.el6uek.x86_64" ]; then
            KER=2.6.39-300
            SUF=el6uek.x86_64
        elif [ "${kernel}" = "2.6.39-400.17.1.el6uek.x86_64" ]; then
            KER=2.6.39-400
            SUF=el6uek.x86_64
        elif [ "${kernel}" = "3.8.13-16.2.1.el6uek.x86_64" ]; then
            KER=3.8.13
            SUF=el6uek.x86_64
        elif $(echo $kernel | grep -q "el6uek.x86_64"); then
            KER=$(echo $kernel | cut -d. -f1-3 | cut -f 1 -d "-")
            SUF="el6uek.x86_64"
        fi

        # Find all available kernel directories matching kernel version and suffix
        for k_dir in `find /lib/modules/${KER}*${SUF} -maxdepth 0 -type d 2>/dev/null`
        do
            base_kdir=`basename $k_dir`
            test -f /boot/vmlinuz-$base_kdir || continue
            NEW_KERNEL=0

            # Copy involflt.ko to each kernel directory
            if [ -f ${k_dir}/kernel/drivers/char/involflt.ko ]; then
                echo "${k_dir}/kernel/drivers/char/involflt.ko already exists" >> /var/log/InMage_drivers.log
            else
                # Copy involflt.ko to each kernel directory
                if [ -d ${k_dir}/kernel/drivers/char ]; then
                    echo "Copying ${DEPLOY_DIR}/bin/involflt.ko.${kernel} to ${k_dir}/kernel/drivers/char/involflt.ko" >> /var/log/InMage_drivers.log
                    cp -f ${DEPLOY_DIR}/bin/involflt.ko.${kernel} ${k_dir}/kernel/drivers/char/involflt.ko
                    if [ $? -eq 0 ]; then
                        echo "Installed involflt.ko successfully under ${k_dir}" >> /var/log/InMage_drivers.log
                        NEW_KERNEL=1
                    fi
                else
                    echo "Could not find ${k_dir}/kernel/drivers/char directory" >> /var/log/InMage_drivers.log
                fi
            fi

            if [ "${NEW_KERNEL}" -eq 1 ]; then
                # Run depmod if it is new kernel
                echo "Running depmod for kernel $k_dir ..." >> /var/log/InMage_drivers.log
                echo "" >> /var/log/InMage_drivers.log
                depmod `basename $k_dir`
                regenerate_list="`basename $k_dir` $regenerate_list" 
            fi
        done
    done
elif [ "${OS}" = "SLES12-64" -o "${OS}" = "SLES15-64" ]; then
    echo "OS is SLES12-64 or SLES15-64. Copying involflt.ko to new kernels which are installed after agent installation." >> /var/log/InMage_drivers.log
    for TOP_DIR in `echo /lib/modules/*`
    do
        base_kdir=`basename $TOP_DIR`
        test -f /boot/vmlinuz-$base_kdir || continue
        DRV_DIR=$TOP_DIR/kernel/drivers/char
        if [ -d ${DRV_DIR} ]; then
            KER_DIR=$(dirname $(dirname $(dirname $DRV_DIR)))
            KER_VER=${KER_DIR##*/}
            if [ -f ${DRV_DIR}/involflt.ko ]; then
                echo "involflt.ko already exits under ${DRV_DIR}." >> /var/log/InMage_drivers.log
            else
                vaild_ker=`is_sles_kernel_supported $KER_VER $DEPLOY_DIR`
                echo "The kernel returned from is_sles_kernel_supported  is ${vaild_ker} and return value is $?" >> /var/log/InMage_drivers.log
                if [ -f ${DEPLOY_DIR}/bin/drivers/involflt.ko.${vaild_ker} ]; then
                    echo "Copying ${DEPLOY_DIR}/bin/drivers/involflt.ko.${vaild_ker} as ${DRV_DIR}/involflt.ko" >> /var/log/InMage_drivers.log
                    cp -f ${DEPLOY_DIR}/bin/drivers/involflt.ko.${vaild_ker} ${DRV_DIR}/involflt.ko >> /var/log/InMage_drivers.log
                    if [ $? -eq 0 ]; then
                        echo "Installed involflt.ko successfully under ${DRV_DIR}." >> /var/log/InMage_drivers.log
                        echo "Running depmod for kernel $KER_VER ..." >> /var/log/InMage_drivers.log
                        depmod $KER_VER
                        if [ $? -eq 0 ]; then
                            echo "depmod successful for above kernel..." >> /var/log/InMage_drivers.log
                            regenerate_list="$KER_VER $regenerate_list" 
                        else
                            echo "depmod did not succeed for kernel ${KER_VER}..." >> /var/log/InMage_drivers.log
                            echo "This may affect loading of drivers when you boot this system using the kernel $KER_VER." >> /var/log/InMage_drivers.log
                        fi
                    else
                        echo "---> Couldn't install involflt.ko under ${DRV_DIR}" >> /var/log/InMage_drivers.log
                    fi
                else
                    echo "Did not find driver for ${KER_VER}. It is not yet supported." >> /var/log/InMage_drivers.log
                fi
            fi
        else
            echo "---> Directory $DRV_DIR could not be found on this system - skipping installation of driver involflt." >> /var/log/InMage_drivers.log
        fi
    done
else
    echo "Copying involflt.ko to new kernels which are installed after agent installation." >> /var/log/InMage_drivers.log
    # Install involflt driver in new kernels. Skip driver installation when involflt.ko already exists.
    for TOP_DIR in `echo /lib/modules/*`
    do
        base_kdir=`basename $TOP_DIR`
        test -f /boot/vmlinuz-$base_kdir || continue
        DRV_DIR=$TOP_DIR/kernel/drivers/char
        if [ -d ${DRV_DIR} ]; then
            KER_DIR=$(dirname $(dirname $(dirname $DRV_DIR)))
            KER_VER=${KER_DIR##*/}
            if [ -f ${DRV_DIR}/involflt.ko ]; then
                echo "involflt.ko already exits under ${DRV_DIR}." >> /var/log/InMage_drivers.log
            else
                if [ -f ${DEPLOY_DIR}/bin/drivers/involflt.ko.${KER_VER} ]; then
                    echo "Copying ${DEPLOY_DIR}/bin/drivers/involflt.ko.${KER_VER} as ${DRV_DIR}/involflt.ko" >> /var/log/InMage_drivers.log
                    cp -f ${DEPLOY_DIR}/bin/drivers/involflt.ko.${KER_VER} ${DRV_DIR}/involflt.ko >> /var/log/InMage_drivers.log
                    if [ $? -eq 0 ]; then
                        echo "Installed involflt.ko successfully under ${DRV_DIR}." >> /var/log/InMage_drivers.log
                        echo "Running depmod for kernel $KER_VER ..." >> /var/log/InMage_drivers.log
                        depmod $KER_VER
                        if [ $? -eq 0 ]; then
                            echo "depmod successful for above kernel..." >> /var/log/InMage_drivers.log
                            regenerate_list="$KER_VER $regenerate_list" 
                        else
                            echo "depmod did not succeed for kernel ${KER_VER}..." >> /var/log/InMage_drivers.log
                            echo "This may affect loading of drivers when you boot this system using the kernel $KER_VER." >> /var/log/InMage_drivers.log
                        fi
                    else
                        echo "---> Couldn't install involflt.ko under ${DRV_DIR}" >> /var/log/InMage_drivers.log
                    fi
                else
                    echo "Did not find driver for ${KER_VER}. It is not yet supported." >> /var/log/InMage_drivers.log
                fi
            fi
        else
            echo "---> Directory $DRV_DIR could not be found on this system - skipping installation of driver involflt." >> /var/log/InMage_drivers.log
        fi
    done
fi

if [ "$1" != "noinitrd" ]; then
    echo "Regenerating initrd for $regenerate_list" >> /var/log/InMage_drivers.log
    for K_VER in $regenerate_list; do
        $DEPLOY_DIR/scripts/initrd/install.sh regenerate $K_VER >> /var/log/InMage_drivers.log
    done
fi
