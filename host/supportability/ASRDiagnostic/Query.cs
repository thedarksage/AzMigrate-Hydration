using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Kusto.Cloud.Platform.Data;
using Kusto.Data.Common;
using Kusto.Data.Net.Client;

namespace Microsoft.Internal.SiteRecovery.ASRDiagnostic
{
    public class Query
    {
        /* query1 : SubscriptionId + ClientRequestId
         * query2 : SubscriptionId + ObjectName
         * query3 : SubscriptionId
         * query4 : SubscriptionId + ObjectName + ClientRequestId 
         * query5 : SubscriptionId + SourceVmName + Hostid
         * query6 : SubscriptionId 
         * query7 : SubscriptionId + SourceVmName
         * query8 : SubscriptionId + HostId
         * */

        public static string query1 = @"SRSOperationEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where ClientRequestId == {3}
                                        | where ScenarioName == ""EnableDr""
                                        | where State in (""Succeeded"",""Failed"")
                                        | extend Provider = case(ReplicationProviderId == ""371f87f9-53e5-429f-b10b-f14700a4b137"", ""H2A"",
                                                                 ReplicationProviderId == ""b5733d78-9ca5-4b41-9906-3646848dee46"", 'V2Av1',
                                                                 ReplicationProviderId == ""c42d7660-b374-4d4b-afb0-e7c67b53e20c"", 'V2A',
                                                                 ReplicationProviderId == ""b68561cf-6b85-4e83-ad51-eb13013ff659"", 'V2A',
                                                                 ReplicationProviderId == ""d9cbc2cd-8a3c-4222-bfa9-7fee17bb81bd"", 'A2A',
                                                                 ""Other"")
                                        | summarize arg_max(PreciseTimeStamp,*) by ObjectId
                                        | project {0},{1},SubscriptionId,ObjectId,ObjectName,LastClientRequestId=ClientRequestId,ScenarioName,State,Provider
                                        | join kind=inner (
                                        SRSDataEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where ClientRequestId == {3}
                                        | where StampName !startswith (""ccy"") and StampName !contains ""-can01""  and StampName !contains ""onebox"" and StampName !startswith ""ecy"" and StampName !contains ""bvtd2"" and isnotempty(StampName)
                                        | where isnotempty(SubscriptionId) and SubscriptionId != ""null"" and isnotempty(ResourceId) and ResourceId != ""null""
                                        | where ResourceId !in (RunnerResourceIDs()) and SubscriptionId !in (""srspartition"", ""12341234-1234-1234-1234-123412341234"", ""fc3c2027-2e1e-4a2d-a49c-6c1208155c06"")
                                        | extend AgentVersion=todouble(extract(""Microsoft-ASR_UA_([0-9]+[.][0-9]+)"",1, Message))
                                        | where isnotempty(AgentVersion)
                                        | summarize arg_max(PreciseTimeStamp,*) by ClientRequestId
                                        | distinct ClientRequestId,AgentVersion) on $left.LastClientRequestId == $right.ClientRequestId
                                        | join (
                                        SRSDataEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where ClientRequestId == {3}
                                        | where StampName !startswith (""ccy"") and StampName !contains ""-can01""  and StampName !contains ""onebox"" and StampName !startswith ""ecy"" and StampName !contains ""bvtd2"" and isnotempty(StampName)
                                        | where isnotempty(SubscriptionId) and SubscriptionId != ""null"" and isnotempty(ResourceId) and ResourceId != ""null""
                                        | where ResourceId !in (RunnerResourceIDs()) and SubscriptionId !in (""srspartition"", ""12341234-1234-1234-1234-123412341234"", ""fc3c2027-2e1e-4a2d-a49c-6c1208155c06"")
                                        | where Message contains ""Check for unsupported OS""
                                        | extend OSType = extract(""osType = ([^,]+)"", 1, Message)
                                        | extend OSName = extract(""osName = ([^,]+)"", 1, Message)
                                        | extend OSVersion = extract(""osVersion = ([^\n]+)"", 1, Message)
                                        | summarize make_set_if(OSType, OSType != """"), make_set_if(OSName, OSName != """"), make_set_if(OSVersion, OSVersion != """") by ClientRequestId
                                        | mv-expand set_OSName, set_OSType, set_OSVersion
                                        | extend OSName = tostring(set_OSName)
                                        | extend OSVersion= tostring(set_OSVersion)
                                        | extend OSType = tostring(set_OSType)
                                        | distinct ClientRequestId,OSName,OSType,OSVersion) on $left.LastClientRequestId == $right.ClientRequestId
                                        | join kind=leftouter (
                                        SRSErrorEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where ClientRequestId == {3}
                                        | where ErrorLevel == ""Error""
                                        | where ErrorTags != """"
                                        | parse ErrorTags with * ""\""Source\"":\"""" ErrorSource ""\"""" *
                                        | parse ErrorTags with * ""\""Category\"":\"""" ErrorCategory ""\"""" *
                                        | summarize arg_max(PreciseTimeStamp,*) by ClientRequestId
                                        | distinct ClientRequestId,ErrorCodeName,ErrorTags,ErrorCategory,ErrorSource) on $left.LastClientRequestId == $right.ClientRequestId
                                        | project Provider,ObjectName,LastClientRequestId,ScenarioName,State,AgentVersion,OSName,OSType,OSVersion,ErrorCodeName";

        public static string query2 = @"SRSOperationEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where ObjectName == {3}
                                        | where ScenarioName == ""EnableDr""
                                        | where State in (""Succeeded"",""Failed"")
                                        | extend Provider = case(ReplicationProviderId == ""371f87f9-53e5-429f-b10b-f14700a4b137"", ""H2A"",
                                                                 ReplicationProviderId == ""b5733d78-9ca5-4b41-9906-3646848dee46"", 'V2Av1',
                                                                 ReplicationProviderId == ""c42d7660-b374-4d4b-afb0-e7c67b53e20c"", 'V2A',
                                                                 ReplicationProviderId == ""b68561cf-6b85-4e83-ad51-eb13013ff659"", 'V2A',
                                                                 ReplicationProviderId == ""d9cbc2cd-8a3c-4222-bfa9-7fee17bb81bd"", 'A2A',
                                                                 ""Other"")
                                        | summarize arg_max(PreciseTimeStamp,*) by ObjectId
                                        | project {0},{1},SubscriptionId,ObjectId,ObjectName,LastClientRequestId=ClientRequestId,ScenarioName,State,Provider
                                        | join kind=inner (
                                        SRSDataEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where StampName !startswith (""ccy"") and StampName !contains ""-can01""  and StampName !contains ""onebox"" and StampName !startswith ""ecy"" and StampName !contains ""bvtd2"" and isnotempty(StampName)
                                        | where isnotempty(SubscriptionId) and SubscriptionId != ""null"" and isnotempty(ResourceId) and ResourceId != ""null""
                                        | where ResourceId !in (RunnerResourceIDs()) and SubscriptionId !in (""srspartition"", ""12341234-1234-1234-1234-123412341234"", ""fc3c2027-2e1e-4a2d-a49c-6c1208155c06"")
                                        | extend AgentVersion=todouble(extract(""Microsoft-ASR_UA_([0-9]+[.][0-9]+)"",1, Message))
                                        | where isnotempty(AgentVersion)
                                        | summarize arg_max(PreciseTimeStamp,*) by ClientRequestId
                                        | distinct ClientRequestId,AgentVersion) on $left.LastClientRequestId == $right.ClientRequestId
                                        | join (
                                        SRSDataEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where StampName !startswith (""ccy"") and StampName !contains ""-can01""  and StampName !contains ""onebox"" and StampName !startswith ""ecy"" and StampName !contains ""bvtd2"" and isnotempty(StampName)
                                        | where isnotempty(SubscriptionId) and SubscriptionId != ""null"" and isnotempty(ResourceId) and ResourceId != ""null""
                                        | where ResourceId !in (RunnerResourceIDs()) and SubscriptionId !in (""srspartition"", ""12341234-1234-1234-1234-123412341234"", ""fc3c2027-2e1e-4a2d-a49c-6c1208155c06"")
                                        | where Message contains ""Check for unsupported OS""
                                        | extend OSType = extract(""osType = ([^,]+)"", 1, Message)
                                        | extend OSName = extract(""osName = ([^,]+)"", 1, Message)
                                        | extend OSVersion = extract(""osVersion = ([^\n]+)"", 1, Message)
                                        | summarize make_set_if(OSType, OSType != """"), make_set_if(OSName, OSName != """"), make_set_if(OSVersion, OSVersion != """") by ClientRequestId
                                        | mv-expand set_OSName, set_OSType, set_OSVersion
                                        | extend OSName = tostring(set_OSName)
                                        | extend OSVersion= tostring(set_OSVersion)
                                        | extend OSType = tostring(set_OSType)
                                        | distinct ClientRequestId,OSName,OSType,OSVersion) on $left.LastClientRequestId == $right.ClientRequestId
                                        | join kind=leftouter (
                                        SRSErrorEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where ErrorLevel == ""Error""
                                        | where ErrorTags != """"
                                        | parse ErrorTags with * ""\""Source\"":\"""" ErrorSource ""\"""" *
                                        | parse ErrorTags with * ""\""Category\"":\"""" ErrorCategory ""\"""" *
                                        | summarize arg_max(PreciseTimeStamp,*) by ClientRequestId
                                        | distinct ClientRequestId,ErrorCodeName,ErrorTags,ErrorCategory,ErrorSource) on $left.LastClientRequestId == $right.ClientRequestId
                                        | project Provider,ObjectName,LastClientRequestId,ScenarioName,State,AgentVersion,OSName,OSType,OSVersion,ErrorCodeName";

        public static string query3 = @"SRSOperationEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where ScenarioName == ""EnableDr""
                                        | where State in (""Succeeded"",""Failed"")
                                        | extend Provider = case(ReplicationProviderId == ""371f87f9-53e5-429f-b10b-f14700a4b137"", ""H2A"",
                                                                 ReplicationProviderId == ""b5733d78-9ca5-4b41-9906-3646848dee46"", 'V2Av1',
                                                                 ReplicationProviderId == ""c42d7660-b374-4d4b-afb0-e7c67b53e20c"", 'V2A',
                                                                 ReplicationProviderId == ""b68561cf-6b85-4e83-ad51-eb13013ff659"", 'V2A',
                                                                 ReplicationProviderId == ""d9cbc2cd-8a3c-4222-bfa9-7fee17bb81bd"", 'A2A',
                                                                 ""Other"")
                                        | summarize arg_max(PreciseTimeStamp,*) by ObjectId
                                        | project {0},{1},SubscriptionId,ObjectId,ObjectName,LastClientRequestId=ClientRequestId,ScenarioName,State,Provider
                                        | join kind=inner (
                                        SRSDataEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where StampName !startswith (""ccy"") and StampName !contains ""-can01""  and StampName !contains ""onebox"" and StampName !startswith ""ecy"" and StampName !contains ""bvtd2"" and isnotempty(StampName)
                                        | where isnotempty(SubscriptionId) and SubscriptionId != ""null"" and isnotempty(ResourceId) and ResourceId != ""null""
                                        | where ResourceId !in (RunnerResourceIDs()) and SubscriptionId !in (""srspartition"", ""12341234-1234-1234-1234-123412341234"", ""fc3c2027-2e1e-4a2d-a49c-6c1208155c06"")
                                        | extend AgentVersion=todouble(extract(""Microsoft-ASR_UA_([0-9]+[.][0-9]+)"",1, Message))
                                        | where isnotempty(AgentVersion)
                                        | summarize arg_max(PreciseTimeStamp,*) by ClientRequestId
                                        | distinct ClientRequestId,AgentVersion) on $left.LastClientRequestId == $right.ClientRequestId
                                        | join (
                                        SRSDataEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where StampName !startswith (""ccy"") and StampName !contains ""-can01""  and StampName !contains ""onebox"" and StampName !startswith ""ecy"" and StampName !contains ""bvtd2"" and isnotempty(StampName)
                                        | where isnotempty(SubscriptionId) and SubscriptionId != ""null"" and isnotempty(ResourceId) and ResourceId != ""null""
                                        | where ResourceId !in (RunnerResourceIDs()) and SubscriptionId !in (""srspartition"", ""12341234-1234-1234-1234-123412341234"", ""fc3c2027-2e1e-4a2d-a49c-6c1208155c06"")
                                        | where Message contains ""Check for unsupported OS""
                                        | extend OSType = extract(""osType = ([^,]+)"", 1, Message)
                                        | extend OSName = extract(""osName = ([^,]+)"", 1, Message)
                                        | extend OSVersion = extract(""osVersion = ([^\n]+)"", 1, Message)
                                        | summarize make_set_if(OSType, OSType != """"), make_set_if(OSName, OSName != """"), make_set_if(OSVersion, OSVersion != """") by ClientRequestId
                                        | mv-expand set_OSName, set_OSType, set_OSVersion
                                        | extend OSName = tostring(set_OSName)
                                        | extend OSVersion= tostring(set_OSVersion)
                                        | extend OSType = tostring(set_OSType)
                                        | distinct ClientRequestId,OSName,OSType,OSVersion) on $left.LastClientRequestId == $right.ClientRequestId
                                        | join kind=leftouter (
                                        SRSErrorEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where ErrorLevel == ""Error""
                                        | where ErrorTags != """"
                                        | parse ErrorTags with * ""\""Source\"":\"""" ErrorSource ""\"""" *
                                        | parse ErrorTags with * ""\""Category\"":\"""" ErrorCategory ""\"""" *
                                        | summarize arg_max(PreciseTimeStamp,*) by ClientRequestId
                                        | distinct ClientRequestId,ErrorCodeName,ErrorTags,ErrorCategory,ErrorSource) on $left.LastClientRequestId == $right.ClientRequestId
                                        | project Provider,ObjectName,LastClientRequestId,ScenarioName,State,AgentVersion,OSName,OSType,OSVersion,ErrorCodeName";

        public static string query4 = @"SRSOperationEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where ObjectName == {3}
                                        | where ClientRequestId == {4}
                                        | where ScenarioName == ""EnableDr""
                                        | where State in (""Succeeded"",""Failed"")
                                        | extend Provider = case(ReplicationProviderId == ""371f87f9-53e5-429f-b10b-f14700a4b137"", ""H2A"",
                                                                 ReplicationProviderId == ""b5733d78-9ca5-4b41-9906-3646848dee46"", 'V2Av1',
                                                                 ReplicationProviderId == ""c42d7660-b374-4d4b-afb0-e7c67b53e20c"", 'V2A',
                                                                 ReplicationProviderId == ""b68561cf-6b85-4e83-ad51-eb13013ff659"", 'V2A',
                                                                 ReplicationProviderId == ""d9cbc2cd-8a3c-4222-bfa9-7fee17bb81bd"", 'A2A',
                                                                 ""Other"")
                                        | summarize arg_max(PreciseTimeStamp,*) by ObjectId
                                        | project {0},{1},SubscriptionId,ObjectId,ObjectName,LastClientRequestId=ClientRequestId,ScenarioName,State,Provider
                                        | join kind=inner (
                                        SRSDataEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where ClientRequestId == {4}
                                        | where StampName !startswith (""ccy"") and StampName !contains ""-can01""  and StampName !contains ""onebox"" and StampName !startswith ""ecy"" and StampName !contains ""bvtd2"" and isnotempty(StampName)
                                        | where isnotempty(SubscriptionId) and SubscriptionId != ""null"" and isnotempty(ResourceId) and ResourceId != ""null""
                                        | where ResourceId !in (RunnerResourceIDs()) and SubscriptionId !in (""srspartition"", ""12341234-1234-1234-1234-123412341234"", ""fc3c2027-2e1e-4a2d-a49c-6c1208155c06"")
                                        | extend AgentVersion=todouble(extract(""Microsoft-ASR_UA_([0-9]+[.][0-9]+)"",1, Message))
                                        | where isnotempty(AgentVersion)
                                        | summarize arg_max(PreciseTimeStamp,*) by ClientRequestId
                                        | distinct ClientRequestId,AgentVersion) on $left.LastClientRequestId == $right.ClientRequestId
                                        | join (
                                        SRSDataEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where ClientRequestId == {4}
                                        | where StampName !startswith (""ccy"") and StampName !contains ""-can01""  and StampName !contains ""onebox"" and StampName !startswith ""ecy"" and StampName !contains ""bvtd2"" and isnotempty(StampName)
                                        | where isnotempty(SubscriptionId) and SubscriptionId != ""null"" and isnotempty(ResourceId) and ResourceId != ""null""
                                        | where ResourceId !in (RunnerResourceIDs()) and SubscriptionId !in (""srspartition"", ""12341234-1234-1234-1234-123412341234"", ""fc3c2027-2e1e-4a2d-a49c-6c1208155c06"")
                                        | where Message contains ""Check for unsupported OS""
                                        | extend OSType = extract(""osType = ([^,]+)"", 1, Message)
                                        | extend OSName = extract(""osName = ([^,]+)"", 1, Message)
                                        | extend OSVersion = extract(""osVersion = ([^\n]+)"", 1, Message)
                                        | summarize make_set_if(OSType, OSType != """"), make_set_if(OSName, OSName != """"), make_set_if(OSVersion, OSVersion != """") by ClientRequestId
                                        | mv-expand set_OSName, set_OSType, set_OSVersion
                                        | extend OSName = tostring(set_OSName)
                                        | extend OSVersion= tostring(set_OSVersion)
                                        | extend OSType = tostring(set_OSType)
                                        | distinct ClientRequestId,OSName,OSType,OSVersion) on $left.LastClientRequestId == $right.ClientRequestId
                                        | join kind=leftouter (
                                        SRSErrorEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where SubscriptionId == {2}
                                        | where ClientRequestId == {4}
                                        | where ErrorLevel == ""Error""
                                        | where ErrorTags != """"
                                        | parse ErrorTags with * ""\""Source\"":\"""" ErrorSource ""\"""" *
                                        | parse ErrorTags with * ""\""Category\"":\"""" ErrorCategory ""\"""" *
                                        | summarize arg_max(PreciseTimeStamp,*) by ClientRequestId
                                        | distinct ClientRequestId,ErrorCodeName,ErrorTags,ErrorCategory,ErrorSource) on $left.LastClientRequestId == $right.ClientRequestId
                                        | project Provider,ObjectName,LastClientRequestId,ScenarioName,State,AgentVersion,OSName,OSType,OSVersion,ErrorCodeName";

        public static string query5 = @"let enumToCodeTable = cluster('asrclus1').database('ASRKustoDB').SrsErrorEnumToCode();
                                        SRSShoeboxEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where category == ""AzureSiteRecoveryReplicatedItems""
                                        | parse resourceId with *""/SUBSCRIPTIONS/""SubscriptionId""/RESOURCEGROUPS""*
                                        | extend SubscriptionId = tolower(SubscriptionId)
                                        | where SubscriptionId == {2}
                                        | where SourceVmName == {3}
                                        | extend x = parse_json(properties)
                                        | extend ReplicationProvider = tostring(x.replicationProviderName)
                                        | extend ReplicationProvider = case(ReplicationProvider  == ""InMageAzureV2"",""V2A"",
                                                                            ReplicationProvider  == ""InMage"",""V2A"",
                                                                            ReplicationProvider  == ""HyperVReplicaAzure"",""V2A"",
                                                                            ReplicationProvider)
                                        | extend ProtectionState=tostring(x.protectionState)
                                        | extend ReplicationHealth=tostring(x.replicationHealth)
                                        | where ReplicationHealth in (""Critical"",""Normal"")
                                        | extend HostId=tostring(x.id)
                                        | extend SourceAgentVersion=tostring(x.agentVersion)
                                        | extend SourceAgentVersion=todouble(extract(""([0-9]+[.][0-9]+)"",1, SourceAgentVersion))
                                        | extend SourceVmName=tostring(x.name)
                                        | extend OsFamily=tostring(x.osFamily)
                                        | extend PSHostId=tostring(x.processServerName)
                                        | extend LastHeartBeat=tostring(x.lastHeartbeat)
                                        | extend RpoInSecs=tostring(x.rpoInSeconds)
                                        | extend HealthIssues = x.replicationHealthErrors
                                        | where HostId == {4}
                                        | mvexpand HealthIssues
                                        | extend ErrorCode = tolong(HealthIssues.errorCode)
                                        | extend ErrorCreationTime = todatetime(HealthIssues.creationTime)
                                        //| summarize arg_max(PreciseTimeStamp,*) by HostId
                                        | join kind=leftouter enumToCodeTable on ErrorCode
                                        | extend OsName = """", OsVersion = """"
                                        |distinct ReplicationProvider,SourceVmName,HostId,ProtectionState,ReplicationHealth,SourceAgentVersion,OsName, OsFamily, OsVersion,ErrorCodeEnum";

        public static string query6 = @"let enumToCodeTable = cluster('asrclus1').database('ASRKustoDB').SrsErrorEnumToCode();
                                        SRSShoeboxEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where category == ""AzureSiteRecoveryReplicatedItems""
                                        | parse resourceId with *""/SUBSCRIPTIONS/""SubscriptionId""/RESOURCEGROUPS""*
                                        | extend SubscriptionId = tolower(SubscriptionId)
                                        | where SubscriptionId == {2}
                                        | extend x = parse_json(properties)
                                        | extend ReplicationProvider = tostring(x.replicationProviderName)
                                        | extend ReplicationProvider = case(ReplicationProvider  == ""InMageAzureV2"",""V2A"",
                                                                            ReplicationProvider  == ""InMage"",""V2A"",
                                                                            ReplicationProvider  == ""HyperVReplicaAzure"",""V2A"",
                                                                            ReplicationProvider)
                                        | extend ProtectionState=tostring(x.protectionState)
                                        | extend ReplicationHealth=tostring(x.replicationHealth)
                                        | where ReplicationHealth in (""Critical"",""Normal"")
                                        | extend HostId=tostring(x.id)
                                        | extend SourceAgentVersion=tostring(x.agentVersion)
                                        | extend SourceAgentVersion=todouble(extract(""([0-9]+[.][0-9]+)"",1, SourceAgentVersion))
                                        | extend SourceVmName=tostring(x.name)
                                        | extend OsFamily=tostring(x.osFamily)
                                        | extend PSHostId=tostring(x.processServerName)
                                        | extend LastHeartBeat=tostring(x.lastHeartbeat)
                                        | extend RpoInSecs=tostring(x.rpoInSeconds)
                                        | extend HealthIssues = x.replicationHealthErrors
                                        | mvexpand HealthIssues
                                        | extend ErrorCode = tolong(HealthIssues.errorCode)
                                        | extend ErrorCreationTime = todatetime(HealthIssues.creationTime)
                                       //| summarize arg_max(PreciseTimeStamp,*) by HostId
                                        | join kind=leftouter enumToCodeTable on ErrorCode
                                        | extend OsName = """", OsVersion = """"
                                        | distinct ReplicationProvider,SourceVmName,HostId,ProtectionState,ReplicationHealth,SourceAgentVersion,OsName, OsFamily, OsVersion,ErrorCodeEnum";

        public static string query7 = @"let enumToCodeTable = cluster('asrclus1').database('ASRKustoDB').SrsErrorEnumToCode();
                                        SRSShoeboxEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where category == ""AzureSiteRecoveryReplicatedItems""
                                        | parse resourceId with *""/SUBSCRIPTIONS/""SubscriptionId""/RESOURCEGROUPS""*
                                        | extend SubscriptionId = tolower(SubscriptionId)
                                        | where SubscriptionId == {2}
                                        | where SourceVmName == {3}
                                        | extend x = parse_json(properties)
                                        | extend ReplicationProvider = tostring(x.replicationProviderName)
                                        | extend ReplicationProvider = case(ReplicationProvider  == ""InMageAzureV2"",""V2A"",
                                                                            ReplicationProvider  == ""InMage"",""V2A"",
                                                                            ReplicationProvider  == ""HyperVReplicaAzure"",""V2A"",
                                                                            ReplicationProvider)
                                        | extend ProtectionState=tostring(x.protectionState)
                                        | extend ReplicationHealth=tostring(x.replicationHealth)
                                        | where ReplicationHealth in (""Critical"",""Normal"")
                                        | extend HostId=tostring(x.id)
                                        | extend SourceAgentVersion=tostring(x.agentVersion)
                                        | extend SourceAgentVersion=todouble(extract(""([0-9]+[.][0-9]+)"",1, SourceAgentVersion))
                                        | extend SourceVmName=tostring(x.name)
                                        | extend OsFamily=tostring(x.osFamily)
                                        | extend PSHostId=tostring(x.processServerName)
                                        | extend LastHeartBeat=tostring(x.lastHeartbeat)
                                        | extend RpoInSecs=tostring(x.rpoInSeconds)
                                        | extend HealthIssues = x.replicationHealthErrors
                                        | mvexpand HealthIssues
                                        | extend ErrorCode = tolong(HealthIssues.errorCode)
                                        | extend ErrorCreationTime = todatetime(HealthIssues.creationTime)
                                        //| summarize arg_max(PreciseTimeStamp,*) by HostId
                                        | join kind=leftouter enumToCodeTable on ErrorCode
                                        | extend OsName = """", OsVersion = """"
                                        | distinct ReplicationProvider,SourceVmName,HostId,ProtectionState,ReplicationHealth,SourceAgentVersion,OsName, OsFamily, OsVersion,ErrorCodeEnum";

        public static string query8 = @"let enumToCodeTable = cluster('asrclus1').database('ASRKustoDB').SrsErrorEnumToCode();
                                        SRSShoeboxEvent
                                        | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                        | where category == ""AzureSiteRecoveryReplicatedItems""
                                        | parse resourceId with *""/SUBSCRIPTIONS/""SubscriptionId""/RESOURCEGROUPS""*
                                        | extend SubscriptionId = tolower(SubscriptionId)
                                        | where SubscriptionId == {2}
                                        | extend x = parse_json(properties)
                                        | extend ReplicationProvider = tostring(x.replicationProviderName)
                                        | extend ReplicationProvider = case(ReplicationProvider  == ""InMageAzureV2"",""V2A"",
                                                                            ReplicationProvider  == ""InMage"",""V2A"",
                                                                            ReplicationProvider  == ""HyperVReplicaAzure"",""V2A"",
                                                                            ReplicationProvider)
                                        | extend ProtectionState=tostring(x.protectionState)
                                        | extend ReplicationHealth=tostring(x.replicationHealth)
                                        | where ReplicationHealth in (""Critical"",""Normal"")
                                        | extend HostId=tostring(x.id)
                                        | extend SourceAgentVersion=tostring(x.agentVersion)
                                        | extend SourceAgentVersion=todouble(extract(""([0-9]+[.][0-9]+)"",1, SourceAgentVersion))
                                        | extend SourceVmName=tostring(x.name)
                                        | extend OsFamily=tostring(x.osFamily)
                                        | extend PSHostId=tostring(x.processServerName)
                                        | extend LastHeartBeat=tostring(x.lastHeartbeat)
                                        | extend RpoInSecs=tostring(x.rpoInSeconds)
                                        | extend HealthIssues = x.replicationHealthErrors
                                        | where HostId == {3}
                                        | mvexpand HealthIssues
                                        | extend ErrorCode = tolong(HealthIssues.errorCode)
                                        | extend ErrorCreationTime = todatetime(HealthIssues.creationTime)
                                        //| summarize arg_max(PreciseTimeStamp,*) by HostId
                                        | join kind=leftouter enumToCodeTable on ErrorCode
                                        | extend OsName = """", OsVersion = """"
                                        | distinct ReplicationProvider,SourceVmName,HostId,ProtectionState,ReplicationHealth,SourceAgentVersion,OsName, OsFamily, OsVersion,ErrorCodeEnum";

        public static string InstallerConfiguratorLogs = @"GetInstallerAndConfiguratorLog({0},todatetime({1}),todatetime({2}))
                                                                | project-away TIMESTAMP";

        public static string GetOtherLogs = @"SRSDataEvent
                                              | where PreciseTimeStamp >= todatetime({0}) and PreciseTimeStamp <= todatetime({1})
                                              | where ClientRequestId == {2}
                                              | where Level <= 3
                                              | distinct Message";

        public static string GetDataSourceId = @"union cluster(""mabprod1"").database(""MABKustoProd1"").HvrDsTelemetryStats,
                                                 cluster(""mabprodweu"").database(""MABKustoProd"").HvrDsTelemetryStats,
                                                 cluster(""mabprodwus"").database(""MABKustoProd"").HvrDsTelemetryStats
                                                 | where TIMESTAMP >= todatetime({0}) and TIMESTAMP <= todatetime({1})
                                                 | where DsUniqueName == {2}                      
                                                 | distinct DataSourceId, DsUniqueName";

        public static string GetDsHealth = @"union (cluster('Asrcluscus').database('ASRKustoDB').GetDsHealth({0}, todatetime({1}), todatetime({2}))                                
                                            | distinct DiskId, SourceChurnRateMBps, ServiceProcessingRateMBps),
                                            (cluster('Asrclussea').database('ASRKustoDB').GetDsHealth({0}, todatetime({1}), todatetime({2}))                                  
                                            | distinct DiskId, SourceChurnRateMBps, ServiceProcessingRateMBps),
                                            (cluster('Asrcluswe').database('ASRKustoDB').GetDsHealth({0}, todatetime({1}), todatetime({2}))                               
                                            | distinct DiskId, SourceChurnRateMBps, ServiceProcessingRateMBps)";

        public static string GetDiskHealth = @"let SourceTable = union cluster(""mabprod1"").database(""MABKustoProd1"").HvrApplySyncSessionStats, 
                            cluster(""mabprodweu"").database(""MABKustoProd"").HvrApplySyncSessionStats,  
                            cluster(""mabprodwus"").database(""MABKustoProd"").HvrApplySyncSessionStats;
                            SourceTable
                            |where TIMESTAMP > todatetime({0}) and TIMESTAMP < todatetime({1})
                            |where DataSourceId == {2} and DiskId == {3}
                            |summarize
                                IncomingRateMBps = round((sum(DiffBlobSizeKB)/1024),2)/((max(SessionUploadEndTimeUtc) - min(SessionUploadStartTimeUtc))/time(1s))
                             by UploadHour = bin(SessionUploadEndTimeUtc,60m)
                            |join kind=inner(
                                SourceTable
                                |where TIMESTAMP > todatetime({0}) and TIMESTAMP < todatetime({1})
                                |where DataSourceId == {2} and DiskId == {3} 
                                |summarize
                                    ProcessingRateMBps = round((sum(DiffBlobSizeKB)/1024),2)/((max(SessionProcessingEndTimeUtc) - min(SessionProcessingStartTimeUtc))/time(1s)),
                                    ApplyRateMBps = round((sum(DiffBlobSizeKB)/1024),2)/sum(SessionTotalProcessingTimeSec),
                                    LastRPOInHrs = (max(SessionProcessingStartTimeUtc) - max(SessionUploadEndTimeUtc))/time(1h)
                                by ProcessedHour = bin(SessionProcessingEndTimeUtc,60m)
                            ) on $left.UploadHour == $right.ProcessedHour
                            | project HourlyTimeStamp = UploadHour, IncomingRateMBps , ApplyRateMBps, ProcessingRateMBps, LastRPOInHrs
                            | where HourlyTimeStamp >= todatetime({0}) and HourlyTimeStamp <= todatetime({1})";

        public static string GetDataSourceHealth =  @"union (cluster('Asrcluscus').database('ASRKustoDB').GetDsHealth({0}, todatetime({1}), todatetime({2}))),
                                            (cluster('Asrclussea').database('ASRKustoDB').GetDsHealth({0}, todatetime({1}), todatetime({1}))),
                                            (cluster('Asrcluswe').database('ASRKustoDB').GetDsHealth({0}, todatetime({1}), todatetime({2})))";

        public static string GetInMageMarkerArrivalInfo = @"union
                                                            (cluster(""mabprod1"").database(""MABKustoProd1"").GetInMageMarkerArrivalInfo({0}, todatetime({1}), todatetime({2}))),
                                                            (cluster(""mabprodweu"").database(""MABKustoProd"").GetInMageMarkerArrivalInfo({0}, todatetime({1}), todatetime({2}))),
                                                            (cluster(""mabprodwus"").database(""MABKustoProd"").GetInMageMarkerArrivalInfo({0}, todatetime({1}), todatetime({2})))";

        public static string HvrApplicationSyncStats = @" union cluster(""mabprod1"").database(""MABKustoProd1"").HvrApplySyncSessionStats, 
                            cluster(""mabprodweu"").database(""MABKustoProd"").HvrApplySyncSessionStats,  
                            cluster(""mabprodwus"").database(""MABKustoProd"").HvrApplySyncSessionStatsHvrApplySyncSessionStats | where DataSourceId == {0} and PreciseTimeStamp >= todatetime({1}) and PreciseTimeStamp <= todatetime({2}) | where IsRecoverable == 1 and MarkerId != ""null""| project PreciseTimeStamp, MarkerId, DiskId ";

        public static string GetLastMarkerId = @"union 
                                                    (cluster(""mabprod1"").database(""MABKustoProd1"").HvrApplySyncSessionStats
                                                    | where PreciseTimeStamp >= todatetime({1}) and PreciseTimeStamp <= todatetime({2})
                                                    | where DataSourceId  == {0}
                                                    | where IsRecoverable == 1 and MarkerId != ""null""
                                                    | summarize DiskCount = dcount(DiskId) by MarkerId
                                                    | where DiskCount == {3}
                                                    | project MarkerId
                                                    | join (
                                                    cluster(""mabprod1"").database(""MABKustoProd1"").HvrApplySyncSessionStats
                                                    | where PreciseTimeStamp >= todatetime({1}) and PreciseTimeStamp <= todatetime({2})
                                                    | where DataSourceId  == {0}
                                                    | where IsRecoverable == 1 and MarkerId != ""null""
                                                    | project MarkerId,PreciseTimeStamp
                                                    ) on MarkerId
                                                    | top 1 by PreciseTimeStamp
                                                    | project MarkerId),
                                                    (cluster(""mabprodweu"").database(""MABKustoProd"").HvrApplySyncSessionStats
                                                    | where PreciseTimeStamp >= todatetime({1}) and PreciseTimeStamp <= todatetime({2})
                                                    | where DataSourceId  == {0}
                                                    | where IsRecoverable == 1 and MarkerId != ""null""
                                                    | summarize DiskCount = dcount(DiskId) by MarkerId
                                                    | where DiskCount == {3}
                                                    | project MarkerId
                                                    | join (
                                                    cluster(""mabprodweu"").database(""MABKustoProd"").HvrApplySyncSessionStats
                                                    | where PreciseTimeStamp >= todatetime({1}) and PreciseTimeStamp <= todatetime({2})
                                                    | where DataSourceId  == {0}
                                                    | where IsRecoverable == 1 and MarkerId != ""null""
                                                    | project MarkerId,PreciseTimeStamp
                                                    ) on MarkerId
                                                    | top 1 by PreciseTimeStamp
                                                    | project MarkerId),
                                                    (cluster(""mabprodwus"").database(""MABKustoProd"").HvrApplySyncSessionStats
                                                    | where PreciseTimeStamp >= todatetime({1}) and PreciseTimeStamp <= todatetime({2})
                                                    | where DataSourceId  == {0}
                                                    | where IsRecoverable == 1 and MarkerId != ""null""
                                                    | summarize DiskCount = dcount(DiskId) by MarkerId
                                                    | where DiskCount == {3}
                                                    | project MarkerId
                                                    | join (
                                                    cluster(""mabprodwus"").database(""MABKustoProd"").HvrApplySyncSessionStats
                                                    | where PreciseTimeStamp >= todatetime({1}) and PreciseTimeStamp <= todatetime({2})
                                                    | where DataSourceId  == {0}
                                                    | where IsRecoverable == 1 and MarkerId != ""null""
                                                    | project MarkerId,PreciseTimeStamp
                                                    ) on MarkerId
                                                    | top 1 by PreciseTimeStamp
                                                    | project MarkerId)";

        public static string GetLastMarkerInfo = @"union 
                                                    (cluster(""mabprodwus"").database(""MABKustoProd"").HvrPITStats 
                                                    | where PreciseTimeStamp >= todatetime({1}) and PreciseTimeStamp <= todatetime({2}) and MarkerId == {0}
                                                    | project PreciseTimeStamp, DiskId, SessionId, SessionType, IsRecoverable, DsType, MarkerId, MarkerType),
                                                    (cluster(""mabprodweu"").database(""MABKustoProd"").HvrPITStats 
                                                    | where PreciseTimeStamp >= todatetime({1}) and PreciseTimeStamp <= todatetime({2}) and MarkerId == {0}
                                                    | project PreciseTimeStamp, DiskId, SessionId, SessionType, IsRecoverable, DsType, MarkerId, MarkerType),
                                                    (cluster(""mabprod1"").database(""MABKustoProd1"").HvrPITStats 
                                                    | where PreciseTimeStamp >= todatetime({1}) and PreciseTimeStamp <= todatetime({2}) and MarkerId == {0}
                                                    | project PreciseTimeStamp, DiskId, SessionId, SessionType, IsRecoverable, DsType, MarkerId, MarkerType)";

        public static string GetVacpTable = @"RcmAgentTelemetryVacpV2
                                            | where HostId == {0}
                                            | where PreciseTimeStamp >= todatetime({1}) and PreciseTimeStamp <= todatetime({2})
                                            | project PreciseTimeStamp , SrcLoggerTime , NumOfDiskInPolicy, NumOfDiskWithTag, TagTypeConf , TagTypeInsert , FinalStatus , Reason, Message";
    }
}
