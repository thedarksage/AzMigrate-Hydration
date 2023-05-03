#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "rcmworkerthread.h"
#include "logger.h"
#include "inmquitfunction.h"
#include "appcommand.h"
#include "volumereporterimp.h"
#include "securityutils.h"
#include "SrcTelemetry.h"

#include "devicestream.h"
#include "filterdrvifmajor.h"
#include "devicefilter.h"
#include "portablehelpersmajor.h"
#include "host.h"
#include "inmageproduct.h"

#include "setpermissions.h"
#include "LogCutter.h"
#include "VacpErrorCodes.h"
#include "resthelper/CloudBlob.h"
#include "proxysettings.h"
#include "gencert.h"
#include "RcmConfigurator.h"
#include "RcmLibConstants.h"

#ifdef SV_UNIX
#include "executecommand.h"
#include "devicefilter.h"
#include "involfltfeatures.h"
#include "ServiceHelper.h"
#else
#include <boost/process.hpp>
#include "indskfltfeatures.h"
#endif

using namespace AzureStorageRest;
using namespace Logger;

Log gSrcTelemetryLog;

using namespace RcmClientLib;
using namespace AzureInstanceMetadata;

#ifdef SV_WINDOWS
#define DRIVER_SUPPORTS_EXTENDED_VOLUME_STATS           1
#elif SV_UNIX
#define DRIVER_SUPPORTS_EXTENDED_VOLUME_STATS           0x8
#define DRIVER_PRESISTENT_DIR                           "/etc/vxagent/involflt"
#define BITMAP_PREFIX                                   "InMage-"
#define BITMAP_SUFFIX                                   ".VolumeLog"
#endif

#define MAX_NUMBER_OF_WORKER_THREADS                    8

extern void GetNicInfos(NicInfos_t &nicinfos);

static const std::string strDataDisk("DataDisk");
static const std::string strBootDisk("/dev/");
static const std::string strRootDisk("RootDisk");

const std::string PACKAGE("package");
const std::string JOBS("jobs");

#define RCM_WORKER_THREAD_CATCH_EXCEPTION(errMsg, status) \
    catch (const std::exception& ex) \
        { \
        errMsg += ex.what(); \
        status = SVE_ABORT; \
        } \
    catch (const char *msg) \
        { \
        errMsg += msg; \
        status = SVE_ABORT; \
        } \
    catch (const std::string &msg) \
        { \
        errMsg += msg; \
        status = SVE_ABORT; \
        } \
    catch (...) \
        { \
        errMsg += "unknown error"; \
        status = SVE_ABORT; \
        }

RcmWorkerThread::RcmWorkerThread(HandlerMap_t& handlers)
    :m_started(false),
    m_active(false),
    m_QuitEvent(new ACE_Event(1, 0)),
    m_protectedDeviceDetailsComplete(false),
    m_handlers(handlers),
    m_ioService(),
    m_worker(m_ioService),
    m_numberOfWorkerThreads(0),
    m_commitTag()
{
    if (m_handlers.end() == m_handlers.find(RcmWorkerThreadHandlerNames::SENTINELSTOP) ||
        m_handlers.end() == m_handlers.find(RcmWorkerThreadHandlerNames::VACPSTOP) ||
        m_handlers.end() == m_handlers.find(RcmWorkerThreadHandlerNames::VACPRESTART))
    {
        throw INMAGE_EX("Required handlers not provided");
    }
}

RcmWorkerThread::~RcmWorkerThread()
{
}

/// \brief starts the worker thread
void RcmWorkerThread::Start()
{
    if (m_started)
    {
        return;
    }
    m_active = true;
    m_QuitEvent->reset();
    m_threadManager.spawn(ThreadFunc, this);
    m_started = true;
}

/// \brief thread entry point
ACE_THR_FUNC_RETURN RcmWorkerThread::ThreadFunc(void* arg)
{
    RcmWorkerThread* p = static_cast<RcmWorkerThread*>(arg);
    return p->run();
}

/// \brief loops until quit. fetches RCM settings every 90 sec
ACE_THR_FUNC_RETURN RcmWorkerThread::run()
{
    do
    {
        SVSTATUS status = SVS_OK;

        try
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s : starting RCM worker thread.\n", FUNCTION_NAME);

            boost::shared_ptr<Logger::LogCutter> telemetryLogCutter(new Logger::LogCutter(gSrcTelemetryLog));

            LocalConfigurator lc;
            std::string logPath;
            if (GetLogFilePath(logPath))
            {
                boost::filesystem::path srcLogPath(logPath);
                srcLogPath /= "srcTelemetry.log";
                gSrcTelemetryLog.m_logFileName = srcLogPath.string();

                const  int maxCompletedLogFiles = lc.getLogMaxCompletedFiles();
                const boost::chrono::seconds cutInterval(lc.getLogCutInterval());
                const uint32_t logFileSize = lc.getLogMaxFileSizeForTelemetry();
                gSrcTelemetryLog.SetLogFileSize(logFileSize);

                telemetryLogCutter->Initialize(srcLogPath, maxCompletedLogFiles, cutInterval);
                telemetryLogCutter->StartCutting();
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s unable to init src telemetry logger\n", FUNCTION_NAME);
            }

            m_SrcTelemeteryPollInterval = lc.getSrcTelemetryPollInterval();
            m_SrcTelemeteryStartDelay = lc.getSrcTelemetryStartDelay();
            m_logContainerRenewalRetryWaitInSecs = lc.getLogContainerRenewalRetryTimeInSecs();
            m_numberOfWorkerThreads = lc.getRcmJobWorkerThreadCount();

            // for A2A, the max time could be no less than couple of mins
            // for V2A RCM failback, it should be more than getVacpTagCommitMaxTimeOut() for the
            // final bookmark job to complete.
            // for V2A RCM, the agent upgrade job is to be considered and using same as failback
            const uint32_t  minAllowedTimeoutForJob = 120;
            m_maxAllowedTimeForJob = lc.getRcmJobMaxAllowedTimeInSec();
            if (lc.IsAzureToAzureReplication())
            {
                if (m_maxAllowedTimeForJob < minAllowedTimeoutForJob)
                    m_maxAllowedTimeForJob = minAllowedTimeoutForJob;
            }
            else
            {
                if (m_maxAllowedTimeForJob < lc.getVacpTagCommitMaxTimeOut())
                    m_maxAllowedTimeForJob = lc.getVacpTagCommitMaxTimeOut() + minAllowedTimeoutForJob;
            }

            m_installPath = lc.getInstallPath();
#ifdef SV_WINDOWS
            m_logDirName = lc.getCacheDirectory();
#else
            m_logDirName = lc.getLogPathname();
#endif

            ACE_Time_Value moveWaitTime(10, 0);
            while (!MoveProtectedDevDetailsCache())
            {
                m_QuitEvent->wait(&moveWaitTime, 0);
                if (m_threadManager.testcancel(ACE_Thread::self()))
                {
                    gSrcTelemetryLog.CloseLogFile();
                    return 0;
                }
            }
            
            m_ptrDevDetailsPersistentStore.reset(new VxProtectedDeviceDetailPeristentStore);
            ACE_Time_Value waitTime(90, 0); // TBD: use config param

            do
            {
                if (m_ptrDevDetailsPersistentStore->Init())
                {
                    break;
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to get persistent device details from cache file.\n");
                }

                m_QuitEvent->wait(&waitTime, 0);

            } while (!m_threadManager.testcancel(ACE_Thread::self()));

            if (!m_numberOfWorkerThreads)
                m_numberOfWorkerThreads = 1;
            else if (m_numberOfWorkerThreads > MAX_NUMBER_OF_WORKER_THREADS)
                m_numberOfWorkerThreads = MAX_NUMBER_OF_WORKER_THREADS;

            for (int i = 0; i < m_numberOfWorkerThreads; ++i)
            {
                m_threadGroup.create_thread(boost::bind(&boost::asio::io_service::run,
                    &m_ioService));
            }

            DebugPrintf(SV_LOG_ALWAYS, "%s: created %d worker threads\n", FUNCTION_NAME, m_threadGroup.size());

            if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication()
                && (RcmConfigurator::getInstance()->getMigrationState() == Migration::MIGRATION_SWITCHED_TO_RCM_PENDING))
            {
                status = PerformMigratePreJobProcesssingTasks();
                if (status == SVE_ABORT)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s migration aborting on quit signal. \n", FUNCTION_NAME);
                    break;
                }
                else if (status != SVS_OK)
                {
                    RcmConfigurator::getInstance()->setMigrationState(Migration::MIGRATION_ROLLBACK_PENDING);
                    DebugPrintf(SV_LOG_ERROR, "%s: migration marked fail. migration state=%d \n", FUNCTION_NAME, RcmConfigurator::getInstance()->getMigrationState());
                    break;
                }
                RcmConfigurator::getInstance()->setMigrationState(Migration::MIGRATION_SWITCHED_TO_RCM);
                DebugPrintf(SV_LOG_ALWAYS, "%s: migration marked success. migration state=%d\n", FUNCTION_NAME, 
                    RcmConfigurator::getInstance()->getMigrationState());
            }

            RcmConfigurator::getInstance()->GetJobsChangedSignal().connect(this, &RcmWorkerThread::ProcessJobsCallback);

            while (!m_threadManager.testcancel(ACE_Thread::self()))
            {
                ProcessAgentSetting();
                m_QuitEvent->wait(&waitTime, 0);
            }

            DebugPrintf(SV_LOG_ALWAYS, "%s: qutting RCM worker thread.\n", FUNCTION_NAME);

            m_ioService.stop();

            m_threadGroup.join_all();

        }
        catch (std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s : caught exception %s. restarting RCM worker thread.\n",
                FUNCTION_NAME,
                e.what());

            status = SVE_ABORT;
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s : caught unknown exception. restarting RCM worker thread.\n",
                FUNCTION_NAME);

            status = SVE_ABORT;
        }

        if (status == SVE_ABORT)
        {
            const time_t waitTimeInSec = 60;
            DebugPrintf(SV_LOG_ALWAYS, "RCM worker thread waiting %d sec on exception.\n", waitTimeInSec);
            ACE_Time_Value waitTime(waitTimeInSec, 0);
            m_QuitEvent->wait(&waitTime, 0);
        }

    } while (!m_threadManager.testcancel(ACE_Thread::self()));

    gSrcTelemetryLog.CloseLogFile();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return 0;
}

/// \brief fetches and processes the RCM jobs 
void RcmWorkerThread::ProcessJobsCallback(RcmJobs_t& receivedJobs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    uint32_t numNewJobs = 0;

    std::vector<RcmJob>::iterator itJob = receivedJobs.begin();
    for (/*empty*/; itJob != receivedJobs.end(); itJob++)
    {
        if (m_jobQueue.find(itJob->Id) == m_jobQueue.end())
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: queuing new job %s for processing.\n", FUNCTION_NAME, itJob->Id.c_str());
            boost::shared_ptr<RcmJob> ptrJob(new RcmJob(*itJob));
            ptrJob->JobStatus = RcmJobStatus::NONE;
            ptrJob->JobState = RcmJobState::QUEUED;
            m_jobQueue[itJob->Id] = ptrJob;
            m_jobQueueTime[itJob->Id] = steady_clock::now();
            m_ioService.post(boost::bind(&RcmWorkerThread::ProcessJob, this, ptrJob));
            numNewJobs++;
        }
        else if ((m_jobQueue[itJob->Id]->JobStatus != RcmJobStatus::NONE) || 
            (m_jobQueue[itJob->Id]->JobState == RcmJobState::SKIPPED))
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: removing job %s from job queue on completion\n",
                FUNCTION_NAME,
                itJob->Id.c_str());

            m_jobQueue.erase(itJob->Id);
            m_jobQueueTime.erase(itJob->Id);
        }
    }

    std::map<std::string, boost::shared_ptr<RcmJob> >::iterator iterQueue = m_jobQueue.begin();
    while (iterQueue != m_jobQueue.end())
    {
        if ((iterQueue->second->JobStatus != RcmJobStatus::NONE) ||
            (iterQueue->second->JobState == RcmJobState::SKIPPED))
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: removing job %s from job queue on completion\n",
                FUNCTION_NAME,
                iterQueue->first.c_str());

            m_jobQueueTime.erase(iterQueue->first);
            m_jobQueue.erase(iterQueue++);
        }
        else
        {
            if (steady_clock::now() > m_jobQueueTime[iterQueue->first] + seconds(m_maxAllowedTimeForJob))
            {
                DebugPrintf(SV_LOG_FATAL, "%s: job id %s of type %s status %s state %s is still not completed after %d secs. Exiting service for restart.\n",
                    FUNCTION_NAME,
                    iterQueue->first.c_str(),
                    iterQueue->second->JobType.c_str(),
                    iterQueue->second->JobStatus.c_str(),
                    iterQueue->second->JobState.c_str(),
                    m_maxAllowedTimeForJob);

                // wait for the log to be flushed for debugging
                ACE_OS::sleep(10);
                ACE_OS::abort();
            }

            DebugPrintf(SV_LOG_DEBUG, "%s: job %s is still not completed.\n",
                FUNCTION_NAME,
                iterQueue->first.c_str());

            iterQueue++;
        }
    }

    if (numNewJobs)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: pending jobs %d received jobs %d new jobs %d.\n",
            FUNCTION_NAME,
            m_jobQueue.size(),
            receivedJobs.size(),
            numNewJobs);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return;
}

void RcmWorkerThread::ProcessJob(boost::shared_ptr<RcmJob> ptrJob)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    ptrJob->JobState = RcmJobState::PROCESSING;

    if (ptrJob->JobType == RcmJobTypes::RESYNC_START_TIMESTAMP)
    {
        status = SendResyncStartNotify(*ptrJob);
    }
    else if (ptrJob->JobType == RcmJobTypes::RESYNC_END_TIMESTAMP)
    {
        status = SendResyncEndNotify(*ptrJob);
    }
    else if ((ptrJob->JobType == RcmJobTypes::STOP_TRACKING) ||
        (ptrJob->JobType == RcmJobTypes::ON_PREM_TO_AZURE_STOP_TRACKING) ||
        (ptrJob->JobType == RcmJobTypes::AZURE_TO_ONPREM_STOP_FILTERING))
    {
        status = StopFiltering(*ptrJob);
    }
    else if(ptrJob->JobType == RcmJobTypes::ISSUE_BASELINE_BOOKMARK)
    {
        status = IssueBaselineBookmark(*ptrJob);
    }
    else if ((ptrJob->JobType == RcmJobTypes::ON_PREM_TO_AZURE_PREPARE_FOR_SYNC) ||
        (ptrJob->JobType == RcmJobTypes::AZURE_TO_ONPREM_PREPARE_FOR_SYNC))
    {
        status = PrepareForSync(*ptrJob);
    }
    else if ((ptrJob->JobType == RcmJobTypes::ON_PREM_TO_AZURE_AGENT_AUTO_UPGRADE) ||
        (ptrJob->JobType == RcmJobTypes::ON_PREM_TO_AZURE_AGENT_USER_TRIGGERED_UPGRADE) ||
        (ptrJob->JobType == RcmJobTypes::AZURE_TO_ONPREM_AGENT_AUTO_UPGRADE) ||
        (ptrJob->JobType == RcmJobTypes::AZURE_TO_ONPREM_AGENT_USER_TRIGGERED_AUTO_UPGRADE))
    {
        status = AgentUpgrade(*ptrJob);
    }
    else if (ptrJob->JobType == RcmJobTypes::AZURE_TO_ONPREM_FINAL_BOOKMARK)
    {
        status = IssueFinalBookmark(ptrJob);
    }
    else if (ptrJob->JobType == RcmJobTypes::ON_PREM_TO_AZURE_AGENT_PREPARE_FOR_SWITCH_APPLIANCE)
    {
        status = PrepareForSwitchAppliance(*ptrJob);
    }
    else if (ptrJob->JobType == RcmJobTypes::ON_PREM_TO_AZURE_AGENT_SWITCH_APPLIANCE)
    {
        status = SwitchAppliance(*ptrJob);
    }
    else if (ptrJob->JobType == RcmJobTypes::ON_PREM_TO_AZURE_SWITCH_BACK_TO_CS_STACK)
    {
        status = SwitchBackToCsStack(*ptrJob);
    }
    else if (ptrJob->JobType == RcmJobTypes::ON_PREM_TO_AZURE_CS_STACK_MACHINE_START_DRAINING)
    {
        status = CsStackMachineStartDraining(*ptrJob);
    }
    else if ((ptrJob->JobType == RcmJobTypes::ONPREM_TO_AZUREAVS_FAILOVER_EXTENDED_LOCATION) ||
        (ptrJob->JobType == RcmJobTypes::AZUREAVS_TO_ONPREM_FAILOVER_EXTENDED_LOCATION))
    {
        status = FailoverExtendedLocation(*ptrJob);
    }
#ifdef SV_WINDOWS
    else if (ptrJob->JobType == RcmJobTypes::SHARED_DISK_START_TRACKING)
    {
        status = SourceAgentStartSharedDisksChangeTracking(*ptrJob);
    }
#endif
    else
    {
        std::string errMsg("Job type is unknown.");

        DebugPrintf(SV_LOG_ERROR,
            "failed to process unknown job : Job Type %s Id %s\n",
            ptrJob->JobType.c_str(),
            ptrJob->Id.c_str());

        ptrJob->JobStatus = RcmJobStatus::FAILED;
        RcmJobError jobError(
            boost::lexical_cast<std::string>(RcmClientLib::RCM_CLIENT_JOB_UNKNOWN_ERROR),
            errMsg,
            std::string(),
            std::string(),
            RcmJobSeverity::RCM_ERROR);

        ptrJob->Errors.push_back(jobError);
    }

    ptrJob->JobState = RcmJobState::PROCESSED;

    if (ptrJob->JobStatus != RcmJobStatus::NONE)
    {
        ptrJob->JobState = RcmJobState::COMPLETING;

        status = RcmConfigurator::getInstance()->GetRcmClient()->UpdateAgentJobStatus(*ptrJob);

        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s failed to update job status : Job Id %s Input %s\n",
                FUNCTION_NAME,
                ptrJob->Id.c_str(),
                ptrJob->InputPayload.c_str());
        }
        else
        {
            ptrJob->JobState = RcmJobState::COMPLETED;
            DebugPrintf(SV_LOG_ALWAYS, "%s: completed job Id %s Type %s.\n",
                FUNCTION_NAME,
                ptrJob->Id.c_str(),
                ptrJob->JobType.c_str());
        }
    }
    else {
        DebugPrintf(SV_LOG_ALWAYS, "Skipping Job: %s\n", ptrJob->Id.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return;
}

/// \brief stops the worker thread
void RcmWorkerThread::Stop()
{
    ACE_Time_Value waitTime(10, 0);

    m_active = false;
    m_threadManager.cancel_all();
    m_QuitEvent->signal();

    do {
        m_commitTag.CancelCommitTagNotify();
    } while (m_threadManager.wait(&waitTime, false, false));

    m_started = false;
    return;
}

/// \brief stops the worker thread
void RcmWorkerThread::Quit()
{
    m_threadManager.cancel_all();
    m_QuitEvent->signal();
    return;
}

/// \brief this function is called by the service main thread when the service is stopping
bool RcmWorkerThread::QuitRequested(int seconds)
{
    ACE_Time_Value waitTime(seconds, 0);
    m_QuitEvent->wait(&waitTime, 0);
    return m_threadManager.testcancel(ACE_Thread::self());
}

void RcmWorkerThread::UpdateLogUploadAccessFail(const std::string & diskId, const std::string& sasUri, const std::string& renewRequestType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    DebugPrintf(SV_LOG_ALWAYS, "%s: diskid %s, renew request type %s.\n", FUNCTION_NAME, diskId.c_str(), renewRequestType.c_str());

    boost::mutex::scoped_lock guard(m_renewUriMutex);
    // todo - until the resync cache location monitoring is added, assume all failures are diff sync only
    if (boost::iequals(renewRequestType, RenewRequestTypes::DiffSyncContainer)) {
        m_renewUriAccessFailList[RenewRequestTypes::DiffSyncContainer].insert(std::make_pair(diskId, sasUri));
    }
    else if (boost::iequals(renewRequestType, RenewRequestTypes::ResyncContainer)) {
        m_renewUriAccessFailList[RenewRequestTypes::ResyncContainer].insert(std::make_pair(diskId, sasUri));
    }
    else {
        DebugPrintf(SV_LOG_ERROR, "%s: unsupported renew request type %s.\n", FUNCTION_NAME, renewRequestType.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

SVSTATUS RcmWorkerThread::CreateDriverStream()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;

    FilterDrvIfMajor drvif;
    std::string driverName = drvif.GetDriverName(VolumeSummary::DISK);

    if (!m_pDriverStream) {
        try {
            m_pDriverStream.reset(new DeviceStream(Device(driverName)));
            DebugPrintf(SV_LOG_DEBUG, "Created device stream.\n");
            m_commitTag.Init(m_pDriverStream);
        }
        catch (std::bad_alloc &e) {
            std::string errMsg("Memory allocation failed for creating driver stream object with exception: ");
            errMsg += e.what();
            DebugPrintf(SV_LOG_ERROR, "%s : error %s.\n", FUNCTION_NAME, errMsg.c_str());
            status = SVE_OUTOFMEMORY;
        }
    }

    //Open device stream in asynchrnous mode if needed
    if (m_pDriverStream && !m_pDriverStream->IsOpen()) {
        if (SV_SUCCESS == m_pDriverStream->Open(DeviceStream::Mode_Open | DeviceStream::Mode_Asynchronous | DeviceStream::Mode_RW | DeviceStream::Mode_ShareRW))
            DebugPrintf(SV_LOG_DEBUG, "Opened driver %s in async mode.\n", driverName.c_str());
        else
        {
            std::string errMsg("Failed to open driver");
            DebugPrintf(SV_LOG_ERROR, "%s : error %s %d.\n",
                FUNCTION_NAME, errMsg.c_str(), m_pDriverStream->GetErrorCode());
            status = SVE_FILE_OPEN_FAILED;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

void RcmWorkerThread::ProcessAgentSetting()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try {
       
        SVSTATUS status = SVS_OK;
        status = CreateDriverStream();
        if (status != SVS_OK)
        {
            return;
        }

        std::string err;
        if (!DataCacher::UpdateVolumesCache(m_volumesCache, err))
        {
            DebugPrintf(SV_LOG_INFO, "%s: update volume cache failed. Error %s.\n",
                FUNCTION_NAME,
                err.c_str());

            return;
        }

        if (SVS_OK != MonitorReplicationSettings())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s : MonitorReplicationSettings failed\n", FUNCTION_NAME);
        }
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s : caught exception %s.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s : caught unknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

/// \brief gets the volume report for the given device Id from discovery info
/// \returns
/// \li SVS_OK on success
/// \li SVE_INVALIDARG on failure to find the volume in volume cache
/// \li SVE_FAIL if data cacher can't be initialized
SVSTATUS RcmWorkerThread::GetVolumeReport(const std::string& deviceId,
                                            VolumeReporter::VolumeReport_t & vr,
                                            std::string& errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;

    do {

        std::string err;
        if (!DataCacher::UpdateVolumesCache(m_volumesCache, err))
        {
            DebugPrintf(SV_LOG_INFO, "%s: update volume cache failed. Error %s",
                FUNCTION_NAME,
                err.c_str());

            status = SVE_FAIL;
            break;
        }

        /// get the native os disks (not multipath)
        /// there could be multiple entries on a physical machine
        /// on Linux, any one device is good to filter all devices. requires scsi-id be reported
        /// on Windows only one device is reported 
        VolumeReporterImp vrimp;
        VolumeReporter::DiskReports_t drs;
#ifdef SV_WINDOWS
        vrimp.GetOsDiskReport(deviceId, m_volumesCache.m_Vs, drs);
#else
        if (boost::starts_with(deviceId, strBootDisk) ||
            boost::starts_with(deviceId, strRootDisk))
        {
            vrimp.GetBootDiskReport(m_volumesCache.m_Vs, drs);
        }
        else if (boost::starts_with(deviceId, strDataDisk))
        {
            vrimp.GetOsDiskReport(deviceId, m_volumesCache.m_Vs, drs);
        }
        else
        {
            status = SVE_INVALIDARG;
            errMsg += "Invalid disk id format for " + deviceId + ". Expecting /dev/<name> or DataDisk<num>.";
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            break;
        }
#endif
        if (drs.size() != 1) {
            status = SVE_INVALIDARG;
            errMsg = "Found " + boost::lexical_cast<std::string>(drs.size());
            errMsg += " disks for id " + deviceId + ".";
            DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.c_str());
            break;
        }

        vrimp.PrintDiskReports(drs);
        
        /// now we have the device name for the device id we want to filter
        /// get the volume report
        vr.m_ReportLevel = VolumeReporter::ReportVolumeLevel;
        vrimp.GetVolumeReport(drs[0].m_Name, m_volumesCache.m_Vs, vr);

        if (!vr.m_bIsReported)
        {
            status = SVE_INVALIDARG;
            errMsg += "Failed to find disk " + deviceId + " in volume cache file";
            DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.c_str());
            break;
        }

        vrimp.PrintVolumeReport(vr);

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

/// \brief flushes on device cache
/// \returns
/// \li SVS_OK on success
/// \li SVE_FILE_OPEN_FAILED if device open fails
/// \li SVE_FAIL if flush fails
SVSTATUS RcmWorkerThread::FlushOnDeviceCache(std::string& deviceId, const VolumeSummaries_t *pvs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS            Status = SVS_OK;
    std::vector<char>   buffer;
    SV_ULONG            ulBufferSize = 4 * 1024;

    // Fluh Blob cache
    BasicIo::BioOpenMode openMode;
    openMode = BasicIo::BioRWExisitng | BasicIo::BioShareAll | BasicIo::BioBinary | BasicIo::BioSequentialAccess;

#ifdef SV_WINDOWS
    openMode |= (BasicIo::BioNoBuffer | BasicIo::BioWriteThrough);
#else
    openMode |= BasicIo::BioDirect;
#endif

    cdp_volume_t::Ptr volume(new cdp_volume_t(deviceId, false, cdp_volume_t::GetCdpVolumeTypeForDisk(), pvs));

    volume->Open(openMode);
    if (!volume->Good())
    {
        DebugPrintf(SV_LOG_ERROR,
            "OnDisk cache flush: failed to open disk  (%s) error=%d\n",
            deviceId.c_str(),
            volume->LastError()
            );
        return SVE_FILE_OPEN_FAILED;
    }

    if (!volume->FlushFileBuffers()){
        DebugPrintf(SV_LOG_ERROR,
            "failed to Flush On Device Cache for disk  %s error=%d\n",
            deviceId.c_str(),
            volume->LastError()
            );

        Status = SVE_FAIL;
#ifdef SV_WINDOWS
        // As per FS team NTFS flushes its cache if first 4K is read from disk
        // with WRITE_THROUGH and NO_BUFFERING attributes
        buffer.resize(ulBufferSize);
        volume->Seek(0, BasicIo::BioBeg);
        if (!volume->Read(&buffer[0], ulBufferSize)) {
            DebugPrintf(SV_LOG_ERROR,
                "Flush OnDevice Cache: failed to read first 4K from disk  %s error=%d\n",
                deviceId.c_str(),
                volume->LastError()
                );
        }
        else {
            DebugPrintf(SV_LOG_DEBUG,
                "Flush OnDevice Cache: Read first 4K from disk  %s\n",
                deviceId.c_str()
                );
            Status = SVS_OK;
        }
#endif
    }
    else {
        DebugPrintf(SV_LOG_INFO,
            "Successfully Flushed On Device Cache for disk  %s\n",
            deviceId.c_str()
            );
    }

    volume->Close();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return Status;
}

/// \brief process the resync start notify job and post the status to RCM
/// \returns
/// \li SVS_OK on success
/// \li SVE_FAIL if ioctl fails
/// \li SVE_ABORT if caught exception
SVSTATUS RcmWorkerThread::SendResyncStartNotify(RcmJob& job)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg, diskId, deviceName;
    uint32_t reason = 0;
    uint32_t jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_RESYNC_START_NOTIFY_ERROR;
    std::string jobErrorMessage = "Failed to get sync start time";
    std::string possibleCauses = "Failed to process the resync start settings.";
    std::string recmAction = "Contact support if the issue persists.";

    try {
        do {
            
            if (!m_pDriverStream->IsOpen())
            {
                possibleCauses = "The data change tracking module of the mobility service is not loaded on the VM.";
                recmAction = "The kernel version of the running operating system kernel on the source machine is not supported on the version of the mobility service installed on the source machine.";
                errMsg = "Filter driver is not initialized.";
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_NOT_INIT_ERROR;
                status = SVE_FAIL;
                break;
            }

            std::string strInput = securitylib::base64Decode(job.InputPayload.c_str(), 
                                                                job.InputPayload.length());
            ResyncStartNotifyInput resyncInput = JSON::consumer<ResyncStartNotifyInput>::convert(strInput);

            VolumeReporter::VolumeReport_t vr;
            status = GetVolumeReport(resyncInput.SourceDiskId, vr, errMsg);
            if (status != SVS_OK)
            {
                possibleCauses = "The disk was either detached from the virtual machine or the disk signature might have been changed.";
                recmAction = "If the disk was detached from the virtual machine, attach the disk and retry resynchronization.";
                recmAction += " If the disk signature was changed, disable and enable replication for the virtual machine.";
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DEVICE_NOT_FOUND_ERROR;
                break;
            }

            diskId = resyncInput.SourceDiskId;
            deviceName = vr.m_DeviceName;

#ifdef _WIN32
            DpDriverComm ddc(resyncInput.SourceDiskId, vr.m_VolumeType);
#else
            DpDriverComm ddc(vr.m_DeviceName, vr.m_VolumeType, resyncInput.SourceDiskId);
#endif
            ddc.StartFiltering(vr, &m_volumesCache.m_Vs);
            reason = LOG_ON_START_FLT;

            if (resyncInput.DeleteAccumulatedChanges)
            {
                ddc.ClearDifferentials();
                reason |= LOG_ON_CLEAR_DIFFS;
            }

            ResyncTimeSettings resyncTime;
            SV_ULONG           ulError;
            if (!ddc.ResyncStartNotify(resyncTime, ulError))
            {
#ifdef SV_WINDOWS
                if (ERROR_FILE_OFFLINE == ulError) {
                    job.JobStatus = RcmJobStatus::NONE;
                    job.JobState = RcmJobState::SKIPPED;
                    status = SVS_OK;
                    DebugPrintf(SV_LOG_ALWAYS,"Disk %s is offline. Skipping Resync Start Notify.\n", resyncInput.SourceDiskId.c_str());
                    break;

                }
#endif
                status = SVE_FAIL;
                errMsg += " Resync start notify ioctl failed.";
                possibleCauses = "The driver failed to stamp resync start time.";
                recmAction = "Check if the disk is still attached.";
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_IOCTL_ERROR;
                break;
            }
            
            reason |= LOG_ON_RESYNC_START;

#ifdef SV_WINDOWS
            std::string deviceNameForFlushDevCache = diskId;
#else
            std::string deviceNameForFlushDevCache = vr.m_DeviceName;
#endif

            status = FlushOnDeviceCache(deviceNameForFlushDevCache, &m_volumesCache.m_Vs);
            if (SVS_OK != status) {
                // It is considered as soft failure now
                status = SVS_OK;
            }
            else
            {
                reason |= LOG_ON_DISK_FLUSH;
            }

            ResyncStartNotifyOutput resyncOutput;
            resyncOutput.ResyncStartTime = resyncTime.time;
            resyncOutput.ResyncStartSequenceId = resyncTime.seqNo;

            std::string strOutput = JSON::producer<ResyncStartNotifyOutput>::convert(resyncOutput);
            job.OutputPayload = securitylib::base64Encode(strOutput.c_str(),
                                                            strOutput.length());
            job.JobStatus = RcmJobStatus::SUCCEEDED;

        } while (0);
    }
    catch (DataProtectionException &e)
    {
        status = SVE_ABORT;
        possibleCauses = "The driver failed to stamp resync start time.";
        recmAction = "Check if the disk is still attached.";
        jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_IOCTL_EXCEPTION;
        errMsg += e.what();
    }
    catch (JSON::json_exception& jsone)
    {
        errMsg += jsone.what();
        possibleCauses = "Failed to parse the resync settings.";
        jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_SETTINGS_PARSE_ERROR;
        status = SVE_ABORT;
    }
    catch (const char *msg)
    {
        errMsg += msg;
        status = SVE_ABORT;
    }
    catch (std::string &msg)
    {
        errMsg += msg;
        status = SVE_ABORT;
    }
    catch (std::exception &e)
    {
        errMsg += e.what();
        status = SVE_ABORT;
    }
    catch (...)
    {
        errMsg += "unknonw error.";
        status = SVE_ABORT;
    }

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: failed to get resync start notify time. Job Id %s Input %s error %d %s\n",
            FUNCTION_NAME,
            job.Id.c_str(),
            job.InputPayload.c_str(),
            jobErrorCode,
            errMsg.c_str());

        job.JobStatus = RcmJobStatus::FAILED;
        RcmJobError jobError(
                boost::lexical_cast<std::string>(jobErrorCode),
                jobErrorMessage,
                possibleCauses,
                recmAction,
                RcmJobSeverity::RCM_ERROR);
        
        job.Errors.push_back(jobError);
    }

    DebugPrintf(SV_LOG_ALWAYS, "%s: disk %s device %s status = %d\n",
        FUNCTION_NAME,
        diskId.c_str(),
        deviceName.c_str(),
        status);

    if (reason != 0)
        LogSourceReplicationStatus(diskId, reason);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

/// \brief process the resync end notify job and post the status to RCM
/// \returns
/// \li SVS_OK on success
/// \li SVE_FAIL if ioctl fails
/// \li SVE_ABORT if caught exception
SVSTATUS RcmWorkerThread::SendResyncEndNotify(RcmJob& job)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    std::string errMsg, diskId, deviceName;
    uint32_t jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_RESYNC_END_NOTIFY_ERROR;
    std::string jobErrorMessage = "Failed to get sync end time";
    std::string possibleCauses = "Failed to process the resync end settings.";
    std::string recmAction = "Contact support if the issue persists.";
    uint32_t reason = 0;

    try {
        do {

            if (!m_pDriverStream->IsOpen())
            {
                possibleCauses = "The data change tracking module of the mobility service is not loaded on the VM.";
                recmAction = "The kernel version of the running operating system kernel on the source machine is not supported on the version of the mobility service installed on the source machine.";
                errMsg = "Filter driver is not initialized.";
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_NOT_INIT_ERROR;
                status = SVE_FAIL;
                break;
            }

            std::string strInput = securitylib::base64Decode(job.InputPayload.c_str(),
                job.InputPayload.length());
            ResyncEndNotifyInput resyncInput = JSON::consumer<ResyncEndNotifyInput>::convert(strInput);

            VolumeReporter::VolumeReport_t vr;
            status = GetVolumeReport(resyncInput.SourceDiskId, vr, errMsg);
            if (status != SVS_OK)
            {
                possibleCauses = "The disk is removed from the system or the disk signature changed.";
                recmAction = "Check if the disk is still attached.";
                recmAction += "If the disk signature changed, disable and enable replication of the system.";
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DEVICE_NOT_FOUND_ERROR;
                break;
            }
            
            diskId = resyncInput.SourceDiskId;
            deviceName = vr.m_DeviceName;

#ifdef _WIN32
            DpDriverComm ddc(resyncInput.SourceDiskId, vr.m_VolumeType);
#else
            DpDriverComm ddc(vr.m_DeviceName, vr.m_VolumeType);
#endif
            SV_ULONG            ulError = 0;
            ResyncTimeSettings  resyncTime;
            if (!ddc.ResyncEndNotify(resyncTime, ulError))
            {
#ifdef SV_WINDOWS
                if (ERROR_FILE_OFFLINE == ulError) {
                    job.JobStatus = RcmJobStatus::NONE;
                    job.JobState = RcmJobState::SKIPPED;
                    status = SVS_OK;
                    DebugPrintf(SV_LOG_ALWAYS, "Disk %s is offline. Skipping Resync End Notify.\n", resyncInput.SourceDiskId.c_str());
                    break;
                }
#endif
                errMsg += "Resync end notify ioctl failed.";
                status = SVE_FAIL;
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_IOCTL_ERROR;
                break;
            }

            reason = LOG_ON_RESYNC_END;

            ResyncEndNotifyOutput resyncOutput;
            resyncOutput.ResyncEndTime = resyncTime.time;
            resyncOutput.ResyncEndSequenceId = resyncTime.seqNo;

            std::string strOutput = JSON::producer<ResyncEndNotifyOutput>::convert(resyncOutput);
            job.OutputPayload = securitylib::base64Encode(strOutput.c_str(),
                                                    strOutput.length());
            
            job.JobStatus = RcmJobStatus::SUCCEEDED;

        } while (0);
    }
    catch (DataProtectionException &e)
    {
        status = SVE_ABORT;
        possibleCauses = "The driver failed to stamp resync end time.";
        recmAction = "Check if the disk is still attached.";
        jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_IOCTL_EXCEPTION;
        errMsg += e.what();
    }
    catch (JSON::json_exception& jsone)
    {
        errMsg += jsone.what();
        possibleCauses = "Failed to parse the resync end settings.";
        jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_SETTINGS_PARSE_ERROR;
        status = SVE_ABORT;
    }
    catch (const char *msg)
    {
        errMsg += msg;
        status = SVE_ABORT;
    }
    catch (std::string &msg)
    {
        errMsg += msg;
        status = SVE_ABORT;
    }
    catch (std::exception &e)
    {
        errMsg += e.what();
        status = SVE_ABORT;
    }
    catch (...)
    {
        errMsg += "unknonw error.";
        status = SVE_ABORT;
    }

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: failed to get resync end notify time. Job Id %s Input %s error %d %s\n",
            FUNCTION_NAME,
            job.Id.c_str(),
            job.InputPayload.c_str(),
            jobErrorCode,
            errMsg.c_str());

        job.JobStatus = RcmJobStatus::FAILED;
        RcmJobError jobError(
            boost::lexical_cast<std::string>(jobErrorCode),
            jobErrorMessage,
            possibleCauses,
            recmAction,
            RcmJobSeverity::RCM_ERROR);

        job.Errors.push_back(jobError);
    }

    DebugPrintf(SV_LOG_ALWAYS, "%s: disk %s device %s status = %d\n",
        FUNCTION_NAME,
        diskId.c_str(),
        deviceName.c_str(),
        status);

    if (reason != 0)
        LogSourceReplicationStatus(diskId, reason);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

/// \brief process the stop filtering job
/// \returns
/// \li SVS_OK on success
/// \li SVE_FAIL if ioctl fails
/// \li SVE_ABORT if caught exception
SVSTATUS RcmWorkerThread::StopFiltering(RcmJob& job)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    std::string errMsg, diskId, deviceName;
    uint32_t jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_STOP_FILTERTING_ERROR;
    std::string jobErrorMessage = "Failed to issue stop data change tracking on device.";
    std::string possibleCauses = "Failed to process the stop data change tracking settings.";
    std::string  recmAction = "Contact support if the issue persists.";
    uint32_t reason = 0;

    try {
        do {

            const StopFilteringInput stopfInput = JSON::consumer<StopFilteringInput>::convert(
                (job.JobType != RcmJobTypes::STOP_TRACKING) ? job.InputPayload :
                securitylib::base64Decode(job.InputPayload.c_str(), job.InputPayload.length()));

            diskId = stopfInput.SourceDiskId;

            std::map<std::string, steady_clock::time_point>::iterator it;
            it = m_renewUriFailTimes[RenewRequestTypes::DiffSyncContainer].find(diskId);
            if (it != m_renewUriFailTimes[RenewRequestTypes::DiffSyncContainer].end())
                m_renewUriFailTimes[RenewRequestTypes::DiffSyncContainer].erase(it);

            it = m_renewUriFailTimes[RenewRequestTypes::ResyncContainer].find(diskId);
            if (it != m_renewUriFailTimes[RenewRequestTypes::ResyncContainer].end())
                m_renewUriFailTimes[RenewRequestTypes::ResyncContainer].erase(it);

            VolumeReporter::VolumeReport_t vr;
            status = GetVolumeReport(stopfInput.SourceDiskId, vr, errMsg);
            if (status != SVS_OK)
            {
                // Since stop filtering is issued only during the disable protection,
                // sending success if the device is not present.
                status = SVS_OK;
                job.JobStatus = RcmJobStatus::SUCCEEDED;
                reason = LOG_ON_STOP_FLT;
                DebugPrintf(SV_LOG_ALWAYS, "%s: device for diskId %s not found. Returning success.\n",
                    FUNCTION_NAME,
                    diskId.c_str());
                break;
            }

            deviceName = vr.m_DeviceName;

            if (m_pDriverStream->IsOpen())
            {
#ifdef _WIN32
                DpDriverComm ddc(stopfInput.SourceDiskId, vr.m_VolumeType);
#else
                DpDriverComm ddc(vr.m_DeviceName, vr.m_VolumeType);
#endif
                HandlerMap_t::iterator stopIter = m_handlers.find(RcmWorkerThreadHandlerNames::SENTINELSTOP);
                if (stopIter == m_handlers.end())
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: no handler found to stop sentinel.\n", FUNCTION_NAME);
                    break;
                }

                stopIter->second();
                ddc.StopFiltering();
            }
            else
            {
                DebugPrintf(SV_LOG_ALWAYS,
                    "%s : succeeding stop filtering for %s as driver is not loaded.\n",
                    FUNCTION_NAME,
                    deviceName.c_str());
            }

            reason = LOG_ON_STOP_FLT;

            job.JobStatus = RcmJobStatus::SUCCEEDED;

        } while (0);
    }
    catch (DataProtectionException &e)
    {
        status = SVE_ABORT;
        possibleCauses = "The data change tracking module failed to stop tracking.";
        recmAction = "Check if the disk is still attached.";
        jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_IOCTL_EXCEPTION;
        errMsg += e.what();
    }
    catch (JSON::json_exception& jsone)
    {
        errMsg += jsone.what();
        possibleCauses = "Failed to parse the stop tracking settings.";
        jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_SETTINGS_PARSE_ERROR;
        status = SVE_ABORT;
    }
    catch (const char *msg)
    {
        errMsg += msg;
        status = SVE_ABORT;
    }
    catch (std::string &msg)
    {
        errMsg += msg;
        status = SVE_ABORT;
    }
    catch (std::exception &e)
    {
        errMsg += e.what();
        status = SVE_ABORT;
    }
    catch (...)
    {
        errMsg += "unknonw error.";
        status = SVE_ABORT;
    }

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "failed to do stop filtering, Job Id %s Input %s error %s\n",
            job.Id.c_str(),
            job.InputPayload.c_str(),
            errMsg.c_str());

        job.JobStatus = RcmJobStatus::FAILED;
        RcmJobError jobError(
            boost::lexical_cast<std::string>(jobErrorCode),
            jobErrorMessage,
            possibleCauses,
            recmAction,
            RcmJobSeverity::RCM_ERROR);

        job.Errors.push_back(jobError);
    }

    DebugPrintf(SV_LOG_ALWAYS, "%s: disk %s device %s status = %d\n",
        FUNCTION_NAME,
        diskId.c_str(),
        deviceName.c_str(),
        status);

    if (reason != 0)
        LogSourceReplicationStatus(diskId, reason);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

SVSTATUS RcmWorkerThread::PrepareForSync(RcmJob& job)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg, diskId, deviceName;
    uint32_t reason = 0;
    uint32_t jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_PREPARE_FOR_SYNC_ERROR;
    std::string jobErrorMessage = "Failed to prepare for sync";
    std::string possibleCauses = "Failed to process prepare for sync settings.";
    std::string recmAction = "Contact support if the issue persists.";

    try {
        do {

            if (!m_pDriverStream->IsOpen())
            {
                possibleCauses = "The data change tracking module of the mobility service is not loaded on the VM.";
                recmAction = "The kernel version of the running operating system kernel on the source machine is not supported on the version of the mobility service installed on the source machine.";
                errMsg = "Filter driver is not initialized.";
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_NOT_INIT_ERROR;
                status = SVE_FAIL;
                break;
            }

            const RcmClientLib::SourceAgentPrepareForSyncInput syncInput =
                JSON::consumer<RcmClientLib::SourceAgentPrepareForSyncInput>::convert(job.InputPayload);

            VolumeReporter::VolumeReport_t vr;
            status = GetVolumeReport(syncInput.SourceDiskId, vr, errMsg);
            if (status != SVS_OK)
            {
                possibleCauses = "The disk was either detached from the virtual machine or the disk signature might have been changed.";
                recmAction = "If the disk was detached from the virtual machine, attach the disk and retry resynchronization.";
                recmAction += " If the disk signature was changed, disable and enable replication for the virtual machine.";
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DEVICE_NOT_FOUND_ERROR;
                break;
            }

            diskId = syncInput.SourceDiskId;
            deviceName = vr.m_DeviceName;

#ifdef _WIN32
            DpDriverComm ddc(syncInput.SourceDiskId, vr.m_VolumeType);
#else
            DpDriverComm ddc(vr.m_DeviceName, vr.m_VolumeType, syncInput.SourceDiskId);
#endif
            ddc.StartFiltering(vr, &m_volumesCache.m_Vs);
            reason = LOG_ON_START_FLT | LOG_ON_PREPARE_FOR_SYNC;

            job.JobStatus = RcmJobStatus::SUCCEEDED;

        } while (0);
    }
    catch (const DataProtectionException &e)
    {
        status = SVE_ABORT;
        possibleCauses = "Failed to issue prepare for sync.";
        recmAction = "Check if the disk is still attached.";
        jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_IOCTL_EXCEPTION;
        errMsg += e.what();
    }
    catch (const JSON::json_exception& jsone)
    {
        errMsg += jsone.what();
        possibleCauses = "Failed to parse the sync settings.";
        jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_SETTINGS_PARSE_ERROR;
        status = SVE_ABORT;
    }
    catch (const char *msg)
    {
        errMsg += msg;
        status = SVE_ABORT;
    }
    catch (const std::string &msg)
    {
        errMsg += msg;
        status = SVE_ABORT;
    }
    catch (const std::exception &e)
    {
        errMsg += e.what();
        status = SVE_ABORT;
    }
    catch (...)
    {
        errMsg += "unknonw error.";
        status = SVE_ABORT;
    }

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: failed to prepare for sync. Job Id %s Input %s error %d %s\n",
            FUNCTION_NAME,
            job.Id.c_str(),
            job.InputPayload.c_str(),
            jobErrorCode,
            errMsg.c_str());

        job.JobStatus = RcmJobStatus::FAILED;
        RcmJobError jobError(
            boost::lexical_cast<std::string>(jobErrorCode),
            jobErrorMessage,
            possibleCauses,
            recmAction,
            RcmJobSeverity::RCM_ERROR,
            errMsg);

        job.Errors.push_back(jobError);
    }

    DebugPrintf(SV_LOG_ALWAYS, "%s: disk %s device %s status = %d\n",
        FUNCTION_NAME,
        diskId.c_str(),
        deviceName.c_str(),
        status);

    if (reason != 0)
        LogSourceReplicationStatus(diskId, reason);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

SVSTATUS RcmWorkerThread::UpgradeAgent(const boost::filesystem::path& agentDownloadPath,
    const std::string& agentInstallPackageName,
    const boost::filesystem::path& serializedJobFilePath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

#ifdef SV_WINDOWS
    const std::string upgraderFileName = "UnifiedAgentUpgrader.exe";
    const std::string upgradeParamName = "UpgradeJobFilePath";
#else
    const std::string upgraderFileName = "AgentUpgrade.sh";
    const std::string upgradeParamName = "-j";
#endif

    std::vector<std::string> args;
    args.push_back(upgradeParamName);
    args.push_back(serializedJobFilePath.string());

    boost::filesystem::path upgraderPath(agentDownloadPath / upgraderFileName);

    if (StartUpgrade(upgraderPath.string(), args) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to start upgrade from service.\n",
            FUNCTION_NAME);
        return SVE_FAIL;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO, "%s: Successfully started upgrade from service.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SVS_OK;
}


SVSTATUS RcmWorkerThread::AgentUpgrade(RcmJob& job)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;
    boost::system::error_code ec;
    uint32_t reason = 0;
    uint32_t jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_AGENT_UPGRADE_ERROR;
    std::string jobErrorMessage = "Failed to upgrade the agent";
    std::string possibleCauses = "Failed to process agent upgrade settings.";
    std::string recmAction = "Contact support if the issue persists.";


    try {
        do {
            const RcmClientLib::SourceAgentUpgradeInput agentUpgradeInput =
                JSON::consumer<RcmClientLib::SourceAgentUpgradeInput>::convert(job.InputPayload);

            std::string agentInstallPackageName;
#ifdef SV_WINDOWS
            boost::filesystem::path agentDownloadBasePath = "C:\\ProgramData\\Microsoft Azure Site Recovery\\AutoUpgrade";
#else
            boost::filesystem::path agentDownloadBasePath = "/tmp/MobilityAgentAutoUpgrade";
#endif
            boost::filesystem::path serializedJobFilePath(agentDownloadBasePath / JOBS / ("job_" + job.Id));
            boost::filesystem::path agentDownloadPath(agentDownloadBasePath / PACKAGE);

            if (boost::filesystem::exists(serializedJobFilePath, ec))
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: Agent Upgrade with Job ID : %s already started.\n",
                    FUNCTION_NAME, job.Id.c_str());
                return status;
            }

            status = CreateDirectoriesWithRetries(agentDownloadBasePath);

            if (status != SVS_OK)
            {
                std::string error = "Failed to create download directory " + agentDownloadBasePath.string();
                DebugPrintf(SV_LOG_ERROR, "%s\n", error.c_str());
                errMsg += error;
                possibleCauses = error;
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_AGENT_UPGRADE_ERROR;
                break;
            }

            boost::filesystem::space_info agentPathSpace;
            status = CheckSpace(agentDownloadBasePath, agentPathSpace);

            if (status != SVS_OK)
            {
                std::string error = "Not enough space available on source vm to download software at "
                    + agentDownloadBasePath.string() + ". Available free space(in bytes) : " +
                    boost::lexical_cast<std::string>(agentPathSpace.available) + ".";
                DebugPrintf(SV_LOG_ERROR, "%s\n", error.c_str());
                errMsg += error;
                possibleCauses = error;
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_AGENT_UPGRADE_LOW_ON_FREE_SPACE_ERROR;
                recmAction = "Please make sure at least " +
                    boost::lexical_cast<std::string>(AutoUpgradeScheduleParams::AgentDownloadDirReqSpace)
                    + " free space(in bytes) is available and retry the operation.";
                break;
            }

            status = DownloadAgent(agentUpgradeInput, agentDownloadPath, agentInstallPackageName);

            if (status == SVE_FILE_NOT_FOUND)
            {
                std::string error = "Agent file " + agentInstallPackageName + " is not present on appliance.";
                DebugPrintf(SV_LOG_ERROR, "%s\n", error.c_str());
                errMsg += error;
                possibleCauses = error;
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_AGENT_INSTALLER_PACKAGE_NOT_FOUND;
                break;
            }
            else if (status == SVE_ABORT)
            {
                std::string error = "Checksum verification failed for downloaded file.";
                DebugPrintf(SV_LOG_ERROR, "%s\n", error.c_str());
                errMsg += error;
                possibleCauses = error;
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_AGENT_INSTALLER_CHECKSUM_MISMATCH;
                break;
            }
            else if (status != SVS_OK)
            {
                std::string error = "Failed to download agents for agent auto upgrade.";
                DebugPrintf(SV_LOG_ERROR, "%s\n", error.c_str());
                errMsg += error;
                possibleCauses = error;
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_AGENT_DOWNLOAD_FAILED;
                break;
            }

            status = UnzipAgentPackage(agentDownloadPath, agentInstallPackageName);

            if (status != SVS_OK)
            {
                std::string error = "Failed to unzip agents for agent auto upgrade.";
                DebugPrintf(SV_LOG_ERROR, "%s\n", error.c_str());
                errMsg += error;
                possibleCauses = error;
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_AGENT_EXTRACTION_FAILED;
                break;
            }

            status = SerializeJobFile(serializedJobFilePath, job);
            if (status != SVS_OK)
            {
                std::string error = "Failed to serialize job file.";
                DebugPrintf(SV_LOG_ERROR, "%s\n", error.c_str());
                errMsg += error;
                possibleCauses = error;
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_AGENT_UPGRADE_JOB_SERIALIZATION_FAILED;
                break;
            }

            status = UpgradeAgent(agentDownloadPath, agentInstallPackageName, serializedJobFilePath);
            if (status != SVS_OK)
            {
                std::string error = "Failed to invoke agent upgrade.";
                DebugPrintf(SV_LOG_ERROR, "%s\n", error.c_str());
                errMsg += error;
                possibleCauses = error;
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_AGENT_UPGRADE_ERROR;
                break;
            }
        } while (0);
    }
    catch (const JSON::json_exception& jsone)
    {
        errMsg += jsone.what();
        possibleCauses = "Failed to upgrade the agent.";
        jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_AGENT_UPGRADE_ERROR;
        status = SVE_ABORT;
    }
    catch (const char *msg)
    {
        errMsg += msg;
        status = SVE_ABORT;
    }
    catch (const std::string &msg)
    {
        errMsg += msg;
        status = SVE_ABORT;
    }
    catch (const std::exception &e)
    {
        errMsg += e.what();
        status = SVE_ABORT;
    }
    catch (...)
    {
        errMsg += "unknonw error.";
        status = SVE_ABORT;
    }

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: failed to upgrade the agent. Job Id %s Input %s error %d %s\n",
            FUNCTION_NAME,
            job.Id.c_str(),
            job.InputPayload.c_str(),
            jobErrorCode,
            errMsg.c_str());

        job.JobStatus = RcmJobStatus::FAILED;
        // To do: Modify the logic to use 
        RcmJobError jobError(
            jobErrorCode == RcmClientLib::RCM_CLIENT_JOB_AGENT_INSTALLER_PACKAGE_NOT_FOUND ?
            "AgentInstallerPackageNotFoundOnAppliance" :
            boost::lexical_cast<std::string>(jobErrorCode),
            jobErrorMessage,
            possibleCauses,
            recmAction,
            RcmJobSeverity::RCM_ERROR);

        job.Errors.push_back(jobError);
    }


    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

sigslot::signal0<>& RcmWorkerThread::GetPrepareSwitchApplianceSignal()
{
    return m_prepareSwitchApplianceSignal;
}

sigslot::signal1<RcmProxyTransportSetting&>& RcmWorkerThread::GetSwitchApplianceSignal()
{
    return m_switchApplianceSignal;
}

SVSTATUS RcmWorkerThread::PrepareForSwitchAppliance(RcmJob& job)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVE_FAIL;
    std::string errMsg;
    uint32_t jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_PREPARE_FOR_SWITCH_APPLIANCE_ERROR;
    std::string jobErrorMessage = "Failed to prepare mobility service for switch appliance.";
    std::string possibleCauses =
        "Couldn't stop replication to current appliance on source machine.";
    std::string recmAction = "Contact support if the issue persists.";
    SwitchAppliance::State state;
    ACE_Time_Value waitTime(SwitchAppliance::WaitTime, 0);

    try {
        RcmConfigurator::getInstance()->setSwitchApplianceState(
            SwitchAppliance::NONE);
        m_prepareSwitchApplianceSignal();
        boost::chrono::system_clock::time_point endTime =
            boost::chrono::system_clock::now() +
            boost::chrono::seconds(SwitchAppliance::JobTimeOut);
        boost::chrono::system_clock::time_point lastChecked =
            boost::chrono::system_clock::time_point::min();

        while(!m_threadManager.testcancel(ACE_Thread::self()))
        {
            boost::chrono::system_clock::time_point curTime = boost::chrono::system_clock::now();
            if (curTime > endTime)
            {
                std::string error = "Prepare for switch appliance timeout";
                errMsg += error;
                status = SVE_ABORT;
                break;
            }

            if (curTime > lastChecked + boost::chrono::seconds(SwitchAppliance::PollTime))
            {
                state = RcmConfigurator::getInstance()->getSwitchApplianceState();
                DebugPrintf(SV_LOG_ALWAYS, "%s : Switch Appliance state : %d\n",
                    FUNCTION_NAME, state);
                if(state == SwitchAppliance::PREPARE_SWITCH_APPLIANCE_SUCCEEDED)
                {
                    status = SVS_OK;
                    break;
                }
                lastChecked = curTime;
            }
            m_QuitEvent->wait(&waitTime, 0);
        }
        job.JobStatus = RcmJobStatus::SUCCEEDED;
    }
    RCM_WORKER_THREAD_CATCH_EXCEPTION(errMsg, status);

    if (status != SVS_OK)
    {
        state = RcmConfigurator::getInstance()->getSwitchApplianceState();
        DebugPrintf(SV_LOG_ERROR,
            "Prepare switch Appliance failed, State : %d. Job Id %s Input %s error %d %s\n",
            state,
            job.Id.c_str(),
            job.InputPayload.c_str(),
            jobErrorCode,
            errMsg.c_str());

        job.JobStatus = RcmJobStatus::FAILED;
        RcmJobError jobError(
            boost::lexical_cast<std::string>(jobErrorCode),
            jobErrorMessage,
            possibleCauses,
            recmAction,
            RcmJobSeverity::RCM_ERROR);

        job.Errors.push_back(jobError);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmWorkerThread::SwitchAppliance(RcmJob& job)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVE_FAIL;
    std::string errMsg;
    uint32_t jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_SWITCH_APPLIANCE_ERROR;
    std::string jobErrorMessage = "Failed to switch appliance.";
    std::string possibleCauses = "Failed to switch appliance.";
    std::string recmAction = "Contact support if the issue persists.";
    SwitchAppliance::State state;
    ACE_Time_Value waitTime(SwitchAppliance::WaitTime, 0);

    try {

        state = RcmConfigurator::getInstance()->getSwitchApplianceState();
        if (state != SwitchAppliance::PREPARE_SWITCH_APPLIANCE_SUCCEEDED)
        {
            std::stringstream strErrMsg;
            strErrMsg << "Cannot start switch appliance. Current state : " << state <<
                ", Required state : " << SwitchAppliance::PREPARE_SWITCH_APPLIANCE_SUCCEEDED;
            possibleCauses = "Prepare Switch Appliance did not succeed";
            throw INMAGE_EX(strErrMsg.str().c_str());
        }

        RcmClientLib::SourceAgentOnPremToAzureSwitchApplianceInput switchApplianceInput =
            JSON::consumer<RcmClientLib::SourceAgentOnPremToAzureSwitchApplianceInput>::convert(
            job.InputPayload);

        m_switchApplianceSignal(switchApplianceInput.ControlPathTransportSettings);

        boost::chrono::system_clock::time_point endTime =
            boost::chrono::system_clock::now() +
            boost::chrono::seconds(SwitchAppliance::JobTimeOut);
        boost::chrono::system_clock::time_point lastChecked =
            boost::chrono::system_clock::time_point::min();

        while(!m_threadManager.testcancel(ACE_Thread::self()))
        {

            boost::chrono::system_clock::time_point curTime = boost::chrono::system_clock::now();
            if (curTime > lastChecked + boost::chrono::seconds(SwitchAppliance::PollTime))
            {
                state = RcmConfigurator::getInstance()->getSwitchApplianceState();
                DebugPrintf(SV_LOG_ALWAYS, "%s : Switch Appliance state : %d\n",
                    FUNCTION_NAME, state);
                if (state == SwitchAppliance::SWITCH_APPLIANCE_SUCCEEDED)
                {
                    status = SVS_OK;
                    break;
                }
                else if (state == SwitchAppliance::NONE)
                {
                    std::string error =
                        "Couldn't verify client with new config. Restored old appliance config";
                    possibleCauses = "Failed to connect to new appliance";
                    errMsg += error;
                    status = SVE_FAIL;
                    break;
                }
                else if (state == SwitchAppliance::INCONSISTENT)
                {
                    std::string error = "Switch Appliance Failed. Restore of old appliance failed.";
                    possibleCauses = "Failed to connect to new appliance as well as old appliance";
                    errMsg += error;
                    status = SVE_FAIL;
                    break;
                }
                lastChecked = curTime;
            }

            if (curTime > endTime)
            {
                std::string error = "Switch Appliance Timedout.";
                errMsg += error;
                status = SVE_ABORT;
                break;
            }
            m_QuitEvent->wait(&waitTime, 0);
        }
        job.JobStatus = RcmJobStatus::SUCCEEDED;
    }
    RCM_WORKER_THREAD_CATCH_EXCEPTION(errMsg, status);

    if (status != SVS_OK)
    {
        if (!errMsg.empty())
            jobErrorMessage = errMsg;

        state = RcmConfigurator::getInstance()->getSwitchApplianceState();
        DebugPrintf(SV_LOG_ERROR,
            "Switch Appliance failed, State : %d. Job Id %s Input %s error %d %s\n",
            state,
            job.Id.c_str(),
            job.InputPayload.c_str(),
            jobErrorCode,
            errMsg.c_str());

        job.JobStatus = RcmJobStatus::FAILED;
        RcmJobError jobError(
            boost::lexical_cast<std::string>(jobErrorCode),
            jobErrorMessage,
            possibleCauses,
            recmAction,
            RcmJobSeverity::RCM_ERROR);

        job.Errors.push_back(jobError);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmWorkerThread::CreateDirectoriesWithRetries(const boost::filesystem::path & path)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::system::error_code ec;
    uint32_t retryCount = 0,
        maxRetryCount = RcmConfigurator::getInstance()->getUpdateDirectoryCreationRetryCount();
    ACE_Time_Value waitTime(RcmConfigurator::getInstance()->getUpdateDirectoryCreationRetryInterval(), 0);
    std::stringstream ss;

    if (boost::filesystem::exists(path, ec))
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Directory already exist %s\n",
            FUNCTION_NAME,
            path.string().c_str());
        return SVS_OK;
    }

    do
    {
        if (boost::filesystem::create_directories(path, ec))
        {
            break;
        }
        m_QuitEvent->wait(&waitTime, 0);
    } while (retryCount++ < maxRetryCount);

    ss.clear();
    ss << "Failed to create directory " << path.string() <<
        " with error code " << ec.value() << " and error message " << ec.message();

    if (!boost::filesystem::exists(path, ec))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: %s\n",
            FUNCTION_NAME,
            ss.str().c_str());
        return SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITING %s\n", FUNCTION_NAME);
    return SVS_OK;
}

SVSTATUS RcmWorkerThread::DeleteDirectoriesWithRetries(const boost::filesystem::path & path)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::system::error_code ec;
    uint32_t retryCount = 0,
        maxRetryCount = RcmConfigurator::getInstance()->getUpdateDirectoryDeletionRetryCount();
    ACE_Time_Value waitTime(RcmConfigurator::getInstance()->getUpdateDirectoryDeletionRetryInterval(), 0);
    std::stringstream ss;

    if (!boost::filesystem::exists(path, ec))
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Directory does not exist %s\n",
            FUNCTION_NAME,
            path.string().c_str());
        return SVS_OK;
    }

    do 
    {
        boost::filesystem::remove_all(path, ec);
        if (ec == boost::system::errc::success)
        {
            break;
        }
        m_QuitEvent->wait(&waitTime, 0);
    } while (retryCount++ < maxRetryCount);

    ss.clear();
    ss << FUNCTION_NAME << ": Failed to delete directory " << path.string()
        << " with error code " << ec.value() << " and error message " << ec.message() << std::endl;

    if (boost::filesystem::exists(path, ec))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: %s\n",
            FUNCTION_NAME,
            ss.str().c_str());
        return SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITING %s\n", FUNCTION_NAME);
    return SVS_OK;
}

bool RcmWorkerThread::IsAgentUnzipped(const boost::filesystem::path& agentDownloadPath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string untarOutput;
    for (boost::filesystem::directory_iterator itr(agentDownloadPath);
        itr!=boost::filesystem::directory_iterator(); ++itr)
    {
        untarOutput += itr->path().filename().string() + "\n";
        if (itr->path().filename().string().find(AutoUpgradeScheduleParams::CommandSuccessIndicator)
            != std::string::npos)
        {
            DebugPrintf(SV_LOG_DEBUG, "EXITING %s\n", FUNCTION_NAME);
            return true;
        }
    }
    DebugPrintf(SV_LOG_ERROR, "%s:Unzip agent output : %s does not contain %s.\n", FUNCTION_NAME,
        untarOutput.c_str(), AutoUpgradeScheduleParams::CommandSuccessIndicator.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITING %s\n", FUNCTION_NAME);
    return false;
}

SVSTATUS RcmWorkerThread::UnzipAgentPackage(
    const boost::filesystem::path& agentDownloadPath,
    const boost::filesystem::path& agentInstallPackageName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    ACE_Time_Value waitTime(RcmConfigurator::getInstance()->getInstallerUnzipRetryInterval(), 0);
    if (agentInstallPackageName.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Empty installer package file path.\n",
            FUNCTION_NAME);
        return SVE_FAIL;
    }

    if (agentDownloadPath.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Empty agent download file path.\n",
            FUNCTION_NAME);
        return SVE_FAIL;
    }

    boost::filesystem::path agentInstallPackagePath = agentDownloadPath / agentInstallPackageName;

#ifdef SV_WINDOWS
    std::string commandLine = "\"" + agentInstallPackagePath.string() + "\"" + " /q /x:" + "\"" + (agentDownloadPath).string() + "\"";
#else
    std::string commandLine = "tar -xvf " + agentInstallPackagePath.string() + " -C " + agentDownloadPath.string();
#endif

    int maxRetryCount = RcmConfigurator::getInstance()->getInstallerUnzipRetryCount();
    int retryCount = 0;
    do {
        DebugPrintf(SV_LOG_DEBUG, "%s: Retry count %d.\n",
            FUNCTION_NAME, retryCount);
        DebugPrintf(SV_LOG_ALWAYS, "%s: Installer extraction command line: %s.\n",
            FUNCTION_NAME,
            commandLine.c_str());

        boost::filesystem::path tempfile(agentDownloadPath / "temp.txt");
        SV_ULONG lexit;
        std::string output;
        bool processActive = true;

        AppCommand app(commandLine, 30, tempfile.string());
        app.Run(lexit, output, processActive);

        DebugPrintf(SV_LOG_INFO, "%s: App Command output %s.\n",
            FUNCTION_NAME, output.c_str());

        boost::system::error_code ec;
        if (boost::filesystem::exists(tempfile, ec))
        {
            // Ignore the errors
            lexit = 0;
            /*
             * Linux Unzip output Requirement : AgentUpgrade
             * Windows Unzip output Requirement : UNIFIEDAGENTUPGRADER
             */
            boost::filesystem::remove(tempfile, ec);
            if (IsAgentUnzipped(agentDownloadPath))
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: Successfully extracted agent installer package.\n",
                    FUNCTION_NAME);
                DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
                return SVS_OK;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to extract agent installer package.\n",
                    FUNCTION_NAME);
            }
        }

        m_QuitEvent->wait(&waitTime, 0);
    } while (retryCount++ < maxRetryCount && !m_threadManager.testcancel(ACE_Thread::self()));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SVE_FAIL;
}

SVSTATUS RcmWorkerThread::SerializeJobFile(
    const boost::filesystem::path& serializedJobFilePath,
    RcmJob& job)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    boost::filesystem::path parentPath = serializedJobFilePath.parent_path();
    
    if (DeleteDirectoriesWithRetries(parentPath) != SVS_OK)
    {
        return SVE_FAIL;
    }

    if (CreateDirectoriesWithRetries(parentPath) != SVS_OK)
    {
        return SVE_FAIL;
    }

    std::string jobDetails = JSON::producer<RcmJob>::convert(job);

    ofstream jobfile(serializedJobFilePath.string().c_str());
    if (!jobfile.good())
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to open file %s\n.", 
            FUNCTION_NAME,
            serializedJobFilePath.string().c_str());
        return SVE_FAIL;
    }

    jobfile << jobDetails;
    jobfile.flush();
    jobfile.close();

    DebugPrintf(SV_LOG_DEBUG, "%s: Successfully persisted job details in file %s\n",
        FUNCTION_NAME,
        serializedJobFilePath.string().c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return SVS_OK;
}

SVSTATUS RcmWorkerThread::CheckSpace(const boost::filesystem::path& agentDownloadPath,
    boost::filesystem::space_info &agentPathSpace)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    agentPathSpace = boost::filesystem::space(agentDownloadPath);
    if (agentPathSpace.available < AutoUpgradeScheduleParams::AgentDownloadDirReqSpace)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: Path : %s, Capacity : %ju, Free : %ju, Available : %ju, Required : %d\n.",
            FUNCTION_NAME, agentDownloadPath.string().c_str(), agentPathSpace.capacity,
            agentPathSpace.free, agentPathSpace.available,
            AutoUpgradeScheduleParams::AgentDownloadDirReqSpace);
        return SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SVS_OK;
}

SVSTATUS RcmWorkerThread::DownloadAgent(const SourceAgentUpgradeInput& agentUpgradeInput,
    const boost::filesystem::path& agentDownloadPath,
    std::string& agentInstallPackagePath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (agentUpgradeInput.DownloadLinkType != AutoUpgradeAgentDownloadLinkType::Appliance)
    {
        std::string errMsg = "The download type " + agentUpgradeInput.DownloadLinkType + " is not implemented.";
        DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
        throw std::runtime_error(errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "%s: Starting download file from appliance.\n", FUNCTION_NAME);

    // All the paths from RCM are expected to have a single forward slash as separator.
    // If that is not the case, then file download can fail.

    SourceAgentDownloadApplianceDetailsInput appDetails =
        JSON::consumer<SourceAgentDownloadApplianceDetailsInput>::convert(agentUpgradeInput.DownloadLinkDetails);
    agentInstallPackagePath = boost::filesystem::path(appDetails.DownloadFilePathOnAppliance).filename().string();
    boost::filesystem::path localPath = (agentDownloadPath / agentInstallPackagePath);

    boost::system::error_code ec;

    if (boost::filesystem::exists(localPath), ec)
    {
        if (!RcmConfigurator::getInstance()->validateAgentInstallerChecksum())
        {
            DebugPrintf(SV_LOG_INFO, "%s: Agent installer file %s is already present. Not validating checksum.\n",
                FUNCTION_NAME,
                localPath.string().c_str());
            return SVS_OK;
        }

        std::string localFileChecksum = securitylib::genSha256MacForFile(localPath.string());
        if (boost::iequals(localFileChecksum, appDetails.Sha256Checksum))
        {
            DebugPrintf(SV_LOG_INFO, "%s: Agent installer file %s with checksum %s is already present.\n",
                FUNCTION_NAME,
                localPath.string().c_str(),
                localFileChecksum.c_str());
            return SVS_OK;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to match checksum for file %s with size %llu, Local file checksum %s Checksum in settings %s\n",
                FUNCTION_NAME,
                localPath.string().c_str(),
                boost::filesystem::file_size(localPath),
                localFileChecksum.c_str(),
                appDetails.Sha256Checksum.c_str());
        }
    }
        
    if (DeleteDirectoriesWithRetries(agentDownloadPath) != SVS_OK)
    {
        return SVE_FAIL;
    }

    if (CreateDirectoriesWithRetries(agentDownloadPath) != SVS_OK)
    {
        return SVE_FAIL;
    }

    std::string dataPathTransportType, dataPathTransportSettings;
    RcmConfigurator::getInstance()->GetDataPathTransportSettings(dataPathTransportType, dataPathTransportSettings);

    if (!boost::iequals(dataPathTransportType, TRANSPORT_TYPE_PROCESS_SERVER))
    {
        std::string errMsg = "The transport type " + dataPathTransportType + " is not implemented.";
        DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
        throw std::runtime_error(errMsg.c_str());
    }

    ProcessServerTransportSettings psTransportSettings =
        JSON::consumer<ProcessServerTransportSettings>::convert(dataPathTransportSettings, true);
    TRANSPORT_CONNECTION_SETTINGS tcs;
    tcs.ipAddress = psTransportSettings.GetIPAddress();
    tcs.sslPort = boost::lexical_cast<std::string>(psTransportSettings.Port);
    CxTransport cxTransport(TRANSPORT_PROTOCOL_HTTP, tcs, true);

    FileInfos_t matchingFiles;

    // check if the file is present on appliance
    boost::tribool tb = cxTransport.listFile(appDetails.DownloadFilePathOnAppliance, matchingFiles, true);
    if (tb.value == boost::tribool::indeterminate_value)
    {
        // File is not found on appliance
        DebugPrintf(SV_LOG_INFO, "%s: The installer file %s is not present on the appliance.\n",
            FUNCTION_NAME,
            appDetails.DownloadFilePathOnAppliance.c_str());
        DebugPrintf(SV_LOG_ERROR, "%s: Listfile status: %s\n",
            FUNCTION_NAME,
            cxTransport.status());
        return SVE_FILE_NOT_FOUND;
    }
    else if (tb.value == boost::tribool::false_value)
    {
        // Could not check if file is present on appliance.
        // Just log the error and continue.
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to check if installer file %s is present on the appliance.\n",
            FUNCTION_NAME,
            appDetails.DownloadFilePathOnAppliance.c_str());
        DebugPrintf(SV_LOG_ERROR, "%s: Listfile status: %s\n",
            FUNCTION_NAME,
            cxTransport.status());
    }
    else if (tb.value == boost::tribool::true_value)
    {
        DebugPrintf(SV_LOG_INFO, "%s: The installer file %s is present on the appliance.\n",
            FUNCTION_NAME,
            appDetails.DownloadFilePathOnAppliance.c_str());
    }

    // Now get the file from appliance
    int retryCount = 0;
    do {
        tb = cxTransport.getFile(appDetails.DownloadFilePathOnAppliance, localPath.string());
    } while (retryCount++ < RcmConfigurator::getInstance()->getInstallerDownloadRetryCount() &&
        tb.value != boost::logic::tribool::true_value);
    
    if (tb.value == boost::logic::tribool::false_value)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to get installer file %s from appliance.\n",
            FUNCTION_NAME,
            appDetails.DownloadFilePathOnAppliance.c_str());
        DebugPrintf(SV_LOG_ERROR, "%s: Getfile status: %s\n",
            FUNCTION_NAME,
            cxTransport.status());
        return SVE_FAIL;
    }
    else if (tb.value == boost::logic::tribool::indeterminate_value)
    {
        // File is not found on appliance
        DebugPrintf(SV_LOG_ERROR, "%s: The installer file %s is not present on the appliance.\n",
            FUNCTION_NAME,
            appDetails.DownloadFilePathOnAppliance.c_str());
        DebugPrintf(SV_LOG_ERROR, "%s: Getfile status: %s\n",
            FUNCTION_NAME,
            cxTransport.status());
        return SVE_FILE_NOT_FOUND;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO, "%s: Successfully copied installer file %s to %s.\n",
            FUNCTION_NAME,
            appDetails.DownloadFilePathOnAppliance.c_str(),
            localPath.string().c_str());
    }

    if (RcmConfigurator::getInstance()->validateAgentInstallerChecksum())
    {
        // Match checksum of downloaded file with the checksum in settings
        std::string localFileChecksum = securitylib::genSha256MacForFile(localPath.string());
        if (!boost::iequals(localFileChecksum, appDetails.Sha256Checksum))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: The checksum %s of downloaded file %s does not match the checksum %s in settings.\n",
                FUNCTION_NAME,
                localFileChecksum.c_str(),
                localPath.string().c_str(),
                appDetails.Sha256Checksum.c_str());
            return SVE_ABORT;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SVS_OK;
}

SVSTATUS RcmWorkerThread::StartUpgrade(const std::string& binPath,
    const std::vector<std::string> & args)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
#ifdef SV_WINDOWS
    try {
        boost::process::spawn(binPath, boost::process::args(args));
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : Starting windows upgrader failed with exception %s.\n",
            FUNCTION_NAME,
            e.what());
        status = SVE_FAIL;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : Starting windows upgrader failed with unknown exception.\n",
            FUNCTION_NAME);
        status = SVE_FAIL;
    }

#else
    try
    {
        std::string command = "nohup " + binPath;
        for (std::vector<std::string>::const_iterator it = args.begin(); it != args.end(); it++)
        {
            command += " " + *it;
        }
        command += " &";

        boost::filesystem::path tempfile("temp.txt");
        SV_ULONG lexit;
        std::string output;
        bool processActive = true;

        AppCommand app(command, 30, tempfile.string());
        app.Run(lexit, output, processActive);

        if (boost::filesystem::exists(tempfile))
        {
            // Ignore the failure in deleting the file.
            boost::system::error_code ec;
            boost::filesystem::remove(tempfile, ec);
        }

        if (lexit == 0)
        {
            DebugPrintf(SV_LOG_DEBUG, "Successfully ran the command %s.\n", command.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "The command %s failed with exit code %lu.\n", command.c_str(), lexit);
            return SVE_FAIL;
        }
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : Starting linux upgrader failed with exception %s.\n",
            FUNCTION_NAME,
            e.what());
        status = SVE_FAIL;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : Starting linux upgrader failed with unknown exception.\n",
            FUNCTION_NAME);
        status = SVE_FAIL;
    }
#endif // SV_WINDOWS

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

void RcmWorkerThread::RenewSasUri(const std::string& diskId, const std::string& renewType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    std::string errMsg, response;

    try {
        bool isVirtualNodeDisk = RcmConfigurator::getInstance()->IsClusterSharedDiskProtected(diskId);
        status = RcmConfigurator::getInstance()->GetRcmClient()->RenewBlobContainerSasUri(diskId, renewType,
             response, isVirtualNodeDisk);

        if (status != SVS_OK)
        {
            int32_t iErrorCode = RcmConfigurator::getInstance()->GetRcmClient()->GetErrorCode();

            DebugPrintf(SV_LOG_ERROR,
                "%s : Renewing Blob container SAS URI for disk %s failed with status %d error %d.\n",
                FUNCTION_NAME,
                diskId.c_str(),
                status,
                iErrorCode);

            std::string strErrorCode;
            std::stringstream ss(response);
            boost::property_tree::ptree pt;
            boost::property_tree::read_xml(ss, pt);

            std::string errorcode = pt.get<std::string>("RcmError.ErrorCode", "");

            try {
                std::stringstream ss(response);
                boost::property_tree::ptree pt;
                boost::property_tree::read_xml(ss, pt);

                strErrorCode = pt.get<std::string>("RcmError.ErrorCode", "");
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_WARNING, "%s: Response data is not in XML format. %s\n", FUNCTION_NAME, response.c_str());
            }

            if ((strErrorCode == RCM_ERRORS::StorageAccountNotAvailable) ||
                (strErrorCode == RCM_ERRORS::AzureResourceManagerOperationFailed) ||
                (strErrorCode == RCM_ERRORS::LogContainerDoesNotExist) ||
                (iErrorCode == RCM_CLIENT_ERROR_NETWORK_TIMEOUT) ||
                (iErrorCode == HttpErrorCode::SERVER_BUSY))
            {

                if (!strErrorCode.empty())
                    DebugPrintf(
                    SV_LOG_ERROR,
                    "%s: RCM error indicates staging storage account error: %s.\n",
                    FUNCTION_NAME,
                    strErrorCode.c_str());

                m_renewUriFailTimes[renewType][diskId] = steady_clock::now();
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS,
                "%s : Renew Blob container SAS URI for disk %s succeeded\n",
                FUNCTION_NAME,
                diskId.c_str());

            std::map<std::string, steady_clock::time_point>& renewUriFailTimes = m_renewUriFailTimes[renewType];

            std::map<std::string, steady_clock::time_point>::iterator itRenewFailTime = renewUriFailTimes.find(diskId);
            if (itRenewFailTime != renewUriFailTimes.end())
                renewUriFailTimes.erase(itRenewFailTime);
        }
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : Renewing Blob container SAS URI for disk %s failed with exception %s.\n",
            FUNCTION_NAME,
            diskId.c_str(),
            e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : Renewing Blob container SAS URI for disk %s failed with unknown exception.\n",
            FUNCTION_NAME,
            diskId.c_str());
    }

    boost::mutex::scoped_lock guard(m_renewUriMutex);
    if (m_renewUriProgressList[renewType].end() == m_renewUriProgressList[renewType].find(diskId))
    {
        DebugPrintf(SV_LOG_ERROR, "disk %s not found in SAS URI renew list while renew is issued\n", diskId.c_str());
    }
    else
    {
        m_renewUriProgressList[renewType].erase(diskId);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

SVSTATUS RcmWorkerThread::CheckClientCertExpiry()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string errMsg;
    SVSTATUS status = SVS_OK;

    try
    {
        static boost::gregorian::date lastCheckCertExpiry(boost::date_time::min_date_time);
        boost::gregorian::date curDate = boost::gregorian::day_clock::local_day();
        if (curDate > lastCheckCertExpiry + boost::gregorian::days(ClientCert::ThrottleLimitInDays))
        {
            if (!securitylib::GenCert::isCertLive(securitylib::CLIENT_CERT_NAME,
                ClientCert::CertRenewBufferInDays))
            {
                DebugPrintf(SV_LOG_ALWAYS,
                    "Client cert will be expired after %d days. Updating Client cert\n",
                    ClientCert::CertRenewBufferInDays);
                RcmConfigurator::getInstance()->UpdateClientCert();
            }
            lastCheckCertExpiry = curDate;
        }
    }
    RCM_WORKER_THREAD_CATCH_EXCEPTION(errMsg, status);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmWorkerThread::MonitorReplicationSettings()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    AgentSettings settings;
    status = RcmConfigurator::getInstance()->GetAgentSettings(settings);
    if (SVS_OK != status)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to fetch cached replication settings %d\n", FUNCTION_NAME, status);
        return status;
    }

    status = MonitorDrainSetting(settings.m_drainSettings);

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s : MonitorDrainSetting failed with status %d\n", FUNCTION_NAME, status);
    }

    status = MonitorSyncSettings(settings.m_syncSettings);

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s : MonitorSyncSettings failed with status %d\n", FUNCTION_NAME, status);
    }

    LogSourceReplicationStatus(settings);

    if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
    {
        status = CheckClientCertExpiry();
        if (status != SVS_OK)
        {
            return status;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmWorkerThread::MonitorDrainSetting(const DrainSettings_t& drainSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    std::string errMsg;

    if (m_threadManager.testcancel(ACE_Thread::self()))
        return SVE_ABORT;

    for (DrainSettings_t::const_iterator dsit = drainSettings.begin();
        !(m_threadManager.testcancel(ACE_Thread::self())) && (dsit != drainSettings.end());
        dsit++)
    {
        if (TRANSPORT_TYPE_AZURE_BLOB != dsit->DataPathTransportType)
        {
            continue;
        }

        try {
            std::string input;
            if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
            {
                input = securitylib::base64Decode(
                    dsit->DataPathTransportSettings.c_str(),
                    dsit->DataPathTransportSettings.length());
            }
            else
            {
                input = dsit->DataPathTransportSettings;
            }

            AzureBlobBasedTransportSettings blobSettings =
                JSON::consumer<AzureBlobBasedTransportSettings>::convert(input, true);

            std::string sasUri = blobSettings.BlobContainerSasUri;
            uint32_t failureRetryWaitTimeInSecs = m_logContainerRenewalRetryWaitInSecs;

            bool bRenewUriOnAccessFailure = false;
            {
                boost::mutex::scoped_lock guard(m_renewUriMutex);
                std::set<std::string>& renewUriProgressList = m_renewUriProgressList[RenewRequestTypes::DiffSyncContainer];
                if (renewUriProgressList.end() != renewUriProgressList.find(dsit->DiskId))
                    continue;

                // check if the sasUri access is failing with auth error (HTTP error code 403).
                // or not found error (HTTP error code 404)
                // if the sasUri is renewed by the time this check is done, no need to renew
                std::map<std::string, std::string>& accessFailList = m_renewUriAccessFailList[RenewRequestTypes::DiffSyncContainer];
                std::map<std::string, std::string>::iterator itAccessFailList = accessFailList.find(dsit->DiskId);
                if ((accessFailList.end() != itAccessFailList) &&
                    boost::iequals(itAccessFailList->second, sasUri))
                {
                    bRenewUriOnAccessFailure = true;
                }

                // always remove from fail list
                accessFailList.erase(dsit->DiskId);
            }

            // in case of ARM throttle, container or storage account deleted do not issue
            // renew request immediately
            std::map<std::string, steady_clock::time_point>& renreUriFailTimes = m_renewUriFailTimes[RenewRequestTypes::DiffSyncContainer];
            std::map<std::string, steady_clock::time_point>::iterator itRenewFailTime = renreUriFailTimes.find(dsit->DiskId);
            if (itRenewFailTime != renreUriFailTimes.end() &&
                (steady_clock::now() < (itRenewFailTime->second + seconds(failureRetryWaitTimeInSecs))))
                continue;

            /// RCM sends time units in 100 ns
            uint64_t expiryTime = blobSettings.SasUriExpirationTime;

            /// convert to sec
            expiryTime /= HUNDRED_NANO_SECS_IN_SEC;

            uint64_t secSinceAd001 = GetTimeInSecSinceAd0001();

            /// renew 30 min before expiry
            uint64_t preExpiryInterval = 30 * 60;

            if (bRenewUriOnAccessFailure || (secSinceAd001 + preExpiryInterval) > expiryTime)
            {
                DebugPrintf(SV_LOG_ALWAYS,
                    "%s : Renewing Blob container SAS URI for disk %s current time %lld expiry time %lld access failure %d\n",
                    FUNCTION_NAME,
                    dsit->DiskId.c_str(),
                    secSinceAd001,
                    expiryTime,
                    bRenewUriOnAccessFailure);

                boost::mutex::scoped_lock guard(m_renewUriMutex);
                m_renewUriProgressList[RenewRequestTypes::DiffSyncContainer].insert(dsit->DiskId);

                m_ioService.post(boost::bind(&RcmWorkerThread::RenewSasUri, this, dsit->DiskId, RenewRequestTypes::DiffSyncContainer));
            }
        }
        catch (const JSON::json_exception& jsone)
        {
            errMsg += jsone.what();
            status = SVE_ABORT;
        }
        catch (const std::exception& e)
        {
            errMsg += e.what();
            status = SVE_ABORT;
        }
        catch (...)
        {
            errMsg += "unknonw error";
            status = SVE_ABORT;
        }
    }

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : failed to check the SAS URI expiry. %s\n",
            FUNCTION_NAME,
            errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmWorkerThread::MonitorSyncSettings(const SyncSettings_t& syncSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    std::string errMsg;

    if (!RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: no resync settings monitoring requried.\n", FUNCTION_NAME);
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    AgentSettings settings;
    status = RcmConfigurator::getInstance()->GetAgentSettings(settings);
    if (SVS_OK != status)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to fetch cached replication settings %d\n", FUNCTION_NAME, status);
        return status;
    }

    if (m_threadManager.testcancel(ACE_Thread::self()))
        return SVE_ABORT;

    if (TRANSPORT_TYPE_AZURE_BLOB != settings.m_dataPathType)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: no resync settings monitoring requried for data path type %s.\n", FUNCTION_NAME, settings.m_dataPathType.c_str());
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    for (SyncSettings_t::const_iterator iterSyncSettings = syncSettings.begin();
        !(m_threadManager.testcancel(ACE_Thread::self())) && (iterSyncSettings != syncSettings.end());
        iterSyncSettings++)
    {
        AzureBlobBasedTransportSettings blobSettings = iterSyncSettings->TransportSettings;

        std::string sasUri = blobSettings.BlobContainerSasUri;
        uint32_t failureRetryWaitTimeInSecs = m_logContainerRenewalRetryWaitInSecs;

        bool bRenewUriOnAccessFailure = false;
        {
            boost::mutex::scoped_lock guard(m_renewUriMutex);
            std::set<std::string> renewUriProgressList = m_renewUriProgressList[RenewRequestTypes::ResyncContainer];
            if (renewUriProgressList.end() != renewUriProgressList.find(iterSyncSettings->DiskId))
                continue;

            // check if the sasUri access is failing with auth error (HTTP error code 403)
            // or not found error (Http error code 404)
            // if the sasUri is renewed by the time this check is done, no need to renew
            std::map<std::string, std::string>& accessFailList = m_renewUriAccessFailList[RenewRequestTypes::ResyncContainer];
            std::map<std::string, std::string>::iterator itAccessFailList = accessFailList.find(iterSyncSettings->DiskId);
            if ((accessFailList.end() != itAccessFailList) &&
                boost::iequals(itAccessFailList->second, sasUri))
            {
                bRenewUriOnAccessFailure = true;
            }

            // always remove from fail list
            accessFailList.erase(iterSyncSettings->DiskId);
        }

        // in case of ARM throttle, container or storage account deleted do not issue
        // renew request immediately
        std::map<std::string, steady_clock::time_point>& renewUriFailTimes = m_renewUriFailTimes[RenewRequestTypes::ResyncContainer];
        std::map<std::string, steady_clock::time_point>::iterator itRenewFailTime = renewUriFailTimes.find(iterSyncSettings->DiskId);
        if (itRenewFailTime != renewUriFailTimes.end() &&
            (steady_clock::now() < (itRenewFailTime->second + seconds(failureRetryWaitTimeInSecs))))
            continue;

        /// RCM sends time units in 100 ns
        uint64_t expiryTime = blobSettings.SasUriExpirationTime;

        /// convert to sec
        expiryTime /= HUNDRED_NANO_SECS_IN_SEC;

        uint64_t secSinceAd001 = GetTimeInSecSinceAd0001();

        /// renew 30 min before expiry
        uint64_t preExpiryInterval = 30 * 60;

        if (bRenewUriOnAccessFailure || (secSinceAd001 + preExpiryInterval) > expiryTime)
        {
            DebugPrintf(SV_LOG_ALWAYS,
                "%s : Renewing Blob container SAS URI for disk %s current time %lld expiry time %lld access failure %d\n",
                FUNCTION_NAME,
                iterSyncSettings->DiskId.c_str(),
                secSinceAd001,
                expiryTime,
                bRenewUriOnAccessFailure);

            boost::mutex::scoped_lock guard(m_renewUriMutex);
            m_renewUriProgressList[RenewRequestTypes::ResyncContainer].insert(iterSyncSettings->DiskId);

            m_ioService.post(boost::bind(&RcmWorkerThread::RenewSasUri, this, iterSyncSettings->DiskId, RenewRequestTypes::ResyncContainer));
        }
    }

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : failed to check the SAS URI expiry. %s\n",
            FUNCTION_NAME,
            errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}


SVSTATUS RcmWorkerThread::IssueBaselineBookmark(RcmJob& job)
{
    boost::mutex::scoped_lock guard(m_baselineBookmarkMutex);

    SVSTATUS status = SVS_OK;
    std::string errMsg, diskId;
    uint32_t jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_BASELINE_BOOKMARK_ERROR;
    std::string jobErrorMessage = "Failed to issue base line bookmark for device.";
    std::string possibleCauses = "Failed to process the baseline bookmark settings.";
    std::string  recmAction = "Contact support if the issue persists.";
    uint32_t reason = 0;

    try {
        do {

            if (!m_pDriverStream->IsOpen())
            {
                possibleCauses = "The data change tracking module of the mobility service is not loaded on the VM.";
                recmAction = "The kernel version of the running operating system kernel on the source machine is not supported on the version of the mobility service installed on the source machine.";
                errMsg = "Filter driver is not initialized.";
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_NOT_INIT_ERROR;
                status = SVE_FAIL;
                break;
            }

            std::string strInput = securitylib::base64Decode(job.InputPayload.c_str(),
                job.InputPayload.length());
            IssueBaselineBookmarkInput bookmarkInput = JSON::consumer<IssueBaselineBookmarkInput>::convert(strInput);

            VolumeReporter::VolumeReport_t vr;
            status = GetVolumeReport(bookmarkInput.SourceDiskId, vr, errMsg);
            if (status != SVS_OK)
            {
                possibleCauses = "The disk is removed from the system or the disk signature changed.";
                recmAction = "Check if the disk is still attached.";
                recmAction += "If the disk signature changed, disable and enable replication of the system.";
                break;
            }

            const std::string vacpCmd = (boost::filesystem::path(m_installPath) / INM_VACP_SUBPATH).string();

#ifdef _WIN32
            diskId = bookmarkInput.SourceDiskId;
            boost::erase_all(diskId, "{");
            boost::erase_all(diskId, "}");
#else
            diskId = vr.m_DeviceName;
#endif
            std::string vacpArgs = " -v ";
            vacpArgs += diskId;
            vacpArgs += " -baseline ";
            vacpArgs += bookmarkInput.BaselineBookmarkId;
            vacpArgs += " -x ";

            DebugPrintf(SV_LOG_ALWAYS, "Running baseline bookmark command \"%s\" %s.\n", vacpCmd.c_str(), vacpArgs.c_str());

            std::string cmdOutput;
            SV_UINT uTimeOut = 30;
            SV_ULONG lInmexitCode = 1;

            AppCommand objAppCommand(vacpCmd, vacpArgs, uTimeOut);
            objAppCommand.Run(lInmexitCode, cmdOutput, m_active, "", NULL);

            if (lInmexitCode != 0)
            {
                DebugPrintf(SV_LOG_ERROR, "Baseline bookmark command failed.\n %s\n", cmdOutput.c_str());
                status = SVE_FAIL;

                if (lInmexitCode == VACP_NO_PROTECTED_VOLUMES)
                {
                    possibleCauses = "The disk is not protected.";
                }
                else if (lInmexitCode == VACP_DISK_NOT_IN_DATAMODE)
                {
                    possibleCauses = "The disk is not in write order mode to issue the bookmark.";
                }
                break;
            }

            reason = LOG_ON_BASELINE_TAG;
            diskId = bookmarkInput.SourceDiskId;
            job.JobStatus = RcmJobStatus::SUCCEEDED;

        } while (0);
    }
    catch (DataProtectionException &e)
    {
        status = SVE_ABORT;
        errMsg += e.what();
    }
    catch (JSON::json_exception& jsone)
    {
        errMsg += jsone.what();
        status = SVE_ABORT;
    }
    catch (const char *msg)
    {
        errMsg += msg;
        status = SVE_ABORT;
    }
    catch (std::string &msg)
    {
        errMsg += msg;
        status = SVE_ABORT;
    }
    catch (std::exception &e)
    {
        errMsg += e.what();
        status = SVE_ABORT;
    }
    catch (...)
    {
        errMsg += "unknonw error";
        status = SVE_ABORT;
    }

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "failed to issue baseline bookmark for input : Job Id %s Input %s error %s\n",
            job.Id.c_str(),
            job.InputPayload.c_str(),
            errMsg.c_str());

        job.JobStatus = RcmJobStatus::FAILED;
        RcmJobError jobError(
            boost::lexical_cast<std::string>(jobErrorCode),
            jobErrorMessage,
            possibleCauses,
            recmAction,
            RcmJobSeverity::RCM_ERROR);

        job.Errors.push_back(jobError);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

void RcmWorkerThread::LogSourceReplicationStatus(const AgentSettings &settings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    static bool initialiUpdateComplete = false;
    static uint64_t lastLogTime = GetTimeInSecSinceEpoch1970();

    uint32_t waitInterval = initialiUpdateComplete ? m_SrcTelemeteryPollInterval : m_SrcTelemeteryStartDelay;
    uint64_t currentTime = GetTimeInSecSinceEpoch1970();
    if ((currentTime - lastLogTime) < waitInterval)
        return;

    uint32_t numDiskProtected = settings.m_protectedDiskIds.size();
    for (std::vector<std::string>::const_iterator dit = settings.m_protectedDiskIds.begin();
        dit != settings.m_protectedDiskIds.end(); dit++)
    {
        LogSourceReplicationStatus(*dit, LOG_PROTECTED, numDiskProtected);
    }

    initialiUpdateComplete = true;
    lastLogTime = currentTime;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void RcmWorkerThread::LogSourceReplicationStatus(
    const std::string& diskId,
    uint32_t reason,
    uint32_t numDiskProtected)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ReplicationStatus repStatus;

    ADD_INT_TO_MAP(repStatus, LOGREASON, reason);
    ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_AGENT_LOG);

    if (numDiskProtected)
        ADD_INT_TO_MAP(repStatus, NUMOFDISKPROT, numDiskProtected);

    uint64_t pairState = PAIR_STATE_DIFF;
    ADD_INT_TO_MAP(repStatus, PAIRSTATE, pairState);


    std::string deviceName;
    AgentLogExtendedProperties extProps;
    try {

        FillReplicationStatusFromSettings(repStatus, diskId, deviceName, extProps);

#ifdef SV_WINDOWS
        std::string deviceNameForDriverStats = diskId;
#else
        std::string deviceNameForDriverStats = deviceName;
#endif

        FillReplicationStatusFromDriverStats(repStatus, deviceNameForDriverStats);

        FillReplicationStatusWithSystemInfo(repStatus);

        FillReplicationStatusWithFabricInfo(repStatus, extProps);

        ADD_STRING_TO_MAP(repStatus, CUSTOMJSON, JSON::producer<AgentLogExtendedProperties>::convert(extProps));

        std::string strRepStatus = JSON::producer<ReplicationStatus>::convert(repStatus);

        gSrcTelemetryLog.Printf(strRepStatus);
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s : failed to serialize replication status. %s\n", FUNCTION_NAME, e.what());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void RcmWorkerThread::FillReplicationStatusFromDriverStats(
    ReplicationStatus &repStatus,
    const std::string& deviceName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!m_pDriverStream)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s : Filter driver is not initialized.\n", FUNCTION_NAME);

        ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_DRIVER_NOT_LOADED);
        ADD_INT_TO_MAP(repStatus, DRVLOADED, DISK_DRIVER_NOT_LOADED);

        return;
    }

    if (!m_pDriverStream->IsOpen())
    {
        FilterDrvIfMajor drvif;
        std::string driverName = drvif.GetDriverName(VolumeSummary::DISK);

        if (SV_SUCCESS == m_pDriverStream->Open(DeviceStream::Mode_Open | DeviceStream::Mode_Asynchronous | DeviceStream::Mode_RW | DeviceStream::Mode_ShareRW))
            DebugPrintf(SV_LOG_DEBUG, "Opened driver %s in async mode.\n", driverName.c_str());
        else
        {
            std::string errMsg("Failed to open driver");
            DebugPrintf(SV_LOG_ERROR, "%s : error %s %d.\n",
            FUNCTION_NAME, errMsg.c_str(), m_pDriverStream->GetErrorCode());

            ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_DRIVER_NOT_LOADED);
            ADD_INT_TO_MAP(repStatus, DRVLOADED, DISK_DRIVER_NOT_LOADED);

            return;
        }
    }

    FilterDrvIfMajor drvif;
    DRIVER_VERSION stDrvVersion;
    if (drvif.GetDriverVersion(m_pDriverStream.get(), stDrvVersion))
    {
        std::stringstream ssDrvVersion;
        ssDrvVersion << stDrvVersion.ulDrMajorVersion << "." << stDrvVersion.ulDrMinorVersion << ".";
        ssDrvVersion << stDrvVersion.ulDrMinorVersion2 << "." << stDrvVersion.ulDrMinorVersion3;

        std::stringstream ssDrvProdVersion;
        ssDrvProdVersion << stDrvVersion.ulPrMajorVersion << "." << stDrvVersion.ulPrMinorVersion << ".";
        ssDrvProdVersion << stDrvVersion.ulPrMinorVersion2 << "." << stDrvVersion.ulPrBuildNumber;

        ADD_STRING_TO_MAP(repStatus, DRVVER, ssDrvVersion.str());
        ADD_STRING_TO_MAP(repStatus, DRVPRODVER, ssDrvProdVersion.str());
        ADD_INT_TO_MAP(repStatus, DRVLOADED, DISK_DRIVER_LOADED);

        if (DRIVER_SUPPORTS_EXTENDED_VOLUME_STATS !=
            (stDrvVersion.ulDrMinorVersion3 & DRIVER_SUPPORTS_EXTENDED_VOLUME_STATS))
        {
            DebugPrintf(SV_LOG_DEBUG,
                "%s : Filter driver version %s doesn't support extended stats.\n",
                FUNCTION_NAME,
                ssDrvVersion.str().c_str());

            ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_DRIVER_STATS_NOT_SUPPORTED);

            return;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : Filter driver version ioctl failed.\n",
            FUNCTION_NAME);

        ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_DRIVER_VERSION_FAILED);
        return;
    }

    if (!deviceName.length())
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: not querying driver stats as device is not found\n.",
            FUNCTION_NAME);

        ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_AGENT_DISK_REMOVED);
        return;
    }

    FilterDrvVol_t v = drvif.GetFormattedDeviceNameForDriverInput(deviceName, VolumeSummary::DISK);

    size_t statSize;

#ifdef SV_WINDOWS
    statSize = sizeof(VOLUME_STATS_DATA) + sizeof(VOLUME_STATS);
#else
    statSize = sizeof(TELEMETRY_VOL_STATS);
#endif



    VOLUME_STATS_DATA* drvStats = 0;

    drvStats = (VOLUME_STATS_DATA *) new (std::nothrow) char[statSize];

    if (NULL == drvStats)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: failed to allocate memory for driver stats. size %d",
            FUNCTION_NAME,
            statSize);
        return;
    }

    memset((void *)drvStats, 0, statSize);

    if (drvif.GetDriverStats(deviceName,
        VolumeSummary::DISK,
        m_pDriverStream.get(),
        drvStats,
        statSize))
    {
        uint64_t nonPgMemLimit = drvStats->ulNonPagedMemoryLimitInMB;
        nonPgMemLimit *= 1024 * 1024;
        ADD_INT_TO_MAP(repStatus, NONPAGEDPOOLLIMIT, nonPgMemLimit);

        ADD_INT_TO_MAP(repStatus, LOCKDATABLKCNT, drvStats->LockedDataBlockCounter);

        ADD_INT_TO_MAP(repStatus, NUMDISKPROTBYDRV, drvStats->ulNumProtectedDisk);
        ADD_INT_TO_MAP(repStatus, TOTALNUMDISKBYDRV, drvStats->ulTotalVolumes);

        uint64_t drvFlags = 0;
        // 63-40 - 24 bits - reserved
        // 39-38 - 02 bits - drvStats->eDiskFilterMode
        // 37-36 - 02 bits - drvStats->eServiceState
        // 35-34 - 02 bits - drvStats->LastShutdownMarker
        // 33-32 - 02 bits - drvStats->PersistentRegistryCreated
        // 31-00 - 32 bits - drvStats->ulDriverFlags

        uint64_t flags = 0;
        flags = drvStats->eDiskFilterMode;
        drvFlags |= (flags << DISK_FILTER_MODE_SHIFT);

        flags = drvStats->eServiceState;
        drvFlags |= (flags << SERVICE_STATE_SHIFT);

        flags = drvStats->LastShutdownMarker;
        drvFlags |= (flags << LAST_SHUTDOWN_MARKER_SHIFT);

        flags = drvStats->PersistentRegistryCreated;
        drvFlags |= (flags << PERSISTENT_REG_CREATED_SHIFT);

        flags = drvStats->ulDriverFlags;
        drvFlags |= (flags << DRIVER_FLAGS_SHIFT);

        ADD_INT_TO_MAP(repStatus, DRVFLAGS, drvFlags);

        ADD_INT_TO_MAP(repStatus, SYSBOOTCNT, drvStats->ulCommonBootCounter);
        ADD_INT_TO_MAP(repStatus, DATAPOOLSIZESET, drvStats->ullDataPoolSizeAllocated);
        ADD_INT_TO_MAP(repStatus, PERSTIMESTAMPONLASTBOOT, drvStats->ullPersistedTimeStampAfterBoot);
        ADD_INT_TO_MAP(repStatus, PERSTIMESEQONLASTBOOT, drvStats->ullPersistedSequenceNumberAfterBoot);

        if (drvStats->ulVolumesReturned != 0)
        {
#ifdef SV_WINDOWS
            PVOLUME_STATS   volumeStats = (PVOLUME_STATS)((PUCHAR)drvStats + sizeof(VOLUME_STATS_DATA));
#else
            VOLUME_STATS_V2 *volumeStats = (VOLUME_STATS_V2 *)&(((TELEMETRY_VOL_STATS *)drvStats)->vol_stats);
#endif
            ADD_INT_TO_MAP(repStatus, DATAPOOLSIZEMAX, volumeStats->ullDataPoolSize);
            ADD_INT_TO_MAP(repStatus, DRVLOADTIME, volumeStats->liDriverLoadTime.QuadPart);

            ADD_INT_TO_MAP(repStatus, STOPALL, volumeStats->liStopFilteringAllTimeStamp.QuadPart);

            ADD_INT_TO_MAP(repStatus, TIMEBEFOREJUMP, volumeStats->llTimeJumpDetectedTS);
            ADD_INT_TO_MAP(repStatus, TIMEJUMPED, volumeStats->llTimeJumpedTS);

#ifdef SV_WINDOWS
            uint32_t fltState =
                (volumeStats->ulVolumeFlags & VSF_FILTERING_STOPPED) ? DISK_FILTERING_STOP : DISK_FILTERING_START;
            ADD_INT_TO_MAP(repStatus, FLTSTATE, fltState);
#endif

            ADD_INT_TO_MAP(repStatus, DEVICEDRVSIZE, volumeStats->ulVolumeSize.QuadPart);
            ADD_INT_TO_MAP(repStatus, DEVCONTEXTTIME, volumeStats->liVolumeContextCreationTS.QuadPart);
            ADD_INT_TO_MAP(repStatus, STARTFILTKERN, volumeStats->liStartFilteringTimeStamp.QuadPart);
            ADD_INT_TO_MAP(repStatus, STARTFILTUSER, volumeStats->liStartFilteringTimeStampByUser.QuadPart);

            ADD_INT_TO_MAP(repStatus, STOPFILTKERN, volumeStats->liStopFilteringTimeStamp.QuadPart);
            ADD_INT_TO_MAP(repStatus, STOPFILTUSER, volumeStats->liStopFilteringTimestampByUser.QuadPart);

            ADD_INT_TO_MAP(repStatus, CLEARDIFF, volumeStats->liClearDiffsTimeStamp.QuadPart);
            ADD_INT_TO_MAP(repStatus, CHURN, volumeStats->ullTotalTrackedBytes);

            ADD_INT_TO_MAP(repStatus, LASTS2START, volumeStats->liLastS2StartTime.QuadPart);
            ADD_INT_TO_MAP(repStatus, LASTS2STOP, volumeStats->liLastS2StopTime.QuadPart);

            ADD_INT_TO_MAP(repStatus, LASTSVCSTART, volumeStats->liLastAgentStartTime.QuadPart);
            ADD_INT_TO_MAP(repStatus, LASTSVCSTOP, volumeStats->liLastAgentStopTime.QuadPart);

            ADD_INT_TO_MAP(repStatus, LASTCOMMITDB, volumeStats->liCommitDBTimeStamp.QuadPart);
            ADD_INT_TO_MAP(repStatus, LASTDRAINDB, volumeStats->liGetDBTimeStamp.QuadPart);

            ADD_INT_TO_MAP(repStatus, LASTTAGREQTIME, volumeStats->liLastTagReq.QuadPart);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: driver stats not returned for disk %s\n.",
                FUNCTION_NAME,
                deviceName.c_str());

            ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_AGENT_DISK_REMOVED);
        }
    }

    if (NULL != drvStats)
        delete drvStats;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void RcmWorkerThread::FillReplicationStatusFromSettings(ReplicationStatus &repStatus,
    const std::string &diskId,
    std::string &deviceName,
    AgentLogExtendedProperties& extProps)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ADD_STRING_TO_MAP(repStatus, PROTDISKID, diskId);
    ADD_INT_TO_MAP(repStatus, PAIRSTATE, PAIR_STATE_DIFF);

    ADD_INT_TO_MAP(repStatus, DISKAVB, DISK_NOT_AVAILABLE);

    VolumeReporterImp vrimp;
    VolumeReporter::DiskReports_t drs;
#ifdef SV_WINDOWS
    vrimp.GetOsDiskReport(diskId, m_volumesCache.m_Vs, drs);
#else 
    if (boost::starts_with(diskId, strBootDisk) ||
        boost::starts_with(diskId, strRootDisk))
    {
        vrimp.GetBootDiskReport(m_volumesCache.m_Vs, drs);
    }
    else if (boost::starts_with(diskId, strDataDisk))
    {
        vrimp.GetOsDiskReport(diskId, m_volumesCache.m_Vs, drs);
    }
    else
    {
        std::string errMsg("Invalid disk id format for ");
        errMsg += diskId;
        errMsg += ". Expecting /dev/<name> or DataDisk<num>.";
        DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
        ADD_STRING_TO_MAP(repStatus, MESSAGE, errMsg);
        ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_AGENT_DISK_CACHE_NOT_FOUND);

        return;
    }
#endif

    VolumeReporter::ConstDiskReportsIter_t diter = drs.begin();
    for ( /* empty */; diter != drs.end(); diter++)
    {
        ADD_INT_TO_MAP(repStatus, RAWSIZEAGENT, diter->m_RawCapacity);
        ADD_INT_TO_MAP(repStatus, DISKAVB, DISK_AVAILABLE);

        ConstAttributesIter_t iter = diter->attributes.find(NsVolumeAttributes::DEVICE_NAME);
        if (iter != diter->attributes.end())
        {
            ADD_STRING_TO_MAP(repStatus, DISKNAME, iter->second);
            deviceName = iter->second;
        }

        iter = diter->attributes.find(NsVolumeAttributes::SCSI_LOGICAL_UNIT);
        if (iter != diter->attributes.end())
        {
            extProps.ExtendedProperties.insert(std::make_pair(NsVolumeAttributes::SCSI_LOGICAL_UNIT, iter->second));
        }

        iter = diter->attributes.find(NsVolumeAttributes::HAS_BOOTABLE_PARTITION);
        if (iter != diter->attributes.end())
        {
            extProps.ExtendedProperties.insert(std::make_pair(NsVolumeAttributes::HAS_BOOTABLE_PARTITION, iter->second));
        }
    }

    uint32_t numDiskDiscovered = 0;
    vrimp.GetNumberOfNativeDisks(m_volumesCache.m_Vs, numDiskDiscovered);

    ADD_INT_TO_MAP(repStatus, NUMDISKBYAGENT, numDiskDiscovered);

    AgentSettings settings;
    SVSTATUS status = RcmConfigurator::getInstance()->GetAgentSettings(settings);
    if (SVS_OK != status)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to fetch cached replication settings %d\n", FUNCTION_NAME, status);
        return;
    }

    std::vector<SyncSettings>::const_iterator syncSettingsItr = settings.m_syncSettings.begin();
    for (/*empty*/; syncSettingsItr != settings.m_syncSettings.end(); syncSettingsItr++)
    {
        if (diskId == syncSettingsItr->DiskId)
        {
            ADD_INT_TO_MAP(repStatus, PAIRSTATE, PAIR_STATE_RESYNC);
            break;
        }
    }

    if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
    {
        FillReplicationStatusFromOnPremSettings(repStatus, diskId);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void RcmWorkerThread::FillReplicationStatusFromOnPremSettings(ReplicationStatus& repStatus,
    const std::string& diskId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    uint32_t port;
    svector_t vProxyAddresses;
    RcmConfigurator::getInstance()->GetRcmProxyAddress(vProxyAddresses, port);

    std::string proxyAddrList;
    for (svectoriter_t iter = vProxyAddresses.begin(); iter != vProxyAddresses.end(); iter++)
    {
        if (!proxyAddrList.empty())
            proxyAddrList += ",";
        proxyAddrList += *iter;
    }

    ADD_STRING_TO_MAP(repStatus, CSIPADDR, proxyAddrList);

    std::string dataPathTransportType, dataPathTransportSettings;
    RcmConfigurator::getInstance()->GetDataPathTransportSettings(dataPathTransportType, dataPathTransportSettings);

    if (boost::iequals(dataPathTransportType, TRANSPORT_TYPE_PROCESS_SERVER))
    {
        ProcessServerTransportSettings psTransportSettings =
            JSON::consumer<ProcessServerTransportSettings>::convert(dataPathTransportSettings, true);

        ADD_STRING_TO_MAP(repStatus, PSHOSTID, psTransportSettings.Id);
        ADD_STRING_TO_MAP(repStatus, PSIPADDR, psTransportSettings.GetIPAddress());
        ADD_STRING_TO_MAP(repStatus, PSPORT, boost::lexical_cast<std::string>(psTransportSettings.Port));
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void RcmWorkerThread::FillReplicationStatusWithSystemInfo(ReplicationStatus &repStatus)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    LocalConfigurator lc;
    std::string hostName;
    // hostname
    if (lc.getUseConfiguredHostname()) {
        hostName = lc.getConfiguredHostname();
    }
    else {
        hostName = Host::GetInstance().GetHostName();
    }

    // ip address
    std::string ipAddress;
    if (lc.getUseConfiguredIpAddress()) {
        ipAddress = lc.getConfiguredIpAddress();
    }
    else {
        ipAddress = Host::GetInstance().GetIPAddress();
    }

    ADD_STRING_TO_MAP(repStatus, HOSTNAME, hostName);
    ADD_STRING_TO_MAP(repStatus, HOSTIPADDR, ipAddress);

    std::string macAddrs;
    NicInfos_t nicinfos;
    GetNicInfos(nicinfos);
    ConstNicInfosIter_t nicIter = nicinfos.begin();
    while (nicIter != nicinfos.end())
    {
        macAddrs += nicIter->first;
        boost::trim(macAddrs);
        macAddrs += ",";
        nicIter++;
    }

    ADD_STRING_TO_MAP(repStatus, MACADDR, macAddrs);

    const Object & osinfo = OperatingSystem::GetInstance().GetOsInfo();
    ConstAttributesIter_t iter = osinfo.m_Attributes.find(NSOsInfo::CAPTION);
    if (iter != osinfo.m_Attributes.end())
        ADD_STRING_TO_MAP(repStatus, SRCOSVER, iter->second);

    ADD_INT_TO_MAP(repStatus, SYSTIME, GetTimeInSecSinceEpoch1970());
    ADD_INT_TO_MAP(repStatus, RAMSIZE, Host::GetInstance().GetAvailableMemory());

    unsigned long long freeMemory = 0;
    if (Host::GetInstance().GetFreeMemory(freeMemory))
        ADD_INT_TO_MAP(repStatus, AVBPHYMEM, freeMemory);

    unsigned long long sysUpTime = 0;
    if (Host::GetInstance().GetSystemUptimeInSec(sysUpTime))
    {
        ADD_INT_TO_MAP(repStatus, SYSUPTIME, sysUpTime);

        uint64_t sysBootTime = GetTimeInSecSinceEpoch1970();
        sysBootTime -= sysUpTime;
        ADD_INT_TO_MAP(repStatus, SYSBOOTTIME, sysBootTime);
    }

    ADD_STRING_TO_MAP(repStatus, SRCAGENTVER, InmageProduct::GetInstance().GetProductVersion());

    std::string strKernelVersion;
    GetKernelVersion(strKernelVersion);
    ADD_STRING_TO_MAP(repStatus, KERNELVERSION, strKernelVersion);

    ADD_STRING_TO_MAP(repStatus, AGENTRESOURCEID, lc.getResourceId());
    ADD_STRING_TO_MAP(repStatus, AGENTSOURCEGROUPID, lc.getSourceGroupId());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void RcmWorkerThread::FillReplicationStatusWithFabricInfo(ReplicationStatus& repStatus,
    AgentLogExtendedProperties& extProps)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    // only for Azure VMs
    // TBD: add Azure stack VMs check too
    if (!RcmConfigurator::getInstance()->IsAzureToAzureReplication() &&
        !RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
    {
        return;
    }

    StorageProfile storageProfile;
    if (SVS_OK == GetAzureVmStorageProfile(storageProfile))
    {
        Disk disk;
        std::map<std::string, std::string>::iterator iter;

        iter = extProps.ExtendedProperties.find(NsVolumeAttributes::HAS_BOOTABLE_PARTITION);
        if (iter != extProps.ExtendedProperties.end())
        {
            disk = storageProfile.osDisk;
        }
        else
        {
            std::string lunId;
            iter = extProps.ExtendedProperties.find(NsVolumeAttributes::SCSI_LOGICAL_UNIT);
            if (iter != extProps.ExtendedProperties.end())
            {
                lunId = iter->second;
            }
            std::vector<Disk>::iterator iterDataDisks = storageProfile.dataDisks.begin();
            for (/*empty*/; iterDataDisks != storageProfile.dataDisks.end(); iterDataDisks++)
            {
                if (lunId == iterDataDisks->lun)
                {
                    disk = *iterDataDisks;
                    break;
                }
            }
        }

        if (!disk.name.empty())
        {
            std::string diskProps = JSON::producer<Disk>::convert(disk);
            extProps.ExtendedProperties.insert(std::make_pair("ArmDiskProperties", diskProps));
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

SVSTATUS RcmWorkerThread::PerformMigratePreJobProcesssingTasks()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    LocalConfigurator lc;
    AgentSettings agentSettings;
    ACE_Time_Value waitTime(Migration::RCMSettingPollSleepTime, 0);
    
    int retryCount = 0;
    std::string errMsg;

    do
    {
        status = RcmConfigurator::getInstance()->GetAgentSettings(agentSettings);
        if (status != SVS_OK
            || agentSettings.m_protectedDiskIds.size() == 0)
        {
            retryCount++;
            m_QuitEvent->wait(&waitTime, 0);
            status = SVE_INVALID_FORMAT;
            continue;
        }
        DebugPrintf(SV_LOG_ALWAYS, "%s Agent settings from RCM fetch success, No of disks =%d \n",
            FUNCTION_NAME, agentSettings.m_protectedDiskIds.size());

#ifdef SV_UNIX
        status = CreateDriverStream();
        if (status != SVS_OK)
        {
            errMsg = "creation of driver stream failed";
            break;
        }
        status = UpdatePnames(agentSettings.m_protectedDiskIds, errMsg);
#endif        
        break;
    } while (!m_threadManager.testcancel(ACE_Thread::self())
        && status != SVS_OK 
        && retryCount < Migration::MaxRCMSettingPollRetryCount);

    if (m_threadManager.testcancel(ACE_Thread::self()))
    {
        status = SVE_ABORT;
        DebugPrintf(SV_LOG_ERROR, "%s migration aborted \n", FUNCTION_NAME);
    }
    else if (status != SVS_OK)
    {
        if (errMsg.empty())
        {
            errMsg = "timeout fetching the rcm settings after migration.";
        }

        DebugPrintf(SV_LOG_ERROR, "%s migration to rcm failed in pretasks with error=%s \n",
            FUNCTION_NAME, errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmWorkerThread::GetAzureVmStorageProfile(StorageProfile& storageProfile)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    static StorageProfile s_storageProfile;
    static steady_clock::time_point s_lastupdatetime = steady_clock::time_point::min();

    SVSTATUS status = SVS_OK;
    if (steady_clock::now() < (s_lastupdatetime + steady_clock::duration(seconds(10))))
    {
        storageProfile = s_storageProfile;
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    try {

        HttpClient client(false);
        HttpRequest request(AzureImdUri);
        HttpResponse response;
        HttpProxy proxy;
        proxy.m_bypasslist = "*";
        client.SetProxy(proxy);
        request.AddHeader("Metadata", "true");

        if (!client.GetResponse(request, response))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: request failed with error %d: %s\n", FUNCTION_NAME, response.GetErrorCode(), response.GetErrorString().c_str());
        }
        else
        {
            if (response.GetResponseCode() == HttpErrorCode::OK)
            {
                std::string jsonOutput = response.GetResponseData();
                if (std::string::npos != jsonOutput.find_first_of('{'))
                {
                    DebugPrintf(SV_LOG_ALWAYS, "%s: storage profile is %s\n", FUNCTION_NAME, jsonOutput.c_str());
                    storageProfile = JSON::consumer<StorageProfile>::convert(jsonOutput, true);

                    s_storageProfile = storageProfile;
                    s_lastupdatetime = steady_clock::now();
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: response is not in JSON. %s\n", FUNCTION_NAME, jsonOutput.c_str());
                    status = SVE_FAIL;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: server returned error %d: %s\n", FUNCTION_NAME, response.GetResponseCode(), response.GetResponseData().c_str());
                status = SVE_HTTP_RESPONSE_FAILED;
            }
        }
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with exception %s.\n", FUNCTION_NAME, e.what());
        status = SVE_ABORT;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unknown exception.\n", FUNCTION_NAME);
        status = SVE_ABORT;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

bool RcmWorkerThread::MoveProtectedDevDetailsCache()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool bstatus = true;

    do 
    {
        boost::system::error_code ec;
        std::string deprecatedDevDetailsCachePath;
        bstatus = LocalConfigurator::getDeprecatedVxProtectedDeviceDetailCachePathname(deprecatedDevDetailsCachePath);
        if (!bstatus)
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: failed to find deprecated protected device details file as failed to get path from config.\n",
                FUNCTION_NAME);
            bstatus = false;
            break;
        }

        std::string devDetailsCachePath;
        bstatus = LocalConfigurator::getVxProtectedDeviceDetailCachePathname(devDetailsCachePath);
        if (!bstatus)
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: failed to find protected device details file as failed to get path from config.\n",
                FUNCTION_NAME);
            bstatus = false;
            break;
        }

        if (!boost::filesystem::exists(deprecatedDevDetailsCachePath, ec))
        {
            DebugPrintf(SV_LOG_DEBUG,
                "%s: protected device details file not found in old location %s. Error %d (%s).\n",
                FUNCTION_NAME,
                deprecatedDevDetailsCachePath.c_str(),
                ec.value(),
                ec.message().c_str());

                bstatus = true;
                break;
        }

        if (boost::filesystem::exists(devDetailsCachePath, ec))
        {
            DebugPrintf(SV_LOG_DEBUG,
                "%s: protected device details file found in new location %s.\n",
                FUNCTION_NAME,
                devDetailsCachePath.c_str());

                bstatus = true;
                break;
        }

        std::string newDir = dirname_r(devDetailsCachePath);
        if (!boost::filesystem::exists(newDir, ec))
        {
            boost::filesystem::create_directory(newDir, ec);

            if (ec)
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: creating directory %s failed. Error %d (%s).\n",
                    FUNCTION_NAME,
                    ec.value(),
                    ec.message().c_str());

                bstatus = false;
                break;
            }

            securitylib::setPermissions(newDir, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
        }

        boost::filesystem::rename(deprecatedDevDetailsCachePath, devDetailsCachePath, ec);
        if (ec)
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: moving protected device details file to new location failed. Error %d (%s).\n",
                FUNCTION_NAME,
                ec.value(),
                ec.message().c_str());
            bstatus = false;
            break;
        }

        securitylib::setPermissions(devDetailsCachePath);

        DebugPrintf(SV_LOG_ALWAYS,
            "%s: protected device details file moved to new location %s.\n",
            FUNCTION_NAME,
            devDetailsCachePath.c_str());

        if (!boost::filesystem::exists(devDetailsCachePath, ec))
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: protected device details file not found in new location %s. Error %d (%s).\n",
                FUNCTION_NAME,
                devDetailsCachePath.c_str(),
                ec.value(),
                ec.message().c_str());

                bstatus = false;
                break;
        }
    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return bstatus;
}

SVSTATUS RcmWorkerThread::PersistALRConfig(ALRMachineConfig& alrMachineConfig,
    std::string& errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string alrConfigPath;

    // write and persist the ALR details to disk
    do {
        try
        {
            alrConfigPath = RcmConfigurator::getInstance()->GetALRConfigPath();
            status = AgentConfigHelpers::PersistALRMachineConfig(alrConfigPath,
                alrMachineConfig, errMsg);
            if (status != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
                break;
            }

            FIO::Fio oFio(alrConfigPath.c_str(), FIO::FIO_RW_ALWAYS | FIO::FIO_WRITE_THROUGH);
            if (!oFio.flushToDisk())
            {
                status = SVE_FAIL;
                errMsg = "Failed to update " + alrConfigPath + " with error " +
                    oFio.errorAsString();
                DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            }
            oFio.close();
        }
        catch (const std::exception& e)
        {
            errMsg = "Failed to update " + alrConfigPath + " with exception " + e.what();
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            status = SVE_FAIL;
        }
    }
    while(false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmWorkerThread::IssueFinalBookmark(boost::shared_ptr<RcmJob> ptrJob)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    std::string errMsg;
    uint32_t jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_FINAL_BOOKMARK_ERROR;
    std::string jobErrorMessage = "Failed to issue final bookmark.";
    std::string possibleCauses = "Failed to process the final bookmark settings.";
    std::string  recmAction = "Contact support if the issue persists.";

    boost::thread *jobThread;
    boost::thread_group threadGroup;
    HandlerMap_t::iterator stopIter = m_handlers.find(RcmWorkerThreadHandlerNames::VACPSTOP);
    HandlerMap_t::iterator startIter = m_handlers.find(RcmWorkerThreadHandlerNames::VACPRESTART);
    boost::asio::deadline_timer commitTimeoutTimer(m_ioService);

    try {
        do {
            TagCommitNotifyState expected = TAG_COMMIT_NOTIFY_STATE_NOT_INPROGRESS;
            if (!m_commitTag.CheckAnotherInstance(expected))
            {
                errMsg = "Another instance of tag commit notify is inprogress. state: " + boost::lexical_cast<std::string>(expected);
                possibleCauses = "A final bookmark processing is already pending at mobility service.";
                recmAction = "Retry the operation after previous operation completes.";
                status = SVE_INVALIDARG;
                break;
            }

            if (!m_pDriverStream->IsOpen())
            {
                possibleCauses = "The data change tracking module of the mobility service is not loaded on the VM.";
                recmAction = "The kernel version of the running operating system kernel on the source machine is not supported on the version of the mobility service installed on the source machine.";
                errMsg = "Filter driver is not initialized.";
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_NOT_INIT_ERROR;
                status = SVE_FAIL;
                break;
            }

            if (!m_commitTag.IsCommitTagNotifySupported())
            {
                possibleCauses = "The data change tracking module of the mobility service does not support the final bookmark feature.";
                recmAction = "Update to the latest version of the mobility service. A reboot is required to load the newer version.";
                errMsg = "Filter driver version doesn't support commit tag notify.";
                jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_COMMIT_NOTIFY_NOT_SUPPORTED_ERROR;
                status = SVE_FAIL;
                break;
            }

            if (stopIter == m_handlers.end() ||
                startIter == m_handlers.end())
            {
                errMsg = "Failed to find handler to start and stop VACP.";
                possibleCauses = "Internal error of mobility service.";
                recmAction = "Restart the mobility service and retry the operation.";
                status = SVE_FAIL;
                break;
            }

            AzureToOnPremSourceAgentIssueFinalReplicationBookmark replicationBookmarkInput =
                JSON::consumer<AzureToOnPremSourceAgentIssueFinalReplicationBookmark>::convert(
                ptrJob->InputPayload);

            ALRMachineConfig alrMachineConfig = replicationBookmarkInput;
            status = PersistALRConfig(alrMachineConfig, errMsg);
            if (status != SVS_OK)
            {
                possibleCauses = "Internal error of mobility service.";
                recmAction = "Restart the mobility service and retry the operation.";
                break;
            }

            CommitTagBookmarkInput commitTagBookmarkInput;
            std::string vacpArgs, appPolicyLogPath, preLogPath;

            commitTagBookmarkInput.BookmarkInput = replicationBookmarkInput;
            status = m_commitTag.PrepareFinalBookmarkCommand(commitTagBookmarkInput, ptrJob->Id,
                vacpArgs, appPolicyLogPath, preLogPath);
            if (status != SVS_OK)
            {
                errMsg = "No consistency settings found.";
                possibleCauses = "Internal error of mobility service.";
                recmAction = "Restart the mobility service and retry the operation.";
                status = SVE_FAIL;
                break;
            }

            // stop regular consistency bookmarks
            stopIter->second();

            // issue commit tag notify
            jobThread = new boost::thread(boost::bind(&RcmWorkerThread::IssueCommitTagNotify, this, ptrJob));
            threadGroup.add_thread(jobThread);

            DebugPrintf(SV_LOG_ALWAYS, "waiting for commit tag ioctl to be issued to driver.\n", FUNCTION_NAME);

            do {
                // wait until the ioctl is issued
                ACE_Time_Value waitTime(10, 0);
                m_QuitEvent->wait(&waitTime, 0);
            } while (!m_threadManager.testcancel(ACE_Thread::self()) && (m_commitTag.GetTagCommitNotifierState() == TAG_COMMIT_NOTIFY_STATE_NOT_ISSUED));

            if (m_threadManager.testcancel(ACE_Thread::self()))
            {
                // no need to update the job on service quit
                DebugPrintf(SV_LOG_ALWAYS, "skipping final bookmark command on quit\n");
                return SVE_ABORT;
            }

            if (m_commitTag.GetTagCommitNotifierState() > TAG_COMMIT_NOTIFY_STATE_WAITING_FOR_COMPLETION)
            {
                // the job status is set by the IssueCommitNotify, so no need to update the status here
                errMsg = "tag commit notify completed before issuing final bookmark command.";
                status = SVE_FAIL;
                break;
            }

            // set timeout to a little bit more than the max tag commit time for vacp process to run
            SV_UINT uTimeOut = (RcmConfigurator::getInstance()->getVacpTagCommitMaxTimeOut() / 1000) + 30;

            commitTimeoutTimer.expires_from_now(boost::posix_time::seconds(uTimeOut + 30));
            commitTimeoutTimer.async_wait(boost::bind(&TagCommitNotifier::HandleCommitTagNotifyTimeout, &m_commitTag, boost::asio::placeholders::error));

            SV_ULONG lInmexitCode;
            lInmexitCode = m_commitTag.RunFinalBookmarkCommand(vacpArgs, appPolicyLogPath, preLogPath, uTimeOut, m_active);

            if (lInmexitCode != 0)
            {
                status = SVE_FAIL;
                if (lInmexitCode == VACP_DISK_NOT_IN_DATAMODE)
                {
                    possibleCauses = "The disk is not in write order mode to issue the bookmark.";
                }
                break;
            }
        } while (0);
    }
    RCM_WORKER_THREAD_CATCH_EXCEPTION(errMsg, status);

    threadGroup.join_all();
    commitTimeoutTimer.cancel();

    if (status != SVE_INVALIDARG)
    {
        m_commitTag.SetTagCommitNotifierState(TAG_COMMIT_NOTIFY_STATE_NOT_INPROGRESS);
    }

    // restart regular consistency bookmarks
    if (startIter != m_handlers.end())
        startIter->second();

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "failed to issue final bookmark for input : Job Id %s Input %s error %s\n",
            ptrJob->Id.c_str(),
            ptrJob->InputPayload.c_str(),
            errMsg.c_str());

        // set the status here only if the IssueCommitTagNotify() has not set
        if (ptrJob->JobStatus == RcmJobStatus::NONE)
        {
            ptrJob->JobStatus = RcmJobStatus::FAILED;
            RcmJobError jobError(
                boost::lexical_cast<std::string>(jobErrorCode),
                jobErrorMessage,
                possibleCauses,
                recmAction,
                RcmJobSeverity::RCM_ERROR);

            ptrJob->Errors.push_back(jobError);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

void RcmWorkerThread::IssueCommitTagNotify(boost::shared_ptr<RcmJob> ptrJob)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    std::string errMsg;
    std::stringstream ssPossibleCauses;
    CommitTagBookmarkInput commitTagBookmarkInput;
    CommitTagBookmarkOutput commitTagBookmarkOutput;
    AzureToOnPremSourceAgentIssueFinalReplicationBookmark replicationBookmarkInput;
    SourceAgentAzureToOnPremIssueFinalReplicationBookmarkOutput replicationBookmarkOutput;
    std::vector<std::string> protectedDiskIds;

    uint32_t jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_FINAL_BOOKMARK_ERROR;
    std::string jobErrorMessage = "Failed to issue final bookmark.";
    std::string possibleCauses = "Failed to upload the final bookmarks.";
    std::string  recmAction = "Please retry the operation. Contact support if the issue persists.";

    try {
        do {
            replicationBookmarkInput =
                JSON::consumer<AzureToOnPremSourceAgentIssueFinalReplicationBookmark>::convert(
                ptrJob->InputPayload);
            commitTagBookmarkInput.BookmarkInput = replicationBookmarkInput;

            status = RcmConfigurator::getInstance()->GetProtectedDiskIds(protectedDiskIds);
            if (status != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR, "%s Failed to get protected deviceIds with %d\n",
                    FUNCTION_NAME,
                    status);
                break;
            }

            status = m_commitTag.IssueCommitTagNotify(commitTagBookmarkInput,
                commitTagBookmarkOutput, protectedDiskIds, ssPossibleCauses);
            replicationBookmarkOutput = commitTagBookmarkOutput.BookmarkOutput;
            if (status != SVS_OK)
            {
                if (!ssPossibleCauses.str().empty())
                {
                    possibleCauses = ssPossibleCauses.str();
                    errMsg = ssPossibleCauses.str();
                }
                break;
            }

            std::string strJobOutput =
                JSON::producer<SourceAgentAzureToOnPremIssueFinalReplicationBookmarkOutput>::convert(
                replicationBookmarkOutput);

            ptrJob->OutputPayload = strJobOutput;
            ptrJob->JobStatus = RcmJobStatus::SUCCEEDED;

        } while (false);
    }
    RCM_WORKER_THREAD_CATCH_EXCEPTION(errMsg, status);

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "failed to issue commit tag notify : Job Id %s Input %s error %s\n",
            ptrJob->Id.c_str(),
            ptrJob->InputPayload.c_str(),
            errMsg.c_str());

        ptrJob->JobStatus = RcmJobStatus::FAILED;

        RcmJobError jobError(
            boost::lexical_cast<std::string>(jobErrorCode),
            jobErrorMessage,
            possibleCauses,
            recmAction,
            RcmJobSeverity::RCM_ERROR);

        ptrJob->Errors.push_back(jobError);
    }

    m_commitTag.SetTagCommitNotifierState(TAG_COMMIT_NOTIFY_STATE_JOB_STATUS_UPDATED);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

SVSTATUS RcmWorkerThread::SwitchBackToCsStack(RcmJob& job)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;
    uint32_t jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_SWITCH_BACK_TO_CS_STACK_ERROR;
    std::string jobErrorMessage = "Failed to switch back to CS stack.";
    std::string possibleCauses = "Failed to set migration state.";
    std::string recmAction = "Contact support if the issue persists.";
    Migration::State state;

    try {
        RcmConfigurator::getInstance()->setMigrationState(
            Migration::MIGRATION_ROLLBACK_PENDING);

        job.JobStatus = RcmJobStatus::SUCCEEDED;
    }
    RCM_WORKER_THREAD_CATCH_EXCEPTION(errMsg, status);

    if (status != SVS_OK)
    {
        state = RcmConfigurator::getInstance()->getMigrationState();
        DebugPrintf(SV_LOG_ERROR,
            "Prepare switch Appliance failed, State : %d. Job Id %s Input %s error %d %s\n",
            state,
            job.Id.c_str(),
            job.InputPayload.c_str(),
            jobErrorCode,
            errMsg.c_str());

        job.JobStatus = RcmJobStatus::FAILED;
        RcmJobError jobError(
            boost::lexical_cast<std::string>(jobErrorCode),
            jobErrorMessage,
            possibleCauses,
            recmAction,
            RcmJobSeverity::RCM_ERROR);

        job.Errors.push_back(jobError);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS RcmWorkerThread::CsStackMachineStartDraining(RcmJob& job)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;
    std::stringstream errStream;
    uint32_t jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_CS_STACK_MACHINE_START_DRAINING_ERROR;
    std::string jobErrorMessage = "Failed to start draining to the Appliance.";
    std::string possibleCauses = "Failed to start draining to Appliance";
    std::string rcmAction = "Contact support if the issue persists.";

    try
    {
        do {
            RcmClientLib::OnPremToAzureCsStackMachineStartDrainingInput startDraingJobInput =
                JSON::consumer<RcmClientLib::OnPremToAzureCsStackMachineStartDrainingInput>::convert(job.InputPayload);      
            status = CreateDriverStream();
            if (status != SVS_OK)
            {
                errMsg += "Driver stream creation failed.";
                break;
            }

            status = m_commitTag.UnblockDrain(startDraingJobInput.ProtectedDiskIds, errStream);
            if (status != SVS_OK)
            {
                errMsg += "Unblock of the draining with appliance failed.";
                errMsg += errStream.str();
                break;
            }
            job.JobStatus = RcmJobStatus::SUCCEEDED;
        } while (false);
    }
    catch (const JSON::json_exception& jsone)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to unmarshall job input with exception %s.\n", FUNCTION_NAME, jsone.what());
        errMsg = "Failed marshalling OnPremToAzureCsStackMachineStartDrainingInput with error=" + std::string(jsone.what());
        status = SVE_INVALID_FORMAT;
    }
    RCM_WORKER_THREAD_CATCH_EXCEPTION(errMsg, status);
    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s CsStackMachineStartDraining job failed. Job Id: %s ,Input: %s, driver error code: %d, error: %d %s\n",
            FUNCTION_NAME,
            job.Id.c_str(),
            job.InputPayload.c_str(),
            m_pDriverStream->GetErrorCode(),
            jobErrorCode,
            errMsg.c_str());

        job.JobStatus = RcmJobStatus::FAILED;

        RcmJobError jobError(
            boost::lexical_cast<std::string>(jobErrorCode),
            jobErrorMessage,
            possibleCauses,
            rcmAction,
            RcmJobSeverity::RCM_ERROR);

        job.Errors.push_back(jobError);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

#ifdef SV_WINDOWS
SVSTATUS RcmWorkerThread::SourceAgentStartSharedDisksChangeTracking(RcmJob& job)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;
    std::stringstream errStream;
    uint32_t jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_SHARED_DISK_START_TRACKING_ERROR;
    std::string jobErrorMessage = "Failed to start tracking shared disk.";
    std::string possibleCauses = "Failed to start tracking shared disk";
    std::string rcmAction = "Contact support if the issue persists.";

    try {
        do {
            
            std::string jobInputPayload = securitylib::base64Decode(job.InputPayload.c_str(),
                job.InputPayload.length());

            RcmClientLib::SourceAgentStartSharedDisksChangeTrackingInput startDraingJobInput =
                JSON::consumer<RcmClientLib::SourceAgentStartSharedDisksChangeTrackingInput>::convert(jobInputPayload);

            status = CreateDriverStream();
            if (status != SVS_OK)
            {
                errMsg += "Driver stream creation failed.";
                break;
            }

            VolumeReporter::VolumeReport_t vr;

            std::vector<std::string>::const_iterator sharedDiskItr =
                startDraingJobInput.ProtectedSharedDiskIds.begin();
            for (/*empty*/;
                sharedDiskItr != startDraingJobInput.ProtectedSharedDiskIds.end();
                sharedDiskItr++)
            {
                status = GetVolumeReport(*sharedDiskItr, vr, errMsg);
                if (status != SVS_OK)
                {
                    possibleCauses = "Node is not part of the cluster having the Shared Disk="+ (*sharedDiskItr);
                    rcmAction = "If the node is no longer part of cluster then please remove the same in portal and retry the job.";
                    errMsg = "If the node is no longer part of cluster then please remove the same in portal and retry the job.";
                    jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DEVICE_NOT_FOUND_ERROR;
                    break;
                }

                DpDriverComm ddc(*sharedDiskItr, vr.m_VolumeType);
                if (!ddc.SetClusterFiltering())
                {
                    status = SVE_FAIL;
                    errMsg += " Set Cluster Filtering ioctl failed.";
                    possibleCauses = "The driver failed to set clusterfiltering.";
                    errMsg = "Check driver logs for ioctl failure.";
                    jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_IOCTL_ERROR;
                    break;
                }

                ddc.StartFiltering(vr, &m_volumesCache.m_Vs);

                status = SVS_OK;
                job.JobStatus = RcmJobStatus::SUCCEEDED;
            }

        } while (false);
    }
    catch (DataProtectionException& e)
    {
        status = SVE_ABORT;
        possibleCauses = "Start Filtering failed for shared disk.";
        rcmAction = "Contact support if the issue persists.";
        jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_DRIVER_IOCTL_EXCEPTION;
        errMsg += e.what();
    }
    catch (JSON::json_exception& jsone)
    {
        errMsg += jsone.what();
        possibleCauses = "Failed to parse the start shared disk tracking.";
        jobErrorCode = RcmClientLib::RCM_CLIENT_JOB_SETTINGS_PARSE_ERROR;
        status = SVE_ABORT;
    }
    RCM_WORKER_THREAD_CATCH_EXCEPTION(errMsg, status);

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "failed to start shared disk filtering, Job Id %s Input %s error %s\n",
            job.Id.c_str(),
            job.InputPayload.c_str(),
            errMsg.c_str());

        job.JobStatus = RcmJobStatus::FAILED;
        RcmJobError jobError(
            boost::lexical_cast<std::string>(jobErrorCode),
            jobErrorMessage,
            possibleCauses,
            rcmAction,
            RcmJobSeverity::RCM_ERROR);

        job.Errors.push_back(jobError);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}
#endif

#ifdef SV_UNIX
SVSTATUS RcmWorkerThread::UpdatePnames(const std::vector<std::string>& diskIdsFromRcm, std::string& errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    VxProtectedDeviceDetail deviceDetails;
    std::string diskId;
    std::string prevPname;
    std::string prevDiskDrDetailsPath;
    std::string newDiskDrDetailsPath;
    std::string oldBitmapPath;
    std::string newBitmapPath;
    std::stringstream errStream;
    bool res;

    for (std::vector<std::string>::const_iterator diskIter = diskIdsFromRcm.begin();
        diskIter != diskIdsFromRcm.end(); diskIter++)
    {
        diskId = *diskIter;
        res = m_ptrDevDetailsPersistentStore->GetMap(diskId, deviceDetails);
        if (!res)
        {
            errMsg = "Disk Ids in job input are not mapped in presistent store";
            DebugPrintf(SV_LOG_ERROR, "%s DiskId=%s does not have entry in presistent store. INVALID JOB INPUT.\n", FUNCTION_NAME, diskId.c_str());
            status = SVE_INVALIDARG;
            break;
        }
        prevPname = boost::replace_all_copy(deviceDetails.m_deviceName, "/", "_");
        
        prevDiskDrDetailsPath = DRIVER_PRESISTENT_DIR;
        prevDiskDrDetailsPath += ACE_DIRECTORY_SEPARATOR_CHAR;
        prevDiskDrDetailsPath += prevPname;

        newDiskDrDetailsPath = DRIVER_PRESISTENT_DIR;
        newDiskDrDetailsPath += ACE_DIRECTORY_SEPARATOR_CHAR;
        newDiskDrDetailsPath += diskId;

        status = ServiceHelper::UpdateSymLinkPath(newDiskDrDetailsPath, prevDiskDrDetailsPath, errMsg);
        if (status != SVS_OK)
        {
            errMsg = "symlink update for the new pname failed";
            DebugPrintf(SV_LOG_ERROR, "%s symlink creation for old driver details path=%s with new details path=%s failed with error=%s.\n",
                FUNCTION_NAME, prevDiskDrDetailsPath.c_str(), newDiskDrDetailsPath.c_str(), errMsg.c_str());
            break;
        }

        oldBitmapPath = newDiskDrDetailsPath;
        oldBitmapPath += ACE_DIRECTORY_SEPARATOR_CHAR;
        oldBitmapPath += BITMAP_PREFIX;

        newBitmapPath = oldBitmapPath;
        oldBitmapPath += prevPname + BITMAP_SUFFIX;
        newBitmapPath += diskId + BITMAP_SUFFIX;

        status = ServiceHelper::UpdateSymLinkPath(newBitmapPath, oldBitmapPath, errMsg);
        if (status != SVS_OK)
        {
            errMsg = "symlink update for the new pname failed";
            DebugPrintf(SV_LOG_ERROR, "%s symlink creation for old driver details path=%s with new details path=%s failed with error=%s.\n",
                FUNCTION_NAME, oldBitmapPath.c_str(), newBitmapPath.c_str(), errMsg.c_str());
            break;
        }

        status = m_commitTag.ModifyPName(deviceDetails.m_deviceName, diskId, errStream);
        if (status != SVS_OK)
        {
            errMsg = "Pname modification failed with " + errStream.str();
            DebugPrintf(SV_LOG_ERROR, "%s Pname mofication for devicename=%s to pname=%s failed with error=%s\n",
                FUNCTION_NAME, deviceDetails.m_deviceName.c_str(), diskId.c_str(), errStream.str().c_str());
            break;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}
#endif

SVSTATUS RcmWorkerThread::FailoverExtendedLocation(RcmJob& job)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;
    std::string jobErrorMessage = "Failed to process failover extended location job data.";
    std::string possibleCauses = "Failed to process the failover extended location job settings.";
    std::string recmAction = "Contact support if the issue persists.";
    uint32_t jobErrorCode = RCM_CLIENT_JOB_FAILOVER_EXTENDED_LOCATION_ERROR;

    try {

        DebugPrintf(SV_LOG_ALWAYS, "%s : Failover job payload received from rcm is %s.\n", FUNCTION_NAME, job.InputPayload.c_str());
        std::string strJobInput = job.InputPayload;

        SourceAgentFailoverExtendedLocationJobInputBase input = JSON::consumer<SourceAgentFailoverExtendedLocationJobInputBase>::convert(strJobInput, true);

        ValidateFailoverExtendedLocationArguments(errMsg, input);

        RcmConfigurator::getInstance()->setFailoverVmBiosId(input.FailoverVmBiosId);
        RcmConfigurator::getInstance()->setFailoverTargetType(input.FailoverTargetType);
        
        DebugPrintf(SV_LOG_ALWAYS, "%s : Updated FailoverVmBiosId is %s and FailoverTargetType is %s.\n", FUNCTION_NAME, RcmConfigurator::getInstance()->getFailoverVmBiosId().c_str(),
            RcmConfigurator::getInstance()->getFailoverTargetType().c_str());
        
        job.JobStatus = RcmJobStatus::SUCCEEDED;
    }
    RCM_WORKER_THREAD_CATCH_EXCEPTION(errMsg, status);

    if (status != SVS_OK)
    {
        if (!errMsg.empty())
            jobErrorMessage = errMsg;

        DebugPrintf(SV_LOG_ERROR,
            "Failover extended location job failed,  Job Id %s Input %s error %d %s\n",
            job.Id.c_str(),
            job.InputPayload.c_str(),
            jobErrorCode,
            errMsg.c_str());

        job.JobStatus = RcmJobStatus::FAILED;
        RcmJobError jobError(
            boost::lexical_cast<std::string>(jobErrorCode),
            jobErrorMessage,
            possibleCauses,
            recmAction,
            RcmJobSeverity::RCM_ERROR);

        job.Errors.push_back(jobError);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

void RcmWorkerThread::ValidateFailoverExtendedLocationArguments(std::string& errMsg, const RcmClientLib::SourceAgentFailoverExtendedLocationJobInputBase& input)
{
    bool bFlag = true;
    boost::regex guidRegex(GUID_REGEX);
    if (!boost::regex_search(input.FailoverVmBiosId, guidRegex))
    {
        bFlag = false;
        errMsg += "Invalid FailoverVmBiosId: " + input.FailoverVmBiosId;
    }
    if (!(boost::iequals(input.FailoverTargetType, RcmClientLib::ONPREM_TO_AZUREAVS_FAILOVER_TARGET_TYPE) ||
        boost::iequals(input.FailoverTargetType, RcmClientLib::AZUREAVS_TO_ONPREM_FAILOVER_TARGET_TYPE)))
    {
        bFlag = false;
        errMsg += "Invalid FailoverTargetType: " + input.FailoverTargetType;
    }

    if (!bFlag)
    {
        throw INMAGE_EX("Invalid input arguments to FailoverExtendedLocation job.");
    }
}
