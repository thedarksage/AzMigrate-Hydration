#!/bin/bash

INSTALL_DIR="INSTALLATION_DIRECTORY"

if [ -f /etc/SuSE-release ] &&  grep -q 'VERSION = 11' /etc/SuSE-release ; then
    echo "Load driver in initrd"
else
	exit 2
fi

if [ ! -e /lib/mkinitrd/boot -o  ! -e /lib/mkinitrd/scripts -o ! -e /lib/mkinitrd/bin/ ]; then
	echo "Some directories are missing under /lib/mkinitrd/"
	exit 3
fi

install()
{
		# Copy the script and inm_dmit tool to /lib/mkinitrd path
		cp ${INSTALL_DIR}/bin/boot-involflt.sh /lib/mkinitrd/scripts/
		cp ${INSTALL_DIR}/bin/inm_dmit /lib/mkinitrd/bin/

		# Output of the volume info collector
		${INSTALL_DIR}/bin/AzureRcmCli --testvolumeinfocollector > /tmp/tvic_$$

		# Parse the above output and find the full disk with attribute "Boot Disk : 1"
		while read line
		do
			param=`echo $line | awk -F":" '{print $1}' | tr -d ' '`
			if [ "$param" = "Name" ]; then
				dev_name=`echo $line | awk -F":" '{print $2}' | tr -d ' '`
				continue
			fi

			if [ "$param" = "Type" ]; then
				type=`echo $line | awk -F":" '{print $2}' | tr -d ' '`
				if [ "$type" != "Disk" ]; then
					continue
				fi
			fi

			if [ "$param" = "BootDisk" ]; then
				value=`echo $line | awk -F":" '{print $2}' | tr -d ' '`
				if [ $value -eq 1 ]; then
					break
				fi
			fi
		done < /tmp/tvic_$$

		rm -f /tmp/tvic_$$

        pname="`/etc/vxagent/bin/get_protected_dev_details.sh -p ${dev_name} | awk '{print $NF}'`"
		# Put the stacking entry for the full disk in the initrd script
		echo "inm_dmit --op=stack --src_vol=${dev_name} --pname=${pname}" >> /lib/mkinitrd/scripts/boot-involflt.sh

		# The directory to store the original initrd images
		mkdir -p /etc/vxagent/sles_orig_initrd_images

		for image in /boot/initrd-*
		do
			echo $image | grep -q "InMage\.orig" && continue

			if [ -e /etc/vxagent/sles_orig_initrd_images/`basename $image` ]; then
				continue
			fi

			cp $image /etc/vxagent/sles_orig_initrd_images/
		done

		# Keeping the original active initrd image in /boot itself with different name
		cp /boot/initrd-`uname -r` /boot/initrd-`uname -r`-InMage.orig

		# For all kernels, generate the initrd images
		cd /lib/modules
		ret=0
		for kernel_version in *
		do
			retries=3
			while true
			do
				if [ ! -e /lib/mkinitrd/boot/60-involflt.sh ]; then
					ln -s /lib/mkinitrd/scripts/boot-involflt.sh /lib/mkinitrd/boot/60-involflt.sh
				fi

				mkinitrd -i initrd-${kernel_version} -k vmlinuz-${kernel_version}
				if [ $? -eq 0 ]; then
					if [ -e /lib/mkinitrd/boot/60-involflt.sh ]; then
						echo "Initrd image for kernel ${kernel_version} created successfully"
						break
					fi

					echo "Retrying the initrd image creation for kernel ${kernel_version}"
					continue
				fi

				retries=`expr $retries - 1`
				if [ $retries -eq 0 ]; then
					echo "Initrd image creation failed for kernel ${kernel_version}"
					ret=4
					break
				fi

				sleep 1
				echo "Retry of creation of initrd image for kernel ${kernel_version}"
			done
		done

		# Moving original kdump initrd images to /boot
		mv -f /etc/vxagent/sles_orig_initrd_images/initrd-*-kdump /boot

		return $ret
}

uninstall()
{
		rm -f /lib/mkinitrd/bin/inm_dmit
		rm -f /lib/mkinitrd/scripts/boot-involflt.sh
		rm -f /lib/mkinitrd/boot/60-involflt.sh
		rm -f /boot/initrd-*-InMage.orig
		cd /lib/modules
		for kernel_version in *
		do
			mkinitrd -i initrd-${kernel_version} -k vmlinuz-${kernel_version}
		done
}

case $1 in
	install)
		install
		ret=$?
		;;
	uninstall)
		uninstall
		ret=0
		;;
	*)
		echo "Usage: $0 <install/uninstall>"
		ret=5
		;;
esac

exit $ret
