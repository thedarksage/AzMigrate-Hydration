namespace MarsAgent.Constants
{
    /// <summary>
    /// Registry setting names.
    /// </summary>
    public static class MarsRegistrySettingNames
    {
        /// <summary>
        /// Root hive.
        /// </summary>
        public const string RootHive = @"Software\Microsoft\Windows Azure Backup";

        /// <summary>
        /// Setup hive.
        /// </summary>
        public const string SetupHive = RootHive + "\\Setup";

        /// <summary>
        /// Registration hive.
        /// </summary>
        public const string RegistrationHive = RootHive + "\\Config\\InMageMT";

        /// <summary>
        /// Registry value name for Subscription Id.
        /// </summary>
        public const string SubscriptionIdRegValue = "SubscriptionId";

        /// <summary>
        /// Registry value name for resource Id.
        /// </summary>
        public const string ResourceIdRegValue = "ResourceId";

        /// <summary>
        /// Registry value name for resource location.
        /// </summary>
        public const string ResourceLocationRegValue = "VaultLocation";

        /// <summary>
        /// Registry value name for cloud container unique name.
        /// </summary>
        public const string ContainerUniqueNameRegValue = "ContainerUniqueName";

        /// <summary>
        /// Registry value name for protection service URI.
        /// </summary>
        public const string ProtectionServiceEndpointRegValue =
            "ProtectionServiceSVCEndpoint";

        /// <summary>
        /// Registry value name for telemetry service URI.
        /// </summary>
        public const string TelemetryServiceEndpointRegValue =
            "TelemetryServiceSVCEndpoint";

        /// <summary>
        /// Registry value name for customer Tenant Id.
        /// </summary>
        public const string SpnTenantIdRegValueName = "ServiceTenantId";

        /// <summary>
        /// Registry value name for customer AAD application Id.
        /// </summary>
        public const string SpnApplicationIdRegValueName = "ServiceClientId";

        /// <summary>
        /// Registry value name for audience of AAD token.
        /// </summary>
        public const string SpnAudienceRegValueName = "ServiceAADAudience";

        /// <summary>
        /// Registry value name for AAD authority.
        /// </summary>
        public const string SpnAadAuthorityRegValueName = "AADAuthority";

        /// <summary>
        /// Registry value name for thumbprint of certificate.
        /// </summary>
        public const string SpnCertificateThumbprintRegValueName = "PrimaryCertThumbprint";

        /// <summary>
        /// Registry value name for mode of Replication.
        /// </summary>
        public const string Mode = "Mode";
    }
}
