#!/bin/bash 

#This script is used for setting the azure ip to DHCP. This script is called during the boot up process of the recovered VM.

os_built_for=""

findostype () {
    os_built_for=`grep OS_BUILT_FOR /usr/local/.vx_version | awk -F= '{ print $2 }'`

    case $os_built_for in
        OL*)
            UNAME=Linux
            OS=OEL
        ;;
        RHEL*)
            UNAME=Linux
            OS=RHEL
        ;;
        SUSE*)
            UNAME=Linux
            OS=SUSE
        ;;
        SLES*) 
            UNAME=Linux
            OS=SUSE
        ;;
        DEBIAN*)
            UNAME=Linux
            OS=DEBIAN
        ;;
        UBUNTU*)
            UNAME=Linux
            OS=UBUNTU
        ;;
        XEN*)
            UNAME=Linux
        OS=XEN
        ;;
    esac
 } 
findmacandnics () {
##Discover the NIC MAC address and their corresponding configuration files ####
    config="/tmp/nicsandnames"
    #ifconfig -a|grep -i HW|awk ' { print $1 "   "  $NF }' > $config
    ip -o link show | awk '{ print $2 }' | sed 's\:\\' > $config
    
    # if mac address also needed then use below command
    # ip -o link show | awk '{ print $2 "   " $(NF-2) }' | sed 's\:\\' > $config
}

setipforubuntu () {
    if [ -f /etc/netplan/50-azure_migrate_dhcp.yaml ]; then
        return
    fi

    _ubuntu_network_interfaces_file="/etc/network/interfaces"
    
    cat $config| while read line
    do
        name=`echo $line|awk '{ print $1 }'`
        
        if [ "$name" = "lo" ] || [ "$name" = "eth0" ]
        then
            echo "Skipping $name as its already configured."
            continue
        fi
        
        # update the additional nic settings to dhcp in interfaces file 
                
        if grep -q "iface[[:space:]]$name[[:space:]].*" $_ubuntu_network_interfaces_file ; then
            echo "Seems $name is already added to $_ubuntu_network_interfaces_file. Skipping it..."
        else
            echo "Configuring $name ..."
            echo "" >> $_ubuntu_network_interfaces_file
            echo "auto $name" >> $_ubuntu_network_interfaces_file
            echo "iface $name inet dhcp" >> $_ubuntu_network_interfaces_file
            
            #TODO: conform IPV6 is required or not before enabling below line
            #echo "iface eth0 inet6 auto" >> $_ubuntu_network_interfaces_file
            
            #restart the interface
            echo "Restarting $name ..."
            ifdown "$name"
            ifup "$name"
        fi
    done
}

setipforrhel () {
#Setting the ipaddress of the all the nics to dhcp for rhel and cent os platforms

##Taking the backup of all the network configuration files.
NETWORKCONFIGFILESDIR="/etc/sysconfig/network-scripts"
BACKUP_NETWORKCONFIGFILES="$NETWORKCONFIGFILESDIR/inmage_nw"

if [ ! -d $BACKUP_NETWORKCONFIGFILES ]
then
    mkdir -p $BACKUP_NETWORKCONFIGFILES
fi

###Moving all the network config files#####
mv $NETWORKCONFIGFILESDIR/ifcfg-eth* $BACKUP_NETWORKCONFIGFILES/

###Creating network config files for all the ethernet cards###

    host_name=`hostname`
cat $config| while read line
do

    name=`echo $line|awk '{ print $1 }'`
    #mac=`echo $line|awk '{ print $2 }'`
    
    if [ "$name" = "lo" ]; then
        echo "Skipping the loopback nic entry"
        continue
    fi
    
    ConfigFileName=$NETWORKCONFIGFILESDIR/ifcfg-$name
    
    echo "DHCP_HOSTNAME=$host_name" >$ConfigFileName
    echo "DEVICE=$name" >> $ConfigFileName
    echo "ONBOOT=yes" >> $ConfigFileName
    echo "DHCP=yes" >> $ConfigFileName
    echo "BOOTPROTO=dhcp" >> $ConfigFileName
    echo "TYPE=Ethernet" >>  $ConfigFileName
    echo "USERCTL=no" >>  $ConfigFileName
    echo "PEERDNS=yes" >>  $ConfigFileName
    echo "IPV6INIT=no" >> $ConfigFileName
    #echo "HWADDR=$mac" >>$ConfigFileName
    if [ -f $BACKUP_NETWORKCONFIGFILES/ifcfg-$name ]
    then
        chown --reference=$BACKUP_NETWORKCONFIGFILES/ifcfg-$name $NETWORKCONFIGFILESDIR/ifcfg-$name
        chmod --reference=$BACKUP_NETWORKCONFIGFILES/ifcfg-$name $NETWORKCONFIGFILESDIR/ifcfg-$name
    fi
    
    #restart the interface for RHEL7/CentOS7
    case $os_built_for in
        RHEL7* | OL7*)
            echo "Restarting $name ..."
            ifdown "$name"
            ifup "$name"
        ;;
        *)
            echo "Skipping $name interface restart"
        ;;
    esac
    
done

### Restart NetworkManager for RHEL7* ###
case $os_built_for in
    RHEL7* | OL7* | RHEL8*)
        echo "Restarting NetworkManager"
        systemctl restart NetworkManager
        echo "Exit code: $?"
    ;;
    *)
        echo "Skipping NetworkManager restart"
    ;;
esac

###Restarting the network services#####
service network restart 

}

setipforsuse () {
#Setting the ipaddress of the all the nics to dhcp for SuSE os platforms

##Taking the backup of all the network configuration files.
NETWORKCONFIGFILESDIR="/etc/sysconfig/network"
BACKUP_NETWORKCONFIGFILES="$NETWORKCONFIGFILESDIR/inmage_nw"

if [ ! -d $BACKUP_NETWORKCONFIGFILES ]
then
    mkdir -p $BACKUP_NETWORKCONFIGFILES
fi

###Moving all the network config files#####
mv -f $NETWORKCONFIGFILESDIR/ifcfg-eth* $BACKUP_NETWORKCONFIGFILES/

###Creating network config files for all the ethernet cards###

    host_name=`hostname`
cat $config| while read line
do

    name=`echo $line|awk '{ print $1 }'`
    #mac=`echo $line|awk '{ print $2 }'`
    
    if [ "$name" = "lo" ]; then
        echo "Skipping the loopback nic entry"
        continue
    fi
    
    ConfigFileName=$NETWORKCONFIGFILESDIR/ifcfg-$name
    
    echo "BOOTPROTO='dhcp'"  >$ConfigFileName
    echo "MTU='' " >> $ConfigFileName
    echo "REMOTE_IPADDR='' " >>$ConfigFileName
    echo "STARTMODE='onboot'" >>$ConfigFileName

    if [ -f $BACKUP_NETWORKCONFIGFILES/ifcfg-$name ]
    then
        chown --reference=$BACKUP_NETWORKCONFIGFILES/ifcfg-$name $NETWORKCONFIGFILESDIR/ifcfg-$name
        chmod --reference=$BACKUP_NETWORKCONFIGFILES/ifcfg-$name $NETWORKCONFIGFILESDIR/ifcfg-$name
    fi

done


###Restarting the network services#####
service network restart

}


##Find OS type so that appropriate ethernet configuration files can be created####
findostype
findmacandnics

case $OS in
    RHEL) setipforrhel
          ;;
    SUSE) setipforsuse
          ;;
    OEL) setipforrhel
          ;;
    UBUNTU) setipforubuntu
          ;;
    DEBIAN) setipforubuntu
          ;;
      *)
        echo "There is support only for suse, ubuntu, oel and redhat"
        echo "please configure network changes manually"

esac
