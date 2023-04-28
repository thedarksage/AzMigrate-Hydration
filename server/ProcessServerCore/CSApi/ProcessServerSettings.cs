using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using RcmClientLib;
using RcmContract;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi
{
    public enum ProtectionDirection
    {
        // NOTE-SanKumar-1904: This enum matches the CS API contract. Don't
        // modify the mapping of existing values, otherwise change the parsing
        // logic of the ProtectionDirection of CSProcessServerSettings
        Unknown = 0,
        Reverse = 1,
        Forward = 2
    }

    public enum ProtectionState
    {
        Unknown = -1,
        ResyncStep1 = 0,
        ResyncStep2 = 1,
        DifferentialSync = 2
    }

    public enum TargetReplicationStatus
    {
        Unknown = -1,
        NonPaused = 0,
        Paused = 1
    }

    //taken from compressmode.h
    public enum EnableCompression
    {
        Unknown = -1,
        NoCompression = 0,              // COMPRESS_NONE
        InLineCompression = 1,          // COMPRESS_CXSIDE
        HostBasedCompression = 2        // COMPRESS_SOURCESIDE
    }

    public enum ExecutionState
    {
        Unknown = -1,
        Default = 0,
        Active = 1,
        Unconfigured = 2,
        UnconfiguredWaiting = 3,
        Waiting = 4
    }

    public enum FarLineProtected
    {
        Unknown = -1,
        UnProtected = 0,
        Protected = 1
    }

    public enum LunState
    {
        Unknown = 0,
        StartProtectionPending = 1,
        Protected = 2,
        StopProtectionPending = 3
    }

    public enum ThrottleState
    {
        Unknown = -1,
        NotThrottled = 0,
        Throttled = 1
    }

    public enum SystemHealth
    {
        Unknown = 0,
        healthy = 1,
        degraded = 2
    }

    public enum PairDeletionStatus
    {
        Unknown = -1,
        NotDeleted = 0,
        Deleted = 1
    }

    public enum PairPauseStatus
    {
        Unknown = -1,
        Pause = 0,
        Restart = 1
    }

    public enum PairAutoResumeStatus
    {
        Unknown = -1,
        NotSet = 0,
        Set = 1
    }

    public enum AutoResyncStartType
    {
        Unknown = -1,
        NotSet = 0,
        Set = 1
    }

    public enum LogRotationState
    {
        Unknown = -1,
        Disabled = 0,
        Enabled = 1
    }

    public enum PolicyState
    {
        Unknown = -1,
        Success = 1,
        Failed = 2,
        Pending = 3,
        InProgress = 4,
        Skipped = 5
    }

    public interface IProcessServerHostSettings
    {
        /// <summary>
        /// Host ID
        /// </summary>
        string HostId { get; }

        // TODO-SanKumar-2001:
        string ProtectionState { get; }

        /// <summary>
        /// Folder under which all the resync and diff data
        /// will go into for the host.
        /// </summary>
        string LogRootFolder { get; }

        IEnumerable<IProcessServerPairSettings> Pairs { get; }

        /// <summary>
        /// Uri to report critical health and events for the host
        /// </summary>
        string CriticalChannelUri { get; }

        /// <summary>
        /// Renewal time of <code>CriticalChannelUri</code>
        /// </summary>
        DateTime CriticalChannelUriRenewalTimeUtc { get; }

        /// <summary>
        /// Uri to report informational health and events
        /// </summary>
        string InformationalChannelUri { get; }

        /// <summary>
        /// Renewal time of <code>InformationalChannelUri</code>
        /// </summary>
        DateTime InformationalChannelUriRenewalTimeUtc { get; }

        /// <summary>
        /// Context to be used, while reporting critical and
        /// informational health and events for the host
        /// </summary>
        string MessageContext { get; }

        /// <summary>
        /// Source Machine BiosId
        /// </summary>
        string BiosId { get; }
    }

    public interface IProcessServerPairSettings
    {
        /// <summary>
        /// Host ID
        /// </summary>
        string HostId { get; }

        /// <summary>
        /// Disk ID
        /// </summary>
        string DeviceId { get; }

        /// <summary>
        /// Direction of protection
        /// </summary>
        ProtectionDirection ProtectionDirection { get; }

        /// <summary>
        /// Folder under which diff files go in from source
        /// (valid if <c>ProcessServerSettings.IsSourceFolderSupported</c>
        /// is set)
        /// </summary>
        string SourceDiffFolder { get; }

        /// <summary>
        /// Folder under which diff files go in from source
        /// (if <c>ProcessServerSettings.IsSourceFolderSupported</c>
        /// is not set) or diff files go in from <c>SourceDiffFolder</c>
        /// (if <c>ProcessServerSettings.IsSourceFolderSupported</c>
        /// is set)
        /// </summary>
        string TargetDiffFolder { get; }

        /// <summary>
        /// Folder under which resync files go in from source
        /// </summary>
        string ResyncFolder { get; }

        /// <summary>
        /// Folder under which diff files that can create
        /// recovery point(s) is put by MT'
        /// </summary>
        string AzurePendingUploadRecoverableFolder { get; }

        /// <summary>
        /// Folder under which diff files that can't create
        /// recovery point(s) is put by MT'
        /// </summary>
        string AzurePendingUploadNonRecoverableFolder { get; }

        ProtectionState ProtectionState { get; }

        TargetReplicationStatus TargetReplicationStatus { get; }

        /// <summary>
        /// Maximum bytes of diff data allowed for the pair on PS
        /// </summary>
        long DiffThrottleLimit { get; }

        /// <summary>
        /// Maximum bytes of resync data allowed for the pair on PS
        /// </summary>
        long ResyncThrottleLimit { get; }

        /// <summary>
        /// Path of the internal file used to monitor incoming data
        /// outage of the pair
        /// </summary>
        string RPOFilePath { get; }

        /// <summary>
        /// Folders to monitor for diffsync related health and statistics
        /// </summary>
        IEnumerable<string> DiffFoldersToMonitor { get; }

        /// <summary>
        /// Folder to monitor for resync related health and statistics
        /// </summary>
        string ResyncFolderToMonitor { get; }

        // TODO-SanKumar-2001:
        string ReplicationSessionId { get; }

        // TODO-SanKumar-2001:
        string ReplicationState { get; }

        /// <summary>
        /// Last IR progress updation time by source to the server
        /// </summary>
        DateTime IrProgressUpdateTimeUtc { get; }

        /// <summary>
        /// Internal folder used to monitor RPO of the pair
        /// </summary>
        string TagFolder { get; }

        /// <summary>
        /// Context to be used on communication about the pair
        /// </summary>
        string MessageContext { get; }

        /// <summary>
        /// JobId for the pair
        /// </summary>
        long JobId { get; }

        /// <summary>
        /// Nature of compression to be applied on the log files
        /// </summary>
        EnableCompression EnableCompression { get; }

        /// <summary>
        /// Execution state for the pair
        /// </summary>
        ExecutionState ExecutionState { get; }

        /// <summary>
        /// Replication status of the pair
        /// </summary>
        int ReplicationStatus { get; }

        /// <summary>
        /// The number of times resync is triggered for the pair
        /// </summary>
        ulong RestartResyncCounter { get; }

        /// <summary>
        /// The time at which last resync was started
        /// seconds since epoch
        /// </summary>
        ulong ResyncStartTime { get; }

        /// <summary>
        /// The time at which last resync ended
        /// seconds since epoch
        /// </summary>
        ulong ResyncEndTime { get; }

        /// <summary>
        /// Tmid for the pair
        /// </summary>
        uint tmid { get; }

        /// <summary>
        /// Flag to check if the disk/volume is protected
        /// </summary>
        FarLineProtected FarlineProtected { get; }

        /// <summary>
        /// Set when resync is marked for a pair
        /// This is the flag that is set if resync is marked for a pair
        /// </summary>
        int DoResync { get; }

        /// <summary>
        /// Flag to check if the drive is visible or not
        /// </summary>
        int IsVisible { get; }

        /// <summary>
        /// Hostid for the target of the pair
        /// </summary>
        string TargetHostId { get; }

        /// <summary>
        /// DeviceId for target of the pair
        /// </summary>
        string TargetDeviceId { get; }

        /// <summary>
        /// LunId for the current volume
        /// </summary>
        string LunId { get; }

        /// <summary>
        /// Lunstate of the current volume
        /// </summary>
        LunState LunState { get; }

        /// <summary>
        /// Resync time set by agent
        /// ticks since epoch
        /// </summary>
        long ResyncStartTagtime { get; }

        /// <summary>
        /// Resync end time set by agent
        /// ticks since epoch
        /// </summary>
        long ResyncEndTagTime { get; }

        /// <summary>
        /// Time at which resync should start automatically
        /// </summary>
        long AutoResyncStartTime { get; }

        /// <summary>
        /// The time at which resync was marked
        /// If its non zero then resync is already marked
        /// </summary>
        long ResyncSetCxtimestamp { get; }

        /// <summary>
        /// Hostname for the current host
        /// </summary>
        string Hostname { get; }

        /// <summary>
        /// Folder containing the perf files for the pair
        /// </summary>
        string PerfFolder { get; }

        /// <summary>
        /// Compatibility number for the source host
        /// </summary>
        uint CompatibilityNumber { get; }

        /// <summary>
        /// Compatibility number of the target host
        /// </summary>
        uint TargetCompatibilityNumber { get; }

        /// <summary>
        /// pairid for the pair
        /// </summary>
        int PairId { get; }

        /// <summary>
        /// The last time when the target updated the cs
        /// also referred to as last heartbeat of target
        /// seconds since epoch
        /// </summary>
        long TargetLastHostUpdateTime { get; }

        /// <summary>
        /// Bitmask set by CS for cleanup actions
        /// </summary>
        string ReplicationCleanupOptions { get; }

        /// <summary>
        /// The flag to check if source is sending
        /// files to ps or not
        /// </summary>
        sbyte SourceCommunicationBlocked { get; }

        /// <summary>
        /// The threshold in minutes after which
        /// ps should raise rpo alerts
        /// </summary>
        int RPOThreshold { get; }

        /// <summary>
        /// The current time of control plane
        /// Seconds since epoch in utc
        /// </summary>
        long ControlPlaneTimestamp { get; }

        /// <summary>
        /// The last time when the status from source
        /// was updated to control plane
        /// Seconds since epoch in utc
        /// </summary>
        long StatusUpdateTimestamp { get; }

        /// <summary>
        /// Replication status of the source machine
        /// </summary>
        TargetReplicationStatus SourceReplicationStatus { get; }

        /// <summary>
        /// The last time when source machine updated cs
        /// Also known as last heartbeat of source
        /// </summary>
        long SourceLastHostUpdateTime { get; }

        /// <summary>
        /// The last time when app agent on source machine updated cs
        /// Also known as last app agent heartbeat of source
        /// </summary>
        long SourceAppAgentLastHostUpdateTime { get; }

        /// <summary>
        /// Flag to check if resync is required by a source
        /// </summary>
        sbyte IsResyncRequired { get; }

        /// <summary>
        /// Flag to check if a pair is deleted
        /// </summary>
        PairDeletionStatus PairDeleted { get; }

        /// <summary>
        /// Flag to check if diff files are throttled
        /// </summary>
        ThrottleState ThrottleDifferentials { get; }

        /// <summary>
        /// Flag to check if resync files are throttled
        /// </summary>
        ThrottleState ThrottleResync { get; }

        /// <summary>
        /// Target Host Name
        /// </summary>
        string TargetHostName { get; }

        /// <summary>
        /// Flag to check if auto resync start type is set or not
        /// </summary>
        AutoResyncStartType AutoResyncStartType { get; }

        // TODO : Using int for all the autoresync start/stop/current time intervals.
        // Revisit later to check if uint needs to be used.

        /// <summary>
        /// Time in hours at which auto resync should be started.
        /// It's value will be between 0-23
        /// </summary>
        int AutoResyncStartHours { get; }

        /// <summary>
        /// Time in minutes at which auto resync should be started.
        /// It's value will be between 0-59
        /// </summary>
        int AutoResyncStartMinutes { get; }

        /// <summary>
        /// Time in hours at which auto resync should be stopped.
        /// It's value will be between 0-23
        /// </summary>
        int AutoResyncStopHours { get; }

        /// <summary>
        /// Time in minutes at which auto resync should be stopped.
        /// It's value will be between 0-59
        /// </summary>
        int AutoResyncStopMinutes { get; }

        /// <summary>
        /// Resync Order for the pair
        /// </summary>
        int ResyncOrder { get; }

        /// <summary>
        /// Current time of CS in hours
        /// Used in combination with CurrentMinutes to compare with auto
        /// resync start/stop window interval to decide if auto resync
        /// should be triggerd or not
        /// </summary>
        int CurrentHour { get; }

        /// <summary>
        /// Current time of CS in minutes
        /// Used in combination with CurrentHours to compare with auto
        /// resync start/stop window interval to decide if auto resync
        /// should be triggerd or not
        /// </summary>
        int CurrentMinutes { get; }

        /// <summary>
        /// ScenarioId for the pair
        /// </summary>
        int ScenarioId { get; }

        /// <summary>
        /// Source Agent Version
        /// </summary>
        string SourceAgentVersion { get; }

        /// <summary>
        /// ProcessServer Version
        /// </summary>
        string PSVersion { get; }

        /// <summary>
        /// Current RPO time in Seconds since epoch
        /// </summary>
        long CurrentRPOTime { get; }

        /// <summary>
        /// Last RPO update time in Seconds since epoch
        /// </summary>
        long StatusUpdateTime { get; }

        /// <summary>
        /// Folder containing the perf files short path for the pair
        /// </summary>
        string PerfShortFolder { get; }
    }

    public interface IProtectedMachineTelemetrySettings
    {
        /// <summary>
        /// Host ID
        /// </summary>
        string HostId { get; }

        /// <summary>
        /// Folder under which the logs and telemetry of
        /// the protected machine would be put in to be
        /// uploaded to Kusto.
        /// </summary>
        string TelemetryFolderPath { get; }

        /// <summary>
        /// Bios Id
        /// </summary>
        string BiosId { get; }
    }

    internal class ProtectedMachineTelemetrySettings : IProtectedMachineTelemetrySettings
    {
        public string HostId { get; }

        public string TelemetryFolderPath { get; }

        public string BiosId { get; }

        public ProtectedMachineTelemetrySettings(
            string hostId, string telemetryFolderPath, string biosId)
        {
            this.HostId = hostId;
            this.TelemetryFolderPath = telemetryFolderPath;
            this.BiosId = biosId;
        }
    }

    public interface ILogRotationSettings
    {
        /// <summary>
        /// Name of the log file
        /// </summary>
        string LogName { get; }

        /// <summary>
        /// Path of the log file
        /// </summary>
        string LogPathWindows { get; }

        /// <summary>
        /// Log Size limit in MB, beyond which log rotation happens
        /// </summary>
        int LogSizeLimit { get; }

        /// <summary>
        /// Log Time Limit in days, beyond which log rotation happens
        /// </summary>
        int LogTimeLimit { get; }

        /// <summary>
        /// Log rotation start time
        /// Seconds elapsed since epoch
        /// </summary>
        long StartEpochTime { get; }

        /// <summary>
        /// Current time in seconds elapsed since epoch
        /// </summary>
        long CurrentEpochTime { get; }
    }

    public interface IPolicySettings
    {
        /// <summary>
        /// Host ID
        /// </summary>
        string HostId { get; }

        /// <summary>
        /// Current PolicyId associated with the PS HostId
        /// </summary>
        int PolicyId { get; }
    }

    public abstract class ProcessServerSettings
    {
        protected internal const string DIFFS_SUB_FOLDER = "diffs";
        protected internal const string TAGS_SUB_FOLDER = "tags";
        protected internal const string MONITOR_TXT = "monitor.txt";
        protected internal const string RESYNC_SUB_FOLDER = "resync";
        protected internal const string AZ_PEND_UPL_REC_SUB_FOLDER = "AzurePendingUploadRecoverable";
        protected internal const string AZ_PEND_UPL_NONREC_SUB_FOLDER = "AzurePendingUploadNonRecoverable";
        protected internal const string PERF_TXT = "perf.txt";
        protected internal const string PERF_LOG = "perf.log";

        #region Abstract members

        /// <summary>
        /// Does the process server type support source folder concept?
        /// </summary>
        public abstract bool IsSourceFolderSupported { get; }

        public abstract IEnumerable<IProcessServerHostSettings> Hosts { get; }

        /// <summary>
        /// Percentage of used cache volume size beyond which the incoming
        /// data must be throttled for the PS
        /// </summary>
        public abstract decimal CumulativeThrottleLimit { get; }

        /// <summary>
        /// Flag to enable/disable logs rotation
        /// </summary>
        public abstract LogRotationState LogRotationState { get; }

        /// <summary>
        /// Number of backup log files to be compressed
        /// </summary>
        public abstract int LogFilesBackupCounter { get; }

        /// <summary>
        /// App insights key to upload logs and telemetry for the PS
        /// Alternate path to main path - <c>TelemetryChannelUri</c>
        /// </summary>
        public abstract string ApplicationInsightsInstrumentationKey { get; }

        /// <summary>
        /// Uri to report critical health and events for the PS
        /// </summary>
        public abstract string CriticalChannelUri { get; }

        /// <summary>
        /// Renewal time of <c>CriticalChannelUri</c>
        /// </summary>
        public abstract DateTime CriticalChannelUriRenewalTimeUtc { get; }

        /// <summary>
        /// Uri to report informational health and events for the PS
        /// </summary>
        public abstract string InformationalChannelUri { get; }

        /// <summary>
        /// Renewal time of <c>InformationalChannelUri</c>
        /// </summary>
        public abstract DateTime InformationalChannelUriRenewalTimeUtc { get; }

        /// <summary>
        /// Uri to upload logs and telemetry of all the connected components
        /// Source(s), PS, MT', MARS
        /// </summary>
        public abstract string TelemetryChannelUri { get; }

        /// <summary>
        /// Renewal time of <c>TelemetryChannelUri</c>
        /// </summary>
        public abstract DateTime TelemetryChannelUriRenewalTimeUtc { get; }

        public abstract IEnumerable<IProtectedMachineTelemetrySettings> ProtectedMachineTelemetrySettings { get; }

        /// <summary>
        /// Message context to use for the communication about the PS
        /// </summary>
        public abstract string MessageContext { get; }

        /// <summary>
        /// Server provided values to override hardcoded settings in
        /// the PS components. Note that, these values don't supersede
        /// a value explicitly set in the corresponding conf file
        /// referred by the component(s).
        /// </summary>
        public abstract IReadOnlyDictionary<string, string> Tunables { get; }

        /// <summary>
        /// Overall system health of the process server
        /// </summary>
        public abstract SystemHealth SystemHealth { get; }

        /// <summary>
        /// Settings for performing log rotation.
        /// </summary>
        public abstract IEnumerable<ILogRotationSettings> Logs { get; }

        /// <summary>
        /// Policy Settings for PS Certificates renewal.
        /// </summary>
        public abstract IEnumerable<IPolicySettings> PolicySettings { get; }

        /// <summary>
        /// True, if cumulative throttle is set otherwise false
        /// </summary>
        public abstract bool IsCummulativeThrottled { get; }

        /// <summary>
        /// CS Host GUID
        /// </summary>
        public abstract string CSHostId { get; }

        /// <summary>
        /// Flag to detect private end points configuration
        /// </summary>
        public abstract bool IsPrivateEndpointEnabled { get; }

        /// <summary>
        /// Enable or Disable Access Control Feature
        /// </summary>
        public abstract bool IsAccessControlEnabled { get; }

        #endregion Abstract members

        #region Derived members

        // This property is meant for ease of coding. Ignoring this property from
        // usage in Json serialization, since the pairs are actually packaged
        // within the host settings.
        [JsonIgnore]
        public IEnumerable<IProcessServerPairSettings> Pairs
        {
            get
            {
                return
                    this.Hosts?.Where(currHost => currHost != null && currHost.Pairs != null)
                    .SelectMany(currHost => currHost.Pairs);
            }
        }

        #endregion Derived members

        #region Cache handling

        private static readonly SemaphoreSlim s_cachedSettingSemaphore = new SemaphoreSlim(1);

        private static ProcessServerSettings s_cachedSettings = null;

        private static readonly JsonSerializer s_cachedSettingsFileSerializerNoIndent
            = JsonUtils.GetStandardSerializer(
                indent: false,
                converters: new JsonConverter[]
                {
                    new VersionConverter(),
                    new IPAddressJsonConverter()
                });

        private static readonly JsonSerializerSettings s_cachedSettingsFileSerializerSettings
            = JsonUtils.GetStandardSerializerSettings(
                indent: true,
                converters: new JsonConverter[]
                {
                    new VersionConverter(),
                    new IPAddressJsonConverter()
                });

        private static readonly JsonSerializerSettings s_cachedSettingsFileSerializerSettingsNoIndent
            = JsonUtils.GetStandardSerializerSettings(
                indent: false,
                converters: new JsonConverter[]
                {
                    new VersionConverter(),
                    new IPAddressJsonConverter()
                });

        public static ProcessServerSettings GetCachedSettings() => s_cachedSettings;

        public static async Task RunMasterPoller(
            string settingsFilePath,
            object assistObj,
            CancellationToken ct)
        {
            async Task WriteSettingsToFileAsync(StreamWriter sw)
            {
                var serializedSettings = JsonConvert.SerializeObject(
                    s_cachedSettings, s_cachedSettingsFileSerializerSettings);

                if (Settings.Default.ProcessServerSettings_CacheIncludesHeader)
                {
                    s_cachedSettingsFileSerializerNoIndent.Serialize(
                                sw, CacheDataHeader.BuildHeader(serializedSettings));

                    await sw.WriteLineAsync().ConfigureAwait(false);
                }

                await sw.WriteAsync(serializedSettings).ConfigureAwait(false);
            }

            // TODO-SanKumar-1905: The StopWatch object would be resilient to system time changes,
            // only if it is using QPC (StopWatch.IsHighResolution == true). Otherwise, it uses
            // System clock for getting elapsed time. In such cases, we should add mecanism to
            // handle System clock adjustments.
            Stopwatch retrievalTimeTakenSW = new Stopwatch();

            object lastSuccRcmSettingsObj = null;
            ProcessServerSettings lastSuccSettings;

            Tracers.Settings.TraceAdminLogV2Message(TraceEventType.Information, "Starting master poller");

            // Lightweight lock replacement in Async pattern
            await s_cachedSettingSemaphore.WaitAsync(ct).ConfigureAwait(false);

            try
            {
                bool attemptLoadFromCachedFile =
                    Settings.Default.ProcessServerSettings_CacheMasterAttemptInitLoadFromCache;

                try
                {
                    // Sending cached SoureMachineIDs to RCM with master poller.
                    if (File.Exists(settingsFilePath) && PSInstallationInfo.Default.CSMode.Equals(CSMode.Rcm))
                    {
                        RcmContract.ProcessServerSettings cacheSourceMachineSettings = new RcmContract.ProcessServerSettings();

                        (ProcessServerSettings lastSuccRcmSettings, _, _, _) = await ReadFileCachedPSSettings(
                            settingsFilePath, (null, null), ct).ConfigureAwait(false);

                        cacheSourceMachineSettings.SourceMachineIds = lastSuccRcmSettings.Hosts
                            .Where(currItem => currItem != null && currItem.HostId != null)
                            .Select(currHost => currHost.HostId).ToList();

                        lastSuccRcmSettingsObj = cacheSourceMachineSettings;
                    }
                }
                catch (OperationCanceledException) when (ct.IsCancellationRequested)
                {
                    throw;
                }
                catch (Exception fileCacheEx)
                {
                    Tracers.Settings.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Error in retrieving and loading the file cached PS settings{0}{1}",
                        Environment.NewLine, fileCacheEx);
                }

                for (int cnt = 0; ; cnt++)
                {
                    retrievalTimeTakenSW.Restart();

                    try
                    {
                        bool psNotRegistered = false;
                        ProcessServerSettings currPSSettings = null;

                        try
                        {
                            switch (PSInstallationInfo.Default.CSMode)
                            {
                                // TODO-SanKumar-2001: Singleton/One-time/lazy init the stubs object
                                case CSMode.LegacyCS:
                                    using (var stubs = CSApiFactory.GetProcessServerCSApiStubs())
                                    {
                                        currPSSettings = await stubs.GetPSSettings(ct).ConfigureAwait(false);
                                        // TODO-SanKumar-2001: Detect if legacy PS is unregistered,
                                        // in which case the settings logic must be signalled with
                                        // psNotRegistered = true. This will help all running activites
                                        // stop, instead of this loop logging failure as well as
                                        // the logics depending on reporting to CS logging failures.
                                    }
                                    break;

                                case CSMode.Rcm:
                                    (currPSSettings, lastSuccRcmSettingsObj, psNotRegistered)
                                        = await MasterRcmSettingsRetrieverAsync(assistObj, lastSuccRcmSettingsObj, ct);
                                    break;

                                case CSMode.Unknown:
                                default:
                                    throw new NotImplementedException(
                                        $"Master settings retrieval not implemented for {PSInstallationInfo.Default.CSMode} mode");
                            }
                        }
                        catch (Exception srvSettingsEx) when (attemptLoadFromCachedFile)
                        {
                            Tracers.Settings.TraceAdminLogV2Message(
                                TraceEventType.Error,
                                "Error in retrieving the PS settings from the server. " +
                                "Attempting once to read the settings from the local cache file{0}{1}",
                                Environment.NewLine, srvSettingsEx);

                            try
                            {
                                (currPSSettings, _, _, _) = await ReadFileCachedPSSettings(
                                                        settingsFilePath, (null, null), ct).ConfigureAwait(false);
                            }
                            catch (OperationCanceledException) when (ct.IsCancellationRequested)
                            {
                                throw;
                            }
                            catch (Exception fileCacheEx)
                            {
                                Tracers.Settings.TraceAdminLogV2Message(
                                    TraceEventType.Error,
                                    "Error in retrieving and loading the file cached PS settings at Master poller{0}{1}",
                                    Environment.NewLine, fileCacheEx);
                            }

                            // It has once been attempted and since this is the master loop, there
                            // mostly wouldn't be another chance, where this operation would succeed.
                            attemptLoadFromCachedFile = false;
                        }

                        if (currPSSettings != null || psNotRegistered)
                        {
                            lastSuccSettings = Interlocked.Exchange(ref s_cachedSettings, currPSSettings);

                            // At least once we've gotten a valid setting or detected that the PS
                            // has been unregistered.
                            attemptLoadFromCachedFile = false;

                            FinalizeSetting(oldSettings: lastSuccSettings, newSettings: s_cachedSettings);

                            using (var lockFile =
                                await FileUtils.AcquireLockFileAsync(settingsFilePath, exclusive: true, ct: ct)
                                .ConfigureAwait(false))
                            {
                                if (psNotRegistered)
                                {
                                    if (File.Exists(settingsFilePath))
                                    {
                                        Tracers.Settings.TraceAdminLogV2Message(
                                            TraceEventType.Information,
                                            "Deleting the Settings file - {0}, since the PS is not registered to the Server",
                                            settingsFilePath);

                                        File.Delete(settingsFilePath);
                                    }
                                }
                                else
                                {
                                    await FileUtils.ReliableSaveFileAsync(
                                        settingsFilePath,
                                        backupFolderPath: null,
                                        multipleBackups: false,
                                        writeDataAsync: WriteSettingsToFileAsync,
                                        flush: Settings.Default.ProcessServerSettings_CacheMasterFlushFile)
                                        .ConfigureAwait(false);
                                }
                            }
                        }

                        if (PSInstallationInfo.Default.CSMode == CSMode.Rcm)
                        {
                            checkAndCreateFolders(
                                currPSSettings: currPSSettings);
                        }

                    }
                    catch (OperationCanceledException) when (ct.IsCancellationRequested)
                    {
                        throw;
                    }
                    catch (Exception ex)
                    {
                        // TODO-SanKumar-2002: Rate control this log?
                        Tracers.Settings.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Error in retrieving and saving the PS settings at Master poller{0}{1}",
                            Environment.NewLine, ex);
                    }

                    TimeSpan toWaitTimeSpan = Settings.Default.ProcessServerSettings_CacheExpiryInterval;

                    toWaitTimeSpan -= retrievalTimeTakenSW.Elapsed;
                    if (toWaitTimeSpan < TimeSpan.Zero)
                    {
                        toWaitTimeSpan = TimeSpan.Zero;
                    }

                    await Task.Delay(toWaitTimeSpan, ct).ConfigureAwait(false);
                }
            }
            finally
            {
                s_cachedSettingSemaphore.Release();
            }
        }

        /// <summary>
        /// Checks if folder path is under telemetry folder path
        /// </summary>
        /// <param name="folderPath">Path of the directory</param>
        /// <param name="traceSource">Trace source to use for logging. Pass null, if logging is not required.</param>
        public static bool EnsureFolderPathUnderTelemetryFolderPath(
            string folderPath,
            TraceSource traceSource)
        {
            var telemetryFolderPath = PSInstallationInfo.Default.GetTelemetryFolderPath();
            try
            {
                FSUtils.EnsureItemUnderParent(
                    itemPath: folderPath,
                    isFile: false,
                    expectedParentFolder: telemetryFolderPath);

                return true;
            }
            catch (Exception ex)
            {
                traceSource?.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Source Folder Path: {0}, is not present under Telemetry Folder: {1}, Exception: {2}",
                        folderPath, telemetryFolderPath, ex);
            }

            return false;
        }

        /// <summary>
        /// Checks if folder path is under replication log folder path
        /// </summary>
        /// <param name="folderPath">Path of the directory</param>
        /// <param name="traceSource">Trace source to use for logging. Pass null, if logging is not required.</param>
        public static bool EnsureFolderPathUnderReplLogFolderPath(
            string folderPath,
            TraceSource traceSource)
        {
            var replicationLogFolderPath = PSInstallationInfo.Default.GetReplicationLogFolderPath();
            try
            {
                FSUtils.EnsureItemUnderParent(
                    itemPath: folderPath,
                    isFile: false,
                    expectedParentFolder: replicationLogFolderPath);

                return true;
            }
            catch (Exception ex)
            {
                traceSource?.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Source Folder Path: {0}, is not present under Replication log Folder: {1}, Exception: {2}",
                        folderPath, replicationLogFolderPath, ex);
            }

            return false;
        }

        private static void checkAndCreateFolderWithRetry(
            string folderPath,
            Func<string, TraceSource, bool> pathValidator)
        {
            if (string.IsNullOrEmpty(folderPath))
                return;

            if (!pathValidator(folderPath, Tracers.Settings))
                return;

            if (Directory.Exists(folderPath))
                return;

            var longPath = FSUtils.GetLongPath(
                folderPath,
                isFile: false,
                optimalPath: Settings.Default.RcmJobProc_LongPathOptimalConversion);


            FSUtils.CreateAdminsAndSystemOnlyDir(folderPath);

            Tracers.Settings.TraceAdminLogV2Message(
                TraceEventType.Information,
                "Successfully created folder : {0} using long path : {1}",
                folderPath, longPath);
        }

        private static bool checkAndCreateFolders(
            ProcessServerSettings currPSSettings)
        {
            try
            {
                currPSSettings?.ProtectedMachineTelemetrySettings?.ForEach(
                telemetrySetting =>

                checkAndCreateFolderWithRetry(
                    telemetrySetting.TelemetryFolderPath,
                    EnsureFolderPathUnderTelemetryFolderPath
                    ));

                currPSSettings?.Hosts?.ForEach(host =>
                {
                    host.Pairs?.ForEach(hostPair =>
                    {
                        checkAndCreateFolderWithRetry(
                            hostPair.TargetDiffFolder,
                            EnsureFolderPathUnderReplLogFolderPath
                            );

                        checkAndCreateFolderWithRetry(
                            hostPair.ResyncFolder,
                            EnsureFolderPathUnderReplLogFolderPath
                            );
                    });
                });
            }
            catch (Exception ex)
            {
                Tracers.Settings.TraceAdminLogV2Message(
                    TraceEventType.Error, "Exception: {0} while creating ReplicationLogFolderPath and Telemetry Folders", ex);
            }
            return true;
        }

        private static async Task<(ProcessServerSettings psSettings, object rawSettingsObj, bool notRegistered)> MasterRcmSettingsRetrieverAsync(
            object assistObj,
            object lastSuccRawSettingsObj,
            CancellationToken ct)
        {
            // Last "successfully merged" settings
            RcmContract.ProcessServerSettings lastMergedRcmPSSettings;
            // Current "retrieved, differential" settings from RCM
            RcmContract.ProcessServerSettings currRetrievedRcmPSSettings;
            // Current "successfully merged" settings
            RcmContract.ProcessServerSettings currMergedRcmPSSettings;
            // Common format PS settings "processed" out of currMergedRcmPSSettings
            ProcessServerSettings currPSSettings;

            RcmOperationContext opContext = new RcmOperationContext();

            try
            {
                lastMergedRcmPSSettings = (RcmContract.ProcessServerSettings)lastSuccRawSettingsObj;
                RcmJobProcessor jobProcessor = (RcmJobProcessor)assistObj;

                GetProcessServerSettingsInput input = new GetProcessServerSettingsInput()
                {
                    SettingsSyncTrackingId = lastMergedRcmPSSettings?.SettingsSyncTrackingId,
                    SourceMachineIds = lastMergedRcmPSSettings?.SourceMachineIds
                };

                if (Settings.Default.ProcessServerSettings_Rcm_StoreSettingsMergeDebugData)
                {
                    var debugDir = Path.Combine(
                        Path.GetDirectoryName(PSInstallationInfo.Default.GetPSCachedSettingsFilePath()),
                        "RcmTracking");

                    if (!Directory.Exists(debugDir))
                        FSUtils.CreateAdminsAndSystemOnlyDir(debugDir);

                    var debugPath = Path.Combine(debugDir, $"TrackingInput_{DateTime.UtcNow.Ticks}.txt");
                    File.WriteAllText(debugPath, input.ToString());
                }

                using (var stubs = RcmApiFactory.GetProcessServerRcmApiStubs(PSConfiguration.Default))
                {
                    currRetrievedRcmPSSettings =
                        await stubs.GetProcessServerSettingsAsync(opContext, input, ct)
                        .ConfigureAwait(false);
                }

                currPSSettings = new RcmProcessServerSettings(
                    prevSettings: lastMergedRcmPSSettings,
                    currSettings: currRetrievedRcmPSSettings,
                    mergedSettings: out currMergedRcmPSSettings,
                    jobProcessor: jobProcessor);
            }
            catch (RcmException rex) when (rex.ErrorCode == RcmErrorCode.ApplianceComponentDoesNotExist.ToString())
            {
                return (psSettings: null, rawSettingsObj: null, notRegistered: true);
            }
            catch (Exception ex)
            {
                Tracers.Settings.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    opContext,
                    "Error in retrieving the PS settings from RCM{0}{1}",
                    Environment.NewLine, ex);

                // On any error in the enclosed logic, the tracking session
                // id must be reset and all the settings must be retrieved
                // from RCM again.
                return (psSettings: null, rawSettingsObj: null, notRegistered: false);
            }

            // Updating the tracking id (indirectly), which is part of the rawSettingsObj.
            return (psSettings: currPSSettings, rawSettingsObj: currMergedRcmPSSettings, notRegistered: false);
        }

        private class CacheDataHeader
        {
            [JsonIgnore]
            [JsonProperty(PropertyName = "Ignore1")]
            public Version Version => m_version;
            [JsonProperty(PropertyName = nameof(Version))]
            private Version m_version;

            [JsonIgnore]
            [JsonProperty(PropertyName = "Ignore2")]
            public string Checksum => m_checksum;
            [JsonProperty(PropertyName = nameof(Checksum))]
            private string m_checksum;

            [JsonIgnore]
            [JsonProperty(PropertyName = "Ignore3")]
            public string ChecksumType => m_checksumType;
            [JsonProperty(PropertyName = nameof(ChecksumType))]
            private string m_checksumType;

            private const string CHECKSUM_TYPE_MD5 = "MD5";

            // NOTE:
            internal static readonly Version CURRENT_CACHED_DATA_VERSION = new Version(1, 10);

            public static CacheDataHeader BuildHeader(string content)
            {
                return new CacheDataHeader()
                {
                    m_version = CURRENT_CACHED_DATA_VERSION,
                    m_checksum = CryptoUtils.GetMd5Checksum(content),
                    m_checksumType = CHECKSUM_TYPE_MD5
                };
            }

            public bool IsMatchingContent(string content)
            {
                CacheDataHeader comp = BuildHeader(content);

                // TODO-SanKumar-2002: Implement the minor and major version
                // restrictions/relaxations as per settings here too.

                return
                    //(this.Version == null || this.Version.Equals(comp.Version)) &&
                    comp.Checksum.Equals(this.Checksum, StringComparison.Ordinal) &&
                    comp.ChecksumType.Equals(this.ChecksumType, StringComparison.Ordinal);
            }

            public bool Equals(CacheDataHeader comp)
            {
                if (comp == null)
                    return false;

                if ((this.Version == null) != (comp.Version == null) ||
                    (this.Checksum == null) != (comp.Checksum == null) ||
                    (this.ChecksumType == null) != (comp.ChecksumType == null))
                {
                    return false;
                }

                return
                    (this.Version == null || this.Version.Equals(comp.Version)) &&
                    (this.Checksum == null || this.Checksum.Equals(comp.Checksum, StringComparison.Ordinal)) &&
                    (this.ChecksumType == null || this.ChecksumType.Equals(comp.ChecksumType, StringComparison.Ordinal));
            }
        }

        public static async Task RunSecondaryPoller(string settingsFilePath, CancellationToken ct)
        {
            // TODO-SanKumar-1905: The StopWatch object would be resilient to system time changes,
            // only if it is using QPC (StopWatch.IsHighResolution == true). Otherwise, it uses
            // System clock for getting elapsed time. In such cases, we should add mecanism to
            // handle System clock adjustments.
            Stopwatch retrievalTimeTakenSW = new Stopwatch();

            ProcessServerSettings lastSuccSettings;

            (ProcessServerSettings psSettings,
            string knownToFailHeader,
            string knownToFailContent,
            bool fileUnavailable) retrievedData = (null, null, null, false);

            Tracers.Settings.TraceAdminLogV2Message(TraceEventType.Information, "Starting secondary poller");

            // Lightweight lock replacement in Async pattern
            await s_cachedSettingSemaphore.WaitAsync(ct).ConfigureAwait(false);

            try
            {
                for (int cnt = 0; ; cnt++)
                {
                    retrievalTimeTakenSW.Restart();

                    try
                    {
                        retrievedData = await ReadFileCachedPSSettings(
                            settingsFilePath,
                            (retrievedData.knownToFailHeader, retrievedData.knownToFailContent),
                            ct);

                        if (retrievedData.psSettings != null || retrievedData.fileUnavailable)
                        {
                            // This updation block is executed only if the settings
                            // was retrieved successfully or if the file isn't
                            // present in the given path.

                            lastSuccSettings = Interlocked.Exchange(
                                ref s_cachedSettings, retrievedData.psSettings);

                            FinalizeSetting(lastSuccSettings, s_cachedSettings);
                        }
                    }
                    catch (OperationCanceledException) when (ct.IsCancellationRequested)
                    {
                        throw;
                    }
                    catch (Exception ex)
                    {
                        Tracers.Settings.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Error in retrieving the PS settings from cached file{0}{1}",
                            Environment.NewLine, ex);
                    }

                    TimeSpan toWaitTimeSpan = Settings.Default.ProcessServerSettings_CacheExpiryInterval;

                    toWaitTimeSpan -= retrievalTimeTakenSW.Elapsed;
                    if (toWaitTimeSpan < TimeSpan.Zero)
                    {
                        toWaitTimeSpan = TimeSpan.Zero;
                    }

                    await Task.Delay(toWaitTimeSpan, ct).ConfigureAwait(false);
                }
            }
            finally
            {
                s_cachedSettingSemaphore.Release();
            }
        }

        private static async Task<(ProcessServerSettings, string knownToFailHeader, string knownToFailContent, bool fileUnavailable)> ReadFileCachedPSSettings(
            string settingsFilePath,
            (string header, string content) knownToFailSetting,
            CancellationToken ct)
        {
            bool fileUnavailable = true;

            if (!File.Exists(settingsFilePath))
            {
                return (null, null, null, fileUnavailable);
            }

            fileUnavailable = false;

            string headerLine, parsedContent;

            // Read the settings from file
            using (var lockFile =
                await FileUtils.AcquireLockFileAsync(settingsFilePath, exclusive: false, ct: ct)
                .ConfigureAwait(false))
            using (var fs = File.OpenRead(settingsFilePath))
            using (var sr = new StreamReader(fs, Encoding.UTF8))
            {
                headerLine =
                    Settings.Default.ProcessServerSettings_CacheIncludesHeader ?
                    sr.ReadLine() : null;

                parsedContent = sr.ReadToEnd();
            }

            if (((headerLine == null && knownToFailSetting.header == null) ||
                 (headerLine != null && headerLine.Equals(knownToFailSetting.header, StringComparison.Ordinal))) &&
                (parsedContent.Equals(knownToFailSetting.content, StringComparison.Ordinal)))
            {
                // By doing this precheck, we avoid repeated logging for a known
                // bad file, which will keep on failing.

                return (null, knownToFailSetting.header, knownToFailSetting.content, fileUnavailable);

                // TODO-SanKumar-2002: Rate controlled log (say once in 10/25 times) in
                // both +ve and -ve cases.
            }

            if ((Settings.Default.ProcessServerSettings_CacheIncludesHeader && string.IsNullOrWhiteSpace(headerLine) ||
                (string.IsNullOrWhiteSpace(parsedContent))))
            {
                Tracers.Settings.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Header line or content missing in the cache settings file");

                return (null, headerLine, parsedContent, fileUnavailable);
            }

            if ((Settings.Default.ProcessServerSettings_CacheIncludesHeader) &&
                (Settings.Default.ProcessServerSettings_CacheEnforceMajorVersionCheck ||
                 Settings.Default.ProcessServerSettings_CacheVerifyHeader))
            {
                CacheDataHeader parsedHeader;
                try
                {
                    parsedHeader = JsonConvert.DeserializeObject<CacheDataHeader>(
                            headerLine, s_cachedSettingsFileSerializerSettingsNoIndent);
                }
                catch (Exception ex)
                {
                    Tracers.Settings.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Json serialization failed for the header in the cached settings file{0}{1}",
                        Environment.NewLine, ex);

                    return (null, headerLine, parsedContent, fileUnavailable);
                }

                if (parsedHeader == null)
                {
                    Tracers.Settings.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Header JSON object is null");

                    return (null, headerLine, parsedContent, fileUnavailable);
                }

                if (Settings.Default.ProcessServerSettings_CacheEnforceMajorVersionCheck)
                {
                    if (parsedHeader.Version.Major != CacheDataHeader.CURRENT_CACHED_DATA_VERSION.Major)
                    {
                        Tracers.Settings.TraceAdminLogV2Message(
                                        TraceEventType.Error,
                                        "Major version of the cached settings file - {0} doesn't match the expected major version - {1}",
                                        parsedHeader.Version, CacheDataHeader.CURRENT_CACHED_DATA_VERSION);

                        return (null, headerLine, parsedContent, fileUnavailable);
                    }

                    if (Settings.Default.ProcessServerSettings_CacheEnforceMinorVersionCheck &&
                        parsedHeader.Version.Minor != CacheDataHeader.CURRENT_CACHED_DATA_VERSION.Minor)
                    {
                        Tracers.Settings.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Minor version of the cached settings file - {0} doesn't match the expected minor version - {1}",
                            parsedHeader.Version, CacheDataHeader.CURRENT_CACHED_DATA_VERSION);

                        return (null, headerLine, parsedContent, fileUnavailable);
                    }
                }

                if (Settings.Default.ProcessServerSettings_CacheVerifyHeader &&
                    !parsedHeader.IsMatchingContent(parsedContent))
                {
                    Tracers.Settings.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Checksum validation failed for the cached settings file - {0} ({1})",
                        parsedHeader.Checksum, parsedHeader.ChecksumType);

                    return (null, headerLine, parsedContent, fileUnavailable);
                }
            }

            ProcessServerSettings fileCachedPSSettings;
            try
            {
                fileCachedPSSettings =
                    JsonConvert.DeserializeObject<CachedProcessServerSettings>(
                        parsedContent, s_cachedSettingsFileSerializerSettingsNoIndent);
            }
            catch (Exception ex)
            {
                Tracers.Settings.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Json serialization failed for the content in the cached settings file{0}{1}",
                    Environment.NewLine, ex);

                return (null, headerLine, parsedContent, fileUnavailable);
            }

            // TODO-SanKumar-2002: By setting header and content values even in +ve case, we could
            // avoid unnecessary JSON parsings as well as callbacks at the caller. Adverse
            // effect would be that if any failed callbacks / other intermittent logic
            // errors wouldn't be retried.
            return (fileCachedPSSettings, null, null, fileUnavailable);
        }

        #endregion Cache handling

        #region Change Notification

        public enum ChangeType
        {
            Added,
            Removed,
            Modified
        }

        private class ProcessServerHostSettingsEqualityComparer : IEqualityComparer<IProcessServerHostSettings>
        {
            public bool Equals(IProcessServerHostSettings x, IProcessServerHostSettings y)
            {
                return x.HostId.Equals(y.HostId);
            }

            public int GetHashCode(IProcessServerHostSettings obj)
            {
                return obj.HostId.GetHashCode();
            }
        }

        private static void FinalizeSetting(ProcessServerSettings oldSettings, ProcessServerSettings newSettings)
        {
            if (oldSettings == null && newSettings == null)
                return;

            // TODO-SanKumar-2005: We raise this event, whenever a new setting
            // has arrived, irrespective of the fact that it is same as the previous
            // setting or not. This could be improved to only be raised on change
            // but that would need investment and we don't have an explicit
            // requirement right now.
            PSSettingsChanged?.Invoke(
                null, new ProcessServerSettingsChangedEventArgs(oldSettings, newSettings));

            // TODO-SanKumar-Later: Raise these events here - HostSettingsChanged
            // and PairSettingsChanged, if there's necessity.

            if (oldSettings?.ApplicationInsightsInstrumentationKey != newSettings?.ApplicationInsightsInstrumentationKey)
            {
                AppInsightsKeyChanged?.Invoke(
                    null,
                    new AppInsightsKeyChangedEventArgs(
                        oldSettings?.ApplicationInsightsInstrumentationKey,
                        newSettings?.ApplicationInsightsInstrumentationKey));
            }

            if (oldSettings?.Tunables != null || newSettings?.Tunables != null)
            {
                TunablesChanged?.Invoke(
                    null,
                    new TunablesChangedEventArgs(oldSettings?.Tunables, newSettings?.Tunables));
            }

            if (oldSettings?.CriticalChannelUri != newSettings?.CriticalChannelUri)
            {
                PSCriticalChannelUriChanged?.Invoke(
                    null,
                    new PSChannelUriChangedEventArgs(
                        oldSettings?.CriticalChannelUri,
                        (oldSettings?.CriticalChannelUriRenewalTimeUtc).GetValueOrDefault(),
                        newSettings?.CriticalChannelUri,
                        (newSettings?.CriticalChannelUriRenewalTimeUtc).GetValueOrDefault()));
            }

            if (oldSettings?.InformationalChannelUri != newSettings?.InformationalChannelUri)
            {
                PSInfoChannelUriChanged?.Invoke(
                    null,
                    new PSChannelUriChangedEventArgs(
                        oldSettings?.InformationalChannelUri,
                        (oldSettings?.InformationalChannelUriRenewalTimeUtc).GetValueOrDefault(),
                        newSettings?.InformationalChannelUri,
                        (newSettings?.InformationalChannelUriRenewalTimeUtc).GetValueOrDefault()));
            }

            var hostEqComp = new ProcessServerHostSettingsEqualityComparer();

            var hostsInOldButNotInNew =
                (oldSettings?.Hosts ?? Enumerable.Empty<IProcessServerHostSettings>())
                .Except(
                    newSettings?.Hosts ?? Enumerable.Empty<IProcessServerHostSettings>(), hostEqComp);

            foreach (var currHost in hostsInOldButNotInNew)
            {
                HostCriticalChannelUriChanged?.Invoke(
                    null,
                    new HostChannelUriChangedEventArgs(
                        currHost.HostId,
                        currHost.CriticalChannelUri,
                        currHost.CriticalChannelUriRenewalTimeUtc,
                        null,
                        default(DateTime)));

                HostInfoChannelUriChanged?.Invoke(
                    null,
                    new HostChannelUriChangedEventArgs(
                        currHost.HostId,
                        currHost.InformationalChannelUri,
                        currHost.InformationalChannelUriRenewalTimeUtc,
                        null,
                        default(DateTime)));
            }

            var hostsInNewButNotInOld =
                (newSettings?.Hosts ?? Enumerable.Empty<IProcessServerHostSettings>())
                .Except(
                    oldSettings?.Hosts ?? Enumerable.Empty<IProcessServerHostSettings>(), hostEqComp);

            foreach (var currHost in hostsInNewButNotInOld)
            {
                HostCriticalChannelUriChanged?.Invoke(
                    null,
                    new HostChannelUriChangedEventArgs(
                        currHost.HostId,
                        null,
                        default(DateTime),
                        currHost.CriticalChannelUri,
                        currHost.CriticalChannelUriRenewalTimeUtc));

                HostInfoChannelUriChanged?.Invoke(
                    null,
                    new HostChannelUriChangedEventArgs(
                        currHost.HostId,
                        null,
                        default(DateTime),
                        currHost.InformationalChannelUri,
                        currHost.InformationalChannelUriRenewalTimeUtc));
            }

            var hostIdsInBothLookup = new HashSet<string>(
                (oldSettings?.Hosts ?? Enumerable.Empty<IProcessServerHostSettings>())
                .Select(currHost => currHost.HostId)
                .Intersect(
                    (newSettings?.Hosts ?? Enumerable.Empty<IProcessServerHostSettings>())
                    .Select(currHost => currHost.HostId)));

            if (hostIdsInBothLookup.Count > 0)
            {
                var oldHostsDict = oldSettings.Hosts
                        .Where(currHost => hostIdsInBothLookup.Contains(currHost.HostId))
                        .ToDictionary(keySelector: currHost => currHost.HostId);
                var newHostsDict = newSettings.Hosts
                    .Where(currHost => hostIdsInBothLookup.Contains(currHost.HostId))
                    .ToDictionary(keySelector: currHost => currHost.HostId);

                foreach (var currHostId in hostIdsInBothLookup)
                {
                    var oldHost = oldHostsDict[currHostId];
                    var newHost = newHostsDict[currHostId];

                    if (oldHost.CriticalChannelUri != newHost.CriticalChannelUri)
                    {
                        HostCriticalChannelUriChanged?.Invoke(
                            null,
                            new HostChannelUriChangedEventArgs(
                                currHostId,
                                oldHost.CriticalChannelUri,
                                oldHost.CriticalChannelUriRenewalTimeUtc,
                                newHost.CriticalChannelUri,
                                newHost.CriticalChannelUriRenewalTimeUtc));
                    }

                    if (oldHost.InformationalChannelUri != newHost.InformationalChannelUri)
                    {
                        HostInfoChannelUriChanged?.Invoke(
                            null,
                            new HostChannelUriChangedEventArgs(
                                currHostId,
                                oldHost.InformationalChannelUri,
                                oldHost.InformationalChannelUriRenewalTimeUtc,
                                newHost.InformationalChannelUri,
                                newHost.InformationalChannelUriRenewalTimeUtc));
                    }
                }
            }
        }

        private static ChangeType DetermineChangeType<T>(T oldObj, T newObj) where T : class
        {
            if (oldObj == null)
                return ChangeType.Added;
            else if (newObj == null)
                return ChangeType.Removed;
            else
                return ChangeType.Modified;
        }

        public static event EventHandler<ProcessServerSettingsChangedEventArgs> PSSettingsChanged;
        //public static event EventHandler<HostSetingsChangedEventArgs> HostSettingsChanged;
        //public static event EventHandler<PairSettingsChangedEventArgs> PairSettingsChanged;

        public static event EventHandler<PSChannelUriChangedEventArgs> PSCriticalChannelUriChanged;
        public static event EventHandler<PSChannelUriChangedEventArgs> PSInfoChannelUriChanged;
        public static event EventHandler<AppInsightsKeyChangedEventArgs> AppInsightsKeyChanged;
        public static event EventHandler<TunablesChangedEventArgs> TunablesChanged;

        public static event EventHandler<HostChannelUriChangedEventArgs> HostCriticalChannelUriChanged;
        public static event EventHandler<HostChannelUriChangedEventArgs> HostInfoChannelUriChanged;

        public class ProcessServerSettingsChangedEventArgs : EventArgs
        {
            internal ProcessServerSettingsChangedEventArgs(
                ProcessServerSettings oldSettings,
                ProcessServerSettings newSettings)
            {
                this.OldSettings = oldSettings;
                this.NewSettings = newSettings;
                this.ChangeType = DetermineChangeType(oldSettings, newSettings);
            }

            public ProcessServerSettings OldSettings { get; }
            public ProcessServerSettings NewSettings { get; }
            public ChangeType ChangeType { get; }
        }

        public class HostSetingsChangedEventArgs : EventArgs
        {
            internal HostSetingsChangedEventArgs(
                string hostId,
                IProcessServerHostSettings oldSettings,
                IProcessServerHostSettings newSettings)
            {
                this.HostId = hostId;
                this.OldSettings = oldSettings;
                this.NewSettings = newSettings;
                this.ChangeType = DetermineChangeType(oldSettings, newSettings);
            }
            public string HostId { get; }
            public IProcessServerHostSettings OldSettings { get; }
            public IProcessServerHostSettings NewSettings { get; }
            public ChangeType ChangeType { get; }
        }

        public class PairSettingsChangedEventArgs : EventArgs
        {
            internal PairSettingsChangedEventArgs(
                string hostId,
                string diskId,
                IProcessServerPairSettings oldSettings,
                IProcessServerPairSettings newSettings)
            {
                this.HostId = hostId;
                this.DiskId = diskId;
                this.OldSettings = oldSettings;
                this.NewSettings = newSettings;
                this.ChangeType = DetermineChangeType(oldSettings, newSettings);
            }

            public string HostId { get; }
            public string DiskId { get; }
            public IProcessServerPairSettings OldSettings { get; }
            public IProcessServerPairSettings NewSettings { get; }
            public ChangeType ChangeType { get; }
        }

        public class PSChannelUriChangedEventArgs : EventArgs
        {
            internal PSChannelUriChangedEventArgs(
                string oldChannelUri,
                DateTime oldRenewalTimeUtc,
                string newChannelUri,
                DateTime newRenewalTimeUtc)
            {
                this.OldChannelUri = oldChannelUri;
                this.OldRenewalTimeUtc = oldRenewalTimeUtc;
                this.NewChannelUri = newChannelUri;
                this.NewRenewalTimeUtc = newRenewalTimeUtc;
                this.ChangeType = DetermineChangeType(oldChannelUri, newChannelUri);
            }

            public string OldChannelUri { get; }
            public DateTime OldRenewalTimeUtc { get; }
            public string NewChannelUri { get; }
            public DateTime NewRenewalTimeUtc { get; }
            public ChangeType ChangeType { get; }
        }

        public class HostChannelUriChangedEventArgs : PSChannelUriChangedEventArgs
        {
            internal HostChannelUriChangedEventArgs(
                string hostId,
                string oldChannelUri,
                DateTime oldRenewalTimeUtc,
                string newChannelUri,
                DateTime newRenewalTimeUtc)
                : base(oldChannelUri, oldRenewalTimeUtc, newChannelUri, newRenewalTimeUtc)
            {
                this.HostId = hostId;
            }

            public string HostId { get; }
        }

        public class AppInsightsKeyChangedEventArgs : EventArgs
        {
            internal AppInsightsKeyChangedEventArgs(string oldAppInsightsKey, string newAppInsightsKey)
            {
                this.OldAppInsightsKey = oldAppInsightsKey;
                this.NewAppInsightsKey = newAppInsightsKey;
                this.ChangeType = DetermineChangeType(oldAppInsightsKey, newAppInsightsKey);
            }

            public string OldAppInsightsKey { get; }
            public string NewAppInsightsKey { get; }
            public ChangeType ChangeType { get; }
        }

        public class TunablesChangedEventArgs : EventArgs
        {
            internal TunablesChangedEventArgs(
                IReadOnlyDictionary<string, string> oldTunables,
                IReadOnlyDictionary<string, string> newTunables)
            {
                this.OldTunables = oldTunables;
                this.NewTunables = newTunables;
                this.ChangeType = DetermineChangeType(oldTunables, newTunables);
            }

            public IReadOnlyDictionary<string, string> OldTunables { get; }
            public IReadOnlyDictionary<string, string> NewTunables { get; }
            public ChangeType ChangeType { get; }
        }

        #endregion Change Notification
    }
}
