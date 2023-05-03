#!/usr/bin/env bash
#//--------------------------------------------------------------------
#//  <copyright file="install" company="Microsoft">
#//      Copyright (c) Microsoft Corporation. All rights reserved.
#//  </copyright>
#//
#//  <summary>
#//     This script uses to install linux agent
#//  </summary>
#//
#//--------------------------------------------------------------------

# exporting LANG variable
export LANG=C

# FUNCTION : Carries out a command and logs output into an uninstall log
#
# Function Name: trace_log_message()
# 
#  Description  Prints date and time followed by all arguments to 
#               stdout and to UNINST_LOG.  If -q is used, only send 
#               message to log file, not to stdout.  If no arguments 
#               are given, a blank line is returned to stdout and to 
#               UNINST_LOG.
#       
#  Usage        trace_log_message [-q] "Message"
#
#  Notes        If -q is used, it must be the first argument.
# 
trace_log_message()
{
    QUIET_MODE=FALSE

    if [ $# -gt 0 ]
    then
        if [ "X$1" = "X-q" ]
        then
            QUIET_MODE=TRUE
            shift
        fi
    fi

    if [ $# -gt 0 ]
    then
        DATE_TIME=`date '+%m/%d/%Y %H:%M:%S'`
        
        if [ "${QUIET_MODE}" = "TRUE" ]
        then
            echo "${DATE_TIME}: $*" >> ${UNINST_LOG}
        else            
            echo -e "$@"
            echo "${DATE_TIME} : $@ " >> ${UNINST_LOG} 2>&1            
        fi
    else
        if [ "${QUIET_MODE}" = "TRUE" ]
        then
            echo "" >> ${UNINST_LOG}
        else
            echo "" | tee -a ${UNINST_LOG}
        fi
    fi
}


# FUNCTION : Print message and quit
SEE_LOG()
{
 local exit_code=$1
 trace_log_message "Check the log file $UNINST_LOG for detailed diagnostic messages during uninstallation ..."
 [ -e $LOCK_FILE ] && rm -f $LOCK_FILE >/dev/null 2>&1
 exit $exit_code
}

Usage() {
    echo
    echo "Usage: $0 [ -Y ] [ -L <Absolute path of the log file (default:/var/log/ua_uninstall.log)> ] [ -h ]"
    echo && exit 1
}

Uninstall_FX()
{
  
  trace_log_message "Uninstalling the FX Agent ..."
  CURRENT_INSTALLATION_DIR=`grep INSTALLATION_DIR /usr/local/${FX_INST_VERSION_FILE} | cut -d'=' -f2-`
  cd $CURRENT_INSTALLATION_DIR
  ./uninstall -Y -L $UNINST_LOG 

}

Uninstall_VX()
{  
  trace_log_message "Uninstalling the VX agent ..."
  CURRENT_INSTALLATION_DIR=`grep INSTALLATION_DIR /usr/local/${INST_VERSION_FILE} | cut -d'=' -f2-`
  cd $CURRENT_INSTALLATION_DIR/bin
  ./uninstall -Y -L $UNINST_LOG 

}

StopServices() {
    local agent_name=$1
    local return_value=0
    case $agent_name in
        Vx)
            VX_INSTALL_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2-`
	    UA_INSTALL_DIR=`dirname ${VX_INSTALL_DIR}`
            # Stopping Vx agent before uninstalling agent            
            trace_log_message            
            trace_log_message "Stopping VX agent service..."
            touch /usr/local/.norespawn_vxagent >/dev/null 2>&1
            ${VX_INSTALL_DIR}/bin/stop uninstall >> ${UNINST_LOG} 2>&1
            if [ $? -eq 2 ]; then                
                trace_log_message               
                trace_log_message "Could not stop Vx agent service. Please re-try after sometime. Aborting..."
                rm -f /usr/local/.norespawn_vxagent >/dev/null 2>&1
                return_value=1
            fi
        ;;
        
        Fx)
            FX_INSTALL_DIR=`grep INSTALLATION_DIR /usr/local/.fx_version | cut -d'=' -f2-`
            # Stopping Fx agent before uninstalling agent            
            trace_log_message
            trace_log_message "Stopping Fx agent service..."
            touch /usr/local/.norespawn_svagent >/dev/null 2>&1
            ${FX_INSTALL_DIR}/stop >> ${UNINST_LOG} 2>&1
            if [ $? -eq 2 ]; then
                trace_log_message
                trace_log_message "Could not stop Fx agent service. Please re-try after sometime. Aborting..."
                rm -f /usr/local/.norespawn_svagent >/dev/null 2>&1
                return_value=1
            fi
        ;;
    esac    
    return $return_value
}

Check_FX_Presence()
{

  FX_INST_VERSION_FILE=.fx_version
  if [ -e /usr/local/${FX_INST_VERSION_FILE} ]; then
	FX_AGENT_FOUND=FX
  fi

}

Check_VX_Presence()
{

  INST_VERSION_FILE=.vx_version
  if [ -e /usr/local/${INST_VERSION_FILE} ]; then
  	VX_AGENT_FOUND=VX
  fi

}

#
#  Function     check_instance()
#
#  Description  Ensures that no other instances of this script are
#               running.  The results of more than one insctnace of
#               this script running simultaneously can produce
#               unexpected results.
#
#  Notes        If the LOCK_FILE exists, but the process ID does not
#               exist, the LOCK_FILE might be errant.  Manually
#               confirm that no other instances of the script are
#               running before deleting the LOCK_FILE.
#
#               The output of the ps -fp command will include a
#               header line.  If the number of lines returned from
#               the ps command is greater than 1, then the process
#               is running.
#
# Return Value: On successful it returns zero, else returns non-zero
#
check_instance()
{
    trace_log_message -q "Confirming availability through lock file ${LOCK_FILE}"

    if [ -f "${LOCK_FILE}" ]
    then
        prev_pid=`cat ${LOCK_FILE}`

        if [ `ps -fp ${prev_pid} | wc -l` -gt 1 ]
        then
            trace_log_message "Instance of ${SCRIPT} already running under the process:"
            ps -fp ${prev_pid} 2>&1 | tee -a ${UNINST_LOG}
            return_value=1
        else
            trace_log_message "Lock file ${LOCK_FILE} is in an inconsistent state"
            trace_log_message "Manually check for multiple instances of ${SCRIPT}"
            return_value=1
        fi
    else
        trace_log_message -q "Lock file ${LOCK_FILE} is free, locking file"
        echo $$ >> ${LOCK_FILE}
        nice sleep 1
        sync


        if [ `head -1 ${LOCK_FILE}` = $$ ]
        then
            trace_log_message -q "Lock file ${LOCK_FILE} is consistent"
        else
            trace_log_message "Lock file ${LOCK_FILE} is in an inconsistent state"
            trace_log_message "Manually check for multiple instances of ${SCRIPT}"
            return_value=1
        fi
    fi

    trace_log_message -q "Check instance return value: $return_value"
    return $return_value
}


#######################
# MAIN ENTRY POINT
#######################
OS="SET_OS_PLACE_HOLER"
SCRIPT_DIR=`dirname $0`
SCRIPT=`basename $0`
LOCK_FILE="${SCRIPT_DIR}/.uninstall.lck"

#// Using trap signal so that if we press Cntrl-C or if we quit the process,
#// we will remove the file uninstall_running
trap "rm -f $LOCK_FILE; exit" 1 2 3 6 9 15 30

# Prompt the user to confirm whether to proceed to uninstall the agent really
#Parse command line options if given
if echo $1 | grep -q '^-' ; then
    while getopts :L:hY OPT
    do
        case $OPT in
            L)  LOG_FILE="$OPTARG"
                if expr "$LOG_FILE" : '-.' > /dev/null; then
                    echo "Option -$OPT requires an argument." && Usage
                fi
            ;;
            Y)  SILENT_UNINSTALL="yes" ;;
            h|-*|*) Usage ;;
        esac
    done
elif [ "$1" = "" ]; then
    SILENT_UNINSTALL="no"
else
    Usage
fi

if  [ -z "$SILENT_UNINSTALL" -o "$SILENT_UNINSTALL" = "no" ] ; then
    # If no option is specified, then the default is interactive    
    echo
    agree=""
    while [ "X$agree" = "X" ]; do
        echo -n "Do you really want to uninstall the Unified Agent? (Y/N) [default N] : "
        read ans
        [ -z "$ans" ] && ans="N"
        case $ans in
            [Yy]) agree=Yes ; SILENT_UNINSTALL="no" ;;
            [Nn]) 
                [ -f $LOCK_FILE ] && rm -f $LOCK_FILE
                echo "Bye..." && exit 1 ;;
            *) echo -e "Error: Invalid input.\n"
            continue
            ;;
        esac
    done
fi

if [ -z "$LOG_FILE" ]; then 
    if [ ! -d /var/log ]; then 
       mkdir -p /var/log >/dev/null 2>&1
    fi
    UNINST_LOG="/var/log/ua_uninstall.log"
elif [ "`echo $LOG_FILE | cut -c1`" != "/" ]; then
    trace_log_message
    trace_log_message "Error: Unintallation logfile must be an absolute path (i.e. beginning with a \"/\")."
    Usage
else
    LOG_FILE_DIR=`dirname "$LOG_FILE"`
    if [ ! -d "$LOG_FILE_DIR" ]; then
        mkdir -p "$LOG_FILE_DIR" >/dev/null 2>&1
    fi
    UNINST_LOG="$LOG_FILE"
fi

trace_log_message -q "UA UNINSTALLATION"
trace_log_message -q "-----------------------------------"

Check_FX_Presence

Check_VX_Presence

if [ -z "$FX_AGENT_FOUND" -a -z "$VX_AGENT_FOUND" ]; then
    trace_log_message "Either FX or VX is not installed in the setup ..."
    trace_log_message "Cannot proceed with uninstallation ..."
    SEE_LOG 1
fi

# Make sure there are no multiple instances are running.
check_instance || SEE_LOG $?
    
if [ "$FX_AGENT_FOUND" = "FX" -a "$VX_AGENT_FOUND" = "VX" ]; then
    # Stopping FX/VX Agent Services
    # Aborting un-installation if it's failed to stop the services
    StopServices "Vx" || SEE_LOG 2
    StopServices "Fx" || SEE_LOG 2	

    Uninstall_VX
    Uninstall_FX

elif [ "$FX_AGENT_FOUND" = "FX" ]; then
    StopServices "Fx" || SEE_LOG 2
    Uninstall_FX

elif [ "$VX_AGENT_FOUND" = "VX" ]; then
    StopServices "Vx" || SEE_LOG 2
    Uninstall_VX
fi

# Specific to RHEL7 platform
if [ "$OS" = "RHEL7-64" -o "$OS" = "RHEL8-64"  -o "$OS" = "RHEL9-64" ]; then 
    #We need to run the following command after all services including involflt and 
    #all the legacy sysv-init service like vxagent, svagent etc have been disabled/removed.

    trace_log_message -q "Reload systemd manager configuration"
    systemctl daemon-reload >> ${UNINST_LOG} 2>&1
fi

# Removing fingerprint/certificate directories & other files
rm -rf /usr/local/InMage/certs /usr/local/InMage/fingerprints /usr/local/InMage/private >/dev/null 2>&1
rm -f /usr/local/unified_uninstall /usr/local/drscout.conf.lck  /usr/local/.norespawn_* >/dev/null 2>&1
rm -f /usr/local/InMage/license.txt /etc/.appliedmtcustomchanges  "${LOCK_FILE}" >/dev/null 2>&1
rm -f ${UA_INSTALL_DIR}/Third-Party_Notices.txt ${UA_INSTALL_DIR}/uninstall.sh ${UA_INSTALL_DIR}/.uninstall.lck >/dev/null 2>&1

trace_log_message -q "END OF UA UNINSTALLATION"
trace_log_message -q "************************"
SEE_LOG 0
