#/bin/sh


#echo "Enabling DefaultFixed network"
#echo "/usr/sbin/netadm enable -p ncp DefaultFixed"
#/usr/sbin/netadm enable -p ncp DefaultFixed

grep -v "^#" /kernel/drv/ixgbe.conf  | grep default_mtu > /dev/null
if [ $? -ne 0 ]; then
    echo "default_mtu=9000;" >> /kernel/drv/ixgbe.conf
fi

octet=117
for device in `/usr/sbin/dladm show-phys | grep ixgbe | awk '{print $1}'| sort`
do

    echo "dladm set-linkprop -p mtu=9000 $device"
    dladm set-linkprop -p mtu=9000 $device

    /usr/sbin/ipadm  | grep $device/san > /dev/null
    if [ $? -ne 0 ]; then
        echo "creating the  ip for $device"
        echo "/usr/sbin/ipadm create-ip $device"
        /usr/sbin/ipadm create-ip $device

        echo "Using 10.${octet}.0.11/16 $device/san "
        echo "/usr/sbin/ipadm create-addr  -T static -a local=10.${octet}.0.11/16 ${device}/san"
        /usr/sbin/ipadm create-addr  -T static -a local=10.${octet}.0.11/16 ${device}/san
    fi
    octet=`expr $octet + 1`
done

echo "Done."
