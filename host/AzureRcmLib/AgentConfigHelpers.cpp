#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/regex.hpp>

#include "AgentConfig.h"
#include "biosidoperations.h"
#include "portablehelperscommonheaders.h"
#include "securityutils.h"
#include "setpermissions.h"

namespace RcmClientLib {

AgentConfiguration AgentConfigHelpers::m_cachedPersistedAgentConfig;
boost::shared_mutex AgentConfigHelpers::m_agentSettingsMutex;

std::string AgentConfigHelpers::GetAgentConfigInputDecoded()
{
    AgentConfigInput configInput;
#ifdef SV_WINDOWS
    configInput.Fqdn = Host::GetInstance().GetFQDN();
#else
    configInput.Fqdn = Host::GetInstance().GetHostName();
#endif

    configInput.BiosId = GetSystemUUID();

    std::string serInput = JSON::producer<AgentConfigInput>::convert(configInput);

    return serInput;
}

std::string AgentConfigHelpers::GetAgentConfigInput()
{
    std::string serInput = GetAgentConfigInputDecoded();
    return securitylib::base64Encode(serInput.c_str(), serInput.length());
}

bool AgentConfigHelpers::IsConfigValid(const AgentConfiguration& agentConfig)
{
    std::string biosId;
    biosId = GetSystemUUID();

    return (boost::iequals(biosId, agentConfig.BiosId) ||
            boost::iequals(BiosID::GetByteswappedBiosID(biosId), agentConfig.BiosId));
}

bool AgentConfigHelpers::IsConfigValid(const ALRMachineConfig& alrMachineConfig,
    std::string& errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool isValid = true;

    if (!IsAzureStackVirtualMachine())
    {
        boost::regex guidRegex(GUID_REGEX);
        if (!boost::regex_search(alrMachineConfig.OnPremMachineBiosId, guidRegex))
        {
            isValid = false;
            errMsg += "Invalid OnPremMachineBiosId: " + alrMachineConfig.OnPremMachineBiosId + "\n";
        }
    }
    
    if (alrMachineConfig.Passphrase.empty())
    {
        isValid = false;
        errMsg += "Passphrase is empty";
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return isValid;
}

template <typename ConfigTemplate>
SVSTATUS AgentConfigHelpers::GetConfig(const std::string& configFilePath,
    ConfigTemplate& config, std::stringstream& strErrMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVE_FAIL;

    do {
        boost::system::error_code ec;
        if (!boost::filesystem::exists(configFilePath, ec))
        {
            strErrMsg << "Config file " << configFilePath << " doesn't exist. error "
                << ec.message();
            status = SVE_FILE_NOT_FOUND;
            break;
        }

        std::string lockFile(configFilePath);
        lockFile += ".lck";
        if (!boost::filesystem::exists(lockFile)) {
            std::ofstream tmpLockFile(lockFile.c_str());
        }
        boost::interprocess::file_lock fileLock(lockFile.c_str());
        boost::interprocess::sharable_lock<boost::interprocess::file_lock> sharableFileLock(
            fileLock);

        std::ifstream confFile(configFilePath.c_str());
        if (!confFile.good()) {
            strErrMsg << "Unable to open config file " << configFilePath << ". error : "
                << boost::lexical_cast<std::string>(errno);
            break;
        }

        try {
            std::string json((std::istreambuf_iterator<char>(confFile)),
                std::istreambuf_iterator<char>());
            config = JSON::consumer<ConfigTemplate>::convert(json, true);
            status = SVS_OK;
        }
        catch (std::exception &e)
        {
            strErrMsg << "Unable to read config file " << configFilePath << ". error : "
                << e.what();
        }
        catch (...)
        {
            strErrMsg << "Unable to read config file " << configFilePath
                << " with unknown exception.";
        }
    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

template <typename ConfigTemplate>
SVSTATUS AgentConfigHelpers::PersistConfig(const std::string& configFilePath,
    ConfigTemplate& config, std::stringstream& strErrMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVE_FAIL;

    do {
        std::string lockFile(configFilePath);
        lockFile += ".lck";
        if (!boost::filesystem::exists(lockFile)) {
            std::ofstream tmpLockFile(lockFile.c_str());
        }
        boost::interprocess::file_lock fileLock(lockFile.c_str());
        boost::interprocess::scoped_lock<boost::interprocess::file_lock> sharableFileLock(fileLock);

        std::ofstream confFile(configFilePath.c_str(), std::ios::trunc);
        if (!confFile.good()) {
            strErrMsg << "unable to open config file " << configFilePath
                << "errno: " << boost::lexical_cast<std::string>(errno);
            break;
        }

        const std::string serConfig = JSON::producer<ConfigTemplate>::convert(config);

        confFile << serConfig;
        confFile.close();

        securitylib::setPermissions(configFilePath);
        status = SVS_OK;

    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

void AgentConfigHelpers::GetSourceAgentConfig(const std::string& sourceConfigFilePath,
    AgentConfiguration& agentConfig)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::stringstream strErrMsg;
    SVSTATUS status = SVS_OK;

    status = GetConfig<AgentConfiguration>(sourceConfigFilePath, agentConfig, strErrMsg);
    if (status != SVS_OK)
    {
        if (strErrMsg.str().empty())
        {
            strErrMsg << "Failed to get source agent config from file " << sourceConfigFilePath
                << "with status : " << status;
        }

        DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, status,
            strErrMsg.str().c_str());
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

        throw INMAGE_EX(strErrMsg.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void AgentConfigHelpers::UpdateRcmProxyTransportSettings(const std::string& sourceConfigFilePath,
    const RcmProxyTransportSetting& rcmProxySettings)
{
    boost::unique_lock<boost::shared_mutex> guard(m_agentSettingsMutex);

    if (rcmProxySettings == m_cachedPersistedAgentConfig.RcmProxyTransportSettings)
        return;

    AgentConfiguration persistedAgentConfig;

    GetSourceAgentConfig(sourceConfigFilePath, persistedAgentConfig);

    if (rcmProxySettings == persistedAgentConfig.RcmProxyTransportSettings)
    {
        m_cachedPersistedAgentConfig = persistedAgentConfig;
        return;
    }

    persistedAgentConfig.RcmProxyTransportSettings = rcmProxySettings;

    PersistSourceAgentConfig(sourceConfigFilePath, persistedAgentConfig);

    m_cachedPersistedAgentConfig = persistedAgentConfig;

    return;
}

void AgentConfigHelpers::PersistSourceAgentConfig(const std::string& sourceConfigFilePath,
    AgentConfiguration& agentConfig)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    std::stringstream strErrMsg;

    status = PersistConfig<AgentConfiguration>(sourceConfigFilePath, agentConfig, strErrMsg);
    if (status != SVS_OK)
    {
        if (strErrMsg.str().empty())
        {
            strErrMsg << "Failed to persist source agent config to file " << sourceConfigFilePath
                << "with status : " << status;
        }
        DebugPrintf(SV_LOG_ERROR, "%s: status = %d, %s\n", FUNCTION_NAME, status,
            strErrMsg.str().c_str());
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

        throw INMAGE_EX(strErrMsg.str().c_str());
    }
}

SVSTATUS AgentConfigHelpers::GetALRMachineConfig(const std::string& alrConfigFilePath,
    ALRMachineConfig& alrMachineConfig, std::stringstream& strErrMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    status = GetConfig<ALRMachineConfig>(alrConfigFilePath, alrMachineConfig, strErrMsg);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS AgentConfigHelpers::PersistALRMachineConfig(const std::string& alrConfigFilePath,
    ALRMachineConfig& alrMachineConfig, std::string& errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    do
    {
        if(!IsConfigValid(alrMachineConfig, errMsg))
        {
            status = SVE_FAIL;
            DebugPrintf(SV_LOG_ERROR, "%s: ALR config validation failed with error message : %s\n",
                FUNCTION_NAME, errMsg.c_str());
            break;
        }

        std::stringstream strErrMsg;
        status = PersistConfig<ALRMachineConfig>(alrConfigFilePath, alrMachineConfig, strErrMsg);
        if (status != SVS_OK)
        {
            errMsg = strErrMsg.str();
        }
    } while(false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

}
