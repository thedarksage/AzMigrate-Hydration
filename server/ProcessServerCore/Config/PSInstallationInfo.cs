using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Setup;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Security.AccessControl;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config
{
    /// <summary>
    /// Mode in which the Process server is installed
    /// </summary>
    public enum CSMode
    {
        /// <summary>
        /// Mode couldn't be determined
        /// </summary>
        Unknown,

        /// <summary>
        /// Legacy V2A CS stack
        /// </summary>
        LegacyCS,

        /// <summary>
        /// V2A Rcm stack
        /// </summary>
        Rcm
    }

    // NOTE-SanKumar-1904: Intentionally not maintaining thread safety, since this
    // is the object expected to be loaded and waited on for validity at the
    // beginning of the service.

    /// <summary>
    /// Installation details of the Process Server
    /// </summary>
    public class PSInstallationInfo
    {
        /// <summary>
        /// Singleton object
        /// </summary>
        public static PSInstallationInfo Default { get; } = new PSInstallationInfo();

        /// <summary>
        /// True, if process server is installed on the machine
        /// </summary>
        public bool IsInstalled { get; private set; }

        /// <summary>
        /// Mode in which the Process server is installed
        /// </summary>
        public CSMode CSMode { get; private set; }

        /// <summary>
        /// Install location of the Process server
        /// </summary>
        private string InstallLocation { get; set; }

        /// <summary>
        /// Replication logs folder path, where the agent will put the files and
        /// MT' will pick the files from.
        /// </summary>
        private string LogFolderPath { get; set; }

        /// <summary>
        /// Folder under which source agents, PS and MT` components will be
        /// staging their logs and telemetry files to be uploaded to Kusto.
        /// </summary>
        private string TelemetryFolderPath { get; set; }

        /// <summary>
        /// Folder under which source agents will work with dummy files in order
        /// to validate if their connectivity with the PS is good.
        /// </summary>
        private string ReqDefFolderPath { get; set; }

        // TODO-SanKumar-2004: The version property is currently valid only for
        // RCM mode, even though we mean this class to be used across modes. For
        // example, reading version of LegacyCS mode PS must be done by reading
        // the Version file [see LegacyCSUtils.cs - GetPSBuildVersionString()].
        // Same applies to InstalledOnUtc and LastUpdatedOnUtc.
        // There should be Get*() methods for these properties abstracting the
        // mode and these must be made private, like other properties in this class.
        /// <summary>
        /// Build version of the installed Process server
        /// </summary>
        private string Version { get; set; }

        /// <summary>
        /// Time when the Process server was first installed
        /// </summary>
        public DateTime? InstalledOnUtc { get; private set; }

        /// <summary>
        /// Time when the Process server was last updated
        /// </summary>
        public DateTime? LastUpdatedOnUtc { get; private set; }

        /// <summary>
        /// Path to the configurator PowerShell script file
        /// </summary>
        private string ConfiguratorPath { get; set; }

        /// <summary>
        /// Path to the cached process server settings
        /// </summary>
        private string SettingsPath { get; set; }

        /// <summary>
        /// Path to the lock file used for idempotency operations
        /// </summary>
        private string IdempotencyLockFilePath { get; set; }

        /// <summary>
        /// Constructor. Meant only for internal use.
        /// </summary>
        private PSInstallationInfo()
        {
            Clear();

            try
            {
                Reload();
            }
            catch
            {
                // NOTE-SanKumar-1904: Don't log here or anywhere else in this
                // class, since its values are the starting parameters for loggers.
            }
        }

        private static RegistryKey OpenRegistryKey(string keyName, bool writable)
        {
            var regViewToUse = Settings.Default.Registry_Use64BitRegView ?
                   RegistryView.Registry64 : RegistryView.Registry32;

            using (var regBaseObj = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, regViewToUse))
                return regBaseObj.OpenSubKey(keyName, writable);
        }

        public static RegistryKey CreateOrOpenRegistryKey(string keyName, bool writable)
        {
            var regViewToUse = Settings.Default.Registry_Use64BitRegView ? RegistryView.Registry64 : RegistryView.Registry32;
            RegistryKey localKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, regViewToUse);
            RegistryKey key = localKey.OpenSubKey(keyName, writable);
            if (key != null)
            {
                return key;
            }

            return localKey.CreateSubKey(keyName, writable);
        }

        internal void Reload()
        {
            string asrPSRegKeySubPath = Settings.Default.Registry_ProcessServerKeySubPath;
            using (var regObj = OpenRegistryKey(asrPSRegKeySubPath, writable: false))
            {
                if (regObj.GetValue(Settings.Default.Registry_CSMode) is string csModeStr &&
                    Enum.TryParse(csModeStr, true, out CSMode parsedCSMode))
                {
                    CSMode = parsedCSMode;
                }
                else
                {
                    CSMode = CSMode.Unknown;
                }

                if (CSMode == CSMode.LegacyCS)
                {
                    Clear();
                    CSMode = CSMode.LegacyCS;

                    // Returning as LegacyPS installer doesn't create/use other registry keys yet.
                    return;
                }

                if (CSMode != CSMode.Rcm)
                {
                    return;
                }

                // Process Server can be assumed to be installed, if the registry
                // key is present.
                IsInstalled = true;

                // NOTE-SanKumar-1904: The "is" checks below also automatically
                // checks for non-null values. Also, note that we are trying to
                // avoid exceptions.

                if (regObj.GetValue(Settings.Default.Registry_InstallLocation) is string installLoc)
                    InstallLocation = Environment.ExpandEnvironmentVariables(installLoc);
                else
                    InstallLocation = null;

                Version = regObj.GetValue(Settings.Default.Registry_Version) as string;

                if (regObj.GetValue(Settings.Default.Registry_LogFolderPath) is string logFolder)
                    LogFolderPath = Environment.ExpandEnvironmentVariables(logFolder);
                else
                    LogFolderPath = null;

                if (regObj.GetValue(Settings.Default.Registry_TelemetryFolderPath) is string telemetryFolder)
                    TelemetryFolderPath = Environment.ExpandEnvironmentVariables(telemetryFolder);
                else
                    TelemetryFolderPath = null;

                if (regObj.GetValue(Settings.Default.Registry_ReqDefFolderPath) is string reqDefFolder)
                    ReqDefFolderPath = Environment.ExpandEnvironmentVariables(reqDefFolder);
                else
                    ReqDefFolderPath = null;

                if (regObj.GetValue(Settings.Default.Registry_InstalledOnUtc) is string installedOnUtcStr &&
                    DateTime.TryParse(installedOnUtcStr, out DateTime parsedInstalledOnUtc))
                {
                    InstalledOnUtc = parsedInstalledOnUtc;
                }
                else
                {
                    InstalledOnUtc = null;
                }

                if (regObj.GetValue(Settings.Default.Registry_LastUpdatedOnUtc) is string lastUpdatedOnUtcStr &&
                    DateTime.TryParse(lastUpdatedOnUtcStr, out DateTime parsedLastUpdatedOnUtc))
                {
                    LastUpdatedOnUtc = parsedLastUpdatedOnUtc;
                }
                else
                {
                    LastUpdatedOnUtc = null;
                }

                if (regObj.GetValue(Settings.Default.Registry_ConfiguratorPath) is string cfgtrPath)
                    ConfiguratorPath = Environment.ExpandEnvironmentVariables(cfgtrPath);
                else
                    ConfiguratorPath = null;

                if (regObj.GetValue(Settings.Default.Registry_SettingsPath) is string settingsPathStr)
                    SettingsPath = Environment.ExpandEnvironmentVariables(settingsPathStr);
                else
                    SettingsPath = null;

                if (regObj.GetValue(Settings.Default.Registry_IdempotencyLockFilePath) is string idempotencyPathStr)
                    IdempotencyLockFilePath = Environment.ExpandEnvironmentVariables(idempotencyPathStr);
                else
                    IdempotencyLockFilePath = null;
            }
        }

        private void Clear()
        {
            IsInstalled = false;

            InstallLocation = null;
            Version = null;
            LogFolderPath = null;
            TelemetryFolderPath = null;
            ReqDefFolderPath = null;
            CSMode = CSMode.Unknown;
            InstalledOnUtc = null;
            LastUpdatedOnUtc = null;
            ConfiguratorPath = null;
            SettingsPath = null;
            IdempotencyLockFilePath = null;
        }

        public bool IsValidInfo(bool includeFirstTimeConfigure)
        {
            Reload();

            return
                !string.IsNullOrWhiteSpace(InstallLocation) && // Install
                !string.IsNullOrWhiteSpace(SettingsPath) && // Install
                !string.IsNullOrWhiteSpace(IdempotencyLockFilePath) && // Install
                !string.IsNullOrWhiteSpace(Version) && // Install/upgrade
                CSMode == CSMode.Rcm && // Install
                ((!includeFirstTimeConfigure) ||
                 (!string.IsNullOrWhiteSpace(LogFolderPath) && // FirstTimeConfigure
                  !string.IsNullOrWhiteSpace(TelemetryFolderPath) && // FirstTimeConfigure
                  !string.IsNullOrWhiteSpace(ReqDefFolderPath))); // FirstTimeConfigure

            // Ignoring the following values in validation:
            // 1. ConfiguratorPath - Since it's not required by the PS but only by the external components.
            // 2. InstalledOnUtc - Informational. Not important for the PS services to run.
            // 3. LastUpdatedOnUtc - Informational. Not important for the PS services to run.
        }

        public async Task BlockUntilValidInstallationDetected(bool includeFirstTimeConfigure, CancellationToken ct)
        {
            for (; ; )
            {
                try
                {
                    // TODO-SanKumar-1904: This has to change, when the LegacyCS
                    // mode is also installed using the new PS installer. Until
                    // then, we could probably add more checks to the LegacyCS
                    // mode, such as valid installation path from App.Config, etc.

                    // Unlike the Rcm mode PS, the LegacyCS mode PS is at the
                    // moment doesn't use the registry key for installation
                    // details. So, quit here, if we are able to detect that the
                    // registry key is missing.
                    if (CSMode == CSMode.LegacyCS)
                        return;

                    if (IsValidInfo(includeFirstTimeConfigure))
                        return;
                }
                catch (Exception)
                {
                    // NOTE-SanKumar-1904: Ignoring the exception for now. We
                    // may not be able to log, as the install directory may not
                    // be understood yet.

                    // TODO-SanKumar-1904: Should we write the failure to the
                    // event log of the service?
                }

                await Task.Delay(
                    Settings.Default.PSInstallation_BlockingWaitRetryInterval, ct).
                    ConfigureAwait(false);
            }
        }

        internal static void UpdateFolderPaths(
            IdempotencyContext idempContext,
            string logFolderPath,
            string telemetryFolderPath,
            string reqDefFolderPath)
        {
            using (var regObj = idempContext.GetPSRegistryKeyTransacted(RegistryRights.WriteKey))
            {
                if (regObj == null)
                {
                    throw new KeyNotFoundException("PS Installation key couldn't be found");
                }

                regObj.SetValue(
                    Settings.Default.Registry_LogFolderPath,
                    logFolderPath,
                    RegistryValueKind.ExpandString);

                regObj.SetValue(
                    Settings.Default.Registry_TelemetryFolderPath,
                    telemetryFolderPath,
                    RegistryValueKind.ExpandString);

                regObj.SetValue(
                    Settings.Default.Registry_ReqDefFolderPath,
                    reqDefFolderPath,
                    RegistryValueKind.ExpandString);
            }
        }

        internal static void ClearFolderPaths(
            IdempotencyContext idempContext,
            TraceSource traceSource = null)
        {
            traceSource?.TraceAdminLogV2Message(
                TraceEventType.Information,
                "Deleting the cache location folder path values in the registry");

            using (var regObj = idempContext.GetPSRegistryKeyTransacted(RegistryRights.WriteKey))
            {
                if (regObj == null)
                {
                    traceSource?.TraceAdminLogV2Message(
                        TraceEventType.Information, "Couldn't find the PS registry key");

                    return;
                }

                var regValuesToDel = new string[]
                {
                    Settings.Default.Registry_LogFolderPath,
                    Settings.Default.Registry_TelemetryFolderPath,
                    Settings.Default.Registry_ReqDefFolderPath
                };

                foreach (var currValToDel in regValuesToDel)
                {
                    traceSource?.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Deleting the registry value : {0} under PS registry key",
                        currValToDel);

                    regObj.DeleteValue(currValToDel, throwOnMissingValue: false);

                    traceSource?.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Successfully deleted the registry value : {0} under PS registry key",
                        currValToDel);
                }
            }

            traceSource?.TraceAdminLogV2Message(
                TraceEventType.Information,
                "Successfuly deleted the cache location folder path values in the registry");
        }

        // TODO-SanKumar-1904: Even the legacy PS, should use this PSInstallationInfo
        // object for retrieving these same paths. All of them must be abstracted
        // by this object and same goes with waiting for validity, etc.

        public string GetBinFolderPath() => FSUtils.CanonicalizePath(
            Path.Combine(GetRootFolderPath(), Settings.Default.CommonMode_SubPath_Bin));

        public string GetProcessServerExePath() => FSUtils.CanonicalizePath(
            Path.Combine(GetRootFolderPath(), Settings.Default.CommonMode_SubPath_ProcessServerExe));

        public string GetProcessServerMonitorExePath() => FSUtils.CanonicalizePath(
            Path.Combine(GetRootFolderPath(), Settings.Default.Common_Mode_SubPath_ProcessServerMonitorExe));

        public string GetTransportFolderPath() => FSUtils.CanonicalizePath(
            Path.Combine(GetRootFolderPath(), Settings.Default.CommonMode_SubPath_Transport));

        public string GetVarFolderPath() => FSUtils.CanonicalizePath(
            Path.Combine(GetRootFolderPath(), Settings.Default.CommonMode_SubPath_Var));

        public string GetTransportPrivateFolderPath()
        {
            var rootFolderPath = GetRootFolderPath();

            if (CSMode != CSMode.Rcm)
                throw new NotSupportedException($"{CSMode} mode doesn't support transport private folder yet");

            return FSUtils.CanonicalizePath(
                Path.Combine(rootFolderPath, Settings.Default.Rcm_SubPath_TransportPrivate));
        }

        public string GetCxpsExePath() => FSUtils.CanonicalizePath(
            Path.Combine(GetRootFolderPath(), Settings.Default.CommonMode_SubPath_CxpsExe));

        public string GetCxpsConfigPath() => FSUtils.CanonicalizePath(
            Path.Combine(GetRootFolderPath(), Settings.Default.CommonMode_SubPath_CxpsConfig));

        public string GetCxpsGoldenConfigPath()
        {
            string toRetPath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    string cxpsConfigPath = GetCxpsConfigPath();
                    toRetPath = Path.Combine(
                        Path.GetDirectoryName(cxpsConfigPath),
                        $"{Path.GetFileNameWithoutExtension(cxpsConfigPath)}_golden{Path.GetExtension(cxpsConfigPath)}");

                    break;

                case CSMode.LegacyCS:
                case CSMode.Unknown:
                default:
                    throw new NotSupportedException($"CSMode {CSMode} doesn't support cxps golden config path");
            }

            return FSUtils.CanonicalizePath(toRetPath);
        }

        public string GetGenCertExePath() => FSUtils.CanonicalizePath(
            Path.Combine(GetRootFolderPath(), Settings.Default.CommonMode_SubPath_GenCertExe));

        public int GetSSLPort() => CxpsConfig.Default.SslPort.Value;

        public int GetNonSSLPort() => CxpsConfig.Default.Port.Value;

        public string GetPSConfigPath()
        {
            var rootFolderPath = GetRootFolderPath();

            if (CSMode != CSMode.Rcm)
                throw new NotSupportedException($"{CSMode} mode doesn't support PS Configuration yet");

            return FSUtils.CanonicalizePath(
                Path.Combine(rootFolderPath, Settings.Default.CommonMode_SubPath_PSConfig));
        }

        public string GetRcmPSConfAppInsightsBackupFolderPath()
        {
            var rootFolderPath = GetRootFolderPath();

            if (CSMode != CSMode.Rcm)
                throw new NotSupportedException($"{CSMode} doesn't support Rcm PS Configurator Application Insights backup folder path");

            return FSUtils.CanonicalizePath(
                Path.Combine(rootFolderPath, Settings.Default.Rcm_SubPath_RcmPSConfAppInsightsBackup));
        }

        public string GetRcmPSConfLogFolderPath()
        {
            var rootFolderPath = GetRootFolderPath();

            if (CSMode != CSMode.Rcm)
                throw new NotSupportedException($"{CSMode} doesn't support Rcm PS Configurator logs folder path");

            return FSUtils.CanonicalizePath(
                Path.Combine(rootFolderPath, Settings.Default.Rcm_SubPath_RcmPSConfLogs));
        }

        public string GetInstallationPath()
        {
            string toRetPath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    toRetPath = InstallLocation;
                    break;

                case CSMode.LegacyCS:
                    toRetPath = AppConfig.Default.InstallationPath;
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return FSUtils.CanonicalizePath(toRetPath);
        }

        public string GetRootFolderPath()
        {
            string toRetPath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    if (string.IsNullOrWhiteSpace(InstallLocation))
                        throw new Exception("PS Install path couldn't be determined");

                    // TODO-SanKumar-1909: In legacy CS mode, the binaries and the replication log
                    // folder resides under the same path. This is currently the case in Rcm mode as
                    // well. We will be moving the replication folder out of the binaries folder.
                    // Also, there's no need for home/svsystems folder path anymore. Binaries would
                    // be installed on the installed directory itself. i.e. InstallLoc == RootFolder
                    toRetPath = Path.Combine(InstallLocation, Settings.Default.Rcm_SubPath_Root);
                    break;

                case CSMode.LegacyCS:
                    toRetPath = Path.Combine(
                        AppConfig.Default.InstallationPath, Settings.Default.LegacyCS_SubPath_Root);
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return FSUtils.CanonicalizePath(toRetPath);
        }

        public string GetReplicationLogFolderPath()
        {
            string toRetPath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    toRetPath = LogFolderPath;
                    break;

                case CSMode.LegacyCS:
                    toRetPath = Path.Combine(
                        AppConfig.Default.InstallationPath, Settings.Default.LegacyCS_SubPath_Root);
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return FSUtils.CanonicalizePath(toRetPath);
        }

        public string GetTelemetryFolderPath()
        {
            string toRetPath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    toRetPath = TelemetryFolderPath;
                    break;

                case CSMode.LegacyCS:
                case CSMode.Unknown:
                default:
                    throw new NotSupportedException($"CSMode {CSMode} doesn't support Telemetry Folder path");
            }

            return FSUtils.CanonicalizePath(toRetPath);
        }

        public string GetReqDefFolderPath()
        {
            string toRetPath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    toRetPath = ReqDefFolderPath;
                    break;

                case CSMode.LegacyCS:
                    toRetPath = CxpsConfig.Default.RequestDefaultDir;

                    if (string.IsNullOrEmpty(toRetPath))
                    {
                        toRetPath = Path.Combine(
                            AppConfig.Default.InstallationPath,
                            Settings.Default.LegacyCS_SubPath_ReqDefDir);
                    }

                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return FSUtils.CanonicalizePath(toRetPath);
        }

        public string GetPSCachedSettingsFilePath()
        {
            string toRet;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    toRet = SettingsPath;
                    break;

                case CSMode.LegacyCS:
                    toRet = Path.Combine(
                        AppConfig.Default.InstallationPath,
                        Settings.Default.LegacyCS_SubPath_Root,
                        Settings.Default.LegacyCS_SubPath_PSSettings);
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return FSUtils.CanonicalizePath(toRet);
        }

        public string GetIdempotencyLockFilePath()
        {
            string toRet;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    toRet = IdempotencyLockFilePath;
                    break;

                case CSMode.LegacyCS:
                    throw new NotSupportedException($"CSMode {CSMode} is not supported");

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return FSUtils.CanonicalizePath(toRet);
        }

        public string GetPSHostId()
        {
            string hostId;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    hostId = PSConfiguration.Default.HostId;
                    break;

                case CSMode.LegacyCS:
                    hostId = AmethystConfig.Default.HostGuid;
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return hostId;
        }

        public IPAddress GetPSIPAddress()
        {
            IPAddress psIpAddress;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    throw new NotImplementedException($"CSMode {CSMode} is not yet implemented");

                case CSMode.LegacyCS:
                    psIpAddress = AmethystConfig.Default.PS_IP;
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return psIpAddress;
        }

        public string GetMonitorFolderPath()
        {
            string toRetPath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    throw new NotSupportedException($"CSMode {CSMode} doesn't support monitor folder path");

                case CSMode.LegacyCS:
                    toRetPath = Path.Combine(
                        GetRootFolderPath(), Settings.Default.LegacyCS_SubPath_Monitor);
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return FSUtils.CanonicalizePath(toRetPath);
        }

        public int GetMaxTmid()
        {
            int maxTmid;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    throw new NotSupportedException($"CSMode {CSMode} doesn't support {nameof(GetMaxTmid)}");

                case CSMode.LegacyCS:
                    maxTmid = AmethystConfig.Default.MaxTmId;
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return maxTmid;
        }

        public TimeSpan GetSourceUploadBlockedDuration()
        {
            TimeSpan sourceFailureDuration;
            switch (CSMode)
            {
                case CSMode.Rcm:
                    sourceFailureDuration = Settings.Default.Monitoring_UploadBlockedTimeThreshold;
                    break;

                case CSMode.LegacyCS:
                    sourceFailureDuration = AmethystConfig.Default.GetSourceUploadBlockedDuration();
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }
            return sourceFailureDuration;
        }

        public TimeSpan GetIRDRStuckCriticalThreshold()
        {
            TimeSpan IRDRStuckCriticalThreshold;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    IRDRStuckCriticalThreshold = Settings.Default.Monitoring_ReplicationBlockedCriticalThreshold;
                    break;

                case CSMode.LegacyCS:
                    IRDRStuckCriticalThreshold = AmethystConfig.Default.GetIRDRStuckCriticalThreshold();
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }
            return IRDRStuckCriticalThreshold;
        }

        public TimeSpan GetIRDRStuckWarningThreshold()
        {
            TimeSpan IRDRStuckWarningThreshold;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    IRDRStuckWarningThreshold = Settings.Default.Monitoring_ReplicationBlockedWarningThreshold;
                    break;

                case CSMode.LegacyCS:
                    IRDRStuckWarningThreshold = AmethystConfig.Default.GetIRDRStuckWarningThreshold();
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }
            return IRDRStuckWarningThreshold;
        }

        public string GetPSCurrentVersion()
        {
            string psVersion;

            // TODO : Sirisha: Revisit later to check if it could be optimized
            // by reading the file only if psVersion is empty or
            // if last mod time changed for the file? Or Lazy ? Or
            // reuse the Version property for this ?
             switch (CSMode)
            {
                case CSMode.Rcm:
                    psVersion = Version;
                    break;

                case CSMode.LegacyCS:
                    psVersion = LegacyCSUtils.GetPSBuildVersionString();
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return psVersion;
        }

        public string GetCSCertificateFilePath()
        {
            string csCertFilePath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    throw new NotSupportedException(
                        $"CSMode {CSMode} doesn't support {nameof(GetCSCertificateFilePath)}");

                case CSMode.LegacyCS:
                    csCertFilePath = FSUtils.CanonicalizePath(LegacyCSUtils.GetCSCertificateFilePath());
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return csCertFilePath;
        }

        public string GetPSCertificateFilePath()
        {
            string psCertFilePath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    psCertFilePath = FSUtils.CanonicalizePath(
                        Path.Combine(GetTransportPrivateFolderPath(), "ps.crt"));
                    break;

                case CSMode.LegacyCS:
                    psCertFilePath = FSUtils.CanonicalizePath(
                        Path.Combine(Settings.Default.LegacyCS_FullPath_Certs, "ps.crt"));
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return psCertFilePath;
        }

        public string GetPSFingerprintFilePath()
        {
            string psFingerPrintFilePath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    psFingerPrintFilePath = FSUtils.CanonicalizePath(
                        Path.Combine(GetTransportPrivateFolderPath(), "ps.fingerprint"));
                    break;

                case CSMode.LegacyCS:
                    psFingerPrintFilePath = FSUtils.CanonicalizePath(
                        Path.Combine(Settings.Default.LegacyCS_FullPath_Fingerprints, "ps.fingerprint"));
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return psFingerPrintFilePath;
        }

        public string GetPSKeyFilePath()
        {
            string psKeyFilePath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    psKeyFilePath = FSUtils.CanonicalizePath(
                        Path.Combine(GetTransportPrivateFolderPath(), "ps.key"));
                    break;

                case CSMode.LegacyCS:
                    psKeyFilePath = FSUtils.CanonicalizePath(
                        Path.Combine(Settings.Default.LegacyCS_FullPath_Private, "ps.key"));
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return psKeyFilePath;
        }

        public string GetPSDhFilePath()
        {
            string psDhFilePath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    psDhFilePath = FSUtils.CanonicalizePath(
                        Path.Combine(GetTransportPrivateFolderPath(), "ps.dh"));
                    break;

                case CSMode.LegacyCS:
                    psDhFilePath = FSUtils.CanonicalizePath(
                        Path.Combine(Settings.Default.LegacyCS_FullPath_Private, "ps.dh"));
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return psDhFilePath;
        }

        public CXType GetCSType()
        {
            CXType csType;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    throw new NotSupportedException($"CSMode {CSMode} doesn't support {nameof(GetCSType)}");

                case CSMode.LegacyCS:
                    csType = AmethystConfig.Default.CXType;
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return csType;
        }

        public string GetAccountId()
        {
            string accountId;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    throw new NotSupportedException(
                        $"CSMode {CSMode} doesn't support {nameof(GetAccountId)}");

                case CSMode.LegacyCS:
                    accountId = AmethystConfig.Default.AccountId;
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return accountId;
        }

        public string GetAccountType()
        {
            string accountType;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    throw new NotSupportedException(
                        $"CSMode {CSMode} doesn't support {nameof(GetAccountType)}");

                case CSMode.LegacyCS:
                    accountType = AmethystConfig.Default.AccountType;
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return accountType;
        }

        public string GetPSInstallationDate()
        {
            string psInstallationDate;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    throw new NotSupportedException(
                        $"CSMode {CSMode} doesn't support {nameof(GetPSInstallationDate)}");

                case CSMode.LegacyCS:
                    psInstallationDate = LegacyCSUtils.GetPSInstallationDate();
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return psInstallationDate;
        }

        public (string, string) GetPSPatchDetails()
        {
            string psPatchVersion, psPatchInstallTime;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    throw new NotSupportedException(
                        $"CSMode {CSMode} doesn't support {nameof(GetPSPatchDetails)}");

                case CSMode.LegacyCS:
                    (psPatchVersion, psPatchInstallTime)  = LegacyCSUtils.GetPSPatchDetails();
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return (psPatchVersion, psPatchInstallTime);
        }

        public string GetNicFromAmethystConf()
        {
            string nicName;
            switch (CSMode)
            {
                case CSMode.Rcm:
                    throw new NotSupportedException(
                        $"CSMode {CSMode} doesn't support {nameof(GetNicFromAmethystConf)}");

                case CSMode.LegacyCS:
                    nicName = AmethystConfig.Default.BpmNetworkDevice;
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return nicName;
        }

        public string GetChurnStatFolderPath()
        {
            string toRetPath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    throw new NotSupportedException(
                        $"CSMode {CSMode} doesn't support {nameof(GetChurnStatFolderPath)}");

                case CSMode.LegacyCS:
                    toRetPath = FSUtils.CanonicalizePath(
                        Path.Combine(GetRootFolderPath(), Settings.Default.CommonMode_SubPath_ChurStat));
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return toRetPath;
        }

        public string GetThrpStatFolderPath()
        {
            string toRetPath;

            switch (CSMode)
            {
                case CSMode.Rcm:
                    throw new NotSupportedException(
                        $"CSMode {CSMode} doesn't support {nameof(GetThrpStatFolderPath)}");

                case CSMode.LegacyCS:
                    toRetPath = FSUtils.CanonicalizePath(
                        Path.Combine(GetRootFolderPath(), Settings.Default.CommonMode_SubPath_ThrpStat));
                    break;

                case CSMode.Unknown:
                    throw new Exception("Invalid CS mode");

                default:
                    throw new NotImplementedException($"CSMode {CSMode} is not implemented");
            }

            return toRetPath;
        }

        public void SetMonitoringLastRebootTime(long rebootTime)
        {
            using (RegistryKey key = CreateOrOpenRegistryKey(Settings.Default.Registry_MonitoringKeySubPath, writable: true))
            {
                if (key == null)
                {
                    throw new KeyNotFoundException("PS monitoring key couldn't be found and created");
                }

                key.SetValue(Settings.Default.Registry_LastRebootTime, rebootTime);
            }
        }

        public long GetMonitoringLastRebootTime()
        {
            long value = long.MinValue;
            using (RegistryKey key = CreateOrOpenRegistryKey(Settings.Default.Registry_MonitoringKeySubPath, writable: true))
            {
                if (key == null)
                {
                    throw new KeyNotFoundException("PS monitoring key couldn't be found and created");
                }

                long.TryParse((string)key.GetValue(Settings.Default.Registry_LastRebootTime), out value);
            }

            return value;
        }

        public void SetMonitoringUpgradedPSVersion(string version)
        {
            using (RegistryKey key = CreateOrOpenRegistryKey(Settings.Default.Registry_MonitoringKeySubPath, writable: true))
            {
                if (key == null)
                {
                    throw new KeyNotFoundException("PS monitoring key couldn't be found and created");
                }

                key.SetValue(Settings.Default.Registry_PSVersion, version);
            }
        }

        public string GetMonitoringLastPSVersion()
        {
            string value = null;
            using (RegistryKey key = CreateOrOpenRegistryKey(Settings.Default.Registry_MonitoringKeySubPath, writable: true))
            {
                if (key == null)
                {
                    throw new KeyNotFoundException("PS monitoring key couldn't be found and created");
                }

                value = (string)key.GetValue(Settings.Default.Registry_PSVersion);
            }

            return value;
        }
    }
}
