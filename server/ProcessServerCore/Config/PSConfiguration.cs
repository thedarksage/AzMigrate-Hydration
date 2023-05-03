using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Setup;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config
{
    public class PSListenerConfiguration
    {
        [JsonProperty]
        public IPAddress NatIpv4Address { get; }

        // NOTE-SanKumar-1904: On future addition of members, ensure to update
        // DeepCopy(), if necessary.

        public PSListenerConfiguration(IPAddress natIpv4Address)
        {
            this.NatIpv4Address = natIpv4Address;
        }

        public PSListenerConfiguration DeepCopy()
        {
            // Shallow copy is sufficient, since all are immutable types
            return (PSListenerConfiguration)this.MemberwiseClone();
        }
    }

    public class AppInsightsConfiguration
    {
        [JsonProperty]
        public string InstrumentationKey { get; }

        [JsonProperty]
        public string ProviderId { get; }

        [JsonProperty]
        public string SiteSubscriptionId { get; }

        // NOTE-SanKumar-2004: On future addition of members, ensure to update
        // DeepCopy(), if necessary.

        public AppInsightsConfiguration(string instrumentationKey, string providerId, string siteSubscriptionId)
        {
            this.InstrumentationKey = instrumentationKey;
            this.ProviderId = providerId;
            this.SiteSubscriptionId = siteSubscriptionId;
        }

        public AppInsightsConfiguration DeepCopy()
        {
            // Shallow copy is sufficient, since all are immutable types
            return (AppInsightsConfiguration)this.MemberwiseClone();
        }
    }

    public class PSConfiguration
    {
        private class PSConfigurationJson
        {
            [JsonProperty]
            public string HostId { get; internal set; }

            [JsonProperty]
            public string BiosId { get; internal set; }

            [JsonProperty]
            public PSListenerConfiguration HttpListenerConfiguration { get; internal set; }

            [JsonProperty]
            public ProxySettings Proxy { get; internal set; }

            #region RCM-specific configuration

            [JsonProperty]
            public AzureADSpn RcmAzureADSpn { get; internal set; }

            [JsonProperty]
            public Uri RcmUri { get; internal set; }

            [JsonProperty]
            public bool DisableRcmSslCertCheck { get; internal set; }

            [JsonProperty]
            public bool IsFirstTimeConfigured { get; internal set; }

            [JsonProperty]
            public AppInsightsConfiguration AppInsightsConfiguration { get; internal set; }

            #endregion RCM-specific configuration

            // NOTE-SanKumar-1904: On future addition of members, ensure to update
            // DeepCopy(), if necessary.

            public PSConfigurationJson DeepCopy()
            {
                var copied = (PSConfigurationJson)this.MemberwiseClone();

                // Rest of the members are immutable and primitive types

                copied.HttpListenerConfiguration = this.HttpListenerConfiguration?.DeepCopy();
                copied.Proxy = this.Proxy?.DeepCopy();
                copied.RcmAzureADSpn = this.RcmAzureADSpn?.DeepCopy();
                copied.AppInsightsConfiguration = this.AppInsightsConfiguration?.DeepCopy();

                return copied;
            }
        }

        /// <summary>
        /// Singleton object
        /// </summary>
        public static PSConfiguration Default { get; } = new PSConfiguration(PSInstallationInfo.Default.GetPSConfigPath());

        private Lazy<PSConfigurationJson> m_ConfigJson;

        private static readonly JsonSerializerSettings s_psConfigurationSerSettings
            = JsonUtils.GetStandardSerializerSettings(
                indent: true,
                converters: new[] { new IPAddressJsonConverter() });

        private static readonly JsonSerializer s_psConfigurationSerializer
            = JsonUtils.GetStandardSerializer(
                indent: true,
                converters: new[] { new IPAddressJsonConverter() });

        private readonly string m_configPath;
        private readonly string m_configLockFilePath;

        private readonly IdempotencyContext m_idempContext;


        #region Read-only mapping for External consumers

        public string HostId => m_ConfigJson.Value.HostId;

        public string BiosId => m_ConfigJson.Value.BiosId;

        public PSListenerConfiguration HttpListenerConfiguration => m_ConfigJson.Value.HttpListenerConfiguration?.DeepCopy();

        public ProxySettings Proxy => m_ConfigJson.Value.Proxy?.DeepCopy();

        public AzureADSpn RcmAzureADSpn => m_ConfigJson.Value.RcmAzureADSpn?.DeepCopy();

        public Uri RcmUri => m_ConfigJson.Value.RcmUri;

        public bool DisableRcmSslCertCheck => m_ConfigJson.Value.DisableRcmSslCertCheck;

        public bool IsFirstTimeConfigured => m_ConfigJson.Value.IsFirstTimeConfigured;

        public AppInsightsConfiguration AppInsightsConfiguration => m_ConfigJson.Value.AppInsightsConfiguration?.DeepCopy();

        #endregion Read-only mapping for External consumers

        public bool IsRcmRegistrationCompleted => this.RcmUri != null;

        public PSConfiguration(string configPath)
        {
            m_configPath = configPath;
            // Even though we are creating a temporary file to store edits and support idempotency,
            // we will be using the same lock file for ProcessServer conf across the PS.
            m_configLockFilePath = PSInstallationInfo.Default.GetPSConfigPath() + ".lck";

            ReloadLazy();
        }

        internal PSConfiguration(IdempotencyContext idempContext)
        {
            var origConfPath = PSInstallationInfo.Default.GetPSConfigPath();

            FSUtils.ParseFilePath(
                origConfPath, out string parentDir, out string fileNameWithoutExt, out string fileExt);

            m_idempContext = idempContext;
            m_configPath = Path.Combine(parentDir, $"{fileNameWithoutExt}_{m_idempContext.Identifier}{fileExt}");
            // Even though we are creating a temporary file to store edits and support idempotency,
            // we will be using the same lock file for ProcessServer conf across the PS.
            m_configLockFilePath = origConfPath + ".lck";

            using (var lockFile = FileUtils.AcquireLockFile(m_configLockFilePath, exclusive: true, appendLckExt: false))
            {
                if (File.Exists(origConfPath))
                {
                    IdempotencyContext.PersistFileToBeDeleted(
                        filePath: m_configPath,
                        lockFilePath: m_configLockFilePath,
                        backupFolderPath: Path.Combine(Path.GetDirectoryName(m_configPath), "Backup"));

                    File.Copy(origConfPath, m_configPath, overwrite: true);
                    FileUtils.FlushFileBuffers(m_configPath);
                }
            }

            ReloadLazy();
        }

        private PSConfiguration(PSConfigurationJson jsonData)
        {
            // m_configPath is intentionally set to null, since we don't want
            // this synthetic configuration to be accidentally used for
            // persistance (or) for any other use than temporary.
            m_configPath = null;
            // Even though we are creating a temporary file to store edits and support idempotency,
            // we will be using the same lock file for ProcessServer conf across the PS.
            m_configLockFilePath = PSInstallationInfo.Default.GetPSConfigPath() + ".lck";

            m_ConfigJson = new Lazy<PSConfigurationJson>(() => jsonData);
        }

        public void ReloadLazy()
        {
            // Caches any exception in loading data from the file, until Reload()
            // is called again.

            var lazyObj = new Lazy<PSConfigurationJson>(
                    () => ReadDataFromExistingConfigFile(m_configPath, m_configLockFilePath, sharedLockAcquired: false));

            Interlocked.Exchange(ref m_ConfigJson, lazyObj);
        }

        public void ReloadNow(bool sharedLockAcquired)
        {
            var psConfigJson =
                ReadDataFromExistingConfigFile(m_configPath, m_configLockFilePath, sharedLockAcquired);

            Interlocked.Exchange(ref m_ConfigJson, new Lazy<PSConfigurationJson>(() => psConfigJson));
        }

        public delegate Task ConfigChangeValidatorAsync(PSConfiguration newConfig);

        internal async Task SetRcmPSApplianceSettingsAsync(
            ApplianceSettings rcmPSApplianceSettings,
            bool firstTimeConfigure,
            ConfigChangeValidatorAsync validatorAsync)
        {
            if (PSInstallationInfo.Default.CSMode != CSMode.Rcm)
            {
                throw new InvalidOperationException(
                    $"{nameof(SetRcmPSApplianceSettingsAsync)} can't be invoked on " +
                    $"PS running in {PSInstallationInfo.Default.CSMode} mode");
            }

            if (rcmPSApplianceSettings == null)
            {
                throw new ArgumentNullException(nameof(rcmPSApplianceSettings));
            }

            if (firstTimeConfigure)
            {
                if (rcmPSApplianceSettings.AgentAuthenticationSpn == null ||
                        !rcmPSApplianceSettings.AgentAuthenticationSpn.ContainsValidProperties())
                {
                    throw new ArgumentException(
                        $"{nameof(rcmPSApplianceSettings)} contains invalid " +
                        $"{nameof(rcmPSApplianceSettings.AgentAuthenticationSpn)}");
                }
            }

            // Sometimes empty proxy object is passed with all values default.
            // Checking the IpAddress to avoid using such object.
            ProxySettings proxyToUse = null;
            if (rcmPSApplianceSettings.Proxy != null && rcmPSApplianceSettings.Proxy.IpAddress != null)
            {
                proxyToUse = rcmPSApplianceSettings.Proxy;

                if (!proxyToUse.ContainsValidProperties())
                {
                    throw new ArgumentException(
                        $"{nameof(rcmPSApplianceSettings)} contains invalid " +
                        $"{nameof(rcmPSApplianceSettings.Proxy)}");
                }
            }

            await EditValidateSaveReloadAsync(
                jsonData =>
                {
                    if (firstTimeConfigure)
                    {
                        jsonData.HostId = rcmPSApplianceSettings.MachineIdentifier;
                        jsonData.BiosId = SystemUtils.GetBiosId();
                        jsonData.RcmAzureADSpn = rcmPSApplianceSettings.AgentAuthenticationSpn?.DeepCopy();

                        // TODO-SanKumar-2005: Save these settings at every RcmConfigurator action
                        jsonData.AppInsightsConfiguration = new AppInsightsConfiguration(
                            instrumentationKey: rcmPSApplianceSettings.AppInsightsInstrumentationKey,
                            providerId: rcmPSApplianceSettings.ProviderId,
                            siteSubscriptionId: rcmPSApplianceSettings.Site?.SubscriptionId);

                        jsonData.IsFirstTimeConfigured = true;
                    }

                    jsonData.Proxy = rcmPSApplianceSettings.Proxy?.DeepCopy();
                },
                validatorAsync)
            .ConfigureAwait(false);
        }

        internal async Task SetRcmUriAsync(
            Uri rcmUri,
            ConfigChangeValidatorAsync validatorAsync)
        {
            if (rcmUri == null)
            {
                throw new ArgumentNullException(nameof(rcmUri));
            }

            // TODO-SanKumar-1904: If it's the same URI should we still do it?
            // IMO, we should do it and be idempotent by keeping it simple. But,
            // this will save lock-unlock work but it's very small.

            await EditValidateSaveReloadAsync(
                jsonData => { jsonData.RcmUri = rcmUri; },
                validatorAsync)
            .ConfigureAwait(false);
        }

        internal Task ClearRcmUriAsync()
        {
            return EditValidateSaveReloadAsync(
                jsonData => { jsonData.RcmUri = null; },
                validatorAsync: null);
        }

        public async Task BlockUntilRcmRegistrationIsCompleted(CancellationToken ct)
        {
            for (; ; )
            {
                try
                {
                    this.ReloadNow(sharedLockAcquired: false);
                    if (this.IsRcmRegistrationCompleted)
                    {
                        Tracers.Misc.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Detected valid registration with Rcm uri: {0}",
                            this.RcmUri);

                        return;
                    }
                }
                catch (Exception ex)
                {
                    Tracers.Misc.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Failed while checking/waiting for valid Rcm registration of the PS{0}{1}",
                            Environment.NewLine, ex);

                    throw;
                }

                await Task.Delay(Settings.Default.PSConf_RcmRegBlockingWaitRetryInterval, ct);
            }
        }

        internal async Task SetListenerConfigurationAsync(
            PSListenerConfiguration listenConfig,
            ConfigChangeValidatorAsync validatorAsync)
        {
            if (listenConfig == null)
            {
                throw new ArgumentNullException(nameof(listenConfig));
            }

            if (listenConfig.NatIpv4Address != null)
            {
                if (listenConfig.NatIpv4Address.AddressFamily != AddressFamily.InterNetwork)
                {
                    throw new ArgumentException("Only Ipv4 addresses are supported", nameof(listenConfig));
                }

                if (listenConfig.NatIpv4Address == IPAddress.None)
                {
                    throw new ArgumentException("Provide a valid NAT Ipv4 Address");
                }
            }

            await EditValidateSaveReloadAsync(
                jsonData => { jsonData.HttpListenerConfiguration = listenConfig.DeepCopy(); },
                validatorAsync)
            .ConfigureAwait(false);
        }

        private delegate void JsonValuesEditor(PSConfigurationJson jsonData);

        private async Task EditValidateSaveReloadAsync(
            JsonValuesEditor editor,
            ConfigChangeValidatorAsync validatorAsync)
        {
            using (var lckFile =
                await FileUtils.AcquireLockFileAsync(
                    m_configLockFilePath,
                    exclusive: true,
                    appendLckExt: false,
                    ct: CancellationToken.None)
                .ConfigureAwait(false))
            {
                // Even though we have a cached set of json values within the
                // object, read from the file after acquiring lock for latest data.

                var toOverwriteData =
                    ReadDataFromExistingConfigFile(m_configPath, m_configLockFilePath, sharedLockAcquired: true);

                editor(toOverwriteData);

                if (validatorAsync != null)
                {
                    var futureConfig = new PSConfiguration(toOverwriteData);

                    await validatorAsync(futureConfig).ConfigureAwait(false);
                }

                string backupFolder = Path.Combine(Path.GetDirectoryName(m_configPath), "Backup");

                // TODO-SanKumar-1904: Move flush true/false to Settings
                FileUtils.ReliableSaveFile(
                    filePath: m_configPath,
                    backupFolderPath: backupFolder,
                    multipleBackups: true,
                    writeData: sw => s_psConfigurationSerializer.Serialize(sw, toOverwriteData),
                    flush: true);

                // This operation by itself is idempotent. i.e. there could be multiple edits
                // made by invoking EditValidateSaveReloadAsync() and the AddFileToBeReplaced()
                // call will keep updating the same registry key with same values.
                m_idempContext?.AddFileToBeReplaced(
                    filePath: PSInstallationInfo.Default.GetPSConfigPath(),
                    latestFilePath: m_configPath,
                    lockFilePath: m_configLockFilePath,
                    backupFolderPath: backupFolder,
                    revertLatestFileCleanupGuard: true);

                // Since the backing file has been modified, trigger a reload
                // before releasing the lock file.
                this.ReloadLazy();
            }
        }

        private static PSConfigurationJson ReadDataFromExistingConfigFile(
            string psConfigPath,
            string lockFilePath,
            bool sharedLockAcquired)
        {
            PSConfigurationJson fileData;

            using (var lockFile =
                sharedLockAcquired ?
                null :
                FileUtils.AcquireLockFile(lockFilePath, exclusive: false, appendLckExt: false))
            {
                if (File.Exists(psConfigPath))
                {
                    // If the config is corrupt for any reason, this block would throw.

                    string allText = File.ReadAllText(psConfigPath);

                    fileData =
                        JsonConvert.DeserializeObject<PSConfigurationJson>(allText, s_psConfigurationSerSettings);
                }
                else
                {
                    // This a valid scenario for the first time configuration after installation or
                    // after unconfigure.

                    fileData = new PSConfigurationJson();
                }
            }

            return fileData;
        }
    }
}
