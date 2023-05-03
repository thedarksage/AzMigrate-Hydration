#!/bin/sh

dirName=`dirname $0`
orgWd=`pwd`

cd $dirName
SCRIPT_PATH=`pwd`
cd $orgWd

LOG_FILE=${SCRIPT_PATH}/support_logs.`date +'%Y-%m-%d-%H-%M-%S'`.txt
INSTALLATION_DIR=`grep INSTALLATION_DIR /usr/local/.sms_version| cut -d'=' -f2`
SMARTCTL=$INSTALLATION_DIR/bin/smartctl

user=`id -un`

if [ "$user" != "root" ]; then
    echo "Please run $0 as user 'root'."
    exit 1
fi

{
echo "OS Details: "
echo "----------------------------------------"
echo
uname -a
echo
cat /etc/*release*
echo
echo "Processor Details: "
echo "----------------------------------------"
echo
# /usr/platform/`uname -i`/sbin/prtdiag
/usr/sbin/prtdiag
echo
echo "Hardware Configuration: "
echo "----------------------------------------"
echo
prtconf
echo
echo "Memory Details: "
echo "----------------------------------------"
echo
swap -s
swap -l
echo
echo "Storage Disc Details: "
echo "----------------------------------------"
echo
df -k
echo
echo "FS Type and Mount points present - : "
echo "----------------------------------------"
echo
cat /etc/vfstab
echo
cat /etc/mnttab
echo
echo "NIC cards present: "
echo "----------------------------------------"
echo
ifconfig -a
echo
echo "Volume Manager Details: "
echo "----------------------------------------"
echo
pkginfo | grep -i "volume manager"
echo
/usr/sbin/vxvset list
echo
metastat -a
echo
metastat -p
echo
zpool status
echo
zpool list
echo
echo "Current Running Processes: "
echo "----------------------------------------"
ps -ef
echo "Packages Installed: "
echo "-----------------------------------------"
pkginfo
echo "crle 32-bit: "
echo "-----------------------------------------"
crle
echo "crle 64-bit: "
echo "-----------------------------------------"
crle -64
echo "Cluster Details: "
echo "-----------------------------------------"
ls -l /global
ls -l /dev/did/
echo "Zone details: "
echo "-----------------------------------------"
zoneadm list -cv

for zone in `zoneadm list | grep -v global`
do
echo
zonecfg -z $zone export
echo
zlogin $zone cat /etc/vfstab
echo
zlogin $zone cat /etc/mnttab
echo
zlogin $zone df -k
done
} > $LOG_FILE 2>&1

{
echo "ZFS Details: "
echo "----------------------------------------"
echo
zpool history
echo
echo "----------------------------------------"
echo
zfs list
echo
echo "ZFS Properties: "
echo "----------------------------------------"
echo
zfs get all
echo
echo "IPMI logs: "
echo "----------------------------------------"
echo "ipmitool sel elist"
ipmitool sel elist
echo
echo "----------------------------------------"
echo "ipmitool sensor"
ipmitool sensor
echo
echo "----------------------------------------"
echo "ipmitool sdr"
ipmitool sdr
echo

echo "----------------------------------------"
echo "fmadm faulty"
fmadm faulty
echo 

count=0
while [ $count -lt 6 ]
do
    zpool iostat -v
    iostat -znx
    echo ::memstat | mdb -k
    echo ::arc | mdb -k
    sleep 5
    count=`expr $count + 1`
done

echo "iSCSI Luns: "
echo "----------------------------------------"
echo
sbdadm list-lu
echo
stmfadm list-lu -v
echo

echo "Network Properties: "
echo "----------------------------------------"
echo
dladm
echo
ipadm
echo
echo "/etc/system: "
echo "----------------------------------------"
cat /etc/system
echo
echo "/kernel/drv/sd.conf: "
echo "----------------------------------------"
cat /kernel/drv/sd.conf
echo
echo "inm_disk.dat: "
echo "----------------------------------------"
cat ${SCRIPT_PATH}/inm_disk.dat
echo
echo "list_sd_config.sh"
echo "----------------------------------------"
${SCRIPT_PATH}/list_sd_config.sh
cat ${SCRIPT_PATH}/sd_params.log
echo

echo "----------------------------------------"
for disk in `iostat -en |  awk '{print $NF}' | egrep -v "\-\-\-|device"`; do $SMARTCTL -d scsi -a /dev/rdsk/${disk}; done
echo

} >> $LOG_FILE 2>&1

gzip $LOG_FILE

echo "The support logs are in ${LOG_FILE}.gz."

