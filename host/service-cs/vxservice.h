#ifndef INMAGE_VXSERVICE_H_
#define INMAGE_VXSERVICE_H_

#include <set>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "ace/Signal.h"
#include "sigslot.h"
#include "cservice.h"
#include "servicemajor.h"
#include "AgentProcess.h"
#include "talwrapper.h"
#include "filterdrivernotifier.h"
#include "rpcconfigurator.h"
#include "registerhostthread.h"
#include "devicestream.h"
#include "filterdrvifmajor.h"
#include "volumeop.h"
#include "localconfigurator.h"
#include "datacacher.h"
#include "monitorhostthread.h"
#include "consistencythread-cs.h"
#include "SrcTelemetry.h"
#include "LogCutter.h"

#ifdef SV_WINDOWS
    #include "marshealthmonitorthread.h"
#endif

using namespace SrcTelemetry;
#define NUMRETRY 5
#define STATUS_SHUTDOWN_UNCLEAN           0

const ACE_TCHAR SERVICE_NAME[] = ACE_TEXT("svagents");
const ACE_TCHAR SERVICE_TITLE[] = ACE_TEXT(INMAGE_VX_SERVICE_LABEL);
const ACE_TCHAR SERVICE_DESC[] = ACE_TEXT("Volume Replication Service");
struct DiskNotFoundErrRateControl
{
    DiskNotFoundErrRateControl() : m_lastDiskNotFoundErrLogTime(0), m_diskNotFoundCnt(0) {}
    long long m_lastDiskNotFoundErrLogTime;
    unsigned int m_diskNotFoundCnt;
};
typedef std::map<std::string, DiskNotFoundErrRateControl> DiskNotFoundErrRateControlMap;

class VxService : public ACE_Event_Handler, public sigslot::has_slots<>
{
public:

    enum WAIT_FOR_STATUS { WAIT_FOR_ERROR = -1, WAIT_FOR_SUCCESS = 0, WAIT_FOR_STOP_AGENTS_FOR_OFFLINE_RESOURCE = 1 };

    typedef std::set<boost::shared_ptr<AgentProcess>, AgentProcess::LessAgentProcess> AgentProcesses_t;
    typedef AgentProcesses_t::iterator AgentProcessesIter_t;
    typedef std::vector<AgentProcessesIter_t> AgentProcessesIterVec_t;
    HOST_VOLUME_GROUP_SETTINGS m_volGroups ;
    
    std::list < boost::shared_ptr < Logger::LogCutter > > m_LogCutters;
    VxService ();
    ~VxService ();


    virtual int handle_exit (ACE_Process *proc);
    virtual int handle_signal(int signum,siginfo_t * = 0,ucontext_t * = 0);

    int parse_args(int argc, ACE_TCHAR* argv[]);
    void display_usage(char *exename = NULL);

    int install_service();
    int remove_service();
    int start_service();
    int stop_service();
    void name(const ACE_TCHAR *name, const ACE_TCHAR *long_name = NULL, const ACE_TCHAR *desc = NULL);
    int run_startup_code();
    static int StartWork(void *data);
    static int StopWork(void *data);
    static int ServiceEventHandler(signed long,void *);
    const bool isInitialized() const;
protected:
    //
    // Callback methods
    //
    void onSetReadOnly( std::string const& deviceName );
    void onSetReadWrite( std::string const& deviceName );
    void onSetVisible( std::string const& deviceName );
    void onSetInvisible( std::string const& deviceName );
    void onThrottle( ConfigureVxAgent::ThrottledAgent a, std::string const& deviceName, bool throttle );
    void onStartSnapshot( int id, std::string const& sourceDeviceName, std::string const& targetDeviceName, std::string const& preScript, std::string const& postScript );
    void onStopSnapshot( int id );

protected:
    //
    // Helper methods
    //
    int init();
    bool updateUpgradeInformation(LocalConfigurator &lc);
    bool updateResourceID(LocalConfigurator &lc);
    bool updateSourceGroupID(LocalConfigurator &lc);
    bool doPreConfiguratorTasks(void);

    void StopAgents();
    void StopSentinel();
    void StopCacheManager();
    void StopCdpManager();
    void StopAgentsRunningOnSource(const std::string &devicename);
    void StopAgentsHavingAttributes(const std::string &DeviceName, const std::string &EndpointDeviceName, const int &group, const uint32_t &waitTimeBeforeTerminate = 0);
    bool PostQuitMessageToAgent(const boost::shared_ptr<AgentProcess> &ap);

    void StartVacp(const LocalConfigurator &lc);
    void StartSentinel();
    void StartEvtCollForw(LocalConfigurator &lc);
    void StartCsJobProcessor(LocalConfigurator &lc);

    /// \brief starts cache manager
    void StartCacheManger(LocalConfigurator &localConfigurator); ///< local configurator

    /// \brief starts cdpmgr
    void StartCdpManger(LocalConfigurator &localConfigurator); ///< local configurator

    ///< \brief check protection status
    bool CanStartVacp(bool &IsIRMode) const;

    ///< \brief starts resync and diffsync dataprotections
    void StartVolumeGroups(LocalConfigurator &localConfigurator); ///< local configurator

    /// \brief registers with involflt and start s2 if prism settings are present
    void StartPrismAgents(LocalConfigurator &lc); ///< local configurator

    void StartFastOffloadSyncAgent(AgentProcess::Ptr& agentInfo, VOLUME_SETTINGS::SYNC_MODE syncMode, CDP_DIRECTION direction);
    void StartDiffSyncTargetAgent(AgentProcess::Ptr& agentInfo, std::vector<std::string> visibleDrives);
    void StartSnapshotAgent(AgentProcess::Ptr& agentInfo);

    void GetAgentsHavingAttributes(const std::string &DeviceName, const std::string &EndpointDeviceName, 
                                   const int &group, AgentProcessesIterVec_t &agentprocesses);

    bool CanStartFastsyncSourceAgent(VOLUME_SETTINGS &vs, VOLUME_GROUP_SETTINGS &vgs, std::string &reasonifcant);
    bool CanStartDirectsyncAgent(VOLUME_SETTINGS &vs, VOLUME_GROUP_SETTINGS &vgs, std::string &reasonifcant);

    /// \brief finds if sync can be started on source by verifying current source capacity, the capacity in settings and the target's capacity
    ///
    /// \return
    /// \li \c true : sync can start
    /// \li \c false : sync cannot start
    bool CanStartSyncAgentOnSource(const std::string &sourcedevicename,             ///< source
                                   const SV_ULONGLONG &sourcecapacity,              ///< source capacity in settings
                                   const SV_ULONGLONG &sourcerawsize,               ///< source raw size in settings
                                   const int &sourcedevicetype,                     ///< source device type (VOLUME/DISK etc,.)
                                   const std::string &targetdevicename,             ///< target
                                   const SV_ULONGLONG &targetrawsize,               ///< target raw size from settings
                                   const VOLUME_SETTINGS::PROTECTION_DIRECTION &pd, ///< protection direction (forward/reverse)
                                   std::string &reasonifcant                        ///< reason if sync cannot be started
                                   );

    bool IsTargetDeviceSizeAppropriate(const std::string &sourcedevicename, const SV_ULONGLONG &sourcecapacity, 
                                       const std::string &targetdevicename, const SV_ULONGLONG &targetrawsize,
                                       std::string &reasonifnot);

    /// \brief gets filter driver's tracking size of a source device and overwrites it, if it is not matching with latest device size
    ///
    /// \return
    /// \li \c true : tasks done
    /// \li \c false : tasks not done
    bool DoPreResyncFilterDriverTasks(const std::string &sourcedevicename, ///< source
        const SV_ULONGLONG &sourcecapacity,                                ///< source capacity in settings
        const SV_ULONGLONG &sourcerawsize,                                 ///< source raw size in settings
        const int &sourcedevicetype,                                       ///< source device type (VOLUME/DISK etc,.)
        std::string &reasonifcant                                          ///< reason if tasks could not be done
        );

    /// \brief deletes filter driver's tracking size of source device
    ///
    /// \return
    /// \li \c true : deleted
    /// \li \c false : not deleted
    bool DeleteFilterDriverVolumeTrackingSize(const std::string &devicename, ///< device
        const int &devicetype,                                               ///< source device type (VOLUME/DISK etc,.)
        std::string &reasonifcant                                            ///< reason if could not delete
        );

    void ProcessMessageLoop();
    int WaitForMultipleAgents(const ACE_Time_Value&);
    void ProcessDeviceEvent(void*);
    //Removed by Ranjan bug#10404
    //bool RegisterXenInfo();
    void StopFiltering();


    //
    // PR# 5554
    // Renamed AddToStopFiltering routine as ConfigChangeCallback
    // Now we get a callback on any change to initial settings
    // InitialSettings are persisted to local store for later
    // use by cdpcli
    //
    void ConfigChangeCallback(InitialSettings);
    void updateConfigurationChange(InitialSettings);
    void SerializeConfigSettings(const InitialSettings&);

    // Callback for the CS address change for Azure components
    void CsAddressForAzureComponentsChangeCallback(LocalConfigurator& lc, std::string);

    //Bug #6890
    void CleanupActionMap(std::map<std::string,std::string> & mapreplog,const std::string & replication_clean);
    bool doUnlockVol(const string & DeviceName, const string & mnt, const string & fsType, string & errorMessage);
    bool CleanupAction(const string & deviceName, 
                       const std::string &endpointDeviceName, 
                       const CDP_DIRECTION &direction,
                       const int &grpid,
                       const string & cleanup_action,
                       const VOLUME_SETTINGS::SYNC_MODE & syncmode
                       );
    bool IsRemoveCleanupDevice(const string & deviceName);
    bool IsDeviceInUse(const std::string & deviceName,
                       const std::string & endpointDeviceName, 
                       const CDP_DIRECTION &direction,
                       const int &grpid,
                       const VOLUME_SETTINGS::SYNC_MODE &syncmode);

    void PerformPendingPauseAction(const LocalConfigurator &lc, 
                                   const std::string &deviceName,
                                   const std::string &endpointDeviceName,
                                   const CDP_DIRECTION &direction,
                                   const int &grpid,
                                   const std::string &maintenance_action,
                                   const VOLUME_SETTINGS::SYNC_MODE &syncmode,
                                   const std::string &prevResyncJobId);

    void StartConsistenyThread();
    void StartMonitors();
    void StartRegistrationTask();
    void StopMonitors();

#ifdef SV_WINDOWS
    void StartMarsHealthMonitor();
    void StopMarsHealthMonitor();
#endif

    bool SetSecurity(char const * server, SV_INT httpPort, char const * secureModesUrl, char const * hostId);
    //added new function to delete replication pair from CX if rollback exists for bug 6723
    bool ProcessRollback(const std::string& Volume, const std::string& hashName);
    // added second parameter to OkToProcessDiffs() for resolving the bug# 5276
    bool OkToProcessDiffs(const VOLUME_SETTINGS & volsetting);
    //added new function to checkconfigurator for the bug  5924 to trace the exact failure message
    bool CheckConfigurator(int &RequestedState,	std::string &sMountPoint, std::string &sFileSystem,const std::string& Volume);
    void ProcessVolumeGroupsPendingVolumeVisibililities(HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator const & volumeGroupIter, std::vector<std::string> & visibleVolumes, bool & atleastOneHiddenDrive);
    bool IsTargetVolumeSet( std::string VolumeSet ) const;
    bool IsTargetThrottled( std::string const& deviceName );
    bool IsSourceThrottled( std::string const& deviceName );
    bool IsResyncThrottled( std::string const& deviceName , const std::string &endpointdeviceName,
                                   const int &grpid);
    bool IsUninstallSignalled() const;
    std::string GetSourceDirectoryNameOnCX(std::string volume);
    void CleanUp();
    bool getDataprotectionDiffLog(std::string & logFile, int id);
    bool getDataprotectionResyncLog(std::string & logFile, const std::string & deviceName,
                                    const std::string &endpointDeviceName, const CDP_DIRECTION &direction,
                                    const int &grpid,
                                    const VOLUME_SETTINGS::SYNC_MODE &syncmode);
    bool deleteDataprotectionLog(const std::string & filename);

    /// \brief performs filter driver notification
    bool NotifyFilterDriver(const std::string &filterDriverName); ///< filter driver name to open and notify

#ifdef SV_UNIX
    bool UpdateCleanShutdownFile();
#endif

#ifdef SV_WINDOWS
    /// \brief performs filter driver data paged pool configuration
    bool ConfigureDataPagedPool(const LocalConfigurator &lc);
#endif
    //
    // routines to help switching to remote cx
    //
    bool InitializeConfigurator();
    bool switchcx() ;
    bool FetchRemoteCxInfo(SV_ULONG & timeout);
    bool SendRetentionRefresh();
    void onFailoverCxSignal(ReachableCx);
    bool CheckCacheDir(void);

    /// \brief returns current source capacity
    ///
    /// \return
    /// \li \c non zero size: success
    /// \li \c 0            : failure
    SV_ULONGLONG GetCurrentSourceCapacity(const std::string &sourcedevicename, const int &sourcedevicetype);

    /// \brief performs filter driver notification
    bool PerformSourceTasks(const std::string &filterDriverName, ///< filter driver name to open and notify
        LocalConfigurator &lc                                    ///< local configurator
        );
    void LogSourceReplicationStatus();
    void LogSourceReplicationStatus(const VOLUME_GROUP_SETTINGS &vgs, uint32_t reason);
    void FillReplicationStatusFromDriverStats(ReplicationStatus &repStatus, const VOLUME_GROUP_SETTINGS &vgs);
    void FillReplicationStatusFromSettings(ReplicationStatus &repStatus, const VOLUME_GROUP_SETTINGS &vgs);
    void FillReplicationStatusWithSystemInfo(ReplicationStatus &repStatus);
public:
    int opt_install;
    int opt_remove;
    int opt_start;
    int opt_kill;
    int opt_standalone;
    int quit;
    void prepareStoppedVolumesList(InitialSettings initialSettings) ;
private:
    //
    // Cached state from CX and registry
    //
    static FilterDriverNotifier* m_sPFilterDriverNotifier; ///< filter driver notifier
    bool m_bInitialized;
    bool m_bUninstall;
    static bool m_VsnapCheckRemount;
    static bool m_VirVolumeCheckRemount;

    Configurator* m_configurator;
    RegisterHostThread* m_RegisterHostThread;
    MonitorHostThread* m_MonitorHostThread;
    ConsistencyThread* m_ConsistencyThread;

#ifdef SV_WINDOWS
    MarsHealthMonitorThread* m_MarsHealthMonitorThread;
#endif

    std::vector<std::string> m_initialSyncComplete;
    time_t m_lastUpdateTime;
    std::vector<std::string> m_cleanupVolumes;
    std::list<std::string> m_unhiddenRawVolumes;
    std::map<std::string, boost::chrono::steady_clock::time_point> m_pausePendingAckMap;

    AgentProcesses_t m_AgentProcesses;

    ACE_Sig_Handler sig_handler_;
    ACE_Mutex m_PropertyChangeLock;

    std::set<std::string> m_protectedVolumes;
    std::set<std::string> m_stoppedVolumes;

    std::string m_sVolumesOnline;

    //
    // We get a “list” of viable CX-s. Only one which can be primary at a time.
    // Any of the other CX’s could become a primary.
    // 
    ViableCxs m_viablecxs;
    ReachableCx m_reachablecx;
    bool m_switchCx;    
    bool m_bTSChecksEnabled;    
    bool m_bEnableVolumeMonitor;
    bool m_bHostMonitorStarted;
    ACE_Process_Manager *m_ProcessMgr;
    FilterDrvIfMajor m_FilterDrvIf;
    std::set<std::string> m_FilterDrvBitmapDeletedVolumes;
    VolumeOps::VolumeOperator m_Vopr;
    DataCacher::VolumesCache m_VolumesCache; ///< volumes cache
    bool m_IsFilterDriverNotified;           ///< true if filter driver was notified else false

    std::map<std::string, ACE_Time_Value> m_agentsExitStatusMap; // TBD: need to persist to file in case of svagents restart state need to be retrieved
    ACE_Mutex m_lastPSTransErrLogTimeLock;
    bool        m_isAgentRoleSource;
    uint64_t    m_SrcTelemeteryPollInterval;
    uint64_t    m_SrcTelemeteryStartDelay;
    DiskNotFoundErrRateControlMap m_diskNotFoundErrRateControlMap;
    boost::chrono::steady_clock::time_point m_lastEvtCollForwForkAttemptTime;
    boost::chrono::steady_clock::time_point m_lastCsJobProcessorForkAttemptTime;

    boost::chrono::steady_clock::time_point m_lastVacpStartTime;
};

//Logger macros
#define LOG2SYSLOG(EVENT_MSG) \
    do { \
    ACE_LOG_MSG->set_flags(ACE_Log_Msg::SYSLOG);\
    ACE_LOG_MSG->open(SERVICE_NAME, ACE_Log_Msg::SYSLOG, NULL);\
    ACE_DEBUG(EVENT_MSG);\
    } while (0)

#define LOG2STDERR(EVENT_MSG) \
    do { \
    ACE_LOG_MSG->set_flags(ACE_Log_Msg::STDERR);\
    ACE_DEBUG(EVENT_MSG);\
    } while (0)

#define LOG_EVENT(EVENT_MSG) \
    do { \
    if (SERVICE::instance ()->isInstalled() == true) \
    LOG2SYSLOG(EVENT_MSG); \
        else \
        LOG2STDERR(EVENT_MSG); \
    } while (0)

// Sample logging examples:

// The following macros print "Hello World 2006"

// LOG_VX_EVENT((LM_DEBUG, "Hello %C %d\n", "World", 2006));
// LOG2STDERR((LM_DEBUG, "Hello %C %d\n", "World", 2006));
// LOG2SYSLOG((LM_DEBUG, "Hello %C %d\n", "World", 2006));

#endif /* INMAGE_VXSERVICE_H */

