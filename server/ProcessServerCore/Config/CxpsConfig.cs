using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Setup;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System;
using System.Globalization;
using System.IO;
using System.Threading;

using IniParserOutput = System.Collections.Generic.IReadOnlyDictionary<
    string,
    System.Collections.Generic.IReadOnlyDictionary<string, string>>;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config
{
    /// <summary>
    /// Wrapper object of cxps service configuration
    /// </summary>
    public class CxpsConfig
    {
        /// <summary>
        /// Singleton object
        /// </summary>
        public static CxpsConfig Default { get; } = new CxpsConfig(PSInstallationInfo.Default.GetCxpsConfigPath());

        private readonly string m_cxpsConfFilePath;
        private readonly string m_cxpsConfLockFilePath;
        private readonly IdempotencyContext m_idempContext;

        private Lazy<IniParserOutput> m_parsedData;

        /// <summary>
        /// Constructor. Meant only for internal use.
        /// </summary>
        /// <param name="filePath">cxps config file path.</param>
        private CxpsConfig(string filePath)
        {
            m_cxpsConfFilePath = filePath;
            // Even though we are creating a temporary file to store edits and support idempotency,
            // we will be using the same lock file for cxps conf across the PS.
            m_cxpsConfLockFilePath = PSInstallationInfo.Default.GetCxpsConfigPath() + ".lck";

            // Since this constructor is used to only initialize static object,
            // keep it light and reliable (no exceptions).

            ReloadLazy();
        }

        internal CxpsConfig(IdempotencyContext idempContext)
        {
            var origConfPath = PSInstallationInfo.Default.GetCxpsConfigPath();

            FSUtils.ParseFilePath(
                origConfPath, out string parentDir, out string fileNameWithoutExt, out string fileExt);

            m_idempContext = idempContext;
            m_cxpsConfFilePath = Path.Combine(parentDir, $"{fileNameWithoutExt}_{m_idempContext.Identifier}{fileExt}");
            // Even though we are creating a temporary file to store edits and support idempotency,
            // we will be using the same lock file for cxps conf across the PS.
            m_cxpsConfLockFilePath = origConfPath + ".lck";

            using (var lockFile = FileUtils.AcquireLockFile(m_cxpsConfLockFilePath, exclusive: true, appendLckExt: false))
            {
                // cxps.conf is always expected to be present in both LegacyCS and Rcm modes.

                IdempotencyContext.PersistFileToBeDeleted(
                    filePath: m_cxpsConfFilePath,
                    lockFilePath: m_cxpsConfLockFilePath,
                    backupFolderPath: Path.Combine(Path.GetDirectoryName(m_cxpsConfFilePath), "Backup"));

                File.Copy(origConfPath, m_cxpsConfFilePath, overwrite: true);
                FileUtils.FlushFileBuffers(m_cxpsConfFilePath);
            }

            ReloadLazy();
        }

        internal IniEditor GetIniEditor(IniParserOptions parserOptions)
        {
            var origConfPath = PSInstallationInfo.Default.GetCxpsConfigPath();

            // NOTE-SanKumar-2006: This call is idempotent. i.e. if there are multiple
            // edits with different objects from the GetIniEditor(), the below call
            // will repeatedly set the same registry key and values.
            m_idempContext?.AddFileToBeReplaced(
                origConfPath,
                m_cxpsConfFilePath,
                m_cxpsConfLockFilePath,
                Path.Combine(Path.GetDirectoryName(origConfPath), "Backup"),
                revertLatestFileCleanupGuard: true);

            return new IniEditor(m_cxpsConfFilePath, m_cxpsConfLockFilePath, parserOptions);
        }

        private IniParserOutput ReadCxpsConfFile(bool sharedLockAcquired)
        {
            // IniParserOptions.AllowDebugLogging could be set, since the
            // only secret stored in this config is RcmProxyPassphrase,
            // which is encrypted. But just to be future safe, we aren't
            // adding it, so any new secret added doesn't accidentaly get
            // printed.
            const IniParserOptions ParserOptions = IniParserOptions.RemoveDoubleQuotesInValues;

            using (var lockFile =
                sharedLockAcquired ?
                null :
                FileUtils.AcquireLockFile(m_cxpsConfLockFilePath, exclusive: false, appendLckExt: false))
            {
                return
                    IniParser.ParseAsync(m_cxpsConfFilePath, ParserOptions)
                    .GetAwaiter()
                    .GetResult();
            }
        }

        /// <summary>
        /// Clears the currently cached settings and loads the values from the
        /// file again. Note that the file is parsed and the values are only
        /// loaded at the first access.
        /// </summary>
        public void ReloadLazy()
        {
            // TODO-SanKumar-1905: If the service is gonna run for a long time,
            // caching the exception that occurred once might not be right.
            // No one is calling Reload() at this point.

            // Any load exception would be cached and replayed by the Lazy object

            Interlocked.Exchange(ref m_parsedData, new Lazy<IniParserOutput>(() => ReadCxpsConfFile(sharedLockAcquired: false)));
        }

        public void ReloadNow(bool sharedLockAcquired)
        {
            var parserOutput = ReadCxpsConfFile(sharedLockAcquired);
            Interlocked.Exchange(ref m_parsedData, new Lazy<IniParserOutput>(() => parserOutput));
        }

        /// <summary>
        /// Key names in cxps.conf INI file
        /// </summary>
        public class Names
        {
            public const string DefaultSectionName = "cxps";

            public const string AllowedDirs = "allowed_dirs";
            public const string CSIPAddress = "cs_ip_address";
            public const string CSSslPort = "cs_ssl_port";
            public const string CSUseSecure = "cs_use_secure";
            public const string EncryptedPassphrase = "encrypted_passphrase";
            public const string Id = "id";
            public const string InstallDir = "install_dir";
            public const string MonitorLogEnabled = "monitor_log_enabled";
            public const string MonitorLogRotateCount = "monitor_log_rotate_count";
            public const string RemapPrefix = "remap_full_path_prefix";
            public const string SslPort = "ssl_port";
            public const string RcmPSFirstTimeConfigured = "rcm_ps_first_time_configured";
            public const string RequestDefaultDir = "request_default_dir";
            public const string Port = "port";
            public const string CaCertThumbprint = "cxps_ca_cert_thumbprint";
            public const string PsFingerprint = "ps_fingerprint";
        }

        private string GetValue(string sectionName, string key)
        {
            return
                m_parsedData.Value.TryGetValue(sectionName, out var section) &&
                section.TryGetValue(key, out string value)
                ? value : null;
        }

        /// <summary>
        /// Non SSL Port
        /// </summary>
        public int? Port
        {
            get
            {
                var valueStr = GetValue(Names.DefaultSectionName, Names.Port);

                return !string.IsNullOrWhiteSpace(valueStr) &&
                    int.TryParse(valueStr, out int port) ?
                    (int?)port : null;
            }
        }

        /// <summary>
        /// HTTPS listening port
        /// </summary>
        public int? SslPort
        {
            get
            {
                var valueStr = GetValue(Names.DefaultSectionName, Names.SslPort);

                return !string.IsNullOrWhiteSpace(valueStr) &&
                    int.TryParse(valueStr, out int sslport) ?
                    (int?)sslport : null;
            }
        }

        /// <summary>
        /// DPAPI encrypted passphrase
        /// </summary>
        public string CSPassphraseEncrypted =>
            GetValue(Names.DefaultSectionName, Names.EncryptedPassphrase);

        /// <summary>
        /// Host Id of the Process Server
        /// </summary>
        public string Id =>
            GetValue(Names.DefaultSectionName, Names.Id);

        /// <summary>
        /// Set to yes, after successful first-time configuration of RCM Process Server
        /// </summary>
        public bool? IsRcmPSFirstTimeConfigured
        {
            get
            {
                var valueStr = GetValue(Names.DefaultSectionName, Names.RcmPSFirstTimeConfigured);

                return !string.IsNullOrWhiteSpace(valueStr) ?
                    (bool?)(valueStr == "yes") : null;
            }
        }

        /// <summary>
        /// Folder to receive dummy files from source agent to test connectivity with the PS
        /// </summary>
        public string RequestDefaultDir =>
            GetValue(Names.DefaultSectionName, Names.RequestDefaultDir);

        /// <summary>
        /// Cxps Ca Cert Thumbprint
        /// </summary>
        public string CaCertThumbprint =>
            GetValue(Names.DefaultSectionName, Names.CaCertThumbprint);

        /// <summary>
        /// Server side Fingerprint 
        /// </summary>
        public string PsFingerprint =>
            GetValue(Names.DefaultSectionName, Names.PsFingerprint);
    }
}
