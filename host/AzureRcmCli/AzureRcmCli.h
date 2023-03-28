/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2019
+------------------------------------------------------------------------------------+
File        : AgentRcmCli.h

Description : Contains definitions related to command line interface AzureRcmCli.

+------------------------------------------------------------------------------------+
*/
#pragma once

namespace RcmCliOptions {

    const char RCM_CLI_OPT_HELP[] = "help";
    const char RCM_CLI_OPT_NOPROXY[] = "noproxy";
    const char RCM_CLI_OPT_CLEANUP[] = "cleanup";
    const char RCM_CLI_OPT_CONFPATH[] = "confpath";

    const char RCM_CLI_OPT_PORT[] = "port";
    const char RCM_CLI_OPT_ADDRESS[] = "address";
    const char RCM_CLI_OPT_VALIDATE_ROOTCERT[] = "validaterootcert";
    const char RCM_CLI_OPT_DISKID[] = "diskid";
    const char RCM_CLI_OPT_CONTAINER_TYPE[] = "containertype";

    const char RCM_CLI_OPT_REGISTER_MACHINE[] = "registermachine";
    const char RCM_CLI_OPT_UNREGISTER_MACHINE[] = "unregistermachine";
    const char RCM_CLI_OPT_REGISTER_SOURCE_AGENT[] = "registersourceagent";
    const char RCM_CLI_OPT_UNREGISTER_SOURCE_AGENT[] = "unregistersourceagent";
    const char RCM_CLI_OPT_RENEW_LOG_CONTAINER_SAS_URI[] = "renewlogcontainersasuri";

    const char RCM_CLI_OPT_REFRESH_PROXY_SETTINGS[] = "refreshproxysettings";
    const char RCM_CLI_OPT_GET_REPLICATION_SETTINGS[] = "getreplicationsettings";

    const char RCM_CLI_OPT_TESTVOLUMEINFOCOLLECTOR[] = "testvolumeinfocollector";
    const char RCM_CLI_OPT_TESTFAILOVERCLUSTERINFOCOLLECTOR[] = "testfailoverclusterinfocollector";

    const char RCM_CLI_OPT_GET_IMDS_METADATA[] = "getimdsmetadata";

    const char RCM_CLI_OPT_COMPLETE_AGENT_UPGRADE[] = "completeagentupgrade";
    const char RCM_CLI_OPT_AGENT_UPGRADE_EXIT_CODE[] = "agentupgradeexitcode";
    const char RCM_CLI_OPT_AGENT_UPGRADE_JOB_DETAIL[] = "agentupgradejobdetails";
    const char RCM_CLI_OPT_AGENT_UPGRADE_ERROR_FILE_PATH[] = "errorfilepath";
    const char RCM_CLI_OPT_AGENT_UPGRADE_UPLOAD_LOGS[] = "agentupgradelogsupload";
    const char RCM_CLI_OPT_AGENT_UPGRADE_LOGS_PATH[] = "agentupgradelogspath";

    const char RCM_CLI_OPT_GET_AGENT_CONFIG_INPUT[] = "getagentconfiginput";
    const char RCM_CLI_OPT_VERIFY_CLIENT_AUTH[] = "verifyclientauth";
    const char RCM_CLI_OPT_CONFIG_FILE[] = "configfile";
    const char RCM_CLI_OPT_GET_NON_CACHED_SETTINGS[] = "getnoncachedsettings";
    const char RCM_CLI_OPT_COMPLETE_AGENT_REGISTRATION[] = "completeagentregistration";
    const char RCM_CLI_OPT_LOG_FILE[] = "logfile";

    const char RCM_CLI_OPT_DIAGNOSE_AND_FIX[] = "diagnoseandfix";
}

const std::string biosIdMismatchError("does not match configured BIOS ID");
const std::string confFileNotFoundError("RCM client settings file not found");
const std::string agentBuildVersion(INMAGE_PRODUCT_VERSION_STR);

#define RCM_CLI_BASE_ERROR_CODE 0x78 // starts at 120
#define RCM_CLI_MAX_ERROR_CODE 0x95 // end at 149

// adding new error codes is deprected,
// use Rcm Client error codes directly for creating extended error json file
typedef enum RcmCliErrorCodes {
    RCM_CLI_UNKNOWN_ERROR = -1,
    RCM_CLI_SUCCESS = 0,
    RCM_CLI_SUCCEEDED_WITH_WARNINGS = 1,
    RCM_CLI_FAILED_WITH_ERRORS = 2,

    // -- start of legacy cli return codes --
    RCM_CLI_AAD_ERROR_APP_ID_NOT_FOUND = RCM_CLI_BASE_ERROR_CODE,
    RCM_CLI_AAD_ERROR_TIME_RANGE_MISMATCH,
    RCM_CLI_AZURE_AD_PRIVATE_KEY_NOT_FOUND,
    RCM_CLI_AAD_CONNECTION_FAILURE,
    RCM_CLI_DNS_CONNECTION_FAILURE,
    RCM_CLI_AAD_URI_CONNECTION_FAILURE,
    RCM_CLI_RCM_URI_CONNECTION_FAILURE,
    RCM_CLI_RCM_PROXY_AUTH_FAILURE,
    RCM_CLI_RCM_PROXY_CSLOGIN_NOT_SUPPORTED,
    RCM_CLI_RCM_PROXY_CLOUD_PAIRING_INCOMPLETE,
    RCM_CLI_INVALID_ARGUMENT = RCM_CLI_BASE_ERROR_CODE + 10,
    RCM_CLI_BIOS_ID_MISMATCH,
    RCM_CLI_CA_CERT_ERROR,
    RCM_CLI_INVALID_RCM_CONF_ENTRY,
    RCM_CLI_NO_SYSTEM_DISK_FOUND,
    RCM_CLI_NO_BOOT_DISK_FOUND,
    RCM_CLI_NO_PARENT_DISK_FOR_VOLUME,
    RCM_CLI_AAD_HTTP_ERROR,
    RCM_CLI_CONNECTION_FAILURE,
    RCM_CLI_COM_INITIALIZATION_FAILED,
    RCM_CLI_FAILED_TO_PERSIST_DISCOVERY_INFO, // RCM_CLI_BASE_ERROR_CODE + 20
    RCM_CLI_CAUGHT_EXCEPTION,
    RCM_CLI_AZURE_AD_AUTH_FAILURE,
    RCM_CLI_AZURE_AD_AUTH_CERT_NOT_FOUND,
    RCM_CLI_MACHINE_DOESNT_EXIST,
    RCM_CLI_MACHINE_IS_PROTECTED,
    RCM_CLI_DATA_DISKS_ON_MULTIPLE_CONTROLLERS,
    RCM_CLI_DUPLICATE_PROCESS,
    RCM_CLI_SERVICE_RUNNING_ERROR,
    RCM_CLI_DRIVER_NOT_LOADED,
    // --end of legacy cli rturn codes --
};
