/*
* FailoverClusterMultiVMUtil
*/

#include "FailoverClusterMultiVMUtil.h"

using namespace SharedDiskClusterMultiVM;

SVSTATUS ClusterConsistencyUtil::GetVacpMultiVMStartUpParameters(std::string& masterNode,
    std::vector<MultiVmConsistencyParticipatingNode>& participatingNodes,
    AgentSettings settings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string masterSelectionErrorMessage = "Failed to select master node.";

    do
    {
        ClusterState clusState = ClusterState::UNKNOWN;
        std::vector<std::string> clusCurrNodes;
        status = GetCurrentClusterState(clusState, participatingNodes, clusCurrNodes, settings);

        if (status != SVS_OK)
        {
            break;
        }

        switch (clusState)
        {
        case UNCHANGED: {
            masterNode = m_masterNode;
            participatingNodes = m_participatingMachines;
            break;
        }

        case CHANGED: {
            status = m_multiNodeSelector->select(clusCurrNodes, masterNode);
            if (status != SVS_OK)
            {
                m_errMsg = masterSelectionErrorMessage;
                break;
            }
            m_masterNode = masterNode;
            m_participatingMachines = participatingNodes;
            break;
        }

        default: {
            // UNKNOWN - state at the start of VACP.
            status = m_multiNodeSelector->select(settings.m_consistencySettings.ParticipatingMachines, masterNode);
            if (status != SVS_OK)
            {
                m_errMsg = masterSelectionErrorMessage;
                break;
            }
            participatingNodes = settings.m_consistencySettings.MultiVmConsistencyParticipatingNodes;
            m_masterNode = masterNode;
            m_participatingMachines = participatingNodes;

            break;
        }
        }

    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}


SVSTATUS ClusterConsistencyUtil::GetCurrentClusterState(ClusterState& state,
    std::vector<MultiVmConsistencyParticipatingNode>& participatingNodes,
    std::vector<std::string>& clusCurrNodes,
    AgentSettings settings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;

    do
    {
        FailoverClusterInfo clusInfo;
        if (!clusInfo.IsClusterNode())
        {
            errMsg = "This node is not part of cluster and is not tracked in cluster context";
            status = SVE_INVALIDARG;
            break;
        }

        if (m_participatingMachines.empty() || m_masterNode.empty())
        {
            state = ClusterState::UNKNOWN;
            DebugPrintf(SV_LOG_DEBUG, " %s cluster State is uknown. starting vacp with rcm specified parameters\n", FUNCTION_NAME);
            break;
        }

        status = clusInfo.CollectFailoverClusterProperties();
        if (status != SVS_OK)
        {
            errMsg = "Failed to get the cluster info";
            status = SVE_ABORT;
            break;
        }

        std::map<std::string, NodeEntity> failoverClusterNodesMap;
        clusInfo.GetClusterUpNodesMap(failoverClusterNodesMap);
        bool isClusterChanged = IsClusterStateChanged(failoverClusterNodesMap);
        if (!isClusterChanged)
        {
            state = ClusterState::UNCHANGED;
            DebugPrintf(SV_LOG_DEBUG, " %s cluster State is unchanged. Restarting VACP with same set of parameters\n", FUNCTION_NAME);
            break;
        }
        std::map<std::string, MultiVmConsistencyParticipatingNode> currCluterProtectedNodes;
        FetchCurrClusterProtectedNodes(currCluterProtectedNodes, settings);

        std::string name;
        MultiVmConsistencyParticipatingNode multiVmSetting;
        bool isNodeSettingNotPresentInRCM = false;

        std::map<std::string, NodeEntity>::iterator itr = failoverClusterNodesMap.begin();
        for (/**/;
            itr != failoverClusterNodesMap.end();
            itr++)
        {
            name = itr->first;
            if (currCluterProtectedNodes.find(name) == currCluterProtectedNodes.end())
            {
                isNodeSettingNotPresentInRCM = true;
                break;
            }
            multiVmSetting = currCluterProtectedNodes.find(name)->second;
            participatingNodes.push_back(multiVmSetting);
            clusCurrNodes.push_back(multiVmSetting.Name);
        }

        if (isNodeSettingNotPresentInRCM)
        {
            errMsg = "Node=" + name + " settings is not present.";
            status = SVE_INVALIDARG;
            clusInfo.dumpInfo();
        }
        state = ClusterState::CHANGED;

    } while (false);

    if (!errMsg.empty())
    {
        m_errMsg = errMsg;
        DebugPrintf(SV_LOG_DEBUG, " %s cluster state fetch failed with error=%s\n", FUNCTION_NAME, errMsg.c_str());
    }
        
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

bool ClusterConsistencyUtil::IsClusterStateChanged(std::map<std::string, NodeEntity> failoverClusterNodesMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTER%ED s\n", FUNCTION_NAME);
    bool isClusterChanged = false;

    do
    {
        if (failoverClusterNodesMap.size() != m_participatingMachines.size())
        {
            isClusterChanged = true;
            break;
        }
        std::string hostName;
        std::vector<std::string> hostNameSplits;
        std::vector<MultiVmConsistencyParticipatingNode>::iterator itr = m_participatingMachines.begin();

        for (/**/;
            itr != m_participatingMachines.end();
            itr++)
        {
            hostName = (*itr).Name;
            boost::split(hostNameSplits, hostName, boost::is_any_of("."));
            if (hostNameSplits.size() <= 0
                || failoverClusterNodesMap.find(hostNameSplits[0]) == failoverClusterNodesMap.end())
            {
                isClusterChanged = true;
                break;
            }
        }

    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s, isClusterChanged=%d\n", FUNCTION_NAME, isClusterChanged);
    return isClusterChanged;
}

void ClusterConsistencyUtil::FetchCurrClusterProtectedNodes(
    std::map<std::string, MultiVmConsistencyParticipatingNode>& currClusterRcmProtectedNodes,
    AgentSettings settings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool isClusterNodeProtectedInRCM = false;

    std::vector<MultiVmConsistencyParticipatingNode>::iterator itr =
        settings.m_consistencySettings.MultiVmConsistencyParticipatingNodes.begin();

    std::string nodeName;
    std::vector<std::string> nodeNameSplits;
    for (/**/;
        itr != settings.m_consistencySettings.MultiVmConsistencyParticipatingNodes.end();
        itr++)
    {
        nodeName = (*itr).Name;
        boost::split(nodeNameSplits, nodeName, boost::is_any_of("."));
        if (nodeNameSplits.size() > 0)
        {
            currClusterRcmProtectedNodes.insert(std::pair<std::string, MultiVmConsistencyParticipatingNode>(nodeNameSplits[0], (*itr)));
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}