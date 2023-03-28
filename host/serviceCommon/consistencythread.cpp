#include <iostream>
#include <vector>
#include <fstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "consistencythread.h"
#include "portablehelpers.h"
#include "inmquitfunction.h"
#include "localconfigurator.h"
#include "transport_settings.h"
#include "cxtransport.h"
#include "inmalertdefs.h"

#ifdef SV_UNIX
#include "setpermissions.h"
#endif

using namespace TagTelemetry;
using namespace boost::chrono;

const std::string ConsistencyScheduleFile = "consistencysched.json"; // TODO: define in VacpConfDefs.h to ba accessible by vacp and svagents
const std::string LastAppConsistencyScheduleTime = "LastAppConsistencyScheduleTime";
const std::string LastCrashConsistencyScheduleTime = "LastCrashConsistencyScheduleTime";

Logger::Log ConsistencyThread::m_TagTelemetryLog;

extern bool GetIPAddressSetFromNicInfo(strset_t &ips, std::string &errMsg);

ConsistencyThread::ConsistencyThread() :
    m_started(false),
    m_active(false),
    m_QuitEvent(new ACE_Event(1, 0))
{
    m_prevSettingsFetchTime = steady_clock::time_point::min();
}

ConsistencyThread::~ConsistencyThread()
{
}

void ConsistencyThread::Start()
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

ACE_THR_FUNC_RETURN ConsistencyThread::ThreadFunc(void* arg)
{
    ConsistencyThread* p = static_cast<ConsistencyThread*>(arg);
    return p->run();
}

ACE_THR_FUNC_RETURN ConsistencyThread::run()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    {
        boost::shared_ptr<Logger::LogCutter> telemetryLogCutter(new Logger::LogCutter(m_TagTelemetryLog));

        try
        {
            LocalConfigurator lc;

            std::string logPath;
            if (Logger::GetLogFilePath(logPath))
            {
                boost::filesystem::path tagLogPath(logPath);
                tagLogPath /= "tagTelemetry.log";
                m_TagTelemetryLog.m_logFileName = tagLogPath.string();

                const  int maxCompletedLogFiles = lc.getLogMaxCompletedFiles();
                const boost::chrono::seconds cutInterval(lc.getLogCutInterval());
                const uint32_t logFileSize = lc.getLogMaxFileSizeForTelemetry();
                m_TagTelemetryLog.SetLogFileSize(logFileSize);

                telemetryLogCutter->Initialize(tagLogPath, maxCompletedLogFiles, cutInterval);
                telemetryLogCutter->StartCutting();
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s unable to init tag telemetry logger\n", FUNCTION_NAME);
            }

            m_hostId = lc.getHostId();
            lc.getConfigDirname(m_configDirName);
#ifdef SV_WINDOWS
            m_logDirName = lc.getCacheDirectory();
#else
            m_logDirName = lc.getLogPathname();
#endif

            if (!boost::filesystem::exists(m_configDirName))
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: directory doesn't exist %s. not starting consistency thread.\n",
                    FUNCTION_NAME,
                    m_configDirName.c_str());
                return 0;
            }

            m_logDirName += ACE_DIRECTORY_SEPARATOR_CHAR;
            m_logDirName += "ApplicationPolicyLogs";
            m_logDirName += ACE_DIRECTORY_SEPARATOR_CHAR;
            SVMakeSureDirectoryPathExists(m_logDirName.c_str());

            m_scheduleFilePath = m_configDirName;
            m_scheduleFilePath /= ConsistencyScheduleFile;
            
            uint32_t logParseInterval = lc.getConsistencyLogParseInterval();
            ACE_Time_Value waitTime(logParseInterval, 0);
            while (!m_threadManager.testcancel(ACE_Thread::self()))
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: calling ProcessConsistencyLogs\n", FUNCTION_NAME);
                
                ProcessConsistencyLogs();

                m_QuitEvent->wait(&waitTime, 0);
            }

        }
        catch (std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Caught exception %s. Exiting consistency thread.\n", FUNCTION_NAME, e.what());
            return 0;
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Caught unknown exception in consistency thread.\n", FUNCTION_NAME);
            return 0;
        }

        
    }
    m_TagTelemetryLog.CloseLogFile();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return 0;
}

void ConsistencyThread::Stop()
{
    m_active = false;
    m_threadManager.cancel_all();
    m_QuitEvent->signal();
    m_threadManager.wait();
    m_started = false;
}

bool ConsistencyThread::QuitRequested(int seconds)
{
    ACE_Time_Value waitTime(seconds, 0);
    m_QuitEvent->wait(&waitTime, 0);
    return m_threadManager.testcancel(ACE_Thread::self());
}

bool ConsistencyThread::GetFileContent(const std::string& filename, std::string& output)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, filename.c_str());

    bool retval = true;
    try
    {
        std::ifstream fileStream(filename.c_str());
        if (fileStream.good())
        {
            output = std::string((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
            fileStream.close();
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to read file %s - file stream not in good state.\n", FUNCTION_NAME, filename.c_str());
            retval = false;
        }
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to read file %s with an exception %s.\n", FUNCTION_NAME, filename.c_str(), e.what());
        retval = false;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to read file %s with an unknown exception.\n", FUNCTION_NAME, filename.c_str());
        retval = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retval;
}

void ConsistencyThread::ParseCmdOptions(const std::string& cmdOptions, TagStatus& tagStatus)
{
    std::string errMsg;

    uint32_t tagTypeConf = 0;

    if (std::string::npos == cmdOptions.find("-distributed"))
        tagTypeConf = SINGLE_NODE;
    else
    {
        tagTypeConf = MULTI_NODE;

        std::size_t mnPos = cmdOptions.find(NsVacpOutputKeys::MultVmMasterNodeKey);
        if (std::string::npos != mnPos)
        {
            std::string masterNode;
            GetValueForPropertyKey(cmdOptions, NsVacpOutputKeys::MultVmMasterNodeKey, masterNode);

            if (masterNode.empty())
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: master node value parsing failed. input %s\n",
                    FUNCTION_NAME,
                    cmdOptions.c_str());
            }

            ADD_STRING_TO_MAP(tagStatus, MASTERNODE, masterNode);
        }

        std::size_t cnPos = cmdOptions.find(NsVacpOutputKeys::MultVmClientNodesKey);
        if (std::string::npos != cnPos)
        {
            std::string strClientNodes;
            GetValueForPropertyKey(cmdOptions, NsVacpOutputKeys::MultVmClientNodesKey, strClientNodes);

            if (strClientNodes.empty())
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: client nodes value parsing failed. input %s\n",
                    FUNCTION_NAME,
                    cmdOptions.c_str());
            }

            ADD_STRING_TO_MAP(tagStatus, CLIENTNODES, strClientNodes);
        }
    }

    try
    {
        tagTypeConf |= boost::lexical_cast<uint32_t>(tagStatus.Map[TAGTYPECONF]);
    }
    catch (const boost::bad_lexical_cast &e)
    {
        std::stringstream ss;
        ss << "Caught exception in parsing key tagTypeConf: " << e.what();
        DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, ss.str().c_str());
    }
    ADD_INT_TO_MAP(tagStatus, TAGTYPECONF, tagTypeConf);

    return;
}

uint64_t ConsistencyThread::GetLastConsistencyScheduleTime(ConsistencyType type)
{
    uint64_t lastRunTime = 0;

    if (!boost::filesystem::exists(m_scheduleFilePath))
        return 0;
    
    namespace bpt = boost::property_tree;
    bpt::ptree pt;
    uint64_t appSchedTime = 0, crashSchedTime = 0;

    try {
        bpt::json_parser::read_json(m_scheduleFilePath.string(), pt);

        appSchedTime = pt.get<uint64_t>(LastAppConsistencyScheduleTime, 0);
        crashSchedTime = pt.get<uint64_t>(LastCrashConsistencyScheduleTime, 0);
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : reading consistency schedule file failed. %s\n",
            FUNCTION_NAME,
            e.what());
        return 0;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : reading consistency schedule file failed with unknown exception\n",
            FUNCTION_NAME);
        return 0;
    }

    DebugPrintf(SV_LOG_INFO,
        "%s: LastAppConsistencyScheduleTime :%lld, LastCrashConsistencyScheduleTime : %lld\n",
        FUNCTION_NAME, appSchedTime, crashSchedTime);

    if (type == CONSISTENCY_TYPE_APP)
        return appSchedTime;
    else if (type == CONSISTENCY_TYPE_CRASH)
        return crashSchedTime;
    else
        return 0;
}

void ConsistencyThread::TruncatePolicyLogs()
{
    const uint64_t ONEDAY = 24 * 60 * 60;
    
    CxTransport::ptr cxTransport(new CxTransport(TRANSPORT_PROTOCOL_FILE, /* dummy place holder */ TRANSPORT_CONNECTION_SETTINGS(), false));
    std::string fileSpec = m_logDirName + "*olicy_*.log";
    FileInfosPtr fileInfos(new FileInfos_t);
    if (!cxTransport->listFile(fileSpec, *fileInfos.get()))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: GetFileList failed with status: %s\n", FUNCTION_NAME, cxTransport->status());
        return;
    }

    FileInfos_t::iterator iter(fileInfos->begin());
    FileInfos_t::iterator iterEnd(fileInfos->end());
    try
    {
        for ( /* empty */; (iter != iterEnd) && (!QuitRequested(0)); ++iter)
        {
            std::string fileName = m_logDirName;
            fileName += iter->m_name;
            time_t modTime = boost::filesystem::last_write_time(fileName);
            time_t curTime = time(NULL);
            time_t duration = curTime - modTime;
            if (duration > ONEDAY)
            {
                boost::filesystem::remove(fileName);
            }
        }
    }
    catch (const boost::filesystem::filesystem_error &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught exception %s.\n", e.what());
    }
    return;
}

void ConsistencyThread::ProcessConsistencyLogs()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try
    {
        static uint16_t ccFailCnt = 0;

        CxTransport fileops(TRANSPORT_PROTOCOL_FILE, /* dummy place holder */ TRANSPORT_CONNECTION_SETTINGS(), false);
        std::string fileSpec = m_logDirName + "VacpLog_*.log";
        FileInfos_t fileInfos;
        if (!fileops.listFile(fileSpec, fileInfos))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: GetFileList failed with status: %s\n", FUNCTION_NAME, fileops.status());
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }

        FileInfos_t::iterator iter(fileInfos.begin());
        FileInfos_t::iterator iterEnd(fileInfos.end());

        for ( /* empty */; (iter != iterEnd) && (!QuitRequested(0)); ++iter)
        {
            TagStatus tagStatus;

            ADD_INT_TO_MAP(tagStatus, MESSAGETYPE, TAG_TABLE_AGENT_LOG_V2);

            std::stringstream sserr;
            std::string fileName = m_logDirName;
            fileName += (*iter).m_name;
            std::string cmdOutput;
            bool bFileRead = GetFileContent(fileName, cmdOutput);
            if (!bFileRead || cmdOutput.empty())
            {
                if (!bFileRead)
                {
                    sserr << "Failed to open file " << fileName << " for reading.";
                    ADD_INT_TO_MAP(tagStatus, MESSAGETYPE, TAG_TABLE_AGENT_LOG_BAD_FILE);
                }
                else
                {
                    sserr << "File " << fileName << " is empty";
                    ADD_INT_TO_MAP(tagStatus, MESSAGETYPE, TAG_TABLE_AGENT_LOG_EMPTY_FILE);
                }
                DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, sserr.str().c_str());

                std::size_t start = fileName.find_last_of("_");
                std::string fileTimeStamp = fileName.substr(start + 1, fileName.find(".log") - (start + 1));

                ADD_INT_TO_MAP(tagStatus, VACPSTARTTIME, fileTimeStamp);
                std::stringstream ssTel;
                ssTel << tagStatus.getValue(MESSAGE) << " ;";
                ssTel << sserr.str();
                ADD_STRING_TO_MAP(tagStatus, MESSAGE, ssTel.str());

                try
                {
                    std::string strTagStatus = JSON::producer<TagStatus>::convert(tagStatus);
                    m_TagTelemetryLog.Printf(strTagStatus);

                    if (!fileops.deleteFile(fileName))
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: Failed to remove %s, status %s\n", FUNCTION_NAME, fileName.c_str(), fileops.status());
                    }
                }
                catch (std::exception &e)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to serialize tag status for telemetry. %s\n", FUNCTION_NAME, e.what());
                }

                continue;
            }


            boost::char_separator<char> sep("\n");
            tokenizer_t linesCmdOutput(cmdOutput, sep);

            std::stringstream sserrReqKeysAbsent;
            std::string strTagVal;
            std::string VacpSysTime, VacpStartTime, VacpEndTime;

            GetValueForPropertyKey(linesCmdOutput, NsVacpOutputKeys::VacpSysTimeKey, VacpSysTime);
            GetValueForPropertyKey(linesCmdOutput, NsVacpOutputKeys::VacpStartTimeKey, VacpStartTime);
            if (VacpStartTime.empty())
            {
                std::size_t start = fileName.find_last_of("_");
                VacpStartTime = fileName.substr(start + 1, fileName.find(".log") - (start + 1));
                sserrReqKeysAbsent << "key " << NsVacpOutputKeys::VacpStartTimeKey << " not found in " << fileName;
            }

            GetValueForPropertyKey(linesCmdOutput, NsVacpOutputKeys::VacpEndTimeKey, VacpEndTime);

            ADD_INT_TO_MAP(tagStatus, SYSTIMETAGT, VacpSysTime);
            ADD_INT_TO_MAP(tagStatus, VACPSTARTTIME, VacpStartTime);
            ADD_INT_TO_MAP(tagStatus, VACPENDTIME, VacpEndTime);

            std::string strInternallyRescheduled;
            GetValueForPropertyKey(linesCmdOutput, NsVacpOutputKeys::InternallyRescheduled, strInternallyRescheduled);

            if (0 == strInternallyRescheduled.compare("1"))
            {
                ADD_INT_TO_MAP(tagStatus, MESSAGETYPE, TAG_TABLE_AGENT_LOG_APP_RETRY_ON_REBOOT);
            }

            TagType tagType = INVALID_TAGTYPE;

            GetValueForPropertyKey(linesCmdOutput, NsVacpOutputKeys::TagType, strTagVal);
            if (0 == strTagVal.compare(CRASH_TAG))
            {
                tagType = CRASH_CONSISTENCY;
            }
            else if (0 == strTagVal.compare(APP_TAG))
            {
                tagType = APP_CONSISTENCY;
            }
            else if (0 == strTagVal.compare(BASELINE_TAG))
            {
                tagType = BASELINE_CONSISTENCY;
            }
            else
            {
                sserrReqKeysAbsent << "\n unknown tag type " << strTagVal << " found in " << fileName;
            }

            if (strTagVal.empty())
            {
                sserrReqKeysAbsent << "\n key " << NsVacpOutputKeys::TagType << " not found in " << fileName;
            }

            uint32_t policyId = 0;
            uint32_t tagTypeConf = INVALID_TAGTYPE;
            switch (tagType)
            {
            case CRASH_CONSISTENCY:
                policyId = POLICY_CRASH;
                tagTypeConf = CRASH_CONSISTENCY;
                break;
            case APP_CONSISTENCY:
                policyId = POLICY_APP;
                tagTypeConf = APP_CONSISTENCY;
                break;
            case BASELINE_CONSISTENCY:
                policyId = POLICY_BASELINE;
                tagTypeConf = BASELINE_CONSISTENCY;
                break;
            default:
                policyId = POLICY_UNKNOWN;
                tagTypeConf = INVALID_TAGTYPE;
                break;
            }

            ADD_INT_TO_MAP(tagStatus, POLICYID, policyId);
            ADD_INT_TO_MAP(tagStatus, TAGTYPECONF, tagTypeConf);

            std::string vacpCmd;
            GetValueForPropertyKey(linesCmdOutput, NsVacpOutputKeys::VacpCommand, vacpCmd);
            ParseCmdOptions(vacpCmd, tagStatus);

            uint64_t crashConsistencyInterval = 0;
            uint64_t appConsistencyInterval = 0;
            GetConsistencyIntervals(crashConsistencyInterval, appConsistencyInterval);
            ADD_INT_TO_MAP(tagStatus, APPCONSINTERVAL, appConsistencyInterval);
            ADD_INT_TO_MAP(tagStatus, CRASHCONSINTERVAL, crashConsistencyInterval);

            ADD_INT_TO_MAP(tagStatus, NUMOFNODES, GetNoOfNodes());

            AddDiskInfoInTagStatus(tagType, cmdOutput, tagStatus);

            SV_ULONG lInmexitCode = 0;
            GetValueForPropertyKey(linesCmdOutput, NsVacpOutputKeys::VacpExitStatus, strTagVal);
            try
            {
                lInmexitCode = boost::lexical_cast<SV_ULONG>(strTagVal);
            }
            catch (const boost::bad_lexical_cast &e)
            {
                sserrReqKeysAbsent << "\n key " << NsVacpOutputKeys::VacpExitStatus << " not found in " << fileName;
            }

            if (!sserrReqKeysAbsent.str().empty())
            {
                std::stringstream ssTel;
                if (!tagStatus.getValue(MESSAGE).empty())
                    ssTel << tagStatus.getValue(MESSAGE) << " ;";
                ssTel << sserrReqKeysAbsent.str();
                ADD_STRING_TO_MAP(tagStatus, MESSAGE, ssTel.str());
                ADD_INT_TO_MAP(tagStatus, MESSAGETYPE, TAG_TABLE_AGENT_LOG_REQUIRED_KEYS_NOT_FOUND);
            }

            std::stringstream failedNodes;
            failedNodes.str("");
            std::size_t start = 0;
            // Using filter "Host:" to identify vacp exit status
            start = cmdOutput.find(HostKey);
            if (start != std::string::npos)
            {
                std::size_t end = cmdOutput.find(SearchEndKey, start);
                if (end != std::string::npos)
                {
                    failedNodes << cmdOutput.substr(start + HostKey.length(), end - (start + HostKey.length())).c_str();
                }
            }
            start = cmdOutput.find(CoordNodeKey);
            while (start != std::string::npos)
            {
                std::size_t end = cmdOutput.find(SearchEndKey, start);
                if (end != std::string::npos)
                {
                    if (!failedNodes.str().empty()) { failedNodes << ", "; } // appending delimeter
                    failedNodes << cmdOutput.substr(start + CoordNodeKey.length(), end - (start + CoordNodeKey.length())).c_str();
                }
                start = cmdOutput.find(CoordNodeKey, end);
            }

            start = cmdOutput.find(NotConnectedNodes);
            if (start != std::string::npos)
            {
                std::size_t end = cmdOutput.find(SearchEndKey, start);
                if (end != std::string::npos)
                {
                    if (!failedNodes.str().empty()) { failedNodes << ", "; }
                    failedNodes << cmdOutput.substr(start, end - start).c_str();
                }
            }

            start = cmdOutput.find(ExcludedNodes);
            if (start != std::string::npos)
            {
                std::size_t end = cmdOutput.find(SearchEndKey, start);
                if (end != std::string::npos)
                {
                    if (!failedNodes.str().empty()) { failedNodes << ", "; }
                    failedNodes << cmdOutput.substr(start, end - start).c_str();
                }
            }

            std::stringstream ssErrMsg;
            bool bSuccess = (0 == lInmexitCode);
            SV_LOG_LEVEL    logLevel = SV_LOG_ALWAYS;
            std::list<HealthIssue>vacpAndTagsHealthIssues;

            ParseCmdOutput(cmdOutput, tagStatus, lInmexitCode);

            if ((tagType == APP_CONSISTENCY) ||
                (tagType == CRASH_CONSISTENCY))
            {
                // TODO - move to a method
                ssErrMsg << ((tagType == APP_CONSISTENCY) ? "AppConsistency" : "CrashConsistency");

                // mark failure if all disks are not tagged
                if (bSuccess)
                {
                    bool    bAllDisksTagged = true;
                    std::string     numDiskInPolicy = tagStatus.getValue(NUMOFDISKINPOLICY);
                    std::string     numDiskTagged = tagStatus.getValue(NUMOFDISKWITHTAG);


                    if (!numDiskInPolicy.empty() && !numDiskTagged.empty() &&
                        (atoi(numDiskInPolicy.c_str()) != atoi(numDiskTagged.c_str())))
                    {
                        bSuccess = bAllDisksTagged = false;
                        logLevel = SV_LOG_ERROR;
                    }

                    ssErrMsg << " Tag " << (bSuccess ? "succeeded." : "failed.");

                    if (!numDiskTagged.empty())
                        ssErrMsg << " numDiskInPolicy = " << numDiskInPolicy
                            << ", numDiskTagged = " << numDiskTagged << ".";
                }
                else
                {
                    ssErrMsg << " Tag failed.";
                }

                // reset the failure count on either of the tags success
                if (bSuccess)
                    ccFailCnt = 0;

                if (!bSuccess || !failedNodes.str().empty())
                {
                    if (!failedNodes.str().empty())
                    {
                        ADD_INT_TO_MAP(tagStatus, MULTIVMSTATUS, VACP_MULTIVM_FAILED);
                        ADD_STRING_TO_MAP(tagStatus, MULTIVMREASON, failedNodes.str());
                    }

                    if (lInmexitCode)
                        ssErrMsg << " Exit code : " << lInmexitCode << ".";

                    if (!failedNodes.str().empty())
                        ssErrMsg << " Failed nodes: " << failedNodes.str() << ".";

                    // only master node can send alert
                    bool bIsMasterNode = IsMasterNode();

                    bool isNonDataNode = false;
                    if (!failedNodes.str().empty())
                    {
                        if (std::string::npos != failedNodes.str().find(NonDataModeError))
                        {
                            // this covers for Linux master node as well
                            isNonDataNode = true;
                        }
                    }

                    std::string strVssWriterName;
                    std::string strVssErrorCode;
                    std::string strVssOperation;
                    std::string strVssProviderTelemetry;

                    boost::shared_ptr<InmAlert> pInmAlert;
                    std::list<HealthIssue> consistencyHealthIssues;

                    if (tagType == CRASH_CONSISTENCY)
                    {
                        ccFailCnt++;
                        if (VACP_E_DRIVER_IN_NON_DATA_MODE == lInmexitCode || isNonDataNode)
                        {
                            pInmAlert.reset(new CrashConsistencyFailureNonDataModeAlert(failedNodes.str(), vacpCmd));
                        }
                        else
                        {
                            pInmAlert.reset(new CrashConsistencyFailureAlert(failedNodes.str(), vacpCmd, lInmexitCode));
                        }

                        if (IsTagFailureHealthIssueRequired())
                        {
                            std::map<std::string, std::string> healthIssueParams;
                            uint64_t crashReplicationInterval = ccFailCnt * crashConsistencyInterval;
                            healthIssueParams.insert(std::make_pair(AgentHealthIssueCodes::VMLevelHealthIssues::CrashConsistencyTagFailure::ReplicationInterval,
                                boost::lexical_cast<std::string>(crashReplicationInterval)));
                            consistencyHealthIssues.push_back(HealthIssue(AgentHealthIssueCodes::VMLevelHealthIssues::CrashConsistencyTagFailure::HealthCode,
                                healthIssueParams));
                        }
                    }
                    else
                    {
                        pInmAlert.reset(new AppConsistencyFailureAlert(failedNodes.str(), vacpCmd, lInmexitCode));
#ifdef SV_WINDOWS
                        GetValueForPropertyKey(linesCmdOutput, NsVacpOutputKeys::VssWriterError, strVssWriterName);
                        GetValueForPropertyKey(linesCmdOutput, NsVacpOutputKeys::VssWriterErrorCode, strVssErrorCode);
                        GetValueForPropertyKey(linesCmdOutput, NsVacpOutputKeys::VssWriterErrorOperation, strVssOperation);
                        GetValueForPropertyKey(linesCmdOutput, NsVacpOutputKeys::VssProviderError, strVssProviderTelemetry);

                        if (!strVssProviderTelemetry.empty())
                        {
                            std::stringstream ssVssTelemetry;
                            ssVssTelemetry << tagStatus.getValue(MESSAGE) << " ; ";
                            ssVssTelemetry << NsVacpOutputKeys::VssProviderError << " : " << strVssProviderTelemetry << " ; ";
                            ADD_STRING_TO_MAP(tagStatus, MESSAGE, ssVssTelemetry.str());
                        }

                        if (!strVssWriterName.empty())
                        {
                            std::stringstream ssVssTelemetry;
                            ssVssTelemetry << tagStatus.getValue(MESSAGE) << " ; ";
                            ssVssTelemetry << NsVacpOutputKeys::VssWriterError << " : " << strVssWriterName << " ; ";
                            ssVssTelemetry << NsVacpOutputKeys::VssWriterErrorCode << " : " << strVssErrorCode << " ; ";
                            ssVssTelemetry << NsVacpOutputKeys::VssWriterErrorOperation << " : " << strVssOperation << " ; ";
                            ADD_STRING_TO_MAP(tagStatus, MESSAGE, ssVssTelemetry.str());
                        }
#endif

                        if (IsTagFailureHealthIssueRequired())
                        {
                            std::map<std::string, std::string> healthIssueParams;
                            uint64_t appReplicationInterval = appConsistencyInterval;
                            healthIssueParams.insert(std::make_pair(AgentHealthIssueCodes::VMLevelHealthIssues::AppConsistencyTagFailure::ReplicationInterval,
                                boost::lexical_cast<std::string>(appReplicationInterval)));
                            consistencyHealthIssues.push_back(HealthIssue(AgentHealthIssueCodes::VMLevelHealthIssues::AppConsistencyTagFailure::HealthCode,
                                healthIssueParams));
                        }
                    }

                    // flag alert for each app and each 3rd consecutive crash consistency failure
                    if ((0 != strInternallyRescheduled.compare("1")) && bIsMasterNode && (tagType == APP_CONSISTENCY || !(ccFailCnt %= 3)))
                    {
                        SendAlert(*pInmAlert);
                        ssErrMsg << " Sent alert \"" << pInmAlert->GetMessage() << "\".";

                        if (!consistencyHealthIssues.empty())
                            ssErrMsg << " Sending health issue " << consistencyHealthIssues.front().IssueCode << ".";

                        vacpAndTagsHealthIssues.insert(vacpAndTagsHealthIssues.end(), consistencyHealthIssues.begin(), consistencyHealthIssues.end());
                    }
                    else
                    {
                        ssErrMsg << " Skipped alert \"" << pInmAlert->GetMessage() << "\".";
                    }
                }

                DebugPrintf(logLevel, "%s\n", ssErrMsg.str().c_str());

                std::string strTagGuid = tagStatus.getValue(TAGGUID);

                ProcessVacpAndTagsHealthIssues(tagType, bSuccess, strTagGuid, vacpAndTagsHealthIssues);
            }

            try {
                std::string strTagStatus = JSON::producer<TagStatus>::convert(tagStatus);
                m_TagTelemetryLog.Printf(strTagStatus);
            }
            catch (std::exception &e)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to serialize tag status for telemetry. Exception: %s\n", e.what());
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to serialize tag status for telemetry with unknown exception.\n");
            }

            // Archieve file
            {
                std::string newName = fileName.substr(fileName.find_last_of(ACE_DIRECTORY_SEPARATOR_CHAR) + 1);
                newName = newName.substr(newName.find("_"));

                std::string appPolicyLogPath = m_logDirName;
                appPolicyLogPath += ACE_DIRECTORY_SEPARATOR_CHAR;
                appPolicyLogPath += "policy";
                appPolicyLogPath += newName;

                int renameRetry = 4;
                while (--renameRetry)
                {
                    if (fileops.renameFile(fileName, appPolicyLogPath, COMPRESS_NONE))
                    {
                        break;
                    }
                    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
                }
                if (!renameRetry)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Rename failed for %s, status %s\n", FUNCTION_NAME, fileName.c_str(), fileops.status());
                }

#if defined(SV_UNIX) || defined(SV_LINUX)
                try
                {
                    securitylib::setPermissions(appPolicyLogPath);
                }
                catch(ErrorException &ec)
                {
                    DebugPrintf(SV_LOG_ERROR, "Exception: %s Failed to set the permissions for file %s\n", ec.what(), appPolicyLogPath.c_str());
                }
                catch(...)
                {
                    DebugPrintf(SV_LOG_ERROR, "Generic exception: Failed to set the permissions for file %s\n", appPolicyLogPath.c_str());
                }
#endif
            }

        } // for loop

        TruncatePolicyLogs();
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Caught exception %s. Exiting consistency thread.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Caught unknown exception.\n", FUNCTION_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void ConsistencyThread::
ProcessVacpAndTagsHealthIssues(TagTelemetry::TagType& tagtype,
    bool bSuccess, std::string strTagGuid, std::list<HealthIssue>&vacpAndTagsHealthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string strVacpJsonFile;
    try
    {
        if (!bSuccess && !strTagGuid.empty())
        {
            strVacpJsonFile = m_logDirName + strTagGuid + std::string(".json");
            if (boost::filesystem::exists(strVacpJsonFile))
            {
                std::ifstream hVacpJsonFile(strVacpJsonFile.c_str());
                if (hVacpJsonFile.good())
                {
                    std::string strVacpJsonErrors((std::istreambuf_iterator<char>(hVacpJsonFile)), (std::istreambuf_iterator<char>()));
                    hVacpJsonFile.close();
                    VacpErrorInfo vacpErrInfo = JSON::consumer<VacpErrorInfo>::convert(strVacpJsonErrors);
                    std::vector<VacpError> vVacpErrors = vacpErrInfo.getVacpErrors();
                    std::vector<VacpError>::iterator iter;
                    for (iter = vVacpErrors.begin(); iter != vVacpErrors.end(); iter++)
                    {
                        vacpAndTagsHealthIssues.push_back(HealthIssue(iter->IssueCode,iter->MessageParams));
                    }
                }
                boost::filesystem::remove(strVacpJsonFile);//since the processing is over, delete this file
            }
        }

         m_SetLastTagStatusCallback(tagtype, bSuccess, vacpAndTagsHealthIssues);
        
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "Caught a generic expression in processing VacpHealth issue files.\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}
