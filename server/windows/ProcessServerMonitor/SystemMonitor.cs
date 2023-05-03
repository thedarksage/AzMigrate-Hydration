// -----------------------------------------------------------------------
// <copyright file="SystemMonitor.cs" company="Microsoft">
// Monitor's system health.
// </copyright>
// -----------------------------------------------------------------------

using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Monitoring;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Microsoft.Azure.SiteRecovery.ProcessServerMonitor;
using Newtonsoft.Json;
using RcmContract;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Management;
using System.Reflection;
using System.Security.Cryptography.X509Certificates;
using System.ServiceProcess;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using static RcmContract.MonitoringMsgEnum;
using ProcessServerSettings = Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.ProcessServerSettings;
using PSCoreSettings = Microsoft.Azure.SiteRecovery.ProcessServer.Core.Settings;

namespace ProcessServerMonitor
{
    /// <summary>
    /// Monitors System Performance and PS services status
    /// </summary>
    internal class SystemMonitor : IDisposable
    {
        /// <summary>
        /// Performance counter for CPU Load
        /// </summary>
        private PerformanceCounter cpuCounter;

        /// <summary>
        /// Performance counter for Virtual Memory Usage Percentage.
        /// </summary>
        private PerformanceCounter memoryPercentInUseCounter;

        /// <summary>
        /// Performance counter for total Virtual Memory.
        /// </summary>
        private PerformanceCounter memoryCommitLimitCounter;

        /// <summary>
        /// Performance counter for Processor Queue Length.
        /// </summary>
        private PerformanceCounter procQueueLenCounter;

        //TODO: Use Lazy<IProcessServerCSApiStubs> object to be resilient to any initialization failures that would lead to crash.
        private IProcessServerCSApiStubs psApi;

        private IProcessServerMonitoringStubs psMonitor;

        private readonly int expVolsyncChildrenCnt = PSInstallationInfo.Default.CSMode == CSMode.LegacyCS ? PSInstallationInfo.Default.GetMaxTmid() : 0;

        private CircularList<StatisticStatus> throughputHealth =
            new CircularList<StatisticStatus>(Settings.Default.MaxHealthyThroughputCntToSkipCurrEvent);

        public const string MONITORING_DISKSPACE = "DiskSpace";
        public const string MONITORING_LAST_REBOOT_TIME = "SystemBootTime";
        public const string MONITORING_CURRENT_RPO = "CurrentRPO";
        public const string MONITORING_PREVIOUS_VERSION = "PreviousVersion";
        public const string MONITORING_CURRENT_VERSION = "CurrentVersion";
        public const string MONITORING_HOST_NAME = "PSHostName";
        public const string MONITORING_IP_ADDRESS = "IPAddress";

        // [Himanshu] send this to Jayesh to ask him to handle this.
        private static class UnknownStat
        {
            public static readonly
                (long ProcessorQueueLength, StatisticStatus Status) SystemLoad =
                (-1, StatisticStatus.Unknown);

            public static readonly
                (decimal Percentage, StatisticStatus Status) CpuLoad =
                (-1, StatisticStatus.Unknown);

            public static readonly
                (long Total, long Used, StatisticStatus Status) MemoryUsage =
                (-1, -1, StatisticStatus.Unknown);

            public static readonly
                (long Total, long Used, StatisticStatus Status) InstallVolumeSpace =
                (-1, -1, StatisticStatus.Unknown);

            public static readonly
                (long UploadPendingData, long ThroughputBytesPerSec, StatisticStatus Status) Throughput =
                (-1, -1, StatisticStatus.Unknown);
        }

        /// <summary>
        /// Monitors Process Server health and sends alerts to CS.
        /// </summary>
        /// <param name="token">Cancellation token.</param>
        internal async Task MonitorSystem(CancellationToken token)
        {
            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                TraceEventType.Information, "Starting SystemMonitor");

            token.ThrowIfCancellationRequested();

            // Parse settings
            bool status = ParseSettings(
                numOfPerfSamples: out int numOfPerfSamples,
                numOfUploadedDataSamples: out int numOfUploadedDataSamples,
                numOfStatsSamples: out int numOfStatSamples,
                psServicesList: out List<string> psServicesList,
                csServicesList: out List<string> csServicesList);

            if (!status)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                    TraceEventType.Error, "Failed to parse settings");

                return;
            }
            List<string> statsServiceList = GetServiceList(psServicesList, csServicesList);

            List<string> notRunningServices = new List<string>();
            var perfStatistics = new CircularList<SystemPerfContainer>(numOfPerfSamples);
            var statsContainer = new CircularList<SystemPerfContainer>(numOfStatSamples);
            var uploadedDataStats = new CircularList<UploadedDataContainer>(numOfUploadedDataSamples);

            var cachedThroughputStat = UnknownStat.Throughput;

            TimeSpan healthReportInterval = Settings.Default.HealthReportInterval;

            // Start stopwatch for service monitoring
            Stopwatch serviceSW = new Stopwatch();
            serviceSW.Start();

            // Start stopwatch for collecting perf samples
            Stopwatch perfSW = new Stopwatch();
            perfSW.Start();

            // Start stopwatch for collecting uploaded data samples
            Stopwatch uploadedDataSW = new Stopwatch();
            uploadedDataSW.Start();

            // Start stopwatch for perf reporting task
            Stopwatch reportPerfSW = new Stopwatch();
            reportPerfSW.Start();

            // Start stopwatch for reporting service task
            Stopwatch reportServiceSW = new Stopwatch();
            reportServiceSW.Start();

            // Start stopwatch for reporting throughput task
            Stopwatch reportThroughPutSW = new Stopwatch();
            reportThroughPutSW.Start();

            // Start stopwatch for monitoring statistics
            Stopwatch monitorStatisticsSW = new Stopwatch();
            monitorStatisticsSW.Start();

            // Start stopwatch for reporting statistics
            Stopwatch reportStatisticsSW = new Stopwatch();
            reportStatisticsSW.Start();

            // Start stopwatch for initializing objects
            Stopwatch initObjSW = new Stopwatch();
            initObjSW.Start();

            // Start stopwatch for reporting ps events.
            Stopwatch reportPsEventsSW = new Stopwatch();
            reportPsEventsSW.Start();

            // Start stopwatch for monitoring cumulative throttle
            Stopwatch monitorCumulativeThrottleSw = new Stopwatch();
            monitorCumulativeThrottleSw.Start();

            // Start stopwatch for monitoring certs.
            Stopwatch monitorCertsSW = new Stopwatch();
            monitorCertsSW.Start();

            // Task for monitoring and reporting PS health
            Task monitorHealthTask = null;

            // Task for reporting PS statistics
            Task reportStatisticsTask = null;

            // Task for reporting PS statistics
            Task reportPSEventsTask = null;

            // Task for starting service
            Task serviceStartTask = null;

            // Task for monitoring cumulative throttle
            Task monitorCumulativeThrottleTask = null;

            // Task for monitoring certs
            Task monitorCertsTask = null;

            bool isFirstRun = true;
            bool isFirstPerfCollection = true;
            bool isFirstServiceCollection = true;
            bool isFirstThrougputCollection = true;
            bool isFirstThroughputAlert = true;
            bool isFirstMonitorCertsTask = true;
            int serviceCntr = 0;
            int initErrCount = 0;

            while (true)
            {
                try
                {
                    // Retain this wait in the beginning of the try..catch loop,
                    // so that a tight loop is avoided, if there's a consistent
                    // failure in the code below.

                    await Task.Delay(
                        Settings.Default.SysMonMainIterWaitInterval, token).ConfigureAwait(false);

                    if (isFirstRun || initObjSW.Elapsed >= Settings.Default.InitFailureRetryInterval)
                    {
                        var initError = Init();

                        if (initError.Length > 0)
                        {
                            initErrCount++;

                            if (initErrCount <= Settings.Default.MaxLimitForRateControllingExLogging)
                            {
                                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Error,
                                "Failed to initialize objects {0} times with error : {1}{2}",
                                initErrCount,
                                Environment.NewLine,
                                initError);
                            }
                            else if ((initErrCount % Settings.Default.MaxErrorCountForInitExLogging) == 0)
                            {
                                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Error,
                                "Failed to initialize objects even after {0} retries. Total number of failures : {1}. Last error : {2}{3}",
                                Settings.Default.MaxErrorCountForInitExLogging,
                                initErrCount,
                                Environment.NewLine,
                                initError);
                            }
                        }
                        else
                        {
                            initErrCount = 0;
                        }

                        initObjSW.Restart();
                    }

                    if (isFirstRun)
                    {
                        Debug.Assert(monitorHealthTask == null);

                        // Reset previous health factors during first run if monitoring is off.
                        monitorHealthTask = this.ResetHealthAsync(token);
                        isFirstRun = false;
                    }

                    List<InfraHealthFactorItem> infraHealthFactorItems =
                        new List<InfraHealthFactorItem>();

                    bool perfStatsRequired = false;
                    bool monitorStatsRequried = false;

                    // Collect perf statistics only if
                    // monitor perf statistics is set or
                    // reporting perf statistics is set
                    if (Settings.Default.ReportStatistics ||
                        Settings.Default.MonitorPerfStatistics)
                    {
                        if (perfSW.Elapsed >=
                            Settings.Default.PerfMonitorInterval)
                        {
                            perfSW.Restart();
                            perfStatsRequired = true;
                        }

                        if (monitorStatisticsSW.Elapsed >=
                            Settings.Default.StatisticsMonitorInterval)
                        {
                            monitorStatisticsSW.Restart();
                            monitorStatsRequried = true;
                        }

                        // collect only once and populate both lists.
                        // collecting twice in the same loop makes the time interval between two collection in micro secs
                        // but performance monitor samples are collected at 1 sec interval.
                        // querying a second sample in less than a sec makes the second value as 0.
                        if (perfStatsRequired || monitorStatsRequried)
                        {
                            var systemStats = this.CollectSystemStats();

                            if (perfStatsRequired)
                            {
                                perfStatistics.CurrentValue = systemStats;
                                perfStatistics.Next();
                            }

                            if (monitorStatsRequried)
                            {
                                statsContainer.CurrentValue = systemStats;
                                statsContainer.Next();
                            }
                        }
                    }

                    if (Settings.Default.MonitorPerfStatistics)
                    {
                        // In the first run fetch the perf health factors only when healthmonitoring interval elapses
                        // From the next run onwards fetch the health factors when configured healthReporting interval elapses.
                        // Restart the report stopwatch
                        if ((isFirstPerfCollection
                            && reportPerfSW.Elapsed >=
                            Settings.Default.HealthMonitorInterval) ||
                            (!isFirstPerfCollection
                            && reportPerfSW.Elapsed >= healthReportInterval))
                        {
                            // Get average of the collected perf samples.
                            var (perfHealthFactors, _) =
                                this.GetPerfHealth(perfStatistics, logHealth: true);
                            infraHealthFactorItems.AddRange(perfHealthFactors);
                            isFirstPerfCollection = false;
                            reportPerfSW.Restart();
                        }
                    }

                    // Fetch service status only if monitor services is enabled.
                    // Disabling in case of Rcm as the contracts are not in yet.
                    if (Settings.Default.MonitorServiceStatus &&
                        PSInstallationInfo.Default.CSMode != CSMode.Rcm)
                    {
                        // Fetch service status when configured service monitoring interval elapses.
                        if (isFirstServiceCollection ||
                            serviceSW.Elapsed >=
                            Settings.Default.ServiceMonitorInterval)
                        {
                            bool IsServiceRunning(string serviceName, int? numOfInstances)
                            {
                                // If null, we failed to retrieve the info about this service.
                                // In that case, defaulting to success.
                                // TODO-SanKumar-1906: Should there be a setting for default value?
                                if (numOfInstances == null)
                                    return true;

                                bool isVolsync = serviceName.Equals("volsync", StringComparison.OrdinalIgnoreCase);

                                return
                                    (isVolsync && numOfInstances == expVolsyncChildrenCnt) ||
                                    (!isVolsync && numOfInstances == 1);
                            }

                            var notRunningCurrIter = this.MonitorServices(psServicesList)?.
                                Where(svcKVPair => !IsServiceRunning(svcKVPair.Key, svcKVPair.Value)).
                                Select(svcKVPair => svcKVPair.Key);

                            if (notRunningCurrIter != null)
                            {
                                notRunningServices.AddRange(notRunningCurrIter);
                            }

                            serviceSW.Restart();
                            isFirstServiceCollection = false;
                            serviceCntr++;

                            // Start push install service, if it is in stopped state
                            if (notRunningServices != null && notRunningServices.Contains("InMage PushInstall"))
                            {
                                if (serviceStartTask == null || serviceStartTask.IsCompleted)
                                {
                                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                        TraceEventType.Information, "Starting the InMage PushInstall service");

                                    serviceStartTask = Task.Run(() => StartService("InMage PushInstall", token), token);
                                }
                                else
                                {
                                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                            TraceEventType.Warning,
                                            $"Skipping the current {nameof(serviceStartTask)} as last " +
                                            $"triggered task is still running.");
                                }
                            }
                        }

                        // Set/Reset the stopped service health factor when healthReporting interval elapses and restart the stop watch.
                        if (reportServiceSW.Elapsed >= healthReportInterval)
                        {
                            var serviceHealthFactors =
                                this.GetServiceHealth(notRunningServices, serviceCntr);
                            notRunningServices.Clear();
                            serviceCntr = 0;

                            infraHealthFactorItems.AddRange(serviceHealthFactors);
                            reportServiceSW.Restart();
                        }
                    }

                    // Fetch throughput status only if monitor throughput stats is enabled (or)
                    // if report statistics is enabled
                    if (Settings.Default.MonitorThroughputStats || Settings.Default.ReportStatistics)
                    {
                        // Monitor uploadedData as per configured healthreportinterval
                        if (isFirstThrougputCollection ||
                            uploadedDataSW.Elapsed >=
                            Settings.Default.HealthReportInterval)
                        {
                            this.MonitorUploadedData(uploadedDataStats);
                            isFirstThrougputCollection = false;
                            uploadedDataSW.Restart();
                        }

                        // In the first run fetch the throughput health factors only when healthmonitoring interval elapses
                        // From the next run onwards fetch the health factors when configured healthReporting interval elapses.
                        // Restart the report stopwatch
                        if ((isFirstThroughputAlert
                            && reportThroughPutSW.Elapsed >=
                            Settings.Default.HealthMonitorInterval) ||
                            (!isFirstThroughputAlert
                            && (reportThroughPutSW.Elapsed >=
                            healthReportInterval)))
                        {
                            // Reset the cached stat before every collection, so
                            // that it's reported as Unknown on failure.
                            cachedThroughputStat = UnknownStat.Throughput;

                            var (throughputHealthFactors, throughputStat) =
                                this.GetUploadDataHealth(uploadedDataStats, token);

                            if (Settings.Default.MonitorThroughputStats)
                            {
                                // The throughput collection could occur, even if
                                // the report statistics is only enabled. In that
                                // case, we shouldn't be raising throughput health
                                // alerts.
                                infraHealthFactorItems.AddRange(throughputHealthFactors);
                            }

                            cachedThroughputStat = throughputStat;

                            isFirstThroughputAlert = false;
                            reportThroughPutSW.Restart();
                        }
                    }

                    ProcessServerStatistics psStatistics = null;

                    // If reporting statistics is enabled, monitor and/or report statistics
                    if (Settings.Default.ReportStatistics)
                    {
                        if (reportStatisticsSW.Elapsed >= Settings.Default.StatisticsReportInterval)
                        {
                            reportStatisticsSW.Restart();

                            // Stats service list include CS services also in an in-built PS.
                            var serviceInstances = this.MonitorServices(statsServiceList);

                            // NOTE-SanKumar-1906: At the beginning of the service, the throughput
                            // stats will be "Not Available" on the portal for the first 15 mins,
                            // since we are hooking on to the report throughput if block instead of
                            // the monitor throughput if block above. This has been done to make the
                            // implementation easier, where the throughput status is calculated with
                            // existing tested health generation code very easily.
                            psStatistics = GeneratePSStatistics(
                                statsContainer, cachedThroughputStat, serviceInstances);
                        }
                    }

                    // If reporting ps events is enabled, monitor and/or report ps events
                    if (Settings.Default.ReportPSEvents &&
                        reportPsEventsSW.Elapsed >= Settings.Default.PSEventsReportInterval)
                    {
                        reportPsEventsSW.Restart();

                        if (reportPSEventsTask == null || reportPSEventsTask.IsCompleted)
                        {
                            reportPSEventsTask = GetPSHealthEvents(token);
                            UpdatePSEventsRelatedInfo();
                        }
                        else
                        {
                            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Warning,
                                    $"Skipping the current {nameof(reportPSEventsTask)} as last " +
                                    $"triggered {nameof(reportPSEventsTask)} is still running.");
                        }
                    }
                    
                    if (PSInstallationInfo.Default.CSMode == CSMode.LegacyCS &&
                        Settings.Default.ReportCumulativeThrottle &&
                        monitorCumulativeThrottleSw.Elapsed >=
                        Settings.Default.CumulativeThrottleMonitorInterval)
                    {
                        monitorCumulativeThrottleSw.Restart();
                        if (monitorCumulativeThrottleTask == null || monitorCumulativeThrottleTask.IsCompleted)
                        {
                            monitorCumulativeThrottleTask = MonitorCumulativeThrottleForLegacyPS(token);
                        }
                        else
                        {
                            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Warning,
                                    $"Skipping the current {nameof(monitorCumulativeThrottleTask)} as last " +
                                    $"triggered {nameof(monitorCumulativeThrottleTask)} is still running.");
                        }
                    }

                    // If monitor certs is enabled, updates the cert expiry details
                    // of CS and PS. Sends alerts if PS cert nears expiry/expired
                    if (PSInstallationInfo.Default.CSMode == CSMode.LegacyCS &&
                        Settings.Default.MonitorCerts &&
                        (isFirstMonitorCertsTask || monitorCertsSW.Elapsed >= Settings.Default.MonitorCertsInterval))
                    {
                        monitorCertsSW.Restart();

                        if (monitorCertsTask == null || monitorCertsTask.IsCompleted)
                        {
                            monitorCertsTask = MonitorCerts(token);
                        }
                        else
                        {
                            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Warning,
                                    $"Skipping the current {nameof(monitorCertsTask)} as last " +
                                    $"triggered {nameof(monitorCertsTask)} is still running.");
                        }

                        isFirstMonitorCertsTask = false;
                    }

                    if (psStatistics != null)
                    {
                        if (reportStatisticsTask == null || reportStatisticsTask.IsCompleted)
                        {
                            reportStatisticsTask =
                                Task.Run(
                                    async () => await ReportStatistics(psStatistics, token).ConfigureAwait(false),
                                    token);
                        }
                        else
                        {
                            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Warning,
                                    $"Skipping the current {nameof(reportStatisticsTask)} as last " +
                                    $"triggered {nameof(reportStatisticsTask)} is still running.");
                        }
                    }

                    // Report health errors
                    // Moving this after report stats so that whn we are posting health factors,
                    // The stats have been refreshed.
                    if (infraHealthFactorItems != null &&
                        infraHealthFactorItems.Count > 0)
                    {
                        if (monitorHealthTask == null || monitorHealthTask.IsCompleted)
                        {
                            monitorHealthTask =
                                Task.Run(
                                    async () => await this.ReportInfraHealthErrors(infraHealthFactorItems, token).ConfigureAwait(false),
                                    token);
                        }
                        else
                        {
                            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Warning,
                                    $"Skipping the current {nameof(monitorHealthTask)} as last " +
                                    $"triggered {nameof(monitorHealthTask)} is still running.");
                        }
                    }
                }
                catch (OperationCanceledException) when (token.IsCancellationRequested)
                {
                    // TODO-SanKumar-1905: There few cases, where the library
                    // could return operation canceled exception, even without
                    // the token getting set. That might lead to stopping of this
                    // task but the service would show up as running. We should
                    // probably check, if the cancellation was actually requested
                    // and then only quit this thread.

                    // Rethrow operation canceled exception if the service is stopped.
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Verbose,
                                    "Cancelling System monitoring Task");
                    throw;
                }
                catch (Exception ex)
                {
                    // TODO: Add an overload to TraceAdminLogV2Message that takes exception object as param to add newlines while logging
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Exception at MonitorSystem - {0}{1}",
                        Environment.NewLine,
                        ex);
                }
            }
        }

        private Task MonitorCumulativeThrottleForLegacyPS(CancellationToken token)
        {
            SystemHealth sysHealth = SystemHealth.Unknown;
            bool isCumulativeThrottleSet = false;

            bool retval = TaskUtils.RunAndIgnoreException(() =>
            {
                ProcessServerSettings psSettings = ProcessServerSettings.GetCachedSettings();
                if (psSettings == null)
                {
                    throw new Exception(
                            "Unable to get the ps settings, so skipping monitoring cumulative throttle");
                }

                isCumulativeThrottleSet = SystemUtils.IsCumulativeThrottled(
                    psSettings.CumulativeThrottleLimit,
                    PSInstallationInfo.Default.GetRootFolderPath(),
                    Constants.s_psSysMonTraceSource);
            }, Constants.s_psSysMonTraceSource);

            if (retval)
            {
                return TaskUtils.RunTaskForAsyncOperation(
                    traceSource: Constants.s_psSysMonTraceSource,
                    name: nameof(MonitorCumulativeThrottleForLegacyPS),
                    ct: token,
                    operation : async(taskCancelToken) =>
                    await psApi.UpdateCumulativeThrottleAsync(
                        isThrottled: isCumulativeThrottleSet,
                        systemHealth: sysHealth,
                        ct: taskCancelToken).ConfigureAwait(false));
            }
            else
            {
                return null;
            }
        }

        internal Task UpgradeTasks(CancellationToken cancellationToken)
        {
            var taskList = new List<Task>();

            // One time upgrade task to move the data in monitoring txt files
            // to registry, so that we don't loose the data captured by perl
            // in case of event migration from perl to .net
            var upgradeTask = TaskUtils.RunTaskForSyncOperation(
            traceSource: Constants.s_psSysMonTraceSource,
            name: $"{nameof(UpgradeTasks)}",
            ct: cancellationToken,
            operation: UpdateV2APSEventsInfoInRegistry);

            taskList.Add(upgradeTask);

            return Task.WhenAll(taskList);
        }

        private DateTime? GetCertExpiryDate(string certFilePath)
        {
            DateTime? certExpiryDate = null;

            TaskUtils.RunAndIgnoreException(() =>
            {
                if (string.IsNullOrWhiteSpace(certFilePath))
                    throw new ArgumentNullException(nameof(certFilePath));

                var cert = new X509Certificate2(certFilePath);

                if (cert != null)
                {
                    certExpiryDate = cert.NotAfter.ToUniversalTime();
                }

                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "{0} cert expiry date : {1}",
                    certFilePath,
                    certExpiryDate);
            }, Constants.s_psSysMonTraceSource);

            return certExpiryDate;
        }

        private Task MonitorCerts(CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            var csType = PSInstallationInfo.Default.GetCSType();
            var certExpiryDict = new Dictionary<string, long>();

            DateTime? psCertExpiryDate = null;
            string hostId = null, hostName = null;

            TaskUtils.RunAndIgnoreException(() =>
            {
                hostId = PSInstallationInfo.Default.GetPSHostId();
                hostName = System.Net.Dns.GetHostName();

                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "Monitoring Certs on {0}({1}). CSType : {2}",
                    hostName, hostId, csType);

                if (csType == CXType.CSAndPS || csType == CXType.PSOnly)
                {
                    var psCertsPath = PSInstallationInfo.Default.GetPSCertificateFilePath();
                    psCertExpiryDate = GetCertExpiryDate(psCertsPath);

                    if (psCertExpiryDate.HasValue)
                    {
                        long psCertExpiryEpochTime =
                            DateTimeUtils.GetSecondsSinceEpoch(psCertExpiryDate.Value);

                        Constants.s_psSysMonTraceSource.DebugAdminLogV2Message(
                            "psCertExpiryEpochTime : {0}", psCertExpiryEpochTime);

                        certExpiryDict.Add("psCertExpiry", psCertExpiryEpochTime);

                        TaskUtils.RunTaskForAsyncOperation(
                            traceSource: Constants.s_psSysMonTraceSource,
                            name: nameof(VerifyAndSendCertExpiryAlertsAsync),
                            ct: ct,
                            operation: async (taskCancToken) =>
                            await VerifyAndSendCertExpiryAlertsAsync(
                                    certExpiryTime: psCertExpiryDate.Value,
                                    csType: csType,
                                    hostId: hostId,
                                    hostName: hostName,
                                    ct: ct)).ConfigureAwait(false);
                    }
                }
            }, Constants.s_psSysMonTraceSource);

            if (certExpiryDict.Count != 0)
            {
                return TaskUtils.RunTaskForAsyncOperation(
                    traceSource: Constants.s_psSysMonTraceSource,
                    name: $"{nameof(MonitorSystem)} - UpdateCertExpiryInfoAsync",
                    ct: ct,
                    operation: async (taskCancToken) =>
                    await psApi.UpdateCertExpiryInfoAsync(
                    certExpiryDict,
                    taskCancToken).ConfigureAwait(false));
            }
            else
            {
                return null;
            }
        }

        private async Task VerifyAndSendCertExpiryAlertsAsync(
            DateTime? certExpiryTime,
            CXType csType,
            string hostId,
            string hostName,
            CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            var currentTimeInUtc = DateTime.UtcNow;

            // Get difference in cert expiry time and current time in days
            var certDiffDays = (int)(certExpiryTime - currentTimeInUtc).Value.TotalDays;
            bool certDiffWeekly = (certDiffDays < 30 && certDiffDays % 7 == 0) ? true : false;

            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                TraceEventType.Information,
                "CertDiffDays : {0}, CertDiffWeekly : {1}",
                certDiffDays,
                certDiffWeekly);

            // Generate alerts in the following cases:
            // In case of certExpiryDate is in between 2 months to 1 month generate alert for every 15 days.
            // In case of certExpiryDate less than 1 month generate alert weekly once.
            // In case of certExpiryDate less than 1 week generate alert daily.
            // TODO: Move the threshold limits to settings
            if (certDiffWeekly || certDiffDays == 60 || certDiffDays == 45 || certDiffDays < 7)
            {
                string errCode;
                string errId;
                string errMessage;
                string errSummary;
                var placeholders = new Dictionary<string, object>();

                errId = hostId;
                placeholders.Add("CsName", hostName);

                if (certDiffDays <= 0)
                {
                    errSummary = "SSL Cert Expired";
                    errMessage = string.Format(
                        CultureInfo.InvariantCulture,
                        "The SSL certificate on {0} has expired.",
                        hostName);
                    errCode = "EA0615";
                }
                else
                {
                    errSummary = "SSL Cert Expiry Warning";
                    errMessage = string.Format(CultureInfo.InvariantCulture,
                        "The SSL certificate on {0} is going to expire in {1} days.",
                        hostName, certDiffDays);
                    placeholders.Add("NoOfDays", certDiffDays);
                    errCode = "EA0616";
                }

                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "Sending {0} alert. {1}",
                    errSummary,
                    errMessage);

                var healthEvent = new PSHealthEvent(
                    errorCode: errCode,
                    errorSummary: errSummary,
                    errorMessage: errMessage,
                    errorPlaceHolders: placeholders,
                    errorTemplateId: "SSL_CERT_ALERTS",
                    hostId: hostId,
                    errorId: hostId);

                await psApi.ReportPSEventsAsync(healthEvent, ct).ConfigureAwait(false);
            }
        }

        private Task GetPSHealthEvents(CancellationToken cancellationToken)
        {
            var monitoringTasks = new List<Task>();

            if (PSInstallationInfo.Default.CSMode == CSMode.Rcm)
            {
                ApplianceHealthEventMsg psEvents = GetPSEvents();

                if (psEvents.ApplianceHealthEvents.Count != 0)
                {
                    monitoringTasks.Add(TaskUtils.RunTaskForAsyncOperation(
                        traceSource: Constants.s_psSysMonTraceSource,
                        name: $"{nameof(GetPSHealthEvents)} - RCM PS",
                        ct: cancellationToken,
                        operation: async (taskCancToken) =>
                        await psApi.ReportPSEventsAsync(
                            psEvents,
                            taskCancToken)
                            .ConfigureAwait(false)));
                }
            }
            else if (PSInstallationInfo.Default.CSMode == CSMode.LegacyCS)
            {
                var psHealthEvents = GetV2APSEvents();

                if (psHealthEvents != null && psHealthEvents.Count > 0)
                {
                    foreach (var psHealthEvent in psHealthEvents)
                    {
                        monitoringTasks.Add(TaskUtils.RunTaskForAsyncOperation(
                            traceSource: Constants.s_psSysMonTraceSource,
                            name: $"{nameof(GetPSHealthEvents)} - V2A PS",
                            ct: cancellationToken,
                            operation: async (taskCancToken) =>
                            await psApi.ReportPSEventsAsync(
                                psHealthEvent,
                                taskCancToken)
                                .ConfigureAwait(false)));
                    }
                }
            }

            return Task.WhenAll(monitoringTasks);
        }

        private ApplianceHealthEventMsg GetPSEvents()
        {
            ApplianceHealthEventMsg psEvents = new ApplianceHealthEventMsg();

            TaskUtils.RunAndIgnoreException(() =>
            {
                //Monitor reboot
                string currRebootTime = GetPSRebootTime();
                if (!string.IsNullOrWhiteSpace(currRebootTime))
                {
                    psEvents.ApplianceHealthEvents.Add(GetPSRebootEvent(currRebootTime));
                }

                string upgradedVersion = GetPSUpgradedVersion();
                if (!string.IsNullOrWhiteSpace(upgradedVersion))
                {
                    psEvents.ApplianceHealthEvents.Add(
                        GetVersionUpgradeEvent(PSInstallationInfo.Default.GetMonitoringLastPSVersion(), upgradedVersion));
                }
            }, Constants.s_psSysMonTraceSource);

            return psEvents;
        }

        private static void UpdatePSEventsRelatedInfo()
        {
            PSInstallationInfo.Default.SetMonitoringUpgradedPSVersion(
                PSInstallationInfo.Default.GetPSCurrentVersion());
            PSInstallationInfo.Default.SetMonitoringLastRebootTime(
                SystemUtils.GetSystemBootUpTimeLocal().Value.ToUniversalTime().Ticks);
        }

        public ApplianceHealthEvent GetPSRebootEvent(string lastRebootTime)
        {
            PSRebootPlaceholders pSRebootPlaceholders = new PSRebootPlaceholders
            {
                SystemBootTime = lastRebootTime,
                HostName = SystemUtils.GetFqdn()
            };

            return new ApplianceHealthEvent("Reboot",
                                            Severity.Information.ToString(),
                                            MonitoringMessageSource.ProcessServer.ToString(),
                                            DateTime.UtcNow.Ticks,
                                            MiscUtils.PlaceholderToDictionary(pSRebootPlaceholders));
        }

        public ApplianceHealthEvent GetVersionUpgradeEvent(string preVersion, string currVersion)
        {
            PSVersionUpgradePlaceholders pSVersionUpgradePlaceholders =
                new PSVersionUpgradePlaceholders
                {
                    PSVersion = currVersion,
                    PreviousVersion = preVersion,
                    HostName = SystemUtils.GetFqdn()
                };

            return new ApplianceHealthEvent("Upgrade",
                                            Severity.Information.ToString(),
                                            MonitoringMessageSource.ProcessServer.ToString(),
                                            DateTime.UtcNow.Ticks,
                                            MiscUtils.PlaceholderToDictionary(pSVersionUpgradePlaceholders));
        }

        private static string GetPSRebootTime()
        {
            string currRebootTimeStr = null;

            TaskUtils.RunAndIgnoreException(() =>
            {
                DateTime? currentRebootTime = SystemUtils.GetSystemBootUpTimeLocal();

                if (currentRebootTime.HasValue)
                {
                    long lastRebootTimeTicks = PSInstallationInfo.Default.GetMonitoringLastRebootTime();
                    var currentRebootTimeUtc = currentRebootTime.Value.ToUniversalTime();
                    if (lastRebootTimeTicks != 0 && currentRebootTimeUtc.Ticks != lastRebootTimeTicks)
                    {
                        currRebootTimeStr = currentRebootTime.ToString();
                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Server has come up after a reboot or shutdown." +
                            " Last Reboot Time : {0}, Current Reboot Time : {1}",
                            new DateTime(lastRebootTimeTicks),
                            currRebootTimeStr);
                    }
                }
            });

            return currRebootTimeStr;
        }

        private static string GetPSUpgradedVersion()
        {
            string upgradedVersion = null;

            TaskUtils.RunAndIgnoreException(() =>
            {
                string prevPSVersion = PSInstallationInfo.Default.GetMonitoringLastPSVersion();
                string currPSVersion = PSInstallationInfo.Default.GetPSCurrentVersion();

                if (!string.IsNullOrWhiteSpace(prevPSVersion) && !currPSVersion.Equals(prevPSVersion))
                {
                    upgradedVersion = currPSVersion;
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "ProcessServer is upgraded from {0} to {1}",
                        prevPSVersion,
                        upgradedVersion);
                }
            });

            return upgradedVersion;
        }

        /// <summary>
        /// Updates registry with the monitoring file contents,
        /// if the file exists and registry values doesn't exist
        /// </summary>
        private static void UpdateV2APSEventsInfoInRegistry(CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            string monitorFilesDir = PSInstallationInfo.Default.GetMonitorFolderPath();

            string psRebootFilePath = FSUtils.GetLongPath(
                Path.Combine(monitorFilesDir, "ps_reboot_time_persist.txt"),
                isFile: true,
                optimalPath: false);

            long lastRebootTime = 0;
            try
            {
                lastRebootTime = PSInstallationInfo.Default.GetMonitoringLastRebootTime();
            }
            catch (Exception ex)
            {
                // Ignoring the exception
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failed to get PS last reboot time with error {0}{1}",
                    Environment.NewLine,
                    ex);
            }

            // If monitoring file exist and registry value doesn't exist,
            // updates registry with file content and deletes the file
            if (lastRebootTime == 0 &&
                FSUtils.IsNonEmptyFile(psRebootFilePath))
            {
                PSInstallationInfo.Default.SetMonitoringLastRebootTime(
                    ManagementDateTimeConverter.ToDateTime(
                        File.ReadAllText(psRebootFilePath)).ToUniversalTime().Ticks);

                File.Delete(psRebootFilePath);
            }
            // Deleting the stale file to handle the cases where registry update succeeds
            // but file deletion fails due to system crash or any other failure during previous run
            else if (lastRebootTime != 0 && File.Exists(psRebootFilePath))
            {
                File.Delete(psRebootFilePath);
            }

            string psVersionFilePath = FSUtils.GetLongPath(
                Path.Combine(monitorFilesDir, "ps_version_info_persist.txt"),
                isFile: true,
                optimalPath: false);

            string lastPSVersion = null;
            try
            {
                lastPSVersion = PSInstallationInfo.Default.GetMonitoringLastPSVersion();
            }
            catch (Exception ex)
            {
                // Ignoring the exception
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failed to get last PS version with error {0}{1}",
                    Environment.NewLine,
                    ex);
            }

            if (string.IsNullOrWhiteSpace(lastPSVersion) &&
                FSUtils.IsNonEmptyFile(psVersionFilePath))
            {
                PSInstallationInfo.Default.SetMonitoringUpgradedPSVersion(
                    File.ReadAllText(psVersionFilePath));

                File.Delete(psVersionFilePath);
            }
            else if (!string.IsNullOrWhiteSpace(lastPSVersion) &&
                File.Exists(psRebootFilePath))
            {
                File.Delete(psRebootFilePath);
            }
        }

        private static IList<PSHealthEvent> GetV2APSEvents()
        {
            const string errorTemplateId = "PS_ERROR";
            const string errorSummary = "Process Server Has Logged Alerts";

            var psEvents = new List<PSHealthEvent>();

            TaskUtils.RunAndIgnoreException(() =>
            {
                var psHostName = SystemUtils.GetFqdn();
                var psHostId = PSInstallationInfo.Default.GetPSHostId();
                var psIPAddress =
                PSInstallationInfo.Default.GetPSIPAddress().ToString();

                //Monitor PS Reboot
                string currRebootTime = GetPSRebootTime();
                if (!string.IsNullOrWhiteSpace(currRebootTime))
                {
                    string errorMessage = string.Format(
                        CultureInfo.InvariantCulture,
                        "Server has come up at {0} after a reboot or shutdown on {1}({2})",
                        currRebootTime,
                        psHostName,
                        psIPAddress);

                    Constants.s_psSysMonTraceSource.DebugAdminLogV2Message(errorMessage);

                    V2APSRebootPlaceholders psRebootPlaceholders = new V2APSRebootPlaceholders
                    {
                        SystemBootTime = currRebootTime,
                        HostName = psHostName,
                        IPAddress = psIPAddress
                    };

                    // TODO : Move error codes to a new class.
                    psEvents.Add(new PSHealthEvent(
                        errorCode: "EC0114",
                        errorSummary: errorSummary,
                        errorMessage: errorMessage,
                        errorPlaceHolders: psRebootPlaceholders,
                        errorTemplateId: errorTemplateId,
                        hostId: psHostId,
                        errorId: psHostId));
                }

                // Monitor PS Version Upgrade
                string upgradedVersion = GetPSUpgradedVersion();
                if (!string.IsNullOrWhiteSpace(upgradedVersion))
                {
                    string upgradeErrorMessage = string.Format(
                        CultureInfo.InvariantCulture,
                        "Process Server is upgraded to {0} on {1}({2})",
                        upgradedVersion,
                        psHostName,
                        psIPAddress);

                    Constants.s_psSysMonTraceSource.DebugAdminLogV2Message(upgradeErrorMessage);

                    V2APSVersionUpgradePlaceholders psVersionUpgradePlaceholders =
                        new V2APSVersionUpgradePlaceholders
                        {
                            PSVersion = upgradedVersion,
                            HostName = psHostName,
                            IPAddress = psIPAddress
                        };

                    psEvents.Add(new PSHealthEvent(
                        errorCode: "EC0115",
                        errorSummary: errorSummary,
                        errorMessage: upgradeErrorMessage,
                        errorPlaceHolders: psVersionUpgradePlaceholders,
                        errorTemplateId: errorTemplateId,
                        hostId: psHostId,
                        errorId: psHostId));
                }
            }, Constants.s_psSysMonTraceSource);

            return psEvents;
        }

        /// <summary>
        /// Gets service list for appliance.
        /// </summary>
        /// <param name="psServicesList">List of services for PS.</param>
        /// <param name="csServicesList">List of services for CS.</param>
        /// <returns></returns>
        private static List<string> GetServiceList(List<string> psServicesList, List<string> csServicesList)
        {
            if (PSInstallationInfo.Default.CSMode == CSMode.LegacyCS)
            {
                return PSInstallationInfo.Default.GetCSType() == CXType.CSAndPS ?
                psServicesList.Concat(csServicesList).ToList() :
                psServicesList;
            }
            else
            {
                //Todo:[Himanshu] populate the service lists in case of rcm.
                return new List<string>();
            }
        }

        private ProcessServerStatistics GeneratePSStatistics(
            CircularList<SystemPerfContainer> statsContainer,
            (long UploadPendingData, long ThroughputBytesPerSec, StatisticStatus Status) cachedThroughputStat,
            IDictionary<string, int?> serviceInstances)
        {
            var (_, psStatistics) = this.GetPerfHealth(statsContainer, logHealth: false);

            psStatistics.Throughput = cachedThroughputStat;

            var svcList = serviceInstances?.
                Where(svcKVPair => svcKVPair.Value.HasValue).
                Select(svcKVPair => (ServiceName: svcKVPair.Key, NumberOfInstances: svcKVPair.Value.Value)).
                ToList();

            var svcNameRemapLookup = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["InMage PushInstall"] = "pushinstalld",
                ["cxprocessserver"] = "cxps"
            };

            psStatistics.Services = svcList?.Select(curr =>
                    svcNameRemapLookup.TryGetValue(curr.ServiceName, out string remapSvcName) ?
                    (remapSvcName, curr.NumberOfInstances) : curr).
                ToList();

            return psStatistics;
        }

        /// <summary>
        /// Get list of services
        /// </summary>
        /// <param name="services">Services and monitoring status</param>
        /// <returns>services list</returns>
        private static List<string> GetServiceList(string services)
        {
            List<string> serviceList = new List<string>();

            try
            {
                // Parse services json string and get all the services that are in "ON" state
                var data =
                    JsonConvert.DeserializeObject<Dictionary<string, string>>(
                    services);
                serviceList = data.Where(
                    item => item.Value.Equals(
                        "ON", StringComparison.OrdinalIgnoreCase))
                    .Select(item => item.Key).ToList();
            }
            catch (Exception ex)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to parse services from json string {0} with error : {1}{2}",
                        services,
                        Environment.NewLine,
                        ex);
            }

            return serviceList;
        }

        /// <summary>
        /// Parse settings and get configured intervals.
        /// </summary>
        /// <param name="numOfPerfSamples">Number of perf samples</param>
        /// <param name="numOfUploadedDataSamples">Number of upload data samples</param>
        /// <param name="numOfStatsSamples">Number of perf stats samples</param>
        /// <param name="psServicesList">List of PS services.</param>
        /// <param name="csServicesList">List of CS services.</param>
        /// <returns>true if inputs are valid, otherwise false</returns>
        private static bool ParseSettings(
            out int numOfPerfSamples,
            out int numOfUploadedDataSamples,
            out int numOfStatsSamples,
            out List<string> psServicesList,
            out List<string> csServicesList)
        {
            numOfPerfSamples = 0;
            numOfUploadedDataSamples = 0;
            numOfStatsSamples = 0;
            psServicesList = new List<string>();
            csServicesList = new List<string>();

            try
            {
                bool monitorServices = Settings.Default.MonitorServiceStatus;
                bool monitorPerf = Settings.Default.MonitorPerfStatistics;
                bool monitorThroughput = Settings.Default.MonitorThroughputStats;
                bool reportStatistics = Settings.Default.ReportStatistics;

                if (!monitorPerf && !monitorServices && !monitorThroughput && !reportStatistics)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Monitoring is OFF for all stats");

                    return false;
                }

                TimeSpan healthReportInterval = Settings.Default.HealthReportInterval;
                TimeSpan healthMonitorInterval = Settings.Default.HealthMonitorInterval;
                TimeSpan statsReportInterval = Settings.Default.StatisticsReportInterval;
                TimeSpan statsMonitorInterval = Settings.Default.StatisticsMonitorInterval;

                if (healthReportInterval <= TimeSpan.Zero)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Invalid health report interval {0} is configured",
                        healthReportInterval);

                    return false;
                }

                if (healthMonitorInterval <= TimeSpan.Zero)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Invalid health monitoring interval {0} is configured",
                        healthMonitorInterval);

                    return false;
                }

                if (healthMonitorInterval <= healthReportInterval)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Health monitoring interval {0} should be greater than Health report interval : {1}",
                        healthMonitorInterval,
                        healthReportInterval);

                    return false;
                }

                if (statsReportInterval <= TimeSpan.Zero)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Invalid statistics reporting interval {0} is configured",
                        statsReportInterval);

                    return false;
                }

                TimeSpan perfMonitorInterval = Settings.Default.PerfMonitorInterval;

                if (!ValidateInterval(
                    monitorPerf, perfMonitorInterval, healthReportInterval, healthMonitorInterval))
                {
                    return false;
                }

                if (!ValidateInterval(
                    monitorServices, Settings.Default.ServiceMonitorInterval,
                    healthReportInterval, healthMonitorInterval))
                {
                    return false;
                }

                // Statistics are calculated and reported for the same duration,
                // since we need the latest data at every reporting.
                if (!ValidateInterval(
                    monitor: reportStatistics,
                    monitoringInterval: statsMonitorInterval,
                    healthReportInterval: statsReportInterval,
                    healthMonitorInterval: statsReportInterval))
                {
                    return false;
                }

                if (monitorServices || reportStatistics)
                {
                    psServicesList = GetServiceList(Settings.Default.PSServices);
                    csServicesList = GetServiceList(Settings.Default.CSServices);

                    if (psServicesList == null || psServicesList.Count <= 0 ||
                        csServicesList == null || csServicesList.Count <= 0)
                    {
                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "PS (or) CS services are not configured for monitoring");

                        if (!monitorPerf && !monitorThroughput && !reportStatistics)
                        {
                            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Information,
                                "Monitoring is OFF for all PS stats");

                            return false;
                        }
                    }
                }

                numOfPerfSamples = Convert.ToInt32(
                    healthMonitorInterval.TotalSeconds / perfMonitorInterval.TotalSeconds);

                numOfUploadedDataSamples = Convert.ToInt32(
                    healthMonitorInterval.TotalSeconds / healthReportInterval.TotalSeconds);

                numOfStatsSamples = Convert.ToInt32(
                    statsReportInterval.TotalSeconds / statsMonitorInterval.TotalSeconds);
            }
            catch (Exception ex)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to parse ProcessServerMonitor settings with error : {0}{1}",
                        Environment.NewLine,
                        ex);
                return false;
            }

            return true;
        }

        /// <summary>
        /// Validate inputs.
        /// </summary>
        /// <param name="monitor">Monitoring is set/reset.</param>
        /// <param name="monitoringInterval">Perf/Service monitoring interval.</param>
        /// <param name="healthReportInterval">Health reporting interval.</param>
        /// <param name="healthMonitorInterval">System health monitoring interval.</param>
        /// <returns>True if validation succeeds otherwise false.</returns>
        private static bool ValidateInterval(
            bool monitor,
            TimeSpan monitoringInterval,
            TimeSpan healthReportInterval,
            TimeSpan healthMonitorInterval)
        {
            if (!monitor)
                return true;

            if (monitoringInterval <= TimeSpan.Zero)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Invalid monitoring interval {0} is configured",
                    monitoringInterval);

                return false;
            }

            if (monitoringInterval >= healthMonitorInterval)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Monitoring interval {0} should be less than system health monitoring interval {1}",
                    monitoringInterval,
                    healthMonitorInterval);

                return false;
            }

            if (monitoringInterval >= healthReportInterval)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Monitoring interval {0} should be less than health reporting interval {1}",
                    monitoringInterval,
                    healthReportInterval);

                return false;
            }

            return true;
        }

        /// <summary>
        /// Get service status
        /// </summary>
        /// <param name="serviceName">Service Name</param>
        /// <returns>True if service is running, otherwise false.</returns>
        private static bool GetServiceStatus(string serviceName)
        {
            bool isRunning = false;

            using (ServiceController controller =
                new ServiceController(serviceName))
            {
                try
                {
                    ServiceControllerStatus status = controller.Status;
                    if (status == ServiceControllerStatus.Running)
                    {
                        isRunning = true;
                    }
                }
                catch (InvalidOperationException ioex)
                {
                    // InvalidOperationException will be thrown if service is not installed.
                    // Not throwing the exception, returning false to set health error.
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to fetch service {0} status with error : {1}{2}",
                        serviceName,
                        Environment.NewLine,
                        ioex);
                }
                catch (Exception ex)
                {
                    // ignoring any other exception.
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Error occured while fetching {0} status : {1}{2}",
                        serviceName,
                        Environment.NewLine,
                        ex);
                    throw;
                }
            }

            return isRunning;
        }

        /// <summary>
        /// Initialize perf counters and psapi stub.
        /// </summary>
        private StringBuilder Init()
        {
            StringBuilder errorStr = new StringBuilder();

            if (psApi == null)
            {
                try
                {
                    psApi = MonitoringApiFactory.GetPSMonitoringApiStubs();
                }
                catch (Exception ex)
                {
                    errorStr.AppendFormat("PSMonitoringApiStubs Error - {0}{1}", ex.Message, Environment.NewLine);
                }
            }

            if (psMonitor == null)
            {
                try
                {
                    psMonitor = new ProcessServerMonitoringStubsImpl();
                }
                catch(Exception ex)
                {
                    errorStr.AppendFormat("PSMonitoringSubs Error - {0}{1}", ex.Message, Environment.NewLine);
                }
            }

            if (cpuCounter == null)
            {
                try
                {
                    cpuCounter =
                    new PerformanceCounter("Processor", "% Processor Time", "_Total");
                }
                catch (Exception ex)
                {
                    errorStr.AppendFormat("Processor(% Processor Time) error - {0}{1}", ex.Message, Environment.NewLine);
                }
            }

            if (memoryPercentInUseCounter == null)
            {
                try
                {
                    memoryPercentInUseCounter =
                    new PerformanceCounter("Memory", "% Committed Bytes In Use");
                }
                catch (Exception ex)
                {
                    errorStr.AppendFormat("Memory(Committed Bytes In Use) error - {0}{1}", ex.Message, Environment.NewLine);
                }
            }

            if (memoryCommitLimitCounter == null)
            {
                try
                {
                    memoryCommitLimitCounter =
                    new PerformanceCounter("Memory", "Commit Limit");
                }
                catch (Exception ex)
                {
                    errorStr.AppendFormat("Memory(Commit Limit) error - {0}{1}", ex.Message, Environment.NewLine);
                }
            }

            if (procQueueLenCounter == null)
            {
                try
                {
                    procQueueLenCounter = new PerformanceCounter("System", "Processor Queue Length");
                }
                catch (Exception ex)
                {
                    errorStr.AppendFormat("System(Processor Queue Length) error - {0}{1}", ex.Message, Environment.NewLine);
                }
            }

            return errorStr;
        }

        /// <summary>
        /// Delete all the previously generated health factors if perf/service monitoring is OFF.
        /// </summary>
        /// <param name="token">Cancellation token</param>
        /// <returns>Health reporting task (if applicable)</returns>
        private Task ResetHealthAsync(CancellationToken token)
        {
            var infraHealthFactorItems = new List<InfraHealthFactorItem>();

            try
            {  
                GetInfraHealthFactorsForResetHealth(infraHealthFactorItems: infraHealthFactorItems);
                // Even if it is empty still needs to be called as in RCM, empty health would be used to reset.
                if (infraHealthFactorItems != null)
                {
                    token.ThrowIfCancellationRequested();

                    return Task.Run(
                        async () => await this.ReportInfraHealthErrors(infraHealthFactorItems, token).ConfigureAwait(false),
                        token);
                }
            }
            catch (OperationCanceledException) when (token.IsCancellationRequested)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    "ResetHealth task is cancelled");
                throw;
            }
            catch (Exception ex)
            {
                // ignoring the exception
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failed to reset health with error : {0}{1}",
                    Environment.NewLine,
                    ex);
                throw;
            }

            return Task.CompletedTask;
        }

        /// <summary>
        /// If legacy mode then populate health factos with reset values,
        /// else leave empty as in new stack empty call would reset everything.
        /// in case of RCM not sending the health alerts will reset them.
        /// </summary>
        /// <param name="infraHealthFactorItems">Health factors to be populated.</param>
        private void GetInfraHealthFactorsForResetHealth(List<InfraHealthFactorItem> infraHealthFactorItems)
        {
            if (PSInstallationInfo.Default.CSMode == CSMode.LegacyCS)
            {
                bool resetEvent = false;
                object placeHolders = null;
                if (!Settings.Default.MonitorPerfStatistics)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Resetting perf health factors as monitoring is OFF");
                    var perfHealthFactorItems = new List<InfraHealthFactorItem>
                    {
                            new InfraHealthFactorItem(
                                InfraHealthFactors.CpuCritical, resetEvent, placeHolders),
                            new InfraHealthFactorItem(
                                InfraHealthFactors.CpuWarning, resetEvent, placeHolders),
                            new InfraHealthFactorItem(
                                InfraHealthFactors.MemoryCritical, resetEvent, placeHolders),
                            new InfraHealthFactorItem(
                                InfraHealthFactors.MemoryWarning, resetEvent, placeHolders),
                            new InfraHealthFactorItem(
                                InfraHealthFactors.DiskCritical, resetEvent, placeHolders),
                            new InfraHealthFactorItem(
                                InfraHealthFactors.DiskWarning, resetEvent, placeHolders)
                    };

                    infraHealthFactorItems.AddRange(perfHealthFactorItems);
                }

                if (!Settings.Default.MonitorServiceStatus)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Resetting service health factors as monitoring is off");
                    infraHealthFactorItems.Add(
                        new InfraHealthFactorItem(
                            InfraHealthFactors.ServicesNotRunning,
                            resetEvent,
                            placeHolders));
                }

                if (!Settings.Default.MonitorThroughputStats)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Resetting throughput health factors as monitoring is off");
                    var tpHealthFactors = this.ResetThroughPutHealth();

                    infraHealthFactorItems.AddRange(tpHealthFactors);
                }
            }
        }

        /// <summary>
        /// This routine is used to collect system statistics.
        /// </summary>
        /// <returns>Returns SystemPerfContainer</returns>
        private SystemPerfContainer CollectSystemStats()
        {
            int cpuLoad = this.GetCPULoad();
            double freeSpacePercent = FSUtils.GetFreeSpace(PSInstallationInfo.Default.GetReplicationLogFolderPath(), out ulong freeSpace, out ulong totalSpace);
            double usedMemPercent = this.GetPercentOfMemoryUsage();
            long totalVirtualMemorySize = this.GetTotalVirtualMemorySize();
            long procQueueLen = this.GetProcessorQueueLength();

            var perf = new SystemPerfContainer
            {
                CpuLoad = cpuLoad,
                FreeSpace = freeSpace,
                TotalSpace = totalSpace,
                FreeSpacePercent = freeSpacePercent,
                UsedMemoryPercent = usedMemPercent,
                TotalVirtualMemorySize = totalVirtualMemorySize,
                ProcessorQueueLength = procQueueLen
            };

            return perf;
        }

        /// <summary>
        /// This routine is used to get the cpu usage load of the system.
        /// </summary>
        /// <returns>Returns CPULoad for success. -1 in case of exception</returns>
        private int GetCPULoad()
        {
            int cpuLoad = -1;

            try
            {
                if (cpuCounter != null)
                {
                    cpuLoad = Convert.ToInt32(Math.Round(cpuCounter.NextValue()));
                }
            }
            catch (Exception ex)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Ignoring error, failed to get cpu load with error : {0}{1}",
                        Environment.NewLine,
                        ex);
            }

            return cpuLoad;
        }

        /// <summary>
        /// This routine is used to get the processor queue length of the system.
        /// </summary>
        /// <returns>Returns ProcessorQueueLength for success. -1 in case of exception</returns>
        private long GetProcessorQueueLength()
        {
            long procQueueLength = -1;

            try
            {
                if (procQueueLenCounter != null)
                {
                    // TODO-SanKumar-1906: Currently processor queue length is
                    // compared against static limits (6 & 12). With any number of
                    // cores, this would be a single perf counter, which means that
                    // the waiting threads could be executed simultaneously between
                    // multiple cores. So we might've to take the number of logical
                    // cores in the system.
                    procQueueLength = Convert.ToInt64(procQueueLenCounter.NextValue());
                }
            }
            catch (Exception ex)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Ignoring error, failed to get processor queue length with error : {0}{1}",
                        Environment.NewLine,
                        ex);
            }

            return procQueueLength;
        }

        /// <summary>
        /// This routine is used to get the PS virtual memory usage.
        /// </summary>
        /// <returns>Used Memory Percentage.</returns>
        private double GetPercentOfMemoryUsage()
        {
            double usedMemoryPercent = -1;

            try
            {
                if (memoryPercentInUseCounter != null)
                {
                    usedMemoryPercent =
                        Convert.ToDouble(Math.Round(memoryPercentInUseCounter.NextValue(), 2));
                }
            }
            catch (Exception ex)
            {
                // ignoring the exception
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to get memory usage with error : {0}{1}",
                        Environment.NewLine,
                        ex);
            }

            return usedMemoryPercent;
        }

        /// <summary>
        /// This routine is used to get the total virtual memory size of the system.
        /// </summary>
        /// <returns>Total virtual memory size, in bytes.</returns>
        private long GetTotalVirtualMemorySize()
        {
            long totalVirtualMemory = -1;

            try
            {
                if (memoryCommitLimitCounter != null)
                {
                    totalVirtualMemory = Convert.ToInt64(memoryCommitLimitCounter.NextValue());
                }
            }
            catch (Exception ex)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Ignoring error, failed to get total virtual memory size with error : {0}{1}",
                        Environment.NewLine,
                        ex);
            }

            return totalVirtualMemory;
        }

        /// <summary>
        /// This routine is used to get the average of the system perf statistics.
        /// </summary>
        /// <param name="perfStatistics">List of system perf statistics.</param>
        /// <returns>Average of the system perf statistics.</returns>
        private SystemPerfContainer GetPerfStatistics(
            CircularList<SystemPerfContainer> perfStatistics)
        {
            SystemPerfContainer systemPerf = new SystemPerfContainer();

            if (perfStatistics == null || perfStatistics.Count <= 0)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Perf Statistics is empty");

                return null;
            }

            try
            {
                long procQueueLen = 0, procQueueLenCnt = 0;
                int cpuload = 0, cpuCnt = 0;
                long totalVirtualMemory = 0, totVirtMemCnt = 0;
                double usedMemoryPercent = 0, freeSpacePercent = 0;
                double defaultVal = -1;
                ulong freeSpace = 0, totalSpace = 0;
                ulong usedMemCounter = 0, spaceCounter = 0;

                foreach (var perf in perfStatistics)
                {
                    if (perf.ProcessorQueueLength != -1)
                    {
                        procQueueLen += perf.ProcessorQueueLength;
                        procQueueLenCnt++;
                    }

                    // Ignoring the unset values in the perf list while calculating average.
                    if (perf.CpuLoad != -1)
                    {
                        cpuload += perf.CpuLoad;
                        cpuCnt++;
                    }

                    if (!perf.FreeSpacePercent.Equals(defaultVal))
                    {
                        freeSpace += perf.FreeSpace;
                        totalSpace += perf.TotalSpace;
                        freeSpacePercent += perf.FreeSpacePercent;
                        spaceCounter++;
                    }

                    if (!perf.UsedMemoryPercent.Equals(defaultVal))
                    {
                        usedMemoryPercent += perf.UsedMemoryPercent;
                        usedMemCounter++;
                    }

                    if (perf.TotalVirtualMemorySize != -1)
                    {
                        totalVirtualMemory += perf.TotalVirtualMemorySize;
                        totVirtMemCnt++;
                    }
                }

                long procQueueLenAvg = -1;
                int cpuLoad = -1;
                long totalVirtualMemoryAvg = -1;
                ulong freeSpaceInBytes = 0, totalSpaceInBytes = 0;
                double availableSpacePercent = -1, memUsagePercent = -1;

                if (procQueueLenCnt != 0)
                {
                    procQueueLenAvg = procQueueLen / procQueueLenCnt;
                }

                if (cpuCnt != 0)
                {
                    cpuLoad = cpuload / cpuCnt;
                }

                if (spaceCounter != 0)
                {
                    freeSpaceInBytes = freeSpace / spaceCounter;
                    totalSpaceInBytes = totalSpace / spaceCounter;
                    availableSpacePercent =
                        Math.Round(freeSpacePercent / spaceCounter, 2);
                }

                if (usedMemCounter != 0)
                {
                    memUsagePercent =
                        Math.Round(usedMemoryPercent / usedMemCounter, 2);
                }

                if (totVirtMemCnt != 0)
                {
                    totalVirtualMemoryAvg = totalVirtualMemory / totVirtMemCnt;
                }

                systemPerf.ProcessorQueueLength = procQueueLenAvg;
                systemPerf.CpuLoad = cpuLoad;
                systemPerf.FreeSpace = freeSpaceInBytes;
                systemPerf.TotalSpace = totalSpaceInBytes;
                systemPerf.FreeSpacePercent = availableSpacePercent;
                systemPerf.UsedMemoryPercent = memUsagePercent;
                systemPerf.TotalVirtualMemorySize = totalVirtualMemoryAvg;

                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Avg Processor Queue Length : {0}",
                        systemPerf.ProcessorQueueLength);
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Avg CPU Load : {0}",
                        systemPerf.CpuLoad);
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Avg FressSpace : {0}",
                        systemPerf.FreeSpace);
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Avg TotalSpace : {0}",
                        systemPerf.TotalSpace);
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Avg FreeSpacePercent : {0}",
                        systemPerf.FreeSpacePercent);
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Avg MemoryUsagePercent : {0}",
                        systemPerf.UsedMemoryPercent);
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Avg TotalVirtualMemorySize : {0}",
                        systemPerf.TotalVirtualMemorySize);
            }
            catch (Exception ex)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to get average load with exception : {0}{1}",
                        Environment.NewLine,
                        ex);

                throw;
            }

            return systemPerf;
        }

        /// <summary>
        /// Get command line arguments for running process
        /// </summary>
        /// <param name="processName">Process Name</param>
        /// <param name="commandStr">CommandLine param</param>
        /// <returns>list of command line arguments</returns>
        private List<string> GetCommandLine(
            string processName,
            string commandStr)
        {
            List<string> results = new List<string>();

            try
            {
                string wmiQuery =
                    string.Format(
                    @"SELECT Name, CommandLine FROM Win32_Process WHERE Name = '{0}' AND CommandLine LIKE '{1}'",
                    processName,
                    commandStr);
                SelectQuery query = new SelectQuery(wmiQuery);

                using (ManagementObjectSearcher searcher =
                    new ManagementObjectSearcher(query))
                {
                    var processes = searcher.Get();
                    if (processes == null || processes.Count <= 0)
                    {
                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Service {0} is not running",
                        processName);

                        return null;
                    }

                    foreach (ManagementObject commandLineObject in processes)
                    {
                        string commandLine =
                            (string)commandLineObject["CommandLine"];
                        results.Add(commandLine);
                    }
                }
            }
            catch (Exception ex)
            {
                // Rethrowing exception
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to fetch service {0} status with exception : {1}{2}",
                        processName,
                        Environment.NewLine,
                        ex);
                throw;
            }

            return results;
        }

        /// <summary>
        /// Monitors services health
        /// </summary>
        /// <param name="serviceList">List of services</param>
        /// <returns>Dictionary of services and their number of instances</returns>
        private IDictionary<string, int?> MonitorServices(List<string> serviceList)
        {
            var svcDict = new Dictionary<string, int?>(serviceList.Count);

            foreach (var service in serviceList)
            {
                int? numOfInstances = null;

                try
                {
                    // TODO: Sirisha: Remove service monitoring for monitor, gentrends, esx, volsync 
                    // once tmansvc functionality is moved to .net PS.
                    if (service.Equals(
                        "volsync", StringComparison.OrdinalIgnoreCase))
                    {
                        numOfInstances = expVolsyncChildrenCnt;
                    }
                    else if (service.StartsWith("monitor", StringComparison.OrdinalIgnoreCase) ||
                        service.Equals("gentrends", StringComparison.OrdinalIgnoreCase) ||
                        service.Equals("esx", StringComparison.OrdinalIgnoreCase))
                    {
                        string serviceDir = @"var\services";
                        string serviceFile = Path.Combine(
                            PSInstallationInfo.Default.GetRootFolderPath(),
                            serviceDir,
                            service);

                        if (service.Equals("monitor_disks", StringComparison.OrdinalIgnoreCase) ||
                            service.Equals("monitor", StringComparison.OrdinalIgnoreCase) ||
                            service.Equals("monitor_protection", StringComparison.OrdinalIgnoreCase) ||
                            service.Equals("monitor_ps", StringComparison.OrdinalIgnoreCase) ||
                            service.Equals("gentrends", StringComparison.OrdinalIgnoreCase) ||
                            service.Equals("esx", StringComparison.OrdinalIgnoreCase))
                            numOfInstances = 1;
                        else
                            numOfInstances = File.Exists(serviceFile) ? 1 : 0;
                    }
                    else
                    {
                        numOfInstances = GetServiceStatus(service) ? 1 : 0;
                    }
                }
                catch (Exception ex)
                {
                    // Rethrowing exception
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Exception occured when trying to fetch status of {0} service : {1}{2}",
                        service,
                        Environment.NewLine,
                        ex);

                    throw;
                }

                // isRunning will be null, if any exception occurs while fetching service status
                // ignoring the services having null value while setting health factor.

                if (!service.Equals("volsync", StringComparison.OrdinalIgnoreCase) &&
                    numOfInstances.HasValue && numOfInstances.Value == 0)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose, "{0} is stopped", service);
                }

                svcDict.Add(service, numOfInstances);
            }

            return svcDict;
        }

        /// <summary>
        /// Get Overall service health status
        /// </summary>
        /// <param name="serviceInstances">Map of services to their number of instances.</param>
        /// <param name="numOfServiceRecords">Service status samples count.</param>
        /// <returns>List of InfraHealthFactorItems</returns>
        private List<InfraHealthFactorItem> GetServiceHealth(
            List<string> notRunningServices,
            int numOfServiceRecords)
        {
            List<InfraHealthFactorItem> infraHealthFactors =
                new List<InfraHealthFactorItem>();

            try
            {
                ServiceNotRunningPlaceholders placeholders = null;
                bool setEvent = false;
                string processServerName = System.Net.Dns.GetHostName();

                if (notRunningServices != null && notRunningServices.Count > 0)
                {
                    var svcDict = notRunningServices.GroupBy(serviceName => serviceName).
                    ToDictionary(stoppedService => stoppedService.Key, stoppedService => stoppedService.Count());

                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Error,
                                    "Following services weren't running for mentioned times in the last {0} instances : {1}",
                                    numOfServiceRecords,
                                    string.Join(";", svcDict.Select(x => x.Key + "=" + x.Value)));

                    var overallStoppedServices = new List<string>();
                    foreach (var service in svcDict)
                    {
                        var stoppedServiceName = service.Key;
                        int stoppedServiceInstances = service.Value;

                        if (stoppedServiceInstances < numOfServiceRecords)
                        {
                            continue;
                        }

                        string serviceName;

                        // Send tmansvc as stopped if any of the ps perl processes are in stopped state.
                        if (stoppedServiceName.StartsWith("monitor", StringComparison.OrdinalIgnoreCase) ||
                            stoppedServiceName.Equals("gentrends", StringComparison.OrdinalIgnoreCase) ||
                            stoppedServiceName.Equals("esx", StringComparison.OrdinalIgnoreCase) ||
                            stoppedServiceName.Equals(
                            "volsync", StringComparison.OrdinalIgnoreCase))
                        {
                            serviceName = "tmansvc";
                        }
                        else
                        {
                            serviceName = stoppedServiceName;
                        }

                        if (!overallStoppedServices.Contains(serviceName))
                            overallStoppedServices.Add(serviceName);
                    }

                    if (overallStoppedServices != null && overallStoppedServices.Count > 0)
                    {
                        string services = string.Join(",", overallStoppedServices);
                        placeholders = new ServiceNotRunningPlaceholders
                        {
                            Services = services,
                            ProcessServer = processServerName
                        };

                        setEvent = true;
                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Set ServiceNotRunning health event as services {0} are not running on {1}",
                            services,
                            processServerName);
                    }
                }

                if (!setEvent)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Reset ServiceNotRunning health event on {0}",
                        processServerName);
                }

                infraHealthFactors.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.ServicesNotRunning, setEvent, placeholders));
            }
            catch (Exception ex)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Exception occured while trying to fetch service status : {0}{1}",
                        Environment.NewLine,
                        ex);
            }

            return infraHealthFactors;
        }

        /// <summary>
        /// This function is used to get system perf health.
        /// </summary>
        /// <param name="perfStatistics">System Perf Stats</param>
        /// <param name="logHealth">If true, log the computed health items</param>
        /// <returns>List of InfraHealthFactorItems and PS statistics</returns>
        private (List<InfraHealthFactorItem>, ProcessServerStatistics) GetPerfHealth(
            CircularList<SystemPerfContainer> perfStatistics, bool logHealth)
        {
            List<InfraHealthFactorItem> infraHealthFactorItems =
                new List<InfraHealthFactorItem>();

            var systemPerf = this.GetPerfStatistics(perfStatistics);
            if (systemPerf == null)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error, "System Perf Statistics is not found");

                return (infraHealthFactorItems, null);
            }

            const bool setAlert = true;
            const bool resetAlert = false;
            int healthMonitorInterval =
                Settings.Default.HealthMonitorInterval.Minutes;
            object nullPlaceHolders = null;

            ProcessServerStatistics psStats = new ProcessServerStatistics();

            #region System Load

            long procQueueLen =
                psStats.SystemLoad.ProcessorQueueLength = systemPerf.ProcessorQueueLength;

            if (procQueueLen == -1)
            {
                psStats.SystemLoad.Status = StatisticStatus.Unknown;
            }
            else if (procQueueLen >= Settings.Default.ProcessorQueueWarningThreshold &&
                procQueueLen < Settings.Default.ProcessorQueueCriticalThreshold)
            {
                psStats.SystemLoad.Status = StatisticStatus.Warning;
            }
            else if (systemPerf.ProcessorQueueLength >= Settings.Default.ProcessorQueueCriticalThreshold)
            {
                psStats.SystemLoad.Status = StatisticStatus.Critical;
            }
            else
            {
                psStats.SystemLoad.Status = StatisticStatus.Healthy;
            }

            #endregion System Load

            #region Cpu Load

            // Set/Reset CPU Utilization health
            int cpuLoad = systemPerf.CpuLoad;
            CpuHealthPlaceholders cpuPlaceholders = new CpuHealthPlaceholders
            {
                HealthMonitorInterval = healthMonitorInterval,
                CPUUtilization = cpuLoad
            };
            psStats.CpuLoad.Percentage = cpuLoad;

            if (cpuLoad >= Settings.Default.CPUWarningThreshold &&
                cpuLoad < Settings.Default.CPUCriticalThreshold)
            {
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.CpuCritical, resetAlert, nullPlaceHolders));
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.CpuWarning, setAlert, cpuPlaceholders));

                psStats.CpuLoad.Status = StatisticStatus.Warning;

                if (logHealth)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "Set cpu usage health to WARNING. Placeholders are {0}, {1}",
                                    cpuLoad,
                                    healthMonitorInterval);
                }
            }
            else if (cpuLoad >= Settings.Default.CPUCriticalThreshold)
            {
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.CpuWarning, resetAlert, nullPlaceHolders));
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.CpuCritical, setAlert, cpuPlaceholders));

                psStats.CpuLoad.Status = StatisticStatus.Critical;

                if (logHealth)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "Set cpu usage health to CRITICAL. Placeholders are {0}, {1}",
                                    cpuLoad,
                                    healthMonitorInterval);
                }
            }
            else
            {
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.CpuWarning, resetAlert, nullPlaceHolders));
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.CpuCritical, resetAlert, nullPlaceHolders));

                psStats.CpuLoad.Status = StatisticStatus.Healthy;

                if (logHealth)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "Reset CPU health. Used CPU : {0}%",
                                    cpuLoad
                                    );
                }
            }

            if (cpuLoad == -1)
            {
                psStats.CpuLoad.Status = StatisticStatus.Unknown;
            }

            #endregion Cpu Load

            #region Memory Usage

            // Set/Reset Memory Usage health
            double usedMemoryPercent = systemPerf.UsedMemoryPercent;
            MemoryHealthPlaceholders memPlaceholders = new MemoryHealthPlaceholders
            {
                HealthMonitorInterval = healthMonitorInterval,
                UsedMemoryPercent = usedMemoryPercent
            };

            psStats.MemoryUsage.Total = systemPerf.TotalVirtualMemorySize;

            if (usedMemoryPercent >= Settings.Default.MemoryWarningThreshold &&
                usedMemoryPercent < Settings.Default.MemoryCriticalThreshold)
            {
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.MemoryCritical, resetAlert, nullPlaceHolders));
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.MemoryWarning, setAlert, memPlaceholders));

                psStats.MemoryUsage.Status = StatisticStatus.Warning;

                if (logHealth)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "Set memory usage health to WARNING. Placeholders are {0}, {1}",
                                    usedMemoryPercent,
                                    healthMonitorInterval);
                }
            }
            else if (usedMemoryPercent >= Settings.Default.MemoryCriticalThreshold)
            {
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.MemoryWarning, resetAlert, nullPlaceHolders));
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.MemoryCritical, setAlert, memPlaceholders));

                psStats.MemoryUsage.Status = StatisticStatus.Critical;

                if (logHealth)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "Set memory usage health to CRITICAL. Placeholders are {0}, {1}",
                                    usedMemoryPercent,
                                    healthMonitorInterval);
                }
            }
            else
            {
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.MemoryWarning, resetAlert, nullPlaceHolders));
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.MemoryCritical, resetAlert, nullPlaceHolders));

                psStats.MemoryUsage.Status = StatisticStatus.Healthy;

                if (logHealth)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "Reset Memory health. Used memory : {0}%",
                                    usedMemoryPercent);
                }
            }

            if (systemPerf.TotalVirtualMemorySize == -1 || systemPerf.UsedMemoryPercent.Equals(-1d))
            {
                psStats.MemoryUsage = UnknownStat.MemoryUsage;
            }
            else
            {
                // NOTE-SanKumar-1906: In a dynamic memory enabled VM, the
                // total physical memory size keeps changing. Similarly, in
                // automatic paging size set system, the page file size keeps
                // changing. So, we can't assume that total virtual memory size
                // is constant.
                // Thus, we don't actually query the CommittedBytes and calculate
                // the average usage but try to get the average total virtual
                // memory size and the used memory percentage to derive a
                // reasonable (if not exact) used bytes value, in order to avoid
                // edge cases which would lead to mismatch between the percentage
                // in perfmon and the percentage calculated by us.
                psStats.MemoryUsage.Used = Convert.ToInt64(
                    systemPerf.TotalVirtualMemorySize / 100d * systemPerf.UsedMemoryPercent);
            }

            #endregion Memory Usage

            #region Disk Space

            // Set/Reset Disk usage health
            double freeSpacePercent = systemPerf.FreeSpacePercent;
            double freeSpaceInGB =
                Math.Round(systemPerf.FreeSpace / Math.Pow(1024, 3), 2);
            DiskHealthPlaceholders diskPlaceholders = new DiskHealthPlaceholders
            {
                HealthMonitorInterval = healthMonitorInterval,
                FreeSpace = freeSpaceInGB,
                FressSpacePercent = freeSpacePercent
            };

            psStats.InstallVolumeSpace.Total = (long)systemPerf.TotalSpace;
            psStats.InstallVolumeSpace.Used = (long)(systemPerf.TotalSpace - systemPerf.FreeSpace);

            bool resetDiskHealth = false;

            if (freeSpacePercent.Equals(-1))
            {
                psStats.InstallVolumeSpace = UnknownStat.InstallVolumeSpace;

                // Reset disk health disk space if it fails continously through out the samples collection time.
                resetDiskHealth = true;
            }
            else if (freeSpacePercent <= Settings.Default.FreeSpaceWarningThreshold &&
                freeSpacePercent > Settings.Default.FreeSpaceCriticalThreshold)
            {
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.DiskCritical, resetAlert, nullPlaceHolders));
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.DiskWarning, setAlert, diskPlaceholders));

                psStats.InstallVolumeSpace.Status = StatisticStatus.Warning;

                if (logHealth)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "Set disk space health to WARNING. Placeholders are {0}, {1}, {2}",
                                    freeSpaceInGB,
                                    freeSpacePercent,
                                    healthMonitorInterval);
                }
            }
            else if (freeSpacePercent <= Settings.Default.FreeSpaceCriticalThreshold)
            {
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.DiskWarning, resetAlert, nullPlaceHolders));
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.DiskCritical, setAlert, diskPlaceholders));

                psStats.InstallVolumeSpace.Status = StatisticStatus.Critical;

                if (logHealth)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "Set disk space health to CRITICAL. Placeholders are {0}, {1}, {2}",
                                    freeSpaceInGB,
                                    freeSpacePercent,
                                    healthMonitorInterval);
                }
            }
            else
            {
                psStats.InstallVolumeSpace.Status = StatisticStatus.Healthy;

                resetDiskHealth = true;
            }
            
            //Todo: [Himanshu] fix this for legacy too.
            //Raising cumulative throttle from here in case of RCM prime.
            if (Settings.Default.RCM_ReportCumulativeThrottle &&
                PSInstallationInfo.Default.CSMode == CSMode.Rcm &&
                freeSpacePercent <= (100 - GetCumulativeThrottleLimit()))
            {
                CumulativeThrottlePlaceholders cumulativeThrottlePlaceholders = new CumulativeThrottlePlaceholders
                {
                    HealthMonitorInterval = healthMonitorInterval,
                    FreeSpace = freeSpaceInGB,
                    FreeSpacePercent = freeSpacePercent
                };

                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.CumulativeThrottle, setAlert, cumulativeThrottlePlaceholders));

                if (logHealth)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "Setting cumulative throttle. Placeholders are {0}, {1}, {2}, {3}",
                                    freeSpaceInGB,
                                    freeSpacePercent,
                                    GetCumulativeThrottleLimit(),
                                    healthMonitorInterval);
                }
            }

            if (resetDiskHealth)
            {
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.DiskWarning, resetAlert, nullPlaceHolders));
                infraHealthFactorItems.Add(new InfraHealthFactorItem(
                    InfraHealthFactors.DiskCritical, resetAlert, nullPlaceHolders));

                if (logHealth)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "Reset disk space health. Disk Freespace is {0} GB, {1}%",
                                    freeSpaceInGB,
                                    freeSpacePercent);
                }
            }

            #endregion Disk Space

            return (infraHealthFactorItems, psStats);
        }

        /// <summary>
        /// Gets the cumulative throttle limit,
        /// in case the RCM settings are not available fallback to default settings.
        /// </summary>
        /// <returns></returns>
        private int GetCumulativeThrottleLimit()
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
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Couldn't get cached PS settings");

                return Settings.Default.Monitoring_CumulativeThrottleThreshold;
            }

            return (int)psSettings.CumulativeThrottleLimit;
        }

        private List<IProcessServerPairSettings> GetPairsInForwardDirection(CancellationToken cancellationToken)
        {
            var fwPairs = new List<IProcessServerPairSettings>();

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
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Couldn't get cached PS settings");

                return fwPairs;
            }

            var pairs = psSettings.Pairs;

            if (pairs == null)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Couldn't get pair settings from CS");

                return fwPairs;
            }
            else if (!pairs.Any())
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Replication pairs are not configured");

                return fwPairs;
            }

            // Fetch the forward replication pairs which are not paused by target
            fwPairs = pairs.Where(
                    pair => pair.ProtectionDirection == ProtectionDirection.Forward
                    && pair.TargetReplicationStatus == TargetReplicationStatus.NonPaused).ToList();

            if (fwPairs == null || fwPairs.Count <= 0)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Forward replications are not configured");
            }

            return fwPairs;
        }

        private void MonitorUploadedData(CircularList<UploadedDataContainer> uploadedDataStats)
        {
            //long uploadedData = -1;
            var uploadedDataTuple = new Tuple<long, double>(-1, -1.0);

            try
            {
                if (psMonitor != null)
                {
                    uploadedDataTuple = psMonitor.GetUploadedData();
                }

                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Verbose,
                                "uploadedData {0} bytes, transfer time {1} secs",
                                uploadedDataTuple.Item1, uploadedDataTuple.Item2);
            }
            catch (Exception ex)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Ignoring exception. Failed to get uploaded load with exception : {0}{1}",
                        Environment.NewLine,
                        ex);
            }

            var data = new UploadedDataContainer
            {
                UploadedData = uploadedDataTuple.Item1,
                TransferTime = uploadedDataTuple.Item2
            };

            uploadedDataStats.CurrentValue = data;
            uploadedDataStats.Next();
        }

        private List<InfraHealthFactorItem> ResetThroughPutHealth()
        {
            bool resetEvent = false;
            object nullPlaceHolders = null;
            var infraHealthFactorItems = new List<InfraHealthFactorItem>
            {
                new InfraHealthFactorItem(
                    InfraHealthFactors.DataUploadCritical, resetEvent, nullPlaceHolders),

                new InfraHealthFactorItem(
                    InfraHealthFactors.DataUploadWarning, resetEvent, nullPlaceHolders)
            };

            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    "Resetting throughput health alert. ");

            return infraHealthFactorItems;
        }

        private
            (List<InfraHealthFactorItem>, (long UploadPendingData, long ThroughputBytesPerSec, StatisticStatus Status))
            GetUploadDataHealth(
                CircularList<UploadedDataContainer> uploadedDataStatistics,
                CancellationToken cancellationToken)
        {
            const bool setAlert = true;
            const bool resetAlert = false;
            bool resetThrougputHealth = false;
            object nullPlaceHolders = null;
            var infraHealthFactors = new List<InfraHealthFactorItem>();
            (long UploadPendingData, long ThroughputBytesPerSec, StatisticStatus Status) throughputStat;

            try
            {
                if (uploadedDataStatistics == null || uploadedDataStatistics.Count <= 0)
                {
                    throw new ArgumentException("Stats is null/empty", "uploadedDataStats");
                }

                // Get forward replication pairs from the PS Cache Settings
                var pairs = this.GetPairsInForwardDirection(cancellationToken);

                if (pairs == null || pairs.Count <= 0)
                {
                    throughputStat.ThroughputBytesPerSec = 0;
                    throughputStat.UploadPendingData = 0;
                    throughputStat.Status = StatisticStatus.Healthy;

                    resetThrougputHealth = true;
                }
                else
                {
                    (var uploadedData, var transferTime) = GetUploadedData(uploadedDataStatistics);
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Verbose,
                                "Uploaded data size : {0}, transfer time : {1}",
                                uploadedData, transferTime);

                    long uploadPendingData = GetUploadPendingData(pairs);
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Verbose,
                                "Upload pending data size on PS : {0}",
                                uploadPendingData
                                );

                    var healthMonitorInterval = Settings.Default.HealthMonitorInterval;
                    double uploadPendingDataInMB =
                    Math.Round(uploadPendingData / Math.Pow(1024, 2), 2);
                    double uploadedDataInMB =
                    Math.Round(uploadedData / Math.Pow(1024, 2), 2);

                    throughputStat.ThroughputBytesPerSec =
                        Convert.ToInt64(unchecked(uploadedData / transferTime));
                    throughputStat.UploadPendingData = uploadPendingData;

                    var actualThroughput = Math.Round(
                        uploadedDataInMB / transferTime, 2);
                    var expectedThroughput = Math.Round(
                        uploadPendingDataInMB / Settings.Default.ThroughputWarningInterval.TotalSeconds, 2);

                    string actualThroughputStr = string.Format("{0:0.00} MBps", actualThroughput);
                    string expectedThroughputStr = string.Format("{0:0.00} MBps", expectedThroughput);
                    string uploadPendingDataInMBStr = string.Format("{0:0} MB", uploadPendingDataInMB);
                    string uploadedDataInMBStr = string.Format("{0:0} MB", uploadedDataInMB);
                    DataUploadHealthPlaceholders dataHealthPlaceholders = new DataUploadHealthPlaceholders
                    {
                        HealthMonitorInterval = healthMonitorInterval.TotalMinutes,
                        ActualThroughput = actualThroughputStr,
                        ExpectedThroughput = expectedThroughputStr,
                        UploadPendingData = uploadPendingDataInMBStr,
                        UploadedData = uploadedDataInMBStr
                    };

                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Information,
                                "actualThroughput : {0}," +
                                " expectedThroughput : {1}," +
                                " uploadpendingdata : {2}," +
                                " uploadeddata : {3}," +
                                " datatransfertime : {4}," +
                                " healthMonitoringInterval : {5}",
                                actualThroughputStr,
                                expectedThroughputStr,
                                uploadPendingDataInMBStr,
                                uploadedDataInMBStr,
                                transferTime,
                                healthMonitorInterval.TotalMinutes);

                    var warnCntr =
                        Settings.Default.ThroughputWarningInterval.TotalSeconds /
                        transferTime;
                    var criticalCntr =
                        Settings.Default.ThroughputCriticalInterval.TotalSeconds /
                        transferTime;
                    var expectedDataUploadWarnLimit = uploadedData * (long)warnCntr;
                    var expectedDataUploadErrorLimit = uploadedData * (long)criticalCntr;
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Information,
                                "expectedDataUploadWarnLimit : {0}," +
                                " expectedDataUploadErrorLimit : {1}",
                                expectedDataUploadWarnLimit,
                                expectedDataUploadErrorLimit);

                    if (uploadPendingData < Settings.Default.UploadPendingDataMinThresholdBytes ||
                            expectedDataUploadErrorLimit == 0 || expectedDataUploadWarnLimit == 0)
                    {
                        throughputStat.Status = StatisticStatus.Healthy;

                        resetThrougputHealth = true;

                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Information,
                                "Upload pending data {0} bytes on PS is less than configured threshold {1} bytes." +
                                " Expected Data Upload Warning Threshold Limit : {2}," +
                                " Expected Data Upload Error Threshold Limit : {3}",
                                uploadPendingData,
                                Settings.Default.UploadPendingDataMinThresholdBytes,
                                expectedDataUploadWarnLimit,
                                expectedDataUploadErrorLimit);
                    }
                    else if (uploadPendingData > expectedDataUploadWarnLimit
                        && uploadPendingData <= expectedDataUploadErrorLimit)
                    {
                        infraHealthFactors.Add(new InfraHealthFactorItem(
                        InfraHealthFactors.DataUploadCritical, resetAlert, nullPlaceHolders));
                        infraHealthFactors.Add(new InfraHealthFactorItem(
                            InfraHealthFactors.DataUploadWarning, setAlert, dataHealthPlaceholders));

                        throughputStat.Status = StatisticStatus.Warning;

                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Information,
                                "Set throughput health to WARNING.");
                    }
                    else if (uploadPendingData > expectedDataUploadErrorLimit)
                    {
                        infraHealthFactors.Add(new InfraHealthFactorItem(
                        InfraHealthFactors.DataUploadWarning, resetAlert, nullPlaceHolders));
                        infraHealthFactors.Add(new InfraHealthFactorItem(
                            InfraHealthFactors.DataUploadCritical, setAlert, dataHealthPlaceholders));

                        throughputStat.Status = StatisticStatus.Critical;

                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Information,
                                "Set throughput health to CRITICAL.");
                    }
                    else
                    {
                        throughputStat.Status = StatisticStatus.Healthy;

                        resetThrougputHealth = true;
                    }
                }

                var maxHealthyTPCntToSkipCurrEvent =
                    Settings.Default.MaxHealthyThroughputCntToSkipCurrEvent;

                // Resetting the current warning/critical alert if the health is normal in the
                // last MaxHealthyThroughputCntToSkipCurrEvent times. This is
                // to handle the cases of sudden burst in upload pending data even when bandwidth is good.
                if (!resetThrougputHealth &&
                    throughputHealth != null &&
                    throughputHealth.Count >= maxHealthyTPCntToSkipCurrEvent)
                {
                    bool hasNonHealthyTPInstances =
                        throughputHealth.Any(curr => curr != StatisticStatus.Healthy);

                    if (!hasNonHealthyTPInstances)
                    {
                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Skipping the current throughput health alert" +
                            " as the health was normal in the last {0} instances",
                            maxHealthyTPCntToSkipCurrEvent);

                        resetThrougputHealth = true;
                    }
                }
            }
            catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Cancelling {0} task",
                        MethodBase.GetCurrentMethod().Name);
                throw;
            }
            catch (Exception ex)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to get uploaded load with exception," +
                        " resetting the throughputalerts : {0}{1}",
                        Environment.NewLine,
                        ex);

                throughputStat = UnknownStat.Throughput;

                resetThrougputHealth = true;
            }

            var currentThroughputHealth = throughputStat.Status;
            if (resetThrougputHealth)
            {
                var tpHealthFactors = ResetThroughPutHealth();
                infraHealthFactors.AddRange(tpHealthFactors);

                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Resetting throughput health.");

                currentThroughputHealth = StatisticStatus.Healthy;
            }

            throughputHealth.CurrentValue = currentThroughputHealth;
            throughputHealth.Next();

            return (infraHealthFactors, throughputStat);
        }

        public long GetUploadPendingData(List<IProcessServerPairSettings> pairs)
        {
            const string SOURCE_DIFFS_PATTERN = "completed_diff*.dat";
            const string TARGET_DIFFS_PATTERN = "completed_ediff*.dat";
            const string SYNC_FILES_PATTERN = "completed_sync*.dat";
            long uploadPendingData = 0;

            // Start pendingDataCalcTimer to measure the time taken to calculate upload
            // pending data on PS
            var pendingDataCalcTimer = new Stopwatch();
            pendingDataCalcTimer.Start();

            foreach (var pair in pairs)
            {
                var resyncFolder = pair.ResyncFolder;
                var sourceDiffFolder = pair.SourceDiffFolder;
                var targetDiffFolder = pair.TargetDiffFolder;

                long pendingSyncs = 0;
                long pendingSourceDiffs = 0;
                long pendingTargetDiffs = 0;

                if(pair.ProtectionState == ProtectionState.Unknown)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Verbose,
                            "Skipping the pair {0} as it's protection state is unknown",
                            pair.DeviceId);
                }

                // Get upload pending data of the sync files when pair is in resync step1
                else if (pair.ProtectionState == ProtectionState.ResyncStep1)
                {
                    // Get pending sync files size, consider it as zero in case of exception
                    try
                    {
                        pendingSyncs = FSUtils.GetFilesSize(
                            resyncFolder, SearchOption.TopDirectoryOnly, SYNC_FILES_PATTERN);

                        Constants.s_psSysMonTraceSource.DebugAdminLogV2Message(
                            "Pending sync files size under {0} dir is {1}",
                            resyncFolder,
                            pendingSyncs);
                    }
                    catch (Exception ex)
                    {
                        // Ignoring the exception, will attempt to fetch data in next interval
                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Warning,
                            "Ignoring the error, will attempt to fetch data size in next interval. " +
                            "Failed to get pending sync files under {0}" +
                            " with error : {1}{2}",
                            resyncFolder,
                            Environment.NewLine,
                            ex);
                    }
                }
                // Get upload pending data of the diff files when the pair is in resync step2 or diff sync
                else
                {
                    // Get pending source diff files size, consider it as zero in case of exception
                    try
                    {
                        // Added this check as in case of RCM, source folder will be null
                        if (!string.IsNullOrWhiteSpace(sourceDiffFolder))
                        {
                            pendingSourceDiffs = FSUtils.GetFilesSize(
                            sourceDiffFolder, SearchOption.TopDirectoryOnly, SOURCE_DIFFS_PATTERN);

                            Constants.s_psSysMonTraceSource.DebugAdminLogV2Message(
                                "Pending source diffs under {0} dir is {1}",
                                sourceDiffFolder,
                                pendingSourceDiffs);
                        }
                    }
                    catch (Exception ex)
                    {
                        // Ignoring the exception, will attempt to fetch data in next interval
                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Warning,
                            "Ignoring the error, will attempt to fetch data size in next interval. " +
                            "Failed to get pending source diffs under {0}" +
                            " with error : {1}{2}",
                            sourceDiffFolder,
                            Environment.NewLine,
                            ex);
                    }

                    // Get pending target dif files size, consider it as zero in case of exception
                    try
                    {
                        // Exclude the files in AzurePendingUploadRecoverable/AzurePendingUploadRecoverable
                        // subfolders as MARS will be uploading them to Azure storage blob.
                        pendingTargetDiffs = FSUtils.GetFilesSize(
                            targetDiffFolder, SearchOption.TopDirectoryOnly, TARGET_DIFFS_PATTERN);

                        Constants.s_psSysMonTraceSource.DebugAdminLogV2Message(
                            "Pending target diffs under {0} dir is {1}",
                            targetDiffFolder,
                            pendingTargetDiffs);
                    }
                    catch (Exception ex)
                    {
                        // Ignoring the exception, will attempt to fetch data in next interval
                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(TraceEventType.Warning,
                            "Ignoring the error, will attempt to fetch data size in next interval. " +
                            "Failed to get pending target diffs under {0}" +
                            " with error : {1}{2}",
                            targetDiffFolder,
                            Environment.NewLine,
                            ex);
                    }
                }
                uploadPendingData += pendingSyncs + pendingSourceDiffs + pendingTargetDiffs;
            }

            pendingDataCalcTimer.Stop();

            // Logging the time taken to calculate upload pending data only
            // when configured UploadPendingDataCalcWarnThreshold value exceeds
            if (pendingDataCalcTimer.Elapsed >= Settings.Default.UploadPendingDataCalcWarnThreshold)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Time taken to calculate upload pending data {0} bytes on PS : {1} ms",
                            uploadPendingData,
                            pendingDataCalcTimer.ElapsedMilliseconds);
            }

            return uploadPendingData;
        }

        private Tuple<long, double> GetUploadedData(
            CircularList<UploadedDataContainer> uploadedDataStatistics)
        {
            long uploadedDataSize = -1;
            double dataTransferTime = -1.0;
            var monitoredInterval =
                Settings.Default.HealthMonitorInterval.TotalSeconds;

            try
            {
                // Check if atleast one data sample is valid or not.
                var count = uploadedDataStatistics.Where(data => data.UploadedData < 0)
                    .Count();

                if (count == uploadedDataStatistics.Count)
                {
                    throw new ArgumentException
                        ("Failed to get uploaded data stats");
                }

                var firstSample = uploadedDataStatistics.CurrentValue;
                var lastSample = uploadedDataStatistics.LatestValue;

                // Check if the first collected sample is valid or not
                if (firstSample.UploadedData < 0 || firstSample.TransferTime < 0.0)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Warning,
                        "MARS service might not be running." +
                        "Couldn't get uploaded data samples, firstSample : {0}",
                        firstSample);
                }
                else if (lastSample.UploadedData < firstSample.UploadedData ||
                    lastSample.TransferTime < firstSample.TransferTime)
                {
                    // Logging collected samples in case of MARS service crash
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Warning,
                        "MARS service might have been restarted." +
                        "Found invalid uploaded data samples, firstSample : {0}, lastSample : {1}",
                        firstSample.UploadedData + ":" + firstSample.TransferTime,
                        lastSample.UploadedData + ":" + lastSample.TransferTime);
                }
                else
                {
                    // MARS perf counter gives aggregate of all the collected data samples
                    // since the service start. So, deducting the first sample from last sample
                    // to get the uploaded data size in last monitoring interval
                    uploadedDataSize = lastSample.UploadedData - firstSample.UploadedData;
                    dataTransferTime = lastSample.TransferTime - firstSample.TransferTime;
                }

                // taking 1 sec as transfertime when the transfertime is very less
                dataTransferTime = (dataTransferTime == 0) ? 1.0 : dataTransferTime;
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Verbose,
                            "latest sample : {0}, old sample : {1}, datatransfersize : {2}, uploadedDataSize : {1}",
                            lastSample.TransferTime, firstSample.TransferTime, dataTransferTime, uploadedDataSize);
            }
            finally
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Exited GetUploadedData");
            }

            return new Tuple<long, double>(uploadedDataSize, dataTransferTime);
        }

        /// <summary>
        /// This function is used to report system health issues.
        /// </summary>
        /// <param name="infraHealthFactorItems">List of infra health factor items.</param>
        /// <param name="token">Cancellation token</param>
        /// <returns>returns Task</returns>
        private async Task ReportInfraHealthErrors(
            List<InfraHealthFactorItem> infraHealthFactorItems,
            CancellationToken token)
        {
            try
            {
                token.ThrowIfCancellationRequested();

                if (psApi != null)
                {
                    await psApi.ReportInfraHealthFactorsAsync(infraHealthFactorItems, token).ConfigureAwait(false);
                }
                else
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Warning,
                        "{0} - PSAPI stubs is not initialized",
                        nameof(ReportInfraHealthErrors));
                }
            }
            catch (OperationCanceledException) when (token.IsCancellationRequested)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Cancelling {0} task",
                        nameof(ReportInfraHealthErrors));
                throw;
            }
            catch (Exception ex)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to report infra health issues with error : {0}{1}",
                        Environment.NewLine,
                        ex);
                throw;
            }
        }

        /// <summary>
        /// This function is used to report PS statistics to CS.
        /// </summary>
        /// <param name="psStatistics">Statistics to be reported</param>
        /// <param name="token">Cancellation token</param>
        /// <returns>Reporting task</returns>
        private async Task ReportStatistics(
            ProcessServerStatistics psStatistics, CancellationToken token)
        {
            try
            {
                token.ThrowIfCancellationRequested();

                if (psApi != null)
                {
                    await psApi.ReportStatisticsAsync(psStatistics, token).ConfigureAwait(false);
                }
                else
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Warning,
                        "{0} - PSAPI stubs is not initialized",
                        nameof(ReportStatistics));
                }
            }
            catch (OperationCanceledException)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Cancelling {0} task",
                        nameof(ReportStatistics));

                throw;
            }
            catch (Exception ex)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to report statistics with error : {0}{1}",
                        Environment.NewLine,
                        ex);

                throw;
            }
        }

        /// <summary>
        /// This function is used to start the service which is in stopped state.
        /// </summary>
        /// <param name="serviceName">Name of the service</param>
        /// <param name="token">Cancellation token</param>
        /// <returns>Service start task</returns>
        private void StartService(
            string serviceName, CancellationToken token)
        {
            try
            {
                token.ThrowIfCancellationRequested();

                ServiceUtils.StartService(serviceName, token);
            }
            catch (OperationCanceledException)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Cancelling {0} task",
                        nameof(StartService));

                throw;
            }
            catch (Exception ex)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to start service with error : {0}{1}",
                        Environment.NewLine,
                        ex);
            }
        }

        ~SystemMonitor()
        {
            Dispose(false);
        }

        /// <summary>
        /// Dispose method to release performance counters.
        /// </summary>
        public void Dispose()
        {
            this.Dispose(true);
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Dispose method to release the counters.
        /// </summary>
        /// <param name="disposing">Is disposing.</param>
        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                try
                {
                    cpuCounter?.Dispose();

                    memoryPercentInUseCounter?.Dispose();

                    memoryCommitLimitCounter?.Dispose();

                    procQueueLenCounter?.Dispose();

                    psApi?.Dispose();
                }
                catch (Exception ex)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Ignoring exception. Failed to dispose performance counters with exception : {0}{1}",
                        Environment.NewLine,
                        ex);
                }
            }
        }

        /// <summary>
        /// System Perf container
        /// </summary>
        internal class SystemPerfContainer
        {
            /// <summary>
            /// Gets or sets utilized CPU percentage
            /// </summary>
            public int CpuLoad { get; set; }

            /// <summary>
            /// Gets or sets Free Space of volume containing PS installation directory in bytes
            /// </summary>
            public ulong FreeSpace { get; set; }

            /// <summary>
            /// Gets or sets total Space of volume containing PS installation directory in bytes
            /// </summary>
            public ulong TotalSpace { get; set; }

            /// <summary>
            /// Gets or sets free Space percentage
            /// </summary>
            public double FreeSpacePercent { get; set; }

            /// <summary>
            /// Gets or sets used memory percentage
            /// </summary>
            public double UsedMemoryPercent { get; set; }

            /// <summary>
            /// Gets or sets the total virtual memory size in bytes
            /// </summary>
            public long TotalVirtualMemorySize { get; set; }

            /// <summary>
            /// Gets or sets process queue length of the system
            /// </summary>
            public long ProcessorQueueLength { get; set; }
        }

        /// <summary>
        /// Mars Uploaded Data Stats container
        /// </summary>
        internal class UploadedDataContainer
        {
            /// <summary>
            /// Mars uploaded(uncompressed) data
            /// </summary>
            public long UploadedData { get; set; }

            /// <summary>
            /// Transfer time for replication data
            /// </summary>
            public double TransferTime { get; set; }
        }
    }
}