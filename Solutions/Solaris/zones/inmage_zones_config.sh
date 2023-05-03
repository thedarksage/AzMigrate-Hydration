#!/bin/sh
###This script is for exporting Vx installation path all the zones and restart zones#####

    INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
    SCRIPTS_DIR=$INMAGE_DIR/scripts
    LOGDIR=$INMAGE_DIR/failover_data
    ALERT=$INMAGE_DIR/bin/alert
	ZONESLIST="$LOGDIR/zoneslist"
    ADD=0
    REMOVE=0
    reboot_required=0

    Usage()
    {
        echo "usage : $0            - to export inmage binaries to zones"
        echo "usage : $0 clean      - to clean up exported inmage binaries to zones"
        exit 1
    }

    Add()
    {
		# zoneadm list -c |grep -v grep|grep -v global >$ZONESLIST
		zoneadm list|grep -v grep|grep -v global >$ZONESLIST
		if [ ! -s $ZONESLIST ]
		then
			echo "There are no running zones...Please make them up"
			exit 1
		fi

		cat $ZONESLIST|while read x
		do

			zonecfg -z $x export -f /tmp/zonecfg
            zonepath=`zonecfg -z $x export|grep "zonepath=" | awk -F= '{print $2}'`
            if [ -z $zonepath ]; then
                echo "Error: zonepath not found for zone $x"
                exit 1
            fi

			grep $INMAGE_DIR /tmp/zonecfg>>/dev/null
			if [ $? -eq 0 ]
			then
				echo "$INMAGE_DIR already exported to zone $x."
			else
				echo "add fs" >zonecfg1
				echo "set dir=$INMAGE_DIR" >>zonecfg1
				echo "set special=$INMAGE_DIR" >>zonecfg1
				echo "set type=lofs" >>zonecfg1
				echo "add options rw" >>zonecfg1
				echo "end">>zonecfg1
				zonecfg -z $x -f zonecfg1
				echo "$INMAGE_DIR exported to zone $x."
            fi
			
            devMaj=`cat /etc/name_to_major | grep involflt | awk '{print $2}'`
            if [ -z $devMaj ]; then
                echo "Error: device file not found in /etc/name_to_major."
                exit 1
            fi

            zlogin $x ls -l /dev/involflt >> /dev/null
			if [ $? -eq 0 ]; then
                echo "Device file /dev/involflt is already present in zone $x."

                tmaj=`zlogin $x ls -lL /dev/involflt | awk '{print $5}' | sed 's/,//' | sed 's/ //g'`
                if [ "$devMaj" != "$tmaj" ]; then
                    echo "Device file /dev/involflt has major number $tmaj in zone $x."
                    rm $zonepath/dev/involflt

                    mknod $zonepath/dev/involflt c $devMaj 0
                    if [ ! -c $zonepath/dev/involflt ]; then
                        echo "Error: device file export failed"
                        exit 1
                    fi
                fi
            else
                mknod $zonepath/dev/involflt c $devMaj 0
                if [ ! -c $zonepath/dev/involflt ]; then
                    echo "Error: device file export failed"
                    exit 1
                fi
            fi

            grep "/dev/involflt" /tmp/zonecfg >>/dev/null
            if [ $? -eq 0 ]
            then
				echo "involflt already exported to zone $x."
            else
                echo "add device" >zonecfg1
                echo "set match=/dev/involflt" >>zonecfg1
                echo "end" >>zonecfg1
				zonecfg -z $x -f zonecfg1
				echo "/dev/involflt exported to zone $x."
			fi

            zlogin $x ls -l /usr/local/.vx_version >> /dev/null
            if [ $? -eq 0 ]; then
                echo "InMage Vx version file found at /usr/local/.vx_version in zone $x."
            else
                echo "Creating InMage Vx version file at /usr/local/.vx_version in zone $x."
                if [ ! -d $zonepath/root/usr/local ]; then
                    zlogin $x ln -s $INMAGE_DIR/.vx_version /usr/local/.vx_version
                    if [ ! -L $zonepath/root/usr/local/.vx_version ]; then
                        echo "Error: Creation of /usr/local/.vx_version failed"
                        exit 1
                    fi
                fi
            fi
		done
    }

    Remove()
    {
		zoneadm list|grep -v grep|grep -v global >$ZONESLIST
		if [ ! -s $ZONESLIST ]
		then

			echo "There are no running zones...Please make them up"
			exit 1

		fi
		cat $ZONESLIST|while read x
		do
			zonecfg -z $x export -f /tmp/zonecfg
            zonepath=`zonecfg -z $x export|grep "zonepath=" | awk -F= '{print $2}'`
            if [ -z $zonepath ]; then
                echo "Error: zonepath not found for zone $x"
                exit 1
            fi

			grep $INMAGE_DIR /tmp/zonecfg|grep -v grep >>/dev/null
			if [ $? -ne 0 ]
			then
				echo "$INMAGE_DIR not exported to zone $x"
			else
				echo "remove fs dir=$INMAGE_DIR" >zonecfg1
				zonecfg -z $x -f zonecfg1
				echo "$INMAGE_DIR removed from zone $x."
            fi

			grep "/dev/involflt" /tmp/zonecfg|grep -v grep >>/dev/null
			if [ $? -ne 0 ]
			then
				echo "/dev/involflt not exported to zone $x"
			else
                echo "remove device match=/dev/involflt" >zonecfg1
				zonecfg -z $x -f zonecfg1
				echo "/dev/involflt removed from zone $x."
            fi

            if [ -c $zonepath/dev/involflt ]; then
                rm -f $zonepath/dev/involflt
            fi

		done
    }

    Reboot()
    {
		zoneadm list|grep -v grep|grep -v global >$ZONESLIST
		if [ ! -s $ZONESLIST ]
		then
			echo "There are no running zones...Please make them up"
			exit 1
		fi

		cat $ZONESLIST|while read x
        do
            echo "Rebooting zone $x"
            zoneadm -z $x reboot
            echo "done."
        done
    }

    if [ $# -eq 0 ]; then
        ADD=1
    elif [ $# -eq 1 ]; then
        if [ $1 != "clean" ]; then
            Usage
        fi 
        REMOVE=1
    else
        Usage
    fi
    
	if [ ! -d $INMAGE_DIR ]
	then
		echo "Scout Vx not installed on this system";
		exit 1;
	fi


    if [ $ADD -eq 1 ]; then
        echo "This script adds InMage agent to the zone configuration of running non-global zones. Do you want to proceed Y/N"
        read ans
        if [ $ans = "y" -o $ans = "Y" ] ; then
            Add
            reboot_required=1
        fi
        
    elif [ $REMOVE -eq 1 ]; then
        echo "This script removes InMage agent from the zone configuration of running non-global zones. Do you want to proceed Y/N"
        read ans
        if [ $ans = "y" -o $ans = "Y" ] ; then
            Remove
            reboot_required=1
        fi
    fi

    if [ $reboot_required -eq 1 ] ;then
        echo "The zone configuration changes are complete. The non-global zones require a reboot for the changes to take effect. Do you want to reboot now Y/N"
        read ans
        if [ $ans = "y" -o $ans = "Y" ] ; then
           Reboot
        else
            echo "You have chosen to not reboot the non-global zones. Please reboot them for the changes to take effect."
            exit 0
        fi
    fi
