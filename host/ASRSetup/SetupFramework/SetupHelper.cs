using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Management;
using System.Net;
using System.Runtime.InteropServices;
using System.Security;
using System.Security.Cryptography;
using System.ServiceProcess;
using System.Text;
using System.Threading;
using System.Windows;
using System.Xml;
using Microsoft.Win32;
using ASRResources;
using Newtonsoft.Json.Linq;

namespace ASRSetupFramework
{
    /// <summary>
    /// Allowed values for role to be passed.
    /// </summary>
    public enum AgentInstallRole
    {
        /// <summary>
        /// Mobility service role.
        /// </summary>
        MS,

        /// <summary>
        /// Master target role.
        /// </summary>
        MT
    }

    /// <summary>
    /// Setup action.
    /// </summary>
    public enum SetupAction
    {
        /// <summary>
        /// If current version and installed version are same we check filter
        /// driver status.
        /// </summary>
        CheckFilterDriver,

        /// <summary>
        /// First time install.
        /// </summary>
        Install,

        /// <summary>
        /// Upgrade is being run.
        /// </summary>
        Upgrade,

        /// <summary>
        /// If current version and installed version are same and
        /// post install actions fail, run post install actions alone.
        /// </summary>
        ExecutePostInstallSteps,

        /// <summary>
        /// Invalid operation (e.g. downgrade) is being run.
        /// </summary>
        InvalidOperation,

        /// <summary>
        /// Upgrade is not supported from this version.
        /// </summary>
        UnSupportedUpgrade
    }

    /// <summary>
    /// Virtualization platforms supported.
    /// </summary>
    public enum Platform
    {
        /// <summary>
        /// Azure virtualization platform.
        /// </summary>
        Azure,

        /// <summary>
        /// VmWare virtualization platform.
        /// </summary>
        VmWare,

        /// <summary>
        /// Azure Stack Hub virtualization platform.
        /// </summary>
        AzureStackHub
    }

    /// <summary>
    /// Agent setup invoker.
    /// </summary>
    public enum SetupInvoker
    {
        /// <summary>
        /// Agent invoked by Push install.
        /// </summary>
        pushinstall,

        /// <summary>
        /// Agent invoked by VmWare tools.
        /// </summary>
        vmtools,

        /// <summary>
        /// Agent invoked by Command line.
        /// </summary>
        commandline
    }

    /// <summary>
    /// Operating system installed.
    /// </summary>
    public enum OSType
    {
        /// <summary>
        /// Vista (OS Version 6.0.0)
        /// </summary>
        Vista,

        /// <summary>
        /// Windows 7 (OS version 6.1.0)
        /// </summary>
        Win7,

        /// <summary>
        /// Windows 8 (OS version 6.2.0)
        /// </summary>
        Win8,

        /// <summary>
        /// Windows 8.1 (OS version 6.3.0)
        /// </summary>
        Win81,

        /// <summary>
        /// All other OSes.
        /// </summary>
        Unsupported
    }

    /// <summary>
    /// Operation to be performed by msiexec.
    /// </summary>
    public enum MsiOperation
    {
        /// <summary>
        /// Install a MSI
        /// </summary>
        Install,

        /// <summary>
        /// Upgrade existing bits.
        /// </summary>
        Upgrade,

        /// <summary>
        /// Uninstall a MSI
        /// </summary>
        Uninstall
    }

    /// <summary>
    /// Installation type for MS
    /// </summary>
    public enum InstallationType
    {
        /// <summary>
        /// Fresh Install
        /// </summary>
        Install,

        /// <summary>
        /// Upgrade
        /// </summary>
        Upgrade
    }

    /// <summary>
    /// CSType for vmware installation
    /// </summary>
    public enum ConfigurationServerType
    {
        /// <summary>
        /// Legacy CS installation
        /// </summary>
        CSLegacy,

        /// <summary>
        /// CSPrime Installation
        /// </summary>
        CSPrime
    }

    /// <summary>
    /// The operation being performed
    /// </summary>
    public enum OperationsPerformed
    {
        Installer,

        Configurator
    }

    /// <summary>
    /// Setup helper methods.
    /// </summary>
    public partial class SetupHelper
    {
        #region Unified Setup Return Values Enums
        /// <summary>
        /// List of values that the setup can return
        /// </summary>
        public enum SetupReturnValues
        {
            /// <summary>
            /// setup was successful
            /// </summary>
            Successful = 0,

            /// <summary>
            /// Setup failed
            /// </summary>
            Failed = 1,

            /// <summary>
            /// Command Line is not valid
            /// </summary>
            InvalidCommandLine = 2,

            /// <summary>
            /// Performing invalid operation
            /// </summary>
            PrerequisiteChecksFailed = 4,

            /// <summary>
            /// DRA instalaltion failed
            /// </summary>
            DRAInstallationFailed = 101,

            /// <summary>
            /// Single return code for any service which is not running
            /// </summary>
            ServicesAreNotRunning = 102,

            /// <summary>
            /// Vault registration failed
            /// </summary>
            VaultRegistrationFailed = 103,

            /// <summary>
            /// MARS Agent Installation Failed
            /// </summary>
            MARSAgentInstallationFailed = 111,

            /// <summary>
            /// Container Registration Failed
            /// </summary>
            ContainerRegistrationFailed = 112,

            /// <summary>
            /// MT Container Registration Failed
            /// </summary>
            MTContainerRegistrationFailed = 113,

            /// <summary>
            /// MARS Agent Proxy is not set.
            /// </summary>
            MARSAgentProxyIsNotSet = 115,

            /// <summary>
            /// Configuration Manager Installation Failed
            /// </summary>
            ConfigManagerInstallationFailed = 116,

            /// <summary>
            /// Product Is Already Installed
            /// </summary>
            ProductIsAlreadyInstalled = 201,

            /// <summary>
            /// Upgrade Not Supported
            /// </summary>
            UpgradeNotSupported = 203,

            /// <summary>
            /// Setup was successful and we need a reboot
            /// </summary>
            SuccessfulNeedReboot = 3010,
        }
        #endregion

        #region Unified Agent Return Values Enums
        /// <summary>
        /// List of values that the setup can return
        /// </summary>
        public enum UASetupReturnValues
        {
            /// <summary>
            /// setup was successful
            /// </summary>
            Successful = 0,

            /// <summary>
            /// Setup failed
            /// </summary>
            Failed = 1,

            /// <summary>
            /// Command Line is not valid
            /// </summary>
            InvalidCommandLine = 2,

            /// <summary>
            /// Command line arguments are not valid
            /// </summary>
            InvalidCommandLineArguments = 3,

            /// <summary>
            /// Invalid installation location was specified.
            /// </summary>
            InvalidInstallLocation = 10,

            /// <summary>
            /// Found a version from which upgrade is not supported.
            /// </summary>
            UpgradeNotSupported = 11,

            /// <summary>
            /// Agent is pointing to different CS.
            /// </summary>
            AgentPointingToDifferentCS = 13,

            /// <summary>
            /// Invalid Passphrase or CS End Point.
            /// </summary>
            InvalidPassphraseOrCSEndPoint = 14,

            /// <summary>
            /// Downgrade is not supported.
            /// </summary>
            DowngradeNotSupported = 15,

            /// <summary>
            /// Dot Net Is Not Available
            /// </summary>
            DotNetIsNotAvailable = 16,

            /// <summary>
            /// Agent Registration Failed
            /// </summary>
            AgentRegistrationFailed = 17,

            /// <summary>
            /// VMware Tools are not available
            /// </summary>
            VMwareToolsAreNotAvailable = 18,

            /// <summary>
            /// Invalid Source config file.
            /// </summary>
            InvalidSourceConfigFile = 19,

            /// <summary>
            /// An Instance Is Already Running
            /// </summary>
            AnInstanceIsAlreadyRunning = 41,

            /// <summary>
            /// Unsupported OS.
            /// </summary>
            UnsupportedOS = 42,

            /// <summary>
            /// Unable to stop agent services.
            /// </summary>
            UnableToStopServices = 43,

            /// <summary>
            /// Unable to start agent services.
            /// </summary>
            UnableToStartServices = 44,

            /// <summary>
            /// VSSProvider installation failure.
            /// </summary>
            VSSProviderInstallationFailed = 45,

            /// <summary>
            /// Indskflt value not updated in Upper filter registry key.
            /// </summary>
            IndskfltValueNotUpdatedInUpperFilterRegistry = 47,

            /// <summary>
            /// Failed to set service startup type.
            /// </summary>
            FailedToSetServiceStartupType = 48,

            /// <summary>
            /// VSS installation failed - out of memory.
            /// </summary>
            VssInstallFailedOutOfMemory = 50,

            /// <summary>
            /// VSS installation failed - service database locked.
            /// </summary>
            VssInstallFailedServiceDatabaseLocked = 51,

            /// <summary>
            /// VSS installation failed - service already exits.
            /// </summary>
            VssInstallFailedServiceAlreadyExists = 52,

            /// <summary>
            /// VSS installation failed - service marked for deletion.
            /// </summary>
            VssInstallFailedServiceMarkedForDeletion = 53,

            /// <summary>
            /// VSS installation failed - cscript acccess denied.
            /// </summary>
            VssInstallFailedCscriptAccessDenied = 54,

            /// <summary>
            /// VSS installation failed - path not found.
            /// </summary>
            VssInstallFailedPathNotFound = 55,

            /// <summary>
            /// Failed to create symbolic links.
            /// </summary>
            FailedToCreateSymlinks = 56,

            /// <summary>
            /// Failed to install and configure services.
            /// </summary>
            FailedToInstallAndConfigureServices = 57,

            /// <summary>
            /// System restart is pending
            /// </summary>
            SystemRestartPending = 62,

            /// <summary>
            /// COM+ applications pre-requisites failure.
            /// </summary>
            VSSProviderPrereqFailure = 63,

            /// <summary>
            /// Boot Drivers are not available.
            /// </summary>
            BootDriversAreNotAvailable = 64,

            /// <summary>
            /// Block Mobility service installation on ASRServerSetup.
            /// </summary>
            BlockMSInstallationOnASRServerSetup = 65,

            /// <summary>
            /// Agent role change is not allowed.
            /// </summary>
            AgentRoleChangeNotAllowed = 75,

            /// <summary>
            /// Platform change is not allowed.
            /// </summary>
            PlatformChangeNotAllowed = 76,

            /// <summary>
            /// Registration of CDPCLI failed.
            /// </summary>
            CDPCLIRegistrationFailed = 77,

            /// <summary>
            /// Invalid proxy details.
            /// </summary>
            InvalidProxyDetails = 78,

            /// <summary>
            /// Failed to configure proxy.
            /// </summary>
            ProxyConfigurationFailed = 79,

            /// <summary>
            /// Failed with out of memory exception
            /// </summary>
            OutOfMemoryException = 90,

            /// <summary>
            /// Mobility agent installation failed with errors. Errors present in json file.
            /// </summary>
            FailedWithErrors = 97,

            /// <summary>
            /// Mobility sgent installation failed with warnings. Errors present in json file.
            /// </summary>
            SucceededWithWarnings = 98,

            /// <summary>
            /// Product pre-requsites valiation failed.
            /// </summary>
            ProductPrereqsValidationFailed = 99,

            #region RCM registration.
            // Azure Configuration Error codes 100 - 150

            /// <summary>
            /// RCM credentials file is missing.
            /// </summary>
            CredentialsFileMissing = 100,

            /// <summary>
            /// Provided credentials file is not in correct format.
            /// </summary>
            CorruptCredentialsFile = 101,

            /// <summary>
            /// Provided credentials file has expired.
            /// </summary>
            CredentialsFileExpired = 102,

            /// <summary>
            /// Failed to get Bios Id from machine.
            /// </summary>
            BiosIdDetectionFailed = 103,

            /// <summary>
            /// BiosId mimatch found.
            /// </summary>
            BiosIdMismatchFound = 118,

            /// <summary>
            /// Configurator fails with this error code, when AzureRcmCli returns any error code that is not in 120-149 range.
            /// </summary>
            RCMRegistrationFailure = 119,

            /// <summary>
            /// Exit code when source registration with csprime fails
            /// </summary>
            CSPrimeLoginFailure = 128,

            /// <summary>
            /// Exit code for the case when Rcm Proxy is not registered with rcm
            /// </summary>
            RcmProxyRegistrationFailureExitCode = 129,

            /// <summary>
            /// Exit code for the case when agent config input failed to generate
            /// </summary>
            AgentConfigInputFailure = 130,

            /// <summary>
            /// Exit code for the case when unconfigure agent fail
            /// </summary>
            UnconfigureAgentFailure = 131,

            /// <summary>
            /// Exit code for the case when diagnose and fix fail
            /// </summary>
            DiagnoseAndFixFailure = 132,

            #endregion
            /// <summary>
            /// Setup was successful but a reboot is mandatory
            /// </summary>
            SuccessfulMandatoryReboot = 1641,

            /// <summary>
            /// Setup was successful but a reboot is recommended
            /// </summary>
            SuccessfulRecommendedReboot = 3010,
        }

        #endregion
       
        /// <summary>
        /// Shows the Error to the user using a messagebox.
        /// </summary>
        /// <param name="errorMessage">The error message.</param>
        /// <param name="uiHandle">UI handle to show modal messages.</param>
        public static void ShowError(string errorMessage, BasePageForWpfControls uiHandle = null)
        {
            if (!PropertyBagDictionary.Instance.PropertyExists(PropertyBagDictionary.Silent))
            {
                Trc.Log(LogLevel.Always, errorMessage);
                if (uiHandle == null)
                {
                    System.Windows.MessageBox.Show(
                       errorMessage,
                       StringResources.SetupMessageBoxTitle,
                       MessageBoxButton.OK,
                       MessageBoxImage.Error);
                }
                else
                {
                    uiHandle.Dispatcher.Invoke((Action)(() =>
                    {
                        System.Windows.MessageBox.Show(
                            Application.Current.MainWindow,
                            errorMessage,
                            StringResources.SetupMessageBoxTitle,
                            MessageBoxButton.OK,
                            MessageBoxImage.Error);
                    }));
                }
            }
            else
            {
                ConsoleUtils.Log(LogLevel.Error, errorMessage);
            }
        }

        /// <summary>
        /// Shows the Error to the user using a messagebox.
        /// </summary>
        /// <param name="errorMessage">The error message.</param>
        /// <param name="uiHandle">UI handle to show modal messages.</param>
        public static void ShowErrorUA(string errorMessage, BasePageForWpfControls uiHandle = null)
        {
            if (!PropertyBagDictionary.Instance.PropertyExists(PropertyBagDictionary.Silent))
            {
                Trc.Log(LogLevel.Always, errorMessage);
                if (uiHandle == null)
                {
                    System.Windows.MessageBox.Show(
                       errorMessage,
                       StringResources.SetupMessageBoxTitleUA,
                       MessageBoxButton.OK,
                       MessageBoxImage.Error);
                }
                else
                {
                    uiHandle.Dispatcher.Invoke((Action)(() =>
                    {
                        System.Windows.MessageBox.Show(
                            Application.Current.MainWindow,
                            errorMessage,
                            StringResources.SetupMessageBoxTitle,
                            MessageBoxButton.OK,
                            MessageBoxImage.Error);
                    }));
                }
            }
            else
            {
                ConsoleUtils.Log(LogLevel.Error, errorMessage);
            }
        }

        /// <summary>
        /// Sets the log path.
        /// </summary>
        /// <returns>a string that is the FQN of the log path.</returns>
        public static string SetLogFolderPath()
        {
            string folderPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), UnifiedSetupConstants.LogFolder);
            if (!Directory.Exists(folderPath))
            {
                Directory.CreateDirectory(folderPath);
            }

            return folderPath;
        }

        /// <summary>
        /// Sets the log file path.
        /// </summary>
        /// <param name="logFilename">The log filename.</param>
        /// <returns>a string that is the FQN of the log file.</returns>
        public static string SetLogFilePath(string logFilename)
        {
            return Path.Combine(SetLogFolderPath(), logFilename);
        }

        /// <summary>
        /// Log the property bag values.
        /// </summary>
        public static void LogPropertyBag()
        {
            Trc.Log(LogLevel.Debug, "Property Bag Values:");
            foreach (KeyValuePair<string, object> pair in PropertyBagDictionary.Instance)
            {
                if (PropertyBagDictionary.IsProtectedProperty(pair.Key))
                {
                    // We do not want to show the passwords.
                    Trc.Log(LogLevel.Debug, "{0} = Due to data security, we will not print the value of the property bag key {0}", pair.Key);
                }
                else
                {
                    // if this can be enumerated then print all values.
                    if (pair.Value is string)
                    {
                        // string is also IEnumerable !
                        Trc.Log(LogLevel.Debug, "{0} = {1}", pair.Key, pair.Value.ToString());
                    }
                    else if (pair.Value is XmlDocument)
                    {
                        // XmlDocument is also IEnumerable !
                        XmlDocument doc = pair.Value as XmlDocument;
                        Trc.Log(LogLevel.Debug, pair.Key);
                        Trc.LogLine(LogLevel.Debug);
                        Trc.Log(LogLevel.Debug, doc.InnerXml);
                        Trc.LogLine(LogLevel.Debug);
                    }
                    else if (pair.Value is IEnumerable)
                    {
                        IEnumerable enumerable = pair.Value as IEnumerable;
                        Trc.Log(LogLevel.Debug, "Collection {0} ({1}):", pair.Key, enumerable.GetType().ToString());
                        int count = 0;
                        foreach (object value in enumerable)
                        {
                            if (!value.GetType().Equals(typeof(string)))
                            {
                                Trc.Log(LogLevel.Debug, "\t[{0}] = {1} ({2}) ", count.ToString(), value.ToString(), value.GetType().ToString());
                            }
                            else
                            {
                                Trc.Log(LogLevel.Debug, "\t[{0}] = {1} ", count.ToString(), value.ToString());
                            }

                            count++;
                        }
                    }
                    else if (pair.Value != null)
                    {
                        Trc.Log(LogLevel.Debug, "{0} = {1}", pair.Key, pair.Value.ToString());
                    }
                }
            }

            Trc.Log(LogLevel.Debug, "End of list Property Bag Values.");
        }

        /// <summary>
        /// Shows the message to the user using a messagebox if UI flow is running.
        /// Otherwise writes message on console.
        /// </summary>
        /// <param name="message">Message to be displayed to user.</param>
        /// <param name="buttons">Buttons/Options to be shown to user.</param>
        /// <param name="image">Image to be shown on the messagebox.</param>
        /// <returns>User selected option, true for yes and ok, false for no and cancel and null otherwise.</returns>
        public static bool? ShowMessage(
            string message,
            MessageBoxButton buttons = MessageBoxButton.YesNo,
            MessageBoxImage image = MessageBoxImage.Asterisk)
        {
            if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagDictionary.Silent))
            {
                ConsoleUtils.Log(LogLevel.Always, message);
            }
            else
            {
                Trc.Log(LogLevel.Always, message);
                MessageBoxResult result = System.Windows.MessageBox.Show(
                    message,
                    StringResources.SetupMessageBoxTitle,
                    buttons,
                    image);
                if (result == MessageBoxResult.Yes || result == MessageBoxResult.OK)
                {
                    return true;
                }
                else if (result == MessageBoxResult.No || result == MessageBoxResult.Cancel)
                {
                    return false;
                }
            }

            return null;
        }

        /// <summary>
        /// Shows the message to the user using a messagebox if UI flow is running.
        /// Otherwise writes message on console.
        /// </summary>
        /// <param name="message">Message to be displayed to user.</param>
        /// <param name="buttons">Buttons/Options to be shown to user.</param>
        /// <param name="image">Image to be shown on the messagebox.</param>
        /// <returns>User selected option, true for yes and ok, false for no and cancel and null otherwise.</returns>
        public static bool? ShowMessageUA(
            string message,
            MessageBoxButton buttons = MessageBoxButton.YesNo,
            MessageBoxImage image = MessageBoxImage.Asterisk)
        {
            if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagDictionary.Silent))
            {
                ConsoleUtils.Log(LogLevel.Always, message);
            }
            else
            {
                Trc.Log(LogLevel.Always, message);
                MessageBoxResult result = System.Windows.MessageBox.Show(
                    message,
                    StringResources.SetupMessageBoxTitleUA,
                    buttons,
                    image);
                if (result == MessageBoxResult.Yes || result == MessageBoxResult.OK)
                {
                    return true;
                }
                else if (result == MessageBoxResult.No || result == MessageBoxResult.Cancel)
                {
                    return false;
                }
            }

            return null;
        }

        /// <summary>
        /// Shows the message to the user using a messagebox if UI flow is running.
        /// Otherwise writes message on console.
        /// </summary>
        /// <param name="message">Message to be displayed to user.</param>
        /// <param name="buttons">Buttons/Options to be shown to user.</param>
        /// <param name="image">Image to be shown on the messagebox.</param>
        /// <returns>User selected option, true for yes and ok, false for no and cancel and null otherwise.</returns>
        public static bool? ShowMessagePatch(
            string message,
            MessageBoxButton buttons = MessageBoxButton.YesNo,
            MessageBoxImage image = MessageBoxImage.Asterisk)
        {
            if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagDictionary.Silent))
            {
                ConsoleUtils.Log(LogLevel.Always, message);
            }
            else
            {
                Trc.Log(LogLevel.Always, message);
                MessageBoxResult result = System.Windows.MessageBox.Show(
                    message,
                    StringResources.SetupPatchMessageBoxTitle,
                    buttons,
                    image);
                if (result == MessageBoxResult.Yes || result == MessageBoxResult.OK)
                {
                    return true;
                }
                else if (result == MessageBoxResult.No || result == MessageBoxResult.Cancel)
                {
                    return false;
                }
            }

            return null;
        }

        /// <summary>
        /// Return a string compressed version of date.
        /// </summary>
        /// <param name="dateTime">Object to be string compressed</param>
        /// <returns>string in format YYMMDD-HHMMSS</returns>
        public static string CompressDate(DateTime dateTime)
        {
            string compressDate = dateTime.Year.ToString().Substring(2) +
                dateTime.Month.ToString().PadLeft(2, '0') +
                dateTime.Day.ToString().PadLeft(2, '0');
            string compressTime = dateTime.Hour.ToString().PadLeft(2, '0') +
                dateTime.Minute.ToString().PadLeft(2, '0') +
                dateTime.Second.ToString().PadLeft(2, '0');
            return compressDate + "-" + compressTime;
        }

        /// <summary>
        /// Get the location at which current CS/PS is installed. 
        /// </summary>
        /// <returns>Location of installation.</returns>
        public static string GetCSInstalledLocation()
        {
            string cSInstallLocation = (string)Registry.GetValue(
                UnifiedSetupConstants.CSPSRegistryName,
                UnifiedSetupConstants.InstDirRegKeyName,
                string.Empty);
            Trc.Log(LogLevel.Always, "CS install location : {0}", cSInstallLocation);

            return cSInstallLocation;
        }

        /// <summary>
        /// Get the Host Guid from Amethyst config. 
        /// </summary>
        /// <returns>Host Guid value</returns>
        public static string GetHostGuidFromAmethystConfig()
        {
            Trc.Log(LogLevel.Always, "Begin GetHostGuidFromAmethystConfig.");
            string csInstallDirectory = GetCSInstalledLocation();
            string amethystConfig = Path.Combine(
                csInstallDirectory,
                UnifiedSetupConstants.AmethystFile);
            Trc.Log(LogLevel.Always, "amethystConfig : {0}", amethystConfig);

            return GrepUtils.GetKeyValueFromFile(
                amethystConfig,
                UnifiedSetupConstants.AmethystConfigHostGuid);
        }

        /// <summary>
        /// Get the mode of installation (CSPSMT/PSMT) from Amethyst config. 
        /// </summary>
        /// <returns>mode of installation</returns>
        public static string GetInstalledServerMode()
        {
            string serverMode = UnifiedSetupConstants.CSServerMode;
            try
            {
                Trc.Log(LogLevel.Always, "Fetching Server mode.");
                string csType =
                    GrepUtils.GetKeyValueFromFile(
                        Path.Combine(GetCSInstalledLocation(), UnifiedSetupConstants.AmethystFile),
                        UnifiedSetupConstants.ConfigurationType);
                Trc.Log(LogLevel.Always, "CX_TYPE value in amethyst.conf - {0}", csType);
                if (csType == "3")
                {
                    serverMode = UnifiedSetupConstants.CSServerMode;
                }
                else if (csType == "2")
                {
                    serverMode = UnifiedSetupConstants.PSServerMode;
                }
            }
            catch (Exception ex)
            {
                Trc.LogErrorException(
                    "Exception occurred at GetInstalledServerMode - ", ex);
            }

            return serverMode;
        }

        /// <summary>
        /// Get the location at which current MS\MT is installed. This routine
        /// assumes MS\MT is already installed.
        /// </summary>
        /// <returns>Location of installation.</returns>
        public static string GetAgentInstalledLocation()
        {
            string agentInstallLocation = (string)Registry.GetValue(
                UnifiedSetupConstants.AgentRegistryName,
                UnifiedSetupConstants.InstDirRegKeyName,
                string.Empty);
            Trc.Log(LogLevel.Always, "Reading value {0} from key {1}", UnifiedSetupConstants.InstDirRegKeyName, UnifiedSetupConstants.InstDirRegKeyName);
            Trc.Log(LogLevel.Always, "Agent install location : {0}", agentInstallLocation);

            return agentInstallLocation;
        }

        /// <summary>
        /// Gets the installed role (MS\MT).
        /// </summary>
        /// <returns>Gets the currently installed role.</returns>
        public static AgentInstallRole GetAgentInstalledRole()
        {
            string agentRole = ValidationHelper.GetKeyValueFromFile(
                Path.Combine(
                    GetAgentInstalledLocation(),
                    UnifiedSetupConstants.DrScoutConfigRelativePath),
                "Role");
            Trc.Log(LogLevel.Always, "Role of the agent in drscout.conf: {0}", agentRole);

            return (agentRole == "MasterTarget") ? AgentInstallRole.MT : AgentInstallRole.MS;
        }

        /// <summary>
        /// Gets the installed version of MS\MT.
        /// </summary>
        /// <returns>Installed MS\MT version.</returns>
        public static Version GetAgentInstalledVersion()
        {
            RegistryKey hklm = Registry.LocalMachine;

            using (RegistryKey inMageReg = hklm.OpenSubKey(UnifiedSetupConstants.InMageVXRegistryPath))
            {
                return new Version(inMageReg.GetValue(UnifiedSetupConstants.AgentProductVersion).ToString());
            }
        }

        /// <summary>
        /// Gets the backing platform.
        /// </summary>
        /// <returns>Gets the current backing platform.</returns>
        public static Platform GetAgentInstalledPlatform()
        {
            try
            {
                RegistryKey hklm = Registry.LocalMachine;

                using (RegistryKey inMageReg = hklm.OpenSubKey(UnifiedSetupConstants.InMageProductKeys))
                {
                    return inMageReg.GetValue(
                        UnifiedSetupConstants.MachineBackingPlatform,
                        Platform.VmWare.ToString())
                        .ToString().AsEnum<Platform>().Value;
                }
            }
            catch (Exception ex)
            {
                Trc.LogErrorException(
                    "Platform is set to VmWare. Exception occurred at GetAgentInstalledPlatform : {0}",
                    ex);
                return Platform.VmWare;
            }
        }

        /// <summary>
        /// Gets last setup action Install or upgrade.
        /// </summary>
        /// <returns>Last setup action.</returns>
        public static SetupAction GetLastAgentSetupAction()
        {
            using (RegistryKey imageKey =
               Registry.LocalMachine.OpenSubKey(UnifiedSetupConstants.InMageVXRegistryPath))
            {
                return imageKey.GetValue(
                    UnifiedSetupConstants.InMageInstallActionKey, string.Empty)
                    .ToString().AsEnum<SetupAction>().Value;
            }
        }

        /// <summary>
        /// Checks if Agent is configured completely until last step.
        /// </summary>
        /// <returns>True is agent is configured completely, false otherwise.</returns>
        public static bool IsAgentConfiguredCompletely()
        {
            string agentConfiguration;

            Trc.Log(
                LogLevel.Always,
                "Checking registry to see agent is already configured");

            RegistryKey hklm = Registry.LocalMachine;
            using (RegistryKey inMageVxAgent = hklm.OpenSubKey(UnifiedSetupConstants.LocalMachineMSMTRegName, true))
            {
                agentConfiguration = (string)inMageVxAgent.GetValue(UnifiedSetupConstants.AgentConfigurationStatus);
            }

            Trc.Log(
                LogLevel.Always,
                "Agent configuration value in registry : " + agentConfiguration);

            return (agentConfiguration == UnifiedSetupConstants.SuccessStatus) ? true : false;
        }

        /// <summary>
        /// Checks if Agent is already configured or not.
        /// </summary>
        /// <returns>True is agent is already configured, false otherwise.</returns>
        public static bool IsAgentConfigured()
        {
            Trc.Log(
                LogLevel.Always,
                "Checking DrScout config to see agent is already configured");

            IniFile drScoutInfo = new IniFile(Path.Combine(
                GetAgentInstalledLocation(),
                UnifiedSetupConstants.DrScoutConfigRelativePath));

            string hostId = drScoutInfo.ReadValue("vxagent", "HostId");

            Trc.Log(
                LogLevel.Always,
                "HostId value in DRScout config is : " + hostId);

            return !string.IsNullOrEmpty(hostId);
        }

        /// <summary>
        /// Fetch the HostID from DrScout config.
        /// </summary>
        /// <returns>hostId value</returns>
        public static string FetchHostIDFromDrScout()
        {
            string hostId = "";
            try
            {
                Trc.Log(
                    LogLevel.Always,
                    "Fetching HostID from DrScout config");

                string agentInstallLocation =
                    GetAgentInstalledLocation();

                if (!string.IsNullOrEmpty(agentInstallLocation))
                {
                    IniFile drScoutInfo = new IniFile(Path.Combine(
                        agentInstallLocation,
                        UnifiedSetupConstants.DrScoutConfigRelativePath));

                    hostId = drScoutInfo.ReadValue("vxagent", "HostId");

                    Trc.Log(
                        LogLevel.Always,
                        "HostId value in DRScout config is : " + hostId);
                }
            }
            catch (Exception ex)
            {
                Trc.LogErrorException("Exception occurred at FetchHostIDFromDrScout : ", ex);
            }
            return hostId;
        }

        /// <summary>
        /// Gets the backing platform.
        /// </summary>
        /// <returns>Gets the platform from drscout.</returns>
        public static string GetAgentInstalledPlatformFromDrScout()
        {
            string platform = string.Empty;
            try
            {
                Trc.Log(
                    LogLevel.Always,
                    "Fetching platform details from DrScout config.");

                IniFile drScoutInfo = new IniFile(Path.Combine(
                    GetAgentInstalledLocation(),
                    UnifiedSetupConstants.DrScoutConfigRelativePath));

                platform = drScoutInfo.ReadValue("vxagent", "VmPlatform");

                Trc.Log(
                    LogLevel.Always,
                    "platform in DRScout config is set to : " + platform);
            }
            catch (Exception ex)
            {
                Trc.LogErrorException(
                    "Exception occurred at GetAgentInstalledPlatformFromDrScout : ",
                    ex);
            }
            return platform;
        }

        /// <summary>
        /// Executes drvutil .--enumeratedisks command to see if 
        /// disks are enumerated properly.
        /// </summary>
        /// <returns>True if reboot is needed, false otherwise.</returns>
        public static bool IsRebootRequiredPostDiskFilterInstall()
        {
            string output;
            string installationLocation = GetAgentInstalledLocation();

            if (string.IsNullOrEmpty(installationLocation))
            {
                throw new Exception("Agent install location is empty or, null");
            }

            string drvutilCmdline = Path.Combine(installationLocation, @"drvutil.exe");
            if (!File.Exists(drvutilCmdline))
            {
                throw new Exception(drvutilCmdline+" doesnt exist");
            }

            Trc.Log(LogLevel.Info, "Executing command drvutil  --EnumerateDisks");
            int errorCode = CommandExecutor.ExecuteCommand(
                drvutilCmdline,
                out output,
                " --EnumerateDisks");
            Trc.Log(LogLevel.Info, "drvutil output: {0}", output);

            if (0 != errorCode)
            {
                Trc.Log(LogLevel.Warn, "drvutil  --EnumerateDisks failed with error={0}", errorCode);
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.MandatoryRebootRequired, true);
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.InstallationStatus, 
                                            UnifiedSetupConstants.InstallationStatus.Failure);

                ErrorLogger.LogInstallerError(
                    new InstallerException(
                        UASetupErrorCode.ASRMobilityServiceInstallerSuccessfulMandatoryReboot.ToString(),
                        null,
                        UASetupErrorCodeMessage.SuccessfulRecommendedReboot
                        )
                    );

                return true;
            }

            Trc.Log(LogLevel.Warn, "drvutil --EnumerateDisks succeeded with error={0}", errorCode);
            return false;
        }

        /// <summary>
        /// Gets proxy settings from proxy configuration 
        /// and stores the same into property bag.
        /// </summary>
        public static void RetrieveExistingProxySettings()
        {
            Trc.Log(
                LogLevel.Always,
                "Checking DrScout config to get proxy path.");

            IniFile drScoutInfo = new IniFile(Path.Combine(
                GetAgentInstalledLocation(),
                UnifiedSetupConstants.DrScoutConfigRelativePath));
            string proxyPath = drScoutInfo.ReadValue("vxagent", "ProxySettingsPath");

            if (string.IsNullOrEmpty(proxyPath))
            {
                    // use the default path
                    string appData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
                    proxyPath = Path.Combine(appData, UnifiedSetupConstants.ProxyConfRelativePath);
            }

            if (File.Exists(proxyPath))
            {
                Trc.Log(
                    LogLevel.Always,
                    "Getting proxy settings from proxy file.");
                IniFile proxyInfo = new IniFile(proxyPath);
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ProxyAddress,
                    proxyInfo.ReadValue("proxy", "Address"));
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ProxyPort,
                    proxyInfo.ReadValue("proxy", "Port"));
            }
            else
            {
                Trc.Log(
                    LogLevel.Always,
                    "No proxy settings found.");
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ProxyAddress,
                    string.Empty);
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ProxyPort,
                    string.Empty);
            }
        }

        /// <summary>
        /// Check is CX is installed on this machine or not.
        /// </summary>
        /// <returns>True if CX is installed, false otherwise.</returns>
        public static bool IsCXInstalled()
        {
            if ((Registry.GetValue(
                UnifiedSetupConstants.CSPSRegistryName,
                    "Version",
                    null) != null) &&
                ServiceControlFunctions.IsInstalled("tmansvc"))
            {
                Trc.Log(
                    LogLevel.Always,
                    "CX is installed on this machine");
                return true;
            }
            else
            {
                Trc.Log(
                    LogLevel.Always,
                    "CX is NOT installed on this machine");
                return false;
            }
        }

        /// <summary>
        /// Verifies if current operating system is 64-bit.
        /// </summary>
        /// <returns>True if OS is 64-bit otherwise false.</returns>
        public static bool Is64BitOperatingSystem()
        {
            Trc.Log(LogLevel.Always, "Checking for OS Architecture");
            bool bIs64BitArchitecture = false;

            // If current process is running as 64-bit process then int pointer size is 8 bytes.
            if (8 == IntPtr.Size)
            {
                bIs64BitArchitecture = true;
            }
            else
            {
                // If it is 32-bit process it is wow process
                using (Process p = Process.GetCurrentProcess())
                {
                    bool bIsWow64Process = false;
                    if (!NativeMethods.IsWow64Process(p.Handle, out bIsWow64Process))
                    {
                        int errorCode = Marshal.GetLastWin32Error();
                        throw new Exception("Is64BitOperatingSystem failed to identify OS Architecture. IsWow64Process failed with error code " + errorCode + ".");
                    }
                    if (bIsWow64Process)
                    {
                        bIs64BitArchitecture = true;
                    }
                }
            }

            if (bIs64BitArchitecture)
            {
                Trc.Log(LogLevel.Always, "OS Architecture is 64-bit");
            }
            else
            {
                Trc.Log(LogLevel.Always, "OS Architecture is 32-bit");
            }

            return bIs64BitArchitecture;
        }

        /// <summary>
        /// Gets the type of OS currently installed.
        /// </summary>
        /// <out="productType">Product type</param>
        /// <returns>Type of OS installed.</returns>
        public static OSType GetOperatingSystemType(out int productType)
        {
            Trc.Log(LogLevel.Always, "Checking for OS Version");

            try
            {
                // Query system for Operating System information
                ObjectQuery query = new ObjectQuery(
                    "SELECT version, producttype FROM Win32_OperatingSystem");

                Version VistaVersion = new Version("6.0.6002");

                Version Win7Version = new Version("6.1.0");
                Version Win7SP1Version = new Version("6.1.7601");
                Version Win8Version = new Version("6.2.0");
                Version Win81Version = new Version("6.3.0");
                Version HigherVersion = new Version("10.1");

                ManagementScope scope = new ManagementScope("\\\\" + SetupHelper.GetFQDN() + "\\root\\cimv2");
                scope.Connect();

                ManagementObjectSearcher searcher = new ManagementObjectSearcher(scope, query);
                foreach (ManagementObject os in searcher.Get())
                {
                    string osVersion = os["version"].ToString();
                    productType = int.Parse(os["producttype"].ToString());

                    Trc.Log(LogLevel.Always, "OS version value is - " + osVersion);
                    Trc.Log(LogLevel.Always, "OS ProductType value is - " + productType);

                    Version currentOSVersion = new Version(osVersion);

                    if (currentOSVersion >= Win81Version)
                    {
                        return OSType.Win81;
                    }

                    if (currentOSVersion >= Win8Version)
                    {
                        return OSType.Win8;
                    }

                    if (currentOSVersion >= Win7SP1Version)
                    {
                        return OSType.Win7;
                    }

                    if (currentOSVersion >= Win7Version)
                    {
                        return OSType.Unsupported;
                    }

                    if (currentOSVersion >= VistaVersion)
                    {
                        return OSType.Vista;
                    }
                }
            }
            catch (Exception exp)
            {
                Trc.LogException(LogLevel.Always, "Failed to determine OS Version", exp);
            }

            productType = -1;
            return OSType.Unsupported;
        }

        /// <summary>
        /// Gets serial number for the BIOS of current machine.
        /// </summary>
        /// <returns>Gets BIOS Id.</returns>
        public static string GetBiosHardwareId()
        {
            Trc.Log(LogLevel.Always, "Getting BIOS Id");

            try
            {
                // Query system for Operating System information
                ObjectQuery query = new ObjectQuery(
                    "SELECT UUID FROM Win32_ComputerSystemProduct");

                ManagementScope scope = new ManagementScope("\\\\" + SetupHelper.GetFQDN() + "\\root\\cimv2");
                scope.Connect();

                ManagementObjectSearcher searcher = new ManagementObjectSearcher(scope, query);
                foreach (ManagementObject bios in searcher.Get())
                {
                    string uuid = bios["UUID"].ToString();

                    Trc.Log(LogLevel.Always, "Bios ID for this machine is : " + uuid);

                    return uuid;
                }
            }
            catch (Exception exp)
            {
                Trc.LogException(LogLevel.Always, "Failed to determine BiosId", exp);
            }

            return null;
        }

        /// <summary>
        /// Checks if a virtual machine is running on Azure.
        /// </summary>
        /// <returns>True if a virtual machine is running on Azure, false otherwise.</returns>
        public static bool IsAzureVirtualMachine()
        {
            Trc.Log(LogLevel.Always, "Checking for virtual machine platform.");
            try
            {
                ObjectQuery query = new ObjectQuery(
                    "SELECT * FROM Win32_SystemEnclosure");

                ManagementScope scope = new ManagementScope("\\\\" + SetupHelper.GetFQDN() + "\\root\\cimv2");
                scope.Connect();

                ManagementObjectSearcher searcher = new ManagementObjectSearcher(scope, query);
                foreach (ManagementObject os in searcher.Get())
                {
                    string assetTag = os["SMBIOSAssetTag"].ToString();
                    Trc.Log(LogLevel.Always, "SMBIOSAssetTag value is : " + assetTag);

                    if (assetTag == "7783-7084-3265-9085-8269-3286-77" && !IsAzureStackVirtualMachine())
                    {
                        Trc.Log(LogLevel.Always, "Virtual machine is running on Azure.");
                        return true;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Virtual machine is not running on Azure.");
                    }
                }
            }
            catch (Exception exp)
            {
                Trc.LogException(LogLevel.Always, "Exception at IsAzureVirtualMachine: ", exp);
            }
            return false;
        }

        /// <summary>
        /// Checks if a virtual machine is running on Azure Stack.
        /// </summary>
        /// <returns>True if a virtual machine is running on Azure Stack, false otherwise.</returns>
        public static bool IsAzureStackVirtualMachine()
        {
            // This function may be called before drscout is created. Check PropertyBagDictionary before check drscout.
            if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.IsAzureStackHubVm))
            {
                return PropertyBagDictionary.Instance.GetProperty<bool>(
                    PropertyBagConstants.IsAzureStackHubVm);
            }

            Trc.Log(LogLevel.Always, "Checking IsAzureStackHubVm from drscout.");
            string agentInstalledLocation = GetAgentInstalledLocation();
            if (string.IsNullOrEmpty(agentInstalledLocation))
            {
                return false;
            }

            // if drscount does not exist, it returns "" and this function returns false.
            string isAzureStackHubVm = ValidationHelper.GetKeyValueFromFile(
                Path.Combine(
                    agentInstalledLocation,
                    UnifiedSetupConstants.DrScoutConfigRelativePath),
                "IsAzureStackHubVm");

            return isAzureStackHubVm == "1";
        }

        /// <summary>
        /// Shuts down InMage agent services.
        /// </summary>
        /// <returns>True if all services were shutdown, false otherwise.</returns>
        public static bool ShutdownInMageAgentServices()
        {
            bool retVal = true;
            UnifiedSetupConstants.InMageAgentServices.ForEach(service =>
            {
                try
                {
                    if (ServiceControlFunctions.IsInstalled(service)
                    && ServiceControlFunctions.IsEnabled(service))
                    {
                        if (service == "cxprocessserver" && IsCXInstalled())
                        {
                            Trc.Log(
                                LogLevel.Always,
                                "As Configuration/Process server is installed, cxprocessserver service is not stopped.");
                        }
                        else if (ServiceControlFunctions.StopService(service))
                        {
                            Trc.Log(LogLevel.Always, "Stopped service " + service);

                            uint procId = ServiceControlFunctions.GetProcessID(service.ToString());
                            if (procId != 0)
                            {
                                Trc.Log(LogLevel.Error, "Process still running for service {0} with PID : {1}", service.ToString(), procId);
                                if (!ServiceControlFunctions.KillServiceProcess(service))
                                {
                                    Trc.Log(LogLevel.Error, "Failed to kill the process for service {0} with PID : {1}", service.ToString(), procId);
                                    retVal = false;
                                }
                                else
                                {
                                    try
                                    {
                                        CommandExecutor.RunCommand("\"sc.exe\"", "stop \"" + service.ToString() + "\"", 10000);
                                        Thread.Sleep(5000);
                                    }
                                    catch (Exception ex)
                                    {
                                        Trc.Log(LogLevel.Always, "Failed to stop service {0} with exception {1}", service.ToString(), ex);
                                    }
                                    finally
                                    {
                                        uint pId = ServiceControlFunctions.GetProcessID(service.ToString());
                                        Trc.Log(LogLevel.Always, "The process Id for the service {0} is {1}", service.ToString(), pId.ToString());
                                    }
                                    Trc.Log(LogLevel.Always, "Successfully killed the process for service {0} with PID : {1}", service.ToString(), procId);
                                }
                            }
                        }
                        else
                        {
                            Trc.Log(LogLevel.Error, "Failed to stop service " + service);
                            if (!ServiceControlFunctions.KillServiceProcess(service))
                            {
                                retVal = false;
                            }
                            else
                            {
                                try
                                {
                                    CommandExecutor.RunCommand("\"sc.exe\"", "stop \"" + service.ToString() + "\"", 10000);
                                    Thread.Sleep(5000);
                                }
                                catch (Exception ex)
                                {
                                    Trc.Log(LogLevel.Always, "Failed to stop service {0} with exception {1}", service.ToString(), ex);
                                }
                                finally
                                {
                                    uint pId = ServiceControlFunctions.GetProcessID(service.ToString());
                                    Trc.Log(LogLevel.Always, "The process Id for the service {0} is {1}", service.ToString(), pId.ToString());
                                }
                            }
                        }
                    }
                }
                catch(Exception ex)
                {
                    Trc.Log(LogLevel.Always, "Failed to stop service {0} with exception {1}.", service.ToString(), ex);
                    retVal = false;
                }
            });

            return retVal;
        }

        /// <summary>
        /// Check if Unified agent is installed or not.
        /// </summary>
        /// <returns>True if unified agent is installed, false otherwise.</returns>
        private static bool IsUAInstalled()
        {
            if ((Registry.GetValue(
                UnifiedSetupConstants.AgentRegistryName,
                "Version",
                null) != null) &&
                ServiceControlFunctions.IsInstalled("svagents"))
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        /// <summary>
        /// Get process id for a process
        /// </summary>
        /// <param name="serviceName"></param>
        /// <returns></returns>
        public static List<uint> GetProcessID(string processName)
        {
            uint procId = 0;
            List<uint> procIdList = new List<uint>();
            try
            {
                string query = string.Format(
                "SELECT ProcessId, ParentProcessId, CommandLine FROM Win32_Process WHERE Name='{0}'",
                processName);
                ManagementObjectSearcher obSearcher = new ManagementObjectSearcher(query);
                foreach (ManagementObject obj in obSearcher.Get())
                {
                    procId = (uint)obj["ProcessId"];
                    procIdList.Add(procId);
                    Trc.Log(LogLevel.Always, "Process ID: {0}", procId);
                    Trc.Log(LogLevel.Always, "Parent process id for the process is {0}", obj.GetPropertyValue("ParentProcessId"));
                    Trc.Log(LogLevel.Always, "Command line used to start process is {0}", obj.GetPropertyValue("CommandLine"));
                }
            }
            catch(Exception ex)
            {
                Trc.Log(LogLevel.Error, "Failed to get process id for the process {0} with exception {1}", processName, ex);
            }

            return procIdList;
        }

        /// <summary>
        /// Gets the vault location from the registry.
        /// </summary>
        /// <returns>Vault location.</returns>
        public static string GetVaultLocationFromCSPSRegistry()
        {
            string vaultLocation = (string)Registry.GetValue(
                UnifiedSetupConstants.CSPSRegistryName,
                UnifiedSetupConstants.VaultGeoRegKeyName,
                string.Empty);
            Trc.Log(LogLevel.Always, "Vault Location : {0}", vaultLocation);

            return vaultLocation;
        }

        /// <summary>
        /// Build WebProxy object from values in PropertyBagDictionary.
        /// </summary>
        /// <param name="proxyAddress">proxy address</param>
        /// <param name="proxyPort">proxy port</param>
        /// <param name="proxyUsername">proxy username</param>
        /// <param name="proxyPassword">proxy password</param>
        /// <returns>WebProxy object</returns>
        public static WebProxy BuildWebProxyFromPropertyBag(
            string proxyAddress = null,
            string proxyPort = null,
            string proxyUsername = null,
            SecureString proxyPassword = null)
        {
            if (string.IsNullOrEmpty(proxyAddress))
            {
                proxyAddress = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ProxyAddress);
                proxyPort = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ProxyPort);
                proxyUsername = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ProxyUsername);
                proxyPassword = PropertyBagDictionary.Instance.GetProperty<SecureString>(PropertyBagConstants.ProxyPassword);
            }

            Trc.Log(LogLevel.Always, "proxyAddress : {0}", proxyAddress);
            Trc.Log(LogLevel.Always, "proxyPort : {0}", proxyPort);
            Trc.Log(LogLevel.Always, "proxyUsername : {0}", proxyUsername);
            Trc.Log(LogLevel.Always, "proxyPassword : {0}", proxyPassword);


            WebProxy webProxy = null;
            if (!string.IsNullOrEmpty(proxyAddress))
            {
                Trc.Log(LogLevel.Always, "Using Proxy");
                UriBuilder buildURI;
                try
                {
                    buildURI = new UriBuilder(proxyAddress);
                    if (!string.IsNullOrEmpty(proxyPort))
                    {
                        buildURI.Port = Int32.Parse(proxyPort);
                    }

                    if (!string.IsNullOrEmpty(proxyUsername))
                    {
                        Trc.Log(LogLevel.Always, "Proxy requires Authentication");
                        webProxy = new WebProxy(
                                                buildURI.Uri,
                                                true,
                                                new string[0],
                                                new NetworkCredential(proxyUsername, SecureStringHelper.SecureStringToString(proxyPassword)));
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Proxy does not require Authentication");
                        webProxy = new WebProxy(buildURI.Uri);
                    }
                }
                catch
                {
                    SetupHelper.ShowError(StringResources.InvalidProxySettings);
                }
            }

            return webProxy;
        }

        /// <summary>
        /// Gets proxy details
        /// </summary>
        public static void GetProxyDetails()
        {
            try
            {
                string proxyAddress;
                string port;
                string userName;
                string bypassUrls;
                SecureString password = new SecureString();
                bool bypassProxy = true;

                Trc.Log(LogLevel.Always, "GetProxyDetails");

                GetProxySettingsFromRegistry(out proxyAddress, out port, out userName, out bypassUrls, out password, out bypassProxy);
                Trc.Log(LogLevel.Always, "Proxy address - {0}", proxyAddress);
                Trc.Log(LogLevel.Always, "Proxy port - {0}", port);
                Trc.Log(LogLevel.Always, "Proxy userName - {0}", userName);
                Trc.Log(LogLevel.Always, "Proxy bypassUrls - {0}", bypassUrls);
                Trc.Log(LogLevel.Always, "Proxy bypassDefault - {0}", bypassProxy.ToString());
                if (bypassProxy)
                {
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ProxyType, UnifiedSetupConstants.BypassProxy);
                    Trc.Log(LogLevel.Always, "Proxy type - bypass");
                }
                else
                {
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ProxyType, UnifiedSetupConstants.CustomProxy);
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ProxyAddress, proxyAddress);
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ProxyPort, port);
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ProxyUsername, userName);
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.BypassProxyURLs, bypassUrls);
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ProxyPassword, password);
                    Trc.Log(LogLevel.Always, "Proxy type - custom");
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Always, "Exception occured while getting proxy details.");
                Trc.Log(LogLevel.Always, "Exception message - {0}", ex.Message);
                Trc.Log(LogLevel.Always, "Exception stack trace - {0}", ex.StackTrace);
            }
        }

        /// <summary>
        /// Gets the system FQDN
        /// </summary>
        /// <returns>FQDN string</returns>
        public static string GetFQDN()
        {
            bool isnetworkavailable = System.Net.NetworkInformation.NetworkInterface.GetIsNetworkAvailable();
            string hostName = System.Net.Dns.GetHostName();
            string domainName = System.Net.NetworkInformation.IPGlobalProperties.GetIPGlobalProperties().DomainName;

            if (isnetworkavailable && !hostName.Contains(domainName))
            {
                hostName = string.Format("{0}.{1}", hostName, domainName);
            }

            return hostName;
        }

        /// <summary>
        /// Checks if a product is installed or not.
        /// </summary>
        /// <param name="productName">Name of product to be searched.</param>
        /// <param name="productVersion">Version of the installed product.</param>
        /// <returns>True if product is installed, false otherwise.</returns>
        public static bool IsInstalled(string productName, out string productVersion)
        {
            productVersion = "";

            try
            {
                // Query system for Operating System information
                ObjectQuery query = new ObjectQuery("SELECT * FROM Win32_Product");

                ManagementScope scope = new ManagementScope("\\\\" + GetFQDN() + "\\root\\cimv2");
                scope.Connect();

                ManagementObjectSearcher searcher = new ManagementObjectSearcher(scope, query);
                foreach (ManagementObject product in searcher.Get())
                {
                    object productNameWmiObj = product["Name"];
                    if (productNameWmiObj == null)
                    {
                        continue;
                    }

                    string productNameWmi = productNameWmiObj.ToString();

                    if (productNameWmi.Contains(productName))
                    {
                        if (product["Version"] != null)
                        {
                            productVersion = product["Version"].ToString();
                        }

                        return true;
                    }
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Always, "Failed to check installation for " + productName, ex);
            }

            return false;
        }

        /// <summary>
        /// Checks is the service is currently present on the system.
        /// </summary>
        /// <param name="serviceName">The service name.</param>
        /// <returns>True if service is present, false otherwise.</returns>
        public static bool IsServicePresent(string serviceName)
        {
            try
            {
                ServiceController sc = null;
                ServiceController[] services = ServiceController.GetServices();
                foreach (ServiceController controller in services)
                {
                    if (controller.ServiceName == serviceName)
                    {
                        sc = controller;
                        break;
                    }
                }

                if (sc != null)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(
                    LogLevel.Always,
                    string.Format("Failed to check service status for {0}", serviceName),
                    ex);
                throw;
            }
        }

        /// <summary>
        /// Checks whether a driver is present or not.
        /// </summary>
        /// <param name="driverName">The driver name.</param>
        /// <returns>True if drive is present, false otherwise.</returns>
        public static bool IsDriverPresent(string driverName)
        {
            try
            {
                ServiceController sc = null;
                ServiceController[] drivers = ServiceController.GetDevices();
                foreach (ServiceController driver in drivers)
                {
                    if (driver.ServiceName == driverName)
                    {
                        sc = driver;
                        break;
                    }
                }

                if (sc != null)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(
                    LogLevel.Always,
                    string.Format("Failed to check driver status for {0}", driverName),
                    ex);
            }
            return false;
        }

        /// <summary>
        /// Copies directory
        /// </summary>
        /// <param name="sourceDirName">Source directory name.</param>
        /// <param name="destDirName">Destination directory name.</param>
        private static void DirectoryCopy(string sourceDirName, string destDirName)
        {
            // Get the subdirectories for the specified directory.
            DirectoryInfo dir = new DirectoryInfo(sourceDirName);
            DirectoryInfo[] dirs = dir.GetDirectories();

            if (!dir.Exists)
            {
                throw new DirectoryNotFoundException(
                    "Source directory does not exist or could not be found: "
                    + sourceDirName);
            }

            // If the destination directory doesn't exist, create it.
            if (!Directory.Exists(destDirName))
            {
                Directory.CreateDirectory(destDirName);
            }

            // Get the files in the directory and copy them to the new location.
            FileInfo[] files = dir.GetFiles();
            foreach (FileInfo file in files)
            {
                string temppath = Path.Combine(destDirName, file.Name);
                Trc.Log(LogLevel.Always, "Copying {0} to {1}", temppath, destDirName);
                file.CopyTo(temppath, true);
            }

            foreach (DirectoryInfo subdir in dirs)
            {
                string temppath = Path.Combine(destDirName, subdir.Name);
                DirectoryCopy(subdir.FullName, temppath);
            }
        }

        /// <summary>
        /// Checks if the input port is a positive integer between 1 and 65535.
        /// </summary>
        /// <param name="strPort">Port number to validate.</param>
        /// <returns>True, if port is valid else false.</returns>
        public static bool IsPortValid(string strPort)
        {
            ushort port;
            if (!string.IsNullOrEmpty(strPort) &&
                (!UInt16.TryParse(strPort, out port) ||
                port == 0))
            {
                Trc.Log(LogLevel.Always, string.Format("Invalid port: {0}", strPort));
                SetupHelper.ShowError(StringResources.InvalidPort);
                return false;
            }

            return true;
        }

        /// <summary>
        /// Take any file and get MD5SUM and then
        /// returns md5sum string
        /// </summary>
        /// <param name="filename">input file you will enterd to get md5sum</param>
        /// <returns>return the md5sum string</returns>
        public static string GetMD5HashFromFile(string fileName)
        {
            try
            {
                using (var md5 = MD5.Create())
                {
                    using (var stream = File.OpenRead(fileName))
                    {
                        return BitConverter.ToString(md5.ComputeHash(stream)).Replace("-", string.Empty);
                    }
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(
                    LogLevel.Always,
                    string.Format("Failed to compute md5sum string for {0}", fileName),
                    ex);
                return "";
            }
        }

        /// <summary>
        /// Gets input file MD5SUM and compare it with
        /// the stored MD5SUM text
        /// </summary>
        /// <param name="mysqlSetupfile">input file to get md5sum</param>
        /// <param name="actualHashData">pre calculated md5sum</param>
        /// <returns>true or false depending on input validation</returns>
        public static bool ValidateMySQLMD5HashData(string mysqlSetupfile, string actualHashData)
        {
            try
            {
                //Get md5sum input file and save it string variable
                string getHashData = GetMD5HashFromFile(mysqlSetupfile);

                if (string.Compare(getHashData, actualHashData) == 0)
                {
                    return true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Incomplete MySQL Setup file downloaded with MD5SUM {0}", getHashData);
                    return false;
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(
                    LogLevel.Always,
                    string.Format("Failed to validate md5sum string for {0}", mysqlSetupfile),
                    ex);
                return false;
            }
        }

        /// <summary>
        /// To Check whether required MySQL setup installed or not
        /// </summary>
        public static bool IsMySQLInstalled(string version)
        {
            string installedVersion;
            try
            {
                string reqString = version.Remove(version.LastIndexOf(@"."), 3);
                bool isMySqlInstalled = SetupHelper.IsInstalled(string.Format("MySQL Server {0}", reqString), out installedVersion);
                if (isMySqlInstalled)
                {
                    Trc.Log(LogLevel.Always, "Current installed MySQL version (" + installedVersion + ").");
                    if (installedVersion == version)
                    {
                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.MySQLInstalled, "Yes");
                        Trc.Log(LogLevel.Always, "Required version (" + installedVersion + ") of MySQL already installed.");
                        return true;
                    }
                    else
                    {
                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.MySQLInstalled, "No");
                        Trc.Log(LogLevel.Always, "A previous or later version (" + installedVersion + ") of MySQL already installed.");
                        return false;
                    }
                }
                else
                {
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.MySQLInstalled, "No");
                    Trc.Log(LogLevel.Always, "MySQL is not installed on the server.");
                    return false;
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(
                    LogLevel.Always,
                    string.Format("Failed to determine whether required MySQL version installed or not."),
                    ex);
                return false;
            }
        }

        /// <summary>
        /// Check whether the address starts with https.
        /// </summary>
        public static bool IsAddressHasHTTPS(string address)
        {
            if (address.StartsWith("https"))
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        /// <summary>
        /// Check whether the address starts with http://
        /// </summary>
        public static bool IsAddressHasHTTP(string address)
        {
            if (address.StartsWith("http://"))
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        /// <summary>
        /// Get the unicode string
        /// </summary>
        /// <param name="sr">Stream reader</param>
        /// <returns>unicode string</returns>
        private static string GetUnicodeEncoding(StreamReader sr)
        {
            string output = sr.ReadToEnd();
            Encoding encoding = sr.CurrentEncoding;

            UnicodeEncoding unicodeEncoding = new UnicodeEncoding();
            return unicodeEncoding.GetString(encoding.GetBytes(output));
        }

        /// <summary>
        /// Append the second string to the first string if second string is not null or emty
        /// </summary>
        /// <param name="errorString">first string</param>
        /// <param name="appendString">second string</param>
        /// <returns>Appended string</returns>
        private static string AppendCustomError(string errorString, string appendString)
        {
            if (string.IsNullOrEmpty(appendString))
            {
                return errorString;
            }

            return string.Format("{0}\r\n{1}", errorString, appendString);
        }

        /// <summary>
        /// Move setup logs to uploaded folder.
        /// </summary>
        public static void MoveAgentSetupLogsToUploadedFolder()
        {
            try
            {
                Trc.Log(LogLevel.Always, "Begin MoveSetupLogsToUploadedFolder.");

                string[] LogFiles = Directory.GetFiles(SetLogFolderPath());
                string uploadFolderPath = Path.Combine(
                    Environment.GetFolderPath(
                        Environment.SpecialFolder.CommonApplicationData),
                    UnifiedSetupConstants.LogUploadFolder);

                if (!Directory.Exists(uploadFolderPath))
                {
                    Directory.CreateDirectory(uploadFolderPath);
                }

                foreach (string file in LogFiles)
                {
                    string destFile = Path.Combine(
                        uploadFolderPath,
                        string.Concat(
                            SetupHelper.CompressDate(DateTime.UtcNow),
                            Path.GetFileName(file)));

                    Trc.Log(LogLevel.Always,
                        "Moving {0} to {1}",
                        file,
                        destFile);
                    File.Move(
                        file,
                        destFile);
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error,
                    "Exception at MoveAgentSetupLogsToUploadedFolder: {0}", ex);
            }
        }

        /// <summary>
        /// Performs MSI operations.
        /// </summary>
        /// <param name="silent">True if operations are to be done in silent manner.</param>
        /// <param name="operation">Operation to perform.</param>
        /// <param name="workingDirectory">Working directory.</param>
        /// <param name="strFileName">Name of file to perform operations on.</param>
        /// <param name="strLogFileName">Log file name.</param>
        /// <param name="msiArguments">Arguments for msi.</param>
        /// <param name="customError">Custom error to append.</param>
        /// <returns>True if installation succeeded, false otherwise.</returns>
        public static bool MSIHelper(
            bool silent,
            MsiOperation operation,
            string workingDirectory,
            string strFileName,
            string strLogFileName,
            string msiArguments,
            string customError,
            out int exitCode)
        {
            try
            {
                Trc.Log(
                    LogLevel.Always,
                    "{0} {1}",
                    (operation == MsiOperation.Install) ? "Installating" :
                    (operation == MsiOperation.Upgrade) ? "Upgrading" :
                    "Uninstalling",
                    strFileName);

                ProcessStartInfo startInfo = new ProcessStartInfo();
                startInfo.UseShellExecute = false;
                startInfo.RedirectStandardError = true;
                startInfo.RedirectStandardOutput = true;
                startInfo.Verb = "runas";
                startInfo.FileName = "msiexec.exe";
                startInfo.Arguments = string.Format(
                    "/q{0} /{1} \"{2}\\{3}\" {4} /L*v+ \"{5}\" /norestart",
                    silent ? "n" : "f",
                    (operation == MsiOperation.Install) ? "i" : (operation == MsiOperation.Upgrade) ? "update" : "x",
                    workingDirectory,
                    strFileName,
                    msiArguments,
                    SetupHelper.SetLogFilePath(strLogFileName));

                Trc.Log(LogLevel.Always,
                    "Starting MSI with arguments - {0}",
                    startInfo.Arguments);

                Process process = new Process();
                process.StartInfo = startInfo;
                process.Start();

                string error = GetUnicodeEncoding(process.StandardError);
                string output = GetUnicodeEncoding(process.StandardOutput);

                process.WaitForExit();

                // Mark that MSI logs needs to be collected.
                SetupReportData.AddLogFilePath(
                    SetupHelper.SetLogFilePath(strLogFileName));
                exitCode = process.ExitCode;
                Trc.Log(LogLevel.Always, "Exit code from MSI : {0}", exitCode);

                switch (process.ExitCode)
                {
                    case UnifiedSetupConstants.Success:
                        Trc.Log(
                            LogLevel.Always,
                            "{0} of {1} successful",
                            operation.ToString(),
                            strFileName);
                        return true;
                    case UnifiedSetupConstants.SuccessWithRebootRequired:
                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagDictionary.RebootRequired, true);
                        Trc.Log(
                            LogLevel.Always,
                            "{0} of {1} successful, reboot is required",
                            operation.ToString(),
                            strFileName);
                        return true;
                    case UnifiedSetupConstants.FailureUninstallInstalledProduct:
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ErrorMessage,
                            StringResources.IncompatibleProductInstalled);
                        Trc.Log(
                            LogLevel.Always,
                            "{0} of {1} failed with error code {2}, invalid product version installed",
                            operation.ToString(),
                            strFileName,
                            process.ExitCode);
                        return false;
                    case UnifiedSetupConstants.InvalidVersionNumber:
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ErrorMessage,
                            StringResources.IncompatibleProductInstalled);
                        Trc.Log(
                            LogLevel.Always,
                            "{0} of {1} failed with error code {2}, invalid product version installed",
                            operation.ToString(),
                            strFileName,
                            process.ExitCode);
                        return false;
                    default:
                        string errorString = process.ExitCode.ToString();

                        if (string.IsNullOrEmpty(error))
                        {
                            if (!string.IsNullOrEmpty(output))
                            {
                                errorString = string.Format("{0}: {1}", process.ExitCode.ToString(), output);
                            }
                        }
                        else
                        {
                            errorString = string.Format("{0}: {1}", process.ExitCode.ToString(), error);
                        }

                        Trc.Log(
                            LogLevel.Always,
                            "Exception in MSI {0} {1}",
                            operation.ToString(),
                            errorString);
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ErrorMessage,
                            AppendCustomError(errorString, customError));
                        return false;
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(
                    LogLevel.Always,
                    string.Format("Exception while MSI {0} ", operation.ToString()),
                    ex);
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ErrorMessage,
                    AppendCustomError(ex.Message, customError));
                exitCode = 1;
            }

            return false;
        }

        /// <summary>
        /// Gets the Operating system name.
        /// </summary>
        /// <returns>Operating system name</returns>
        public static string GetOSName()
        {
            string osName = string.Empty;
            try
            {
                ManagementObjectSearcher searcher =
                    new ManagementObjectSearcher("SELECT Caption FROM Win32_OperatingSystem");
                foreach (ManagementObject os in searcher.Get())
                {
                    osName = os["Caption"].ToString();
                    break;
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error,
                    "Exception at GetOSName: {0}", ex);
            }
            return osName;
        }

        /// <summary>
        /// Gets the asset tag associated with machine.
        /// </summary>
        /// <returns>asset tag</returns>
        public static string GetAssetTag()
        {
            string assetTag = string.Empty;
            try
            {
                ManagementObjectSearcher searcher =
                    new ManagementObjectSearcher("SELECT * FROM Win32_SystemEnclosure");
                foreach (ManagementObject os in searcher.Get())
                {
                    assetTag = os["SMBIOSAssetTag"].ToString();
                    break;
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error,
                    "Exception at GetAssetTag: {0}", ex);
            }
            return assetTag;
        }

        /// <summary>
        /// Gets the hostname of the machine.
        /// </summary>
        /// <returns>host name</returns>
        public static string GetHostName()
        {
            string hostName = string.Empty;
            try
            {
                hostName = Dns.GetHostName();
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error,
                    "Exception at GetHostName: {0}", ex);
            }
            return hostName;
        }

        /// <summary>
        /// Gets the total machine memory in GB.
        /// </summary>
        /// <returns>total machine memory</returns>
        public static double GetMachineMemory()
        {
            double totalMemoryInGB = 0;

            try
            {
                NativeMethods.MemoryStatus memoryStatus = new NativeMethods.MemoryStatus();
                totalMemoryInGB = (double)memoryStatus.TotalPhysical/(1024*1024*1024);
            }
            catch (Win32Exception ex)
            {
                Trc.Log(LogLevel.Error, "Failed to retrive total memory using GlobalMemoryStatusEx error={0} ex={1}",
                            Marshal.GetLastWin32Error(), ex);
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Failed to retrive total memory ex=", ex);
            }
            finally
            {
                Trc.Log(LogLevel.Always,
                   "Total machine memory size : {0} GB",
                   totalMemoryInGB.ToString());
            }

            return totalMemoryInGB;
        }

        /// <summary>
        /// Gets the total machine memory in Bytes.
        /// </summary>
        /// <returns>total machine memory</returns>
        public static ulong GetMachineMemoryBytes()
        {
            ulong totalMemoryInBytes = 0;

            try
            {
                NativeMethods.MemoryStatus memoryStatus = new NativeMethods.MemoryStatus();
                totalMemoryInBytes = memoryStatus.TotalPhysical;
            }
            catch (Win32Exception ex)
            {
                Trc.Log(LogLevel.Error, "Failed to retrive total memory using GlobalMemoryStatusEx error={0} ex={1}",
                            Marshal.GetLastWin32Error(), ex);
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Failed to retrive total memory ex=", ex);
            }
            finally
            {
                Trc.Log(LogLevel.Always,
                   "Total machine memory size : {0} Bytes",
                   totalMemoryInBytes.ToString());
            }

            return totalMemoryInBytes;
        }


        /// <summary>
        /// Gets the available machine memory.
        /// </summary>
        /// <returns>available machine memory</returns>
        public static double GetAvailableMemory()
        {
            double availableMemoryInGB = 0;
            try
            {
                NativeMethods.MemoryStatus memoryStatus = new NativeMethods.MemoryStatus();
                availableMemoryInGB = memoryStatus.AvailPhysical/(1024 * 1024 * 1024);
            }
            catch (Win32Exception ex)
            {
                Trc.Log(LogLevel.Error, "Failed to retrive available memory using GlobalMemoryStatusEx error={0} ex={1}",
                            Marshal.GetLastWin32Error(),ex);
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Failed to retrive available memory ex=", ex);
        }
            finally
            {
                Trc.Log(LogLevel.Always,
                    "Available machine memory size : {0} GB",
                    availableMemoryInGB.ToString());
            }

            return availableMemoryInGB;
        }

        /// <summary>
        /// Update the Proxy settings in Registry.
        /// </summary>
        /// <param name="address">Proxy Address.</param>
        /// <param name="port">Proxy Port.</param>
        /// <param name="userName">Proxy UserName.</param>
        /// <param name="password">Proxy Password.</param>
        public static void UpdateProxySettingsInRegistry(
            string address,
            string port,
            string userName,
            SecureString password,
            bool bypassProxy)
        {
            Trc.Log(
                LogLevel.Always,
                string.Format(
                "UpdateProxySettingsInRegistry : address = {0}, port = {1}, userName = {2}",
                address,
                port,
                userName));

            try
            {
                Registry.SetValue(
                    UnifiedSetupConstants.CSPSRegistryName,
                    UnifiedSetupConstants.ProxyAddressRegValue,
                    address);

                Registry.SetValue(
                    UnifiedSetupConstants.CSPSRegistryName,
                    UnifiedSetupConstants.ProxyPortRegValue,
                    port);

                Registry.SetValue(
                    UnifiedSetupConstants.CSPSRegistryName,
                    UnifiedSetupConstants.ProxyUserNameRegValue,
                    userName);

                Registry.SetValue(
                    UnifiedSetupConstants.CSPSRegistryName,
                    UnifiedSetupConstants.BypassProxy,
                    bypassProxy);

                Registry.SetValue(
                    UnifiedSetupConstants.CSPSRegistryName,
                    UnifiedSetupConstants.ProxyPasswordRegValue,
                    new byte[] { });

                if (password != null)
                {
                    string strPassword = SecureStringHelper.SecureStringToString(password);
                    Encoding encodingUnicode = Encoding.Unicode;
                    byte[] encryptedPassword = DataProtection.Encrypt(encodingUnicode.GetBytes(strPassword));

                    Registry.SetValue(
                        UnifiedSetupConstants.CSPSRegistryName,
                        UnifiedSetupConstants.ProxyPasswordRegValue,
                        encryptedPassword);

                    strPassword = null;
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Always, "UpdateProxySettingsInRegistry Failed - ", ex);
            }
        }

        /// <summary>
        /// Sends scale out unit to Configuration/Process server.
        /// </summary>
        public static void TransferScaleOutUnitLogsToCSPSSetup(string csIp)
        {
            try
            {
                if (!string.IsNullOrEmpty(csIp))
                {
                    string machineIdentifier = GetMachineIdentifier();
                    string hostName = Dns.GetHostName();
                    string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);
                    string remoteDir = Path.Combine(
                            UnifiedSetupConstants.ScaleOutUnitLogsAbsolutePath,
                            machineIdentifier + "_" + hostName);
                    string output;
                    string sslPort = UnifiedSetupConstants.CxPsSSLPort;
                    if (FetchCxPsPortFromCS(csIp, out output))
                    {
                        sslPort = output;
                    }

                    Trc.Log(LogLevel.Always, "Machine Identifier - {0}", machineIdentifier);
                    Trc.Log(LogLevel.Always, "Host Name - {0}", hostName);
                    Trc.Log(LogLevel.Always, "Remote Directory - {0}", remoteDir);
                    Trc.Log(LogLevel.Always, "CxPs ssl port - {0}", sslPort);
                    Trc.Log(LogLevel.Always, "CxPs ssl port - {0}", sslPort);

                    List<string> LogFiles = new List<string>();

                    // Add Unified Setup logs.
                    string unifiedSetupLogsFolder =
                        sysDrive +
                        UnifiedSetupConstants.UnifiedSetupLogFolder;
                    Trc.Log(LogLevel.Always, "unifiedSetupLogsFolder - {0}", unifiedSetupLogsFolder);
                    string[] unifiedSetupLogs = Directory.GetFiles(unifiedSetupLogsFolder);

                    // Add Summary files.
                    string unifiedSummaryLogsFolder =
                        sysDrive +
                        UnifiedSetupConstants.DRALogFolder;
                    Trc.Log(LogLevel.Always, "unifiedSummaryLogsFolder - {0}", unifiedSummaryLogsFolder);
                    string[] unifiedSummaryLogs = Directory.GetFiles(unifiedSummaryLogsFolder);

                    foreach (string files in unifiedSetupLogs.Union(unifiedSummaryLogs))
                    {
                        LogFiles.Add(files);
                    }

                    // Add MARS logs.
                    LogFiles.Add(sysDrive + UnifiedSetupConstants.MARSMsiLog);
                    LogFiles.Add(sysDrive + UnifiedSetupConstants.MARSPatchLog);
                    LogFiles.Add(sysDrive + UnifiedSetupConstants.MARSInstallLog);
                    LogFiles.Add(sysDrive + UnifiedSetupConstants.MARSManagedInstallLog);
                    LogFiles.Add(sysDrive + UnifiedSetupConstants.MARSServiceLog);

                    foreach (string file in LogFiles)
                    {
                        Trc.Log(LogLevel.Always, "file - {0}", file);
                        if (File.Exists(file))
                        {
                            FileTransferUsingCxpsclient(
                                csIp,
                                sslPort,
                                file,
                                remoteDir);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trc.LogErrorException(
                    "Exception at FileTransferToCSPSSetup: {0}",
                    ex);
            }
        }

        /// <summary>
        /// Get the Proxy settings from Registry.
        /// </summary>
        /// <param name="address">out param - url of the proxy server.</param>
        /// <param name="port">Port for the proxy.</param>
        /// <param name="userName">UserName for the proxy setting.</param>
        /// <param name="bypassUrls">Bypass URLs.</param>
        /// <param name="proxyPassword">Password for the proxy setting.</param>
        public static void GetProxySettingsFromRegistry(
            out string address,
            out string port,
            out string userName,
            out string bypassUrls,
            out SecureString proxyPassword,
            out bool bypassProxy)
        {
            address = null;
            port = null;
            userName = null;
            bypassUrls = null;
            proxyPassword = null;
            bypassProxy = true;

            try
            {
                string bypassProxyStr = Registry.GetValue(
                    UnifiedSetupConstants.ProxyRegistryHive,
                    UnifiedSetupConstants.ProxyBypass,
                    string.Empty).ToString();

                bypassProxy = (bypassProxyStr == true.ToString()) ?
                    true :
                    false;

                address = Registry.GetValue(
                    UnifiedSetupConstants.ProxyRegistryHive,
                    UnifiedSetupConstants.ProxyAddressRegValue,
                    null).ToString();

                port = Registry.GetValue(
                    UnifiedSetupConstants.ProxyRegistryHive,
                    UnifiedSetupConstants.ProxyPortRegValue,
                    null).ToString();

                userName = Registry.GetValue(
                    UnifiedSetupConstants.ProxyRegistryHive,
                    UnifiedSetupConstants.ProxyUserNameRegValue,
                    null).ToString();

                bypassUrls = Registry.GetValue(
                    UnifiedSetupConstants.ProxyRegistryHive,
                    UnifiedSetupConstants.ProxyBypassUrlsRegValue,
                    null).ToString();

                // Extract password details
                byte[] encyptedPassword = (byte[])Registry.GetValue(
                    UnifiedSetupConstants.ProxyRegistryHive,
                    UnifiedSetupConstants.ProxyPasswordRegValue,
                    null);

                if (encyptedPassword != null && encyptedPassword.Length > 0)
                {
                    byte[] decryptedPassword = DataProtection.Decrypt(encyptedPassword);
                    proxyPassword = SecureStringHelper.StringToSecureString(
                        Encoding.Unicode.GetString(decryptedPassword));
                }
            }
            catch (Exception ex)
            {
                Trc.LogErrorException(
                    "Exception occurred at GetProxySettingsFromRegistry - {0}",
                    ex);
            }
        }

        /// <summary>
        /// Generates machine identifier.
        /// </summary>
        public static void GenerateMachineIdentifier()
        {
            RegistryKey key = null;

            try
            {
                Guid machineIdentifier = Guid.Empty;

                // Open Inmage hive in write mode
                key = Registry.LocalMachine.OpenSubKey(
                    UnifiedSetupConstants.InmageRegistryHive,
                    true);

                // If hive found then try to get value else create the key.
                if (key != null)
                {
                    string machineIdentifierStr = key.GetValue(
                        UnifiedSetupConstants.MachineIdentifierRegKeyName,
                        string.Empty).ToString();
                    if (!string.IsNullOrEmpty(machineIdentifierStr))
                    {
                        machineIdentifier = new Guid(machineIdentifierStr);
                    }
                }
                else
                {
                    key = Registry.LocalMachine.CreateSubKey(
                        UnifiedSetupConstants.InmageRegistryHive);
                }

                // If value not found/empty, generate it.
                if (machineIdentifier == Guid.Empty)
                {
                    Trc.Log(LogLevel.Always,
                        "Generating new Guid as machineidentifier.");
                    machineIdentifier = Guid.NewGuid();
                }
                else
                {
                    Trc.Log(LogLevel.Always,
                        "Found machine identifier as {0}.",
                        machineIdentifier);
                }

                Trc.Log(LogLevel.Always,
                    "Creating machine identifier registry key with value - {0}.",
                    machineIdentifier);
                key.SetValue(
                    UnifiedSetupConstants.MachineIdentifierRegKeyName,
                    machineIdentifier);

                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.MachineIdentifier,
                    machineIdentifier);
            }
            catch (Exception e)
            {
                Trc.LogException(LogLevel.Error,
                    "Create machine identifier failed with exception",
                    e);
                throw;
            }
            finally
            {
                if (key != null)
                {
                    key.Close();
                }
            }
        }

        /// <summary>
        /// Get the machne identifier value. 
        /// </summary>
        /// <returns>machine identifier value</returns>
        public static string GetMachineIdentifier()
        {
            string machineIdentifier = (string)Registry.GetValue(
                UnifiedSetupConstants.InmageRegistryLMHive,
                UnifiedSetupConstants.MachineIdentifierRegKeyName,
                string.Empty);
            Trc.Log(LogLevel.Always,
                "Machine identifier value  in registry : {0}",
                machineIdentifier);

            return machineIdentifier;
        }

        /// <summary>
        /// Fetch the Configiurtion server IP for Telemetry from DrScout config.
        /// </summary>
        /// <returns>Configiurtion server IP</returns>
        public static string FetchCSIPforTelemetryFromDrScout()
        {
            string ip = "";
            try
            {
                Trc.Log(
                    LogLevel.Always,
                    "Fetching CSIPforTelemetry from DrScout config");

                string agentInstallLocation =
                    GetAgentInstalledLocation();

                if (!string.IsNullOrEmpty(agentInstallLocation))
                {

                    IniFile drScoutInfo = new IniFile(Path.Combine(
                        agentInstallLocation,
                        UnifiedSetupConstants.DrScoutConfigRelativePath));

                    ip = drScoutInfo.ReadValue(
                        "vxagent.transport",
                        UnifiedSetupConstants.CSIPforTelemetry);

                    Trc.Log(
                        LogLevel.Always,
                        "CSIPforTelemetry in DRScout config is - " + ip);
                }
            }
            catch (Exception ex)
            {
                Trc.LogErrorException(
                    "Exception occurred at FetchCSIPforTelemetryFromDrScout - {0}",
                    ex);
            }
            return ip;
        }

        /// <summary>
        /// Fetch the Configiurtion server IP  from DrScout config.
        /// </summary>
        /// <returns>Configiurtion server IP</returns>
        public static string FetchCSIPFromDrScout()
        {
            string ip = "";
            try
            {
                Trc.Log(
                    LogLevel.Always,
                    "Fetching FetchCSIPFromDrScout from DrScout config");

                string agentInstallLocation =
                    GetAgentInstalledLocation();

                if (!string.IsNullOrEmpty(agentInstallLocation))
                {
                    IniFile drScoutInfo = new IniFile(Path.Combine(
                        agentInstallLocation,
                        UnifiedSetupConstants.DrScoutConfigRelativePath));

                    ip = drScoutInfo.ReadValue(
                        "vxagent.transport",
                        "Hostname");

                    Trc.Log(
                        LogLevel.Always,
                        "CSIP in DRScout config is - " + ip);
                }
            }
            catch (Exception ex)
            {
                Trc.LogErrorException(
                    "Exception occurred at FetchCSIPFromDrScout - {0}",
                    ex);
            }
            return ip;
        }

        /// <summary>
        /// Sends file to Configuration/Process server using cxpsclient.
        /// </summary>
        /// <param name="ip">Configuration/Process server IP</param>
        /// <param name="sslPort">Configuration/Process server ssl port</param>
        /// <param name="sourceFile">full path of source file</param>
        /// <param name="remoteDir">remote directory path</param>
        /// <param name="retryCount">no of re-tries</param>
        /// <param name="cmdParams">command line params for cxpsclient (optional)</param>
        /// <returns>true if file is transferred, false otherwise</returns>
        public static bool FileTransferUsingCxpsclient(
            string ip,
            string sslPort,
            string sourceFile,
            string remoteDir,
            int retryCount = 3,
            string cmdParams = null)
        {
            bool isFileTransferred = false;

            try
            {
                Trc.Log(LogLevel.Always,
                    "Transferring file - {0}",
                    sourceFile);

                if (cmdParams == null)
                {
                    cmdParams =
                        string.Format(" -i \"{0}\" -l \"{1}\" --put \"{2}\" -C -d \"{3}\" ",
                           ip,
                           sslPort,
                           sourceFile,
                           remoteDir);
                }

                // Retry the operation, if error is encountered during file transfer.
                for (int i = 1; i <= retryCount; i++)
                {
                    Trc.Log(LogLevel.Always,
                        "Retry count : {0}",
                        i);
                    int retCode =
                        CommandExecutor.ExecuteCommand(
                            UnifiedSetupConstants.CxpsclientExeName,
                            cmdParams);

                    if (retCode == 0)
                    {
                        isFileTransferred = true;
                        break;
                    }
                    else
                    {
                        /* sleep for 2 secs and retry 
                         * just to handle any connectivity issues
                         * at that particular time. */
                        Thread.Sleep(2000);
                    }
                }
            }
            catch (Exception ex)
            {
                Trc.LogErrorException(
                    "Exception at SendFileToCSUsingCxpsclient: {0}",
                    ex);
            }

            return isFileTransferred;
        }

        /// <summary>
        /// Sends Agent setup logs to Configuration/Process server.
        /// </summary>
        public static void TransferAgentSetupLogsToCSPSSetup()
        {
            try
            {
                string ip = FetchCSIPforTelemetryFromDrScout();

                if ((string.IsNullOrEmpty(ip)) ||
                    (ip == UnifiedSetupConstants.CSPSDefaultIPInDrscout))
                {
                    ip = FetchCSIPFromDrScout();
                }

                if ((!string.IsNullOrEmpty(ip)) &&
                    (ip != UnifiedSetupConstants.CSPSDefaultIPInDrscout))
                {
                    string machineIdentifier = GetMachineIdentifier();
                    string hostName = Dns.GetHostName();
                    string logFolder = SetLogFolderPath();
                    string remoteDir = Path.Combine(
                            UnifiedSetupConstants.CommandLineAgentSetupLogsAbsolutePath,
                            machineIdentifier + "_" + hostName);
                    string output;
                    string sslPort = UnifiedSetupConstants.CxPsSSLPort;
                    if (FetchCxPsPortFromCS(ip, out output))
                    {
                        sslPort = output;
                    }

                    Trc.Log(LogLevel.Always, "Machine Identifier - {0}", machineIdentifier);
                    Trc.Log(LogLevel.Always, "Host Name - {0}", hostName);
                    Trc.Log(LogLevel.Always, "Log Folder - {0}", logFolder);
                    Trc.Log(LogLevel.Always, "Remote Directory - {0}", remoteDir);
                    Trc.Log(LogLevel.Always, "CxPs ssl port - {0}", sslPort);

                    List<string> ListOfFiles = new List<string>()
                    {

                    };
                    string[] FileNameArray = ListOfFiles.ToArray();

                    string[] LogFiles = Directory.GetFiles(
                        logFolder,
                        UnifiedSetupConstants.LogFiles,
                        SearchOption.TopDirectoryOnly);
                    string[] JsonFiles = Directory.GetFiles(
                        logFolder,
                        UnifiedSetupConstants.JsonFiles,
                        SearchOption.TopDirectoryOnly);

                    foreach (var file in JsonFiles.Union(LogFiles.Union(FileNameArray)))
                    {
                        string sourceFile =
                            Path.Combine(logFolder, file);
                        if (FileTransferUsingCxpsclient(
                            ip,
                            sslPort,
                            sourceFile,
                            remoteDir))
                        {
                            try
                            {
                                string uploadFolderPath = Path.Combine(
                                    Environment.GetFolderPath(
                                        Environment.SpecialFolder.CommonApplicationData),
                                    UnifiedSetupConstants.LogUploadFolder);
                                if (!Directory.Exists(uploadFolderPath))
                                {
                                    Directory.CreateDirectory(uploadFolderPath);
                                }

                                if (Path.GetExtension(file) ==
                                    UnifiedSetupConstants.JsonFileFormat)
                                {
                                    Trc.Log(LogLevel.Always,
                                        "Moving file - {0} to uploaded folder {1}.",
                                        file,
                                        Path.Combine(uploadFolderPath, Path.GetFileName(file)));
                                    File.Move(
                                        sourceFile,
                                        Path.Combine(uploadFolderPath, Path.GetFileName(file)));
                                }
                            }
                            catch (Exception exc)
                            {
                                Trc.LogErrorException(
                                    "Exception at FileTransferToCSPSSetup uploadlogs: {0}",
                                    exc);
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trc.LogErrorException(
                    "Exception at FileTransferToCSPSSetup: {0}",
                    ex);
            }
        }

        /// <summary>
        /// Sends file to Configuration/Process server using cxpsclient.
        /// </summary>
        /// <param name="ip">Configuration/Process server IP</param>
        /// <param name="sslPort">Configuration/Process server ssl port</param>
        /// <param name="sourceFile">full path of source file</param>
        /// <param name="remoteDir">remote directory path</param>
        /// <param name="retryCount">no of re-tries</param>
        /// <param name="cmdParams">command line params for cxpsclient (optional)</param>
        /// <returns>true if file is transferred, false otherwise</returns>
        public static bool FetchCxPsPortFromCS(
            string csIp,
            out string output)
        {
            bool result = false;
            output = string.Empty;
            try
            {
                Trc.Log(LogLevel.Always, "Begin FetchCxPsPortFromCS");

                string cmdParams = string.Format(" -i \"{0}\" ",
                        csIp);
                int retCode = CommandExecutor.ExecuteCommand(
                        UnifiedSetupConstants.CxcliExeName,
                        out output,
                        cmdParams);

                if (retCode == 0)
                {
                    result = true;
                }
            }
            catch (Exception ex)
            {
                Trc.LogErrorException(
                    "Exception at FetchCxPsPortFromCS - {0}",
                    ex);
            }

            return result;
        }

        /// <summary>
        /// Change service startup type.
        /// </summary>
        /// <param name="startupType"></param>
        /// <returns>true on successfully changing the startup type</returns>
        public static bool ChangeAgentServicesStartupType(
            NativeMethods.SERVICE_START_TYPE startupType)
        {
            bool result = false;

            List<string> serviceNames = new List<string>() 
            { 
                UnifiedSetupConstants.AppServiceName,
                UnifiedSetupConstants.SvagentsServiceName
            };


            /* DelayedAutoStart property is already set to service during 
               its install in MSI. So setting the startup type to automatic will
               change the startup type to Automatic (Delayed Start). */
            foreach (var serviceName in serviceNames)
            {
                if (ServiceControlFunctions.IsInstalled(serviceName)
                    && ServiceControlFunctions.IsEnabled(serviceName))
                {
                for (int i = 0; i < 3; i++)
                {
                    Trc.Log(LogLevel.Always, "Retry count - {0}", i);
                    if (ServiceControlFunctions.ChangeServiceStartupType(
                        serviceName,
                        startupType))
                    {
                        Trc.Log(
                            LogLevel.Always,
                            "Successfully set startup type to {0} for service - {1}",
                            startupType,
                            serviceName);
                        result = true;
                        break;
                    }

                    Trc.Log(
                            LogLevel.Error,
                            "Failed to set startup type to {0} for service - {1}",
                            startupType,
                            serviceName);
                }
            }
                else
                {
                    Trc.Log(LogLevel.Always, "Ther service {0} is either not installed or disabled. So skipping changing startup type to manual");
                    return true;
                }
            }

            if (result)
            {
                Trc.Log(LogLevel.Always, "Successfully changed startup type of all services to manual");
            }
            else
            {
                Trc.Log(LogLevel.Always, "Failed to set startup type of one or more service");
            }
            return result;
        }

        /// <summary>
        /// Check if svagents is a symbolic link or, permanent file.
        /// </summary>
        /// <param name="installLocation">Install location path.</param>
        /// <returns></returns>
        public static bool IsSvagentsSymbolicLink(string installLocation)
        {
            bool isSymLink = false;

            try
            {
                string filePath = installLocation + "\\svagents.exe";

                if (File.Exists(filePath))
                {
                    FileAttributes attributes = File.GetAttributes(filePath);
                    isSymLink = (FileAttributes.ReparsePoint == (FileAttributes.ReparsePoint & attributes));
                    if (isSymLink)
                    {
                        Trc.Log(LogLevel.Info, "File {0} is symbolic link", filePath);
                    }
                    else
                    {
                        Trc.Log(LogLevel.Info, "File {0} is normal file", filePath);
                    }
                }
                else
                {
                    Trc.Log(LogLevel.Info, "File {0} doesn't exist", filePath);
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, ex.ToString());
            }

            return isSymLink;
        }

        /// <summary>
        /// Sets up the path for symlinks and stops the services
        /// </summary>
        /// <param name="installLocation">Installation location</param>
        /// <param name="platform">Platform</param>
        /// <param name="operation">Installer/Configurator</param>
        /// <returns></returns>
        public static bool SetupSymbolicPaths(string installLocation, Platform platform, OperationsPerformed operation)
        {
            string svagentsTargetName = string.Empty;
            string s2TargetName = string.Empty;
            string csType = string.Empty;

            if (operation == OperationsPerformed.Installer)
            {
                if (Platform.Azure == platform)
                {
                    s2TargetName = installLocation + "\\s2RCM.exe";
                    svagentsTargetName = installLocation + "\\svagentsRCM.exe";
                }
                else if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.CSType))
                {
                    if (PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSType) ==
                    ConfigurationServerType.CSPrime.ToString())
                    {
                        s2TargetName = installLocation + "\\s2RCM.exe";
                        svagentsTargetName = installLocation + "\\svagentsRCM.exe";
                    }
                    else
                    {
                        s2TargetName = installLocation + "\\s2CS.exe";
                        svagentsTargetName = installLocation + "\\svagentsCS.exe";
                    }
                }
                else
                {
                    csType = GetCSTypeFromDrScout();
                    if (csType == ConfigurationServerType.CSPrime.ToString())
                    {
                        s2TargetName = installLocation + "\\s2RCM.exe";
                        svagentsTargetName = installLocation + "\\svagentsRCM.exe";
                    }
                    else
                    {
                        s2TargetName = installLocation + "\\s2CS.exe";
                        svagentsTargetName = installLocation + "\\svagentsCS.exe";
                    }
                }
            }
            else if (operation == OperationsPerformed.Configurator)
            {
                if (Platform.VmWare == platform && PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.AgentConfigured)
                    && !PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.AgentConfigured))
                {
                    csType = GetCSTypeFromDrScout();
                    if (csType == ConfigurationServerType.CSPrime.ToString())
                    {
                        s2TargetName = installLocation + "\\s2RCM.exe";
                        svagentsTargetName = installLocation + "\\svagentsRCM.exe";
                    }
                    else
                    {
                        s2TargetName = installLocation + "\\s2CS.exe";
                        svagentsTargetName = installLocation + "\\svagentsCS.exe";
                    }
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Symlinks are created during install in A2A");
                    return true;
                }
            }

            // Stop services before updating sym links
            // Stop Service if it is still running
            if (ServiceControlFunctions.IsInstalled(UnifiedSetupConstants.SvagentsServiceName))
            {
                if (OperationsPerformed.Installer == operation)
                {
                    SetupAction setupAction = PropertyBagDictionary.Instance.GetProperty<SetupAction>(
                        PropertyBagConstants.SetupAction);
                    if (setupAction == SetupAction.Upgrade && !ServiceControlFunctions.StopService(UnifiedSetupConstants.SvagentsServiceName))
                    {
                        Trc.Log(LogLevel.Error, "Failed to stop service {0} stopping install", UnifiedSetupConstants.SvagentsServiceName);
                        return false;
                    }
                }
                else
                {
                    if (!ServiceControlFunctions.StopService(UnifiedSetupConstants.SvagentsServiceName))
                    {
                        Trc.Log(LogLevel.Error, "Failed to stop service {0} stopping install", UnifiedSetupConstants.SvagentsServiceName);
                        return false;
                    }
                }
            }

            return CreateSymLinks(installLocation, s2TargetName, svagentsTargetName);
        }

        /// <summary>
        /// Create symbolic links
        /// </summary>
        /// <param name="installLocation">Installation location</param>
        /// <param name="s2TargetName">s2 symlink target file name</param>
        /// <param name="svagentsTargetName">svagents symlink target file name</param>
        /// <returns></returns>
        private static bool CreateSymLinks(string installLocation, string s2TargetName, string svagentsTargetName)
        {
            string s2SymlinkName = installLocation + "\\s2.exe";
            string svagentsSymlinkName = installLocation + "\\svagents.exe";

            // Create symbolic link for s2.exe
            IDictionary<string, string> linkTargetMap = new Dictionary<string, string>()
            {
                {s2SymlinkName, s2TargetName},
                {svagentsSymlinkName, svagentsTargetName}
            };

            foreach (var symlink in linkTargetMap.Keys)
            {
                if (!DeleteOrMoveFile(symlink, true))
                {
                    Trc.Log(LogLevel.Error, " Failed to delete file {0}.. Skipping installation", symlink);
                    return false;
                }

                if (SetupHelper.CreateSymLink(symlink,
                                            linkTargetMap[symlink],
                                            NativeMethods.SymbolicLinkFlags.SYMBOLIC_LINK_FLAG_FILE))
                {
                    Trc.Log(LogLevel.Always, "Created symbolic link for {0} Target: {1} successfully.",
                                                        symlink,
                                                        linkTargetMap[symlink]);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Failed to create symbolic link for {0} Target: {1}.", symlink,
                                                        linkTargetMap[symlink]);
                    return false;
                }

            }

            return true;
        }

        /// <summary>
        /// Deletes or moves file to a temporary file.
        /// </summary>
        /// <param name="filePath">File to be deleted/moved</param>
        /// <param name="bMoveIfDelFailed">Move to a temporary file if delete fails</param>
        /// <returns></returns>
        internal static bool DeleteOrMoveFile(string filePath, bool bMoveIfDelFailed)
        {
            bool isFileDeleted = false;

            // If file doesn't exist, it means it is already deleted.
            if (!File.Exists(filePath))
            {
                Trc.Log(LogLevel.Info, "File {0} doesn't exist. Skipping delete", filePath);
                return true;
            }

            for (int iRetry = 0; iRetry < 3; iRetry++)
            {
                if (iRetry > 0)
                {
                    // Sleep for 1 secs before trying to delete file
                    Trc.Log(LogLevel.Info, "Retry {0}: Sleeping for 1 sec before trying to delete file {1}", iRetry, filePath);
                    Thread.Sleep(1000);
                }

                try
                {
                    File.Delete(filePath);
                    Trc.Log(LogLevel.Info, "Deleted file {0} successfully", filePath);
                    isFileDeleted = true;
                    break;
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Warn, "Failed to delete file {0} on {1} attempt Exception: {2}",
                                            filePath,
                                            iRetry,
                                            ex.ToString());
                }
            }

            if (!isFileDeleted && bMoveIfDelFailed)
            {
                string currtime = DateTime.Now.ToString("yyyy-MM-ddTHH-mm-ss");
                string destFile = string.Format("{0}-{1}", filePath, currtime);

                try
                {
                    File.Move(filePath, destFile);
                    Trc.Log(LogLevel.Info, "Moved file {0} to {1} successfully", filePath, destFile);
                    isFileDeleted = true;
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, "Faile to move file {0} to file {1} Exception: {2}",
                                        filePath,
                                        destFile,
                                        ex.ToString());
                }
            }
            return isFileDeleted;
        }

        /// <summary>
        /// This function always returns a valid cstype
        /// irrespective of the state of the system.
        /// The default CSType is CSLegacy
        /// Please do not change the behaviour.
        /// </summary>
        /// <returns>Valid CSType</returns>
        public static ConfigurationServerType GetCSTypeEnumFromDrScout()
        {
            string csType = GetCSTypeFromDrScout();
            // To do: Below function returns true/false.
            // See if it can be used to enhance error detection
            csType.TryAsEnum<ConfigurationServerType>(out ConfigurationServerType csTypeEnum);
            return csTypeEnum;
        }

        /// <summary>
        /// This function always returns a valid cstype
        /// irrespective of the state of the system.
        /// The default CSType is CSLegacy
        /// Please do not change the behaviour.
        /// </summary>
        /// <returns>Valid CSType as string</returns>
        public static string GetCSTypeFromDrScout()
        {
            try
            {
                string drscoutInfoFilePath;
                do
                {
                    if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.InstallationLocation))
                    {
                        drscoutInfoFilePath = Path.Combine(PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.InstallationLocation),
                            UnifiedSetupConstants.DrScoutConfigRelativePath);
                        if (File.Exists(drscoutInfoFilePath))
                        {
                            break;
                        }

                        Trc.Log(LogLevel.Warn, "drscout.conf is not present at the location "
                            + PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.InstallationLocation) 
                            + " fetching installation path from registry");

                    }

                    drscoutInfoFilePath = Path.Combine(
                         (string)Registry.GetValue(
                            UnifiedSetupConstants.AgentRegistryName,
                            UnifiedSetupConstants.InMageInstallPathReg,
                            string.Empty),
                         UnifiedSetupConstants.DrScoutConfigRelativePath);

                    if (!File.Exists(drscoutInfoFilePath))
                    {
                        throw new FileNotFoundException("drscout.conf file not found at " + drscoutInfoFilePath);
                    }

                } while (false);
                
                IniFile drScoutInfo = new IniFile(drscoutInfoFilePath);

                string cstype = drScoutInfo.ReadValue("vxagent", PropertyBagConstants.CSType);
                // if the value is not defined in drscout.conf, then use default value CSLegacy
                if(Enum.IsDefined(typeof(ConfigurationServerType), cstype))
                {
                    return cstype;
                }
            }
            catch(Exception ex)
            {
                Trc.Log(LogLevel.Error, "Failed to get cstype from drscout.conf with exception " + ex);
            }
            return ConfigurationServerType.CSLegacy.ToString();
        }

        /// <summary>
        /// Creates symbolic link.
        /// </summary>
        /// <param name="symlinkName">Symbolic link name.</param>
        /// <param name="targetName">Target name.</param>
        /// <param name="symlinkFlag">Symbolic link flag - file/directory.</param>
        /// <returns>true when symbolic link is created successfully, false otherwise</returns>
        public static bool CreateSymLink(string symlinkName, string targetName, NativeMethods.SymbolicLinkFlags symlinkFlag)
        {
            try
            {
                Trc.Log(LogLevel.Always, "SymbolicLinkFlag: {0}", symlinkFlag.ToString());
                Trc.Log(LogLevel.Always, "Creating symbolic link to {0} with name {1}.", targetName, symlinkName);

                if (!Path.IsPathRooted(symlinkName) || !Path.IsPathRooted(targetName))
                {
                    Trc.Log(LogLevel.Always, "Symbolic link and/or target file names are not absolute paths.");
                    return false;
                }

                if (!NativeMethods.CreateSymbolicLink(symlinkName, targetName, symlinkFlag))
                {
                    Trc.Log(LogLevel.Always, "Symbolic link creation failed with error code: {0}", Marshal.GetLastWin32Error());
                    return false;
                }

                Trc.Log(LogLevel.Always, "Succesfully created symbolic link.");
                return true;
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error, "Softlink creation failed with an exception", ex);
            }

            return false;
        }

        /// <summary>
        /// Create services.
        /// </summary>
        /// <param name=installLocation>Installation location.</param>
        public static bool CreateAndConfigureServices(string installLocation)
        {
            // Install and configure svagents service.
            string svagentsServiceName = UnifiedSetupConstants.SvagentsServiceName;
            string svagentsDisplayName = UnifiedSetupConstants.SvagentsServiceDisplayName;
            string svagentsPath = Path.Combine(installLocation, UnifiedSetupConstants.SvagentsExeName);
            string svagentsDesc = UnifiedSetupConstants.SvagentsServiceDescription;

            if (!ServiceControlFunctions.InstallService(
                svagentsServiceName,
                svagentsDisplayName,
                NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START,
                svagentsPath))
            {
                Trc.Log(LogLevel.Always, "Failed to install svagents service.");
                return false;
            }

            ServiceControlFunctions.FixImagePath(svagentsServiceName);

            if (!ServiceControlFunctions.SetDelayedAutoStart(svagentsServiceName, true))
            {
                Trc.Log(LogLevel.Always, "Failed to set DelayedAutoStart property for svagents service.");
                return false;
            }

            if (!ServiceControlFunctions.SetDescription(svagentsServiceName, svagentsDesc))
            {
                Trc.Log(LogLevel.Always, "Failed to change description of svagents service.");
                return false;
            }

            ServiceControlFunctions.SetServiceDefaultFailureActions(svagentsServiceName);

            // Install and configure appservice.
            string appserviceServiceName = UnifiedSetupConstants.AppserviceName;
            string appserviceDisplayName = UnifiedSetupConstants.AppserviceDisplayName;
            string appservicePath = Path.Combine(installLocation, UnifiedSetupConstants.AppserviceExeName);
            string appserviceDesc = UnifiedSetupConstants.AppserviceDescription;

            if (!ServiceControlFunctions.InstallService(
                appserviceServiceName,
                appserviceDisplayName,
                NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START,
                appservicePath))
            {
                Trc.Log(LogLevel.Always, "Failed to install appservice.");
                return false;
            }

            ServiceControlFunctions.FixImagePath(appserviceServiceName);
            if (!ServiceControlFunctions.SetDelayedAutoStart(appserviceServiceName, true))
            {
                Trc.Log(LogLevel.Always, "Failed to set DelayedAutoStart property for appservice.");
                return false;
            }
            if (!ServiceControlFunctions.SetDescription(appserviceServiceName, appserviceDesc))
            {
                Trc.Log(LogLevel.Always, "Failed to change description of appservice.");
                return false;
            }

            ServiceControlFunctions.SetServiceDefaultFailureActions(appserviceServiceName);

            return true;
        }

        /// <summary>
        /// Update drscout values for dp and s2 after figuring out if it is legacy cs stack or csprime stack
        /// </summary>
        /// <param name="cstype"></param>
        public static void UpdateDrScout(ConfigurationServerType cstype)
        {
            string installationDir = SetupHelper.GetAgentInstalledLocation();
            if (String.IsNullOrEmpty(installationDir) || installationDir.Trim().Length == 0)
            {
                Trc.Log(LogLevel.Always, "Failed to get installation location for agent");
                throw new Exception("Failed to get installation location for agent");
            }
            IniFile drScoutInfo = new IniFile(Path.Combine(
                installationDir,
                UnifiedSetupConstants.DrScoutConfigRelativePath));

            drScoutInfo.WriteValue("vxagent", PropertyBagConstants.CSType, cstype.ToString());

            string platform = drScoutInfo.ReadValue("vxagent", "VmPlatform");

            UpdateS2AndDpPaths(platform, cstype, drScoutInfo, installationDir);
        }

        public static void UpdateS2AndDpPaths(string platform, ConfigurationServerType cstype, IniFile drScoutInfo, string installationDir)
        {
            // Writing values for s2 in drscout.conf
            string s2path = (cstype == ConfigurationServerType.CSLegacy && String.Equals(platform, Platform.VmWare.ToString(), StringComparison.OrdinalIgnoreCase)) ? @"s2CS.exe" : @"s2RCM.exe";
            drScoutInfo.WriteValue("vxagent", "DiffSourceExePathname", Path.Combine(installationDir, s2path));

            // Writing values for dataprotection in drscout.conf
            if(String.Equals(platform, Platform.VmWare.ToString(), StringComparison.OrdinalIgnoreCase))
            {
                string dppath = (cstype == ConfigurationServerType.CSPrime) ? @"DataProtectionSyncRcm.exe" : @"dataprotection.exe";
                drScoutInfo.WriteValue("vxagent", "OffloadSyncPathname", Path.Combine(installationDir, dppath));
                drScoutInfo.WriteValue("vxagent", "FastSyncExePathname", Path.Combine(installationDir, dppath));
                drScoutInfo.WriteValue("vxagent", "DiffTargetExePathname", Path.Combine(installationDir, dppath));
                drScoutInfo.WriteValue("vxagent", "DataProtectionExePathname", Path.Combine(installationDir, dppath));
            }
        }
        public static string GetOSNameFromOSVersion()
        {
            Microsoft.VisualBasic.Devices.ComputerInfo compInfo = new Microsoft.VisualBasic.Devices.ComputerInfo();
            return compInfo.OSFullName;
        }

    }

    /// <summary>
    /// Class for checking all the prerequisites like WMI calls etc.
    /// </summary>
    public class PrechecksSetupHelper
    {

        private static IList<string> WMIQueryFailureQueryParams = new List<string>(),
            WMIQueryFailureMessageParams = new List<string>();

        /// <summary>
        /// Main function that invokes all the prerequisite checks
        /// and stores their value at appropriate places
        /// </summary>
        /// <returns>Prerequisite checks succeeded or failed</returns>
        public static bool PreInstallationPrechecks()
        {
            bool isInstalled, isAzureVm, is64Bitos, prechecksResult = true, WmiFailure = false;
            string version, biosID, osName;
            ICollection<int> SystemDrives = new HashSet<int>();

            if(PrechecksSetupHelper.QueryInstallationStatus(UnifiedSetupConstants.MS_MTProductGUID, out version, out isInstalled))
            {
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.IsProductInstalled,
                    isInstalled);
                if(isInstalled)
                {
                    Trc.Log(LogLevel.Always, "Already Installed Version: {0}", (new Version(version)).ToString());
                    PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.AlreadyInstalledVersion,
                        version);
                }
            }
            else
            {
                Trc.Log(LogLevel.Error, "Failed to check if the product " + UnifiedSetupConstants.MS_MTProductName + " is already installed.");
                Trc.Log(LogLevel.Info, "MSI will give an error if product is already installed and above query failed.");
            }

            int productType;
            OSType osType = OSType.Unsupported;
            if(PrechecksSetupHelper.GetOperatingSystemType(out productType, out osType))
            {
                Trc.Log(LogLevel.Debug,string.Format("Successfully added Guest OS type {0} and Guest OS product type {1}",osType.ToString(),productType.ToString()));
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.GuestOSType, osType);
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.GuestOSProductType, productType);
            }
            else
            {
                Trc.Log(LogLevel.Error,"Failed to get Guest VM Operating System type");
                WmiFailure = true;
                prechecksResult = false;
            }

            if(PrechecksSetupHelper.GetBiosHardwareId(out biosID))
            {
                Trc.Log(LogLevel.Debug,string.Format("Successfully added BIOS ID {0} to property bag dictionary",biosID));
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.BiosId, biosID);
            }
            else
            {
                Trc.Log(LogLevel.Error,"Failed to get BiosId of the system");
                WmiFailure = true;
                prechecksResult = false;
            }

            if(PrechecksSetupHelper.IsAzureVirtualMachine(out isAzureVm))
            {
                Trc.Log(LogLevel.Debug, string.Format("The given machine is {0} Azure VM", isAzureVm ? "" : "not"));
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.IsAzureVm, isAzureVm);
            }
            else
            {
                Trc.Log(LogLevel.Error,"Failed to check if the given machine is Azure virtual machine");
                WmiFailure = true;
                prechecksResult = false;
            }

            if(PrechecksSetupHelper.GetOSName(out osName))
            {
                Trc.Log(LogLevel.Debug, "Successfully retrived os name : " + osName);
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.osName, osName);
            }
            else
            {
                Trc.Log(LogLevel.Warn, "Failed to get os name for the given machine");
                WmiFailure = true;
                prechecksResult = false;
            }

            if(PrechecksSetupHelper.GetSystemDrives(out SystemDrives))
            {
                Trc.Log(LogLevel.Debug, "Successfully retrived system drives for the given machine");
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.SystemDrives, SystemDrives);
            }
            else
            {
                Trc.Log(LogLevel.Error,"Failed to retrive system drives for the given machine");
                WmiFailure = true;
                prechecksResult = false;
            }

            if(PrechecksSetupHelper.Is64BitOperatingSystem(out is64Bitos))
            {
                Trc.Log(LogLevel.Debug, string.Format("The given machine is {0} bit machine", is64Bitos ? "64" : "32"));
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.Is64BitOS,is64Bitos);
            }
            else
            {
                Trc.Log(LogLevel.Error,"Failed to determine if the given machine is 64 bit");
                prechecksResult = false;
            }

            if(WmiFailure)
            {
                int counter = 1;
                string wmiQuery = String.Join(";", WMIQueryFailureQueryParams.ToArray()),
                    wmiQueryErrorMessage = String.Join("", WMIQueryFailureMessageParams.Select(x => (counter++).ToString() + "." + x + "\n").ToArray());
                ErrorLogger.LogInstallerError(
                    new InstallerException(
                        UASetupErrorCode.ASRMobilityServiceInstallerWMIQueryFailure.ToString(),
                        new Dictionary<string, string>()
                        {
                            {UASetupErrorCodeParams.WMIQuery, wmiQuery},
                            {UASetupErrorCodeParams.WMIQueryErrorMessage, wmiQueryErrorMessage}
                        },
                        string.Format(UASetupErrorCodeMessage.WMIQueryFailures, wmiQueryErrorMessage, wmiQueryErrorMessage)
                        )
                    );
            }

            return prechecksResult;
        }

        /// <summary>
        /// Function to check if the product is already installed
        /// </summary>
        /// <param name="productGUID">GUID of installed product</param>
        /// <param name="productVersion">Current Version Installed</param>
        /// <param name="isInstalled">boolean to indicate if Product is installed.</param>
        /// <returns>Success/Failure</returns>
        public static bool QueryInstallationStatus(string productGUID, out string productVersion, out bool isInstalled)
        {
            Trc.Log(LogLevel.Debug, "Checking if ASR is installed");

            Int32 length = 10*1024;
            StringBuilder versionInfo = new StringBuilder(length);

            productVersion = "";
            isInstalled = false;

            try
            {
                Int32 error = NativeMethods.MsiGetProductInfo(productGUID, NativeMethods.MsiAttribute.INSTALLPROPERTY_VERSIONSTRING, versionInfo, ref length);
                if (0 == error)
                {
                    isInstalled = true;
                    productVersion = versionInfo.ToString();
                    Trc.Log(LogLevel.Debug, "Product {0} version={1}", productGUID, productVersion);
                    return true;
                }

                Trc.Log(LogLevel.Error, "Failed to get product {0} version with error={1}", productGUID, error.ToString());
                return true;
            }
            catch(Exception ex)
            {
                Trc.LogException(LogLevel.Always, "Failed to check installation for " + productGUID, ex);
            }

            return false;
        }

        /// <summary>
        /// Function to get Operating System type
        /// </summary>
        /// <param name="productType"></param>
        /// <param name="osType"></param>
        /// <returns>Success/Failure</returns>
        public static bool GetOperatingSystemType(out int productType, out OSType osType)
        {
            Trc.Log(LogLevel.Always, "Checking for OS Version");

            try
            {
                // Query system for Operating System information
                ObjectQuery query = new ObjectQuery(
                    "SELECT version, producttype FROM Win32_OperatingSystem");

                Version VistaVersion = new Version("6.0.0");

                Version Win7Version = new Version("6.1.0");
                Version Win7SP1Version = new Version("6.1.7601");
                Version Win8Version = new Version("6.2.0");
                Version Win81Version = new Version("6.3.0");
                Version HigherVersion = new Version("10.1");

                ManagementScope scope = new ManagementScope("\\\\" + SetupHelper.GetFQDN() + "\\root\\cimv2");
                scope.Connect();

                ManagementObjectSearcher searcher = new ManagementObjectSearcher(scope, query);
                foreach (ManagementObject os in searcher.Get())
                {
                    string osVersion = os["version"].ToString();
                    productType = int.Parse(os["producttype"].ToString());

                    Trc.Log(LogLevel.Always, "OS version value is - " + osVersion);
                    Trc.Log(LogLevel.Always, "OS ProductType value is - " + productType);

                    Version currentOSVersion = new Version(osVersion);

                    if (currentOSVersion >= Win7SP1Version)
                    {
                        if (currentOSVersion < Win8Version)
                        {
                            osType =  OSType.Win7;
                            return true;
                        }
                        else if (currentOSVersion > Win8Version && currentOSVersion < Win81Version)
                        {
                            osType = OSType.Win8;
                            return true;
                        }
                        else
                        {
                            osType =  OSType.Win81;
                            return true;
                        }
                    }
                    else if (currentOSVersion >= Win7Version)
                    {
                        osType = OSType.Unsupported;
                        return true;
                    }
                    else if (currentOSVersion >= VistaVersion)
                    {
                        osType = OSType.Vista;
                        return true;
                    }
                }
                productType = -1;
                osType = OSType.Unsupported;
                return true;
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Always, "Failed to determine OS Version", ex);
                productType = -1;
                osType = OSType.Unsupported;
                WMIQueryFailureQueryParams.Add(WMIQueryFailureErrorReason.OperatingSystem);
                WMIQueryFailureMessageParams.Add(WMIQueryFailureErrorMessage.OperatingSystem);
                return false;
            }
        }

        /// <summary>
        /// Function to get the BIOS hardware id
        /// </summary>
        /// <param name="biosId"></param>
        /// <returns>Success/Failure</returns>
        public static bool GetBiosHardwareId(out string biosId)
        {
            Trc.Log(LogLevel.Always, "Getting BIOS Id");

            try
            {
                // Query system for Operating System information
                ObjectQuery query = new ObjectQuery(
                    "SELECT UUID FROM Win32_ComputerSystemProduct");

                ManagementScope scope = new ManagementScope("\\\\" + SetupHelper.GetFQDN() + "\\root\\cimv2");
                scope.Connect();

                ManagementObjectSearcher searcher = new ManagementObjectSearcher(scope, query);
                foreach (ManagementObject bios in searcher.Get())
                {
                    biosId = bios["UUID"].ToString();
                    Trc.Log(LogLevel.Always, "Bios ID for this machine is : " + biosId);
                    return true;
                }

                biosId = String.Empty;
                return true;
            }
            catch (Exception exp)
            {
                Trc.LogException(LogLevel.Always, "Failed to determine BiosId", exp);
                biosId = String.Empty;
                WMIQueryFailureQueryParams.Add(WMIQueryFailureErrorReason.BIOSId);
                WMIQueryFailureMessageParams.Add(WMIQueryFailureErrorMessage.BIOSId);
                return false;
            }
        }

        /// <summary>
        /// Function to check if the given machine is azure VM
        /// </summary>
        /// <param name="isAzureVM"></param>
        /// <returns>Success/Failure</returns>
        public static bool IsAzureVirtualMachine(out bool isAzureVM)
        {
            Trc.Log(LogLevel.Always, "Checking for virtual machine platform.");
            if(PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.Platform) && 
                PropertyBagDictionary.Instance.GetProperty<int>(PropertyBagConstants.Platform) == (int)Platform.VmWare)
            {
                Trc.Log(LogLevel.Info,"Platform is specified as VmWare. So skipping this check");
                isAzureVM = false;
                return true;
            }
            try
            {
                ObjectQuery query = new ObjectQuery(
                    "SELECT * FROM Win32_SystemEnclosure");

                ManagementScope scope = new ManagementScope("\\\\" + SetupHelper.GetFQDN() + "\\root\\cimv2");
                scope.Connect();

                ManagementObjectSearcher searcher = new ManagementObjectSearcher(scope, query);
                foreach (ManagementObject os in searcher.Get())
                {
                    string assetTag = os["SMBIOSAssetTag"].ToString();
                    Trc.Log(LogLevel.Always, "SMBIOSAssetTag value is : " + assetTag);

                    if (assetTag == "7783-7084-3265-9085-8269-3286-77")
                    {
                        Trc.Log(LogLevel.Always, "Virtual machine is running on Azure.");
                        isAzureVM = true;
                        return true;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Virtual machine is not running on Azure.");
                    }
                }
                isAzureVM = false;
                return true;
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Always, "Exception at IsAzureVirtualMachine: ", ex);
                isAzureVM = false;
                WMIQueryFailureQueryParams.Add(WMIQueryFailureErrorReason.SystemEnclosure);
                WMIQueryFailureMessageParams.Add(WMIQueryFailureErrorMessage.SystemEnclosure);
                return false;
            }           
        }

        /// <summary>
        /// Function to get OS name
        /// </summary>
        /// <param name="osName"></param>
        /// <returns>Success/Failure</returns>
        public static bool GetOSName(out string osName)
        {
            osName = string.Empty;
            try
            {
                ManagementObjectSearcher searcher =
                    new ManagementObjectSearcher("SELECT Caption FROM Win32_OperatingSystem");
                foreach (ManagementObject os in searcher.Get())
                {
                    osName = os["Caption"].ToString();
                    break;
                }
                return true;
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error,
                    "Exception at GetOSName: {0}", ex);
                WMIQueryFailureQueryParams.Add(WMIQueryFailureErrorReason.OperatingSystemCaption);
                WMIQueryFailureMessageParams.Add(WMIQueryFailureErrorMessage.OperatingSystem);
                return false;
            }
        }

        /// <summary>
        /// Function to get list of system drives
        /// </summary>
        /// <param name="SystemDrives"></param>
        /// <returns>Success/Failure</returns>
        public static bool GetSystemDrives(out ICollection<int> SystemDrives)
        {
            SystemDrives = new HashSet<int>();

            Trc.Log(LogLevel.Debug, "Enterting GetSystemDrives");

            // Get the system logical disk id (drive letter)
            string systemLogicalDiskDeviceId = Environment.GetFolderPath(Environment.SpecialFolder.System).Substring(0, 2);

            if (string.IsNullOrEmpty(systemLogicalDiskDeviceId))
            {
                Trc.Log(LogLevel.Error, "Failed to get Environment.SpecialFolder.System");
                ErrorLogger.LogInstallerError(
                    new InstallerException(
                        UASetupErrorCode.ASRMobilityServiceInstallerFailedToGetSystemFolder.ToString(),
                        null,
                        UASetupErrorCodeMessage.SystemFolderFailure
                        ));
                return false;               
            }

            string query = string.Format("SELECT * FROM Win32_LogicalDisk WHERE DeviceID='{0}'", systemLogicalDiskDeviceId);
            Trc.Log(LogLevel.Debug, "Query to get system disk: {0}", query);

            // Start by enumerating the logical disks
            try
            {
                using (var searcher = new ManagementObjectSearcher(query))
                {
                    foreach (ManagementObject logicalDisk in searcher.Get())
                    {
                        foreach (ManagementObject partition in logicalDisk.GetRelated("Win32_DiskPartition"))
                        {
                            foreach (ManagementObject diskDrive in partition.GetRelated("Win32_DiskDrive"))
                            {
                                SystemDrives.Add(int.Parse(diskDrive["Index"].ToString()));
                                Trc.Log(LogLevel.Debug, "Adding disk {0}", diskDrive["Index"].ToString());
                            }
                        }
                    }
                }
            }
            catch(Exception ex)
            {
                Trc.Log(LogLevel.Error, "Failed to get disk drive information ex: {0}", ex);
                WMIQueryFailureQueryParams.Add(WMIQueryFailureErrorReason.LogicalDisk);
                WMIQueryFailureMessageParams.Add(WMIQueryFailureErrorMessage.LogicalDisk);
                return false;
            }
            Trc.Log(LogLevel.Debug, "Exiting GetSystemDrives");
            return true;
        }

        /// <summary>
        /// Function to check process architecture
        /// </summary>
        /// <param name="bIs64BitArchitecture"></param>
        /// <returns>Success/Failure</returns>
        public static bool Is64BitOperatingSystem(out bool bIs64BitArchitecture)
        {
            Trc.Log(LogLevel.Always, "Checking for OS Architecture");
            bIs64BitArchitecture = false;

            // If current process is running as 64-bit process then int pointer size is 8 bytes.
            if (8 == IntPtr.Size)
            {
                bIs64BitArchitecture = true;
            }
            else
            {
                // If it is 32-bit process it is wow process
                using (Process p = Process.GetCurrentProcess())
                {
                    bool bIsWow64Process = false;
                    if (!NativeMethods.IsWow64Process(p.Handle, out bIsWow64Process))
                    {
                        int errorCode = Marshal.GetLastWin32Error();
                        Trc.Log(LogLevel.Error, "Is64BitOperatingSystem failed to identify OS Architecture. IsWow64Process failed with error code " + errorCode + ".");
                        ErrorLogger.LogInstallerError(
                            new InstallerException(
                                UASetupErrorCode.ASRMobilityServiceInstallerProcessArchitectureDeterminationFailed.ToString(),
                                new Dictionary<string, string>()
                                {
                                    {UASetupErrorCodeParams.ProcessArchitecture, errorCode.ToString()}
                                },
                                string.Format(UASetupErrorCodeMessage.ProcessArchitectureFailure, errorCode.ToString())
                                ));
                        return false;
                    }
                    if (bIsWow64Process)
                    {
                        bIs64BitArchitecture = true;
                    }
                }
            }

            if (bIs64BitArchitecture)
            {
                Trc.Log(LogLevel.Always, "OS Architecture is 64-bit");
            }
            else
            {
                Trc.Log(LogLevel.Always, "OS Architecture is 32-bit");
            }
            return true;
        }
    }
}
