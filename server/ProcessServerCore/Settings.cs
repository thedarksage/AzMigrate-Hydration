using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System;
using System.Configuration;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core
{
    /// <summary>
    /// Settings for Microsoft.Azure.SiteRecovery.ProcessServer.Core.dll
    /// </summary>
    [SettingsProvider(typeof(TunablesBasedSettingsProvider))]
    public sealed class Settings : ApplicationSettingsBase
    {
        public static Settings Default { get; } =
            (Settings)ApplicationSettingsBase.Synchronized(new Settings());

        public Settings()
        {
            this.InitializeSettings(freezeProperties: true, arePropertyValuesReadOnly: true);
        }

        #region Legacy CS Mode - Files full paths

        /// <summary>
        /// Path of Application config file in Process Server running in LegacyCS mode.
        /// %ProgramData%/Microsoft Azure Site Recovery/Config/App.Conf
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("%ProgramData%/Microsoft Azure Site Recovery/Config/App.Conf")]
        public string LegacyCS_FullPath_AppConf =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(LegacyCS_FullPath_AppConf)]);

        /// <summary>
        /// Path of Passphrase file in Process Server running in LegacyCS mode.
        /// %ProgramData%/Microsoft Azure Site Recovery/private/connection.passphrase
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("%ProgramData%/Microsoft Azure Site Recovery/private/connection.passphrase")]
        public string LegacyCS_FullPath_CSPassphrase =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(LegacyCS_FullPath_CSPassphrase)]);

        #endregion Legacy CS Mode - Files full paths

        #region Legacy CS Mode - Folders full paths

        /// <summary>
        /// Path of Certificates folder in Process Server running in LegacyCS mode.
        /// %ProgramData%/Microsoft Azure Site Recovery/certs/
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("%ProgramData%/Microsoft Azure Site Recovery/certs/")]
        public string LegacyCS_FullPath_Certs =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(LegacyCS_FullPath_Certs)]);

        /// <summary>
        /// Path of Fingerprints folder in Process Server running in LegacyCS mode.
        /// %ProgramData%/Microsoft Azure Site Recovery/fingerprints/
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("%ProgramData%/Microsoft Azure Site Recovery/fingerprints/")]
        public string LegacyCS_FullPath_Fingerprints =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(LegacyCS_FullPath_Fingerprints)]);

        /// <summary>
        /// Path of passhprase folder in Process Server running in LegacyCS mode.
        /// %ProgramData%/Microsoft Azure Site Recovery/private/
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("%ProgramData%/Microsoft Azure Site Recovery/private/")]
        public string LegacyCS_FullPath_Private =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(LegacyCS_FullPath_Private)]);

        #endregion Legacy CS Mode - Folders full paths

        #region Legacy CS Mode - Files sub paths

        /// <summary>
        /// Root sub path under PS installation path in Process Server running in LegacyCS mode.
        /// home/svsystems/
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("home/svsystems/")]
        public string LegacyCS_SubPath_Root =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(LegacyCS_SubPath_Root)]);

        /// <summary>
        /// Relative path of version file under root folder in Process Server running in LegacyCS mode.
        /// etc/version
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("etc/version")]
        public string LegacyCS_SubPath_Version =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(LegacyCS_SubPath_Version)]);

        /// <summary>
        /// Path of Amethyst config file under root folder in Process Server running in LegacyCS mode.
        /// etc/amethyst.conf
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("etc/amethyst.conf")]
        public string LegacyCS_SubPath_AmethystConf =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(LegacyCS_SubPath_AmethystConf)]);

        /// <summary>
        /// Path of the cached PS settings file under root folder in Process Server running in
        /// LegacyCS mode.
        /// etc/PSSettings.json
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("etc/PSSettings.json")]
        public string LegacyCS_SubPath_PSSettings =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(LegacyCS_SubPath_PSSettings)]);

        /// <summary>
        /// Path of the default folder where all the web pages are stored in LegacyCS mode
        /// admin/web
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("admin/web")]
        public string LegacyCS_SubPath_Web =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(LegacyCS_SubPath_Web)]);

        /// <summary>
        /// Path of the default folder where reboot and version info files are present in LegacyCS mode
        /// monitor
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("monitor")]
        public string LegacyCS_SubPath_Monitor =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(LegacyCS_SubPath_Monitor)]);

        /// <summary>
        /// Path of the telemetry logs folder under /home/svsystems.
        /// telemetry_logs
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("telemetry_logs")]
        public string LegacyCS_SubPath_TelLogsDir =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(LegacyCS_SubPath_TelLogsDir)]);

        #endregion Legacy CS Mode - Files sub paths

        #region Legacy CS Mode - Folders sub paths

        /// <summary>
        /// Path of the default folder under root folder in Process Server running in LegacyCS mode
        /// for putfile requests containing relative paths that doesn't start with /home/svsystems.
        /// transport/data
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("transport/data")]
        public string LegacyCS_SubPath_ReqDefDir =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(LegacyCS_SubPath_ReqDefDir)]);

        #endregion Legacy CS Mode - Folders sub paths

        #region Rcm Mode - Folders sub paths

        /// <summary>
        /// Root sub path under PS installation path in Rcm mode.
        /// home/svsystems/
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("home/svsystems/")]
        public string Rcm_SubPath_Root =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(Rcm_SubPath_Root)]);

        /// <summary>
        /// Sub path of transport private folder under root folder in Process Server.
        /// transport/private/
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("transport/private/")]
        public string Rcm_SubPath_TransportPrivate =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(Rcm_SubPath_TransportPrivate)]);

        /// <summary>
        /// Sub path of Rcm PS Configurator's app insights backup folder under root folder in Process Server.
        /// var/RcmPSConfigurator_AppInsightsBackup/
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("var/RcmPSConfigurator_AppInsightsBackup/")]
        public string Rcm_SubPath_RcmPSConfAppInsightsBackup =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(Rcm_SubPath_RcmPSConfAppInsightsBackup)]);

        /// <summary>
        /// Sub path of Rcm PS Configurator's MDS logs folder under root folder in Process Server.
        /// var/RcmPSConfigurator_Logs/
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("var/RcmPSConfigurator_Logs/")]
        public string Rcm_SubPath_RcmPSConfLogs =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(Rcm_SubPath_RcmPSConfLogs)]);

        #endregion Rcm Mode - Folders sub paths

        #region Firewall Rules constants

        /// <summary>
        /// Firewall group name for cxps
        /// Azure Site Recovery Process Server
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Azure Site Recovery Process Server")]
        public string AsrPSFwGroupName =>
            (string)this[nameof(AsrPSFwGroupName)];

        /// <summary>
        /// Firewall rule name prefix for cxps
        /// Azure Site Recovery Process Server - Incoming Replication traffic - TCP Port
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Azure Site Recovery Process Server - Incoming Replication traffic - TCP Port")]
        public string AsrPSFWRulePrefix =>
            (string)this[nameof(AsrPSFWRulePrefix)];

        #endregion Firewall Rules constants

        #region Common - Files sub paths

        /// <summary>
        /// Sub path of bin folder under root folder in Process Server.
        /// bin/
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("bin/")]
        public string CommonMode_SubPath_Bin =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(CommonMode_SubPath_Bin)]);

        /// <summary>
        /// Sub path of GenCert.exe under root folder in Process Server.
        /// bin/gencert.exe
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("bin/gencert.exe")]
        public string CommonMode_SubPath_GenCertExe =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(CommonMode_SubPath_GenCertExe)]);

        /// <summary>
        /// Sub path of ProcessServer.exe under root folder in Process Server.
        /// bin/ProcessServer.exe
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("bin/ProcessServer.exe")]
        public string CommonMode_SubPath_ProcessServerExe =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(CommonMode_SubPath_ProcessServerExe)]);

        /// <summary>
        /// Sub path of ProcessServerMonitor.exe under root folder in Process Server.
        /// bin/ProcessServerMonitor.exe
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("bin/ProcessServerMonitor.exe")]
        public string Common_Mode_SubPath_ProcessServerMonitorExe =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(Common_Mode_SubPath_ProcessServerMonitorExe)]);

        /// <summary>
        /// Sub path of etc folder under root folder in Process Server.
        /// etc/
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("etc/")]
        public string EtcFolderPath =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(EtcFolderPath)]);

        /// <summary>
        /// Sub path of process server configuration file under root folder in Process Server.
        /// etc/ProcessServer.conf
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("etc/ProcessServer.conf")]
        public string CommonMode_SubPath_PSConfig =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(CommonMode_SubPath_PSConfig)]);

        /// <summary>
        /// Sub path of transport folder under root folder in Process Server.
        /// transport/
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("transport/")]
        public string CommonMode_SubPath_Transport =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(CommonMode_SubPath_Transport)]);

        /// <summary>
        /// Sub path of Cxps config file under root folder in Process Server.
        /// transport/cxps.conf
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("transport/cxps.conf")]
        public string CommonMode_SubPath_CxpsConfig =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(CommonMode_SubPath_CxpsConfig)]);

        /// <summary>
        /// Sub path of cxps.exe under root folder in Process Server.
        /// transport/cxps.exe
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("transport/cxps.exe")]
        public string CommonMode_SubPath_CxpsExe =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(CommonMode_SubPath_CxpsExe)]);

        /// <summary>
        /// Sub path of transport log folder under root folder in Process Server.
        /// transport/log/
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("transport/log/")]
        public string CommonMode_SubPath_TransportLog =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(CommonMode_SubPath_TransportLog)]);

        /// <summary>
        /// Sub path of var folder under root folder in Process Server.
        /// var/
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("var/")]
        public string CommonMode_SubPath_Var =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(CommonMode_SubPath_Var)]);

        /// <summary>
        /// Path of the default folder where churn stat files are present
        /// ChurStat
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("ChurStat")]
        public string CommonMode_SubPath_ChurStat =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(CommonMode_SubPath_ChurStat)]);

        /// <summary>
        /// Path of the default folder where throughput stat files are present
        /// ThrpStat
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("ThrpStat")]
        public string CommonMode_SubPath_ThrpStat =>
            Environment.ExpandEnvironmentVariables((string)this[nameof(CommonMode_SubPath_ThrpStat)]);

        #endregion Common - Files sub paths

        #region ProcessServerCSApiStubsImpl

        /// <summary>
        /// Timeout for HTTP calls to CS API in <see cref="ProcessServerCSApiStubsImpl"/>.
        /// 2 minutes
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:02:00")]
        public TimeSpan PSCSApiImpl_HttpTimeout => (TimeSpan)this[nameof(PSCSApiImpl_HttpTimeout)];

        #endregion ProcessServerCSApiStubsImpl

        #region PhpFormatter

        /// <summary>
        /// Timeout for the expiry of cached serializer actions for complex
        /// types in <see cref="PhpFormatter"/>.
        /// 1 hour
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("01:00:00")]
        public TimeSpan PhpFormatter_CacheSlidingExpiration =>
            (TimeSpan)this[nameof(PhpFormatter_CacheSlidingExpiration)];

        #endregion PhpFormatter

        #region MdsLogCutterTraceListener

        /// <summary>
        /// Default interval after which the current log file would be cut by
        /// <see cref="MdsLogCutterTraceListener"/>.
        /// 5 minutes
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:05:00")]
        public TimeSpan MdsLogCutterTraceListener_CutInterval =>
            (TimeSpan)this[nameof(MdsLogCutterTraceListener_CutInterval)];

        /// <summary>
        /// Default maximum completed files beyond which the current log file
        /// wouldn't be cut anymore by <see cref="MdsLogCutterTraceListener"/>.
        /// 3 files
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("3")]
        public uint MdsLogCutterTraceListener_MaxCompletedFilesCount =>
            (uint)this[nameof(MdsLogCutterTraceListener_MaxCompletedFilesCount)];

        /// <summary>
        /// Default maximum file size in KB after which the current log file
        /// would be cut (or) truncated by <see cref="MdsLogCutterTraceListener"/>.
        /// 10 MB
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("10240")] // 10 MB
        public ulong MdsLogCutterTraceListener_MaxFileSizeKB =>
            (ulong)this[nameof(MdsLogCutterTraceListener_MaxFileSizeKB)];

        #endregion MdsLogCutterTraceListener

        #region Process Server Settings Caching

        /// <summary>
        /// Interval after which the cached process server settings expire.
        /// 1 minute
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan ProcessServerSettings_CacheExpiryInterval =>
            (TimeSpan)this[nameof(ProcessServerSettings_CacheExpiryInterval)];

        /// <summary>
        /// Does cached settings file contain the header in the first line or not?
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool ProcessServerSettings_CacheIncludesHeader =>
            (bool)this[nameof(ProcessServerSettings_CacheIncludesHeader)];

        /// <summary>
        /// Should enforce strict major version checking (should be same as
        /// supported by code) in parsing the cached setting file?
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool ProcessServerSettings_CacheEnforceMajorVersionCheck =>
            (bool)this[nameof(ProcessServerSettings_CacheEnforceMajorVersionCheck)];

        /// <summary>
        /// Should enforce strict minor version checking (should be same as
        /// supported by code) in parsing the cached setting file?
        /// Valid, only if <c>ProcessServerSettings_CacheEnforceMajorVersionCheck</c>
        /// is true.
        /// false
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("false")]
        public bool ProcessServerSettings_CacheEnforceMinorVersionCheck =>
            (bool)this[nameof(ProcessServerSettings_CacheEnforceMinorVersionCheck)];

        /// <summary>
        /// Should verify the header against conent of the cached setting file
        /// with Checksum validation, etc.
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool ProcessServerSettings_CacheVerifyHeader =>
            (bool)this[nameof(ProcessServerSettings_CacheVerifyHeader)];

        /// <summary>
        /// Should master poller attempt once to load from the cached setting file,
        /// if the server couldn't be communicated to retrieve the latest settings?
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool ProcessServerSettings_CacheMasterAttemptInitLoadFromCache =>
            (bool)this[nameof(ProcessServerSettings_CacheMasterAttemptInitLoadFromCache)];

        /// <summary>
        /// Should master poller use file buffer flushing to store the cached
        /// setting file? (costly)
        /// false
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("false")]
        public bool ProcessServerSettings_CacheMasterFlushFile =>
            (bool)this[nameof(ProcessServerSettings_CacheMasterFlushFile)];

        #endregion Process Server Settings Caching

        #region CSProcessServerSettings

        /// <summary>
        /// True, if the paths returned by the Process Server Settings in CS
        /// Mode should be in long path format optimally.
        /// false
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("false")]
        public bool ProcessServerSettings_CS_LongPathOptimalConversion =>
            (bool)this[nameof(ProcessServerSettings_CS_LongPathOptimalConversion)];

        #endregion CSProcessServerSettings

        #region RcmProcessServerSettings

        /// <summary>
        /// True, if the paths returned by the Process Server Settings in RCM
        /// Mode should be in long path format optimally.
        /// false
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("false")]
        public bool ProcessServerSettings_Rcm_LongPathOptimalConversion =>
            (bool)this[nameof(ProcessServerSettings_Rcm_LongPathOptimalConversion)];

        /// <summary>
        /// Default differential throttle limit set for a pair
        /// 8 GB
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("8589934592")] // 8 GB
        public long ProcessServerSettings_Rcm_DefaultDiffThrottleLimit =>
            (long)this[nameof(ProcessServerSettings_Rcm_DefaultDiffThrottleLimit)];

        /// <summary>
        /// Default diff throttle limitfor a pair in resync
        /// 1 GB
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("1073741824")] // 1 GB
        public long ProcessServerSettings_DiffThrottleLimitInBytesForPairInResync =>
            (long)this[nameof(ProcessServerSettings_DiffThrottleLimitInBytesForPairInResync)];

        /// <summary>
        /// Default resync throttle limit set for a pair
        /// 8 GB
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("8589934592")] // 8 GB
        public long ProcessServerSettings_Rcm_DefaultResyncThrottleLimit =>
            (long)this[nameof(ProcessServerSettings_Rcm_DefaultResyncThrottleLimit)];

        /// <summary>
        /// Default cumulative throttle limit for the Process server
        /// 80%
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("80")]
        public decimal ProcessServerSettings_Rcm_DefaultCumulativeThrottleLimit =>
            (decimal)this[nameof(ProcessServerSettings_Rcm_DefaultCumulativeThrottleLimit)];

        /// <summary>
        /// If true, settings tracking related data is stored on the machine to
        /// debug any issue around merging the settings with the tracking id logic.
        /// false
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("false")]
        public bool ProcessServerSettings_Rcm_StoreSettingsMergeDebugData =>
            (bool)this[nameof(ProcessServerSettings_Rcm_StoreSettingsMergeDebugData)];

        /// <summary>
        /// Enable/Disable Access Control Feature
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool ProcessServerSettings_Rcm_EnableAccessControl =>
            (bool)this[nameof(ProcessServerSettings_Rcm_EnableAccessControl)];

        #endregion RcmProcessServerSettings

        #region ServiceUtils Settings

        /// <summary>
        /// Timeout to wait until the requested service starts.
        /// 1 minute
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan Setup_DefaultServicesStartTimeout =>
            (TimeSpan)this[nameof(Setup_DefaultServicesStartTimeout)];

        /// <summary>
        /// Interval to poll, if the service attained the requested state.
        /// 5 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:05")]
        public TimeSpan Setup_DefaultServicesPollChangeTimeout =>
            (TimeSpan)this[nameof(Setup_DefaultServicesPollChangeTimeout)];

        /// <summary>
        /// Timeout to wait until the requested service stops.
        /// 2 minutes
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:02:00")]
        public TimeSpan Setup_DefaultServicesStopTimeout =>
            (TimeSpan)this[nameof(Setup_DefaultServicesStopTimeout)];

        /// <summary>
        /// Number of retries for starting the services.
        /// 3
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("3")]
        public int Setup_ServiceStartMaxRetryCnt =>
            (int)this[nameof(Setup_ServiceStartMaxRetryCnt)];
        /// <summary>
        /// Timeout before retrying service start.
        /// 5 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:05")]
        public TimeSpan Setup_ServiceStartRetryInterval =>
            (TimeSpan)this[nameof(Setup_ServiceStartRetryInterval)];

        #endregion ServiceUtils Settings

        #region PSInstallationInfo

        #region Registry

        /// <summary>
        /// If true, use 64-bit registry key. Otherwise, 32-bit.
        /// true
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("true")]
        public bool Registry_Use64BitRegView => (bool)this[nameof(Registry_Use64BitRegView)];

        /// <summary>
        /// Process Server registry key subpath under HKLM.
        /// Software\Microsoft\Azure Site Recovery Process Server
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue(@"Software\Microsoft\Azure Site Recovery Process Server")]
        public string Registry_ProcessServerKeySubPath =>
            (string)this[nameof(Registry_ProcessServerKeySubPath)];

        /// <summary>
        /// Install location of the process server.
        /// Install Location
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Install Location")]
        public string Registry_InstallLocation => (string)this[nameof(Registry_InstallLocation)];

        /// <summary>
        /// Version of the process server.
        /// Build_Version
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Build_Version")]
        public string Registry_Version => (string)this[nameof(Registry_Version)];

        /// <summary>
        /// Path of the configurator script.
        /// Configurator Path
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Configurator Path")]
        public string Registry_ConfiguratorPath => (string)this[nameof(Registry_ConfiguratorPath)];

        /// <summary>
        /// Path of the replication log folder.
        /// Log Folder Path
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Log Folder Path")]
        public string Registry_LogFolderPath => (string)this[nameof(Registry_LogFolderPath)];

        /// <summary>
        /// Path of the telemetry folder.
        /// Telemetry Folder Path
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Telemetry Folder Path")]
        public string Registry_TelemetryFolderPath => (string)this[nameof(Registry_TelemetryFolderPath)];

        /// <summary>
        /// Path of the folder under which source agents will work with dummy
        /// files in order to validate if their connectivity with the PS is good.
        /// Request Default Folder Path
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Request Default Folder Path")]
        public string Registry_ReqDefFolderPath => (string)this[nameof(Registry_ReqDefFolderPath)];

        /// <summary>
        /// Mode of the Process server.
        /// CS Mode
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("CS Mode")]
        public string Registry_CSMode => (string)this[nameof(Registry_CSMode)];

        /// <summary>
        /// First installation time of the Process server.
        /// Installed On UTC
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Installed On UTC")]
        public string Registry_InstalledOnUtc => (string)this[nameof(Registry_InstalledOnUtc)];

        /// <summary>
        /// Last updation time of the Process server.
        /// Last Updated On UTC
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Last Updated On UTC")]
        public string Registry_LastUpdatedOnUtc => (string)this[nameof(Registry_LastUpdatedOnUtc)];

        /// <summary>
        /// Path of the cached PS settings file.
        /// Settings Path
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Settings Path")]
        public string Registry_SettingsPath => (string)this[nameof(Registry_SettingsPath)];

        /// <summary>
        /// Path of the lock file for idempotency operations.
        /// Settings Path
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Idempotency Lock File Path")]
        public string Registry_IdempotencyLockFilePath => (string)this[nameof(Registry_IdempotencyLockFilePath)];

        /// <summary>
        /// Idempotency registry key subpath under HKLM.
        /// Software\Microsoft\Azure Site Recovery Process Server\Idempotency
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue(@"SOFTWARE\Microsoft\Azure Site Recovery Process Server\Idempotency")]
        public string Registry_IdempotencyKeySubPath =>
            (string)this[nameof(Registry_IdempotencyKeySubPath)];

        /// <summary>
        /// Registry key subpath under <c>Registry_IdempotencyKeySubPath</c> for
        /// files to be replaced as part of idempotency operations.
        /// FilesToReplace
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("FilesToReplace")]
        public string Registry_IdempotencyFilesToReplaceKeySubPath =>
            (string)this[nameof(Registry_IdempotencyFilesToReplaceKeySubPath)];

        /// <summary>
        /// Registry key subpath under <c>Registry_IdempotencyKeySubPath</c> for
        /// folders to be deleted as part of idempotency operations.
        /// FoldersToDelete
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("FoldersToDelete")]
        public string Registry_IdempotencyFoldersToDeleteKeySubPath =>
            (string)this[nameof(Registry_IdempotencyFoldersToDeleteKeySubPath)];

        /// <summary>
        /// Process Server monitoring registry key subpath under HKLM.
        /// Software\Microsoft\Azure Site Recovery Process Server\Monitoring
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue(@"SOFTWARE\Microsoft\Azure Site Recovery Process Server\Monitoring")]
        public string Registry_MonitoringKeySubPath =>
            (string)this[nameof(Registry_MonitoringKeySubPath)];

        /// <summary>
        /// Path of ps version.
        /// PSVersion
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("PSVersion")]
        public string Registry_PSVersion => (string)this[nameof(Registry_PSVersion)];

        /// <summary>
        /// Path of last reboot time.
        /// LastRebootTime
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("LastRebootTime")]
        public string Registry_LastRebootTime => (string)this[nameof(Registry_LastRebootTime)];
        #endregion Registry

        /// <summary>
        /// Time to wait before retrying to verify if the PS installation is valid.
        /// 1 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan PSInstallation_BlockingWaitRetryInterval =>
            (TimeSpan)this[nameof(PSInstallation_BlockingWaitRetryInterval)];

        #endregion PSInstallationInfo

        #region Process Server services

        /// <summary>
        /// Name of the ProcessServer service.
        /// ProcessServer
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("ProcessServer")]
        public string ServiceName_ProcessServer => (string)this[nameof(ServiceName_ProcessServer)];

        /// <summary>
        /// Name of the ProcessServerMonitor service.
        /// ProcessServerMonitor
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("ProcessServerMonitor")]
        public string ServiceName_ProcessServerMonitor =>
            (string)this[nameof(ServiceName_ProcessServerMonitor)];

        /// <summary>
        /// Name of the cxps service.
        /// cxprocessserver
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("cxprocessserver")]
        public string ServiceName_CXProcessServer => (string)this[nameof(ServiceName_CXProcessServer)];

        #endregion Process Server services

        #region MiscUtils

        /// <summary>
        /// If not Unknown, the process would verify if all DLLs correctly
        /// configured to be loaded for that corresponding mode.
        /// Unknown
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Unknown")]
        public CSMode MiscUtils_AllDllsLoadTest_CSMode =>
            (CSMode)this[nameof(MiscUtils_AllDllsLoadTest_CSMode)];

        #endregion MiscUtils

        #region RcmConfigurator

        /// <summary>
        /// Id to retrieve RcmProxyAgentPassphrase from Credential Store.
        /// RcmProxyAgentPassphrase
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("RcmProxyAgentPassphrase")]
        public string RcmConf_RcmProxyAgentPassphraseId =>
            (string)this[nameof(RcmConf_RcmProxyAgentPassphraseId)];

        /// <summary>
        /// Name of the subfolder created to hold the replication data from source machines in the PS.
        /// PSCache
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("PSCache")]
        public string RcmConf_ReplLogFolderName => (string)this[nameof(RcmConf_ReplLogFolderName)];

        /// <summary>
        /// Name of the subfolder created to hold logs and telemetry in the PS.
        /// PSTelemetry
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("PSTelemetry")]
        public string RcmConf_TelemetryFolderName => (string)this[nameof(RcmConf_TelemetryFolderName)];

        /// <summary>
        /// Name of the subfolder created to receive dummy files from source
        /// agent to test connectivity with the PS.
        /// PSReqDefDir
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("PSReqDefDir")]
        public string RcmConf_RequestDefaultFolderName => (string)this[nameof(RcmConf_RequestDefaultFolderName)];

        #endregion RcmConfigurator

        #region FileUtils

        /// <summary>
        /// Number of retries before giving up on acquiring lock over the .lck file
        /// 3
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("3")]
        public int FileUtils_AcquireLockRetryCnt => (int)this[nameof(FileUtils_AcquireLockRetryCnt)];

        /// <summary>
        /// Interval between retries to acquire lock over the .lck file
        /// 2 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:02")]
        public TimeSpan FileUtils_AcquireLockRetryInterval =>
            (TimeSpan)this[nameof(FileUtils_AcquireLockRetryInterval)];

        #endregion FileUtils

        #region PSConfiguration

        /// <summary>
        /// Time to wait before retrying to verify if the PS has been registered with RCM.
        /// 1 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan PSConf_RcmRegBlockingWaitRetryInterval =>
            (TimeSpan)this[nameof(PSConf_RcmRegBlockingWaitRetryInterval)];

        #endregion PSConfiguration

        #region Monitoring
        /// <summary>
        /// Ps Health and stats will be posted to event hub with this interval.
        /// This is used in RCM.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:05:00")]
        public TimeSpan RCM_WaitIntervalPSHealthMsg => (TimeSpan)this[nameof(RCM_WaitIntervalPSHealthMsg)];

        /// <summary>
        /// Number of retries to send sync messages. Where we want to ensure delivery.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("3")]
        public int EventHubWrapper_SyncMsgRetryCount => (int)this[nameof(EventHubWrapper_SyncMsgRetryCount)];

        /// <summary>
        /// Interval between successive retries to post message.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:05")]
        public TimeSpan EventHubWrapper_SyncMsgRetryInterval => (TimeSpan)this[nameof(EventHubWrapper_SyncMsgRetryInterval)];

        /// <summary>
        /// Threashold to raise upload blocked health.
        /// 30 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:30:00")]
        public TimeSpan Monitoring_UploadBlockedTimeThreshold => (TimeSpan)this[nameof(Monitoring_UploadBlockedTimeThreshold)];

        /// <summary>
        /// Threashold to raise replication blocked warning health.
        /// 30 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:30:00")]
        public TimeSpan Monitoring_ReplicationBlockedWarningThreshold => (TimeSpan)this[nameof(Monitoring_ReplicationBlockedWarningThreshold)];

        /// <summary>
        /// Threashold to raise upload blocked critical health.
        /// Default 1 hour.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("01:00:00")]
        public TimeSpan Monitoring_ReplicationBlockedCriticalThreshold => (TimeSpan)this[nameof(Monitoring_ReplicationBlockedCriticalThreshold)];

        /// <summary>
        /// Wait time between each iteration for health monitor service.
        /// 5 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:05:00")]
        public TimeSpan MonHealthServiceMainIterWaitInterval => (TimeSpan)this[nameof(MonHealthServiceMainIterWaitInterval)];

        #endregion Monitoring

        #region ServiceUtils Settings

        /// <summary>
        /// Time to wait, for the service to reach the specified status
        /// 5 seconds.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:05")]
        public TimeSpan PSMonitor_DefaultServicesPollChangeTimeout =>
            (TimeSpan)this[nameof(PSMonitor_DefaultServicesPollChangeTimeout)];

        /// <summary>
        /// Number of times we will poll the service to reach the specified status
        /// 12
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("12")]
        public int PSMonitor_DefaultServicesMaxRetryCount =>
            (int)this[nameof(PSMonitor_DefaultServicesMaxRetryCount)];

        /// <summary>
        /// Number of times service start will be attempted for renew certs
        /// 10
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("10")]
        public int PSTask_RenewCertsStartServiceMaxRetryCount =>
            (int)this[nameof(PSTask_RenewCertsStartServiceMaxRetryCount)];

        /// <summary>
        /// Time to wait, for the service to reach the start status for renew certs
        /// 10 seconds.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:10")]
        public TimeSpan PSTask_RenewCertsStartServicePollChangeTimeout =>
            (TimeSpan)this[nameof(PSTask_RenewCertsStartServicePollChangeTimeout)];

        /// <summary>
        /// Number of times service stop will be attempted for renew certs
        /// 5
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("5")]
        public int PSTask_RenewCertsStopServiceMaxRetryCount =>
            (int)this[nameof(PSTask_RenewCertsStopServiceMaxRetryCount)];

        /// <summary>
        /// Time to wait, for the service to reach the stop status for renew certs
        /// 30 seconds.
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:30")]
        public TimeSpan PSTask_RenewCertsStopServicePollChangeTimeout =>
            (TimeSpan)this[nameof(PSTask_RenewCertsStopServicePollChangeTimeout)];

        #endregion ServiceUtils Settings

        #region RestWrapper Settings

        /// <summary>
        /// If set, logs the response of the post request call
        /// false
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("false")]
        public bool RestWrapper_LogPostCallResponse =>
            (bool)this[nameof(RestWrapper_LogPostCallResponse)];

        /// <summary>
        /// If set, use Azure ServiceBus ConnectivityMode and SecurityProtocol settings
        /// for network connections
        /// false
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("false")]
        public bool RestWrapper_SetAzureServiceBusOptions =>
            (bool)this[nameof(RestWrapper_SetAzureServiceBusOptions)];

        /// <summary>
        /// Security Protocol used for network connections.
        /// Tls12
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("Tls12")]
        public string RestWrapper_SecurityProtocol => (string)this[nameof(RestWrapper_SecurityProtocol)];

        #endregion RestWrapper Settings

        #region Rcm Job Processor

        /// <summary>
        /// Length of various queues in Rcm Job Processor
        /// 1000
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("1000")]
        public int RcmJobProc_QueueLen => (int)this[nameof(RcmJobProc_QueueLen)];

        /// <summary>
        /// Timeout before retrying to update the job status to Rcm
        /// 30 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:30")]
        public TimeSpan RcmJobProc_UpdateJobStatusRetryInterval =>
            (TimeSpan)this[nameof(RcmJobProc_UpdateJobStatusRetryInterval)];

        /// <summary>
        /// True, if the paths to be operated by RcmJobProcessor should be
        /// converted to long path format optimally.
        /// false
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("false")]
        public bool RcmJobProc_LongPathOptimalConversion =>
            (bool)this[nameof(RcmJobProc_LongPathOptimalConversion)];

        /// <summary>
        /// Number of retries in recursive folder deletion in Rcm Job Processor
        /// 15
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("15")]
        public int RcmJobProc_RecDelFolderMaxRetryCnt =>
            (int)this[nameof(RcmJobProc_RecDelFolderMaxRetryCnt)];

        /// <summary>
        /// Timeout before retrying recursive folder deletion in Rcm Job Processor
        /// 5 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:05")]
        public TimeSpan RcmJobProc_RecDelFolderRetryInterval =>
            (TimeSpan)this[nameof(RcmJobProc_RecDelFolderRetryInterval)];

        /// <summary>
        /// Number of retries in recursive folder deletion in Rcm Job Processor
        /// 3
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("3")]
        public int RcmJobProc_DelFolderMaxRetryCnt =>
            (int)this[nameof(RcmJobProc_DelFolderMaxRetryCnt)];

        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:01")]
        public TimeSpan RcmJobProc_HostMonitoringSettingsRetryInterval =>
            (TimeSpan)this[nameof(RcmJobProc_HostMonitoringSettingsRetryInterval)];

        [ApplicationScopedSetting]
        [DefaultSettingValue("00:30:00")]
        public TimeSpan RcmJobProc_HostMonitoringSettingsRetryTimeout =>
            (TimeSpan)this[nameof(RcmJobProc_HostMonitoringSettingsRetryTimeout)];

        /// <summary>
        /// Number of retries in recursive dir creation in Prepare for Sync Rcm Job
        /// 3
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("3")]
        public int RcmJobProc_CreateDirMaxRetryCnt =>
            (int)this[nameof(RcmJobProc_CreateDirMaxRetryCnt)];

        /// <summary>
        /// Timeout before retrying recursive folder creation in Prepare for Sync Rcm Job
        /// 1 sec
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:01")]
        public TimeSpan RcmJobProc_CreateDirRetryInterval =>
            (TimeSpan)this[nameof(RcmJobProc_CreateDirRetryInterval)];



        #endregion Rcm Job Processor

        #region AppInsights TraceListener

        /// <summary>
        /// Time to wait on closing in order to ensure that the messages reach
        /// the cloud after flush has been triggered.
        /// 5 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:05")]
        public TimeSpan AITraceListener_WaitTimeForFlush =>
            (TimeSpan)this[nameof(AITraceListener_WaitTimeForFlush)];

        #endregion AppInsights TraceListener

        #region TaskManager

        /// <summary>
        /// Number of retries in recursive folder deletion in PS Task Manager
        /// 3
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("3")]
        public int TaskMgr_RecDelFolderMaxRetryCnt =>
            (int)this[nameof(TaskMgr_RecDelFolderMaxRetryCnt)];

        /// <summary>
        /// Timeout before retrying recursive folder deletion in PS Task Manager
        /// 5 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:05")]
        public TimeSpan TaskMgr_RecDelFolderRetryInterval =>
            (TimeSpan)this[nameof(TaskMgr_RecDelFolderRetryInterval)];

        /// <summary>
        /// Number of retries in file deletion in PS TaskManager
        /// 3
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("3")]
        public int TaskMgr_DeleteFileMaxRetryCnt =>
            (int)this[nameof(TaskMgr_DeleteFileMaxRetryCnt)];

        /// <summary>
        /// Wait time in seconds before retrying file deletion in PS TaskManager
        /// 5 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:05")]
        public TimeSpan TaskMgr_DeleteFileRetryInterval =>
            (TimeSpan)this[nameof(TaskMgr_DeleteFileRetryInterval)];

        /// <summary>
        /// Max number of file deletion errors to log,
        /// beyond which the logs will be rate controlled
        /// 5
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("5")]
        public int TaskMgr_MaxLimitForRateControllingFileDelExLogging =>
            (int)this[nameof(TaskMgr_MaxLimitForRateControllingFileDelExLogging)];

        /// <summary>
        /// Number of retries in PS backup cert file deletion in PS TaskManager
        /// 5
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("5")]
        public int TaskMgr_CertFileMaxRetryCnt =>
            (int)this[nameof(TaskMgr_CertFileMaxRetryCnt)];

        /// <summary>
        /// Wait time in seconds before retrying PS backup Cert file deletion in PS TaskManager
        /// 1 seconds
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:00:01")]
        public TimeSpan TaskMgr_CertFileRetryInterval =>
            (TimeSpan)this[nameof(TaskMgr_CertFileRetryInterval)];

        /// <summary>
        /// The wait interval between every Task Manager main iteration
        /// 1 min
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("00:01:00")]
        public TimeSpan TaskMgr_MainIterWaitInterval =>
            (TimeSpan)this[nameof(TaskMgr_MainIterWaitInterval)];

        /// <summary>
        /// Interval after which stuck Legacy PS tasks should be terminated.
        /// 2 hours
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("02:00:00")]
        public TimeSpan TaskMgr_StuckTaskCancellationInterval =>
            (TimeSpan)this[nameof(TaskMgr_StuckTaskCancellationInterval)];

        /// <summary>
        /// Max batch count in perf files upload in PS TaskManager
        /// 25
        /// </summary>
        [ApplicationScopedSetting]
        [DefaultSettingValue("25")]
        public int PerfFileMaxCount =>
            (int)this[nameof(PerfFileMaxCount)];

        #endregion TaskManager

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
