using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System;
using System.Configuration;

namespace Microsoft.Azure.SiteRecovery.ProcessServerMonitor
{
    /// <summary>
    /// Settings for ProcessServerMonitor.exe
    /// </summary>
    [SettingsProvider(typeof(TunablesBasedSettingsProvider))]
    internal class Settings : ApplicationSettingsBase
    {
        public static Settings Default { get; } =
            (Settings)ApplicationSettingsBase.Synchronized(new Settings());

        public Settings()
        {
            this.InitializeSettings(freezeProperties: true, arePropertyValuesReadOnly: true);
        }

        #region PS/CS services

        /// <summary>
        /// Services in PS that will be included in generating health alerts
        /// and reporting statistics
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue(@"{'monitor': 'ON', 'monitor_disks': 'ON',
            'monitor_protection': 'ON', 'monitor_ps': 'ON', 'gentrends': 'ON',
            'esx': 'ON', 'volsync': 'ON', 'InMage PushInstall': 'ON',
            'cxprocessserver': 'ON', 'ProcessServer': 'ON'}")]
        public string PSServices => (string)(this[nameof(PSServices)]);

        /// <summary>
        /// Services in CS that will be included in reporting statistics in an
        /// in-built PS
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue(@"{'W3SVC': 'ON', 'MySQL': 'ON'}")]
        public string CSServices => (string)(this[nameof(CSServices)]);

        #endregion PS/CS services

        #region SystemMonitor Settings

        /// <summary>
        /// The wait interval between every System Monitor main iteration
        /// 1 second
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:01")]
        public TimeSpan SysMonMainIterWaitInterval =>
            (TimeSpan)this[nameof(SysMonMainIterWaitInterval)];

        /// <summary>
        /// The wait interval between every Perf Collector main iteration
        /// 1 second
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:01")]
        public TimeSpan PerfColMainIterWaitInterval =>
            (TimeSpan)this[nameof(PerfColMainIterWaitInterval)];

        /// <summary>
        /// The wait interval between retries to create the ChurnAndThrpCollector.
        /// 5 Min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:05:00")]
        public TimeSpan ChurnAndThrpCollectorCreationIterWaitInterval =>
            (TimeSpan)this[nameof(ChurnAndThrpCollectorCreationIterWaitInterval)];

        /// <summary>
        /// The wait interval between retries to initialize the objects.
        /// 1 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan InitFailureRetryInterval =>
            (TimeSpan)this[nameof(InitFailureRetryInterval)];

        /// <summary>
        /// Default max number of errors, beyond which the exception will be logged
        /// 5
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("5")]
        public int MaxErrorCountForInitExLogging =>
            (int)this[nameof(MaxErrorCountForInitExLogging)];

        /// <summary>
        /// Max number of errors to log, beyond which the logs will be rate controlled
        /// 2
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("2")]
        public int MaxLimitForRateControllingExLogging =>
            (int)this[nameof(MaxLimitForRateControllingExLogging)];

        #endregion SystemMonitor Settings

        #region Process Server Monitoring Settings

        /// <summary>
        /// If set, PS collects perf statistics at every <see cref="PerfMonitorInterval"/>
        /// and reports health alerts at every <see cref="HealthReportInterval"/>
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool MonitorPerfStatistics => (bool)this[nameof(MonitorPerfStatistics)];

        /// <summary>
        /// If set, PS collects service status at every <see cref="ServiceMonitorInterval"/>
        /// and reports health alerts at every <see cref="HealthReportInterval"/>
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool MonitorServiceStatus => (bool)this[nameof(MonitorServiceStatus)];

        /// <summary>
        /// If set, PS collects and reports throughput health at every <see cref="HealthReportInterval"/>
        /// for the last <see cref="HealthMonitorInterval"/>
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool MonitorThroughputStats => (bool)this[nameof(MonitorThroughputStats)];

        /// <summary>
        /// If set, PS collects its statistics at every <see cref="StatisticsMonitorInterval"/> 
        /// and reports its statistics at every <see cref="StatisticsReportInterval"/>
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool ReportStatistics => (bool)this[nameof(ReportStatistics)];

        /// <summary>
        /// If set, PS will report cumulative throttle health issue.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool ReportCumulativeThrottle => (bool)this[nameof(ReportCumulativeThrottle)];

        /// <summary>
        /// If set, PS will report cumulative throttle health issue in RCM mode.
        /// false
        /// </summary>
        /// <remarks>Turning off this monitoring by default as this events is raised by agent</remarks>
        [ApplicationScopedSetting]
        [DefaultSettingValue("false")]
        public bool RCM_ReportCumulativeThrottle => (bool)this[nameof(RCM_ReportCumulativeThrottle)];

        /// <summary>
        /// If set, PS will report Ps Events.
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool ReportPSEvents => (bool)this[nameof(ReportPSEvents)];

        /// <summary>
        /// If set, updates the cert expiry details of CS and PS.
        /// Sends alerts if PS cert nears expiry/expired
        /// false
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool MonitorCerts => (bool)this[nameof(MonitorCerts)];

        /// <summary>
        /// The interval at which the PS collects its statistics
        /// 5 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:05")]
        public TimeSpan StatisticsMonitorInterval =>
            (TimeSpan)this[nameof(StatisticsMonitorInterval)];

        /// <summary>
        /// The interval at which the PS reports its statistics
        /// 1 min
        /// </summary>
        /// <remarks>Used to be 75 seconds in Amethyst.conf as SYSTEM_MONITOR_INTERVAL</remarks>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan StatisticsReportInterval =>
            (TimeSpan)this[nameof(StatisticsReportInterval)];

        /// <summary>
        /// The interval at which the PS events are reported
        /// 5 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:05:00")]
        public TimeSpan PSEventsReportInterval =>
            (TimeSpan)this[nameof(PSEventsReportInterval)];

        /// <summary>
        /// The interval at which PS health alerts will be reported
        /// 5 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:05:00")]
        public TimeSpan HealthReportInterval => (TimeSpan)this[nameof(HealthReportInterval)];

        /// <summary>
        /// The interval at which PS collects its perf statistics
        /// 5 sec
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:05")]
        public TimeSpan PerfMonitorInterval => (TimeSpan)this[nameof(PerfMonitorInterval)];

        /// <summary>
        /// The interval at which PS collects its service statistics
        /// 1 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan ServiceMonitorInterval => (TimeSpan)this[nameof(ServiceMonitorInterval)];

        /// <summary>
        /// The interval at which PS statistics will be monitored to send alerts
        /// 15 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:15:00")]
        public TimeSpan HealthMonitorInterval => (TimeSpan)this[nameof(HealthMonitorInterval)];

        /// <summary>
        /// The interval after which cumulative throttle will be checked
        /// to send alerts for legacy ps
        /// 5 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:05:00")]
        public TimeSpan CumulativeThrottleMonitorInterval =>
            (TimeSpan)this[nameof(CumulativeThrottleMonitorInterval)];

        /// The interval at which CS/PS certs will be monitored to send alerts
        /// 1 hour
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("01:00:00")]
        public TimeSpan MonitorCertsInterval => (TimeSpan)this[nameof(MonitorCertsInterval)];

        #endregion Process Server Monitoring Settings

        #region Cpu Load

        /// <summary>
        /// CPU load warning threshold limit
        /// 80
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("80")]
        public int CPUWarningThreshold => (int)this[nameof(CPUWarningThreshold)];

        /// <summary>
        /// CPU load critical threshold limit
        /// 95
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("95")]
        public int CPUCriticalThreshold => (int)this[nameof(CPUCriticalThreshold)];

        #endregion Cpu Load

        #region Memory Usage

        /// <summary>
        /// Memory usage warning threshold limit
        /// 80
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("80")]
        public int MemoryWarningThreshold => (int)this[nameof(MemoryWarningThreshold)];

        /// <summary>
        /// Memory usage critical threshold limit
        /// 95
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("95")]
        public int MemoryCriticalThreshold => (int)this[nameof(MemoryCriticalThreshold)];

        #endregion Memory Usage

        #region Disk Space

        /// <summary>
        /// Installation volume freespace warning threshold limit
        /// 30
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("30")]
        public int FreeSpaceWarningThreshold => (int)this[nameof(FreeSpaceWarningThreshold)];

        /// <summary>
        /// Installation volume freespace critical threshold limit
        /// 25
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("25")]
        public int FreeSpaceCriticalThreshold => (int)this[nameof(FreeSpaceCriticalThreshold)];

        /// <summary>
        /// Percentange of used space above which cumulative
        /// throttle will be set.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("80")]
        public int Monitoring_CumulativeThrottleThreshold => (int)this["Monitoring_CumulativeThrottleThreshold"];

        #endregion Disk Space

        #region System Load

        /// <summary>
        /// System load warning threshold limit
        /// 6
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("6")]
        public int ProcessorQueueWarningThreshold => (int)this[nameof(ProcessorQueueWarningThreshold)];

        /// <summary>
        /// System load critical threshold limit
        /// 12
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("12")]
        public int ProcessorQueueCriticalThreshold => (int)this[nameof(ProcessorQueueCriticalThreshold)];

        #endregion System Load

        #region PS Throughput

        /// <summary>
        /// Throughput warning threshold interval
        /// 30 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:30:00")]
        public TimeSpan ThroughputWarningInterval => (TimeSpan)this[nameof(ThroughputWarningInterval)];

        /// <summary>
        /// Throughput critical threshold interval
        /// 45 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:45:00")]
        public TimeSpan ThroughputCriticalInterval => (TimeSpan)this[nameof(ThroughputCriticalInterval)];

        /// <summary>
        /// Upload pending data log threshold interval
        /// 30 sec
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:30")]
        public TimeSpan UploadPendingDataCalcWarnThreshold =>
            (TimeSpan)this[nameof(UploadPendingDataCalcWarnThreshold)];

        /// <summary>
        /// Upload pending data threshold limit
        /// 304857600 Bytes(300MB)
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("304857600")]
        public long UploadPendingDataMinThresholdBytes => (long)this[nameof(UploadPendingDataMinThresholdBytes)];

        /// <summary>
        /// Max number of times throughput health should be normal in order to reset the current health event
        /// 6
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("6")]
        public int MaxHealthyThroughputCntToSkipCurrEvent =>
            (int)this[nameof(MaxHealthyThroughputCntToSkipCurrEvent)];

        #endregion PS Throughput

        #region Debugger

        /// <summary>
        /// Maximum wait time, until which the service will wait for a debugger
        /// to be attached at service startup.
        /// 0 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:00")]
        public TimeSpan Debugger_MaxWaitTimeForAttachOnStart =>
            (TimeSpan)this[nameof(Debugger_MaxWaitTimeForAttachOnStart)];

        /// <summary>
        /// If true, the debugger will break soon after (if) it is attached at
        /// the service startup.
        /// False
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("false")]
        public bool Debugger_BreakOnStart => (bool)this[nameof(Debugger_BreakOnStart)];

        #endregion Debugger

        #region PSChurnAndThroughputCollector
        /// <summary>
        /// The wait interval between every iteration of PS churn and thrp collection and perf creation
        ///  5 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:05:00")]
        public TimeSpan PSChurnAndThrpSamplingInterval =>
            (TimeSpan)this[nameof(PSChurnAndThrpSamplingInterval)];

        /// <summary>
        /// Old Churn and Throughput stat files deletion interval
        /// 1 hour
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("01:00:00")]
        public TimeSpan StaleChurnAndThrpStatFilesDelInterval =>
            (TimeSpan)this[nameof(StaleChurnAndThrpStatFilesDelInterval)];

        /// <summary>
        /// Churn and Throughput stat files collection threshold interval
        /// 30 sec
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:30")]
        public TimeSpan ChurnAndThrpStatsColWarnThreshold =>
            (TimeSpan)this[nameof(ChurnAndThrpStatsColWarnThreshold)];

        /// <summary>
        /// Churn and Throughput stats files deletion threshold interval
        /// 30 sec
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:30")]
        public TimeSpan ChurnAndThrpStatFilesDelWarnThreshold =>
            (TimeSpan)this[nameof(ChurnAndThrpStatFilesDelWarnThreshold)];

        /// <summary>
        /// Perf collection threshold interval
        /// 1 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan PerfCollectionWarnThreshold =>
            (TimeSpan)this[nameof(PerfCollectionWarnThreshold)];

        #endregion 

        /// <summary>
        /// Time duration after which the process will be respawned
        /// 5 minutes
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:05:00")]
        public TimeSpan ProcessRespawnInterval => (TimeSpan)this[nameof(ProcessRespawnInterval)];

        /// <summary>
        /// Time duration after which process status is checked
        /// 5 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:05")]
        public TimeSpan ProcessPollInterval => (TimeSpan)this[nameof(ProcessPollInterval)];

        /// <summary>
        /// EvtCollForw exe name
        /// evtcollforw.exe
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue(@"evtcollforw.exe")]
        public string EvtCollForwPath => (string)this[nameof(EvtCollForwPath)];

        /// <summary>
        /// EvtCollForw process name
        /// evtcollforw
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue(@"evtcollforw")]
        public string EvtCollForwProcName => (string)this[nameof(EvtCollForwProcName)];

        // To do: Change the environment wrt Nitin's changes
        /// <summary>
        /// EvtCollForw command line parameters for starting it in Rcm ps mode
        /// /Environment V2A_RCM_Process_Server
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue(@"/Environment V2A_RCM_Process_Server")]
        public string EvtCollForwArgs => (string)this[nameof(EvtCollForwArgs)];

        /// <summary>
        /// Prefix for inmage agent process events. The name of the event is ProcessEventPrefix<ProcessId>
        /// InMageAgent
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue(@"InMageAgent")]
        public string ProcessEventPrefix => (string)this[nameof(ProcessEventPrefix)];

        // Notes for adding a new setting:
        // 1. Add the property only in the code. Never add it in the app.config.
        //    This helps the size of the app.config to remain minimum as well as
        //    keeping these keys private.
        // 2. Always qualify the property with [ApplicationScopedSetting].
        //    All the ASR assemblies run as System, so there's no necessity to
        //    call out a property as [UserScopedSetting].
        // 3. As much as possible, try to add [DefaultSettingValue("...")].
        //    Note that, if value in configuration file couldn't be deserialized
        //    into the type of the property, the default value would be parsed.
        //    If the default value couldn't be parsed, an exception would be thrown.
        //    So, make sure to validate that the default value is parsed successfully,
        //    before checking-in.
        //    If the default value is not provided, the automatic default value
        //    of the type would be returned.
        //    (For more, search SettingsPropertyValue.PropertyValue)
        // 4. Since the application settings are ignored and only the user settings
        //    are affected during upgrade() and save() invocations, it would be
        //    misleading if you add setter to an application setting property.
        //    Moreover, InitializeSettings(arePropertyValuesReadOnly: true); sets
        //    every property in this settings class to be read-only. That means,
        //    this["name"] = value; will always result in exception.
        //    So, do not add a setter.
        // 5. Always test to 100% code coverage for all the modifications made
        //    to this file. Otherwise, hidden bugs could be hit in production.
        //    Ex: The values from the configuration file aren't enumerated, until
        //    a property is first accessed.
        //    Ex: Custom logic in a getter wouldn't be hit until explicitly called.
    }
}
