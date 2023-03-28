#!/bin/sh

#date +"%b %d %H:%M:%S" >> /etc/vxagent/logs/at_lun_masking.log
#echo "Executing $2 masking command for /dev/$1 " >> /etc/vxagent/logs/at_lun_masking.log
if [ $2 == "add" ]
then
	/etc/vxagent/bin/inm_dmit --op=block_at_lun --lun_name=/dev/$1;
fi
if [ $2 == "del" ]
then
	 /etc/vxagent/bin/inm_dmit --op=unblock_at_lun --lun_name=/dev/$1;
fi
