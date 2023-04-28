using System;
using System.Globalization;
using System.IO;
using System.Net;
using System.Threading;

using IniParserOutput = System.Collections.Generic.IReadOnlyDictionary<
    string,
    System.Collections.Generic.IReadOnlyDictionary<string, string>>;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config
{
    public enum CXType
    {
        CSOnly = 1,
        PSOnly = 2,
        CSAndPS = 3
    }

    public enum HttpScheme
    {
        Https = 0,
        Http = 1
    }

    public class AmethystConfig
    {
        public static AmethystConfig Default { get; } = new AmethystConfig();

        private Lazy<IniParserOutput> m_parsedData;

        private AmethystConfig()
        {
            // Since this constructor is used to only initialize static object,
            // keep it light and reliable (no exceptions).

            Reload();
        }

        public void Reload()
        {
            IniParserOutput ParseAmethystConfFile()
            {
                string amethystConfPath = Path.Combine(
                    PSInstallationInfo.Default.GetRootFolderPath(),
                    Settings.Default.LegacyCS_SubPath_AmethystConf);

                return IniParser.ParseAsync(
                    amethystConfPath, IniParserOptions.RemoveDoubleQuotesInValues)
                    .GetAwaiter().GetResult();
            }

            // TODO-SanKumar-1905: If the service gonna run for a long time,
            // caching the exception that occurred once might not be right.
            // No one is calling Reload() at this point.

            // Any load exception would be cached and replayed by the Lazy object

            Interlocked.Exchange(ref m_parsedData, new Lazy<IniParserOutput>(ParseAmethystConfFile));
        }

        public string HostGuid => m_parsedData.Value[string.Empty]["HOST_GUID"];

        public IPAddress PS_CS_IP => IPAddress.Parse(m_parsedData.Value[string.Empty]["PS_CS_IP"]);

        public IPAddress PS_IP => IPAddress.Parse(m_parsedData.Value[string.Empty]["PS_IP"]);

        public int PS_CS_Port =>
            int.Parse(m_parsedData.Value[string.Empty]["PS_CS_PORT"], CultureInfo.InvariantCulture);

        public HttpScheme PS_CS_Http
        {
            get
            {
                return
                    Convert.ToBoolean(int.Parse(m_parsedData.Value[string.Empty]["PS_CS_SECURED_COMMUNICATION"])) ?
                    HttpScheme.Https : HttpScheme.Http;
            }
        }

        public IPAddress CS_IP => IPAddress.Parse(m_parsedData.Value[string.Empty]["CS_IP"]);

        public int CS_Port =>
            int.Parse(m_parsedData.Value[string.Empty]["CS_PORT"], CultureInfo.InvariantCulture);

        public HttpScheme CS_Http
        {
            get
            {
                return
                    Convert.ToBoolean(int.Parse(m_parsedData.Value[string.Empty]["CS_SECURED_COMMUNICATION"])) ?
                    HttpScheme.Https : HttpScheme.Http;
            }
        }

        internal CXType CXType
        {
            get
            {
                var parsedInt =
                    int.Parse(m_parsedData.Value[string.Empty]["CX_TYPE"], CultureInfo.InvariantCulture);

                if (!Enum.IsDefined(typeof(CXType), parsedInt))
                    throw new NotSupportedException("Unknown CXType : " + parsedInt);

                return (CXType)parsedInt;
            }
        }

        public int MaxTmId =>
            int.Parse(m_parsedData.Value[string.Empty]["MAX_TMID"], CultureInfo.InvariantCulture);

        public long MaxResyncFilesThreshold =>
            long.Parse(m_parsedData.Value[string.Empty]["MAX_RESYNC_FILES_THRESHOLD"], CultureInfo.InvariantCulture);

        public long MaxDiffFilesThreshold =>
            long.Parse(m_parsedData.Value[string.Empty]["MAX_DIFF_FILES_THRESHOLD"], CultureInfo.InvariantCulture);

        internal string AccountId
        {
            get
            {
                const string ACCOUNT_ID = "5C1DAEF0-9386-44a5";

                return m_parsedData.Value[string.Empty]["ACCOUNT_ID"] ?? ACCOUNT_ID;
            }
        }

        internal string AccountType
        {
            get
            {
                const string ACCOUNT_TYPE = "PARTNER";

                return m_parsedData.Value[string.Empty]["ACCOUNT_TYPE"] ?? ACCOUNT_TYPE;
            }
        }

        public string BpmNetworkDevice => m_parsedData.Value[string.Empty]["BPM_NTW_DEVICE"];

        // TODO-SanKumar-1903: Should it rather be based on CXType?! Currently,
        // following the legacy perl script model.
        public IPAddress GetConfigurationServerIP()
        {
            var dict = m_parsedData.Value[string.Empty];

            return IPAddress.Parse(
                dict.ContainsKey("PS_CS_IP") ? dict["PS_CS_IP"] : dict["CS_IP"]);
        }

        public int GetConfigurationServerPort()
        {
            var dict = m_parsedData.Value[string.Empty];

            return int.Parse(
                dict.ContainsKey("PS_CS_PORT") ? dict["PS_CS_PORT"] : dict["CS_PORT"],
                CultureInfo.InvariantCulture);
        }

        public HttpScheme GetConfigurationServerScheme()
        {
            return
                ((this.CXType == CXType.CSAndPS) || (this.CXType == CXType.PSOnly)) ?
                this.PS_CS_Http : this.CS_Http;
        }

        /// <summary>
        /// Fetches the value of paramName from config file as Timespan
        /// If the key is not present returns default value.
        /// </summary>
        /// <param name="paramName">The name of parameter in config file</param>
        /// <param name="defaultValue">Default value of the parameter</param>
        /// <returns>The value fro parameter name as Timespan</returns>
        private TimeSpan GetParameterValue(string paramName, TimeSpan defaultValue)
        {
            var dict = m_parsedData.Value[string.Empty];
            if (dict.ContainsKey(paramName) &&
                int.TryParse(dict[paramName], NumberStyles.None,
                CultureInfo.InvariantCulture, out int retVal))
            {
                return new TimeSpan(0, 0, retVal);
            }
            else
            {
                return defaultValue;
            }
        }

        /// <summary>
        /// Returns the agent upload blocked duration
        /// </summary>
        /// <returns>Upload blocked duration as Timespan</returns>
        internal TimeSpan GetSourceUploadBlockedDuration()
        {
            return GetParameterValue("SOURCE_FAILURE",
                Settings.Default.Monitoring_UploadBlockedTimeThreshold);            // default value of 30 min
        }

        /// <summary>
        /// Returns the threshold for IR/DR files stuck critical alert
        /// </summary>
        /// <returns>Critical thresholdfor IR/DR stuck alert</returns>
        internal TimeSpan GetIRDRStuckCriticalThreshold()
        {
            return GetParameterValue("IR_DR_STUCK_CRITICAL_THRESHOLD",
                Settings.Default.Monitoring_ReplicationBlockedCriticalThreshold);
        }

        /// <summary>
        /// Returns the threshold for IR/DR files stuck warning alert.
        /// </summary>
        /// <returns>Warning threshold for IR/DR stuck alert</returns>
        internal TimeSpan GetIRDRStuckWarningThreshold()
        {
            return GetParameterValue("IR_DR_STUCK_WARNING_THRESHOLD",
                Settings.Default.Monitoring_ReplicationBlockedWarningThreshold);
        }
    }
}
