using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.ServiceProcess;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi
{
    public class PSTaskFactory
    {
        #region Constants

        private const string PS_KEY_FILE = "ps.key";
        private const string PS_DH_FILE = "ps.dh";
        private const string PS_FP_FILE = "ps.fingerprint";
        private const string PS_CERT_FILE = "ps.crt";

        private const string PS_CERT_BKP_FILES_PATTERN = "ps.crt.bak*";
        private const string PS_DH_BKP_FILES_PATTERN = "ps.dh.bak*";
        private const string PS_FP_BKP_FILES_PATTERN = "ps.fingerprint.bak*";
        private const string PS_KEY_BKP_FILES_PATTERN = "ps.key.bak*";

        private const string PERF_TXT_FILENAME = @"perf.txt";
        private const string PERF_PS_TXT_FILENAME = @"perf_ps.txt";

        #endregion Constants

        private readonly Lazy<IProcessServerCSApiStubs> m_stubs;

        public PSTaskFactory()
        {
            m_stubs = new Lazy<IProcessServerCSApiStubs>(
                () => CSApiFactory.GetProcessServerCSApiStubs());
        }

        public async Task DeleteReplicationAsync(
            ProcessServerSettings psSettings,
            CancellationToken ct)
        {
            const int SRC_DEL_DONE = 30;

            var pairs = psSettings.Pairs;

            foreach (var pair in pairs)
            {
                try
                {
                    ct.ThrowIfCancellationRequested();

                    var replicationStatus = pair.ReplicationStatus;

                    if (replicationStatus == SRC_DEL_DONE)
                    {
                        var pairId = pair.PairId.ToString();

                        Tracers.TaskMgr.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "DeleteReplication : Deleting replication for the PairId : {0}, Source HostId : {1}, Target HostId : {2}," +
                            " Source DeviceName: {3}, Target DeviceName: {4}",
                            pairId, pair.HostId, pair.TargetHostId,
                            pair.DeviceId, pair.TargetDeviceId);

                        var stopReplicationInput = new StopReplicationInput(
                            sourceHostId: pair.HostId,
                            sourceDeviceName: pair.DeviceId,
                            destinationHostId: pair.TargetHostId,
                            destinationDeviceName: pair.TargetDeviceId);

                        await m_stubs.Value.StopReplicationAtPSAsync(stopReplicationInput, ct).
                            ConfigureAwait(false);

                        Tracers.TaskMgr.DebugAdminLogV2Message("DeleteReplication : Deleting folders for pair : {0}", pairId);
                        await DeleteReplicationFoldersAsync(pair, ct)
                            .ConfigureAwait(false);

                        await m_stubs.Value.UpdateReplicationStatusForDeleteReplicationAsync(pairId).
                            ConfigureAwait(false);

                        Tracers.TaskMgr.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "DeleteReplication : Successfully deleted replication for pair {0} on host {1}",
                            pair.DeviceId,
                            pair.HostId);
                    }
                }
                catch (Exception ex)
                {
                    Tracers.TaskMgr.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "DeleteReplication : Failed to delete replication for pair {0} on host {1} with error {2}{3}",
                        pair.DeviceId,
                        pair.HostId,
                        Environment.NewLine,
                        ex);
                }
            }
        }

        private static async Task DeleteFilesMatchingPatternAsync(
            string dir,
            string pattern,
            SearchOption option,
            CancellationToken ct)
        {
            if (string.IsNullOrEmpty(dir))
                throw new ArgumentNullException(nameof(dir));

            if (!Directory.Exists(dir))
            {
                return;
            }

            Tracers.TaskMgr.TraceAdminLogV2Message(
                TraceEventType.Information,
                "DeleteReplication : Deleting {0} files under folder : {1}",
                pattern,
                dir);

            var filesToDelete = FSUtils.GetFileList(
                dir: dir,
                pattern: pattern,
                option: option);

            await FSUtils.DeleteFilesAsync(
                filesToDelete: filesToDelete,
                maxRetryCount: Settings.Default.TaskMgr_DeleteFileMaxRetryCnt,
                retryInterval: Settings.Default.TaskMgr_DeleteFileRetryInterval,
                maxErrorCountToLog: Settings.Default.TaskMgr_MaxLimitForRateControllingFileDelExLogging,
                traceSource: Tracers.TaskMgr,
                ct: ct).ConfigureAwait(false);
        }

        private static async Task DeleteReplicationFoldersAsync(
            IProcessServerPairSettings pair,
            CancellationToken ct)
        {
            const string ALL_FILES_PATTERN = "*.*";
            const string MAP_FILES_PATTERN = "*.Map";
            const string DAT_FILES_PATTERN = "*.dat*";

            ct.ThrowIfCancellationRequested();

            try
            {
                // Deleting all files from top level source diffs directory.
                await DeleteFilesMatchingPatternAsync(
                    dir: pair.SourceDiffFolder,
                    pattern: ALL_FILES_PATTERN,
                    option: SearchOption.TopDirectoryOnly,
                    ct: ct).ConfigureAwait(false);
            }
            catch (Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "DeleteReplication : Failed to delete files from source diffs folder {0} with error {1}{2}",
                    pair.SourceDiffFolder,
                    Environment.NewLine,
                    ex);
            }

            try
            {
                // Deleting all *.dat* files from top level target diffs folder
                await DeleteFilesMatchingPatternAsync(
                    dir: pair.TargetDiffFolder,
                    pattern: DAT_FILES_PATTERN,
                    option: SearchOption.TopDirectoryOnly,
                    ct: ct).ConfigureAwait(false);
            }
            catch (Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "DeleteReplication : Failed to delete files from target diffs folder {0} with error {1}{2}",
                    pair.TargetDiffFolder,
                    Environment.NewLine,
                    ex);
            }

            try
            {
                // Deleting all *.map files from top level resync folder
                await DeleteFilesMatchingPatternAsync(
                    dir: pair.ResyncFolder,
                    pattern: MAP_FILES_PATTERN,
                    option: SearchOption.TopDirectoryOnly,
                    ct: ct).ConfigureAwait(false);
            }
            catch (Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "DeleteReplication : Failed to delete map files from resync diffs folder {0} with error {1}{2}",
                    pair.ResyncFolder,
                    Environment.NewLine,
                    ex);
            }

            try
            {
                // Deleting all *.dat* files from top level resync folder
                await DeleteFilesMatchingPatternAsync(
                    dir: pair.ResyncFolder,
                    pattern: DAT_FILES_PATTERN,
                    option: SearchOption.TopDirectoryOnly,
                    ct: ct).ConfigureAwait(false);
            }
            catch (Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "DeleteReplication : Failed to delete dat files from target diffs folder {0} with error {1}{2}",
                    pair.ResyncFolder,
                    Environment.NewLine,
                    ex);
            }

            string[] dirsToDelete = new string[] {
                pair.AzurePendingUploadRecoverableFolder, pair.AzurePendingUploadNonRecoverableFolder };

            // TODO: Add parallelism to process quickly
            foreach (var currDir in dirsToDelete)
            {
                if (string.IsNullOrWhiteSpace(currDir))
                    continue;

                try
                {
                    await DirectoryUtils.DeleteDirectoryAsync(
                            folderPath: currDir,
                            recursive: true,
                            useLongPath: true,
                            maxRetryCount: Settings.Default.TaskMgr_RecDelFolderMaxRetryCnt,
                            retryInterval: Settings.Default.TaskMgr_RecDelFolderRetryInterval,
                            traceSource: Tracers.TaskMgr,
                            ct: ct).ConfigureAwait(false);
                }
                catch (Exception ex)
                {
                    Tracers.TaskMgr.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "DeleteReplication : Failed to delete directory {0} with error {1}{2}",
                        currDir,
                        Environment.NewLine,
                        ex);
                }
            }
        }

        public async Task RegisterProcessServerAsync(CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            try
            {
                var hostInfo = SystemUtils.GetHostInfo();

                var networkInfo = SystemUtils.GetNetworkInfo();

                await m_stubs.Value.RegisterPSAsync(hostInfo, networkInfo, ct).ConfigureAwait(false);

                Tracers.TaskMgr.TraceAdminLogV2Message(TraceEventType.Information,
                    "RegisterProcessServer : ProcessServer Registration succeeded");
            }
            catch (Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "RegisterProcessServer : ProcessServer Registration failed with error {0}{1}",
                    Environment.NewLine,
                    ex);
            }
        }

        public Task MonitorLogsAsync(ProcessServerSettings psSettings, CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            var logRotUpdateDBTasks = new List<Task>();

            if (psSettings.LogRotationState == LogRotationState.Enabled)
            {
                var logRotationDict = new Dictionary<string, LogRotationInput>();

                // Get the log files configured for rotation as per the settings in CS DB
                logRotationDict = GetLogFilesConfiguredForRotation(psSettings, ct);

                var logsToRotate = new Dictionary<string, int>();
                foreach (var log in logRotationDict)
                {
                    try
                    {
                        var fullLogName = log.Key;
                        var logRotSettings = log.Value;
                        var logName = logRotSettings.LogName;
                        var logRotPolicyType = logRotSettings.PolicyType;

                        // Rotate the log file based on the policy type.
                        if (logRotPolicyType == LogRotPolicyType.TimeLimitLogRotPolicy ||
                            logRotPolicyType == LogRotPolicyType.TimeAndSizeLimitLogRotPolicy)
                        {
                            // If logTimeLimit is set and if present time > start time,
                            // then mark the file for rotation and update the logRotationList table
                            if (logRotSettings.StartEpochTime != 0 &&
                                logRotSettings.StartEpochTime < logRotSettings.CurrentEpochTime)
                            {
                                logsToRotate[fullLogName] = logRotSettings.LogTimeLimit;
                            }
                        }

                        // Default log rotation policy type is SizeBasedLogRotation.
                        // User doesn't have an option to modify the logRotation policy type and
                        // manual intervention is needed to set it to TimeBased/Time+SpaceBased in CS DB
                        if (logRotPolicyType == LogRotPolicyType.SizeLimitLogRotPolicy ||
                            logRotPolicyType == LogRotPolicyType.TimeAndSizeLimitLogRotPolicy)
                        {
                            var maxLogSizeInMB = logRotSettings.LogSizeLimit;
                            var logFileInfo = new FileInfo(fullLogName);

                            // Mark the current log for rotation if log size limit is set
                            // and log size is greater than max log size limit
                            if (logFileInfo.Exists && logFileInfo.Length > 0)
                            {
                                var logSizeInMB = Math.Round(logFileInfo.Length / Math.Pow(1024, 2), 2);
                                Tracers.TaskMgr.DebugAdminLogV2Message(
                                    "MonitorLogs : Log {0} size : {1} MB, Max Log Size : {2}",
                                    fullLogName, logSizeInMB, maxLogSizeInMB);

                                if (logSizeInMB > maxLogSizeInMB)
                                {
                                    // Log Rotation type is set to size based here,
                                    // this will also overwrite the existing set policy type.
                                    logsToRotate[fullLogName] = 0;
                                }
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        Tracers.TaskMgr.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "MonitorLogs : Failed to configure rotation for log : {0} with error {1}{2}",
                            log.Key,
                            Environment.NewLine,
                            ex);
                    }
                }

                foreach (var currLog in logsToRotate)
                {
                    ct.ThrowIfCancellationRequested();

                    TaskUtils.RunAndIgnoreException(() =>
                    {
                        var backupLogsCount = psSettings.LogFilesBackupCounter;

                        // TODO: MonitorLogs event performs log rotation of CS logs also in Perl.
                        // Porting the same behaviour for now as there is no flag in logRotationList
                        // table to differentiate CS and PS logs. Check with CS team for contract changes
                        // to find out a way to filter PS logs and incorporate the same in this task.
                        if (currLog.Key.Equals(Path.Combine(
                                PSInstallationInfo.Default.GetVarFolderPath(), "asrapi_reporting.log")))
                        {
                            backupLogsCount = 15;
                        }

                        var logRotStatus = RotateAndCompressLog(currLog.Key, backupLogsCount, ct);

                        // Call CSAPI to update the Log Rotation Start Time only
                        // if LogTimeLimit(Time based log rotation) is set
                        // and log rotation is performed successfully
                        if (logRotStatus && currLog.Value != 0)
                        {
                            logRotUpdateDBTasks.Add(TaskUtils.RunTaskForAsyncOperation(
                            traceSource: Tracers.TaskMgr,
                                name: nameof(MonitorLogsAsync),
                                ct: ct,
                                operation: async (taskCancelToken) =>
                                await m_stubs.Value.UpdateLogRotationInfoAsync(
                                    currLog.Key, currLog.Value, taskCancelToken).ConfigureAwait(false)));
                        }
                    }, Tracers.TaskMgr);
                }
            }

            return Task.WhenAll(logRotUpdateDBTasks);
        }

        private static Dictionary<string, LogRotationInput> GetLogFilesConfiguredForRotation(
            ProcessServerSettings psSettings, CancellationToken ct)
        {
            const string HOST_LOG = "host.log";
            const string PERF_LOG = "perf.log";

            var logRotationDict = new Dictionary<string, LogRotationInput>();

            foreach (var log in psSettings.Logs)
            {
                ct.ThrowIfCancellationRequested();

                TaskUtils.RunAndIgnoreException(() =>
                {
                    var logName = log.LogName;

                    LogRotPolicyType policyType;
                    // Time based policy is set if size limit is not set for the log and viceversa.
                    // Ported this logic as is from the perl script.
                    if (log.LogSizeLimit == 0)
                    {
                        policyType = LogRotPolicyType.TimeLimitLogRotPolicy;
                    }
                    else if (log.LogTimeLimit == 0)
                    {
                        policyType = LogRotPolicyType.SizeLimitLogRotPolicy;
                    }
                    else
                    {
                        policyType = LogRotPolicyType.TimeAndSizeLimitLogRotPolicy;
                    }

                    var logPath = log.LogPathWindows;

                    // Construct all the possible file paths for the current log item
                    if (logName.Equals(HOST_LOG, StringComparison.OrdinalIgnoreCase))
                    {
                        foreach (var host in psSettings.Hosts)
                        {
                            ct.ThrowIfCancellationRequested();

                            TaskUtils.RunAndIgnoreException(() =>
                            {
                                var hostLogFile = FSUtils.CanonicalizePath(Path.Combine(logPath, host.HostId + ".log"));

                                if (File.Exists(hostLogFile))
                                {
                                    logRotationDict[hostLogFile] = PopulateLogRotationInput(policyType, log);
                                }
                            }, Tracers.TaskMgr);
                        }
                    }
                    else if (logName.Equals(PERF_LOG, StringComparison.OrdinalIgnoreCase))
                    {
                        foreach (var pair in psSettings.Pairs)
                        {
                            ct.ThrowIfCancellationRequested();

                            TaskUtils.RunAndIgnoreException(() =>
                            {
                                var sourcePerfLogFile = Path.Combine(pair.PerfFolder, PERF_LOG);
                                if (File.Exists(sourcePerfLogFile))
                                {
                                    logRotationDict[sourcePerfLogFile] = PopulateLogRotationInput(policyType, log);
                                }
                            }, Tracers.TaskMgr);
                        }
                    }
                    else
                    {
                        var logFileName = FSUtils.CanonicalizePath(Path.Combine(logPath, logName));

                        if (File.Exists(logFileName))
                        {
                            logRotationDict[logFileName] = PopulateLogRotationInput(policyType, log);
                        }
                    }
                }, Tracers.TaskMgr);
            }

            return logRotationDict;
        }

        private static LogRotationInput PopulateLogRotationInput(
            LogRotPolicyType policyType,
            ILogRotationSettings logSettings)
        {
            var logRotationInput = new LogRotationInput();

            logRotationInput = new LogRotationInput
            {
                LogName = logSettings.LogName,
                StartEpochTime = logSettings.StartEpochTime,
                PolicyType = policyType,
                LogSizeLimit = logSettings.LogSizeLimit,
                LogTimeLimit = logSettings.LogTimeLimit,
                CurrentEpochTime = logSettings.CurrentEpochTime
            };

            return logRotationInput;
        }

        /// <summary>
        /// This function performs log rotation of given log.
        /// Deletes the oldest backup log, if the number of backup logs > backup logs count
        /// and then rotates the logs and stores them in compressed format.
        /// </summary>
        /// <param name="logName"></param>
        /// <param name="backupLogsCnt"></param>
        private static bool RotateAndCompressLog(string logName, int backupLogsCnt, CancellationToken ct)
        {
            const string GZIP_EXTENSION = ".gz";

            try
            {
                ct.ThrowIfCancellationRequested();
                Tracers.TaskMgr.TraceAdminLogV2Message(
                            TraceEventType.Information, "MonitorLogs : Attempting to rotate {0} log", logName);

                if (backupLogsCnt <= 0)
                {
                    throw new Exception(
                        String.Format("MonitorLogs : Log {0} is configured with invalid backup logs counter {1}",
                        logName, backupLogsCnt));
                }

                // Remove the oldest backup file, if it exists
                var oldBackupLogFile = logName + "." + backupLogsCnt.ToString() + GZIP_EXTENSION;
                if (File.Exists(oldBackupLogFile))
                {
                    if (!FSUtils.DeleteFile(oldBackupLogFile))
                    {
                        // Returning in case of failure, so that this operation will be tried in next iteration.
                        return false;
                    }
                }

                // Bump down previous backup files.
                for (int i = backupLogsCnt - 1; i > 0; i--)
                {
                    var currBackupFile = logName + "." + i.ToString() + GZIP_EXTENSION;
                    if (File.Exists(currBackupFile))
                    {
                        var newBkpFile = logName + "." + (i + 1).ToString() + GZIP_EXTENSION;
                        // Move file i to file i + 1.
                        if (!FSUtils.MoveFile(currBackupFile, newBkpFile))
                        {
                            Tracers.TaskMgr.TraceAdminLogV2Message(
                                TraceEventType.Error,
                                "MonitorLogs : Failed to compress {0} log to {1}",
                                logName, newBkpFile);

                            return false;
                        }
                    }
                }

                var firstBackupLog = logName + ".1" + GZIP_EXTENSION;
                // Compresses the original log file to *.1.gz
                if (!FSUtils.CompressFile(logName, firstBackupLog))
                {
                    Tracers.TaskMgr.TraceAdminLogV2Message(
                                TraceEventType.Error,
                                "MonitorLogs : Failed to compress {0} log to {1}",
                                logName, firstBackupLog);

                    return false;
                }
                else if (!FSUtils.DeleteFile(logName))
                {
                    return false;
                }

                Tracers.TaskMgr.TraceAdminLogV2Message(
                            TraceEventType.Information, "MonitorLogs : Successfully rotated {0} log", logName);

                return true;
            }
            catch (Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "MonitorLogs : Failed to rotate log {0} with error {1}{2}",
                    logName, Environment.NewLine, ex);

                return false;
            }
        }

        public async Task AutoResyncOnResyncSetAsync(
            ProcessServerSettings psSettings,
            CancellationToken ct)
        {
            bool doPause = false;

            var pairs = psSettings.Pairs;

            (var batchResyncDict, var resyncPairsDict) = GetBatchResyncAndResyncStep1PairsInfo(pairs, ct);

            foreach (var pair in pairs)
            {
                try
                {
                    // By default autoresyncstarttime is set to 1800 in CSDB.
                    // It's set to 0, if PS is unable to fetch the setting.
                    // Ignore the pair, if auto resync start time is 0
                    if (pair.AutoResyncStartTime == 0)
                    {
                        continue;
                    }

                    var sourceHostId = pair.HostId;
                    var sourceDeviceName = pair.DeviceId;
                    var scenarioId = pair.ScenarioId;
                    var pairId = pair.PairId.ToString();

                    // Compute auto resync start/stop window by adding auto resync start/stop hours and minutes
                    var autoResyncStartWindow =
                        new TimeSpan(pair.AutoResyncStartHours, pair.AutoResyncStartMinutes, 0);
                    var autoResyncStopWindow
                        = new TimeSpan(pair.AutoResyncStopHours, pair.AutoResyncStopMinutes, 0);

                    var currentLocalTime = new TimeSpan(pair.CurrentHour, pair.CurrentMinutes, 0);

                    // Ignoring the pairs whose auto resync stop hours/minutes is less than auto resync start hours/minutes.
                    // This will not happen unless CS DB entries are modified manually.
                    if (autoResyncStopWindow < autoResyncStartWindow)
                    {
                        Tracers.TaskMgr.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "AutoResyncOnResyncSet : Auto resync stop window {0} should be greater than auto resync start window : {1}",
                            autoResyncStopWindow,
                            autoResyncStartWindow);

                        continue;
                    }

                    // Process further if below conditions are satisfied,
                    // 1. AutoResyncStartType is not set(default setting)
                    // 2. Current time falls in between the auto resync start and auto resync stop window
                    // 3. The difference between CS current timestamp and resync set timestamp is greater than Auto Resync Start Time.
                    //    To delay the auto resync for the configured interval(default 30mins) after the pair is marked for resync
                    // 4. Resync set timestamp is set for the pair
                    // 5. Pair is marked for resync required
                    // 6. IsVisible is not set. By default it will be 0 for disk level replication.
                    // 7. Replication Status is 0(default value) for the pair
                    if (pair.AutoResyncStartType == AutoResyncStartType.NotSet &&
                        currentLocalTime >= autoResyncStartWindow &&
                        currentLocalTime <= autoResyncStopWindow &&
                        (pair.AutoResyncStartTime <= pair.ControlPlaneTimestamp - pair.ResyncSetCxtimestamp) &&
                        pair.ResyncSetCxtimestamp != 0 && pair.IsResyncRequired == 0 &&
                        pair.IsVisible == 0 && pair.ReplicationStatus == 0)
                    {
                        Tracers.TaskMgr.TraceAdminLogV2Message(
                            TraceEventType.Verbose,
                            "AutoResyncOnResyncSet : AutoResyncStartTime : {0}, CurrentTimeStampOnCS : {1}," +
                            " ResyncSetCXTimeStamp : {2}, IsResync : {3}, IsVisible : {4}," +
                            " ReplicationStatus : {5} are properties for SourceDeviceId - {6} on SourceHostId : {7}",
                            pair.AutoResyncStartTime,
                            pair.ControlPlaneTimestamp, pair.ResyncSetCxtimestamp, pair.IsResyncRequired,
                            pair.IsVisible, pair.ReplicationStatus, sourceDeviceName, sourceHostId);

                        Tracers.TaskMgr.DebugAdminLogV2Message("AutoResyncOnResyncSet : ScenarioId is {0}", scenarioId);
                        if (scenarioId > 0)
                        {
                            // Get batch resync value
                            if (!batchResyncDict.TryGetValue(scenarioId, out int batchResyncValue))
                            {
                                batchResyncValue = 0;
                                batchResyncDict[scenarioId] = 0;
                            }

                            // Get number of pairs in resync
                            if (!resyncPairsDict.TryGetValue(scenarioId, out int numOfResyncPairs))
                            {
                                numOfResyncPairs = 0;
                                resyncPairsDict[scenarioId] = 0;
                            }

                            if (batchResyncValue != 0)
                            {
                                // If the number of pairs in resync are less than batchresyncvalue(default 3),
                                // update the reseync pairs data otherwise queue the pairs to delay the auto resync process.
                                if (numOfResyncPairs < batchResyncValue)
                                {
                                    doPause = true;
                                    resyncPairsDict[scenarioId] = resyncPairsDict[scenarioId] + 1;
                                }
                                else
                                {
                                    Tracers.TaskMgr.TraceAdminLogV2Message(
                                        TraceEventType.Information, "AutoResyncOnResyncSet : Queued pairId : {0}", pairId);

                                    // Delay Auto Resync for pairs whose resync Order is not 1. Update executionState to 4(Delay Resync)
                                    await m_stubs.Value.UpdateExecutionStateAsync(
                                        pairId, ExecutionState.Waiting, ct).ConfigureAwait(false);
                                }
                            }
                            else
                            {
                                // Set pause, if batch resync is not configured
                                doPause = true;
                            }
                        }
                        else
                        {
                            // This case is not expected as scenarioId will always be set in V2A scenario. Ported it from legacy perl code.
                            // TODO: Revisit to remove this?
                            doPause = true;
                        }

                        Tracers.TaskMgr.DebugAdminLogV2Message("AutoResyncOnResyncSet : Pause Pair : {0}", doPause);
                        if (doPause)
                        {
                            await PauseReplicationAsync(
                                pairId, sourceHostId, sourceDeviceName, ct).ConfigureAwait(false);
                        }
                    }
                }
                catch (Exception ex)
                {
                    Tracers.TaskMgr.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "AutoResyncOnResyncSet : Failed to set auto resync for pair {0} on host {1} with error {2}{3}",
                        pair.DeviceId,
                        pair.HostId,
                        Environment.NewLine,
                        ex);
                }
            }
        }

        private (Dictionary<int, int>, Dictionary<int, int>) GetBatchResyncAndResyncStep1PairsInfo(
            IEnumerable<IProcessServerPairSettings> pairs,
            CancellationToken ct)
        {
            var resyncPairsDict = new Dictionary<int, int>();
            var batchResyncDict = new Dictionary<int, int>();

            foreach (var pair in pairs)
            {
                TaskUtils.RunAndIgnoreException(() =>
                {
                    var scenarioId = pair.ScenarioId;

                    // TODO : Batch resync value is 3 by default in CS DB, should we use constant
                    // instead of using below logic to get the batch resync value?
                    // It may cause issue if batch resync value is modified in CS.

                    // ResyncOrder will be 1 for the first 3(default batch resync count) pairs
                    // and it will be incremented by 1 for the next set of batch resync pairs
                    if (pair.ResyncOrder == 1)
                    {
                        // Update the pair count, if the scenarioId already exists
                        if (batchResyncDict.ContainsKey(scenarioId))
                        {
                            batchResyncDict[scenarioId] = batchResyncDict[scenarioId] + 1;
                        }
                        else
                        {
                            batchResyncDict.Add(scenarioId, 1);
                        }
                    }

                    // Get the pairs which are in resync step-1
                    if (pair.ProtectionState == ProtectionState.ResyncStep1)
                    {
                        // Update the pair count, if the scenarioId already exists
                        if (resyncPairsDict.ContainsKey(scenarioId))
                        {
                            resyncPairsDict[scenarioId] = resyncPairsDict[scenarioId] + 1;
                        }
                        else
                        {
                            resyncPairsDict.Add(scenarioId, 1);
                        }
                    }
                }, Tracers.TaskMgr);
            }

            return (batchResyncDict, resyncPairsDict);
        }

        private async Task PauseReplicationAsync(string pairId, string sourceHostId, string sourceDeviceName, CancellationToken ct)
        {
            var rollbackStatus =
                await GetRollbackStatusAsync(sourceHostId, ct).ConfigureAwait(false);

            if (rollbackStatus < 0)
            {
                // Stop processing further, if rollback status couldn't be fetched.
                Tracers.TaskMgr.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "PauseReplicationAsync : Returning from here as API Result is {0}" +
                        " for SourceHostId : {1} - SourceDeviceId : {2}",
                        rollbackStatus,
                        sourceHostId,
                        sourceDeviceName);
            }
            else if (rollbackStatus == 0)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "PauseReplicationAsync : Pausing the pair." +
                    " PairId : {0}, Source HostId {1} - SourceDeviceId {2}",
                    pairId,
                    sourceHostId,
                    sourceDeviceName);

                // Pause the pair if it is not in rollback state.
                await m_stubs.Value.PauseReplicationAsync(pairId, ct).ConfigureAwait(false);

                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information, "PauseReplicationAsync : Successfully paused the pair." +
                    " PairId: {0}, Source HostId : {1}, SourceDeviceId : {2}",
                    pairId,
                    sourceHostId,
                    sourceDeviceName);
            }
            else
            {
                // Ignore the pairs which are in rollback(failback) state.
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "PauseReplicationAsync : Rollback is in progress fo the pair." +
                    " PairId : {0}, Source HostId : {1} - SourceDeviceId : {2}",
                    pairId,
                    sourceHostId,
                    sourceDeviceName);
            }
        }

        private async Task<int> GetRollbackStatusAsync(string sourceHostId, CancellationToken ct)
        {
            int rollbackStatus = -1;

            try
            {
                // Check if the pair is under rollback progress(executionStatus=116)
                // or rollback completed(executionStatus=130) or not.
                var apiResult = await m_stubs.Value.GetRollbackStatusAsync(
                    sourceHostId, ct).ConfigureAwait(false);

                if (!string.IsNullOrWhiteSpace(apiResult) &&
                    int.TryParse(apiResult, out int status))
                {
                    rollbackStatus = status;
                }
            }
            catch (Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failed to get rollback status for the sourceHostId : {0} with exception {1}{2}",
                    sourceHostId,
                    Environment.NewLine,
                    ex);
            }

            Tracers.TaskMgr.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                "Rollback status : {0} for sourceHostId : {1}",
                rollbackStatus, sourceHostId);

            return rollbackStatus;
        }

        public async Task RenewPSCertificatesAsync(
            ProcessServerSettings psSettings, CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            Tracers.TaskMgr.TraceAdminLogV2Message(
                TraceEventType.Information,
                "RenewPSCertificates : Process Server Renew" +
                " Certificates jobs count : {0}",
                psSettings.PolicySettings.Count());

            if (psSettings.PolicySettings.Count() == 0)
            {
                return;
            }

            // Setting default renew cert status
            RenewCertsStatus renewCertsStatus = GetDefaultRenewCertsStatus();

            var renewCertsUpdateDBTasks = new List<Task>();
            foreach (var policy in psSettings.PolicySettings)
            {
                try
                {
                    ct.ThrowIfCancellationRequested();

                    var hostId = policy.HostId;

                    int retryCntr = 0;
                    while (retryCntr < Settings.Default.TaskMgr_CertFileMaxRetryCnt)
                    {
                        ct.ThrowIfCancellationRequested();

                        try
                        {
                            renewCertsStatus =
                                await RegeneratePSCertsAsync(ct).ConfigureAwait(false);

                            if (renewCertsStatus.Status)
                            {
                                Tracers.TaskMgr.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "RenewPSCertificates : Successfully renewed" +
                                    " Process Server Certificates for PolicyId : {0}",
                                    policy.PolicyId);

                                break;
                            }
                            else
                            {
                                Tracers.TaskMgr.TraceAdminLogV2Message(
                                    TraceEventType.Verbose,
                                    "RenewPSCertificates : Failed to renew" +
                                    " Process Server Certificates {0} times", retryCntr + 1);
                            }

                            retryCntr++;

                            await Task.Delay(
                                Settings.Default.TaskMgr_CertFileRetryInterval,
                                ct).ConfigureAwait(false);
                        }
                        catch (Exception ex)
                        {
                            Tracers.TaskMgr.TraceAdminLogV2Message(
                                TraceEventType.Verbose,
                                "RenewPSCertificates : Failed to renew Process Server" +
                                " Certificates for PolicyId : {0}, {1} times with error {2}{3}",
                                policy.PolicyId,
                                retryCntr,
                                Environment.NewLine,
                                ex);
                        }
                    }
                }
                catch (OperationCanceledException) when (ct.IsCancellationRequested)
                {
                    Tracers.TaskMgr.DebugAdminLogV2Message("Cancelling {0} task",
                            nameof(RenewPSCertificatesAsync));
                    throw;
                }
                catch (Exception ex)
                {
                    Tracers.TaskMgr.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "RenewPSCertificates : Failed to renew Process Server" +
                        " Certificates for PolicyId : {0} with error {1}{2}",
                        policy.PolicyId,
                        Environment.NewLine,
                        ex);
                }

                // Update policy state with the corresponding state and log data.
                TaskUtils.RunAndIgnoreException(() =>
                {
                    Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "RenewPSCertificates : Renew Process Server" +
                    " Certificates status : {0}", renewCertsStatus.Status);

                    var renewCertsPolicyInfo = new RenewCertsPolicyInfo
                    {
                        PolicyId = policy.PolicyId,
                        PolicyState = (renewCertsStatus.Status) ?
                        PolicyState.Success : PolicyState.Failed,
                        renewCertsStatus = (!renewCertsStatus.Status) ? renewCertsStatus : null
                    };

                    // Call CX API to update states in policy table.
                    renewCertsUpdateDBTasks.Add(
                        TaskUtils.RunTaskForAsyncOperation(
                            traceSource: Tracers.TaskMgr,
                            name: nameof(RenewPSCertificatesAsync),
                                ct: ct,
                                operation: async (taskCancelToken) =>
                                await m_stubs.Value.UpdatePolicyInfoForRenewCertsAsync(
                                    renewCertsPolicyInfo, taskCancelToken).ConfigureAwait(false)));
                }, Tracers.TaskMgr);
            }

            await Task.WhenAll(renewCertsUpdateDBTasks).ConfigureAwait(false);
        }

        internal static RenewCertsStatus GetDefaultRenewCertsStatus()
        {
            // Setting default renew cert status
            var renewCertsStatus = new RenewCertsStatus
            {
                ErrorDescription = "Success",
                ErrorCode = "0",
                PlaceHolders = string.Empty,
                LogMessage = string.Empty,
                Status = true
            };

            return renewCertsStatus;
        }

        /// <summary>
        /// This function takes care of PS Certs renewal
        /// </summary>
        /// <param name="ct">Cancellation token</param>
        /// <returns></returns>
        public static async Task<RenewCertsStatus> RegeneratePSCertsAsync(CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            string serviceName = Settings.Default.ServiceName_CXProcessServer;

            // Setting default renew cert status
            var renewCertsStatus = GetDefaultRenewCertsStatus();

            try
            {
                renewCertsStatus = StopService(serviceName, ct);

                if (!renewCertsStatus.Status)
                {
                    return renewCertsStatus;
                }

                // Delete the old key backup files.
                renewCertsStatus = await DeletePSBackupCertFilesAsync(
                    certsDir: Settings.Default.LegacyCS_FullPath_Private,
                    pattern: PS_KEY_BKP_FILES_PATTERN,
                    ct: ct).ConfigureAwait(false);

                if (!renewCertsStatus.Status)
                {
                    return renewCertsStatus;
                }

                // Delete the old dh backup files.
                renewCertsStatus = await DeletePSBackupCertFilesAsync(
                    certsDir: Settings.Default.LegacyCS_FullPath_Private,
                    pattern: PS_DH_BKP_FILES_PATTERN,
                    ct: ct).ConfigureAwait(false);

                if (!renewCertsStatus.Status)
                {
                    return renewCertsStatus;
                }

                // Delete the old fingerprint backup files.
                renewCertsStatus = await DeletePSBackupCertFilesAsync(
                    certsDir: Settings.Default.LegacyCS_FullPath_Fingerprints,
                    pattern: PS_FP_BKP_FILES_PATTERN,
                    ct: ct).ConfigureAwait(false);

                if (!renewCertsStatus.Status)
                {
                    return renewCertsStatus;
                }

                // Delete the old certificate backup files.
                renewCertsStatus = await DeletePSBackupCertFilesAsync(
                    certsDir: Settings.Default.LegacyCS_FullPath_Certs,
                    pattern: PS_CERT_BKP_FILES_PATTERN,
                    ct: ct).ConfigureAwait(false);

                if (!renewCertsStatus.Status)
                {
                    return renewCertsStatus;
                }

                await Task.Delay(
                    Settings.Default.TaskMgr_CertFileRetryInterval, ct).ConfigureAwait(false);

                renewCertsStatus =
                    await GeneratePSCertificate(serviceName, ct).ConfigureAwait(false);

                if (!renewCertsStatus.Status)
                {
                    return renewCertsStatus;
                }
            }
            catch (OperationCanceledException) when (ct.IsCancellationRequested)
            {
                Tracers.TaskMgr.DebugAdminLogV2Message("Cancelling {0} task",
                        nameof(RegeneratePSCertsAsync));
                throw;
            }
            catch (Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "RenewPSCertificates : Failed to renew PS certificates with error {0}{1}",
                    Environment.NewLine,
                    ex);

                // Rename backup ps cert files to original files
                await UndoPSCertFiles(ct).ConfigureAwait(false);

                renewCertsStatus = StartOrRestartService(serviceName, ct);

                // Set error in case of generic exception
                var errorCode = "ECA0169";
                renewCertsStatus = new RenewCertsStatus
                {
                    ErrorCode = errorCode,
                    ErrorDescription = string.Format(
                        CultureInfo.InvariantCulture,
                        "SSL certificate renewal failed on server {0}",
                        SystemUtils.GetHostName()),
                    PlaceHolders = new Dictionary<string, string>()
                    {
                        ["PsName"] = SystemUtils.GetHostName()
                    },
                    LogMessage = string.Format(
                        CultureInfo.InvariantCulture,
                        "{0}: RegeneratePSCertsAsync: Exception in PS cert generation.",
                        errorCode),
                    Status = false
                };
            }

            return renewCertsStatus;
        }

        /// <summary>
        /// Generate PS Certicates
        /// </summary>
        /// <param name="serviceName">ServiceName to stop</param>
        /// <param name="ct">Cancellation Token</param>
        /// <returns>Cert Update Status</returns>
        private static async Task<RenewCertsStatus> GeneratePSCertificate(
            string serviceName,
            CancellationToken ct)
        {
            string genCertExePath = PSInstallationInfo.Default.GetGenCertExePath();

            string genCertArgs = $"-n ps --dh";

            int returnVal = ProcessUtils.RunProcess(
                exePath: genCertExePath,
                args: genCertArgs,
                stdOut: out string gencertOutput,
                stdErr: out string gencertError,
                pid: out int gencertPid);

            Tracers.TaskMgr.TraceAdminLogV2Message(
                returnVal == 0 ? TraceEventType.Information : TraceEventType.Error,
                "RenewPSCertificates : {0} {1} returned exit code : {2}{3}PID : {4}{5}Output : {6}{7}Error : {8}",
                genCertExePath, genCertArgs, returnVal, Environment.NewLine,
                gencertPid, Environment.NewLine,
                gencertOutput, Environment.NewLine,
                gencertError);

            var renewCertsStatus = GetDefaultRenewCertsStatus();

            // PS certificate regeneration failed.
            if (returnVal != 0)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error, "RenewPSCertificates : Failed to regenerate PS certificate");

                // undo ps cert files
                await UndoPSCertFiles(ct).ConfigureAwait(false);

                renewCertsStatus = StartOrRestartService(serviceName, ct);

                // ps cert gen error
                var errorCode = "ECA0169";
                renewCertsStatus = new RenewCertsStatus
                {
                    ErrorCode = errorCode,
                    ErrorDescription = string.Format(
                        CultureInfo.InvariantCulture,
                        "SSL certificate renewal failed on server {0}",
                        SystemUtils.GetHostName()),
                    PlaceHolders = new Dictionary<string, string>()
                    {
                        ["PsName"] = SystemUtils.GetHostName()
                    },
                    LogMessage = string.Format(
                        CultureInfo.InvariantCulture,
                        "{0}: GeneratePSCertificate: Could not generate PS certificate and Status is: {1}",
                        errorCode,
                        returnVal),
                    Status = false
                };
            }
            else
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "RenewPSCertificates : Successfully regenerated PS certificate");

                renewCertsStatus =
                    await ValidatePSCertFiles(serviceName, ct).ConfigureAwait(false);
            }

            return renewCertsStatus;
        }

        /// <summary>
        /// 1. Validates if,
        ///     a. Newly generated PS certificate files exist or not
        ///     b. Corresponding certificate backup files exist or not
        /// 2. Starts the stopped service back and sets the corresponding status.
        /// </summary>
        /// <param name="serviceName"></param>
        /// <param name="ct"></param>
        /// <returns></returns>
        private static async Task<RenewCertsStatus> ValidatePSCertFiles(
            string serviceName,
            CancellationToken ct)
        {
            var renewCertsStatus = GetDefaultRenewCertsStatus();

            var bkpCrtFilesCount = FSUtils.GetFileListIgnoreException(
                dir: Settings.Default.LegacyCS_FullPath_Certs,
                pattern: PS_CERT_BKP_FILES_PATTERN,
                option: SearchOption.TopDirectoryOnly).Count();

            var bkpFpFilesCount = FSUtils.GetFileListIgnoreException(
                dir: Settings.Default.LegacyCS_FullPath_Fingerprints,
                pattern: PS_FP_BKP_FILES_PATTERN,
                option: SearchOption.TopDirectoryOnly).Count();

            var bkpKeyFilesCount = FSUtils.GetFileListIgnoreException(
                dir: Settings.Default.LegacyCS_FullPath_Private,
                pattern: PS_KEY_BKP_FILES_PATTERN,
                option: SearchOption.TopDirectoryOnly).Count();

            var bkpDhFilesCount = FSUtils.GetFileListIgnoreException(
                dir: Settings.Default.LegacyCS_FullPath_Private,
                pattern: PS_DH_BKP_FILES_PATTERN,
                option: SearchOption.TopDirectoryOnly).Count();

            // Validate if,
            // 1. Newly generated PS certificate files exist or not
            // 2. Corresponding certificate backup files exist or not
            // If files exists, then proceed further.
            if (FSUtils.IsNonEmptyFile(PSInstallationInfo.Default.GetPSCertificateFilePath()) &&
                FSUtils.IsNonEmptyFile(PSInstallationInfo.Default.GetPSFingerprintFilePath()) &&
                FSUtils.IsNonEmptyFile(PSInstallationInfo.Default.GetPSKeyFilePath()) &&
                FSUtils.IsNonEmptyFile(PSInstallationInfo.Default.GetPSDhFilePath()) &&
                bkpCrtFilesCount != 0 &&
                bkpFpFilesCount != 0 &&
                bkpKeyFilesCount != 0 &&
                bkpDhFilesCount != 0)
            {
                renewCertsStatus = StartOrRestartService(serviceName, ct);
            }
            else
            {
                // TODO: Revisit later to replace below hardcoded values with PSInstallationInfo methods
                // and remove additional slashes from the paths in errorMessage and Placeholders
                var errorMessage = "PS cert generation passed, but one or some of the files: " +
                    "C:\\\\ProgramData\\\\Microsoft Azure Site Recovery\\\\certs\\\\ps.crt," +
                    "C:\\\\ProgramData\\\\Microsoft Azure Site Recovery\\\\private\\\\ps.dh," +
                    "C:\\\\ProgramData\\\\Microsoft Azure Site Recovery\\\\private\\\\ps.key," +
                    "C:\\\\ProgramData\\\\Microsoft Azure Site Recovery\\\\fingerprints\\\\ps.fingerprint " +
                    "not exists in folder(s)";

                Tracers.TaskMgr.TraceAdminLogV2Message(TraceEventType.Error, errorMessage);

                await UndoPSCertFiles(ct).ConfigureAwait(false);

                renewCertsStatus = StartOrRestartService(serviceName, ct);

                var errorCode = "ECA0170";
                renewCertsStatus = new RenewCertsStatus
                {
                    ErrorCode = errorCode,
                    ErrorDescription = string.Format(
                        CultureInfo.InvariantCulture,
                        "SSL certificate renewal failed on server {0}.",
                        SystemUtils.GetHostName()),
                    PlaceHolders = new Dictionary<string, string>()
                    {
                        ["PsName"] = SystemUtils.GetHostName(),
                        ["FolderName"] = Environment.ExpandEnvironmentVariables(
                            "C:\\\\ProgramData\\\\Microsoft Azure Site Recovery\\\\"),
                        ["FileNames"] = "private\\\\ps.key,certs\\\\ps.crt," +
                        "fingerprints\\\\ps.fingerprint,private\\\\ps.dh"
                    },
                    LogMessage = string.Format(
                        CultureInfo.InvariantCulture,
                        "{0}: ValidatePSCertFiles: {1}",
                        errorCode, errorMessage),
                    Status = false
                };
            }

            return renewCertsStatus;
        }

        /// <summary>
        /// Attempts to delete the old backup PS certificates and sets the corresponding status.
        /// On failure, it starts the service back
        /// </summary>
        /// <param name="certsDir">PS certificates directory</param>
        /// <param name="pattern">Pattern of files to delete</param>
        /// <param name="ct">Cancellation token</param>
        /// <returns>Cert Renewal Status</returns>
        private static async Task<RenewCertsStatus> DeletePSBackupCertFilesAsync(
            string certsDir, string pattern, CancellationToken ct)
        {
            bool deleteBkpCertsStatus = false;

            ct.ThrowIfCancellationRequested();

            Tracers.TaskMgr.TraceAdminLogV2Message(
                TraceEventType.Information,
                "RenewPSCertificates : Deleting {0} files under folder : {1}",
                pattern,
                certsDir);

            var renewCertsStatus = GetDefaultRenewCertsStatus();

            try
            {
                var filesToDelete = FSUtils.GetFileList(
                    dir: certsDir,
                    pattern: pattern,
                    option: SearchOption.TopDirectoryOnly);

                if (filesToDelete == null && filesToDelete.Count() <= 0)
                {
                    return renewCertsStatus;
                }

                deleteBkpCertsStatus = await FSUtils.DeleteFilesAsync(
                    filesToDelete: filesToDelete,
                    maxRetryCount: Settings.Default.TaskMgr_CertFileMaxRetryCnt,
                    retryInterval: Settings.Default.TaskMgr_CertFileRetryInterval,
                    maxErrorCountToLog: Settings.Default.TaskMgr_MaxLimitForRateControllingFileDelExLogging,
                    traceSource: Tracers.TaskMgr,
                    ct: ct).ConfigureAwait(false);

                if (!deleteBkpCertsStatus)
                    throw new Exception(string.Format("Unable to delete {0} files", pattern));
            }
            catch (Exception ex)
            {
                // Attempt to start the stopped services back in case of failure
                renewCertsStatus = StartOrRestartService(Settings.Default.ServiceName_CXProcessServer, ct);

                var certFileName = pattern.Remove(pattern.Length - 1, 1);

                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "RenewPSCertificates : Failed to delete files from PS certs folder {0} with error {1}{2}",
                    certsDir,
                    Environment.NewLine,
                    ex);

                var errorCode = "ECA0169";
                renewCertsStatus = new RenewCertsStatus
                {
                    ErrorDescription = string.Format(
                        CultureInfo.InvariantCulture,
                        "SSL certificate renewal failed on server {0}",
                        SystemUtils.GetHostName()),
                    ErrorCode = errorCode,
                    PlaceHolders = new Dictionary<string, string>()
                    {
                        ["PsName"] = SystemUtils.GetHostName()
                    },
                    LogMessage = string.Format(
                        CultureInfo.InvariantCulture,
                        "{0}: {1} deletion failed.",
                        errorCode, certFileName),
                    Status = false
                };
            }

            return renewCertsStatus;
        }

        /// <summary>
        /// Attempts to stop the service. In case of failure,
        /// updates the cert update status and starts back the stopped service
        /// </summary>
        /// <param name="serviceName">ServiceName to stop</param>
        /// <param name="ct">Cancellation Token</param>
        /// <returns>Service Stop Status</returns>
        private static RenewCertsStatus StopService(
            string serviceName, CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            var maxRetryCount = Settings.Default.PSTask_RenewCertsStopServiceMaxRetryCount;

            var renewCertsStatus = GetDefaultRenewCertsStatus();

            var serviceStopStatus = TaskUtils.RunAndIgnoreException(() =>
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "RenewPSCertificates : Attempting to stop {0} service",
                    serviceName);

                ServiceUtils.StopServiceWithRetries(
                    serviceName: serviceName,
                    maxRetryCount: maxRetryCount,
                    pollTimeout: Settings.Default.PSMonitor_DefaultServicesPollChangeTimeout,
                    ct: ct);

                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "RenewPSCertificates : Successfully stopped {0} service",
                    serviceName);

            }, Tracers.TaskMgr);

            if (!serviceStopStatus)
            {
                var errorCode = "ECA0168";
                var errorDescription = string.Format(
                    CultureInfo.InvariantCulture,
                    "Unable to stop Service {0} on server {1}",
                    serviceName, SystemUtils.GetHostName());

                // Update cert status with service stop failure error
                renewCertsStatus = new RenewCertsStatus
                {
                    ErrorDescription = errorDescription,
                    ErrorCode = errorCode,
                    LogMessage = string.Format(
                        CultureInfo.InvariantCulture,
                        "{0}: StopService: Unable to stop {1} in {2} attempts.",
                        errorCode, serviceName, maxRetryCount),
                    PlaceHolders = new Dictionary<string, string>()
                    {
                        ["Name"] = SystemUtils.GetHostName(),
                        ["ServiceName"] = serviceName
                    },
                    Status = serviceStopStatus
                };

                // Attempt to start the stopped services back in case of failure
                var serviceStartStatus = StartService(serviceName, ct);
                if (!serviceStartStatus.Status)
                {
                    renewCertsStatus = serviceStartStatus;
                }
            }

            return renewCertsStatus;
        }

        /// <summary>
        /// If the service is in stopped state, start it now.
        /// Otherwise stop and start the service
        /// </summary>
        /// <param name="serviceName">Service Name</param>
        /// <param name="ct">Cancellation Token</param>
        /// <returns>Service Start/Stop Status</returns>
        private static RenewCertsStatus StartOrRestartService(string serviceName, CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            var renewCertsStatus = GetDefaultRenewCertsStatus();

            if (!ServiceUtils.IsServiceInstalled(serviceName))
            {
                throw new Exception(string.Format("{0} is not installed", serviceName));
            }

            if (ServiceUtils.IsServiceInExpectedState(serviceName, ServiceControllerStatus.Stopped, Tracers.TaskMgr))
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "RenewPSCertificates : {0} service is in stopped state, starting it now",
                    serviceName);
                renewCertsStatus = StartService(serviceName, ct);
            }
            else
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "RenewPSCertificates : {0} service is already in running state, restarting it now",
                    serviceName);

                renewCertsStatus = StopService(serviceName, ct);

                // Starting the service only if the service is successfully stopped
                if (renewCertsStatus.Status)
                {
                    renewCertsStatus = StartService(serviceName, ct);
                }
            }

            return renewCertsStatus;
        }

        /// <summary>
        /// Attempts to start the service and update the cert update status in case of failure
        /// </summary>
        /// <param name="serviceName">Service to start</param>
        /// <param name="ct">Cancellation Token</param>
        /// <returns>Service Start Status</returns>
        private static RenewCertsStatus StartService(
            string serviceName, CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            var maxRetryCount = Settings.Default.PSTask_RenewCertsStartServiceMaxRetryCount;

            var renewCertsStatus = GetDefaultRenewCertsStatus();

            var serviceStartStatus = TaskUtils.RunAndIgnoreException(() =>
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "RenewPSCertificates : Attempting to start {0} service",
                    serviceName);

                ServiceUtils.StartServiceWithRetries(
                    serviceName: serviceName,
                    maxRetryCount: maxRetryCount,
                    pollTimeout: Settings.Default.PSTask_RenewCertsStartServicePollChangeTimeout,
                    ct: ct);

                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "RenewPSCertificates : Successfully started {0} service",
                    serviceName);

            }, Tracers.TaskMgr);

            if (!serviceStartStatus)
            {
                var errorCode = "ECA0166";

                var errorDescription = string.Format(
                    CultureInfo.InvariantCulture,
                    "Unable to start {0} in {1} attempts.",
                    serviceName, maxRetryCount);

                // Update cert status with service start failure error
                renewCertsStatus = new RenewCertsStatus
                {
                    ErrorDescription = errorDescription,
                    ErrorCode = errorCode,
                    LogMessage = string.Format(
                        CultureInfo.InvariantCulture,
                        "{0}: StartService: {1}",
                        errorCode, errorDescription),
                    PlaceHolders = new Dictionary<string, string>()
                    {
                        ["Name"] = SystemUtils.GetHostName(),
                        ["ServiceName"] = serviceName
                    },
                    Status = serviceStartStatus
                };
            }

            return renewCertsStatus;
        }

        /// <summary>
        /// Rename the PS certificate backup files to their original names.
        /// </summary>
        /// <param name="ct">Cancellation Token</param>
        /// <returns>Move Files task</returns>
        private static async Task UndoPSCertFiles(CancellationToken ct)
        {
            string certsDir = Settings.Default.LegacyCS_FullPath_Certs;
            string fingerPrintsDir = Settings.Default.LegacyCS_FullPath_Fingerprints;
            string privateDir = Settings.Default.LegacyCS_FullPath_Private;

            try
            {
                await FSUtils.MoveFilesMatchingPatternAsync(
                    dir: certsDir,
                    pattern: PS_CERT_BKP_FILES_PATTERN,
                    option: SearchOption.TopDirectoryOnly,
                    originalFileName: PS_CERT_FILE,
                    maxRetryCount: Settings.Default.TaskMgr_CertFileMaxRetryCnt,
                    retryInterval: Settings.Default.TaskMgr_CertFileRetryInterval,
                    traceSource: Tracers.TaskMgr,
                    ct: ct).ConfigureAwait(false);
            }
            catch (Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "RenewPSCertificates : Failed to rename {0} files to {1} under {2} folder with error {3}{4}",
                    PS_CERT_BKP_FILES_PATTERN,
                    PS_CERT_FILE,
                    certsDir,
                    Environment.NewLine,
                    ex);
            }

            try
            {
                await FSUtils.MoveFilesMatchingPatternAsync(
                    dir: privateDir,
                    pattern: PS_KEY_BKP_FILES_PATTERN,
                    option: SearchOption.TopDirectoryOnly,
                    originalFileName: PS_KEY_FILE,
                    maxRetryCount: Settings.Default.TaskMgr_CertFileMaxRetryCnt,
                    retryInterval: Settings.Default.TaskMgr_CertFileRetryInterval,
                    traceSource: Tracers.TaskMgr,
                    ct: ct).ConfigureAwait(false);
            }
            catch (Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "RenewPSCertificates : Failed to rename {0} files to {1} under {2} folder with error {3}{4}",
                    PS_KEY_BKP_FILES_PATTERN,
                    PS_KEY_FILE,
                    certsDir,
                    Environment.NewLine,
                    ex);
            }

            try
            {
                await FSUtils.MoveFilesMatchingPatternAsync(
                    dir: privateDir,
                    pattern: PS_DH_BKP_FILES_PATTERN,
                    option: SearchOption.TopDirectoryOnly,
                    originalFileName: PS_DH_FILE,
                    maxRetryCount: Settings.Default.TaskMgr_CertFileMaxRetryCnt,
                    retryInterval: Settings.Default.TaskMgr_CertFileRetryInterval,
                    traceSource: Tracers.TaskMgr,
                    ct: ct).ConfigureAwait(false);
            }
            catch (Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "RenewPSCertificates : Failed to rename {0} files to {1} under {2} folder with error {3}{4}",
                    PS_DH_BKP_FILES_PATTERN,
                    PS_DH_FILE,
                    certsDir,
                    Environment.NewLine,
                    ex);
            }

            try
            {
                await FSUtils.MoveFilesMatchingPatternAsync(
                    dir: fingerPrintsDir,
                    pattern: PS_FP_BKP_FILES_PATTERN,
                    option: SearchOption.TopDirectoryOnly,
                    originalFileName: PS_FP_FILE,
                    maxRetryCount: Settings.Default.TaskMgr_CertFileMaxRetryCnt,
                    retryInterval: Settings.Default.TaskMgr_CertFileRetryInterval,
                    traceSource: Tracers.TaskMgr,
                    ct: ct).ConfigureAwait(false);
            }
            catch (Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "RenewPSCertificates : Failed to rename {0} files to {1} under {2} folder with error {3}{4}",
                    PS_FP_BKP_FILES_PATTERN,
                    PS_FP_FILE,
                    certsDir,
                    Environment.NewLine,
                    ex);
            }
        }

        public Task MonitorTelemetryAsync(
            ProcessServerSettings psSettings,
            CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            const string SRC_DIFF_FILES_PATTERN = "completed_diff*";
            const string TGT_DIFF_FILES_PATTERN = "completed_ediff*";
            const string SYNC_FILES_PATTERN = "completed_sync*.dat";

            long lastSystemRebootTime = SystemUtils.GetLastSystemRebootTime(Tracers.TaskMgr);
            var (totCacheFolSizeInBytes, freeVolSizeInKB, usedVolSizeInKB) =
                FSUtils.GetCacheVolUsageStats(
                    PSInstallationInfo.Default.GetRootFolderPath(), Tracers.TaskMgr);
            var isCumulativeThrottled = psSettings.IsCummulativeThrottled ? 1 : 0;

            var csHostId = psSettings.CSHostId;

            foreach (var pair in psSettings.Pairs)
            {
                ct.ThrowIfCancellationRequested();

                Tracers.TaskMgr.TraceAdminLogV2Message(TraceEventType.Information,
                    "MonitorTelemetry : Collecting telemetry data for pair : {0}, source hostId: {1}," +
                    " source device : {2}, target hostId : {3}, target device : {4}",
                    pair.PairId, pair.HostId, pair.DeviceId, pair.TargetHostId, pair.TargetDeviceId);

                var telemetryData = InMageTelemetryPSV2Row.BuildDefaultRow();
                TaskUtils.RunAndIgnoreException(() =>
                {
                    var (srcToPSFileUploadStuck, monitorTxtFileExists,
                    lastMoveFile, lastMoveFileModTime, lastMoveFileProcTime) =
                    DiffFileUtils.GetRPOData(pair, Tracers.TaskMgr, ct);

                    var pairRPOViolation = IsRPOViolated(pair, ct) ? 1 : 0;

                    Tracers.TaskMgr.TraceAdminLogV2Message(TraceEventType.Verbose,
                        "MonitorTelemetry : CSHostId : {0}", csHostId);

                    telemetryData.CSHostId = csHostId;
                    telemetryData.SrcAgentVer = pair.SourceAgentVersion;
                    telemetryData.SysTime = DateTimeUtils.GetSecondsAfterEpoch();
                    telemetryData.MessageType = 3;
                    telemetryData.HostId =
                    (!string.IsNullOrWhiteSpace(pair.HostId)) ? pair.HostId : string.Empty;
                    telemetryData.DiskId =
                    (!string.IsNullOrWhiteSpace(pair.DeviceId)) ? pair.DeviceId : string.Empty;
                    telemetryData.PSHostId = PSInstallationInfo.Default.GetPSHostId();
                    telemetryData.PsAgentVer = pair.PSVersion;
                    telemetryData.SysBootTime = lastSystemRebootTime;
                    telemetryData.SvAgtHB = pair.SourceLastHostUpdateTime;
                    telemetryData.AppAgtHB = pair.SourceAppAgentLastHostUpdateTime;
                    telemetryData.DiffThrotLimit = pair.DiffThrottleLimit;
                    telemetryData.ResyncThrotLimit = pair.ResyncThrottleLimit;
                    telemetryData.VolUsageLimit = psSettings.CumulativeThrottleLimit;
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
                    }, Tracers.TaskMgr);

                    Tracers.TaskMgr.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "MonitorTelemetry : Total resync files size is {0} for pair {1}, host {2}, dir {3}",
                        telemetryData.ResyncFldSize,
                        pair.DeviceId,
                        pair.HostId,
                        pair.ResyncFolder);

                    var quasiFlag = (pair.IsResyncRequired == 1) ? (int)pair.ProtectionState : 0;

                    // PairState
                    // By default all these values are 0's
                    // 0 - Replication deletion initiated then its value 1
                    // 1 - Target replication paused then its value 1
                    // 2 - Source replication paused then its value 1
                    // 3 - Cumulative throttle happened then its value 1
                    // 4 - Diff throttle happened then its value 1
                    // 5 - Resync throttle happened then its value 1
                    // 6 - Replication RPO violation happened then its value 1
                    // 7 - Source to PS file upload stuck then its value 1
                    // 8 - monitor.txt file exists then its value 1
                    // 9 - If the pair is in Resync, then its value 1.
                    // 10 - If the pair is in Resync and in Stage 2(quasi diff), then its value 1.
                    telemetryData.PairState = string.Format("{0}{1}{2}{3}{4}{5}{6}{7}{8}{9}{10}",
                        quasiFlag, pair.IsResyncRequired, monitorTxtFileExists, srcToPSFileUploadStuck,
                        pairRPOViolation, (int)pair.ThrottleResync, (int)pair.ThrottleDifferentials,
                        isCumulativeThrottled, (int)pair.SourceReplicationStatus,
                        (int)pair.TargetReplicationStatus, (int)pair.PairDeleted);

                    Tracers.TaskMgr.TraceAdminLogV2Message(TraceEventType.Information,
                    "MonitorTelemetry : Pair State is {0}", telemetryData.PairState);

                    var srcDiffsData = DiffFileUtils.ProcessDiffFolder(
                        pair.SourceDiffFolder, SRC_DIFF_FILES_PATTERN, Tracers.TaskMgr);

                    telemetryData.SrcFldSize = srcDiffsData.TotalPendingDiffs;
                    telemetryData.SrcNumOfFiles = (ulong)srcDiffsData.DiffFilesCount;
                    telemetryData.SrcFirstFile = srcDiffsData.DiffFirstFileName;
                    telemetryData.SrcFF_ModTime = srcDiffsData.DiffFirstFileTimeStamp;
                    telemetryData.SrcLastFile = srcDiffsData.DiffLastFileName;
                    telemetryData.SrcLF_ModTime = srcDiffsData.DiffLastFileTimeStamp;
                    telemetryData.SrcNumOfTagFile = srcDiffsData.DiffTagFilesCount;
                    telemetryData.SrcFirstTagFileName = srcDiffsData.DiffFirstTagFileName;
                    telemetryData.SrcFT_ModTime = srcDiffsData.DiffFirstTagFileTimeStamp;
                    telemetryData.SrcLastTagFileName = srcDiffsData.DiffLastTagFileName;
                    telemetryData.SrcLT_ModTime = srcDiffsData.DiffLastTagFileTimeStamp;

                    var tgtDiffsData = DiffFileUtils.ProcessDiffFolder(
                        pair.TargetDiffFolder, TGT_DIFF_FILES_PATTERN, Tracers.TaskMgr);

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
                }, Tracers.TaskMgr);

                Tracers.PSV2Telemetry.TracePsTelemetryV2Message(
                    TraceEventType.Information, telemetryData);

                Tracers.TaskMgr.TraceAdminLogV2Message(TraceEventType.Information,
                    "MonitorTelemetry : Successfully written telemetry data to json file" +
                    " for the pair with PairId : {0}",
                    pair.PairId);
            }

            return Task.CompletedTask;
        }

        /// <summary>
        /// Checks if RPO threshold is exceeded or not
        /// </summary>
        /// <param name="pair">Pair Settings</param>
        /// <param name="ct">Cancellation Token</param>
        /// <returns></returns>
        private static bool IsRPOViolated(IProcessServerPairSettings pair, CancellationToken ct)
        {
            bool isRpoViolated = false;

            ct.ThrowIfCancellationRequested();

            TaskUtils.RunAndIgnoreException(() =>
            {
                var currentRPOTimeInEpoch = pair.CurrentRPOTime;
                var statusUpdateTimeInEpoch = pair.StatusUpdateTime;
                var currentRPO = statusUpdateTimeInEpoch - currentRPOTimeInEpoch;

                int rpoThresholdInSecs = pair.RPOThreshold * 60;
                if (pair.IsResyncRequired != 1 && currentRPO > rpoThresholdInSecs)
                {
                    isRpoViolated = true;
                }
            }, Tracers.TaskMgr);

            Tracers.TaskMgr.TraceAdminLogV2Message(
                isRpoViolated ? TraceEventType.Error : TraceEventType.Information,
                "MonitorTelemetry : RPO is {0} for pair : {1}, source hostId: {2}, source device : {3}," +
                " target hostId : {4}, target device : {5}",
                isRpoViolated ? "violated" : "non-violated", pair.PairId, pair.HostId,
                pair.DeviceId, pair.TargetHostId, pair.TargetDeviceId);

            return isRpoViolated;
        }

        public Task AccumulatePerfDataAsync(
            ProcessServerSettings psSettings, CancellationToken ct)
        {
            if (ct.IsCancellationRequested)
                ct.ThrowIfCancellationRequested();

            TaskUtils.RunAndIgnoreException(() =>
            {
                foreach (var pair in psSettings.Pairs)
                {
                    string perfFile = Path.Combine(pair.PerfFolder, PERF_TXT_FILENAME);
                    string perfPSFile = Path.Combine(pair.PerfFolder, PERF_PS_TXT_FILENAME);

                    TaskUtils.RunAndIgnoreException(() =>
                    {
                        // Reading contents from perf.txt file for every pair and writing accumulated data to perf_ps.txt file.
                        // perf.txt file sample content 208,208
                        if (FSUtils.IsNonEmptyFile(perfFile, false))
                        {
                            string[] lines = File.ReadAllLines(perfFile).Where(str => !string.IsNullOrWhiteSpace(str)).ToArray();

                            long uncompressedSize = 0;
                            long compressedSize = 0;

                            foreach (var linetoParse in lines)
                            {
                                if (!string.IsNullOrWhiteSpace(linetoParse))
                                {
                                    String[] lineSplit = linetoParse.Split(',');

                                    if (lineSplit.Length == 2)
                                    {
                                        if (!string.IsNullOrWhiteSpace(lineSplit[0]))
                                        {
                                            uncompressedSize += long.Parse(lineSplit[0]);
                                        }

                                        if (!string.IsNullOrWhiteSpace(lineSplit[1]))
                                        {
                                            compressedSize += long.Parse(lineSplit[1]);
                                        }
                                    }
                                }
                            }

                            if (uncompressedSize != 0 && compressedSize != 0)
                            {
                                string perfData = string.Join(", ", uncompressedSize, compressedSize);
                                perfData = DateTimeUtils.GetSecondsAfterEpoch() + ": " + perfData;

                                File.AppendAllLines(perfPSFile, new string[] { perfData });

                                FSUtils.DeleteFile(perfFile);

                                Tracers.TaskMgr.TraceAdminLogV2Message(TraceEventType.Information,
                                    "AccumulatePerfData : Successfully written perf data: {0} to file" +
                                    " for the pair with DeviceId : {1}, HostId: {2}",
                                    perfData,
                                    pair.DeviceId,
                                    pair.HostId);
                            }
                        }
                    }, Tracers.TaskMgr);
                }

            }, Tracers.TaskMgr);

            return Task.CompletedTask;
        }

        public async Task UploadPerfDataAsync(
            ProcessServerSettings psSettings, CancellationToken ct)
        {
            var pairs = psSettings.Pairs;

            if (!pairs.Any())
            {
                return;
            }

            var psAlone =
                    (PSInstallationInfo.Default.GetCSType() == CXType.PSOnly) ? true : false;

            if (ct.IsCancellationRequested)
                ct.ThrowIfCancellationRequested();
            try
            {
                if (psAlone)
                {
                    int batchCount = 0;
                    int indexCount = 0;
                    List<Tuple<string, string>> batchFiles = new List<Tuple<string, string>>();

                    foreach (var pair in pairs)
                    {
                        try
                        {
                            string perfFileName = Path.Combine(pair.PerfShortFolder, PERF_PS_TXT_FILENAME);
                            string perfFilePath = pair.PerfShortFolder.Replace(PSInstallationInfo.Default.GetInstallationPath(), null);

                            if (FSUtils.IsNonEmptyFile(perfFileName))
                            {
                                batchFiles.Add(new Tuple<string, string>(perfFileName, perfFilePath));
                            }

                            batchCount++;
                            indexCount++;

                            // Uploading 25 files in one batch. Upload gets started when perf files count reaches to max supported limit or 
                            // files count reaches to pair count
                            if (batchCount == Settings.Default.PerfFileMaxCount ||
                                indexCount == psSettings.Pairs.Count())
                            {
                                await m_stubs.Value.UploadFilesAsync(batchFiles).ConfigureAwait(false);

                                batchFiles.ForEach(
                                    perfFile =>
                                    {
                                        Tracers.TaskMgr.TraceAdminLogV2Message(TraceEventType.Information,
                                            "UploadPerfDataAsync : Successfully uploaded perf file: {0} ",
                                            perfFile.Item1);

                                        if (File.Exists(perfFile.Item1))
                                        {
                                            FSUtils.DeleteFile(perfFile.Item1);
                                        }
                                    });

                                batchCount = 0;
                                batchFiles = new List<Tuple<string, string>>();
                            }
                        }
                        catch (Exception ex)
                        {
                            Tracers.TaskMgr.TraceAdminLogV2Message(
                                TraceEventType.Error,
                                "UploadPerfDataAsync : Failed to upload perf files: {0} : " +
                                " with error {1}",
                                string.Join(",", batchFiles.Select(perfFile => perfFile.Item1)),
                                ex);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "UploadPerfDataAsync : Failed to upload perf file with error {0}",
                    ex);
            }
        }
    }
}

