#include <vector>
#include <sstream>
#include <fstream>

#include <boost/bind.hpp>
#include <boost/tokenizer.hpp>
#include <boost/chrono.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "monitorhostthread.h"
#include "logger.h"
#include "portablehelpers.h"
#include "inmquitfunction.h"
#include "appcommand.h"
#include "inmalertdefs.h"
#include "inmageproduct.h"
#include "configwrapper.h"
#include "operatingsystem.h"
#include "monitoringparams.h"
#include "Monitoring.h"
#include "AgentSettings.h"
#include "securityutils.h"
#include "error.h"
#include "filterdrvifmajor.h"
#include "involfltfeatures.h"
#include "RcmConfigurator.h"
#include "transportstream.h"
#include "genericstream.h"
#include "genpassphrase.h"
#include "inmcertutil.h"
#include "CertUtil.h"
#include "azurepageblobstream.h"
#include "azureblockblobstream.h"

#ifdef SV_WINDOWS
#include "portablehelpersmajor.h"
#include "indskfltfeatures.h"
#include "Failoverclusterinfocollector.h"
#endif

#include "json_reader.h"
#include "json_writer.h"
#include "setpermissions.h"


#define MONITOR_LOG_LOC_WRITE_SIZE 4194304 

const int BUFFERLEN = 1024*15;

using namespace AzureStorageRest;
using namespace RcmClientLib;

static const std::string strDataDisk("DataDisk");
static const std::string strBootDisk("/dev/");
static const std::string strRootDisk("RootDisk");
static const std::string strAuthorizationFailure("AuthorizationFailure");

MonitorHostThread::MonitorHostThread(unsigned int monitorHostInterval):
  m_started(false),
  m_QuitEvent(new ACE_Event(1, 0)),
  m_MonitorHostInterval(monitorHostInterval),
  m_bFilterDriverSupportsCxSession(false),
  m_CxClearHealthIssueCounter(0),
  m_agentHealthCheckInterval(300)//default 5 minutes
{
}

MonitorHostThread::~MonitorHostThread()
{
}

void MonitorHostThread::Start()
{
  if (m_started)
  {
      return;
  }    
  m_QuitEvent->reset();
  m_threadManager.spawn( ThreadFunc, this );
  m_threadManager.spawn(CxStatsThreadFunc, this);
  m_started = true;
}

ACE_THR_FUNC_RETURN MonitorHostThread::ThreadFunc( void* arg )
{
    MonitorHostThread* p = static_cast<MonitorHostThread*>( arg );
    return p->run();
}

ACE_THR_FUNC_RETURN MonitorHostThread::CxStatsThreadFunc(void* arg)
{
    MonitorHostThread* p = static_cast<MonitorHostThread*>(arg);
    return p->MonitorCxEvents();
}

ACE_THR_FUNC_RETURN MonitorHostThread::run()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    
    QuitFunction_t qf = std::bind1st(std::mem_fun(&MonitorHostThread::QuitRequested), this);

    m_hostId = RcmConfigurator::getInstance()->getHostId();
    m_MonitorHostStartDelay = RcmConfigurator::getInstance()->getMonitorHostStartDelay();
    m_agentHealthCheckInterval = RcmConfigurator::getInstance()->getAgentHealthCheckInterval();
    m_logUploadLocationCheckTimeout = RcmConfigurator::getInstance()->getAzureBlobOperationsTimeout();
    m_strHealthCollatorPath = RcmConfigurator::getInstance()->getHealthCollatorDirPath();

    m_nextAgentHealthCheckTime = steady_clock::now();
    m_nextMonitorHostRunTime = steady_clock::now() + seconds(m_MonitorHostStartDelay);
    
    std::string proxySettingsPath = RcmConfigurator::getInstance()->getProxySettingsPath();
    
    if (!boost::filesystem::exists(proxySettingsPath))
    {
        DebugPrintf(SV_LOG_INFO, "Proxy settings not found.\n");
    }
    else
    {
        m_ptrProxySettings.reset(new ProxySettings(proxySettingsPath));
    }

    // run once before the monitoring events as it requrires access to URLs
    MonitorUrlAccess();

    // this is a blocking call until the health event is raised
    VerifyMonitoringEvents();

    // first call to refresh the protected device list
    HasProtectedDeviceListChanged();

    ACE_Time_Value waitTime(60, 0);

    do
    {
        DebugPrintf(SV_LOG_DEBUG, "%s Checking Agent health\n", FUNCTION_NAME);

        CheckAgentHealth();

        MonitorHost(qf);

        if (m_bFilterDriverSupportsCxSession &&
            m_pDriverStream &&
            HasProtectedDeviceListChanged())
        {
            FilterDrvIfMajor drvif;
            drvif.CancelCxStats(m_pDriverStream.get());
        }

        MonitorUrlAccess();

        UpdateFilePermissions();

        m_QuitEvent->wait(&waitTime,0);

    } while (!m_threadManager.testcancel(ACE_Thread::self()));
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return 0;
}

void MonitorHostThread::Stop()
{
    ACE_Time_Value waitTime(10, 0);

    m_logUploadAccessFailSignal.disconnect_all();

    m_resyncBatchSignal.disconnect_all();

    m_threadManager.cancel_all();
    m_QuitEvent->signal();

    do {
        if (m_bFilterDriverSupportsCxSession && m_pDriverStream) {
            FilterDrvIfMajor drvif;
            drvif.CancelCxStats(m_pDriverStream.get());
        }
    } while (m_threadManager.wait(&waitTime, false, false));

    m_started = false;
}

void MonitorHostThread::ExecCmd(const std::string &cmd, const std::string &tmpOpFile)
{
    if (!cmd.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "Running cmd \"%s\"\n", cmd.c_str());
        std::string cmdOutput;
        SV_UINT uTimeOut = 300;
        SV_ULONG lInmexitCode = 1;
        AppCommand objAppCommand(cmd, uTimeOut, tmpOpFile);
        bool bProcessActive = true;
        objAppCommand.Run(lInmexitCode, cmdOutput, bProcessActive, "", NULL);
        ACE_OS::unlink(getLongPathName(tmpOpFile.c_str()).c_str());

        unsigned int cmdOpLen = cmdOutput.length();
        int repeatCnt = (cmdOpLen <= BUFFERLEN) ? 1 : (cmdOpLen / BUFFERLEN + ((cmdOpLen % BUFFERLEN) ? 1 : 0));
        for (int cnt = 0; cnt < repeatCnt; ++cnt)
        {
            int offset = cnt * BUFFERLEN;
            int length = ((cmdOpLen - offset) < BUFFERLEN) ? (cmdOpLen - offset) : BUFFERLEN;
            if (!cnt)
            {
                DebugPrintf(SV_LOG_ALWAYS, "cmd output: %s\n", cmdOutput.substr(offset, length).c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_ALWAYS, "\n%s\n", cmdOutput.substr(offset, length).c_str());
            }
        }
    }
}

void MonitorHostThread::MonitorHost(QuitFunction_t qf)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_MonitorHostMutex);

    if (steady_clock::now() < m_nextMonitorHostRunTime)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: time not elapsed for next monitoring.\n", FUNCTION_NAME);
        return;
    }

    m_nextMonitorHostRunTime = steady_clock::now() + seconds(m_MonitorHostInterval);

    try {
        std::vector<std::string> monitorHostCmdsV;
        std::string cmds = RcmConfigurator::getInstance()->getMonitorHostCmdList();
        boost::char_separator<char> delm(",");
        boost::tokenizer < boost::char_separator<char> > strtokens(cmds, delm);
        for (boost::tokenizer< boost::char_separator<char> > ::iterator it = strtokens.begin(); it != strtokens.end(); ++it)
        {
            /// remove leading and trailing white space if any
            size_t firstws = (*it).find_first_not_of(" ");
            monitorHostCmdsV.push_back((*it).substr(firstws, (*it).find_last_not_of(" ") - firstws + 1));
        }

        std::stringstream tmpOpFile;
        tmpOpFile << RcmConfigurator::getInstance()->getInstallPath();
        tmpOpFile << ACE_DIRECTORY_SEPARATOR_CHAR_A;
        tmpOpFile << "tmp_svagentspid_";
        tmpOpFile << ACE_OS::getpid();
        static const int TaskWaitTimeInSeconds = 5;

#ifdef SV_WINDOWS
        for (std::vector<std::string>::iterator itr = monitorHostCmdsV.begin(); itr != monitorHostCmdsV.end(); ++itr)
        {
            ExecCmd(*itr, tmpOpFile.str());
            if (qf && qf(TaskWaitTimeInSeconds)) {
                DebugPrintf(SV_LOG_DEBUG, "Exiting %s, service shutdown requested\n", FUNCTION_NAME);
                break;
            }
        }
#else
        std::string linuxcmd = "/bin/sh ";
        linuxcmd += RcmConfigurator::getInstance()->getInstallPath();
        linuxcmd += "/bin/collect_support_materials.sh _mds_";
        ExecCmd(linuxcmd, tmpOpFile.str());
#endif
    }
    catch (std::exception &exception)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with exception %s\n", FUNCTION_NAME, exception.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with unknown exception\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

bool MonitorHostThread::QuitRequested(int seconds)
{
    ACE_Time_Value waitTime(seconds, 0);
    m_QuitEvent->wait(&waitTime, 0);
    return m_threadManager.testcancel(ACE_Thread::self());
}

void MonitorHostThread::VerifyMonitoringEvents()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool rebootAlert = false;
    bool upgradeAlert = false;

    MoitoringParams currMonParams;
    try
    {
        currMonParams.AgentVersion = InmageProduct::GetInstance().GetProductVersion();

        const Object & osinfo = OperatingSystem::GetInstance().GetOsInfo();
        ConstAttributesIter_t iter = osinfo.m_Attributes.find(NSOsInfo::LASTBOOTUPTIME);
        if (iter != osinfo.m_Attributes.end())
        {
            currMonParams.LastRebootTime = iter->second;

            if (currMonParams.LastRebootTime.empty())
                DebugPrintf(SV_LOG_ERROR, "%s last boot time in OS info is empty.\n", FUNCTION_NAME);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s failed to find last boot time in OS info.\n", FUNCTION_NAME);
        }

       
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with exception: %s\n", FUNCTION_NAME, e.what());
        return;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with unknown exception.\n", FUNCTION_NAME);
        return;
    }

    std::string moitoringParamsFile;
    RcmConfigurator::getInstance()->getConfigDirname(moitoringParamsFile);
    moitoringParamsFile += ACE_DIRECTORY_SEPARATOR_STR_A;
    moitoringParamsFile += MoitoringParamsFileName;
    MoitoringParams mpInFile;
    bool bMpFileExists = boost::filesystem::exists(moitoringParamsFile);
    if (bMpFileExists)
    {
        std::string lockFile(moitoringParamsFile);
        lockFile += ".lck";
        if (!boost::filesystem::exists(lockFile))
        {
            std::ofstream tmpLockFile(lockFile.c_str());
            tmpLockFile.close();

            std::string errormsg;
            if (!securitylib::setPermissions(lockFile, errormsg)) {
                DebugPrintf(SV_LOG_ERROR, "%s\n", errormsg.c_str());
            }
        }
        boost::interprocess::file_lock fileLock(lockFile.c_str());
        boost::interprocess::scoped_lock<boost::interprocess::file_lock> fileLockGuard(fileLock);

        try
        {
            std::string moitoringParams;
            getFileContent(moitoringParamsFile, moitoringParams);
            mpInFile = JSON::consumer<MoitoringParams>::convert(moitoringParams);

            if (!boost::iequals(mpInFile.AgentVersion, currMonParams.AgentVersion))
            {
                upgradeAlert = true;
            }

            if (!currMonParams.LastRebootTime.empty() && (!boost::iequals(mpInFile.LastRebootTime, currMonParams.LastRebootTime)))
            {
                rebootAlert = true;
            }
        }
        catch (std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to read MoitoringParams in %s. Exception: %s\n", FUNCTION_NAME, moitoringParamsFile.c_str(), e.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to read MoitoringParams in %s with unknown exception.\n", FUNCTION_NAME, moitoringParamsFile.c_str());
        }
    }

    if (rebootAlert)
    {
        RebootAlert a(currMonParams.LastRebootTime);
        while (!MonitoringLib::SendAlertToRcm(SV_ALERT_SIMPLE, SV_ALERT_MODULE_GENERAL, a, m_hostId))
        {
            if (QuitRequested(30))
            {
                break;
            }
        }
        DebugPrintf(SV_LOG_ALWAYS, "%s: Sent alert %s.\n", FUNCTION_NAME, a.GetMessage().c_str());
    }

    if (upgradeAlert)
    {
        AgentUpgradeAlert a(currMonParams.AgentVersion);
        while (!MonitoringLib::SendAlertToRcm(SV_ALERT_SIMPLE, SV_ALERT_MODULE_GENERAL, a, m_hostId))
        {
            if (QuitRequested(30))
            {
                break;
            }
        }
        DebugPrintf(SV_LOG_ALWAYS, "%s: Sent alert %s.\n", FUNCTION_NAME, a.GetMessage().c_str());
    }
    
    if (rebootAlert || upgradeAlert || !bMpFileExists)
    {
        try
        {
            std::string strTagStatus = JSON::producer<MoitoringParams>::convert(currMonParams);
            std::ofstream mpFile(moitoringParamsFile.c_str(), std::ios::out | std::ios::trunc);
            mpFile << strTagStatus;
            mpFile.close();

            std::string errormsg;
            if (!securitylib::setPermissions(moitoringParamsFile, errormsg)) {
                DebugPrintf(SV_LOG_ERROR, "%s\n", errormsg.c_str());
            }
            DebugPrintf(SV_LOG_DEBUG, "%s: Serialized MoitoringParams to %s.\n", FUNCTION_NAME, moitoringParamsFile.c_str());
        }
        catch (std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to serialize MoitoringParams to %s. Exception: %s\n", FUNCTION_NAME, moitoringParamsFile.c_str(), e.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to serialize MoitoringParams to %s with unknown exception.\n", FUNCTION_NAME, moitoringParamsFile.c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

bool MonitorHostThread:: GetDevicesInResyncMode(std::set<std::string>& deviceids)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    deviceids.clear();
    AgentSettings settings;
    RcmConfigurator::getInstance()->GetAgentSettings(settings);
    SyncSettings_t::const_iterator iter = settings.m_syncSettings.begin();
    for (/*empty*/; iter != settings.m_syncSettings.end(); iter++)
    {
        deviceids.insert(iter->DiskId);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

SVSTATUS MonitorHostThread::GetProtectedDeviceIds(std::set<std::string>&diskIds)
{
    return GetProtectedDisksIdsInSettings(diskIds);
}
SVSTATUS MonitorHostThread::GetProtectedDisksIdsInSettings(std::set<std::string> &diskIds)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    diskIds.clear();
    std::vector<std::string>    protectedDiskIds;
    
    SVSTATUS status = RcmConfigurator::getInstance()->GetProtectedDiskIds(protectedDiskIds);
    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "%s Failed to get protected deviceIds with %d\n",
            FUNCTION_NAME,
            status);
        return status;
    }

    if (protectedDiskIds.empty())
    {
        // this is not an error
        DebugPrintf(SV_LOG_DEBUG, "%s: no protected disks in settings.\n", FUNCTION_NAME);
    }

    diskIds.insert(protectedDiskIds.begin(), protectedDiskIds.end());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

void MonitorHostThread::UpdateMissingDiskList(std::vector<AgentDiskLevelHealthIssue>& diskHealthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::set<std::string> protectedDiskIds;
    SVSTATUS ret = GetProtectedDisksIdsInSettings(protectedDiskIds);
    if (SVS_OK != ret)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to get protected disk ids with error %d", FUNCTION_NAME, ret);
        return;
    }

    std::set<std::string>::const_iterator diskIdit = protectedDiskIds.begin();
    for (/*empty*/;
        diskIdit != protectedDiskIds.end() && !m_threadManager.testcancel(ACE_Thread::self());
        diskIdit++)
    {
        std::string deviceName;
        ret = IsDiskDiscovered(*diskIdit, deviceName);
        if (SVS_OK != ret)
        {
            diskHealthIssues.push_back(AgentDiskLevelHealthIssue((*diskIdit),
                AgentHealthIssueCodes::DiskLevelHealthIssues::DiskRemoved::HealthCode));
            DebugPrintf(SV_LOG_ERROR, "%s: diskid %s is missing with error %d.\n", FUNCTION_NAME, diskIdit->c_str(), ret);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void MonitorHostThread::CheckAgentHealth()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    static bool bLogHealthCheckSkipped = true;

    try
    {
        steady_clock::time_point currentRunTime = steady_clock::now();
        if (currentRunTime < m_nextAgentHealthCheckTime)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: time not elapsed for next health check.\n", FUNCTION_NAME);
            return;
        }

        std::set<std::string> protectedDiskIds;
        SVSTATUS ret = GetProtectedDisksIdsInSettings(protectedDiskIds);
        if (SVS_OK != ret)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to get protected disk ids with error %d. skipping health check\n", FUNCTION_NAME, ret);
            return;
        }

        if (protectedDiskIds.empty())
        {
            int logLevel = bLogHealthCheckSkipped ? SV_LOG_ALWAYS : SV_LOG_DEBUG;
            DebugPrintf(logLevel, "%s: no protected disks found. skipping health check.\n", FUNCTION_NAME);
            bLogHealthCheckSkipped = false;
            return;
        }

        bLogHealthCheckSkipped = true;
        SourceAgentProtectionPairHealthIssues   healthIssues;

        MonitorResyncBatching(healthIssues.DiskLevelHealthIssues);

#ifdef SV_WINDOWS
        DebugPrintf(SV_LOG_DEBUG, "%s: Validating cluster\n", FUNCTION_NAME);
        UpdateSharedDiskClusterStatus(healthIssues.HealthIssues);
#endif

        CreateAgentHealthIssueMap(healthIssues);

        RcmClientLib::MonitoringLib::PostHealthIssuesToRcm(healthIssues, m_hostId);

        HealthIssueReported();

        m_nextAgentHealthCheckTime = currentRunTime + seconds(m_agentHealthCheckInterval);
    }
    catch (const std::exception &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with standard exception : %s\n", FUNCTION_NAME, ex.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s Caught an unknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return;
}

SVSTATUS MonitorHostThread::IsDiskDiscovered(const std::string& diskId, std::string &name)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;

    do {

        std::string errMsg;
        if (!DataCacher::UpdateVolumesCache(m_volumesCache, errMsg))
        {
            DebugPrintf(SV_LOG_INFO, "%s: update volume cache failed. Error %s",
                FUNCTION_NAME,
                errMsg.c_str());

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
            status = SVE_INVALIDARG;
            errMsg += "Invalid disk id format for " + diskId + ". Expecting /dev/<name> or DataDisk<num>.";
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            break;
        }
#endif
        if (drs.size() != 1) {
            status = SVE_INVALIDARG;
            errMsg = "Found " + boost::lexical_cast<std::string>(drs.size());
            errMsg += " disks for id " + diskId + ".";
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            break;
        }

        vrimp.PrintDiskReports(drs);
        name = drs[0].m_Name;

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

sigslot::signal3<const std::string&, const std::string&, const std::string&>& MonitorHostThread::GetLogUploadAccessFailSignal()
{
    return m_logUploadAccessFailSignal;
}

sigslot::signal1<ResyncBatch &> & MonitorHostThread::GetResyncBatchSignal()
{
    return m_resyncBatchSignal;
}

void MonitorHostThread::MonitorLogUploadLocation(std::string& error)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try
    {
        AgentSettings settings;
        SVSTATUS ret = RcmConfigurator::getInstance()->GetAgentSettings(settings);
        if (ret != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to fetch cached replication settings: %d\n",
                FUNCTION_NAME, ret);
            return;
        }

        if (RcmConfigurator::getInstance()->IsAzureToAzureReplication() || RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
        {
            MonitorAzureBlobLogUploadLocation(settings, error);
        }
        else
        {
            MonitorProcessServerLogUploadLocation(settings, error);
        }
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to check log upload location status with error %s\n", e.what());
    }
    catch (const std::string &e)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to check log upload location status with error %s\n", e.c_str());
    }
    catch (const char *e)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to check log upload location status with error %s\n", e);
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to check log upload location status with unknown error.\n");
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void MonitorHostThread::MonitorAzureBlobLogUploadLocation(const AgentSettings& settings, std::string& message)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if(m_mapPtrAzureBlobTranportStream.empty())
    {
        std::string sasUri;
        m_mapPtrAzureBlobTranportStream.insert(std::make_pair(AZURE_BLOB_TYPE_PAGE, new AzurePageBlobStream(sasUri, m_logUploadLocationCheckTimeout)));
        m_mapPtrAzureBlobTranportStream.insert(std::make_pair(AZURE_BLOB_TYPE_BLOK, new AzureBlockBlobStream(sasUri, m_logUploadLocationCheckTimeout)));
    }

    std::set<std::string> diskIdsForCleanup;

    std::vector<DrainSettings>::const_iterator dsit = settings.m_drainSettings.begin();
    for (/*empty*/;
        dsit != settings.m_drainSettings.end() && !m_threadManager.testcancel(ACE_Thread::self());
        dsit++)
    {
        std::string errMsg;
        const DrainSettings &drainSettings = *dsit;

        diskIdsForCleanup.insert(drainSettings.DiskId);

        if (TRANSPORT_TYPE_AZURE_BLOB != drainSettings.DataPathTransportType)
        {
            DebugPrintf(SV_LOG_DEBUG, "Unsupported Transport type %s for disk %s\n",
                drainSettings.DataPathTransportType.c_str(),
                drainSettings.DiskId.c_str());
            continue;
        }

        if (drainSettings.DataPathTransportSettings.empty())
        {
            DebugPrintf(SV_LOG_DEBUG, "Transport settings are empty for disk %s\n", drainSettings.DiskId.c_str());
            continue;
        }

        std::string input;
        if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
        {
            input = securitylib::base64Decode(
                drainSettings.DataPathTransportSettings.c_str(),
                drainSettings.DataPathTransportSettings.length());
        }
        else
        {
            input = drainSettings.DataPathTransportSettings;
        }

        AzureBlobBasedTransportSettings blobSettings =
            JSON::consumer<AzureBlobBasedTransportSettings>::convert(input, true);
    
        CheckLogUploadLocationStatus(blobSettings, drainSettings.DiskId, RenewRequestTypes::DiffSyncContainer, message);
    }

    //syncsettings monitoring - resync related syncsettings handling for the case of 
    //AUTH FAIL AND NOT FOUND container errors

    std::vector<SyncSettings>::const_iterator ssit = settings.m_syncSettings.begin();
    for (/*empty*/;
        ssit != settings.m_syncSettings.end() && !m_threadManager.testcancel(ACE_Thread::self());
        ssit++)
    {
        std::string errMsg;
        const SyncSettings &syncSettings = *ssit;

        diskIdsForCleanup.insert(syncSettings.DiskId);

        if (syncSettings.TransportSettings.BlobContainerSasUri.empty())
        {
            DebugPrintf(SV_LOG_DEBUG, "Sync settings are empty for disk %s\n", syncSettings.DiskId.c_str());
            continue;
        }

        AzureBlobBasedTransportSettings blobSettings =
            syncSettings.TransportSettings;

        CheckLogUploadLocationStatus(blobSettings, syncSettings.DiskId, RenewRequestTypes::ResyncContainer, message);
    }

    CleanLogUploadStatus(diskIdsForCleanup);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void MonitorHostThread::MonitorProcessServerLogUploadLocation(const AgentSettings& settings, std::string& message)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    // cache the PS settings that were used to create the transport stream
    // used to compare if settings have changed
    static ProcessServerTransportSettings s_psdetails;

    if (!boost::iequals(settings.m_dataPathType, TRANSPORT_TYPE_PROCESS_SERVER))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Invalid datapath settings. Type %s", FUNCTION_NAME, settings.m_dataPathType.c_str());
        return;
    }

    if (settings.m_dataPathSettings.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Empty datapath settings. Type %s", FUNCTION_NAME, settings.m_dataPathType.c_str());
        return;
    }

    const size_t BufferSize = 1048576;
    const size_t TotalbufferSize = 4 * 1048576;

    int status = SV_SUCCESS;
    size_t bytesSent = 0, sendBytes = 0;
    std::vector<char> buffer(BufferSize);
        
    std::string remoteFilename = m_hostId;
    remoteFilename += ".mon";

    steady_clock::duration elapsedTime = seconds(0);
    ProcessServerTransportSettings psdetails;
    std::string port;

    try {
        psdetails = JSON::consumer<ProcessServerTransportSettings>::convert(settings.m_dataPathSettings, true);

        port = boost::lexical_cast<std::string>(psdetails.Port);
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: parsing transpot settings failed with exception %s.\n", FUNCTION_NAME, e.what());
        return;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: parsing transpot settings failed with unknown exception.\n", FUNCTION_NAME);
        return;
    }

    try
    {
        if (!m_ptrCxTransportStream || !(psdetails == s_psdetails))
        {
            TRANSPORT_CONNECTION_SETTINGS transportSettings;
            transportSettings.sslPort = port;
            transportSettings.ipAddress = psdetails.GetIPAddress();

            DebugPrintf(SV_LOG_DEBUG, "%s: PS Name: %s, Address: %s, Port %d\n", FUNCTION_NAME,
                psdetails.FriendlyName.c_str(),
                psdetails.GetIPAddress().c_str(),
                psdetails.Port);

            m_ptrCxTransportStream.reset(new TransportStream(TRANSPORT_PROTOCOL_HTTP, transportSettings));

            s_psdetails = psdetails;
        }

        // since the health check interval can be more than the session timeout (5 min default), open for every iteration
        STREAM_MODE mode = GenericStream::Mode_Open | GenericStream::Mode_RW | GenericStream::Mode_Secure;
        if (SV_FAILURE == m_ptrCxTransportStream->Open(mode))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Unable to open transport stream.\n", FUNCTION_NAME);
            UpdateLogUploadStatus(psdetails.FriendlyName, SV_FAILURE, message, false);
            return;
        }

        steady_clock::time_point startTime = steady_clock::now();
        do
        {
            sendBytes = buffer.size() < (TotalbufferSize - bytesSent) ? buffer.size() : (TotalbufferSize - bytesSent);
            status = m_ptrCxTransportStream->Write(&buffer[0], sendBytes, remoteFilename, (bytesSent + sendBytes) < TotalbufferSize, COMPRESS_NONE, true);
            if (SV_SUCCESS != status)
            {
                elapsedTime = steady_clock::now() - startTime;

                DebugPrintf(SV_LOG_ERROR, "%s: the network transfer failed with error %d. %d bytes sent in %d ms.\n",
                    FUNCTION_NAME, status,
                    bytesSent, elapsedTime.count() / (1000 * 1000));

                break;
            }

            bytesSent += sendBytes;

        } while (bytesSent < TotalbufferSize);

        if (bytesSent >= TotalbufferSize)
        {
            elapsedTime = steady_clock::now() - startTime;
            DebugPrintf(SV_LOG_ALWAYS, "%s: the network transfer succeeded. %d bytes sent in %d ms\n",
                FUNCTION_NAME,
                TotalbufferSize,
                elapsedTime.count() / (1000 * 1000));
        }

        int ret = m_ptrCxTransportStream->DeleteFile(remoteFilename);
        if (SV_SUCCESS != ret)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: failed to delete the monitoring file on PS. file name %s\n.", FUNCTION_NAME, remoteFilename.c_str());
        }

        m_ptrCxTransportStream->Close();
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: the network transfer failed with exception %s. %d bytes sent in %d m\n",
            FUNCTION_NAME, e.what(),
            bytesSent, elapsedTime.count() / (1000 * 1000));

        status = SV_FAILURE;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: the network transfer failed with unknown exception.\n", FUNCTION_NAME);
        status = SV_FAILURE;
    }

    UpdateLogUploadStatus(psdetails.FriendlyName, status, message, false);

    std::set<std::string> psForCleanup;
    psForCleanup.insert(psdetails.FriendlyName);
    CleanLogUploadStatus(psForCleanup);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void MonitorHostThread::UpdateLogUploadStatus(const std::string& id, const int status, std::string &message, bool bForce)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (status != SV_SUCCESS)
    {
        bool bInclude = false;
        std::map<std::string, LogUploadLocationStatus>::iterator it = m_mapLogUploadStatus.find(id);
        if (it != m_mapLogUploadStatus.end())
        {
            if (!it->second.m_bFailed)
            {
                it->second.m_firstFailTime = steady_clock::now();
                it->second.m_bFailed = true;
            }
            else if ( steady_clock::now() > it->second.m_firstFailTime + seconds(10 * 60))
            {
                bInclude = true;
            }
        }
        else
        {
            LogUploadLocationStatus locStatus;
            locStatus.m_firstFailTime = steady_clock::now();
            locStatus.m_bFailed = true;
            m_mapLogUploadStatus.insert(std::make_pair(id, locStatus));
        }

        if (bForce || bInclude)
        {
            if (!message.empty())
                message += ',';
            message += id;
        }
    }
    else
    {
        std::map<std::string, LogUploadLocationStatus>::iterator it = m_mapLogUploadStatus.find(id);
        if (it != m_mapLogUploadStatus.end())
        {
            it->second.m_bFailed = false;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void MonitorHostThread::CleanLogUploadStatus(const std::set<std::string>& ids)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::map<std::string, LogUploadLocationStatus>::iterator it = m_mapLogUploadStatus.begin();
    while( it != m_mapLogUploadStatus.end())
    {
        if (ids.find(it->first) == ids.end())
        {
            m_mapLogUploadStatus.erase(it++);
        }
        else
        {
            it++;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

SVSTATUS MonitorHostThread::GetDeviceIdsForHealthMonitoring(std::set<std::string> &deviceIds)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    deviceIds.clear();
    std::set<std::string> protectedDiskIds;
    SVSTATUS ret =  GetProtectedDisksIdsInSettings(protectedDiskIds);
    if (SVS_OK != ret)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to get protected disk ids with error %d", FUNCTION_NAME, ret);
        return ret;
    }

    // empty list is not an error, it is a change and stops the health monitoring

    std::set<std::string>::iterator diskIter = protectedDiskIds.begin();
    for (/**/; diskIter != protectedDiskIds.end(); diskIter++)
    {
        std::string deviceName;
        if (SVS_OK != IsDiskDiscovered(*diskIter, deviceName))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: protected disk with id %s doesn't exist.\n",
                FUNCTION_NAME,
                diskIter->c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: for protected disk with id %s, device is %s.\n",
                FUNCTION_NAME,
                diskIter->c_str(),
                deviceName.c_str());
        }

        deviceIds.insert(GetPersistentDeviceIdForDriverInput(*diskIter));
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return ret;
}

bool MonitorHostThread::HasProtectedDeviceListChanged()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool bRet = false;
    std::set<std::string> devIds;
    if (SVS_OK != GetDeviceIdsForHealthMonitoring(devIds))
    {
        // on failure to get the deviceids, force a change
        return true;
    }

    if (devIds != m_monitorDeviceIds)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: updating device ids\n", FUNCTION_NAME);
        bRet = true;
        boost::unique_lock<boost::mutex> lock(m_mutexMonitorDeviceIds);
        m_monitorDeviceIds = devIds;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return bRet;
}

bool MonitorHostThread::SendHealthAlert(InmErrorAlertImp &a)
{
    return MonitoringLib::SendAlertToRcm(SV_ALERT_SIMPLE, SV_ALERT_MODULE_GENERAL, a, m_hostId);
}

void    MonitorHostThread::UpdateLogUploadLocationStatus(std::vector<HealthIssue>& healthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string strMessage;

    MonitorLogUploadLocation(strMessage);

    if (!strMessage.empty())
    {
        healthIssues.push_back(HealthIssue(AgentHealthIssueCodes::VMLevelHealthIssues::LogUploadLocationNotHealthy::HealthCode));
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


#ifdef SV_WINDOWS
void MonitorHostThread::UpdateSharedDiskClusterStatus(std::vector<HealthIssue>& healthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    
    SVSTATUS status;
    AgentSettings settings;
    RcmConfigurator::getInstance()->GetAgentSettings(settings);

    do
    {
        bool IsSharedDiskCluster = RcmConfigurator::getInstance()->IsClusterSharedDiskEnabled();

        if (!IsSharedDiskCluster)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s not a shared disk cluster.\n", FUNCTION_NAME);
            break;
        }

        std::string clusterName = RcmConfigurator::getInstance()->getClusterName();
        FailoverClusterInfo clusInfo; 
        bool isClusterUp = false;

        status = clusInfo.CheckClusterHealth(clusterName, isClusterUp);

        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s machine failed to get cluster handle with status=%d. unknown cluster state\n", FUNCTION_NAME, status);
            break;
        }

        if (!isClusterUp)
        {
            std::map<std::string, std::string> params;
            params.insert(std::pair<std::string, std::string>(SharedDiskClusterHealthIssueCodes::ClusterLevelHealthIssues::SharedDiskClusterDown::ClusterName,
                clusterName));
            HealthIssue clusterDownHealthIssue(SharedDiskClusterHealthIssueCodes::ClusterLevelHealthIssues::SharedDiskClusterDown::HealthCode, 
                EVENT_SEVERITY_ERROR, EVENT_SHARED_DISK_CLUSTER, params);
            healthIssues.push_back(clusterDownHealthIssue);
            DebugPrintf(SV_LOG_ERROR, "%s shared disk cluster down.\n", FUNCTION_NAME);
            break;
        }

        status = clusInfo.CollectFailoverClusterProperties();

        if (status != SVS_OK)
            break;

        std::map<std::string, NodeEntity> currClusNodes;
        clusInfo.GetClusterUpNodesMap(currClusNodes);

        std::vector<std::string> currProtectedNodes = settings.m_consistencySettings.ParticipatingMachines;

        std::stringstream protectedNodesNotInClusterStream;
        protectedNodesNotInClusterStream.clear();
        
        std::vector<std::string> nodeFQDNSplits;

        for (std::vector<std::string>::iterator protectedNodesItr = currProtectedNodes.begin();
            protectedNodesItr != currProtectedNodes.end();
            protectedNodesItr++)
        {
            boost::split(nodeFQDNSplits, *protectedNodesItr, boost::is_any_of("."));
            
            if (nodeFQDNSplits.size() <= 0)
                continue;

            /* if machine is protected and present in cluster */
            if (currClusNodes.find(nodeFQDNSplits[0]) != currClusNodes.end())
            {
                currClusNodes.erase(nodeFQDNSplits[0]);
            }
            else
            {
                if (protectedNodesNotInClusterStream.str().empty())
                {
                    protectedNodesNotInClusterStream << nodeFQDNSplits[0];
                }
                else
                {
                    protectedNodesNotInClusterStream << ", " << nodeFQDNSplits[0];
                }                
            }
        }

        std::string protectedNodesNotIncluster = protectedNodesNotInClusterStream.str();

        if (!protectedNodesNotIncluster.empty())
        {
            DebugPrintf(SV_LOG_INFO, "%s protected machines=%s are not in cluster.\n", FUNCTION_NAME, protectedNodesNotIncluster);
            ProtectedMachinesNotInClusterAlert a(protectedNodesNotIncluster, clusterName);
            MonitoringLib::SendAlertToRcm(SV_ALERT_SIMPLE, SV_ALERT_MODULE_CLUSTER, a,
                RcmConfigurator::getInstance()->getClusterId());
        }

        // if nodes are present in cluster map then they are unprotected
        if (!currClusNodes.empty())
        {
            std::stringstream unprotectedNodesInClusterStream;
            unprotectedNodesInClusterStream.clear();
            for (std::map<std::string, NodeEntity>::iterator unprotectedNodesItr = currClusNodes.begin();
                unprotectedNodesItr != currClusNodes.end();
                unprotectedNodesItr++)
            {
                if (unprotectedNodesInClusterStream.str().empty())
                {
                    unprotectedNodesInClusterStream << (*unprotectedNodesItr).first;
                }
                else
                {
                    unprotectedNodesInClusterStream << ", " << (*unprotectedNodesItr).first;
                }
            }

            DebugPrintf(SV_LOG_ERROR, "%s unprotected machines=%s are part of cluster. generating health issue.\n", FUNCTION_NAME, unprotectedNodesInClusterStream.str().c_str());

            std::map<std::string, std::string> params;
            params.insert(std::pair<std::string, std::string>(SharedDiskClusterHealthIssueCodes::ClusterLevelHealthIssues::SharedDiskClusterUnProtectedMachinesInCluster::UnProtectedMachines, 
                unprotectedNodesInClusterStream.str()));
            HealthIssue unProtectedMachinesInClusterHealthIssue(SharedDiskClusterHealthIssueCodes::ClusterLevelHealthIssues::SharedDiskClusterUnProtectedMachinesInCluster::Healthcode, 
                EVENT_SEVERITY_ERROR, EVENT_SHARED_DISK_CLUSTER, params);
            healthIssues.push_back(unProtectedMachinesInClusterHealthIssue);
        }

    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}
#endif 



void MonitorHostThread::MonitorUrlAccess()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    static std::set<std::string> urlsVerified;

    if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
    {
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return;
    }

    SVSTATUS status = SVS_OK;
    std::set<std::string> urlsToCheck, urlsInSettings;
   
    try
    {
        // get the URLs from settings
        status = RcmConfigurator::getInstance()->GetUrlsForAccessCheck(urlsInSettings);
        if (SVS_OK != status)
        {
            DebugPrintf(SV_LOG_ERROR, "%s : GetUrlsForAccessCheck failed with status %d\n", FUNCTION_NAME, status);
            return;
        }

        HttpClient certTestClient;

        for (std::set<std::string>::iterator itr = urlsToCheck.begin();
            itr != urlsToCheck.end();
            ++itr)
        {
            if (urlsVerified.find(*itr) != urlsVerified.end())
            {
                DebugPrintf(SV_LOG_DEBUG, "%s : URI  %s already verified.\n", FUNCTION_NAME, itr->c_str());
                continue;
            }

            DebugPrintf(SV_LOG_ALWAYS, "%s : for URI  %s running cert check.\n", FUNCTION_NAME, itr->c_str());

            std::string endUri(*itr);

            if (!boost::starts_with(endUri, HTTPS_PREFIX))
                endUri = HTTPS_PREFIX + endUri;

            HttpRequest request(endUri);
            HttpResponse response;

            HttpProxy proxy;
            std::string proxyarg;
            std::string customProxy;

            if (RcmConfigurator::getInstance()->GetProxySettings(proxy.m_address, proxy.m_port, proxy.m_bypasslist))
            {
                certTestClient.SetProxy(proxy);
                customProxy = proxy.m_address + ":" + proxy.m_port;
                proxyarg = IsAddressInBypasslist(*itr, proxy.m_bypasslist) ? std::string() : customProxy;
            }

            request.SetHttpMethod(AzureStorageRest::HTTP_GET);

            if (!certTestClient.GetResponse(request, response))
            {
                long responseErrorCode = response.GetErrorCode();
                if (CURLE_SSL_CACERT != responseErrorCode)
                    continue;
                std::string port("443");
                std::stringstream ssmsg;
                CertUtil certutil(endUri, port);
                if (!certutil.get_last_error().empty())
                {
                    ssmsg << "Failed to connect to server " << endUri << " with error : " << certutil.get_last_error() << "." << std::endl;
                }
                else
                {
                    std::string msg;
                    certutil.validate_root_cert(msg);
                    ssmsg << "For server " << endUri << " " << msg << std::endl;
                }

                DebugPrintf(SV_LOG_ERROR, "%s : URI %s access check failed with curl error %ld\n", FUNCTION_NAME, itr->c_str(), responseErrorCode);
                DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, ssmsg.str().c_str());
            }
            else
            {
                // ignore any response
                urlsVerified.insert(*itr);
                DebugPrintf(SV_LOG_ALWAYS, "%s : URI %s access check verified. Http response code %ld\n", FUNCTION_NAME, itr->c_str(), response.GetResponseCode());

                continue;
            }

            RunUrlAccessCommand(*itr, proxyarg);
        }
    }
    catch(const std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "%s : caught exception %s.\n", FUNCTION_NAME, ex.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s : caught unknown exception\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


void MonitorHostThread::CheckLogUploadLocationStatus(const AzureBlobBasedTransportSettings &blobSettings,
    const std::string &id,
    const std::string& renewRequestType,
    std::string& msg)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::vector<char> writeBuffer(MONITOR_LOG_LOC_WRITE_SIZE, 0);
    std::string sasUri = blobSettings.BlobContainerSasUri;
    std::string blobName = id;
    boost::replace_all(blobName, "/", "_");
    blobName += ".mon";

    std::string containerName, sasParams, blobSasUri;
    size_t qpos = sasUri.find('?');
    if (qpos == std::string::npos)
    {
        DebugPrintf(SV_LOG_DEBUG, "Sync transport settings has invalid URI format for disk %s\n", id.c_str());
        return;
    }

    containerName = sasUri.substr(0, qpos);
    sasParams = sasUri.substr(qpos + 1);
    blobSasUri = containerName + "/" + blobName + "?" + sasParams;

    boost::shared_ptr<AzureBlobStream> ptrAzureBlobStream;

    const std::string logStorageBlobType = boost::iequals(blobSettings.LogStorageBlobType, AZURE_BLOB_TYPE_BLOK) ? AZURE_BLOB_TYPE_BLOK : AZURE_BLOB_TYPE_PAGE;
    std::map<std::string, boost::shared_ptr<AzureBlobStream> >::iterator itr = m_mapPtrAzureBlobTranportStream.find(logStorageBlobType);
    if (itr != m_mapPtrAzureBlobTranportStream.end())
    {
        ptrAzureBlobStream = itr->second;
    }
    else
    {
        throw INMAGE_EX(std::string("Transport not initialized for Azure " + logStorageBlobType + " blob.").c_str());
    }

    if (m_ptrProxySettings.get() && !m_ptrProxySettings->m_Address.empty() && !m_ptrProxySettings->m_Port.empty())
    {
        ptrAzureBlobStream->SetHttpProxy(m_ptrProxySettings->m_Address, m_ptrProxySettings->m_Port, m_ptrProxySettings->m_Bypasslist);

        DebugPrintf(SV_LOG_INFO, "Using proxy settings %s %s for transport stream.\n",
            m_ptrProxySettings->m_Address.c_str(),
            m_ptrProxySettings->m_Port.c_str());
    }

    bool bRaiseHealthIssue = false;
    int status = ptrAzureBlobStream->Write(&writeBuffer[0], MONITOR_LOG_LOC_WRITE_SIZE, 0, blobSasUri);
    if (SV_SUCCESS != status)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to create blob for disk %s at log location %s with status %d error %d and error data %s.\n",
            FUNCTION_NAME,
            id.c_str(),
            blobName.c_str(),
            status,
            ptrAzureBlobStream->GetLastError(),
            ptrAzureBlobStream->GetLastResponseData().c_str());

        if (ptrAzureBlobStream->GetLastError() == AzureStorageRest::HttpErrorCode::NOT_FOUND)
        {
            bRaiseHealthIssue = true;
            m_logUploadAccessFailSignal(id, sasUri, renewRequestType);
        }
        else if (ptrAzureBlobStream->GetLastError() == AzureStorageRest::HttpErrorCode::FORBIDDEN)
        {
            if (!ptrAzureBlobStream->GetLastResponseData().empty() && ptrAzureBlobStream->GetLastResponseData().find(strAuthorizationFailure) == std::string::npos)
            {
                m_logUploadAccessFailSignal(id, sasUri, renewRequestType);
            }
            bRaiseHealthIssue = true;
        }
    }
    UpdateLogUploadStatus(id, status, msg, bRaiseHealthIssue);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void MonitorHostThread::MonitorResyncBatching(std::vector<AgentDiskLevelHealthIssue>& diskHealthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try
    {
        AgentSettings settings;

        SVSTATUS ret = RcmConfigurator::getInstance()->GetAgentSettings(settings);
        if (ret != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to fetch cached replication settings: %d\n", FUNCTION_NAME, ret);
            return;
        }

        ResyncBatch resyncBatch;

        m_resyncBatchSignal(resyncBatch);

        if (resyncBatch.InprogressResyncDiskIDsMap.empty())
        {
            DebugPrintf(SV_LOG_WARNING, "%s: resync batch is empty.\n", FUNCTION_NAME);

            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }

        std::vector<SyncSettings>::const_iterator syncSettingsItr = settings.m_syncSettings.begin();
        for (/*empty*/; syncSettingsItr != settings.m_syncSettings.end(); syncSettingsItr++)
        {
            // Check resync is scheduled for this DiskId
            std::map<std::string, int>::const_iterator resyncBatchItr = resyncBatch.InprogressResyncDiskIDsMap.find(syncSettingsItr->DiskId);
            if (resyncBatchItr != resyncBatch.InprogressResyncDiskIDsMap.end())
            {
                // Resync is scheduled for this DiskId and is in progress.
                continue;
            }

            // Resync is not yet scheduled, check ResyncCopyInitiatedTime is within permiable delay limits
            const boost::posix_time::ptime currentTime = boost::posix_time::microsec_clock::universal_time();

            const uint64_t resyncSetTimeInSec = syncSettingsItr->ResyncCopyInitiatedTimeTicks / HUNDRED_NANO_SECS_IN_SEC;

            const uint64_t secSinceAd001 = GetTimeInSecSinceAd0001();

            std::string healthIssueCode;
            SV_UINT startThresholdInSecs = 0;
            if (boost::iequals(syncSettingsItr->ResyncProgressType, RcmClientLib::ResyncProgressType::ManualResync))
            {
                static const SV_UINT manualResyncStartThresholdInSecs = RcmConfigurator::getInstance()->getManualResyncStartThresholdInSecs();
                if (secSinceAd001 >= (resyncSetTimeInSec + manualResyncStartThresholdInSecs))
                {
                    healthIssueCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncBatching::ManualResyncNotStarted;
                    startThresholdInSecs = manualResyncStartThresholdInSecs;
                }
            }
            else if (boost::iequals(syncSettingsItr->ResyncProgressType, RcmClientLib::ResyncProgressType::InitialReplication))
            {
                static const SV_UINT initialReplicationStartThresholdInSecs = RcmConfigurator::getInstance()->getInitialReplicationStartThresholdInSecs();
                if (secSinceAd001 >= (resyncSetTimeInSec + initialReplicationStartThresholdInSecs))
                {
                    healthIssueCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncBatching::InitialResyncNotStarted;
                    startThresholdInSecs = initialReplicationStartThresholdInSecs;
                }
            }
            else if (boost::iequals(syncSettingsItr->ResyncProgressType, RcmClientLib::ResyncProgressType::AutoResync))
            {
                static const SV_UINT autoResyncStartThresholdInSecs = RcmConfigurator::getInstance()->getAutoResyncStartThresholdInSecs();
                if (secSinceAd001 >= (resyncSetTimeInSec + autoResyncStartThresholdInSecs))
                {
                    healthIssueCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncBatching::AutoResyncNotStarted;
                    startThresholdInSecs = autoResyncStartThresholdInSecs;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Invalid ResyncProgressType %s for DiskId %s\n",
                    FUNCTION_NAME, syncSettingsItr->ResyncProgressType.c_str(), syncSettingsItr->DiskId.c_str());
            }

            if (!healthIssueCode.empty())
            {
                std::map<std::string, std::string> msgParams;
                msgParams.insert(std::make_pair(AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncBatching::StartThresholdInSecs,
                    boost::lexical_cast<std::string>(startThresholdInSecs)));
                diskHealthIssues.push_back(AgentDiskLevelHealthIssue(syncSettingsItr->DiskId, healthIssueCode, msgParams));
            }
        }
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
}
