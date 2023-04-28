#!/bin/sh
# Name: vsnap_functionality.sh
# Purpose:  To enable/disable vsnap functionality
###################################################

# FUNCTION : Carries out a command and logs output into a log
DO()
{
    eval "$1 2>> $LOG_FILE | tee -a $LOG_FILE"
}

# FUNCTION : Echoes a command into a log
LOG()
{
    echo "$1" >> $LOG_FILE
}

STOP_VXAGENT() {
    DO 'echo "Stopping VX agent service..."'
    ${VX_INSTALL_DIR}/bin/stop >> $LOG_FILE 2>&1
    if [ $? -ne 0 ]; then
        DO 'echo "----------------------------------------------------------------------"'
        DO 'echo "Could not stop volume agent service. Please try after sometime."'
        DO 'echo "----------------------------------------------------------------------"'
        LOG ''
        DO 'echo "Starting volume agent service..."'
        ${VX_INSTALL_DIR}/bin/start >> $LOG_FILE 2>&1
        LOG ''
        exit 1
    fi
}

START_VXAGENT() {
    DO 'echo "Starting VX agent service..."'
    ${VX_INSTALL_DIR}/bin/start >> $LOG_FILE 2>&1
}

MODIFY_DRSCOUT() {
    LOG "Set drscout.conf parameter VsnapDriverAvailable value to $1"
    cp -f ${VX_INSTALL_DIR}/etc/drscout.conf ${VX_INSTALL_DIR}/drscout.conf-vsnap-backup > /dev/null 2>&1
        
    sed -e "s/VsnapDriverAvailable=.*/VsnapDriverAvailable="$1"/g" ${VX_INSTALL_DIR}/etc/drscout.conf > ${VX_INSTALL_DIR}/etc/drscout.conf-modified
    mv ${VX_INSTALL_DIR}/etc/drscout.conf-modified ${VX_INSTALL_DIR}/etc/drscout.conf    
}

VSNAP_ENABLE() {
    # Check if linvsnap is already loaded or not    
    if genkex | grep -q linvsnap ; then
        LOG "Virtual volume driver is already loaded."
        MODIFY_DRSCOUT 1
    else
        STOP_VXAGENT
        DO 'echo "Virtual volume driver is not loaded. Attempting to load it, please wait..."'
        ${VX_INSTALL_DIR}/drivers/module load /usr/lib/drivers/linvsnap
        if [ $? -eq 0 ]; then
            echo
            DO 'echo "Loaded virtual volume driver successfully ..."'
        else    
            echo
            DO 'echo "Failed to load virtual volume driver. Aborting..."'
            START_VXAGENT
            exit 1
        fi      
        MODIFY_DRSCOUT  1        
        START_VXAGENT
    fi
    DO 'echo "vsnap functionality is enabled."'
}

VSNAP_DISABLE() {
    # Check if linvsnap is already loaded or not    
    if genkex | grep -q linvsnap ; then        
        LOG "Virtual volume driver is already loaded."
        STOP_VXAGENT        
        DO 'echo "Removing all existing vsnaps..."'
        LOG "Running the command : ${VX_INSTALL_DIR}/bin/cdpcli --vsnap --op=unmountall to remove all existing vsnaps."
        ${VX_INSTALL_DIR}/bin/cdpcli --vsnap --op=unmountall >> $LOG_FILE 2>&1
        if [ $? -ne 0 ]; then
            DO 'echo "Unable to remove all existing vsnaps. Aborting..."'           
            START_VXAGENT
            echo && exit 1
        fi        
        DO 'echo "Virtual volume driver is already loaded. Attempting to unload it, please wait..."'
        ${VX_INSTALL_DIR}/drivers/module unload /usr/lib/drivers/linvsnap 
        if [ $? -ne 0 ]; then
            DO 'echo "--------------------------------------------"'
            DO 'echo "Unable to unload the virtual volume driver. "'
            DO 'echo "--------------------------------------------"'
            LOG ''
            START_VXAGENT
            echo && exit 1
        fi            
        MODIFY_DRSCOUT 0        
        START_VXAGENT    
    else
        LOG "Virtual volume driver is already unloaded."
        MODIFY_DRSCOUT 0        
    fi
    DO 'echo "vsnap functionality is disabled."'
}    

##################
# MAIN ENTRY POINT
##################
LOG_FILE=/var/log/vsnap_functionality.log
echo "-------------------------------------------" >> ${LOG_FILE}
echo `date` >> ${LOG_FILE}
echo "-------------------------------------------" >> ${LOG_FILE}

if [ `uname` != 'AIX' ]; then
    DO 'echo "This script is intended only for AIX. Aborting..."'
    exit 1
fi

if [ ! -f /usr/local/.vx_version ]; then
    DO 'echo "Agent build does not exists."'
    exit 1
fi
VX_INSTALL_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`

case "$1" in
  enable)
        VSNAP_ENABLE
        ;;
  disable)
        VSNAP_DISABLE
        ;;
  *)
        echo "Usage: $0 {enable|disable}"
        exit 1
esac
