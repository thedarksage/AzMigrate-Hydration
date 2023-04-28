#!/bin/bash

#We have to create Initrd Image file for RHEL OS. RHEL 4 has different command and RHEL 5 and 6 has different commands.
#first we will create the Initrd Image file
#prior to this point we have execute zcat commands and see whether the Modules are there or not.


#zcat /boot/initrd-`uname -r`.img | cpio -itv | grep mptspi
_OS_DETAILS_SH_F_="OS_details.sh"
_OS_NAME_=""
_azure_workflow=""
_KERNEL_LIST_=""
_KERNEL_VERSION_LIST_=""
_INITRD_FILENAMES_=""
_FAILED_=1
_SUCCESS_=0
PATH=/bin:/sbin:/usr/sbin:/usr/bin:$PATH
export PATH
_CHECKSUM_FILE_FOR_INITRD_="/etc/vxagent/vcon/checksums.info"
_MOD_MPTSAS_LOADED_=0
_PRELOAD_MODULES_=""
_BUILTIN_KMODS_=""

function FindBuiltInKernelModules
{
	_kernel_=$1
	_BUILTIN_KMODS_=""
	_rc_=$_SUCCESS_ 
	_tmp_out_=`grep CONFIG_USB_OHCI_HCD /boot/config-${_kernel_}`
	_tmp_out_=`echo $_tmp_out_ | grep "CONFIG_USB_OHCI_HCD.*=.*y"`
	if [ $? -eq 0 ]; then
	    _BUILTIN_KMODS_="${_BUILTIN_KMODS_}--builtin=ohci-hcd "
	fi
	_tmp_out_=`grep CONFIG_USB_EHCI_HCD /boot/config-${_kernel_}`
	_tmp_out_=`echo $_tmp_out_ | grep "CONFIG_USB_EHCI_HCD.*=.*y"`
	if [ $? -eq 0 ]; then
	    _BUILTIN_KMODS_="${_BUILTIN_KMODS_}--builtin=ehci-hcd "
	fi
	_tmp_out_=`grep CONFIG_USB_UHCI_HCD /boot/config-${_kernel_}`
	_tmp_out_=`echo $_tmp_out_ | grep "CONFIG_USB_UHCI_HCD.*=.*y"`
	if [ $? -eq 0 ]; then
	    _BUILTIN_KMODS_="${_BUILTIN_KMODS_}--builtin=uhci-hcd "
	fi
	return $_rc_;
    
}

function ConstructKernelImg_RHEL4
{
	#specific function for RHEL4
	_kernel_=$1
	_PRELOAD_MODULES_=""
	_rc_=$_SUCCESS_ 
	if [ $_MOD_MPTSAS_LOADED_ -eq 1 ]; then
		_PRELOAD_MODULES_="sd_mod mptsas mptspi"
	else
		_PRELOAD_MODULES_="sd_mod mptspi"
	fi
	echo "Making kernel image file for kernel version \"$_kernel_\"."
	mkinitrd  --preload="${_PRELOAD_MODULES_}" --with=e1000 --builtin=involflt -f /boot/initrd-$_kernel_.img-InMage-Virtual $_kernel_ 2> /dev/null
	_rc_=$?	

	_initrd_file_name_="/boot/initrd-$_kernel_.img-InMage-Virtual"
	/etc/vxagent/bin/inm_dmit --fsync_file "$_initrd_file_name_"
	if [ 0 -ne $? ]; then
		echo "FileSync operation failed for file ${_initrd_file_name_}."
	fi

	return $_rc_
}

function ConstructKernelImg_RHEL
{
	#common function for RHEL 5 and 6
	_kernel_=$1
	_PRELOAD_MODULES_=""
	
    _hv_drivers=""
	if [ "azure" = "$_azure_workflow" ]; then
		_hv_drivers="hv_vmbus hv_storvsc"
	fi

	_rc_=$_SUCCESS_
	if [ $_MOD_MPTSAS_LOADED_ -eq 1 ]; then
		_PRELOAD_MODULES_="sd_mod mptsas mptspi $_hv_drivers"
	else
		_PRELOAD_MODULES_="sd_mod mptspi $_hv_drivers"
	fi
	FindBuiltInKernelModules "${_kernel_}"
	echo "Making Kernel image file for kernel version \"$_kernel_\"."
	_initrd_file_name_=""
	#should we have to check whether kernel image file name os initrd or initramfs?
	if [ -f /boot/initrd-$_kernel_.img ];	then 
		_initrd_file_name_="/boot/initrd-$_kernel_.img-InMage-Virtual"
		mkinitrd  --preload="${_PRELOAD_MODULES_}" --with=e1000 ${_BUILTIN_KMODS_} -f /boot/initrd-$_kernel_.img-InMage-Virtual $_kernel_ 2> /dev/null
	elif [ -f /boot/initramfs-$_kernel_.img ];	then
		_initrd_file_name_="/boot/initramfs-$_kernel_.img-InMage-Virtual"
		mkinitrd  --preload="${_PRELOAD_MODULES_}" --with=e1000 ${_BUILTIN_KMODS_} -f /boot/initramfs-$_kernel_.img-InMage-Virtual $_kernel_ 2> /dev/null
	fi
	_rc_=$?

	/etc/vxagent/bin/inm_dmit --fsync_file "$_initrd_file_name_"
	if [ 0 -ne $? ]; then
		echo "FileSync operation failed for file ${_initrd_file_name_}."
	fi

	return $_rc_
}

function ConstructKernelImg_OL
{
	#common function for OL 5 and 6
	_kernel_=$1
	_PRELOAD_MODULES_=""
	
    _hv_drivers=""
	if [ "azure" = "$_azure_workflow" ]; then
		_hv_drivers="hv_vmbus hv_storvsc"
	fi

	_rc_=$_SUCCESS_
	if [ $_MOD_MPTSAS_LOADED_ -eq 1 ]; then
		_PRELOAD_MODULES_="sd_mod mptsas mptspi $_hv_drivers"
	else
		_PRELOAD_MODULES_="sd_mod mptspi $_hv_drivers"
	fi
	FindBuiltInKernelModules "${_kernel_}"
	echo "Making Kernel image file for kernel version \"$_kernel_\"."
	_initrd_file_name_=""
	#should we have to check whether kernel image file name os initrd or initramfs?
	if [ -f /boot/initrd-$_kernel_.img ];	then 
		_initrd_file_name_="/boot/initrd-$_kernel_.img-InMage-Virtual"
		mkinitrd  --preload="${_PRELOAD_MODULES_}" --with=e1000 ${_BUILTIN_KMODS_} -f /boot/initrd-$_kernel_.img-InMage-Virtual $_kernel_ 2> /dev/null
	elif [ -f /boot/initramfs-$_kernel_.img ];	then
		_initrd_file_name_="/boot/initramfs-$_kernel_.img-InMage-Virtual"
		mkinitrd  --preload="${_PRELOAD_MODULES_}" --with=e1000 ${_BUILTIN_KMODS_} -f /boot/initramfs-$_kernel_.img-InMage-Virtual $_kernel_ 2> /dev/null
	fi
	_rc_=$?

	/etc/vxagent/bin/inm_dmit --fsync_file "$_initrd_file_name_"
	if [ 0 -ne $? ]; then
		echo "FileSync operation failed for file ${_initrd_file_name_}."
	fi
	return $_rc_
}

function ConstructKernelImg_SLES
{
	#Check if reiserfs is required?
	#in -m list also specify reiserfs list.
	_kernel_=$1
	_reiserfs_module=""
	_hv_drivers=""
	_root_device_fs_type_=`mount | grep " on / " | awk '{print $5}'`
	if [ "reiserfs" = "{$_root_device_fs_type_}" ]; then
		_reiserfs_module="reiserfs"
	fi
	if [ "azure" = "$_azure_workflow" ]; then
		_hv_drivers="hv_vmbus hv_storvsc hv_netvsc"
	fi

	_PRELOAD_MODULES_=""
	if [ $_MOD_MPTSAS_LOADED_ -eq 1 ]; then
		_PRELOAD_MODULES_="sd_mod mptsas mptspi ${_reiserfs_module} ${_hv_drivers} e1000"
	else
		_PRELOAD_MODULES_="sd_mod mptspi ${_reiserfs_module} ${_hv_drivers} e1000"
	fi

	if [ -f /boot/vmlinux-$_kernel_ ]; then
		_kernel_file_name_="/boot/vmlinux-$_kernel_"
	elif [ -f /boot/vmlinuz-$_kernel_ ]; then
		_kernel_file_name_="/boot/vmlinuz-$_kernel_"
	fi
	
	if [ -f /boot/initrd-$_kernel_ ];	then 
		_initrd_file_name_="/boot/initrd-$_kernel_"	
	elif [ -f /boot/initramfs-$_kernel_ ];	then
		_initrd_file_name_="/boot/initramfs-$_kernel_"
	fi
	temp_modules=`cat /etc/sysconfig/kernel | grep "^INITRD_MODULES" | awk -F'=' '{ print $2 }'`
	if [ -z $temp_modules ]; then
		_module_list_new=\"$_PRELOAD_MODULES_\"
	else
        	str="s/\"$/ $_PRELOAD_MODULES_\"/"
	        echo "STRING: $str"
        	_module_list_new=`echo $temp_modules | sed "s/\"$/ $_PRELOAD_MODULES_\"/"`
	fi
	
	_initrd_symlink_="/boot/initrd"
	_initrd_symlink_tmp_="/boot/initrd_tmp"
	`/bin/cp -l ${_initrd_symlink_} ${_initrd_symlink_tmp_}`

	mkinitrd -f block -f lvm -m "$_module_list_new" -k "$_kernel_file_name_" -i "$_initrd_file_name_-InMage-Virtual" 2> /dev/null
	_returnCode_=$?

	#Here we need to perform file Sync op
	_initrd_file_name_="$_initrd_file_name_-InMage-Virtual"
	/etc/vxagent/bin/inm_dmit --fsync_file "$_initrd_file_name_"
	if [ 0 -ne $? ]; then
		echo "FileSync operation failed for file ${_initrd_file_name_}."
	fi

	`/bin/mv ${_initrd_symlink_tmp_} ${_initrd_symlink_}`
	return $_returnCode_
}

function GetOS
{
	echo "Find OS."
	#check if the _OS_DETAILS_SH_F_ exists and executable permissions are assigned to it.
	if [ ! -f "../$_OS_DETAILS_SH_F_" ] || [ ! -x "../$_OS_DETAILS_SH_F_" ]; then
		echo "ERROR :: $_OS_DETAILS_SH_F_ file is missing."
		echo "ERROR :: 1. Cross check if above file exists and executable permissions are assigned"
		return $_FAILED_
	fi
	
	#Invoke _OS_DETAILS_SH_F_ to find OS.
	_OS_NAME_=`.././$_OS_DETAILS_SH_F_ "print"`
	if [ ! ${_OS_NAME_} ]; then
		echo "ERROR :: Failed to Determine OS."
		return $_FAILED_
	fi
	echo "OS NAME=" $_OS_NAME_
	return $_SUCCESS_
}

function GetListOfKernelsInstalled
{
	echo "Finding List of Kernels Installed."
	#Finds out List of Kernels Installed in the Machine.
	#use path /lib/modules
	_kernels_=`ls /lib/modules/`
	#do anyother files exists under this Path?? Need to Cross check??
	_index_=0
	for _kernel_ in ${_kernels_}
	do
		ls /boot | grep -q "vmlinuz-$_kernel_*"
		if [ 0 -eq $? ]; then
			echo "Kernel $_kernel_ is installed."
			_KERNEL_LIST_[$_index_]=$_kernel_
			_index_=$((_index_+1))
			continue
		fi
		
		ls /boot | grep -q "vmlinux-$_kernel_*"
		if [ 0 -eq $? ]; then
			echo "Kernel $_kernel_ is installed."
			_KERNEL_LIST_[$_index_]=$_kernel_
			_index_=$((_index_+1))
			continue
		fi
		echo "Kernel $_kernel_ is not installed."
	done
	echo "List of Kernels Installed=${_KERNEL_LIST_[@]}"	
	return $_SUCCESS_
}

function check_hyperv_drivers
{
    local lsinitrd="lsinitrd $1" # Command may change for other OS
    local hv_drivers="hv_vmbus hv_storvsc"
    local drv=""
    local retval=0

    # Skip the checks if not for azure
	if [ "azure" = "$_azure_workflow" ]
    then
        for drv in $hv_drivers
        do
        	$lsinitrd | grep -q $drv 
	        if [ 0 -ne $? ]
            then
		        echo "$1 has to be build for $drv"
        		retval=2
	        fi
        done
    fi

    return $retval
}

function get_initrd_contents
{
	zcat $1 | cpio -itv
}

function CheckForRequiredModules
{
	#we have to use zcat command to find whether mptspi, e1000 and other modules pre exist?
	#on which file we have to check it is initramfs or initrd image file?
	#either it can be .img file or without any extension.
	_kernel_=$1
	err_code=$_SUCCESS_
	_kernel_file_name_=""
	if [ -f /boot/initrd-$_kernel_.img ];	then 
		_kernel_file_name_=initrd-$_kernel_.img
	elif [ -f /boot/initramfs-$_kernel_.img ];	then 
		_kernel_file_name_=initramfs-$_kernel_.img
	elif [ -f /boot/initrd-$_kernel_ ];	then
		_kernel_file_name_=initrd-$_kernel_
	elif [ -f /boot/initramfs-$_kernel_ ];	then 
		_kernel_file_name_=initramfs-$_kernel_
	elif [ -f /boot/initrd.img-$_kernel_ ]; then
        _kernel_file_name_=initrd.img-$_kernel_
	fi
	
	if [ ! ${_kernel_file_name_} ];	then
		echo "ERROR :: Failed to Find kernel image file for kernel \"$_kernel_\"."
		return $_FAILED_
	fi
	
	#check if file already exists?
	#is this right coding? we have to store MD5 checksumm of existing kernel and then we have check if it has changed?
	if [ -f "$_kernel_file_name_-InMage-Virtual" ]; then
		echo "InMage Init/Initramfs virtual compliant file already exists for kernel \"$_kernel_\"."
		return $_SUCCESS_
	fi

	lsmod | grep -i mptsas 2>&1 /dev/null
	if [ $? -eq 0 ]; then
            _MOD_MPTSAS_LOADED_=1
	fi

    local lsinitrd=""
    case $_OS_NAME_ in
        RHEL7-64 | OL7-64 | SLES12*)   # TODO: Should be checking for hyper-v drivers on SLES as well
                    check_hyperv_drivers "/boot/$_kernel_file_name_"
                    err_code=$?
                    lsinitrd="lsinitrd /boot/$_kernel_file_name_"
                    ;;

        *)          # TODO: We should move to using standard lsinitrd command 
                    # in future for all OS instead of using our own custom commands
                    lsinitrd="get_initrd_contents /boot/$_kernel_file_name_"
                    ;;
    esac
        
    # TODO: Do we skip these checks for azure workflow ?????
    #we have kernel image file name.Now use zcat command to check for modules.
	$lsinitrd | grep mptspi
	if [ 0 -ne $? ];	then
		echo "Initrd/Initramfs Image has to be build for kernel $_kernel_ for mptspi."
		err_code=2
	fi

	$lsinitrd | grep e1000
	if [ 0 -ne $? ];	then
		echo "Initrd/Initramfs Image has to be built for kernel $_kernel_."
		err_code=2
	fi

	$lsinitrd | grep "sd_mod.ko"
	if [ 0 -ne $? ];	then
		echo "Initrd/Initramfs Image has to be built for kernel $_kernel_."
		err_code=2
	fi

	if  [ ! -d "/etc/vxagent/vcon" ]; then
		mkdir -p "/etc/vxagent/vcon"
    fi

	if [ -f /boot/$_kernel_file_name_-InMage-Virtual ] && [ -f "$_CHECKSUM_FILE_FOR_INITRD_" ]; then
		md5sum_current=`md5sum /boot/$_kernel_file_name_ | awk '{ print $1 }'`
		md5sum_cache=`cat $_CHECKSUM_FILE_FOR_INITRD_ | grep "/boot/$_kernel_file_name_" | awk '{ print $1 }'`
                if [ "${md5sum_current}" != "${md5sum_cache}" ]; then
            echo "md5sum($_kernel_file_name_): cur = $md5sum_current old = $md5sum_cache"
            # Fix for bug where multiple checksum entries were 
            # created for single initrd image. We delete all entries
            # that refer to the single initrd file name
            sed -i /$_kernel_file_name_/d $_CHECKSUM_FILE_FOR_INITRD_
            # In case there are some characters in the initrd fname
            # which are not handled by sed, remove all the entries 
            # with matching checksum
            sed -i /$md5sum_cache/d $_CHECKSUM_FILE_FOR_INITRD_
                        md5sum "/boot/$_kernel_file_name_" >> $_CHECKSUM_FILE_FOR_INITRD_
			err_code=2
        else
            # InMage-Virtual image already generated for this initrd
		    err_code=0
		fi
	else
        # Only add md5sum if InMage-Virtual needs to be generated
        if [ $err_code -eq 2 ]; then
		md5sum /boot/$_kernel_file_name_ >> $_CHECKSUM_FILE_FOR_INITRD_
	fi
	fi

	
	return $err_code
}

function CheckInitrdImage
{
	#first we need to identify whether we need to BUILD or is already available.
	#So we need to check whether initrd image file contains required modules.
	#first we need to use zcat
	#if found no need to build initrd image file.
	#we need to write a file which has information of ListOfKernels:NewInitrdFileName:checkSum
	for _kernel_ in ${_KERNEL_LIST_[@]}
	do 
		CheckForRequiredModules "$_kernel_"
		case $? in 
			0)
				echo "No Need to Build kernel module for kernel $_kernel_."
				;;
			1)
				echo "ERROR :: Failed to check whether Kernel has to be built or not."
				;;
			2)
				echo "Kernel has to be rebuilt for $_kernel_."
				#here we need to contruct kernel image with required modules.
				#we have to check return codes for this.
				ConstructKernelImage "$_kernel_"
				;;
			*)
				echo "ERROR :: Un-expected return code."
				echo "ERROR :: Function Name: CheckForRequiredModules,return code=$?."
				;;
		esac
	done
}

function ConstructKernelImage
{
	#Contruct Kernel Image file according to the OS type for RHEL4? RHEL 5 &6 ? SLES?
	#we have to collect return code from the function and make sure that it is success.
	_ret_code_=""
	case $_OS_NAME_ in 
		RHEL3*)
			echo "It is RHEL 3. It is not supported right now"
			;;
		RHEL4*)
			echo "It is RHEL 4."
			ConstructKernelImg_RHEL4 "$1"
			_ret_code_=$?
			;;
		RHEL*) 
			echo "It is either RHEL 5 or 6"
			ConstructKernelImg_RHEL "$1"
			_ret_code_=$?
			;;
		SLES*) 
			echo "SLES Machine."
			ConstructKernelImg_SLES "$1"
			_ret_code_=$?
			;;
		OL*) 
			echo "It is either OL 5 or 6"
			ConstructKernelImg_OL "$1"
			_ret_code_=$?
			;;
		UBUNTU*)
			echo "initrd/initramfs files are not rebuild for Ubuntu as of now."
			return $_SUCCESS_ 
			;;
			
		*)
			echo "Currently This OS is not supported?Need to Add for Centos?"
			return 1
			;;
	esac
	
	#check returncode.. 
	if [ 0 -ne $_ret_code_ ]; then
		echo "ERROR :: Failed to construct kernel image file for kernel \"$1\"."
		return 1
	fi
	echo "Successfully built new kernel img file for kernel \"$1\"."	
}

function GetInitrdNamesFromMenuListFile
{
    case $_OS_NAME_ in
        RHEL7* | OL7* | SLES12*) _menu_lst_="/boot/grub2/grub.cfg"
                _GRUB_=2
                ;;

        *)      _menu_lst_="/boot/grub/menu.lst"
                _GRUB_=1
                ;;
    esac
    
    # Grub v1 & v2 have different commands/labels. We set appropriate
    # patterns here to detect them and use the patterns in our parsing
    case $_GRUB_ in
        1)      root_pat="root"
                kernel_pat="kernel"
                initrd_pat="initrd"
                ;;

        2)      root_pat="set root" 
                kernel_pat="linux"
                initrd_pat="initrd"
                ;;
    esac

	echo $_menu_lst_
        _line_numbers_=`cat "$_menu_lst_" | grep -n -P "^(\s*)$root_pat.*(.*hd.*)" |  awk -F":" '{print $1}'`
        _last_line_num_=`cat -n "$_menu_lst_" | tail -1 | awk -F" " '{print $1}'`

        #insert an element.
        _line_numbers_=( ${_line_numbers_[@]} $_last_line_num_ )

        echo ${_line_numbers_[@]}

        for (( i=0 ; i<${#_line_numbers_[@]} ; i++  ))
        do
                _kernel_version_=""
                _initrd_filename_=""
                if [ $i -eq $((${#_line_numbers_[@]}-1)) ]; then
                        continue;
                fi

                _read_lines_until_=$((${_line_numbers_[$i+1]}))
                for (( j=${_line_numbers_[$i]} ; j<=$_read_lines_until_ ; j++  ))
                do
                        _line_=`sed -n "$j p" "$_menu_lst_"`
                        #here we shall check for kernel and initrd.

                        echo $_line_ | grep -q -P "^(\s*)$kernel_pat"
                        if [ 0 -eq $? ]; then
                                #here we need to find the pattern matching with list of kernels installed.
                                for _kernel_ in ${_KERNEL_LIST_[@]}
                                do
                                        #echo "searching for kernel : $_kernel_ in line : $_line_"
                                        echo $_line_ | egrep -i "\<$_kernel_\>"
                                        if [ 0 -ne $? ]; then
                                                continue
                                        fi
                                        echo "kernel matched = $_kernel_"
                                        _kernel_version_=$_kernel_
                                        break
                                done
                                continue
                        fi

                        echo $_line_ | grep -q -P "^(\s*)$initrd_pat"
			if [ 0 -eq $?  ]; then
				echo "came here."
                                #here we need to Check with kernel version? or proceed with kernel version received in above statements.
				#_initrd_filename_=`echo $_line_ | awk -F "/" '{print $2}'`
				_initrd_filename_=`echo $_line_ | awk '{print $2}'`
				_initrd_filename_=`echo $_initrd_filename_ | sed -e "s:[/].*[/]::"`
				_initrd_filename_=`echo $_initrd_filename_ | sed -e "s:/::"`
                                echo "initrd filename : $_initrd_filename_"
                                if [ $_kernel_version_ ]; then
                                        break
                                fi

                                for _kernel_ in ${_KERNEL_LIST_[@]}
                                do
                                        #echo "searching for kernel : $_kernel_ in line : $_line_"
                                        echo $_line_ | egrep -i "\<$_kernel_\>"
                                        if [ 0 -ne $? ]; then
                                                continue
                                        fi
                                        _kernel_version_=$_kernel_
                                        break
                                done
                                continue;
                        fi
                done

                if [ $_kernel_version_ ] && [ $_initrd_filename_ ]; then
                        _KERNEL_VERSION_LIST_[$i]=$_kernel_version_
                        _INITRD_FILENAMES_[$i]=$_initrd_filename_
                fi
        done

	echo "_KERNEL_VERSION_LIST_ = ${_KERNEL_VERSION_LIST_[@]}"
	echo "_INITRD_FILENAMES_ = ${_INITRD_FILENAMES_[@]}"
}

function CheckForRequiredModulesInCustomInitrd
{
	_initrd_filename_=$1
        err_code=$_SUCCESS_
        if [ ! -f "/boot/$_initrd_filename_" ]; then
		echo "ERROR :: Failed to find initrd image file : $_initrd_filename_"
		return $_FAILED_
        fi

        #check if file already exists?
        if [ -f "/boot/$_initrd_filename_-InMage-Virtual" ]; then
                echo "InMage Init/Initramfs virtual compliant file already exists for image \"$_initrd_filename_\"."
                return $_SUCCESS_
        fi

        lsmod | grep -i mptsas 2>&1 /dev/null
        if [ $? -eq 0 ]; then
            _MOD_MPTSAS_LOADED_=1
        fi
    
        local lsinitrd=""
        case $_OS_NAME_ in
            RHEL7-64 | OL7* | SLES12*)   # TODO: Should be checking for hyper-v drivers on SLES as well
                        check_hyperv_drivers "/boot/$_kernel_file_name_"
                        err_code=$?
                        lsinitrd="lsinitrd /boot/$_kernel_file_name_"
                        ;;

            *)          # TODO: We should move to using standard lsinitrd command 
                        # in future for all OS instead of custom commands
                        lsinitrd="get_initrd_contents /boot/$_kernel_file_name_"
                        ;;
        esac
        
        #we have kernel image file name.Now use "lsinitrd" command to check for modules.
        $lsinitrd | grep mptspi
        if [ 0 -ne $? ];        then
                echo "Initrd/Initramfs Image has to be build for img : $_initrd_filename_ for mptspi."
                err_code=2
        fi

        $lsinitrd | grep e1000
        if [ 0 -ne $? ];        then
                echo "Initrd/Initramfs Image has to be built for img : $_initrd_filename_."
                err_code=2
        fi

        $lsinitrd | grep "sd_mod.ko"
        if [ 0 -ne $? ];        then
                echo "Initrd/Initramfs Image has to be built for img : $_initrd_filename_."
                err_code=2
        fi

	if  [ ! -d "/etc/vxagent/vcon" ]; then
                mkdir -p "/etc/vxagent/vcon"
        fi

        if [ -f /boot/$_initrd_filename_-InMage-Virtual ] && [ -f "$_CHECKSUM_FILE_FOR_INITRD_" ]; then
                md5sum_current=`md5sum /boot/$_initrd_filename_ | awk '{ print $1 }'`
                md5sum_cache=`cat $_CHECKSUM_FILE_FOR_INITRD_ | grep "/boot/$_initrd_filename_" | awk '{ print $1 }'`
                if [ "${md5sum_current}" != "${md5sum_cache}" ]; then
		        echo "md5sum($_initrd_filename_): cur = $md5sum_current old = $md5sum_cache"
                # Fix for bug where multiple checksum entries were 
                # created for single initrd image. We delete all entries
                # that refer to the single initrd file name
                sed -i /$_initrd_filename_/d $_CHECKSUM_FILE_FOR_INITRD_
                # In case there are some characters in the initrd fname
                # which are not handled by sed, remove all the entries 
                # with matching checksum
                sed -i /$md5sum_cache/d $_CHECKSUM_FILE_FOR_INITRD_
                        md5sum /boot/$_initrd_filename_ >> $_CHECKSUM_FILE_FOR_INITRD_
                        err_code=2
            else
                # InMage-Virtual image already generated for this initrd
                err_code=0
            fi
        else
            # Only add md5sum if InMage-Virtual needs to be generated
            if [ $err_code -eq 2 ]; then
                md5sum /boot/$_initrd_filename_ >> $_CHECKSUM_FILE_FOR_INITRD_
            fi
        fi


        return $err_code
}

function ConstructCustomInitrdImg_RHEL4
{
        #specific function for RHEL4
	_initrd_filename_=$1
        _kernel_=$2
        _PRELOAD_MODULES_=""
        _rc_=$_SUCCESS_
        if [ $_MOD_MPTSAS_LOADED_ -eq 1 ]; then
                _PRELOAD_MODULES_="sd_mod mptsas mptspi"
        else
                _PRELOAD_MODULES_="sd_mod mptspi"
        fi
        echo "Making initrd image file for kernel version \"$_kernel_\"."
        mkinitrd  --preload="${_PRELOAD_MODULES_}" --with=e1000 --builtin=involflt -f /boot/$_initrd_filename_-InMage-Virtual $_kernel_ 2> /dev/null
        _rc_=$?

        _initrd_file_name_="/boot/$_initrd_filename_-InMage-Virtual"
        /etc/vxagent/bin/inm_dmit --fsync_file "$_initrd_file_name_"
        if [ 0 -ne $? ]; then
                echo "FileSync operation failed for file ${_initrd_file_name_}."
        fi

        return $_rc_
}

function ConstructCustomInitrdImg_RHEL
{
        #common function for RHEL 5 and 6
	    _initrd_filename_=$1
        _kernel_=$2
        _PRELOAD_MODULES_=""
	
        _hv_drivers=""
    	if [ "azure" = "$_azure_workflow" ]; then
	    	_hv_drivers="hv_vmbus hv_storvsc"
    	fi

        _rc_=$_SUCCESS_
        if [ $_MOD_MPTSAS_LOADED_ -eq 1 ]; then
                _PRELOAD_MODULES_="sd_mod mptsas mptspi $_hv_drivers"
        else
                _PRELOAD_MODULES_="sd_mod mptspi $_hv_drivers"
        fi
	
	    FindBuiltInKernelModules "${_kernel_}"
        echo "Making initrd image file for kernel version \"$_kernel_\"."
        _initrd_file_name_=""
        _initrd_file_name_="/boot/$_initrd_filename_-InMage-Virtual"
        mkinitrd  --preload="${_PRELOAD_MODULES_}" --with=e1000 ${_BUILTIN_KMODS_}  -f /boot/$_initrd_filename_-InMage-Virtual $_kernel_ 2> /dev/null
        _rc_=$?

        /etc/vxagent/bin/inm_dmit --fsync_file "$_initrd_file_name_"
        if [ 0 -ne $? ]; then
                echo "FileSync operation failed for file ${_initrd_file_name_}."
        fi

        return $_rc_
}

function ConstructCustomInitrdImg_OL
{
        #common function for OL 5 and 6
	_initrd_filename_=$1
        _kernel_=$2
        _PRELOAD_MODULES_=""
        _rc_=$_SUCCESS_
        if [ $_MOD_MPTSAS_LOADED_ -eq 1 ]; then
                _PRELOAD_MODULES_="sd_mod mptsas mptspi"
        else
                _PRELOAD_MODULES_="sd_mod mptspi"
        fi
	
	FindBuiltInKernelModules "${_kernel_}"
        echo "Making initrd image file for kernel version \"$_kernel_\"."
        _initrd_file_name_=""
        _initrd_file_name_="/boot/$_initrd_filename_-InMage-Virtual"
        mkinitrd  --preload="${_PRELOAD_MODULES_}" --with=e1000 ${_BUILTIN_KMODS_} -f /boot/$_initrd_filename_-InMage-Virtual $_kernel_ 2> /dev/null
        _rc_=$?

        /etc/vxagent/bin/inm_dmit --fsync_file "$_initrd_file_name_"
        if [ 0 -ne $? ]; then
                echo "FileSync operation failed for file ${_initrd_file_name_}."
        fi
        return $_rc_
}

function ConstructCustomInitrdImg_SLES
{
	_initrd_filename_=$1
        _kernel_=$2
        _reiserfs_module=""
        _root_device_fs_type_=`mount | grep " on / " | awk '{print $5}'`
        if [ "reiserfs" = "{$_root_device_fs_type_}" ]; then
                _reiserfs_module="reiserfs"
        fi

        _PRELOAD_MODULES_=""
        if [ $_MOD_MPTSAS_LOADED_ -eq 1 ]; then
                _PRELOAD_MODULES_="sd_mod mptsas mptspi ${_reiserfs_module} e1000"
        else
                _PRELOAD_MODULES_="sd_mod mptspi ${_reiserfs_module} e1000"
        fi

        if [ -f /boot/vmlinux-$_kernel_ ]; then
                _kernel_file_name_="/boot/vmlinux-$_kernel_"
        elif [ -f /boot/vmlinuz-$_kernel_ ]; then
                _kernel_file_name_="/boot/vmlinuz-$_kernel_"
        fi

        _initrd_file_name_="/boot/$_initrd_filename_"
        temp_modules=`cat /etc/sysconfig/kernel | grep "^INITRD_MODULES" | awk -F'=' '{ print $2 }'`

        str="s/\"$/ $_PRELOAD_MODULES_\"/"
        echo "STRING: $str"
        _module_list_new=`echo $temp_modules | sed "s/\"$/ $_PRELOAD_MODULES_\"/"`

        _initrd_symlink_="/boot/initrd"
        _initrd_symlink_tmp_="/boot/initrd_tmp"
        `/bin/cp -l ${_initrd_symlink_} ${_initrd_symlink_tmp_}`

        mkinitrd -f block -f lvm -m "$_module_list_new" -k "$_kernel_file_name_" -i "$_initrd_file_name_-InMage-Virtual" 2> /dev/null
        _returnCode_=$?

        #Here we need to perform file Sync up
        /etc/vxagent/bin/inm_dmit --fsync_file "$_initrd_file_name_"
        if [ 0 -ne $? ]; then
                echo "FileSync operation failed for file ${_initrd_file_name_}."
        fi

        `/bin/mv ${_initrd_symlink_tmp_} ${_initrd_symlink_}`

        return $_returnCode_
}


function ConstructCustomInitrdImage
{
	_ret_code_=""
        case $_OS_NAME_ in
                RHEL3*)
                        echo "It is RHEL 3. It is not supported right now"
                        ;;
                RHEL4*)
                        echo "It is RHEL 4."
                        ConstructCustomInitrdImg_RHEL4 "$1" "$2"
                        _ret_code_=$?
                        ;;
                RHEL*)
                        echo "It is either RHEL 5 or 6"
                        ConstructCustomInitrdImg_RHEL "$1" "$2"
                        _ret_code_=$?
                        ;;
                SLES*)
                        echo "SLES Machine."
                        ConstructCustomInitrdImg_SLES "$1" "$2"
                        _ret_code_=$? 
                        ;;
                OL*)
                        echo "It is either OL 5 or 6"
                        ConstructCustomInitrdImg_OL "$1" "$2"
                        _ret_code_=$?
                        ;;

                *)
                        echo "Current This OS is not supported?Need to Add for Centos?"
                        return 1
                        ;;
        esac

        #check returncode..
        if [ 0 -ne $_ret_code_ ]; then
                echo "ERROR :: Failed to construct initrd image : \"$1\" file for kernel \"$2\"."
                return 1
        fi
        echo "Successfully built new initrd img : \"$1\" file for kernel \"$2\"."
}

function CheckCustomInitrdImage
{
	for (( i=0 ; i<${#_INITRD_FILENAMES_[@]} ; i++  ))
        do
                CheckForRequiredModulesInCustomInitrd "${_INITRD_FILENAMES_[$i]}"
                case $? in
                        0)
                                echo "No Need to Build initrd/initramfs module for img :${_INITRD_FILENAMES_[$i]}."
                                ;;
                        1)
                                echo "ERROR :: Failed to check whether initrd/initramfs has to be built or not for img : ${_INITRD_FILENAMES_[$i]}."
                                ;;
                        2)
                                echo "initrd/initramfs has to be rebuilt for img:${_INITRD_FILENAMES_[$i]}."
                                ConstructCustomInitrdImage "${_INITRD_FILENAMES_[$i]}" "${_KERNEL_VERSION_LIST_[$i]}"
                                ;;
                        *)
                                echo "ERROR :: Un-expected return code."
                                echo "ERROR :: Function Name: CheckForRequiredModulesInCustomInitrd,return code=$?."
                                ;;
                esac
        done

}

#MAIN#

echo "Construct Kernel Image files."
if [ ! "$1" ]; then 
	echo "OS information is not passed finding it internally"
	GetOS
	if [ 0 -ne $? ]; then
		exit $?
	fi
else
	_OS_NAME_=$1
	_azure_workflow=$2
	echo "OS Name received=$_OS_NAME_"
fi

#Get List of Kernels Installed

GetListOfKernelsInstalled
if [ 0 -ne $? ]; then
	exit $?
fi

CheckInitrdImage
if [ 0 -ne $? ]; then
	exit $?
fi

#here first we need to read the menu.lst file/grub.conf file.
#Make the List of initrd Image files names and their kernel versions if possible.
#then go over each file name and check it's equivalent counter part already exists..
#if does not exists , we have the Name of file and its version create image.
#if exits continue.

GetInitrdNamesFromMenuListFile
if [ 0 -ne $? ]; then
	exit $?
fi

CheckCustomInitrdImage
if [ 0 -ne $? ]; then
	exit $?
fi

