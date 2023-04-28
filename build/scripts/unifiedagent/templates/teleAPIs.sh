
# Path on CS machine where we upload telemetry files
CMDLINE_TELEMTRY_PATH_ON_CS="/home/svsystems/AgentSetupLogs/CommandLineInstall"
INSTALLER_GUID=""
HOSTID_IN_DRSCOUT=""
HOSTID_FILE='/usr/local/.vx_host_id'
agentrole=""

is_azure_platform()
{
    local return_value=0

    if [ -n "$VM_PLATFORM" ]; then
        vmplatform=$(tr [A-Z] [a-z] <<<$VM_PLATFORM)
        if [ "$vmplatform" == "azure" ] ; then
            return 1
        fi
    fi
    return 0
}

is_azure_stack_platform()
{
    local return_value=0

    if [ -n "$IsAzureStackHub" ]; then
            return $IsAzureStackHub
    fi
    return 0
}

# This function sends given file to CS machine using cxpsclient binary
# After sending, moves the file to uploaded files directory
# Note: This function is called when invoked from commandline, 
#       Incase of pushinstall, pushinstall service pulls telemetry files
# input: path of the file to send to CS
# return code: 0 for success, non-zero for failure
send_tele_file()
{
    local fname=$1
    local sendinglogfile=$2
    local return_code=0

    if [ $sendinglogfile == 1 ]; then
        cmd="$INSTALLATION_DIR/bin/cxpsclient -i $CS_IP_FOR_TELEMETRY -l $CXPS_PORT --put $fname -C -d $CMDLINE_TELEMTRY_PATH_ON_CS/'$INSTALLER_GUID'_'$HOST_NAME' >> /dev/null 2>&1"
    else
        cmd="$INSTALLATION_DIR/bin/cxpsclient -i $CS_IP_FOR_TELEMETRY -l $CXPS_PORT --put $fname -C -d $CMDLINE_TELEMTRY_PATH_ON_CS/'$INSTALLER_GUID'_'$HOST_NAME' >> $VERBOSE_LOGFILE 2>&1"
        trace_log_message -q "Executing cmd: $cmd"
    fi
    local count=0
    while [ $count -le 3 ]; do
        eval $cmd
        return_code=$?
        if [ $return_code -ne 0 ]; then
            count=$(expr $count + 1)
            continue
        fi
        mv -f $fname $UPLOADED_TELEDIR
        break
    done
    if [ $return_code -ne 0 ]; then
        trace_log_message -q "Failed to send telemetry file $fname to CS $CS_IP_FOR_TELEMETRY, Port $CXPS_PORT"
    fi
    return $return_code
}

# Function to read all files in telemetry directory and send to CS machine
# Input: nothing
# return code: nothing
send_telemetry()
{
    trace_log_message -q "Sending telemetry files to CS"
    mkdir -p $UPLOADED_TELEDIR

    # Send telemetry files
    find $SETUP_TELEDIR -maxdepth 1 -type f | while read telefile
    do
        send_tele_file $telefile 0
    done

    cp -f $VERBOSE_LOGFILE $televerbose_file
    send_tele_file $televerbose_file 1
}

set_scenario()
{
    SCENARIO_NAME=$1
}

SetOP()
{
    OP_NAME=$1
}

# Function to start telemetry, truncates json files
# This function assumes verbose filename is in VERBOSE_LOGFILE variable
# TEMP_SUMMARY_FILE is temporary file created to write telemetry records, jq reads
# this file and creates JSON file
# input: nothing
# return code: nothing
StartSummary()
{
    is_azure_platform || return 0

    local operation=$installtype

read -d '' op_details <<EOF
{ "ScenarioName": "$SCENARIO_NAME",
  "OperationName": "$OP_NAME",
  "Result": "Success",
  "ErrorCode": 0
}
EOF
    echo $op_details > $TEMP_SUMMARY_FILE

    if [ -n "$INVOKER" ]; then
        # Convert to lowercase and remove leading spaces
        INVOKER=$(tr [A-Z] [a-z] <<<$INVOKER) >> $VERBOSE_LOGFILE 2>&1
        trace_log_message -q "INVOKER=$INVOKER after translate."
    fi

    trace_log_message -q "RUNID = $RUNID"
    return 0
}

# Function to record a telemetry operation
# TEMP_SUMMARY_FILE is temporary file created to write telemetry records, jq reads
# this file and creates JSON file
# input: (ScenarioName, OperationName, Result, ErrorCode, Message)
# return code: nothing
RecordOP()
{
    is_azure_platform || return 0

    nargs=$#
    if [ $nargs -ne 2 ]; then
        trace_log_message -q "telemetry error: Passed $nargs args to RecordOP, expecting 2 args"
        trace_log_message -q "arg1: $1, arg2: $2"
    fi

    if [ -z "$SCENARIO_NAME" ]; then
        trace_log_message -q "Warning: SCENARIO_NAME is empty"
    fi

    if [ -z "$OP_NAME" ]; then
        trace_log_message -q "Warning: OP_NAME is empty"
    fi

    errcode=$1
    message=$2
    result="Success"

    if [ "$errcode" != "0" ]; then
        result="Failed"
    fi


read -d '' op_details <<EOF
{ "ScenarioName": "$SCENARIO_NAME",
  "OperationName": "$OP_NAME",
  "Result": "$result",
  "ErrorCode": "$errcode",
  "Message": "$message"
}
EOF

    echo ", $op_details" >> $TEMP_SUMMARY_FILE
    return 0
}

# Function to generate JSON file and invoke functions to send telemetry to CS
# input args: $1 install/configuration status
EndSummaryOP()
{
    is_azure_platform
    ret=$?
    if [ $ret != 0 ]; then
        trace_log_message -q "Not generating telemetry because platform is: $VM_PLATFORM"
        return 0
    fi

    read_unique_GUIDs

    if [ -n "$AGENT_ROLE" ]; then            
        agentrole=$(tr [A-Z] [a-z] <<<$AGENT_ROLE)
        if [ "$agentrole" == "ms" -o "$agentrole" == "agent" ] ; then
            agentrole="MS"
        elif [ "$agentrole" == "mt" -o "$agentrole" == "mastertarget" ] ; then
            agentrole="MT"
        fi
    fi

    local finalsummary=$(cat $TEMP_SUMMARY_FILE)
    if [ $OS == "RHEL5-64" ]; then
        biosid=$(dmidecode -t system | grep UUID | awk '{print $2}')
    else
        biosid=$(cat /sys/class/dmi/id/product_uuid)
    fi

read -d '' install_summary <<EOF
{  "StartTime": "$starttime",
   "ComponentName": "Microsoft Azure Site Recovery Unified Agent Setup",
   "ComponentVersion": "$tele_current_version",
   "ExistingVersion": "$tele_existing_version",
   "InstallType": "$installtype",
   "InstallStatus": "$1",
   "ExitCode": "$exit_code",
   "MachineIdentifier": "$INSTALLER_GUID",
   "BIOSid": "$biosid",
   "HostName": "$HOST_NAME",
   "HostId": "$HOSTID_IN_DRSCOUT",
   "InstallScenario": "$VM_PLATFORM",
   "InstallRole": "$agentrole",
   "InvokerType": "$INVOKER",
   "SilentInstall": "$SILENT_ACTION",
   "OsType": "Linux",
   "OsDistro": "$OS",
   "OSkernel": "$(uname -r)",
   "RAMtotal": "$(cat /proc/meminfo | grep -i memtotal | awk -F' ' '{print $2,$3}')",
   "RAMfree": "$(cat /proc/meminfo | grep -i memfree | awk -F' ' '{print $2,$3}')",
   "NumCores": "$(grep -c ^processor /proc/cpuinfo)",
   "RUNID": "$RUNID",
   "EndTime": "$(date -Iseconds)",
   "OperationDetails":[ $finalsummary ]
}
EOF

    mkdir -p $SETUP_TELEDIR >> $VERBOSE_LOGFILE 2>&1
    echo "$install_summary" | $JQPATH . > $json_file
    return_code=$?
    if [ $return_code -ne 0 ]; then
        trace_log_message -q  "telemetry error: Failed to generate JSON file $json_file"
        # Remove empty json file
        rm -f $json_file
    fi

    if [ "$INVOKER" == "pushinstall" ] ||
       [ "$INVOKER" == "vmtools" ]; then
        # Don't send telemetry files, pushinstall service picks them
        # and puts them on CS machine under category "PushInstall" or "VmTools"
        trace_log_message -q  "Not sending telemetry files, because INVOKER is $INVOKER"
        return 0
    fi

    if [ -z "$INSTALLATION_DIR" ]; then
        trace_log_message -q  "Not sending telemetry files, because INSTALLATION_DIR is empty"
        return 0
    fi

    if [ "$installtype" == "Configuration" ] && [ $passphrasevalidated -eq 1 ]; then
        CS_IP_FOR_TELEMETRY=$CS_IP_ADDRESS
        get_cxps_port
        update_csdetails_in_drscout
    elif [ "$installtype" == "Upgrade" ]; then
        # In upgrade case read values from drscout.conf file
        read_csdetails_from_drscout
    fi


    if [ ! -z "$CS_IP_FOR_TELEMETRY" ] && [ ! -z "$CXPS_PORT" ]; then
        # Save verbose file in telemetry directory
        send_telemetry
    else
        trace_log_message -q "Not sending telemetry files, CS IP: $CS_IP_FOR_TELEMETRY, CXPS port: $CXPS_PORT"
        # Save verbose file in telemetry directory for uploading later
        cp -f $VERBOSE_LOGFILE $televerbose_file
    fi
    return 0
}


# Function to generate/read unique host ID to/from persisten file 
# We first read unique hostid stored in drscout.conf file, if doesn't exists
# then we read unique hostid file, if doesn't exists then we generate new unique host id
# we update drscout.conf file an hostid file with unique hostid
# input: nothing
# return code: nothing
read_unique_GUIDs()
{
    is_azure_platform
    ret=$?
    if [ $ret != 0 ]; then
        trace_log_message -q "Not reading HostID because platform is: $VM_PLATFORM"
        return 0
    fi

    if [ -f "$HOSTID_FILE" ]; then
        INSTALLER_GUID=$(cat $HOSTID_FILE)
    else
        trace_log_message -q "Generating new HostId"
        INSTALLER_GUID=$(cat /proc/sys/kernel/random/uuid)
        echo $INSTALLER_GUID > $HOSTID_FILE
    fi

    if [ -f "$DRSCOUT_CONF" ]; then
        HOSTID_IN_DRSCOUT=$(grep ^HostId $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " ")
    fi

    trace_log_message -q "Installer GUID is: $INSTALLER_GUID"
    trace_log_message -q "HostId value in drscout.conf: $HOSTID_IN_DRSCOUT"

    return 0
}

# This function updates CS IP and CXPS port number in drscout.conf for 
# telemetry use later on. Writes CSIPforTelemetry and CxpsPortforTelemetry to drscout
# input: nothing
# return code: nothing
update_csdetails_in_drscout()
{
    trace_log_message -q "Writing CSIPforTelemetry=$CS_IP_FOR_TELEMETRY to $DRSCOUT_CONF"
    grep -i "CSIPforTelemetry" $DRSCOUT_CONF >> $VERBOSE_LOGFILE 2>&1
    if [ $? -eq 0 ]; then
        sed -i -e "s|^CSIPforTelemetry.*|CSIPforTelemetry=$CS_IP_FOR_TELEMETRY|g" $DRSCOUT_CONF >> $VERBOSE_LOGFILE 2>&1
    else
        sed -i -e "/^HostId/ a CSIPforTelemetry=$CS_IP_FOR_TELEMETRY" $DRSCOUT_CONF >> $VERBOSE_LOGFILE 2>&1
    fi

    trace_log_message -q "Writing CxpsPortforTelemetry=$CXPS_PORT to $DRSCOUT_CONF"
    grep -i "CxpsPortforTelemetry" $DRSCOUT_CONF >> $VERBOSE_LOGFILE 2>&1
    if [ $? -eq 0 ]; then
        sed -i -e "s|^CxpsPortforTelemetry.*|CxpsPortforTelemetry=$CXPS_PORT|g" $DRSCOUT_CONF >> $VERBOSE_LOGFILE 2>&1
    else
        sed -i -e "/^HostId/ a CxpsPortforTelemetry=$CXPS_PORT" $DRSCOUT_CONF >> $VERBOSE_LOGFILE 2>&1
    fi
}

# This function reads CS IP and CXPS port values from drscout.conf file for
# sending telemetry to CS machine
# input: nothing
# return code: nothing
read_csdetails_from_drscout()
{
    if [ -f "$DRSCOUT_CONF" ]; then
        trace_log_message -q "Reading CSIPforTelemetry and CxpsPortforTelemetry from drscout.conf file"
        CS_IP_FOR_TELEMETRY=$(grep ^CSIPforTelemetry $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " ")
        CXPS_PORT=$(grep ^CxpsPortforTelemetry $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " ")

        # Agents installed before 1710 won't have CSIPforTelemetry, CxpsPortforTelemetry
        if [ -z "$CS_IP_FOR_TELEMETRY" ]; then
            CS_IP_FOR_TELEMETRY=$(grep ^Hostname $DRSCOUT_CONF | cut -d"=" -f2 | tr -d " ")
        fi
        if [ ! -z "$CS_IP_FOR_TELEMETRY" ] && [ -z "$CXPS_PORT" ]; then
            get_cxps_port
        fi
    fi
}

# This function invokes CS API to get CXPS port number from CS
# Default value for CXPS port is 9443, assumes initialized during
# begining of the installation/configuration
# input: nothing
# return code: nothing
get_cxps_port()
{
    port=`$INSTALLATION_DIR/bin/cxcli -i $CS_IP_FOR_TELEMETRY 2>/dev/null`
    return_code=$?
    if [ $return_code -eq 0 ]; then
        CXPS_PORT=$port
        trace_log_message -q "cxps port: $CXPS_PORT"
    else
        CXPS_PORT="9443"
        trace_log_message -q "Failed to get cxps port, trying with default value $CXPS_PORT"
    fi
}

