using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System;
using System.Configuration;

namespace Microsoft.Azure.SiteRecovery.ProcessServer
{
    /// <summary>
    /// Settings for ProcessServer.exe
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

        #region Modify RCM PS

        /// <summary>
        /// Minimum time to wait before checking if the Process server configuration
        /// to reported to RCM has been modified.
        /// 2 minutes
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:02:00")]
        public TimeSpan ModifyRcmPS_PollInterval =>
            (TimeSpan)this[nameof(ModifyRcmPS_PollInterval)];

        /// <summary>
        /// Minimum time to wait before retrying a failed modify Process server
        /// RCM API invocation.
        /// 1 minute
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan ModifyRcmPS_FailureRetryInterval =>
            (TimeSpan)this[nameof(ModifyRcmPS_FailureRetryInterval)];

        #endregion Modify RCM PS

        #region MonitorHealth
        /// <summary>
        /// Flag to disable the monitor health service.
        /// true.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool MonitorHealthService =>
            (bool)this[nameof(MonitorHealthService)];

        /// <summary>
        /// Flag to enable/disable the replication health monitoring in RCM PS mode.
        /// true.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool RCM_MonitorReplicationHealth =>
            (bool)this[nameof(RCM_MonitorReplicationHealth)];

        /// <summary>
        /// Flag to enable/disable the telemetry monitoring in RCM PS mode.
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool RCM_MonitorPSV2Telemetry =>
            (bool)this[nameof(RCM_MonitorPSV2Telemetry)];

        /// <summary>
        /// The inetrval after which source to ps data upload
        /// is checked to raise alerts
        /// 15 min.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:15:00")]
        public TimeSpan MonitorHealthSourceReplicationBlockedInterval =>
            (TimeSpan)this[nameof(MonitorHealthSourceReplicationBlockedInterval)];

        /// <summary>
        /// Flag to check if source to ps data upload should 
        /// be monitored or not
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool MonitorSourceReplicationBlocked =>
            (bool)this[nameof(MonitorSourceReplicationBlocked)];

        /// <summary>
        /// The time interval for which Monitor health service should
        /// wait before starting next iteration.
        /// 1 sec
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:01")]
        public TimeSpan MonitorHealthServiceWaitInterval =>
            (TimeSpan)this[nameof(MonitorHealthServiceWaitInterval)];

        /// <summary>
        /// The time interval for which Monitor health service should
        /// wait before starting next iteration.
        /// 30 sec
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:30")]
        public TimeSpan MonitorHealthServiceInitialWaitInterval =>
            (TimeSpan)this[nameof(MonitorHealthServiceInitialWaitInterval)];

        /// <summary>
        /// Flag to check if Ir/Dr stuck files should be
        /// monitored to raise alerts.
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool MonitorIrDrStuckFile => (bool)this[nameof(MonitorIrDrStuckFile)];

        /// <summary>
        /// Flag to check if RPO should be monitored and raise alerts
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool MonitorRPO => (bool)this[nameof(MonitorRPO)];

        /// <summary>
        /// Flag to check if resync throttle should be monitored or not
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool MonitorResyncThrottle => (bool)this[nameof(MonitorResyncThrottle)];

        /// <summary>
        /// Flag to check if diff throttle should be monitored.
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool MonitorDiffThrottle =>
            (bool)this[nameof(MonitorDiffThrottle)];

        /// <summary>
        /// Interval after which IR/DR stuck files should be monitored 
        /// to raise alerts.
        /// 1 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan MonitorIrDrStuckFileInterval =>
            (TimeSpan)this[nameof(MonitorIrDrStuckFileInterval)];

        /// <summary>
        /// Sentinel error threshold for legacy ps
        /// 30 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:30:00")]
        public TimeSpan SentinelErrorThreshold =>
            (TimeSpan)this[nameof(SentinelErrorThreshold)];

        /// <summary>
        /// Interval after which pairs should be monitored for RPO
        /// 1 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan MonitorRPOInterval =>
            (TimeSpan)this[nameof(MonitorRPOInterval)];

        /// <summary>
        /// The time period after which resync throttle is 
        /// checked to raise alerts
        /// 1 min.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan ResyncThrottleMonitorInterval =>
            (TimeSpan)this[nameof(ResyncThrottleMonitorInterval)];

        /// <summary>
        /// Interval after which diff throttle should be monitored.
        /// 1 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan MonitorDiffThrottleInterval =>
            (TimeSpan)this[nameof(MonitorDiffThrottleInterval)];

        /// <summary>
        /// Interval after which PSV2 telemetry should be monitored.
        /// 15 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:15:00")]
        public TimeSpan RCM_MonitorPSV2TelemetryInterval =>
            (TimeSpan)this[nameof(RCM_MonitorPSV2TelemetryInterval)];

        /// <summary>
        /// Interval under which calculation of diff folder size
        /// for all pairs should be completed.
        /// 30 seconds.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:30")]
        public TimeSpan DiffFolderSizeCalcThreshold =>
            (TimeSpan)this[nameof(DiffFolderSizeCalcThreshold)];

        /// <summary>
        /// Interval under which calculation of resync folder size
        /// for all pairs should be completed.
        /// 30 seconds.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:30")]
        public TimeSpan ResyncFolderSizeCalcThreshold =>
            (TimeSpan)this[nameof(ResyncFolderSizeCalcThreshold)];

        /// <summary>
        /// The agent version upto which resync throttle should be monitored.
        /// 9.36
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("9360000")]
        public uint ResyncThrottleMaxVer =>
            (uint)this[nameof(ResyncThrottleMaxVer)];

        /// <summary>
        /// The agent version upto which resync throttle should be monitored.
        /// 9.36
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("9360000")]
        public uint DiffThrottleMaxVer =>
            (uint)this[nameof(DiffThrottleMaxVer)];

        #endregion MonitorHealth

        #region Task Manager

        /// <summary>
        /// Flag to disable the PS Tasks execution.
        /// false.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool RunLegacyPSTasks =>
            (bool)this[nameof(RunLegacyPSTasks)];

        /// <summary>
        /// Json string with PS Tasks
        /// PSTaskList
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue(@"[
                                {
                                    'TaskExecutionState': 'ON',
                                    'TaskAttributes': {
                                                    'NameSpaceName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi',
                                                    'AssemblyName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core',
                                                    'ClassName': 'PSTaskFactory',
                                                    'MethodName': 'DeleteReplicationAsync'
                                                },
                                    'TaskInterval': '60',
                                    'TaskName': 'DeleteReplication'
                                },
                                {
                                    'TaskExecutionState': 'ON',
                                    'TaskAttributes': {
                                                    'NameSpaceName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi',
                                                    'AssemblyName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core',
                                                    'ClassName': 'PSTaskFactory',
                                                    'MethodName': 'RegisterProcessServerAsync'
                                                },
                                    'TaskInterval': '300',
                                    'TaskName': 'RegisterPS'
                                },
                                {
                                    'TaskExecutionState': 'ON',
                                    'TaskAttributes': {
                                                    'NameSpaceName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi',
                                                    'AssemblyName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core',
                                                    'ClassName': 'PSTaskFactory',
                                                    'MethodName': 'AutoResyncOnResyncSetAsync'
                                                },
                                    'TaskInterval': '300',
                                    'TaskName': 'AutoResyncOnResetSet'
                                },
                                {
                                    'TaskExecutionState': 'ON',
                                    'TaskAttributes': {
                                                    'NameSpaceName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi',
                                                    'AssemblyName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core',
                                                    'ClassName': 'PSTaskFactory',
                                                    'MethodName': 'MonitorLogsAsync'
                                                },
                                    'TaskInterval': '300',
                                    'TaskName': 'MonitorLogs'
                                },
                                {
                                    'TaskExecutionState': 'ON',
                                    'TaskAttributes': {
                                                    'NameSpaceName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi',
                                                    'AssemblyName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core',
                                                    'ClassName': 'PSTaskFactory',
                                                    'MethodName': 'RenewPSCertificatesAsync'
                                                },
                                    'TaskInterval': '60',
                                    'TaskName': 'RenewCertificates'
                                },
                                {
                                    'TaskExecutionState': 'ON',
                                    'TaskAttributes': {
                                                    'NameSpaceName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi',
                                                    'AssemblyName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core',
                                                    'ClassName': 'PSTaskFactory',
                                                    'MethodName': 'MonitorTelemetryAsync'
                                                },
                                    'TaskInterval': '900',
                                    'TaskName': 'MonitorTelemetry'
                                },
                                {
                                    'TaskExecutionState': 'ON',
                                    'TaskAttributes': {
                                                    'NameSpaceName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi',
                                                    'AssemblyName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core',
                                                    'ClassName': 'PSTaskFactory',
                                                    'MethodName': 'AccumulatePerfDataAsync'
                                                },
                                    'TaskInterval': '60',
                                    'TaskName': 'AccumulatePerfData'
                                },
                                {
                                    'TaskExecutionState': 'ON',
                                    'TaskAttributes': {
                                                    'NameSpaceName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi',
                                                    'AssemblyName': 'Microsoft.Azure.SiteRecovery.ProcessServer.Core',
                                                    'ClassName': 'PSTaskFactory',
                                                    'MethodName': 'UploadPerfDataAsync'
                                                },
                                    'TaskInterval': '300',
                                    'TaskName': 'UploadPerfData'
                                }]")]
        public string LegacyPS_TaskSettings => (string)this[nameof(LegacyPS_TaskSettings)];

        #endregion Task Manager

        #region Volsync

        /// <summary>
        /// Switch to turn off volsync
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool StartVolsync =>
            (bool)this[nameof(StartVolsync)];

        /// <summary>
        /// Time after which volsync should move the files again
        /// from source to target
        /// 5 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:05")]
        public TimeSpan VosyncRunInterval =>
            (TimeSpan)this[nameof(VosyncRunInterval)];

        /// <summary>
        /// Time for which volsync thread should sleep,
        /// if it encounters a non protected pair
        /// 15 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:15")]
        public TimeSpan VolsyncNPPairSleepInterval =>
            (TimeSpan)this[nameof(VolsyncNPPairSleepInterval)];

        /// <summary>
        /// diffdatasort exe sub path
        /// bin/diffdatasort.exe
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("bin/diffdatasort.exe")]
        public string DiffdatasortBinarySubpath =>
            (string)this[nameof(DiffdatasortBinarySubpath)];

        /// <summary>
        /// Max time for which a pair should be processed
        /// 10 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:10:00")]
        public TimeSpan MaxBatchProcessingChunk =>
            (TimeSpan)this[nameof(MaxBatchProcessingChunk)];

        /// <summary>
        /// No of file moves after which perf.log
        /// file should be updated
        /// 500
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("500")]
        public int PerfDataFlushThreshold =>
            (int)this[nameof(PerfDataFlushThreshold)];

        /// <summary>
        /// No of file moves after which perf.txt
        /// file should be updated
        /// 500
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("500")]
        public int CompressionDataFlushThreshold =>
            (int)this[nameof(CompressionDataFlushThreshold)];

        /// <summary>
        /// Should all the pairs for a tmid be processed in parallel
        /// false
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("false")]
        public bool ProcessPairsInParallel =>
            (bool)this[nameof(ProcessPairsInParallel)];

        /// <summary>
        /// The no of retries for directory creation
        /// 3
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("3")]
        public int Volsync_DirectoryCreationRetryCount =>
            (int)this[nameof(Volsync_DirectoryCreationRetryCount)];

        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:01")]
        public TimeSpan Volsync_DirectoryCreationRetryInterval =>
            (TimeSpan)this[nameof(Volsync_DirectoryCreationRetryInterval)];

        #endregion Volsync

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
