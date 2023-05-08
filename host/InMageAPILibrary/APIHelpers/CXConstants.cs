using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;

namespace InMage.APIHelpers
{
    public class Constants
    {
        // Strings for Functions Requests
        public static readonly string FunctionGetRequestStatus = "GetRequestStatus"; 
        public static readonly string FunctionInfrastructureDiscovery = "InfrastructureDiscovery";
        public static readonly string FunctionCreateProtection = "CreateProtection";
        public static readonly string FunctionGetProtectionState = "GetProtectionState";
        public static readonly string FunctionListInfrastructure = "ListInfrastructure";
        public static readonly string FunctionListScoutComponents = "ListScoutComponents";
        public static readonly string FunctionSetPSNatIP = "SetPSNatIP";
        public static readonly string FunctionCreateRollback = "CreateRollback";
        public static readonly string FuntionGetRollbackState = "GetRollbackState";
        public static readonly string FuntionStopProtection = "StopProtection";
        public static readonly string FunctionListEvents = "ListEvents";
        public static readonly string FunctionCleanRollbackPlans = "CleanRollbackPlans";
        public static readonly string FunctionRemoveInfrastructure = "RemoveInfrastructure";
        public static readonly string FunctionInstallMobilityService = "InstallMobilityService";
        public static readonly string FunctionUnInstallMobilityService = "UnInstallMobilityService";
        public static readonly string FunctionListConsistencyPoints = "ListConsistencyPoints";
        public static readonly string FunctionModifyProtectionProperties = "ModifyProtectionProperties";
        public static readonly string FunctionModifyProtection = "ModifyProtection";
        public static readonly string FunctionProtectionReadinessCheck = "ProtectionReadinessCheck";
        public static readonly string FunctionRollbackReadinessCheck = "RollbackReadinessCheck";
        public static readonly string FunctionRestartResync = "RestartResync";
        public static readonly string FunctionGetCSPSHealth = "GetCSPSHealth";
        public static readonly string FunctionListPlans = "ListPlans";
        public static readonly string FunctionGetProtectionDetails = "GetProtectionDetails";
        public static readonly string FunctionSetVPNDetails = "SetVPNDetails";
        public static readonly string FunctionPSFailover = "PSFailover";        
        public static readonly string FunctionCreateAccount = "CreateAccount";
        public static readonly string FunctionUpdateAccount = "UpdateAccount";
        public static readonly string FunctionRemoveAccount = "RemoveAccount";
        public static readonly string FunctionListAccounts = "ListAccounts";
        public static readonly string FunctionModifyProtectionV2 = "ModifyProtectionV2";
        public static readonly string FunctionModifyProtectionPropertiesV2 = "ModifyProtectionPropertiesV2";
        public static readonly string FunctionCreateProtectionV2 = "CreateProtectionV2";         

        // Other strings
        public static readonly string KeyRequestId = "RequestId";
        public static readonly string SuccessReturnCode = "0";

        public const int UndefinedReturnCode = -11;
    }

    public enum CXProtocol
    {
        HTTP,
        HTTPS,
    }

    public enum InfrastructureSource
    {
        Primary,
        Recovery,
    }

    public enum SolutionType
    {
        Unknown,
        Azure,
        ESX,
        Application,
        Bulk,
        Other
    }

    public enum InfrastructureType
    {
        Unknown,
        Physical,
        vCenter,
        vSphere,
        vCloud,
        AWS
    }

    public enum OperationStatus
    {
        Unknown,
        Completed,
        Success,
        Failed,
        InProgress,
        Pending,
    }

    public enum ProtectionStatus
    {
        Unknown,
        InitialSync,
        DifferentialSync,
        ResyncStepI,
        ResyncStepII,
    }

    public enum ProtectionComponents
    {
        None,
        Source,
        Target,
        Both
    }

    public enum ScoutComponentType
    {
        All,
        ConfigurationServer,
        ProcessServer,
        MasterTarget,
        ScoutAgent
    }

    public enum OSType
    {
        Unknown,
        Windows,
        Linux
    }

    public enum MachineType
    {
        Unknown,
        Physical,
        Virtual
    }

    public enum RecoveryPoint
    {
        LatestTag,
        LatestTime,
        Custom
    }

    public enum EventSeverity
    {
        All,
        Error,
        Warning,
        Notice
    }

    public enum ConsistencyTagAccuracy
    {
        Exact,
        Approximate,
        NotGuaranteed
    }

    public enum HealthState
    {
        Unknown,
        Normal,
        Warning,
        Critical,
        None
    }

    public enum ServiceStatus
    {
        Unknown,
        Running,
        Warning,
        Stopped
    }

    public enum PlanType
    {
        Unknown,
        Protection,
        Rollback
    }

    public enum HealthFactorType
    {
        Unknown,        
        RPO,
        Retention,
        Protection,
        Consistency
    }

    public enum Criticality
    {
        Unknown,
        Warning,
        Critical        
    }

    public enum ConnectivityType
    {
        Unknown,
        VPN,
        NonVPN
    }

    public enum ProtectionEnvironment
    {
        Unknown,
        E2A,
        A2E
    }

    public enum FailoverType
    {
        Unknown,
        SystemLevel,
        ServerLevel
    }
}
