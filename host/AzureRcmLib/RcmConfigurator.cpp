/*
+------------------------------------------------------------------------------------------------ - +
Copyright(c) Microsoft Corp. 2019
+ ------------------------------------------------------------------------------------------------ - +
File        : RcmConfigurator.cpp

Description : RcmConfigurator class implementation

+ ------------------------------------------------------------------------------------------------ - +
*/

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/pointer_cast.hpp>

#include "RcmLibConstants.h"
#include "csgetfingerprint.h"
#include "RcmConfigurator.h"
#include "HttpClient.h"
#include "proxysettings.h"
#include "gencert.h"
#include "appcommand.h"
#include "inmcertutil.h"
#include "SyncSettings.h"
#include "ProtectionState.h"
#include "RcmDataProtectionSyncTargetSettings.h"
#include "RcmProxyDetails.h"
#include "CertHelpers.h"
#include "portablehelperscommonheaders.h"
#include "ServiceHelper.h"
#include "biosidoperations.h"

using namespace AzureStorageRest;
using namespace RcmClientLib;
using namespace securitylib;
using boost::property_tree::ptree;

#define RCM_PROTECTION_STATE_FILE_NAME "RcmProtectionState.json"

extern int nLogLevel;

RcmConfiguratorPtr RcmConfigurator::s_instancePtr;
boost::mutex RcmConfigurator::s_instanceMutex;
std::string RcmConfigurator::s_replicationSettingsPath;
static int s_logLevel = nLogLevel;

void RcmConfigurator::Initialize(FileConfiguratorMode mode, const std::string& confFilePath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    if (FILE_CONFIGURATOR_MODE_VX_AGENT != mode)
    {
        FileConfigurator::setInitMode(mode);
    }
    if (!confFilePath.empty())
    {
        s_replicationSettingsPath = confFilePath;
    }
    HttpClient::GlobalInitialize();
    HttpClient::SetRetryPolicy(0);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void RcmConfigurator::Uninitialize()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    HttpClient::GlobalUnInitialize();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void RcmConfigurator::EnableVerboseLogging()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    s_logLevel = nLogLevel;
    nLogLevel = SV_LOG_ALWAYS;
    HttpClient::SetVerbose(true, CURLINFO_TEXT);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void RcmConfigurator::DisableVerboseLogging()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    nLogLevel = s_logLevel;
    HttpClient::SetVerbose(false, CURLINFO_TEXT);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

RcmConfigurator::RcmConfigurator()
    :m_QuitEvent(new ACE_Event(1, 0)),
    m_started(false),
    m_settingsSource(SETTINGS_SOURCE_LOCAL_CACHE)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    Initialize();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

RcmConfigurator::~RcmConfigurator()
{
}

void RcmConfigurator::Init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::stringstream errMsg;

    if (getInitMode() == FILE_CONFIGURATOR_MODE_VX_AGENT)
    {
        std::string hostId = getHostId();
        std::string installPath = getInstallPath();
        std::string rcmSettingsPath = getRcmSettingsPath();

        if (hostId.empty() ||
            installPath.empty() ||
            rcmSettingsPath.empty())
        {
            errMsg << "Insufficient information to initialize RCM client "
                << " hostid " << hostId
                << " installpath " << installPath
                << "rcmSettingsPath " << rcmSettingsPath;
            throw INMAGE_EX(errMsg.str().c_str());
        }

        boost::system::error_code ec;
        if (!boost::filesystem::exists(rcmSettingsPath, ec))
        {
            errMsg << "RCM client settings file "
                << rcmSettingsPath
                << " does not exist. Error=" << ec.message().c_str() << '.';
            throw INMAGE_EX(errMsg.str().c_str());
        }

        m_pRcmClientSettings.reset(new RcmClientSettings(rcmSettingsPath));

        m_pAgentSettingsCacher.reset(new AgentSettingsCacher());

        m_pRcmProxySettingsCacherPtr.reset(new RcmProxySettingsCacher());

        RcmClientLib::RcmClientImplTypes implType =
            IsAzureToAzureReplication() || IsAzureToOnPremReplication() ?
            RcmClientLib::RCM_AZURE_IMPL :
            RcmClientLib::RCM_PROXY_IMPL;

        m_pRcmClient.reset(new RcmClient(implType));

        if (!m_devDetailsPersistentStore.Init())
        {
            errMsg << "Device details persistent store init failed.";
            throw INMAGE_EX(errMsg.str().c_str());
        }
    }
    else if (getInitMode() == FILE_CONFIGURATOR_MODE_CS_PRIME_APPLAINCE_TO_AZURE)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Initializing RcmConfigurator with init mode FILE_CONFIGURATOR_MODE_CS_PRIME_APPLAINCE_TO_AZURE", FUNCTION_NAME);
    }
    else if (getInitMode() == FILE_CONFIGURATOR_MODE_CS_PRIME_APPLIANCE_EVTCOLLFORW)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Initializing RcmConfigurator with init mode FILE_CONFIGURATOR_MODE_CS_PRIME_APPLIANCE_EVTCOLLFORW", FUNCTION_NAME);
    }
    else
    {
        errMsg << "Unknown init mode " << getInitMode() ;
        throw INMAGE_EX(errMsg.str().c_str());
    }

    try {
        // Initialize with cached setting as default settings source is local cache
        PollReplicationSettings();
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught exception %s", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught unknown exception", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return;
}

RcmConfiguratorPtr RcmConfigurator::getInstance()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (s_instancePtr == NULL)
    {
        boost::unique_lock<boost::mutex> lock(s_instanceMutex);
        if (s_instancePtr == NULL)
        {
            RcmConfiguratorPtr rcmCfgPtr(new RcmConfigurator);
            rcmCfgPtr->Init();
            s_instancePtr = rcmCfgPtr;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return s_instancePtr;
}

void RcmConfigurator::Start(const RcmSettingsSource source)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (m_started)
    {
        if (source != m_settingsSource)
        {
            std::stringstream errMsg;
            errMsg << "RcmConfigurator is already initialized in mode "
                << source;
            throw INMAGE_EX(errMsg.str().c_str());
        }

        return;
    }
    m_settingsSource = source;

    m_QuitEvent->reset();
    m_threadManager.spawn(ThreadFunc, this);
    m_started = true;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return;
}

void RcmConfigurator::Stop()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!m_started)
        return;

    m_threadManager.cancel_all();
    m_QuitEvent->signal();
    m_threadManager.wait();
    m_started = false;
    m_settingsSource = SETTINGS_SOURCE_UNINITIALIZED;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}


ACE_THR_FUNC_RETURN RcmConfigurator::ThreadFunc(void* arg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    RcmConfigurator* p = static_cast<RcmConfigurator*>(arg);
    return p->run();
}


ACE_THR_FUNC_RETURN RcmConfigurator::run()
{
    DebugPrintf(SV_LOG_ALWAYS, "Starting RcmConfigurator to periodically poll setting\n");

    ACE_Time_Value waitTime(getReplicationSettingsFetchInterval(), 0);
    do
    {
        try {
            PollReplicationSettings();
            m_QuitEvent->wait(&waitTime, 0);
        }
        catch (const std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: caught exception %s", FUNCTION_NAME, e.what());
            m_QuitEvent->wait(&waitTime, 0);
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: caught unknown exception", FUNCTION_NAME);
            m_QuitEvent->wait(&waitTime, 0);
        }
    } while (!m_threadManager.testcancel(ACE_Thread::self()));

    DebugPrintf(SV_LOG_ALWAYS, "%s: Quitting RcmConfigurator\n", FUNCTION_NAME);

    return 0;
}

bool RcmConfigurator::VerifyRcmDetails(const RcmDetails& rcmDetails)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool areDetailsValid = true;

    if (rcmDetails.ServiceConnectionMode.empty() ||
        rcmDetails.Certificate.empty() ||
        rcmDetails.EndPointType.empty() ||
        rcmDetails.SpnIdentity.empty())
    {
        areDetailsValid = false;
    }
    else if ((rcmDetails.ServiceConnectionMode == ServiceConnectionModes::DIRECT) &&
        rcmDetails.RcmServiceUrl.empty())
    {
        areDetailsValid = false;
    }
    else if ((rcmDetails.ServiceConnectionMode == ServiceConnectionModes::RELAY) &&
        rcmDetails.RelaySettings.empty())
    {
        areDetailsValid = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return areDetailsValid;
}

RcmDetails RcmConfigurator::GetRcmDetailsFromRcmInfo()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    RcmDetails rcmDetails;

    rcmDetails.ServiceConnectionMode = m_pRcmClientSettings->m_ServiceConnectionMode;
    rcmDetails.EndPointType = m_pRcmClientSettings->m_EndPointType;

    rcmDetails.SpnIdentity.TenantId = m_pRcmClientSettings->m_AADTenantId;
    rcmDetails.SpnIdentity.ApplicationId = m_pRcmClientSettings->m_AADClientId;
    rcmDetails.SpnIdentity.ObjectId = m_pRcmClientSettings->m_ManagementId;
    rcmDetails.SpnIdentity.Audience = m_pRcmClientSettings->m_AADAudienceUri;
    rcmDetails.SpnIdentity.AadAuthority = m_pRcmClientSettings->m_AADUri;
    rcmDetails.SpnIdentity.CertificateThumbprint = m_pRcmClientSettings->m_AADAuthCert;

    if (rcmDetails.ServiceConnectionMode == ServiceConnectionModes::DIRECT)
    {
        rcmDetails.RcmServiceUrl = m_pRcmClientSettings->m_ServiceUri;
    }
    else if (rcmDetails.ServiceConnectionMode == ServiceConnectionModes::RELAY)
    {
        rcmDetails.RelaySettings.RelayServiceUri = m_pRcmClientSettings->m_ServiceUri;
        rcmDetails.RelaySettings.RelayKeyPolicyName = m_pRcmClientSettings->m_RelayKeyPolicyName;
        rcmDetails.RelaySettings.RelayServicePathSuffix = m_pRcmClientSettings->m_RelayServicePathSuffix;
        rcmDetails.RelaySettings.RelaySharedKey = m_pRcmClientSettings->m_RelaySharedKey;
        rcmDetails.RelaySettings.ExpiryTimeoutInSeconds = m_pRcmClientSettings->m_ExpiryTimeoutInSeconds;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rcmDetails;
}

SVSTATUS RcmConfigurator::UpdateRcmSettingsFromRcmDetails(const RcmDetails& rcmDetails)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    m_pRcmClientSettings->m_AADTenantId = rcmDetails.SpnIdentity.TenantId;
    m_pRcmClientSettings->m_AADClientId = rcmDetails.SpnIdentity.ApplicationId;
    m_pRcmClientSettings->m_ManagementId = rcmDetails.SpnIdentity.ObjectId;
    m_pRcmClientSettings->m_AADAudienceUri = rcmDetails.SpnIdentity.Audience;

    m_pRcmClientSettings->m_AADUri = rcmDetails.SpnIdentity.AadAuthority;
    m_pRcmClientSettings->m_AADAuthCert = rcmDetails.SpnIdentity.CertificateThumbprint;
    m_pRcmClientSettings->m_EndPointType = rcmDetails.EndPointType;

    if (rcmDetails.ServiceConnectionMode == ServiceConnectionModes::DIRECT)
    {
        m_pRcmClientSettings->m_ServiceUri = rcmDetails.RcmServiceUrl;

    }
    else if (rcmDetails.ServiceConnectionMode == ServiceConnectionModes::RELAY)
    {
        m_pRcmClientSettings->m_ServiceUri = rcmDetails.RelaySettings.RelayServiceUri;
        m_pRcmClientSettings->m_RelayKeyPolicyName = rcmDetails.RelaySettings.RelayKeyPolicyName;
        m_pRcmClientSettings->m_RelayServicePathSuffix= rcmDetails.RelaySettings.RelayServicePathSuffix;
        m_pRcmClientSettings->m_RelaySharedKey = rcmDetails.RelaySettings.RelaySharedKey;
        m_pRcmClientSettings->m_ExpiryTimeoutInSeconds = rcmDetails.RelaySettings.ExpiryTimeoutInSeconds;
        m_pRcmClientSettings->m_ServiceConnectionMode = ServiceConnectionModes::RELAY;
    }

    status = m_pRcmClientSettings->PersistRcmClientSettings();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

#ifdef SV_WINDOWS
SVSTATUS RcmConfigurator::InstallCertificateWindows(const std::string& certificate)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    SV_ULONG lexit;
    std::string output, cmd;
    bool processActive = true;

    boost::filesystem::path pfxCertDir(securitylib::getPrivateDir());
    boost::filesystem::path pfxCertPath = pfxCertDir / "rcm.pfx";
    boost::filesystem::save_string_file(pfxCertPath, certificate);
    boost::filesystem::path tempfile(pfxCertDir / "temp.txt");

    cmd = "certutil -f -p \"\" -importpfx \"" + pfxCertPath.string() + "\" AT_SIGNATURE";
    DebugPrintf(SV_LOG_DEBUG, "Running cmd \"%s\"\n", cmd.c_str());
    AppCommand app(cmd, 30, tempfile.string());
    app.Run(lexit, output, processActive);
    boost::system::error_code ec;

    DebugPrintf(SV_LOG_INFO, "App Command output %s.\n", output.c_str());

    if (lexit == 0)
    {
        DebugPrintf(SV_LOG_DEBUG, "Succesfully imported certificate.\n");
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to import certificate with exitcode %lu.\n", lexit);
        status = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

void RcmConfigurator::LogErrorMessage(DWORD& errorMessage)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    LPSTR messageBuffer = nullptr;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessage, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0,
        NULL);
    DebugPrintf(SV_LOG_ERROR, "Error Message : %s\n", messageBuffer);
    LocalFree(messageBuffer);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

#else
SVSTATUS RcmConfigurator::InstallCertificateLinux(const std::string& certificate)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string pfxCert, passphrase, crtCert, certStart, certEnd;
    std::stringstream buffer;

    boost::filesystem::path pfxCertPath(securitylib::getPrivateDir());
    pfxCertPath /= "rcm.pfx";
    boost::filesystem::path crtCertPath(securitylib::getCertDir());
    crtCertPath /= "rcm.crt";

    pfxCert = securitylib::base64Decode(certificate.c_str(),certificate.length());
    boost::filesystem::save_string_file(pfxCertPath, pfxCert);
    securitylib::CertData certData;
    certData.updateCertData();
    securitylib::GenCert genCert(std::string(), certData, true, false, false,
        pfxCertPath.c_str(), std::string(), false, std::string());

    //Redirect stdout to a stringstream
    std::streambuf *coutbuf = std::cout.rdbuf();
    std::cout.rdbuf(buffer.rdbuf());
    passphrase = securitylib::PFX_PASSPHRASE;
    genCert.extractPfxPrivateKey(pfxCertPath.string(), passphrase);
    std::cout.rdbuf(coutbuf);
    crtCert = buffer.str();

    certStart = "-----BEGIN CERTIFICATE-----";
    certEnd = "-----END CERTIFICATE-----\n";

    if (crtCert.find(certStart) != std::string::npos)
    {
        crtCert = crtCert.substr(crtCert.find(certStart));
        crtCert = crtCert.substr(0, crtCert.rfind(certEnd) + certEnd.size());
        boost::filesystem::save_string_file(crtCertPath, crtCert);
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to extract certificate from : %s\n", crtCert.c_str());
        status = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}
#endif

SVSTATUS RcmConfigurator::InstallCertifcate(const std::string& certificate)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

#ifdef SV_WINDOWS
    status = InstallCertificateWindows(certificate);
#else
    status = InstallCertificateLinux(certificate);
#endif

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::UpdateRcmInfoFromRcmDetails(const RcmDetails& rcmDetails)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    if (GetRcmDetailsFromRcmInfo() != rcmDetails)
    {
        DebugPrintf(SV_LOG_ALWAYS, "Rcm Details changed from %s to %s\n", GetRcmDetailsFromRcmInfo().c_str(), rcmDetails.c_str());

        status = UpdateRcmSettingsFromRcmDetails(rcmDetails);
        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_DEBUG, "Failed to update RcmInfo.conf from Rcm Details\n");
            return status;
        }
        status = InstallCertifcate(rcmDetails.Certificate);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

void RcmConfigurator::UpdateRcmDetails()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    // GetRcmDetails for Reprotect
    LocalConfigurator lc;
    static boost::chrono::steady_clock::time_point lastUpdateRcmDetailsTime =
        boost::chrono::steady_clock::time_point::min();
    boost::chrono::steady_clock::time_point curTime = boost::chrono::steady_clock::now();
    if (curTime < lastUpdateRcmDetailsTime + boost::chrono::seconds(lc.getRcmDetailsPollInterval()))
        return;

    std::string serRcmDetails;
    status = m_pRcmClient->GetRcmDetails(serRcmDetails);
    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "Get Rcm Details RCM API call failed.\n");
        return;
    }
    lastUpdateRcmDetailsTime = curTime;
    const RcmDetails rcmDetails = JSON::consumer<RcmDetails>::convert(serRcmDetails, true);
    if (VerifyRcmDetails(rcmDetails))
        status = UpdateRcmInfoFromRcmDetails(rcmDetails);
    else
        DebugPrintf(SV_LOG_ERROR, "%s: Rcm details verification failed. %s\n", FUNCTION_NAME, rcmDetails.c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

SVSTATUS RcmConfigurator::PollReplicationSettings()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    RcmSettingsSource settingsSource = m_settingsSource;
    SVSTATUS status = SVS_OK;

    if ((settingsSource == SETTINGS_SOURCE_RCM) ||
        (settingsSource == SETTINGS_SOURCE_LOCAL_CACHE_IF_RCM_NOT_AVAILABLE))
    {
        DebugPrintf(SV_LOG_DEBUG, "Fetching settings from RCM.\n");

        std::string serSettings;
        status = m_pRcmClient->GetReplicationSettings(serSettings);
        if (status == SVS_OK)
        {
            status = IsAzureToAzureReplication() ?
                UpdateAgentSettingsCache(serSettings) : (IsOnPremToAzureReplication() ?
                    UpdateOnPremToAzureAgentSettingsCache(serSettings) : UpdateAzureToOnPremAgentSettingsCache(serSettings));
        }

        if (status != SVS_OK)
        {
            int32_t errorCode = m_pRcmClient->GetErrorCode();
            DebugPrintf(SV_LOG_ERROR, "%s: failed to get settings from RCM with error %d\n", FUNCTION_NAME, errorCode);

            if (errorCode == RCM_CLIENT_PROTECTION_DISABLED)
            {
                m_protectionDisabledSignal();
            }

            if (settingsSource == SETTINGS_SOURCE_LOCAL_CACHE_IF_RCM_NOT_AVAILABLE)
            {
                settingsSource = SETTINGS_SOURCE_LOCAL_CACHE;
            }
        }
    }

    if (settingsSource == SETTINGS_SOURCE_LOCAL_CACHE)
    {
        if (getInitMode() == FILE_CONFIGURATOR_MODE_VX_AGENT)
        {
            DebugPrintf(SV_LOG_DEBUG, "Reading cached settings.\n");
            AgentSettings agentSettings;
            status = m_pAgentSettingsCacher->Read(agentSettings);
            if (status != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: failed to read cached settings with error %s\n", FUNCTION_NAME, m_pAgentSettingsCacher->Error().c_str());
            }
            else
            {
                AgentSettings prevSettings;
                status = GetAgentSettings(prevSettings);
                if (status != SVS_OK)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: failed to get current agent settings with error %d\n", FUNCTION_NAME, status);
                    return status;
                }
                {
                    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
                    m_agentSettings = agentSettings;
                }

                HandleChangeInAgentSettings(prevSettings);
            }
        }
        else if (getInitMode() == FILE_CONFIGURATOR_MODE_CS_PRIME_APPLAINCE_TO_AZURE)
        {
            if (s_replicationSettingsPath.empty())
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Replication settings file path supplied is empty on appliance.\n", FUNCTION_NAME);
                return SVE_FAIL;
            }
            boost::system::error_code ec;
            if (!boost::filesystem::exists(s_replicationSettingsPath, ec))
            {
                std::stringstream errMsg;
                errMsg << "Replication settings file " << s_replicationSettingsPath
                    << " does not exist. Error: " << ec.message().c_str() << '.';
                DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, errMsg.str().c_str());
                return SVE_FAIL;
            }
            std::string lockFile(s_replicationSettingsPath);
            lockFile += ".lck";
            if (!boost::filesystem::exists(lockFile))
            {
                std::ofstream tmpLockFile(lockFile.c_str());
            }
            boost::interprocess::file_lock fileLock(lockFile.c_str());
            boost::interprocess::scoped_lock<boost::interprocess::file_lock> fileLockGuard(fileLock);

            std::ifstream confFile(s_replicationSettingsPath.c_str());
            if (!confFile.good()) {
                int errorCode = errno;
                DebugPrintf(SV_LOG_ERROR, "%s: unable to open settings file %s with error %x\n",
                    FUNCTION_NAME,
                    s_replicationSettingsPath.c_str(),
                    errorCode);
                return SVE_FAIL;
            }

            std::string serializedSettings((std::istreambuf_iterator<char>(confFile)),
                std::istreambuf_iterator<char>());

            UpdateDataProtectionSyncRcmTargetSettings(serializedSettings);
        }
        else if (getInitMode() == FILE_CONFIGURATOR_MODE_CS_PRIME_APPLIANCE_EVTCOLLFORW)
        {
            // No-op
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::UpdateAgentSettingsCache(const std::string& serializedSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    try {

        if (getInitMode() != FILE_CONFIGURATOR_MODE_VX_AGENT)
        {
            std::stringstream errMsg;
            errMsg << "Invalid configurator mode " + FileConfiguratorModes[getInitMode()];
            throw INMAGE_EX(errMsg.str().c_str());
        }

        AzureToAzureSourceAgentReplicationSettings azureToAzureSourceAgentReplicationSettings = JSON::consumer<AzureToAzureSourceAgentReplicationSettings>::convert(serializedSettings, true);
        AgentReplicationSettings replicationSettings;

        do {

            std::string hostId = getHostId();

            boost::to_lower(hostId);
            if (azureToAzureSourceAgentReplicationSettings.ReplicationSettings.find(hostId) != azureToAzureSourceAgentReplicationSettings.ReplicationSettings.end())
            {
                replicationSettings = azureToAzureSourceAgentReplicationSettings.ReplicationSettings[hostId];
                break;
            }

            boost::to_upper(hostId);
            if (azureToAzureSourceAgentReplicationSettings.ReplicationSettings.find(hostId) != azureToAzureSourceAgentReplicationSettings.ReplicationSettings.end())
            {
                replicationSettings = azureToAzureSourceAgentReplicationSettings.ReplicationSettings[hostId];
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
            m_pRcmClient->m_errorCode = RCM_CLIENT_HOSTID_NOT_PRESENT_IN_AZURETOAZURE_REPLICATIONESETTINGS;

            return SVE_FAIL;

        } while (0);

        AgentSettings agentSettings;

#ifdef SV_WINDOWS
        if (replicationSettings.IsClusterMember)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: replication settings recieved isclusternode as true\n", FUNCTION_NAME);
            agentSettings.m_properties.insert(std::make_pair(FailvoerClusterMemberType::IsClusterMember, "true"));

            AgentReplicationSettings clusterRepSetings;
            
            do {
                std::string clusterId = getClusterId();
                boost::to_lower(clusterId);
                if (azureToAzureSourceAgentReplicationSettings.ReplicationSettings.find(clusterId) != azureToAzureSourceAgentReplicationSettings.ReplicationSettings.end())
                {
                    clusterRepSetings = azureToAzureSourceAgentReplicationSettings.ReplicationSettings[clusterId];
                    break;
                }

                boost::to_upper(clusterId);
                if (azureToAzureSourceAgentReplicationSettings.ReplicationSettings.find(clusterId) != azureToAzureSourceAgentReplicationSettings.ReplicationSettings.end())
                {
                    clusterRepSetings = azureToAzureSourceAgentReplicationSettings.ReplicationSettings[clusterId];
                    break;
                }

                DebugPrintf(SV_LOG_ERROR, "%s: No entry present in the ReplicationSettings with hostId: %s. and clusterId: %s \n", FUNCTION_NAME, RcmConfigurator::getInstance()->getHostId().c_str(),
                    clusterId.c_str());
                m_pRcmClient->m_errorCode = RCM_CLIENT_CLUSTERID_NOT_PRESENT_IN_AZURETOAZURE_REPLICATIONESETTINGS;
                return SVE_FAIL;

            } while (false);
            
            replicationSettings.RcmProtectionPairActions.insert(replicationSettings.RcmProtectionPairActions.end(),
                clusterRepSetings.RcmProtectionPairActions.begin(),
                clusterRepSetings.RcmProtectionPairActions.end());

            replicationSettings.RcmReplicationPairActions.insert(replicationSettings.RcmReplicationPairActions.end(),
                clusterRepSetings.RcmReplicationPairActions.begin(),
                clusterRepSetings.RcmReplicationPairActions.end());

            replicationSettings.ProtectedDiskIds.insert(replicationSettings.ProtectedDiskIds.end(),
                clusterRepSetings.ProtectedDiskIds.begin(),
                clusterRepSetings.ProtectedDiskIds.end());

            replicationSettings.ReplicationPairMessageContexts.insert(
                clusterRepSetings.ReplicationPairMessageContexts.begin(),
                clusterRepSetings.ReplicationPairMessageContexts.end());
            
            agentSettings.m_ProtectedSharedDiskIds = clusterRepSetings.ProtectedDiskIds;

            agentSettings.m_clusterVirtualNodeProtectionContext = clusterRepSetings.ProtectionPairContext;

            agentSettings.m_clusterProtectionMessageContext = clusterRepSetings.ClusterProtectionMessageContext;

            if (clusterRepSetings.MonitoringMsgSettings.length())
            {
                const std::string input = securitylib::base64Decode(clusterRepSetings.MonitoringMsgSettings.c_str(),
                    clusterRepSetings.MonitoringMsgSettings.length());

                DebugPrintf(SV_LOG_DEBUG, "Cluster Monitor Msg Settings %s.\n", input.c_str());

                agentSettings.m_clusterMonitorMsgSettings =
                    JSON::consumer<AgentMonitoringMsgSettings>::convert(input, true);
            }

            replicationSettings.RcmJobs.insert(replicationSettings.RcmJobs.end(),
                clusterRepSetings.RcmJobs.begin(),
                clusterRepSetings.RcmJobs.end());
        }
        
#endif
        std::vector<RcmProtectionPairAction>::iterator iterProtPairActions =
            replicationSettings.RcmProtectionPairActions.begin();
        while (iterProtPairActions != replicationSettings.RcmProtectionPairActions.end())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Protection pair actions\n", FUNCTION_NAME);

            if (iterProtPairActions->ActionType == RcmActionTypes::CONSISTENCY)
            {
                std::string input = securitylib::base64Decode(iterProtPairActions->InputPayload.c_str(),
                    iterProtPairActions->InputPayload.length());

                agentSettings.m_consistencySettings =
                    JSON::consumer<ConsistencySettings>::convert(input, true);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Received unknown protection pair action type %s.\n",
                    FUNCTION_NAME, iterProtPairActions->ActionType.c_str());
            }
            iterProtPairActions++;
        }

        std::vector<RcmReplicationPairAction>::iterator iterRepPairActions =
            replicationSettings.RcmReplicationPairActions.begin();
        while (iterRepPairActions != replicationSettings.RcmReplicationPairActions.end())
        {
            if (iterRepPairActions->ActionType == RcmActionTypes::DRAIN_LOGS)
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: Replication pair action: drain logs\n", FUNCTION_NAME);

                const AzureToAzureDrainLogSettings azureToAzureDrainLogSettings =
                    JSON::consumer<AzureToAzureDrainLogSettings>::convert(
                        securitylib::base64Decode(iterRepPairActions->InputPayload.c_str(), iterRepPairActions->InputPayload.length()), true);
                DrainSettings drainSettings;
                drainSettings.DiskId = iterRepPairActions->DiskId;
                drainSettings.DataPathTransportType = azureToAzureDrainLogSettings.DataPathTransportType;
                drainSettings.DataPathTransportSettings = azureToAzureDrainLogSettings.DataPathTransportSettings;

                agentSettings.m_drainSettings.push_back(drainSettings);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Received unknown replication pair action type %s for disk %s\n",
                    FUNCTION_NAME, iterRepPairActions->ActionType.c_str(), iterRepPairActions->DiskId.c_str());
            }
            iterRepPairActions++;
        }

        if (replicationSettings.MonitoringMsgSettings.length())
        {
            const std::string input = securitylib::base64Decode(replicationSettings.MonitoringMsgSettings.c_str(),
                replicationSettings.MonitoringMsgSettings.length());

            DebugPrintf(SV_LOG_DEBUG, "Monitoring Message Settings %s.\n", input.c_str());

            agentSettings.m_monitorMsgSettings =
                JSON::consumer<AgentMonitoringMsgSettings>::convert(input, true);
        }

        if (replicationSettings.ShoeboxEventSettings.length())
        {
            const std::string input = securitylib::base64Decode(replicationSettings.ShoeboxEventSettings.c_str(),
                replicationSettings.ShoeboxEventSettings.length());

            DebugPrintf(SV_LOG_DEBUG, "Shoebox Event Settings %s.\n", input.c_str());

            agentSettings.m_shoeboxEventSettings =
                JSON::consumer<ShoeboxEventSettings>::convert(input, true);
        }
        agentSettings.m_protectedDiskIds = replicationSettings.ProtectedDiskIds;
        agentSettings.m_protectionPairContext = replicationSettings.ProtectionPairContext;
        agentSettings.m_replicationPairMsgContexts = replicationSettings.ReplicationPairMessageContexts;


        status = m_pAgentSettingsCacher->Persist(agentSettings);
        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to persist agent settings with error %s\n", FUNCTION_NAME, m_pAgentSettingsCacher->Error().c_str());
            // in case the settings are not cached, just use the in-memory settings
            // since this will be called on every settings change, agent will try to update the file again
            //return status;
        }

        AgentSettings prevSettings;
        status = GetAgentSettings(prevSettings);
        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to get current agent settings with error %d\n", FUNCTION_NAME, status);
            return status;
        }
        {
            boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
            m_agentSettings = agentSettings;
        }

        HandleChangeInAgentSettings(prevSettings);

        m_jobsChangedSignal(replicationSettings.RcmJobs);

    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with exception %s.\n settings : %s\n", FUNCTION_NAME, e.what(), serializedSettings.c_str());
        status = SVE_FAIL;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with unknown exception.\n settings : %s \n", FUNCTION_NAME, serializedSettings.c_str());
        status = SVE_FAIL;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

bool RcmConfigurator::IsTimeForAutoUpgrade(const MobilityAgentAutoUpgradeSchedule& autoUpgradeSchedule)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool isScheduleValid = false;

    if (autoUpgradeSchedule.RecurrencePattern == AutoUpgradeScheduleParams::Daily)
    {
        boost::posix_time::time_duration currentTime = boost::posix_time::second_clock::local_time().time_of_day();
        MobilityAgentUpgradeDailySchedule dailySchedule =
            JSON::consumer<MobilityAgentUpgradeDailySchedule>::convert(autoUpgradeSchedule.RecurrenceDetails, true);
        TimeWindow timeWindow = dailySchedule.UpgradeWindow;

        DebugPrintf(SV_LOG_DEBUG, "Current time -> %d:%d Scheduled Start time ->  %d:%d"
            "Scheduled End time ->  %d:%d\n", currentTime.hours(), currentTime.minutes(),
            timeWindow.StartHour, timeWindow.StartMinute, timeWindow.EndHour, timeWindow.EndMinute);

        if ((currentTime.hours() < timeWindow.StartHour) || ((currentTime.hours() == timeWindow.StartHour)
            && (currentTime.minutes() < timeWindow.StartMinute)))
        {
            isScheduleValid = false;
        }
        else if ((currentTime.hours() > timeWindow.EndHour) || ((currentTime.hours() == timeWindow.EndHour)
            && (currentTime.minutes() > timeWindow.EndMinute)))
        {
            isScheduleValid = false;
        }
        else
        {
            isScheduleValid = true;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Unexpected recurrence type :  %s\n", autoUpgradeSchedule.RecurrencePattern.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return isScheduleValid;
}

bool RcmConfigurator::IsAgentUpgradeJobValid(const std::vector<RcmJob>& RcmJobs,
    int autoUpgradeRetryCount)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    DebugPrintf(SV_LOG_DEBUG, "Retry count : %d\n", autoUpgradeRetryCount);
    if (autoUpgradeRetryCount >= AutoUpgradeScheduleParams::InitiateJobMaxRetryCount)
    {
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }
    for (std::vector<RcmJob>::const_iterator it = RcmJobs.begin(); it != RcmJobs.end(); it++)
    {
        RcmJob rcmJob = *it;
        if (rcmJob.JobType == RcmJobTypes::ON_PREM_TO_AZURE_AGENT_AUTO_UPGRADE ||
            rcmJob.JobType == RcmJobTypes::AZURE_TO_ONPREM_AGENT_AUTO_UPGRADE)
        {
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return false;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

SVSTATUS RcmConfigurator::ProcessAgentAutoUpgradeSetting(
    const MobilityAgentAutoUpgradeSchedule& autoUpgradeSchedule,
    const std::vector<RcmJob>& RcmJobs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    static int autoUpgradeRetryCount = 0;
    static boost::chrono::steady_clock::time_point lastUpdateTime =
        boost::chrono::steady_clock::time_point::min();
    boost::chrono::steady_clock::time_point curTime = boost::chrono::steady_clock::now();

    //TODO
    //  Check if Upgrade RCM Job is already invoked.
    if (IsTimeForAutoUpgrade(autoUpgradeSchedule))
    {
        if ((curTime > lastUpdateTime
            + boost::chrono::seconds(AutoUpgradeScheduleParams::ThrottleLimitInSeconds)) &&
            IsAgentUpgradeJobValid(RcmJobs, autoUpgradeRetryCount))
        {
            status = m_pRcmClient->InitiateOnPremToAzureAgentAutoUpgrade();
            if (status != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR,
                    "On Prem to Azure Agent Auto Upgrade RCM API call failed.\n");
            }
            else
            {
                autoUpgradeRetryCount++;
                DebugPrintf(SV_LOG_ALWAYS, "%s:Retry count : %d\n", FUNCTION_NAME,
                    autoUpgradeRetryCount);
                lastUpdateTime = curTime;
            }
        }
    }
    else
    {
        autoUpgradeRetryCount = 0;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::UpdateOnPremToAzureAgentSettingsCache(const std::string& serializedSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    try {
        OnPremToAzureSourceAgentSettings onPremToAzureSourceAgentSettings =
            JSON::consumer<OnPremToAzureSourceAgentSettings>::convert(serializedSettings, true);

        if (onPremToAzureSourceAgentSettings.IsAgentUpgradeable)
        {
            ProcessAgentAutoUpgradeSetting(onPremToAzureSourceAgentSettings.AutoUpgradeSchedule,
                onPremToAzureSourceAgentSettings.ReplicationSettings.RcmJobs);
        }

        UpdateRcmDetails();

        if (getMigrationState() == Migration::MIGRATION_SWITCHED_TO_RCM
            && onPremToAzureSourceAgentSettings.ReplicationSettings.IsInitialReplicationComplete)
        {
            setMigrationState(Migration::MIGRATION_SUCCESS);
            DebugPrintf(SV_LOG_ALWAYS, "%s: migration completed successfully.\n", FUNCTION_NAME);
        }

        AgentSettings onPremToAzureAgentSettings;

        if (!onPremToAzureSourceAgentSettings.ControlPathTransportType.empty())
        {
            if (!boost::iequals(onPremToAzureSourceAgentSettings.ControlPathTransportType, CONTROL_PATH_TYPE_RCM_PROXY))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: unknown control path type %s.\n", FUNCTION_NAME,
                    onPremToAzureSourceAgentSettings.ControlPathTransportType.c_str());
                return SVE_FAIL;
            }
            else
            {
                onPremToAzureAgentSettings.m_controlPathType = onPremToAzureSourceAgentSettings.ControlPathTransportType;
                onPremToAzureAgentSettings.m_controlPathSettings = onPremToAzureSourceAgentSettings.ControlPathTransportSettings;
            }
        }

        if (!onPremToAzureSourceAgentSettings.DataPathTransportType.empty())
        {
            onPremToAzureAgentSettings.m_dataPathType = onPremToAzureSourceAgentSettings.DataPathTransportType;
            onPremToAzureAgentSettings.m_dataPathSettings = onPremToAzureSourceAgentSettings.DataPathTransportSettings;
        }

        std::vector<RcmProtectionPairAction>::iterator iterProtPairActions =
            onPremToAzureSourceAgentSettings.ReplicationSettings.RcmProtectionPairActions.begin();
        while (iterProtPairActions != onPremToAzureSourceAgentSettings.ReplicationSettings.RcmProtectionPairActions.end())
        {
            if (iterProtPairActions->ActionType == RcmActionTypes::ON_PREM_TO_AZURE_CONSISTENCY)
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: Protection pair action: %s\n", FUNCTION_NAME,
                    RcmActionTypes::ON_PREM_TO_AZURE_CONSISTENCY);
                onPremToAzureAgentSettings.m_consistencySettings =
                    JSON::consumer<ConsistencySettings>::convert(iterProtPairActions->InputPayload, true);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Received unknown protection pair action type %s.\n",
                    FUNCTION_NAME, iterProtPairActions->ActionType.c_str());
            }
            iterProtPairActions++;
        }

        std::vector<RcmReplicationPairAction>::iterator iterRepPairActions =
            onPremToAzureSourceAgentSettings.ReplicationSettings.RcmReplicationPairActions.begin();
        while (iterRepPairActions != onPremToAzureSourceAgentSettings.ReplicationSettings.RcmReplicationPairActions.end())
        {
            if (iterRepPairActions->ActionType == RcmActionTypes::ON_PREM_TO_AZURE_DRAIN_LOGS)
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: Replication pair action: %s\n", FUNCTION_NAME,
                    RcmActionTypes::ON_PREM_TO_AZURE_DRAIN_LOGS);

                const OnPremToAzureDrainLogSettings onPremToAzureDrainLogSettings =
                    JSON::consumer<OnPremToAzureDrainLogSettings>::convert(iterRepPairActions->InputPayload, true);
                DrainSettings drainSettings;
                drainSettings.DiskId = iterRepPairActions->DiskId;
                drainSettings.LogFolder = onPremToAzureDrainLogSettings.LogFolder;

                onPremToAzureAgentSettings.m_drainSettings.push_back(drainSettings);
            }
            else if (iterRepPairActions->ActionType == RcmActionTypes::ON_PREM_TO_AZURE_SYNC)
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: Replication pair action: %s\n", FUNCTION_NAME,
                    RcmActionTypes::ON_PREM_TO_AZURE_SYNC);
                const OnPremToAzureSourceAgentProcessServerBasedSyncSettings sourceAgentSyncSettings =
                    JSON::consumer<OnPremToAzureSourceAgentProcessServerBasedSyncSettings>::convert(iterRepPairActions->InputPayload, true);

                SyncSettings syncSettings;
                syncSettings.DiskId = iterRepPairActions->DiskId;
                syncSettings.ReplicationPairId = sourceAgentSyncSettings.ReplicationPairId;
                syncSettings.ReplicationSessionId = sourceAgentSyncSettings.ReplicationSessionId;
                syncSettings.LogFolder = sourceAgentSyncSettings.LogFolder;
                syncSettings.ResyncProgressType = sourceAgentSyncSettings.ResyncProgressType;
                syncSettings.ResyncCopyInitiatedTimeTicks = sourceAgentSyncSettings.ResyncCopyInitiatedTimeTicks;

                onPremToAzureAgentSettings.m_syncSettings.push_back(syncSettings);
            }
            else if (iterRepPairActions->ActionType == RcmActionTypes::ON_PREM_TO_AZURE_NO_DATA_TRNSFER_SYNC)
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: Replication pair action: %s\n", FUNCTION_NAME,
                    RcmActionTypes::ON_PREM_TO_AZURE_NO_DATA_TRNSFER_SYNC);
                const OnPremToAzureSourceAgentNoDataTransferSyncSettings sourceAgentNoDataSyncSettings =
                    JSON::consumer<OnPremToAzureSourceAgentNoDataTransferSyncSettings>::convert(iterRepPairActions->InputPayload, true);

                SyncSettings syncSettings;
                syncSettings.DiskId = iterRepPairActions->DiskId;
                syncSettings.ReplicationPairId = sourceAgentNoDataSyncSettings.ReplicationPairId;
                syncSettings.ReplicationSessionId = sourceAgentNoDataSyncSettings.ReplicationSessionId;
                syncSettings.LogFolder = sourceAgentNoDataSyncSettings.LogFolder;
                syncSettings.ResyncProgressType = sourceAgentNoDataSyncSettings.ResyncProgressType;
                syncSettings.SyncType = SYNC_TYPE_NO_DATA_TRANSFER;
                syncSettings.LatestReplicationCycleBookmarkSequenceId = sourceAgentNoDataSyncSettings.LatestReplicationCycleBookmarkSequenceId;
                syncSettings.LatestReplicationCycleBookmarkTime = sourceAgentNoDataSyncSettings.LatestReplicationCycleBookmarkTime;
                syncSettings.ResyncCopyInitiatedTimeTicks = sourceAgentNoDataSyncSettings.ResyncCopyInitiatedTimeTicks;

                onPremToAzureAgentSettings.m_syncSettings.push_back(syncSettings);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Received unknown replication pair action type %s for disk %s.\n",
                    FUNCTION_NAME, iterRepPairActions->ActionType.c_str(), iterRepPairActions->DiskId.c_str());
            }
            iterRepPairActions++;
        }

        onPremToAzureAgentSettings.m_monitorMsgSettings = onPremToAzureSourceAgentSettings.ReplicationSettings.VmMonitoringMsgSettings;
        onPremToAzureAgentSettings.m_protectedDiskIds = onPremToAzureSourceAgentSettings.ReplicationSettings.ProtectedDiskIds;
        onPremToAzureAgentSettings.m_protectionPairContext = onPremToAzureSourceAgentSettings.ReplicationSettings.ProtectionPairContext;
        onPremToAzureAgentSettings.m_replicationPairMsgContexts = onPremToAzureSourceAgentSettings.ReplicationSettings.ReplicationPairMessageContexts;
        onPremToAzureAgentSettings.m_tunables = onPremToAzureSourceAgentSettings.Tunables;
        onPremToAzureAgentSettings.m_telemetrySettings = JSON::producer<ProtectedMachineTelemetrySettings>::convert(onPremToAzureSourceAgentSettings.TelemetrySettings);
        onPremToAzureAgentSettings.m_autoResyncTimeWindow = onPremToAzureSourceAgentSettings.AutoResyncTimeWindow;

        if (getInitMode() == FILE_CONFIGURATOR_MODE_VX_AGENT)
        {
            status = m_pAgentSettingsCacher->Persist(onPremToAzureAgentSettings);
            if (status != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: failed to persist agent settings with error %s\n", FUNCTION_NAME, m_pAgentSettingsCacher->Error().c_str());
                // in case the settings are not cached, just use the in-memory settings
                // since this will be called on every settings change, agent will try to update the file again
                //return status;
            }
        }

        AgentSettings prevOnPremToAzureAgentSettings;
        status = GetAgentSettings(prevOnPremToAzureAgentSettings);
        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to get current agent settings with error %d\n", FUNCTION_NAME, status);
            return status;
        }
        {
            boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
            m_agentSettings = onPremToAzureAgentSettings;
        }

        HandleChangeInAgentSettings(prevOnPremToAzureAgentSettings);

        m_jobsChangedSignal(onPremToAzureSourceAgentSettings.ReplicationSettings.RcmJobs);

    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with exception %s.\n settings : %s\n", FUNCTION_NAME, e.what(), serializedSettings.c_str());
        status = SVE_FAIL;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with unknown exception.\n settings : %s \n", FUNCTION_NAME, serializedSettings.c_str());
        status = SVE_FAIL;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

SVSTATUS RcmConfigurator::UpdateAzureToOnPremAgentSettingsCache(const std::string& serializedSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    try {
        AzureToOnPremSourceAgentSettings azureToOnPremSourceAgentSettings =
            JSON::consumer<AzureToOnPremSourceAgentSettings>::convert(serializedSettings, true);

        if (azureToOnPremSourceAgentSettings.IsAgentUpgradeable)
        {
            ProcessAgentAutoUpgradeSetting(azureToOnPremSourceAgentSettings.AutoUpgradeSchedule,
                azureToOnPremSourceAgentSettings.ReplicationSettings.RcmJobs);
        }

        AgentSettings azureToOnPremAgentSettings;

        std::vector<RcmProtectionPairAction>::iterator iterProtPairActions =
            azureToOnPremSourceAgentSettings.ReplicationSettings.RcmProtectionPairActions.begin();
        while (iterProtPairActions != azureToOnPremSourceAgentSettings.ReplicationSettings.RcmProtectionPairActions.end())
        {
            if (iterProtPairActions->ActionType == RcmActionTypes::AZURE_TO_ONPREM_CONSISTENCY)
            {
                azureToOnPremAgentSettings.m_consistencySettings =
                    JSON::consumer<ConsistencySettings>::convert(iterProtPairActions->InputPayload, true);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Received unknown protection pair action type %s.\n",
                    FUNCTION_NAME, iterProtPairActions->ActionType.c_str());
            }
            iterProtPairActions++;
        }

        std::vector<RcmReplicationPairAction>::iterator iterRepPairActions =
            azureToOnPremSourceAgentSettings.ReplicationSettings.RcmReplicationPairActions.begin();
        while (iterRepPairActions != azureToOnPremSourceAgentSettings.ReplicationSettings.RcmReplicationPairActions.end())
        {
            if (iterRepPairActions->ActionType == RcmActionTypes::AZURE_TO_ONPREM_DRAIN_LOGS)
            {
                AzureToOnPremDrainLogSettings azureToOnPremDrainLogSettings =
                    JSON::consumer<AzureToOnPremDrainLogSettings>::convert(iterRepPairActions->InputPayload, true);

                DrainSettings drainSettings;
                drainSettings.DiskId = iterRepPairActions->DiskId;
                drainSettings.DataPathTransportType = TRANSPORT_TYPE_AZURE_BLOB;
                drainSettings.DataPathTransportSettings = JSON::producer<AzureBlobBasedTransportSettings>::convert(azureToOnPremDrainLogSettings.TransportSettings);

                azureToOnPremAgentSettings.m_drainSettings.push_back(drainSettings);

            }
            else if (iterRepPairActions->ActionType == RcmActionTypes::AZURE_TO_ONPREM_SYNC)
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: Replication pair action: %s\n", FUNCTION_NAME,
                    RcmActionTypes::ON_PREM_TO_AZURE_SYNC);
                const AzureToOnPremSyncSettings sourceAgentSyncSettings =
                    JSON::consumer<AzureToOnPremSyncSettings>::convert(iterRepPairActions->InputPayload, true);

                SyncSettings syncSettings;
                syncSettings.DiskId = iterRepPairActions->DiskId;
                syncSettings.ReplicationPairId = sourceAgentSyncSettings.ReplicationPairId;
                syncSettings.ReplicationSessionId = sourceAgentSyncSettings.ReplicationSessionId;
                syncSettings.TransportSettings = sourceAgentSyncSettings.TransportSettings;
                syncSettings.ResyncProgressType = sourceAgentSyncSettings.ResyncProgressType;
                syncSettings.ResyncCopyInitiatedTimeTicks = sourceAgentSyncSettings.ResyncCopyInitiatedTimeTicks;

                azureToOnPremAgentSettings.m_syncSettings.push_back(syncSettings);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Received unknown replication pair action type %s for disk %s.\n",
                    FUNCTION_NAME, iterRepPairActions->ActionType.c_str(), iterRepPairActions->DiskId.c_str());
            }
            iterRepPairActions++;
        }

        azureToOnPremAgentSettings.m_monitorMsgSettings = azureToOnPremSourceAgentSettings.ReplicationSettings.VmMonitoringMsgSettings;
        azureToOnPremAgentSettings.m_protectedDiskIds = azureToOnPremSourceAgentSettings.ReplicationSettings.ProtectedDiskIds;
        azureToOnPremAgentSettings.m_protectionPairContext = azureToOnPremSourceAgentSettings.ReplicationSettings.ProtectionPairContext;
        azureToOnPremAgentSettings.m_replicationPairMsgContexts = azureToOnPremSourceAgentSettings.ReplicationSettings.ReplicationPairMessageContexts;
        azureToOnPremAgentSettings.m_tunables = azureToOnPremSourceAgentSettings.Tunables;
        azureToOnPremAgentSettings.m_telemetrySettings = JSON::producer<AzureToOnPremTelemetrySettings>::convert(azureToOnPremSourceAgentSettings.ReplicationSettings.TelemetrySettings);
        azureToOnPremAgentSettings.m_dataPathType = TRANSPORT_TYPE_AZURE_BLOB;
        azureToOnPremAgentSettings.m_autoResyncTimeWindow = azureToOnPremSourceAgentSettings.AutoResyncTimeWindow;
        azureToOnPremAgentSettings.m_bPrivateEndpointEnabled = azureToOnPremSourceAgentSettings.IsPrivateEndpointEnabled;

        if (getInitMode() == FILE_CONFIGURATOR_MODE_VX_AGENT)
        {
            status = m_pAgentSettingsCacher->Persist(azureToOnPremAgentSettings);
            if (status != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: failed to persist agent settings with error %s\n", FUNCTION_NAME, m_pAgentSettingsCacher->Error().c_str());
                // in case the settings are not cached, just use the in-memory settings
                // since this will be called on every settings change, agent will try to update the file again
                //return status;
            }
            status = m_pRcmProxySettingsCacherPtr->PersistRCMProxySettings(azureToOnPremSourceAgentSettings.RcmProxyTransportSettings);
            if (status != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: failed to persist RCM proxy settings with error %s\n", FUNCTION_NAME, m_pRcmProxySettingsCacherPtr->Error().c_str());
            }

            // since an exception here will block processing the replication settings,
            // just catch and log failures
            try {
                AgentConfigHelpers::UpdateRcmProxyTransportSettings(GetSourceAgentConfigPath(), azureToOnPremSourceAgentSettings.RcmProxyTransportSettings);
            }
            catch (const std::exception& e)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: failed to update source config settings with error %s\n", FUNCTION_NAME, e.what());
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: failed to update source config settings with unknown exception.\n", FUNCTION_NAME);
            }
        }
        else
        {
            // in DP mode it is no-op
        }

        AgentSettings prevAzureToOnPremAgentSettings;
        status = GetAgentSettings(prevAzureToOnPremAgentSettings);
        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to get current agent settings with error %d\n", FUNCTION_NAME, status);
            return status;
        }
        {
            boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
            m_agentSettings = azureToOnPremAgentSettings;
        }

        HandleChangeInAgentSettings(prevAzureToOnPremAgentSettings);

        m_jobsChangedSignal(azureToOnPremSourceAgentSettings.ReplicationSettings.RcmJobs);

    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with exception %s.\n settings : %s\n", FUNCTION_NAME, e.what(), serializedSettings.c_str());
        status = SVE_FAIL;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with unknown exception.\n settings : %s \n", FUNCTION_NAME, serializedSettings.c_str());
        status = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::UpdateDataProtectionSyncRcmTargetSettings(const std::string& serializedSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    try {
        const DataProtectionSyncRcmTargetSettings dpSyncRcmTargetSettings =
            JSON::consumer<DataProtectionSyncRcmTargetSettings>::convert(serializedSettings, true);

        AgentSettings agentSettings;

        // check the control path and data path types for supported types
        if (!dpSyncRcmTargetSettings.ControlPathTransportType.empty())
        {
            if (!boost::iequals(dpSyncRcmTargetSettings.ControlPathTransportType, CONTROL_PATH_TYPE_RCM))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: unknown control path type %s.\n", FUNCTION_NAME,
                    dpSyncRcmTargetSettings.ControlPathTransportType.c_str());
                return SVE_FAIL;
            }
            else
            {
                agentSettings.m_controlPathType = dpSyncRcmTargetSettings.ControlPathTransportType;
                agentSettings.m_controlPathSettings = dpSyncRcmTargetSettings.ControlPathTransportSettings;
            }
        }

        if (!dpSyncRcmTargetSettings.DataPathTransportType.empty())
        {
            agentSettings.m_dataPathType = dpSyncRcmTargetSettings.DataPathTransportType;
            agentSettings.m_dataPathSettings = dpSyncRcmTargetSettings.DataPathTransportSettings;
        }

        if (dpSyncRcmTargetSettings.ReplicationPairAction.ActionType == RcmActionTypes::ON_PREM_TO_AZURE_SYNC)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Replication pair action: %s\n", FUNCTION_NAME,
                RcmActionTypes::ON_PREM_TO_AZURE_SYNC);

            const OnPremToAzureSourceAgentProcessServerBasedSyncSettings sourceAgentSyncSettings =
                JSON::consumer<OnPremToAzureSourceAgentProcessServerBasedSyncSettings>::convert(
                    dpSyncRcmTargetSettings.ReplicationPairAction.InputPayload, true);

            SyncSettings syncSettings;
            syncSettings.DiskId = dpSyncRcmTargetSettings.ReplicationPairAction.DiskId;
            syncSettings.ReplicationPairId = sourceAgentSyncSettings.ReplicationPairId;
            syncSettings.ReplicationSessionId = sourceAgentSyncSettings.ReplicationSessionId;
            syncSettings.LogFolder = sourceAgentSyncSettings.LogFolder;
            syncSettings.ResyncProgressType = sourceAgentSyncSettings.ResyncProgressType;
            syncSettings.ResyncCopyInitiatedTimeTicks = sourceAgentSyncSettings.ResyncCopyInitiatedTimeTicks;

            agentSettings.m_syncSettings.push_back(syncSettings);
        }
        else if (dpSyncRcmTargetSettings.ReplicationPairAction.ActionType == RcmActionTypes::ON_PREM_TO_AZURE_NO_DATA_TRNSFER_SYNC)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Replication pair action: %s\n", FUNCTION_NAME,
                RcmActionTypes::ON_PREM_TO_AZURE_NO_DATA_TRNSFER_SYNC);

            const OnPremToAzureSourceAgentNoDataTransferSyncSettings sourceAgentNoDataSyncSettings =
                JSON::consumer<OnPremToAzureSourceAgentNoDataTransferSyncSettings>::convert(dpSyncRcmTargetSettings.ReplicationPairAction.InputPayload, true);

            SyncSettings syncSettings;
            syncSettings.DiskId = dpSyncRcmTargetSettings.ReplicationPairAction.DiskId;
            syncSettings.ReplicationPairId = sourceAgentNoDataSyncSettings.ReplicationPairId;
            syncSettings.ReplicationSessionId = sourceAgentNoDataSyncSettings.ReplicationSessionId;
            syncSettings.LogFolder = sourceAgentNoDataSyncSettings.LogFolder;
            syncSettings.ResyncProgressType = sourceAgentNoDataSyncSettings.ResyncProgressType;
            syncSettings.SyncType = SYNC_TYPE_NO_DATA_TRANSFER;
            syncSettings.LatestReplicationCycleBookmarkSequenceId = sourceAgentNoDataSyncSettings.LatestReplicationCycleBookmarkSequenceId;
            syncSettings.LatestReplicationCycleBookmarkTime = sourceAgentNoDataSyncSettings.LatestReplicationCycleBookmarkTime;
            syncSettings.ResyncCopyInitiatedTimeTicks = sourceAgentNoDataSyncSettings.ResyncCopyInitiatedTimeTicks;

            agentSettings.m_syncSettings.push_back(syncSettings);
        }
        else if ((dpSyncRcmTargetSettings.ReplicationPairAction.ActionType == RcmActionTypes::AZURE_TO_ONPREM_SYNC) ||
            (dpSyncRcmTargetSettings.ReplicationPairAction.ActionType == RcmActionTypes::AZURE_TO_ONPREM_REPROTECT_SYNC_APPLIANCE))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Replication pair action: %s\n", FUNCTION_NAME,
                dpSyncRcmTargetSettings.ReplicationPairAction.ActionType.c_str());

            const AzureToOnPremSyncSettings sourceAgentSyncSettings =
                JSON::consumer<AzureToOnPremSyncSettings>::convert(
                    dpSyncRcmTargetSettings.ReplicationPairAction.InputPayload, true);

            SyncSettings syncSettings;
            syncSettings.DiskId = dpSyncRcmTargetSettings.ReplicationPairAction.DiskId;
            syncSettings.ReplicationPairId = sourceAgentSyncSettings.ReplicationPairId;
            syncSettings.ReplicationSessionId = sourceAgentSyncSettings.ReplicationSessionId;
            syncSettings.TargetDiskId = sourceAgentSyncSettings.TargetDiskId;
            syncSettings.TransportSettings = sourceAgentSyncSettings.TransportSettings;
            syncSettings.ResyncProgressType = sourceAgentSyncSettings.ResyncProgressType;
            syncSettings.ResyncCopyInitiatedTimeTicks = sourceAgentSyncSettings.ResyncCopyInitiatedTimeTicks;

            agentSettings.m_syncSettings.push_back(syncSettings);
            agentSettings.m_dataPathType = TRANSPORT_TYPE_AZURE_BLOB;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Received unknown replication pair action type %s for disk %s.\n",
                FUNCTION_NAME, dpSyncRcmTargetSettings.ReplicationPairAction.ActionType.c_str(),
                dpSyncRcmTargetSettings.ReplicationPairAction.DiskId.c_str());
        }

        agentSettings.m_protectionPairContext = dpSyncRcmTargetSettings.ProtectionPairContext;
        agentSettings.m_replicationPairMsgContexts.insert(
            std::make_pair(dpSyncRcmTargetSettings.ReplicationPairAction.DiskId,
                dpSyncRcmTargetSettings.ReplicationPairMessageContext));

        ProtectedMachineTelemetrySettings telemetrySettings = dpSyncRcmTargetSettings.TelemetrySettings;
        agentSettings.m_telemetrySettings = JSON::producer<ProtectedMachineTelemetrySettings>::convert(telemetrySettings);
        agentSettings.m_tunables = dpSyncRcmTargetSettings.Tunables;
        agentSettings.m_properties = dpSyncRcmTargetSettings.Properties;

        setTelemetryFolderPathOnCsPrimeApplianceToAzure(dpSyncRcmTargetSettings.TelemetrySettings.TelemetryFolder);
        setGlobalTunablesOnCsPrimeApplianceToAzure(dpSyncRcmTargetSettings.Tunables);
        setPropertiesOnCsPrimeApplianceToAzure(dpSyncRcmTargetSettings.Properties);
        initConfigParamsOnCsPrimeApplianceToAzureAgent();

        AgentSettings prevAgentSettings;
        status = GetAgentSettings(prevAgentSettings);
        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to get current agent settings with error %d\n", FUNCTION_NAME, status);
            return status;
        }
        {
            boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
            m_agentSettings = agentSettings;
        }

        HandleChangeInAgentSettings(prevAgentSettings);

    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with exception %s.\n settings : %s\n", FUNCTION_NAME, e.what(), serializedSettings.c_str());
        status = SVE_FAIL;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with unknown exception.\n settings : %s \n", FUNCTION_NAME, serializedSettings.c_str());
        status = SVE_FAIL;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}


void RcmConfigurator::HandleChangeInAgentSettings(const AgentSettings& oldAgentSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    try {
        bool bRcmProxyDetailsChanged = false;
        bool bProcessServerDetailsChanged = false;
        bool bSyncSettingsChanged = false;
        bool bDrainSettingsChanged = false;
        bool bReplicaioniDisksChanged = false;
        bool bProtectedMachineTelemetrySettingsChanged = false;
        bool bTunablesChanged = false;
        bool bPropertiesChanged = false;


        bDrainSettingsChanged = !(oldAgentSettings.m_drainSettings == m_agentSettings.m_drainSettings);
        bReplicaioniDisksChanged = !(oldAgentSettings.m_protectedDiskIds == m_agentSettings.m_protectedDiskIds);
        if (!IsAzureToAzureReplication())
        {
            if (!oldAgentSettings.m_controlPathType.empty())
            {
                bRcmProxyDetailsChanged = !(oldAgentSettings.m_controlPathSettings ==
                    m_agentSettings.m_controlPathSettings);
            }

            if (!oldAgentSettings.m_dataPathType.empty())
            {
                bProcessServerDetailsChanged = !(oldAgentSettings.m_dataPathSettings ==
                    m_agentSettings.m_dataPathSettings);
            }

            bSyncSettingsChanged = !(oldAgentSettings.m_syncSettings == m_agentSettings.m_syncSettings);
            bProtectedMachineTelemetrySettingsChanged = !(oldAgentSettings.m_telemetrySettings == m_agentSettings.m_telemetrySettings);
            bTunablesChanged = !(oldAgentSettings.m_tunables == m_agentSettings.m_tunables);
            bPropertiesChanged = !(oldAgentSettings.m_properties == m_agentSettings.m_properties);
        }


        if (bReplicaioniDisksChanged || bSyncSettingsChanged || bProcessServerDetailsChanged)
        {
            m_syncSettingsChangedSignal();
        }

        if (bProtectedMachineTelemetrySettingsChanged || bTunablesChanged || bPropertiesChanged)
        {
            m_tunablePropertiesChangedSignal();
        }

        if (bReplicaioniDisksChanged || bDrainSettingsChanged || bProcessServerDetailsChanged)
        {
            m_drainSettingsChangedSignal();
        }
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with exception %s\n", FUNCTION_NAME, e.what());
        status = SVE_FAIL;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with unknown exception\n", FUNCTION_NAME);
        status = SVE_FAIL;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

SVSTATUS RcmConfigurator::GetDataPathTransportSettings(std::string& dataTransportType,
    std::string& dataPathTransportSettings) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);

    dataTransportType = m_agentSettings.m_dataPathType;
    dataPathTransportSettings = m_agentSettings.m_dataPathSettings;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SVS_OK;
}

SVSTATUS RcmConfigurator::GetSyncSettings(const std::string& diskid,
    SyncSettings& settings,
    std::string& dataTransportType,
    std::string& dataPathTransportSettings) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVE_FAIL;

    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);

    SyncSettings_t::const_iterator iter = m_agentSettings.m_syncSettings.begin();
    for (/*empty*/; iter != m_agentSettings.m_syncSettings.end(); iter++)
    {
        if (boost::iequals(diskid, iter->DiskId))
        {
            settings = *iter;
            dataTransportType = m_agentSettings.m_dataPathType;
            dataPathTransportSettings = m_agentSettings.m_dataPathSettings;
            status = SVS_OK;
            break;
        }
    }
    if (iter == m_agentSettings.m_syncSettings.end() &&
        status == SVE_FAIL)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: SyncSettings not found for disk %s\n", FUNCTION_NAME, diskid.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetDrainSettings(
    DrainSettings_t& drainsettings,
    std::string& dataTransportType,
    std::string& dataPathTransportSettings) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
    drainsettings = m_agentSettings.m_drainSettings;
    dataTransportType = m_agentSettings.m_dataPathType;
    dataPathTransportSettings = m_agentSettings.m_dataPathSettings;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SVS_OK;
}

bool RcmConfigurator::IsClusterSharedDiskProtected(const std::string& diskId) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool isClusterVirtualNodeDisk = false;

    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
    std::vector<std::string>::const_iterator itr = m_agentSettings.m_ProtectedSharedDiskIds.begin();
    std::string clusterVirtualNodeDisk;
    for (/**/; 
        itr != m_agentSettings.m_ProtectedSharedDiskIds.end(); 
        itr++)
    {
        clusterVirtualNodeDisk = *itr;
        if (boost::iequals(diskId, clusterVirtualNodeDisk))
        {
            isClusterVirtualNodeDisk = true;
            break;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s, %d\n", FUNCTION_NAME, isClusterVirtualNodeDisk);
    return isClusterVirtualNodeDisk;
}

SVSTATUS RcmConfigurator::GetAgentSettings(AgentSettings& settings) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
    settings = m_agentSettings;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetProtectedDiskIds(std::vector<std::string> &diskIds) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
    diskIds = m_agentSettings.m_protectedDiskIds;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetConsistencySettings(ConsistencySettings &consistencySettings) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
    consistencySettings = m_agentSettings.m_consistencySettings;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetShoeboxEventSettings(ShoeboxEventSettings& settings) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
    settings = m_agentSettings.m_shoeboxEventSettings;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetMonitoringMessageSettings(AgentMonitoringMsgSettings& settings, const bool& isClusterVirtualNodeContext) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
    if (isClusterVirtualNodeContext)
    {
        settings = m_agentSettings.m_clusterMonitorMsgSettings;
    }
    else
    {
        settings = m_agentSettings.m_monitorMsgSettings;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

RcmClientPtr RcmConfigurator::GetRcmClient() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return m_pRcmClient;
}

SVSTATUS RcmConfigurator::RefreshRcmClientSettings()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::system::error_code ec;
    std::string rcmSettingsPath = getRcmSettingsPath();
    if (!boost::filesystem::exists(rcmSettingsPath, ec))
    {
        std::stringstream errMsg;
        errMsg << "RCM client settings file "
            << rcmSettingsPath
            << " does not exist. Error=" << ec.message().c_str() << '.';
        throw INMAGE_EX(errMsg.str().c_str());
    }

    m_pRcmClientSettings.reset(new RcmClientSettings(rcmSettingsPath));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return SVS_OK;
}

RcmClientSettingsConstPtr RcmConfigurator::GetRcmClientSettings() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return m_pRcmClientSettings;
}

std::string RcmConfigurator::GetConfiguredBiosId() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return m_pRcmClientSettings->m_BiosId;
}

std::string RcmConfigurator::GetManagementId() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return m_pRcmClientSettings->m_ManagementId;
}

std::string RcmConfigurator::GetClusterManagementId() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "%s: clustermanagementid is: %s\n", FUNCTION_NAME, m_pRcmClientSettings->m_ClusterManagementId.c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return m_pRcmClientSettings->m_ClusterManagementId;
}

std::string RcmConfigurator::GetClientRequestId() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return m_pRcmClientSettings->m_ClientRequestId;
}

std::string RcmConfigurator::GetRcmServiceUri() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return m_pRcmClientSettings->m_ServiceUri;
}

std::string RcmConfigurator::GetAzureADServiceUri() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return m_pRcmClientSettings->m_AADUri;
}

bool RcmConfigurator::IsPrivateEndpointEnabled() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (m_agentSettings.m_bPrivateEndpointEnabled ||
        boost::iequals(m_pRcmClientSettings->m_EndPointType, NetworkEndPointType::PRIVATE_ENDPOINT) ||
        boost::iequals(m_pRcmClientSettings->m_EndPointType, NetworkEndPointType::MIXED_ENDPOINT));
}

bool RcmConfigurator::IsClusterSharedDiskEnabled() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

#ifdef SV_WINDOWS
    return (IsAzureToAzureReplication() 
        && !getClusterId().empty() 
        && m_agentSettings.m_properties.find(FailvoerClusterMemberType::IsClusterMember)
        != m_agentSettings.m_properties.end());
#endif
    return false;
}

std::string RcmConfigurator::GetRcmProtectionStatePath() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    const std::string RcmProtectionStateFile(RCM_PROTECTION_STATE_FILE_NAME);
    std::string rcmProtectionStatePath;

    if (getConfigDirname(rcmProtectionStatePath))
    {
        BOOST_ASSERT(!rcmProtectionStatePath.empty());
        boost::trim(rcmProtectionStatePath);
        if (!boost::ends_with(rcmProtectionStatePath, ACE_DIRECTORY_SEPARATOR_STR_A))
            rcmProtectionStatePath += ACE_DIRECTORY_SEPARATOR_STR_A;

        rcmProtectionStatePath += RcmProtectionStateFile;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rcmProtectionStatePath;
}

void RcmConfigurator::PersistDisableProtectionState()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::shared_mutex> guard(m_protectionStateMutex);

    std::string filePath = GetRcmProtectionStatePath();
    boost::system::error_code ec;
    if (!boost::filesystem::exists(filePath, ec))
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Rcm protection state file %s does not exist. Error=%s, creating one.\n", FUNCTION_NAME, filePath.c_str(), ec.message().c_str());
        std::ofstream stateFile(filePath.c_str());
        if (!stateFile.good())
        {
            std::stringstream errMsg;
            errMsg << "Failed to create replication state file at "
                << filePath;
            throw INMAGE_EX(errMsg.str().c_str());
        }

        securitylib::setPermissions(filePath, SET_PERMISSIONS_NO_CREATE);

        // write the hostid for which the protection is disabled
        ProtectionState state;

        state.m_stateIndicators.insert(std::make_pair(RCM_PROTECTION_STATE_INDICATOR_DISABLED_HOSTID, getHostId()));

        std::string serializedState = JSON::producer<ProtectionState>::convert(state);

        DebugPrintf(SV_LOG_ALWAYS, "%s: serialized protection state %s\n", FUNCTION_NAME, serializedState.c_str());

        stateFile << serializedState;

        stateFile.flush();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

std::string RcmConfigurator::GetProtectionStateIndicatorValue(const std::string& indicatorKey)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::shared_lock<boost::shared_mutex> guard(m_protectionStateMutex);

    std::string indicatorValue;
    std::string filePath = GetRcmProtectionStatePath();
    boost::system::error_code ec;
    if (!boost::filesystem::exists(filePath, ec))
    {
        std::stringstream errMsg;
        errMsg << "Failed to find replication state file at "
            << filePath << " with error " << ec.value() << " : " << ec.message();
        throw INMAGE_EX(errMsg.str().c_str());
    }

    std::ifstream stateFile(filePath.c_str());
    if (!stateFile.good())
    {
        std::stringstream errMsg;
        errMsg << "Failed to read replication state file at "
            << filePath;
        throw INMAGE_EX(errMsg.str().c_str());
    }

    std::string jsonInput((std::istreambuf_iterator<char>(stateFile)), std::istreambuf_iterator<char>());

    if (jsonInput.empty())
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: state indicator %s is not found in persisted state as file is empty.\n",
            FUNCTION_NAME,
            indicatorKey.c_str());
    }
    else
    {
        ProtectionState state = JSON::consumer<ProtectionState>::convert(jsonInput);
        std::map<std::string, std::string>::iterator it = state.m_stateIndicators.find(indicatorKey);

        if (it == state.m_stateIndicators.end())
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: state indicator %s is not found in persisted state.\n",
                FUNCTION_NAME,
                indicatorKey.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS,
                "%s: state indicator %s has a value %s in persisted state.\n",
                FUNCTION_NAME,
                indicatorKey.c_str(),
                it->second.c_str());

            indicatorValue = it->second;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return indicatorValue;
}

std::string RcmConfigurator::GetProtectionDisabledHostId()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return GetProtectionStateIndicatorValue(RCM_PROTECTION_STATE_INDICATOR_DISABLED_HOSTID);
}

void RcmConfigurator::ClearDisableProtectionState()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::shared_mutex> guard(m_protectionStateMutex);

    std::string filePath = GetRcmProtectionStatePath();
    boost::system::error_code ec;
    if (boost::filesystem::exists(filePath, ec))
    {
        DebugPrintf(SV_LOG_INFO, "%s: Clearing disable protection state %s.\n", FUNCTION_NAME, filePath.c_str());
        if (!boost::filesystem::remove(filePath, ec))
        {
            std::stringstream errMsg;
            errMsg << "Failed to clear replication state file at "
                << filePath
                << " with error "
                << ec.message().c_str();
            throw INMAGE_EX(errMsg.str().c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

bool RcmConfigurator::IsProtectionDisabled()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::shared_lock<boost::shared_mutex> guard(m_protectionStateMutex);
    bool ret = boost::filesystem::exists(GetRcmProtectionStatePath());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return ret;
}

bool RcmConfigurator::IsRegistrationNotSupported()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool ret = false;
    if (m_pRcmClient->GetErrorCode() == RCM_CLIENT_REGISTRATION_NOT_SUPPORTED) {
        ret = true;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return ret;
}

bool RcmConfigurator::IsRegistrationNotAllowed()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool ret = false;
    if (m_pRcmClient->GetErrorCode() == RCM_CLIENT_REGISTRATION_NOT_ALLOWED) {
        ret = true;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return ret;
}

bool RcmConfigurator::IsRegistrationFailedDueToMachinedoesNotExistError()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool ret = false;
    if (m_pRcmClient->GetErrorCode() == RCM_CLIENT_PROTECTION_DISABLED) {
        ret = true;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return ret;
}

bool RcmConfigurator::UpdateResourceID()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool updated = true;
    if (RcmConfigurator::getInstance()->getResourceId().empty()) {
        int retryCount = 0;
        do {
            try {
                DebugPrintf(SV_LOG_DEBUG, "Generating new resource id with retry count: %d\n", retryCount);
                RcmConfigurator::getInstance()->setResourceId(GenerateUuid());
                break;
            }
            catch (ContextualException &e) {
                updated = false;
                DebugPrintf(SV_LOG_ERROR, "Failed to generate/update resource id with exception: %s\n", e.what());
            }
            retryCount++;
        } while (!updated && retryCount < 3);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return updated;
}

uint32_t RcmConfigurator::GetRcmClientRequestTimeout() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return getRcmRequestTimeout();
}

bool RcmConfigurator::GetProxySettings(std::string &proxyAddress, std::string &proxyPort, std::string &bypassList) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool bIsProxySet = false;
    std::string proxySettingsPath = getProxySettingsPath();
    boost::system::error_code ec;
    if (!boost::filesystem::exists(proxySettingsPath, ec))
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Proxy settings file %s does not exist for RCM client. Error= %s.\n", FUNCTION_NAME, proxySettingsPath.c_str(), ec.message().c_str());
    }
    else
    {
        ProxySettings proxySettings(proxySettingsPath);

        if (proxySettings.m_Address.length() && proxySettings.m_Port.length())
        {
            // TODO - log bypass list
            DebugPrintf(SV_LOG_DEBUG, "%s: Using proxy settings %s %s %s for RCM client.\n",
                FUNCTION_NAME, proxySettings.m_Address.c_str(), proxySettings.m_Port.c_str(), proxySettings.m_Bypasslist.c_str());
            bIsProxySet = true;
            proxyAddress = proxySettings.m_Address;
            proxyPort = proxySettings.m_Port;
            bypassList = proxySettings.m_Bypasslist;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Proxy settings not set for RCM client.\n", FUNCTION_NAME);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bIsProxySet;
}

std::string RcmConfigurator::GetVacpCertFingerprint() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string fp;
    try {
        boost::mutex::scoped_lock gaurd(m_vacpCertMutex);

        X509* vacpcert = NULL;
        ON_BLOCK_EXIT(&X509_free, vacpcert);

        std::string certName = securitylib::getCertDir();
        certName += securitylib::SECURITY_DIR_SEPARATOR;
        certName += "vacp";
        certName += securitylib::EXTENSION_CRT;

        std::string keyName = securitylib::getPrivateDir();
        keyName += securitylib::SECURITY_DIR_SEPARATOR;
        keyName += "vacp";
        keyName += securitylib::EXTENSION_KEY;

        std::string dhName = securitylib::getPrivateDir();
        dhName += securitylib::SECURITY_DIR_SEPARATOR;
        dhName += "vacp";
        dhName += securitylib::EXTENSION_DH;

        bool isCertLive = false;

        if (boost::filesystem::exists(certName))
        {
            vacpcert = securitylib::GenCert::readCert(certName);
            isCertLive = securitylib::GenCert::isCertLive(vacpcert);
        }

        if (!isCertLive ||
            !boost::filesystem::exists(keyName) ||
            !boost::filesystem::exists(dhName))
        {
            if (vacpcert)
            {
                X509_free(vacpcert);
                vacpcert = NULL;
            }
            securitylib::CertData certData;
            certData.updateCertData();
            securitylib::GenCert genCert("vacp", certData);
            genCert.selfSignReq();
            genCert.generateDiffieHellman(certName);
            isCertLive = true;
        }

        if (!vacpcert && isCertLive)
        {
            vacpcert = securitylib::GenCert::readCert(certName);
        }
        ON_BLOCK_EXIT(&X509_free, vacpcert);

        fp = securitylib::GenCert::extractFingerprint(vacpcert);
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught exception %s", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught unknown exception.", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return fp;
}

SVSTATUS RcmConfigurator::GetCurrentPersistentDetails()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!m_devDetailsPersistentStore.GetCurrentDetails(m_devDetails))
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: failed to get current persistent map. Error: %s \n",
            FUNCTION_NAME,
            m_devDetailsPersistentStore.GetErrorMessage().c_str());
        return SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SVS_OK;
}

sigslot::signal1<RcmJobs_t&>& RcmConfigurator::GetJobsChangedSignal()
{
    return m_jobsChangedSignal;
}

sigslot::signal0<>& RcmConfigurator::GetProtectionDisabledSignal()
{
    return m_protectionDisabledSignal;
}

sigslot::signal0<>& RcmConfigurator::GetSyncSettingsChangedSignal()
{
    return m_syncSettingsChangedSignal;
}

sigslot::signal0<>& RcmConfigurator::GetDrainSettingsChangeddSignal()
{
    return m_drainSettingsChangedSignal;
}

sigslot::signal0<>& RcmConfigurator::GetTunablePropertiesChangeddSignal()
{
    return m_tunablePropertiesChangedSignal;
}

SVSTATUS RcmConfigurator::ClearCachedReplicationSettings()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string err;
    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);

    m_agentSettings.ClearCachedSettings();

    status = m_pAgentSettingsCacher->Persist(m_agentSettings);
    if (status != SVS_OK)
    {
        DebugPrintf(
            SV_LOG_ERROR,
            "%s: Failed to clean up persisted replication settings: %s\n",
            FUNCTION_NAME,
            m_pAgentSettingsCacher->Error().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

bool RcmConfigurator::IsCloudPairingComplete()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string status = getCloudPairingStatus();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return IsAzureToAzureReplication() ? true : boost::iequals(status, "complete");
}

SVSTATUS RcmConfigurator::GetReplicationPairMessageContext(std::map<std::string, std::string>& contexts) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
    contexts = m_agentSettings.m_replicationPairMsgContexts;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetReplicationPairMessageContext(const std::string& diskid, std::string& context) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);

    std::map<std::string, std::string>::const_iterator contextMapIter = m_agentSettings.m_replicationPairMsgContexts.find(diskid);
    if (contextMapIter != m_agentSettings.m_replicationPairMsgContexts.end())
    {
        context = contextMapIter->second;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetClusterVirtualNodeProtectionContext(std::string& context) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
    context = m_agentSettings.m_clusterVirtualNodeProtectionContext;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetClusterProtectionContext(std::string& context) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
    context = m_agentSettings.m_clusterProtectionMessageContext;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetProtectionPairContext(std::string& context) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);
    context = m_agentSettings.m_protectionPairContext;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::CompleteSyncAction(const std::string & syncStatus)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    status = m_pRcmClient->CompleteSync(syncStatus);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

bool RcmConfigurator::IsOnPremToAzureReplication() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bFlag = false;

    std::string sourceControlPlane = getSourceControlPlane();
    if (boost::iequals(getVmPlatform(), "VmWare"))
    {
        bFlag = (!sourceControlPlane.empty() && boost::iequals(sourceControlPlane, RcmClientLib::RCM_ONPREM_CONTROL_PLANE))
            || (sourceControlPlane.empty() && !IsAgentRunningOnAzureVm());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bFlag;
}

bool RcmConfigurator::IsAzureToOnPremReplication() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bFlag = false;

    std::string sourceControlPlane = getSourceControlPlane();
    if (boost::iequals(getVmPlatform(), "VmWare"))
    {
        bFlag = (!sourceControlPlane.empty() && boost::iequals(sourceControlPlane, RcmClientLib::RCM_AZURE_CONTROL_PLANE))
            || IsAgentRunningOnAzureVm();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bFlag;
}

std::string RcmConfigurator::GetTelemetryFolderPathForOnPremToAzure() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!IsOnPremToAzureReplication())
        throw INMAGE_EX("Invalid method invocation");

    std::string folderPath;
    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);

    try {
        ProtectedMachineTelemetrySettings telemetrySettings = JSON::consumer<ProtectedMachineTelemetrySettings>::convert(m_agentSettings.m_telemetrySettings);
        folderPath = telemetrySettings.TelemetryFolder;
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught exception %s\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught unknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return folderPath;
}

std::string RcmConfigurator::GetTelemetrySasUriForAzureToOnPrem() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!IsAzureToOnPremReplication())
        throw INMAGE_EX("Invalid method invocation");

    std::string sasUri;
    boost::unique_lock<boost::mutex> lock(m_agentSettingMutex);

    try {
        AzureToOnPremTelemetrySettings telemetrySettings = JSON::consumer<AzureToOnPremTelemetrySettings>::convert(m_agentSettings.m_telemetrySettings);
        sasUri = telemetrySettings.KustoEventHubUri;
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught exception %s\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught unknown exception\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return sasUri;
}

template<typename T1, typename T2>
SVSTATUS RcmConfigurator::ConnectWithProxyOptions (bool bDoNotUseProxy, T1 arg1, T2 arg2, boost::function<SVSTATUS(T1, T2)> function)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS ret = SVS_OK;

    std::string customProxy;
    bool bHaveCustomProxySettings = false;
    boost::shared_ptr<ProxySettings> ptrCustomProxySettings;
    std::string proxySettingsPath = getProxySettingsPath();
    if (boost::filesystem::exists(proxySettingsPath))
    {
        ptrCustomProxySettings.reset(new ProxySettings(proxySettingsPath));
        if (!ptrCustomProxySettings->m_Address.empty() &&
            !ptrCustomProxySettings->m_Port.empty())
        {
            bHaveCustomProxySettings = true;
            customProxy = ptrCustomProxySettings->m_Address + ":" + ptrCustomProxySettings->m_Port;
        }
    }

    if (bHaveCustomProxySettings)
        CheckAccessToAzureServiceUrls(ptrCustomProxySettings->m_Bypasslist, customProxy);
    else
        CheckAccessToAzureServiceUrls();

    ret = function(arg1, arg2);

    if (!bDoNotUseProxy &&
        (ret != SVS_OK) &&
        ((m_pRcmClient->GetErrorCode() == RCM_CLIENT_COULDNT_CONNECT) ||
            (m_pRcmClient->GetErrorCode() == RCM_CLIENT_AAD_COULDNT_CONNECT)))
    {
        bool bShouldRetry = false;
        std::string internetProxyAddress, internetProxyPort, internetProxyBypasslist;
        if (GetInternetProxySettings(internetProxyAddress, internetProxyPort, internetProxyBypasslist))
        {
            bShouldRetry = true;
            if (!bHaveCustomProxySettings)
            {
                DebugPrintf(SV_LOG_INFO,
                    "Failed to connect without proxy settings. Retryig with internet proxy settings.\n");
            }
            else if (!boost::iequals(internetProxyAddress, ptrCustomProxySettings->m_Address) ||
                !boost::iequals(internetProxyPort, ptrCustomProxySettings->m_Port))
            {
                DebugPrintf(SV_LOG_INFO,
                    "Failed to connect with custom proxy settings %s:%s. Retrying with internet proxy settings.\n",
                    ptrCustomProxySettings->m_Address.c_str(),
                    ptrCustomProxySettings->m_Port.c_str());
            }
            else
            {
                bShouldRetry = false;
                DebugPrintf(SV_LOG_INFO,
                    "Custom proxy settings are same as internet proxy settings.\n");
            }
        }

        if (bShouldRetry)
        {
            const char HTTP_PREFIX[] = "http://";
            if (!boost::starts_with(internetProxyAddress, HTTP_PREFIX))
                internetProxyAddress = HTTP_PREFIX + internetProxyAddress;

            // persist the proxy address with the http:// prefix as setup is using 
            // Uri.IsWellFormedUriString(proxyAddress, UriKind.Absolute) API to check
            // a valid URI
            PersistProxySetting(proxySettingsPath,
                internetProxyAddress,
                internetProxyPort,
                internetProxyBypasslist);

            internetProxyAddress += ":" + internetProxyPort;

            CheckAccessToAzureServiceUrls(internetProxyBypasslist, internetProxyAddress);

            ret = function(arg1, arg2);

            if ((ret != SVS_OK) && ((m_pRcmClient->GetErrorCode() == RCM_CLIENT_COULDNT_CONNECT) ||
                (m_pRcmClient->GetErrorCode() == RCM_CLIENT_AAD_COULDNT_CONNECT)))
            {
                DebugPrintf(SV_LOG_INFO,
                    "Failed to connect with internet proxy settings.\n");

                // restore custom proxy settings
                if (bHaveCustomProxySettings)
                {
                    DebugPrintf(SV_LOG_INFO, "Restoring custom proxy settings.\n");

                    PersistProxySetting(proxySettingsPath,
                        ptrCustomProxySettings->m_Address,
                        ptrCustomProxySettings->m_Port,
                        ptrCustomProxySettings->m_Bypasslist);
                }
                else
                {
                    DebugPrintf(SV_LOG_INFO, "Deleting proxy settings.\n");
                    DeleteProxySetting(proxySettingsPath);
                }
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return ret;

}

SVSTATUS RcmConfigurator::RegisterMachine(QuitFunction_t qf, bool bDoNotUseProxy, bool update)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS ret = SVS_OK;

    boost::function<SVSTATUS(QuitFunction_t, bool)> registerMachine =
        boost::bind(&RcmClient::RegisterMachine, m_pRcmClient, _1, _2);

    ret = ConnectWithProxyOptions(bDoNotUseProxy, qf, update, registerMachine);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return ret;
}

SVSTATUS RcmConfigurator::CsLogin(const std::string& ipAddr, const std::string& port, bool bDoNotUseProxy)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    uint32_t uiPort = boost::lexical_cast<uint32_t>(port);

    boost::function<SVSTATUS(std::string, uint32_t)> cslogin =
        boost::bind(&RcmClient::CsLogin, m_pRcmClient, _1, _2);

    status = ConnectWithProxyOptions(bDoNotUseProxy, ipAddr, uiPort, cslogin);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

std::string RcmConfigurator::GetSourceAgentConfigPath() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string sourceAgentConfigPath;

    if (GetConfigDir(sourceAgentConfigPath))
    {
        sourceAgentConfigPath += SourceConfigFile;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return sourceAgentConfigPath;
}

std::string RcmConfigurator::GetALRConfigPath()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string alrConfigPath;

    if (GetConfigDir(alrConfigPath))
    {
        alrConfigPath += ALRConfigFile;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return alrConfigPath;
}

SVSTATUS RcmConfigurator::ParseSourceConfig(const std::string& configFile,
    AgentConfiguration &agentConfig, std::string &serAgentConfig, RCM_CLIENT_STATUS rcmStatus,
    bool validateConfig)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    try {
        AgentConfigHelpers::GetSourceAgentConfig(configFile, agentConfig);
        serAgentConfig = JSON::producer<AgentConfiguration>::convert(agentConfig);
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to parse source config : %s, exception : %s\n",
            FUNCTION_NAME,
            configFile.c_str(),
            e.what());
        m_pRcmClient->m_errorCode = rcmStatus;
        status = SVE_FAIL;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to parse source config: %s with unknown exception.\n",
            FUNCTION_NAME,
            configFile.c_str());
        m_pRcmClient->m_errorCode = rcmStatus;
        status = SVE_FAIL;
    }

    if (validateConfig && !AgentConfigHelpers::IsConfigValid(agentConfig))
    {
        DebugPrintf(SV_LOG_ERROR, "Invalid config. File path : %s, serialized content : %s\n",
            configFile.c_str(), serAgentConfig.c_str());
        m_pRcmClient->m_errorCode = RCM_CLIENT_INVALID_SOURCE_CONFIG_BIOS_ID_OR_FQDN;
        status = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetPersistedConfig(AgentConfiguration &persistedConfig,
    std::string &serPersistedConfig, bool validateConfig)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string persistedConfigFile;

    persistedConfigFile = GetSourceAgentConfigPath();
    if (boost::filesystem::exists(persistedConfigFile))
    {
        status = ParseSourceConfig(persistedConfigFile, persistedConfig,
            serPersistedConfig, RCM_CLIENT_INVALID_PERSISTED_SOURCE_CONFIG,
            validateConfig);
    }
    else
    {
        DebugPrintf(SV_LOG_ALWAYS, "There is no persisted source config file.\n");
        status = SVS_FALSE;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::VerifyVault(const AgentConfiguration &agentConfig,
    const AgentConfiguration &persistedConfig)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    if (!agentConfig.IsSameVault(persistedConfig))
    {
        m_pRcmClient->m_errorCode = RCM_CLIENT_VAULT_MISMATCH;
        status = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::VerifyClientAuthCommon(const AgentConfiguration &agentConfig,
    const std::string &serConfig)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    do {
        status = securitylib::CertHelpers::DecodeAndDumpCert(securitylib::CLIENT_CERT_NAME,
            agentConfig.ClientCertificate, true);
        if (status != SVS_OK)
            break;

        boost::function<SVSTATUS(RcmProxyTransportSetting, bool)> verifyClientAuth =
            boost::bind(&RcmClient::VerifyClientAuth, m_pRcmClient, _1, _2);

        status = ConnectWithProxyOptions(false, agentConfig.RcmProxyTransportSettings, false,
            verifyClientAuth);
        if (status != SVS_OK)
            break;

        std::string sourceConfigFilePath = GetSourceAgentConfigPath();
        securitylib::CertHelpers::BackupFile(sourceConfigFilePath);
        securitylib::CertHelpers::SaveStringToFile(sourceConfigFilePath,
            serConfig, boost::filesystem::owner_all);

    } while(false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::VerifyClientAuth(const std::string& configFile)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    AgentConfiguration agentConfig, persistedConfig;
    std::string serAgentConfig, serPersistedConfig;

    do {
        status = ParseSourceConfig(configFile, agentConfig, serAgentConfig,
            RCM_CLIENT_INVALID_ARGUMENT);
        if (status != SVS_OK)
            break;
        status = GetPersistedConfig(persistedConfig, serPersistedConfig);
        if (status != SVS_OK)
        {
            if (status != SVS_FALSE)
                break;
            status = SVS_OK;
        }
        else
        {
            status = VerifyVault(agentConfig, persistedConfig);
            if (status != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR,
                    "Mismatch in vault details. Passed Config : %s, Persisted Config : %s\n",
                    serAgentConfig.c_str(), serPersistedConfig.c_str());
                break;
            }
        }

        status = VerifyClientAuthCommon(agentConfig, serAgentConfig);

    } while(false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::VerifyClientAuth()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string persistedConfigFile;

    persistedConfigFile = GetSourceAgentConfigPath();
    if (boost::filesystem::exists(persistedConfigFile))
    {
        status = VerifyClientAuth(persistedConfigFile);
    }
    else
    {
        DebugPrintf(SV_LOG_ALWAYS, "There is no persisted source config file.\n");
        status = SVE_FILE_NOT_FOUND;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetNonCachedSettings()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    std::map<std::string, std::string> headers;
    headers[X_MS_AGENT_DO_NOT_USE_CACHED_SETTINGS] = "true";

    std::string serSettings;
    status = m_pRcmClient->GetReplicationSettings(serSettings, headers);

    // If the call fails with any other error then appliance mismatch, complete with warnings
    if (status != SVS_OK && m_pRcmClient->m_errorCode != RCM_CLIENT_APPLIANCE_MISMATCH)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s : Original error code : %d\n",
            FUNCTION_NAME, m_pRcmClient->m_errorCode);
        m_pRcmClient->m_errorCode = RCM_CLIENT_GET_NON_CACHED_SETTINGS_WARNING;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::DiagnoseAndFix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    do
    {
        if (getMigrationState() == Migration::MIGRATION_ROLLBACK_PENDING ||
            getMigrationState() == Migration::MIGRATION_ROLLBACK_FAILED)
        {
            setMigrationState(Migration::MIGRATION_ROLLBACK_PENDING);
            setCSType("CSLegacy");
            DebugPrintf(SV_LOG_ALWAYS,
                "%s: Set migration state to rollback pending and cstype to cslegacy.\n",
                FUNCTION_NAME);
        }

        if (getCSType() == "CSPrime")
        {
            status = CompleteAgentRegistration(0, false, true);
            if (status != SVS_OK &&
                (getMigrationState() == Migration::MIGRATION_SWITCHED_TO_RCM_PENDING ||
                getMigrationState() == Migration::MIGRATION_SWITCHED_TO_RCM) &&
                m_pRcmClient->GetErrorCode() == RCM_CLIENT_PROTECTION_DISABLED)
            {
                DebugPrintf(SV_LOG_ALWAYS,
                    "%s: Agent registration failed, rolling back to legacy stack.\n",
                    FUNCTION_NAME);
                setMigrationState(Migration::MIGRATION_ROLLBACK_PENDING);
                setCSType("CSLegacy");
                DebugPrintf(SV_LOG_ALWAYS,
                    "%s: Set migration state to rollback pending and cstype to cslegacy.\n",
                    FUNCTION_NAME);

                status = SVS_OK;
            }
        }
    } while (false);

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Diagnose and fix failed, status = %d.\n",
            FUNCTION_NAME, status);

        m_pRcmClient->m_errorCode = RCM_CLIENT_DIAGNOSE_AND_FIX_FAILED;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::CompleteAgentRegistration(QuitFunction_t qf,
    bool bDoNotUseProxy, bool update)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    do {
        status = RegisterMachine(qf, bDoNotUseProxy, update);
        if (status == SVS_OK)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: Register Machine succeeded.\n", FUNCTION_NAME);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Register Machine failed, status = %d.\n",
                FUNCTION_NAME, status);
            break;
        }

        status = m_pRcmClient->RegisterSourceAgent();
        if (status == SVS_OK)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: Register Source Agent succeeded.\n", FUNCTION_NAME);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Register Source Agent failed, status = %d.\n",
                FUNCTION_NAME, status);
            break;
        }
    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::SwitchAppliance(const RcmProxyTransportSetting &rcmProxyTransportSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    AgentConfiguration persistedConfig;
    std::string serPersistedConfig;

    do {
        status = GetPersistedConfig(persistedConfig, serPersistedConfig);
        if (status != SVS_OK)
            break;

        if (persistedConfig.RcmProxyTransportSettings == rcmProxyTransportSettings)
        {
            DebugPrintf(SV_LOG_ALWAYS,
                "Switch appliance is called with same RCMProxy Transport settings.\n");
            setSwitchApplianceState(SwitchAppliance::SWITCH_APPLIANCE_SUCCEEDED);
            break;
        }

        persistedConfig.RcmProxyTransportSettings = rcmProxyTransportSettings;
        serPersistedConfig = JSON::producer<AgentConfiguration>::convert(persistedConfig);

        status = VerifyClientAuthCommon(persistedConfig, serPersistedConfig);
        DebugPrintf(SV_LOG_ALWAYS, "%s : Verify Client Auth status : %d\n", FUNCTION_NAME, status);
        if (status != SVS_OK)
        {
            setSwitchApplianceState(SwitchAppliance::VERIFY_CLIENT_FAILED);
            break;
        }
        setSwitchApplianceState(SwitchAppliance::VERIFY_CLIENT_SUCCEEDED);

        status = m_pRcmClient->RegisterSourceAgent();
        DebugPrintf(SV_LOG_ALWAYS, "%s : Register Source Agent status : %d\n",
            FUNCTION_NAME, status);
        if (status != SVS_OK)
        {
            setSwitchApplianceState(SwitchAppliance::REGISTER_SOURCE_AGENT_FAILED);
            break;
        }
        setSwitchApplianceState(SwitchAppliance::REGISTER_SOURCE_AGENT_SUCCEEDED);

        status = GetNonCachedSettings();
        DebugPrintf(SV_LOG_ALWAYS, "%s : Get Non-cached settings status : %d\n",
            FUNCTION_NAME, status);
        if (status != SVS_OK)
        {
            setSwitchApplianceState(SwitchAppliance::GET_NON_CACHED_SETTINGS_FAILED);
            break;
        }
        setSwitchApplianceState(SwitchAppliance::SWITCH_APPLIANCE_SUCCEEDED);
        DebugPrintf(SV_LOG_ALWAYS, "%s : Switch Appliance Succeeded.\n", FUNCTION_NAME);
    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::CheckAccessToAzureServiceUrls(
    const std::string bypasslist,
    std::string proxy)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;

    if (!IsAgentRunningOnAzureVm())
    {
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    std::vector<std::string> vServiceUris;
    vServiceUris.push_back(GetAzureADServiceUri());
    vServiceUris.push_back(GetRcmServiceUri());

    for (std::vector<std::string>::iterator itr = vServiceUris.begin();
        itr != vServiceUris.end();
        ++itr)
    {
        SV_ULONG ret;
        std::string proxyarg = IsAddressInBypasslist(*itr, bypasslist) ? std::string() : proxy;
        ret = RunUrlAccessCommand(*itr, proxyarg);

        if (!ret)
            DebugPrintf(SV_LOG_ALWAYS, "%s : URI access is successful.\n", FUNCTION_NAME);
        else
            status = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

SVSTATUS RcmConfigurator::GetRcmProxyAddress(const RcmProxyTransportSetting& proxySetting,
    std::vector<std::string>& rcmProxyAddr, uint32_t& port) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    port = proxySetting.Port;
    if (!proxySetting.NatIpAddress.empty())
        rcmProxyAddr.push_back(proxySetting.NatIpAddress);
    else
    {
        if (!proxySetting.FriendlyName.empty())
            rcmProxyAddr.push_back(proxySetting.FriendlyName);

        for (std::vector<std::string>::const_iterator iter = proxySetting.IpAddresses.begin();
            iter != proxySetting.IpAddresses.end();
            iter++)
        {
            if (!iter->empty())
            {
                rcmProxyAddr.push_back(*iter);
            }
        }
    }

    if (rcmProxyAddr.empty() || !port)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get Rcm Proxy address\n");
        status = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetRcmProxyAddress(std::vector<std::string>& rcmProxyAddr,
    uint32_t& port) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    std::string sourceConfigFilePath = GetSourceAgentConfigPath();
    AgentConfiguration agentConfig;

    try {

        AgentConfigHelpers::GetSourceAgentConfig(sourceConfigFilePath, agentConfig);
        RcmProxyTransportSetting &proxySetting = agentConfig.RcmProxyTransportSettings;

        status = GetRcmProxyAddress(proxySetting, rcmProxyAddr, port);
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to get the agent config with error %s\n",
            FUNCTION_NAME,
            e.what());
        status = SVE_FAIL;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: failed to get the agent config from file %s with unknown exception.\n",
            FUNCTION_NAME,
            sourceConfigFilePath.c_str());
        status = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::UpdateClientCert()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string serConfigInput;

    status = m_pRcmClient->RenewClientCertDetails(serConfigInput);
    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "RCM API call : Renew Client Cert Details failed.\n");
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    AgentConfiguration agentConfig =
        JSON::consumer<AgentConfiguration>::convert(serConfigInput, true);
    try {
        if (!AgentConfigHelpers::IsConfigValid(agentConfig))
        {
            DebugPrintf(SV_LOG_ERROR, "Invalid config. serialized content : %s\n",
                serConfigInput.c_str());
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return SVE_FAIL;
        }

        status = securitylib::CertHelpers::DecodeAndDumpCert(securitylib::CLIENT_CERT_NAME,
            agentConfig.ClientCertificate, true);
        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "Decode and dump cert failed\n");
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return status;
        }

        std::string sourceConfigPath = GetSourceAgentConfigPath();
        securitylib::CertHelpers::BackupFile(sourceConfigPath);
        AgentConfigHelpers::PersistSourceAgentConfig(sourceConfigPath, agentConfig);
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Renew cert failed with error %s\n",
            FUNCTION_NAME,
            e.what());
        status = SVE_FAIL;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Renew cert failed with unknown exception.\n",
            FUNCTION_NAME);
        status = SVE_FAIL;
    }


    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::ApplianceLogin(const RcmRegistratonMachineInfo& machineInfo, std::string& errMsg)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    try {
        do
        {
            boost::function<SVSTATUS(RcmRegistratonMachineInfo, std::string&)> appliancelogin =
                boost::bind(&RcmClient::ApplianceLogin, m_pRcmClient, _1, _2);
            std::string response;
            status = ConnectWithProxyOptions<RcmRegistratonMachineInfo, std::string&>(false, machineInfo, response, appliancelogin);
            if (status != SVS_OK)
            {
                errMsg += "Fetch of agentconfigurationmigration from rcmproxy failed.";
                break;
            }
            AgentConfigurationMigration agentMigConfig = JSON::consumer<AgentConfigurationMigration>::convert(response, true);
            if (!AgentConfigHelpers::IsConfigValid(agentMigConfig.MobilityAgentConfig))
            {
                errMsg += "Invalid config recieved from RCMProxyAgent. serialized content." + response;
                status = SVE_FAIL;
                break;
            }
            status = securitylib::CertHelpers::DecodeAndDumpCert(securitylib::CLIENT_CERT_NAME,
                agentMigConfig.MobilityAgentConfig.ClientCertificate, true);
            if (status != SVS_OK)
            {
                errMsg += "Decode and dump cert failed\n";
                break;
            }
            std::string sourceConfigPath = GetSourceAgentConfigPath();
            securitylib::CertHelpers::BackupFile(sourceConfigPath);
            AgentConfigHelpers::PersistSourceAgentConfig(sourceConfigPath, agentMigConfig.MobilityAgentConfig);
        } while (false);
    }
    catch (const JSON::json_exception& jsone)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to unmarshall AgentConfigurationMigration with exception %s.\n", FUNCTION_NAME, jsone.what());
        status = SVE_FAIL;
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s Exception on appliance login = %s.\n", FUNCTION_NAME, e.what());
        status = SVE_FAIL;
    }

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "%s Appliance Login and AgentConfiguration fetch failed. Error Message= %s.\n", FUNCTION_NAME, errMsg.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

std::string RcmConfigurator::GetFqdn()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string fqdn;

#ifdef SV_WINDOWS
    fqdn = Host::GetInstance().GetFQDN();
#else
    fqdn = Host::GetInstance().GetHostName();
#endif

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return fqdn;
}

SVSTATUS RcmConfigurator::FetchAndUpdateSourceConfig(const ALRMachineConfig& alrMachineConfig)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;
    try {
        do
        {
            boost::function<SVSTATUS(ALRMachineConfig, std::string&)> appliancelogin =
                boost::bind(&RcmClient::ALRApplianceLogin, m_pRcmClient, _1, _2);
            std::string response;
            status = ConnectWithProxyOptions<ALRMachineConfig, std::string&>(
                false, alrMachineConfig, response, appliancelogin);
            if (status != SVS_OK)
            {
                errMsg += "Fetch of agent configuration for ALR from rcmproxy failed.";
                break;
            }
            CreateMobilityAgentConfigForOnPremAlrMachineOutput alrConfigOutput =
                JSON::consumer<CreateMobilityAgentConfigForOnPremAlrMachineOutput>::convert(
                response, true);
            if (!AgentConfigHelpers::IsConfigValid(alrConfigOutput.MobilityAgentConfig))
            {
                errMsg = "Invalid config recieved from RCMProxyAgent. serialized content." +
                    alrConfigOutput.str();
                status = SVE_FAIL;
                break;
            }
            status = securitylib::CertHelpers::DecodeAndDumpCert(securitylib::CLIENT_CERT_NAME,
                alrConfigOutput.MobilityAgentConfig.ClientCertificate, true);
            if (status != SVS_OK)
            {
                errMsg = "Decode and dump cert failed\n";
                break;
            }

            std::string sourceConfigPath = GetSourceAgentConfigPath();
            securitylib::CertHelpers::BackupFile(sourceConfigPath);
            AgentConfigHelpers::PersistSourceAgentConfig(sourceConfigPath,
                alrConfigOutput.MobilityAgentConfig);
        } while (false);
    }
    catch (const JSON::json_exception& jsone)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s failed to unmarshall CreateMobilityAgentConfigForOnPremAlrMachineOutput"
            " with exception %s.\n",
            FUNCTION_NAME, jsone.what());
        status = SVE_FAIL;
    }

    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s Exception on appliance login = %s.\n",
            FUNCTION_NAME, e.what());
        status = SVE_FAIL;
    }

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s Appliance Login and AgentConfiguration fetch failed. Error Message= %s.\n",
            FUNCTION_NAME, errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::ALRRecoverySteps()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    AgentConfiguration persistedConfig;
    std::string serPersistedConfig;
    boost::system::error_code ec;
    SVSTATUS status = SVS_OK;
    std::stringstream strErrMsg;
    do
    {
        status = GetPersistedConfig(persistedConfig, serPersistedConfig, false);
        if (status == SVS_FALSE)
        {
            status = SVS_OK;
            break;
        }
        else if (status != SVS_OK)
        {
            break;
        }

        if (AgentConfigHelpers::IsConfigValid(persistedConfig))
        {
            status = SVS_OK;
            break;
        }
        DebugPrintf(SV_LOG_ALWAYS, "%s: Checking for ALR scenario.\n", FUNCTION_NAME);

        // Check for ALR scenario
        std::string alrConfigPath = GetALRConfigPath();
        if (!boost::filesystem::exists(alrConfigPath, ec))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: ALR config file : %s doesn't exist.\n",
                FUNCTION_NAME, alrConfigPath.c_str());
            status = SVE_ABORT;
            break;
        }

        ALRMachineConfig alrMachineConfig;
        status = AgentConfigHelpers::GetALRMachineConfig(alrConfigPath,
            alrMachineConfig, strErrMsg);
        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, strErrMsg.str().c_str());
            break;
        }

        std::string biosId = GetSystemUUID();
        if (!BiosID::MatchBiosID(biosId, alrMachineConfig.OnPremMachineBiosId))
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: System BiosId : %s and config BiosId : %s do not match.\n",
                FUNCTION_NAME, biosId.c_str(), alrMachineConfig.OnPremMachineBiosId.c_str());
            status = SVE_FAIL;
            break;
        }

        DebugPrintf(SV_LOG_ALWAYS, "%s: Fetching new config from appliance\n", FUNCTION_NAME);
        status = FetchAndUpdateSourceConfig(alrMachineConfig);
        if (status == SVS_OK)
        {
            status = securitylib::CertHelpers::BackupFile(alrConfigPath);
            if (status == SVS_OK)
            {
                if (!boost::filesystem::remove(alrConfigPath, ec))
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "%s: Failed to remove alr config file with error : %s.\n",
                        FUNCTION_NAME, alrConfigPath.c_str(), ec.message().c_str());
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to backup alr config file : %s.\n",
                    FUNCTION_NAME, alrConfigPath.c_str());
            }
        }
    } while(false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetUrlsForAccessCheck(std::set<std::string>& urls)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    try
    {
        // get the URLs from settings
        std::set<std::string> urlsInSettings;
        AgentMonitoringMsgSettings monitoringSettings;
        status = GetMonitoringMessageSettings(monitoringSettings);
        if (SVS_OK == status)
        {
            if (!monitoringSettings.CriticalEventHubUri.empty())
                urlsInSettings.insert(monitoringSettings.CriticalEventHubUri);

            if (!monitoringSettings.InformationEventHubUri.empty())
                urlsInSettings.insert(monitoringSettings.InformationEventHubUri);

            if (!monitoringSettings.LoggingEventHubUri.empty())
                urlsInSettings.insert(monitoringSettings.LoggingEventHubUri);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s : GetMonitoringMessageSettings failed with status %d.\n", FUNCTION_NAME, status);
        }

        if (IsAzureToOnPremReplication())
        {
            std::string telemetryUri = GetTelemetrySasUriForAzureToOnPrem();
            if (!telemetryUri.empty())
                urlsInSettings.insert(telemetryUri);
        }

        AgentSettings settings;
        status = GetAgentSettings(settings);
        if (SVS_OK == status)
        {
            std::vector<DrainSettings>::const_iterator dsit = settings.m_drainSettings.begin();
            for (/*empty*/;
                dsit != settings.m_drainSettings.end() && !m_threadManager.testcancel(ACE_Thread::self());
                dsit++)
            {
                const DrainSettings& drainSettings = *dsit;

                if (TRANSPORT_TYPE_AZURE_BLOB != drainSettings.DataPathTransportType)
                {
                    DebugPrintf(SV_LOG_DEBUG, "%s: Transport type %s for disk %s.\n",
                        FUNCTION_NAME,
                        drainSettings.DataPathTransportType.c_str(),
                        drainSettings.DiskId.c_str());
                    continue;
                }

                if (drainSettings.DataPathTransportSettings.empty())
                {
                    DebugPrintf(SV_LOG_DEBUG, "Transport settings are empty for disk %s.\n", drainSettings.DiskId.c_str());
                    continue;
                }

                std::string input;
                if (IsAzureToAzureReplication())
                {
                    input = securitylib::base64Decode(
                        drainSettings.DataPathTransportSettings.c_str(),
                        drainSettings.DataPathTransportSettings.length());
                }
                else
                {
                    input = drainSettings.DataPathTransportSettings;
                }

                AzureBlobBasedTransportSettings blobSettings =
                    JSON::consumer<AzureBlobBasedTransportSettings>::convert(input, true);

                urlsInSettings.insert(blobSettings.BlobContainerSasUri);
            }

            for (SyncSettings_t::const_iterator iterSyncSettings = settings.m_syncSettings.begin();
                !(m_threadManager.testcancel(ACE_Thread::self())) && (iterSyncSettings != settings.m_syncSettings.end());
                iterSyncSettings++)
            {
                AzureBlobBasedTransportSettings blobSettings = iterSyncSettings->TransportSettings;
                urlsInSettings.insert(blobSettings.BlobContainerSasUri);
            }

            const char* eventHubUriPat = "^Endpoint=sb://(.+)/(.+)$";
            const char* sasUriPat = "^https://(.+)/(.+)$";
            const boost::regex eventHubRegex(eventHubUriPat);
            const boost::regex storageUriRegex(sasUriPat);

            std::set<std::string>::iterator it = urlsInSettings.begin();
            for (/**/; it != urlsInSettings.end(); it++)
            {
                boost::smatch matches;
                if (boost::regex_search(*it, matches, eventHubRegex))
                {
                    if (matches[1].matched)
                        urls.insert(matches[1].str());
                }
                else if (boost::regex_search(*it, matches, storageUriRegex))
                {
                    if (matches[1].matched)
                        urls.insert(matches[1].str());
                }
                else
                {
                    const std::string sasSignature("&sig=");
                    if (boost::icontains(*it, sasSignature))
                    {
                        std::size_t sigStartPos = it->find(sasSignature);
                        DebugPrintf(SV_LOG_ERROR, "%s : unmatched URI  %s [redacted].\n", FUNCTION_NAME, it->substr(0, sigStartPos).c_str());
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s : unmatched URI  %s.\n", FUNCTION_NAME, it->c_str());
                    }
                }
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s : GetAgentSettings failed with status %d.\n", FUNCTION_NAME, status);
        }
    }
    catch (const std::exception& e)
    {
        status = SVE_ABORT;
        DebugPrintf(SV_LOG_ERROR, "%s : failed with error %s.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        status = SVE_ABORT;
        DebugPrintf(SV_LOG_ERROR, "%s : failed with unknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmConfigurator::GetContainerType(const std::string& diskid, std::string& containerType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVE_FAIL;

    RcmClientLib::SyncSettings syncSettings;
    std::string dataPathTransportType;
    std::string strDataPathTransportSettings;
    status = GetSyncSettings(diskid, syncSettings, dataPathTransportType, strDataPathTransportSettings);

    if (SVS_OK == status)
    {
        containerType = RenewRequestTypes::ResyncContainer;
    }
    else
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: Disk %s is not in resync mode\n", FUNCTION_NAME, diskid.c_str());

        DrainSettings_t drainSettings;
        status = GetDrainSettings(drainSettings, dataPathTransportType, strDataPathTransportSettings);
        if (SVS_OK == status)
        {
            DrainSettings_t::const_iterator iter = drainSettings.begin();
            for (/*empty*/; iter != drainSettings.end(); iter++)
            {
                if (boost::iequals(diskid, iter->DiskId))
                {
                    containerType = RenewRequestTypes::DiffSyncContainer;
                    break;
                }
            }
            if (iter == drainSettings.end() &&
                containerType.empty())
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: Disk %s is not in diffsync mode\n", FUNCTION_NAME, diskid.c_str());
            }
        }
    }

    if (SVS_OK != status)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Settings not found for disk %s\n", FUNCTION_NAME, diskid.c_str());
    }

    if (containerType.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Could not fetch the container type for disk %s\n", FUNCTION_NAME, diskid.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

