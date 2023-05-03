#!/bin/sh

for i in `ls /dev/ | grep sd`
do 
	cat /sys/block/$i/device/vendor 2>/dev/null | grep "InMage  " > /dev/null;
	 if [ $? -eq 0 ] 
	then 
		/etc/vxagent/bin/inm_dmit --op=block_at_lun --lun_name=/dev/$i;
	fi
done

