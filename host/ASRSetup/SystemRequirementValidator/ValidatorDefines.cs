using System;
using System.Collections.Generic;
using System.Runtime.Serialization;
using ASRSetupFramework;

namespace SystemRequirementValidator
{
    /// <summary>
    /// This is the exMessage when system requirement match fails.
    /// </summary>
    [Serializable()]
    public class SystemRequirementNotMet : System.Exception
    {
        private string                      m_ErrorCode { get; set; }
        private IDictionary<string, string> m_ErrorParams;
        private string                      m_DefaultMessage;

        /// <summary>
        /// Constructor for system requirement failed exMessage.
        /// </summary>
        /// <param name="ErrorCode">Error code for system requirement failure.</param>
        /// <param name="ErrorParams">Paramaeters for system requirement failure.</param>
        /// <param name="DefaultMessage">Default english message for this failure.</param>
        public SystemRequirementNotMet(string ErrorCode, IDictionary<string, string> ErrorParams, string DefaultMessage)
        {
            m_ErrorCode = ErrorCode;
            m_ErrorParams = ErrorParams;
            m_DefaultMessage = DefaultMessage;
        }

        /// <summary>
        /// Method that converts the exMessage into ErrorInfo object.
        /// </summary>
        /// <returns>ErrorInfo Object</returns>
        public InstallerException GetErrorInfo()
        {
            return new InstallerException(m_ErrorCode, m_ErrorParams, m_DefaultMessage);
        }
    }

    /// <summary>
    /// Class that defines different system requirements.
    /// </summary>
    class SystemRequirementNames
    {
        public const string BootDriversCheck = "BootDriversCheck";
        public const string BootAndSystemDiskCheck = "BootAndSystemDiskCheck";
        public const string ActivePartitonsCheck = "ActivePartitionsCheck";
        public const string CriticalServicesCheck = "CriticalServicesCheck";
        public const string BootUEFICheck = "BootUEFICheck";
        public const string VSSProviderInstallationCheck = "VSSProviderInstallationCheck";
        public const string FileValidationCheck = "FileValidationCheck";
        public const string CommandExecutionCheck = "CommandExecutionCheck";
        public const string SHA2CompatibilityCheck = "SHA2CompatibilityCheck";
        public const string AgentMandatoryFilesCheck = "AgentMandatoryFilesCheck";
        public const string MachineMemoryCheck = "MachineMemoryCheck";
        public const string InvolFltDriverCheck = "InvolFltDriverCheck";
    }

    struct CommonAttributeValues
    {
        public const string ControlSets = "ControlSets";
    }

    struct CommonValues
    {
        public const string AllControlSets = "all";
    }

    /// <summary>
    /// Parameter names for Boot Drivers check.
    /// </summary>
    struct BootDriverCheckParams
    {
        public const string DriversList = "Drivers";
        public const string ControlSet = "ControlSet";
        public const string System = "System";
    }

    /// <summary>
    /// Parameter names for critical services check.
    /// </summary>
    struct CriticalServicesCheckParams
    {
        public const string ServicesList = "Services";
    }

    /// <summary>
    /// Parameter names for Boot and System Disk check.
    /// </summary>
    struct BootAndSystemDiskCheckParams
    {
        public const string BootAndSystemPartitionsOnSameDisk = "BootAndSystemPartitionsOnSameDisk";
    }

    /// <summary>
    /// Parameter names for Active partition check.
    /// </summary>
    struct ActivePartitonsCheckParams
    {
        public const string NumberOfActivePartitions = "NumberOfActivePartitions";
    }

    struct BootUEFIParams
    {
        public const string MinOSMajorVersion = "MinOSMajorVersion";
        public const string MinOSMinorVersion = "MinOSMinorVersion";
        public const string MaxNumPartititons = "MaxNumPartitions";
        public const string BytesPerSector = "BytesPerSector";
        public const string Hypervisor = "Hypervisor";
        public const string SecureBoot = "SecureBoot";
    };

    struct VSSInstallationCheckParams
    {
        public const string MinOSMajorVersion = "MinOSMajorVersion";
        public const string MinOSMinorVersion = "MinOSMinorVersion";
    }

    struct FileValidationCheckParams
    {
        public const string Filename = "Filename";
        public const string Path = "Path";
    }

    struct CommandExecutionCheckParams
    {
        public const string CommandName = "CommandLine";
        public const string Result = "Result";
        public const string Arguments = "Arguments";
    }

    struct AgentMandatoryFilesCheckParams
    {
        public const string FileName = "FileName";
    }

    struct MemoryCheckParams
    {
        public const string RequiredMemoryInMB = "RequiredMemoryInMB";
    }

    /// <summary>
    /// Requirement name and its parameters.
    /// </summary>
    [DataContract]
    public class SystemRequirementCheck
    {
        [DataMember]
        public string CheckName { get; set; }

        [DataMember]
        public string OperationName { get; set; }

        [DataMember]
        public string IsMandatory { get; set; }

        [DataMember]
        public string InstallAction { get; set; }

        [DataMember]
        public IDictionary<string, string> Params { get; set; }

    }

    public class GenericErrorMsgArguments
    {
        public const string Error = "error";
    };

    public class BootAndSystemDiskOnSeparateDisksMsgArgs
    {
        public const string BootDisk = "bootdisk";
        public const string SystemDisk = "systemdisk";
    };

    public class MultipleSystemDisksMsgArgs
    {
        public const string SystemDisk = "systemdisk";
    };

    public class MultipleBootDisksMsgArgs
    {
        public const string BootDisk = "bootdisk";
    };

    public class MultipleActivePartitionsMsgArgs
    {
        public const string Partitions = "partitions";
    };

    public class BootDriversMissingMsgArgs
    {
        public const string Drivers = "drivers";
    };

    public class CriticalServicesMissingMsgArgs
    {
        public const string Services = "services";
    };

    public class HypervisorNameArg
    {
        public const string HypervisorName = "HypervisorName";
    };

    public class BootUEFIUnsupportedOSMajorMsgArgs
    {
        public const string MinOsMajorVersion = "minOsMajorVersion";
        public const string currentOsMajorVersion = "currentOsMajorVersion";
    };

    public class BootUEFIUnsupportedOSMsgArgs
    {
        public const string MinOsMajorVersion = "minOsMajorVersion";
        public const string MinOsMinorVersion = "minOsMinorVersion";
        public const string CurrentOsMajorVersion = "currentOsMajorVersion";
        public const string CurrentOsMinorVersion = "currentOsMinorVersion";
    };

    public class BootUEFIUnsupportedDiskSectorSizeMsgArgs
    {
        public const string SupportedBytesPerSector = "supportedBytesPerSector";
        public const string BootDiskBytesPerSector = "bootDiskBytesPerSector";
    };

    public class BootUEFIUnsupportedNumberOfPartitionsMsgArgs
    {
        public const string MaxNumPartitionsSupported = "maxNumPartitionsSupported";
        public const string NumPartitions = "NumPartitions";
    };

    public class ComputerManufacturers
    {
        public const string HYPERVMANUFACTURER = "Microsoft Corporation";
        public const string HYPERVMODEL = "Virtual Machine";
        public const string VMWAREMODEL        = "Vmware";
    };

    public class Hypervisor
    {
        public const string VMWARE = "vmware";
        public const string HYPERV = "hyperv";
        public const string PHYSICAL = "physical";
    };

    public class VSSInstallationFailedInPrecheckMsgArgs
    {
        public const string ErrorCode = "ErrorCode";
    }

    public class FileValidationCheckPrecheckMsgArgs
    {
        public const string MissingExe = "MissingExe";
    }

    public class CommandExecutionCheckPrecheckMsgArgs
    {
        public const string FailedCommands = "FailedCommands";
    }

    public class SHA2CompatibilityCheckPrecheckMsgArgs
    {
        public const string OSVersion = "OSVersion";
    }

    public class AgentMandatoryFilesCheckPrecheckMsgArgs
    {
        public const string MissingFile = "MissingFile";
    }

    /// <summary>
    /// Error resources that contains messages for different errors.
    /// </summary>
    class ErrorResources
    {
        public const string EnvironmentVariableNotFoundMsg = @"Environment variable {0} doesnt exist";
        public const string FileNotFoundMsg = @"File {0} doesnt exist";
        public const string BootAndSystemDiskOnSeparateDisk = @"Boot partition is on disk number {0} while system partition is on {1}.
                                                                Azure requires both partition on same disk to boot.";
        public const string NoSystemDisk = @"No System Disk found on this system.";
        public const string NoBootDisk = @"No Boot Disk found on this system.";
        public const string MultipleSystemDisks = @"Multiple system disks {0} found. Azure doesn't support multiple system disks.";
        public const string MultipleBootDisks = @"Multiple boot disks {0} found. Azure doesn't support multiple boot disks.";
        public const string MultipleActivePartitions = @"Multiple Active Partitions {0} found";
        public const string ServiceInvalidStartState = @"Service Start Type is set to {0}. Please enable these services.";
        public const string ServiceNotInstalled = @" Service {0} is not installed. Please install this service";

        public const string BootDriverNotPresent = @"
                    Validation failed for boot drivers {0}. 
                    Possible causes
                        1. Driver is not installed in one of control sets.
                        2. Driver registry is not in proper format in one of control sets.
                        3. Driver binary is missing.
                        4. Read failed for driver registry.

                    Resoultion:
                        1. If driver is not installed or, there is some issue with registry key, please contact windows team.";

        public const string ServicesNotReady = @"
                    Services {0} are either either not installed or, disabled. These services are required by ASR.

                    Resolution:
                        1. Install or, enable these services.
                    ";

        public const string BootUEFIUnsupportedOSMajor = @"
                        Boot UEFI is not supported on current OS.
                        Minimum Expected OS Major Version is {0} 
                        Current OS Major Version is {1}
        ";

        public const string BootUEFIUnsupportedOS = @"
                        Boot UEFI is not supported on current OS.
                        Minimum Expected OS Major Version {0} Minor Version {1}
                        Current OS Major Version {2} Minor Version {3}
                    ";

        public const string BootUEFIUnsupportedDiskSectorSize = @"
                        BootUEFI is supported only for disk with sector size {0}.
                        Current Boot Disk Sector Size {1}
                    ";

        public const string BootUEFIUnsupportedNumberOfPartitions = @"
                        BootUEFI is supported only for disk with partition count less than {0}.
                        Current Boot Disk has {1} number of partitions.
                    ";

        public const string BootUEFIUnsupportedHypervisor = @"
                        BootUEFI is not supported on current hypervisor {0}.
                    ";

        public const string VSSInstallationFailure = @"
                        VSS Installation failed with exit code {0}.
                    ";

        public const string MandatoryMissingFiles = @"
                        Following mandatory files are missing : {0}";

        public const string BootUEFISecureBootNotSupported = @"
                    UEFI Secure boot is not supported";

        public const string CommandsNotRunning = @"
                        Following commands could not be run on the system : {0}";

        public const string SHA2SigningSupportFailure = @"
                        SHA2 signing support is not present. Install the following Kbs to get the support {0}";

        public const string MemoryError = @"
                    Failed to enable replication due to a prerequisite check error.
                    Possible causes: The memory on the machine is less than 1 GB. For Azure Site Recovery to efficiently function, a minimum memory of 1 GB is required.
                    Recommended action: Ensure memory of the machine is greater than or equal to 1 GB and retry the operation.";

        public const string InvolFltDriverError = @"
                    Scout product installed on the machine. Please uninstall scout product and re-try the installation.";
    }

    /// <summary>
    /// Error code map for VSS Provider Installation Failure
    /// </summary>
    public class VSSProviderInstallErrors
    {
        public static IDictionary<int, SystemRequirementFailure> ErrorCodeMap = new Dictionary<int, SystemRequirementFailure>
        {
            {514,SystemRequirementFailure.ASRMobilityServiceVssProviderInstallFailedOutOfMemory},
            {515,SystemRequirementFailure.ASRMobilityServiceVssProviderInstallFailedServiceDatabaseLocked},
            {516,SystemRequirementFailure.ASRMobilityServiceVssProviderInstallFailedServiceAlreadyExists},
            {517,SystemRequirementFailure.ASRMobilityServiceVssProviderInstallFailedServiceMarkedForDeletion},
            {802,SystemRequirementFailure.ASRMobilityServiceVSSProviderInstallFailedDCOMServiceError},
            {806,SystemRequirementFailure.ASRMobilityServiceVssProviderInstallFailedCscriptAccessDenied},
            {9009,SystemRequirementFailure.ASRMobilityServiceVssProviderInstallFailedPathNotFound}
        };

    }

    public class FileValidationCheckHelper
    {
        public const string Rundll32Exe = "rundll32.exe";
        public const string Rundll32ExeRelativePath = @"\rundll32.exe";

        public static IDictionary<string, string> FilePathMap = new Dictionary<string, string>
        {
            {Rundll32Exe, Rundll32ExeRelativePath}
        };
    }

    public class UEFISecureBoot
    {
        public const string RegKey = @"System\CurrentControlSet\Control\SecureBoot\State";
        public const string ValueName = "UEFISecureBootEnabled";
    }

    public enum InstallAction
    {
        Install,
        Upgrade,
        All
    }

    /// <summary>
    /// Different failures for System Requirements.
    /// </summary>
    public enum SystemRequirementFailure
    {
        ASRMobilityServiceGenericError,
        ASRMobilityServiceBootAndSystemDiskOnSeparateDisks,
        ASRMobilityServiceNoSystemDiskFound,
        ASRMobilityServiceNoBootDiskFound,
        ASRMobilityServiceMultipleSystemDisks,
        ASRMobilityServiceMultipleBootDisks,
        ASRMobilityServiceMultipleActivePartitions,
        ASRMobilityServiceBootDriversMissing,
        ASRMobilityServiceCriticalServicesMissing,
        ASRMobilityServiceBootUEFIUnsupportedHypervisor,
        ASRMobilityServiceBootUEFIUnsupportedOSMajor,
        ASRMobilityServiceBootUEFIUnsupportedOS,
        ASRMobilityServiceBootUEFIUnsupportedDiskSectorSize,
        ASRMobilityServiceBootUEFIUnsupportedNumberOfPartitions,
        ASRMobilityServiceVSSProviderInstallationFailed,
        ASRMobilityServiceVssProviderInstallFailedOutOfMemory,
        ASRMobilityServiceVssProviderInstallFailedServiceDatabaseLocked,
        ASRMobilityServiceVssProviderInstallFailedServiceAlreadyExists,
        ASRMobilityServiceVssProviderInstallFailedServiceMarkedForDeletion,
        ASRMobilityServiceVssProviderInstallFailedCscriptAccessDenied,
        ASRMobilityServiceVssProviderInstallFailedPathNotFound,
        ASRMobilityServiceVSSProviderInstallFailedDCOMServiceError,
        ASRMobilityServiceInstallerMandatoryFilesMissing,
        ASRMobilityServiceBootUEFISecureBootNotSupported,
        ASRMobilityServiceInstallerCommandExecutionFailure,
        ASRMobilityServiceInstallerUnsupportedSHA2SigningFailureWindows2008,
        ASRMobilityServiceInstallerUnsupportedSHA2SigningFailureWindows2008R2,
        ASRMobilityServiceInstallerUpgradeMandatoryFilesMissing,
        ASRMobilityServiceInstallerRequiredMemoryError,
        ASRMobilityServiceInstallerInvolFltDriverError
    };

    public enum ExitStatus
    {
        Success = 0,
        ErrorSystemRequirementMismatch = 1,
        ErrorArgumentsMissing = -1,
        ErrorUnknownException = -2,
        WarningSystemRequirementMismatch = 2,
        OutofMemoryException = 3
    };
}

