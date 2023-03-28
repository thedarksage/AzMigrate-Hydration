/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File        : AzureRcmCli.cpp

Description : Implements the command line interface for AzureRcmCli.

+------------------------------------------------------------------------------------+
*/
#ifdef SV_WINDOWS
#include <Objbase.h>
#include "windows.h"
#include "processenv.h"
#endif
#include <ace/Init_ACE.h>
#include <boost/foreach.hpp>

#include "RcmConfigurator.h"
#include "common/Trace.h"
#include "logger.h"
#include "terminateonheapcorruption.h"
#include "ConverterErrorCodes.h"
#include "azurevmrecoverymanager.h"
#include "proxysettings.h"
#include "version.h"
#include "appcommand.h"
#include <fstream>
#include <iostream>
#include "scopeguard.h"
#include "AzureRcmCli.h"
#include "volumeinfocollector.h"
#include "AzureRcmCliException.h"
#include "ErrorLogger.h"
#include "cxtransport.h"
#include "AgentConfig.h"
#include "createpaths.h"
#include "CertUtil.h"
#include "portablehelpers.h"

#ifdef SV_WINDOWS
#include "Failoverclusterinfocollector.h"
#endif

#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/assign.hpp>

using namespace std;
using namespace RcmClientLib;
using namespace boost::chrono;
namespace po = boost::program_options;
using boost::property_tree::ptree;
using namespace RcmCliOptions;

bool ParseArgs(int argc, char *argv[], po::variables_map &commandOptions)
{
    po::options_description usage("Allowed options");
    usage.add_options()
        (RCM_CLI_OPT_HELP, "show usage")
        (RCM_CLI_OPT_REGISTER_MACHINE, "To register machine with RCM")
        (RCM_CLI_OPT_UNREGISTER_MACHINE, "To unregister machine with RCM")
        (RCM_CLI_OPT_REGISTER_SOURCE_AGENT, "To register source agent with RCM")
        (RCM_CLI_OPT_UNREGISTER_SOURCE_AGENT, "To unregister source agent with RCM")
        (RCM_CLI_OPT_RENEW_LOG_CONTAINER_SAS_URI, "To invoke renew log container sas uri api on RCM")
        (RCM_CLI_OPT_DISKID, po::value<std::string>(), "Source Agent Disk Identifier")
        (RCM_CLI_OPT_CONTAINER_TYPE, po::value<std::string>(), "Container Identifier")
        (RCM_CLI_OPT_CONFPATH, po::value<std::string>(), "File path to RcmInfo.conf")
        (RCM_CLI_OPT_REFRESH_PROXY_SETTINGS, "To update the proxy settings")
        (RCM_CLI_OPT_CLEANUP, "To cleanup config after unregister source agent with RCM")
        (RCM_CLI_OPT_GET_REPLICATION_SETTINGS, "fetch the replication settings from RCM")
        (RCM_CLI_OPT_NOPROXY, "To not use proxy settings, default is to use proxy settings if found")
        (RCM_CLI_OPT_VALIDATE_ROOTCERT, "To check if a Root CA cert of a giver server is present on local machine")
        (RCM_CLI_OPT_ADDRESS, po::value<std::string>(), "address of server")
        (RCM_CLI_OPT_PORT, po::value<std::string>(), "network port of server")
        (RCM_CLI_OPT_TESTVOLUMEINFOCOLLECTOR, "To run test disk discovery")
        (RCM_CLI_OPT_TESTFAILOVERCLUSTERINFOCOLLECTOR, "To run test of cluster discovery")
        (RCM_CLI_OPT_GET_IMDS_METADATA, "To get IMDS metadata")
        (RCM_CLI_OPT_COMPLETE_AGENT_UPGRADE, "Complete agent upgrade job")
        (RCM_CLI_OPT_AGENT_UPGRADE_EXIT_CODE, po::value<int>(), "Agent upgrade exit code")
        (RCM_CLI_OPT_AGENT_UPGRADE_JOB_DETAIL, po::value<std::string>(), "Agent upgrade job details file")
        (RCM_CLI_OPT_AGENT_UPGRADE_ERROR_FILE_PATH, po::value<std::string>(), "Agent upgrade error json file path")
        (RCM_CLI_OPT_AGENT_UPGRADE_UPLOAD_LOGS, "Upload Agent upgrade logs")
        (RCM_CLI_OPT_AGENT_UPGRADE_LOGS_PATH, po::value<std::string>(), "Agent upgrade logs folder path")
        (RCM_CLI_OPT_GET_AGENT_CONFIG_INPUT, "Get agent config input")
        (RCM_CLI_OPT_VERIFY_CLIENT_AUTH, "Verify Client Auth")
        (RCM_CLI_OPT_CONFIG_FILE, po::value<std::string>(), "Config File Path")
        (RCM_CLI_OPT_GET_NON_CACHED_SETTINGS, "Get Non-cached settings")
        (RCM_CLI_OPT_COMPLETE_AGENT_REGISTRATION, "Complete agent registration")
        (RCM_CLI_OPT_LOG_FILE, po::value<std::string>(), "Log File Path")
        (RCM_CLI_OPT_DIAGNOSE_AND_FIX, "Diagnose and Fix")
        ;

    po::store(po::parse_command_line(argc, argv, usage), commandOptions);
    po::notify(commandOptions);

    if (commandOptions.count("help") || commandOptions.empty()) {
        std::cout << usage << "\n";
        return false;
    }

    return true;
}

bool IsVxServiceRunning()
{
    std::string serviceName("svagents");
#ifdef SV_WINDOWS
    serviceName += ".exe";
#endif
    const uint32_t  MAX_TIME_SLEEP_IN_SECONDS = 5;
    const uint32_t  MAX_TIME_RETRIES_IN_SECONDS = 30;
    steady_clock::time_point endTime = steady_clock::now();
    endTime += seconds(MAX_TIME_RETRIES_IN_SECONDS);
    while (IsProcessRunning(serviceName, 0))
    {
        if (steady_clock::now() < endTime)
        {
            ACE_OS::sleep(MAX_TIME_SLEEP_IN_SECONDS); // sleep check again after few seconds
            continue;
        }

        DebugPrintf(SV_LOG_ERROR, "%s running. Exiting...\n", serviceName.c_str());
        return true;
    }

    return false;
}

#ifdef SV_WINDOWS
HRESULT InitializeCom()
{
    HRESULT hr;

    do
    {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

        if (hr != S_OK)
        {
            DebugPrintf("FAILED: CoInitializeEx() failed. hr = 0x%08x \n.", hr);
            break;
        }

        hr = CoInitializeSecurity(NULL,
                                    -1,
                                    NULL,
                                    NULL,
                                    RPC_C_AUTHN_LEVEL_CONNECT,
                                    RPC_C_IMP_LEVEL_IMPERSONATE,
                                    NULL,
                                    EOAC_NONE,
                                    NULL);


        if (hr == RPC_E_TOO_LATE)
        {
            hr = S_OK;
        }

        if (hr != S_OK)
        {
            DebugPrintf("FAILED: CoInitializeSecurity() failed. hr = 0x%08x \n.", hr);
            CoUninitialize();
            break;
        }
    } while (false);

    return hr;
}

void UninitializeCom()
{
    CoUninitialize();
}

#endif

/// \brief a callback function for the Trace logger in recovery lib 
static void LogCallback(unsigned int logLevel, const char *msg)
{
    string format;
    if (logLevel == SV_LOG_ERROR)
        format = "FAILED ";
    else if (logLevel == SV_LOG_WARNING)
        format = "WARNING ";

    format += "%s";
    DebugPrintf(format.c_str(), msg);
    return;
}

void SetupLogging(LocalConfigurator &localconfigurator, std::string &logFilePath)
{
    boost::filesystem::path logPath(logFilePath);

    CreatePaths::createPathsAsNeeded(logPath);

    SetDaemonMode();
    SetLogFileName(logPath.string().c_str());
    SetLogLevel(SV_LOG_ALWAYS);

    Trace::Init(logPath.string(),
        LogLevelAlways,
        boost::bind(&LogCallback, _1, _2));

    return;
}

int GetReturnCode(int32_t errorCode)
{
    switch (errorCode)
    {
    case RCM_CLIENT_BIOSID_MISMATCH_ERROR:
        return RCM_CLI_BIOS_ID_MISMATCH;
    case RCM_CLIENT_DATA_CACHER_ERROR :
        return RCM_CLI_FAILED_TO_PERSIST_DISCOVERY_INFO;
    case RCM_CLIENT_EMPTY_BIOSID_ERROR:
    case RCM_CLIENT_EMPTY_HOSTID_ERROR:
    case RCM_CLIENT_EMPTY_MANAGEMENTID_ERROR:
        return RCM_CLI_INVALID_RCM_CONF_ENTRY;
    case RCM_CLIENT_CAUGHT_EXCEPTION:
        return RCM_CLI_CAUGHT_EXCEPTION;
    case RCM_CLIENT_SERVER_CERT_VALIDATION_ERROR:
        return RCM_CLI_CA_CERT_ERROR;
    case CONVERTER_FOUND_NO_SYSTEM_DISKS:
        return RCM_CLI_NO_SYSTEM_DISK_FOUND;
    case CONVERTER_FOUND_NO_BOOT_DISKS:
        return RCM_CLI_NO_BOOT_DISK_FOUND;
    case CONVERTER_NO_PARENT_DISK_FOR_VOLUME:
        return RCM_CLI_NO_PARENT_DISK_FOR_VOLUME;
    case RCM_CLIENT_POST_FAILED:
    case RCM_CLIENT_COULDNT_RESOLVE_PROXY:
    case RCM_CLIENT_ERROR_NETWORK_TIMEOUT:
        return RCM_CLI_CONNECTION_FAILURE;
    case RCM_CLIENT_COULDNT_CONNECT:
        return RCM_CLI_RCM_URI_CONNECTION_FAILURE;
    case RCM_CLIENT_COULDNT_RESOLVE_HOST:
        return RCM_CLI_DNS_CONNECTION_FAILURE;
    case RCM_CLIENT_GET_BEARER_TOKEN_FAILED:
    case RCM_CLIENT_AAD_ERROR_CACHED_TOKEN_INVALID:
    case RCM_CLIENT_CREATE_JWT_FAILED:
        return RCM_CLI_AZURE_AD_AUTH_FAILURE;
    case RCM_CLIENT_AAD_AUTH_CERT_NOT_FOUND:
        return RCM_CLI_AZURE_AD_AUTH_CERT_NOT_FOUND;
    case RCM_CLIENT_PROTECTION_DISABLED:
        return RCM_CLI_MACHINE_DOESNT_EXIST;
    case RCM_CLIENT_PROTECTION_ENABLED:
        return RCM_CLI_MACHINE_IS_PROTECTED;
    case CONVERTER_FOUND_MULTIPLE_CONTROLLERS_FOR_DATA_DISKS:
        return RCM_CLI_DATA_DISKS_ON_MULTIPLE_CONTROLLERS;
    case RCM_CLIENT_EMPTY_DRIVER_VERSION_ERROR:
        return RCM_CLI_DRIVER_NOT_LOADED;
    case RCM_CLIENT_AAD_ERROR_APP_ID_NOT_FOUND:
        return RCM_CLI_AAD_ERROR_APP_ID_NOT_FOUND;
    case RCM_CLIENT_AAD_ERROR_SYSTEM_TIME_OUT_OF_SYNC:
        return RCM_CLI_AAD_ERROR_TIME_RANGE_MISMATCH;
    case RCM_CLIENT_AAD_PRIVATE_KEY_NOT_FOUND:
        return RCM_CLI_AZURE_AD_PRIVATE_KEY_NOT_FOUND;
    case RCM_CLIENT_AAD_POST_FAILED:
    case RCM_CLIENT_AAD_COULDNT_RESOLVE_PROXY:
        return RCM_CLI_AAD_CONNECTION_FAILURE;
    case RCM_CLIENT_AAD_COULDNT_CONNECT:
        return RCM_CLI_AAD_URI_CONNECTION_FAILURE;
    case RCM_CLIENT_AAD_COULDNT_RESOLVE_HOST:
        return RCM_CLI_DNS_CONNECTION_FAILURE;
    case RCM_CLIENT_PROXY_IMPL_LOGIN_AUTH_FAILURE:
        return RCM_CLI_RCM_PROXY_AUTH_FAILURE;
    case RCM_CLIENT_PROXY_IMPL_LOGIN_FORBIDDEN:
        return RCM_CLI_RCM_PROXY_CSLOGIN_NOT_SUPPORTED;
    case RCM_CLIENT_PROXY_IMPL_CLOUD_PAIRING_INCOMPLETE:
        return RCM_CLI_RCM_PROXY_CLOUD_PAIRING_INCOMPLETE;
    case RCM_CLIENT_AAD_ERROR_WITH_HTTP_RESPONSE:
        return RCM_CLI_AAD_HTTP_ERROR;
    case RCM_CLIENT_REGISTRATION_NOT_SUPPORTED:
    case RCM_CLIENT_INVALID_ARGUMENT:
        return RCM_CLI_INVALID_ARGUMENT;
    default:
        break;
    }

    return errorCode;
}

bool QuitRequested(int seconds)
{
    boost::this_thread::sleep(boost::posix_time::seconds(seconds));
    return false;
}

int WriteErrorsToJsonFile(int status, int ErrorCode)
{
    const std::string defaultWarningMsg = "Configuration succeeded with warnings.";
    const std::string defaultErrorMsg = "Configuration failed with internal error.";
    int finalStatus = RCM_CLI_FAILED_WITH_ERRORS;

    ExtendedErrorLogger::ExtendedError cliError;
    cliError.default_message = defaultErrorMsg;
    cliError.error_params["VMName"] = RcmConfigurator::GetFqdn();
    cliError.error_params["ErrorCode"] = boost::lexical_cast<std::string>(ErrorCode);

    switch (status)
    {
    case RCM_CLI_SUCCESS:
    case RCM_CLI_RCM_PROXY_CSLOGIN_NOT_SUPPORTED:
    case RCM_CLI_RCM_PROXY_CLOUD_PAIRING_INCOMPLETE:
    case RCM_CLI_SERVICE_RUNNING_ERROR:
    case RCM_CLIENT_DIAGNOSE_AND_FIX_FAILED:
        return status;
    case RCM_CLI_AAD_ERROR_APP_ID_NOT_FOUND:
        cliError.error_name = "ConfiguratorRecoveryServicesVaultDeleted";
        break;
    case RCM_CLI_AAD_ERROR_TIME_RANGE_MISMATCH:
        cliError.error_name = "ConfiguratorTimeRangeMismatch";
        break;
    case RCM_CLI_AZURE_AD_PRIVATE_KEY_NOT_FOUND:
        cliError.error_name = "ConfiguratorPrivateKeyNotFound";
        break;
    case RCM_CLI_AAD_CONNECTION_FAILURE:
        cliError.error_name = "ConfiguratorConfigurationFailed";
        break;
    case RCM_CLI_DNS_CONNECTION_FAILURE:
        cliError.error_name = "ConfiguratorConfigurationFailedWithDNSissue";
        break;
    case RCM_CLI_AAD_URI_CONNECTION_FAILURE:
        cliError.error_name = "ConfiguratorConfigurationFailedWithNoConnectivityToOffice365Ips";
        break;
    case RCM_CLI_RCM_URI_CONNECTION_FAILURE:
        cliError.error_name = "ConfiguratorConfigurationFailedWithNoConnectivityToRcmIps";
        break;
    case RCM_CLI_RCM_PROXY_AUTH_FAILURE:
        cliError.error_name = "ConfiguratorRcmProxyAuthenticationFailure";
        break;
    case RCM_CLI_INVALID_ARGUMENT:
        cliError.error_name = "ConfiguratorInvalidCommandLineParameter";
        break;
    case RCM_CLI_BIOS_ID_MISMATCH:
        cliError.error_name = "ConfiguratorSystemIdMismatch";
        break;
    case RCM_CLI_CA_CERT_ERROR:
        cliError.error_name = "ConfiguratorTrustedRootCertificatesNotFound";
        break;
    case RCM_CLI_INVALID_RCM_CONF_ENTRY:
        cliError.error_name = "ConfiguratorInternalErrorIncorrectConfigurationParameters";
        break;
    case RCM_CLI_NO_SYSTEM_DISK_FOUND:
        cliError.error_name = "ConfiguratorSystemDiskNotFound";
        break;
    case RCM_CLI_NO_BOOT_DISK_FOUND:
        cliError.error_name = "ConfiguratorBootDiskNotFound";
        break;
    case RCM_CLI_NO_PARENT_DISK_FOR_VOLUME:
        cliError.error_name = "ConfiguratorVolumeDiskMapFailed";
        break;
    case RCM_CLI_AAD_HTTP_ERROR:
        cliError.error_name = "ConfiguratorAADHttpError";
        break;;
    case RCM_CLI_CONNECTION_FAILURE:
        cliError.error_name = "ConfiguratorRecoveryServiceConnectionFailed";
        break;
    case RCM_CLI_COM_INITIALIZATION_FAILED:
        cliError.error_name = "ConfiguratorComInitializationFailed";
        break;
    case RCM_CLI_FAILED_TO_PERSIST_DISCOVERY_INFO:
        cliError.error_name = "ConfiguratorWriteToInstallLocationFailed";
        break;
    case RCM_CLI_CAUGHT_EXCEPTION:
        cliError.error_name = "ConfiguratorUnknownException";
        break;
    case RCM_CLI_AZURE_AD_AUTH_FAILURE:
        cliError.error_name = "ConfiguratorAADAuthFailure";
        break;
    case RCM_CLI_AZURE_AD_AUTH_CERT_NOT_FOUND:
        cliError.error_name = "ConfiguratorAuthCertNotFound";
        break;
    case RCM_CLI_MACHINE_DOESNT_EXIST:
        cliError.error_name = "ConfiguratorMachineDoesNotExist";
        break;
    case RCM_CLI_MACHINE_IS_PROTECTED:
        cliError.error_name = "ConfiguratorMachineAlreadyProtected";
        break;
    case RCM_CLI_DATA_DISKS_ON_MULTIPLE_CONTROLLERS:
        cliError.error_name = "ConfiguratorFoundNonAzureStorageBlobBasedDisks";
        break;
    case RCM_CLI_DUPLICATE_PROCESS:
        cliError.error_name = "ConfiguratorProcessAlreadyRunning";
        break;
    case RCM_CLI_DRIVER_NOT_LOADED:
        cliError.error_name = "InstallerConfigurationFailedWithInternalError";
        break;
    case RCM_CLIENT_ERROR_HTTP_RESPONSE_UNKNOWN:
        cliError.error_name = "ConfiguratorRecoveryServiceUnknownHttpError";
        break;
    case RCM_CLIENT_REGISTRATION_NOT_ALLOWED:
        cliError.error_name = "ConfiguratorRegistrationNotAllowed";
        break;
    case RCM_CLIENT_APPLIANCE_MISMATCH:
        cliError.error_name = "ConfiguratorApplianceMismatch";
        break;
    case RCM_CLIENT_INVALID_PERSISTED_SOURCE_CONFIG:
    {
        cliError.error_name = "ConfiguratorInvalidPersistedSourceConfig";
        std::string sourceConfigPath = RcmConfigurator::getInstance()->GetSourceAgentConfigPath();
        cliError.error_params["PersistedConfigPath"] = sourceConfigPath;
    }
        break;
    case RCM_CLIENT_INVALID_SOURCE_CONFIG_BIOS_ID_OR_FQDN:
        cliError.error_name = "ConfiguratorInvalidSourceConfigBiosIdOrFqdn";
        break;
    case RCM_CLIENT_VAULT_MISMATCH:
    {
        LocalConfigurator lc;
        cliError.error_name = "ConfiguratorVaultMismatch";
        std::string installPath = lc.getInstallPath();
        if (!boost::ends_with(installPath, ACE_DIRECTORY_SEPARATOR_STR_A))
            installPath += ACE_DIRECTORY_SEPARATOR_STR_A;
#ifdef SV_WINDOWS
        std::string configuratorCmd = "'" + installPath
            + "UnifiedAgentConfigurator' /Unconfigure true";
#else
        std::string configuratorCmd = "'" + installPath
            + "bin/UnifiedAgentConfigurator.sh' -q -U true -c CSPrime";
#endif
        cliError.error_params["ConfiguratorCommand"] = configuratorCmd;
    }
        break;
    // Warnings
    case RCM_CLIENT_GET_NON_CACHED_SETTINGS_WARNING:
        cliError.error_name = "ConfiguratorNonCachedSettingsWarning";
        cliError.default_message = defaultWarningMsg;
        finalStatus = RCM_CLI_SUCCEEDED_WITH_WARNINGS;
        break;
    case RCM_CLIENT_INVALID_CLUSTER_SERVICE_STATUS:
        cliError.error_name = "ConfiguratorInvalidClusterServiceStatus";
        break;
    case RCM_CLIENT_FAILOVER_CLUSTER_PROPERTIES_COLLECTOR_ERROR:
        cliError.error_name = "ConfiguratorCollectFailoverClusterPropertiesError";
        break;
    case RCM_CLIENT_FAILOVER_CLUSTER_EMPTY_ID:
        cliError.error_name = "ConfiguratorEmptyClusterId";
        break;
    case RCM_CLIENT_FAILOVER_CLUSTER_EMPTY_NAME:
        cliError.error_name = "ConfiguratorEmptyClusterName";
        break;
    case RCM_CLIENT_FAILOVER_CLUSTER_EMPTY_TYPE:
        cliError.error_name = "ConfiguratorEmptyClusterType";
        break;
    case RCM_CLIENT_FAILOVER_CLUSTER_EMPTY_CURRENT_NODE_NAME:
        cliError.error_name = "ConfiguratorEmptyClusterCurrentNodeName";
        break;
    case RCM_CLIENT_FAILOVER_CLUSTER_EMPTY_MANAGEMENT_ID:
        cliError.error_name = "ConfiguratorEmptyClusterManagementId";
        break;
    case RCM_CLIENT_FAILOVER_CLUSTER_EMPTY_NODE_LIST:
        cliError.error_name = "ConfiguratorEmptyClusterNodeList";
        break;
    default:
        cliError.error_name = "ConfiguratorInternalError";
        break;
    }

    try {
        DebugPrintf(SV_LOG_DEBUG, "%s: Error name : %s\n", FUNCTION_NAME, cliError.error_name.c_str());
        // split camel case error name to multi-string
        std::cerr << "Error: " << boost::regex_replace(cliError.error_name, boost::regex("(?<=[a-z])(?=[A-Z])"), "  $1") << std::endl;
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: caught exception : %s\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: unknown exception.\n", FUNCTION_NAME);
    }

    ExtendedErrorLogger::ErrorLogger::LogExtendedError(cliError);
    return finalStatus;
}

void InitialiseErrorJsonFile(const po::variables_map & commandOptions)
{
    if (commandOptions.count(RCM_CLI_OPT_AGENT_UPGRADE_ERROR_FILE_PATH))
    {
        ExtendedErrorLogger::ErrorLogger::Init(
            commandOptions[RCM_CLI_OPT_AGENT_UPGRADE_ERROR_FILE_PATH]
            .as<std::string>());
        return;
    }

    DebugPrintf(SV_LOG_INFO,
        "%s: Using default path for error json file\n",
        FUNCTION_NAME);

    std::string ErrorJsonFilePath;
#ifdef SV_WINDOWS
    std::vector<char> buffer(32 * 1024);
    size_t size = ExpandEnvironmentStrings(
        "%ProgramData%\\ASRSetupLogs\\InstallerErrors.json",
        &buffer[0],
        (DWORD)buffer.size());
    if (size == 0)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: ExpandEnvironmentStrings failed with error %d\n",
            FUNCTION_NAME,
            GetLastError());
        ErrorJsonFilePath = "C:\\ProgramData\\ASRSetupLogs\\InstallerErrors.json";
    }
    else
    {
        buffer[size] = '\0';
        ErrorJsonFilePath = std::string(&buffer[0]);
    }
#else
    ErrorJsonFilePath = "/var/log/InstallerErrors.json";
#endif

    ExtendedErrorLogger::ErrorLogger::Init(ErrorJsonFilePath);
}

std::string GetClientRequestId(const std::string& jobDetails)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string clientRequestId;
    std::ifstream ifs(jobDetails.c_str());
    if (!ifs.good())
    {
        DebugPrintf(SV_LOG_ERROR, "Unable to open the file %s\n", jobDetails.c_str());
        throw std::runtime_error("Unable to open the file " + jobDetails);
    }
    std::string jobstr((std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>());
    RcmJob job = JSON::consumer<RcmJob>::convert(jobstr);
    clientRequestId = job.Context.ClientRequestId;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return clientRequestId;
}

void UploadAgentUpgradeFile(CxTransport& cxTransport, boost::filesystem::path& localPath,
    const std::string& localFile, boost::filesystem::path& remotePath,
    const std::string& remoteFile)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string localFilePath, remoteProgFilePath, remoteCompFilePath, utcTime, fileExtension;

    ListFile::listFileGlob(localPath / localFile, localFilePath, false);
    if (localFilePath.empty())
    {
        DebugPrintf(SV_LOG_INFO, "Unable to find file %s in directory : %s\n",
            localFile.c_str(), localPath.string().c_str());
    }
    else
    {
        localFilePath.erase(std::remove(localFilePath.begin(), localFilePath.end(), '\n'),
            localFilePath.end());
        remoteProgFilePath = (remotePath / (AutoUpgradeLogParams::Remote::Prefix +
            remoteFile)).string();
        remoteCompFilePath = (remotePath / remoteFile).string();
        utcTime = boost::posix_time::to_iso_extended_string(
            boost::posix_time::second_clock::universal_time());
        // Windows doesn't allow : in file name, changing : to _
        boost::algorithm::replace_all(utcTime, ":", "_");
        fileExtension = boost::filesystem::extension(remoteCompFilePath);
        boost::algorithm::replace_last(remoteCompFilePath, fileExtension,
            utcTime + fileExtension);

        if (cxTransport.putFile(remoteProgFilePath, localFilePath, COMPRESS_NONE, true))
        {
            DebugPrintf(SV_LOG_INFO, "putFile Succeded. Local path : %s Remote path : %s\n",
                localFilePath.c_str(), remoteProgFilePath.c_str());

            boost::tribool tb = cxTransport.renameFile(remoteProgFilePath,
                remoteCompFilePath, COMPRESS_NONE);

            if (!boost::indeterminate(tb) && tb)
            {
                DebugPrintf(SV_LOG_INFO, "Rename file succeded from : %s to : %s\n",
                    remoteProgFilePath.c_str(), remoteCompFilePath.c_str());
            }
        }
        else
        {
            DebugPrintf(SV_LOG_INFO, "putFile Failure. Local path : %s Remote path : %s\n",
                localFilePath.c_str(), remoteProgFilePath.c_str());
            DebugPrintf(SV_LOG_ERROR, "putFile status: %s\n",  cxTransport.status());
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

SVSTATUS UploadAgentUpgradeLogs(const std::string& logsPath, const std::string& jobDetails)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string dataPathTransportType, dataPathTransportSettings, clientRequestId;

    RcmConfigurator::getInstance()->GetDataPathTransportSettings(dataPathTransportType,
        dataPathTransportSettings);

    if (!boost::iequals(dataPathTransportType, TRANSPORT_TYPE_PROCESS_SERVER))
    {
        std::string errMsg = "The transport type " + dataPathTransportType +
            " is not implemented.";
        DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
        throw std::runtime_error(errMsg.c_str());
    }

    clientRequestId = GetClientRequestId(jobDetails);
    if (clientRequestId.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "Client Request ID obtained from job file is empty\n");
        clientRequestId = RcmConfigurator::getInstance()->GetClientRequestId();
    }
    DebugPrintf(SV_LOG_DEBUG, "Client Request ID : %s\n", clientRequestId.c_str());
    // Windows doesn't allow : in directory name, changing : to _
    boost::algorithm::replace_all(clientRequestId, ":", "_");
    boost::filesystem::path remotePath(RcmConfigurator::getInstance()->GetTelemetryFolderPathForOnPremToAzure());
    remotePath /= clientRequestId;

    ProcessServerTransportSettings psTransportSettings =
        JSON::consumer<ProcessServerTransportSettings>::convert(dataPathTransportSettings, true);
    TRANSPORT_CONNECTION_SETTINGS tcs;
    tcs.ipAddress = psTransportSettings.GetIPAddress();
    tcs.sslPort = boost::lexical_cast<std::string>(psTransportSettings.Port);
    CxTransport cxTransport(TRANSPORT_PROTOCOL_HTTP, tcs, true);

    for (int i = 0; i < sizeof(AutoUpgradeLogParams::Local::InstallFiles)/
        sizeof(AutoUpgradeLogParams::Local::InstallFiles[0]); i++)
    {
        boost::filesystem::path localPath(logsPath);

        UploadAgentUpgradeFile(cxTransport, localPath, AutoUpgradeLogParams::Local::InstallFiles[i],
            remotePath, AutoUpgradeLogParams::Remote::InstallFiles[i]);
    }

    for (int i = 0; i < sizeof(AutoUpgradeLogParams::Local::AgentFiles)/
        sizeof(AutoUpgradeLogParams::Local::AgentFiles[0]); i++)
    {
        boost::filesystem::path localPath(AutoUpgradeLogParams::Local::AgentLogDir);

        UploadAgentUpgradeFile(cxTransport, localPath, AutoUpgradeLogParams::Local::AgentFiles[i],
            remotePath, AutoUpgradeLogParams::Remote::AgentFiles[i]);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

void ValidateRootCert(std::set<std::string> addrList=std::set<std::string>(),
    const std::string& port=std::string("443"))
{
    if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
    {
        std::cout << "option " << RCM_CLI_OPT_VALIDATE_ROOTCERT
            << " not valid for on-prem machines." << std::endl;
        DebugPrintf(SV_LOG_DEBUG,
            "option %s not valid for on-prem machines.\n",
            RCM_CLI_OPT_VALIDATE_ROOTCERT);
        return;
    }

    if (addrList.empty())
    {
        std::cout << "No address provided for option " << RCM_CLI_OPT_VALIDATE_ROOTCERT
            << ". Fetching service addresses from settings." << std::endl;
        DebugPrintf(SV_LOG_DEBUG,
            "No address provided for option %s. Fetching service addresses from settings.\n",
            RCM_CLI_OPT_VALIDATE_ROOTCERT);

        std::string aadUri = RcmConfigurator::getInstance()->GetAzureADServiceUri();
        std::string rcmUri = RcmConfigurator::getInstance()->GetRcmServiceUri();
        if (aadUri.empty() || rcmUri.empty())
        {
            DebugPrintf(SV_LOG_ERROR,
                "Empty service addresses from settings. AADUri %s, RCMUri %s\n",
                aadUri.c_str(), rcmUri.c_str());
            std::cerr << "Empty service addresses from settings.AADUri " << aadUri << " RCMUri " << rcmUri << std::endl;
        }
        else
        {
            addrList.insert(aadUri);
            addrList.insert(rcmUri);
        }

        SVSTATUS status = RcmConfigurator::getInstance()->GetUrlsForAccessCheck(addrList);
        if (SVS_OK != status)
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to get URLs from cached settings.\n");
            std::cerr << "Failed to get URLs from cached settings."<< std::endl;
        }
    }

    std::set<std::string>::iterator addrListIter = addrList.begin();
    for (/**/; addrListIter != addrList.end(); addrListIter++)
    {
        CertUtil certutil(*addrListIter, port);
        if (!certutil.get_last_error().empty())
        {
            std::cerr << "Failed to connect to server " << *addrListIter << " with error : " << certutil.get_last_error() << "." << std::endl;
        }
        else
        {
            std::string errmsg;
            if (certutil.validate_root_cert(errmsg))
            {
                std::cout << "For server " << *addrListIter << " " << errmsg << std::endl;
            }
            else
            {
                std::cerr << "For server " << *addrListIter << " " << errmsg << std::endl;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    TerminateOnHeapCorruption();
    ACE::init();

#ifdef SV_UNIX
    umask(S_IWOTH);
#endif

    int status = 0; // return code of cli
    int errorCode = 0; // error code from RcmLib
    SVSTATUS ret = SVS_OK;

    std::cout << std::endl << "Build Version: " << agentBuildVersion << std::endl;

    try {
        po::variables_map commandOptions;
        if (!ParseArgs(argc, argv, commandOptions))
        {
             return RCM_CLI_INVALID_ARGUMENT;
        }

        LocalConfigurator localconfigurator;
        std::string logPath = localconfigurator.getLogPathname() +
            ACE_DIRECTORY_SEPARATOR_STR_A + "AzureRcmCli.log";
        if (commandOptions.count(RCM_CLI_OPT_LOG_FILE))
        {
            logPath = commandOptions[RCM_CLI_OPT_LOG_FILE].as<std::string>();
        }
        SetupLogging(localconfigurator, logPath);

        DebugPrintf(SV_LOG_INFO,
                    "%s: agent build version %s\n",
                    FUNCTION_NAME,
                    agentBuildVersion.c_str());

        if (IsProcessRunning(basename_r(argv[0])))
        {
            DebugPrintf(SV_LOG_ERROR, "AzureRcmCli already running. Exiting...\n");
            throw AzureRcmCliException(
                "AzureRcmCli already running",
                RCM_CLI_DUPLICATE_PROCESS);
        }

        if (!commandOptions.count(RCM_CLI_OPT_TESTVOLUMEINFOCOLLECTOR) &&
            !commandOptions.count(RCM_CLI_OPT_GET_IMDS_METADATA) &&
            !commandOptions.count(RCM_CLI_OPT_REGISTER_SOURCE_AGENT) &&
            !commandOptions.count(RCM_CLI_OPT_COMPLETE_AGENT_UPGRADE) &&
            !commandOptions.count(RCM_CLI_OPT_AGENT_UPGRADE_UPLOAD_LOGS) &&
            !commandOptions.count(RCM_CLI_OPT_VALIDATE_ROOTCERT) &&
            !commandOptions.count(RCM_CLI_OPT_GET_AGENT_CONFIG_INPUT) &&
            !commandOptions.count(RCM_CLI_OPT_RENEW_LOG_CONTAINER_SAS_URI))
        {
            if (IsVxServiceRunning())
            {
                throw AzureRcmCliException(
                    "Vxagent service is already running",
                    RCM_CLI_SERVICE_RUNNING_ERROR);
            }
        }

        if(commandOptions.count(RCM_CLI_OPT_CLEANUP) ||
           commandOptions.count(RCM_CLI_OPT_REGISTER_SOURCE_AGENT) ||
           commandOptions.count(RCM_CLI_OPT_UNREGISTER_SOURCE_AGENT) ||
           commandOptions.count(RCM_CLI_OPT_REGISTER_MACHINE) ||
           commandOptions.count(RCM_CLI_OPT_UNREGISTER_MACHINE) ||
           commandOptions.count(RCM_CLI_OPT_VALIDATE_ROOTCERT) ||
           commandOptions.count(RCM_CLI_OPT_COMPLETE_AGENT_UPGRADE) ||
           commandOptions.count(RCM_CLI_OPT_VERIFY_CLIENT_AUTH) ||
           commandOptions.count(RCM_CLI_OPT_GET_NON_CACHED_SETTINGS) ||
           commandOptions.count(RCM_CLI_OPT_COMPLETE_AGENT_REGISTRATION))
        {
            InitialiseErrorJsonFile(commandOptions);
        }
            

#ifdef SV_WINDOWS
        HRESULT hr = InitializeCom();
        if (S_OK != hr)
        {
            std::cerr << "Error: Failed to initialize COM. hr = " << std::hex << hr << std::endl;
            throw AzureRcmCliException(
            "Failed to initialize COM",
            RCM_CLI_COM_INITIALIZATION_FAILED);
        }

        SCOPE_GUARD uninitComGuard = MAKE_SCOPE_GUARD(boost::bind(UninitializeCom));
#endif

        if (commandOptions.count(RCM_CLI_OPT_CLEANUP))
        {
            DebugPrintf(SV_LOG_INFO, "Running cleanup for source agent.\n");

            AzureVmRecoveryManager::ResetReplicationState();

            if (!DeleteProtectedDeviceDetails())
                status = RCM_CLI_BASE_ERROR_CODE;

            throw AzureRcmCliException("Cleanup failed.", status);
        }

        if (commandOptions.count(RCM_CLI_OPT_TESTVOLUMEINFOCOLLECTOR))
        {
            try {
                SetDaemonMode(false);

                VolumeSummaries_t volumeSummaries;
                VolumeDynamicInfos_t volumeDynamicInfos;

#ifdef SV_UNIX
                VolumeInfoCollector volumeCollector((DeviceNameTypes_t)GetDeviceNameTypeToReport(), true);
#else
                VolumeInfoCollector volumeCollector;
#endif
                volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, true);

            }
            catch (std::exception& e) {
                std::cout << "caught " << e.what() << '\n';
            }
            catch (...) {
                std::cout << "caught unknow exception\n";
            }

            throw AzureRcmCliException(
                    RCM_CLI_OPT_TESTVOLUMEINFOCOLLECTOR,
                    status);
        }

        if (commandOptions.count(RCM_CLI_OPT_TESTFAILOVERCLUSTERINFOCOLLECTOR))
        {
            std::stringstream errMsg;

            if (!RcmConfigurator::getInstance()->IsAzureToAzureReplication())
            {
                errMsg << "Not applicable as called in non A2A scenario";

                throw AzureRcmCliException(
                    errMsg.str(),
                    SVE_ABORT);

            }
#ifdef SV_WINDOWS
            SetDaemonMode(false);

            DebugPrintf(SV_LOG_DEBUG, "%s: starting failover cluster discovery\n", FUNCTION_NAME);

            FailoverClusterInfo clusterInfo;
            if (!clusterInfo.IsClusterNode())
            {
                errMsg << "InValid cluster service status. Error is: " << clusterInfo.GetLastErrorMessage();

                throw AzureRcmCliException(
                    errMsg.str(),
                    SVE_ABORT);
            }

            SVSTATUS status = SVS_OK;
            clusterInfo.AddFailoverClusterProperty(FailoverCluster::FAILOVER_CLUSTER_TYPE, FailoverCluster::DEFAULT_FAILOVER_CLUSTER_TYPE);
            status = clusterInfo.CollectFailoverClusterProperties(true);

            if (status != SVS_OK)
            {
                errMsg << "failed to collect the cluster info with error: " << clusterInfo.GetLastErrorMessage();
            }
            else
            {
                clusterInfo.dumpInfo();
                DebugPrintf(SV_LOG_DEBUG, "%s: finished failover cluster discovery\n", FUNCTION_NAME);
            }
#else 
            errMsg << "Not applicable as called in non Windows system";
#endif

            throw AzureRcmCliException(
                errMsg.str().empty()? RCM_CLI_OPT_TESTFAILOVERCLUSTERINFOCOLLECTOR : errMsg.str(),
                errMsg.str().empty()? SVS_OK: SVE_ABORT);
        }

        if (commandOptions.count(RCM_CLI_OPT_GET_IMDS_METADATA))
        {
            try {
                SetDaemonMode(false);

                std::string imds = GetImdsMetadata();
                std::cout << imds << std::endl;
            }
            catch (std::exception& e) {
                std::cerr << "caught " << e.what() << '\n';
            }
            catch (...) {
                std::cerr << "caught unknown exception\n";
            }

            throw AzureRcmCliException(
                    RCM_CLI_OPT_GET_IMDS_METADATA,
                    status);
        }

        if (commandOptions.count(RCM_CLI_OPT_GET_AGENT_CONFIG_INPUT))
        {
            std::cout << RCM_CLI_OPT_GET_AGENT_CONFIG_INPUT << ":" << AgentConfigHelpers::GetAgentConfigInput() << std::endl;

            throw AzureRcmCliException(
                RCM_CLI_OPT_GET_AGENT_CONFIG_INPUT,
                status);
        }

        std::string strKernelVersion;
        GetKernelVersion(strKernelVersion);

        DebugPrintf(SV_LOG_ALWAYS,
            "%s: kernel version %s.\n",
            FUNCTION_NAME,
            strKernelVersion.c_str());

        SCOPE_GUARD uninitGuard = MAKE_SCOPE_GUARD(boost::bind(RcmConfigurator::Uninitialize));
        RcmConfigurator::Initialize();
        RcmConfigurator::EnableVerboseLogging();

        RcmClientPtr rcmClientPtr = RcmConfigurator::getInstance()->GetRcmClient();

        if (commandOptions.count(RCM_CLI_OPT_REGISTER_SOURCE_AGENT))
        {
            ret = rcmClientPtr->RegisterSourceAgent();
        }
        else if (commandOptions.count(RCM_CLI_OPT_UNREGISTER_SOURCE_AGENT))
        {
            ret = rcmClientPtr->UnregisterSourceAgent();
        }
        else if (commandOptions.count(RCM_CLI_OPT_REGISTER_MACHINE))
        {
            bool bDoNotUseProxy = commandOptions.count(RCM_CLI_OPT_NOPROXY) > 0;
            QuitFunction_t qf = QuitRequested;
            ret = RcmConfigurator::getInstance()->RegisterMachine(qf, bDoNotUseProxy);

            if (RCM_CLIENT_SERVER_CERT_VALIDATION_ERROR == rcmClientPtr->GetErrorCode())
            {
                ValidateRootCert();
            }
        }
        else if (commandOptions.count(RCM_CLI_OPT_UNREGISTER_MACHINE))
        {
            ret = rcmClientPtr->UnregisterMachine();
        }
        else if (commandOptions.count(RCM_CLI_OPT_REFRESH_PROXY_SETTINGS))
        {
            std::string ip, port, bypasslist;
            if (GetInternetProxySettings(ip, port, bypasslist))
            {
                DebugPrintf(SV_LOG_INFO,
                    "Updating proxy settings that are auto discovered from IE or environment.\n");

                PersistProxySetting(localconfigurator.getProxySettingsPath(),
                    ip, port, bypasslist);

                ret = SVS_OK;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Not updating proxy settings.\n");
            }
        }
        else if (commandOptions.count(RCM_CLI_OPT_VALIDATE_ROOTCERT))
        {
            std::string addr, port("443");
            std::set<std::string> addrList;

            if (commandOptions.count(RCM_CLI_OPT_ADDRESS))
            {
                addr = commandOptions[RCM_CLI_OPT_ADDRESS].as<std::string>();
                port = (commandOptions.count(RCM_CLI_OPT_PORT)) ? commandOptions[RCM_CLI_OPT_PORT].as<std::string>() : port;
                addrList.insert(addr);
            }

            ValidateRootCert(addrList, port);
        }
        else if (commandOptions.count(RCM_CLI_OPT_GET_REPLICATION_SETTINGS))
        {

            std::string settings;
            ret = rcmClientPtr->GetReplicationSettings(settings);
            
            DebugPrintf(SV_LOG_INFO, "replication settings: %s\n", settings.c_str());

            try {
                std::string strrs;
                if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
                {
                    AzureToAzureSourceAgentReplicationSettings azureToAzureSourceAgentReplicationSettings = JSON::consumer<AzureToAzureSourceAgentReplicationSettings>::convert(settings, true);
                    AgentReplicationSettings rs;

                    do {
                        std::string hostId = RcmConfigurator::getInstance()->getHostId();

                        boost::to_lower(hostId);
                        if (azureToAzureSourceAgentReplicationSettings.ReplicationSettings.find(hostId) != azureToAzureSourceAgentReplicationSettings.ReplicationSettings.end())
                        {
                            rs = azureToAzureSourceAgentReplicationSettings.ReplicationSettings[hostId];
                            break;
                        }

                        boost::to_upper(hostId);
                        if (azureToAzureSourceAgentReplicationSettings.ReplicationSettings.find(hostId) != azureToAzureSourceAgentReplicationSettings.ReplicationSettings.end())
                        {
                            rs = azureToAzureSourceAgentReplicationSettings.ReplicationSettings[hostId];
                            break;
                        }
                        
                        std::string keysPresentInReplicationSettings;
                        std::map<std::string, AgentReplicationSettings>::iterator iter;

                        for (iter = azureToAzureSourceAgentReplicationSettings.ReplicationSettings.begin(); iter != azureToAzureSourceAgentReplicationSettings.ReplicationSettings.end(); ++iter)
                        {
                            keysPresentInReplicationSettings = keysPresentInReplicationSettings + iter->first + " ";
                        }

                        DebugPrintf(SV_LOG_ERROR, "%s: No entry present in the ReplicationSettings with hostId: %s. comparision is case insensitive. Keys present in settings recieved from RCM are: %s\n", FUNCTION_NAME, RcmConfigurator::getInstance()->getHostId().c_str(),
                            keysPresentInReplicationSettings.c_str());
                        errorCode = RCM_CLIENT_HOSTID_NOT_PRESENT_IN_AZURETOAZURE_REPLICATIONESETTINGS;
                        
                    } while (0);

                    strrs = JSON::producer<AgentReplicationSettings>::convert(rs);
                    
                }
                else if (RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
                {
                    AzureToOnPremSourceAgentSettings rs = JSON::consumer<AzureToOnPremSourceAgentSettings>::convert(settings, true);
                    strrs = JSON::producer<AzureToOnPremSourceAgentSettings>::convert(rs);
                }
                else
                {
                    OnPremToAzureSourceAgentSettings rs = JSON::consumer<OnPremToAzureSourceAgentSettings>::convert(settings, true);
                    strrs = JSON::producer<OnPremToAzureSourceAgentSettings>::convert(rs);
                }
                
                DebugPrintf(SV_LOG_INFO, "ptree replication settings %s\n", strrs.c_str());
            }
            catch (boost::property_tree::json_parser_error& pe)
            {
                DebugPrintf("error reading josn stream %s\n", pe.what());
            }
            catch (const std::exception& e)
            {
                DebugPrintf("error serialize %s\n", e.what());
            }
            //PrintSettings(ptSettings);
            //PrintActions(ptSettings);
        }
        else if (commandOptions.count(RCM_CLI_OPT_COMPLETE_AGENT_UPGRADE))
        {
            if (!commandOptions.count(RCM_CLI_OPT_AGENT_UPGRADE_EXIT_CODE) ||
                !commandOptions.count(RCM_CLI_OPT_AGENT_UPGRADE_JOB_DETAIL) ||
                !commandOptions.count(RCM_CLI_OPT_AGENT_UPGRADE_ERROR_FILE_PATH))
            {
                std::cerr << "Error: agent exitcode, upgrade job details and error json file path options required for "
                    << RCM_CLI_OPT_COMPLETE_AGENT_UPGRADE << std::endl;
                DebugPrintf(SV_LOG_ERROR, "Error: agent exitcode, upgrade job details and error json file path options required for %s\n",
                    RCM_CLI_OPT_COMPLETE_AGENT_UPGRADE);
                status = RCM_CLI_INVALID_ARGUMENT;
            }

            ret = rcmClientPtr->CompleteAgentUpgradeJob(commandOptions[RCM_CLI_OPT_AGENT_UPGRADE_EXIT_CODE].as<int>(),
                commandOptions[RCM_CLI_OPT_AGENT_UPGRADE_JOB_DETAIL].as<std::string>(),
                commandOptions[RCM_CLI_OPT_AGENT_UPGRADE_ERROR_FILE_PATH].as<std::string>());
        }
        else if (commandOptions.count(RCM_CLI_OPT_AGENT_UPGRADE_UPLOAD_LOGS))
        {
            if (!commandOptions.count(RCM_CLI_OPT_AGENT_UPGRADE_LOGS_PATH) ||
                !commandOptions.count(RCM_CLI_OPT_AGENT_UPGRADE_JOB_DETAIL))
            {
                std::cerr << "Error: agent upgrade logs path and job details are required for "
                    << RCM_CLI_OPT_AGENT_UPGRADE_UPLOAD_LOGS << std::endl;
                DebugPrintf(SV_LOG_ERROR, "Error: agent upgrade logs path and job details are required for %s\n",
                    RCM_CLI_OPT_AGENT_UPGRADE_UPLOAD_LOGS);
                status = RCM_CLI_INVALID_ARGUMENT;
            }

            ret = UploadAgentUpgradeLogs(
                    commandOptions[RCM_CLI_OPT_AGENT_UPGRADE_LOGS_PATH].as<std::string>(),
                    commandOptions[RCM_CLI_OPT_AGENT_UPGRADE_JOB_DETAIL].as<std::string>());
        }
        else if (commandOptions.count(RCM_CLI_OPT_VERIFY_CLIENT_AUTH))
        {
            if (!commandOptions.count(RCM_CLI_OPT_CONFIG_FILE))
            {
                std::cerr << "Error: config file is required for "
                    << RCM_CLI_OPT_VERIFY_CLIENT_AUTH << std::endl;
                DebugPrintf(SV_LOG_ERROR, "Error: config file is required for %s\n",
                    RCM_CLI_OPT_VERIFY_CLIENT_AUTH);
                status = RCM_CLI_INVALID_ARGUMENT;
            }
            else
            {
                ret = RcmConfigurator::getInstance()->VerifyClientAuth(
                    commandOptions[RCM_CLI_OPT_CONFIG_FILE].as<std::string>());
                if (ret == SVS_OK)
                {
                    std::cout << "Verify client authentication succeeded." << std::endl;
                    DebugPrintf(SV_LOG_ALWAYS, "Verify client authentication succeeded.\n");
                }
            }
        }
        else if (commandOptions.count(RCM_CLI_OPT_DIAGNOSE_AND_FIX))
        {
            ret = RcmConfigurator::getInstance()->DiagnoseAndFix();
            if (ret == SVS_OK)
            {
                std::cout << "Diagnose and Fix succeeded." << std::endl;
                DebugPrintf(SV_LOG_ALWAYS, "Diagnose and Fix succeeded.\n");
            }
        }
        else if (commandOptions.count(RCM_CLI_OPT_COMPLETE_AGENT_REGISTRATION))
        {
            bool bDoNotUseProxy = commandOptions.count(RCM_CLI_OPT_NOPROXY) > 0;
            QuitFunction_t qf = QuitRequested;
            ret = RcmConfigurator::getInstance()->CompleteAgentRegistration(qf, bDoNotUseProxy);
            if (ret == SVS_OK)
            {
                std::cout << "Complete agent registration succeeded." << std::endl;
                DebugPrintf(SV_LOG_ALWAYS, "Complete agent registration succeeded.\n");
            }
            else if (RCM_CLIENT_SERVER_CERT_VALIDATION_ERROR == rcmClientPtr->GetErrorCode())
            {
                ValidateRootCert();
            }
        }
        else if (commandOptions.count(RCM_CLI_OPT_RENEW_LOG_CONTAINER_SAS_URI))
        {
            std::string diskId, containerType, response, clientRequestId;

            if (!commandOptions.count(RCM_CLI_OPT_DISKID))
            {
                diskId = "RootDisk";
            }
            else
            {
                diskId = commandOptions[RCM_CLI_OPT_DISKID].as<std::string>();
            }

            if (commandOptions.count(RCM_CLI_OPT_CONTAINER_TYPE))
            {
                containerType = commandOptions[RCM_CLI_OPT_CONTAINER_TYPE].as<std::string>();
            }
            else
            {
                RcmConfigurator::getInstance()->GetContainerType(diskId, containerType);
            }

            clientRequestId = GenerateUuid();
            std::cout << "Invoking RenewLogContainerSasUri API. ClientRequestId: " << clientRequestId << std::endl;

            bool isClusterVirtualNodeContext = RcmConfigurator::getInstance()->IsClusterSharedDiskProtected(diskId);
            ret = rcmClientPtr->RenewBlobContainerSasUri(diskId, containerType, response, isClusterVirtualNodeContext, clientRequestId);

            if (ret == SVS_OK)
            {
                DebugPrintf(SV_LOG_ALWAYS, "RenewLogContainerSasUri api invocation succeeded. Disk: %s\n", diskId.c_str());
            }
        }
        else if (commandOptions.count(RCM_CLI_OPT_GET_NON_CACHED_SETTINGS))
        {
            ret = RcmConfigurator::getInstance()->GetNonCachedSettings();
            if (ret == SVS_OK)
            {
                std::cout << "Get Non-cached settings succeeded." << std::endl;
                DebugPrintf(SV_LOG_ALWAYS, "Get Non-cached settings succeeded.\n");
            }
        }
        else
        {
            std::cerr << "Error: unknown option: " << argv[1]  << std::endl;
            DebugPrintf(SV_LOG_ERROR, "Error: unknown option: %s\n", argv[1]);
            status = RCM_CLI_INVALID_ARGUMENT;
        }

        if (ret != SVS_OK)
        {
            errorCode = rcmClientPtr->GetErrorCode();
            std::cerr << "Error: request failed with return value "
                << ret << ", error code " << errorCode << "." << std::endl;

            // handle when error code is not set and handle empty http response
            status = errorCode ? GetReturnCode(errorCode) :
                    ((ret == SVE_HTTP_RESPONSE_FAILED) ? RCM_CLIENT_ERROR_HTTP_RESPONSE_UNKNOWN : RCM_CLI_INVALID_ARGUMENT);

            if (status == RCM_CLIENT_DIAGNOSE_AND_FIX_FAILED)
            {
                std::cerr << "Failed to fix the issue. Please check log file : " <<
                    logPath << " for more details" << std::endl;
            }
        }

        if ((ret == SVS_OK) &&
            (commandOptions.count(RCM_CLI_OPT_REGISTER_MACHINE) ||
             commandOptions.count(RCM_CLI_OPT_COMPLETE_AGENT_REGISTRATION)))
        {
            // RegisterMachine is successful, clean up any disable protection state
            RcmConfigurator::getInstance()->ClearDisableProtectionState();
        }

    }
    catch (const AzureRcmCliException& cliex)
    {
        status = cliex.status();
        if (status) {
            DebugPrintf(SV_LOG_ERROR, "Error: command failed with exception %s\n", cliex.what());
            std::cerr << "Error: command failed with exception : " << cliex.what() << endl;
        }
    }
    catch (ContextualException & ce)
    {
        std::string errMsg = ce.what();
        DebugPrintf(SV_LOG_ERROR, "Error: command failed with exception %s\n", errMsg.c_str());
        std::cerr << "Error: command failed with exception : " << errMsg << endl;

        if (errMsg.find(confFileNotFoundError) != std::string::npos)
            status = RCM_CLI_INVALID_RCM_CONF_ENTRY;
        else
            status = RCM_CLI_CAUGHT_EXCEPTION;
    }
    catch (const char *msg)
    {
        DebugPrintf(SV_LOG_ERROR, "Error: command failed with exception %s\n", msg);
        std::cerr << "Error: command failed with exception : " << msg << endl;
        status = RCM_CLI_CAUGHT_EXCEPTION;
    }
    catch (std::string &msg)
    {
        DebugPrintf(SV_LOG_ERROR, "Error: command failed with exception %s\n", msg.c_str());
        std::cerr << "Error: command failed with exception : " << msg << endl;
        status = RCM_CLI_CAUGHT_EXCEPTION;
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "Error: command failed with exception %s\n", e.what());
        std::cerr << "Error: command failed with exception : " << e.what() << endl;
        status = RCM_CLI_CAUGHT_EXCEPTION;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "Error: command failed with unknown exception\n");
        std::cerr << "Error: command failed with unknown exception." << endl;
        status = RCM_CLI_CAUGHT_EXCEPTION;
    }

    DebugPrintf(SV_LOG_INFO, "%s: Exiting with status %d.\n", FUNCTION_NAME, status);
    return WriteErrorsToJsonFile(status, errorCode);
}
