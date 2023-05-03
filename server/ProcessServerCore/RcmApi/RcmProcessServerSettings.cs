using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using RcmContract;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core
{
    internal class RcmProcessServerSettings : CSApi.ProcessServerSettings
    {
        internal static readonly JsonSerializerSettings s_rcmSettingsSerSettings
            = JsonUtils.GetStandardSerializerSettings(indent: false);

        public RcmProcessServerSettings(
            RcmContract.ProcessServerSettings prevSettings,
            RcmContract.ProcessServerSettings currSettings,
            out RcmContract.ProcessServerSettings mergedSettings,
            RcmJobProcessor jobProcessor)
        {
            string GetDebugDir()
            {
                if (!Settings.Default.ProcessServerSettings_Rcm_StoreSettingsMergeDebugData)
                    return null;

                var rcmTrackingDir = Path.Combine(
                    Path.GetDirectoryName(PSInstallationInfo.Default.GetPSCachedSettingsFilePath()),
                    "RcmTracking");

                if (!Directory.Exists(rcmTrackingDir))
                    FSUtils.CreateAdminsAndSystemOnlyDir(rcmTrackingDir);

                return rcmTrackingDir;
            }

            if (currSettings == null)
                throw new ArgumentNullException(nameof(currSettings));

            if (jobProcessor == null)
                throw new ArgumentNullException(nameof(jobProcessor));

            Debug.Assert(currSettings.ControlPathTransportSettings == string.Empty);
            Debug.Assert(
                JsonConvert.DeserializeObject<RcmEnum.ControlPathTransport>($"\"{currSettings.ControlPathTransportType}\"", new StringEnumConverter())
                == RcmEnum.ControlPathTransport.RcmControlPlane);

            this.ApplicationInsightsInstrumentationKey = currSettings.ApplicationInsightsInstrumentationKey;

            var monSettings = currSettings.ComponentMonitoringMessageSettings;

            if (monSettings != null)
            {
                this.CriticalChannelUri = monSettings.CriticalEventHubUri;
                this.InformationalChannelUri = monSettings.InformationEventHubUri;
                this.CriticalChannelUriRenewalTimeUtc
                    = this.InformationalChannelUriRenewalTimeUtc
                    = new DateTime(monSettings.SasUriRenewalTimeUtc, DateTimeKind.Utc);
            }

            var telSettings = currSettings.TelemetrySettings;
            if (telSettings != null)
            {
                this.TelemetryChannelUri = telSettings.KustoEventHubUri;
                this.TelemetryChannelUriRenewalTimeUtc = telSettings.SasUriRenewalTimeUtc;
                this.ProtectedMachineTelemetrySettings = telSettings.TelemetrySettings?.Select(
                    currPair => new CSApi.ProtectedMachineTelemetrySettings(
                        hostId: currPair.Key,
                        telemetryFolderPath: currPair.Value?.TelemetryFolder,
                        biosId: currPair.Value?.SourceMachineBiosId));
            }

            string debugDir;
            if ((debugDir = GetDebugDir()) != null)
            {
                var debugPath = Path.Combine(debugDir, $"TelemetrySettings_{DateTime.UtcNow.Ticks}.txt");
                File.WriteAllText(debugPath, telSettings?.ToString());
            }

            this.MessageContext = currSettings.ComponentMsgContext;

            this.IsPrivateEndpointEnabled = currSettings.IsPrivateEndpointEnabled;

            this.Tunables = currSettings.Tunables;

            if (currSettings.ProtectionDisabledSourceMachineIds != null)
            {
                jobProcessor.RemoveAllJobs(currSettings.ProtectionDisabledSourceMachineIds);
                jobProcessor.DeleteDisabledReplicationFolderAsync(currSettings.ProtectionDisabledSourceMachineIds);
            }

            if (currSettings.DisassociatedSourceMachineIds != null)
            {
                jobProcessor.RemoveAllJobs(currSettings.DisassociatedSourceMachineIds);
                jobProcessor.DeleteDisabledReplicationFolderAsync(currSettings.DisassociatedSourceMachineIds);
            }

            if ((debugDir = GetDebugDir()) != null)
            {
                string debugPath;
                debugPath = Path.Combine(debugDir, $"CurrSettings_{DateTime.UtcNow.Ticks}.txt");
                File.WriteAllText(debugPath, JsonConvert.SerializeObject(currSettings?.ReplicationSettings, Formatting.Indented));
                debugPath = Path.Combine(debugDir, $"PrevSettings_{DateTime.UtcNow.Ticks}.txt");
                File.WriteAllText(debugPath, JsonConvert.SerializeObject(prevSettings?.ReplicationSettings, Formatting.Indented));
                debugPath = Path.Combine(debugDir, $"CurrTracking_{DateTime.UtcNow.Ticks}.txt");
                File.WriteAllText(debugPath, currSettings?.SettingsSyncTrackingId);
                debugPath = Path.Combine(debugDir, $"PrevTracking_{DateTime.UtcNow.Ticks}.txt");
                File.WriteAllText(debugPath, prevSettings?.SettingsSyncTrackingId);
                debugPath = Path.Combine(debugDir, $"CurrSrcMacList_{DateTime.UtcNow.Ticks}.txt");
                File.WriteAllText(debugPath, string.Join(",", currSettings?.SourceMachineIds ?? Enumerable.Empty<string>()));
                debugPath = Path.Combine(debugDir, $"PrevSrcMacList_{DateTime.UtcNow.Ticks}.txt");
                File.WriteAllText(debugPath, string.Join(",", prevSettings?.SourceMachineIds ?? Enumerable.Empty<string>()));
                debugPath = Path.Combine(debugDir, $"CurrDisassMacList_{DateTime.UtcNow.Ticks}.txt");
                File.WriteAllText(debugPath, string.Join(",", currSettings?.DisassociatedSourceMachineIds ?? Enumerable.Empty<string>()));
                debugPath = Path.Combine(debugDir, $"PrevDisassMacList_{DateTime.UtcNow.Ticks}.txt");
                File.WriteAllText(debugPath, string.Join(",", prevSettings?.DisassociatedSourceMachineIds ?? Enumerable.Empty<string>()));
                debugPath = Path.Combine(debugDir, $"CurrDisabledMacList_{DateTime.UtcNow.Ticks}.txt");
                File.WriteAllText(debugPath, string.Join(",", currSettings?.ProtectionDisabledSourceMachineIds ?? Enumerable.Empty<string>()));
                debugPath = Path.Combine(debugDir, $"PrevDisabledMacList_{DateTime.UtcNow.Ticks}.txt");
                File.WriteAllText(debugPath, string.Join(",", prevSettings?.ProtectionDisabledSourceMachineIds ?? Enumerable.Empty<string>()));
            }

            mergedSettings =
                RcmDataContractUtils<RcmContract.ProcessServerSettings>.RcmDeepClone(currSettings);

            if (mergedSettings.ReplicationSettings == null)
                mergedSettings.ReplicationSettings = new Dictionary<string, ProcessServerReplicationSettings>();

            if (prevSettings != null && prevSettings.ReplicationSettings != null)
            {
                foreach (var currHostKVPair in prevSettings.ReplicationSettings)
                {
                    var hostId = currHostKVPair.Key;
                    var replSettings = currHostKVPair.Value;

                    if (!(mergedSettings.ProtectionDisabledSourceMachineIds?.Contains(hostId)).GetValueOrDefault() &&
                        !(mergedSettings.DisassociatedSourceMachineIds?.Contains(hostId)).GetValueOrDefault() &&
                        !mergedSettings.ReplicationSettings.ContainsKey(hostId))
                    {
                        mergedSettings.ReplicationSettings.Add(hostId, replSettings);
                    }
                }
            }

            var hostSettingsList =
                new List<RcmProcessServerHostSettings>(mergedSettings.ReplicationSettings.Count);

            foreach (var currHostKVPair in mergedSettings.ReplicationSettings)
            {
                var hostId = currHostKVPair.Key;
                var replSettings = currHostKVPair.Value;

                jobProcessor.ProcessIncomingJobs(hostId, replSettings.RcmJobs);

                hostSettingsList.Add(new RcmProcessServerHostSettings(hostId, replSettings));
            }

            string hostBiosId = null;
            if (hostSettingsList != null && hostSettingsList.Count > 0)
            {
                var hostObj = hostSettingsList.Where(
                    currItem => currItem != null && currItem.BiosId != null).FirstOrDefault();
                hostBiosId = (hostObj != null) ? hostObj.BiosId : null;
            }

            string telBiosId = null;
            if (this.ProtectedMachineTelemetrySettings != null && this.ProtectedMachineTelemetrySettings.Any())
            {
                var telObj = this.ProtectedMachineTelemetrySettings.Where(
                    currItem => currItem != null && currItem.BiosId != null).FirstOrDefault();
                telBiosId = (telObj != null) ? telObj.BiosId : null;
            }

            IsAccessControlEnabled = true;
            // Disable access control if there is atleast one protected VM and biosid is not set or
            // if ProcessServerSettings_Rcm_EnableAccessControl setting is not enabled
            if ((hostBiosId == null && telBiosId == null && hostSettingsList.Count > 0) ||
                    !Settings.Default.ProcessServerSettings_Rcm_EnableAccessControl)
            {
                IsAccessControlEnabled = false;
            }

            this.Hosts = hostSettingsList;

            if ((debugDir = GetDebugDir()) != null)
            {
                string debugPath = Path.Combine(debugDir, $"MergedSettings_{DateTime.UtcNow.Ticks}.txt");
                File.WriteAllText(debugPath, JsonConvert.SerializeObject(this, Formatting.Indented));
            }
        }

        #region Base abstract members implementation

        // Agents upload directly to their target folder in RCM mode.
        public override bool IsSourceFolderSupported => false;

        public override IEnumerable<IProcessServerHostSettings> Hosts { get; }

        public override decimal CumulativeThrottleLimit =>
            Settings.Default.ProcessServerSettings_Rcm_DefaultCumulativeThrottleLimit;

        public override LogRotationState LogRotationState => LogRotationState.Unknown;

        public override int LogFilesBackupCounter => 0;

        public override string ApplicationInsightsInstrumentationKey { get; }

        public override string CriticalChannelUri { get; }

        public override DateTime CriticalChannelUriRenewalTimeUtc { get; }

        public override string InformationalChannelUri { get; }

        public override DateTime InformationalChannelUriRenewalTimeUtc { get; }

        public override string TelemetryChannelUri { get; }

        public override DateTime TelemetryChannelUriRenewalTimeUtc { get; }

        public override IEnumerable<IProtectedMachineTelemetrySettings> ProtectedMachineTelemetrySettings { get; }

        public override string MessageContext { get; }

        public override SystemHealth SystemHealth => SystemHealth.healthy;

        public override IEnumerable<ILogRotationSettings> Logs => null;

        public override IEnumerable<IPolicySettings> PolicySettings => null;

        public override IReadOnlyDictionary<string, string> Tunables { get; }

        public override bool IsCummulativeThrottled => false;

        public override string CSHostId => string.Empty;

        public override bool IsPrivateEndpointEnabled { get; }

        public override bool IsAccessControlEnabled { get; }

        #endregion Base abstract members implementation
    }

    internal class RcmProcessServerHostSettings : IProcessServerHostSettings
    {
        public RcmProcessServerHostSettings(string hostId, ProcessServerReplicationSettings replSettings)
        {
            this.HostId = hostId;

            if (replSettings != null)
            {
                var monMsgSettings = replSettings.VmMonitoringMsgSettings;

                if (monMsgSettings != null)
                {
                    this.CriticalChannelUri = monMsgSettings.CriticalEventHubUri;
                    this.CriticalChannelUriRenewalTimeUtc =
                        new DateTime((monMsgSettings.SasUriRenewalTimeUtc), DateTimeKind.Utc);

                    this.InformationalChannelUri = monMsgSettings.InformationEventHubUri;
                    this.InformationalChannelUriRenewalTimeUtc =
                        new DateTime((monMsgSettings.SasUriRenewalTimeUtc), DateTimeKind.Utc);
                }

                this.MessageContext = replSettings.ProtectionPairContext;

                List<RcmProcessServerPairSettings> pairSettingsList =
                    new List<RcmProcessServerPairSettings>(replSettings.ProtectedDiskIds.Count);

                ProcessServerMonitorProtectionHealthInput monitorHealthActionInput = null;
                foreach (var currAction in replSettings.RcmProtectionPairActions)
                {
                    if (Enum.TryParse<RcmActionEnum.ProcessServerProtectionPairActionType>(currAction.ActionType, out var actionType) &&
                        actionType == RcmActionEnum.ProcessServerProtectionPairActionType.MonitorProtectionHealth &&
                        currAction.InputPayload != null)
                    {
                        if (monitorHealthActionInput != null)
                        {
                            throw new InvalidDataException(
                                "More than one " +
                                RcmActionEnum.ProcessServerProtectionPairActionType.MonitorProtectionHealth +
                                $" action received for HostId:{hostId}");
                        }

                        monitorHealthActionInput =
                            JsonConvert.DeserializeObject<ProcessServerMonitorProtectionHealthInput>(
                                currAction.InputPayload, RcmProcessServerSettings.s_rcmSettingsSerSettings);
                        this.ProtectionState = monitorHealthActionInput.ProtectionState;
                        this.LogRootFolder = monitorHealthActionInput.LogRootFolder;
                        this.BiosId = monitorHealthActionInput.SourceMachineBiosId;
                    }
                    else
                    {
                        Tracers.Settings.TraceAdminLogV2Message(
                            TraceEventType.Warning,
                            "Unrecognized/incorrect action - {0}. Input - {1}",
                            currAction.ActionType, currAction.InputPayload);
                    }
                }

                if (monitorHealthActionInput != null)
                {
                    Debug.Assert(
                        monitorHealthActionInput.ReplicationMonitoringInputs
                        .Select(input => input.DiskId)
                        .Except(replSettings.ProtectedDiskIds)
                        .Any() == false);

                    foreach (var currProtDiskId in replSettings.ProtectedDiskIds)
                    {
                        string currProtDiskMsgCtxt = null;
                        replSettings.ReplicationPairMessageContexts?.TryGetValue(currProtDiskId, out currProtDiskMsgCtxt);

                        ProcessServerMonitorReplicationHealthInput replMonitoringInput =
                            monitorHealthActionInput.ReplicationMonitoringInputs
                            .Where(input => input.DiskId == currProtDiskId)
                            .SingleOrDefault();

                        pairSettingsList.Add(
                            new RcmProcessServerPairSettings(
                                hostId: hostId,
                                deviceId: currProtDiskId,
                                logRootFolder: this.LogRootFolder,
                                monitorActionInput: replMonitoringInput,
                                msgContext: currProtDiskMsgCtxt));
                    }
                }

                this.Pairs = pairSettingsList;
            }
        }

        public string HostId { get; }

        public string ProtectionState { get; }

        public string LogRootFolder { get; }

        public IEnumerable<IProcessServerPairSettings> Pairs { get; }

        public string CriticalChannelUri { get; }

        public DateTime CriticalChannelUriRenewalTimeUtc { get; }

        public string InformationalChannelUri { get; }

        public DateTime InformationalChannelUriRenewalTimeUtc { get; }

        public string MessageContext { get; }

        public string BiosId { get; }
    }

    class RcmProcessServerPairSettings : IProcessServerPairSettings
    {
        public RcmProcessServerPairSettings(
            string hostId,
            string deviceId,
            string logRootFolder,
            ProcessServerMonitorReplicationHealthInput monitorActionInput,
            string msgContext)
        {
            this.HostId = hostId;
            this.DeviceId = deviceId;

            this.ProtectionState = ProtectionState.Unknown;

            if (monitorActionInput != null)
            {
                Debug.Assert(monitorActionInput.DiskId == deviceId);

                if (string.IsNullOrWhiteSpace(logRootFolder))
                {
                    throw new ArgumentNullException(
                        paramName: nameof(logRootFolder),
                        message: $"{nameof(logRootFolder)} found to be empty for HostId:{hostId}, DeviceId:{deviceId}");
                }

                var optimalPath = Settings.Default.ProcessServerSettings_Rcm_LongPathOptimalConversion;

                this.TargetDiffFolder = Path.Combine(logRootFolder, monitorActionInput.DiffSyncSubFolderPath);
                this.DiffFoldersToMonitor = new[] { Path.Combine(logRootFolder, this.TargetDiffFolder) };
                this.ReplicationSessionId = monitorActionInput.ReplicationSessionId;
                this.ReplicationState = monitorActionInput.ReplicationState;
                this.ResyncFolder = this.ResyncFolderToMonitor =
                    Path.Combine(logRootFolder, monitorActionInput.SyncSubFolderPath);

                if (this.TargetDiffFolder != null)
                {
                    this.AzurePendingUploadRecoverableFolder = FSUtils.GetLongPath(
                        Path.Combine(this.TargetDiffFolder, CSApi.ProcessServerSettings.AZ_PEND_UPL_REC_SUB_FOLDER),
                        isFile: false, optimalPath: optimalPath);

                    this.AzurePendingUploadNonRecoverableFolder = FSUtils.GetLongPath(
                        Path.Combine(this.TargetDiffFolder, CSApi.ProcessServerSettings.AZ_PEND_UPL_NONREC_SUB_FOLDER),
                        isFile: false, optimalPath: optimalPath);

                    this.RPOFilePath = FSUtils.GetLongPath(
                        Path.Combine(this.TargetDiffFolder, CSApi.ProcessServerSettings.MONITOR_TXT),
                        isFile: true, optimalPath: optimalPath);

                    this.TagFolder = FSUtils.GetLongPath(
                        Path.Combine(this.TargetDiffFolder, CSApi.ProcessServerSettings.TAGS_SUB_FOLDER),
                        isFile: false, optimalPath: optimalPath);
                }

                if (!string.IsNullOrWhiteSpace(monitorActionInput.ReplicationState))
                {
                    if (string.Equals(
                        monitorActionInput.ReplicationState,
                        RcmEnum.ReplicationPairState.DiffSync.ToString(),
                        StringComparison.OrdinalIgnoreCase))
                    {
                        this.ProtectionState = ProtectionState.DifferentialSync;
                    }
                    else
                    {
                        this.ProtectionState = ProtectionState.ResyncStep1;
                    }

                    this.DiffThrottleLimit = Settings.Default.ProcessServerSettings_Rcm_DefaultDiffThrottleLimit;
                    // If pair state is not DiffSync and PreDiffSync and DefaultDiffThrottleLimit is not set in config then make DiffThrottleLimit as 1GB
                    if (!string.Equals(monitorActionInput.ReplicationState, RcmEnum.ReplicationPairState.DiffSync.ToString(), StringComparison.OrdinalIgnoreCase) &&
                        !string.Equals(monitorActionInput.ReplicationState, RcmEnum.ReplicationPairState.PreDiffSync.ToString(), StringComparison.OrdinalIgnoreCase))
                    {
                        this.DiffThrottleLimit = Settings.Default.ProcessServerSettings_DiffThrottleLimitInBytesForPairInResync;
                    }
                }
            }

            this.MessageContext = msgContext;
        }

        public string HostId { get; }

        public string DeviceId { get; }

        // As of now, it is always forward protection through the RCM PS.
        public ProtectionDirection ProtectionDirection => ProtectionDirection.Forward;

        public string SourceDiffFolder => null;

        public string TargetDiffFolder { get; }

        public string ResyncFolder { get; }

        public string AzurePendingUploadRecoverableFolder { get; }

        public string AzurePendingUploadNonRecoverableFolder { get; }

        public long DiffThrottleLimit { get; }

        public long ResyncThrottleLimit => Settings.Default.ProcessServerSettings_Rcm_DefaultResyncThrottleLimit;

        public string RPOFilePath { get; }

        public IEnumerable<string> DiffFoldersToMonitor { get; }

        public string ResyncFolderToMonitor { get; }

        public string ReplicationSessionId { get; }

        public string ReplicationState { get; }

        // Not received as PS settings in RCM mode.
        public DateTime IrProgressUpdateTimeUtc => new DateTime(1601, 1, 1, 0, 0, 0, DateTimeKind.Utc);

        public string TagFolder { get; }

        public string MessageContext { get; }

        public ProtectionState ProtectionState { get; }

        // There's no concept of paused in the Rcm stack (..yet)
        public TargetReplicationStatus TargetReplicationStatus => TargetReplicationStatus.NonPaused;

        public long JobId => 0;

        public EnableCompression EnableCompression => EnableCompression.NoCompression;

        public ExecutionState ExecutionState => ExecutionState.Default;

        public int ReplicationStatus => 0;

        public ulong RestartResyncCounter => 0;

        public ulong ResyncStartTime => 0;

        public ulong ResyncEndTime => 0;

        public uint tmid => 0;

        public FarLineProtected FarlineProtected => FarLineProtected.Protected;

        public int DoResync => 0;

        public string TargetHostId => null;

        public string TargetDeviceId => null;

        public string LunId => null;

        public LunState LunState => LunState.Protected;

        public long ResyncStartTagtime => 0;

        public long ResyncEndTagTime => 0;

        public long AutoResyncStartTime => 0;

        public long ResyncSetCxtimestamp => 0;

        public string Hostname => null;

        public string PerfFolder => null;

        public uint CompatibilityNumber => 0;

        public uint TargetCompatibilityNumber => 0;

        public int PairId => 0;

        public long TargetLastHostUpdateTime => 0;

        public string ReplicationCleanupOptions => null;

        public sbyte SourceCommunicationBlocked => 0;

        public int RPOThreshold => 0;

        public long ControlPlaneTimestamp => 0;

        public long StatusUpdateTimestamp => 0;

        public TargetReplicationStatus SourceReplicationStatus => TargetReplicationStatus.NonPaused;

        public long SourceLastHostUpdateTime => 0;

        public long SourceAppAgentLastHostUpdateTime => 0;

        public sbyte IsResyncRequired => 0;

        public PairDeletionStatus PairDeleted => PairDeletionStatus.NotDeleted;

        public ThrottleState ThrottleDifferentials => ThrottleState.NotThrottled;

        public ThrottleState ThrottleResync => ThrottleState.NotThrottled;

        public string TargetHostName => null;

        public AutoResyncStartType AutoResyncStartType => AutoResyncStartType.NotSet;

        public int AutoResyncStartHours => 0;

        public int AutoResyncStartMinutes => 0;

        public int AutoResyncStopHours => 0;

        public int AutoResyncStopMinutes => 0;

        public int ResyncOrder => 0;

        public int CurrentHour => 0;

        public int CurrentMinutes => 0;

        public int ScenarioId => 0;

        public int IsVisible => 0;

        public string SourceAgentVersion => string.Empty;

        public string PSVersion => PSInstallationInfo.Default.GetPSCurrentVersion();

        public long CurrentRPOTime => 0;

        public long StatusUpdateTime => 0;

        public string PerfShortFolder => null;
    }
}
