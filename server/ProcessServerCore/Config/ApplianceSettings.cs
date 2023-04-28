using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using System;
using System.IO;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config
{
    public class ApplianceSettings
    {
        [JsonProperty]
        public AzureADSpn AgentAuthenticationSpn { get; private set; }

        [JsonProperty]
        public ProxySettings Proxy { get; private set; }

        [JsonProperty]
        public string MachineIdentifier { get; private set; }

        [JsonProperty]
        public string AppInsightsInstrumentationKey { get; private set; }

        [JsonProperty]
        public string ProviderId { get; private set; }

        [JsonProperty]
        public Site Site { get; private set; }

        private static readonly JsonSerializer s_jsonSerializer
            = JsonUtils.GetStandardSerializer(indent: true);

        public static ApplianceSettings LoadFromFile(string filePath)
        {
            if (filePath == null)
                throw new ArgumentNullException(nameof(filePath));

            if (string.IsNullOrWhiteSpace(filePath))
                throw new ArgumentException("Empty file path", nameof(filePath));

            // TODO-SanKumar-1904: Does appliance settings need lock to be acquired?
            using (StreamReader sr = new StreamReader(filePath))
            {
                return (ApplianceSettings)s_jsonSerializer.Deserialize(sr, typeof(ApplianceSettings));
            }
        }
    }

    public class AzureADSpn
    {
        [JsonProperty]
        public Uri AadAuthority { get; private set; }

        [JsonProperty]
        public string ApplicationId { get; private set; }

        [JsonProperty]
        public Uri Audience { get; private set; }

        [JsonProperty]
        public string CertificateThumbprint { get; private set; }

        [JsonProperty]
        public string ObjectId { get; private set; }

        [JsonProperty]
        public string TenantId { get; private set; }

        // NOTE-SanKumar-1904: On future addition of members, ensure to update
        // DeepCopy() and ContainsValidProperties(), if necessary.

        public AzureADSpn DeepCopy()
        {
            // Shallow copy is sufficient, since all are immutable types
            return (AzureADSpn)this.MemberwiseClone();
        }

        public bool ContainsValidProperties()
        {
            return
                AadAuthority != null &&
                !string.IsNullOrWhiteSpace(ApplicationId) &&
                Audience != null &&
                !string.IsNullOrWhiteSpace(CertificateThumbprint) &&
                !string.IsNullOrWhiteSpace(ObjectId) &&
                !string.IsNullOrWhiteSpace(TenantId);
        }
    }

    public class ProxySettings
    {
        [JsonProperty]
        public string BypassList { get; private set; }

        [JsonProperty]
        public bool BypassProxyOnLocal { get; private set; }

        [JsonProperty]
        public Uri IpAddress { get; private set; }

        [JsonProperty]
        public string Password { get; private set; }

        [JsonProperty]
        public int PortNumber { get; private set; }

        [JsonProperty]
        public string UserName { get; private set; }

        // NOTE-SanKumar-1904: On future addition of members, ensure to update
        // DeepCopy() and ContainsValidProperties(), if necessary.

        public ProxySettings DeepCopy()
        {
            // Shallow copy is sufficient, since all are immutable and primitive types
            return (ProxySettings)this.MemberwiseClone();
        }

        public bool ContainsValidProperties()
        {
            // TODO-SanKumar-1904: Temporarily disabling these checks as some
            // default appliance json entry isn't working with this.
            return true;

            // Sometimes empty proxy object is passed with all values default.
            // Checking the IpAddress to avoid using such object.

            //return
            //    IpAddress != null &&
            //    // PortNumber != 0 // TODO: Should we add this?
            //    (Password != null || UserName != null);
        }
    }

    public class Site
    {
        [JsonProperty]
        public string SubscriptionId { get; private set; }

        // NOTE-SanKumar-2004: On future addition of members, ensure to update
        // DeepCopy() and ContainsValidProperties(), if necessary.

        public Site DeepCopy()
        {
            // Shallow copy is sufficient, since all are immutable and primitive types
            return (Site)this.MemberwiseClone();
        }

        public bool ContainsValidProperties()
        {
            // Site info might be empty before logging into Azure, so ignore
            // if not present.

            return true;
        }
    }
}
