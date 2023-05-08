namespace ASRSetupFramework
{
    /// <summary>
    /// UA Setup error codes for output json file
    /// </summary>
    public enum UASetupErrorCode
    {
        ASRMobilityServiceInstallerVSSProviderRegistrationFailed,
        ASRMobilityServiceInstallerSuccessfulRecommendedReboot,
        ASRMobilityServiceInstallerSuccessfulMandatoryReboot,
        ASRMobilityServiceInstallerWMIQueryFailure,
        ASRMobilityServiceInstallerProcessArchitectureDeterminationFailed,
        ASRMobilityServiceInstallerFailedToGetSystemFolder,
        ASRMobilityServiceInstallerNativeCallFailure,
        ASRMobilityServiceInstallerConfigFilesBackupFailed,
        ASRMobilityServiceInstallerConfigFilesMergeFailed,
        ASRMobilityServiceInstallerConfigFilesCreationFailed,
        ASRMobilityServiceInstallerFailedToSetPermissions,
        //To be used when we give the service names that failed to start as error
        ASRMobilityServiceInstallerFailedToStartAgentServices,
        ASRMobilityServiceInstallerReplicationEngineError,
        ASRMobilityServiceInstallerInstallingComponentsFailedWithInternalError,
        ASRMobilityServiceInstallerMSIInstallationFailure,
        ASRMobilityServiceInstallerAgentServicesStartFailedWithInternalError,
        ASRMobilityServiceInstallerPrepareForInstallStepsFailed,
        ASRMobilityServiceInstallerPostInstallationStepsFailed,
        ASRMobilityServiceInstallerCommandLineParametersMissingFailure,
        ASRMobilityServiceInstallerInstallationTypeMismatch,
        ASRMobilityServiceInstallerCSTypeMismatch,
        ASRMobilityServiceInstallerPrecheckInternalError,
        ASRMobilityServiceInstallerPlatformMismatch
    }

    /// <summary>
    /// UA Setup Error Code messages for output json file
    /// </summary>
    public class UASetupErrorCodeMessage
    {
        public const string VSSProviderRegistrationFailed = @"VSS Provider registration failed with error code {0}";
        public const string SuccessfulRecommendedReboot = @"Please reboot your machine after upgrade for some changes to take effect";
        public const string SuccessfulMandatoryReboot = @"Please reboot your machine for some changes to take effect";
        public const string WMIQueryFailures = @"{0}. Following WMI call failed : {1}. Please contact Windows team.";
        public const string ProcessArchitectureFailure = @"Is64BitOperatingSystem failed to identify OS Architecture. IsWow64Process failed with error code {0}.";
        public const string SystemFolderFailure = @"Failed to get Environment.SpecialFolder.System";
        public const string NativeCallFailure = @"Following native calls failed : {0}";
        public const string ConfigFilesBackupFailure = @"Upgrade has failed as an error occurred while performing a backup of the existing Azure Site Recovery configurations.";
        public const string ConfigFilesMergeFailure = @"Upgrade has failed as an error occurred while performing merge of the existing Azure Site Recovery configurations.";
        public const string ConfigFilesCreationFailure = @"Setup has failed as an error occurred while creating Azure Site Recovery configurations.";
        public const string FailedToSetPermissions = @"Failed to set required permissions on the following files/directories : {0}";
        public const string FailedToStartAgentServicesInternalError = @"Failed to start agent services due to some internal error";
        public const string ReplicationEngineFailure = @"Failed to check filter driver status due to a generic error";
        public const string InstallingComponentsFailedWithInternalError = @"Internal error encountered while installing Azure Site Recovery MSI";
        public const string MSIInstallationFailure = @"ASR MSI Installation failed with exit code {0}";
        public const string PrepareForInstallStepsFailed = @"Prepare for install step failed with internal error";
        public const string PostInstallationStepsFailed = @"Post Installation steps failed with internal error";
        public const string MissingCommandLineArguments = @"Following command line params are missing : {0}";
        public const string InstallationTypeMismatch = @"The installation type passed to installer and that detected by installer do not match. Installation type passed : {0} ; Installation type detected : {1} ";
        public const string CSTypeMismatch = @"The CSType passed to the installer and that detected by installer do not match. CSType passed : {0} : CSType detected : {1} ";
        public const string PrecheckCrash = @"Prechecks failed due to low memory";
        public const string PlatformChangeFailure = @"The platform type passed to installer do not match with VMware VM. Platform type passed: {0}";
    }

    /// <summary>
    /// UA setup error code params for output json file
    /// </summary>
    public class UASetupErrorCodeParams
    {
        public const string ErrorCode = "ErrorCode";
        public const string WMIQuery = "WMIQuery";
        public const string WMIQueryErrorMessage = "WMIQueryErrorMessage";
        public const string ProcessArchitecture = "ProcessArchitectureErrorCode";
        public const string NativeCallFailure = "NativeCalls";
        public const string PermissionsFiles = "PermissionsFiles";
        public const string MissingCmdArguments = "MissingCmdArguments";
        public const string InstallationTypePassed = "InstallationTypePassed";
        public const string InstallationTypeDetected = "InstallationTypeDetected";
        public const string CSTypePassed = "CSTypePassed";
        public const string CSTypeDetected = "CSTypeDetected";
        public const string ErrorMessage = "ErrorMessage";
    }

    public class WMIQueryFailureErrorMessage
    {
        public const string Product = @"Retrieving installed product details has failed";
        public const string SystemEnclosure = @"Retrieving BIOS details has failed";
        public const string OperatingSystem = @"Retrieving operating system details has failed";
        public const string LogicalDisk = @"Retrieving system drive details, partition details of volume, disk drive for partition has failed";
        public const string BIOSId = @"Failed to get BIOS Id of system";
    }

    public class WMIQueryFailureErrorReason
    {
        public const string Product = @"Wmic product get Name, Version";
        public const string SystemEnclosure = @"Wmic SystemEnclosure get SMBIOSAssetTag";
        public const string OperatingSystem = @"Wmic os get version, producttype";
        public const string OperatingSystemCaption = @"Wmic os get caption";
        public const string LogicalDisk = @"wmic logicaldisk get DeviceID, wmic partition get DeviceId, wmic diskdrive get Partitions";
        public const string BIOSId = @"wmic computersystemproduct get uuid";
    }

    public class UAInstallerErrorPrefixConstants
    {
        public const string ASRMobilityServiceInstallerMSIInstallationFailure = "ASRMobilityServiceInstallerMSIInstallationFailure_";
    }

    public enum UAMSIErrorCodeList
    {
        TempDirectoryFull = -2147286787,
        InstallerNotAccessible = 1601,
        InternalError = 1603,
        InstallationAlreadyRunning = 1618,
        IncompatibleGroupPolicy = 1625
    }

    public enum MSIError1603
    {
        Default,
        KeyDeleted,
        NoSystemResource,
        GenericError
    }

    public enum MSIError1601
    {
        Default,
        ConnectToServerFailed
    }
}