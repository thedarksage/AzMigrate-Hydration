#! /bin/sh

########################
## DECLARED VARIABLES ##
########################

LOG_FILE=/var/log/Post_Process_Linux_MT.log

if [ ! -f /usr/local/.vx_version ] && [ ! -f /usr/local/.fx_version ]; then
    echo "FX/VX is not installed on this setup. Aborting the Post Process script execution."
    exit 1
fi

if [ -f /usr/local/.vx_version ]; then
    VX_INST_DIR=`grep 'INSTALLATION_DIR' /usr/local/.vx_version |awk -F"=" '{print $2}'`
else
    echo "VX is not installed on this setup."
fi

if [ -f /usr/local/.fx_version ]; then
    FX_INST_DIR=`grep 'INSTALLATION_DIR' /usr/local/.fx_version |awk -F"=" '{print $2}'`
else
    echo "FX is not installed on this setup."
fi

###############
## FUNCTIONS ##
###############

DO()
{
 eval "$1 2>> $LOG_FILE | tee -a $LOG_FILE"
}

LOG()
{
 echo "$1" >> $LOG_FILE
}

Validate_IP()
{
    IP=$1
    echo $IP | grep "\.$" >/dev/null 2>&1
    if [ "$?" -eq "0" ]; then
        DO 'echo "Invalid IP:$IP. Try again with the correct IP"'
        exit 1
    else

        FLD1=`echo "$IP" | awk -F'.' '{print $1}'`
        FLD2=`echo "$IP" | awk -F'.' '{print $2}'`
        FLD3=`echo "$IP" | awk -F'.' '{print $3}'`
        FLD4=`echo "$IP" | awk -F'.' '{print $4}'`
        if [ -z "${FLD1}" -o -z "${FLD2}" -o -z "${FLD3}" -o -z "${FLD4}" ];then
            DO 'echo -e "Invalid IP:$IP. Try again with correct IP "'
            exit 1
        else
            oldIFS=$IFS
            IFS=.
            set -- $IP

            if [ "$#" -ne "4" ]; then
                DO 'echo "Invalid IP:$IP. Try again with the correct IP"'
                exit 1
            elif [ "$1" -eq "0" ]; then
                DO 'echo "Invalid IP:$IP. Try again with the correct IP"'
                exit 1
            fi
            IFS=$oldIFS
        fi

        for oct in $FLD1 $FLD2 $FLD3 $FLD4;
        do
            echo $oct | egrep "^[0-9]+$" >/dev/null 2>&1
            if [ "$?" -ne "0" ]; then
                DO 'echo -e "Invalid IP:$IP. Try again with correct IP"'
                exit 1
            else
                if [ "$oct" -lt "0" -o "$oct" -gt "255" ]; then
                    DO 'echo -e "Invalid IP:$IP. Try again with correct IP"'
                    exit 1
                fi
            fi
        done
    fi
}

Validate_Port()
{
    CX_PORT=$1
    echo $CX_PORT | grep -q "^[0-9]\+$"
    if [ $? -ne 0 ]; then
        DO 'echo "Invalid port. Try again with correct port in the range 1-65535."'
        exit 1
    fi
    if [ "$CX_PORT" -lt "1" -o "$CX_PORT" -gt "65535" ]; then
        DO 'echo "Invalid port. Try again with correct port in the range 1-65535."'
        exit 1
    fi
}

Success_Msg()
{
    DO 'echo '
    DO 'echo "Post Processing steps for Linux MT are successfully completed."'
}

Replace()
{

    DO 'echo "Chosen CX IP is $SERVER_IP_ADDRESS"'
    DO 'echo "Chosen CX PORT is $SERVER_PORT_NUMBER"'
    DO 'echo "Chosen Agent is $AGENT_IP"'

    if [ ! -z "$VX_INST_DIR" ]; then
	DO 'echo '
        DO 'echo "Keeping ${AGENT_IP} in /etc/hosts"'
	sed -i -e "s|10.80.253.124|${AGENT_IP}|g" /etc/hosts

	DO 'echo '
        DO 'echo "Changing Hostname key in drscout.conf with chosen CX IP : $SERVER_IP_ADDRESS"'
        sed -i -e "s|^Hostname.*|Hostname=$SERVER_IP_ADDRESS|g" $VX_INST_DIR/etc/drscout.conf
        DO 'echo "Changing Port key in drscout.conf with chosen CX PORT : $SERVER_PORT_NUMBER"'
        sed -i -e "s|^Port.*|Port=$SERVER_PORT_NUMBER|g" $VX_INST_DIR/etc/drscout.conf

        if [ ! -z "$GUID" ];then
            DO 'echo "Adding HostId key in drscout.conf"'
            sed -i -e "s|^HostId.*|HostId=$GUID|g" $VX_INST_DIR/etc/drscout.conf
        fi

        DO 'echo '
        DO 'echo "Enabling vxagent service"'
        chkconfig --level 1235 vxagent on 
 
        DO 'echo '
        DO 'echo "Starting the vxagent service"'
        /etc/init.d/vxagent start 
    fi

    if [ ! -z "$FX_INST_DIR" ]; then
        DO 'echo '
        DO 'echo "Changing SVServer key in config.ini with chosen CX IP : $SERVER_IP_ADDRESS"'
        sed -i -e "s|^SVServer =.*|SVServer = $SERVER_IP_ADDRESS|g" $FX_INST_DIR/config.ini

        DO 'echo "Changing SVServerPort key in config.ini with chosen CX PORT : $SERVER_PORT_NUMBER"'
        sed -i -e "s|^SVServerPort =.*|SVServerPort = $SERVER_PORT_NUMBER|g" $FX_INST_DIR/config.ini

        if [ ! -z "$GUID" ];then
            DO 'echo "Adding HostId key in config.ini"'
            sed -i -e "s|^HostId =.*|HostId = $GUID|g" $FX_INST_DIR/config.ini
        fi

        DO 'echo '
        DO 'echo "Moving /etc/svagent to /etc/init.d/"'
        mv /etc/svagent /etc/init.d/svagent >> $LOG_FILE 2>&1

        DO 'echo "Starting svagent service"'
        /etc/init.d/svagent start 
    fi

    # If HOST_NAME argument is passed, we are changing the hostname and rebooting the system.
    if [ ! -z "$HOST_NAME" ]; then
        DO 'echo "Changing the HOSTNAME value in /etc/sysconfig/network and invoking hostname command."'
        sed -i -e "s|HOSTNAME.*|HOSTNAME=$HOST_NAME|g" /etc/sysconfig/network
        hostname "$HOST_NAME"

        #Rebooting the system to change the hostname.
        DO 'echo "Rebooting the system to change the hostname"'
        reboot
    fi  

}

##########
## MAIN ##
##########

DO 'echo '
LOG "-------------------------------------------------------------------"
LOG "`date`" 
LOG "-------------------------------------------------------------------"
 
if [ ! -z $1 ]; then
    if echo $1 | grep -q ^- ; then
        while getopts :h:a:i:p:g:H: opt
        do
            case $opt in
                i)SERVER_IP_ADDRESS="$OPTARG";;
                p)SERVER_PORT_NUMBER="$OPTARG" ;;
                a)AGENT_IP="$OPTARG";;
                g)GUID="$OPTARG";;
                H)HOST_NAME="$OPTARG";;
                h|-*|*) echo "Usage: $0 [ -i <IP Address of the CX> ] [ -p <CX Server Port> ] [ -a <IP Address of the Agent> ] [ -g <GUID of CX Server> ] [ -H <HOTNAME> ]"
                    exit 1
                    ;;
            esac
        done
    fi
else
    echo "Please specify the installation options on the command-line."
    exit 1
fi

# If SERVER_IP_ADDRESS, SERVER_PORT_NUMBER and AGENT_IP are not empty, we are validating them.
if [ ! -z "$SERVER_IP_ADDRESS" -a ! -z "$SERVER_PORT_NUMBER" -a ! -z "$AGENT_IP" ]; then
    Validate_IP "$SERVER_IP_ADDRESS"

    Validate_Port "$SERVER_PORT_NUMBER"

    Validate_IP "$AGENT_IP"
else
    DO 'echo "please enter -i <IP Address of the CX> -p <CX Server Port> -a <IP Address of the Agent> -g <GUID of CX Server> -H <HOSTNAME> as command line arguments to the script."'
    exit 1
fi


EXISTING_GUID=`grep "HostId" $VX_INST_DIR/etc/drscout.conf|awk -F'=' '{print$2}'`

if [ -z "$EXISTING_GUID" -a -z "$GUID" ];then
    DO 'echo "Generating fresh Host Guid."'
    GUID=`uuidgen`
fi

Replace

Success_Msg
