#! /bin/sh

########################
## DECLARED VARIABLES ##
########################

LOG_FILE=/var/log/Post_Process_Linux_CX.log

if rpm -qa | grep -q inmageCX ; then
    CX_INST_DIR="/home/svsystems"
else
    echo "CX is not installed on this setup. Aborting the Post Process script execution."
    exit 1
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
    CX_IP=$1
    echo $CX_IP | grep "\.$" >/dev/null 2>&1
    if [ "$?" -eq "0" ]; then
        DO 'echo "Invalid IP:$CX_IP. Try again with the correct IP"'
        exit 1
    else

        FLD1=`echo "$CX_IP" | awk -F'.' '{print $1}'`
        FLD2=`echo "$CX_IP" | awk -F'.' '{print $2}'`
        FLD3=`echo "$CX_IP" | awk -F'.' '{print $3}'`
        FLD4=`echo "$CX_IP" | awk -F'.' '{print $4}'`
        if [ -z "${FLD1}" -o -z "${FLD2}" -o -z "${FLD3}" -o -z "${FLD4}" ];then
            DO 'echo -e "Invalid IP:$CX_IP. Try again with correct IP "'
            exit 1
        else
            oldIFS=$IFS
            IFS=.
            set -- $CX_IP

            if [ "$#" -ne "4" ]; then
                DO 'echo "Invalid IP:$CX_IP. Try again with the correct IP"'
                exit 1
            elif [ "$1" -eq "0" ]; then
                DO 'echo "Invalid IP:$CX_IP. Try again with the correct IP"'
                exit 1
            fi
            IFS=$oldIFS
        fi

        for oct in $FLD1 $FLD2 $FLD3 $FLD4;
        do
            echo $oct | egrep "^[0-9]+$" >/dev/null 2>&1
            if [ "$?" -ne "0" ]; then
                DO 'echo -e "Invalid IP:$CX_IP. Try again with correct IP"'
                exit 1
            else
                if [ "$oct" -lt "0" -o "$oct" -gt "255" ]; then
                    DO 'echo -e "Invalid IP:$CX_IP. Try again with correct IP"'
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

Replace()
{
    DO 'echo "Chosen CX IP is $SERVER_IP_ADDRESS"'
    DO 'echo "Chosen CX Port is $SERVER_PORT_NUMBER"'

    DO 'echo '
    DO 'echo "Changing CS_IP and PS_CS_IP key in amethyst.conf with chosen CX IP : $SERVER_IP_ADDRESS"'
    sed -i -e "s|^CS_IP =.*|CS_IP = \"$SERVER_IP_ADDRESS\"|g" $CX_INST_DIR/etc/amethyst.conf
    sed -i -e "s|^PS_CS_IP =.*|PS_CS_IP = \"$SERVER_IP_ADDRESS\"|g" $CX_INST_DIR/etc/amethyst.conf
    DO 'echo "Changing CS_PORT and PS_CS_PORT key in amethyst.conf with chosen CX Port : $SERVER_PORT_NUMBER"'
    sed -i -e "s|^CS_PORT =.*|CS_PORT = \"$SERVER_PORT_NUMBER\"|g" $CX_INST_DIR/etc/amethyst.conf
    sed -i -e "s|^PS_CS_PORT =.*|PS_CS_PORT = \"$SERVER_PORT_NUMBER\"|g" $CX_INST_DIR/etc/amethyst.conf

    # Replacing GUID value only if GUID is not null.
    if [ ! -z $GUID ];then
        DO 'echo "Adding HOST_GUID value in amethyst.conf"'
        sed -i -e "s|^HOST_GUID =.*|HOST_GUID = \"$GUID\"|g" $CX_INST_DIR/etc/amethyst.conf

        DO 'echo "Adding HOST_GUID value in system.conf"'
        sed -i -e "s|^HOST_GUID =.*|HOST_GUID = \"$GUID\"|g" $CX_INST_DIR/system/etc/system.conf       
    fi

    DO 'echo '
    DO 'echo "Changing SYSTEM_IP key in system.conf with chosen CX IP : $SERVER_IP_ADDRESS"'
    sed -i -e "s|^SYSTEM_IP.*|SYSTEM_IP = \"$SERVER_IP_ADDRESS\"|g" $CX_INST_DIR/system/etc/system.conf

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

Success_Msg()
{
    DO 'echo '
    DO 'echo "Post Processing steps for CX are successfully completed."'
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
        while getopts :h:i:p:g:H: opt
        do
            case $opt in
                i)SERVER_IP_ADDRESS="$OPTARG";;
                p)SERVER_PORT_NUMBER="$OPTARG" ;;
                g)GUID="$OPTARG";;
                H)HOST_NAME="$OPTARG";;                
                h|-*|*) echo "Usage: $0 [ -i <IP Address of the CX> ] [ -p <CX Server Port Number> ] [ -g <GUID of CX Server> ] [ -H <HOTNAME> ] "
	            exit 1
                ;;
            esac
        done
    fi
else
    echo "specify the installation options on the command-line."
    exit 1
fi

# If SERVER_IP_ADDRESS and SERVER_PORT_NUMBER are not empty, we are validating them.
if [ ! -z "$SERVER_IP_ADDRESS" -a ! -z "$SERVER_PORT_NUMBER" ]; then
    Validate_IP "$SERVER_IP_ADDRESS"
    
    Validate_Port "$SERVER_PORT_NUMBER"
else
    DO 'echo "please enter -i <IP Address of the CX> -p <CX Server Port Number> -g <GUID of CX Server> -H <HOTNAME> as command line arguments to the script."'
    exit 1
fi

EXISTING_GUID=`grep "^HOST_GUID" $CX_INST_DIR/etc/amethyst.conf|awk -F'"' '{print$2}'`

# Generating GUID value if both EXISTING_GUID and GUID argument are null.
if [ -z "$EXISTING_GUID" -a -z "$GUID" ];then
    DO 'echo "Generating fresh Host Guid."'
    GUID=`uuidgen`
fi
 
Replace

Success_Msg
