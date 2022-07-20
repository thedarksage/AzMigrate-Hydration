#!/bin/sh

umask 0002
VX_VERSION_FILE="/usr/local/.vx_version"
SCRIPT_DIR=$(readlink -f `dirname "$0"`)
INSTALL_LOG_FILE="$SCRIPT_DIR/ua_install.log"
UPGRADE_LOG_FILE="$SCRIPT_DIR/ua_upgrader.log"
CUR_DIR=`pwd`
INSTALLATION_DIR=$(grep ^INSTALLATION_DIR $VX_VERSION_FILE | cut -d"=" -f2 | tr -d " ")
INSTALL_ERROR_JSON="/var/log/InstallerErrors.json"
PAR_DIR=`dirname $SCRIPT_DIR`
GLOB_LOG_DIR="$PAR_DIR/logs"
GLOB_INSTALL_LOG_FILE="$GLOB_LOG_DIR/ua_install.log"
GLOB_UPGRADE_LOG_FILE="$GLOB_LOG_DIR/ua_upgrader.log"

trace_log_message() {
    DATE_TIME=`date '+%Y/%m/%d %H:%M:%S'`
    echo -e "$@"
    echo "${DATE_TIME} : $@ " >> ${UPGRADE_LOG_FILE} 2>&1
}

exit_with_retcode() {
    #Moving to original directory
    cd $CUR_DIR

    trace_log_message "Dumping Job file content in upgrader log file"
    cat $JOB_FILE_PATH >> $UPGRADE_LOG_FILE

    #TODO Upload upgrader and AzureRcmCLi log.
    ${INSTALLATION_DIR}/bin/AzureRcmCli --agentupgradelogsupload --agentupgradelogspath \
        "$SCRIPT_DIR" --agentupgradejobdetails "$JOB_FILE_PATH"

    #Dump data to global log file
    mkdir -p $GLOB_LOG_DIR
    cat $INSTALL_LOG_FILE >> $GLOB_INSTALL_LOG_FILE
    cat $UPGRADE_LOG_FILE >> $GLOB_UPGRADE_LOG_FILE

    exit $1
}

upgrade_mobility_service() {
    trace_log_message "ENTERED $FUNCNAME"

    $SCRIPT_DIR/install -r MS -v VmWare -q -a Upgrade -c CSPrime -l $INSTALL_LOG_FILE \
        -e $INSTALL_ERROR_JSON -j $SCRIPT_DIR/installerjson.json
    local ret_val=$?

    trace_log_message "EXITED $FUNCNAME"
    return $ret_val
}

register_source_agent() {
    trace_log_message "ENTERED $FUNCNAME"
    local ret_val=1
    local count=0

    while [ $count -le 2 ]; do
        trace_log_message "AzureRcmCli registersourceagent operation - iteration : $count"
        trace_log_message "-------------------------------------------------------------"
        trace_log_message "Executing the command : ${INSTALLATION_DIR}/bin/AzureRcmCli " \
            "--registersourceagent --errorfilepath $INSTALL_ERROR_JSON"
        ${INSTALLATION_DIR}/bin/AzureRcmCli --registersourceagent --errorfilepath \
            $INSTALL_ERROR_JSON >> $UPGRADE_LOG_FILE 2>&1
        ret_val=$?
        if [ $ret_val -eq 0 ]; then
            trace_log_message -q "registersourceagent operation has succeeded."
            break
        else
            trace_log_message -q "registersourceagent failed with exit code - $ret_val"
            count=$(expr $count + 1)
            sleep 5
        fi
    done

    trace_log_message "EXITED $FUNCNAME"
    return $ret_val
}

upgrade_action() {
    trace_log_message "ENTERED $FUNCNAME"
    upgrade_mobility_service
    upgrade_ret_val=$?

    if [ $upgrade_ret_val -ne 0 -a $upgrade_ret_val -ne 98 \
        -a $upgrade_ret_val -ne 209 ]; then
        trace_log_message "Upgrade agent failed with return value : $upgrade_ret_val"
        return $upgrade_ret_val
    fi
    register_source_agent
    reg_source_exit_code=$?
    trace_log_message ""
    if [ $reg_source_exit_code -eq 0 ]; then
        trace_log_message "registersourceagent operation has succeeded."
    else
        trace_log_message "registersourceagent failed with exit code - $reg_source_exit_code"
    fi

    trace_log_message "EXITED $FUNCNAME"
    return $reg_source_exit_code
}

complete_job_to_rcm() {
    trace_log_message "ENTERED $FUNCNAME"

    ${INSTALLATION_DIR}/bin/AzureRcmCli --completeagentupgrade --agentupgradeexitcode \
        "$upgrade_action_return_value" --agentupgradejobdetails "$JOB_FILE_PATH" --errorfilepath \
        "$INSTALL_ERROR_JSON" >> $UPGRADE_LOG_FILE 2>&1
    local ret_val=$?

    trace_log_message "EXITED $FUNCNAME"
    return $ret_val
}

post_action() {
    trace_log_message "ENTERED $FUNCNAME"

    complete_job_to_rcm
    post_exit_code=$?
    if [ $post_exit_code -eq 0 ]; then
        trace_log_message "Post actions completed successfully"
    else
        trace_log_message "Post actions failed"
    fi

    trace_log_message "EXITED $FUNCNAME"
    return $post_exit_code
}

main() {
    while getopts :j:  opt
    do
        case $opt in
        j)  JOB_FILE_PATH="$OPTARG"
        ;;
        *)  trace_log_message "Incorrect usage"
            exit_with_retcode 1
        ;;
        esac
    done

    if [ -z "$JOB_FILE_PATH" ]; then
        trace_log_message "Job File not provided"
        exit_with_retcode 1
    fi
    if [ ! -f "$JOB_FILE_PATH" ]; then
        trace_log_message "File path: $JOB_FILE_PATH doesn't exist"
        exit_with_retcode 1
    fi
    #installer needs to be invoked from the same directory
    cd $SCRIPT_DIR

    upgrade_action
    upgrade_action_return_value=$?
    post_action || exit_with_retcode $?
    exit_with_retcode 0
}

main "$@"
