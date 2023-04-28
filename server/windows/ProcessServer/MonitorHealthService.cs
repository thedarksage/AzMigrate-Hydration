using Microsoft.Azure.SiteRecovery.ProcessServer;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Monitoring;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi;
using RcmContract;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using static RcmContract.MonitoringMsgEnum;
using ProcessServerSettings = Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.ProcessServerSettings;
using PSCoreSettings = Microsoft.Azure.SiteRecovery.ProcessServer.Core.Settings;

namespace ProcessServer
{
    /// <summary>
    /// Monitors PS health.
    /// </summary>
    class MonitorHealthService
    {
        #region Rcm_Constants

        private const int HealthFactorWarning = 1, HealthFactorCritical = 2;
        private const string MONITOR_FILE = "monitor.txt";
        private const string DR_STUCK_FILE = "dr_stuck_file.txt";
        private const string SYNC_FILES_PATTERN = "completed_sync*.dat";
        private const string TAG_FILES_PATTERN = "*completed_ediffcompleted_diff_tag_P*";
        private const string RPO_LASTTAGTIMEUTC = "lasttagtimestamp.txt";
        private const string DiffThrottleCode = "EC0111";
        private const string ResyncThrottleCode = "EC0110";
        private const string RCMSourceDataUploadBlockedEC = "DataUploadBlocked";
        private const string RpoTimeDateFormat = "MM/dd/yyyy HH:mm:ss";
        private static readonly string[] targetDiffFilePatterns = new[] {
                                                                "completed_ediff*.dat"};
        // This does not include cluster files (during failback),
        // files with *.Map extension and few other files.
        // But since all the files except sync files are very small in size,
        // this shouldn't make a huge difference.
        private static readonly string[] resyncFilePatterns = new[] {
                                                                "completed_sync*.dat",
                                                                "completed_hcd*.dat"};

        #endregion

        #region Cs_constants

        private static readonly string[] sourceDiffFilePatterns = new[] {
                                                                "completed_diff*.dat"};
        private const string IRDRStuckWarningHealthFactorCode = "ECH00015";
        private const string IRDRStuckCriticalHealthFactorCode = "ECH00014";

        #endregion


        private const string RPO_FILE = "rpo.txt";
        private const string IRCriticalHealthFactor = "IR_FILE_STUCK_CRITICAL";
        private const string IRWarningHealthFactor = "IR_FILE_STUCK_WARNING";
        private const string DRCriticalHealthFactor = "DR_FILE_STUCK_CRITICAL";
        private const string DRWarningHealthFactor = "DR_FILE_STUCK_WARNING";


        readonly IProcessServerCSApiStubs psApiStubsImpl = MonitoringApiFactory.GetPSMonitoringApiStubs();

        /// <summary>
        /// It invokes monitor health function based on cstype
        /// </summary>
        /// <param name="cancellationToken">Cancellation token</param>
        internal Task MonitorHealth(CancellationToken cancellationToken)
        {
            if(PSInstallationInfo.Default.CSMode == CSMode.Rcm)
            {
                if (Settings.Default.RCM_MonitorReplicationHealth)
                {
                    return MonitorHealthForRcmPS(cancellationToken);
                }
            }
            else if(PSInstallationInfo.Default.CSMode == CSMode.LegacyCS)
            {
                return MonitorHealthForLegacyPS(cancellationToken);
            }
            else
            {
                throw new NotImplementedException(
                    $"{nameof(MonitorHealth)} is not implemented for given cs mode {PSInstallationInfo.Default.CSMode}");
            }

            // Using Task.CompletedTask for less runtime overhead.
            return Task.CompletedTask;
        }

        /// <summary>
        /// Main task which monitors pair level healths
        /// and raises alerts
        /// </summary>
        /// <param name="cancellationToken">Cancellation Token</param>
        /// <returns>Task that monitors pair level health</returns>
        private async Task MonitorHealthForLegacyPS(CancellationToken cancellationToken)
        {
            // Stopwatch for Source upload blocked event
            Stopwatch monitorSourceReplicationBlockedSw = new Stopwatch();
            monitorSourceReplicationBlockedSw.Start();

            // Stopwatch for IR/DR stuck files
            Stopwatch monitorIrDrStuckFileSw = new Stopwatch();
            monitorIrDrStuckFileSw.Start();

            // Stopwatch for RPO monitoring
            Stopwatch monitorRPOSw = new Stopwatch();
            monitorRPOSw.Restart();

            // Stopwatch for reysnc throttle monitor
            Stopwatch resyncThrottleMonitorSw = new Stopwatch();
            resyncThrottleMonitorSw.Start();

            // Stopwatch for diff throttle event
            Stopwatch monitorDiffThrottleSw = new Stopwatch();
            monitorDiffThrottleSw.Start();

            // Task for monitoring if upload is blocked
            Task monitorUploadBlockedTask = null;

            // Task for monitoring if IR/DR files are stuck
            Task monitorIrDrStuckFileTask = null;

            // Task for monitoring RPO
            Task monitorRPOTask = null;

            // Task for monitoring resync throttle
            Task monitorResyncThrottleTask = null;

            // Task for monitoring diff throttle
            Task monitorDiffThrottleTask = null;

            // First time initial wait
            await Task.Delay(Settings.Default.MonitorHealthServiceInitialWaitInterval, cancellationToken).
                ConfigureAwait(false);

            while(true)
            {
                try
                {
                    if (cancellationToken.IsCancellationRequested)
                        cancellationToken.ThrowIfCancellationRequested();

                    if (Settings.Default.MonitorSourceReplicationBlocked &&
                            monitorSourceReplicationBlockedSw.Elapsed >=
                            Settings.Default.MonitorHealthSourceReplicationBlockedInterval)
                    {
                        monitorSourceReplicationBlockedSw.Restart();
                        if (monitorUploadBlockedTask == null || monitorUploadBlockedTask.IsCompleted)
                        {
                            monitorUploadBlockedTask = MonitorSouceReplicationBlockedAsync(cancellationToken);
                        }
                        else
                        {
                            Tracers.Monitoring.TraceAdminLogV2Message(
                                    TraceEventType.Warning,
                                    $"Skipping the current {nameof(monitorUploadBlockedTask)} as last " +
                                    $"triggered {nameof(monitorUploadBlockedTask)} is still running.");
                        }
                    }

                    if (cancellationToken.IsCancellationRequested)
                        cancellationToken.ThrowIfCancellationRequested();

                    if (Settings.Default.MonitorRPO &&
                        monitorRPOSw.Elapsed >= 
                        Settings.Default.MonitorRPOInterval)
                    {
                        monitorRPOSw.Restart();
                        if(monitorRPOTask == null || monitorRPOTask.IsCompleted)
                        {
                            monitorRPOTask = MonitorRPOAsync(cancellationToken);
                        }
                        else
                        {
                            Tracers.Monitoring.TraceAdminLogV2Message(
                                    TraceEventType.Warning,
                                    $"Skipping the current {nameof(monitorRPOTask)} as last " +
                                    $"triggered {nameof(monitorRPOTask)} is still running.");
                        }
                    }

                    if (cancellationToken.IsCancellationRequested)
                        cancellationToken.ThrowIfCancellationRequested();

                    if (Settings.Default.MonitorIrDrStuckFile &&
                        monitorIrDrStuckFileSw.Elapsed >=
                        Settings.Default.MonitorIrDrStuckFileInterval)
                    {
                        monitorIrDrStuckFileSw.Restart();
                        if (monitorIrDrStuckFileTask == null || monitorIrDrStuckFileTask.IsCompleted)
                        {
                            monitorIrDrStuckFileTask = MonitorIRDRStuckFileAsync(cancellationToken);
                        }
                        else
                        {
                            Tracers.Monitoring.TraceAdminLogV2Message(
                                    TraceEventType.Warning,
                                    $"Skipping the current {nameof(monitorIrDrStuckFileTask)} as last " +
                                    $"triggered {nameof(monitorIrDrStuckFileTask)} is still running.");
                        }
                    }

                    if (cancellationToken.IsCancellationRequested)
                        cancellationToken.ThrowIfCancellationRequested();

                    if (Settings.Default.MonitorResyncThrottle &&
                        resyncThrottleMonitorSw.Elapsed >=
                        Settings.Default.ResyncThrottleMonitorInterval)
                    {
                        resyncThrottleMonitorSw.Restart();
                        if (monitorResyncThrottleTask == null || monitorResyncThrottleTask.IsCompleted)
                        {
                            monitorResyncThrottleTask = MonitorResyncThrottleAsync(cancellationToken);
                        }
                        else
                        {
                            Tracers.Monitoring.TraceAdminLogV2Message(
                                    TraceEventType.Warning,
                                    $"Skipping the current {nameof(monitorResyncThrottleTask)} as last " +
                                    $"triggered {nameof(monitorResyncThrottleTask)} is still running.");
                        }
                    }

                    if (cancellationToken.IsCancellationRequested)
                        cancellationToken.ThrowIfCancellationRequested();

                    if (Settings.Default.MonitorDiffThrottle &&
                        monitorDiffThrottleSw.Elapsed >=
                        Settings.Default.MonitorDiffThrottleInterval)
                    {
                        monitorDiffThrottleSw.Restart();
                        if(monitorDiffThrottleTask == null || monitorDiffThrottleTask.IsCompleted)
                        {
                            monitorDiffThrottleTask = MonitorDiffThrottleAsync(cancellationToken);
                        }
                        else
                        {
                            Tracers.Monitoring.TraceAdminLogV2Message(
                                    TraceEventType.Warning,
                                     $"Skipping the current {nameof(monitorDiffThrottleTask)} as last " +
                                    $"triggered {nameof(monitorDiffThrottleTask)} is still running.");
                        }
                    }
                }
                catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
                {
                    // Rethrow operation canceled exception if the service is stopped.
                    Tracers.Monitoring.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "Cancelling monitor health service Task");
                    throw;
                }
                catch (Exception ex)
                {
                    Tracers.Monitoring.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Exception at MonitorHealthService - {0}{1}",
                        Environment.NewLine, ex);
                }

                await Task.Delay(Settings.Default.MonitorHealthServiceWaitInterval, cancellationToken).
                    ConfigureAwait(false);
            }
        }

        private Task MonitorIRDRStuckFileAsync(CancellationToken token)
        {
            List<IProcessServerPairSettings> replicationPairs = GetReplicationPairs(token);
            List<IRDRStuckHealthEvent> IRDRHealth = new List<IRDRStuckHealthEvent>();
            foreach (var pair in replicationPairs)
            {
                IRDRStuckHealthFactor IRDRStuckHealthFactor = IRDRStuckHealthFactor.Healthy;
                string IRDRStuckHealthDescription = string.Empty;
                bool isPairInIr = false;
                TaskUtils.RunAndIgnoreException(() =>
                {
                    if (pair.FarlineProtected != FarLineProtected.Protected)
                    {
                        return;
                    }

                    // To do:
                    // Combine all these conditions in one setting
                    // Once all the events are ported to .net

                    // IR File Stuck should be monitored only in the following conditions
                    // 1. If the pair state is not Unconfigured and UnconfiguredWaiting, ie,
                    //    the pair is either active or in waiting state
                    // 2. If pair is not deleted.
                    // 3. If the replication is not paused.
                    // 4. If pair is in resync(isResync flag is set in csdb)
                    // 5. If pair is in resync step 1.
                    // 6. If both source agent and target(MT) are up and running
                    //    (ie, their heartbeat is not older than SentinelErrorThreshold duration)
                    if ((pair.ExecutionState != ExecutionState.Unconfigured ||
                    pair.ExecutionState != ExecutionState.UnconfiguredWaiting) &&
                    pair.PairDeleted == PairDeletionStatus.NotDeleted &&
                    pair.SourceReplicationStatus == TargetReplicationStatus.NonPaused &&
                    pair.TargetReplicationStatus == TargetReplicationStatus.NonPaused &&
                    pair.IsResyncRequired == 1 &&
                    pair.ProtectionState == ProtectionState.ResyncStep1 &&
                    ((DateTimeUtils.GetSecondsAfterEpoch() - pair.SourceLastHostUpdateTime) <
                    Settings.Default.SentinelErrorThreshold.TotalSeconds) &&
                    ((DateTimeUtils.GetSecondsAfterEpoch() - pair.TargetLastHostUpdateTime) <
                    Settings.Default.SentinelErrorThreshold.TotalSeconds))
                    {
                        (IRDRStuckHealthFactor, IRDRStuckHealthDescription) = GetIRStuckHealth(pair);
                        isPairInIr = true;
                    }
                    // To do:
                    // Combine all these conditions in one setting
                    // Once all the events are ported to .net

                    // DR File Stuck health factor should be monitored only in the following conditions
                    // 1. If pair is in resync step 2 or diffsync.
                    // 2. If pair is not deleted.
                    // 3. If replication status is 0 (default).
                    // 4. If target(MT) is up and running,
                    //    (ie, the heartbeat is not older than SentinelErrorThreshold duration)
                    else if (pair.ProtectionState != ProtectionState.ResyncStep1 &&
                    pair.PairDeleted == PairDeletionStatus.NotDeleted &&
                    pair.TargetReplicationStatus == TargetReplicationStatus.NonPaused &&
                    pair.ReplicationStatus == 0 &&
                    (DateTimeUtils.GetSecondsAfterEpoch() - pair.TargetLastHostUpdateTime) <
                    Settings.Default.SentinelErrorThreshold.TotalSeconds)
                    {
                        (IRDRStuckHealthFactor, IRDRStuckHealthDescription) = VerifyDRFileStuck(pair);
                    }

                    if (isPairInIr || pair.PairDeleted == PairDeletionStatus.Deleted ||
                    pair.TargetReplicationStatus == TargetReplicationStatus.Paused)
                    {
                        string drStuckFilePath = Path.Combine(pair.TargetDiffFolder, DR_STUCK_FILE);

                        if (File.Exists(drStuckFilePath))
                        {
                            File.Delete(drStuckFilePath);
                        }
                    }

                    IRDRStuckHealthEvent iRDRStuckHealthEvent = null;
                    if (IRDRStuckHealthFactor == IRDRStuckHealthFactor.Warning)
                    {
                        Tracers.Monitoring.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Health Factor : {0}, Health Factor Description : {1} for pair {2}-{3}",
                            IRDRStuckHealthFactor, IRDRStuckHealthDescription,
                            pair.HostId, pair.DeviceId);

                        iRDRStuckHealthEvent = new IRDRStuckHealthEvent(
                            errorCode: IRDRFileStuckHealthErrorCodes.IRDRStuckWarningCode,
                            healthFactorCode: IRDRStuckWarningHealthFactorCode,
                            healthFactor: "1",
                            priority: "1",
                            healthDescription: IRDRStuckHealthDescription,
                            health: IRDRStuckHealthFactor.Warning,
                            pair: pair);
                    }
                    else if (IRDRStuckHealthFactor == IRDRStuckHealthFactor.Critical)
                    {
                        Tracers.Monitoring.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Health Factor : {0}, Health Factor Description : {1} for pair {2}-{3}",
                            IRDRStuckHealthFactor, IRDRStuckHealthDescription,
                            pair.HostId, pair.DeviceId);

                        iRDRStuckHealthEvent = new IRDRStuckHealthEvent(
                            errorCode: IRDRFileStuckHealthErrorCodes.IRDRStuckCriticalCode,
                            healthFactorCode: IRDRStuckCriticalHealthFactorCode,
                            healthFactor: "2",
                            priority: "2",
                            healthDescription: IRDRStuckHealthDescription,
                            health: IRDRStuckHealthFactor.Critical,
                            pair: pair);
                    }
                    else if(IRDRStuckHealthFactor == IRDRStuckHealthFactor.Healthy)
                    {
                        iRDRStuckHealthEvent = new IRDRStuckHealthEvent(
                            errorCode: string.Empty,
                            healthFactorCode: string.Empty,
                            healthFactor: string.Empty,
                            priority: string.Empty,
                            healthDescription: IRDRStuckHealthDescription,
                            health: IRDRStuckHealthFactor.Healthy,
                            pair: pair);
                    }
                    else
                    {
                        throw new NotImplementedException($"The health factor {IRDRStuckHealthFactor} is not implemented.");
                    }

                    IRDRHealth.Add(iRDRStuckHealthEvent);
                }, Tracers.Monitoring);
            }

            if (IRDRHealth.Count > 0)
            {
                return TaskUtils.RunTaskForAsyncOperation(
                        traceSource: Tracers.Monitoring,
                        name: nameof(psApiStubsImpl.UpdateIRDRStuckHealthAsync),
                        ct: token,
                        operation: async (taskCancelToken) =>
                        await psApiStubsImpl.UpdateIRDRStuckHealthAsync(
                            iRDRStuckHealthEventList: IRDRHealth,
                            ct: taskCancelToken).
                        ConfigureAwait(false));
            }
            else
            {
                return null;
            }
        }
        private Task MonitorRPOAsync(CancellationToken token)
        {
            List<IProcessServerPairSettings> replicationPairs = GetReplicationPairs(token);
            Dictionary<IProcessServerPairSettings, TimeSpan> RPOInfo = new Dictionary<IProcessServerPairSettings, TimeSpan>();
            foreach (var pair in replicationPairs)
            {
                TaskUtils.RunAndIgnoreException(() =>
                {
                    TimeSpan rpo = TimeSpan.Zero;
                    if (pair.FarlineProtected != FarLineProtected.Protected)
                    {
                        return;
                    }

                    MonitorPairForRPO(pair, out rpo);

                    UpdateRPOFile(pair, rpo);
                    RPOInfo.Add(pair, rpo);

                    Tracers.Monitoring.TraceAdminLogV2Message(
                        (new TimeSpan(0, pair.RPOThreshold, 0) > rpo) ?
                        TraceEventType.Verbose :
                        TraceEventType.Error,
                        "RPO for the pair {0}-{1} is {2}",
                        pair.HostId, pair.DeviceId, rpo);
                }, Tracers.Monitoring);
            }

            if (RPOInfo.Any())
            {
                return TaskUtils.RunTaskForAsyncOperation(
                    traceSource: Tracers.Monitoring,
                    name: nameof(psApiStubsImpl.UpdateRPOAsync),
                    ct: token,
                    operation: async (taskCancelToken) =>
                    await psApiStubsImpl.UpdateRPOAsync(
                        RPOInfo: RPOInfo,
                        ct: taskCancelToken)
                        .ConfigureAwait(false)
                    );
                            }
            else
            {
                return null;
            }
        }

        private Task MonitorResyncThrottleAsync(CancellationToken token)
        {
            List<IProcessServerPairSettings> replicationPairs = GetReplicationPairs(token);
            Dictionary<int, Tuple<ThrottleState, long>> pairResyncStatus = new Dictionary<int, Tuple<ThrottleState, long>>();
            TimeSpan resyncFolderSizeCalcTime = TimeSpan.Zero;
            foreach (var pair in replicationPairs)
            {
                TaskUtils.RunAndIgnoreException(() =>
                {
                    if(pair.FarlineProtected != FarLineProtected.Protected)
                    {
                        Tracers.Monitoring.TraceAdminLogV2Message(
                            TraceEventType.Verbose,
                            "The disk is not protected : {0}-{1}",
                            pair.HostId, pair.DeviceId);
                        return;
                    }

                    long resyncFileSize = 0;
                    Stopwatch pairResyncFolderSizeCalcTimeSw = new Stopwatch();
                    pairResyncFolderSizeCalcTimeSw.Start();
                    if (Directory.Exists(pair.ResyncFolder))
                    {
                        resyncFileSize += FSUtils.GetFilesSize(pair.ResyncFolder,
                        SearchOption.AllDirectories, resyncFilePatterns);
                    }
                    resyncFolderSizeCalcTime = resyncFolderSizeCalcTime.Add(pairResyncFolderSizeCalcTimeSw.Elapsed);
                    pairResyncFolderSizeCalcTimeSw.Stop();

                    ThrottleState resyncThrottleState = ThrottleState.Unknown;

                    if (pair.CompatibilityNumber > Settings.Default.ResyncThrottleMaxVer)
                    {
                        Tracers.Monitoring.DebugAdminLogV2Message(
                            "The compatibility number {0} for the current host is greater than supported version {1}",
                            pair.CompatibilityNumber, Settings.Default.ResyncThrottleMaxVer);

                        pairResyncStatus.Add(pair.PairId, new Tuple<ThrottleState, long>(
                            ThrottleState.NotThrottled, resyncFileSize));
                        return;
                    }

                    if (pair.ResyncThrottleLimit < 0)
                        throw new Exception("Invalid resync throttle threshold");

                    string resyncThreshold = FSUtils.GetMemoryWithHighestUnits((ulong)pair.ResyncThrottleLimit);

                    if (pair.ResyncThrottleLimit != 0 && resyncFileSize >= pair.ResyncThrottleLimit)
                    {
                        Tracers.Monitoring.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Resync throttle set for pair {0}:{1}. Pending resync data {2} bytes.",
                        pair.HostId, pair.DeviceId, resyncFileSize);

                        resyncThrottleState = ThrottleState.Throttled;

                        Dictionary<string, object> placeholders = new Dictionary<string, object>()
                        {
                            { "SrcHostName", pair.Hostname },
                            { "SrcVolume", pair.DeviceId },
                            { "DestHostName", pair.TargetHostName },
                            { "DestVolume", pair.TargetDeviceId },
                            { "VolumeSize", resyncThreshold }
                        };

                        string errMessage = $"Resync cache folder for the pair {pair.Hostname} ({pair.DeviceId}) " +
                        $"==> {pair.TargetHostName} ({pair.TargetDeviceId}) has exceeded the " +
                        $"configured threshold of {resyncThreshold}";
                        string errSummary = "Resync Data flow controlled -Resync Cache Folder " +
                        "for replication pair has exceeded configured threshold";
                        string errId = $"{pair.HostId}_{pair.DeviceId}_{pair.TargetHostId}_{pair.TargetDeviceId}";

                        var psHealthEvent = new PSHealthEvent(
                            errorCode: "EC0110",
                            errorSummary: errSummary,
                            errorMessage: errMessage,
                            errorPlaceHolders: placeholders,
                            errorTemplateId: "CXSTOR_WARN",
                            hostId: pair.HostId,
                            errorId: errId);

                        TaskUtils.RunTaskForAsyncOperation(
                            traceSource: Tracers.Monitoring,
                            name: $"{nameof(psApiStubsImpl.ReportPSEventsAsync)}",
                            ct: token,
                            operation: async (taskCancelToken) =>
                            {
                                await psApiStubsImpl.
                                ReportPSEventsAsync(psHealthEvent, taskCancelToken).
                                ConfigureAwait(false);
                            });
                    }
                    else
                    {
                        resyncThrottleState = ThrottleState.NotThrottled;
                    }

                    pairResyncStatus.Add(pair.PairId, new Tuple<ThrottleState, long>(
                        resyncThrottleState, resyncFileSize));
                }, Tracers.Monitoring);
            }

            if (resyncFolderSizeCalcTime > Settings.Default.ResyncFolderSizeCalcThreshold)
            {
                Tracers.Monitoring.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "Time taken to calculate resync folder size for {0} pairs is {1} ms.",
                    replicationPairs.Count, resyncFolderSizeCalcTime.TotalMilliseconds);
            }

            if (pairResyncStatus.Any())
            {
                return TaskUtils.RunTaskForAsyncOperation(
                    traceSource: Tracers.Monitoring,
                    name: $"{nameof(psApiStubsImpl.UpdateResyncThrottleAsync)}",
                    ct: token,
                    operation: async (taskCancelToken) =>
                    await psApiStubsImpl.UpdateResyncThrottleAsync(
                        resyncThrottleInfo: pairResyncStatus,
                        token: taskCancelToken).
                        ConfigureAwait(false));
            }
                        else
            {
                return null;
            }
        }

        private (IRDRStuckHealthFactor, string) GetIRStuckHealth(IProcessServerPairSettings pair)
        {
            IRDRStuckHealthFactor IRStuckHealthFactor = IRDRStuckHealthFactor.Healthy;
            string IRStuckHealthDescription = string.Empty;

            if (DateTime.UtcNow - pair.IrProgressUpdateTimeUtc >
                PSInstallationInfo.Default.GetIRDRStuckCriticalThreshold())
            {
                IRStuckHealthFactor = IRDRStuckHealthFactor.Critical;
                IRStuckHealthDescription = IRCriticalHealthFactor;
            }
            else if (DateTime.UtcNow - pair.IrProgressUpdateTimeUtc >
                PSInstallationInfo.Default.GetIRDRStuckWarningThreshold())
            {
                IRStuckHealthFactor = IRDRStuckHealthFactor.Warning;
                IRStuckHealthDescription = IRWarningHealthFactor;
            }

            Tracers.Monitoring.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                "IR stuck health factors for pair {0}-{1} are {2}IR health factor : {3}, IR Health Description {4}",
                pair.HostId, pair.DeviceId, Environment.NewLine,
                IRStuckHealthFactor, IRStuckHealthDescription);

            return (IRStuckHealthFactor, IRStuckHealthDescription);
        }

        private void UpdateRPOFile(IProcessServerPairSettings pair, TimeSpan rpo)
        {
            TaskUtils.RunAndIgnoreException(() =>
            {
                string rpoFilePath = Path.Combine(pair.PerfFolder, RPO_FILE);
                File.AppendAllLines(rpoFilePath, new string[] { ((long)rpo.TotalSeconds).ToString() });
            }, Tracers.Monitoring);
        }

        private Task MonitorDiffThrottleAsync(CancellationToken cancellationToken)
        {
            List<IProcessServerPairSettings> replicationPairs = GetReplicationPairs(cancellationToken);
            Dictionary<int, Tuple<ThrottleState, long>> pairDiffStatus = new Dictionary<int, Tuple<ThrottleState, long>>();
            TimeSpan diffFolderSizeCalcTime = TimeSpan.Zero;
            foreach (var pair in replicationPairs)
            {
                TaskUtils.RunAndIgnoreException(() =>
                {
                    if (pair.FarlineProtected != FarLineProtected.Protected)
                    {
                        Tracers.Monitoring.TraceAdminLogV2Message(
                            TraceEventType.Verbose,
                            "The pair is not protected {0}:{1}",
                            pair.HostId, pair.DeviceId);
                        return;
                    }

                    Stopwatch pairDiffFolderSizeCalcTimeSw = new Stopwatch();
                    pairDiffFolderSizeCalcTimeSw.Start();
                    long diffFileSize = 0;
                    if (Directory.Exists(pair.SourceDiffFolder))
                    {
                        diffFileSize += FSUtils.GetFilesSize(pair.SourceDiffFolder,
                            SearchOption.TopDirectoryOnly,
                            sourceDiffFilePatterns);
                    }
                    if (Directory.Exists(pair.TargetDiffFolder))
                    {
                        diffFileSize += FSUtils.GetFilesSize(pair.TargetDiffFolder,
                            SearchOption.TopDirectoryOnly,
                            targetDiffFilePatterns);
                    }
                    diffFolderSizeCalcTime = diffFolderSizeCalcTime.Add(pairDiffFolderSizeCalcTimeSw.Elapsed);
                    pairDiffFolderSizeCalcTimeSw.Stop();

                    if (pair.CompatibilityNumber > Settings.Default.DiffThrottleMaxVer)
                    {
                        Tracers.Monitoring.DebugAdminLogV2Message(
                            "The compatibility number {0} for the current host is greater than supported version {1}",
                            pair.CompatibilityNumber, Settings.Default.DiffThrottleMaxVer);

                        pairDiffStatus.Add(pair.PairId, new Tuple<ThrottleState, long>(
                            ThrottleState.NotThrottled, diffFileSize));
                        return;
                    }

                    if (pair.DiffThrottleLimit < 0)
                        throw new Exception("Invalid diff throttle threshold");

                    string diffThreshold = FSUtils.GetMemoryWithHighestUnits((ulong)pair.DiffThrottleLimit);

                    ThrottleState diffThrottleState = ThrottleState.Unknown;

                    if (pair.DiffThrottleLimit != 0 && diffFileSize >= pair.DiffThrottleLimit)
                    {
                        Tracers.Monitoring.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Diff throttle set for pair {0}:{1}. Pending diff data {2} bytes.",
                            pair.HostId, pair.DeviceId, diffFileSize);

                        diffThrottleState = ThrottleState.Throttled;

                        Dictionary<string, object> placeholders = new Dictionary<string, object>()
                        {
                            { "SrcHostName", pair.Hostname },
                            { "SrcVolume", pair.DeviceId },
                            { "DestHostName", pair.TargetHostName },
                            { "DestVolume", pair.TargetDeviceId },
                            { "VolumeSize", diffThreshold }
                        };

                        string errMessage = $"Differential sync cache folder for the pair: " +
                        $"{pair.Hostname} [{pair.DeviceId}] -> {pair.TargetHostName} [{pair.TargetDeviceId}] " +
                        $"has exceeded the configured threshold of {diffThreshold}";

                        string errSummary = "Differential Data flow controlled - " +
                        "Differential Sync Cache Folder for replication pair has exceeded configured threshold";
                        string errId = $"{pair.HostId}_{pair.DeviceId}_{pair.TargetHostId}_{pair.TargetDeviceId}";

                        var psHealthEvent = new PSHealthEvent(
                            errorCode: "EC0111",
                            errorSummary: errSummary,
                            errorMessage: errMessage,
                            errorPlaceHolders: placeholders,
                            errorTemplateId: "CXSTOR_WARN",
                            hostId: pair.HostId,
                            errorId: errId);

                        TaskUtils.RunTaskForAsyncOperation(
                            traceSource: Tracers.Monitoring,
                            name: $"{nameof(psApiStubsImpl.ReportPSEventsAsync)}",
                            ct: cancellationToken,
                            operation: async (taskCancelToken) =>
                            {
                                await psApiStubsImpl.
                                ReportPSEventsAsync(
                                    psHealthEvent: psHealthEvent,
                                    cancellationToken: taskCancelToken).
                                ConfigureAwait(false);
                            });
                    }
                    else
                    {
                        diffThrottleState = ThrottleState.NotThrottled;
                    }

                    pairDiffStatus.Add(pair.PairId, new Tuple<ThrottleState, long>(
                        diffThrottleState, diffFileSize));

                }, Tracers.Monitoring);
            }

            if (diffFolderSizeCalcTime > Settings.Default.DiffFolderSizeCalcThreshold)
            {
                Tracers.Monitoring.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "Time taken to calculate diff folder size for {0} pairs is {1} ms.",
                    replicationPairs.Count, diffFolderSizeCalcTime.TotalMilliseconds);
            }

            if (pairDiffStatus.Any())
            {
                return TaskUtils.RunTaskForAsyncOperation(
                    traceSource: Tracers.Monitoring,
                    name: $"{nameof(psApiStubsImpl.UpdateDiffThrottleAsync)}",
                    ct: cancellationToken,
                    operation: async (taskCancelToken) =>
                    await psApiStubsImpl.UpdateDiffThrottleAsync(
                        diffThrottleInfo: pairDiffStatus,
                        token: taskCancelToken).
                        ConfigureAwait(false));
            }
            else
            {
                return null;
            }
        }

        private Task MonitorSouceReplicationBlockedAsync(CancellationToken token)
        {
            List<IProcessServerPairSettings> replicationPairs = GetReplicationPairs(token);
            Dictionary<int, int> dataUploadBlockedStatus = new Dictionary<int, int>();
            foreach (var pair in replicationPairs)
            {
                TaskUtils.RunAndIgnoreException(() =>
                {
                    if (pair.ExecutionState != ExecutionState.Default &&
                        pair.ExecutionState != ExecutionState.Active &&
                        pair.ExecutionState != ExecutionState.Waiting)
                    {
                        Tracers.Monitoring.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "The pair {0}:{1} is currently not replicating, so skipping data upload block check",
                            pair.HostId, pair.DeviceId);
                        return;
                    }

                    string[] monitorFileContents = File.ReadAllLines(pair.RPOFilePath);
                    if (monitorFileContents.Count() != 2)
                    {
                        Tracers.Monitoring.TraceAdminLogV2Message(
                            TraceEventType.Warning,
                            "Monitor file for the pair {0}:{1} is inconsistent, so skipping data upload block check",
                            pair.HostId, pair.DeviceId);
                        return;
                    }

                    string[] fileUploadTimeStamps = monitorFileContents[1].Split(new char[] { ':' },
                        StringSplitOptions.None);

                    if (fileUploadTimeStamps.Length != 3)
                    {
                        Tracers.Monitoring.TraceAdminLogV2Message(
                            TraceEventType.Warning,
                            "Monitor file for the pair {0}:{1} is inconsistent, so skipping data upload block check",
                            pair.HostId, pair.DeviceId);
                        return;
                    }

                    // if there is a failure in parsing the timestamps,
                    // only the current pair will be impacted.
                    ulong lastProcessTime = ulong.Parse(fileUploadTimeStamps[1], CultureInfo.InvariantCulture);
                    ulong lastFileUploadTime = ulong.Parse(fileUploadTimeStamps[0], CultureInfo.InvariantCulture);

                    if (lastFileUploadTime != 0 && 
                    (lastProcessTime - lastFileUploadTime) >=
                    (ulong)PSInstallationInfo.Default.GetSourceUploadBlockedDuration().TotalSeconds)
                    {
                        dataUploadBlockedStatus.Add(pair.PairId, 1);
                        Tracers.Monitoring.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Setting data upload blocked for pair {0}:{1}",
                            pair.HostId, pair.DeviceId);
                    }
                    else if (pair.SourceCommunicationBlocked == (sbyte)1)
                    {
                        dataUploadBlockedStatus.Add(pair.PairId, 0);
                        Tracers.Monitoring.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Clear data upload blocked for pair {0}:{1}",
                        pair.HostId, pair.DeviceId);
                    }
                }, Tracers.Monitoring);
            }

            if (dataUploadBlockedStatus.Any())
            {
                return TaskUtils.RunTaskForAsyncOperation(
                    traceSource: Tracers.Monitoring,
                    name: nameof(psApiStubsImpl.ReportDataUploadBlockedAsync),
                    ct: token,
                    operation: async (taskCancToken) =>
                    await psApiStubsImpl.ReportDataUploadBlockedAsync(
                    new DataUploadBlockedUpdateItem(dataUploadBlockedStatus),
                    taskCancToken).ConfigureAwait(false));
            }
            else
            {
                // Should we return completed task here??
                // return Task.CompletedTask
                return null;
            }
        }

        private List<IProcessServerPairSettings> GetReplicationPairs(CancellationToken cancellationToken)
        {
            ProcessServerSettings psSettings;

            try
            {
                psSettings = ProcessServerSettings.GetCachedSettings();
            }
            catch (Exception ex)
            {
                throw new Exception("Failed to get PS cache settings", ex);
            }

            if (psSettings == null)
            {
                Tracers.Monitoring.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Couldn't get cached PS settings");

                return new List<IProcessServerPairSettings>();
            }

            if (psSettings.Pairs == null)
            {
                Tracers.Monitoring.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Couldn't get pair settings from CS");

                return new List<IProcessServerPairSettings>();
            }
            else if (!psSettings.Pairs.Any())
            {
                Tracers.Monitoring.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Replication pairs are not configured");

                return new List<IProcessServerPairSettings>();
            }

            return psSettings.Pairs.ToList();
        }

        private IEnumerable<IProcessServerHostSettings> getMonitoredHost
            (IEnumerable<IProcessServerHostSettings> hosts)
        {
            if (hosts != null)
            {
                return
                    hosts
                    .Where(host => host.ProtectionState != null)
                    .ToList();
            }

            return Enumerable.Empty<IProcessServerHostSettings>();
        }

        private async Task MonitorHealthForRcmPS(CancellationToken cancellationToken)
        {
            // Stopwatch for monitor telemetry event
            Stopwatch monitorTelemetrySW = new Stopwatch();
            monitorTelemetrySW.Start();

            // Task for monitoring telemetry
            Task monitorTelemetryTask = null;

            while (true)
            {
                try
                {
                    var psSettings = ProcessServerSettings.GetCachedSettings();

                    if (psSettings != null)
                    {
                        IEnumerable<IProcessServerHostSettings> monitoredHost = getMonitoredHost(psSettings.Hosts);
                        RcmJobProcessor.SetMonitoredHost(monitoredHost);

                        Dictionary<string, string> hostsWithSameBiosId = new Dictionary<string, string>();
                        hostsWithSameBiosId = monitoredHost.GroupBy(host => host.BiosId)
                                                    .Where(host => host.Count() > 1)
                                                    .SelectMany(host => host)
                                                    .ToDictionary(host => host.HostId, host => host.BiosId);

                        if (hostsWithSameBiosId.Count > 0)
                        {
                            Tracers.Monitoring.TraceAdminLogV2Message(
                                            TraceEventType.Information,
                                            "Following hosts have same biosid : {0}",
                                            string.Join(";", hostsWithSameBiosId.Select(x => x.Key + "=" + x.Value)));
                        }

                        //ToDo:[Himanshu] Add parallelism to make the process faster
                        foreach (var host in monitoredHost ?? Enumerable.Empty<IProcessServerHostSettings>())
                        {
                            cancellationToken.ThrowIfCancellationRequested();

                            ProcessServerProtectionPairHealthMsg hostHealth = new ProcessServerProtectionPairHealthMsg();
                            //ToDo:[Himanshu] Handle cancellation token
                            foreach (var pair in host.Pairs ?? Enumerable.Empty<IProcessServerPairSettings>())
                            {
                                ProcessServerReplicationPairHealthMsg pairHealth = new ProcessServerReplicationPairHealthMsg
                                {
                                    ReplicationPairContext = pair.MessageContext
                                };

                                if (MonitorPairForUploadBlocked(pair))
                                {
                                    pairHealth.HealthIssues.Add(new HealthIssue(RCMSourceDataUploadBlockedEC,
                                                                      HealthFactorWarning.ToString(),
                                                                      MonitoringMessageSource.ProcessServer.ToString(),
                                                                      DateTime.UtcNow,
                                                                      null,
                                                                      MonitoringMessageSource.ProcessServer.ToString()));
                                }

                                MonitorPairForReplicationBlocked(
                                        pair,
                                        out string errorCode);
                                if (errorCode != null)
                                {
                                    pairHealth.HealthIssues.Add(new HealthIssue(errorCode,
                                                                      HealthFactorCritical.ToString(),
                                                                      MonitoringMessageSource.ProcessServer.ToString(),
                                                                      DateTime.UtcNow,
                                                                      null,
                                                                      MonitoringMessageSource.ProcessServer.ToString()));
                                }

                                hostHealth.ReplicationPairHealthDetails.Add(pairHealth);
                            }

                            //Todo: replace with TaskUtils runAsyncOp
                            await Task.Run(() => psApiStubsImpl.ReportHostHealthAsync(psSettings.IsPrivateEndpointEnabled, host, hostHealth, cancellationToken),
                                            cancellationToken).ConfigureAwait(false);

                            if (Settings.Default.RCM_MonitorPSV2Telemetry &&
                                monitorTelemetrySW.Elapsed >= Settings.Default.RCM_MonitorPSV2TelemetryInterval)
                            {
                                monitorTelemetrySW.Restart();
                                if (monitorTelemetryTask == null || monitorTelemetryTask.IsCompleted)
                                {
                                    monitorTelemetryTask = MonitorTelemetryForRCMPS(host.Pairs, cancellationToken);
                                }
                                else
                                {
                                    Tracers.Monitoring.TraceAdminLogV2Message(
                                            TraceEventType.Warning,
                                             $"Skipping the current {nameof(monitorTelemetryTask)} as last " +
                                            $"triggered {nameof(monitorTelemetryTask)} is still running.");
                                }
                            }
                        }
                    }
                }
                catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
                {
                    // Rethrow operation canceled exception if the service is stopped.
                    Tracers.Monitoring.TraceAdminLogV2Message(
                                    TraceEventType.Verbose,
                                    "Cancelling monitor health service Task");
                    throw;
                }
                catch (Exception ex)
                {
                    Tracers.Monitoring.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Exception at MonitorHealthService - {0}",
                        ex);
                }

                // Wait time before starting another iteration to check for monitor health.
                await Task.Delay(
                    PSCoreSettings.Default.MonHealthServiceMainIterWaitInterval, cancellationToken).ConfigureAwait(false);
            }
        }

        private bool MonitorPairForUploadBlocked(
            IProcessServerPairSettings pair)
        {
            if (PSInstallationInfo.Default.CSMode == CSMode.Rcm)
            {
                try
                {
                    // Here checks based on execution states were put in the perl scripts in the legacy stack.
                    // Removing them as execution state is no longer available in settings.
                    //Todo: think about taking lock on the file
                    string monitor_file = FSUtils.GetLongPath(
                        Path.Combine(GetDiffFolderToMonitorForPair(pair), MONITOR_FILE),
                        isFile: true,
                        optimalPath: false);
                    string[] lines = File.ReadAllLines(monitor_file);
                    string timeStampEpoch = lines[1].Split(':')[1];
                    DateTimeOffset lastFileInTime =
                        DateTimeOffset.FromUnixTimeSeconds(long.Parse(timeStampEpoch));
                    if (
                        DateTime.UtcNow - lastFileInTime >
                        PSCoreSettings.Default.Monitoring_UploadBlockedTimeThreshold)
                    {
                        return true;
                    }
                }
                catch (Exception ex)
                {
                    Tracers.Monitoring.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "[MonitorUploadBlocked]Failed for pair {0}-{1} and folder {2} " +
                        "with error {3}{4}",
                        pair.DeviceId,
                        pair.HostId,
                        GetDiffFolderToMonitorForPair(pair),
                        Environment.NewLine,
                        ex);
                    return false;
                }
            }

            return false;
        }

        private static string GetDiffFolderToMonitorForPair(IProcessServerPairSettings pair)
        {
            // In case of legacy, DiffFoldersToMonitor will be more than one, in case of RCM its one folder.
            return pair.DiffFoldersToMonitor.Single();
        }

        private void MonitorPairForReplicationBlocked(
            IProcessServerPairSettings pair,
            out string errorCode)
        {
            errorCode = null;
            string filePath = PSInstallationInfo.Default.CSMode != CSMode.Rcm ?
                pair.SourceDiffFolder : pair.TargetDiffFolder;

            // In legacy CS they are checking for execution state, deleted flag,
            // tar_replication_status and src_replication_status to check that replication is
            // healthy and isresync and isquasi flag to check
            // if in IR and source and target heartbeats to ensure both are up
            bool isPairStateIr = false;
            IRDRStuckHealthFactor irDrFileStuckHealthFactor = IRDRStuckHealthFactor.Healthy;
            if (pair.ProtectionState == ProtectionState.ResyncStep1)
            {
                VerifyIRFileStuck(pair, out irDrFileStuckHealthFactor, out string healthDescription);
                isPairStateIr = true;
            }
            else if (pair.ProtectionState == ProtectionState.ResyncStep2 || pair.ProtectionState == ProtectionState.DifferentialSync)
            {
                (irDrFileStuckHealthFactor, _) = VerifyDRFileStuck(pair);
            }

            if (irDrFileStuckHealthFactor == IRDRStuckHealthFactor.Critical)
            {
                errorCode = RCMIRDRFileStuckHealthErrorCodes.IRDRStuckCriticalCode;
            }
            else if (irDrFileStuckHealthFactor == IRDRStuckHealthFactor.Warning)
            {
                errorCode = RCMIRDRFileStuckHealthErrorCodes.IRDRStuckWarningCode;
            }

            if (isPairStateIr)
            {
                // delete drStuck file
                string drStuckFilePath = FSUtils.GetLongPath(
                                   Path.Combine(pair.TargetDiffFolder, DR_STUCK_FILE),
                                   isFile: true,
                                   optimalPath: false);
                if (File.Exists(drStuckFilePath))
                {
                    File.Delete(drStuckFilePath);
                }
            }
        }

        private (IRDRStuckHealthFactor, string)
            VerifyDRFileStuck(IProcessServerPairSettings pair)
        {
            string oldestFileName = string.Empty;
            string oldestFilePath = string.Empty;
            IRDRStuckHealthFactor irDrFileStuckHealthFactor = IRDRStuckHealthFactor.Healthy;
            string healthDescription = string.Empty;
            TaskUtils.RunAndIgnoreException(() =>
            {
                string drStuckFilePath = FSUtils.GetLongPath(
                                    Path.Combine(pair.TargetDiffFolder, DR_STUCK_FILE),
                                    isFile: true,
                                    optimalPath: false);
                if (File.Exists(drStuckFilePath))
                {
                    oldestFileName = File.ReadAllText(drStuckFilePath);
                }

                if (!string.IsNullOrWhiteSpace(oldestFileName))
                {
                    oldestFilePath = FSUtils.GetLongPath(
                                        Path.Combine(pair.TargetDiffFolder, oldestFileName),
                                        isFile: true,
                                        optimalPath: false);
                }

                if (File.Exists(oldestFilePath))
                {
                    DateTime lastWriteTime = File.GetLastWriteTimeUtc(oldestFilePath);
                    if (DateTime.UtcNow - lastWriteTime >
                            PSInstallationInfo.Default.GetIRDRStuckCriticalThreshold())
                    {
                        irDrFileStuckHealthFactor = IRDRStuckHealthFactor.Critical;
                        healthDescription = DRCriticalHealthFactor;
                    }
                    else if (DateTime.UtcNow - lastWriteTime >
                        PSInstallationInfo.Default.GetIRDRStuckWarningThreshold())
                    {
                        irDrFileStuckHealthFactor = IRDRStuckHealthFactor.Warning;
                        healthDescription = DRWarningHealthFactor;
                    }
                }
                else
                {
                    if (File.Exists(drStuckFilePath))
                    {
                        File.Delete(drStuckFilePath);
                    }

                    FileInfo file = FSUtils.GetOldestFile(
                            pair.TargetDiffFolder,
                            SearchOption.TopDirectoryOnly,
                            targetDiffFilePatterns);

                    if (file != null && !string.IsNullOrWhiteSpace(file.Name))
                    {
                        File.WriteAllText(drStuckFilePath, file.Name);
                    }
                }
            }, Tracers.Monitoring);
            
            return (irDrFileStuckHealthFactor, healthDescription);
        }

        /// <summary>
        /// Checks for IR stuck.
        /// </summary>
        /// <param name="oldestFileMTimeUTC">IR progress time.</param>
        /// <param name="irDrFileStuckHealthFactor">Health factor to signify warning or critical.</param>
        /// <param name="healthDescription">Health description.</param>
        private void VerifyIRFileStuck(IProcessServerPairSettings pair,
            out IRDRStuckHealthFactor irDrFileStuckHealthFactor,
            out string healthDescription)
        {
            irDrFileStuckHealthFactor = IRDRStuckHealthFactor.Healthy;
            healthDescription = string.Empty;
            try
            {
                FileInfo oldestFile = FSUtils.GetOldestFile(
                        pair.ResyncFolderToMonitor,
                        SearchOption.TopDirectoryOnly,
                        resyncFilePatterns);
                if (oldestFile != null)
                {
                    DateTime oldestFileMTimeUTC = oldestFile.LastWriteTimeUtc;
                    if ((DateTime.UtcNow - oldestFileMTimeUTC) > PSCoreSettings.Default.Monitoring_ReplicationBlockedCriticalThreshold)
                    {
                        irDrFileStuckHealthFactor = IRDRStuckHealthFactor.Critical;
                        healthDescription = IRCriticalHealthFactor;
                    }
                    else if (DateTime.UtcNow - oldestFileMTimeUTC >
                                PSCoreSettings.Default.Monitoring_ReplicationBlockedWarningThreshold)
                    {
                        irDrFileStuckHealthFactor = IRDRStuckHealthFactor.Warning;
                        healthDescription = IRWarningHealthFactor;
                    }
                }
            }
            catch (Exception ex)
            {
                Tracers.Monitoring.TraceAdminLogV2Message(
                                TraceEventType.Error,
                                "Failed to monitor IR stuck for pair {0}-{1} and folder {2}" +
                                " with error : {3}{4}",
                                pair.DeviceId,
                                pair.HostId,
                                pair.ResyncFolderToMonitor,
                                Environment.NewLine,
                                ex);
            }
        }

        /// <summary>
        /// Monitors pair's RPO.
        /// </summary>
        /// <param name="pair">Replication pair.</param>
        /// <param name="current_rpo">Current RPO.</param>
        /// <returns></returns>
        private void MonitorPairForRPO(
                IProcessServerPairSettings pair,
                out TimeSpan current_rpo)
        {
            current_rpo = TimeSpan.MaxValue;
            DateTime LastTagFileMTime = DateTime.UtcNow;
            bool deletedTagFile = false;

            try
            {
                IEnumerable<FileInfo> tagFiles = 
                    FSUtils.GetFileListIgnoreException(pair.TagFolder,
                    TAG_FILES_PATTERN,
                    SearchOption.TopDirectoryOnly);

                IEnumerable<FileInfo> tagFilesInTargetFolder =
                    FSUtils.GetFileListIgnoreException(pair.TargetDiffFolder,
                    TAG_FILES_PATTERN,
                    SearchOption.TopDirectoryOnly);

                IEnumerable<FileInfo> tagFilesInPendingRecoverableFolder =
                    FSUtils.GetFileListIgnoreException(pair.AzurePendingUploadRecoverableFolder,
                    TAG_FILES_PATTERN,
                    SearchOption.TopDirectoryOnly);

                IEnumerable<FileInfo> tagFilesInPendingNonRecoverableFolder =
                    FSUtils.GetFileListIgnoreException(pair.AzurePendingUploadNonRecoverableFolder,
                    TAG_FILES_PATTERN,
                    SearchOption.TopDirectoryOnly);

                foreach (FileInfo file in tagFiles)
                {
                    //Check if the tag file has crossed the fault boundary.
                    //checking with fileName as exact object might differ.

                    // if the tag file is present in target source directory, AzurePendingRecoverable,
                    // or AzurePendingNonRecoverable folders, then they are not uploaded by Mars yet.
                    // So ignoring them from RPO calculation.

                    if (tagFilesInTargetFolder.Any(f =>
                    f.Name.Equals(file.Name, StringComparison.OrdinalIgnoreCase)) ||
                    tagFilesInPendingRecoverableFolder.Any(f =>
                    f.Name.Equals(file.Name, StringComparison.OrdinalIgnoreCase)) ||
                    tagFilesInPendingNonRecoverableFolder.Any(f =>
                    f.Name.Equals(file.Name, StringComparison.OrdinalIgnoreCase)))
                    {
                        continue;
                    }

                    // RPO is taken as the difference of current time and the last
                    // uploaded tag file write time.
                    // To get the last uploaded tag file, select the file which is uploaded
                    // and which has the lowest time difference.
                    if (DateTime.UtcNow - file.LastWriteTimeUtc < current_rpo)
                    {
                        current_rpo = DateTime.UtcNow - file.LastWriteTimeUtc;
                        LastTagFileMTime = file.LastWriteTimeUtc;
                    }

                    // Delete the tag files already processed.
                    file.Delete();
                    deletedTagFile = true;
                }

                if (!deletedTagFile)
                {
                    string lastTagTimeStampFilePath = FSUtils.GetLongPath(
                                Path.Combine(pair.TagFolder, RPO_LASTTAGTIMEUTC),
                                isFile: true,
                                optimalPath: false);
                    DateTime lastTagTime = DateTime.MinValue;
                    string errorMsg = string.Empty;
                    if (File.Exists(lastTagTimeStampFilePath))
                    {
                        string fileContents = File.ReadAllText(lastTagTimeStampFilePath);
                        if (!DateTime.TryParseExact(
                                fileContents,
                                RpoTimeDateFormat,
                                CultureInfo.InvariantCulture,
                                DateTimeStyles.None,
                                out lastTagTime))
                        {
                            // Earlier in legacy ps, the last file write was stored as seconds since epoch.
                            // Changing it to Datetime format.
                            if(PSInstallationInfo.Default.CSMode == CSMode.LegacyCS &&
                                long.TryParse(fileContents,
                                NumberStyles.None,
                                CultureInfo.InvariantCulture,
                                out long RPOInSec))
                            {
                                lastTagTime = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc)
                                        .AddSeconds(RPOInSec)
                                        .ToUniversalTime();
                            }
                            else
                            {
                                errorMsg = string.Format(
                                    "Failed to parse {0} content : {1}",
                                    lastTagTimeStampFilePath, fileContents);
                                lastTagTime = DateTime.MinValue;
                            }
                        }
                        else
                        {
                            errorMsg = string.Format(
                                "Failed to parse datetime {0} content : {1}",
                                lastTagTimeStampFilePath, fileContents);
                        }
                    }
                    else
                    {
                        errorMsg = string.Format("{0} is not found", lastTagTimeStampFilePath);
                    }

                    // If the last file move time can't be found,
                    // set the RPO value to 0
                    if (lastTagTime == DateTime.MinValue)
                    {
                        Tracers.Monitoring.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "{0}. Setting the rpo to zero for the pair {1}-{2}",
                            errorMsg, pair.HostId, pair.DeviceId);

                        current_rpo = TimeSpan.Zero;
                    }
                    else
                    {
                        current_rpo = DateTime.UtcNow - lastTagTime;
                    }
                }
                else
                {
                    string lastTagTimeStampFilePath = FSUtils.GetLongPath(
                                Path.Combine(pair.TagFolder, RPO_LASTTAGTIMEUTC),
                                isFile: true,
                                optimalPath: false);
                    File.WriteAllText(lastTagTimeStampFilePath,
                        LastTagFileMTime.ToString(RpoTimeDateFormat));
                }

                if (current_rpo < TimeSpan.Zero)
                {
                    Tracers.Monitoring.TraceAdminLogV2Message(
                        TraceEventType.Warning,
                        "Current rpo is coming as negative {0} signalling time push back, resetting the RPO to 0",
                        current_rpo);
                    current_rpo = TimeSpan.Zero;
                }
            }
            catch (Exception ex)
            {
                Tracers.Monitoring.TraceAdminLogV2Message(
                                TraceEventType.Error,
                                "Failed to monitor RPO for pair {0}-{1}" +
                                " with exception : {2}{3}{4}." +
                                "So resetting RPO to zero.",
                                pair.HostId,
                                pair.DeviceId,
                                Environment.NewLine,
                                ex,
                                Environment.NewLine);
                // Setting RPO to 0 for any exceptions.
                current_rpo = TimeSpan.Zero;
            }
        }

        private bool MonitorPairForDRThrottle(
            IProcessServerPairSettings pair)
        {
            try
            {
                long totalSize = 0;
                // Should be checked only if IR is not in progress
                // in perl scripts, it is getting raised if isQuasiFLag != 0
                if (pair.ProtectionState == ProtectionState.ResyncStep2 ||
                    pair.ProtectionState == ProtectionState.DifferentialSync)
                {
                    //Todo:[Himanshu] Replace with PS Mode check.
                    if (PSInstallationInfo.Default.CSMode == CSMode.Rcm)
                    {
                       totalSize += FSUtils.GetFilesSize(GetDiffFolderToMonitorForPair(pair),
                                SearchOption.AllDirectories,
                                targetDiffFilePatterns);

                            Tracers.Monitoring.TraceAdminLogV2Message(
                                TraceEventType.Verbose,
                                "Total diff file size is {0} for pair {1} host {2} dir is {3}",
                                totalSize.ToString(),
                                pair.DeviceId,
                                pair.HostId,
                                GetDiffFolderToMonitorForPair(pair));
                    }
                    else
                    {
                        throw new NotImplementedException();
                    }
                    return totalSize >= pair.DiffThrottleLimit;
                }
            }
            catch (Exception ex)
            {
                Tracers.Monitoring.DebugAdminLogV2Message(
                                "Failed to check for diff throttle pair {0}-{1} and folder {2}" +
                                " with error : {3}{4}",
                                pair.DeviceId,
                                pair.HostId,
                                GetDiffFolderToMonitorForPair(pair),
                                Environment.NewLine,
                                ex);
            }
            
            return false;
        }

        /// <summary>
        /// Monitors resync folder for throttle.
        /// </summary>
        /// <param name="pair">Replicatioin pair.</param>
        /// <returns>True if IR throttle is to be set.</returns>
        private bool MonitorPairForIRThrottle(
            IProcessServerPairSettings pair)
        {
            try
            {
                // This check is not there in legacy, Added this check here
                // to avoid directory not found exceptions during diffsync stage
                if (pair.ProtectionState == ProtectionState.ResyncStep1)
                {
                    long totalSize = 0;

                    totalSize += FSUtils.GetFilesSize(
                                        pair.ResyncFolderToMonitor,
                                        SearchOption.AllDirectories,
                                        resyncFilePatterns);

                    Tracers.Monitoring.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Total resync file size is {0} pair {1} host {2} dir is {3}",
                        totalSize,
                        pair.DeviceId,
                        pair.HostId,
                        pair.ResyncFolderToMonitor);
                    return totalSize >= pair.ResyncThrottleLimit;
                }
            }
            catch (Exception ex)
            {
                Tracers.Monitoring.TraceAdminLogV2Message(
                                TraceEventType.Error,
                                "Failed to check for resync throttle pair {0}-{1} folder {2}" +
                                " with error : {3}{4}",
                                pair.DeviceId,
                                pair.HostId,
                                pair.ResyncFolderToMonitor,
                                Environment.NewLine,
                                ex);
            }
            
            return false;
        }

        private Task MonitorTelemetryForRCMPS(IEnumerable<IProcessServerPairSettings> pairs, CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            const string TGT_DIFF_FILES_PATTERN = "completed_ediff*";
            const string SYNC_FILES_PATTERN = "completed_sync*.dat";

            long lastSystemRebootTime = SystemUtils.GetLastSystemRebootTime(Tracers.Monitoring);
            var (totCacheFolSizeInBytes, freeVolSizeInKB, usedVolSizeInKB) =
                FSUtils.GetCacheVolUsageStats(
                    PSInstallationInfo.Default.GetReplicationLogFolderPath(),
                    Tracers.Monitoring);

            bool isCumulativeThrottleSet = SystemUtils.IsCumulativeThrottled(
                PSCoreSettings.Default.ProcessServerSettings_Rcm_DefaultCumulativeThrottleLimit,
                PSInstallationInfo.Default.GetReplicationLogFolderPath(),
                Tracers.Monitoring);
            int isCumulativeThrottled = isCumulativeThrottleSet ? 1 : 0;

            foreach (var pair in pairs)
            {
                ct.ThrowIfCancellationRequested();

                var telemetryData = InMageTelemetryPSV2Row.BuildDefaultRow();
                TaskUtils.RunAndIgnoreException(() =>
                {
                    Tracers.Monitoring.TraceAdminLogV2Message(TraceEventType.Information,
                    "MonitorTelemetry : Collecting telemetry data for Source HostId: {0}," +
                    " Device Id: {1}, ReplicationState  {2}, Resync Folder : {3}, DiffSync Folder : {4}",
                    pair.HostId, pair.DeviceId, pair.ReplicationState,
                    pair.ResyncFolder, pair.TargetDiffFolder);

                    var (srcToPSFileUploadStuck, monitorTxtFileExists,
                    lastMoveFile, lastMoveFileModTime, lastMoveFileProcTime) =
                    DiffFileUtils.GetRPOData(pair, Tracers.Monitoring, ct);

                    telemetryData.SysTime = DateTimeUtils.GetSecondsAfterEpoch();
                    telemetryData.MessageType = 3;
                    telemetryData.HostId =
                    (!string.IsNullOrWhiteSpace(pair.HostId)) ? pair.HostId : string.Empty;
                    telemetryData.DiskId =
                    (!string.IsNullOrWhiteSpace(pair.DeviceId)) ? pair.DeviceId : string.Empty;
                    telemetryData.PSHostId = PSInstallationInfo.Default.GetPSHostId();
                    telemetryData.PsAgentVer = pair.PSVersion;
                    telemetryData.SysBootTime = lastSystemRebootTime;
                    telemetryData.DiffThrotLimit = pair.DiffThrottleLimit;
                    telemetryData.ResyncThrotLimit = pair.ResyncThrottleLimit;
                    telemetryData.VolUsageLimit = PSCoreSettings.Default.ProcessServerSettings_Rcm_DefaultCumulativeThrottleLimit;
                    telemetryData.CacheFldSize = totCacheFolSizeInBytes;
                    telemetryData.UsedVolSize = usedVolSizeInKB;
                    telemetryData.FreeVolSize = freeVolSizeInKB;
                    telemetryData.LastMoveFile = lastMoveFile;
                    telemetryData.LastMoveFileModTime = lastMoveFileModTime;
                    telemetryData.LastMoveFileProcTime = lastMoveFileProcTime;

                    TaskUtils.RunAndIgnoreException(() =>
                    {
                        telemetryData.ResyncFldSize += FSUtils.GetFilesSize(
                                        pair.ResyncFolder,
                                        SearchOption.TopDirectoryOnly,
                                        SYNC_FILES_PATTERN);
                    }, Tracers.Monitoring);

                    Tracers.Monitoring.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "MonitorTelemetry : Total resync files size is {0}" +
                        " for pair {1}, host {2}, dir {3}",
                        telemetryData.ResyncFldSize,
                        pair.DeviceId,
                        pair.HostId,
                        pair.ResyncFolder);

                    var isDiffSync = (pair.ProtectionState == ProtectionState.DifferentialSync) ? 1 : 0;
                    var isResync = (pair.ProtectionState == ProtectionState.ResyncStep1) ? 1 : 0;

                    // PairState - By default all these values are 0's.
                    // 0-2, 4-6 - Taking default values(0's) from the pssettings as these are not applicable for RCMPS.
                    // 3 - Cumulative throttle happened then its value is 1
                    // 7 - Source to PS file upload stuck then its value is 1
                    // 8 - monitor.txt file exists then its value is 1
                    // 9 - If the pair is in Resync, then its value is 1.
                    // 10 - If the pair is in Diff Sync, then its value is 1.
                    telemetryData.PairState = string.Format("{0}{1}{2}{3}{4}{5}{6}{7}{8}{9}{10}",
                        isDiffSync, isResync, monitorTxtFileExists, srcToPSFileUploadStuck,
                        0, (int)pair.ThrottleResync, (int)pair.ThrottleDifferentials,
                        isCumulativeThrottled, (int)pair.SourceReplicationStatus,
                        (int)pair.TargetReplicationStatus, (int)pair.PairDeleted);

                    Tracers.Monitoring.TraceAdminLogV2Message(TraceEventType.Information,
                        "MonitorTelemetry : Pair State is {0}", telemetryData.PairState);

                    var tgtDiffsData = DiffFileUtils.ProcessDiffFolder(
                        pair.TargetDiffFolder, TGT_DIFF_FILES_PATTERN, Tracers.Monitoring);
                    telemetryData.TgtFldSize = tgtDiffsData.TotalPendingDiffs;
                    telemetryData.TgtNumOfFiles = (ulong)tgtDiffsData.DiffFilesCount;
                    telemetryData.TgtFirstFile = tgtDiffsData.DiffFirstFileName;
                    telemetryData.TgtFF_ModTime = tgtDiffsData.DiffFirstFileTimeStamp;
                    telemetryData.TgtLastFile = tgtDiffsData.DiffLastFileName;
                    telemetryData.TgtLF_ModTime = tgtDiffsData.DiffLastFileTimeStamp;
                    telemetryData.TgtNumOfTagFile = tgtDiffsData.DiffTagFilesCount;
                    telemetryData.TgtFirstTagFileName = tgtDiffsData.DiffFirstTagFileName;
                    telemetryData.TgtFT_ModTime = tgtDiffsData.DiffFirstTagFileTimeStamp;
                    telemetryData.TgtLastTagFileName = tgtDiffsData.DiffLastTagFileName;
                    telemetryData.TgtLT_ModTime = tgtDiffsData.DiffLastTagFileTimeStamp;
                }, Tracers.Monitoring);

                Microsoft.Azure.SiteRecovery.ProcessServer.Core.
                    Tracing.Tracers.PSV2Telemetry.TracePsTelemetryV2Message(
                    TraceEventType.Information, telemetryData);

                Tracers.Monitoring.TraceAdminLogV2Message(TraceEventType.Information,
                    "MonitorTelemetry : Successfully written telemetry data to json file" +
                    " for the pair with HostId : {0}", pair.HostId);
            }

            return Task.CompletedTask;
        }
    }
}
