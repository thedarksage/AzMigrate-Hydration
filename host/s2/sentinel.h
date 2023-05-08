//#pragma once

#ifndef SENTINEL__H
#define SENTINEL__H


#include <map>
#include <cstddef>
#include "volumegroupsettings.h"
#include "event.h"
#include "s2libsthread.h"
#include "volumeop.h"
#include "prismvolume.h"
#include "mirrorerror.h"
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Manual_Event.h>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include "configwrapper.h"
#include "stopvolumegroups.h"
#include "svproactor.h"
#include "datacacher.h"
#include "filterdrivernotifier.h"
#include "devicefilterfeatures.h"
#include "ChurnAndUploadStatsCollector.h"

#ifdef SV_WINDOWS
#include <windows.h>
#include <winioctl.h>
#include <atlbase.h>
#endif
#include "devicefilter.h"
#include "AgentSettings.h"
#include "devmetrics.h"

using namespace RcmClientLib;

struct Entity;
class Lockable;
struct Compress;
class ProtectedGroupVolumes;
struct VOLUME_GROUP_SETTINGS;
class Event;
class DeviceStream;
struct Configurator;
class Thread;
//namespace inmage {

typedef std::string DiskId;

typedef std::map<ProtectedGroupVolumes*, Thread*> MASTER_PROTECTED_VOLUME_GROUP;
typedef std::map<PrismVolume*, Thread*> MASTER_PRISM_VOLUMES;
typedef std::map<DiskId, ProtectedGroupVolumes*> MASTER_PROTECTED_VOLUMES;
typedef std::map<DiskId, DATA_PATH_TRANSPORT_SETTINGS> MASTER_TRANSPORT_SETTINGS;


const int MAXIMUM_THREADS  = 32;
const int DEFAULT_SENTINEL_EXIT_TIME            =   24 * 60 * 60;  // seconds
const int SENTINEL_TRANSFER_WAIT_TIME           =   15;            // seconds
#define SENTINEL_HOST_ID_VALUE_NAME                     _T( "HostId" )
#define SENTINEL_EXIT_TIME_VALUE_NAME                   _T("ExitTime")


class Sentinel: public Entity,public sigslot::has_slots<>
{
public:
    Sentinel();
    ~Sentinel();
    int Init();

    /// \brief registers with filter driver
    bool RegisterWithDriver(const std::string &driverName); ///< driver name

    static THREAD_FUNC_RETURN RunProtect(void*);
    int Protect();
    /* PR # 6513 : START 
     * Removed the liQuitInfo parameter since m_liProcessState member
     * is added to Sentinel class to hold process state */
    int StopProtection(void);
    /* PR # 6513 : END */
    int Destroy();
    const bool IsProtecting() const;
    int GetSentinelExitTime();
    void Signal(const long int);
    bool QuitSignalled();
    bool IsTimeUp(const ACE_Time_Value&);
    WAIT_STATE Wait(const int iSeconds);
    /* PR # 6513 : START 
     * Added setter and getter methods for m_liProcessState */
    void SetProcessState(const long int liProcessState);
    long int GetProcessState(void);
    /* PR # 6513 : END */

    /// \brief Checks compatibility with driver
    bool IsFilterDriverCompatible(const DRIVER_VERSION &dv); ///< driver version

    /// \brief gets filter driver features like per io time stamp support etc,.
    ///
    /// \return
    /// \li \c SV_FAILURE: on error
    /// \li \c SV_SUCCESS: on success
    int GetDriverFeatures(void);

    void SetUpFileXferLog(const VOLUME_SETTINGS& VolumeSettings);

    /// \brief sets the vic cache to share with replication threads
    void SetVolumesCache(void);

    /// \brief callback when the drain settings change
    void OnDrainSettingsChange();

private:
    bool m_bProtecting;
    bool m_bShutdownInProgress;
    pid_t m_ProcessId;
    boost::shared_ptr<boost::thread> m_CxTransportXferLogMonitor;

    MASTER_PROTECTED_VOLUME_GROUP m_MasterProtectedVolumeGroup;
    MASTER_PRISM_VOLUMES m_MasterPrismVolumes;
    long int m_liMasterGroupId;
    HOST_VOLUME_GROUP_SETTINGS m_HostVolumeGroupSettings;
    PRISM_SETTINGS m_PrismSettings;
    boost::shared_ptr<Event> m_Quit;
    Thread m_MasterThread;
    int m_ExitTime;
    bool m_bLogFileXfer;
    SVProactorTask*  m_ProactorTask ;
    ACE_Thread_Manager m_ThreadManager;
    /* PR # 6513 : START 
     * Added a member m_liProcessState to keep track of s2 process state */
    long int m_liProcessState;
    /* PR # 6513 : END */
    
    FilterDriverNotifier::Ptr m_pFilterDriverNotifier; ///< filter driver notifier

    DeviceFilterFeatures::Ptr m_pDeviceFilterFeatures; ///< device filter features

    DrainSettings_t m_drainSettings;
    std::string m_dataTransportType;
    std::string m_dataTransportSettings;
    boost::mutex m_mutexDrainSettingsChange;
    
    MASTER_PROTECTED_VOLUMES m_MasterProtectedVolumes;

    MASTER_TRANSPORT_SETTINGS m_MasterTransportSettings;

    ChurnAndUploadStatsCollectorBasePtr     m_statsCollectorPtr;

public:
    std::string m_HostId;
    MirrorError *m_pMirrorError;
    DataCacher::VolumesCache m_VolumesCache; ///< vic cache to share with replication threads
    size_t m_MetadataReadBufLen;

private:
    void StartNewProtectedGroupVolumes();

    Sentinel(const Sentinel&);
    void InitQuitEvent();

    void SendChurnAndUploadStats(QuitFunction_t qf);
    SVSTATUS InitStatsCollectorPtr();

    /// \brief returns filter driver name based on device type specified in volume settings
    std::string GetFilterDriverName(ConstVolToVgsIter_t cit); ///< First source volume group settings

    bool HandleDrainSettingsChangeForOnPremToAzureAgent();
    bool HandleDrainSettingsChangeForAzureVm();
};
//}

#endif

