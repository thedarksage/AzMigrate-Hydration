#ifndef RESYNC_RCM_SETTINGS_H
#define RESYNC_RCM_SETTINGS_H

#include "sigslot.h"

#include <boost/thread/mutex.hpp>
#include <boost/atomic.hpp>

#include "RcmConfigurator.h"
#include "dpRcmProgramargs.h"

class ResyncRcmSettings : public sigslot::has_slots<>
{
public:
    static bool CreateInstance(const DpRcmProgramArgs &args);

    static bool GetInstance(boost::shared_ptr<ResyncRcmSettings> &resyncRcmSettings);

    const RcmClientLib::SyncSettings& GetSourceAgentSyncSettings() const
    {
        return m_SyncSettings;
    }

    const std::string& GetDataPathTransportType() const
    {
        return m_DataPathTransportType;
    }

    const std::string& GetDataPathTransportSettings() const
    {
        return m_strDataPathTransportSettings;
    }

    void RcmSettingsCallback();

    void RcmTunablePropertiesChangeCallback();

    bool IsSourceSide() const { return m_Args.IsSourceSide(); }

    bool IsOnpremReprotectTargetSide() const { return m_Args.IsOnpremReprotectTargetSide(); }

    void CacheVolumeGroupSettings(const VOLUME_GROUP_SETTINGS &volGroupSettings);

    ~ResyncRcmSettings();

private:
    explicit ResyncRcmSettings(const DpRcmProgramArgs &args) :
        m_Args(args),
        m_RcmConfiguratorPtr(RcmClientLib::RcmConfigurator::getInstance())
    {
    }

    bool Initialize();

    void PersistConfigSettings(const InitialSettings & initialSettings);

private:
    static boost::shared_ptr<ResyncRcmSettings> m_ResyncRcmSettings;

    static boost::atomic<bool> m_IsInitialized;

    static boost::mutex m_Lock;

    const DpRcmProgramArgs &m_Args;

    RcmClientLib::RcmConfiguratorPtr m_RcmConfiguratorPtr;

    RcmClientLib::SyncSettings m_SyncSettings;

    std::string m_DataPathTransportType;

    std::string m_strDataPathTransportSettings;

    std::string m_settingsCachePathOnCsPrimeAppliance;
};

#endif // RESYNC_RCM_SETTINGS_H