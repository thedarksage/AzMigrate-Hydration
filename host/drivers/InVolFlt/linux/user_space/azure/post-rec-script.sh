#!/bin/bash -x

_DATE_STRING_=`date +'%b_%d_%H_%M_%S'`
# Linux Integration Service binaries provided by Microsoft
lispath=""
# root fs mountpoint
mntpath=""

pstore=/etc/vxagent/involflt
pkgstore=/opt/draas_packages/
#Fetch hostname from rootfs
#hostname="`hostname`"
vx_dir=""
fx_dir=""
os_name=""
pyasn_rhel6_pkg="python-pyasn1-0.0.12a-1.el6.noarch.rpm"
wal_rhel6_pkg="WALinuxAgent-*.noarch.rpm"
wal_rhel6_pkg="$lispath/kmod-microsoft-hyper-v-rhel63*.x86_64.rpm"

_DATE_STRING_=`date +'%b_%d_%H_%M_%S'`

_SUCCESS_=0
_FAILED_=1
_INMAGE_ORG_="-INMAGE-ORG"
_INMAGE_MOD_="-INMAGE-MOD"

#This handles change requirements in lvm.conf file in initrd image.
#input : initrd file name.
#Output : Either SUCCESS or FAILED.
function CheckLvmConfFile
{
        _retCode_=$_SUCCESS_

         #filter=[ "a|^/dev/cciss/c0d0$|", "a|^/dev/mapper/*|", "r/.*/" ]
        #need to split to get initrd file Name.
        initrd_image_absolutepath=$1
        initrd_image=`echo $initrd_image_absolutepath | sed -e "s:[/].*[/]::"`
        initrd_image=`echo $initrd_image | sed -e "s:/::"`
        out_initrd_image="$initrd_image.out"
        DATE_TIME_SUFFIX=`date +%d`_`date +%b`_`date +%Y`_`date +%H`_`date +%M`_`date +%S`
        initrd_temp_dir="/tmp/temp_$DATE_TIME_SUFFIX"
        boot_temp_dir=$initrd_temp_dir
        /bin/mkdir $initrd_temp_dir

        (cd $initrd_temp_dir ; gunzip -9 < $initrd_image_absolutepath > ../$initrd_image.unzipped ; cpio -id < ../$initrd_image.unzipped)
        cd $initrd_temp_dir
        _lvm_conf_file_="$initrd_temp_dir/etc/lvm/lvm.conf"
        if [ ! -f "$_lvm_conf_file_" ]; then
                #mean to say lvm.conf does not exist no need to change any thing...
                echo "initrd : $initrd_image, does not contain lvm.conf file."
        else
                cp -f "$_lvm_conf_file_" "$_lvm_conf_file_.old"
                #cp -f "${initrd_temp_dir}/init" "${_LOGS_}"
                #now we have to make in place replacement in lvm.conf file.
                #First we need to get the Line and then
                #check if all block devices are allowed?
                #if not, then check whether sd pattern is already allowed.
                #if not, then add sd* pattern to list of allowed block devices.
                _line_=`cat "$_lvm_conf_file_" | grep -P "^(\s*)filter(\s*)=(\s*).*"`
                echo $_line_
                _line_number_=`cat "$_lvm_conf_file_" | grep -P -n "^(\s*)filter(\s*)=(\s*).*" | awk -F":" '{print $1}'`
                sed -i "$_line_number_ c\filter=\"a\/.*\/\"" $_lvm_conf_file_

                #cp -rf "etc/" "${_LOGS_}"
                #now again we have zip the file and then replace image file.
                (cd $initrd_temp_dir/ ; find . | cpio --create --format='newc' > $boot_temp_dir/../$initrd_image.unzipped_$DATE_TIME_SUFFIX)
                (cd $initrd_temp_dir/ ; gzip -9 < $boot_temp_dir/../$initrd_image.unzipped_$DATE_TIME_SUFFIX > $boot_temp_dir/../$out_initrd_image)
                mv -f "$boot_temp_dir/../$out_initrd_image" "$initrd_image_absolutepath"
                rm -f "$boot_temp_dir/../$initrd_image.unzipped_$DATE_TIME_SUFFIX"
        fi

        rm -f "/$boot_temp_dir/../$initrd_image.unzipped"
        rm -rf "$initrd_temp_dir"


        return $_retCode_
}
#This handles all requiments to make a modification in initrd file..
#input : either it can be initrd file or virtual initrd file.
#return SUCCESS or FAILURE.
function ModifyVirtualInitrdFile
{
        _retCode_=$_SUCCESS_
        _initrd_filename_=$1
        echo "Checking \"$_initrd_filename_\" for modifications if required any."

        CheckLvmConfFile $_initrd_filename_
        if [ 0 -ne $? ]; then
                echo "ERROR :: Failed to modify lvm.conf file in initrd image : $_initrd_filename_."
                _retCode_=$_FAILED_
        fi

        return $_retCode_
}
#THIS FUNCTION RENAMES INITRD OR INITRAMFS FILES.
function Rename_INITRD_RAMFS_Files
{
        #FIRST LIST ALL PATTERNS WHICH ARE IN THE FORM OF "-InMage-Virtual"
        _boot_path_=$1
        _virtual_initrd_ramfs_files_=`find $_boot_path_ -name "*-InMage-Virtual"`
        echo "initrd files = $_virtual_initrd_ramfs_files_"
        for _virtual_initrd_ramfs_file_ in ${_virtual_initrd_ramfs_files_}
        do
                #here we need to check if we have to modify lvm.conf file in virtual Initrd image.
                ModifyVirtualInitrdFile $_virtual_initrd_ramfs_file_
                if [ 0 -ne $? ]; then
                        echo "Error :: Failed to modify virtual initrd image file for any changes."
                fi

                _initrd_ramfs_file_=`echo $_virtual_initrd_ramfs_file_ | sed "s/-InMage-Virtual$//g"`
                echo "Initrd file = $_initrd_ramfs_file_"
                #TAKE COPY OF THIS FILE
                #CHECK IF THE ORIG FILE ALREADY EXISTS?
                if [ -f "${_initrd_ramfs_file_}${_INMAGE_ORG_}" ]; then
                	/bin/mv -f  "${_initrd_ramfs_file_}${_INMAGE_ORG_}" "$_initrd_ramfs_file_"
                fi

                /bin/cp -f  "$_initrd_ramfs_file_" "${_initrd_ramfs_file_}${_INMAGE_ORG_}"
               /bin/cp -f  "$_virtual_initrd_ramfs_file_" "$_initrd_ramfs_file_"
        done
}
# This function appends kernel entry in grub with numa=off console=ttyS0 earlyprintk=ttyS0 rootdelay=300
# Remove rhgb quiet from grub configuration file
function ModifyMenuListFile
{
        _grub_path_="$1/grub"
        _menu_list_file_path_="$_grub_path_/menu.lst"
        # ensure file exists
        if [ ! -f "$_menu_list_file_path_" ]; then
                echo "ERROR :: Failed to find file \"menu.lst\" in path \"$_grub_path_\"."
                echo "ERROR :: Could not modify \"menu.lst\" file."
                return $_FAILED_
        fi
        if [ -h "$_menu_list_file_path_" ]; then
            tmp_file=`ls -l "${_menu_list_file_path_}" | awk -F'->' '{ print $2 }'`
            tmp_file=`echo $tmp_file | sed -e 's: ::g'`
            _menu_list_file_path_=$_grub_path_/$tmp_file
            echo "Found grub file - ${_menu_list_file_path_}"
            if [ ! -f "$_menu_list_file_path_" ]; then
                echo "ERROR :: Failed to find file ${_menu_list_file_path_}"
                echo "ERROR :: Could not modify grub file."
                return $_FAILED_
            fi
        fi

        # backup the original file
        _menu_list_file_orig_="${_menu_list_file_path_}${_INMAGE_ORG_}"
        if [ -f "${_menu_list_file_orig_}" ]; then
                /bin/mv -f "${_menu_list_file_orig_}" "$_menu_list_file_path_"
        fi

        /bin/cp -f "$_menu_list_file_path_" "${_menu_list_file_orig_}"
        sed -i --follow-symlinks "s/ rhgb / /" ${_menu_list_file_path_}
        sed -i --follow-symlinks "s/quiet/ /" ${_menu_list_file_path_}
        grep "numa=off  console=ttyS0  earlyprintk=ttyS0  rootdelay=300 " ${_menu_list_file_path_}
        if [ $? -ne 0 ]; then
                sed -i  --follow-symlinks  '/[\t]*[ ]*kernel.*\/.*root=/s/$/& numa=off  console=ttyS0  earlyprintk=ttyS0  rootdelay=300 /' ${_menu_list_file_path_}
        fi

        # Temporary workaround for sym link breakages
        _menu_list_file_path_bak_="$_grub_path_/grub.conf"
        sed -i --follow-symlinks "s/ rhgb / /" ${_menu_list_file_path_bak_}
        sed -i --follow-symlinks "s/quiet/ /" ${_menu_list_file_path_bak_}
        grep "numa=off  console=ttyS0  earlyprintk=ttyS0  rootdelay=300 " ${_menu_list_file_path_bak_}
        if [ $? -ne 0 ]; then
                sed -i  --follow-symlinks  '/[\t]*[ ]*kernel.*\/.*root=/s/$/& numa=off  console=ttyS0  earlyprintk=ttyS0  rootdelay=300 /' ${_menu_list_file_path_bak_}
        fi
        return $_SUCCESS_
}

# Check if file exists
file_exist()
{
   file="$1"
   if [ ! -f $file ]
   then
     return  1
   else
      return 0
   fi
}

echo "-------------------  $_DATE_STRING_ : Starting post recovery script -------------------"

if [ ! $1 ]
then
    echo "Provide Linux Root Filesystem mount point"
    exit 1
else
    if [ -d $1 ]
    then
        mntpath=$1
        /bin/mount | grep "$mntpath"
        if [ $? -eq 0 ]; then
            echo "Root fs $mntpath is mounted"
        else
            echo "ERROR - Root Filesystem is not mounted at directory $mntpath"
            exit 1
        fi
    else
        echo "ERROR - Provided Linux Root Filesystem mount point is not a directory $mntpath"
        exit 1
    fi
fi

if [ -f /usr/local/.vx_version ]; then
    vx_dir=`grep INSTALLATION_DIR /usr/local/.vx_version | awk -F'=' '{print $2 }'`
else
    if [ -d /usr/local/ASR/Vx ]; then
        vx_dir=/usr/local/ASR/Vx
    else
        echo ""
        echo "InMage Scout product is not installed on the system. Exiting!"
        echo ""
        exit
    fi
fi

if [ -f /usr/local/.fx_version ]; then
    fx_dir=`grep INSTALLATION_DIR /usr/local/.fx_version | awk -F'=' '{print $2 }'`
else
    if [ -d /usr/local/ASR/Fx ]; then
        fx_dir=/usr/local/ASR/Fx
    fi
fi

# Needs to be modified with generic name for configuration files
config_path=$vx_dir/scripts/azure/config/
os_details_script=$vx_dir/scripts/azure/OS_details_target.sh

lis_package="LinuxICv*.iso"

if [ ! -f $vx_dir/scripts/azure/.azurepackages ]; then
    pushd .
    mkdir -p $config_path
    /bin/cp -f -R $pkgstore/* $config_path
    cp -f $config_path/../* $config_path/ 

    file_exist $config_path/$lis_package
    if [ $? -eq 0 ]; then
       echo "$lis_package package exists."
    else
       echo "ERROR - $lis_package package does not exist"
    fi

    cd $config_path
    iso_file=`ls $lis_package`
    if [ $? -ne 0 ]; then
       echo "ERROR - More than one $lis_package package exist"
       popd
    fi

    mkdir -p $vx_dir/scripts/azure/isomountpoint
    /bin/mount -o loop $iso_file $vx_dir/scripts/azure/isomountpoint
    if [ $? -eq 0 ]; then
        echo "LIS package iso mount success"
        cp -R $vx_dir/scripts/azure/isomountpoint $vx_dir/scripts/azure/packages/
        touch $vx_dir/scripts/azure/.azurepackages
        echo "Unmounting $vx_dir/scripts/azure/isomountpoint ISO  $iso_file"
        umount $vx_dir/scripts/azure/isomountpoint
        /bin/mv -f $iso_file .$iso_file
    else
        echo "Error: LIS package iso mount failure"
    fi
    popd
fi

os_name=`$os_details_script $mntpath $vx_dir`

echo "-- OS Information --"
echo "$os_name is being recovered now"
cat $mntpath/etc/*release*

echo "-- Starting system reconfig --"
case $os_name in
    RHEL6U3-64)
        lispath=$vx_dir/scripts/azure/packages/RHEL63

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel63*.x86_64.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel63*.x86_64.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL6U3-32)
        lispath=$vx_dir/scripts/azure/packages/RHEL63

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel63*.i686.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel63*.i686.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL6U4-*)
        lispath=$vx_dir/scripts/azure/packages/RHEL63

        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL6U5-*)
        lispath=$vx_dir/scripts/azure/packages/RHEL63

        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL6U6p-*)
        lispath=$vx_dir/scripts/azure/packages/RHEL63

        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL6-64)
        lispath=$vx_dir/scripts/azure/packages/RHEL6012

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel6012*.x86_64.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel6012*.x86_64.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL6-32)
        lispath=$vx_dir/scripts/azure/packages/RHEL6012

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel6012*.i686.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel6012*.i686.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL6U1-64)
        lispath=$vx_dir/scripts/azure/packages/RHEL6012

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel6012*.x86_64.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel6012*.x86_64.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL6U1-32)
        lispath=$vx_dir/scripts/azure/packages/RHEL6012

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel6012*.i686.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel6012*.i686.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL6U2-64)
        lispath=$vx_dir/scripts/azure/packages/RHEL6012

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel6012*.x86_64.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel6012*.x86_64.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL6U2-32)
        lispath=$vx_dir/scripts/azure/packages/RHEL6012

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel6012*.i686.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel6012*.i686.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL5U5-64)
        lispath=$vx_dir/scripts/azure/packages/RHEL56/lis-55/x86_64/

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v*.x86_64.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v*.x86_64.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el5.rf.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL5U5-32)
        lispath=$vx_dir/scripts/azure/packages/RHEL56/lis-55/x86/

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v*.i686.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v*.i686.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el5.rf.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL5U6-64)
        lispath=$vx_dir/scripts/azure/packages/RHEL56/lis-56/x86_64/

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v*.x86_64.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v*.x86_64.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el5.rf.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL5U6-32)
        lispath=$vx_dir/scripts/azure/packages/RHEL56/lis-56/x86/

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v*.i686.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v*.i686.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el5.rf.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL5U7-64)
        lispath=$vx_dir/scripts/azure/packages/RHEL57/lis-57/x86_64/

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v*.x86_64.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v*.x86_64.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el5.rf.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL5U7-32)
        lispath=$vx_dir/scripts/azure/packages/RHEL57/lis-57/x86/

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v*.i686.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v*.i686.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el5.rf.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL5U8-64)
        lispath=$vx_dir/scripts/azure/packages/RHEL58/lis-58/x86_64/

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v*.x86_64.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v*.x86_64.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el5.rf.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL5U8-32)
        lispath=$vx_dir/scripts/azure/packages/RHEL58/lis-58/x86/

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v*.i686.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v*.i686.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el5.rf.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL5U9-64)
        lispath=$vx_dir/scripts/azure/packages/RHEL59/lis-59/x86_64/

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v*.x86_64.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v*.x86_64.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el5.rf.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    RHEL5U9-32)
        lispath=$vx_dir/scripts/azure/packages/RHEL59/lis-59/x86/

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v*.i686.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v*.i686.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el5.rf.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    OL6-64)
        lispath=$vx_dir/scripts/azure/packages/RHEL6012

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel6012*.x86_64.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel6012*.x86_64.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    OL6-32)
        lispath=$vx_dir/scripts/azure/packages/RHEL6012

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel6012*.i686.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel6012*.i686.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    OL6U1-64)
        lispath=$vx_dir/scripts/azure/packages/RHEL6012

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel6012*.x86_64.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel6012*.x86_64.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    OL6U1-32)
        lispath=$vx_dir/scripts/azure/packages/RHEL6012

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel6012*.i686.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel6012*.i686.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    OL6U2-64)
        lispath=$vx_dir/scripts/azure/packages/RHEL6012

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel6012*.x86_64.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel6012*.x86_64.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    OL6U2-32)
        lispath=$vx_dir/scripts/azure/packages/RHEL6012

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel6012*.i686.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel6012*.i686.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    OL6U3-64)
        lispath=$vx_dir/scripts/azure/packages/RHEL63

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel63*.x86_64.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel63*.x86_64.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    OL6U3-32)
        lispath=$vx_dir/scripts/azure/packages/RHEL63

        rpm --root $mntpath --replacepkgs -ivh $lispath/kmod-microsoft-hyper-v-rhel63*.i686.rpm
        rpm --root $mntpath  --replacepkgs -ivh $lispath/microsoft-hyper-v-rhel63*.i686.rpm
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    OL6U4-64)
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    OL6U4-32)
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    OL6U5-64)
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    OL6U5-32)
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    OL6U6p-*)
        rpm --root $mntpath --replacepkgs -ivh $config_path/python-pyasn1-0.0.12a-1.el6.noarch.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/WALinuxAgent-*.noarch.rpm
    ;;
    SLES11-SP3-64)
        rpm --root $mntpath --replacepkgs -ivh $config_path/suse/python-pyasn1*.x86_64.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/suse/WALinuxAgent*.noarch.rpm
    ;;
    SLES11-SP3-32)
        rpm --root $mntpath --replacepkgs -ivh $config_path/suse/python-pyasn1*.i586.rpm
        # Install Windows Azure Agent
        rpm --root $mntpath --replacepkgs --nodeps -ivh $config_path/suse/WALinuxAgent*.noarch.rpm
    ;;
    *)
        echo "Incorrect input for os -  $os_name "
        lispath="/UNKNOWN/"
        ;;
esac

case $os_name in
    RHEL6*)
	ModifyMenuListFile $mntpath/boot
	echo "install ata_piix  { /sbin/modprobe hv_storvsc 2>&1 || /sbin/modprobe --ignore-install ata_piix; }" > $mntpath/etc/modprobe.d/hyperv_pvdrivers.conf
    ;;
    OL6*)
	ModifyMenuListFile $mntpath/boot
	echo "install ata_piix  { /sbin/modprobe hv_storvsc 2>&1 || /sbin/modprobe --ignore-install ata_piix; }" > $mntpath/etc/modprobe.d/hyperv_pvdrivers.conf
    ;;
    SLES11*)
	ModifyMenuListFile $mntpath/boot
	echo "install ata_piix  { /sbin/modprobe hv_storvsc 2>&1 || /sbin/modprobe --ignore-install ata_piix; }" > $mntpath/etc/modprobe.d/hyperv_pvdrivers.conf
    ;;
    *)
        echo "Incorrect input for os to modify grub entries-  $os_name "
        ;;
esac

if [ ! $lispath ]
then
    echo "Provide Linux integration service (LIS) binary locations. Continuing!"
else
    if [ ! -d $2 ]
    then
        echo "Provided LIS path is not a directory. Continuting!"
    fi
fi
echo "Rootfs mount point:$mntpath LIS Path:$lispath configuration path:$config_path"

# OS configuration changes
case $os_name in
    RHEL6*)
        hostname=`grep HOSTNAME  $mntpath/etc/sysconfig/network | awk -F= '{ print $2 }'`
        hostname=`echo $hostname | sed -e 's: ::g'`

        if [ ! ${hostname} ]; then
                hostname=localhost
        fi
        #backup network file
        #/bin/cp -f $mntpath/etc/sysconfig/network-scripts/ifcfg-eth0 $mntpath/etc/sysconfig/network-scripts/ifcfg-eth0"${_INMAGE_ORG_}"
        #/bin/cp -f $config_path/ifcfg-eth0 $mntpath/etc/sysconfig/network-scripts/ifcfg-eth0
        #chown --reference=$mntpath/etc/sysconfig/network-scripts/ifcfg-eth0"${_INMAGE_ORG_}" $mntpath/etc/sysconfig/network-scripts/ifcfg-eth0
        #chmod --reference=$mntpath/etc/sysconfig/network-scripts/ifcfg-eth0"${_INMAGE_ORG_}" $mntpath/etc/sysconfig/network-scripts/ifcfg-eth0
        #sed -i --follow-symlinks "/DHCP_HOSTNAME*=*/c DHCP_HOSTNAME=${hostname}"  $mntpath/etc/sysconfig/network-scripts/ifcfg-eth0
	touch $mntpath/etc/vxagent/setazureip

        # /etc/sysconfig/network file modifications
        /bin/cp -f $mntpath/etc/sysconfig/network $mntpath/etc/vxagent/logs/network"${_INMAGE_ORG_}"
        /bin/cp -f $config_path/network $mntpath/etc/sysconfig/network
        chown --reference=$mntpath/etc/vxagent/logs/network"${_INMAGE_ORG_}" $mntpath/etc/sysconfig/network
        chmod --reference=$mntpath/etc/vxagent/logs/network"${_INMAGE_ORG_}" $mntpath/etc/sysconfig/network
        sed -i --follow-symlinks  "/HOSTNAME*=*/c HOSTNAME=${hostname}"  $mntpath/etc/sysconfig/network
	echo "GATEWAYDEV=eth0" >> $mntpath/etc/sysconfig/network

        /bin/rm -f $mntpath/etc/udev/rules.d/*-persistent-net.rules
        /bin/rm -f $mntpath/lib/udev/rules.d/*-persistent-net-generator.rules
        /bin/rpm --root $mntpath -e --nodeps NetworkManager
	/bin/rm -f $mntpath/etc/init.d/NetworkManager
        sed -i --follow-symlinks "/DHCP_HOSTNAME*=*/c DHCP_HOSTNAME=${hostname}" $mntpath/etc/waagent.conf
        sed -i --follow-symlinks  "/ResourceDisk.EnableSwap*=*/c ResourceDisk.EnableSwap=y"  $mntpath/etc/waagent.conf
        sed -i --follow-symlinks  "/ResourceDisk.SwapSizeMB*=*/c ResourceDisk.SwapSizeMB=32768"  $mntpath/etc/waagent.conf
    ;;
    OL6*)
        hostname=`grep HOSTNAME  $mntpath/etc/sysconfig/network | awk -F= '{ print $2 }'`
        hostname=`echo $hostname | sed -e 's: ::g'`

        if [ ! ${hostname} ]; then
                hostname=localhost
        fi
        #backup network file
        #/bin/cp -f $mntpath/etc/sysconfig/network-scripts/ifcfg-eth0 $mntpath/etc/sysconfig/network-scripts/ifcfg-eth0"${_INMAGE_ORG_}"
        #/bin/cp -f $config_path/ifcfg-eth0 $mntpath/etc/sysconfig/network-scripts/ifcfg-eth0
        #chown --reference=$mntpath/etc/sysconfig/network-scripts/ifcfg-eth0"${_INMAGE_ORG_}" $mntpath/etc/sysconfig/network-scripts/ifcfg-eth0
        #chmod --reference=$mntpath/etc/sysconfig/network-scripts/ifcfg-eth0"${_INMAGE_ORG_}" $mntpath/etc/sysconfig/network-scripts/ifcfg-eth0
        #sed -i --follow-symlinks "/DHCP_HOSTNAME*=*/c DHCP_HOSTNAME=${hostname}"  $mntpath/etc/sysconfig/network-scripts/ifcfg-eth0
	touch $mntpath/etc/vxagent/setazureip

        # /etc/sysconfig/network file modifications
        /bin/cp -f $mntpath/etc/sysconfig/network $mntpath/etc/vxagent/logs/network"${_INMAGE_ORG_}"
        /bin/cp -f $config_path/network $mntpath/etc/sysconfig/network
        chown --reference=$mntpath/etc/vxagent/logs/network"${_INMAGE_ORG_}" $mntpath/etc/sysconfig/network
        chmod --reference=$mntpath/etc/vxagent/logs/network"${_INMAGE_ORG_}" $mntpath/etc/sysconfig/network
        sed -i --follow-symlinks  "/HOSTNAME*=*/c HOSTNAME=${hostname}"  $mntpath/etc/sysconfig/network

        /bin/rm -f $mntpath/etc/udev/rules.d/*-persistent-net.rules
        /bin/rm -f $mntpath/lib/udev/rules.d/*-persistent-net-generator.rules
        /bin/rpm --root $mntpath -e --nodeps NetworkManager
	/bin/rm -f $mntpath/etc/init.d/NetworkManager
        sed -i --follow-symlinks  "/ResourceDisk.EnableSwap*=*/c ResourceDisk.EnableSwap=y"  $mntpath/etc/waagent.conf
        sed -i --follow-symlinks  "/ResourceDisk.SwapSizeMB*=*/c ResourceDisk.SwapSizeMB=32768"  $mntpath/etc/waagent.conf
    ;;
    SLES11*)
	#/bin/mv -f $mntpath/etc/sysconfig/network/ifcfg-eth0 $mntpath/etc/sysconfig/network/ifcfg-eth0"${_INMAGE_ORG_}"
	#/bin/cp -f $config_path/suse_ifcfg_eth0 $mntpath/etc/sysconfig/network/ifcfg-eth0
	#chown --reference=$mntpath/etc/sysconfig/network/ifcfg-eth0"${_INMAGE_ORG_}" $mntpath/etc/sysconfig/network/ifcfg-eth0
	#chmod --reference=$mntpath/etc/sysconfig/network/ifcfg-eth0"${_INMAGE_ORG_}" $mntpath/etc/sysconfig/network/ifcfg-eth0
	touch $mntpath/etc/vxagent/setazureip
	/bin/mv -f $mntpath/etc/resolv.conf $mntpath/etc/resolv.conf"${_INMAGE_ORG_}"
	/bin/touch  $mntpath/etc/resolv.conf
	chown --reference=$mntpath/etc/resolv.conf"${_INMAGE_ORG_}" $mntpath/etc/resolv.conf
	chmod --reference=$mntpath/etc/resolv.conf"${_INMAGE_ORG_}" $mntpath/etc/resolv.conf
	/bin/mv -f $mntpath/etc/sysconfig/network/ifroute-eth0 $mntpath/etc/sysconfig/network/ifroute-eth0"${_INMAGE_ORG_}"
	/bin/mv -f $mntpath/etc/sysconfig/network/routes $mntpath/etc/sysconfig/network/routes"${_INMAGE_ORG_}"
	Rename_INITRD_RAMFS_Files $mntpath/boot/

	newstr=`sed  -n "/^INITRD_MODULES*/p " $mntpath/etc/sysconfig/kernel`
	echo "$newstr"|grep -q "hv_storvsc"
	if [ $? -eq 0 ];then
		echo "HyperV drivers are already present"
	else
		newstr=`echo $newstr|awk -F\" ' {print $1"\""$2 " hv_vmbus hv_storvsc hv_netvsc" "\""}'`
		sed  -i  "s/^INITRD_MODULES=\".*\"/$newstr/"  $mntpath/etc/sysconfig/kernel
	fi

        /bin/rm -f $mntpath/etc/udev/rules.d/*-persistent-net.rules
        /bin/rm -f $mntpath/lib/udev/rules.d/*-persistent-net-generator.rules
        /bin/rpm --root $mntpath -e --nodeps NetworkManager
	/bin/rm -f $mntpath/etc/init.d/NetworkManager
        sed -i --follow-symlinks  "/ResourceDisk.EnableSwap*=*/c ResourceDisk.EnableSwap=y"  $mntpath/etc/waagent.conf
        sed -i --follow-symlinks  "/ResourceDisk.SwapSizeMB*=*/c ResourceDisk.SwapSizeMB=32768"  $mntpath/etc/waagent.conf
    ;;	
    *)
        echo "In correct input for os to modify OS configuration entries-  $os_name "
        ;;
esac

pid=`ps -aef | grep hv_kvp_daemon | grep -v "grep" | awk '{ print $2 }'`
if [ $pid ]; then
    kill -9 $pid
fi

_DATE_STRING_=`date +'%b_%d_%H_%M_%S'`
echo "------------  $_DATE_STRING_: Finishing post recovery script for $os_name ----------"
exit 0
