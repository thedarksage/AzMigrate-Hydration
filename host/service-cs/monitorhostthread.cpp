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

#include "svstatus.h"
#include "monitorhostthread.h"
#include "logger.h"
#include "portablehelpers.h"
#include "inmquitfunction.h"
#include "appcommand.h"
#include "AgentHealthContract.h"
#include "inmalertdefs.h"
#include "inmageproduct.h"
#include "configwrapper.h"
#include "operatingsystem.h"
#include "monitoringparams.h"
#include "filterdrvifmajor.h"
#include "involfltfeatures.h"

#ifdef SV_WINDOWS
#include "portablehelpersmajor.h"
#include "indskfltfeatures.h"
#endif

#include "json_reader.h"
#include "json_writer.h"
#include "setpermissions.h"

using namespace boost::chrono;

const int BUFFERLEN = 1024 * 15;


MonitorHostThread::MonitorHostThread(Configurator* configurator, const unsigned int monitorHostInterval) :
  m_configurator(configurator),
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
    m_hostId = m_configurator->getVxAgent().getHostId();
    m_strHealthCollatorPath = m_configurator->getVxAgent().getHealthCollatorDirPath();

    VerifyMonitoringEvents();

    // first call to get the protected device list
    GetProtectedDeviceIds(m_monitorDeviceIds);

    m_configurator->getHostVolumeGroupSignal().connect(this, &MonitorHostThread::OnSettingsChange);

    steady_clock::time_point nextMonitorHostTime = steady_clock::now() +
        seconds(m_configurator->getVxAgent().getMonitorHostStartDelay());

    m_agentHealthCheckInterval = m_configurator->getVxAgent().getAgentHealthCheckInterval();
    ACE_Time_Value waitTime(m_agentHealthCheckInterval, 0);
    do
    {
        try
        {
            SourceAgentProtectionPairHealthIssues   healthIssues;
            CreateAgentHealthIssueMap(healthIssues);
            m_configurator->getVxAgent().ReportAgentHealthStatus(healthIssues);
            HealthIssueReported();

            UpdateFilePermissions();
        }
        catch (std::exception& e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s:Caught an exception %s while checking for Agent Health Issues.\n", 
                FUNCTION_NAME,e.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s:Caught a generic exception while checking for Agent Health Issues.\n",FUNCTION_NAME);
        }

        if (steady_clock::now() > nextMonitorHostTime)
        {
            MonitorHost(qf);
            nextMonitorHostTime = steady_clock::now() + seconds(m_MonitorHostInterval);
        }

        m_QuitEvent->wait(&waitTime, 0);

    } while (!m_threadManager.testcancel(ACE_Thread::self()));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return 0;
}

void MonitorHostThread::Stop()
{
    ACE_Time_Value waitTime(10, 0);

    m_configurator->getHostVolumeGroupSignal().disconnect(this);
    m_threadManager.cancel_all();
    m_QuitEvent->signal();

    do {
        if (m_bFilterDriverSupportsCxSession && m_pDriverStream)
        {
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
    ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_MonitorHostMutex);
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    try {
        std::vector<std::string> monitorHostCmdsV;
        LocalConfigurator localConfigurator;
        std::string cmds = localConfigurator.getMonitorHostCmdList();
        boost::char_separator<char> delm(",");
        boost::tokenizer < boost::char_separator<char> > strtokens(cmds, delm);
        for (boost::tokenizer< boost::char_separator<char> > ::iterator it = strtokens.begin(); it != strtokens.end(); ++it)
        {
            /// remove leading and trailing white space if any
            size_t firstws = (*it).find_first_not_of(" ");
            monitorHostCmdsV.push_back((*it).substr(firstws, (*it).find_last_not_of(" ") - firstws + 1));
        }

        std::stringstream tmpOpFile;
        tmpOpFile << m_configurator->getVxAgent().getInstallPath();
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
        linuxcmd += m_configurator->getVxAgent().getInstallPath();
        linuxcmd += "/bin/collect_support_materials.sh _mds_";
        ExecCmd(linuxcmd, tmpOpFile.str());
#endif
    }
    catch (std::exception &exception)
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED: %s with exception %s\n", FUNCTION_NAME, exception.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED: %s\n", FUNCTION_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

bool MonitorHostThread::QuitRequested(int seconds)
{
    ACE_Time_Value waitTime(seconds, 0);
    m_QuitEvent->wait(&waitTime, 0);
    return m_threadManager.testcancel(ACE_Thread::self());
}

void MonitorHostThread::MonitorLogUploadLocation(const std::string& ip, const std::string& port, std::string& message)
{
    static steady_clock::time_point firstFailTime = steady_clock::now();
    static bool LogUploadFailure = false;

    const std::string strCxPsClient("cxpsclient");
    const std::string strCxPsClientParamIpAddr(" -i ");
    const std::string strCxPsClientParamSslPort(" -l ");
    const std::string strCxPsClientParamTimeout(" -y ");

    std::string cmd = m_configurator->getVxAgent().getInstallPath();
    cmd += ACE_DIRECTORY_SEPARATOR_CHAR_A;
#ifdef SV_UNIX
    cmd += "bin";
    cmd += ACE_DIRECTORY_SEPARATOR_CHAR_A;
#endif
    cmd += strCxPsClient;
    cmd += strCxPsClientParamIpAddr;
    cmd += ip;
    cmd += strCxPsClientParamSslPort;
    cmd += port;
    cmd += strCxPsClientParamTimeout; // time out option
    cmd += boost::lexical_cast<std::string>(m_configurator->getVxAgent().getMonitoringCxpsClientTimeoutInSec());

    DebugPrintf(SV_LOG_DEBUG, "%s: Running cmd \"%s\"\n", FUNCTION_NAME, cmd.c_str());

    std::string cmdOutput;
    SV_UINT uTimeOut = 120;
    SV_ULONG lInmexitCode = 1;

    std::stringstream tmpOpFile;
    tmpOpFile << m_configurator->getVxAgent().getInstallPath();
    tmpOpFile << ACE_DIRECTORY_SEPARATOR_CHAR_A;
    tmpOpFile << "tmp_monloguploadloc_";
    tmpOpFile << ACE_OS::getpid();

    AppCommand objAppCommand(cmd, uTimeOut, tmpOpFile.str());
    bool bProcessActive = true;
    objAppCommand.Run(lInmexitCode, cmdOutput, bProcessActive, "", NULL);
    ACE_OS::unlink(getLongPathName(tmpOpFile.str().c_str()).c_str());

    DebugPrintf(SV_LOG_DEBUG, "%s: cmd output\"%s\"\n", FUNCTION_NAME, cmdOutput.c_str());

    // TODO-SanKumar-1807: There are few errors that doesn't contain the check server logs err token.
    // Should they be also included here?
    // 2018-Jun-21 07:34:10 threadId: 1a70 *** run 1: [at D:\en\InMage\host\cxpslib\client.h:BasicClient<class HttpTraits>::renameFile:614]   (sid: 934.234b56ddae0) oldName: tstdata/start_filename_start.1a70-0-234f3cec83198c00 newName: tstdata/end_filename_end.1a70-0-234f3cec83198c00 [at D:\en\InMage\host\cxpslib\client.h:BasicClient<class HttpTraits>::syncReadSome:1393]   read data from socket failed: [at d:\en\inmage\host\cxpslib\connection.h:BasicConnection<class boost::asio::ssl::stream<class boost::asio::basic_stream_socket<class boost::asio::ip::tcp,class boost::asio::stream_socket_service<class boost::asio::ip::tcp> > > >::readSome:344]   error reading data from socket: system:10054

    // look for error tokens as below
    // 2017-Oct-12 14:54:16 threadId: 7fa374513700 *** run 1: [at cxpslib/client.h:putFile:476]   (sid: ), remoteName: tstdata/start_filename_start.7fa374513700-0-c05b0331e0fae085, dataSize: 1048576, moreData: 0, error: [at cxpslib/client.h:asyncConnect:1976]   connect socket failed: system:111 Connection refused (may want to check server side logs for this sid)
    //2017-Oct-12 14:52:20 threadId: 7f78e028e700 *** run 1: [at cxpslib/client.h:putFile:476]   (sid: ), remoteName: tstdata/start_filename_start.7f78e028e700-0-390a296c7cd974ff, dataSize: 1048576, moreData: 0, error: [at cxpslib/client.h:asyncConnect:1973]   timed out (10) (may want to check server side logs for this sid)
    const std::string errorToken = "(may want to check server side logs for this sid)";
    size_t errIdx = cmdOutput.rfind(errorToken);
    if (errIdx != std::string::npos)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: LogUploadFailure = %d \n", FUNCTION_NAME, LogUploadFailure);

        if (!LogUploadFailure)
        {
            firstFailTime = steady_clock::now();
            LogUploadFailure = true;
        }
        else
        {
            if (steady_clock::now() > firstFailTime + seconds(10 * 60))
            {
                size_t idx = cmdOutput.rfind("]");
                message = cmdOutput.substr(idx, errIdx - idx);

                DebugPrintf(SV_LOG_ERROR, "%s: Drain log upload location access failed. Error %s\n",
                    FUNCTION_NAME,
                    message.c_str());
            }
        }
    }
    else
    {
        LogUploadFailure = false;
    }

    return;
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
        }
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with exception: %s\n", FUNCTION_NAME, e.what());
        return;
    }

    LocalConfigurator lc;
    std::string moitoringParamsFile;
    lc.getConfigDirname(moitoringParamsFile);
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
        while (!SendAlertToCx(SV_ALERT_SIMPLE, SV_ALERT_MODULE_GENERAL, a))
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
        while (!SendAlertToCx(SV_ALERT_SIMPLE, SV_ALERT_MODULE_GENERAL, a))
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
            DebugPrintf(SV_LOG_DEBUG, "%s: Serialized MoitoringParams to %s.\n", FUNCTION_NAME, moitoringParamsFile.c_str());

            std::string errormsg;
            if (!securitylib::setPermissions(moitoringParamsFile, errormsg)) {
                DebugPrintf(SV_LOG_ERROR, "%s\n", errormsg.c_str());
            }
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

void MonitorHostThread::GetDisksInDiscovery(std::set<std::string>& disksInDiscoverySet)
{
    std::string err;
    DataCacher::VolumesCache    volumesCache;

    if (!DataCacher::UpdateVolumesCache(volumesCache, err))
    {
        throw INMAGE_EX(err.c_str());
    }

    ConstVolumeSummariesIter itrVS = volumesCache.m_Vs.begin();
    for (/*empty*/; itrVS != volumesCache.m_Vs.end(); ++itrVS)
    {
        if (itrVS->type == VolumeSummary::DISK)
        {
            disksInDiscoverySet.insert(itrVS->name);
        }
    }
}

void    MonitorHostThread::UpdateMissingDiskList(std::vector<AgentDiskLevelHealthIssue>& diskHealthIssues)
{
    std::set<std::string> disksInDiscoverySet;
    GetDisksInDiscovery(disksInDiscoverySet);

    HOST_VOLUME_GROUP_SETTINGS hostVolumeGroupSettings = m_configurator->getHostVolumeGroupSettings();
    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupIter = hostVolumeGroupSettings.volumeGroups.begin();
    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupEnd = hostVolumeGroupSettings.volumeGroups.end();

    for (/* empty */; volumeGroupIter != volumeGroupEnd; ++volumeGroupIter)
    {
        VOLUME_GROUP_SETTINGS::volumes_iterator volumeIter = (*volumeGroupIter).volumes.begin();
        VOLUME_GROUP_SETTINGS::volumes_iterator volumeEnd = (*volumeGroupIter).volumes.end();

        for (/* empty*/; volumeIter != volumeEnd; ++volumeIter)
        {
            if (disksInDiscoverySet.find((*volumeIter).second.deviceName) == disksInDiscoverySet.end())
            {
                diskHealthIssues.push_back(AgentDiskLevelHealthIssue((*volumeIter).second.deviceName,
                    AgentHealthIssueCodes::DiskLevelHealthIssues::DiskRemoved::HealthCode));
            }
        }
    }
}

bool MonitorHostThread::GetDevicesInResyncMode(std::set<std::string> &deviceIds)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    HOST_VOLUME_GROUP_SETTINGS volGroups;

    deviceIds.clear();

    volGroups = m_configurator->getHostVolumeGroupSettings();
    std::string strDeviceId;

    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volGrpIter = volGroups.volumeGroups.begin();
    for (/*empty*/; volGrpIter != volGroups.volumeGroups.end(); volGrpIter++)
    {
        VOLUME_GROUP_SETTINGS::volumes_constiterator volIter = volGrpIter->volumes.begin();
        if (volIter->second.syncMode == VOLUME_SETTINGS::SYNC_FAST_TBC)
        {
            deviceIds.insert(volIter->first);
        }
    }

    if (deviceIds.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: no protected disks in resync mode.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

SVSTATUS MonitorHostThread::GetProtectedDeviceIds(std::set<std::string> &deviceIds)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    HOST_VOLUME_GROUP_SETTINGS volGroups;

    volGroups = m_configurator->getHostVolumeGroupSettings();

    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volGrpIter = volGroups.volumeGroups.begin();
    for (/*empty*/; volGrpIter != volGroups.volumeGroups.end(); volGrpIter++)
    {
        if (volGrpIter->direction == SOURCE)
        {
            VOLUME_GROUP_SETTINGS::volumes_constiterator volIter = volGrpIter->volumes.begin();
            deviceIds.insert(GetPersistentDeviceIdForDriverInput(volIter->first));
        }
    }

    if (deviceIds.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: no protected disks discovered.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

bool MonitorHostThread::SendHealthAlert(InmErrorAlertImp &a)
{
    return SendAlertToCx(SV_ALERT_SIMPLE, SV_ALERT_MODULE_GENERAL, a);
}

void MonitorHostThread::OnSettingsChange(HOST_VOLUME_GROUP_SETTINGS settings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!m_bFilterDriverSupportsCxSession || !m_pDriverStream)
        return;

    std::set<std::string> devIds;
    if (SVS_OK != GetProtectedDeviceIds(devIds))
    {
        return;
    }

    if (devIds != m_monitorDeviceIds)
    {
        boost::unique_lock<boost::mutex> lock(m_mutexMonitorDeviceIds);
        m_monitorDeviceIds = devIds;

        FilterDrvIfMajor drvif;
        drvif.CancelCxStats(m_pDriverStream.get());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void    MonitorHostThread::UpdateLogUploadLocationStatus(std::vector<HealthIssue>& healthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string strPsIp, strPsPort, strErrMessage;

    HOST_VOLUME_GROUP_SETTINGS hostVolumeGroupSettings = m_configurator->getHostVolumeGroupSettings();
    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupIter = hostVolumeGroupSettings.volumeGroups.begin();
    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupEnd = hostVolumeGroupSettings.volumeGroups.end();

    if (volumeGroupIter != volumeGroupEnd)
    {
        strPsIp = volumeGroupIter->transportSettings.ipAddress;
        strPsPort = volumeGroupIter->transportSettings.sslPort;
    }

    if (strPsIp.empty() || strPsPort.empty())
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: failed to get the drain log upload location details. IP %s Port %s.\n",
            FUNCTION_NAME,
            strPsIp.c_str(),
            strPsPort.c_str());
        return;
    }

    MonitorLogUploadLocation(strPsIp, strPsPort, strErrMessage);

    if (!strErrMessage.empty())
    {
        healthIssues.push_back(HealthIssue(AgentHealthIssueCodes::VMLevelHealthIssues::LogUploadLocationNotHealthy::HealthCode));
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}
