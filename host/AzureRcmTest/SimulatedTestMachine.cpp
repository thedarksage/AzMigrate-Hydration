#include <boost/algorithm/string/case_conv.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random/uniform_int.hpp>
#include <iostream>
#include <fstream>

#include "AgentRcmLogging.h"
#include "Monitoring.h"
#include "MonitoringMsgSettings.h"
#include "portablehelpers.h"
#include "securityutils.h"
#include "SimulatedTestMachine.h"

using namespace RcmClientLib;
namespace pt = boost::posix_time;

#define LOG(content) cout << "[" + pt::to_simple_string(pt::second_clock::local_time()) + "] " + FUNCTION_NAME + " - " + m_hostName + " : " + content + "\n"

const SV_LONGLONG GB = 1024 * 1024 * 1024;

#pragma region Utils

static std::string GenerateBiosId(const std::string *pUuid = NULL)
{
    std::string biosId;
    std::string uuid = (NULL == pUuid) ? GenerateUuid() : *pUuid;

    // 995f423a-29f5-4ea3-9c4e-8a3c60050fd5
    // VMware-99 5f 42 3a 29 f5 4e a3-9c 4e 8a 3c 60 05 0f d5
    int uuidByteIndPart1[] = { 0, 2, 4, 6, 9, 11, 14, 16 };
    int uuidByteIndPart2[] = { 19, 21, 24, 26, 28, 30, 32, 34 };

    biosId = "VMware-";
    for (int currInd = 0; currInd < ARRAYSIZE(uuidByteIndPart1); currInd++)
        biosId += uuid.substr(uuidByteIndPart1[currInd], 2) + ' ';
    biosId.erase(biosId.end() - 1);
    biosId += '-';
    for (int currInd = 0; currInd < ARRAYSIZE(uuidByteIndPart2); currInd++)
        biosId += uuid.substr(uuidByteIndPart2[currInd], 2) + ' ';
    biosId.erase(biosId.end() - 1);

    return biosId;
}

#pragma endregion


#pragma region Static members

boost::mt19937 SimulatedTestMachine::randNumGen_32((uint32_t)time(NULL));

uint32_t SimulatedTestMachine::GenerateRandomNumber32(uint32_t min, uint32_t max)
{
    boost::uniform_int<uint32_t> uniformDistMinMax32(min, max);
    boost::variate_generator<boost::mt19937&, boost::uniform_int<uint32_t> >
        nextRandom32(SimulatedTestMachine::randNumGen_32, uniformDistMinMax32);

    return nextRandom32();
}

void SimulatedTestMachine::CreateSimulatedTestMachines(size_t count, const RcmClientSettings &settings, SimulatedTestMachinesMap &machinesMap)
{
    RcmClientSettings commonSettings(settings);

    // Relay is not used in communication of these machines.
    commonSettings.m_RelayKeyPolicyName = "";
    commonSettings.m_RelayServicePathSuffix = "";
    commonSettings.m_RelaySharedKey = "";
    commonSettings.m_ExpiryTimeoutInSeconds = 0;

    // Bios ID will be generated for each machine.
    commonSettings.m_BiosId = "";

    // Generating a common mgmt id for this set of machines.
    commonSettings.m_ManagementId = GenerateUuid();
    // Enforcing direction connection mode.
    commonSettings.m_ServiceConnectionMode = "direct";

    for (size_t i = 0; i < count; i++)
    {
        std::string hostId = GenerateUuid();

        SimulatedTestMachine currMachine(commonSettings, hostId);
        currMachine.m_clientSettings.m_BiosId = GenerateBiosId(&currMachine.m_hostId);

        // Create boot disk
        currMachine.CreateDisk();

        uint32_t numOfDataDisks = GenerateRandomNumber32(0, 3);
        for (size_t i = 0; i < numOfDataDisks; i++)
            currMachine.CreateDisk();

        machinesMap.insert(std::make_pair(hostId, currMachine));
    }
}

#pragma endregion

SimulatedTestMachine::SimulatedTestMachine(RcmClientSettings clientSettings, std::string hostId)
    : m_clientSettings(clientSettings),
    m_hostId(hostId),
    m_hostName(boost::algorithm::to_upper_copy(hostId.substr(0, 8))),
    m_rcmTestClient(clientSettings, hostId),
    m_nextAvailDiskNum(0),
    m_nextAvailMountPt("C:\\")
{
    // Log detailed members only one. In all other places, host name would be used as identifier for a machine.
    LOG("Host ID = " + m_hostId);
}

void SimulatedTestMachine::CreateLogicalVolume(const std::string& name,
    const std::string& mountPoint,
    const std::string& volumeLabel,
    const std::string& volumeGroup,
    SV_LONGLONG rawCapacity,
    VolumeSummary::FormatLabel formatLabel)
{
    SV_LONGLONG capacity = rawCapacity - 1 * GB;
    VolumeSummary volSum;

    volSum.id = "";
    volSum.name = name;
    volSum.type = VolumeSummary::LOGICAL_VOLUME;
    volSum.vendor = VolumeSummary::NATIVE;

    volSum.fileSystem = "NTFS";
    volSum.mountPoint = mountPoint;
    volSum.isMounted = true;
    volSum.isSystemVolume = mountPoint == "C:\\";
    volSum.isCacheVolume = volSum.isSystemVolume;

    volSum.capacity = capacity;
    volSum.locked = false;
    volSum.physicalOffset = 0;

    volSum.sectorSize = 4096;
    volSum.volumeLabel = volumeLabel;
    volSum.containPagefile = volSum.isSystemVolume;
    volSum.isStartingAtBlockZero = false;

    volSum.volumeGroup = volumeGroup;
    volSum.volumeGroupVendor = VolumeSummary::NATIVE;
    volSum.isMultipath = false;

    volSum.rawcapacity = rawCapacity;
    volSum.writeCacheState = VolumeSummary::WRITE_CACHE_DONTKNOW;
    volSum.formatLabel = formatLabel;

    m_diskAndVolSummaries.push_back(volSum);
}

void SimulatedTestMachine::CreateDisk()
{
    SV_LONGLONG rawCapacity = GenerateRandomNumber32(50, 500) * GB;
    SV_LONGLONG capacity = rawCapacity - 1 * GB;
    VolumeSummary diskSum;

    std::string diskUuid = GenerateUuid();
    bool isBootDisk = m_nextAvailMountPt == "C:\\";
    bool isMbr = isBootDisk;
    bool isBasic = isBootDisk;

    // Guid with hyphens removed.
    diskSum.id = diskUuid.substr(0, 8) + diskUuid.substr(9, 4) + diskUuid.substr(14, 4) + diskUuid.substr(19, 4) + diskUuid.substr(24, 12);
    diskSum.name = "{" + (isMbr ? boost::lexical_cast<std::string>(GenerateRandomNumber32(100000000, 900000000)) : GenerateUuid()) + "}";
    diskSum.type = VolumeSummary::DISK;
    diskSum.vendor = VolumeSummary::NATIVE;

    diskSum.fileSystem = "";
    diskSum.mountPoint = "";
    diskSum.isMounted = true;
    diskSum.isSystemVolume = false;
    diskSum.isCacheVolume = false;

    diskSum.capacity = capacity;
    diskSum.locked = false;
    diskSum.physicalOffset = 0;

    diskSum.sectorSize = 4096;
    diskSum.volumeLabel = "";
    diskSum.containPagefile = isBootDisk;
    diskSum.isStartingAtBlockZero = false;

    diskSum.volumeGroup = isBasic ? diskSum.name : "__INMAGE__DYNAMIC__DG__";
    diskSum.volumeGroupVendor = VolumeSummary::NATIVE;
    diskSum.isMultipath = false;

    diskSum.rawcapacity = rawCapacity;
    diskSum.writeCacheState = VolumeSummary::WRITE_CACHE_DONTKNOW;
    diskSum.formatLabel = isMbr ? VolumeSummary::MBR : VolumeSummary::GPT;

    diskSum.attributes.insert(std::make_pair(NsVolumeAttributes::INTERFACE_TYPE, "\\\\.\\PHYSICALDRIVE" + m_nextAvailDiskNum));
    diskSum.attributes.insert(std::make_pair(NsVolumeAttributes::HAS_BOOTABLE_PARTITION, isBootDisk ? "true" : "false"));
    diskSum.attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_BUS, "0"));
    diskSum.attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_LOGICAL_UNIT, "0"));
    diskSum.attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_PORT, "2"));
    diskSum.attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_TARGET_ID, boost::lexical_cast<std::string>(m_nextAvailDiskNum)));
    diskSum.attributes.insert(std::make_pair(NsVolumeAttributes::STORAGE_TYPE, isBasic ? NsVolumeAttributes::BASIC : NsVolumeAttributes::DYNAMIC));
    diskSum.attributes.insert(std::make_pair(NsVolumeAttributes::NAME_BASED_ON, NsVolumeAttributes::SIGNATURE));

    m_diskAndVolSummaries.push_back(diskSum);

    if (isBootDisk)
    {
        assert(m_nextAvailDiskNum == 0);
        assert(m_nextAvailMountPt == "C:\\");
        CreateLogicalVolume(m_nextAvailMountPt, m_nextAvailMountPt, "System Drive", diskSum.volumeGroup, diskSum.capacity - 5 * GB, diskSum.formatLabel);
        // TODO-SanKumar-1612: UpdateParentDiskAttributes() is trying to see if the installation path lies in the particular volume, if it isn't system or cached. This is virtual setup and we should
        // somehow find a way to avoid that.
        //CreateLogicalVolume(m_nextAvailMountPt + "NewMp\\", m_nextAvailMountPt + "NewMp\\", "System Reserved", diskSum.volumeGroup, 5 * GB, diskSum.formatLabel);
    }
    else
    {
        // TODO-SanKumar-1612: Same issue as mentioned above.
        // CreateLogicalVolume(m_nextAvailMountPt, m_nextAvailMountPt, "Data Volume : " + m_nextAvailMountPt[0], diskSum.volumeGroup, diskSum.capacity, diskSum.formatLabel);
    }

    m_nextAvailMountPt[0]++;
    m_nextAvailDiskNum++;
}

int32_t SimulatedTestMachine::RegisterMachine()
{
    int32_t ret = 0;

    HypervisorInfo_t hypervinfo;
    hypervinfo.state = HypervisorInfo::HyperVisorPresent;
    hypervinfo.name = "VMware";
    hypervinfo.version = "6.0";

    CpuInfos_t cpuInfo;
    Object osObject;
    osObject.m_Attributes.insert(std::make_pair(NSOsInfo::NAME, "Windows NT 6.2Build 9200"));
    osObject.m_Attributes.insert(std::make_pair(NSOsInfo::CAPTION, "Microsoft Windows Server 2012 R2 Standard"));
    osObject.m_Attributes.insert(std::make_pair(NSOsInfo::SYSTEMTYPE, "x64-based PC"));
    osObject.m_Attributes.insert(std::make_pair(NSOsInfo::MAJOR_VERSION, "6"));
    osObject.m_Attributes.insert(std::make_pair(NSOsInfo::MINOR_VERSION, "2"));
    osObject.m_Attributes.insert(std::make_pair(NSOsInfo::BUILD, "9200"));

    ret = m_rcmTestClient.TestRegisterMachine(m_hostName,
        m_clientSettings.m_BiosId,
        m_clientSettings.m_ManagementId,
        osObject,
        SV_WIN_OS,
        ENDIANNESS_LITTLE,
        cpuInfo,
        hypervinfo,
        m_diskAndVolSummaries,
        false
        );

    if (ret != 0)
    {
        ret = m_rcmTestClient.GetErrorCode();
        LOG("RegisterMachine failed with " + boost::lexical_cast<std::string>(ret));
    }
    else
    {
        LOG("RegisterMachine succeeded");
    }

    return ret;
}

int32_t SimulatedTestMachine::RegisterSourceAgent()
{
    int32_t ret = 0;

    std::string agentVersion = "9.0.1.0 GA";
    std::string driverVersion = "1.5";
    std::string installPath = "C:\\Program Files\\Microsoft Azure Site Recovery\\agent";

    ret = m_rcmTestClient.TestRegisterSourceAgent(m_hostName, agentVersion, driverVersion, installPath);

    if (ret != 0)
    {
        ret = m_rcmTestClient.GetErrorCode();
        LOG("RegisterSourceAgent failed with " + boost::lexical_cast<std::string>(ret));
    }
    else
    {
        LOG("RegisterSourceAgent succeeded");
    }

    return ret;
}

// TODO-SanKumar-1612: Optionally get a flag in the parameter to query this value at regular intervals
// and cache locally in the object and a backing file.
int32_t SimulatedTestMachine::GetAgentReplicationSettings(AgentSettings &agentSettings)
{
    int32_t ret = 0;

    std::vector<RcmJob> jobs;
    std::string settingsTempPath = (boost::filesystem::current_path() / "AgentSettingsTemp_").string() +
        boost::lexical_cast<std::string>(GenerateRandomNumber32()) + ".conf";

    ret = m_rcmTestClient.GetReplicationSettings(jobs, settingsTempPath);

    if (ret != 0)
    {
        ret = m_rcmTestClient.GetErrorCode();
        LOG("GetAgentReplicationSettings failed with " + boost::lexical_cast<std::string>(ret)+". Cache file path : " + settingsTempPath);
    }
    else
    {
        LOG("GetAgentReplicationSettings succeeded");

        std::ifstream ifsTempSettingsFile(settingsTempPath.c_str());
        std::string settingsStr = std::string(
            std::istreambuf_iterator<char>(ifsTempSettingsFile),
            std::istreambuf_iterator<char>());

        // No need to catch any exception as the above test client does this parsing as well and 
        // if we are in this step, it means that the parsing was successful.
        agentSettings = JSON::consumer<AgentSettings>::convert(settingsStr);
    }

    // Delete the temp file only on success.
    if (ret == 0)
        boost::filesystem::remove(settingsTempPath);

    return ret;
}

int32_t SimulatedTestMachine::PostAgentLogsToRcm(const AgentSettings &agentSettings,
    const std::string &messageType,
    const std::string &messageSource,
    const std::string &serializedLogMessages)
{
    int32_t ret = 0;

    ret = m_rcmTestClient.PostAgentLogsToRcm(
        agentSettings, messageType, messageSource, serializedLogMessages);

    if (ret != 0)
    {
        if (m_rcmTestClient.GetErrorCode() != 0)
            ret = m_rcmTestClient.GetErrorCode();
        LOG("SendAgentLog failed with " + boost::lexical_cast<std::string>(ret));
    }
    else
    {
        LOG("SendAgentLog succeeded.");
    }

    return ret;
}

int32_t SimulatedTestMachine::UnregisterSourceAgent()
{
    int32_t ret = 0;

    ret = m_rcmTestClient.TestUnregisterSourceAgent();

    if (ret != 0)
    {
        ret = m_rcmTestClient.GetErrorCode();
        LOG("UnregisterSourceAgent failed with " + boost::lexical_cast<std::string>(ret));
    }
    else
    {
        LOG("UnregisterSourceAgent succeeded");
    }

    return ret;
}

int32_t SimulatedTestMachine::UnregisterMachine()
{
    int32_t ret = 0;

    ret = m_rcmTestClient.TestUnregisterMachine();

    if (ret != 0)
    {
        ret = m_rcmTestClient.GetErrorCode();
        LOG("UnregisterMachine failed with " + boost::lexical_cast<std::string>(ret));
    }
    else
    {
        LOG("UnregisterMachine succeeded");
    }

    return ret;
}
