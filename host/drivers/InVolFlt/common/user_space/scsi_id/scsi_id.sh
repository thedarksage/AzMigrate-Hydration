#!/bin/sh

get_scsid_cmd=/etc/vxagent/bin/inm_scsi_id
os=`uname -s`
script_path=/etc/init.d/
partition=""
#case "${os}" in
#	SunOS)
#		get_scsid_cmd="${script_path}prg"
#		;;
#	Linux)
#		get_scsid_cmd="${script_path}scsi_id --whitelisted --replace-whitespace"
#		;;
#	*)
#		echo "OS is not supported"
#		exit
#		;;
#esac

# returns OK if $1 contains $2
strstr() {
  [ "${1#*$2*}" = "$1" ] && return 1
  return 0
}

# a function to find the device name based on scsi id
get_scsid()
{
	sdev=$1
	scsi_id=""
	#echo "get_scsid: sdev:${sdev}"
	case "${os}" in
		SunOS)
			sdev=`echo $sdev | sed -e "s:dmp:rdmp:"`
			sdev=`echo $sdev | sed -e "s:dsk:rdsk:"`
			#scsi_id=`${get_scsid_cmd} $sdev 2> /dev/null | grep "page 83" | awk -F= '{ print $2 }' | sed -e "s: ::"`
			scsi_id=`${get_scsid_cmd} $sdev 2> /dev/null | sed -e "s: ::"`
			#if [ ! "${scsi_id}" ]
			#then
			#	#scsi_id=`$get_scsid_cmd $sdev 2> /dev/null | grep "page 80" | awk -F= '{ print $2 }' | sed -e "s: ::"`
			#fi
			;;
		Linux)
			scsi_id=`$get_scsid_cmd $sdev 2> /dev/null`
			;;
		*)
			#echo "OS is not supported"
			exit
			;;
	esac
	scsi_id=`echo ${scsi_id} | sed -e 's: ::g'`
	if [ "${scsi_id}" ]
	then 
	    echo $scsi_id
	else
	    tmp=`echo $sdev | grep mapper`
	    if [ "${tmp}" ]
	    then
	    	scsi_id=`echo $sdev | sed -e "s:[/].*[/]::"`
	    	echo $scsi_id
	    fi
		
	fi
	
}
src_dev=$1
if [ ! "${src_dev}" ]
then
	echo "Error device name missing"
	exit
fi
cur_tgt_scsi_id=`get_scsid $src_dev`
echo $cur_tgt_scsi_id
