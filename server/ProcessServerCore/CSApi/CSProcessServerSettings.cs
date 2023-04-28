using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.XmlElements;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi
{

    internal class CSProcessServerSettings : ProcessServerSettings
    {
        private readonly IReadOnlyDictionary<string, CSProcessServerHostSettings> m_dict;
        private readonly string m_csUri;

        public CSProcessServerSettings(FunctionResponseElement xmlResponse, Uri csUri)
        {
            m_csUri = csUri.AbsoluteUri;

            string baseFolder = PSInstallationInfo.Default.GetReplicationLogFolderPath();

            var pairData = xmlResponse.ParameterGroupList.SingleOrDefault(
                paramGrp => paramGrp.Id.Equals("pair_data", StringComparison.Ordinal));

            var tbsData = xmlResponse.ParameterGroupList.Single(
                paramGrp => paramGrp.Id.Equals("tbs_data", StringComparison.Ordinal));

            var lvData = xmlResponse.ParameterGroupList.SingleOrDefault(
                paramGrp => paramGrp.Id.Equals("lv_data", StringComparison.Ordinal));

            var hostData = xmlResponse.ParameterGroupList.SingleOrDefault(
                paramGrp => paramGrp.Id.Equals("host_data", StringComparison.Ordinal));

            var sysHealth = xmlResponse.ParameterGroupList.SingleOrDefault(
                paramGrp => paramGrp.Id.Equals("sys_health", StringComparison.Ordinal));

            var logRotData = xmlResponse.ParameterGroupList.SingleOrDefault(
                paramGrp => paramGrp.Id.Equals("log_rot_data", StringComparison.Ordinal));

            var policyData = xmlResponse.ParameterGroupList.SingleOrDefault(
                paramGrp => paramGrp.Id.Equals("policy_data", StringComparison.Ordinal));

            var psData = xmlResponse.ParameterGroupList.SingleOrDefault(
                paramGrp => paramGrp.Id.Equals("ps_data", StringComparison.Ordinal));

            if (pairData == null)
            {
                m_dict = new Dictionary<string, CSProcessServerHostSettings>();
            }
            else
            {
                Debug.Assert(pairData.ParameterList?.Count == 0);

                m_dict = pairData.ParameterGroupList.
                    Select(paramGrp => new CSProcessServerPairSettings(paramGrp, lvData, hostData, baseFolder)).
                    GroupBy(currPair => currPair.HostId).
                    ToDictionary(
                        currGrp => currGrp.Key,
                        currGrp => new CSProcessServerHostSettings(currGrp.Key, currGrp, baseFolder, m_csUri));
            }

            this.ProtectedMachineTelemetrySettings =
                m_dict.Values.Select(
                    currHost => new ProtectedMachineTelemetrySettings(currHost.HostId, currHost.SourceFolder, string.Empty)
                    ).ToList();

            this.CumulativeThrottleLimit = decimal.Parse(
                tbsData.ParameterList.Single(
                    param => param.Name.Equals("DISK_SPACE_WARN_THRESHOLD", StringComparison.Ordinal)
                    ).Value,
                CultureInfo.InvariantCulture);

            var isLogRotationEnabledStr = tbsData.ParameterList.Single(
                    param => param.Name.Equals("isEnabledLogRotation", StringComparison.Ordinal)).Value;

            this.LogRotationState = LogRotationState.Unknown;
            if (!string.IsNullOrWhiteSpace(isLogRotationEnabledStr) &&
                Enum.TryParse(isLogRotationEnabledStr, out LogRotationState logRotationState))
            {
                this.LogRotationState = logRotationState;
            }

            var logRotationBackupCntStr = tbsData.ParameterList.Single(
                    param => param.Name.Equals("logRotationBkUp", StringComparison.Ordinal)).Value;

            if (!string.IsNullOrWhiteSpace(logRotationBackupCntStr) &&
                int.TryParse(logRotationBackupCntStr, out int logFilesBkpCount))
            {
                this.LogFilesBackupCounter = logFilesBkpCount;
            }
            else
            {
                this.LogFilesBackupCounter = 0;
            }

            this.IsCummulativeThrottled = false;
            if (psData != null)
            {
                var psHostId = PSInstallationInfo.Default.GetPSHostId();
                var psInfo = psData.ParameterGroupList.SingleOrDefault(paramGrp =>
                psHostId.Equals(paramGrp.Id, StringComparison.Ordinal));

                if (psInfo != null)
                {
                    string cumulativeThrottleStr = psInfo.ParameterList.Single(
                        param => param.Name.Equals("cummulativeThrottle", StringComparison.Ordinal)).Value;
                    if (int.TryParse(cumulativeThrottleStr, out int cumulativeThrottle))
                    {
                        this.IsCummulativeThrottled = Convert.ToBoolean(cumulativeThrottle);
                    }
                }
            }

            this.SystemHealth = SystemHealth.Unknown;
            if (sysHealth != null)
            {
                string systemHealthStr = sysHealth.ParameterList.SingleOrDefault(param =>
                    param.Name.Equals("healthFlag", StringComparison.Ordinal)).Value;
                if (!string.IsNullOrWhiteSpace(systemHealthStr) &&
                    Enum.TryParse<SystemHealth>(systemHealthStr, out SystemHealth systemHealth))
                {
                    this.SystemHealth = systemHealth;
                }
            }

            if (logRotData != null)
            {
                this.Logs = logRotData.ParameterGroupList.Select(
                    paramGrp => new LogRotationSettings(paramGrp)).ToList();
            }
            else
            {
                this.Logs = new List<LogRotationSettings>();
            }

            if (policyData != null)
            {
                this.PolicySettings = policyData.ParameterGroupList.Select(
                    paramGrp => new PolicySettings(paramGrp)).ToList();
            }
            else
            {
                this.PolicySettings = new List<PolicySettings>();
            }

            this.CSHostId = string.Empty;
            if (hostData != null)
            {
                foreach (var currHost in hostData.ParameterGroupList)
                {
                    var hostParameters = new ReadOnlyDictionary<string, string>(
                    currHost.ParameterList.ToDictionary(param => param.Name, param => param.Value));

                    var isCSEnabledStr = hostParameters.FirstOrDefault(
                        x => x.Key == "csEnabled").Value;
                    if (!string.IsNullOrWhiteSpace(isCSEnabledStr) &&
                        int.TryParse(isCSEnabledStr, out int csEnabled))
                    {
                        this.CSHostId = (Convert.ToBoolean(csEnabled)) ? currHost.Id : string.Empty;

                        if (!string.IsNullOrWhiteSpace(this.CSHostId))
                            break;
                    }
                }
            }
        }

        #region Base abstract members implementation

        public override bool IsSourceFolderSupported => true;

        public override IEnumerable<IProcessServerHostSettings> Hosts
        {
            get
            {
                return m_dict.Values;
            }
        }

        public override decimal CumulativeThrottleLimit { get; }

        public override LogRotationState LogRotationState { get; }

        public override int LogFilesBackupCounter { get; }

        public override string ApplicationInsightsInstrumentationKey => null;

        public override string CriticalChannelUri => m_csUri;

        public override DateTime CriticalChannelUriRenewalTimeUtc => DateTime.MinValue;

        public override string InformationalChannelUri => m_csUri;

        public override DateTime InformationalChannelUriRenewalTimeUtc => DateTime.MinValue;

        public override string TelemetryChannelUri => m_csUri;

        public override DateTime TelemetryChannelUriRenewalTimeUtc => DateTime.MinValue;

        public override IEnumerable<IProtectedMachineTelemetrySettings> ProtectedMachineTelemetrySettings { get; }

        public override string MessageContext => null;

        public override SystemHealth SystemHealth { get; }

        /// <summary>
        /// Indicates if Cumulative Throttle is set or not
        /// </summary>
        public override bool IsCummulativeThrottled { get; }

        public override IEnumerable<ILogRotationSettings> Logs { get; }

        public override IEnumerable<IPolicySettings> PolicySettings { get; }

        public override IReadOnlyDictionary<string, string> Tunables => null;

        public override string CSHostId { get; }

        public override bool IsPrivateEndpointEnabled => false;

        public override bool IsAccessControlEnabled => false;

        #endregion Base abstract members implementation
    }

    internal class CSProcessServerHostSettings : IProcessServerHostSettings
    {
        private readonly IReadOnlyList<CSProcessServerPairSettings> m_pairs;
        private readonly string m_csUri;

        public CSProcessServerHostSettings(
            string hostId, IEnumerable<CSProcessServerPairSettings> pairs, string baseFolder, string csUri)
        {
            this.HostId = hostId;
            m_pairs = pairs.ToList();
            m_csUri = csUri;

            var optimalPath = Settings.Default.ProcessServerSettings_CS_LongPathOptimalConversion;

            this.SourceFolder = FSUtils.GetLongPath(
                Path.Combine(baseFolder, this.HostId),
                isFile: false,
                optimalPath: optimalPath);
        }

        public string HostId { get; }

        public IEnumerable<IProcessServerPairSettings> Pairs => m_pairs;

        // TODO-SanKumar-2001:
        public string ProtectionState => null;

        // Data of a host goes to source and target folders on PS
        public string LogRootFolder => null;

        public string CriticalChannelUri => m_csUri;

        public DateTime CriticalChannelUriRenewalTimeUtc => DateTime.MinValue;

        public string InformationalChannelUri => m_csUri;

        public DateTime InformationalChannelUriRenewalTimeUtc => DateTime.MinValue;

        public string MessageContext => null;

        public string BiosId => null;

        internal string SourceFolder { get; }
    }

    internal class CSProcessServerPairSettings : IProcessServerPairSettings
    {
        public CSProcessServerPairSettings(ParameterGroupElement pairDataElement,
            ParameterGroupElement lvData,
            ParameterGroupElement hostData,
            string baseFolder)
        {
            // TODO-SanKumar-1907: Instead of building the dictionary here of
            // 50 elements for parsing 5 elements, it would be better to build
            // the dictionary during Xml parsing itself [PERFORMANCE]
            var parameters = new ReadOnlyDictionary<string, string>(
                pairDataElement.ParameterList.ToDictionary(param => param.Name, param => param.Value));

            string ReadParamValue(string paramName, bool ignoreIfNotPresent = false)
            {
                if (!parameters.TryGetValue(paramName, out string retValue) && !ignoreIfNotPresent)
                {
                    throw new KeyNotFoundException(
                        $"Unable to find {paramName} for the pair in the PS setting retrieved from CS");
                }

                return retValue;
            }

            this.HostId = ReadParamValue("sourceHostId");
            this.DeviceId = ReadParamValue("sourceDeviceName");

            this.TargetHostId = ReadParamValue("destinationHostId");
            this.TargetDeviceId = ReadParamValue("destinationDeviceName");

            var optimalPath = Settings.Default.ProcessServerSettings_CS_LongPathOptimalConversion;

            // TODO-SanKumar-1904: Convert any / to \. Not necessary, since Windows honors both slashes.

            // In Windows, if a path starts with \ (or /), it is considered as
            // under the current drive (Ex: C:\<path>). So, we have to trim the
            // beginning of the device id, since Linux devices starts generally
            // as /dev/...
            // To do: explore the use of trim instead of TrimStart below
            var normalizedSourceDeviceId = this.DeviceId.TrimStart(new[] { '\\', '/' });

            var normalizedTargetDeviceId = this.TargetDeviceId.TrimStart(new[] { '\\', '/' });

            this.SourceDiffFolder = FSUtils.GetLongPath(
                Path.Combine(
                    baseFolder,
                    this.HostId,
                    normalizedSourceDeviceId,
                    ProcessServerSettings.DIFFS_SUB_FOLDER),
                isFile: false,
                optimalPath: optimalPath);

            // TODO-SanKumar-1907: Tightly coupled with Windows. We can change
            // this by adding Linux variant too. Refer makePath subroutine
            // under Utilities.pm

            // Remove any colon from device name
            this.TargetDiffFolder = FSUtils.GetLongPath(
                Path.Combine(
                    baseFolder, this.TargetHostId, normalizedTargetDeviceId.Replace(":", null)),
                isFile: false,
                optimalPath: optimalPath);

            this.DiffFoldersToMonitor = new[] { this.SourceDiffFolder, this.TargetDiffFolder };

            this.ResyncFolder = this.ResyncFolderToMonitor = FSUtils.GetLongPath(
                Path.Combine(
                    this.TargetDiffFolder, ProcessServerSettings.RESYNC_SUB_FOLDER),
                isFile: false,
                optimalPath: optimalPath);

            this.AzurePendingUploadRecoverableFolder = FSUtils.GetLongPath(
                Path.Combine(
                    this.TargetDiffFolder, ProcessServerSettings.AZ_PEND_UPL_REC_SUB_FOLDER),
                isFile: false,
                optimalPath: optimalPath);

            this.AzurePendingUploadNonRecoverableFolder = FSUtils.GetLongPath(
                Path.Combine(
                    this.TargetDiffFolder, ProcessServerSettings.AZ_PEND_UPL_NONREC_SUB_FOLDER),
                isFile: false,
                optimalPath: optimalPath);

            this.RPOFilePath = FSUtils.GetLongPath(
                Path.Combine(
                    this.SourceDiffFolder, ProcessServerSettings.MONITOR_TXT),
                isFile: true,
                optimalPath: optimalPath);

            this.TagFolder = FSUtils.GetLongPath(
                Path.Combine(
                    this.SourceDiffFolder, ProcessServerSettings.TAGS_SUB_FOLDER),
                isFile: false,
                optimalPath: optimalPath);

            this.PerfFolder = FSUtils.GetLongPath(
                Path.Combine(
                    baseFolder,
                    this.HostId,
                    normalizedSourceDeviceId),
                isFile: false,
                optimalPath: optimalPath);

            this.PerfShortFolder = Path.Combine(
                baseFolder,
                this.HostId,
                normalizedSourceDeviceId);

            // NOTE-SanKumar-1904: This parameter is only sent by CS starting
            // from 1904 (9.25).
            string targetDataPlaneStr = ReadParamValue("TargetDataPlane", ignoreIfNotPresent: true);
            if (targetDataPlaneStr == null ||
                !int.TryParse(targetDataPlaneStr, out int targetDataPlane) ||
                !Enum.IsDefined(typeof(ProtectionDirection), targetDataPlane))
            {
                this.ProtectionDirection = ProtectionDirection.Unknown;
            }
            else
            {
                this.ProtectionDirection = (ProtectionDirection)targetDataPlane;
            }

            string isQuasiFlagStr = ReadParamValue("isQuasiflag");
            if (!int.TryParse(isQuasiFlagStr, out int isQuasiFlag) ||
                !Enum.IsDefined(typeof(ProtectionState), isQuasiFlag))
            {
                this.ProtectionState = ProtectionState.Unknown;
            }
            else
            {
                this.ProtectionState = (ProtectionState)isQuasiFlag;
            }

            string tarReplicationStatusStr = ReadParamValue("tar_replication_status");
            if (!int.TryParse(tarReplicationStatusStr, out int tgtReplicationStatus) ||
                !Enum.IsDefined(typeof(TargetReplicationStatus), tgtReplicationStatus))
            {
                this.TargetReplicationStatus = TargetReplicationStatus.Unknown;
            }
            else
            {
                this.TargetReplicationStatus = (TargetReplicationStatus)tgtReplicationStatus;
            }

            this.DiffThrottleLimit = AmethystConfig.Default.MaxDiffFilesThreshold;
            string diffThrottleLimitStr = ReadParamValue("maxDiffFilesThreshold", ignoreIfNotPresent: true);
            if ((diffThrottleLimitStr != null &&
                long.TryParse(diffThrottleLimitStr, out long parsedDiffLimit)))
            {
                this.DiffThrottleLimit = parsedDiffLimit;
            }
            // If pair state is not DifferentialSync and ResyncStep2 and DefaultDiffThrottleLimit is not set in config then make DiffThrottleLimit as 1GB
            if ((this.ProtectionState != ProtectionState.ResyncStep2) &&
                (this.ProtectionState != ProtectionState.DifferentialSync))
            {
                this.DiffThrottleLimit = Settings.Default.ProcessServerSettings_DiffThrottleLimitInBytesForPairInResync;
            }

            string resyncThrottleLimitStr = ReadParamValue("maxResyncFilesThreshold", ignoreIfNotPresent: true);
            if (resyncThrottleLimitStr != null && long.TryParse(resyncThrottleLimitStr, out long parsedResyncLimit))
            {
                this.ResyncThrottleLimit = parsedResyncLimit;
            }
            else
            {
                this.ResyncThrottleLimit = AmethystConfig.Default.MaxResyncFilesThreshold;
            }

            string enableCompressionStr = ReadParamValue("compressionEnable");
            if(!int.TryParse(enableCompressionStr, out int enableCompression) ||
                !Enum.IsDefined(typeof(EnableCompression), enableCompression))
            {
                this.EnableCompression = EnableCompression.Unknown;
            }
            else
            {
                this.EnableCompression = (EnableCompression)enableCompression;
            }

            string executionStateStr = ReadParamValue("executionState");
            if (!int.TryParse(executionStateStr, out int executionState) ||
                !Enum.IsDefined(typeof(ExecutionState), executionState))
            {
                this.ExecutionState = ExecutionState.Unknown;
            }
            else
            {
                this.ExecutionState = (ExecutionState)executionState;
            }

            string replicationStatusStr = ReadParamValue("replication_status");
            this.ReplicationStatus = !int.TryParse(replicationStatusStr, out int replicationStatus)
                ? 0 : replicationStatus;

            string lunStateStr = ReadParamValue("lunState");
            if (!int.TryParse(lunStateStr, out int lunState) ||
                !Enum.IsDefined(typeof(LunState), lunState))
            {
                this.LunState = LunState.Unknown;
            }
            else
            {
                this.LunState = (LunState)lunState;
            }

            string jobIdStr = ReadParamValue("jobId");
            this.JobId = !long.TryParse(jobIdStr, out long jobId) ? 0 : jobId;

            this.LunId = ReadParamValue("Phy_Lunid");
            if(string.IsNullOrWhiteSpace(this.LunId))
            {
                this.LunId = string.Empty;
            }

            string restartResyncCounterStr = ReadParamValue("restartResyncCounter");
            this.RestartResyncCounter = !ulong.TryParse(restartResyncCounterStr,
                out ulong restartResyncCounter) ? 0 : restartResyncCounter;

            string resyncStartTimeStr = ReadParamValue("resyncStartTime");
            this.ResyncStartTime = !ulong.TryParse(resyncStartTimeStr,
                out ulong resyncStartTime) ? 0 : resyncStartTime;

            string resyncEndTimeStr = ReadParamValue("resyncEndTime");
            this.ResyncEndTime = !ulong.TryParse(resyncEndTimeStr,
                out ulong resyncEndTime) ? 0 : resyncEndTime;

            string resyncStartTagtimeStr = ReadParamValue("resyncStartTagtime");
            this.ResyncStartTagtime = !long.TryParse(resyncStartTagtimeStr,
                out long resyncStartTagtime) ? 0 : resyncStartTagtime;

            string autoResyncStartTimeStr = ReadParamValue("autoResyncStartTime");
            this.AutoResyncStartTime = !long.TryParse(autoResyncStartTimeStr,
                out long autoResyncStartTime) ? 0 : autoResyncStartTime;

            string autoResyncStartHoursStr = ReadParamValue("autoResyncStartHours");
            this.AutoResyncStartHours = !int.TryParse(autoResyncStartHoursStr,
                out int autoResyncStartHours) ? 0 : autoResyncStartHours;

            string autoResyncStartMinutesStr = ReadParamValue("autoResyncStartMinutes");
            this.AutoResyncStartMinutes = !int.TryParse(autoResyncStartMinutesStr,
                out int autoResyncStartMinutes) ? 0 : autoResyncStartMinutes;

            string autoResyncStartTypeStr = ReadParamValue("autoResyncStartType");
            if (!int.TryParse(autoResyncStartTypeStr, out int autoResyncStartType) ||
                !Enum.IsDefined(typeof(AutoResyncStartType), autoResyncStartType))
            {
                this.AutoResyncStartType = AutoResyncStartType.Unknown;
            }
            else
            {
                this.AutoResyncStartType = (AutoResyncStartType)autoResyncStartType;
            }

            string autoResyncStopHoursStr = ReadParamValue("autoResyncStopHours");
            this.AutoResyncStopHours = !int.TryParse(autoResyncStopHoursStr,
                out int autoResyncStopHours) ? 0 : autoResyncStopHours;

            string autoResyncStopMinutesStr = ReadParamValue("autoResyncStopMinutes");
            this.AutoResyncStopMinutes = !int.TryParse(autoResyncStopMinutesStr,
                out int autoResyncStopMinutes) ? 0 : autoResyncStopMinutes;

            string currentHourStr = ReadParamValue("hour");
            this.CurrentHour = !int.TryParse(currentHourStr, out int currentHour) ? 0 : currentHour;

            string currentMinutesStr = ReadParamValue("minute");
            this.CurrentMinutes = !int.TryParse(currentMinutesStr, out int currentMinutes) ?
                0 : currentMinutes;

            string resyncOrderStr = ReadParamValue("resyncOrder");
            this.ResyncOrder = !int.TryParse(resyncOrderStr, out int resyncOrder) ?
                0 : resyncOrder;

            string scenarioIdStr = ReadParamValue("scenarioId");
            this.ScenarioId = !int.TryParse(scenarioIdStr, out int scenarioId) ?
                0 : scenarioId;

            string resyncSetCxtimestampStr = ReadParamValue("resyncSetCxtimestamp");
            this.ResyncSetCxtimestamp = !long.TryParse(resyncSetCxtimestampStr,
                out long resyncSetCxtimestamp) ? 0 : resyncSetCxtimestamp;

            string pairidStr = pairDataElement.Id;
            string[] idlist = pairidStr.Split(new[] { "!!" }, StringSplitOptions.None);
            this.PairId = !int.TryParse(idlist[0], out int pairId) ? 0 : pairId;

            string replicationCleanupOptions = ReadParamValue("replicationCleanupOptions");
            this.ReplicationCleanupOptions = !string.IsNullOrWhiteSpace(replicationCleanupOptions) ?
                replicationCleanupOptions : string.Empty;

            string sourceCommunicationBlockedStr = ReadParamValue("isCommunicationfromSource");
            this.SourceCommunicationBlocked = !sbyte.TryParse(sourceCommunicationBlockedStr,
                out sbyte sourceCommunicationBlocked) ? (sbyte)0 : sourceCommunicationBlocked;

            string rpoThresholdStr = ReadParamValue("rpoSLAThreshold");
            this.RPOThreshold = !int.TryParse(rpoThresholdStr, out int rpoThreshold) ?
                0 : rpoThreshold;

            string controlPlaneTimestampStr = ReadParamValue("unix_time");
            this.ControlPlaneTimestamp = !long.TryParse(controlPlaneTimestampStr,
                out long controlPlaneTimestamp) ? 0 : controlPlaneTimestamp;

            string statusUpdateUnixTimestampStr = ReadParamValue("statusUpdateTime_unix_time");
            this.StatusUpdateTimestamp = !long.TryParse(statusUpdateUnixTimestampStr,
                out long statusUpdateUnixTimestamp) ? 0 : statusUpdateUnixTimestamp;

            string sourceReplicationStatusStr = ReadParamValue("src_replication_status");
            if(!int.TryParse(sourceReplicationStatusStr, out int sourceReplicationStatus) ||
                !Enum.IsDefined(typeof(TargetReplicationStatus), sourceReplicationStatus))
            {
                this.SourceReplicationStatus = TargetReplicationStatus.Unknown;
            }
            else
            {
                this.SourceReplicationStatus = (TargetReplicationStatus)sourceReplicationStatus;
            }

            string isResyncRequiredStr = ReadParamValue("isResync");
            if(sbyte.TryParse(isResyncRequiredStr, out sbyte isResyncRequired))
            {
                this.IsResyncRequired = isResyncRequired;
            }
            else
            {
                this.IsResyncRequired = 0;
            }

            string pairDeletedStr = ReadParamValue("deleted");
            if(!int.TryParse(pairDeletedStr, out int pairDeleted) ||
                !Enum.IsDefined(typeof(PairDeletionStatus), pairDeleted))
            {
                this.PairDeleted = PairDeletionStatus.Unknown;
            }
            else
            {
                this.PairDeleted = (PairDeletionStatus)pairDeleted;
            }

            string throttleDifferentialsStr = ReadParamValue("throttleDifferentials");
            if(!int.TryParse(throttleDifferentialsStr, out int throttleDifferentials) ||
                !Enum.IsDefined(typeof(ThrottleState), throttleDifferentials))
            {
                this.ThrottleDifferentials = ThrottleState.Unknown;
            }
            else
            {
                this.ThrottleDifferentials = (ThrottleState)throttleDifferentials;
            }

            string throttleResyncStr = ReadParamValue("throttleresync");
            if(!int.TryParse(throttleResyncStr, out int throttleResync) ||
                !Enum.IsDefined(typeof(ThrottleState), throttleResync))
            {
                this.ThrottleResync = ThrottleState.Unknown;
            }
            else
            {
                this.ThrottleResync = (ThrottleState)throttleResync;
            }

            string currentRPOTimeStr = ReadParamValue("currentRPOTime");
            if (DateTime.TryParse(currentRPOTimeStr, out DateTime parsedCurrRPOTime))
            {
                this.CurrentRPOTime = DateTimeUtils.GetSecondsSinceEpoch(parsedCurrRPOTime);
            }
            else
            {
                this.CurrentRPOTime = 0;
            }

            string statusUpdateTimeStr = ReadParamValue("statusUpdateTime");
            if (DateTime.TryParse(statusUpdateTimeStr, out DateTime parsedStatusUpdateTime))
            {
                this.StatusUpdateTime = DateTimeUtils.GetSecondsSinceEpoch(parsedStatusUpdateTime);
            }
            else
            {
                this.StatusUpdateTime = 0;
            }

            this.IrProgressUpdateTimeUtc =
                new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Local)
                .AddSeconds(
                    long.Parse(ReadParamValue("lastTMChange"), CultureInfo.InvariantCulture)
                    ).ToUniversalTime(); // Linux epoch - local time of CS

            string lvDataKey = this.HostId + "!!" + this.DeviceId;

            var sourceLvData = lvData?.ParameterGroupList.SingleOrDefault(paramGrp =>
            lvDataKey.Equals(paramGrp.Id, StringComparison.Ordinal));

            var sourceHostData = hostData?.ParameterGroupList.SingleOrDefault(paramGrp =>
            this.HostId.Equals(paramGrp.Id, StringComparison.Ordinal));

            var targetHostData = hostData?.ParameterGroupList.SingleOrDefault(paramGrp =>
            this.TargetHostId.Equals(paramGrp.Id, StringComparison.Ordinal));

            var psHostId = PSInstallationInfo.Default.GetPSHostId();
            var psData = hostData?.ParameterGroupList.SingleOrDefault(
                paramGrp => psHostId.Equals(paramGrp.Id, StringComparison.Ordinal));

            string targetlvDataKey = this.TargetHostId + "!!" + this.TargetDeviceId;

            var targetLvData = lvData?.ParameterGroupList.SingleOrDefault(paramGrp =>
            targetlvDataKey.Equals(paramGrp.Id, StringComparison.Ordinal));

            // explicitly setting default values for the settings retrieved by lvdata and hostdata
            this.tmid = 0;
            this.FarlineProtected = FarLineProtected.Unknown;
            this.DoResync = 0;
            this.IsVisible = 0;
            this.Hostname = string.Empty;
            this.TargetHostName = string.Empty;
            this.CompatibilityNumber = 0;
            this.TargetLastHostUpdateTime = 0;
            this.TargetCompatibilityNumber = 0;
            this.SourceLastHostUpdateTime = 0;
            this.SourceAppAgentLastHostUpdateTime = 0;
            this.SourceAgentVersion = string.Empty;
            this.PSVersion = string.Empty;

            if (sourceLvData != null)
            {
                string tmidStr = sourceLvData.ParameterList.Single(param =>
                param.Name.Equals("tmId", StringComparison.Ordinal)).Value;
                if (uint.TryParse(tmidStr, out uint tmid))
                {
                    this.tmid = tmid;
                }

                string farLineProtectedStr = sourceLvData.ParameterList.Single(param =>
                param.Name.Equals("farLineProtected", StringComparison.Ordinal)).Value;
                if (int.TryParse(farLineProtectedStr, out int farLineProtected) &&
                    Enum.IsDefined(typeof(FarLineProtected), farLineProtected))
                {
                    this.FarlineProtected = (FarLineProtected)farLineProtected;
                }

                string doResyncStr = sourceLvData.ParameterList.Single(param =>
                param.Name.Equals("doResync", StringComparison.Ordinal)).Value;
                if(int.TryParse(doResyncStr, out int doResync))
                {
                    this.DoResync = doResync;
                }
            }

            if (sourceHostData != null)
            {
                string hostnameStr = sourceHostData.ParameterList.Single(param =>
                param.Name.Equals("name", StringComparison.Ordinal)).Value;
                if(!string.IsNullOrWhiteSpace(hostnameStr))
                {
                    this.Hostname = hostnameStr;
                }

                string compatibilityNumberStr = sourceHostData.ParameterList.Single(param =>
                param.Name.Equals("compatibilityNo", StringComparison.Ordinal)).Value;
                if(uint.TryParse(compatibilityNumberStr, out uint compatibilityNumber))
                {
                    this.CompatibilityNumber = compatibilityNumber;
                }

                string sourceLastHostUpdateTimeStr = sourceHostData.ParameterList.Single(param =>
                param.Name.Equals("lastHostUpdateTime", StringComparison.Ordinal)).Value;
                if(long.TryParse(sourceLastHostUpdateTimeStr, out long sourceLastHostUpdateTime))
                {
                    this.SourceLastHostUpdateTime = sourceLastHostUpdateTime;
                }

                string sourceLastHostUpdateTimeAppStr = sourceHostData.ParameterList.Single(param =>
                param.Name.Equals("lastHostUpdateTimeApp", StringComparison.Ordinal)).Value;
                if (long.TryParse(sourceLastHostUpdateTimeAppStr, out long sourceLastHostUpdateTimeApp))
                {
                    SourceAppAgentLastHostUpdateTime = sourceLastHostUpdateTimeApp;
                }

                string srcAgentVersionStr = targetHostData.ParameterList.Single(param =>
                param.Name.Equals("prod_version", StringComparison.Ordinal)).Value;
                if (!string.IsNullOrWhiteSpace(srcAgentVersionStr))
                {
                    this.SourceAgentVersion = srcAgentVersionStr;
                }
            }

            if(targetHostData != null)
            {
                string targetHostnameStr = targetHostData.ParameterList.Single(param =>
                param.Name.Equals("name", StringComparison.Ordinal)).Value;
                if (!string.IsNullOrWhiteSpace(targetHostnameStr))
                {
                    this.TargetHostName = targetHostnameStr;
                }

                string compatibilityNumberStr = targetHostData.ParameterList.Single(param =>
                param.Name.Equals("compatibilityNo", StringComparison.Ordinal)).Value;
                if (uint.TryParse(compatibilityNumberStr, out uint compatibilityNumber))
                {
                    this.TargetCompatibilityNumber = compatibilityNumber;
                }

                string lastHostUpdateTimeStr = targetHostData.ParameterList.Single(param =>
                param.Name.Equals("lastHostUpdateTime", StringComparison.Ordinal)).Value;
                if (long.TryParse(lastHostUpdateTimeStr, out long lastHostUpdateTime))
                {
                    this.TargetLastHostUpdateTime = lastHostUpdateTime;
                }
            }

            if (targetLvData != null)
            {
                string isVisibleStr = targetLvData.ParameterList.Single(param =>
                param.Name.Equals("visible", StringComparison.Ordinal)).Value;
                if (int.TryParse(isVisibleStr, out int isVisible))
                {
                    this.IsVisible = isVisible;
                }
            }

            if (psData != null)
            {
                string psVersionStr = psData.ParameterList.Single(param =>
                param.Name.Equals("prod_version", StringComparison.Ordinal)).Value;
                if (!string.IsNullOrWhiteSpace(psVersionStr))
                {
                    this.PSVersion = psVersionStr;
                }
            }
        }

        public string HostId { get; }

        public string DeviceId { get; }

        public ProtectionDirection ProtectionDirection { get; }

        public string SourceDiffFolder { get; }

        public string TargetDiffFolder { get; }

        public string ResyncFolder { get; }

        public string AzurePendingUploadRecoverableFolder { get; }

        public string AzurePendingUploadNonRecoverableFolder { get; }

        public ProtectionState ProtectionState { get; }

        public TargetReplicationStatus TargetReplicationStatus { get; }

        public long DiffThrottleLimit { get; }

        public long ResyncThrottleLimit { get; }

        public string RPOFilePath { get; }

        public IEnumerable<string> DiffFoldersToMonitor { get; }

        public string ResyncFolderToMonitor { get; }

        public string ReplicationSessionId => null;

        // TODO-SanKumar-2001:
        public string ReplicationState => null;

        public DateTime IrProgressUpdateTimeUtc { get; }

        public string TagFolder { get; }

        public string MessageContext => null;

        /// <summary>
        /// JobId for the pair
        /// </summary>
        public long JobId { get; }

        public EnableCompression EnableCompression { get; }

        /// <summary>
        /// Execution state for the pair
        /// </summary>
        public ExecutionState ExecutionState { get; }

        /// <summary>
        /// Replication status of the pair
        /// </summary>
        public int ReplicationStatus { get; }

        /// <summary>
        /// The number of times resync is triggered for the pair
        /// </summary>
        public ulong RestartResyncCounter { get; }

        /// <summary>
        /// The time at which last resync was started (set by cs)
        /// seconds since epoch
        /// </summary>
        public ulong ResyncStartTime { get; }

        /// <summary>
        /// The time at which last resync ended (set by cs)
        /// seconds since epoch
        /// </summary>
        public ulong ResyncEndTime { get; }

        /// <summary>
        /// Tmid for the pair
        /// </summary>
        public uint tmid { get; }

        /// <summary>
        /// Flag to check if the disk/volume is protected
        /// </summary>
        public FarLineProtected FarlineProtected { get; }

        /// <summary>
        /// Set when resync is marked for a pair
        /// This is the flag that is set if resync is marked for a pair
        /// </summary>
        public int DoResync { get; }

        public int IsVisible { get; }

        public string TargetHostId { get; }

        public string TargetDeviceId { get; }

        public string LunId { get; }

        public LunState LunState { get; }

        /// <summary>
        /// Resync start time set by agent
        /// ticks since epoch
        /// </summary>
        public long ResyncStartTagtime { get; }

        /// <summary>
        /// Resync end time set by agent
        /// ticks since epoch
        /// </summary>
        public long ResyncEndTagTime { get; }

        /// <summary>
        /// Time at which resync should start automatically
        /// Hardcoded in cs db
        /// </summary>
        public long AutoResyncStartTime { get; }

        /// <summary>
        /// The time at which resync was marked
        /// If its non zero then resync is already marked
        /// </summary>
        public long ResyncSetCxtimestamp { get; }

        public string Hostname { get; }

        public string PerfFolder { get; }

        public uint CompatibilityNumber { get; }

        public uint TargetCompatibilityNumber { get; }

        public int PairId { get; }

        /// <summary>
        /// The last time when the target updated the cs
        /// also referred to as last heartbeat of target
        /// seconds since epoch
        /// </summary>
        public long TargetLastHostUpdateTime { get; }

        /// <summary>
        /// Bitmask set by CS for cleanup actions
        /// </summary>
        public string ReplicationCleanupOptions { get; }

        /// <summary>
        /// Flag to check if source communication is blocked
        /// </summary>
        public sbyte SourceCommunicationBlocked { get; }

        public int RPOThreshold { get; }

        public long ControlPlaneTimestamp { get; }

        public long StatusUpdateTimestamp { get; }

        public TargetReplicationStatus SourceReplicationStatus { get; }

        public long SourceLastHostUpdateTime { get; }

        public long SourceAppAgentLastHostUpdateTime { get; }

        public sbyte IsResyncRequired { get; }

        public PairDeletionStatus PairDeleted { get; }

        public ThrottleState ThrottleDifferentials { get; }

        public ThrottleState ThrottleResync { get; }

        public string TargetHostName { get; }

        public AutoResyncStartType AutoResyncStartType { get; }

        public int AutoResyncStartHours { get; }

        public int AutoResyncStartMinutes { get; }

        public int AutoResyncStopHours { get; }

        public int AutoResyncStopMinutes { get; }

        public int ResyncOrder { get; }

        public int CurrentHour { get; }

        public int CurrentMinutes { get; }

        public int ScenarioId { get; }

        public string SourceAgentVersion { get; }

        public string PSVersion { get; }

        public long CurrentRPOTime { get; }

        public long StatusUpdateTime { get; }

        public string PerfShortFolder { get; }
    }

    internal class LogRotationSettings : ILogRotationSettings
    {
        public LogRotationSettings(ParameterGroupElement logRotDataElement)
        {
            var parameters = new ReadOnlyDictionary<string, string>(
                logRotDataElement.ParameterList.ToDictionary(param => param.Name, param => param.Value));

            string ReadParamValue(string paramName, bool ignoreIfNotPresent = false)
            {
                if (!parameters.TryGetValue(paramName, out string retValue) && !ignoreIfNotPresent)
                {
                    throw new KeyNotFoundException(
                        $"Unable to find {paramName} for the log in the PS setting retrieved from CS");
                }

                return retValue;
            }

            this.LogName = ReadParamValue("logName");

            this.LogPathWindows = FSUtils.CanonicalizePath(ReadParamValue("logPathWindows"));

            string logSizeLimitStr = ReadParamValue("logSizeLimit");
            this.LogSizeLimit = !int.TryParse(logSizeLimitStr, out int logSizeLimit) ? 0 : logSizeLimit;

            string logTimeLimitStr = ReadParamValue("logTimeLimit");
            this.LogTimeLimit = !int.TryParse(logTimeLimitStr, out int logTimeLimit) ? 0 : logTimeLimit;

            string startTimeStr = ReadParamValue("startTime");
            this.StartEpochTime = !long.TryParse(startTimeStr, out long logRotStartTime) ? 0 : logRotStartTime;

            string presentTimeStr = ReadParamValue("presentTime");
            this.CurrentEpochTime = !long.TryParse(presentTimeStr, out long currentTime) ? 0 : currentTime;
        }

        public string LogName { get; }

        public string LogPathWindows { get; }

        public int LogSizeLimit { get; }

        public int LogTimeLimit { get; }

        public long StartEpochTime { get; }

        public long CurrentEpochTime { get; }
    }

    internal class PolicySettings : IPolicySettings
    {
        public PolicySettings(ParameterGroupElement policyDataElement)
        {
            var parameters = new ReadOnlyDictionary<string, string>(
                policyDataElement.ParameterList.ToDictionary(param => param.Name, param => param.Value));

            string ReadParamValue(string paramName, bool ignoreIfNotPresent = false)
            {
                if (!parameters.TryGetValue(paramName, out string retValue) && !ignoreIfNotPresent)
                {
                    throw new KeyNotFoundException(
                        $"Unable to find {paramName} for the policy in the PS setting retrieved from CS");
                }

                return retValue;
            }

            this.HostId = ReadParamValue("hostId");

            string policyIdStr = ReadParamValue("policyId");
            this.PolicyId = !int.TryParse(policyIdStr, out int policyId) ? 0 : policyId;
        }

        public string HostId { get; }

        public int PolicyId { get; }
    }
}
