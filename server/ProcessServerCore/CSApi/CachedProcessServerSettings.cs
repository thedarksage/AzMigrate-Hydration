using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi
{
#pragma warning disable 0649
    internal class CachedProcessServerSettings : ProcessServerSettings
    {
        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore1")]
        public override decimal CumulativeThrottleLimit => m_cumulativeThrottleLimit;
        [JsonProperty(PropertyName = nameof(CumulativeThrottleLimit))]
        private decimal m_cumulativeThrottleLimit;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore2")]
        public override bool IsSourceFolderSupported => m_isSourceFolderSupported;
        [JsonProperty(PropertyName = nameof(IsSourceFolderSupported))]
        private bool m_isSourceFolderSupported;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore3")]
        public override IEnumerable<IProcessServerHostSettings> Hosts => m_hosts;
        [JsonProperty(PropertyName = nameof(Hosts))]
        private List<CachedProcessServerHostSettings> m_hosts;

        // Letting the default implementation of enumerating this from the hosts
        // to avoid the unnecessary footprint and compute.
        //[JsonIgnore]
        //[JsonProperty(PropertyName = "Ignore4")]
        //public override IEnumerable<IProcessServerPairSettings> Pairs

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore5")]
        public override string ApplicationInsightsInstrumentationKey => m_appInsightsKey;
        [JsonProperty(PropertyName = nameof(ApplicationInsightsInstrumentationKey))]
        private string m_appInsightsKey;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore6")]
        public override string CriticalChannelUri => m_criticalChannelUri;
        [JsonProperty(PropertyName = nameof(CriticalChannelUri))]
        private string m_criticalChannelUri;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore7")]
        public override DateTime CriticalChannelUriRenewalTimeUtc => m_criticalChannelUriRenewalTimeUtc;
        [JsonProperty(PropertyName = nameof(CriticalChannelUriRenewalTimeUtc))]
        private DateTime m_criticalChannelUriRenewalTimeUtc;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore8")]
        public override string InformationalChannelUri => m_informationalChannelUri;
        [JsonProperty(PropertyName = nameof(InformationalChannelUri))]
        private string m_informationalChannelUri;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore9")]
        public override DateTime InformationalChannelUriRenewalTimeUtc => m_informationalChannelUriRenewalTimeUtc;
        [JsonProperty(PropertyName = nameof(InformationalChannelUriRenewalTimeUtc))]
        private DateTime m_informationalChannelUriRenewalTimeUtc;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore10")]
        public override string TelemetryChannelUri => m_telemetryChannelUri;
        [JsonProperty(PropertyName = nameof(TelemetryChannelUri))]
        private string m_telemetryChannelUri;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore11")]
        public override DateTime TelemetryChannelUriRenewalTimeUtc => m_telemetryChannelUriRenewalTimeUtc;
        [JsonProperty(PropertyName = nameof(TelemetryChannelUriRenewalTimeUtc))]
        private DateTime m_telemetryChannelUriRenewalTimeUtc;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore12")]
        public override IEnumerable<IProtectedMachineTelemetrySettings> ProtectedMachineTelemetrySettings => m_protectedMachineTelemetrySettings;
        [JsonProperty(PropertyName = nameof(ProtectedMachineTelemetrySettings))]
        private List<CachedProtectedMachineTelemetrySettings> m_protectedMachineTelemetrySettings;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore13")]
        public override string MessageContext => m_messageContext;
        [JsonProperty(PropertyName = nameof(MessageContext))]
        private string m_messageContext;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore14")]
        public override IReadOnlyDictionary<string, string> Tunables => m_tunables;
        [JsonProperty(PropertyName = nameof(Tunables))]
        private Dictionary<string, string> m_tunables;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore15")]
        public override SystemHealth SystemHealth => m_systemHealth;
        [JsonProperty(PropertyName = nameof(SystemHealth))]
        private SystemHealth m_systemHealth;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore16")]
        public override LogRotationState LogRotationState => m_logRotationState;
        [JsonProperty(PropertyName = nameof(LogRotationState))]
        private LogRotationState m_logRotationState;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore17")]
        public override int LogFilesBackupCounter => m_logFilesBackupCounter;
        [JsonProperty(PropertyName = nameof(LogFilesBackupCounter))]
        private int m_logFilesBackupCounter;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore18")]
        public override IEnumerable<ILogRotationSettings> Logs => m_logs;
        [JsonProperty(PropertyName = nameof(Logs))]
        private List<CachedLogRotationSettings> m_logs;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore19")]
        public override IEnumerable<IPolicySettings> PolicySettings => m_policySettings;
        [JsonProperty(PropertyName = nameof(PolicySettings))]
        private List<CachedPolicySettings> m_policySettings;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore20")]
        public override bool IsCummulativeThrottled => m_isCummulativeThrottled;
        [JsonProperty(PropertyName = nameof(IsCummulativeThrottled))]
        private bool m_isCummulativeThrottled;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore21")]
        public override string CSHostId => m_csHostId;
        [JsonProperty(PropertyName = nameof(CSHostId))]
        private string m_csHostId;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore22")]
        public override bool IsPrivateEndpointEnabled => m_IsPrivateEndpointEnabled;
        [JsonProperty(PropertyName = nameof(IsPrivateEndpointEnabled))]
        private bool m_IsPrivateEndpointEnabled;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore23")]
        public override bool IsAccessControlEnabled => m_IsAccessControlEnabled;
        [JsonProperty(PropertyName = nameof(IsAccessControlEnabled))]
        private bool m_IsAccessControlEnabled;
    }

    internal class CachedProcessServerHostSettings : IProcessServerHostSettings
    {
        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore1")]
        public string HostId => m_hostId;
        [JsonProperty(PropertyName = nameof(HostId))]
        private string m_hostId;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore2")]
        public IEnumerable<IProcessServerPairSettings> Pairs => m_pairs;
        [JsonProperty(PropertyName = nameof(Pairs))]
        private List<CachedProcessServerPairSettings> m_pairs;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore3")]
        public string ProtectionState => m_protectionState;
        [JsonProperty(PropertyName = nameof(ProtectionState))]
        private string m_protectionState;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore4")]
        public string LogRootFolder => m_logRootFolder;
        [JsonProperty(PropertyName = nameof(LogRootFolder))]
        private string m_logRootFolder;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore5")]
        public string CriticalChannelUri => m_criticalChannelUri;
        [JsonProperty(PropertyName = nameof(CriticalChannelUri))]
        private string m_criticalChannelUri;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore6")]
        public DateTime CriticalChannelUriRenewalTimeUtc => m_criticalChannelUriRenewalTimeUtc;
        [JsonProperty(PropertyName = nameof(CriticalChannelUriRenewalTimeUtc))]
        private DateTime m_criticalChannelUriRenewalTimeUtc;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore7")]
        public string InformationalChannelUri => m_informationalChannelUri;
        [JsonProperty(PropertyName = nameof(InformationalChannelUri))]
        private string m_informationalChannelUri;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore8")]
        public DateTime InformationalChannelUriRenewalTimeUtc => m_informationalChannelUriRenewalTimeUtc;
        [JsonProperty(PropertyName = nameof(InformationalChannelUriRenewalTimeUtc))]
        private DateTime m_informationalChannelUriRenewalTimeUtc;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore9")]
        public string MessageContext => m_messageContext;
        [JsonProperty(PropertyName = nameof(MessageContext))]
        private string m_messageContext;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore10")]
        public string BiosId => m_biosId;
        [JsonProperty(PropertyName = nameof(BiosId))]
        private string m_biosId;
    }

    internal class CachedProcessServerPairSettings : IProcessServerPairSettings
    {
        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore1")]
        public string HostId => m_hostId;
        [JsonProperty(PropertyName = nameof(HostId))]
        private string m_hostId;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore2")]
        public string DeviceId => m_deviceId;
        [JsonProperty(PropertyName = nameof(DeviceId))]
        private string m_deviceId;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore3")]
        public ProtectionDirection ProtectionDirection => m_protectionDirection;
        [JsonProperty(PropertyName = nameof(ProtectionDirection))]
        private ProtectionDirection m_protectionDirection;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore4")]
        public string SourceDiffFolder => m_sourceDiffFolder;
        [JsonProperty(PropertyName = nameof(SourceDiffFolder))]
        private string m_sourceDiffFolder;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore5")]
        public string TargetDiffFolder => m_targetDiffFolder;
        [JsonProperty(PropertyName = nameof(TargetDiffFolder))]
        private string m_targetDiffFolder;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore6")]
        public string ResyncFolder => m_resyncFolder;
        [JsonProperty(PropertyName = nameof(ResyncFolder))]
        private string m_resyncFolder;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore7")]
        public string AzurePendingUploadRecoverableFolder => m_azurePendingUploadRecoverableFolder;
        [JsonProperty(PropertyName = nameof(AzurePendingUploadRecoverableFolder))]
        private string m_azurePendingUploadRecoverableFolder;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore8")]
        public string AzurePendingUploadNonRecoverableFolder => m_azurePendingUploadNonRecoverableFolder;
        [JsonProperty(PropertyName = nameof(AzurePendingUploadNonRecoverableFolder))]
        private string m_azurePendingUploadNonRecoverableFolder;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore9")]
        public long DiffThrottleLimit => m_diffThrottleLimit;
        [JsonProperty(PropertyName = nameof(DiffThrottleLimit))]
        private long m_diffThrottleLimit;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore10")]
        public long ResyncThrottleLimit => m_resyncThrottleLimit;
        [JsonProperty(PropertyName = nameof(ResyncThrottleLimit))]
        private long m_resyncThrottleLimit;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore11")]
        public string RPOFilePath => m_rPOFilePath;
        [JsonProperty(PropertyName = nameof(RPOFilePath))]
        private string m_rPOFilePath;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore12")]
        public IEnumerable<string> DiffFoldersToMonitor => m_diffFoldersToMonitor;
        [JsonProperty(PropertyName = nameof(DiffFoldersToMonitor))]
        private string[] m_diffFoldersToMonitor;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore13")]
        public string ResyncFolderToMonitor => m_resyncFolderToMonitor;
        [JsonProperty(PropertyName = nameof(ResyncFolderToMonitor))]
        private string m_resyncFolderToMonitor;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore14")]
        public string ReplicationSessionId => m_replicationSessionId;
        [JsonProperty(PropertyName = nameof(ReplicationSessionId))]
        private string m_replicationSessionId;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore15")]
        public string ReplicationState => m_replicationState;
        [JsonProperty(PropertyName = nameof(ReplicationState))]
        private string m_replicationState;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore16")]
        public DateTime IrProgressUpdateTimeUtc => m_irProgressUpdateTimeUtc;
        [JsonProperty(PropertyName = nameof(IrProgressUpdateTimeUtc))]
        private DateTime m_irProgressUpdateTimeUtc;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore17")]
        public string TagFolder => m_tagFolder;
        [JsonProperty(PropertyName = nameof(TagFolder))]
        private string m_tagFolder;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore18")]
        public string MessageContext => m_messageContext;
        [JsonProperty(PropertyName = nameof(MessageContext))]
        private string m_messageContext;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore19")]
        public ProtectionState ProtectionState => m_protectionState;
        [JsonProperty(PropertyName = nameof(ProtectionState))]
        private ProtectionState m_protectionState;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore20")]
        public TargetReplicationStatus TargetReplicationStatus => m_targetReplicationStatus;
        [JsonProperty(PropertyName = nameof(TargetReplicationStatus))]
        private TargetReplicationStatus m_targetReplicationStatus;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore21")]
        public long JobId => m_jobId;
        [JsonProperty(PropertyName = nameof(JobId))]
        private long m_jobId;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore22")]
        public EnableCompression EnableCompression => m_enableCompression;
        [JsonProperty(PropertyName = nameof(EnableCompression))]
        private EnableCompression m_enableCompression;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore23")]
        public ExecutionState ExecutionState => m_executionState;
        [JsonProperty(PropertyName = nameof(ExecutionState))]
        private ExecutionState m_executionState;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore24")]
        public int ReplicationStatus => m_replicationStatus;
        [JsonProperty(PropertyName = nameof(ReplicationStatus))]
        private int m_replicationStatus;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore25")]
        public ulong RestartResyncCounter => m_restartResyncCounter;
        [JsonProperty(PropertyName = nameof(RestartResyncCounter))]
        private ulong m_restartResyncCounter;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore26")]
        public ulong ResyncStartTime => m_resyncStartTime;
        [JsonProperty(PropertyName = nameof(ResyncStartTime))]
        private ulong m_resyncStartTime;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore27")]
        public ulong ResyncEndTime => m_resyncEndTime;
        [JsonProperty(PropertyName = nameof(ResyncEndTime))]
        private ulong m_resyncEndTime;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore28")]
        public uint tmid => m_tmid;
        [JsonProperty(PropertyName = nameof(tmid))]
        private uint m_tmid;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore29")]
        public FarLineProtected FarlineProtected => m_farlineProtected;
        [JsonProperty(PropertyName = nameof(FarlineProtected))]
        private FarLineProtected m_farlineProtected;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore30")]
        public int DoResync => m_doResync;
        [JsonProperty(PropertyName = nameof(DoResync))]
        private int m_doResync;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore31")]
        public string TargetHostId => m_targetHostId;
        [JsonProperty(PropertyName = nameof(TargetHostId))]
        private string m_targetHostId;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore32")]
        public string TargetDeviceId => m_targetDeviceId;
        [JsonProperty(PropertyName = nameof(TargetDeviceId))]
        private string m_targetDeviceId;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore33")]
        public string LunId => m_lunId;
        [JsonProperty(PropertyName = nameof(LunId))]
        private string m_lunId;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore34")]
        public LunState LunState => m_lunstate;
        [JsonProperty(PropertyName = nameof(LunState))]
        private LunState m_lunstate;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore35")]
        public long ResyncStartTagtime => m_resyncStartTagtime;
        [JsonProperty(PropertyName = nameof(ResyncStartTagtime))]
        private long m_resyncStartTagtime;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore36")]
        public long AutoResyncStartTime => m_autoResyncStartTime;
        [JsonProperty(PropertyName = nameof(AutoResyncStartTime))]
        private long m_autoResyncStartTime;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore37")]
        public long ResyncSetCxtimestamp => m_resyncSetCxtimestamp;
        [JsonProperty(PropertyName = nameof(ResyncSetCxtimestamp))]
        private long m_resyncSetCxtimestamp;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore38")]
        public string Hostname => m_hostname;
        [JsonProperty(PropertyName = nameof(Hostname))]
        private string m_hostname;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore39")]
        public string PerfFolder => m_perfFolder;
        [JsonProperty(PropertyName = nameof(PerfFolder))]
        private string m_perfFolder;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore40")]
        public uint CompatibilityNumber => m_compatibilityNumber;
        [JsonProperty(PropertyName = nameof(CompatibilityNumber))]
        private uint m_compatibilityNumber;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore41")]
        public uint TargetCompatibilityNumber => m_targetCompatibilityNumber;
        [JsonProperty(PropertyName = nameof(TargetCompatibilityNumber))]
        private uint m_targetCompatibilityNumber;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore42")]
        public int PairId => m_pairId;
        [JsonProperty(PropertyName = nameof(PairId))]
        private int m_pairId;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore43")]
        public long TargetLastHostUpdateTime => m_targetLastHostUpdateTime;
        [JsonProperty(PropertyName = nameof(TargetLastHostUpdateTime))]
        private long m_targetLastHostUpdateTime;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore44")]
        public string ReplicationCleanupOptions => m_replicationCleanupOptions;
        [JsonProperty(PropertyName = nameof(ReplicationCleanupOptions))]
        private string m_replicationCleanupOptions;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore45")]
        public sbyte SourceCommunicationBlocked => m_sourceCommunicationBlocked;
        [JsonProperty(PropertyName = nameof(SourceCommunicationBlocked))]
        private sbyte m_sourceCommunicationBlocked;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore46")]
        public int RPOThreshold => m_RPOThreshold;
        [JsonProperty(PropertyName = nameof(RPOThreshold))]
        private int m_RPOThreshold;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore47")]
        public long ControlPlaneTimestamp => m_controlPlaneTimestamp;
        [JsonProperty(PropertyName = nameof(ControlPlaneTimestamp))]
        private long m_controlPlaneTimestamp;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore48")]
        public long StatusUpdateTimestamp => m_statusUpdateTimestamp;
        [JsonProperty(PropertyName = nameof(StatusUpdateTimestamp))]
        private long m_statusUpdateTimestamp;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore49")]
        public TargetReplicationStatus SourceReplicationStatus => m_sourceReplicationStatus;
        [JsonProperty(PropertyName = nameof(SourceReplicationStatus))]
        private TargetReplicationStatus m_sourceReplicationStatus;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore50")]
        public long SourceLastHostUpdateTime => m_sourceLastHostUpdateTime;
        [JsonProperty(PropertyName = nameof(SourceLastHostUpdateTime))]
        private long m_sourceLastHostUpdateTime;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore51")]
        public sbyte IsResyncRequired => m_isResyncRequired;
        [JsonProperty(PropertyName = nameof(IsResyncRequired))]
        private sbyte m_isResyncRequired;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore52")]
        public PairDeletionStatus PairDeleted => m_pairDeleted;
        [JsonProperty(PropertyName = nameof(PairDeleted))]
        private PairDeletionStatus m_pairDeleted;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore53")]
        public ThrottleState ThrottleDifferentials => m_throttleDifferentials;
        [JsonProperty(PropertyName = nameof(ThrottleDifferentials))]
        private ThrottleState m_throttleDifferentials;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore54")]
        public long ResyncEndTagTime => m_resyncEndTagTime;
        [JsonProperty(PropertyName = nameof(ResyncEndTagTime))]
        private long m_resyncEndTagTime;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore55")]
        public ThrottleState ThrottleResync => m_throttleResync;
        [JsonProperty(PropertyName = nameof(ThrottleResync))]
        private ThrottleState m_throttleResync;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore56")]
        public string TargetHostName => m_targetHostName;
        [JsonProperty(PropertyName = nameof(TargetHostName))]
        private string m_targetHostName;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore57")]
        public AutoResyncStartType AutoResyncStartType => m_autoResyncStartType;
        [JsonProperty(PropertyName = nameof(AutoResyncStartType))]
        private AutoResyncStartType m_autoResyncStartType;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore58")]
        public int AutoResyncStartHours => m_autoResyncStartHours;
        [JsonProperty(PropertyName = nameof(AutoResyncStartHours))]
        private int m_autoResyncStartHours;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore59")]
        public int AutoResyncStartMinutes => m_autoResyncStartMinutes;
        [JsonProperty(PropertyName = nameof(AutoResyncStartMinutes))]
        private int m_autoResyncStartMinutes;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore60")]
        public int AutoResyncStopHours => m_autoResyncStopHours;
        [JsonProperty(PropertyName = nameof(AutoResyncStopHours))]
        private int m_autoResyncStopHours;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore61")]
        public int AutoResyncStopMinutes => m_autoResyncStopMinutes;
        [JsonProperty(PropertyName = nameof(AutoResyncStopMinutes))]
        private int m_autoResyncStopMinutes;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore62")]
        public int ResyncOrder => m_resyncOrder;
        [JsonProperty(PropertyName = nameof(ResyncOrder))]
        private int m_resyncOrder;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore63")]
        public int CurrentHour => m_currentHour;
        [JsonProperty(PropertyName = nameof(CurrentHour))]
        private int m_currentHour;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore64")]
        public int CurrentMinutes => m_currentMinutes;
        [JsonProperty(PropertyName = nameof(CurrentMinutes))]
        private int m_currentMinutes;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore65")]
        public int ScenarioId => m_scenarioId;
        [JsonProperty(PropertyName = nameof(ScenarioId))]
        private int m_scenarioId;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore66")]
        public int IsVisible => m_isVisible;
        [JsonProperty(PropertyName = nameof(IsVisible))]
        private int m_isVisible;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore67")]
        public long SourceAppAgentLastHostUpdateTime => m_sourceAppAgentLastHostUpdateTime;
        [JsonProperty(PropertyName = nameof(SourceAppAgentLastHostUpdateTime))]
        private long m_sourceAppAgentLastHostUpdateTime;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore68")]
        public string SourceAgentVersion => m_sourceAgentVersion;
        [JsonProperty(PropertyName = nameof(SourceAgentVersion))]
        private string m_sourceAgentVersion;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore69")]
        public string PSVersion => m_psVersion;
        [JsonProperty(PropertyName = nameof(PSVersion))]
        private string m_psVersion;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore70")]
        public long CurrentRPOTime => m_currentRPOTime;
        [JsonProperty(PropertyName = nameof(CurrentRPOTime))]
        private long m_currentRPOTime;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore71")]
        public long StatusUpdateTime => m_statusUpdateTime;
        [JsonProperty(PropertyName = nameof(StatusUpdateTime))]
        private long m_statusUpdateTime;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore72")]
        public string PerfShortFolder => m_perfShortFolder;
        [JsonProperty(PropertyName = nameof(PerfShortFolder))]
        private string m_perfShortFolder;
    }

    internal class CachedProtectedMachineTelemetrySettings : IProtectedMachineTelemetrySettings
    {
        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore1")]
        public string HostId => m_hostId;
        [JsonProperty(PropertyName = nameof(HostId))]
        private string m_hostId;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore2")]
        public string TelemetryFolderPath => m_telemetryFolderPath;
        [JsonProperty(PropertyName = nameof(TelemetryFolderPath))]
        private string m_telemetryFolderPath;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore3")]
        public string BiosId => m_biosId;
        [JsonProperty(PropertyName = nameof(BiosId))]
        private string m_biosId;
    }

    internal class CachedLogRotationSettings : ILogRotationSettings
    {
        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore1")]
        public string LogName => m_logName;
        [JsonProperty(PropertyName = nameof(LogName))]
        private string m_logName;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore2")]
        public string LogPathWindows => m_logPathWindows;
        [JsonProperty(PropertyName = nameof(LogPathWindows))]
        private string m_logPathWindows;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore3")]
        public int LogSizeLimit => m_logSizeLimit;
        [JsonProperty(PropertyName = nameof(LogSizeLimit))]
        private int m_logSizeLimit;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore4")]
        public int LogTimeLimit => m_logTimeLimit;
        [JsonProperty(PropertyName = nameof(LogTimeLimit))]
        private int m_logTimeLimit;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore5")]
        public long StartEpochTime => m_startEpochTime;
        [JsonProperty(PropertyName = nameof(StartEpochTime))]
        private long m_startEpochTime;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore6")]
        public long CurrentEpochTime => m_currentEpochTime;
        [JsonProperty(PropertyName = nameof(CurrentEpochTime))]
        private long m_currentEpochTime;
    }

    internal class CachedPolicySettings : IPolicySettings
    {
        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore1")]
        public string HostId => m_hostId;
        [JsonProperty(PropertyName = nameof(HostId))]
        private string m_hostId;

        [JsonIgnore]
        [JsonProperty(PropertyName = "Ignore2")]
        public int PolicyId => m_policyId;
        [JsonProperty(PropertyName = nameof(PolicyId))]
        private int m_policyId;
    }

#pragma warning restore 0649
}
