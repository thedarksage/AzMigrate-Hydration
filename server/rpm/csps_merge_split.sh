#!/bin/sh

UPGERR="/var/log/cxserver.err"
UPGLOG="/var/log/cxserver.log"
ETC_PATH="/etc/init.d"
CXPSCTL="${ETC_PATH}/cxpsctl"
TMANAGERD="${ETC_PATH}/tmanagerd"
SENDMAIL="${ETC_PATH}/sendmail"
CFGFILE=install.cfg
install_dir=/home/svsystems
INST_LOG=/var/log/cx_install.log

if [ -f /etc/redhat-release ]; then
    HTTPD="${ETC_PATH}/httpd"
    MYSQLD="${ETC_PATH}/mysqld"
fi

RPM_ARCH=`rpm -q --qf "%{ARCH}" rpm`

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

english=`ifconfig | grep "inet addr:" 2>/dev/null`
german=`ifconfig | grep "inet Adresse:" 2>/dev/null`
french=`ifconfig | grep "inet adr:" 2>/dev/null`

if [ ! -z "$english" ]; then
        value=addr
elif [ ! -z "$german" ]; then
        value=Adresse
elif [ ! -z "$french" ]; then
        value=adr
fi

# Check what is the CX_TYPE in /home/svsystems/etc/amethyst.conf
cx_type=`grep ^CX_TYPE /home/svsystems/etc/amethyst.conf | cut -d'=' -f2 | tr -d '""' | tr -d " "`

ReadConfFile()
{
    SECTION=""
    MYSQLFILESLIST=""
    RPMFILESLIST=""
    SVSFLAG=0
    SQLDUMPFLAG=0
    LOGFLAG=0
    
    while read file_line
    do    
        echo $file_line | grep "\[\|\]"  >/dev/null 2>&1    
        if [ $? -eq 0 ]; then    
            SECTION=$file_line
            continue    
        elif [ "$SECTION" =  "[MYSQL]" ] ; then    
            MYSQLFILESLIST="$MYSQLFILESLIST  $file_line"    
        elif [ "$SECTION" = "[RPM]" ] ;  then    
            RPMFILESLIST="$RPMFILESLIST $file_line"    
        elif  [ "$SECTION" = "[VERSION]" ] ; then    
            echo $file_line | grep -i "UPGRADEVERSION" >/dev/null 2>&1    
            if [ $? -eq 0 ] ; then
                UPGRADEVERSION=`echo $file_line | cut -d "=" -f2 | tr -d " "`
                CURRENTVERSION=$UPGRADEVERSION
            fi    
        elif [ "$SECTION" = "[BACKUP]" ] ; then    
            BKPTYPE=`echo $file_line | cut -d "=" -f1 | tr -d " " | tr -s [a-z] [A-Z]`
            BKPOPT=`echo $file_line | cut -d "=" -f2 | tr -d " "  | tr -s [a-z] [A-Z]`    
            if [ "$BKPOPT" = "YES" ] ; then
                [ "$BKPTYPE" = "SVSLOGBKP" ] && LOGFLAG=1
                [ "$BKPTYPE" = "SVSLOGBKP" ] && LOGFLAG=1
                [ "$BKPTYPE" = "SQLBKP" ] && SQLDUMPFLAG=1
                [ "$BKPTYPE" = "EXEBKP" ] && SVSFLAG=1
            fi    
        fi
    
    done <$CFGFILE
}

# Put conditions on this cx_type
ValidateInputAdd()
{
    cx_type=$1
    silent_add=$2

    if [ "$cx_type" = "CS" ]; then
        comp_tb_installed=PS	
    elif [ "$cx_type" = "PS" ]; then
        comp_tb_installed=CS
    fi
    echo
    if [ -e /usr/local/.pillar ]; then
        LOG "ControlService is already installed on this setup..."
    else
        LOG "$cx_type is already installed on this system..."
    fi

    if [ "$silent_add" = "Y" ]; then
        ans=Y
    else
        DO 'echo -n "Would you like to add $comp_tb_installed?(Y|N) [default N]: "'
        read ans
        if [ "$ans" = "Y" -o "$ans" = "y" ]; then
            LOG "User Input : ${ans}"
        else
            LOG "User Input : ${ans}"
            echo
            exit 1
        fi
    fi
}

ValidateInputRemove()
{
    DO 'echo'
    DO 'echo "Configuration Server and Process Server are already installed on this system..."'
    DO 'echo -n "Would you like to remove the Process Server?(Y|N) [default N]:"'
    read ans
    
    if [ -z "$ans" ]; then
        ans=N
    fi
    
    if [ "$ans" = "Y" -o "$ans" = "y" ]; then
        LOG "User Input : ${ans}"
        cx_type_to_remove=PS
    else
        LOG "User Input : ${ans}"
        exit 0
    fi    
    cx_type_to_remove=$cx_type_to_remove
}

ValidateConfigServer()
{
    returnvalue=0;
    local SRV_IP=${1?"Missing IP address argument in $FUNCNAME"}
    
    if [ -n "$2" ] ; then
        PORT=$2
    else
        PORT=80
    fi
    
    EXIT_CODE=`GET -s -d http://${SRV_IP}:${PORT}/request_handler.php | awk '{print $1}'`    
    if [ -n "$EXIT_CODE" -a $EXIT_CODE -eq 200 ] ; then
        returnvalue=0;
    else
        if [ $? -ne 0 ] ; then
            DO 'echo'
            DO 'echo  "WARNING:"'            
            if [ -e /usr/local/.pillar ]; then
                DO 'echo "The ControlService http://${SRV_IP}:${PORT} is currently not reachable."'
                DO 'echo "ProcessService registration is incomplete. Please ensure ControlService"'
                DO 'echo "is available for the ProcessService to register successfully."'
            else
                DO 'echo "Configuration Server http://${SRV_IP}:${PORT} is currently not reachable."'
                DO 'echo "Process Server registration is incomplete. Please ensure Configuration Server"'
                DO 'echo "is available for the Process Server to register successfully."'
            fi    
            DO 'echo'
        fi   
        returnvalue=5;
    fi
}

# Check the CX type.
if [ "$cx_type" = "1" ]; then
    if [ "$1" = "Y" ]; then
        ValidateInputAdd CS Y
    else
        ValidateInputAdd CS
    fi
elif [ "$cx_type" = "2" ]; then
    ValidateInputAdd PS
elif [ "$cx_type" = "3" ]; then
    ValidateInputRemove
fi

StopServer()
{
    $TMANAGERD stop 1>>$UPGLOG 2>>$UPGERR
    $SENDMAIL stop 1>>$UPGLOG 2>>$UPGERR
    $MYSQLD stop 1>>$UPGLOG 2>>$UPGERR
    $HTTPD stop 1>>$UPGLOG 2>>$UPGERR
}

AddOrReplace()
{
    local keyname=${1?:"Key name to be added/changed is required as 1st argument in function $FUNCNAME"}
    local conffile=${2?:"Configuration file name is required as 2nd argument in function $FUNCNAME"}
    local keyvalue=${3?:"Key value to be used for the key name is required as 3rd argument in function $FUNCNAME"}
    
    [ ! -f $conffile ] && echo "$conffile does not exist.Aborting." && exit 1
    
    # Check if keyname is already present in conffile. If so, just do a sed on conffile and change value
    # Otherwise, append a new line to conffile for the keyname
    
    if grep -q "^[[:space:]]*${keyname}[[:space:]]*=[[:space:]]*.\+$" $conffile ; then    
        sed -e "s/^[[:space:]]*${keyname}[[:space:]]*=[[:space:]]*.\+$/${keyname} = \"$keyvalue\"/" $conffile > ${conffile}.new
        mv ${conffile}.new $conffile
        chmod 644 $conffile    
    else    
        echo "" >> $conffile
        echo "$keyname = \"$keyvalue\"" >> $conffile    
    fi
}

StartServer()
{
    $SENDMAIL start 1>>$UPGLOG 2>>$UPGERR
    
    if [ "$comp_tb_installed" = "PS" ]; then
        chkconfig cxpsctl on 2>>$UPGERR
        $CXPSCTL start 2>>$UPGERR
    fi
    
    $MYSQLD start 1>>$UPGLOG 2>>$UPGERR
    $HTTPD start 1>>$UPGLOG 2>>$UPGERR
    $TMANAGERD start 1>>$UPGLOG 2>>$UPGERR
}

CreateUser()
{
    if grep -q svsystems /etc/passwd ; then
        return 0
    fi
    
    groupadd svsystems
    
    if ! useradd svsystems -g svsystems -p AAsg9gWFrcpe6 2>/dev/null ; then
        DO 'echo "Error while creating the user 'svsystems'... Aborting."'
        exit 1
    fi
}

PromptForCXPort()
{
    # Find out IP address or hostname
    # If number of IP addresses on the box is 1, use it directly in the variable IPAddress
    # If number of IP addresses on box is > 1, string all of them together in IPAddress for use in a for-do-done later
    # In this case, we need to create multiple virtual host blocks in dps_inmage.conf file so that the box responds
    # when the UI is accessed via any of its assigned IP addresses.
    # This is applicable in situations with multiple NICs with or without bonding - bug 4537
    
    NumIP=`ifconfig |sed -e "/vmnet/,/^$/ d" |grep "inet ${value}:" | grep -v '127.0.0.1' |cut -d: -f2 |awk '{ print $1}'|wc -l`    
    if [ $NumIP -eq 1 ]; then
        IPAddress=`ifconfig |sed -e "/vmnet/,/^$/ d" |grep "inet ${value}:" | grep -v '127.0.0.1' |cut -d: -f2 |awk '{ print $1}'`
    elif [ $NumIP -gt 1 ]; then
        for ip_add in `ifconfig |sed -e "/vmnet/,/^$/ d" |grep "inet ${value}:" | grep -v '127.0.0.1' |cut -d: -f2 |awk '{ print $1}'`
        do
            IPAddress="$ip_add $IPAddress"
        done
    fi
    
    [ -e dps_inmage.conf ] && rm -f dps_inmage.conf    
    if [ -z "$port_y_n" -o "$port_y_n" = "N" -o "$port_y_n" = "n" ]; then    
        CXPort=80    
    else
        DO 'echo -n "Please enter the port number of your choice: "'
        read CXPort
        i=0
        while [ -z "$CXPort" ]
        do
            i=$((i+1))
            if [ $i -eq 3 ]; then
    
                DO 'echo "Failed receiving input after 3 attempts. Defaulting port number to 80..."'
                    CXPort=80
                    break
    
            fi    
            DO 'echo -n "Invalid input.Please enter the port number of your choice:"'
            read CXPort    
        done
    
        # Make sure the port entered is not in use - bug 4475 (exception is when it is defaulted to port 80)
        while [ $CXPort -ne 80 ] && grep -q "\b$CXPort\/tcp" /etc/services
        do
            DO 'echo "Port $CXPort is already in use."'
            DO 'echo -n "Please enter another port number:"'
            read CXPort
        done
    
        if [ $CXPort -ne 80 ]; then
        if [ $NumIP -eq 1 ]; then
            sed -e "s/IPADDR/$IPAddress/g" virtual_host_template | sed -e "s/PORT/$CXPort/g" | sed -e "s/USER/$USER/g" | \
            sed -e "s/HOSTNAME/$HOSTNAME/g" | sed -e "s!DOCUMENT_ROOT!${install_dir}/admin/web!g" | \
            sed -e "s/ERROR_LOG/cx_error.log/g" | sed -e "s/ACCESS_LOG/cx_access.log/g" > dps_inmage.conf
        elif [ $NumIP -gt 1 ]; then
            vhost_start_line_num=$(grep -n \<VirtualHost.*\> virtual_host_template | cut -d':' -f1)
            vhost_start_line_num=$((vhost_start_line_num-1))
            sed -n "1, $vhost_start_line_num p" virtual_host_template | sed -e "s/PORT/$CXPort/g" > dps_inmage.conf
            for ip_entry in $IPAddress
            do
                sed -n "/<VirtualHost.*>/,/<\/VirtualHost\>/ p" virtual_host_template | sed -e "s/IPADDR/$ip_entry/g" | \
                sed -e "s/PORT/$CXPort/g" | sed -e "s/USER/$USER/g" | sed -e "s/HOSTNAME/$HOSTNAME/g" | \
                sed -e "s!DOCUMENT_ROOT!${install_dir}/admin/web!g" | sed -e "s/ERROR_LOG/cx_error.log/g" | \
                sed -e "s/ACCESS_LOG/cx_access.log/g" >> dps_inmage.conf
                echo >> dps_inmage.conf
                echo >> dps_inmage.conf
            done
            vhost_end_line_num=$(grep -n \</VirtualHost\> virtual_host_template | cut -d':' -f1)
            vhost_end_line_num=$((vhost_end_line_num+1))
            sed -n "$vhost_end_line_num, $ p" virtual_host_template >> dps_inmage.conf
        fi
        # Updating DcoumentRoot in ssl.conf to get CX/RX UI over https
        sed -i -e "s/.*\(DocumentRoot.*\)/\1/g; s/DocumentRoot.*/DocumentRoot \"\/home\/svsystems\/admin\/web\"/g" /etc/httpd/conf.d/ssl.conf
        elif [ $CXPort -eq 80 ]; then
        :
        fi
    
    fi
    
    # Place the so-formed dps_inmage.conf in appropriate location
    [ -f /etc/redhat-release ] && [ -f dps_inmage.conf ] && mv dps_inmage.conf /etc/httpd/conf.d/ 2>>$UPGERR
    # Place the php_inmage.conf file in appropriate location
    [ -f /etc/redhat-release ] && cp php_inmage.conf /etc/httpd/conf.d/ 2>>$UPGERR
}

ModifyConfIniFiles()
{
    # Redhat 4 and 5 have /etc/php.ini    
    if [ -f /etc/redhat-release -a $CXPort -eq 80 ]; then    
        # For httpd.conf
        ExistingDocRoot=`grep "DocumentRoot[ ]*\".*\"" /etc/httpd/conf/httpd.conf | grep -v ^# | awk '{print $2}' | awk -F\" '{print $2}'`    
        NewDocRoot=${install_dir}/admin/web    
        if [ "$ExistingDocRoot" != "$NewDocRoot" ]; then    
            cp /etc/httpd/conf/httpd.conf /etc/httpd/conf/httpd.conf.install_save 2>>$UPGERR
            DO 'echo "NOTE: Changing the document root in /etc/httpd/conf/httpd.conf from $ExistingDocRoot to ${NewDocRoot}..."'
            DO 'echo "NOTE: Saved a copy of the previous httpd.conf file to /etc/httpd/conf/httpd.conf.install_save..."'
            sed -e "s|$ExistingDocRoot|$NewDocRoot|g" /etc/httpd/conf/httpd.conf > /etc/httpd/conf/httpd.conf.new
            sed -i -e "s/^DocumentRoot.*/#&/g" /etc/httpd/conf.d/ssl.conf 2>>$UPGERR
            mv /etc/httpd/conf/httpd.conf.new /etc/httpd/conf/httpd.conf 2>>$UPGERR    
        fi    
    fi
    
    # BUG 6086 - Turn off access logging for Apache on both RHEL/SLES systems
    if [ -f /etc/redhat-release ]; then
        sed -i -e "s/^[ ]*CustomLog/# CustomLog/g" /etc/httpd/conf/httpd.conf 2>>$UPGERR
    fi
    
}

ChangeMySQLDataDir()
{
    # Stop the service and wait for the inno db engine to shutdown cleanly
    $MYSQLD stop 1>>$UPGLOG 2>>$UPGERR
    sleep 5
    
    # Save copy of my.cnf
    mv /etc/my.cnf /etc/my.cnf.install_save 2>>$UPGERR
    
    # Change the datadir field value in my.cnf to install_dir/mysql and create such a directory
    sed "s|^\(datadir=\).*$|\1${install_dir}/mysql|" /etc/my.cnf.install_save | \
    sed "s/\(\[mysql\.server\]\)/set-variable = max_allowed_packet=16M\nset-variable = max_connections=350\nset-variable = max_connect_errors=25\n\n\1/" > /etc/my.cnf
    chmod +x /etc/my.cnf
    mkdir $install_dir/mysql
    # Start the service again
    $MYSQLD start 1>>$UPGLOG 2>>$UPGERR
}

InstallSQL()
{
    DO 'echo "Creating the database..."'
    
    if ! $MYSQLD start 1>>$UPGLOG 2>>$UPGERR ; then
        DO 'echo "Error while starting MySQL service... Aborting."'
        exit 1
    fi
    
    sleep 1
    
    LOG "Checking MySQL connection and assigning MySQL root password if not set. "
    while [ "X$MYSQL_ROOT_PWD" = "X" ]
    do
    if mysql --user=root -e exit 1>/dev/null 2>&1; then
        DO 'echo -n "Please set the MySQL root password: "'
        ROOT_PWD=1
    else
        DO 'echo -n "Please enter the MySQL root password: "'
    fi
    stty -echo
    read MYSQL_ROOT_PWD
    stty echo
    if [ -z "$MYSQL_ROOT_PWD" ]; then
        DO 'echo -e "\nInvalid input. Password can not be empty.\n"'
    elif [ "$ROOT_PWD" = "1" ]; then
        mysqladmin -u root password "$MYSQL_ROOT_PWD"
    elif ! mysql --user=root -p$MYSQL_ROOT_PWD -e exit 1>/dev/null 2>&1; then
        DO 'echo -e "\nInvalid password. Unable to connect, please retry.\n"'
        MYSQL_ROOT_PWD=""
    fi
    done 
    echo ; echo
    
    while [ "X$MYSQL_PASSWORD" = "X" ]
    do
        DO 'echo -n "Please enter the database user (svsystems) password: "'
        stty -echo
        read MYSQL_PASSWORD
        stty echo
        [ -z "$MYSQL_PASSWORD" ] && DO 'echo -e "\nInvalid input. Password can not be empty.\n"'
    done
    echo ; echo
    
    # For changing the password entry in the create_user.sql file
    sed -i -e "s/IDENTIFIED.*WITH/IDENTIFIED BY \'$MYSQL_PASSWORD\' WITH/g" create_user.sql
    
    if ! mysql -u root -p$MYSQL_ROOT_PWD < create_user.sql ; then
        DO 'echo "Error while creating the required MySQL database user... Aborting."'
        DO 'echo'
        exit 1
    fi
    
    # Assuming svs*.sql will be installed without any problems
    for sqlfile in $MYSQLFILESLIST svsdb_triggers.sql
    do
        [ -e $sqlfile ] && mysql -u svsystems --password=$MYSQL_PASSWORD --database=svsdb1 < $sqlfile
        if [ $? -ne 0 ]; then
            DO 'echo'
            exit 1
        fi
    done
    
    LOG "Writing DB_PASSWD=${MYSQL_PASSWORD} into amethyst.conf"
    AddOrReplace "DB_PASSWD" "$install_dir/etc/amethyst.conf" "$MYSQL_PASSWORD"
    
    LOG "Writing DB_ROOT_PASSWD=${MYSQL_ROOT_PWD} into amethyst.conf"
    AddOrReplace "DB_ROOT_PASSWD" "$install_dir/etc/amethyst.conf" "$MYSQL_ROOT_PWD"

}


# configures cxpsctl and cxps.conf so that it has the correct install information
ConfigureCxPs()
{
    CXPS_DIR="$install_dir/transport"
    # conf file
    sed -i -e "s|^install_dir =.*|install_dir = ${CXPS_DIR}|g" ${CXPS_DIR}/cxps.conf
    # control file
    sed -i -e "s|^CXPS_HOME[ \t]*=.*$|CXPS_HOME=${CXPS_DIR}|g" ${CXPSCTL} 
    sed -i -e "s|^CXPS_CONF[ \t]*=.*$|CXPS_CONF=${CXPS_DIR}/cxps.conf|g" ${CXPSCTL}
}

Validate_IP() {
    IP_ADDR=$1
    FLD1=`echo "$IP_ADDR" | awk -F'.' '{print $1}'`
    FLD2=`echo "$IP_ADDR" | awk -F'.' '{print $2}'`
    FLD3=`echo "$IP_ADDR" | awk -F'.' '{print $3}'`
    FLD4=`echo "$IP_ADDR" | awk -F'.' '{print $4}'`
    
    if [ -z "${FLD1}" -o -z "${FLD2}" -o -z "${FLD3}" -o -z "${FLD4}" ];then
        echo -e "*** Invalid IP address: $IP_ADDR. Try again with correct IP Address.\n"
        return 1
    fi
    
    echo $FILED1 | egrep '^(0|127)' > /dev/null 2>&1
    if [ "$?" -eq "0" ]; then
    echo -e "*** Invalid IP address: $IP_ADDR. Try again with correct IP Address.\n"
    return 1	
    fi
    
    for oct in $FLD1 $FLD2 $FLD3 $FLD4;
    do
        echo $oct | egrep "^[0-9]+$" >/dev/null 2>&1
        if [ "$?" -ne "0" ]; then
            echo -e "*** Invalid IP address: $IP_ADDR. Try again with correct IP Address.\n"
            return 1
        elif [ "$oct" -lt "0" -o "$oct" -gt "255" ]; then
            echo -e "*** Invalid IP address: $IP_ADDR. Try again with correct IP Address.\n"
            return 1
        fi
    done
}

Ask_CSIP() {
    ps_cs_ip=""
    until [ -n "$ps_cs_ip" ]
    do
        DO 'echo -n  "$QUESTION"'
        read ps_cs_ip
        echo
    done
    Validate_IP $ps_cs_ip
    [[ $? -ne 0 ]] && Ask_CSIP
    LOG ""
    LOG "PS_CS_IP given by user : ${ps_cs_ip}"
}

if [ "$comp_tb_installed" = "PS" ]; then
    # Stop the tmanagerd services.
    # Install the PS related rpms.
    # List the CS_IP, CS_PORT and validate
    # Ask user for confirmation. If Y, proceed. Else exit	 
    # Change the values accordingly in the amethyst.conf file.
    
    cs_ip=`grep ^CS_IP /home/svsystems/etc/amethyst.conf | cut -d'=' -f2 | tr -d '""' | tr -d " "`
    cs_port=`grep ^CS_PORT /home/svsystems/etc/amethyst.conf | cut -d'=' -f2 | tr -d '""' | tr -d " "`    
    if [ -e /usr/local/.pillar ]; then
        QUESTION='This ProcessService can register to any ControlService. Please enter ipaddress of Primary ControlService/Cluster ControlService : '
        Ask_CSIP
    
        # Taking 80 as default port regardless of no. of NICs(as suggested by Satish)
        ps_cs_port=80
    
        LOG ""
        LOG "PS_CS_PORT given by user : ${ps_cs_port}"
        DO 'echo " Validating existing ControlService..."'
    else
        QUESTION='This process server can register to any configuration server. Please enter ipaddress of Primary CS/Cluster CS : '
        Ask_CSIP
    
        DO 'echo -n "Please enter the given configuration server port number: "'
        read ps_cs_port
    
        LOG ""
        LOG "PS_CS_PORT given by user : ${ps_cs_port}"
            DO 'echo "Validating existing Configuration Server..."'
    fi
    
        # Validation for the IP address and port number of the Configuration server
        ValidateConfigServer $ps_cs_ip $ps_cs_port
    
    # Add the entries in conf file
        LOG "Writing PS_CS_IP=${ps_cs_ip} into amethyst.conf"
    AddOrReplace "PS_CS_IP" "/home/svsystems/etc/amethyst.conf" "$ps_cs_ip"
        LOG "Writing PS_CS_PORT=${ps_cs_port} into amethyst.conf"
    AddOrReplace "PS_CS_PORT" "/home/svsystems/etc/amethyst.conf" "$ps_cs_port"
    
    DO 'echo "Stopping the services..."'
    StopServer
    
    if [ -e /usr/local/.pillar ]; then
        DO 'echo "Installing the ProcessService..."'
    else
        DO 'echo "Installing the Process Server..."'
    fi
    
    ConfigureCxPs
    
    for i in {0,1,2,3,4,5,6}
    do
        # cxpsctl
        [ ! -h /etc/rc.d/rc${i}.d/S84cxpsctl ] && ln -s $CXPSCTL /etc/rc.d/rc${i}.d/S84cxpsctl
        [ ! -h /etc/rc.d/rc${i}.d/K84cxpsctl ] && ln -s $CXPSCTL /etc/rc.d/rc${i}.d/K84cxpsctl
    done
   
    # Edit the values in the amethyst.conf file.
    LOG "Writing CX_TYPE=3 into amethyst.conf"	
    AddOrReplace "CX_TYPE" "/home/svsystems/etc/amethyst.conf" "3"	
    
    groupadd nogroup 2>/dev/null
    chmod +x ${install_dir}/bin/*.pl
    DO 'echo "Starting the services..."'    
    StartServer    
    
    service cxpsctl restart
    
    if [ -e /usr/local/.pillar ]; then
        DO 'echo "The ProcessService Installation is successful..."'
    else
        DO 'echo "The PS Installation is successful..."'
    fi
        
elif [ "$comp_tb_installed" = "CS" ]; then
    # Install the CS related rpms.
    # Check for the number of IP address.
    # If IP address are more then 1, select one.
    # Else take the default IP.
    # Enter the port for the apache service.
    # Enter the mysql database password.
    
    DO 'echo -n "Would you like to continue with the CS Server Installation?(Y|N) [default Y] ? "'
    read ans
    if [ -z "$ans" ]; then
            ans=Y
    fi
    if [ "$ans" = "Y" -o "$ans" = "y" ]; then
    :
    else
        exit 1
    fi
    
    DO 'echo "Stopping the services..."'
    StopServer
    
    echo
    DO 'echo -n "Would you like to install the Configuration Server on a different port than port 80?(Y/N) [default N] :"'
    read port_y_n
    
    if [ -z "$port_y_n" -o "$port_y_n" = "N" -o "$port_y_n" = "n" -o "$port_y_n" = "Y" -o "$port_y_n" = "y" ]; then
        :
    else
        DO 'echo "Invalid Option... Aborting."'
        exit 1
    fi
    
    CreateUser
    # Read the configuration file.	
    ReadConfFile 
    PromptForCXPort
    ModifyConfIniFiles
    ChangeMySQLDataDir
    InstallSQL
    
    # In this case CS IP will be the same as the DB_HOST. and the DB_HOST value will change to "localhost"
    cs_ip=`grep ^DB_HOST /home/svsystems/etc/amethyst.conf | cut -d"=" -f2 | tr -d '""' | tr -d " "`
    
    # Enter CS port and CX type in amethyst.conf
    LOG "Writing CS_PORT=${CXPort} in amethyst.conf"
    AddOrReplace "CS_PORT" "/home/svsystems/etc/amethyst.conf" "$CXPort"
    LOG "Writing CS_IP=${cs_ip} in amethyst.conf"
    AddOrReplace "CS_IP" "/home/svsystems/etc/amethyst.conf" "$cs_ip"
    LOG "Writing DB_HOST=localhost in amethyst.conf"
    AddOrReplace "DB_HOST" "/home/svsystems/etc/amethyst.conf" "localhost"
    LOG "Writing CX_TYPE=3 in amethyst.conf"
    AddOrReplace "CX_TYPE" "/home/svsystems/etc/amethyst.conf" "3"
    [ -d ${install_dir}/admin/web/inmages/ ] && cp images/*.* ${install_dir}/admin/web/inmages/
    DO 'echo "Starting the services..."'
    StartServer
    
    # TO BE DONE: Copy the proper tgz
    [[ ! -d $install_dir/admin/web/sw ]] && mkdir -p $install_dir/admin/web/sw/ && chmod 777 $install_dir/admin/web/sw/    
    cp -f ../*.tar.gz ../*.exe ../*.zip $install_dir/admin/web/sw/ 2>>$UPGERR
    
    # To be done: Copy the pdf,txt,doc in /admin/web/docs
    [[ ! -d $install_dir/admin/web/docs ]] && mkdir -p $install_dir/admin/web/docs
    cp -f ../*.pdf ../*.doc ../*.txt $install_dir/admin/web/docs 2>>$UPGERR   
elif [ "$cx_type_to_remove" = "CS" ]; then
    DO 'echo " WARNING: All replication pairs configured on this Server will be deleted."'
    DO 'echo " WARNING: Agents will be unregistered from the Server"'
    DO 'echo -n "Would you like to remove the Configuration Server?(Y|N) [default N]:"'
    read ans
    if [ -z "$ans" ]; then
        ans=N
    fi
    if [ "$ans" = "Y" -o "$ans" = "y" ]; then
    :
	else
        exit 1
    fi

	DO 'echo "Stopping services..."'
	/etc/init.d/tmanagerd stop 2>/dev/null

	if [ -e /etc/redhat-release ]; then
        APACHE_SERVICE="httpd"
        PHP_INI="/etc/php.ini"
    fi

    /etc/init.d/$APACHE_SERVICE stop 2>/dev/null

	cat /home/svsystems/etc/version | grep -qi InMage
	if [ $? -eq 0 ]; then
        rpm -e --nodeps inmageCS >/dev/null 2>&1 
	else
        rpm -e --nodeps xiotechCS >/dev/null 2>&1 
	fi

    DO 'echo "Dropping the database..."'
    if mysql --user=root -e exit 1>/dev/null 2>&1; then
        mysql -u root --execute="drop database svsdb1" 2>/dev/null            
    else
        while [ "X$MYSQL_ROOT_PWD" = "X" ]
        do
            DO 'echo -n "Please enter the MySQL root password: "'
            stty -echo
            read MYSQL_ROOT_PWD
            stty echo
            if [ -z "$MYSQL_ROOT_PWD" ]; then
                DO 'echo -e "\nInvalid input. Password can not be empty.\n"'
            elif ! mysql --user=root -p$MYSQL_ROOT_PWD -e exit 1>/dev/null 2>&1; then
                DO 'echo -e "\nInvalid password. Unable to connect, please retry.\n"'
                MYSQL_ROOT_PWD=""
            else
                mysql -u root -p$MYSQL_ROOT_PWD --execute="drop database svsdb1" 2>/dev/null 
            fi
        done
    fi

    rm -rf /home/svsystems/mysql
    
    [ -f /etc/httpd/conf.d/dps_inmage.conf ] && rm -f /etc/httpd/conf.d/dps_inmage.conf
    [ -f /etc/apache2/vhosts.d/dps_inmage.conf ] && rm -f /etc/apache2/vhosts.d/dps_inmage.conf
    
    [ -f /etc/httpd/conf.d/php_inmage.conf ] && rm -f /etc/httpd/conf.d/php_inmage.conf
    [ -f /etc/apache2/vhosts.d/php_inmage.conf ] && rm -f /etc/apache2/vhosts.d/php_inmage.conf

    for httpd_conf_file in /etc/httpd/conf/httpd.conf.install_save /etc/httpd/conf/httpd.conf.upgrade_save
    do
        if [ -f $httpd_conf_file ]; then
            echo "FOUND: $httpd_conf_file"
            echo "This file was saved as a copy of the file httpd.conf at the time of installation of this product."
            echo "Restoring this file to its original name to regain the web-server settings in it..."
            mv $httpd_conf_file /etc/httpd/conf/httpd.conf
        fi
    done

    if [ -f ${PHP_INI}.install_save ]; then
        echo "FOUND: ${PHP_INI}.install_save"
        echo "This file was saved as a copy of the file $PHP_INI at the time of installation of this product."
        echo "Restoring this file to its original name to regain the php-related settings in it..."
        mv ${PHP_INI}.install_save $PHP_INI
    fi
    if [ -f /etc/my.cnf.install_save ]; then
        echo "FOUND: /etc/my.cnf.install_save"
        echo "This file was saved as a copy of the file my.cnf at the time of installation of this product."
        echo "Restoring this file to its original name to regain the php-related settings in it..."
        mv /etc/my.cnf.install_save /etc/my.cnf
    fi
    
    if /etc/init.d/$APACHE_SERVICE status 2>&1 | grep -q running ; then
        /etc/init.d/$APACHE_SERVICE restart 1>/dev/null 2>&1
    else
        /etc/init.d/$APACHE_SERVICE start 1>/dev/null 2>&1
    fi

    LOG "Writing CX_TYPE=2 in amethyst.conf"
    AddOrReplace "CX_TYPE" "/home/svsystems/etc/amethyst.conf" "2"

	DO 'echo "Starting the services..."'
	StartServer
    DO 'echo "The Uninstallation of CS is successful."'

elif [ "$cx_type_to_remove" = "PS" ]; then
    DO 'echo " WARNING: All replication pairs configured in this server will be deleted."'
    DO 'echo -n "Would you like to remove the Process Server?(Y|N) [default N]: "'
    read ans
    if [ -z "$ans" ]; then
        ans=N
    fi
    if [ "$ans" = "Y" -o "$ans" = "y" ]; then
        LOG "User Choice : ${ans}"
	else
        LOG "User Choice : ${ans}"
        exit 1
    fi
    
    DO 'echo "Stopping services..."'
    /etc/init.d/tmanagerd stop 2>/dev/null
	$CXPSCTL stop 2>>/dev/null
	chkconfig cxpsctl off 2>/dev/null

    # Unregister PS with CS
    perl ${install_dir}/bin/unregisterps.pl 2>&1

	cat /home/svsystems/etc/version | grep -q -i InMage
    # partner=`grep ^IN_APPLABEL /home/svsystems/etc/amethyst.conf | cut -d"=" -f2 | tr -d " " | tr -d '""'`
    if [ $? -eq 0 ]; then
        DO 'echo "Removing the PS rpm..."'        
        rpm -e --nodeps inmagePS >/dev/null 2>&1 
    fi

    LOG "Writing CX_TYPE=1 in amethyst.conf"
    AddOrReplace "CX_TYPE" "/home/svsystems/etc/amethyst.conf" "1"
    DO 'echo "Starting the services..."'
    StartServer
    DO 'echo "The Uninstallation of PS is successful."'    
fi
