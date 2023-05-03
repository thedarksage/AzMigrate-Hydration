#ifndef SIMULATED_TEST_MACHINE__H
#define SIMULATED_TEST_MACHINE__H

#include <boost/random/mersenne_twister.hpp>
#include <map>
#include <string>

#include "RcmClientSettings.h"
#include "RcmTestClient.h"
#include "volumegroupsettings.h"

#define INT32_MAX        2147483647
#define UINT32_MAX       0xffffffff

class SimulatedTestMachine
{
public:
    typedef std::map<std::string, SimulatedTestMachine> SimulatedTestMachinesMap;

    static void CreateSimulatedTestMachines(size_t count, const RcmClientSettings &settings, SimulatedTestMachinesMap &machinesMap);

    int32_t RegisterMachine();
    int32_t RegisterSourceAgent();
    int32_t GetAgentReplicationSettings(AgentSettings &agentSettings);

    int32_t PostAgentLogsToRcm(const AgentSettings &agentSettings,
        const std::string &messageType,
        const std::string &messageSource,
        const std::string &serializedLogMessages);

    int32_t UnregisterSourceAgent();
    int32_t UnregisterMachine();

    std::string GetHostId() const
    {
        return m_hostId;
    }

    static uint32_t GenerateRandomNumber32(uint32_t min = 0, uint32_t max = UINT32_MAX);

private:
    static boost::mt19937 randNumGen_32;

    RcmClientSettings m_clientSettings;
    std::string m_hostId;
    std::string m_hostName;
    RcmTestClient::TestClient m_rcmTestClient;

    std::vector<VolumeSummary> m_diskAndVolSummaries;
    uint32_t m_nextAvailDiskNum;
    std::string m_nextAvailMountPt;

    SimulatedTestMachine(RcmClientSettings clientSettings, std::string hostId);

    void CreateLogicalVolume(const std::string& name,
        const std::string& mountPoint,
        const std::string& volumeLabel,
        const std::string& volumeGroup,
        SV_LONGLONG rawCapacity,
        VolumeSummary::FormatLabel formatLabel);

    void CreateDisk();
};

#endif //SIMULATED_TEST_MACHINE__H
