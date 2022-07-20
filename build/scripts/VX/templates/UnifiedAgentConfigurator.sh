#!/usr/bin/env bash
#//--------------------------------------------------------------------
#//  <copyright file="UnifiedAgentConfigurator.sh" company="Microsoft">
#//      Copyright (c) Microsoft Corporation. All rights reserved.
#//  </copyright>
#//
#//  <summary>
#//     This script uses to configure linux agent with CS or RCM service
#//  </summary>
#//
#//--------------------------------------------------------------------

#
#//--------------------------------------------------------------------
#
#  Global Variables:
#
#//--------------------------------------------------------------------
#

# exporting LANG variable
export LANG=C
export LC_NUMERIC=C

starttime=$(date -Iseconds)
export SETUP_TELEDIR='/var/log/ASRsetuptelemetry'
export UPLOADED_TELEDIR='/var/log/ASRsetuptelemetry_uploaded'
export json_file=$SETUP_TELEDIR/ASRconfigsummary_`date '+%Y_%m_%d_%H_%M_%S'`.json
export televerbose_file=$SETUP_TELEDIR/ASRconfigverbose_`date '+%Y_%m_%d_%H_%M_%S'`.txt
export installtype="Configuration"
export HOST_NAME=$(hostname)
export DRSCOUT_CONF=""
export CXPS_PORT="9443"
TEMP_SUMMARY_FILE="/tmp/asrconfigsummary.txt"
export INVOKER="CommandLine"
export passphrasevalidated=0
export exit_code=0
export RUNID=$(cat /proc/sys/kernel/random/uuid)

umask 0002
VX_VERSION_FILE="/usr/local/.vx_version"
DATE_TIME_STAMP=$(date  +'%d-%b-%Y--%T')
LOGFILE='/var/log/ua_install.log'
# Telemetry APIs which are common for install and 
# configurator references VERBOSE_LOGFILE
VERBOSE_LOGFILE="$LOGFILE"
SILENT_ACTION=""
SCRIPT=`basename $0`
AGENT_INSTALL_DIR=$(grep ^INSTALLATION_DIR $VX_VERSION_FILE | cut -d"=" -f2 | tr -d " ")
RCMCONFIG_FOLDER="/usr/local/InMage/config"
JQPATH="$AGENT_INSTALL_DIR/bin/jq"
source $AGENT_INSTALL_DIR/bin/teleAPIs.sh
DEFAULT_CONFIGURE_ERRORS_JSON='/var/log/InstallerErrors.json'

#ErrorCode
FailedWithErrors=97
SuccededWithWarnings=98
Sucess=0

# set_scenario and SetOP sets these values, must be set appropriately
# before calling RecordOP
# Note: Don't move these variables to common file teleAPIs.sh because
# in bash child can only read parent script variables
SCENARIO_NAME="PreConfiguration"
OP_NAME="ConfigurationStarted"

#
# Function Name: trace_log_message()
# 
#  Description  Prints date and time followed by all arguments to 
#               stdout and to LOG_FILE.  If -q is used, only send 
#               message to log file, not to stdout.  If no arguments 
#               are given, a blank line is returned to stdout and to 
#               LOG_FILE.
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
            echo "${DATE_TIME}: $*" >> ${LOGFILE}
        else            
            echo -e "$@"
            echo "${DATE_TIME} : $@ " >> ${LOGFILE} 2>&1            
        fi
    else
        if [ "${QUIET_MODE}" = "TRUE" ]
        then
            echo "" >> ${LOGFILE}
        else
            echo "" | tee -a ${LOGFILE}
        fi
    fi
}

process_configurator_exit_code()
{
    trace_log_message -q "ENTERED $FUNCNAME"

    local val="$1"

    # For CSPrime, configurator should return only 3 values : 0, 97, 98
    if [ "$CS_TYPE" = "CSPrime" ]; then
        if [ "$val" -ne "0" -a "$val" -ne "$SuccededWithWarnings" -a "$val" -ne "$FailedWithErrors" ]; then
            log_to_json_file ConfiguratorInternalError "Configurator failing with unknown error."
            val="$FailedWithErrors"
        fi

        if [ "$val" -eq "$FailedWithErrors" ]; then
            trace_log_message "Configurator failing with exit code : $val"
            trace_log_message "Please check $CONFIGURE_ERRORS_JSON file for more details."
        else
            trace_log_message "Configurator succeeding with exit code : $val"
            if [ "$val" -eq "$SuccededWithWarnings" ]; then
                trace_log_message "Please check $CONFIGURE_ERRORS_JSON file for more details."
            fi
        fi
    fi

    trace_log_message -q "EXITED $FUNCNAME"
    return $val
}

exit_with_retcode() {
    exit_code=$1
    trace_log_message "Check the log file $LOGFILE for detailed diagnostic messages or configuration success/failures..."

    process_configurator_exit_code "$exit_code"
    exit_code=$?

    if [ $exit_code = "0" ]; then
        EndSummaryOP "Success"
    else
        EndSummaryOP "Failed"
    fi
    trace_log_message "Configurator exiting with code: $exit_code"
    exit $exit_code
}

check_return_code()
{
    trace_log_message -q "ENTERED $FUNCNAME"
    local val="$1"

    if [ "$val" -ne "0" -a "$val" -ne "$SuccededWithWarnings" ]; then
        trace_log_message -q "EXITED $FUNCNAME"
        exit_with_retcode $val
    fi

    trace_log_message -q "EXITED $FUNCNAME"
    return $val
}

invoke_deamon_reload() {
    # Inovke systemctl daemon-reload command if systemctl exists on the system.
    which systemctl >/dev/null 2>&1
    if [ $? -eq 0 ];then
        trace_log_message -q "systemctl command exists. Invoking the following commnad - systemctl daemon-reload"
        systemctl daemon-reload >> $LOGFILE 2>&1
        if [ $? -eq 0 ];then
            trace_log_message -q "systemctl daemon-reload executed successfully."
        else
            trace_log_message -q "systemctl daemon-reload execution failed."
        fi
    else
       trace_log_message -q "systemctl command doesn't exist."
    fi
}

# FUNCTION: Creates softlinks for s2 and svagents binaries.
create_soft_links()
{
	cstype=$(grep ^CSType $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " " )
    for exe in ${AGENT_INSTALL_DIR}/bin/s2 ${AGENT_INSTALL_DIR}/bin/svagents
    do
        # Remove exe if it exists already.
        if [ -f ${exe} ] || [ -h ${exe} ] ; then
            trace_log_message -q "${exe} exists. Removing it."
            rm -f ${exe}
            if [ $? -eq 0 ]; then
                trace_log_message -q "Succesfully removed ${exe}."
            else
                trace_log_message "Failed to remove ${exe}."
                return 1
            fi
        else
            trace_log_message -q "${exe} doesn't exist."
        fi

        # Create softlink based on the VM platform.
        if [ z"${VM_PLATFORM}" = z"VmWare" ]; then
			if [ "$cstype" = "CSLegacy" ]; then
				ln -f -s ${exe}V2A ${exe} >> $LOGFILE 2>&1
				SLC_RET_VAL=$?
			else
				ln -f -s ${exe}A2A ${exe} >> $LOGFILE 2>&1
				SLC_RET_VAL=$?
			fi
        else
            trace_log_message -q "Invalid platform - ${VM_PLATFORM}."
            return 1
        fi

        if [ ${SLC_RET_VAL} -eq 0 ]; then
            trace_log_message -q "Successfully created softlink for ${exe}."
        else
            trace_log_message "Failed to create softlink for ${exe}. Exit code returned by ln: ${SLC_RET_VAL}."
            return 1
        fi
    done
    return 0
}

#
# Function Name: stop_agent_service
#
# Description:
#    This function stops the Unified agent (Vx) service.
#
# Parameters:None
#
# Return Value: On success it returns zero, else returns non-zero
#
# Exceptions:None
#
stop_agent_service() {
    local return_value=0
    SetOP "StopAgentService"
    trace_log_message "Stopping Vx agent service..."

    if list_contains "$SYSTEMCTL_DISTRO" "$OS"; then
        trace_log_message -q "Invoking following command to stop vxagent service - systemctl stop vxagent."
        systemctl stop vxagent >> $LOGFILE 2>&1
        SVAGENTS_PID=`pgrep svagents`
        APP_SERVICE_PID=`pgrep appservice`
        if [ ! -z "$SVAGENTS_PID" ] || [ ! -z "$APP_SERVICE_PID" ]; then
            trace_log_message -q "SVAGENT_PID: ${SVAGENTS_PID}, APP_SERVICE_PID: ${APP_SERVICE_PID}"
            return_value=2
        fi
    else
        trace_log_message -q "Invoking ${AGENT_INSTALL_DIR}/bin/stop script to stop vxagent service."
        ${AGENT_INSTALL_DIR}/bin/stop >> $LOGFILE 2>&1
        return_value=$?
    fi

    if [ $return_value -eq 2 ]; then
        trace_log_message -q "Passing force argument to stop script."
        ${AGENT_INSTALL_DIR}/bin/stop force >> $LOGFILE 2>&1
        return_value=$?
        if [ $return_value -eq 2 ]; then
            trace_log_message "Could not stop Vx agent service. Please re-try after sometime. Aborting..."
            return_value=115
        fi
    fi

    trace_log_message -q "stop agent service return value : $return_value"
    if [ $return_value -eq 0 ]; then
        RecordOP 0 ""
    else
        RecordOP $return_value "Couldn't stop VX agent service"
    fi
    return $return_value
}

# 
# Function Name: start_agent_service
# 
# Description:
#    This function starts the Unified agent (Vx) services. 
# 
# Parameters:None
# 
# Return Value: On success it returns zero, else returns non-zero
# 
# Exceptions:None
# 
start_agent_service() {
    local return_value=0    
    if [ "${AGENT_ROLE}" = "MasterTarget" ]; then
        SetOP "ScoutTuningExe"
        trace_log_message -q "Executing ${INSTALLATION_DIR}/bin/ScoutTuning -version \"${VERSION}\" -role \"vconmt\" command"
        ${INSTALLATION_DIR}/bin/ScoutTuning -version ${VERSION} -role vconmt >> $LOGFILE 2>&1
        RecordOP 0 ""
    fi

    SetOP "StartVxAgent"
    trace_log_message -q "Starting VX agent daemon..."

    if list_contains "$SYSTEMCTL_DISTRO" "$OS"; then
        chmod +x /etc/vxagent/bin/vxagent
        invoke_deamon_reload
        trace_log_message -q "Invoking following command to start vxagent service - systemctl start vxagent."
        systemctl start vxagent >> $LOGFILE 2>&1
        return_value=$?
        sleep 5 ;
        /etc/vxagent/bin/vxagent status >> $LOGFILE 2>&1
    else
        chmod +x /etc/init.d/vxagent
        trace_log_message -q "Invoking following command to start vxagent service - /etc/init.d/vxagent start."
        /etc/init.d/vxagent start >> $LOGFILE 2>&1
        return_value=$?
        /etc/init.d/vxagent status >> $LOGFILE 2>&1
    fi

    trace_log_message -q "Start vxagent service return value : $return_value"
    if [ $return_value -ne 0 ]; then
        trace_log_message -q "Failed to start vxagent service with exit code : 44"
        RecordOP 44 "Couldn't start VX agent service"
        stop_agent_service
        return 44
    fi
    RecordOP 0 ""
	
    echo "Starting UA Respawn daemon..."
    if list_contains "$SYSTEMCTL_DISTRO" "$OS"; then
        systemctl restart uarespawnd >> $LOGFILE 2>&1
    else
        /etc/init.d/uarespawnd restart >> $LOGFILE 2>&1
    fi
    return_value=$?
    RecordOP $return_value ""
    
    return $return_value
}

# 
# Function Name: set_permissions
# 
# Description:
#    This function sets restricted permissions
# 
# Parameters:None
# 
# Return Value: On successful it returns zero
# 
# Exceptions:None
# 
 set_permissions() {
    # Adding 770 permissions to main installation directory.
    trace_log_message -q "Adding 770 permissions to main installation directory."
    chmod -R 770 $INSTALLATION_DIR/../ >> $LOGFILE 2>&1
    chmod 700 $RCMINFO_CONF >> $LOGFILE 2>&1
    chmod 700 /usr/local/InMage/private/rcm* >> $LOGFILE 2>&1  
    chmod 700 /usr/local/InMage/certs/rcm* >> $LOGFILE 2>&1      
    
    # Setting write permissions for /proc/scsi_tgt/InMageEMD/InMageEMD /proc/scsi_tgt/groups/Default/devices
    [ -e /proc/scsi_tgt/InMageEMD/InMageEMD ] && \
    chmod 666 /proc/scsi_tgt/InMageEMD/InMageEMD /proc/scsi_tgt/groups/Default/devices

    return 0
}

# 
# Function Name: get_client_cert
# 
# Description:
#    This function used to extract the private & public keys from RcmVaultCreds certificate file
# 
# Parameters:None
# 
# Return Value: On successful it returns zero, else returns non-zero
# 
# Exceptions:None
# 
get_client_cert() {
    local return_value=0
    local pfx_cert_file='/usr/local/InMage/private/rcm.pfx'    
    local public_cert_file='/usr/local/InMage/certs/rcm.crt'      
    SetOP "GetAuthCertificate"
    local errorstring=""

    # Create a pfx certification file from encoded value
    cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .aadDetails.authCertificate | base64 -d > $pfx_cert_file
    
    # Extract and create public certificate and private key files
    $INSTALLATION_DIR/bin/gencert -x $pfx_cert_file | sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' > $public_cert_file     
    
    # Generates fingerprint from public certificate
    if [ -s "$public_cert_file" ]; then 
        trace_log_message -q "Generating fingerprint value from public certificate"
        FINGERPRINT=$(openssl x509 -inform PEM -in ${public_cert_file} -fingerprint -noout | cut -d= -f2|tr -d ":")
        if [ -z "$FINGERPRINT" ]; then
            trace_log_message "Failed to fetch fingerprint value from $public_cert_file file."
            errorstring="Failed to fetch fingerprint"
            return_value=1
        fi
    else
        trace_log_message "Failed to fetch public certificate from $pfx_cert_file file"
        errorstring="Failed to fetch public certificate"
        return_value=1
    fi
    
    trace_log_message -q "Get client certificate return value: $return_value"
    RecordOP $return_value $errorstring
    return $return_value        
}

process_azurercmcli_exitcode()
{
    trace_log_message -q "ENTERED $FUNCNAME"

    local cli_val="$1"
    local ret_val="$cli_val"
    if [ "$cli_val" -eq "2" ]; then
        ret_val="$FailedWithErrors"
    elif [ "$cli_val" -eq "1" ]; then
        ret_val="$SuccededWithWarnings"
    fi

    trace_log_message -q "EXITED $FUNCNAME"
    return $ret_val
}

#
# Function Name: update_config_general
#
# Description:
#    This function is used to update settings in a conf file
#
# Parameters:filename, key, val
#
# Return Value: On successful it returns zero, else returns non-zero
#
# Exceptions:None
#
update_config_general()
{
    trace_log_message -q "ENTERED $FUNCNAME"

    if [ "$#" -ne "3" ]; then
        trace_log_message -q "Invalid number of arguments"
        return 1
    fi
    local file_name="$1"
    local key="$2"
    local val="$3"
    trace_log_message -q "Updating $key in $file_name"
    sed -i -e "/^${key}/d; /^\[vxagent\]/ a ${key}=$val" $file_name >> $LOGFILE 2>&1
    local ret=$?
    if [ "$ret" -ne 0 ]; then
        trace_log_message -q "Failed to update config file : $file_name"
    fi

    trace_log_message -q "EXITED $FUNCNAME"
    return $ret
}

update_discovery()
{
    trace_log_message -q "ENTERED $FUNCNAME"
    local ret=0

    echo "$IS_CREDENTIAL_LESS_DISCOVERY" | grep -qi "true" && IS_CREDENTIAL_LESS_DISCOVERY=1
    echo "$IS_CREDENTIAL_LESS_DISCOVERY" | grep -qi "false" && IS_CREDENTIAL_LESS_DISCOVERY=0
    local disovery_from_drscout=`cat $DRSCOUT_CONF | grep "IsCredentialLessDiscovery" | cut -f2 -d"="`

    local fin_val=""
    if [ -z "$IS_CREDENTIAL_LESS_DISCOVERY" ]; then
        if [ -z "$disover_from_drscout" ]; then
            fin_val=0
        else
            fin_val="$disover_from_drscout"
        fi
    elif [ "$IS_CREDENTIAL_LESS_DISCOVERY" -eq "0" ]; then
        if [ "$disover_from_drscout" = "1" ]; then
            trace_log_message -q "Change from credential-less discovery to credential-based discovery is not supported."
            trace_log_message -q "drscout val : $disovery_from_drscout, param passed in configurator : $IS_CREDENTIAL_LESS_DISCOVERY"
            ret=1
        else
            fin_val=0
        fi
    elif [ "$IS_CREDENTIAL_LESS_DISCOVERY" -eq "1" ]; then
        if [ "$disover_from_drscout" = "0" ]; then
            trace_log_message -q "Change from credential-based discovery to credential-less discovery is not supported."
            trace_log_message -q "drscout val : $disovery_from_drscout, param passed in configurator : $IS_CREDENTIAL_LESS_DISCOVERY"
            ret=1
        else
            fin_val=1
        fi
    else
        trace_log_message -q "Invalid value for credential less discovery flag : $IS_CREDENTIAL_LESS_DISCOVERY"
        ret=1
    fi

    if [ "$ret" -eq "0" ]; then
        update_config_general "$DRSCOUT_CONF" "IsCredentialLessDiscovery" "$fin_val"
        ret=$?
    fi

    trace_log_message -q "EXITED $FUNCNAME"
    return $ret
}

rcm_exec_with_common_params()
{
    trace_log_message -q "ENTERED $FUNCNAME"
    local ret=0

    local params=("$@")
    trace_log_message -q "Params passed : ${params[@]}"
    params+=("--errorfilepath" "$CONFIGURE_ERRORS_JSON")
    if [ "$CS_TYPE" = "CSPrime" ]; then
        params+=("--logfile" "$LOGFILE")
    fi

    trace_log_message -q "--------------------------------------------------------"
    trace_log_message -q "Executing the command : ${INSTALLATION_DIR}/bin/AzureRcmCli ${params[@]}"
    ${INSTALLATION_DIR}/bin/AzureRcmCli "${params[@]}" >> $LOGFILE 2>&1
    ret=$?
    trace_log_message -q "AzureRcmCli operation exit code : $ret"

    trace_log_message -q "EXITED $FUNCNAME"
    return $ret
}

#
# Function Name: unregister_agent
#
# Description:
#    Unregisters source agent and machine with RCM server. Proceeds ahead even if they fails.
#	 It also invokes AzureRcmCli --cleanup operation. 
#
# Return Value: Returns non-zero exit code when AzureRcmCli cleanup operation fails, otherwise returns zero.
unregister_agent() {
    # Unregister source agent.
    SetOP "UnregisterSourceAgent"
    trace_log_message "Unregistering source agent with the RCM Server."
    local params=("--unregistersourceagent")
    rcm_exec_with_common_params "${params[@]}"
    UNREGAGENT_RETVAL=$?
    trace_log_message -q "AzureRcmCli --unregistersourceagent exit code : $UNREGAGENT_RETVAL"

    if [ $UNREGAGENT_RETVAL -eq 0 ] || [ $UNREGAGENT_RETVAL -eq 144 ]; then
        trace_log_message "Successfully unregistered source agent with the RCM Server."
    else
        trace_log_message "Failed to unregister source agent with the RCM Server."
    fi

	if [ $UNREGAGENT_RETVAL -eq 0 ] || [ $UNREGAGENT_RETVAL -eq 144 ]; then
        RecordOP 0 ""
    else
        RecordOP $UNREGAGENT_RETVAL "Failed to unregister source agent with RCM"
    fi

    # Unregister machine.
    SetOP "UnregisterMachine"
    trace_log_message "Unregistering machine  with the RCM Server."
    local params=("--unregistermachine")
    rcm_exec_with_common_params "${params[@]}"
    UNREGMACHINE_RETVAL=$?
    trace_log_message -q "AzureRcmCli --unregistermachine exit code : $UNREGMACHINE_RETVAL"

    if [ $UNREGMACHINE_RETVAL -eq 0 ] || [ $UNREGMACHINE_RETVAL -eq 144 ]; then
        trace_log_message "Successfully unregistered machine with the RCM Server."
    else
        trace_log_message "Failed to unregister machine with the RCM Server."
    fi

    if [ $UNREGMACHINE_RETVAL -eq 0 ] || [ $UNREGMACHINE_RETVAL -eq 144 ]; then
        RecordOP 0 ""
    else
        RecordOP $UNREGMACHINE_RETVAL "Failed to unregister machine with RCM"
    fi

    # AzureRcmCli cleanup.
    SetOP "AzureRcmCliCleanup"
    local params=("--cleanup")
    rcm_exec_with_common_params "${params[@]}"
    CLEANUP_RETVAL=$?
	trace_log_message -q "AzureRcmCli --cleanup exit code: $CLEANUP_RETVAL"

    if [ $CLEANUP_RETVAL -eq 0 ]; then
        RecordOP 0 ""
        trace_log_message -q "AzureRcmCli --cleanup has succeeded."
    else
        RecordOP $CLEANUP_RETVAL "AzureRcmCli cleanup has failed"
        trace_log_message -q "AzureRcmCli --cleanup has failed."
    fi

    if [ $CLEANUP_RETVAL -eq 2 ]; then
        CLEANUP_RETVAL=$FailedWithErrors
    fi
    return $CLEANUP_RETVAL
}

# 
# Function Name : byteswapped_biosid
#
# Description:
#		This function converts the biosid to byteswapped bios id
#
# Parameters : $original_bios_id, the bios id of the system
#
# Return value: None
#
# Exceptions : None
#
byteswapped_biosid() {
	local original_bios_id="$1"
	
	words=([0]=${original_bios_id:0:2} [1]=${original_bios_id:2:2} [2]=${original_bios_id:4:2} [3]=${original_bios_id:6:2} [4]=${original_bios_id:9:2} [5]=${original_bios_id:11:2} [6]=${original_bios_id:14:2} [7]=${original_bios_id:16:2})
    BYTESWAPPED_BIOSID="${words[3]}${words[2]}${words[1]}${words[0]}-${words[5]}${words[4]}-${words[7]}${words[6]}-${original_bios_id:19:17}"
}


# 
# Function Name: parse_update_rcm_creds
# 
# Description:
#    This function used to parse RCM Creds file and updates RCMInfo.conf
# 
# Parameters:$rcmcreds_file
# 
# Return Value: On successful it returns zero, else returns non-zero
# 
# Exceptions:None
# 
parse_update_rcm_creds() {
    local rcmcreds_file="$1"
    local return_value=0

    SetOP "ParseRCMcreds"

    # Parse RCM management details and update RCMInfo.conf
    RCM_URI=$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .rcmUri)
    RCM_MGMT_ID=$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .managementId)
    RCM_CLIREQ_ID=$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .clientRequestId)
    RCM_ACTIVITY_ID=$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .activityId)
	
	# On RHEL5, the /sys interface is not available so using "/usr/sbin/dmidecode -s system-uuid" command to get the BIOS-ID
	if [ -e /sys/class/dmi/id/product_uuid ]; then
		SYSTEM_BIOS_ID=$(cat /sys/class/dmi/id/product_uuid)
	else
		SYSTEM_BIOS_ID=$(/usr/sbin/dmidecode -s system-uuid)
	fi
    SERVICE_CONNECTION_MODE="direct"

   #Check if END_POINT_TYPE_NAME is found in the RCMInfo.conf file or not
   END_POINT_TYPE_NAME=$(grep ^EndPointType $RCMINFO_CONF | cut -d"=" -f1 | tr -d " ")

   #If EndPointType is not found, then add entry in RCMInfo.conf file
    if [ -z "$END_POINT_TYPE_NAME" ]; then
        sed -i '/\[rcm.auth\]/i EndPointType=' $RCMINFO_CONF
    fi

   #Read the EndPointType property obtained from creds file from SRS (coming as vmNetworkTypeBasedOnPrivateEndpoint)
   END_POINT_TYPE_VALUE=$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .vmNetworkTypeBasedOnPrivateEndpoint)
   if [ "$END_POINT_TYPE_VALUE" = "null" ]; then
         END_POINT_TYPE_VALUE="Public"
   fi

    # Clean up existing settings and generate new HostId if ManagementId in RCMCredentials file is different from RCMInfo.conf.
    RCM_CONF_MGMT_ID=$(grep ^ManagementId $RCMINFO_CONF | cut -d"=" -f2 | tr -d " ")
	RCM_CONF_BIOS_ID=$(grep ^BiosId $RCMINFO_CONF | cut -d"=" -f2 | tr -d " ")

    trace_log_message -q "RCM_MGMT_ID=$RCM_MGMT_ID"
    trace_log_message -q "RCM_CONF_MGMT_ID=$RCM_CONF_MGMT_ID"
	trace_log_message -q "RCM_CONF_BIOS_ID=$RCM_CONF_BIOS_ID"
	trace_log_message -q "SYSTEM_BIOS_ID=$SYSTEM_BIOS_ID"
    trace_log_message -q "END_POINT_TYPE_VALUE=$END_POINT_TYPE_VALUE"	
	BYTESWAPPED_BIOSID=$SYSTEM_BIOS_ID
	byteswapped_biosid "$SYSTEM_BIOS_ID"
	
	if [ -n "$RCM_CONF_MGMT_ID" ]; then
		if [[ $(tr [A-Z] [a-z] <<<"$RCM_MGMT_ID") != $(tr [A-Z] [a-z] <<<"$RCM_CONF_MGMT_ID") ]]; then
			trace_log_message -q "Invoking unregister_agent as RCM_MGMT_ID and RCM_CONF_MGMT_ID are different."
			unregister_agent || exit_with_retcode $?

			trace_log_message -q "Generating new HostId as ManagementId in RCMCredentials file is different from RCMInfo.conf"
			generate_host_id "true"
		else
			SetOP "ValidateBiosId"
			if [ $(tr [a-z] [A-Z] <<<"$SYSTEM_BIOS_ID") != $(tr [a-z] [A-Z] <<<"$RCM_CONF_BIOS_ID") ] && [ $(tr [a-z] [A-Z] <<<"$BYTESWAPPED_BIOSID") != $(tr [a-z] [A-Z] <<<"$RCM_CONF_BIOS_ID") ]; then
				trace_log_message -q "BiosId of the machine and biosId in the RCM Config are different (and) ManagementID in RCM Credentials file and RCM Conf file are same."
				RecordOP 118 "BiosId mismatch found"
				return 118
			fi
			RecordOP 0 ""
		fi
	else
		trace_log_message -q "Generating new hostId as ManagementId in RCM Conf is null."
		generate_host_id "true"
    fi

    # Clean up existing settting when ManagementId in RCMCredentials file is empty and PLATFORM_CHANGE value in drscout.conf is VmWareToAzure.
    PLATFORM_CHANGE=$(grep "^PlatformChange=" $INSTALLATION_DIR/etc/drscout.conf | cut -d= -f2)
    if [ -z "$RCM_CONF_MGMT_ID" ] && [ "${PLATFORM_CHANGE}" = "VmWareToAzure" ]; then
        trace_log_message -q "Invoking unregister_agent as RCM_MGMT_ID is empty and PLATFORM_CHANGE value is VmWareToAzure."
        unregister_agent || exit_with_retcode $?
    fi
    
    # Update [rcm] section in RCMInfo.conf file
    sed -i -e "s|ServiceUri=.*|ServiceUri=$RCM_URI|g; s|ManagementId=.*|ManagementId=$RCM_MGMT_ID|g; \
	s|ClientRequestId=.*|ClientRequestId=$RCM_CLIREQ_ID|g; s|ActivityId=.*|ActivityId=$RCM_ACTIVITY_ID|g; \
        s|BiosId=.*|BiosId=$SYSTEM_BIOS_ID|g; s|EndPointType=.*|EndPointType=$END_POINT_TYPE_VALUE|g; \
        s|ServiceConnectionMode=.*|ServiceConnectionMode=$SERVICE_CONNECTION_MODE|g" $RCMINFO_CONF >> $LOGFILE 2>&1
    
    # Parse Authentication details and update RCMInfo.conf
    get_client_cert || return $? # Get AADAuthCert from rcmCred's pfx certificate
    AAD_AUTH_CERTIFICATE=$FINGERPRINT
    AAD_SERVICE_URI=$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .aadDetails.serviceUri)
    AAD_TENANT_ID=$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .aadDetails.tenantId)
    AAD_CLIENT_ID=$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .aadDetails.clientId)
    AAD_AAUDIENCE_URI=$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .aadDetails.audienceUri)
    
    #Update [rcm.auth] section in RCMInfo.conf file
    sed -i -e "s|AADUri=.*|AADUri=$AAD_SERVICE_URI|g; s|AADAuthCert=.*|AADAuthCert=$AAD_AUTH_CERTIFICATE|g;\
        s|AADTenantId=.*|AADTenantId=$AAD_TENANT_ID|g; s|AADClientId=.*|AADClientId=$AAD_CLIENT_ID|g;\
        s|AADAudienceUri=.*|AADAudienceUri=$AAD_AAUDIENCE_URI|g" $RCMINFO_CONF  >> $LOGFILE 2>&1
    
    # Parse Relay details and update RCMInfo.conf
    if $(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -e 'has("relayDetails")' >/dev/null 2>&1) && \
        [ "$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .relayDetails)" != "null" ]
    then
        RELAY_KEY_NAME=$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .relayDetails.relayKeyPolicyName)
        RELAY_SHARED_KEY=$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .relayDetails.relaySharedKey)
        RELAY_SERVICE_PATH_SUFFIX=$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .relayDetails.relayServicePathSuffix)
        EXPIRY_TIMEOUT_INSECONDS=$(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -r .relayDetails.expiryTimeoutInSeconds)
        SERVICE_CONNECTION_MODE="relay"
    
        #Update [rcm.relay] section in RCMInfo.conf file
        sed -i -e "s|RelayKeyPolicyName=.*|RelayKeyPolicyName=$RELAY_KEY_NAME|g; s|RelaySharedKey=.*|RelaySharedKey=$RELAY_SHARED_KEY|g;\
            s|RelayServicePathSuffix=.*|RelayServicePathSuffix=$RELAY_SERVICE_PATH_SUFFIX|g;\
            s|ExpiryTimeoutInSeconds=.*|ExpiryTimeoutInSeconds=$EXPIRY_TIMEOUT_INSECONDS|g;\
            s|ServiceConnectionMode=.*|ServiceConnectionMode=$SERVICE_CONNECTION_MODE|g" $RCMINFO_CONF  >> $LOGFILE 2>&1
    fi
    
    sed -i -e "s|^RcmSettingsPath=.*|RcmSettingsPath=$RCMINFO_CONF|g" $DRSCOUT_CONF >> $LOGFILE 2>&1
    sed -i -e "s/null//g" $RCMINFO_CONF
	
	rcmConfContent=`cat $RCMINFO_CONF`
	trace_log_message -q "Rcm info.conf contents are : $rcmConfContent"
    
    trace_log_message -q "RCM Creds file parse and update RCMInfo.conf return value: $return_value"
    if [ $return_value -eq 0 ]; then
        RecordOP 0 ""
    else
        RecordOP $return_value "Parse and update RCM creds failed"
    fi
    return $return_value
}

#
# Function Name: generate_host_id
#
# Description:
#    This function will fetch hostid from drscout.conf
#    New hostId is generated only in case of below conditions. Otherwise, existing HostId in drscout.conf is used.
#    1) gen_host_id flag is set to true
#    2) HostId value in drscout.conf is either empty or default value(1234_45466_67689).
#
# Parameters:gen_host_id
#
# Return Value:None
#
# Exceptions:None
#
generate_host_id()
{
	local gen_host_id="$1"
	SetOP "GenerateHostId"
	
	if [ "$gen_host_id" = "true" ]; then
		trace_log_message -q "Generating new HostId"
		UNIQUE_HOST_ID=$(cat /proc/sys/kernel/random/uuid)
	else
		DRSCOUT_HOST_ID=$(grep ^HostId $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " ")
		trace_log_message -q "HostId value in $DRSCOUT_CONF : $DRSCOUT_HOST_ID"
		if [ ! -z "$DRSCOUT_HOST_ID" ] && [ "$DRSCOUT_HOST_ID" != "1234_45466_67689" ]
		then
			trace_log_message -q "HostId in DRSCOUT Config is not null/default value. Using existing hostId."
			UNIQUE_HOST_ID=$DRSCOUT_HOST_ID
		else
			trace_log_message -q "HostId in DRSCOUT Config is either null or default value. Generating new hostId."
			UNIQUE_HOST_ID=$(cat /proc/sys/kernel/random/uuid)
		fi
	fi
	
	trace_log_message -q "Writing HostId=${UNIQUE_HOST_ID} to $DRSCOUT_CONF"
	sed -i -e "s|^HostId.*|HostId=${UNIQUE_HOST_ID}|g" $DRSCOUT_CONF >> $LOGFILE 2>&1
	
	RecordOP 0 "Generated host ID $UNIQUE_HOST_ID"
}

update_drscout_config()
{
	generate_host_id "false"

        SetOP "UpdatingDrscout"
		# On RHEL5, the /sys interface is not available so using "/usr/sbin/dmidecode -s system-uuid" command to get the BIOS-ID
		# Same logic is used for getting BiosID in portablehelpersmajor.cpp GetSystemUUID() function as well
		# Do not change this order
		if [ -e /sys/class/dmi/id/product_uuid ]; then
			BIOS_ID=$(cat /sys/class/dmi/id/product_uuid)
		elif [ -x /usr/sbin/dmidecode ]; then
	        BIOS_ID=$(/usr/sbin/dmidecode -s system-uuid)
		fi
		if [ ! -e "$RCMINFO_CONF" ]; then 
			touch $RCMINFO_CONF
			echo "[rcm]" >> $RCMINFO_CONF
		fi
		
		grep -i "BiosId" $RCMINFO_CONF >> $LOGFILE 2>&1
		if [ $? -eq 0 ]; then
			trace_log_message "Updating bios id in RCMInfo.conf"
		    sed -i -e "s|^BiosId.*|BiosId=$BIOS_ID|g" $RCMINFO_CONF >> $LOGFILE 2>&1
		else
			trace_log_message "Writing new bios id in RCMInfo.conf"
		    sed -i -e "/rcm/ a BiosId=$BIOS_ID" $RCMINFO_CONF >> $LOGFILE 2>&1
		fi
                
                if [ ! -z "$CLIENT_REQUEST_ID" ]; then
			trace_log_message "Updating client request id in RCMInfo.conf"
                        sed -i -e "s|^ClientRequestId.*|ClientRequestId=$CLIENT_REQUEST_ID|g" $RCMINFO_CONF >> $LOGFILE 2>&1
		fi
		
		trace_log_message -q "Writing RcmInfo path to $DRSCOUT_CONF"
		sed -i -e "/^RcmSettingsPath/d; /^HostId/ a RcmSettingsPath=$RCMINFO_CONF" $DRSCOUT_CONF >> $LOGFILE 2>&1

	return 0
}

update_drscout_config_for_cs()
{
	trace_log_message -q "Writing Hostname = $CS_IP_ADDRESS to $DRSCOUT_CONF"
	trace_log_message -q "Writing Port = $CS_PORT_NUMBER to $DRSCOUT_CONF"
    sed -i -e "s|^Hostname.*|Hostname = $CS_IP_ADDRESS|g; s|^Port.*|Port = $CS_PORT_NUMBER|g" $DRSCOUT_CONF

	if [ -n "$EXTERNAL_IP" ]; then
	    trace_log_message -q "Writing ExternalIpAddress=$EXTERNAL_IP to $DRSCOUT_CONF"
		grep -i "ExternalIpAddress" $DRSCOUT_CONF >> $LOGFILE 2>&1
		if [ $? -eq 0 ]; then
		    sed -i -e "s|^ExternalIpAddress.*|ExternalIpAddress=$EXTERNAL_IP|g" $DRSCOUT_CONF >> $LOGFILE 2>&1
		else
		    sed -i -e "/^HostId/ a ExternalIpAddress=$EXTERNAL_IP" $DRSCOUT_CONF >> $LOGFILE 2>&1
		fi
	fi
	
	return 0
}

# 
# Function Name: cs_agent_registration
# 
# Description:
#    This function used to invoke cdpcli command to register host with V2A CS
# 
# Parameters:$agent
# 
# Return Value: On successful it returns zero, else returns non-zero
# 
# Exceptions:None
# 
cs_agent_registration()
{
    local agent="$1"
    local count=1

    case "$agent" in
        Vx)
            cstype=$(grep ^CSType $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " " )

            if [ "${AGENT_ROLE}" = "MasterTarget" -o "$cstype" = "CSLegacy" ]; then
                # Making an explicit agent registration call using ${INSTALLATION_DIR}/bin/cdpcli --registerhost command
                trace_log_message -q ""
                trace_log_message "\nInvoking an agent registration call..."
                mv -f /var/log/cdpcli.log /var/log/cdpcli.log.$RANDOM >/dev/null 2>&1

                while [ $count -le 3 ]; do
                    trace_log_message -q "Trying to make an agent registration call - Count: $count "
                    trace_log_message -q "----------------------------------------------------------"
                    trace_log_message -q "Executing the command : ${INSTALLATION_DIR}/bin/cdpcli --registerhost --loglevel=7"
                    ${INSTALLATION_DIR}/bin/cdpcli --registerhost --loglevel=7 >> $LOGFILE 2>&1
                    REGHOST_RETVAL=$?
                    if [ $REGHOST_RETVAL -ne 0 ]; then
                        trace_log_message -q ""
                        trace_log_message -q "Failed $count attempt of agent registration call."
                        count=$(expr $count + 1)
                        sleep 30 ; continue
                    fi
                    break
                done

                # Appending /var/log/cdpcli.log contents to agent installation log
                trace_log_message -q ""
                trace_log_message -q "****** Beginning of  cdpcli --registerhost --loglevel=7 detailed command output **********"
                cat /var/log/cdpcli.log >> $LOGFILE 2>&1
                trace_log_message -q ""
                trace_log_message -q "****** End of cdpcli --registerhost --loglevel=7 detailed command output ********"

                if [ $REGHOST_RETVAL -ne 0 ]; then
                    trace_log_message
                    trace_log_message "Agent registration has failed."
                    trace_log_message -q "Writing AGENT_CONFIGURATION_STATUS=Failed in $VX_VERSION_FILE"
                    echo "AGENT_CONFIGURATION_STATUS=Failed" >> "$VX_VERSION_FILE"

                    trace_log_message -q "Replacing Hostname value to null in $DRSCOUT_CONF"
                    sed -i -e "s|^Hostname.*|Hostname = |g" $DRSCOUT_CONF

                    trace_log_message -q "Removing ExternalIpAddress value from $DRSCOUT_CONF"
                    sed -i -e "/^ExternalIpAddress/d" $DRSCOUT_CONF >> $LOGFILE 2>&1
                    RecordOP $REGHOST_RETVAL "Agent registration failed"

                    return 17
                fi

                trace_log_message "Agent registration has completed successfully."
                trace_log_message -q "Writing AGENT_CONFIGURATION_STATUS=Succeeded in $VX_VERSION_FILE"
                echo "AGENT_CONFIGURATION_STATUS=Succeeded" >> "$VX_VERSION_FILE"

                cp $DRSCOUT_CONF ${INSTALLATION_DIR}/drscout.conf.initialbackup
                RecordOP 0 ""
            else
                rcm_agent_registration
                regexitcode=$?
                if [ $regexitcode -eq 119 ]; then
                    regexitcode=$FailedWithErrors
                    log_to_json_file ASRMobilityServiceConfiguratorInternalError "AzureRcmCli failing with unknown error."
                fi
                if [ $regexitcode -ne 0 ]; then
                    return $regexitcode
                fi

                get_non_cached_settings
                return $?
            fi
            ;;
    esac
    return 0
 }

rcm_register_machine()
{
    trace_log_message -q "ENTERED $FUNCNAME"
    local exitcode=1
    local count=0

    # Perform AzureRcmCli registermachine operation
    while [ $count -le 2 ]; do
        trace_log_message -q "AzureRcmCli registermachine operation - iteration:$count"
        local params=("--registermachine")
        rcm_exec_with_common_params "${params[@]}"
        exitcode=$?
        if [ $exitcode -eq 0 ]; then
            trace_log_message -q ""
            trace_log_message -q "registermachine operation has succeeded."
            break
		elif [ $exitcode -eq 129 -a "$VM_PLATFORM" != "Azure" ]; then
                trace_log_message -q "Rcm proxy not configured with rcm".
				sed -i -e "/^CloudPairingStatus/d; /^HostId/ a CloudPairingStatus=Incomplete" $DRSCOUT_CONF >> $LOGFILE 2>&1
                break
        else
            trace_log_message -q ""
            trace_log_message -q "registermachine operation has failed with exit code - $exitcode"
            if [ $exitcode -eq 126 ]; then
                break
            fi
            count=$(expr $count + 1)
            sleep 5 ; continue
        fi
    done
    trace_log_message -q "EXITED $FUNCNAME"
    return $exitcode
}

rcm_register_sourceagent()
{
    trace_log_message -q "ENTERED $FUNCNAME"
    local exitcode=1
    local count=0

    while [ $count -le 2 ]; do
        trace_log_message -q "AzureRcmCli registersourceagent operation - iteration:$count"
        local params=("--registersourceagent")
        rcm_exec_with_common_params "${params[@]}"
        exitcode=$?
        if [ $exitcode -eq 0 ]; then
            trace_log_message -q ""
            trace_log_message -q "registersourceagent operation has succeeded."
            break
        else
            trace_log_message -q ""
            trace_log_message -q "registersourceagent operation has failed with exit code - $exitcode"
            if [ $exitcode -eq 126 ]; then
                break
            fi
            count=$(expr $count + 1)
            sleep 5 ; continue
        fi
    done
    trace_log_message -q "EXITED $FUNCNAME"
    return $exitcode
}

#
# Function Name: rcm_agent_registration
#
# Description:
#    This function used to invoke AzureRcmCli commands to register host with RCM service
#
# Parameters:None
#
# Return Value: Returns zero when both registermachine and registersourceagent operations succeeds.
#
# Exceptions:None
#
rcm_agent_registration()
{
    local regmachineexitcode=1
    local regsourceexitcode=1
    local regexitcode=1
    local count=0

    SetOP "RegisterAgent"

    rcm_register_machine
    regmachineexitcode=$?

    # Perform AzureRcmCli registersourceagent operation only when registermachine succeeds.
    if [ $regmachineexitcode -eq 0 ]; then
        rcm_register_sourceagent
        regsourceexitcode=$?
    else
        trace_log_message -q ""
        trace_log_message -q "Skipping registersourceagent operation."
    fi

    trace_log_message -q ""
    trace_log_message -q "****** Beginning of RCMInfo.conf file content **********"
    cat $RCMINFO_CONF >> $LOGFILE 2>&1
    trace_log_message -q ""
    trace_log_message -q "****** End of RCMInfo.conf file content ********"

    # Determine overall registation return code based on registermachine and registersourceagent exit codes.
    # When registermachine operation fails, registersourceagent operation is not performed. Overall exit code is assigned based on this.
    # Set regexitcode to 0 when both registermachine and registersourceagent succeeds.
    # Set regexitcode to 119 when regmachineexitcode is not in 120-149 range. Otherwise set regmachineexitcode as regexitcode.
    # Set regexitcode to 119 when regsourceexitcode is not in 120-149 range. Otherwise set regsourceexitcode as regexitcode.
	if [ $regmachineexitcode -eq 129 -a "$VM_PLATFORM" != "Azure" ]; then
		regexitcode=0
        RecordOP 0 ""
    elif [ $regmachineexitcode -eq 2 ] || [ $regsourceexitcode -eq 2 ]; then
        regexitcode=2
    elif [ $regmachineexitcode -eq 0 ] && [ $regsourceexitcode -eq 0 ]; then
        regexitcode=0
        RecordOP 0 ""
    elif [ $regmachineexitcode -ne 0 ]; then
        if [ $regmachineexitcode -lt 120 ] || [ $regmachineexitcode -gt 149 ]; then
            regmachineexitcode=119
        fi
        regexitcode=$regmachineexitcode
        RecordOP $regexitcode "Register machine operation failed"
    elif [ $regsourceexitcode -ne 0 ]; then
        if [ $regsourceexitcode -lt 120 ] || [ $regsourceexitcode -gt 149 ]; then
           regsourceexitcode=119
        fi
        regexitcode=$regsourceexitcode
        RecordOP $regexitcode "Register agent operation failed"
    fi

    if [ $regexitcode -ne 0 ]; then
        trace_log_message
        trace_log_message "Agent registration has failed."
        trace_log_message -q "Writing AGENT_CONFIGURATION_STATUS=Failed in $VX_VERSION_FILE"
        echo "AGENT_CONFIGURATION_STATUS=Failed" >> "$VX_VERSION_FILE"
		if [ "$VM_PLATFORM" != "Azure" ]; then
			trace_log_message -q "Replacing Hostname value to null in $DRSCOUT_CONF"
			sed -i -e "s|^Hostname.*|Hostname = |g" $DRSCOUT_CONF

			trace_log_message -q "Removing ExternalIpAddress value from $DRSCOUT_CONF"
			sed -i -e "/^ExternalIpAddress/d" $DRSCOUT_CONF >> $LOGFILE 2>&1
		fi
    else
        trace_log_message "RCM agent registration has completed successfully."
        trace_log_message -q "Writing AGENT_CONFIGURATION_STATUS=Succeeded in $VX_VERSION_FILE"
        echo "AGENT_CONFIGURATION_STATUS=Succeeded" >> "$VX_VERSION_FILE"
        cp $DRSCOUT_CONF ${INSTALLATION_DIR}/drscout.conf.initialbackup
    fi
	
    trace_log_message -q "RCM agent registration exit code: $regexitcode"

    if [ $regexitcode -ge 120 -a $regexitcode -le 149 -a $regexitcode -ne 128 -a $regexitcode -ne 129 -a $regexitcode -ne 148 -a $regexitcode -ne 126 ]; then
        regexitcode=$FailedWithErrors
    fi
    if [ $regexitcode -eq 2 ]; then
        regexitcode=$FailedWithErrors
    fi
    return $regexitcode
}

get_non_cached_settings()
{
    trace_log_message -q "ENTERED $FUNCNAME"
    local ret=0

    local count=0
    while [ "$count" -le "3" ]; do
        trace_log_message -q "Get non-cached settings"
        local params=("--getnoncachedsettings")
        rcm_exec_with_common_params "${params[@]}"
        ret=$?

        if [ "$ret" -eq "0" ]; then
            break;
        else
            count=$(expr $count + 1)
            trace_log_message -q "Either the Source Config is invalid or unable to establish a connection with RCM Proxy. Retry count : $count"
            sleep 30
        fi
    done

    if [ "$count" -eq "4" ]; then
        trace_log_message "Unable to establish a connection with RCM Proxy."
    fi

    trace_log_message -q "Get non-cached settings for csprime return value: $ret"

    process_azurercmcli_exitcode "$ret"
    ret=$?
    trace_log_message -q "Get non-cached settings return value after processing: $ret"

    trace_log_message -q "EXITED $FUNCNAME"
    return $ret
}

validate_passphrase_for_cs()
{
	local return_value=0
    local errmessage=""
	SetOP "GetCSfingerprint"
	
	trace_log_message "Validating the passphrase."
        Count=0
        while [ "${Count}" -le "3" ]; do
            # Invoking csgetfingerprint binary with the server ip, server port for https communication.
            trace_log_message -q "Invoking csgetfingerprint binary for https communication."
            $INSTALLATION_DIR/bin/csgetfingerprint -i "$CS_IP_ADDRESS" -p "$CS_PORT_NUMBER" >> $LOGFILE 2>&1
            return_code=$?

            if [ "${return_code}" = "0" ]; then
                if [ -f /usr/local/InMage/private/connection.passphrase ]; then
                    trace_log_message -q "Passphrase file is available at /usr/local/InMage/private/connection.passphrase"
                    trace_log_message -q "Passphrase validation is successful."
                    passphrasevalidated=1
                    errmessage=""
                    break;
                else
                    trace_log_message -q "Passphrase file is not available at /usr/local/InMage/private/connection.passphrase"
                    errmessage="Passphrase file not available"
                    return_value=14 
                    break
                fi
            else
                Count=$(expr $Count + 1)
                if [ "${Count}" = "4" ]; then
                    trace_log_message "Unable to establish a connection with Configuration server."
                    trace_log_message "Either wrong FQDN/IP/Passphrase has been provided. Aborting the installation."
                    errmessage="Unable to connect CS"
                    return_value=14 
                    break
                fi
                trace_log_message -q "Either the FQDN/IP/Passphrase is invalid or unable to establish a connection with Configuration server. Retry count : $Count"
                sleep 30
            fi
        done
		
	trace_log_message -q "Passphrase validation for cs return value: $return_value"
    RecordOP $return_value "$errmessage"
    return $return_value
}

verify_client_auth()
{
    trace_log_message -q "ENTERED $FUNCNAME"
    local ret=0

    local count=0
    while [ "$count" -le "3" ]; do
        trace_log_message -q "Verifying client auth"
        local params=("--verifyclientauth" "--configfile" "$SOURCE_CONFIG_FILE")
        rcm_exec_with_common_params "${params[@]}"
        ret=$?

        if [ "$ret" -eq "0" ]; then
            break;
        else
            count=$(expr $count + 1)
            trace_log_message -q "Either the Source Config is invalid or unable to establish a connection with RCM Proxy. Retry count : $count"
            sleep 30
        fi
    done

    if [ "$count" -eq "4" ]; then
        trace_log_message "Unable to establish a connection with RCM Proxy."
    else
        update_cstype_in_drscout "CSPrime"
    fi
    trace_log_message -q "Verify client auth for csprime return value: $ret"

    process_azurercmcli_exitcode "$ret"
    ret=$?
    trace_log_message -q "Verify client auth return value after processing: $ret"

    trace_log_message -q "EXITED $FUNCNAME"
    return $ret
}

update_s2_path()
{
    trace_log_message -q "ENTERED $FUNCNAME"
    local csType="$CS_TYPE"
    local s2path="$AGENT_INSTALL_DIR/bin/s2A2A"
    if [ "$csType" = "CSLegacy" -a "$VM_PLATFORM" != "Azure" ] ; then
        s2path="$AGENT_INSTALL_DIR/bin/s2V2A"
    fi
    trace_log_message -q "Adding s2 path : $s2path in drscout.conf"
    sed -i -e "s|^DiffSourceExePathname.*|DiffSourceExePathname=$s2path|g" $DRSCOUT_CONF >> $LOGFILE 2>&1
    trace_log_message -q "EXITED $FUNCNAME"
}

update_cstype_in_drscout()
{
	local csType="$1"
	CS_TYPE=$csType
	sed -i -e "/^CSType/d; /^HostId/ a CSType=$csType" $DRSCOUT_CONF >> $LOGFILE 2>&1
	
	dppath="$AGENT_INSTALL_DIR/bin/dataprotection"
	if [ "$csType" = "CSPrime" ] ; then
		dppath="$AGENT_INSTALL_DIR/bin/DataProtectionSyncRcm"
	fi
	trace_log_message -q "Adding dp path in drscout.conf"
	sed -i -e "s|^OffloadSyncPathname.*|OffloadSyncPathname=$dppath|g; s|^FastSyncExePathname.*|FastSyncExePathname=$dppath|g; s|^DataProtectionExePathname.*|DataProtectionExePathname=$dppath|g; s|^DiffTargetExePathname.*|DiffTargetExePathname=$dppath|g" $DRSCOUT_CONF >> $LOGFILE 2>&1
}

# 
# Function Name: get_cs_fingerprint
# 
# Description:
#    This function used to get fingerprint from Configuration Server.
# 
# Parameters:$agent
# 
# Return Value: On successful it returns zero, else returns non-zero
# 
# Exceptions:None
# 
get_cs_fingerprint()
{
    local return_value=0
    local errmessage=""

    SetOP "Passphrase validation"
    
    if [ -n "$PASSPHRASE_FILE"  ]; then
        # Creating connection.passphrase file under /usr/local/InMage/private and writing passphrase to it.
        cp -f $PASSPHRASE_FILE "/usr/local/InMage/private/connection.passphrase"
        chmod 700 "/usr/local/InMage/private/connection.passphrase"
		
		echo "Value of cstype in csgetfingerprint is $CS_TYPE"
        # For Silent installation, invoking csgetfingerprint binary.
        validate_passphrase_for_cs
        return_value=$?
        update_cstype_in_drscout "CSLegacy"
    else
        # Loop until correct passphrase is entered to communicate with cs.
        while true
        do
            # Writing cs connection passphrase to the connection.passphrase file
            echo "$CS_PASSPHRASE" > /usr/local/InMage/private/connection.passphrase
            chmod 700 /usr/local/InMage/private/connection.passphrase

            validate_passphrase_for_cs
            return_code=$?
            update_cstype_in_drscout "CSLegacy"
			
			if [ "${return_code}" = "0" ]; then
				trace_log_message -q "Passphrase file is available at /usr/local/InMage/private/connection.passphrase"
                trace_log_message -q "Passphrase validation is successful."
                passphrasevalidated=1
                errmessage=""
                break;
            else
                trace_log_message "Wrong Passphrase has been entered."
                errmessage="Wrong Passphrase"
                get_cs_passphrase
                rm -f /usr/local/InMage/private/connection.passphrase
            fi
        done
    fi
    
    if [ "${AGENT_ROLE}" = "MasterTarget" -o "$CS_TYPE" = "CSLegacy" ]; then
        sed -i -e "/^RcmSettingsPath/d" $DRSCOUT_CONF >> $LOGFILE 2>&1
    fi
	
    trace_log_message -q "Get CS fingerprint return value: $return_value"
    RecordOP $return_value "$errmessage"
    return $return_value
}

# 
# Function Name: is_agent_installed
# 
# Description:
#    This function used to check whether Vx agent is already installed or not
# 
# Parameters:$agent
# 
# Return Value: On successful it returns zero, else returns non-zero
# 
# Exceptions:None
# 
is_agent_installed() {
    local agent="$1"
    local return_value=0
    
    case "$agent" in
        Vx)
            if [ -e $VX_VERSION_FILE ]; then
                trace_log_message -q "Vx agent is already installed."
                source $VX_VERSION_FILE
                OS=$OS_BUILT_FOR
                DRSCOUT_CONF="$INSTALLATION_DIR/etc/drscout.conf"
                vx_version=$(grep -w VERSION $VX_VERSION_FILE | cut -d'=' -f2)
                CURRENT_VERSION=$vx_version
                VM_PLATFORM=$(grep ^VmPlatform $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " ")
                RCMINFO_CONF="/usr/local/InMage/config/RCMInfo.conf"
                AGENT_CSIP=$(grep ^Hostname $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " ")
                AGENT_CSPORT=$(grep ^Port $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " ")
                AGENT_ROLE=$(grep ^Role $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " ")
                CURRENT_BLDNUMBER=$(grep -w BUILD_TAG $VX_VERSION_FILE | cut -d'_' -f5)
                tele_current_version=$(awk -F. -v OFS=. '/^VERSION=/{$3 = "BLDNO"}{ print }' $VX_VERSION_FILE | grep -w "^VERSION" | sed -e "s/BLDNO/$CURRENT_BLDNUMBER/g;"|cut -d= -f2)
                tele_existing_version=$tele_current_version
            else
                trace_log_message -q "Vx agent is not installed."
                return_value=1
            fi
        ;;
    esac
    trace_log_message -q "Is agent installed return value: $return_value"
    return $return_value    
}

# 
# Function Name: is_valid_creds_file
# 
# Description:
#    This function used to check whether RCM credential file is valid or not
# 
# Parameters:$agent
# 
# Return Value: On successful it returns zero, else returns non-zero
# 
# Exceptions:None
#
is_valid_creds_file() {

    local rcmcreds_file="$1"
    local return_value=0
    
    if [ ! -e "$rcmcreds_file" ] || \
        grep -q "false" <<< $(cat $rcmcreds_file | $INSTALLATION_DIR/bin/jq -e 'has("rcmUri", "aadDetails", "managementId")')
    then
        trace_log_message "$rcmcreds_file file does not exists or invalid credential file"
        return_value=1
    fi

    trace_log_message -q "RCM Creds file validation return value: $return_value"
    return $return_value
}

# 
# Function Name: is_valid_ipv4
# 
# Description:
#    This function used to validate given cs ip address
# 
# Parameters:$ip
# 
# Return Value: On successful it returns zero, else returns non-zero
# 
# Exceptions:None
# 
is_valid_ipv4()
{
    local ip=$1
    local return_value=0
    if [[ $ip =~ ^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$ ]]; then
        OIFS=$IFS
        IFS='.'
        ip=($ip)
        IFS=$OIFS
        if [[ ${ip[0]} -le 255 && ${ip[1]} -le 255 && ${ip[2]} -le 255 && ${ip[3]} -le 255 ]]; then
            trace_log_message -q "Valid ip address."            
        else
            trace_log_message "Invalid ip address. Please retry."
            return_value=3
        fi
    else
        trace_log_message "Invalid ip address. Please retry."
        return_value=3
    fi
    trace_log_message -q "IP validation return value: $return_value"
    return $return_value
}

# 
# Function Name: get_cs_passphrase
# 
# Description:
#    This function used to get Configuration Server passphrase from user
# 
# Parameters:None
# 
# Return Value: On successful it returns zero
# 
# Exceptions:None
# 
get_cs_passphrase() {
    CS_PASSPHRASE=""
    SetOP "GetCSpassphrase"
    while [ "X$CS_PASSPHRASE" = "X" ]
    do
            echo -en "\nProvide Configuration Server passphrase ? : "
            stty -echo
            read CS_PASSPHRASE
            stty echo
            [ -z "$CS_PASSPHRASE" ] && echo -e "\nInvalid input. Please try again."
        done
    echo
    RecordOP 0 ""
    return 0
}

# Function Name: get_cs_ipaddress
# 
# Description:
#    This function used to get Configuration Server IP address from user
# 
# Parameters:None
# 
# Return Value: On successful it returns zero
# 
# Exceptions:None
# 
get_cs_ipaddress() {
    SetOP "GetCSipAddr"
    while true; do
        echo -en "\nProvide Configuration Server IP address ? : "
        read CS_IP_ADDRESS
        
        if [ -n "$CS_IP_ADDRESS" ]; then
            break;
        fi
        continue
    done
    RecordOP 0 ""
    return 0
}

# Function Name: get_cs_port
# 
# Description:
#    This function used to get Configuration Server port from user
# 
# Parameters:None
# 
# Return Value: On successful it returns zero
# 
# Exceptions:None
# 
get_cs_port() {
	SetOP "GetCSPort"
	echo -en "\nPlease provide CS port number. Default port number is 443. "
	read CS_PORT
	
	if [ -z "$CS_PORT" ]; then
		CS_PORT="443"
	fi
	
	RecordOP 0 ""
    return 0
}

#
# Function Name: get_rcm_creds
# 
# Description:
#    This function used to take RCM credential file path from user and do the validation
# 
# Parameters:None
# 
# Return Value: On successful it returns zero
# 
# Exceptions:None
# 
get_rcm_creds() {
    RCM_CREDS_FILE=""
    SetOP "GetRCMcreds"
    while [ "X$RCM_CREDS_FILE" = "X" ]
    do
        echo -en "\nProvide RCM Credentials file path ? : "
        read RCM_CREDS_FILE
        if [ -z "$RCM_CREDS_FILE" ]; then
            echo -e "\nInvalid input. Please try again."
        elif is_valid_creds_file "$RCM_CREDS_FILE" ; then
            break;
        else
            echo -e "\nInvalid credential file. Please try again."
            continue
        fi        
    done
    RecordOP 0 ""
    return 0
}

get_source_config_file() {
    trace_log_message -q "ENTERED $FUNCNAME"
    SetOP "GetSourceConfigFile"
    while :
    do
        echo -en "\nProvide Source Config File Path : "
        read SOURCE_CONFIG_FILE
        if [ -f "$SOURCE_CONFIG_FILE" ]; then
            break
        fi
    done
    echo
    RecordOP 0 ""
    trace_log_message -q "EXITED $FUNCNAME"
}

get_discovery_flag() {
    trace_log_message -q "ENTERED $FUNCNAME"

    while true; do
        echo -en "\nSelect if credential-less discovery should be performed for this machine ? [Options: true|false, Default: false ]"
        read IS_CREDENTIAL_LESS_DISCOVERY
        [ -z "$IS_CREDENTIAL_LESS_DISCOVERY" ] && IS_CREDENTIAL_LESS_DISCOVERY="false"
        if [ "$IS_CREDENTIAL_LESS_DISCOVERY" = "true" -o "$IS_CREDENTIAL_LESS_DISCOVERY" = "false" ]; then
            break;
        else
            trace_log_message "Invalid input. Please try again."
        fi
    done

    trace_log_message -q "EXITED $FUNCNAME"
}

log_to_json_file() {
    log_to_json_errors_file $json_errors_file "$@"
}

IsAzureVirtualMachine()
{
    trace_log_message -q "ENTERED $FUNCNAME"
    trace_log_message -q "Detecting VM Platform."
    local tag=$(cat /sys/devices/virtual/dmi/id/chassis_asset_tag 2>/dev/null)
    if [ -z "$tag" ]; then
        tag=`dmidecode -t chassis 2>/dev/null | grep "Asset Tag" | cut -f2 -d":" | tr -d " "`
    fi
    trace_log_message -q "dmi chassis asset tag = $tag"
    local return_value=1    #VmWare
    if [ "$tag" = "7783-7084-3265-9085-8269-3286-77" ] && [ ! is_azure_stack_platform ] ; then
        return_value=0   #Azure
    fi
    trace_log_message -q "return value: $return_value"
    trace_log_message -q "EXITED $FUNCNAME"
    return $return_value
}


IsFailedOverVm() {
    trace_log_message -q "ENTERED $FUNCNAME"
    local return_value=1
    local platform=`grep ^VmPlatform $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " "`
    local cs_type=`grep ^CSType $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " "`

    IsAzureVirtualMachine
    local is_azure_vm=$?
    if [ "$platform" = "VmWare" -a "$cs_type" = "CSPrime" -a $is_azure_vm -eq 0 ]; then
        return_value=0
    fi
    trace_log_message -q "return value : $return_value"
    trace_log_message -q "EXITED $FUNCNAME"
    return $return_value
}

unconfigure_rcmproxy() {
    trace_log_message -q "ENTERED $FUNCNAME"
    local ret=0

    if [ "$CS_TYPE" = "CSPrime" -a "$UNCONFIGURE" = "true" ]; then
        stop_agent_service || exit_with_retcode $?
        unregister_agent
        rm -f ${AGENT_INSTALL_DIR}/etc/SourceConfig.json ${AGENT_INSTALL_DIR}/etc/SourceConfig.json.lck >> $LOGFILE 2>&1

        #Remove HostId and ResourceId
        update_config_general "$DRSCOUT_CONF" "HostId" ""
        update_config_general "$DRSCOUT_CONF" "ResouceId" ""
        ret=$?
    fi

    trace_log_message -q "EXITED $FUNCNAME"
    exit_with_retcode $ret
}

#
# Function Name: usage
#
# Description:
#    This function will provide script usage.
#
# Parameters:None
#
# Return Value: It returns zero,
#
# Exceptions:None
#
usage() {
    trace_log_message "Usage:  UnifiedAgentConfigurator.sh [options]"
    trace_log_message "Options:"
    trace_log_message "  -i <Configuration Server IP Address>"
    trace_log_message "  -e <External IP>"
    trace_log_message "  -l <Log file name (absolute path)>"
    trace_log_message "  -P <Passphrase file name(absolute path)>"
    trace_log_message "  -S <Source config file name(absolute path)>"
    trace_log_message "  -C <RCM credential file path(absolute path)>"
    trace_log_message "  -D <Credential Less Discovery(true/false)>"
    trace_log_message "  -U <Unconfigure appliance>"
    trace_log_message "  -c <CSType (CSLegacy/CSPrime)>"
    trace_log_message "  -h <help> "
    exit 0
}

main() 
{
    local return_value=0
    SetOP "GetCmdArgs"

    if [ $# -ge 1 ] && $(echo $1 | grep -q ^- ); then
        while getopts :i:e:l:C:P:S:D:U:m:j:c:p:r:f:qh opt
        do
            case $opt in
                i) CS_IP_ADDRESS="$OPTARG" ;;
                e) EXTERNAL_IP="$OPTARG" ;;
                l) CUSTOM_LOGFILE="$OPTARG" ;;
                P) PASSPHRASE_FILE="$OPTARG" ;;
                S) SOURCE_CONFIG_FILE="$OPTARG" ;;
                C) RCM_CREDS_FILE="$OPTARG" ;;
                D) IS_CREDENTIAL_LESS_DISCOVERY="$OPTARG" ;;
                U) UNCONFIGURE="$OPTARG" ;;
                m) INVOKER="$OPTARG" ;;
                j) json_file="$OPTARG" ;;
                c) CS_TYPE="$OPTARG"
                    if [ "$CS_TYPE" != "CSLegacy" -a "$CS_TYPE" != "CSPrime" ]; then
                        usage && exit 96
                    fi ;;
                p) CS_PORT="$OPTARG" ;;
                r) CLIENT_REQUEST_ID="$OPTARG" ;;
                f) CONFIGURE_ERRORS_JSON="$OPTARG" ;;
                q) SILENT_ACTION="true" ;;
                h|-*|*)
                    trace_log_message -q "Specify the CS registration options to the command-line!"
                    usage && exit 96
                ;;
            esac
        done

        if [ ! -z "$CUSTOM_LOGFILE" ] && [ "$LOGFILE" != "$CUSTOM_LOGFILE" ]; then
            LOGFILE=$CUSTOM_LOGFILE
            VERBOSE_LOGFILE=$CUSTOM_LOGFILE
        fi
    fi

    if [ -z "$CS_TYPE" ]; then
        CS_TYPE="CSLegacy"
    fi

    echo "Value of cstype is $CS_TYPE"

    if [ "$CS_TYPE" = "CSLegacy" ]; then
        if [ -n "$RCM_CREDS_FILE" ] ; then
            SILENT_ACTION="true"
        elif [ -z "$CS_IP_ADDRESS" -o -z "$PASSPHRASE_FILE" ]; then
            SILENT_ACTION="false"
        else
            SILENT_ACTION="true"
        fi
    fi

    trace_log_message -q
    trace_log_message -q "AGENT CONFIGURATION"
    trace_log_message -q "*******************"


    if [ -z "$CONFIGURE_ERRORS_JSON" ]; then
        CONFIGURE_ERRORS_JSON=$DEFAULT_CONFIGURE_ERRORS_JSON
    fi

    json_errors_file=${CONFIGURE_ERRORS_JSON}
    if [ -f $AGENT_INSTALL_DIR/bin/libcommon.sh ]; then
        . $AGENT_INSTALL_DIR/bin/libcommon.sh
    else
        trace_log_message "File not found : libcommon.sh"
        exit_with_retcode 1
    fi

    IsFailedOverVm
    if [ $? -eq 0 ]; then
        trace_log_message "This is a failed over vm in CSPrime stack. Configuration operation is a no op here"
        exit_with_retcode $Success
    fi
    StartSummary
    RecordOP 0 ""

    SetOP "GetAgentStatus"
    if is_agent_installed "Vx" ; then
        if [ ! -z "$UNCONFIGURE" ]; then
            unconfigure_rcmproxy
        fi
        if [ "$SILENT_ACTION" = "true" ]; then
            trace_log_message -q "Silent agent configuration."
            if [ "$VM_PLATFORM" = "Azure" ] ; then
                SetOP "ValidateCredsFile"
                is_valid_creds_file "$RCM_CREDS_FILE"
                return_value=$?
                if [ $return_value -ne 0 ]; then
                    RecordOP $return_value "Invalid credential file $RCM_CREDS_FILE"
                    exit_with_retcode $return_value 
                fi
                RecordOP 0 ""
            elif [ "$VM_PLATFORM" = "VmWare" ]; then
                SetOP "ValidateExternalIPaddr"
                trace_log_message -q "EXTERNAL_IP=$EXTERNAL_IP"
                if [ -n "$EXTERNAL_IP" ]; then
                    trace_log_message -q "Performing External IP validation."
                    is_valid_ipv4 "$EXTERNAL_IP"
                    return_value=$?
                    if [ $return_value -ne 0 ]; then
                        RecordOP $return_value "Invalid external IP address $EXTERNAL_IP"
                        exit_with_retcode $return_value 
                    fi
                    RecordOP 0 ""
                fi

                if [ "$CS_TYPE" = "CSLegacy" ]; then
                    SetOP "VerifyPassphraseFile"
                    if [ ! -f "$PASSPHRASE_FILE" ]; then
                        trace_log_message "Passphrase file passed to the installer does not exist."
                        RecordOP 1 "Passphrase file $PASSPHRASE_FILE doesn't exists"
                        exit_with_retcode 1
                    fi
                    RecordOP 0 ""
                fi
            else
                trace_log_message "Invalid entry of VM_PLATFORM : \"$VM_PLATFORM\""
                log_to_json_file ASRMobilityServiceVMPlatformNotSet "Unable to read the configuration of the source machine."
                exit_with_retcode $FailedWithErrors
            fi
        else
            trace_log_message -q "Interactive agent configuration."
            if [ "$VM_PLATFORM" = "Azure" ]; then
                get_rcm_creds
            elif [ "$VM_PLATFORM" = "VmWare" ]; then
                if [ "$CS_TYPE" = "CSPrime" ]; then
                    get_source_config_file
                    get_discovery_flag
                else
                    get_cs_ipaddress
                    get_cs_passphrase
                fi
            else
                trace_log_message "Invalid entry of VM_PLATFORM : \"$VM_PLATFORM\""
                log_to_json_file ASRMobilityServiceVMPlatformNotSet "Unable to read the configuration of the source machine."
                exit_with_retcode $FailedWithErrors
            fi
        fi

        # Adding 443 as the default Configuration server port if no port is given
        if [ -z "$CS_PORT" ]; then
            CS_PORT_NUMBER=443
        else
            CS_PORT_NUMBER="$CS_PORT"
        fi

        # Fetch VM Platform information from drscout.conf
        SetOP "GetVMplatform"
        VM_PLATFORM=$(grep ^VmPlatform $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " ")
        trace_log_message "Platform value in drscout.conf: ${VM_PLATFORM}"
        if [ -z "${VM_PLATFORM}" ] || [ "$VM_PLATFORM" != "VmWare" -a "$VM_PLATFORM" != "Azure" ]; then
            trace_log_message "Aborting configuration as VM_PLATFORM value is either null or it is not VmWare/Azure."
            RecordOP 1 "Invalid VM_PLATFORM value ${VM_PLATFORM}"
            exit_with_retcode 1
        fi

        # Verify whether agent is already registered to configuration server.
        SetOP "IsAlreadyRegistered"
        PLATFORM_CHANGE_TO_CSPRIME=$(grep ^PlatformChangeToCSPrime $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " ")
        if [ "$VM_PLATFORM" = "VmWare" ] ; then
            if [ ! -z "$AGENT_CSIP" ] && [ ! -z "$AGENT_CSPORT" ]; then
                if [ "$AGENT_CSIP" = "$CS_IP_ADDRESS" ] && [ "$AGENT_CSPORT" = "$CS_PORT_NUMBER" ]; then
                    trace_log_message -q "Agent is already registerd to same Configuration server ($AGENT_CSIP:$AGENT_CSPORT)."
                elif [ -n "$PLATFORM_CHANGE_TO_CSPRIME" ] && [ "$PLATFORM_CHANGE_TO_CSPRIME" = "true" ]; then
                    trace_log_message "Allowing upgrade from CS to CSPRIME."
                else
                    trace_log_message "Agent is already registerd to different Configuration server ($AGENT_CSIP:$AGENT_CSPORT)."
                    trace_log_message "return value: 13"
                    RecordOP 13 "Agent already registerd to different Configuration server $AGENT_CSIP:$AGENT_CSPORT"
                    exit_with_retcode 13
                fi
            fi
            RecordOP 0 ""
        fi

        # Remove AGENT_CONFIGURATION_STATUS entry from .vx_version
        sed -i -e '/^AGENT_CONFIGURATION_STATUS/d' $VX_VERSION_FILE

        # Stop the services.
        stop_agent_service || exit_with_retcode $?

        if [ "$CS_TYPE" = "CSPrime" ]; then
            if [ ! -f "$SOURCE_CONFIG_FILE" ]; then
                trace_log_message "Unable to find Source config file : "
                # TODO Add specific error name
                exit_with_retcode 1
            fi

            update_discovery || exit_with_retcode $?
        fi

        set_scenario "Configuration"
		
		if [ ! -d $RCMCONFIG_FOLDER ]; then
	        trace_log_message -q "Creating $RCMCONFIG_FOLDER"
	        mkdir -p $RCMCONFIG_FOLDER >> $LOGFILE 2>&1
	    fi

	    if [ ! -f $RCMCONFIG_FOLDER/RCMInfo.conf ]; then
		trace_log_message -q "Copying ${AGENT_INSTALL_DIR}/etc/RCMInfo.conf to $RCMCONFIG_FOLDER"
		cp ${AGENT_INSTALL_DIR}/etc/RCMInfo.conf $RCMCONFIG_FOLDER >> $LOGFILE 2>&1
	    fi

        if [ "$VM_PLATFORM" = "Azure" ]; then
            generate_host_id "false"

			# Set LogLevel to 7 before performing AzureRcmCli operations.
			trace_log_message -q "Setting LogLevel to 7 before performing AzureRcmCli operations."
    		sed -i -e "s|^LogLevel.*|LogLevel = 7|g" $DRSCOUT_CONF
    		mv -f /var/log/AzureRcmCli.log /var/log/AzureRcmCli.log.`date '+%Y_%m_%d_%H_%M_%S'` >/dev/null 2>&1

            parse_update_rcm_creds "$RCM_CREDS_FILE" || exit $?
            rcm_agent_registration "Vx" || exit $?

			# Set LogLevel to 3 after performing AzureRcmCli operations.
			trace_log_message -q "Setting LogLevel to 3 after performing AzureRcmCli operations."
    		sed -i -e "s|^LogLevel.*|LogLevel = 3|g" $DRSCOUT_CONF
        else
			update_drscout_config || exit_with_retcode $?
            if [ "$CS_TYPE" = "CSPrime" ]; then
                verify_client_auth || exit_with_retcode $?
            else
                get_cs_fingerprint || exit_with_retcode $?
            fi
			update_drscout_config_for_cs || exit_with_retcode $?
            cs_agent_registration "Vx" || check_return_code $?
			create_soft_links || exit_with_retcode $?
        fi
        update_s2_path
        set_scenario "PostConfiguration"

        start_agent_service || exit_with_retcode $?
        set_permissions || exit_with_retcode $?
		sed -i -e "s|^PlatformChangeToCSPrime=.*|PlatformChangeToCSPrime=false|g" $DRSCOUT_CONF >> $LOGFILE 2>&1
	trace_log_message "Agent configuration is completed successfully."
	CLOUD_PAIRING_STATUS=$(grep ^CloudPairingStatus $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " ")
	if [ -n "$CLOUD_PAIRING_STATUS" -a "$CLOUD_PAIRING_STATUS" = "Incomplete" ]; then
		echo "Please register your cs appliance with RCM service for the agent registration to complete."
		# To do
		# Check if we want to exit with a specific error code for manual install when Rcm proxy is not registered with Rcm during agent registration
		#if [ "$SILENT_ACTION" != "true" ]; then
		#	exit_with_retcode "NEW ERROR CODE"
		#fi
	fi
    else
        echo "Vx agent is not installed."
        RecordOP 1 "Vxagent not installed"
        return 1
    fi
    return 0
}

trap "trace_log_message 'Program terminated at user request'; EndSummaryOP "Aborted"; exit 1" 1 2 3 6 9 15 30

set_scenario "PreConfiguration"

# Read conf values from vx_version file (needed upfront for telemetry)
is_agent_installed "Vx"

trace_log_message -q
trace_log_message -q "Script initiated with the following command:"
trace_log_message -q "${SCRIPT} $@"
trace_log_message -q

main "$@"

exit_with_retcode $?
