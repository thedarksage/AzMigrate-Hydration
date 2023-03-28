#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "SimulatedTestMachine.h"
#include "RcmTestClient.h"
#include "CmdArgs.h"
#include "RcmClientImpl.h"
#include "MachineEntity.h"
#include "common/Trace.h"
#include "HttpClient.h"
#include "volumegroupsettings.h"
#include "logger.h"
#include "securityutils.h"
#include "HttpUtil.h"

#include "inmalertdefs.h"
#include "ServiceBusQueue.h"
#include "MonitoringMsgSettings.h"
#include "Monitoring.h"
#include "AgentRcmLogging.h"

#ifdef SV_WINDOWS
#include "portablehelpersmajor.h"
#endif

using namespace std;
using namespace RcmTestClient;
using namespace RcmClientLib;
using namespace AzureStorageRest;
using  namespace AzureStorageRest::HttpRestUtil::ServiceBusConstants;



void FillVolumeSummary(SerailizableVolumeSummary & serVolSum,
    VolumeSummary & volSum)
{
    volSum.id = serVolSum.id;
    volSum.name = serVolSum.name;
    volSum.type = (VolumeSummary::Devicetype)serVolSum.type;
    volSum.vendor = (VolumeSummary::Vendor)serVolSum.vendor;

    volSum.fileSystem = serVolSum.fileSystem;
    volSum.mountPoint = serVolSum.mountPoint;
    volSum.isMounted = serVolSum.isMounted;
    volSum.isSystemVolume = serVolSum.isSystemVolume;
    volSum.isCacheVolume = serVolSum.isCacheVolume;

    volSum.capacity = serVolSum.capacity;
    volSum.locked = serVolSum.locked;
    volSum.physicalOffset = serVolSum.physicalOffset;

    volSum.sectorSize = serVolSum.sectorSize;
    volSum.volumeLabel = serVolSum.volumeLabel;
    volSum.containPagefile = serVolSum.containPagefile;
    volSum.isStartingAtBlockZero = serVolSum.isStartingAtBlockZero;

    volSum.volumeGroup = serVolSum.volumeGroup;
    volSum.volumeGroupVendor = (VolumeSummary::Vendor) serVolSum.volumeGroupVendor;
    volSum.isMultipath = serVolSum.isMultipath;

    volSum.rawcapacity = serVolSum.rawcapacity;
    volSum.writeCacheState = (VolumeSummary::WriteCacheState) serVolSum.writeCacheState;
    volSum.formatLabel = (VolumeSummary::FormatLabel) serVolSum.formatLabel;

    if (serVolSum.attributes.length() != 0)
    {
        std::vector<std::string> attribute_tokens;
        boost::split(attribute_tokens, serVolSum.attributes, boost::is_any_of(","));

        std::vector<std::string>::iterator iter = attribute_tokens.begin();
        while (iter != attribute_tokens.end())
        {
            std::vector<std::string> attribute;
            boost::split(attribute, *iter, boost::is_any_of(":"));
            if (attribute.size() == 2)
            {
                boost::trim(attribute[0]);
                boost::trim(attribute[1]);
                volSum.attributes.insert(std::make_pair(attribute[0], attribute[1]));
            }
            else
                std::cout << "Error: attributes are not in required format" << std::endl;

            ++iter;
        }
    }

    SerializableVolumeSummariesIterator serVolIter = serVolSum.svsChildren.begin();
    while (serVolIter != serVolSum.svsChildren.end())
    {
        VolumeSummary cvolSum;
        FillVolumeSummary(*serVolIter, cvolSum);
        volSum.children.insert(volSum.children.end(), cvolSum);
        serVolIter++;
    }

    return;
}

void CreateVolumeSummaries(SerailizableVolumeSummaries_t serVolSums,
    VolumeSummaries_t& volSums)
{
    SerializableVolumeSummariesIterator serVolIter = serVolSums.begin();
    while (serVolIter != serVolSums.end())
    {
        VolumeSummary volSum;
        FillVolumeSummary(*serVolIter, volSum);
        volSums.insert(volSums.end(), volSum);
        serVolIter++;
    }
    return;
}

size_t ReadFromFile(std::string filename, std::string& str)
{
    std::ifstream infile;
    infile.open(filename.c_str(), std::ifstream::in);

    if (infile.good())
    {
        std::stringstream strstream;
        strstream << infile.rdbuf();
        str = strstream.str();
    }

    infile.close();

    return str.size();
}

size_t WriteToFile(std::string filename, std::string& str)
{
    std::ofstream outfile;
    outfile.open(filename.c_str(), std::ofstream::out);

    if (outfile.good())
    {
        outfile << str;
    }

    outfile.close();

    return str.size();
}

#ifdef _DEBUG
int32_t TestConnection(RcmClientSettings& settings)
{
    int32_t ret;
    std::string hostId = GenerateUuid();

    RcmTestClient::TestClient rcmTestClient(settings, hostId);

    ret = rcmTestClient.TestConnection();

    if (ret != 0)
    {
        ret = rcmTestClient.GetErrorCode();
        cout << "TestConnection failed with " << ret << std::endl;
    }
    else
    {
        std::cout << "TestConnection succeeded." << std::endl;
    }

    return ret;
}
#endif //_DEBUG

int32_t TestRegisterMachine(RcmClientSettings& settings, std::string& strJson)
{
    int32_t ret = -1;

    MachineSummary machineSummary =
        JSON::consumer<MachineSummary>::convert(strJson);

    VolumeSummaries_t volSums;
    CreateVolumeSummaries(machineSummary.VolumeSummaries, volSums);

    Object osObject;
    CpuInfos_t cpuInfo;

    if (machineSummary.osAttributes.length() != 0)
    {
        std::vector<std::string> attribute_tokens;
        boost::split(attribute_tokens, machineSummary.osAttributes, boost::is_any_of(","));

        std::vector<std::string>::iterator iter = attribute_tokens.begin();
        while (iter != attribute_tokens.end())
        {
            std::vector<std::string> attribute;
            boost::split(attribute, *iter, boost::is_any_of(":"));
            if (attribute.size() == 2)
            {
                boost::trim(attribute[0]);
                boost::trim(attribute[1]);
                osObject.m_Attributes.insert(std::make_pair(attribute[0], attribute[1]));
            }
            else
                std::cout << "Error: OS attributes are not in required format" << std::endl;

            ++iter;
        }
    }

    HypervisorInfo_t hypervinfo;
    if (machineSummary.hyperVisorName.length() != 0)
    {
        hypervinfo.state = HypervisorInfo::HyperVisorPresent;
        hypervinfo.name = machineSummary.hyperVisorName;
        hypervinfo.version = machineSummary.hyperVisorVersion;
    }

    RcmTestClient::TestClient rcmTestClient(settings, machineSummary.hostId);

    ret = rcmTestClient.TestRegisterMachine(machineSummary.hostName,
        machineSummary.biosId,
        machineSummary.managementId,
        osObject,
        (OS_VAL)machineSummary.osType,
        (ENDIANNESS)machineSummary.endianness,
        cpuInfo,
        hypervinfo,
        volSums,
        true);
    if (ret != 0)
    {
        ret = rcmTestClient.GetErrorCode();
        cout << "RegisterMachine failed with " << ret << std::endl;
    }
    else
    {
        std::cout << "RegisterMachine succeeded." << std::endl;
    }

    return ret;
}

int32_t TestRegisterSourceAgent(RcmClientSettings& settings, std::string& strJson)
{
    int32_t ret = -1;
    std::string hostName = "MSW12R2-01";
    std::string hostId = "d20d3be0-489b-4603-a3bf-92042226e43f";
    std::string agentVer = "9.0.1.0 GA";
    std::string driverVer = "1.5";
    std::string installPath = "C:\\Program Files\\Microsoft Azure Site Recovery\\agent";

    RcmTestClient::TestClient rcmTestClient(settings, hostId);
    if (rcmTestClient.TestRegisterSourceAgent(hostName,
        agentVer,
        driverVer,
        installPath))
    {
        ret = rcmTestClient.GetErrorCode();
        cout << "RegisterSourceAgent failed with " << ret << std::endl;
    }
    else
    {
        std::cout << "RegisterSourceAgent succeeded." << std::endl;
        ret = 0;
    }

    return ret;
}

int32_t TestUnregisterSourceAgent(RcmClientSettings& settings, std::string& strJson)
{
    int32_t ret = -1;

    std::string hostId = "d20d3be0-489b-4603-a3bf-92042226e43f";

    TestClient rcmTestClient(settings, hostId);
    if (rcmTestClient.TestUnregisterSourceAgent())
    {
        ret = rcmTestClient.GetErrorCode();
        cout << "UnregisterSourceAgent failed with " << ret << std::endl;

    }
    else
    {
        std::cout << "UnregisterSourceAgent succeeded." << std::endl;
        ret = 0;
    }

    return ret;
}

int32_t TestUnregisterMachine(RcmClientSettings& settings, std::string& strJson)
{
    int32_t ret = -1;

    std::string hostId = "d20d3be0-489b-4603-a3bf-92042226e43f";

    TestClient rcmTestClient(settings, hostId);
    if (rcmTestClient.TestUnregisterMachine())
    {
        ret = rcmTestClient.GetErrorCode();
        cout << "UnregisterMachine failed with " << ret << std::endl;

    }
    else
    {
        std::cout << "UnregisterMachine succeeded." << std::endl;
        ret = 0;
    }

    return ret;
}

int32_t TestPostStats(RcmClientSettings& settings, std::string& strinput)
{
    uint32_t ret = 0;

    ServiceBusQueue sbq(strinput, false);
    std::string hostId = "9ab25244-d00b-4ede-9fed-1b3ef734edd6";

    AgentProtectionPairHealthMsg agentHealthMsg;
    AgentReplicationPairHealthDetails replicationPairHealthMsg;
    replicationPairHealthMsg.SourceDiskId = "{446481152}";
    replicationPairHealthMsg.DataPendingAtSourceAgentInMB = 10 * 1024 * 1024;
    replicationPairHealthMsg.ReplicationPairsContext = "W10=";

    agentHealthMsg.DiskDetails.push_back(replicationPairHealthMsg);

    std::string statsMsg;
    try {
        statsMsg = JSON::producer<AgentProtectionPairHealthMsg>::convert(agentHealthMsg);
    }
    catch (JSON::json_exception &je)
    {
        return 1;
    }

    std::map<std::string, std::string> propertyMap;
    RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
        hostId,
        settings,
        MonitoringMessageType::SOURCE_AGENT_STATS,
        MonitoringMessageSource::SOURCE_AGENT,
        propertyMap);

    std::string message = securitylib::base64Encode(statsMsg.c_str(), statsMsg.length());

    ret = sbq.Put(message, hostId, propertyMap);
    return ret;
}

int32_t TestAlertsToRcm(RcmClientSettings& settings, std::string& strinput)
{
    uint32_t ret = 0;

    std::string hostId = "9ab25244-d00b-4ede-9fed-1b3ef734edd6";
    AppConsistencyFailureAlert alert("abcd", "vacp", 127);

    AgentEventMsg eventMsg;

    eventMsg.EventCode = "EA0431";
    eventMsg.EventSeverity = "Information";
    eventMsg.EventSource = "SourceAgent";

    // get time in 100 ns granularity
    const uint64_t HUNDRED_NANO_SECS_IN_SEC = 10 * 1000 * 1000;
    uint64_t eventTime = GetTimeInSecSinceAd0001();
    eventTime = eventTime * HUNDRED_NANO_SECS_IN_SEC;
    eventMsg.EventTimeUtc = eventTime;

    InmAlert::Parameters_t params = alert.GetParameters();

    if (params.size())
    {
        InmAlert::Parameters_t::iterator paramIter = params.begin();
        for (; paramIter != params.end(); paramIter++)
            eventMsg.MessageParams.insert(std::make_pair(paramIter->first, paramIter->second));
    }

    std::string eventMessage;
    try {
        eventMessage = JSON::producer<AgentEventMsg>::convert(eventMsg);
    }
    catch (JSON::json_exception &je)
    {
        std::string errMsg = "Failed to serialize agent event with error";
        errMsg += je.what();
        return ret;
    }

    std::map<std::string, std::string> propertyMap;
    RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
        hostId,
        settings,
        MonitoringMessageType::PROTECTION_PAIR_HEALTH_EVENT,
        MonitoringMessageSource::SOURCE_AGENT,
        propertyMap);

    ServiceBusQueue sbq(strinput, false);

    bool status = sbq.Put(eventMessage, hostId, propertyMap);

    if (!status)
    {
        std::string errMsg = "Failed to put the alert in service bus queue";
        return ret;
    }


    return ret;
}

int32_t TestDiskAlertsToRcm(RcmClientSettings& settings, std::string& strinput)
{
    uint32_t ret = 0;

    std::string hostId = "9ab25244-d00b-4ede-9fed-1b3ef734edd6";
    std::string diskId = "{446481152}";
    VxToAzureBlobCommunicationFailAlert alert("blobsasuri");

    AgentEventMsg eventMsg;

    eventMsg.EventCode = "EA0432";
    eventMsg.EventSeverity = "Critical";
    eventMsg.EventSource = "SourceAgent";

    // get time in 100 ns granularity
    const uint64_t HUNDRED_NANO_SECS_IN_SEC = 10 * 1000 * 1000;
    uint64_t eventTime = GetTimeInSecSinceEpoch1970();
    eventTime = eventTime * HUNDRED_NANO_SECS_IN_SEC;
    eventMsg.EventTimeUtc = eventTime;

    InmAlert::Parameters_t params = alert.GetParameters();

    if (params.size())
    {
        InmAlert::Parameters_t::iterator paramIter = params.begin();
        for (; paramIter != params.end(); paramIter++)
            eventMsg.MessageParams.insert(std::make_pair(paramIter->first, paramIter->second));
    }

    std::string eventMessage;
    try {
        eventMessage = JSON::producer<AgentEventMsg>::convert(eventMsg);
    }
    catch (JSON::json_exception &je)
    {
        std::string errMsg = "Failed to serialize agent event with error";
        errMsg += je.what();
        return ret;
    }

    std::map<std::string, std::string> propertyMap;
    RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
        hostId,
        diskId,
        settings,
        MonitoringMessageType::REPLICATION_PAIR_HEALTH_EVENT,
        MonitoringMessageSource::SOURCE_AGENT,
        propertyMap);

    std::string encodedEventMsg = securitylib::base64Encode(eventMessage.c_str(), eventMessage.length());

    ServiceBusQueue sbq(strinput, false);

    bool status = sbq.Put(encodedEventMsg, hostId, propertyMap);

    if (!status)
    {
        std::string errMsg = "Failed to put the alert in service bus queue";
        return ret;
    }


    return ret;
}

int32_t GetSourceAgentSettings(RcmClientSettings& settings, std::string & fileName)
{
    int32_t ret = -1;

    std::string hostId = "d20d3be0-489b-4603-a3bf-92042226e43f";
    /// std::string hostId = "d7263f58-676f-4132-a7ee-1f44e6075c30";

    std::vector<RcmJob> agentJobs;

    TestClient rcmTestClient(settings, hostId);
    if (rcmTestClient.GetReplicationSettings(agentJobs, fileName))
    {
        ret = rcmTestClient.GetErrorCode();
        cout << "GetSourceAgentSettings failed with " << ret << std::endl;

    }
    else
    {
        std::cout << "GetSourceAgentSettings succeeded." << std::endl;
        ret = 0;
    }

    return ret;
}

void CreateTestJobs(const std::string& input, const std::string& fileName)
{


    // std::list<std::string> disks = { "{0480177D-CB18-4A88-A782-EF9CA1AD57F3}", "{668693456}", "{971F7BEF-0C3A-4AEF-9A6D-57D59D5C936F}" };
    /// std::list<std::string> disks = { "{668693456}", "{971F7BEF-0C3A-4AEF-9A6D-57D59D5C936F}" };
    // {"{668693456}"};

    CreateJobsInput jobsInput = JSON::consumer<CreateJobsInput>::convert(input);
    ReplicationSettings settings;

    std::cout << "creating resync start jobs .." << std::endl;
    /// add resync start jobs for each disk
    std::vector<std::string>::iterator diskIter = jobsInput.m_disks.begin();
    while (diskIter != jobsInput.m_disks.end())
    {
        RcmJob job;

        job.Id = GenerateUuid();
        job.JobType = "SourceAgentNotifyResyncStart";
        job.JobStatus = "None";

        ResyncStartNotifyInput rsni;
        rsni.SourceDiskId = *diskIter;
        rsni.ResyncSessionId = GenerateUuid();
        rsni.DeleteAccumulatedChanges = true;
        std::string rsinput = JSON::producer<ResyncStartNotifyInput>::convert(rsni);
        job.InputPayload = securitylib::base64Encode(rsinput.c_str(), rsinput.length());
        settings.RcmJobs.push_back(job);

        diskIter++;
    }

    std::string rcmSettings = JSON::producer<ReplicationSettings>::convert(settings);
    WriteToFile(fileName, rcmSettings);

    std::cout << "Waiting after creating resync start jobs .." << std::endl;
    ACE_OS::sleep(120);

    /// clear the resync start jobs
    settings.RcmJobs.clear();
    rcmSettings = JSON::producer<ReplicationSettings>::convert(settings);
    WriteToFile(fileName, rcmSettings);

    std::cout << "Creating drain actions .." << std::endl;

    /// create the drain actions for each disk
    diskIter = jobsInput.m_disks.begin();
    while (diskIter != jobsInput.m_disks.end())
    {
        RcmReplicationPairAction rpaction;
        rpaction.ActionType = RcmActionTypes::DRAIN_LOGS;
        rpaction.DiskId = *diskIter;

        DrainLogSettings ds;
        ds.DataPathTransportType = TRANSPORT_TYPE_AZURE_BLOB;

        AzureBlobBasedTransportSettings abbts;
        abbts.BlobContainerSasUri = jobsInput.m_sasUri;
        // abbts.SasUriExpirationTime = GetTimeInSecSinceAd0001();
        // abbts.SasUriExpirationTime += 30 * 60;
        abbts.SasUriExpirationTime = jobsInput.m_sasUriExpiryTime;
        abbts.SasUriExpirationTime *= 10 * 1000 * 1000;

        std::cout << "SAS URI expiry time " << abbts.SasUriExpirationTime << std::endl;

        std::string bbinput = JSON::producer<AzureBlobBasedTransportSettings>::convert(abbts);
        ds.DataPathTransportSettings = securitylib::base64Encode(bbinput.c_str(), bbinput.length());

        bbinput = JSON::producer<DrainLogSettings>::convert(ds);
        rpaction.InputPayload = securitylib::base64Encode(bbinput.c_str(), bbinput.length());
        settings.RcmReplicationPairActions.push_back(rpaction);

        diskIter++;
    }

    /// create consistency action
    RcmProtectionPairAction action;
    action.ActionType = RcmActionTypes::CONSISTENCY;
    ConsistencySettings cs;
    cs.CoordinatorMachine = jobsInput.m_hostname;
    cs.ParticipatingMachines.push_back(jobsInput.m_hostname);
    //cs.AppConsistencyInterval = 60 * 60;
    cs.AppConsistencyInterval = jobsInput.m_appConInterval;
    cs.AppConsistencyInterval *= 10 * 1000 * 1000;
    // cs.CrashConsistencyInterval = 5 * 60;
    cs.CrashConsistencyInterval = jobsInput.m_crashConInterval;
    cs.CrashConsistencyInterval *= 10 * 1000 * 1000;

    std::string coninput = JSON::producer<ConsistencySettings>::convert(cs);
    action.InputPayload = securitylib::base64Encode(coninput.c_str(), coninput.length());
    settings.RcmProtectionPairActions.push_back(action);

    rcmSettings = JSON::producer<ReplicationSettings>::convert(settings);
    WriteToFile(fileName, rcmSettings);

    std::cout << "Waiting after creating drain actions .." << std::endl;
    ACE_OS::sleep(120);

    std::cout << "Waiting for IR time .." << std::endl;
    ACE_OS::sleep(jobsInput.m_irTime);

    std::cout << "creating resync end job .." << std::endl;
    /// add resync start jobs for each disk
    diskIter = jobsInput.m_disks.begin();
    while (diskIter != jobsInput.m_disks.end())
    {
        RcmJob job;

        job.Id = GenerateUuid();
        job.JobType = "SourceAgentNotifyResyncEnd";
        job.JobStatus = "None";

        ResyncEndNotifyInput rsni;
        rsni.SourceDiskId = *diskIter;
        rsni.ResyncSessionId = GenerateUuid();
        std::string rsinput = JSON::producer<ResyncEndNotifyInput>::convert(rsni);
        job.InputPayload = securitylib::base64Encode(rsinput.c_str(), rsinput.length());
        settings.RcmJobs.push_back(job);

        diskIter++;
    }

    rcmSettings = JSON::producer<ReplicationSettings>::convert(settings);
    WriteToFile(fileName, rcmSettings);

    std::cout << "Waiting after creating resync end jobs .." << std::endl;
    ACE_OS::sleep(120);

    /// clear the resync end jobs
    settings.RcmJobs.clear();
    rcmSettings = JSON::producer<ReplicationSettings>::convert(settings);
    WriteToFile(fileName, rcmSettings);

    std::cout << "creating baseline bookmark jobs .." << std::endl;
    /// add resync start jobs for each disk
    diskIter = jobsInput.m_disks.begin();
    while (diskIter != jobsInput.m_disks.end())
    {
        RcmJob job;

        job.Id = GenerateUuid();
        job.JobType = "SourceAgentIssueBaselineBookmark";
        job.JobStatus = "None";

        IssueBaselineBookmarkInput rsni;
        rsni.SourceDiskId = *diskIter;
        rsni.BaselineBookmarkId = GenerateUuid();
        std::string rsinput = JSON::producer<IssueBaselineBookmarkInput>::convert(rsni);
        job.InputPayload = securitylib::base64Encode(rsinput.c_str(), rsinput.length());
        settings.RcmJobs.push_back(job);

        diskIter++;
    }

    rcmSettings = JSON::producer<ReplicationSettings>::convert(settings);
    WriteToFile(fileName, rcmSettings);

    std::cout << "Waiting after baseline bookmark jobs .." << std::endl;

    ACE_OS::sleep(120);

    /// clear the base line jobs
    settings.RcmJobs.clear();
    rcmSettings = JSON::producer<ReplicationSettings>::convert(settings);
    WriteToFile(fileName, rcmSettings);


    std::cout << "Waiting for DR time .." << std::endl;
    ACE_OS::sleep(jobsInput.m_drTime);

    /// stop draining
    settings.RcmProtectionPairActions.clear();
    settings.RcmReplicationPairActions.clear();
    rcmSettings = JSON::producer<ReplicationSettings>::convert(settings);
    WriteToFile(fileName, rcmSettings);

    std::cout << "Waiting after deleting drain & consistency actions .." << std::endl;
    ACE_OS::sleep(120);

    /// stop filtering
    diskIter = jobsInput.m_disks.begin();
    while (diskIter != jobsInput.m_disks.end())
    {
        RcmJob job;

        job.Id = GenerateUuid();
        job.JobType = "SourceAgentStopFilter";
        job.JobStatus = "None";

        StopFilteringInput rsni;
        rsni.SourceDiskId = *diskIter;
        std::string stpinput = JSON::producer<StopFilteringInput>::convert(rsni);
        job.InputPayload = securitylib::base64Encode(stpinput.c_str(), stpinput.length());
        settings.RcmJobs.push_back(job);

        diskIter++;
    }

    rcmSettings = JSON::producer<ReplicationSettings>::convert(settings);
    WriteToFile(fileName, rcmSettings);
    std::cout << "Waiting after stop filtering job .." << std::endl;
    ACE_OS::sleep(120);

    /// clear the resync end jobs
    settings.RcmJobs.clear();
    rcmSettings = JSON::producer<ReplicationSettings>::convert(settings);
    WriteToFile(fileName, rcmSettings);

    std::cout << "cleared stop filtering job" << std::endl;

    return;
}

int GetBearerToken(RcmClientSettings& settings)
{
    std::string token;
    std::string hostId = "d20d3be0-489b-4603-a3bf-92042226e43f";

    TestClient rcmTestClient(settings, hostId);
    std::string address = "http://10.150.2.209";
    std::string port = "555";
    std::string user; // = "1";
    std::string password; // = "1";
    rcmTestClient.TestGetBearerToken(token);
    std::cout << "AAD Bearer Token: " << token << std::endl;
    /*rcmTestClient.TestGetBearerToken(token);
    std::cout << "AAD Bearer Token: " << token << std::endl;
    */
    return 0;
}

static void LogCallback(unsigned int logLevel, const char *msg)
{
    string format;
    if (logLevel == SV_LOG_ERROR)
        format = "FAILED ";
    else if (logLevel == SV_LOG_WARNING)
        format = "WARNING ";

    format += "%s";
    DebugPrintf(format.c_str(), msg);
    return;
}

void usage(char * cmd)
{
    stringstream out, err;
    out << "Usage : " << cmd << " <optionsfile> \n"
        << "where optionsfile is JSON format file with specific options.\n"
        << "Options can be :\n"
        << "registermachine           - To register machine with RCM \n"
        << "unregistermachine         - To unregister machine with RCM \n"
        << "registersourceagent       - To register source Agent with RCM \n"
        << "unregistersourceagent     - To unregister source Agent with RCM \n"
        << "getsourceagentsettings    - To fetch source agent settings from RCM \n"
        << "createtestjobs            - To create an RCM Jobs file \n"
        << "getbearertoken            - To get AAD token \n"
        << "testagentlogworkflows     - To test various agent RCM - MDS logging workflows \n"
        << "genSimTestAgSettings     - To generate a simulated test machine and fetch source agent setings \n";

    std::cout << out.str().c_str() << std::endl;
}

typedef void(*AgentLogTestMethod)(SimulatedTestMachine&, const AgentSettings&, std::string &output);

void RunTestAgentLoggingWorkflow(AgentLogTestMethod testMethod, SimulatedTestMachine &testMachine, std::string &output)
{
    testMachine.RegisterMachine();
    testMachine.RegisterSourceAgent();

    AgentSettings agentSettings;
    testMachine.GetAgentReplicationSettings(agentSettings);

    testMethod(testMachine, agentSettings, output);

    testMachine.UnregisterSourceAgent();
    testMachine.UnregisterMachine();
}

void TestServiceBusMessageLimit(SimulatedTestMachine &testMachine, const AgentSettings &agentSettings, std::string &output)
{
    const std::string::size_type maxStringSize = StandardSBMessageInBase64MaxSize;

    std::string singleMessageMetadata = "[{Message:\"\"}]";
    std::string singleFullMessage = "[{Message:\"" + std::string(maxStringSize - singleMessageMetadata.size(), 'a') + "\"}]";

    BOOST_VERIFY(SVS_OK == testMachine.PostAgentLogsToRcm(agentSettings,
        MonitoringMessageType::AGENT_LOG_ADMIN_EVENT,
        MonitoringMessageSource::SOURCE_AGENT,
        singleFullMessage));

    std::string justOverflowingMessage = singleFullMessage + "   "; // Ensuring one more char in base 64 is used.

    BOOST_VERIFY(SVS_OK != testMachine.PostAgentLogsToRcm(agentSettings,
        MonitoringMessageType::AGENT_LOG_ADMIN_EVENT,
        MonitoringMessageSource::SOURCE_AGENT,
        justOverflowingMessage));
}

void TestBuildJsonArray(SimulatedTestMachine &testMachine, const AgentSettings &agentSettings, std::string &output)
{
    const std::string::size_type maxStringSize = StandardSBMessageInBase64MaxSize;

    std::string singleMessageMetadata = "{Message:\"\"}";
    // A single element array that would occupy entire message.
    std::string singleFullMessge = "{Message:\"" + std::string(maxStringSize - singleMessageMetadata.size() - 2, 'b') + "\"}";
    std::string resultMessageArray;

    BOOST_VERIFY(HttpRestUtil::BuildJsonArray(resultMessageArray, singleFullMessge, maxStringSize, false));

    BOOST_VERIFY(SVS_OK == testMachine.PostAgentLogsToRcm(agentSettings,
        MonitoringMessageType::AGENT_LOG_ANALYTIC_EVENT,
        MonitoringMessageSource::GATEWAY_SERVICE,
        resultMessageArray));


    // A single element array that would occupy entire message, without any space for square brackets. For a single element,
    // we will succeed the call anyway, otherwise we would end up in infinite loop.
    std::string singleFullMessgeExceeding = "{Message:\"" + std::string(maxStringSize - singleMessageMetadata.size(), 'c') + "\"}";
    resultMessageArray.clear();

    BOOST_VERIFY(HttpRestUtil::BuildJsonArray(resultMessageArray, singleFullMessgeExceeding, maxStringSize, false));

    // The service bus call would fail anyway, as the size has exceeded the limit.
    BOOST_VERIFY(SVS_OK != testMachine.PostAgentLogsToRcm(agentSettings,
        MonitoringMessageType::AGENT_LOG_ANALYTIC_EVENT,
        MonitoringMessageSource::GATEWAY_SERVICE,
        resultMessageArray));

    // Two element array such that the serialized array is exactly the max size.
    std::string doubleExactFittingMessage1 = "{Message:\"" + std::string(maxStringSize / 2 - singleMessageMetadata.size() - 2, 'd') + "\"}";
    std::string doubleExactFittingMessage2 = "{Message:\"" + std::string(maxStringSize / 2 - singleMessageMetadata.size() - 1, 'e') + "\"}";
    resultMessageArray.clear();

    BOOST_VERIFY(HttpRestUtil::BuildJsonArray(resultMessageArray, doubleExactFittingMessage1, maxStringSize, true));
    bool randBool = SimulatedTestMachine::GenerateRandomNumber32() > INT32_MAX;
    // If true, we are informing the method that the second one is the last element and so handled as positive case.
    BOOST_VERIFY(HttpRestUtil::BuildJsonArray(resultMessageArray, doubleExactFittingMessage2, maxStringSize, randBool));
    // If false, the method would carefully append the second one, ensuring that there's space for adding ] in the end,
    // which in this case would be 1 pending byte.
    if (randBool)
    {
        // Any of the flag value wouldn't affect the result.
        randBool = SimulatedTestMachine::GenerateRandomNumber32() > INT32_MAX;
        // The method would say that it couldn't fill the next element into array but will close the array with ].
        BOOST_VERIFY(!HttpRestUtil::BuildJsonArray(resultMessageArray, std::string(), maxStringSize, randBool));
    };

    BOOST_VERIFY(SVS_OK == testMachine.PostAgentLogsToRcm(agentSettings,
        MonitoringMessageType::AGENT_LOG_DEBUG_EVENT,
        MonitoringMessageSource::PROTECTION_SERVICE,
        resultMessageArray));


    // Overflowing message array, where only 2 elements could be filled into maxSize.
    std::string message1Ov = "{Message:\"" + std::string(maxStringSize / 2 - singleMessageMetadata.size() - 1000, 'f') + "\"}";
    std::string message2Ov = "{Message:\"" + std::string(maxStringSize / 2 - singleMessageMetadata.size() - 1000, 'g') + "\"}";
    std::string message3Ov = "{Message:\"" + std::string(maxStringSize / 2 - singleMessageMetadata.size() - 1000, 'h') + "\"}";
    resultMessageArray.clear();


    BOOST_VERIFY(HttpRestUtil::BuildJsonArray(resultMessageArray, message1Ov, maxStringSize, true));
    BOOST_VERIFY(HttpRestUtil::BuildJsonArray(resultMessageArray, message2Ov, maxStringSize, true));
    // On appending the third element, the method detects the overflow and reverts that the element is
    // not added to the array.
    // (Randomizing the more data = true/false, since both of them shouldn't affect the result.
    randBool = SimulatedTestMachine::GenerateRandomNumber32() > INT32_MAX;
    BOOST_VERIFY(!HttpRestUtil::BuildJsonArray(resultMessageArray, message3Ov, maxStringSize, randBool));

    BOOST_VERIFY(SVS_OK == testMachine.PostAgentLogsToRcm(agentSettings,
        MonitoringMessageType::AGENT_LOG_DEBUG_EVENT,
        MonitoringMessageSource::PROTECTION_SERVICE,
        resultMessageArray));
}

void TestPositiveRandomLogging(SimulatedTestMachine &testMachine, const AgentSettings &agentSettings, std::string &output)
{
    const std::string::size_type maxStringSize = StandardSBMessageInBase64MaxSize;

    AgentLogExtendedProperties extProps;
    AgentLogAdminEventInput adminEvent;
    adminEvent.ComponentId = RcmComponentId::SOURCE_AGENT;
    adminEvent.DiskId = GenerateUuid();
    adminEvent.AgentLogLevel = "Information";
    adminEvent.AgentPid = boost::lexical_cast<std::string>(SimulatedTestMachine::GenerateRandomNumber32(1000, 2000));
    adminEvent.AgentTid = boost::lexical_cast<std::string>(SimulatedTestMachine::GenerateRandomNumber32(3000, 4000));
    adminEvent.AgentTimeStamp = boost::posix_time::to_iso_extended_string(boost::posix_time::second_clock::universal_time());
    adminEvent.DiskNumber = boost::lexical_cast<std::string>(SimulatedTestMachine::GenerateRandomNumber32(8000000, 9000000));
    adminEvent.ErrorCode = boost::lexical_cast<std::string>(0);
    extProps.ExtendedProperties.insert(std::make_pair("prop1", "54"));
    extProps.ExtendedProperties.insert(std::make_pair("x", "xxxxx"));
    extProps.ExtendedProperties.insert(std::make_pair("y", "yyyyy"));
    adminEvent.ExtendedProperties = JSON::producer<AgentLogExtendedProperties>::convert(extProps);
    adminEvent.MachineId = testMachine.GetHostId();
    adminEvent.MemberName = FUNCTION_NAME;
    adminEvent.Message = "Fully composed admin message!";
    adminEvent.SourceEventId = boost::lexical_cast<std::string>(SimulatedTestMachine::GenerateRandomNumber32(50000, 50500));
    adminEvent.SourceFilePath = __FILE__;
    adminEvent.SourceLineNumber = __LINE__;
    adminEvent.SubComponent = "AzureRcmTest";

    std::string serializedAdminEvent = JSON::producer<AgentLogAdminEventInput>::convert(adminEvent);
    std::string resultMessageArray;
    BOOST_VERIFY(HttpRestUtil::BuildJsonArray(resultMessageArray, serializedAdminEvent, maxStringSize, false));

    BOOST_VERIFY(SVS_OK == testMachine.PostAgentLogsToRcm(agentSettings,
        MonitoringMessageType::AGENT_LOG_ADMIN_EVENT,
        MonitoringMessageSource::SOURCE_AGENT,
        resultMessageArray));

    AgentLogDebugEventInput debugEvent;
    debugEvent.ComponentId = RcmComponentId::SOURCE_AGENT;
    debugEvent.DiskId = GenerateUuid();
    debugEvent.AgentLogLevel = "Debug";
    debugEvent.AgentPid = boost::lexical_cast<std::string>(SimulatedTestMachine::GenerateRandomNumber32(1000, 2000));
    debugEvent.AgentTid = boost::lexical_cast<std::string>(SimulatedTestMachine::GenerateRandomNumber32(3000, 4000));
    debugEvent.AgentTimeStamp = boost::posix_time::to_iso_extended_string(boost::posix_time::second_clock::universal_time());
    debugEvent.DiskNumber = boost::lexical_cast<std::string>(SimulatedTestMachine::GenerateRandomNumber32(8000000, 9000000));
    debugEvent.ErrorCode = boost::lexical_cast<std::string>(100);
    extProps.ExtendedProperties.clear();
    extProps.ExtendedProperties.insert(std::make_pair("prop6", "235423"));
    extProps.ExtendedProperties.insert(std::make_pair("t", "ta"));
    extProps.ExtendedProperties.insert(std::make_pair("r", "rr"));
    debugEvent.ExtendedProperties = JSON::producer<AgentLogExtendedProperties>::convert(extProps);
    debugEvent.MachineId = testMachine.GetHostId();
    debugEvent.MemberName = FUNCTION_NAME;
    debugEvent.Message =
#ifdef SV_WINDOWS
        // Event log needs UTF-16 wstring to string conversion.
        convertWstringToUtf8(L"Fully composed debug message! - வணக்கம்.");
#else
        "Fully composed debug message!";
#endif
    // SourceEventId : Debug, Analytic differs from Admin, Operational by SourceEventId currently.
    debugEvent.SourceFilePath = __FILE__;
    debugEvent.SourceLineNumber = __LINE__;
    debugEvent.SubComponent = "AzureRcmTest";

    std::string serializedDebugEvent = JSON::producer<AgentLogDebugEventInput>::convert(debugEvent);
    resultMessageArray.clear();
    BOOST_VERIFY(HttpRestUtil::BuildJsonArray(resultMessageArray, serializedDebugEvent, maxStringSize, false));

    BOOST_VERIFY(SVS_OK == testMachine.PostAgentLogsToRcm(agentSettings,
        MonitoringMessageType::AGENT_LOG_DEBUG_EVENT,
        MonitoringMessageSource::SOURCE_AGENT,
        resultMessageArray));
}

int main(int argc, char *argv[])
{
    int ret = 0;
    std::cout << std::endl << "Command Line: ";

    for (int i = 0; i < argc; i++)
        std::cout << argv[i] << " ";

    std::cout << "\n" << std::endl;

    if (argc != 2 || argv[1] == "-h")
    {
        usage(argv[0]);
        return -1;
    }

    std::string optionfile = argv[1];
    if (!boost::filesystem::exists(optionfile))
    {
        std::cout << "options file not found." << std::endl;
        return -1;
    }

    std::string strOptions;
    if (!ReadFromFile(optionfile, strOptions))
    {
        std::cout << "Error reading the input options file " << optionfile << "." << std::endl;
        return -1;

    }

    boost::filesystem::path full_path(boost::filesystem::current_path());
    std::string LogFile = full_path.string() +
        ACE_DIRECTORY_SEPARATOR_STR_A +
        "AzureRcmTestRun.log";

    SetDaemonMode();
    SetLogFileName(LogFile.c_str());
    SetLogLevel(SV_LOG_ALWAYS);

    Trace::Init(LogFile,
        LogLevelAlways,
        boost::bind(&LogCallback, _1, _2));

    CmdOptions cmdOpts;

    try {

        cmdOpts = JSON::consumer<CmdOptions>::convert(strOptions);

        if (!boost::filesystem::exists(cmdOpts.m_settingsFilepath))
        {
            std::cout << "Error reading the settings file " << cmdOpts.m_settingsFilepath << "." << std::endl;
            return 1;
        }

        RcmClientSettings settings(cmdOpts.m_settingsFilepath);

        HttpClient::GlobalInitialize();
        boost::shared_ptr<void> guardRcm(static_cast<void*>(0), boost::bind(HttpClient::GlobalUnInitialize));

        if (cmdOpts.m_option == "registermachine")
        {
            ret = TestRegisterMachine(settings, cmdOpts.m_input);
        }
#ifdef _DEBUG
        else if (cmdOpts.m_option == "testconnection")
        {
            ret = TestConnection(settings);
        }
#endif
        else if (cmdOpts.m_option == "registersourceagent")
        {
            ret = TestRegisterSourceAgent(settings, cmdOpts.m_input);
        }
        else if (cmdOpts.m_option == "createtestjobs")
        {
            CreateTestJobs(cmdOpts.m_input, cmdOpts.m_outputFilepath);
        }
        else if (cmdOpts.m_option == "getsourceagentsettings")
        {
            ret = GetSourceAgentSettings(settings, cmdOpts.m_outputFilepath);
        }
        else if (cmdOpts.m_option == "getbearertoken")
        {
            ret = GetBearerToken(settings);
        }
        else if (cmdOpts.m_option == "testmt")
        {
            boost::thread_group thread_group;
            boost::thread *thread = new boost::thread(boost::bind(&GetSourceAgentSettings, settings, cmdOpts.m_outputFilepath));
            thread_group.add_thread(thread);
            thread = new boost::thread(boost::bind(&TestRegisterMachine, settings, cmdOpts.m_input));
            thread_group.add_thread(thread);
            thread_group.join_all();

        }
        else if (cmdOpts.m_option == "unregistersourceagent")
        {
            ret = TestUnregisterSourceAgent(settings, cmdOpts.m_input);
        }
        else if (cmdOpts.m_option == "unregistermachine")
        {
            ret = TestUnregisterMachine(settings, cmdOpts.m_input);
        }
        else if (cmdOpts.m_option == "poststats")
        {
            ret = TestPostStats(settings, cmdOpts.m_input);
        }
        else if (cmdOpts.m_option == "sendalert")
        {
            ret = TestAlertsToRcm(settings, cmdOpts.m_input);
        }
        else if (cmdOpts.m_option == "testagentlogworkflows")
        {
            AgentLogTestMethod scenarios[] = { TestServiceBusMessageLimit, TestBuildJsonArray, TestPositiveRandomLogging };
            const int scenarioCnt = sizeof(scenarios) / sizeof(scenarios[0]);
            std::string outputs[scenarioCnt];

            SimulatedTestMachine::SimulatedTestMachinesMap machinesMap;
            SimulatedTestMachine::CreateSimulatedTestMachines(scenarioCnt, settings, machinesMap);

            boost::thread_group thread_group;

            SimulatedTestMachine::SimulatedTestMachinesMap::const_iterator mapIter = machinesMap.begin();

            for (int currCnt = 0; currCnt < scenarioCnt; currCnt++, mapIter++)
            {
                boost::thread *thread = new boost::thread(boost::bind(&RunTestAgentLoggingWorkflow, scenarios[currCnt], mapIter->second, outputs[currCnt]));
                thread_group.add_thread(thread);
            }

            BOOST_VERIFY(mapIter == machinesMap.end());
            thread_group.join_all();
        }
        else if (cmdOpts.m_option == "genSimTestAgSettings")
        {
            SimulatedTestMachine::SimulatedTestMachinesMap machinesMap;
            SimulatedTestMachine::CreateSimulatedTestMachines(1, settings, machinesMap);
            const std::string &newlyGenTestMachineId = machinesMap.begin()->first;
            SimulatedTestMachine &newlyGenTestMachine = machinesMap.begin()->second;

            newlyGenTestMachine.RegisterMachine();
            newlyGenTestMachine.RegisterSourceAgent();

            AgentSettings agentSettings;
            newlyGenTestMachine.GetAgentReplicationSettings(agentSettings);

            std::string serializedAgentSettings = JSON::producer<AgentSettings>::convert(agentSettings);

            if (cmdOpts.m_outputFilepath.empty())
            {
                std::cout << serializedAgentSettings << std::endl;
            }
            else
            {
                std::ofstream ofs(cmdOpts.m_outputFilepath.c_str());
                ofs << serializedAgentSettings;
            }
        }
        else
        {
            std::cerr << "Error : unknown option" << std::endl;
        }
    }
    catch (JSON::json_exception jsonex)
    {
        std::cout << "Error: parsing JSON string: " << jsonex.what();
    }
    catch (const char *msg)
    {
        std::cout << "Error: " << cmdOpts.m_option << " failed : " << msg << std::endl;
    }
    catch (std::string &msg)
    {
        std::cout << "Error: " << cmdOpts.m_option << " failed : " << msg << std::endl;
    }
    catch (std::exception &e)
    {
        std::cout << "Error: " << cmdOpts.m_option << " failed : " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Error: " << cmdOpts.m_option << " failed : unknown exception " << std::endl;
    }

    if (ret)
        std::cerr << "Error: " << cmdOpts.m_option << " failed with error code: " << ret << std::endl;

    return ret;
}

