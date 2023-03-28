#ifndef MONITORHOSTTHREAD_H
#define MONITORHOSTTHREAD_H

#include <ace/Thread_Manager.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Event.h>

#include <boost/shared_ptr.hpp>
#include <boost/chrono.hpp>

#include "svstatus.h"
#include "localconfigurator.h"
#include "inmquitfunction.h"
#include "sigslot.h"
#include "datacacher.h"
#include "volumereporterimp.h"
#include "proxysettings.h"
#include "azureblobstream.h"
#include "devicestream.h"
#include "inmalertdefs.h"

#include "filterdrvifmajor.h"
#include "involfltfeatures.h"
#include "AgentHealthContract.h"
#include "consistencythread.h"
#include "AgentSettings.h"

using namespace boost::chrono;
using namespace RcmClientLib;

#define TEST_FLAG(_a, _f)       (((_a) & (_f)) == (_f))

struct LogUploadLocationStatus
{
    LogUploadLocationStatus()
        : m_bFailed(false)
    {}

    steady_clock::time_point m_firstFailTime;
    bool m_bFailed;
};

class MonitorHostThread : public sigslot::has_slots<>
{
public:
    MonitorHostThread(unsigned int monitorHostInterval = 43200);
    ~MonitorHostThread();
    void Start();
    void Stop();

    /// \brief waits for quit event for given seconds and returns true if quit is set (even before timeout expires).
    bool QuitRequested(int seconds); ///< seconds

    void    SetLastTagState(TagTelemetry::TagType tagType, bool bSuccess, const std::list<HealthIssue>&vacpHealthIssues);

    /// \brief returns sigslot reference for log upload failure signal
    sigslot::signal3<const std::string &, const std::string&, const std::string&>& GetLogUploadAccessFailSignal();	

    /// \brief returns sigslot reference for RetrieveResyncBatch signal
    sigslot::signal1<ResyncBatch &> & GetResyncBatchSignal();

protected:
    ACE_Thread_Manager m_threadManager;
    static ACE_THR_FUNC_RETURN ThreadFunc(void* pArg);
    static ACE_THR_FUNC_RETURN CxStatsThreadFunc(void* pArg);

    ACE_THR_FUNC_RETURN run();
    void MonitorHost(QuitFunction_t qf = 0);
    void ExecCmd(const std::string &cmd, const std::string &tmpOpFile);
    void VerifyMonitoringEvents();
    
    /// \brief checks agent health status
    void CheckAgentHealth();

    /// \brief checks if disk Id present in disk discovery info
    /// \returns
    /// \li SVS_OK on success
    /// \li SVE_INVALIDARG on failure to find the volume in volume cache
    /// \li SVE_FAIL if data cacher can't be initialized
    SVSTATUS IsDiskDiscovered(const std::string& diskId, std::string &name);

    /// \brief checks drain and sync log upload location status
    void MonitorLogUploadLocation(std::string& message);

	/// \brief check the status of blob SAS Uri
	void CheckLogUploadLocationStatus(const AzureBlobBasedTransportSettings &blobSettings,
        const std::string &id,
        const std::string& renewRequestType,
        std::string &msg);

    /// \brief checks if any of protected disks are missing
    void    UpdateMissingDiskList(std::vector<AgentDiskLevelHealthIssue>& diskHealthIssues);

    /// \brief checks if filter driver is loaded
    bool    IsDriverMissing(std::string &msg);

    /// \brief Prepares health issues based upon log location status.
    void    UpdateLogUploadLocationStatus(std::vector<HealthIssue>& healthIssues);

#ifdef SV_WINDOWS
    /// \brief Prepares health issues based upon cluster health
    void  UpdateSharedDiskClusterStatus(std::vector<HealthIssue>& healthIssues);
#endif

private:
    boost::shared_ptr<ACE_Event> m_QuitEvent;
    bool m_started;
    ACE_Mutex m_MonitorHostTimeLock;
    ACE_Mutex m_MonitorHostMutex; //Provide mutual exclusive access to MonitorHost() calls
    uint32_t    m_MonitorHostStartDelay;

    /// \brief Maps filter device name to protected device name.
    std::map<FilterDrvVol_t, std::string>      m_drvDevIdtoProtectedIdMap;

    uint32_t    m_MonitorHostInterval;
    uint32_t    m_agentHealthCheckInterval;

    steady_clock::time_point m_nextMonitorHostRunTime;
    steady_clock::time_point m_nextAgentHealthCheckTime;

    std::list<HealthIssue>                  m_vmHealthIssues;
    std::list<AgentDiskLevelHealthIssue>    m_diskHealthIssues;
    std::string                             m_strHealthCollatorPath;

    /// \brief Mutex used to synchronize health issues across disk and vm.
    ACE_Mutex                                m_agentHealthMutex;

    /// \brief Mutex used to synchronize disk level health issues.
    ACE_Mutex   m_diskHealthMutex;

    /// \brief Mutex used to synchronize vm level health issues.
    ACE_Mutex   m_vmHealthMutex;

    /// \brief This counter is used to clear CX events.
    uint32_t                    m_CxClearHealthIssueCounter;

    std::string m_hostId;

    /// \brief in-memory copy of the discovered volumes
    DataCacher::VolumesCache m_volumesCache;

    std::map<std::string, LogUploadLocationStatus> m_mapLogUploadStatus;
    uint32_t    m_logUploadLocationCheckTimeout;

    boost::shared_ptr<ProxySettings>            m_ptrProxySettings;
    boost::shared_ptr<BaseTransportStream>      m_ptrCxTransportStream;
    std::map<std::string, boost::shared_ptr <AzureBlobStream> >      m_mapPtrAzureBlobTranportStream;

    /// \brief driver stream handle
    DeviceStream::Ptr m_pDriverStream;

    /// \brief list of protected disks to be monitored for cx events
    std::set<std::string>   m_monitorDeviceIds;

    /// \brief serializes access m_monitorDeviceIds
    boost::mutex            m_mutexMonitorDeviceIds;

    /// \brief flag indicating if the cx session supported by filter driver
    bool m_bFilterDriverSupportsCxSession;

    /// \brief a call back to update the renewUriAuthFailList
    sigslot::signal3<const std::string&, const std::string&, const std::string&>  m_logUploadAccessFailSignal;

    /// \brief a call back to retrieve latest resync batch
    sigslot::signal1<ResyncBatch &> m_resyncBatchSignal;

private:
    /// \brief open the filter driver handle
    SVSTATUS CreateDriverStream();

    /// \brief monitor the cx envents raised by filter driver
    ACE_THR_FUNC_RETURN MonitorCxEvents();

    /// \brief fetches the protected device ids from the settings
    SVSTATUS GetProtectedDeviceIds(std::set<std::string> &deviceIds);

    /// \brief fetches the protected disk ids in settings
    SVSTATUS GetProtectedDisksIdsInSettings(std::set<std::string>& diskIds);

    /// \brief fetches the device ids that require health monitoring
    /// after excluding any devices that are not connected 
    SVSTATUS GetDeviceIdsForHealthMonitoring(std::set<std::string> &deviceIds);

    /// \brief check if the protected device ids for monitoring changed
    bool HasProtectedDeviceListChanged();

    /// \brief returns the persistent device id for the filter driver
    std::string GetPersistentDeviceIdForDriverInput(const std::string& deviceId);

    /// \brief returns the device id from the filter driver provided persistent device id
    std::string GetDeviceIdForPersistentDeviceId(const std::string& persistentDeviceId);

    /// \brief send a health issue to control plane
    bool SendHealthAlert(InmErrorAlertImp &a);

    /// \brief Remove Cx Health Issues 
    void RemoveCxHealthIssues(bool isDiskLevel);

    /// \brief Process Disk CX events. raise an alert/health issues as needd 
    bool VerifyDiskCxEvents(VM_CXFAILURE_STATS *ptrCxStats, bool& bHealthEventRequired);

    /// \brief Process VM CX events. raise an alert/health issues as needd 
    bool VerifyVMCxEvents(VM_CXFAILURE_STATS *ptrCxStats, bool& bHealthEventRequired);

    /// \brief Creates health issue map. 
    void CreateAgentHealthIssueMap(SourceAgentProtectionPairHealthIssues&   healthIssues);

    /// \brief Once health issues are sent to RCM, callee is called back. 
    void HealthIssueReported();
    
    /// \brief Remove VacpHealthIssues
    void RemoveVacpHealthIssues();

    /// \brief Remove ConsistencyTagHealthIssues
    void RemoveConsistencyTagHealthIssues(TagTelemetry::TagType& tagType);

    /// \brief Insert health issues from VM level health issues with given health code without duplication
    void InsertHealthIssues(const std::list<HealthIssue>& healthissues);

    /// \brief checks Azure blob location status
    void MonitorAzureBlobLogUploadLocation(const AgentSettings& settings, std::string& message);

    /// \brief checks Process Server log location status
    void MonitorProcessServerLogUploadLocation(const AgentSettings& settings, std::string& message);

    /// \brief updates the log upload location status map
    void UpdateLogUploadStatus(const std::string& id, const int status, std::string &message, bool bForce);

    /// \brief cleansup any stale log upload location status on settings change
    void CleanLogUploadStatus(const std::set<std::string>& ids);

    /// \brief Centrally Collate all the Health Issues from various components by reading .json files.
    bool CollateAllComponentsHealthIssues(SourceAgentProtectionPairHealthIssues& healthIssues);

    /// \brief get the health issues from a file
    bool GetHealthIssues(const std::string&strJsonHealthFile, SourceAgentProtectionPairHealthIssues& healthIssues);

    bool PopulateHealthIssues(const std::set<std::string>& setProtectedDiskIds,
        const std::set<std::string>& setResyncDiskIds,
        const SourceAgentProtectionPairHealthIssues& componentHealthIssues,
        SourceAgentProtectionPairHealthIssues& healthIssues);

    /// \brief gets the lists of protected disks which are in resync mode
    bool GetDevicesInResyncMode(std::set<std::string> &deviceIds);

    /// \brief monitor if any URL access has Root CA missing error 
    void MonitorUrlAccess();

    /// \brief monitor DiskIds set for resync are onboarded for resync
    void MonitorResyncBatching(std::vector<AgentDiskLevelHealthIssue>& diskHealthIssues);

    /// \brief update file attributes to remove world writable pemissions
    void UpdateFilePermissions();
};

#endif /* MONITORHOSTTHREAD_H */
