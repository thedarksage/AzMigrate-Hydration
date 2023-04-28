using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using RcmContract;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi
{
    public enum InfraHealthFactors
    {
        [HealthFactor(PlaceHoldersNeeded = true)]
        CpuWarning,
        [HealthFactor(PlaceHoldersNeeded = true)]
        CpuCritical,
        [HealthFactor(PlaceHoldersNeeded = true)]
        MemoryWarning,
        [HealthFactor(PlaceHoldersNeeded = true)]
        MemoryCritical,
        [HealthFactor(PlaceHoldersNeeded = true)]
        DiskWarning,
        [HealthFactor(PlaceHoldersNeeded = true)]
        DiskCritical,
        [HealthFactor(PlaceHoldersNeeded = true)]
        ServicesNotRunning,
        [HealthFactor(PlaceHoldersNeeded = true)]
        DataUploadWarning,
        [HealthFactor(PlaceHoldersNeeded = true)]
        DataUploadCritical,
        [HealthFactor(PlaceHoldersNeeded = true)]
        //Todo:[Himanshu] create a separate enum for events if needed.
        CumulativeThrottle,
        [HealthFactor(PlaceHoldersNeeded = true)]
        PSReboot,
        [HealthFactor(PlaceHoldersNeeded = true)]
        PSVersionUpgrade
    }

    /// <summary>
    /// Device Type defines the interface type
    /// </summary>
    public enum DeviceType
    {
        /// <summary>
        /// Normal Interfaces
        /// </summary>
        Normal = 1,

        /// <summary>
        /// Bonding Masters
        /// </summary>
        Masters = 2,

        /// <summary>
        /// Bonding Secondary
        /// </summary>
        Secondary = 3
    }

    public enum OSFlag
    {
        Windows = 1,

        Linux = 2
    }

    public enum LogRotPolicyType
    {
        Unknown = -1,
        TimeLimitLogRotPolicy = 0,  // Time based log rotation
        SizeLimitLogRotPolicy = 1,  // Space based log rotation
        TimeAndSizeLimitLogRotPolicy = 2 // Both time and space based log rotation
    }

    public static class HealthUtils
    {
        public const int HealthFactorWarning = 1, HealthFactorCritical = 2;

        public static readonly IReadOnlyDictionary<InfraHealthFactors, Tuple<string, int>> InfraHealthFactorStrings
        = new Dictionary<InfraHealthFactors, Tuple<string, int>>()
            {
                    {InfraHealthFactors.CpuCritical, new Tuple<string, int>("EPH0003", HealthFactorCritical)},
                    {InfraHealthFactors.CpuWarning, new Tuple<string, int>("EPH0004", HealthFactorWarning)},
                    {InfraHealthFactors.MemoryCritical, new Tuple<string, int>("EPH0005", HealthFactorCritical)},
                    {InfraHealthFactors.MemoryWarning, new Tuple<string, int>("EPH0006", HealthFactorWarning)},
                    {InfraHealthFactors.DiskCritical, new Tuple<string, int>("EPH0007", HealthFactorCritical)},
                    {InfraHealthFactors.DiskWarning, new Tuple<string, int>("EPH0008", HealthFactorWarning)},
                    {InfraHealthFactors.CumulativeThrottle, new Tuple<string, int>("ECH00020", HealthFactorCritical)},
                    {InfraHealthFactors.ServicesNotRunning, new Tuple<string, int>("EPH0009", HealthFactorCritical)},
                    {InfraHealthFactors.DataUploadWarning, new Tuple<string, int>("EPH0010", HealthFactorWarning)},
                    {InfraHealthFactors.DataUploadCritical, new Tuple<string, int>("EPH0011", HealthFactorCritical)}
            };

        public static readonly IReadOnlyDictionary<InfraHealthFactors, Tuple<string, int>> RCMInfraHealthFactorStrings
        = new Dictionary<InfraHealthFactors, Tuple<string, int>>()
            {
                    {InfraHealthFactors.CpuCritical, new Tuple<string, int>("CPUUtilizationCritical", HealthFactorCritical)},
                    {InfraHealthFactors.CpuWarning, new Tuple<string, int>("CPUUtilizationWarning", HealthFactorWarning)},
                    {InfraHealthFactors.MemoryCritical, new Tuple<string, int>("MemoryUsageCritical", HealthFactorCritical)},
                    {InfraHealthFactors.MemoryWarning, new Tuple<string, int>("MemoryUsageWarning", HealthFactorWarning)},
                    {InfraHealthFactors.DiskCritical, new Tuple<string, int>("FreeSpaceCritical", HealthFactorCritical)},
                    {InfraHealthFactors.DiskWarning, new Tuple<string, int>("FreeSpaceWarning", HealthFactorWarning)},
                    {InfraHealthFactors.ServicesNotRunning, new Tuple<string, int>("ServicesNotRunning", HealthFactorCritical)},
                    {InfraHealthFactors.DataUploadWarning, new Tuple<string, int>("ThroughputWarning", HealthFactorWarning)},
                    {InfraHealthFactors.DataUploadCritical, new Tuple<string, int>("ThroughputError", HealthFactorCritical)}
            };
    }

    [AttributeUsage(AttributeTargets.Field, Inherited = false, AllowMultiple = false)]
    public sealed class HealthFactorAttribute : Attribute
    {
        public bool PlaceHoldersNeeded { get; set; }
    }

    [AttributeUsage(AttributeTargets.Class, Inherited = true, AllowMultiple = true)]
    public sealed class InfraHealthFactorPlaceholdersAttribute : Attribute
    {
        public InfraHealthFactors InfraHealthFactor { get; }

        public InfraHealthFactorPlaceholdersAttribute(InfraHealthFactors infraHealthFactor)
        {
            InfraHealthFactor = infraHealthFactor;
        }
    }

    [InfraHealthFactorPlaceholders(InfraHealthFactors.CpuCritical)]
    [InfraHealthFactorPlaceholders(InfraHealthFactors.CpuWarning)]
    [Serializable]
    public class CpuHealthPlaceholders
    {
        public int HealthMonitorInterval;

        public int CPUUtilization;
    }

    [InfraHealthFactorPlaceholders(InfraHealthFactors.MemoryCritical)]
    [InfraHealthFactorPlaceholders(InfraHealthFactors.MemoryWarning)]
    [Serializable]
    public class MemoryHealthPlaceholders
    {
        public int HealthMonitorInterval;

        public double UsedMemoryPercent;
    }

    [InfraHealthFactorPlaceholders(InfraHealthFactors.DiskCritical)]
    [InfraHealthFactorPlaceholders(InfraHealthFactors.DiskWarning)]
    [Serializable]
    public class DiskHealthPlaceholders
    {
        public int HealthMonitorInterval;

        public double FreeSpace;

        public double FressSpacePercent;
    }

    [InfraHealthFactorPlaceholders(InfraHealthFactors.CumulativeThrottle)]
    [Serializable]
    public class CumulativeThrottlePlaceholders
    {
        public int HealthMonitorInterval;

        public double FreeSpace;

        public double FreeSpacePercent;
    }

    [InfraHealthFactorPlaceholders(InfraHealthFactors.PSVersionUpgrade)]
    [Serializable]
    public class PSVersionUpgradePlaceholders
    {
        public string PreviousVersion;

        public string PSVersion;

        public string HostName;

    }

    [InfraHealthFactorPlaceholders(InfraHealthFactors.PSReboot)]
    [Serializable]
    public class PSRebootPlaceholders
    {
        public string SystemBootTime;

        public string HostName;
    }

    [InfraHealthFactorPlaceholders(InfraHealthFactors.PSVersionUpgrade)]
    [Serializable]
    public class V2APSVersionUpgradePlaceholders
    {
        public string PSVersion;

        public string IPAddress;

        public string HostName;
    }

    [InfraHealthFactorPlaceholders(InfraHealthFactors.PSReboot)]
    [Serializable]
    public class V2APSRebootPlaceholders
    {
        public string SystemBootTime;

        public string HostName;

        public string IPAddress;
    }

    [InfraHealthFactorPlaceholders(InfraHealthFactors.ServicesNotRunning)]
    [Serializable]
    public class ServiceNotRunningPlaceholders
    {
        public string Services;

        public string ProcessServer;
    }

    [InfraHealthFactorPlaceholders(InfraHealthFactors.DataUploadCritical)]
    [InfraHealthFactorPlaceholders(InfraHealthFactors.DataUploadWarning)]
    [Serializable]
    public class DataUploadHealthPlaceholders
    {
        public double HealthMonitorInterval;

        public string ActualThroughput;

        public string ExpectedThroughput;

        public string UploadPendingData;

        public string UploadedData;
    }

    // TODO-SanKumar-1903: This should be made an interface and the actual
    // implementation of placeholder checking should go into Impl.cs file
    public class InfraHealthFactorItem
    {
        public InfraHealthFactors InfraHealthFactor { get; }

        public bool Set { get; }

        public object Placeholders { get; }

        public InfraHealthFactorItem(
            InfraHealthFactors infraHealthFactor, bool set, object placeholders)
        {
            if (set)
            {
                var healthFactorType = typeof(InfraHealthFactors).GetMember(
                    Enum.GetName(typeof(InfraHealthFactors), infraHealthFactor));

                HealthFactorAttribute hfAttr = healthFactorType.Single()
                    .GetCustomAttribute<HealthFactorAttribute>(inherit: false);

                if ((hfAttr != null && hfAttr.PlaceHoldersNeeded) != (placeholders != null))
                {
                    throw new ArgumentException(
                        string.Format(CultureInfo.InvariantCulture,
                            "placeholders object is {0}null, when {1} {2} placeholders",
                            placeholders == null ? string.Empty : "non-",
                            infraHealthFactor,
                            placeholders == null ? "expects" : "doesn't expect"),
                        nameof(placeholders));
                }

                if (placeholders != null)
                {
                    var placeholdersAttributes = placeholders.GetType()
                        .GetCustomAttributes<InfraHealthFactorPlaceholdersAttribute>(inherit: true);

                    if (placeholdersAttributes == null || !placeholdersAttributes.Any())
                    {
                        throw new ArgumentException(
                            "placeholders object must be of type that has InfraHealthFactorPlaceHolder attribute",
                            nameof(placeholders));
                    }

                    if (!placeholdersAttributes.Any(
                            currAttr => currAttr.InfraHealthFactor == infraHealthFactor))
                    {
                        throw new ArgumentException(
                            $"placeholders object is not meant for the infraHealthFactor: {infraHealthFactor}",
                            nameof(placeholders));
                    }
                }
            }
            else
            {
                if (placeholders != null)
                {
                    throw new ArgumentException(
                        "placeholders must not be passed, while resetting infra health factor",
                        nameof(placeholders));
                }
            }

            this.InfraHealthFactor = infraHealthFactor;
            this.Set = set;
            this.Placeholders = placeholders;
        }
    }

    /// <summary>
    /// Parameters required for jobId update call
    /// </summary>
    public class JobIdUpdateItem
    {
        // Source host id of the pair
        public string srcHostId;

        // Destination host id for the pair
        public string destHostId;

        // Formatted source disk id for the pair
        public string srcHostVolume;

        // Formatted target diskid for the pair
        public string destHostVolume;

        // Tmid of the pair
        public long tmid;

        // Restart resync counter value for the pair
        public long restartResyncCounter;

        public bool IsValid()
        {
            return (!string.IsNullOrWhiteSpace(srcHostId) &&
                !string.IsNullOrWhiteSpace(destHostId) &&
                !string.IsNullOrWhiteSpace(srcHostVolume) &&
                !string.IsNullOrWhiteSpace(destHostVolume) &&
                tmid > 0 && restartResyncCounter >= 0);
        }
    }

    public enum StatisticStatus
    {
        // NOTE-SanKumar-1904: These values are provided based on the CS contract.
        // So, don't change the mappings of existing values, otherwise provide
        // correct mapping inside the CS Api wrapper.
        Healthy = 1,
        Warning = 2,
        Critical = 3,
        Unknown = 4
    }

    public class ProcessServerStatistics
    {
        public (long ProcessorQueueLength, StatisticStatus Status) SystemLoad;
        public (decimal Percentage, StatisticStatus Status) CpuLoad;
        public (long Total, long Used, StatisticStatus Status) MemoryUsage;
        public (long Total, long Used, StatisticStatus Status) InstallVolumeSpace;
        public (long UploadPendingData, long ThroughputBytesPerSec, StatisticStatus Status) Throughput;
        public List<(string ServiceName, int NumberOfInstances)> Services;

        // NOTE-SanKumar-1904: On future addition of members, ensure to update
        // DeepCopy(), if necessary.

        public virtual ProcessServerStatistics DeepCopy()
        {
            var copied = (ProcessServerStatistics)this.MemberwiseClone();

            // ValueTuples are value types. They are cloned as part of the above shallow copy.

            if (this.Services != null)
                copied.Services = new List<(string ServiceName, int NumberOfInstances)>(this.Services);

            return copied;
        }
    }

    public class HostInfo
    {
        public string AccountId;

        public string PSInstallTime;

        public int CSEnabled;

        public string HypervisorName;

        public int SslPort;

        public string PSPatchVersion;

        public int ProcessServerEnabled;

        public string PatchInstallTime;

        public string OSType;

        public string HostId;

        public int HypervisorState;

        public string HostName;

        public string PSVersion;

        public int Port;

        public IPAddress IPAddress;

        public string AccountType;

        public int OSFlag;
    }

    public class NicInfo
    {
        public string Dns;

        public string NicSpeed;

        public string Gateway;

        public List<IPAddress> IpAddresses;

        public DeviceType DeviceType;

        public string DeviceName;

        public string HostId;

        public List<NetMask> NetMask;
    }

    public class NetMask
    {
        public string NicIPAddress;

        public string SubNetMask;
    }

    public interface IStopReplicationInput
    {
        string SourceHostId { get; }

        string SourceDeviceName { get; }

        string DestinationHostId { get; }

        string DestinationDeviceName { get; }
    }

    public class StopReplicationInput : IStopReplicationInput
    {
        public StopReplicationInput(
            string sourceHostId,
            string sourceDeviceName,
            string destinationHostId,
            string destinationDeviceName)
        {
            string GetParamValue(string paramVal, string paramName)
            {
                if (string.IsNullOrWhiteSpace(paramVal))
                {
                    throw new ArgumentNullException(paramName);
                }

                return paramVal;
            }

            this.SourceHostId = GetParamValue(sourceHostId, "SourceHostId");
            this.SourceDeviceName = GetParamValue(sourceDeviceName, "SourceDeviceName");
            this.DestinationHostId = GetParamValue(destinationHostId, "DestinationHostId");
            this.DestinationDeviceName = GetParamValue(destinationDeviceName, "DestinationDeviceName");
            this.StopReplicationPHPInput = new Dictionary<string, string>()
            {
                ["sourceHostId"] = this.SourceHostId,
                ["sourceDeviceName"] = this.SourceDeviceName,
                ["destinationHostId"] = this.DestinationHostId,
                ["destinationDeviceName"] = this.DestinationDeviceName,
            };
        }

        public string SourceHostId { get; }

        public string SourceDeviceName { get; }

        public string DestinationHostId { get; }

        public string DestinationDeviceName { get; }

        internal IReadOnlyDictionary<string, string> StopReplicationPHPInput { get; }
    }

    public interface IPSHealthEvent
    {
        string ErrorCode { get; }

        string ErrorSummary { get; }

        string ErrorMessage { get; }

        object ErrorPlaceHolders { get; }

        string ErrorTemplateId { get; }

        string HostId { get; }

        string ErrorId { get; }
    }

    public class PSHealthEvent : IPSHealthEvent
    {
        public PSHealthEvent(
            string errorCode,
            string errorSummary,
            string errorMessage,
            object errorPlaceHolders,
            string errorTemplateId,
            string hostId,
            string errorId)
        {
            string GetStringParamValue(string paramVal, string paramName)
            {
                if (string.IsNullOrWhiteSpace(paramVal))
                {
                    throw new ArgumentNullException(paramName);
                }

                return paramVal;
            }

            object GetPlaceholders(object placeholders)
            {
                // TODO : Add more validations for placeholders
                if (placeholders == null)
                {
                    throw new ArgumentNullException(nameof(placeholders));
                }

                return placeholders;
            }

            this.ErrorCode = GetStringParamValue(errorCode, "ErrorCode");

            this.ErrorSummary = GetStringParamValue(errorSummary, "ErrorSummary");

            this.ErrorMessage = GetStringParamValue(errorMessage, "ErrorMessage");

            this.ErrorPlaceHolders = GetPlaceholders(errorPlaceHolders);

            this.ErrorTemplateId = GetStringParamValue(errorTemplateId, "ErrorTemplateId");

            this.HostId = GetStringParamValue(hostId, "HostId");

            this.ErrorId = GetStringParamValue(errorId, "ErrorId");

            this.PHPSerializedEventInput = MiscUtils.SerializeObjectToPHPFormat(
                new Dictionary<string, object>
                {
                    ["err_code"] = ErrorCode,
                    ["summary"] = ErrorSummary,
                    ["err_msg"] = ErrorMessage,
                    ["err_placeholders"] =
                    MiscUtils.PlaceholderToDictionary(ErrorPlaceHolders),
                    ["err_temp_id"] = ErrorTemplateId,
                    ["id"] = HostId,
                    ["error_id"] = ErrorId
                });
        }

        public string ErrorCode { get; }

        public string ErrorSummary { get; }

        public string ErrorMessage { get; }

        public object ErrorPlaceHolders { get; }

        public string ErrorTemplateId { get; }

        public string HostId { get; }

        public string ErrorId { get; }

        internal string PHPSerializedEventInput { get; }
    }

    public class DataUploadBlockedUpdateItem
    {
        public IDictionary<int, int> UploadStatusForRepItems { get; }

        public DataUploadBlockedUpdateItem(IDictionary<int, int> dataUploadStatus)
        {
            UploadStatusForRepItems = dataUploadStatus;
        }

        public bool IsValid()
        {
            if (!UploadStatusForRepItems.Any())
                return false;
            return true;
        }
    }

    public class IRDRFileStuckHealthErrorCodes
    {
        public const string IRDRStuckCriticalCode = "EC0129";
        public const string IRDRStuckWarningCode = "EC0127";
    }

    public class RCMIRDRFileStuckHealthErrorCodes
    {
        public const string IRDRStuckCriticalCode = "IRDRStuckCritical";
        public const string IRDRStuckWarningCode = "IRDRStuckWarning";
    }

    public enum IRDRStuckHealthFactor
    {
        Healthy = 0,
        Warning = 1,
        Critical = 2
    }

    public class IRDRStuckHealthEvent
    {
        public string ErrorCode { get; }

        public string HealthFactorCode { get; }

        public string HealthFactor { get; }

        public string Priority { get; }

        public string HealthDescription { get; }

        public IRDRStuckHealthFactor Health { get; }

        public IProcessServerPairSettings Pair { get; }

        public IRDRStuckHealthEvent(
            string errorCode,
            string healthFactorCode,
            string healthFactor,
            string priority,
            string healthDescription,
            IRDRStuckHealthFactor health,
            IProcessServerPairSettings pair)
        {
            this.ErrorCode = errorCode;
            this.HealthFactorCode = healthFactorCode;
            this.HealthFactor = healthFactor;
            this.Priority = priority;
            this.HealthDescription = healthDescription;
            this.Health = health;
            this.Pair = pair;
        }

        public bool IsValid()
        {
            if (!Enum.IsDefined(typeof(IRDRStuckHealthFactor), this.Health))
                return false;

            if (this.Pair == null)
                return false;

            if (this.Health == IRDRStuckHealthFactor.Critical ||
                this.Health == IRDRStuckHealthFactor.Warning)
            {
                return
                    !string.IsNullOrWhiteSpace(this.ErrorCode) &&
                    !string.IsNullOrWhiteSpace(this.HealthFactorCode) &&
                    !string.IsNullOrWhiteSpace(this.HealthFactor) &&
                    !string.IsNullOrWhiteSpace(this.Priority) &&
                    !string.IsNullOrWhiteSpace(this.HealthDescription);
            }

            return true;
        }
    }

    public class RenewCertsPolicyInfo
    {
        public int PolicyId;

        public PolicyState PolicyState;

        public RenewCertsStatus renewCertsStatus;

        public bool IsValid()
        {
            if (!Enum.IsDefined(typeof(PolicyState), PolicyState) || PolicyId == 0)
                return false;

            if (renewCertsStatus != null)
            {
                return
                    !string.IsNullOrWhiteSpace(renewCertsStatus.ErrorCode) &&
                    !string.IsNullOrWhiteSpace(renewCertsStatus.ErrorDescription) &&
                    !string.IsNullOrWhiteSpace(renewCertsStatus.LogMessage) &&
                    renewCertsStatus.PlaceHolders != null;
            }

            return true;
        }
    }

    public class RenewCertsStatus
    {
        // Error Code encountered while renewing PS certificates
        public string ErrorCode;

        // PS Renew Certs error description
        public string ErrorDescription;

        // Indicates the PS renew certificates error message
        public string LogMessage;

        // Placeholders for the renew certs error
        public object PlaceHolders;

        // Indicates the status of the Renew PS Certificates policy
        public bool Status;
    }

    public class LogRotationInput
    {
        // Name of the log file
        public string LogName;

        // Log Rotation start time in seconds elapsed since epoch
        // StartTime is epoch of current time on CS + LogTimeLimit
        public long StartEpochTime;

        // Indicates LogRotationPolicyType (Space based/Time based/Space+Time based)
        public LogRotPolicyType PolicyType;

        // Log Size limit in MB, beyond which log rotation happens
        public int LogSizeLimit;

        // Log Time Limit in days, beyond which log rotation happens
        public int LogTimeLimit;

        // Current time on CS in seconds elapsed since epoch
        public long CurrentEpochTime;
    }

    public interface IProcessServerCSApiStubs : IDisposable
    {
        void Close();

        Task ReportInfraHealthFactorsAsync(IEnumerable<InfraHealthFactorItem> infraHealthFactors);

        Task ReportInfraHealthFactorsAsync(
            IEnumerable<InfraHealthFactorItem> infraHealthFactors,
            CancellationToken cancellationToken);

        Task<ProcessServerSettings> GetPSSettings();

        Task<ProcessServerSettings> GetPSSettings(CancellationToken cancellationToken);

        Task ReportStatisticsAsync(ProcessServerStatistics stats);

        Task ReportStatisticsAsync(ProcessServerStatistics stats, CancellationToken cancellationToken);

        Task ReportPSEventsAsync(ApplianceHealthEventMsg psEvents, CancellationToken token);

        Task ReportHostHealthAsync(bool IsPrivateEndpointEnabled, IProcessServerHostSettings host, ProcessServerProtectionPairHealthMsg hostHealth, CancellationToken cancellationToken);

        Task UpdateJobIdAsync(JobIdUpdateItem jobIdUpdateItem);

        Task UpdateJobIdAsync(JobIdUpdateItem jobIdUpdateItem, CancellationToken cancellationToken);

        Task<string> GetPsConfigurationAsync(string pairid);

        Task<string> GetPsConfigurationAsync(string pairid, CancellationToken cancellationToken);

        Task<string> GetRollbackStatusAsync(string sourceid);

        Task<string> GetRollbackStatusAsync(string sourceid, CancellationToken cancellationToken);

        Task SetResyncFlagAsync(string hostId, string deviceName, string destHostId, string destDeviceName,
            string resyncReasonCode, string detectionTime, string hardlinkFromFile, string hardlinkToFile);

        Task SetResyncFlagAsync(string hostId, string deviceName, string destHostId, string destDeviceName,
            string resyncReasonCode, string detectionTime, string hardlinkFromFile, string hardlinkToFile, CancellationToken cancellationToken);

        Task RegisterPSAsync(
            HostInfo hostInfo,
            Dictionary<string, NicInfo> networkInfo);

        Task RegisterPSAsync(
            HostInfo hostInfo,
            Dictionary<string, NicInfo> networkInfo,
            CancellationToken cancellationToken);

        Task StopReplicationAtPSAsync(IStopReplicationInput stopReplicationInput);

        Task StopReplicationAtPSAsync(
            IStopReplicationInput stopReplicationInput,
            CancellationToken cancellationToken);

        Task ReportPSEventsAsync(IPSHealthEvent psHealthEvent);

        Task ReportPSEventsAsync(
            IPSHealthEvent psHealthEvent,
            CancellationToken cancellationToken);

        Task UpdateReplicationStatusForDeleteReplicationAsync(string pairId);

        Task UpdateReplicationStatusForDeleteReplicationAsync(string pairId, CancellationToken ct);

        Task UpdateCumulativeThrottleAsync(bool isThrottled, SystemHealth systemHealth);

        Task UpdateCumulativeThrottleAsync(bool isThrottled, SystemHealth systemHealth, CancellationToken ct);

        Task UpdateCertExpiryInfoAsync(
            Dictionary<string, long> certExpiryInfo);

        Task UpdateCertExpiryInfoAsync(
            Dictionary<string, long> certExpiryInfo,
            CancellationToken ct);

        Task ReportDataUploadBlockedAsync(DataUploadBlockedUpdateItem dataUploadBlockedUpdateItem);

        Task ReportDataUploadBlockedAsync(DataUploadBlockedUpdateItem dataUploadBlockedUpdateItem, CancellationToken ct);

        Task UpdateIRDRStuckHealthAsync(List<IRDRStuckHealthEvent> iRDRStuckHealthEventList);

        Task UpdateIRDRStuckHealthAsync(List<IRDRStuckHealthEvent> iRDRStuckHealthEventList, CancellationToken ct);

        Task UpdateRPOAsync(Dictionary<IProcessServerPairSettings, TimeSpan> RPOInfo);

        Task UpdateRPOAsync(Dictionary<IProcessServerPairSettings, TimeSpan> RPOInfo, CancellationToken ct);

        Task UpdateExecutionStateAsync(string pairId, ExecutionState executionState);

        Task UpdateExecutionStateAsync(string pairId, ExecutionState executionState, CancellationToken ct);

        Task PauseReplicationAsync(string pairId);

        Task PauseReplicationAsync(string pairId, CancellationToken ct);

        Task UpdateLogRotationInfoAsync(string logName, long logRotTriggerTime);

        Task UpdateLogRotationInfoAsync(string logName, long logRotTriggerTime, CancellationToken ct);

        Task UpdatePolicyInfoForRenewCertsAsync(RenewCertsPolicyInfo renewCertsPolicyInfo);

        Task UpdatePolicyInfoForRenewCertsAsync(RenewCertsPolicyInfo renewCertsPolicyInfo, CancellationToken ct);

        Task UpdateResyncThrottleAsync(Dictionary<int, Tuple<ThrottleState, long>> resyncThrottleInfo);

        Task UpdateResyncThrottleAsync(Dictionary<int, Tuple<ThrottleState, long>> resyncThrottleInfo, CancellationToken token);

        Task UpdateDiffThrottleAsync(Dictionary<int, Tuple<ThrottleState, long>> diffThrottleInfo);

        Task UpdateDiffThrottleAsync(Dictionary<int, Tuple<ThrottleState, long>> diffThrottleInfo, CancellationToken token);

        Task UploadFilesAsync(List<Tuple<string, string>> uploadFileInputItems);

        Task UploadFilesAsync(List<Tuple<string, string>> uploadFileInputItems, CancellationToken token);

        Task SendMonitoringMessageAsync(SendMonitoringMessageInput input, CancellationToken cancellationToken);
    }
}
