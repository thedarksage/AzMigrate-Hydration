/*
+-------------------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+-------------------------------------------------------------------------------------------------+
File        :   AgentRcmLogging.cpp

Description :   AgentRcmLogging Library implementation

+-------------------------------------------------------------------------------------------------+
*/

#include "AgentRcmLogging.h"
#include "logger.h"
#include "portable.h"
#include "portablehelpers.h"
#include "ServiceBusQueue.h"
#include "MonitoringMsgSettings.h"

using namespace AzureStorageRest;
using namespace RcmClientLib;

SVSTATUS AgentRcmLoggingLib::PostLogToRcm(const std::string &hostId,
    const AgentSettings &agentSettings,
    const std::string &eventMessage,
    const std::map<std::string, std::string> &propMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string errMsg;

    std::string uri;
    if (!MonitoringAgent::GetInstance().GetTransportUri(agentSettings, RcmMsgLogs, uri))
    {
        errMsg = "Agent Logging Queue URI in Cached settings is empty.";
        DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, errMsg.c_str());
        return SVE_INVALIDARG;
    }

    DebugPrintf(SV_LOG_DEBUG, "%s: Agent Logging Queue URI %s\n", FUNCTION_NAME, uri.c_str());
    bool status = MonitoringAgent::GetInstance().SendMessageToRcm(uri,
        eventMessage,
        propMap);

    if (!status)
    {
        errMsg = "Failed to send agent logs to RCM.";
        DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, errMsg.c_str());
        return SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SVS_OK;
}