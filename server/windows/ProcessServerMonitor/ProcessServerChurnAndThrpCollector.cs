using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Microsoft.Azure.SiteRecovery.ProcessServerMonitor;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using PSCoreSettings = Microsoft.Azure.SiteRecovery.ProcessServer.Core.Settings;
using PSSettings = Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.ProcessServerSettings;

namespace ProcessServerMonitor
{
    /// <summary>
    /// Collects churn and throughput data and creates perf counters.
    /// </summary>
    internal class ProcessServerChurnAndThrpCollector : IPerfCollector
    {
        public const string ASRAnalyticsCategoryName = "ASRAnalytics";
        public const string ASRAnalyticsCategotyHelp = 
            "Performance counter related to ASR services";
        public const string ASRAnalyticsChurnCounter =
            "SourceVmChurnRate";
        public const string ASRAnalyticsThrpCounter =
            "SourceVmThrpRate";
        private const string ASRAnalyticsChurnCounterHelp = "Churn data for source vm";
        private const string ASRAnalyticsThrpCounterHelp = "Throughput data for source vm";
        private const int EXPECTED_FIELDS_CHURN_THRP_DATA = 7;
        private Stopwatch perfSW;

        /// <summary>
        /// Initializes stopwatch to collect and create perfs periodically
        /// </summary>
        public ProcessServerChurnAndThrpCollector()
        {
            // Start stopwatch for collecting perf samples.
            this.perfSW = new Stopwatch();
            perfSW.Start();
        }

        /// <summary>
        /// Manages periodic collection of data.
        /// If elapsed time is less then skips data collection and perf creation.
        /// </summary>
        public void TryCollectPerfData(CancellationToken ct)
        {
            if (perfSW.Elapsed >=
                            Settings.Default.PSChurnAndThrpSamplingInterval)
            {
                ct.ThrowIfCancellationRequested();

                this.CollectPerfData(ct);
                perfSW.Restart();
            }
            
        }

        /// <summary>
        /// Processes the churn and throughput files,
        /// creates the perf counters and deletes the processed files.
        /// </summary>
        public void CollectPerfData(CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                                TraceEventType.Information,
                                                "[PSChurnAndThrpCollector]Entered CollectPerfData");
            var endTime = DateTime.Now;
            var startTime = endTime - Settings.Default.PSChurnAndThrpSamplingInterval;

            // Collecting stats for the configured sampling interval
            GetProcessedChurnAndThrpFiles(startTime, endTime, false, ct,
                                          out IEnumerable<FileInfo> churFiles,
                                          out IEnumerable<FileInfo> thrpFiles);

            Dictionary<string, double> churDict = new Dictionary<string, double>();
            Dictionary<string, double> thrpDict = new Dictionary<string, double>();
            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                                TraceEventType.Information,
                                                "[PSChurnAndThrpCollector] Churn files count:{0}" +
                                                " and thrp files count {1}",
                                                churFiles.Count(), thrpFiles.Count());
            foreach (var fi in churFiles)
            {
                ct.ThrowIfCancellationRequested();

                TaskUtils.RunAndIgnoreException(() =>
                {
                    StreamReader reader;
                    reader = File.OpenText(fi.FullName);

                    string line;
                    while ((line = reader.ReadLine()) != null)
                    {
                        //Skip the disks where the instance name is too big, or data is not in proper format
                        if (ValidateAndParseInstanceNameAndValues(line, out string instName, out string value))
                        {
                            double.TryParse(value?.Replace("TrackedBytesRate=", ""), out double TrackRate);
                            if (churDict.ContainsKey(instName) == true)
                            {
                                churDict[instName] = churDict[instName] + TrackRate;
                            }
                            else
                            {
                                churDict.Add(instName, TrackRate);
                            }
                        }
                    }
                    reader.Close();
                }, Constants.s_psSysMonTraceSource);
            }

            foreach (var fi in thrpFiles)
            {
                ct.ThrowIfCancellationRequested();

                TaskUtils.RunAndIgnoreException(() =>
                {
                    StreamReader reader;
                    reader = File.OpenText(fi.FullName);

                    string line;
                    while ((line = reader.ReadLine()) != null)
                    {
                        //Skip the disks where the instance name is too big, or data is not in proper format
                        if (ValidateAndParseInstanceNameAndValues(line, out string instName, out string value))
                        {
                            double.TryParse(value?.Replace("SentBytesRate=", ""), out double sentRate);
                            if (thrpDict.ContainsKey(instName) == true)
                            {
                                thrpDict[instName] = thrpDict[instName] + sentRate;
                            }
                            else
                            {
                                thrpDict.Add(instName, sentRate);
                            }
                        }
                    }
                    reader.Close();
                }, Constants.s_psSysMonTraceSource);
            }

            if (!PerfHelper.CategoryExists(ASRAnalyticsCategoryName))
            {
                CounterCreationDataCollection counterCreationDataCollection =
                    new CounterCreationDataCollection
                    {
                         new CounterCreationData(ASRAnalyticsChurnCounter,
                             ASRAnalyticsChurnCounterHelp,
                             PerformanceCounterType.NumberOfItems64),
                         new CounterCreationData(ASRAnalyticsThrpCounter,
                             ASRAnalyticsThrpCounterHelp,
                             PerformanceCounterType.NumberOfItems64)
                    };
                PerfHelper.CreateCategory(
                    ASRAnalyticsCategoryName,
                    ASRAnalyticsCategotyHelp,
                    PerformanceCounterCategoryType.MultiInstance,
                                        counterCreationDataCollection);
            }

            PerformanceCounter churnPerfCounter;
            PerformanceCounter thrpPerfCounter;
            foreach (KeyValuePair<string, double> item in churDict)
            {
                ct.ThrowIfCancellationRequested();

                TaskUtils.RunAndIgnoreException(() =>
                {
                    churnPerfCounter = new PerformanceCounter(ASRAnalyticsCategoryName,
                                       ASRAnalyticsChurnCounter,
                                       item.Key,
                                       false)
                    {
                        RawValue = (long)item.Value
                    };

                    thrpDict.TryGetValue(item.Key, out double thrpData);
                    thrpPerfCounter = new PerformanceCounter(ASRAnalyticsCategoryName,
                                            ASRAnalyticsThrpCounter,
                                            item.Key,
                                            false)
                    {
                        RawValue = (long)thrpData
                    };
                }, Constants.s_psSysMonTraceSource);
            }

            DisposeProcessedFiles(ct);
            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                                TraceEventType.Verbose,
                                                "[PSChurnAndThrpCollector] Exited CollectPerfData");
        }

        /// <summary>
        /// Abstracted the code to retrieve the instance name for each churn and thrp record.
        /// </summary>
        /// <param name="line">Churn and thrp record.</param>
        /// <param name="items">Items in records. </param>
        /// <param name="instanceName">Instance name.</param>
        /// <returns>false if the data is not in expected format or 
        /// instance name is too big to create perf counters, else true.</returns>
        private static bool ValidateAndParseInstanceNameAndValues(string line, out string instanceName, out string value)
        {
            string[] items = line.Split(',');
            value = "";
            instanceName = "";
            if (items.Length != EXPECTED_FIELDS_CHURN_THRP_DATA)
            {
                return false;
            }

            string instName1 = items[0].Replace("MachineName=", "");
            string instName2 = items[2].Replace("DeviceId=", "");
            string instName3 = items[1].Replace("HostId=", "");
            instanceName = (instName1 + "_" + instName2 + "_" + instName3);
            value = items[6];

            // Perf counter name cannot be longer than 127
            // https://docs.microsoft.com/en-us/dotnet/api/system.diagnostics.performancecounter.-ctor?view=netframework-4.8
            if (instanceName.Length >= 127)
            {
                int lengthOfMachineNamePrefix = 127 - (instName2.Length + instName3.Length + 2);
                if (lengthOfMachineNamePrefix <= 0)
                {
                    // This means the device name is too big, generally the case of docker disk
                    // so skip it.
                    return false;
                }
                else
                {
                    lengthOfMachineNamePrefix = lengthOfMachineNamePrefix < instName1.Length ?
                        lengthOfMachineNamePrefix : instName1.Length; // To avoid indexOutOfRangeException
                    instName1 = instName1.Substring(0, lengthOfMachineNamePrefix);
                    instanceName = (instName1 + "_" + instName2 + "_" + instName3);
                }
            }

            return true;
        }


        /// <summary>
        /// Disposes the processed files.
        /// </summary>
        private void DisposeProcessedFiles(CancellationToken ct)
        {
            DateTime endTime = DateTime.Now;
            DateTime startTime = endTime - TimeSpan.FromTicks(
                Settings.Default.PSChurnAndThrpSamplingInterval.Ticks * 3);

            // Delete all the stat files that were created in the last (3 * sampling interval) minutes
            DisposeFiles(startTime, endTime, ct);
        }

        /// <summary>
        /// Disposes all existing files in churn and thrp folders.
        /// </summary>
        public void DisposeAllExistingFiles(CancellationToken ct)
        {
            var startTime = DateTime.MinValue;
            var endTime = DateTime.Now - Settings.Default.PSChurnAndThrpSamplingInterval;
            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                TraceEventType.Information,
                "[PSChurnAndThrpCollector] Deleting files that were created in" +
                "between {0} and {1}",
                startTime, endTime);

            // Dispose the files older than sampling interval
            DisposeFiles(startTime, endTime, ct);
        }

        /// <summary>
        /// Disposes all churn and thrp files where creation time is in between startTime and endTime.
        /// </summary>
        /// <param name="startTime">Files that were created after the starttime will be fetched</param>
        /// <param name="endTime">Files that were created earlier to endtime will be fetched</param>
        private void DisposeFiles(DateTime startTime, DateTime endTime, CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();
            Constants.s_psSysMonTraceSource.DebugAdminLogV2Message(
                "Deleting the files which were created in between {0} and {1}", startTime, endTime);

            GetProcessedChurnAndThrpFiles(
                startTime,
                endTime,
                true,
                ct,
                out IEnumerable<FileInfo> churFiles,
                out IEnumerable<FileInfo> thrpFiles);

            // Delete churn stat files
            DeleteStatFiles(churFiles, ct);

            // Delete throughput stat files
            DeleteStatFiles(thrpFiles, ct);
        }

        private void DeleteStatFiles(IEnumerable<FileInfo> statFiles, CancellationToken ct)
        {
            // Start statsDelTimer to measure the time taken to delete churn
            // and throughput stat files on PS
            var statsDelTimer = new Stopwatch();
            statsDelTimer.Start();

            var fileCount = statFiles.Count();

            if (fileCount <= 0)
            {
                return;
            }

            foreach (FileInfo file in statFiles)
            {
                try
                {
                    ct.ThrowIfCancellationRequested();

                    if (file.Exists)
                    {
                        file.Delete();
                    }
                    else
                    {
                        Constants.s_psSysMonTraceSource.DebugAdminLogV2Message(
                            "File not found {0}.", file);
                    }
                }
                catch (OperationCanceledException) when (ct.IsCancellationRequested)
                {
                    throw;
                }
                catch (Exception ioExp)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "[PSChurnAndThrpCollector] File {0} deletion failed with {1}{2}.",
                            file,
                            Environment.NewLine,
                            ioExp);
                }
            }

            statsDelTimer.Stop();

            // Logging the time taken to delete churn and throughput stat files only
            // when configured ChurnAndThrpStatsColWarnThreshold value exceeds
            if (statsDelTimer.Elapsed >= Settings.Default.ChurnAndThrpStatFilesDelWarnThreshold)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "[PSChurnAndThrpCollector] Time taken to delete {0} " +
                            "churn/throughput stat files on PS : {1} ms",
                            fileCount,
                            statsDelTimer.ElapsedMilliseconds);
            }
        }

        /// <summary>
        /// Recursively deletes churn and throughput stats folders
        /// </summary>
        public async Task DeleteStatsFolders(CancellationToken ct)
        {
            List<string> statsDirs = new List<string>();

            GetChurnStatFolderPaths(true, ct, out List<string> churnStatFolderPaths);
            GetThrpStatFolderPaths(true, ct, out List<string> thrpStatFolderPaths);
            statsDirs.AddRange(churnStatFolderPaths);
            statsDirs.AddRange(thrpStatFolderPaths);

            foreach (var currDir in statsDirs)
            {
                if (string.IsNullOrWhiteSpace(currDir))
                    continue;

                try
                {
                    ct.ThrowIfCancellationRequested();

                    await DirectoryUtils.DeleteDirectoryAsync(
                            folderPath: currDir,
                            recursive: true,
                            useLongPath: true,
                            maxRetryCount: PSCoreSettings.Default.TaskMgr_RecDelFolderMaxRetryCnt,
                            retryInterval: PSCoreSettings.Default.TaskMgr_RecDelFolderRetryInterval,
                            traceSource: Constants.s_psSysMonTraceSource,
                            ct: ct).ConfigureAwait(false);
                }
                catch (OperationCanceledException) when (ct.IsCancellationRequested)
                {
                    throw;
                }
                catch (Exception ex)
                {
                    // Ignoring the exception
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Warning,
                        "[PSChurnAndThrpCollector] Failed to delete directory {0} with error {1}{2}",
                        currDir,
                        Environment.NewLine,
                        ex);
                }
            }
        }

        /// <summary>
        /// Get stats files from the given folders.
        /// </summary>
        /// <param name="startTime">StartTime</param>
        /// <param name="endTime">EndTime</param>
        /// <param name="isDispose">If true, returns pre_complete* files also to be deleted</param>
        /// <param name="folderPaths">Stats folder paths</param>
        /// <param name="ct">Cancellation Token</param>
        /// <returns>Stats FileInfo</returns>
        private static IEnumerable<FileInfo> GetStatsFileList(
            DateTime startTime,
            DateTime endTime,
            bool isDispose,
            List<string> folderPaths,
            CancellationToken ct)
        {
            IEnumerable<FileInfo> statFiles = new List<FileInfo>();

            var filters = new List<string>() { "completed_*" };

            if (isDispose) filters.Add("pre_complete_*");

            foreach (var currDir in folderPaths)
            {
                ct.ThrowIfCancellationRequested();

                try
                {
                    foreach (var filter in filters)
                    {
                        try
                        {
                            var currStatFiles = FSUtils.GetFileListIgnoreException(
                               currDir, filter, SearchOption.TopDirectoryOnly)?.
                               Where(file => file.CreationTime >= startTime && file.CreationTime <= endTime);

                            statFiles = statFiles?.Concat(currStatFiles);
                        }
                        catch (Exception ex)
                        {
                            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Warning,
                                "[PSChurnAndThrpCollector] Failed to get {0} pattern stats files" +
                                " from directory {1} with error {2}{3}",
                                filter, currDir, Environment.NewLine, ex);
                        }
                    }
                }
                catch (Exception ex)
                {
                    // Ignoring the exception
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Warning,
                        "[PSChurnAndThrpCollector] Failed to get stats " +
                        "files from directory {0} with error {1}{2}",
                        currDir,
                        Environment.NewLine,
                        ex);
                }
            }

            return statFiles;
        }

        /// <summary>
        /// Populates the churn and thrp files older than startTime.
        /// </summary>
        /// <param name="startTime">Start time.</param>
        /// <param name="endTime">End time.</param>
        /// <param name="isDispose">Flag to identify deletestats call</param>
        /// <param name="ct">Cancellation Token.</param>
        /// <param name="churFiles">Churn files list.</param>
        /// <param name="thrpFiles">Throughput files list.</param>
        private void GetProcessedChurnAndThrpFiles(
            DateTime startTime,
            DateTime endTime,
            bool isDispose,
            CancellationToken ct,
            out IEnumerable<FileInfo> churFiles,
            out IEnumerable<FileInfo> thrpFiles)
        {
            ct.ThrowIfCancellationRequested();

            // Start statsColTimer to measure the time taken to get churn
            // and throughput stat files on PS
            var statsColTimer = new Stopwatch();
            statsColTimer.Start();

            GetChurnStatFolderPaths(isDispose, ct, out List<string> churnStatFolderPaths);
            GetThrpStatFolderPaths(isDispose, ct, out List<string> thrpStatFolderPaths);

            churFiles = GetStatsFileList(startTime, endTime, isDispose, churnStatFolderPaths, ct);
            thrpFiles = GetStatsFileList(startTime, endTime, isDispose, thrpStatFolderPaths, ct);

            statsColTimer.Stop();

            // Logging the time taken to get churn and throughput stat files only
            // when configured ChurnAndThrpStatsColWarnThreshold value exceeds
            if (statsColTimer.Elapsed >= Settings.Default.ChurnAndThrpStatsColWarnThreshold)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "[PSChurnAndThrpCollector] Time taken to fetch {0} churn stat files," +
                            " {1} throughput stat files on PS : {2} ms",
                            churFiles.Count(),
                            thrpFiles.Count(),
                            statsColTimer.ElapsedMilliseconds);
            }
        }

        /// <summary>
        /// Populates churn stat folder paths
        /// </summary>
        /// <param name="isDispose">Flag for Delete call</param>
        /// <param name="ct">Cancellation token</param>
        /// <param name="churnStatFolderPaths">Chrun stats folder path list</param>
        private void GetChurnStatFolderPaths(
            bool isDispose,
            CancellationToken ct,
            out List<string> churnStatFolderPaths)
        {
            ct.ThrowIfCancellationRequested();

            churnStatFolderPaths = new List<string>();
            if (PSInstallationInfo.Default.CSMode == CSMode.LegacyCS)
            {
                churnStatFolderPaths.Add(PSInstallationInfo.Default.GetChurnStatFolderPath());
            }
            else if (PSInstallationInfo.Default.CSMode == CSMode.Rcm)
            {
                GetStatFolderPaths(PSCoreSettings.Default.CommonMode_SubPath_ChurStat,
                    isDispose, ct, out List<string> statFolderPaths);
                churnStatFolderPaths.AddRange(statFolderPaths);
            }
        }

        /// <summary>
        /// Populates throughput stat folder path
        /// </summary>
        /// <param name="isDispose">Flag for Delete call</param>
        /// <param name="ct">Cancellation token</param>
        /// <param name="thrpStatFolderPaths">Throughput stat folder path list</param>
        private void GetThrpStatFolderPaths(
            bool isDispose,
            CancellationToken ct,
            out List<string> thrpStatFolderPaths)
        {
            ct.ThrowIfCancellationRequested();

            thrpStatFolderPaths = new List<string>();
            if (PSInstallationInfo.Default.CSMode == CSMode.LegacyCS)
            {
                thrpStatFolderPaths.Add(PSInstallationInfo.Default.GetThrpStatFolderPath());
            }
            else if (PSInstallationInfo.Default.CSMode == CSMode.Rcm)
            {
                GetStatFolderPaths(PSCoreSettings.Default.CommonMode_SubPath_ThrpStat,
                    isDispose, ct, out List<string> statFolderPaths);
                thrpStatFolderPaths.AddRange(statFolderPaths);
            }
        }

        /// <summary>
        /// Populates the stat folder paths
        /// </summary>
        /// <param name="type">Stats type</param>
        /// <param name="ct">Cancellation token</param>
        /// <param name="statFolderPaths">Stats folder paths list</param>
        private void GetStatFolderPaths(
            String type,
            bool isDispose,
            CancellationToken ct,
            out List<string> statFolderPaths)
        {
            ct.ThrowIfCancellationRequested();

            statFolderPaths = new List<string>();

            // Returning all the churn and throughput folders for dispose call,
            // so that any folders for disabled pairs would be cleanedup.
            if (isDispose)
            {
                ct.ThrowIfCancellationRequested();

                var statsDirs = new List<string>();
                TaskUtils.RunAndIgnoreException(() =>
                {
                    var statsDirInfo = new DirectoryInfo(
                        PSInstallationInfo.Default.GetTelemetryFolderPath());

                    statsDirs?.AddRange(statsDirInfo.EnumerateDirectories(
                        type, SearchOption.AllDirectories)?.Select(d => d.FullName));
                }, Constants.s_psSysMonTraceSource);

                statFolderPaths = statsDirs;
            }
            // Returning Churn and Throughput stats folders only for the active replications for perf collection
            else
            {
                PSSettings psSettings = PSSettings.GetCachedSettings();

                if (psSettings == null || psSettings.ProtectedMachineTelemetrySettings == null)
                {
                    Constants.s_psSysMonTraceSource.DebugAdminLogV2Message(
                                                    "GetStatFolderPaths : Returning as TelemetrySettings are not yet available");
                    return;
                }

                foreach (var enumerableTelemetrySettingsPath in psSettings.ProtectedMachineTelemetrySettings)
                {
                    try
                    {
                        ct.ThrowIfCancellationRequested();

                        var statDirectory = new DirectoryInfo(FSUtils.CanonicalizePath(Path.Combine(enumerableTelemetrySettingsPath.TelemetryFolderPath,
                            type)));
                        if (statDirectory.Exists)
                        {
                            statFolderPaths.Add(statDirectory.ToString());
                        }
                    }
                    catch (Exception ex)
                    {
                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Ignoring exception, while running the action{0}{1}",
                            Environment.NewLine, ex);
                    }
                }
            }
        }
    }
}
