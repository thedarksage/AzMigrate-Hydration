#ifndef FAILOVER_CLUSTER_MULTIVM_UTIL
#define FAILOVER_CLUSTER_MULTIVM_UTIL

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <string>


#include "agentSettings.h"
#include "svtypes.h"
#include "MultiVMMasterNodeSelector.h"
#include "Failoverclusterinfocollector.h"

using namespace RcmClientLib;

namespace SharedDiskClusterMultiVM {

    enum ClusterState {
        UNKNOWN = 0,
        UNCHANGED,
        CHANGED
    };

};

class ClusterConsistencyUtil
{

public:
    ClusterConsistencyUtil()
    {
        m_multiNodeSelector = new NodeNameBasedMasterNodeSelector;
    }

    ~ClusterConsistencyUtil() {
        if (m_multiNodeSelector)
        {
            delete m_multiNodeSelector;
            m_multiNodeSelector = NULL;
        }
    }

    /// gets vacp multi-vm startup parameters
    SVSTATUS GetVacpMultiVMStartUpParameters(std::string& masterNode,
        std::vector<MultiVmConsistencyParticipatingNode>& participatingNodes,
        AgentSettings settings);

    std::string GetErrorMessage() { return m_errMsg; }

private:
    /// checks and gets the current state of cluster
    SVSTATUS GetCurrentClusterState(SharedDiskClusterMultiVM::ClusterState& state,
        std::vector<MultiVmConsistencyParticipatingNode>& participatingNodes,
        std::vector<std::string>& clusCurrNodes,
        AgentSettings settings);

    /// creates a map of the multi-vm participating nodes
    void FetchCurrClusterProtectedNodes(
        std::map<std::string, MultiVmConsistencyParticipatingNode>& currCluterProtectedNodes,
        AgentSettings settings);

    /// compares the current and previous in failover cluster
    bool IsClusterStateChanged(std::map<std::string, NodeEntity> failoverClusterNodesMap);

    MasterNodeSelectorPtr m_multiNodeSelector;
    std::vector<MultiVmConsistencyParticipatingNode> m_participatingMachines;
    std::string m_masterNode;

    std::string m_errMsg;

};

#endif