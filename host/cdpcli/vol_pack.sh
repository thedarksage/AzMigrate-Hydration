#!/bin/sh

##
# defaults
##
VV_MAX_LOOP=256
VV_ONE_M=`expr 1024 \* 1024`
VV_VX_VER_FILE="/usr/local/.vx_version"
VV_VX_INSTALL_DIR=`cat $VV_VX_VER_FILE | grep "INSTALLATION_DIR" | cut -d"=" -f2`
VV_CONF_FILE="$VV_VX_INSTALL_DIR/etc/volpack.conf"
VV_TEMP_CONF_FILE="$VV_VX_INSTALL_DIR/volpack.conf.temp"

##
# Error codes table
##
VV_E_SUCCESS=0
VV_E_MODULE=32
VV_E_NOMODULE=33
VV_E_FILE_ARG_MISSING=34
VV_E_OP_MISSING=35
VV_E_FILE_SIZE_ERR=36
VV_E_FILE_EXISTS=37
VV_E_NO_MORE_DEV=38
VV_E_FILE_NO_EXISTS=37
VV_E_NO_FILES=39
VV_E_EXISTS=40
VV_E_FILE_USED=41
VV_E_DEV_ARG_MISSING=42
VV_E_DEV_NO_EXISTS=43

##
# usage function
##
function usage ()
{ 
	echo "Usage: vol_pack [OPTIONS]..." 
	echo "   Create a Sparse file of specified size and attach the same with a" 
	echo "   available loop file" 
	echo 
	echo "      --op=[mount/unmount/list] the operation to be performed" 
	echo "      --file=FILEPATH           the path for where the sparse file should be created" 
	echo "      --size=MEGABYTES          the size should be in units of MB" 
	echo "      --device=LOOP DEV PATH    the path for the device to be deleted"
	echo "      --help                    this message" 
	echo 
	echo "  \$ vol_pack --op=mount --file=/temp --size=10" 
	echo "         to create a 10MB sparse file(/temp) and associate with a loop device" 
	echo "  \$ vol_pack --op=unmount --device=/dev/loop1"
	echo "         to deassociate the sparse file(/temp) from the loop device"
	echo "  \$ vol_pack --op=list"
	echo "         to list the volpack volumes exported till now"
	exit 1
}

##
# parse_cmd_line function
##
function parse_cmd_line ()
{
	if [ "$1" == "" ]; then
		usage
	else
		while [ "$1" != "" ]; do
			COMPARE_VAR=`echo "$1" | cut -d"=" -f1`
			case $COMPARE_VAR in
				--op)
					VV_OP=`echo "$1" | cut -d"=" -f2`
					;;
				--file)
					VV_FILE=`echo "$1" | cut -d"=" -f2`
					;;
				--size)
					VV_SIZE=`echo "$1" | cut -d"=" -f2`
					;;

				--device)
					VV_DEVICE=`echo "$1" | cut -d"=" -f2`
					;;

				--help)
					usage
					;;
				*)
					usage
					;;
			esac
			shift
		done
	fi
}

##
# is_module function
##
function is_module ()
{
	if [ -b /dev/loop0 ]; then
		lsmod | grep loop > /dev/null
		if [ $? -eq 1 ]; then
			return $VV_E_NOMODULE
		fi
	fi

	# if here then module
	return $VV_E_MODULE 
}

##
# print the error string according to the error code
##
function print_err()
{
	status=$1
	case $status in
		$VV_E_SUCCESS) ;;

		$VV_E_FILE_ARG_MISSING)
			echo "file path for sparse file missing"
			exit $VV_E_FILE_ARG_MISSING
			;;

		$VV_E_SIZE_ARG_MISSING)
			echo "file size for sparse file missing"
			exit $VV_E_SIZE_ARG_MISSING
			;;

		$VV_E_OP_MISSING)
			echo "op missing"
			exit $VV_E_OP_MISSING
			;;

		$VV_E_FILE_SIZE_ERR)
			echo "wrong file size"
			exit $VV_E_FILE_SIZE_ERR
			;;

		$VV_E_FILE_EXISTS)
			echo "file already exists"
			exit $VV_E_FILE_EXISTS
			;;

		$VV_E_NO_MORE_DEV)
			echo "All devices under use. Release some devices"
			echo "Cleaned up the sparse file created."
			exit $VV_E_NO_MORE_DEV
			;;

		$VV_E_FILE_NO_EXISTS)
			echo "file is not a sparse file"
			exit $VV_E_FILE_NO_EXISTS
			;;

		$VV_E_NO_FILES)
			echo "no vol packs exists"
			exit 0
			;;

		$VV_E_EXISTS)
			echo "success; vol_pack already exists"
			exit 0
			;;

		$VV_E_DEV_USED)
			echo "failure; device cannot be re-used"
			exit $VV_E_DEV_USED
			;;

		$VV_E_DEV_ARG_MISSING)
			echo "device name input missing"
			exit $VV_E_DEV_ARG_MISSING
			;;

		$VV_E_DEV_NO_EXISTS)
			echo "file is not a sparse file"
			exit $VV_E_DEV_NO_EXISTS
			;;

	esac
}

##
# mod_usage_count function
##
function mod_usage_count ()
{
	return `awk '$1 == "loop" {print $3}' < /proc/modules`
}

##
# is same function
##
function is_same ()
{
	VV_USED_DEV=$2
	VV_USED_FILE=$1
	`losetup $VV_USED_DEV | awk '{print $3}' | grep "$VV_USED_FILE" > /dev/null`
	return $?
}

##
# reusable function
##
function reusable ()
{
	while read VV_SPARSE_FILE VV_LOOP_DEV VV_SIZE
	do
		if [ "$VV_SPARSE_FILE" == "$1" ] && [ "$VV_SIZE" -eq "$2" ]; then
			echo "Log suggests $VV_LOOP_DEV is associated with $1"
			is_same $VV_SPARSE_FILE $VV_LOOP_DEV
			if [ $? -eq 0 ]; then 
				print_err $VV_E_EXISTS
			else
				print_err $VV_E_DEV_USED
			fi
		fi
	done < $VV_CONF_FILE

	return $VV_E_FILE_EXISTS
}

##
# create sparse file function
##
function create_sparse_file()
{
	# bail out if file exists. 
	# this will take care of re-use of the same file
	if [ -f $1 ]; then
		reusable $1 $2
		print_err $?
	fi

	echo "Creating the file $1 of size $2..."
	dd if=/dev/zero of="$1" count=0 bs=1M seek="$2" 2> /dev/null

	return $?
}

##
# update vv conf file function
##
function insert_in_vv_conf_file ()
{
	echo "Updating conf file..."
	if [ ! -f $VV_CONF_FILE ]; then
		touch $VV_CONF_FILE
	fi
	echo "$1 $2 $3" >> $VV_CONF_FILE
}

##
# upgrade mod function 
##
function upgrade_mod
{
	mod_usage_count
	if [ $? -eq 0 ]; then
		[ -b /dev/loop255 ]
		if [ $? -ne 0 ]; then 
			echo "upgrading the unused driver to use $VV_MAX_LOOP devices"
			modprobe -r loop 
			modprobe loop max_loop=$VV_MAX_LOOP
		fi
	fi
}

##
# vv_mount function
##
function vv_mount()
{
	# verify the input args
	if [ -z "$1" ]; then
		print_err $VV_E_FILE_ARG_MISSING
	fi
	if [ "$2" -eq 0 ]; then
		print_err $VV_E_FILE_SIZE_ERR
	fi

	# if loop driver a module & unused then upgrade
	is_module
	if [ $? -eq $VV_E_MODULE ]; then
		echo "loop driver is a loadable module"
		upgrade_mod
	else
		echo "loop driver is a statically linked module"
	fi

	# create the sparse file
	create_sparse_file $1 $2
	print_err $?

	# find a free device and perform losetup
	i=0
	while [ "$i" -lt "$VV_MAX_LOOP" ]
	do 
		losetup /dev/loop$i > /dev/null 2>&1
		VV_DEV_STATUS=$?
		
		# device not found
		if [ $VV_DEV_STATUS -eq "2" ]; then
			rm -f $1
			print_err $VV_E_NO_MORE_DEV
		fi

		# device free
		if [ $VV_DEV_STATUS -eq 1 ]; then
			echo "Associating $1 with /dev/loop$i"
			losetup /dev/loop$i $1 > /dev/null 2>&1
			break 2
		fi

		i=`expr $i + 1`
	done

	if [ "$i" -eq "$VV_MAX_LOOP" ]; then
		rm -f $1
		print_err $VV_E_NO_MORE_DEV
	fi

	insert_in_vv_conf_file $1 /dev/loop$i $2

	return $?
}

##
# update vv conf file function
##
function update_vv_conf_file ()
{
	echo "Updating conf file..."
	#grep "$1" "$VV_CONF_FILE" > /dev/null 2>&1 && sed -e "$1/d" "$VV_CONF_FILE" > $VV_TEMP_CONF_FILE
	grep --invert-match "$1" "$VV_CONF_FILE" > $VV_TEMP_CONF_FILE
	rm -f $VV_CONF_FILE
	mv $VV_TEMP_CONF_FILE $VV_CONF_FILE
}

##
# vv_unmount function
##
function vv_unmount()
{
	if [ -z "$1" ]; then
		print_err $VV_E_DEV_ARG_MISSING
	fi

	while read VV_SPARSE_FILE VV_LOOP_DEV VV_SIZE
	do
		if [ "$VV_LOOP_DEV" == "$1" ]; then
			echo "Found $VV_LOOP_DEV to be associated with $VV_SPARSE_FILE"
			echo "De-associating..."
			losetup -d $VV_LOOP_DEV > /dev/null 2>&1
			echo "Deleting the sparse file $VV_SPARSE_FILE"
			rm -f $VV_SPARSE_FILE
			update_vv_conf_file $VV_SPARSE_FILE
			return $?
		fi
	done < $VV_CONF_FILE

	return "$VV_E_DEV_NO_EXISTS"
}

##
# vv_unmountall function
##
function vv_unmountall()
{
	while read VV_SPARSE_FILE VV_LOOP_DEV VV_SIZE
	do
		echo "Found $VV_LOOP_DEV to be associated with $VV_SPARSE_FILE"
		echo "De-associating..."
		losetup -d $VV_LOOP_DEV >/dev/null 2>&1
		echo "Deleting the sparse file $VV_SPARSE_FILE"
		rm -f $VV_SPARSE_FILE
	done < $VV_CONF_FILE

	# delete the conf file
	echo "Updating conf file..."
	rm -f $VV_CONF_FILE
}

##
# vv print dash function
##
function vv_print_dash ()
{
	for i in {0..69}
	do
		echo -e "-\\c"
	done
	echo
}

##
# vv_list function
##
function vv_list()
{
	if [ -f $VV_CONF_FILE ]; then
		vv_print_dash
		printf "%37s\t" "SPARSE_FILE"
		printf "%20s\t" "VOL PACK DEVICE"
		printf "%5s\n" "SIZE"
		vv_print_dash
		while read VV_SPARSE VV_DEV VV_SIZE
		do
			printf "%40s\t" $VV_SPARSE
			printf "%5s\t" $VV_DEV
			printf "%5s\n" $VV_SIZE
		done < $VV_CONF_FILE
		vv_print_dash
	else
		print_err $VV_E_NO_FILES
	fi
}

##
# STEP 0. Parse the command line
##
parse_cmd_line $@

##
# STEP 1. play according to the command
##
case $VV_OP in
	mount)
		vv_mount $VV_FILE $VV_SIZE
		;;

	unmount)
		vv_unmount $VV_DEVICE
		;;

	unmountall)
		vv_unmountall
		;;

	list)
		vv_list
		;;

	*)
		print_err $VV_E_OP_MISSING
		;;
esac

##
# STEP 2. return error code
##
print_err $?
