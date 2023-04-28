#!/bin/sh

if [ -z "$1" ]; then
    INST_LOG=/var/log/cx_install.log
else
    INST_LOG=$1
fi

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

# Check if CS or PS or both is installed in the setup
# If the CX_TYPE is 1, it is CS alone
# If the CX_TYPE is 2, it is PS alone
# If the CX_TYPE is 3, it is both the CS and PS

install_dir=/home/svsystems
cx_type=`cat ${install_dir}/etc/amethyst.conf | grep ^CX_TYPE | cut -d"=" -f2 | tr -d " " | tr -d '"'`

if [ -e /usr/local/.pillar ]; then
    cx_name="ReplicationEngine"
else
    cx_name="CX"
fi

case $cx_type in
    1)  if [ -e /usr/local/.pillar ]; then
            SERVER_STRING="ControlService"
        else
            SERVER_STRING="Configuration Server"
        fi
    ;;
    2)  if [ -e /usr/local/.pillar ]; then
            SERVER_STRING="ProcessService"
        else
            SERVER_STRING="Process Server"
        fi
    ;;

    3)  if [ -e /usr/local/.pillar ]; then
            SERVER_STRING="ReplicationEngine"
        else
            SERVER_STRING="Combined Configuration-Process Server"
        fi
    ;;
esac

#############################
# Prompt the user for the CX 
# web directory path
#############################

if [ "$cx_type" = "1" -o "$cx_type" = "3" ]; then
    DO 'echo'
    DO 'echo "================================================================================="'
    if [ -e /usr/local/.pillar ]; then
            DO 'echo "To improve ${cx_name} performance when replicating a large number of LUNs,"'
    else
            DO 'echo "To improve ${cx_name} performance when replicating a large number of pairs,"'
    fi
    DO 'echo "it is advised to have the Web server document root on a separate disk."'
    DO 'echo'
    DO 'echo "By default, the Web server document root is on the same disk as /home/svsystems"'
    DO 'echo "================================================================================="'
    DO 'echo'
    DO 'echo -n "Do you want to have the document root on a separate disk? [Y|N] [default N] : "'

    read ans

    while [ ! -z "$ans" -a "$ans" != "Y" -a "$ans" != "y" -a "$ans" != "N" -a "$ans" != "n" ]
    do
        DO 'echo -n "Invalid input.Do you want to have the document root on a separate disk? [Y|N] [default N] : "'
        read ans    
        if [ -z "$ans" ]; then
            ans=N
        fi
    done

    ##################################################################
    # Read the input from the user and create a folder web under this 
    # Link the existing /home/svsystems/admin/web to this web
    ##################################################################

    if [ "$ans" = "Y" -o "$ans" = "y" ] ; then
        echo
        while true
        do
            echo -n "Please specify the directory for the ${cx_name} web :"
            read web_input
            ([[ -n "$web_input" ]] && [[ "${web_input:0:1}" = "/" ]] ) && break || echo -e "Invalid directory. Try again.\n"
        done

        [[ ! -d "$web_input" ]] && mkdir -p $web_input

        echo "${cx_name} web split takes a while..."
        cp -prf ${install_dir}/admin/web ${web_input}/
        rm -rf ${install_dir}/admin/web
        ln -f -s "$web_input"/web ${install_dir}/admin/web

        # Stop mysql, copy mysql directory to webroot/mysql and create a softlink and start mysql
        /etc/init.d/mysqld stop
        cp -r ${install_dir}/mysql ${web_input}/
        rm -rf ${install_dir}/mysql
        ln -f -s "$web_input"/mysql ${install_dir}/mysql
        chmod -R 777 "$web_input"/mysql
        nice -n -2 /etc/init.d/mysqld start
        /etc/init.d/httpd restart 2>/dev/null
        [[ $? -ne 0 ]] && DO 'echo "$SERVER_STRING installation has failed!"'

        #########################################
        # Adding the entry to amethyst.conf file
        #########################################

        echo "WEB_SPLIT_DATA_DIR=$web_input" >> ${install_dir}/etc/amethyst.conf
        echo "${cx_name} web split has been done successfully ..."
        echo
    fi
fi

LOG '' && DO 'echo'
[[ ! -f /usr/local/.pillar ]] && DO 'echo "$SERVER_STRING installation is successful."'
