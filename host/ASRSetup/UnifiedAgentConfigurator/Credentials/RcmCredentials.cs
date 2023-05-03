using Newtonsoft.Json;

namespace UnifiedAgentConfigurator.Credentials
{
    /// <summary>
    /// Defines the contract for serialization and deserialization of Rcm credentials.
    /// </summary>
    public class RcmCredentials
    {
        /// <summary>
        /// Gets or sets the Replication control manager url. In case
        /// of Relay connection this points to Relay endpoint.
        /// </summary>
        [JsonProperty(PropertyName="rcmUri")]
        public string RcmUri { get; set; }

        /// <summary>
        /// Gets or sets the management/machine Id.
        /// </summary>
        [JsonProperty(PropertyName = "managementId")]
        public string ManagementId { get; set; }

        ///<summary>
        /// Gets or sets the cluster management Id.
        /// </summary>
        [JsonProperty(PropertyName = "clusterManagementId")]
        public string ClusterManagementId { get; set; }

        /// <summary>
        /// Gets or sets the authentication details for AAD.
        /// </summary>
        [JsonProperty(PropertyName = "aadDetails")]
        public AADAuthDetails AADDetails { get; set; }

        /// <summary>
        /// Gets or sets the service bus relay details for onebox connections.
        /// </summary>
        [JsonProperty(PropertyName = "relayDetails")]
        public RelayDetails RelayDetails { get; set; }

        /// <summary>
        /// Gets or sets the client request Id.
        /// </summary>
        [JsonProperty(PropertyName = "clientRequestId")]
        public string ClientRequestId { get; set; }

        /// <summary>
        /// Gets or sets the activity Id.
        /// </summary>
        [JsonProperty(PropertyName = "activityId")]
        public string ActivityId { get; set; }

        /// <summary>
        /// Gets the EndPoint Type (Public/Private/Mixed) .
        /// </summary>
        [JsonProperty(PropertyName = "vmNetworkTypeBasedOnPrivateEndpoint")]
        public string VMNetworkTypeBasedOnPrivateEndpoint { get; set; }
    }
}
