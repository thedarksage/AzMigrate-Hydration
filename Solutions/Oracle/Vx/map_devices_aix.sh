#!/bin/sh

lv_name=""
vg=""
pv_list=""

# Mount point passed by user
mnt_pt=$1

device=`df | grep "$mnt_pt"$ | awk -F" " '{print $1}'`
if [ "$device" == "" ]; then
	echo "Wrong mount point $mnt_pt"
	exit 1
fi

device_name=`basename $device`

# Capture the output of mount command and find the log device for the mount point
mount > /tmp/mount$$
found=0
while read line
do
	first_field=`echo $line | awk -F" " '{print $1}'`
	second_field=`echo $line | awk -F" " '{print $2}'`
	if [ "$first_field" = "$device" ] || [ "$second_field" = "$device" ]; then
		found=1
		break;
	fi
done < /tmp/mount$$

rm -f /tmp/mount$$

last_field=`echo $line | awk -F" " '{print $NF}'`

i=1
while true
do
        param=`echo $last_field | cut -d"," -f${i}`

        echo $param | grep "log=" > /dev/null
        ret=$?
        if [ $ret -eq "0" ]; then
                break
        fi

        i=`expr $i + 1`
done
log_dev=`echo $param | cut -d"=" -f2`

# Capture the list of PVs in a temporary file
lspv > /tmp/lspv$$

# Check whether the device is PV or not
while read line
do
	pv_name=`echo $line | awk -F" " '{print $1}'`
	if [ $pv_name == $device_name ]; then
		echo "::${device}:${mnt_pt}:${log_dev}"
		rm -f /tmp/lspv$$
		exit 0
	fi
done < /tmp/lspv$$

# It is an LV.
lv_dev=$device
lv_name=$device_name
disk=`lslv -l $lv_name | tail -1 | awk -F" " '{print $1}'`

while read line
do
	pv_name=`echo $line | awk -F" " '{print $1}'`
	if [ $pv_name == $disk ]; then
		vg_name=`echo $line | awk -F" " '{print $3}'`
		break
	fi
done < /tmp/lspv$$

vg=$vg_name
pv_list=/dev/${disk}

# Get all other PVs associacted with this VG
while read line
do
	pv_name=`echo $line | awk -F" " '{print $1}'`
	if [ $pv_name == $disk ]; then
		continue
	fi
	vg_name=`echo $line | awk -F" " '{print $3}'`
	if [ $vg_name == $vg ]; then
		pv_list=`echo ${pv_list},/dev/${pv_name}`
	fi
done < /tmp/lspv$$

echo "${lv_dev}:${vg}:${pv_list}:${mnt_pt}:${log_dev}"
rm -f /tmp/lspv$$
exit 0
