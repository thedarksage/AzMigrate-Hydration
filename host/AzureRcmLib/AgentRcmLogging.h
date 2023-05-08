/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File        :   AgentRcmLogging.h

Description :   Contains helper routines to post events and trace logs to an Azure
service bus for RCM to consume.

+------------------------------------------------------------------------------------+
*/

#ifndef _AGENT_RCM_LOGGING_H
#define _AGENT_RCM_LOGGING_H

#include <map>
#include <json_writer.h>

#include "AgentSettings.h"

namespace RcmClientLib
{

    class AgentLogEventInputBase
    {
    public:
        std::string MemberName;
        std::string SourceFilePath;
        int32_t SourceLineNumber;
        std::string AgentTimeStamp; // DateTime .Net object;
        std::string AgentLogLevel;
        std::string MachineId;
        std::string ComponentId;
        std::string ErrorCode;
        std::string DiskId;
        std::string DiskNumber;
        std::string SubComponent;
        std::string AgentPid;
        std::string AgentTid;
        std::string ExtendedProperties;
        std::string Message;

        void serialize(JSON::Adapter &adapter)
        {
            JSON::Class root(adapter, "AgentLogEventInputBase", false);

            JSON_E(adapter, MemberName);
            JSON_E(adapter, SourceFilePath);
            JSON_E(adapter, SourceLineNumber);
            JSON_E(adapter, AgentTimeStamp);
            JSON_E(adapter, AgentLogLevel);
            JSON_E(adapter, MachineId);
            JSON_E(adapter, ComponentId);
            JSON_E(adapter, ErrorCode);
            JSON_E(adapter, DiskId);
            JSON_E(adapter, DiskNumber);
            JSON_E(adapter, SubComponent);
            JSON_E(adapter, AgentPid);
            JSON_E(adapter, AgentTid);
            JSON_E(adapter, ExtendedProperties);
            JSON_T(adapter, Message);
        }
    };

    class AgentLogSchematizedEventInputBase : public AgentLogEventInputBase
    {
    public:
        std::string SourceEventId;

        void serialize(JSON::Adapter &adapter)
        {
            JSON::Class root(adapter, "AgentLogSchematizedEventInputBase", false);

            JSON_E(adapter, MemberName);
            JSON_E(adapter, SourceFilePath);
            JSON_E(adapter, SourceLineNumber);
            JSON_E(adapter, AgentTimeStamp);
            JSON_E(adapter, AgentLogLevel);
            JSON_E(adapter, MachineId);
            JSON_E(adapter, ComponentId);
            JSON_E(adapter, ErrorCode);
            JSON_E(adapter, DiskId);
            JSON_E(adapter, DiskNumber);
            JSON_E(adapter, SubComponent);
            JSON_E(adapter, AgentPid);
            JSON_E(adapter, AgentTid);
            JSON_E(adapter, ExtendedProperties);
            JSON_E(adapter, Message);
            JSON_T(adapter, SourceEventId);
        }
    };

    class AgentLogExtendedProperties
    {
    public:
        std::map<std::string, std::string> ExtendedProperties;

        void serialize(JSON::Adapter &adapter)
        {
            JSON::Class root(adapter, "AgentLogExtendedProperties", false);

            JSON_T(adapter, ExtendedProperties);
        }
    };

    class AgentLogAdminEventInput : public AgentLogSchematizedEventInputBase
    {
    };

    class AgentLogOperationalEventInput : public AgentLogSchematizedEventInputBase
    {
    };

    class AgentLogDebugEventInput : public AgentLogEventInputBase
    {
    };

    class AgentLogAnalyticEventInput : public AgentLogEventInputBase
    {
    };
}

#endif //_AGENT_RCM_LOGGING_H
