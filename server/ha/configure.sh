#!/bin/sh

INST_LOG=/var/log/cx_install.log

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

AddOrReplace()
{
    local keyname=${1?:"Key name to be added/changed is required as 1st argument in function $FUNCNAME"}
    local conffile=${2?:"Configuration file name is required as 2nd argument in function $FUNCNAME"}
    local keyvalue=${3?:"Key value to be used for the key name is required as 3rd argument in function $FUNCNAME"}

    [ ! -f $conffile ] && echo "$conffile does not exist! Aborting..." && exit 1

    # Check if keyname is already present in conffile. If so, just do a sed on conffile and change value
    # Otherwise, append a new line to conffile for the keyname

    if grep -q "^[[:space:]]*${keyname}[[:space:]]*=[[:space:]]*.\+$" $conffile ; then
        sed -i -e "s/^[[:space:]]*${keyname}[[:space:]]*=[[:space:]]*.\+$/${keyname} = \"$keyvalue\"/" $conffile
    else
        echo "" >> $conffile
        echo "$keyname = \"$keyvalue\"" >> $conffile
    fi
}

# Source function library.
. /etc/rc.d/init.d/functions

base_dir=/home/svsystems

mkdir -p /etc/ha.d >/dev/null 2>&1
cp -f ha.cf /etc/ha.d/ha.cf >/dev/null 2>&1 
cp -f haresources /etc/ha.d/haresources >/dev/null 2>&1 
cp -f authkeys /etc/ha.d/authkeys >/dev/null 2>&1
chmod 600 /etc/ha.d/authkeys >/dev/null 2>&1

DO 'echo "Configuring Linux HA..."'
primary_node=""
secondary_node=""
multicast_group_ip=""
network_interface=""
ping_ip=""
cluster_ip=""

while [ -z $primary_node ]
do
    echo "Enter hostname of primary node: " | tr -d "\n"
    read primary_node
done

ping -c3 $primary_node >/dev/null 2>&1
result=$?

while [ "$result" -ne "0" ]
do
    echo "Unable to ping to the hostname $primary_node"
    echo "Enter hostname of primary node: " | tr -d "\n"
    read primary_node
    ping -c3 $primary_node >/dev/null 2>&1
    result=$?
done

while [ -z $primary_ip ]
do
    echo "Enter IP address of primary node: " | tr -d "\n"
    read primary_ip
done

while [ -z $primary_fqdn ]
do
    echo "Enter Fully Qualified Domain Name of primary node: " | tr -d "\n"
    read primary_fqdn
done

while [ -z $secondary_node ]
do
    echo "Enter hostname of secondary node: " | tr -d "\n"
    read secondary_node
done

ping -c3 $secondary_node >/dev/null 2>&1
result=$?

while [ "$result" -ne "0" ]
do
    echo "Unable to ping to the hostname $secondary_node"
    echo "Enter hostname of secondary_node: " | tr -d "\n"
    read secondary_node
    ping -c3 $secondary_node >/dev/null 2>&1
    result=$?
done

while [ -z $secondary_ip ]
do
    echo "Enter IP address of secondary node: " | tr -d "\n"
    read secondary_ip
done

while [ -z $secondary_fqdn ]
do
    echo "Enter Fully Qualified Domain Name of secondary node: " | tr -d "\n"
    read secondary_fqdn
done

while ! echo $multicast_group_ip | grep -q "^[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}$"
do
    echo "Enter multicast group IP address: " | tr -d "\n"
    read multicast_group_ip
done

while [ -z "$network_interface" ]
do
    echo "Enter network interface: " | tr -d "\n"
    read network_interface

    if [ ! -z "$network_interface" ] ; then
        ifconfig $network_interface >/dev/null 2>/dev/null
	if [ $? -eq 1 ] || [ "$network_interface" = "lo" ] ; then
	    network_interface=""
	fi
    fi
done

while ! echo $ping_ip | grep -q "^[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}$"
do
    echo "Enter IP address of ping node [ Please choose an IP which is highly available for ping node ]: " | tr -d "\n"
    read ping_ip

    ping -c3 $ping_ip 1>/dev/null 2>&1

    while [ $? -ne 0 ]
    do
        echo "Enter IP address of ping node [ Please choose an IP which is highly available for ping node ]: " | tr -d "\n"
        read ping_ip
        ping -c3 $ping_ip 1>/dev/null 2>&1
    done
done

while ! echo $cluster_ip | grep -q "^[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}$"
do
    echo "Enter cluster IP address: " | tr -d "\n"
    read cluster_ip
done

echo -n "Enter the appliance high availability cluster name: "
read appliance_name

echo $appliance_name > file
input=`tr 'a-z' 'A-Z' < file`

appliance_name=$input

# Add the Primary and secondary IP address to /etc/hosts file
primary_available=`grep $primary_ip /etc/hosts`
secondary_available=`grep $secondary_ip /etc/hosts`

if [ -z "$primary_available" ]; then
    echo "$primary_ip $primary_fqdn $primary_node" >> /etc/hosts
fi

if [ -z "$secondary_available" ]; then
    echo "$secondary_ip $secondary_fqdn $secondary_node" >> /etc/hosts
fi

# Adding the Cluster ip to the dps_inmage.conf file
install_dir=`cat /home/svsystems/etc/amethyst.conf | grep INSTALLATION_DIR | cut -d"=" -f2 | tr -d '""' | tr -d " "`
port=`cat /home/svsystems/etc/amethyst.conf | grep ^CS_PORT | cut -d"=" -f2 | tr -d '""' | tr -d " "`
DOCUMENT_ROOT=${install_dir}/admin/web
ERROR_LOG=cx_error.log
ACCESS_LOG=cx_access.log

echo "" >> /etc/httpd/conf.d/dps_inmage.conf
echo "<VirtualHost ${cluster_ip}:${port} >" >> /etc/httpd/conf.d/dps_inmage.conf
echo "  ServerName localhost" >> /etc/httpd/conf.d/dps_inmage.conf
echo "  ServerAdmin ${USER}@${HOSTNAME}" >> /etc/httpd/conf.d/dps_inmage.conf
echo "  DocumentRoot ${DOCUMENT_ROOT}" >> /etc/httpd/conf.d/dps_inmage.conf
echo "  ErrorLog logs/${ERROR_LOG}" >> /etc/httpd/conf.d/dps_inmage.conf
echo "  CustomLog logs/${ACCESS_LOG} common" >> /etc/httpd/conf.d/dps_inmage.conf
echo "</VirtualHost>" >> /etc/httpd/conf.d/dps_inmage.conf

# If both CS_IP and PS_CS_IP are same, then assign Cluster IP to PS_CS_IP
CS_IP=`grep -w CS_IP /home/svsystems/etc/amethyst.conf | tr -d ' '| cut -d"=" -f2`
PS_CS_IP=`grep -w PS_CS_IP /home/svsystems/etc/amethyst.conf | tr -d ' '| cut -d"=" -f2`
if [ "${CS_IP}" = "${PS_CS_IP}" ]; then
    AddOrReplace "PS_CS_IP" "/home/svsystems/etc/amethyst.conf" "$cluster_ip"
    if [ -e /usr/local/.fx_version ]; then
        FX_INSTALLATION_DIR=`grep ^INSTALLATION_DIR /usr/local/.fx_version | awk -F= '{ print $2 }'` 
        [ -e ${FX_INSTALLATION_DIR}/config.ini ] && sed -i -e "s/^SVServer =.*/SVServer = $cluster_ip/g" ${FX_INSTALLATION_DIR}/config.ini
    fi
    if [ -e /usr/local/.vx_version ]; then
        VX_INSTALLATION_DIR=`grep ^INSTALLATION_DIR /usr/local/.vx_version | awk -F= '{ print $2 }'`
        [ -e ${VX_INSTALLATION_DIR}/etc/drscout.conf ] && sed -i -e "s/^Hostname.*/Hostname=$cluster_ip/g" ${VX_INSTALLATION_DIR}/etc/drscout.conf
    fi
fi

# Adding the CLUSTER IP to the amethyst.conf file 
AddOrReplace "CLUSTER_IP" "/home/svsystems/etc/amethyst.conf" "$cluster_ip"
AddOrReplace "DB_HOST" "/home/svsystems/etc/amethyst.conf" "localhost"
AddOrReplace "CS_IP" "/home/svsystems/etc/amethyst.conf" "$cluster_ip"
AddOrReplace "CLUSTER_NAME" "/home/svsystems/etc/amethyst.conf" "$appliance_name"

# Writing the entries to logfile
LOG "CLUSTER_IP is $cluster_ip"
LOG "DB_Host is localhost"
LOG "CS_IP is $cluster_ip"
LOG "CLUSTER_NAME is $appliance_name"
LOG "appliance_name is $appliance_name"
LOG "primary_ip is $primary_ip"
LOG "primary_fqdn is $primary_fqdn"
LOG "secondary_node is $secondary_node"
LOG "secondary_ip is $secondary_ip"
LOG "secondary_fqdn is $secondary_fqdn"
LOG "multicast_group_ip is $multicast_group_ip"
LOG "network_interface is $network_interface"

LIBDIR="lib"
if uname -a | grep -q i[0-9]86; then
    LIBDIR="lib"
elif uname -a | grep -q x86_64; then
    LIBDIR="lib64"
fi

sed -i -e "s/hostname1/$primary_node/g; s/hostname2/$secondary_node/g; s/multicastgroup/$multicast_group_ip/g; s/interface/$network_interface/g; s/pingip/$ping_ip/g; s/libdir/$LIBDIR/g" /etc/ha.d/ha.cf
sed -i -e "s/hostname/$primary_node/g; s/ipaddress/$cluster_ip/g" /etc/ha.d/haresources

mkdir -p /home/svsystems/bin/{db1,db2}
sed -e "s/ipaddress/$cluster_ip/g" db1_backup.sh > /home/svsystems/bin/db1/db_backup.sh
sed -e "s/ipaddress/$cluster_ip/g" db1_restore.sh > /home/svsystems/bin/db1/db_restore.sh
sed -e "s/ipaddress/$cluster_ip/g" db2_backup.sh > /home/svsystems/bin/db2/db_backup.sh
sed -e "s/ipaddress/$cluster_ip/g" db2_restore.sh > /home/svsystems/bin/db2/db_restore.sh
chmod -R 755 /home/svsystems/bin/{db1,db2} 

cp report_global_resync.php /home/svsystems/admin/web/
chmod 755 /home/svsystems/admin/web/report_global_resync.php

chkconfig --level 35 heartbeat on >/dev/null 2>&1

LOG "Stopping heartbeat"
/etc/init.d/heartbeat stop
LOG "Starting mysql"
/etc/init.d/mysqld start
LOG "Starting httpd"
/etc/init.d/httpd start
LOG "Starting heartbeat"
/etc/init.d/heartbeat start

# Copy the node ntw daemon and start it post heartbeat installation
cp node_ntw_fail /home/svsystems/bin/
cp nodentwd /etc/init.d/
chmod +x /etc/init.d/nodentwd

# Getting the hostname from /etc/ha.d/haresources and comparing with hostname
hostname_hares=`cat /etc/ha.d/haresources | cut -d" " -f1`
if [ "${hostname_hares}" = `uname -n` ]; then
   /etc/init.d/nodentwd start
fi

# Restarting the tmanagerd services.
if [ -f /usr/local/.pillar ]; then
    :
else
    LOG "Starting tmanagerd services"
    /etc/init.d/tmanagerd stop
    /etc/init.d/tmanagerd start
fi

# Sometimes cluster IP not getting populated. So restarting heartbeat service.
LOG "Stopping heartbeat"
/etc/init.d/heartbeat stop  >/dev/null 2>&1 
LOG "Starting heartbeat"
/etc/init.d/heartbeat start >/dev/null 2>&1
LOG "Re-starting httpd"
/etc/init.d/httpd restart >/dev/null 2>&1
