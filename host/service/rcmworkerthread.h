#ifndef _RCM_WORKER_THREAD_H
#define _RCM_WORKER_THREAD_H

#include <ace/Thread_Manager.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Event.h>
#include <ace/Message_Queue_T.h>

#include <boost/shared_ptr.hpp>
#include <boost/chrono.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

#include "sigslot.h"

#include "inmquitfunction.h"
#include "svstatus.h"
#include "AgentSettings.h"
#include "RcmJobs.h"
#include "CommitTag.h"
#include "dpdrivercomm.h"
#include "protectedgroupvolumes.h"
#include "volumegroupsettings.h"
#include "SrcTelemetry.h"
#include "protecteddevicedetails.h"
#include "AgentRcmLogging.h"
#include "FabricDetails.h"

using namespace boost::chrono;

using namespace RcmClientLib;
using namespace SrcTelemetry;
using namespace AzureInstanceMetadata;

typedef std::map<std::string, boost::function<void()> > HandlerMap_t;

namespace RcmWorkerThreadHandlerNames {
    const std::string SENTINELSTOP = "sentinelstop";
    const std::string VACPSTOP = "vacpstop";
    const std::string VACPRESTART = "vacprestart";
}

namespace ClientCert {
    const int CertRenewBufferInDays = 30;
    const int ThrottleLimitInDays = 1;
}

/// \brief a worker thread in service to complete the RCM jobs
class RcmWorkerThread : public sigslot::has_slots<>
{
public:
    RcmWorkerThread(HandlerMap_t& handlers);
    ~RcmWorkerThread();

    /// \brief starts the worker thread 
    void Start();

    /// \brief stops the worker thread by issuing quit signal
    void Stop();

    /// \brief waits for quit event for given seconds and
    ///  returns true if quit is set (even before timeout expires).
    bool QuitRequested(int seconds); ///< seconds

    /// \brief update the disk-ids for which the log upload has failed with auth error or not found error
    void UpdateLogUploadAccessFail(const std::string& diskId, const std::string& sasUri, const std::string& renewRequestType);

    sigslot::signal0<>& GetPrepareSwitchApplianceSignal();

    sigslot::signal1<RcmProxyTransportSetting&>& GetSwitchApplianceSignal();

protected:
   
    /// \breaf the entry point function for the thread
    static ACE_THR_FUNC_RETURN ThreadFunc(void* pArg);

    /// \brief thread run loop until quit
    ACE_THR_FUNC_RETURN run();

    /// \brief process a single job
    void ProcessJob(boost::shared_ptr<RcmJob> ptrJob);

    /// \brief post a SAS URI renew request to RCM
    void RenewSasUri(const std::string& diskId, const std::string& renewType);

    /// \brief ACE thread manager
    ACE_Thread_Manager m_threadManager;

private:

    /// \brief indicates if the thread already started
    bool m_started;

    /// \brief quit event
    boost::shared_ptr<ACE_Event> m_QuitEvent;

    /// \brief in-memory copy of the discovered volumes
    DataCacher::VolumesCache m_volumesCache;

    /// \brief persistent device details store
    boost::shared_ptr<VxProtectedDeviceDetailPeristentStore> m_ptrDevDetailsPersistentStore;

    /// \brief the log dir name for the agent
    std::string m_logDirName;

    /// \brief the agent install path name
    std::string m_installPath;

    /// \brief source telemetry poll interval
    uint64_t    m_SrcTelemeteryPollInterval;

    /// \brief source telemetry start delay
    uint64_t    m_SrcTelemeteryStartDelay;

    /// \brief log container renewal retry wait time in seconds
    uint32_t   m_logContainerRenewalRetryWaitInSecs;

    /// \brief flag that indicates if persistent device details are complete
    volatile bool m_protectedDeviceDetailsComplete;

    /// \brief driver stream handle
    DeviceStream::Ptr m_pDriverStream;

    /// \brief a callback to stop sentinel, vacp
    HandlerMap_t  m_handlers;

    /// \brief a job queue into which the received jobs are added and processed by worker threads
    std::map<std::string, boost::shared_ptr<RcmJob> >   m_jobQueue;

    /// \brief flag to check if the thread is active or quitting
    bool m_active;

    /// \brief a map of disk-id to last renew blob SAS URI failure time
    std::map<std::string, std::map<std::string, steady_clock::time_point> > m_renewUriFailTimes;

    /// \brief a lock to serialize SAS URI renew
    boost::mutex    m_renewUriMutex;

    /// \brief list of resync or diff-sync disk-ids for which a renew is in-progress
    std::map<std::string, std::set<std::string> >   m_renewUriProgressList;

    /// \brief list of resync or diff-sync <disk-ids, sasUri> tuple for which log upload monitor failed with auth error and not found signal
    std::map<std::string, std::map<std::string, std::string> >   m_renewUriAccessFailList;

    /// \brief a lock to serialize baseline bookmark job threads
    boost::mutex    m_baselineBookmarkMutex;

    /// \brief io_service for thread pool
    boost::asio::io_service m_ioService;

    /// \brief thread group
    boost::thread_group m_threadGroup;

    /// \brief a worker object to wait on while io_service has no pending jobs
    boost::asio::io_service::work   m_worker;

    /// \brief a config param for number of worker threads
    uint32_t m_numberOfWorkerThreads;

    TagCommitNotifier m_commitTag;

    /// \brief maximum allowed time for a job to run
    uint32_t m_maxAllowedTimeForJob;

    /// \brief holds a mapping of jobid to job queue time
    std::map<std::string, steady_clock::time_point> m_jobQueueTime;

    /// \brief signal for prepare switch appliance call back
    sigslot::signal0<> m_prepareSwitchApplianceSignal;

    /// \brief signal for switch appliance call back
    sigslot::signal1<RcmProxyTransportSetting&> m_switchApplianceSignal;

    /// \brief called to signal a quit internally
    void Quit();

    /// \brief call back for new job processing
    void ProcessJobsCallback(RcmJobs_t& jobs);

    /// \brief process loop for the RCM 
    void ProcessAgentSetting();

    /// \brief get the discovered volume report
    SVSTATUS GetVolumeReport(const std::string& deviceId,
        VolumeReporter::VolumeReport_t& vr,
        std::string& errMsg);

    /// \brief open the filter driver handle
    SVSTATUS CreateDriverStream();

    /// \brief check client cert expiry
    SVSTATUS CheckClientCertExpiry();

    /// \brief flushes device cache
    SVSTATUS FlushOnDeviceCache(std::string& deviceId, const VolumeSummaries_t *pvs);

    /// \brief processes the resync start notify job
    SVSTATUS SendResyncStartNotify(RcmJob& job);

    /// \brief processes the resync end notify job
    SVSTATUS SendResyncEndNotify(RcmJob& job);

    /// \brief stop filtering for a given disk
    SVSTATUS StopFiltering(RcmJob& job);

    /// \brief issue a baseline bookmark on a disk
    SVSTATUS IssueBaselineBookmark(RcmJob& job);

    /// \brief process prepare for IR/resync job
    SVSTATUS PrepareForSync(RcmJob& job);

    /// \brief Agent Upgrade job
    SVSTATUS AgentUpgrade(RcmJob& job);

    /// \brief Prepare for Switch Appliance
    SVSTATUS PrepareForSwitchAppliance(RcmJob& job);

    /// \brief Switch Appliance
    SVSTATUS SwitchAppliance(RcmJob& job);

    /// \brief Invokes the upgrader to start upgrade.
    SVSTATUS StartUpgrade(const std::string& binPath, 
        const std::vector<std::string> & args);

    /// \brief Recursively create directories
    SVSTATUS CreateDirectoriesWithRetries(const boost::filesystem::path & path);

    /// \brief Recursively delete directories
    SVSTATUS DeleteDirectoriesWithRetries(const boost::filesystem::path & path);

    /// \brief Monitor replication settings that required renew
    SVSTATUS MonitorReplicationSettings();

    // \brief monitors the resync settings SAS URI renew
    SVSTATUS MonitorSyncSettings(const SyncSettings_t& syncSettings);

    // \brief monitors the diff sync settings SAS URI renew
    SVSTATUS MonitorDrainSetting(const DrainSettings_t& drainSettings);

    /// \brief logs source telemetry data from the drain settings periodically
    void LogSourceReplicationStatus(const AgentSettings &settings);

    /// \brief logs source telemetry data of a protected disk
    void LogSourceReplicationStatus(const std::string& diskId, uint32_t reason, uint32_t numDiskProtected = 0);

    /// \brief fills the source telemetry with the system info like version, hostname, IP address etc.
    void FillReplicationStatusWithSystemInfo(ReplicationStatus &repStatus);

    /// \brief fills the source telemetry with the driver stats and status
    void FillReplicationStatusFromDriverStats(ReplicationStatus& repStatus,
        const std::string& deviceName);

    /// \brief fills the source telemetry with the discovered disk info 
    void FillReplicationStatusFromSettings(ReplicationStatus& repStatus,
        const std::string& diskId,
        std::string &deviceName,
        AgentLogExtendedProperties& extProps);

    void FillReplicationStatusFromOnPremSettings(ReplicationStatus& repStatus,
        const std::string& diskId);

    /// \brief fills the source telemetry with the fabric/hypervisor info like diskArcmId etc.
    void FillReplicationStatusWithFabricInfo(ReplicationStatus& repStatus,
        AgentLogExtendedProperties& extProps);

    /// \brief gets the storage profile from Azure Instance Metadata
    SVSTATUS GetAzureVmStorageProfile(StorageProfile& storegeProfile);

    /// \brief moves the protected device details file to driver persistent store location
    bool MoveProtectedDevDetailsCache();

    SVSTATUS CheckSpace(const boost::filesystem::path& agentDownloadPath,
        boost::filesystem::space_info &agentPathSpace);

    SVSTATUS DownloadAgent(const SourceAgentUpgradeInput& agentUpgradeInput, const boost::filesystem::path& agentDownloadPath, std::string& agentInstallPackagePath);

    bool IsAgentUnzipped(const boost::filesystem::path& agentDownloadPath);

    SVSTATUS UnzipAgentPackage(const boost::filesystem::path& agentDownloadPath, const boost::filesystem::path& agentInstallPackagePath);

    SVSTATUS SerializeJobFile(const boost::filesystem::path& serializedJobFilePath, RcmJob& job);

    SVSTATUS UpgradeAgent(const boost::filesystem::path& agentDownloadPath, const std::string& agentInstallPackageName, const boost::filesystem::path& serializedJobFilePath);

    /// \brief issues bookmark / tag by invoking VACP and wait for it to drain by driver
    SVSTATUS IssueFinalBookmark(boost::shared_ptr<RcmJob> ptrJob);

    SVSTATUS PersistALRConfig(ALRMachineConfig& alrMachineConfig, std::string& errMsg);

    void IssueCommitTagNotify(boost::shared_ptr<RcmJob> ptrJob);

    SVSTATUS SwitchBackToCsStack(RcmJob& job);

    /// \brief AVS DR supporting jobs
    SVSTATUS FailoverExtendedLocation(RcmJob& job);

    /// \brief unblocks draning and start uploading to appliance
    SVSTATUS CsStackMachineStartDraining(RcmJob& job);

#ifdef SV_WINDOWS
    /// \brief start tracking for all the shared disks
    SVSTATUS SourceAgentStartSharedDisksChangeTracking(RcmJob& job);
#endif

#ifdef SV_UNIX
    /// \brief updates the persistent name in Linux driver for a given device name when migrating from CS to RCM stack
    ///  ex : / dev / sda->_dev_sda to / dev / sda->RootDisk
    SVSTATUS UpdatePnames(const std::vector<std::string>& diskIdsFromRcm, std::string& errMsg);
#endif

    ///\ brief validate input parameters to FailoverExtendedLocation Rcm job
    void ValidateFailoverExtendedLocationArguments(std::string& errMsg, const RcmClientLib::SourceAgentFailoverExtendedLocationJobInputBase& input);

    ///\ brief perform tasks before processing jobs from rcm. 
    SVSTATUS PerformMigratePreJobProcesssingTasks();

};

#endif
