#include <boost/make_shared.hpp>

#include "cdputil.h"
#include "inmageex.h"
#include "securityutils.h"
#include "configwrapper.h"
#include "rpcconfigurator.h"
#include "resyncRcmSettings.h"
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#define SafeCloseAceHandle(handle, filepath) \
    if (handle != ACE_INVALID_HANDLE) \
    { \
        if (ACE_OS::close(handle) == -1) \
        { \
            DebugPrintf(SV_LOG_ERROR, \
                "%s failed to close handle %#p for %s. Error: %d.\n", \
                FUNCTION_NAME, handle, filepath, ACE_OS::last_error()); \
        } \
        else \
        { \
            handle = ACE_INVALID_HANDLE; \
        } \
    }

extern Configurator* TheConfigurator;

boost::shared_ptr<ResyncRcmSettings> ResyncRcmSettings::m_ResyncRcmSettings;

boost::atomic<bool> ResyncRcmSettings::m_IsInitialized;

boost::mutex ResyncRcmSettings::m_Lock;

bool ResyncRcmSettings::Initialize()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    // Design:
    //  - Get sync settings from rcm configurator for protected device
    //      - Raise process exit signal if settings not present

    boost::property_tree::ptree pt;
    std::stringstream jsonstream;
    try
    {
        if (m_IsInitialized.load(boost::memory_order_consume))
        {
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return true;
        }

        if (SVS_OK != m_RcmConfiguratorPtr->GetSyncSettings(m_Args.GetDiskId(),
            m_SyncSettings, m_DataPathTransportType, m_strDataPathTransportSettings))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to get SyncSettings for disk %s\n",
                FUNCTION_NAME, m_Args.GetDiskId().c_str());

            // IR/resync completed or protection disabled - raise process quit signal
            CDPUtil::SignalQuit();
            
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return false;
        }

        if (!boost::iequals(m_DataPathTransportType, RcmClientLib::TRANSPORT_TYPE_PROCESS_SERVER) &&
            !boost::iequals(m_DataPathTransportType, RcmClientLib::TRANSPORT_TYPE_LOCAL_FILE) &&
            !boost::iequals(m_DataPathTransportType, RcmClientLib::TRANSPORT_TYPE_AZURE_BLOB))
        {
            // Unexpected transport data path type received - throw
            throw INMAGE_EX("Unexpected data path transport type received " + m_DataPathTransportType);
        }

        bool isNoDataTrSyncType = boost::iequals(m_ResyncRcmSettings->GetSourceAgentSyncSettings().SyncType,    
            RcmClientLib::SYNC_TYPE_NO_DATA_TRANSFER);
        
        if (isNoDataTrSyncType)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s DP started for diskId=%s in SYNC_TYPE_NO_DATA_TRANSFER syncType.\n", FUNCTION_NAME, m_Args.GetDiskId().c_str());
        }

        // Attach settings change signal in RcmConfigurator with RcmSettingsCallback
        m_RcmConfiguratorPtr->GetSyncSettingsChangedSignal().connect(this, &ResyncRcmSettings::RcmSettingsCallback);

        m_IsInitialized.store(true, boost::memory_order_release);
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with exception %s.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return m_IsInitialized.load(boost::memory_order_consume);
}

//
// Handles settings change signal from rcm configurator.
// This will raise process exit signal if settings are changed while IR/resync is in progress.
//
// Common settings change can be,
//      - Change in data path mode
//      - Data path transport settings change - PS failover - IP/port change
//      - Change in resync session ID - case of disable and enable protection while VM in shutdown state
//      - Change in PS LogFolder - resync cache path
//
void ResyncRcmSettings::RcmSettingsCallback()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if(!m_IsInitialized.load(boost::memory_order_consume))
    {
        DebugPrintf(SV_LOG_INFO, "%s: ResyncRcmSettings not initialized yet.\n", FUNCTION_NAME);
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return;
    }

    bool IsSettingsChanged = false;

    try
    {
        RcmClientLib::SyncSettings syncSettings;
        std::string dataPathTransportType;
        std::string strDataPathTransportSettings;

        if (SVS_OK != m_RcmConfiguratorPtr->GetSyncSettings(m_Args.GetDiskId(), syncSettings, dataPathTransportType, strDataPathTransportSettings))
        {
            // Protection disabled - raise process quit signal.
            DebugPrintf(SV_LOG_ALWAYS, "%s IR/resync completed or protection disabled for %s - signalling process quit.\n", FUNCTION_NAME, m_Args.GetDiskId().c_str());
            
            CDPUtil::SignalQuit();
            
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }

        // Verify newly fetched settings matches with existing settings
        if (!(m_SyncSettings == syncSettings))
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: SourceAgentSyncSettings changed.\n", FUNCTION_NAME);
            
            IsSettingsChanged = true;
        }
        else if(!boost::iequals(m_DataPathTransportType, dataPathTransportType))
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: DataTransportType changed.\n", FUNCTION_NAME);
            
            IsSettingsChanged = true;
        }
        else if(!boost::iequals(m_strDataPathTransportSettings, strDataPathTransportSettings) &&
            boost::iequals(dataPathTransportType, RcmClientLib::TRANSPORT_TYPE_PROCESS_SERVER))
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: DataPathTransportSettings changed.\n", FUNCTION_NAME);

            IsSettingsChanged = true;
        }
        else if (boost::iequals(dataPathTransportType, RcmClientLib::TRANSPORT_TYPE_LOCAL_FILE) ||
            boost::iequals(m_DataPathTransportType, RcmClientLib::TRANSPORT_TYPE_AZURE_BLOB))
        {
            // For <see cref="RcmEnum.DataPathTransport.AzureBlobStorageBasedTransport"/> and
            // <see cref="RcmEnum.DataPathTransport.LocalFileTransport"/>, dataPathTransportSettings will be empty string.
            // So processServerDetails remain uninitialized for RcmEnum.DataPathTransport.LocalFileTransport.
        }
        else
        {
            // Unexpected transport data path type received - throw
            throw INMAGE_EX("Unexpected data path transport type received " + dataPathTransportType);
        }
    }
    catch (const std::exception &e)
    {
        // Ignore and continue with current replication settings.
        DebugPrintf(SV_LOG_ERROR, "%s failed with exception %s. Continuing with current settings.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        // Ignore and continue with current replication settings.
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unknown exception. Continuing with current settings.\n", FUNCTION_NAME);
    }

    if (IsSettingsChanged)
    {
        // Log settings changed message then raise process quit signal
        DebugPrintf(SV_LOG_ALWAYS, "%s: SyncSettings changed for DiskId %s - quitting current resync session.\n", FUNCTION_NAME, m_SyncSettings.DiskId.c_str());
        CDPUtil::SignalQuit();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void ResyncRcmSettings::RcmTunablePropertiesChangeCallback()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    DebugPrintf(SV_LOG_ALWAYS, "%s: TunableProperties changed - quitting current session to consume new TunableProperties.\n", FUNCTION_NAME);
    CDPUtil::SignalQuit();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

bool ResyncRcmSettings::CreateInstance(const DpRcmProgramArgs &args)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!m_IsInitialized.load(boost::memory_order_consume))
    {
        boost::mutex::scoped_lock guard(m_Lock);
        if (!m_IsInitialized.load(boost::memory_order_consume))
        {
            m_ResyncRcmSettings = boost::shared_ptr<ResyncRcmSettings>(new ResyncRcmSettings(args));
            m_ResyncRcmSettings->Initialize();
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return m_IsInitialized.load(boost::memory_order_consume);
}

bool ResyncRcmSettings::GetInstance(boost::shared_ptr<ResyncRcmSettings> &resyncRcmSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool IsInitialized = m_IsInitialized.load(boost::memory_order_consume);
    if (IsInitialized)
    {
        resyncRcmSettings = m_ResyncRcmSettings;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return IsInitialized;
}

void ResyncRcmSettings::PersistConfigSettings(const InitialSettings & initialSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ACE_HANDLE hOut = ACE_INVALID_HANDLE;
    std::string temp_cachepath;

    try
    {
        const std::string &cachePath(m_settingsCachePathOnCsPrimeAppliance);

        std::string lockPath = cachePath + ".lck";
        temp_cachepath = cachePath + "_tmp";

        ACE_File_Lock lock(getLongPathName(lockPath.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS, 0);
        lock.acquire();

        hOut = ACE_OS::open(temp_cachepath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, ACE_DEFAULT_OPEN_PERMS);
        if (hOut == ACE_INVALID_HANDLE)
        {
            int err = ACE_OS::last_error();
            std::ostringstream msg;
            msg << "Failed to create file " << temp_cachepath << " with error " << err << std::endl;
            throw INMAGE_EX(msg.str());
        }

        std::stringstream ss;
        ss << CxArgObj<InitialSettings>(initialSettings);
        const size_t slen = ss.str().length();
        ssize_t bytesWritten = 0;
        do
        {
            ssize_t tmp = ACE_OS::write(hOut, ss.str().c_str() + bytesWritten, slen - bytesWritten);
            if (tmp == -1)
            {
                int err = ACE_OS::last_error();
                std::stringstream msg;
                msg << "Failed to cache the replication settings to " << temp_cachepath << " with error " << err;
                throw INMAGE_EX(msg.str());
            }
            bytesWritten += tmp;
        } while (bytesWritten != slen);

        SafeCloseAceHandle(hOut, temp_cachepath.c_str());

        if (ACE_OS::rename(temp_cachepath.c_str(), cachePath.c_str()) < 0)
        {
            int err = ACE_OS::last_error();
            std::stringstream msg;
            msg << "Failed to cache the replication settings, rename failed from "
                << temp_cachepath.c_str() << " to " << cachePath.c_str()
                << " with error " << err;
            ACE_OS::unlink(temp_cachepath.c_str());
            throw INMAGE_EX(msg.str());
        }
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with exception %s\n", FUNCTION_NAME, e.what());
        SafeCloseAceHandle(hOut, temp_cachepath.c_str());
        throw;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unknown exception.\n", FUNCTION_NAME);
        SafeCloseAceHandle(hOut, temp_cachepath.c_str());
        throw;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

//
// Persists sttings to local cache.
// Local caching is requird on target for reprotect scenario where cdpv2writer use configurator instance
// to read required settings from local settings cache.
//
void ResyncRcmSettings::CacheVolumeGroupSettings(const VOLUME_GROUP_SETTINGS &volGroupSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (m_Args.IsOnpremReprotectTargetSide())
    {
        boost::filesystem::path settingsCachePathOnCsPrimeAppliance(
            boost::filesystem::path(m_Args.GetTargetReplicationSettingsFileName()).parent_path().string());

        settingsCachePathOnCsPrimeAppliance /= m_Args.GetSourceHostId() + "_" + m_Args.GetDiskId() + "_reprotectconfig.dat";

        m_settingsCachePathOnCsPrimeAppliance = settingsCachePathOnCsPrimeAppliance.string();

        InitialSettings initialSettings;

        initialSettings.hostVolumeSettings.volumeGroups.insert(initialSettings.hostVolumeSettings.volumeGroups.end(), volGroupSettings);

        PersistConfigSettings(initialSettings);

        try
        {
            InitializeConfigurator(&TheConfigurator, USE_FILE_PROVIDED, PHPSerialize, settingsCachePathOnCsPrimeAppliance.string());
        }
        catch (const std::exception & e)
        {
            std::stringstream sserr("Failed to initialize InitializeConfigurator from local cache path");
            sserr << settingsCachePathOnCsPrimeAppliance.string() << " with exception " << e.what() << '.';
            throw INMAGE_EX(sserr.str().c_str());
        }
        catch (...)
        {
            std::stringstream sserr("Failed to initialize InitializeConfigurator from local cache path");
            sserr << settingsCachePathOnCsPrimeAppliance.string() << " with an unknown exception";
            throw INMAGE_EX(sserr.str().c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

ResyncRcmSettings::~ResyncRcmSettings()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    m_RcmConfiguratorPtr->GetSyncSettingsChangedSignal().disconnect(this);

    boost::system::error_code ec;
    if (IsOnpremReprotectTargetSide() &&
        !m_settingsCachePathOnCsPrimeAppliance.empty() &&
        boost::filesystem::exists(m_settingsCachePathOnCsPrimeAppliance, ec))
    {
        ACE_OS::unlink(getLongPathName(m_settingsCachePathOnCsPrimeAppliance.c_str()).c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}