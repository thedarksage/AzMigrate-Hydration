using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace UnifiedAgentConfigurator.Credentials
{
    /// <summary>
    /// Service bus relay settings to be used for connecting agent to onebox service.
    /// </summary>
    public class RelayDetails
    {
        /// <summary>
        /// Gets or sets the Service bus relay shared key policy name.
        /// </summary>
        [JsonProperty(PropertyName = "relayKeyPolicyName")]
        public string RelayKeyPolicyName { get; set; }

        /// <summary>
        /// Gets or sets the service bus relay shared key.
        /// </summary>
        [JsonProperty(PropertyName = "relaySharedKey")]
        public string RelaySharedKey { get; set; }

        /// <summary>
        /// Gets or sets the service path suffix to use with relay connections.
        /// </summary>
        [JsonProperty(PropertyName = "relayServicePathSuffix")]
        public string RelayServicePathSuffix { get; set; }

        /// <summary>
        /// Gets or sets the expiry timeout to be used by the client in seconds.
        /// </summary>
        [JsonProperty(PropertyName = "expiryTimeoutInSeconds")]
        public int ExpiryTimeoutInSeconds { get; set; }
    }
}
