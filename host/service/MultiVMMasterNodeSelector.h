#ifndef MULTI_VM_MASTER_NODE_SELECTOR
#define MULTI_VM_MASTER_NODE_SELECTOR


#include "logger.h"
#include "agentSettings.h"
#include "svtypes.h"
#include "portablehelpers.h"

#include <string>

using namespace RcmClientLib;

class MasterNodeSelector
{
public:
    virtual ~MasterNodeSelector() {}
	virtual SVSTATUS select(std::vector<std::string>& participatingMachines, std::string& coordinatorNode) = 0;
};

typedef MasterNodeSelector* MasterNodeSelectorPtr;

class NodeNameBasedMasterNodeSelector : public MasterNodeSelector
{
    SVSTATUS getSharedDiskClusterCoordinatorMachine(std::vector<std::string>& participatingMachines, std::string& coordinatorNode)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        do
        {
            if (participatingMachines.empty())
            {
                DebugPrintf(SV_LOG_ERROR, "%s no participating machines in consistency settings\n", FUNCTION_NAME);
                status = SVE_FAIL;
                break;
            }

            std::sort(participatingMachines.begin(),
                participatingMachines.end());

            coordinatorNode = participatingMachines.front();

        } while (false);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s, coordinator node=%s\n", FUNCTION_NAME, coordinatorNode.c_str());
        return status;
    }

public:
    SVSTATUS select(std::vector<std::string>& participatingMachines,std::string& coordinatorNode)
	{
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        status = getSharedDiskClusterCoordinatorMachine(participatingMachines, coordinatorNode);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

        return status;
	}
};

#endif