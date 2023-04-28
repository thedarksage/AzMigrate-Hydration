// DO NOT EDIT! Auto-generated file.
// Find instructions in RcmAgentErrors.xml to make modifications to this file.
// Ensure to check-in the changes to this file without ignoring.

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Setup
{
    public enum PSRcmAgentErrorEnum
    {
        /// <Summary>
        /// Operation completed successfully.
        /// </Summary>
        Success = 0,

        /// <Summary>
        /// The operation failed due to an internal error.
        /// </Summary>
        ConfiguratorUnexpectedError = 520300,

        /// <Summary>
        /// The operation failed due to an internal error.
        /// </Summary>
        ConfiguratorActionFailed = 520301,

        /// <Summary>
        /// Process Server installation couldn't be found on the appliance.
        /// </Summary>
        NotInstalled = 520302,

        /// <Summary>
        /// Process Server is expected to be installed in %ExpectedCSMode; mode but found to be installed in an unexpected mode %InstalledCSMode;.
        /// </Summary>
        UnexpectedMode = 520303,

        /// <Summary>
        /// Replication data port cannot be zero.
        /// </Summary>
        BadInputReplicationDataPort = 520304,

        /// <Summary>
        /// Process Server is already first time configured.
        /// </Summary>
        AlreadyFirstTimeConfigured = 520305,

        /// <Summary>
        /// Process Server is not yet first time configured.
        /// </Summary>
        NotYetFirstTimeConfigured = 520306,

        /// <Summary>
        /// Cache location input cannot be empty.
        /// </Summary>
        BadInputEmptyCacheLocation = 520307,

        /// <Summary>
        /// Cache location input %InvalidCacheLocation; cannot be a relative path.
        /// </Summary>
        BadInputCacheLocationRelativePath = 520308,

        /// <Summary>
        /// Replication log folder path is already configured for the Process Server - %AlreadyConfiguredFolderPath;.
        /// </Summary>
        ReplicationLogFolderAlreadyConfigured = 520309,

        /// <Summary>
        /// Replication log folder is already existing in the Process Server - %ExistingFolderPath;.
        /// </Summary>
        ReplicationLogFolderAlreadyPresent = 520310,

        /// <Summary>
        /// Telemetry folder path is already configured for the Process Server - %AlreadyConfiguredFolderPath;.
        /// </Summary>
        TelemetryFolderAlreadyConfigured = 520311,

        /// <Summary>
        /// Telemetry folder is already existing in the Process Server - %ExistingFolderPath;.
        /// </Summary>
        TelemetryFolderAlreadyPresent = 520312,

        /// <Summary>
        /// Default request folder path is already configured for the Process Server - %AlreadyConfiguredFolderPath;.
        /// </Summary>
        ReqDefFolderAlreadyConfigured = 520313,

        /// <Summary>
        /// Default request folder is already existing in the Process Server - %ExistingFolderPath;.
        /// </Summary>
        ReqDefFolderAlreadyPresent = 520314,

        /// <Summary>
        /// Generating certificates for Process Server's HTTPS listener failed with exit code %ReturnValue;.
        /// </Summary>
        GenCertFailed = 520315,

        /// <Summary>
        /// Process Server is expected to be registered with Rcm.
        /// </Summary>
        NotRegistered = 520316,

        /// <Summary>
        /// Process Server is expected to be unregistered.
        /// </Summary>
        UnregisterNeeded = 520317,

        /// <Summary>
        /// Process Server installation is corrupted.
        /// </Summary>
        CorruptedInstallation = 520318,

        /// <Summary>
        /// Failed to Register or Modify the Process Server with Rcm.
        /// </Summary>
        RegisterOrModifyWithRcmFailed = 520319,

        /// <Summary>
        /// Failed to Test the connection of the Process Server with Rcm.
        /// </Summary>
        TestConnectionWithRcmFailed = 520320,

        /// <Summary>
        /// Failed to Unregister the Process Server with Rcm.
        /// </Summary>
        UnregisterWithRcmFailed = 520321,

        /// <Summary>
        /// Failed to create the folder %FolderPath;.
        /// </Summary>
        FolderCreationFailed = 520322,

        /// <Summary>
        /// Failed to delete the folder %FolderPath;.
        /// </Summary>
        FolderDeletionFailed = 520323,

        /// <Summary>
        /// Replication data port %InvalidPort; is in use by another process %ProcessName;.
        /// </Summary>
        ReplicationDataPortIsInUse = 520324,

        /// <Summary>
        /// The operation could not be completed because there is no outbound connectivity to the internet.
        /// </Summary>
        ConfiguratorConnectFailure = 520325,

        /// <Summary>
        /// The operation could not be completed because there is no outbound connectivity to the internet.
        /// </Summary>
        ConfiguratorNameResolutionFailure = 520326,

        /// <Summary>
        /// The operation could not be completed because there is no outbound connectivity to the proxy server.
        /// </Summary>
        ConfiguratorProxyNameResolutionFailure = 520327,

        /// <Summary>
        /// The operation could not be completed because there is no outbound connectivity to the internet.
        /// </Summary>
        ConfiguratorRequestProhibitedByProxy = 520328,

        /// <Summary>
        /// The appliance configuration could not be performed successfully.
        /// </Summary>
        FingerprintAccessFailure = 520329,

        /// <Summary>
        /// The operation failed due to an internal error.
        /// </Summary>
        FingerprintReadInternalError = 520330,

        /// <Summary>
        /// The operation failed due to FQDN resolution error.
        /// </Summary>
        DnsFqdnFetchFailed = 520331,

        /// <Summary>
        /// Unable to start %Services; on ProcessServer
        /// </Summary>
        FailedToStartServices = 520332,

    }
}

// DO NOT EDIT! Auto-generated file.
// Find instructions in RcmAgentErrors.xml to make modifications to this file.
// Ensure to check-in the changes to this file without ignoring.
