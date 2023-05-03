#ifndef MONITORHOSTTHREAD_H
#define MONITORHOSTTHREAD_H

#include <ace/Thread_Manager.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Event.h>

#include <boost/shared_ptr.hpp>

#include "inmquitfunction.h"

#include "sigslot.h"
#include "rpcconfigurator.h"
#include "devicestream.h"
#include "inmalertdefs.h"

#include "filterdrvifmajor.h"
#include "involfltfeatures.h"

#include "AgentHealthContract.h"
#include "consistencythread.h"

#define TEST_FLAG(_a, _f)       (((_a) & (_f)) == (_f))

class MonitorHostThread : public sigslot::has_slots<>
{
public:
    MonitorHostThread(Configurator* configurator, const unsigned int monitorHostInterval = 43200);
    ~MonitorHostThread();
    void Start();
    void Stop();

    /// \brief waits for quit event for given seconds and returns true if quit is set (even before timeout expires).
    bool QuitRequested(int seconds); ///< seconds

    void MonitorLogUploadLocation(const std::string& ip, const std::string& port, std::string& message);

    /// \brief call back for the settings change by configurator
    void    OnSettingsChange(HOST_VOLUME_GROUP_SETTINGS);

    void    SetLastTagState(TagTelemetry::TagType tagType, bool bSuccess, const std::list<HealthIssue>&vacpHealthIssues);

    void    CreateAgentHealthIssueMap(SourceAgentProtectionPairHealthIssues&   healthIssues);
    void    GetDisksInDiscovery(std::set<std::string>& disksInDiscoverySet);

    void    UpdateMissingDiskList(std::vector<AgentDiskLevelHealthIssue>& diskHealthIssues);
    bool    IsDriverMissing(std::string &msg);
    void    UpdateLogUploadLocationStatus(std::vector<HealthIssue>& healthIssues);

protected:
    ACE_Thread_Manager m_threadManager;
    static ACE_THR_FUNC_RETURN ThreadFunc(void* pArg);
    static ACE_THR_FUNC_RETURN CxStatsThreadFunc(void* pArg);

    ACE_THR_FUNC_RETURN run();
    void MonitorHost(QuitFunction_t qf = 0);
    void ExecCmd(const std::string &cmd, const std::string &tmpOpFile);
    void VerifyMonitoringEvents();


private:
    Configurator*                   m_configurator;
    boost::shared_ptr<ACE_Event>    m_QuitEvent;
    bool                            m_started;
    ACE_Mutex                       m_MonitorHostMutex; //Provide mutual exclusive access to MonitorHost() calls
    ACE_Mutex                       m_agentHealthMutex;
    std::map<FilterDrvVol_t, std::string>      m_drvDevIdtoProtectedIdMap;
    const unsigned int              m_MonitorHostInterval;
    uint32_t                        m_agentHealthCheckInterval;
    std::set<std::string>           m_monitorDeviceIds;

    /// \brief driver stream handle
    DeviceStream::Ptr m_pDriverStream;

    /// \brief list of protected disks to be monitored for cx events
    uint32_t                    m_CxClearHealthIssueCounter;

    std::string m_hostId;

    /// \brief serializes access protectedDeviceIds
    boost::mutex            m_mutexMonitorDeviceIds;

    /// \brief flag indicating if the cx session supported by filter driver
    bool m_bFilterDriverSupportsCxSession;

    ACE_Mutex   m_diskHealthMutex;
    ACE_Mutex   m_vmHealthMutex;

    std::list<HealthIssue>                  m_vmHealthIssues;
    std::list<AgentDiskLevelHealthIssue>    m_diskHealthIssues;
    std::string                             m_strHealthCollatorPath;

    /// \brief open the filter driver handle
    SVSTATUS CreateDriverStream();

    /// \brief monitor the cx envents raised by driver
    ACE_THR_FUNC_RETURN MonitorCxEvents();

    /// \brief fetches the protected device ids from the settings
    SVSTATUS GetProtectedDeviceIds(std::set<std::string> &deviceIds);

    /// \brief returns the persistent device id for the filter driver
    std::string GetPersistentDeviceIdForDriverInput(const std::string& deviceId);

    /// \brief returns the device id from the filter driver provided persistent device id
    std::string GetDeviceIdForPersistentDeviceId(const std::string& persistentDeviceId);

    /// \brief send a health issue to control plane
    bool SendHealthAlert(InmErrorAlertImp &a);

    /// \brief Process Disk CX events. raise an alert/health issues as needd 
    bool VerifyDiskCxEvents(VM_CXFAILURE_STATS *ptrCxStats, bool& bHealthEventRequired);

    /// \brief Process VM CX events. raise an alert/health issues as needed 
    bool VerifyVMCxEvents(VM_CXFAILURE_STATS *ptrCxStats, bool& bHealthEventRequired);

    /// \brief Remove Cx Health Issues 
    void RemoveCxHealthIssues(bool isDiskLevel);

    /// \brief Insert health issues from VM level health issues with given health code without duplication
    void InsertHealthIssues(const std::list<HealthIssue>& healthissues);

    void HealthIssueReported();

    /// \brief Remove VacpHealthIssues
    void RemoveVacpHealthIssues();

    /// \brief Remove ConsistencyTagFailureHealthIssues
    void RemoveConsistencyTagHealthIssues(TagTelemetry::TagType& tagType);

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

    /// \brief update file attributes to remove world writable pemissions
    void UpdateFilePermissions();
};

#endif /* MONITORHOSTTHREAD_H */
