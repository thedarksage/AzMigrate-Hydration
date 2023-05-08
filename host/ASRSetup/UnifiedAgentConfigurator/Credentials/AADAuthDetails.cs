using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace UnifiedAgentConfigurator.Credentials
{
    /// <summary>
    /// AAD authentication details.
    /// </summary>
    public class AADAuthDetails
    {
        /// <summary>
        /// Gets or sets the certificate to be used for AAD authentication.
        /// </summary>
        [JsonProperty(PropertyName = "authCertificate")]
        public string AuthCertificate { get; set; }

        /// <summary>
        /// Gets or set the AAD Url.
        /// </summary>
        [JsonProperty(PropertyName = "serviceUri")]
        public string ServiceUri { get; set; }

        /// <summary>
        /// Gets or sets the AAD Tenant Id.
        /// </summary>
        [JsonProperty(PropertyName = "tenantId")]
        public string TenantId { get; set; }

        /// <summary>
        /// Gets or sets the AAD Client Id.
        /// </summary>
        [JsonProperty(PropertyName = "clientId")]
        public string ClientId { get; set; }

        /// <summary>
        /// Gets or sets the AAD Audience.
        /// </summary>
        [JsonProperty(PropertyName = "audienceUri")]
        public string AudienceUri { get; set; }
    }
}
