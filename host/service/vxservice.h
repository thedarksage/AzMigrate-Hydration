#ifndef INMAGE_VXSERVICE_H_
#define INMAGE_VXSERVICE_H_

#include <set>
#include <string>
#include <vector>

#include <boost/atomic.hpp>
#include <boost/shared_ptr.hpp>

#include "ace/Signal.h"
#include "sigslot.h"
#include "cservice.h"
#include "servicemajor.h"
#include "AgentProcess.h"
#include "filterdrivernotifier.h"
#include "registerhostthread.h"
#include "devicestream.h"
#include "filterdrvifmajor.h"
#include "localconfigurator.h"
#include "datacacher.h"
#include "monitorhostthread.h"
#include "RcmClient.h"
#include "rcmworkerthread.h"
#include "consistencythread-rcm.h"
#include "LogCutter.h"

#ifdef SV_WINDOWS 
#include "FailoverClusterMultiVmUtil.h"
#endif

#define NUMRETRY 5

#define STATUS_SHUTDOWN_UNCLEAN           0

const ACE_TCHAR SERVICE_NAME[] = ACE_TEXT("svagents");
const ACE_TCHAR SERVICE_TITLE[] = ACE_TEXT(INMAGE_VX_SERVICE_LABEL);
const ACE_TCHAR SERVICE_DESC[] = ACE_TEXT("Volume Replication Service");

class VxService : public ACE_Event_Handler, public sigslot::has_slots<>
{
public:

    enum WAIT_FOR_STATUS { WAIT_FOR_ERROR = -1, WAIT_FOR_SUCCESS = 0, WAIT_FOR_STOP_AGENTS_FOR_OFFLINE_RESOURCE = 1 };

    typedef std::set<boost::shared_ptr<AgentProcess>, AgentProcess::LessAgentProcess> AgentProcesses_t;
    typedef AgentProcesses_t::iterator AgentProcessesIter_t;
    typedef std::vector<AgentProcessesIter_t> AgentProcessesIterVec_t;
    
    VxService ();
    ~VxService ();

    virtual int handle_exit (ACE_Process *proc);
    virtual int handle_signal(int signum,siginfo_t * = 0,ucontext_t * = 0);

    int parse_args(int argc, ACE_TCHAR* argv[]);
    
    static int StartWork(void *data);
    static int StopWork(void *data);
    static int ServiceEventHandler(signed long,void *);

    void name();
    int run_startup_code();
    const bool isInitialized() const;

    boost::atomic<int> m_quit;
    boost::shared_ptr<Logger::LogCutter> m_LogCutter;

protected:
    //
    // Helper methods
    //

    /// \brief checks if the host is a recovered host and runs recovery commands
    void RunRecoveryCommand();
    SVSTATUS CompleteRecoverySteps();
    SVSTATUS CompleteRecoveryStepsForHybrid(LocalConfigurator &lc, bool& bClone);
    SVSTATUS CompleteRecoveryStepsForAzureToAzure(LocalConfigurator &lc);
    SVSTATUS CompleteRecoveryStepsForOnPremToAzure(LocalConfigurator &lc);
    SVSTATUS CompleteRecoveryStepsForAzureToOnPrem(LocalConfigurator& lc);
    SVSTATUS CompleteRecoveryStepsCommon(LocalConfigurator& lc);

    int init();
    bool updateResourceID();
    bool updateSourceGroupID();
    bool doPreConfiguratorTasks();
    bool UpdateConfiguredBiosId(LocalConfigurator &lc);
    bool IsConfiguredBiosIdMatchingSystemBiosId(LocalConfigurator &lc, bool update=false);

    void StartMonitors();
    void StartRegistrationTask();
    void StartRcmWorker();
    void StartConsistenyThread();

    SwitchAppliance::State HandleSwitchApplianceState();
    void ProcessMessageLoop();
    
    /// \brief performs filter driver notification
    bool PerformSourceTasks(const std::string &filterDriverName, ///< filter driver name to open and notify
        LocalConfigurator &lc                                    ///< local configurator
        );

    /// \brief performs filter driver notification
    bool NotifyFilterDriver(const std::string &filterDriverName); ///< filter driver name to open and notify

    void StartVacp(const LocalConfigurator &lc);
    void StartSentinel(LocalConfigurator &lc);
    void StartEvtCollForw(LocalConfigurator &lc);
    void StartDataProtection(LocalConfigurator &lc);

    void StopAgents();
    void StopSentinel();
    void StopAgentsRunningOnSource(const std::string &devicename);
    bool PostQuitMessageToAgent(const boost::shared_ptr<AgentProcess> &ap);

    void StopVacp();
    void RestartVacp();

    bool IsSentinelRunning();

    void GetAgentsHavingAttributes(const std::string &DeviceName, const std::string &EndpointDeviceName, 
                                   const int &group, AgentProcessesIterVec_t &agentprocesses);
    void StopAgentsHavingAttributes(const std::string &DeviceName,
        const std::string &EndpointDeviceName,
        const int &group,
        const uint32_t &waitTimeBeforeTerminate = 0);

    int WaitForMultipleAgents(const ACE_Time_Value&);
    
    void ProcessDeviceEvent(void*);
#ifdef SV_WINDOWS
    void ProcessBitLockerEvent(void*);
#endif // SV_WINDOWS

    /// \brief sets the rollback states
    bool PerformRollbackIfMigrationInProgress();

    void ProtectionDisabledCallback();
    void PrepareSwitchApplianceCallback();
    void SwitchApplianceCallback(RcmProxyTransportSetting &rcmProxySettings);
    void StopReplicationOnProtectionDisabled();
    SVSTATUS CompleteAgentRegistration();

    bool IsUninstallSignalled() const;
    void StopFiltering();
    void StopMonitors();
    void CleanUp();

#ifdef SV_UNIX
    bool UpdateCleanShutdownFile();
    
    /// \brief verifies if the driver has stacked the correct devices during booting
    SVSTATUS VerifyProtectedDevicesOnStartup();
    
    /// \brief returns the drivered filtered device for a persistent name (i.e. diskid)
    bool GetDriverFilteredDeviceForPersistentDevice(const std::string &persistentName, std::string& deviceName);

    /// \brief check if filter driver supports persistent names
    bool IsDriverPnameCheckSupported();

#endif

#ifdef SV_WINDOWS
    /// \brief performs filter driver data paged pool configuration
    bool ConfigureDataPagedPool(const LocalConfigurator &lc);
#endif

    void GetResyncBatch(ResyncBatch &resyncBatch);
    void ReadPersistedResyncBatching();
    
public:
    int m_optStandalone; 

private:

    static FilterDriverNotifier* m_sPFilterDriverNotifier; ///< filter driver notifier
    bool m_bInitialized;
    bool m_bUninstall;

    RegisterHostThread* m_RegisterHostThread;
    MonitorHostThread* m_MonitorHostThread;
    RcmWorkerThread* m_RcmWorkerThread;
    ConsistencyThread* m_ConsistencyThread;

    AgentProcesses_t m_AgentProcesses;

    ACE_Sig_Handler sig_handler_;
    ACE_Mutex m_PropertyChangeLock;
    ACE_Mutex m_sentinelLock;
    ACE_Mutex m_quitEventLock;

    bool        m_bVacpStopRequested;
    ACE_Mutex   m_vacpLock;
    ACE_Mutex   m_migrationStateLock;

    std::string m_sVolumesOnline;
    bool m_bEnableVolumeMonitor;
    bool m_bHostMonitorStarted;
    bool m_bIsProtectionDisabled;
    RcmProxyTransportSetting m_switchApplianceSettings;

    ACE_Process_Manager *m_ProcessMgr;
    FilterDrvIfMajor m_FilterDrvIf;
    DataCacher::VolumesCache m_VolumesCache; ///< volumes cache
    
    bool m_IsFilterDriverNotified;           ///< true if filter driver was notified else false

#ifdef SV_WINDOWS 
    ClusterConsistencyUtil *m_failoverClusterConsistencyUtil;
#endif
    boost::chrono::steady_clock::time_point m_lastEvtCollForwForkAttemptTime;

    std::string m_prevConsSetChecksum; ///< Last ConsistencySettings checksum
    boost::chrono::steady_clock::time_point m_lastVacpStartTime;

    /// \brief flag indicating a service reinitialization required on disable replication
    /// this is required in case of V2A only
    bool m_bRestartOnDisableReplication;

    ResyncBatch m_ResyncBatch;
    std::string m_strResyncBatch;
    ACE_Mutex   m_ResyncBatchLock;
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

