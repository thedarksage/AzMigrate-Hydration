#ifndef DATAPROTECTION_RCM_H
#define DATAPROTECTION_RCM_H

#include <boost/noncopyable.hpp>

#include "cdplock.h"
#include "fastsyncRcm.h"
#include "RcmConfigurator.h"
#include "dpRcmProgramargs.h"
#include "resyncRcmSettings.h"
#include "resyncStateManager.h"
#include "dataprotectionBase.h"

class DataProtectionRcm : public DataProtectionBase, boost::noncopyable
{
public:
    explicit DataProtectionRcm(const DpRcmProgramArgs &args,
        const boost::shared_ptr<ResyncStateManager> resyncStateManager);

    virtual ~DataProtectionRcm();

    virtual void Protect();

protected:
    virtual void ProcessHostVolumeGroupSettings(HOST_VOLUME_GROUP_SETTINGS& hostVolGroupSettings);

    virtual std::string GetLogDir();

private:
    void ProcessVolumeGroupSettingsForSource(VOLUME_GROUP_SETTINGS& volGroupSettings);

    void ProcessVolumeGroupSettingsForTarget(VOLUME_GROUP_SETTINGS& volGroupSettings);

    void Create();

    void Start();

    void Stop();

private:
    const DpRcmProgramArgs &m_Args;

    RcmClientLib::RcmConfiguratorPtr m_RcmConfigurator;
    
    boost::shared_ptr<ResyncRcmSettings> m_ResyncRcmSettings;

    const boost::shared_ptr<ResyncStateManager> m_ResyncStateManager;

    HOST_VOLUME_GROUP_SETTINGS m_HostVolGroupSettings; // TODO: can this be smart pointer?

    boost::shared_ptr<FastSync> m_FastSyncJob;

    std::vector<CDPLock::Ptr> m_cdplocks;
};

#endif // DATAPROTECTION_RCM_H