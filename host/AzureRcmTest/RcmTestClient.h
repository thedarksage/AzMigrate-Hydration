#ifndef _RCM_TEST_CLIENT_H
#define _RCM_TEST_CLIENT_H

#include "json_writer.h"
#include "json_reader.h"
#include "RcmClientImpl.h"

using namespace std;
using namespace RcmClientLib;

namespace RcmTestClient
{
    class SerailizableVolumeSummary;

    typedef std::vector<SerailizableVolumeSummary> 
                    SerailizableVolumeSummaries_t;
    typedef SerailizableVolumeSummaries_t::iterator
                    SerializableVolumeSummariesIterator;

    class SerailizableVolumeSummary
    {
    public:
        std::string id;
        std::string name;
        int type;
        int vendor;
        std::string fileSystem;
        std::string mountPoint;
        bool isMounted;
        bool isSystemVolume;
        bool isCacheVolume;
        uint64_t capacity;
        bool locked;
        uint64_t physicalOffset;
        int sectorSize;
        std::string volumeLabel;
        bool containPagefile;
        bool isStartingAtBlockZero;
        std::string volumeGroup;
        int volumeGroupVendor;
        bool isMultipath;
        uint64_t rawcapacity;
        int writeCacheState;
        int formatLabel;
        std::string attributes;
        SerailizableVolumeSummaries_t svsChildren;
    public:
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SerailizableVolumeSummary", false);

            JSON_E(adapter, id);
            JSON_E(adapter, name);
            JSON_E(adapter, type);
            JSON_E(adapter, vendor);
            JSON_E(adapter, fileSystem);
            JSON_E(adapter, mountPoint);
            JSON_E(adapter, isMounted);
            JSON_E(adapter, isSystemVolume);
            JSON_E(adapter, isCacheVolume);
            JSON_E(adapter, capacity);
            JSON_E(adapter, locked);
            JSON_E(adapter, physicalOffset);
            JSON_E(adapter, sectorSize);
            JSON_E(adapter, volumeLabel);
            JSON_E(adapter, containPagefile);
            JSON_E(adapter, isStartingAtBlockZero);
            JSON_E(adapter, volumeGroup);
            JSON_E(adapter, volumeGroupVendor);
            JSON_E(adapter, isMultipath);
            JSON_E(adapter, rawcapacity);
            JSON_E(adapter, writeCacheState);
            JSON_E(adapter, formatLabel);
            JSON_E(adapter, attributes);
            JSON_T(adapter,svsChildren);
        }
    };

    class MachineSummary
    {
    public :
        std::string hostId;
        std::string hostName;
        std::string biosId;
        std::string managementId;
        std::string osAttributes;
        uint32_t osType;
        uint32_t endianness;
        std::string hyperVisorName;
        std::string hyperVisorVersion;

        SerailizableVolumeSummaries_t VolumeSummaries;
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "MachineSummary", false);

            JSON_E(adapter, hostId);
            JSON_E(adapter, hostName);
            JSON_E(adapter, biosId);
            JSON_E(adapter, managementId);
            JSON_E(adapter, osAttributes);
            JSON_E(adapter, osType);
            JSON_E(adapter, endianness);
            JSON_E(adapter, hyperVisorName);
            JSON_E(adapter, hyperVisorVersion);
            JSON_T(adapter, VolumeSummaries);
        }
    };

    
    class TestClient {
        
        RcmClientImpl* m_pimpl;

    public:
        TestClient(RcmClientSettings& settings, std::string& hostId)
            :m_pimpl(new RcmClientImpl(settings, hostId))
        {}

        int TestPost(std::string& endPoint, std::string&jsonInput, std::string& jsonOutput)
        {
            return m_pimpl->Post(endPoint, jsonInput, jsonOutput);
        }

#ifdef _DEBUG
        int TestConnection()
        {
            return m_pimpl->TestConnection();
        }
#endif

        int TestUnregisterMachine()
        {
            return m_pimpl->Unregister(UNREGISTER_MACHINE);
        }

        int TestUnregisterSourceAgent()
        {
            return m_pimpl->Unregister(UNREGISTER_SOURCE_AGENT);
        }

        int TestRegisterSourceAgent(std::string& hostname,
            std::string& agentVersion,
            std::string& driverVersion,
            std::string& installPath)
        {
            return m_pimpl->RegisterSourceAgent(hostname,
                agentVersion,
                driverVersion,
                installPath);
        }

        int TestRegisterMachine(std::string& hostName,
            std::string& biosId,
            std::string& managementId,
            Object const& osinfo,
            OS_VAL osval,
            ENDIANNESS e,
            CpuInfos_t const& cpuinfo,
            HypervisorInfo_t const& hypervinfo,
            VolumeSummaries_t const& volumeSummaries,
            bool update)
        {
            return m_pimpl->RegisterMachine(hostName,
                biosId,
                managementId,
                osinfo,
                osval,
                e,
                cpuinfo,
                hypervinfo,
                volumeSummaries,
                update);
        }

        int GetReplicationSettings(std::vector<RcmJob>& jobs, std::string& settingsFilePath)
        {
            return m_pimpl->GetReplicationSettings(jobs,settingsFilePath);
        }

        int UpdateAgentJobStatus(RcmJob& job)
        {
            return m_pimpl->UpdateAgentJobStatus(job);
        }

        int TestGetBearerToken(std::string& token)
        {
            return m_pimpl->GetBearerToken(token);
        }

        int PostAgentLogsToRcm(const AgentSettings &agentSettings,
            const std::string &messageType,
            const std::string &messageSource,
            const std::string &serializedLogMessages)
        {
            return m_pimpl->PostAgentLogsToRcm(
                agentSettings, messageType, messageSource, serializedLogMessages);
        }

        int32_t GetErrorCode()
        {
            return m_pimpl->GetErrorCode();
        }
    };
}
#endif
