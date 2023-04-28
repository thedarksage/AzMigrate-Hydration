#!/bin/sh

INST_LOG=/var/log/cx_uninstall.log

# FUNCTION : Carries out a command and logs output into an install log
DO()
{
 eval "$1 2>> $INST_LOG | tee -a $INST_LOG"
}

# FUNCTION : Echoes a command into an install log
LOG()
{
 echo "$1" >> $INST_LOG
}

# Source function library.
. /etc/rc.d/init.d/functions

base_dir=/home/svsystems

if [ -z $1 ]
then
    echo -e " Do you really want to uninstall HA service? [Y/N] [ default N ] : "
    read n
    n=`echo $n | tr [a-z] [A-Z]`
    if [ "$n" != "Y" ]
    then
        exit 1
    fi
fi

echo "" >> ${INST_LOG}
echo "-------------------------------------------------------------------" >> ${INST_LOG}
echo `date` >> ${INST_LOG}
echo "-------------------------------------------------------------------" >> ${INST_LOG}

DO 'echo "Stopping HA services ..."'
/etc/init.d/heartbeat stop 2>/dev/null

#Stop HA monitor process
if [ -f "/etc/ha.d/haresources" ]; then
    if [ -f "$base_dir/var/tman_monitor_ha.pid" ]; then
        /bin/kill -9 `cat $base_dir/var/tman_monitor_ha.pid  2> /dev/null ` >/dev/null 2>&1
      	ret=$?
      	if [ $ret -eq 0 ]; then
        	action $"Stopping HA monitor thread" /bin/true
      	else
        	action $"Stopping HA monitor thread" /bin/false
      	fi
    fi
fi

LOG "Stopping mysql service"
/etc/init.d/mysqld start 2>/dev/null
LOG "Starting httpd service"
/etc/init.d/httpd start 2>/dev/null

#
# Stopping the nodentwd service if it is running
#
pid_nodent=`ps -ef | grep "/home/svsystems/bin/node_ntw_fail" | grep -v grep | awk '{print $2}'`

if [ ! -z $pid_nodent ]; then
	LOG "Stopping nodentwd"
	/etc/init.d/nodentwd stop 2>/dev/null
fi

if [ -d /home/svsystems/ha ]; then
	rm -rf /home/svsystems/ha
fi

# Removing nodentwd and cxfailoverd
LOG "Removing nodentwd and cxfailoverd"
rm -f /etc/init.d/nodentwd /etc/init.d/cxfailoverd
