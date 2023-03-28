using System.Diagnostics.CodeAnalysis;

namespace ASRSetupFramework
{
    /// <summary>
    /// Contains Property bag constants to use in the setup
    /// </summary>
    public static class PropertyBagConstants
    {
        /// <summary>
        ///  GUID of the Machine.
        /// </summary>
        public const string MachineIdentifier = "MachineIdentifier";

        /// <summary>
        ///  New GUID created per instance.
        /// </summary>
        public const string RUNID = "RUNID";

        /// <summary>
        ///  Choice of installation in InstallationChoice page.
        /// </summary>
        public const string InstallType = "InstallType";

        /// <summary>
        ///  Selected the option of Protecting VMWare vm in Environment details Page.
        /// </summary>
        public const string ProtectVmWare = "ProtectVmWare";

        /// <summary>
        ///  Install button in Summary page is clicked or not.
        /// </summary>
        public const string summarypageinstallclick = "summarypageinstallclick";

        /// <summary>
        ///  User selected installation location.
        /// </summary>
        public const string InstallationLocation = "InstallationLocation";

        /// <summary>
        ///  User entered passphrase.
        /// </summary>
        public const string IsPassphraseValid = "IsPassphraseValid";

        /// <summary>
        ///  Unified Agent MSI log.
        /// </summary>
        public const string msiLog = "UnifiedAgentMSIInstall.log";

        /// <summary>
        ///  Azure VM Agent MSI log.
        /// </summary>
        public const string azureVMMsiLog = "AzureVMAgentMSIInstall.log";

        /// <summary>
        ///  Azure VM Agent uninstall log.
        /// </summary>
        public const string azureVMUninstallLog = "AzureVMAgentMSIUninstall.log";

        /// <summary>
        ///  Setup start time.
        /// </summary>
        public const string SetupStartTime = "SetupStartTime";

        /// <summary>
        /// Default log file name.
        /// </summary>
        public const string DefaultLogName = "DefaultLogName";

        /// <summary>
        /// Integrity check log file name.
        /// </summary>
        public const string IntegrityCheckLogName = "IntegrityCheckLogName";

        /// <summary>
        /// Path to be combined log file.
        /// </summary>
        public const string CombinedLogFile = "CombinedLogFile";

        /// <summary>
        /// Path to be combined summary file.
        /// </summary>
        public const string CombinedSummaryFile = "CombinedSummaryFile";

        /// <summary>
        /// summary file name
        /// </summary>
        public const string SummaryFileName = "SummaryFileName";

        /// <summary>
        /// Mobiltiy Service validations output json file path.
        /// </summary>        
        public const string ValidationsOutputJsonFilePath = "ValidationsOutputJsonFilePath";

        /// <summary>
        /// If logs are to be sent to Watson.
        /// </summary>
        public const string SendSetupLogsToWatson = "SendSetupLogsToWatson";

        /// <summary>
        /// Log file for machine status.
        /// </summary>
        public const string MachineStatusFile = "MachineStatusFile";

        /// <summary>
        /// If wizard can be closed or not.
        /// </summary>
        public const string CanClose = "CanClose";

        /// <summary>
        /// If installation aborted by the user.
        /// </summary>
        public const string InstallationAborted = "InstallationAborted";

        /// <summary>
        /// If installation finished and no operations to be done furhter.
        /// </summary>
        public const string InstallationFinished = "InstallationFinished";

        /// <summary>
        /// Required MySQL version installed
        /// </summary>
        public const string MySQLInstalled = "MySQLInstalled";

        /// <summary>
        /// Required MySQL version installed
        /// </summary>
        public const string MySQL55Installed = "MySQL55Installed";

        /// <summary>
        /// ProxyType
        /// </summary>
        public const string ProxyType = "ProxyType";

        /// <summary>
        /// Workflow for current execution.
        /// </summary>
        public const string WorkFlow = "WorkFlow";

        /// <summary>
        /// Fabric adapter for which operations are to be performed.
        /// </summary>
        public const string FabricAdapter = "FabricAdapter";

        /// <summary>
        /// Error message to be shown on summary page.
        /// </summary>
        public const string ErrorMessage = "ErrorMessage";

        /// <summary>
        /// Management cert to be used to fetch resources.
        /// </summary>
        public const string ManagementCertThumbprint = "ManagementCertThumbprint";

        /// <summary>
        /// Resource ID of resource corresponding to management cert.
        /// </summary>
        public const string ServiceResourceId = "ServiceResourceId";

        /// <summary>
        /// Subscription Id of the user which is obtained from portal.
        /// </summary>
        public const string SubscriptionId = "SubscriptionId";

        /// <summary>
        /// Resource token.
        /// </summary>
        public const string ResourceToken = "ResourceToken";

        /// <summary>
        /// Site ID of site DRA to be registered with.
        /// </summary>
        public const string SiteId = "SiteId";

        /// <summary>
        /// Unique identifier for this DRA.
        /// </summary>
        public const string DraId = "DraId";

        /// <summary>
        /// Site name of the site DRA to be registeted with.
        /// </summary>
        public const string SiteName = "SiteName";

        /// <summary>
        /// Resource name of selected resource.
        /// </summary>
        public const string ResourceName = "ResourceName";

        /// <summary>
        /// Channel integrity key.
        /// </summary>
        public const string ChannelIntegrityKey = "ChannelIntegrityKey";

        /// <summary>
        /// Proxy address.
        /// </summary>
        public const string ProxyAddress = "ProxyAddress";

        /// <summary>
        /// Proxy port.
        /// </summary>
        public const string ProxyPort = "ProxyPort";

        /// <summary>
        /// Proxy username.
        /// </summary>
        public const string ProxyUsername = "ProxyUsername";

        /// <summary>
        /// Proxy password.
        /// </summary>
        public const string ProxyPassword = "Password";

        /// <summary>
        /// If DRA should bypass default system proxy.
        /// </summary>
        public const string BypassDefaultProxy = "BypassDefaultProxy";

        /// <summary>
        /// Bypass proxy for URLs.
        /// </summary>
        public const string BypassProxyURLs = "BypassProxyURLs";

        /// <summary>
        /// If proxy details were changed during this setup.
        /// </summary>
        public const string ProxyChanged = "ProxyChanged";

        /// <summary>
        /// Full file path for Vault credential file.
        /// </summary>
        public const string CredentialFilePath = "CredentialFilePath";

        /// <summary>
        /// Configurator should be run instead of registration.
        /// </summary>
        public const string RunConfigurator = "RunConfigurator";

        /// <summary>
        /// Friendly name for this server to appear on portal.
        /// </summary>
        public const string FriendlyName = "FriendlyName";

        /// <summary>
        /// ACS url for this registration.
        /// </summary>
        public const string AcsUrl = "AcsUrl";

        /// <summary>
        /// SSL port of CxPs client.
        /// </summary>
        public const string CxPsSslPort = "CxPsSslPort";

        /// <summary>
        /// Vault location.
        /// </summary>
        public const string VaultLocation = "VaultLocation";

        /// <summary>
        /// Audience url for this registration.
        /// </summary>
        public const string RelyingPartyScope = "RelyingPartyScope";

        /// <summary>
        /// Thumbprint for the cert to be used for SRS communication.
        /// </summary>
        public const string SRSAuthCertThumbprint = "SRSAuthCertThumbprint";

        /// <summary>
        /// KEK status for this setup.
        /// </summary>
        public const string KekStatus = "KekStatus";

        /// <summary>
        /// Current Wizard title.
        /// </summary>
        public const string WizardTitle = "WizardTitle";

        /// <summary>
        /// Sync initial cloud data - VMM Fabric
        /// </summary>
        public const string SyncInitialCloudData = "SyncInitialCloudData";

        /// <summary>
        /// KEK filename suffix.
        /// </summary>
        public const string KekFilenameSuffix = "KekFilenameSuffix";

        /// <summary>
        /// Filename to store KEK to.
        /// </summary>
        public const string KekFullfileName = "KekFullfileName";

        /// <summary>
        /// KEK certificate blob.
        /// </summary>
        public const string KekBlob = "KekBlob";

        /// <summary>
        /// KEK certificate details.
        /// </summary>
        public const string KekDetails = "KekDetails";

        /// <summary>
        /// KEK operation to perform.
        /// </summary>
        public const string KekOperation = "KekOperation";

        /// <summary>
        /// Rollover KEK operation.
        /// </summary>
        public const string RolloverKekLocation = "RolloverKekLocation";

        /// <summary>
        /// Unattended upgrade is in progress.
        /// </summary>
        public const string UnattendedUpgradeRunning = "UnattendedUpgradeRunning";

        /// <summary>
        /// If DR service needs to be restarted at end.
        /// </summary>
        public const string RebootDrServiceAtEnd = "RebootDrServiceAtEnd";

        /// <summary>
        /// VMM adapter installed on this machine.
        /// </summary>
        public const string VmmAdapterInstalled = "VmmAdapterInstalled";

        /// <summary>
        /// If all KEK certs are to be exported
        /// </summary>
        public const string ExportKEKCerts = "ExportKEKCerts";

        /// <summary>
        /// Exported KEK cert save location
        /// </summary>
        public const string KEKCertExportLocation = "KEKCertExportLocation";

        /// <summary>
        /// Virtualization platform.
        /// </summary>
        public const string Platform = "Platform";

        /// <summary>
        /// Installer role.
        /// </summary>
        public const string InstallRole = "InstallRole";

        /// <summary>
        /// Server mode.
        /// </summary>
        public const string ServerMode = "ServerMode";

        /// <summary>
        /// Setup action.
        /// </summary>
        public const string SetupAction = "SetupAction";

        /// <summary>
        /// DRA UTC log file name.
        /// </summary>
        public const string DRAUTCLogName = "DRAUTCLogName";

        /// <summary>
        /// Setup Invoker.
        /// </summary>
        public const string SetupInvoker = "SetupInvoker";

        /// <summary>
        /// Prechecks reult (Success,Failure or Warning)
        /// </summary>
        public const string PrechecksResult = "PrechecksResult";

        /// <summary>
        /// Installation status
        /// </summary>
        public const string InstallationStatus = "InstallationStatus";

        /// <summary>
        /// Mandatory reboot required flag
        /// </summary>
        public const string MandatoryRebootRequired = "MandatoryRebootRequired";

        /// <summary>
        /// Guest OS type
        /// </summary>
        public const string GuestOSType = "GuestOSType";

        /// <summary>
        /// Guest OS product type
        /// </summary>
        public const string GuestOSProductType = "GuestOSProductType";

        /// <summary>
        /// Flag for checking if vm is in azure
        /// </summary>
        public const string IsAzureVm = "IsAzureVm";

        /// <summary>
        /// Flag for checking if vm is in azure stack hub
        /// </summary>
        public const string IsAzureStackHubVm = "IsAzureStackHubVm";

        /// <summary>
        /// Guest OS Name
        /// </summary>
        public const string osName = "osName";

        /// <summary>
        /// System drives name
        /// </summary>
        public const string SystemDrives = "SystemDrives";

        /// <summary>
        /// Flag for checking if OS is 32 bit or 64 bit
        /// </summary>
        public const string Is64BitOS = "Is64BitOS";
        
        /// <summary>
        /// Flag for checking if the product is already installed 
        /// </summary>
        public const string IsProductInstalled = "IsProductInstalled";

        /// <summary>
        /// Installation type - Fresh Install/Upgrade
        /// </summary>
        public const string InstallationType = "InstallationType";

        /// <summary>
        /// CSLegacy or CSPrime
        /// </summary>
        public const string CSType = "CSType";

        /// <summary>
        /// Flag to check Platform upgrade from cs to CSPrime
        /// </summary>
        public const string PlatformUpgradeFromCSToCSPrime = "PlatformUpgradeFromCSToCSPrime";

        /// <summary>
        /// Private endpoint state.
        /// </summary>
        public const string PrivateEndpointState = "PrivateEndpointState";

        /// <summary>
        /// Private endpoint state.
        /// </summary>
        public const string ResourceId = "ResourceId";

        #region UA configurator

        /// <summary>
        /// Configuration server endpoint.
        /// </summary>
        public const string CSEndpoint = "CSEndpoint";

        /// <summary>
        /// Configuration server endpoint.
        /// </summary>
        public const string CSPort = "Port";

        /// <summary>
        /// Passphrase file path.
        /// </summary>
        public const string PassphraseFilePath = "PassphraseFilePath";
        
        /// <summary>
        /// Source Config file path.
        /// </summary>
        public const string SourceConfigFilePath = "SourceConfigFilePath";

        /// <summary>
        /// External IP.
        /// </summary>
        public const string ExternalIP = "ExternalIP";

        /// <summary>
        /// Agent's host Id.
        /// </summary>
        public const string HostId = "HostId";

        /// <summary>
        /// Replication control manager endpoint.
        /// </summary>
        public const string RcmCredsPath = "RcmCredsPath";

        /// <summary>
        /// Stores RCM credentials.
        /// </summary>
        public const string RcmCredentials = "RcmCredentials";

        /// <summary>
        /// Rcm auth certificate.
        /// </summary>
        public const string RcmAuthCertThumbprint = "RcmAuthCertThumbprint";

        /// <summary>
        /// Serial number of the BIOS.
        /// </summary>
        public const string BiosId = "BiosId";

        /// <summary>
        /// User selected configure proxy option.
        /// </summary>
        public const string ConfigureProxy = "ConfigureProxy";
        
        /// <summary>
        /// Windows Azure VM Agent name
        /// </summary>
        public const string WindowsAzureVMAgentName = "Windows Azure VM Agent";

        /// <summary>
        /// Windows Azure VM Agent GUID
        /// </summary>
        public const string WindowsAzureVMAgentGUID = "{5CF4D04A-F16C-4892-9196-6025EA61F964}";

        /// <summary>
        /// Already installed component version.
        /// </summary>
        public const string AlreadyInstalledVersion = "AlreadyInstalledVersion";

        /// <summary>
        /// Exit code.
        /// </summary>
        public const string ExitCode = "ExitCode";

        /// <summary>
        /// True.
        /// </summary>
        public const string TrueValue = "True";

        /// <summary>
        /// DiagnoseAndFix.
        /// </summary>
        public const string DiagnoseAndFix = "DiagnoseAndFix";

        /// <summary>
        /// Cloud pairing status
        /// </summary>
        public const string CloudPairingStatus = "CloudPairingStatus";

        /// <summary>
        /// Property to check if the installation is manual or command line
        /// </summary>
        public const string IsManualInstall = "IsManualInstall";

        /// <summary>
        /// Clientrequestid for enable replication for csprime
        /// </summary>
        public const string ClientRequestId = "ClientRequestId";

        /// <summary>
        /// Flag to store if agent is already configured 
        /// </summary>
        public const string AgentConfigured = "AgentConfigured";

        /// <summary>
        /// Flag to store Credential less discovery 
        /// </summary>
        public const string CredLessDiscovery = "CredLessDiscovery";

        /// <summary>
        /// Flag to Unconfigure 
        /// </summary>
        public const string Unconfigure = "Unconfigure";

        /// <summary>
        /// Flag to perform configuration operation
        /// </summary>
        public const string Operation = "Operation";

        #endregion
    }
}
