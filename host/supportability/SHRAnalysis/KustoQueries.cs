namespace Microsoft.Internal.SiteRecovery.SHRAnalysis
{
    class SHRQuery {
        public static string SHRErrorQuery = @"GetASRErrorInfo({0}, {1}, {2}, {3})
                        | where OperationName in (""EnableProtectionWorkflow"",""CompleteInitialReplicationWorkflow"")
                        | where ErrorGroup != ""CX""
                        | where ErrorType != ""ARMError""
                        | where ErrorName !in (""VirtualMachineNotFound"", ""VirtualMachinePoweredOff"", ""BlobNotAccessible"")
                        | where Scenario == ""A2A"" 
                        | where StampName !contains ""ecy-pod01-srs1""
                        | summarize arg_max(PreciseTimeStamp, *) by ClientRequestId, 
                                    ErrorName, 
                                    ErrorType,
                                    ErrorGroup,
                                    StampName,
                                    SubscriptionId,
                                    ObjectId
                        | distinct  ClientRequestId, 
                                    PreciseTimeStamp, 
                                    ErrorName, 
                                    ErrorType,
                                    ErrorGroup,
                                    StampName,
                                    SubscriptionId,
                                    ObjectId
                                    | join kind=leftouter (SRSDataEvent
                                    | where PreciseTimeStamp between({2}..{3})
| where StampName !startswith (""ccy"") and StampName !contains ""-can01""  and StampName !contains ""onebox"" and StampName !startswith ""ecy"" and StampName !contains ""bvtd2"" and isnotempty(StampName)
                                    | where ComponentName == ""MobilityServiceInstallationExtensions.cs""
                                    |where Message contains ""Check for unsupported OS""
                                    | extend OSType = extract(""osType = ([^,]+)"", 1, Message) 
                                    | extend OSName = extract(""osName = ([^,]+)"", 1, Message)
                                    | extend OSVersion = extract(""osVersion = ([^\n]+)"", 1, Message)
                                    | summarize make_set_if(OSType, OSType != """"), make_set_if(OSName, OSName != """"), make_set_if(OSVersion, OSVersion != """") by ClientRequestId
                                    | mv-expand set_OSName, set_OSType, set_OSVersion
                                    | extend OSName = tostring(set_OSName)
                                    | extend OSVersion= tostring(set_OSVersion)
                                    | extend OSType = tostring(set_OSType)
                                    | distinct ClientRequestId, OSType, OSName, OSVersion  
                                    ) on ClientRequestId
                                    | join kind=leftouter (SRSDataEvent
                                    | where PreciseTimeStamp between({2}..{3})
                                    | where StampName !startswith (""ccy"") and StampName !contains ""-can01""  and StampName !contains ""onebox"" and StampName !startswith ""ecy"" and StampName !contains ""bvtd2"" and isnotempty(StampName)
                                    | where ComponentName == ""A2ASiteRecoveryExtensionBaseTask.cs""
                                    | where Message contains ""Microsoft-ASR_UA_""
                                    | extend AgentVersion1 = extract(""Microsoft-ASR_UA_([0-9]+[.][0-9]+)"", 1, Message)
                                    | where AgentVersion1 != """"
                                    | summarize AgentVersion = max(AgentVersion1) by ClientRequestId
                                    | distinct ClientRequestId, AgentVersion
                                    ) on ClientRequestId
                                    | distinct ClientRequestId,
                                                PreciseTimeStamp,
                                                ErrorName,
                                                ErrorType,
                                                ErrorGroup,
                                                StampName,
                                                SubscriptionId,
                                                ObjectId,
                                                OSType,
                                                AgentVersion,
                                                OSName,
                                                OSVersion
                ";

        public static string lastRequest = @"
                        | join kind=leftanti (
                        SRSOperationEvent
                        | where PreciseTimeStamp between ({0}..now())
| where StampName !startswith (""ccy"") and StampName !contains ""-can01""  and StampName !contains ""onebox"" and StampName !startswith ""ecy"" and StampName !contains ""bvtd2"" and isnotempty(StampName)
| where SRSOperationName == ""EnableProtectionWorkflow"" or SRSOperationName == ""CompleteInitialReplicationWorkflow""
                        | where State != ""Restart"" and State != ""Start""
                        | summarize arg_max(PreciseTimeStamp, *) by ObjectId
                        | where State == ""Succeeded""
                        | distinct ObjectId, ScenarioName, State
                        )
                        on ObjectId
                        | summarize arg_max(PreciseTimeStamp, *) by ObjectId 
                                    | distinct ClientRequestId,
                                                PreciseTimeStamp,
                                                ErrorName,
                                                ErrorType,
                                                ErrorGroup,
                                                StampName,
                                                SubscriptionId,
                                                ObjectId,
                                                OSType,
                                                AgentVersion,
                                                OSName,
                                                OSVersion
                        ";

        public static string query3 = @"GetASROperationGroup()
                        | distinct OperationName, OperationGroup";
        };

}