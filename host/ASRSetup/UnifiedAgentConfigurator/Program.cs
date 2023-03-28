//---------------------------------------------------------------
//  <copyright file="Program.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Application entry logic and silent configuration logic.
//  </summary>
//
//  History:     05-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Security;
using System.Text;
using System.Threading;
using System.Windows;
using System.Xml;
using System.Xml.Serialization;
using ASRResources;
using ASRSetupFramework;

namespace UnifiedAgentConfigurator
{
    /// <summary>
    /// This is the main class that control the Microsoft Azure Site Recovery Provider configurator.
    /// </summary>
    public static class Program
    {
        /// <summary>
        /// Blocks multiple instances of same exe.
        /// </summary>
        private static Mutex mutex;

        /// <summary>
        /// Main entry point
        /// </summary>
        /// <returns>int a value from the UASetupReturnValues</returns>
        [System.Diagnostics.CodeAnalysis.SuppressMessage(
            "Microsoft.Design",
            "CA1031:DoNotCatchGeneralExceptionTypes",
            Justification = "Setup should never crash on the user." +
            "By catching the general exception at this level, we keep that from happening.")]
        [SecurityCritical]
        [STAThread]
        public static int Main()
        {
            SetupHelper.UASetupReturnValues returnValue = SetupHelper.UASetupReturnValues.Failed;

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.ExitCode,
                (int)returnValue);

            // If user specified command line arguments then show console window otherwise hide it.
            if (Environment.GetCommandLineArgs().Length > 1)
            {
                ConsoleUtils.ShowConsoleWindow();
            }
            else
            {
                ConsoleUtils.HideConsoleWindow();
            }

            // This is needed to load the WPF stuff so
            // that we can get at the resources.
            Application apptry = Application.Current;
            Application.Equals(apptry, null);

            bool createdNew;
            mutex = new Mutex(true, "Microsoft Azure Site Recovery Unified Agent Configurator", out createdNew);
            if (!createdNew)
            {
                if (Environment.GetCommandLineArgs().Length > 1)
                {
                    ConsoleUtils.Log(LogLevel.Error, StringResources.ProcessAlreadyLaunchedUA);
                    return (int)SetupHelper.UASetupReturnValues.AnInstanceIsAlreadyRunning;
                }
                else
                {
                    SetupHelper.ShowErrorUA(StringResources.ProcessAlreadyLaunchedUA);
                    return (int)returnValue;
                }
            }

            SetDefaultPropertyValues();
            Trc.Initialize(
                (LogLevel.Always | LogLevel.Debug | LogLevel.Error | LogLevel.Warn | LogLevel.Info),
                PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.DefaultLogName));
            Trc.Log(LogLevel.Always, "Application Started");

            try
            {
                // Get the version of the current exe.
                Version currentBuildVersion = ValidationHelper.GetBuildVersion(
                    UnifiedSetupConstants.ConfiguratorExeName);

                SummaryFileWrapper.StartSummary(
                    UnifiedSetupConstants.UnifiedAgentSetupLogsComponentName,
                    currentBuildVersion,
                    InstallType.Configuration,
                    AdapterType.InMageAdapter);

                InitializeSetup();

                if (ParseCommandLine())
                {
                    string jsonFilename = string.Empty;
                    if (PropertyBagDictionary.Instance.PropertyExists(
                        PropertyBagConstants.ValidationsOutputJsonFilePath))
                    {
                        jsonFilename = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath);
                    }
                    if (!ErrorLogger.InitializeInstallerOutputJson(jsonFilename))
                    {
                        Trc.Log(LogLevel.Error, "Failed to Initialise the output json file");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Info, string.Format("Successfully initialised the output json file {0}", ErrorLogger.OutputJsonFileName));
                    }

                    if (IsFailedOverVm())
                    {
                        Trc.Log(LogLevel.Always, "This is a failed over vm in CSPrime stack. Configuration operation is a no op here.");
                        returnValue = SetupHelper.UASetupReturnValues.Successful;
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ExitCode,
                            (int)returnValue);
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.InstallationFinished,
                            true);
                    }
                    else if (Environment.GetCommandLineArgs().Length > 1 &&
                        PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.Unconfigure))
                    {
                        returnValue = UnconfigureAgentWithCSPrime();
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ExitCode,
                            (int)returnValue);
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.InstallationFinished,
                            true);
                    }
                    else if (Environment.GetCommandLineArgs().Length > 1 &&
                        PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.Operation))
                    {
                        returnValue = ConfigureOperation();
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ExitCode,
                            (int)returnValue);
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.InstallationFinished,
                            true);
                    }
                    else if (Environment.GetCommandLineArgs().Length > 1)
                    {
                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagDictionary.Silent, true);
                        returnValue = SilentConfiguration();
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ExitCode,
                            (int)returnValue);
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.InstallationFinished,
                            true);

                        if ((returnValue != SetupHelper.UASetupReturnValues.Successful) &&
                            (returnValue != SetupHelper.UASetupReturnValues.SuccessfulRecommendedReboot))
                        {
                            // Update configuration status as failed.	 
                            RegistrationOperations.UpdateConfigurationStatus(UnifiedSetupConstants.FailedStatus);
                        }
                    }
                    else
                    {
                        UiRun();
                    }
                }
                else
                {
                    returnValue = SetupHelper.UASetupReturnValues.InvalidCommandLine;
                    PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.ExitCode,
                        (int)returnValue);
                    PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.InstallationFinished,
                        true);
                }
            }
            catch(OutOfMemoryException)
            {
                returnValue = SetupHelper.UASetupReturnValues.OutOfMemoryException;
                return (int)returnValue;
            }
            catch (Exception ex)
            {
                Trc.LogErrorException("APPCRASH: ", ex);
                SetupHelper.ShowErrorUA(StringResources.CriticalExceptionOccurred);
            }
            finally
            {
                if (returnValue != SetupHelper.UASetupReturnValues.OutOfMemoryException)
                {
                    try
                    {
                        SummaryFileWrapper.EndSummary(
                            PropertyBagDictionary.Instance.GetProperty<int>(
                                PropertyBagConstants.ExitCode) == 0 ?
                                    OpResult.Success :
                                    OpResult.Failed,
                            PropertyBagDictionary.Instance.GetProperty<Guid>(
                                PropertyBagConstants.MachineIdentifier),
                            UnifiedSetupConstants.WindowsOs,
                            SetupHelper.GetOSName(),
                            SetupHelper.GetHostName(),
                            SetupHelper.GetAssetTag(),
                            PropertyBagDictionary.Instance.GetProperty<string>(
                                PropertyBagConstants.Platform),
                            SetupHelper.GetAgentInstalledRole().ToString(),
                            SetupHelper.GetMachineMemory(),
                            SetupHelper.GetAvailableMemory(),
                            PropertyBagDictionary.Instance.GetProperty<string>(
                                PropertyBagConstants.AlreadyInstalledVersion),
                            PropertyBagDictionary.Instance.GetProperty<bool>(
                                PropertyBagDictionary.Silent),
                            PropertyBagDictionary.Instance.GetProperty<int>(
                                PropertyBagConstants.ExitCode),
                            PropertyBagDictionary.Instance.GetProperty<string>(
                                PropertyBagConstants.SetupInvoker),
                            SetupHelper.GetBiosHardwareId(),
                            SetupHelper.FetchHostIDFromDrScout(),
                            PropertyBagDictionary.Instance.GetProperty<string>(
                                PropertyBagConstants.RUNID));

                        SummaryFileWrapper.DisposeSummary();

                        ErrorLogger.ConsolidateErrors();
                        //Appending output json file contents to installer logs
                        try
                        {
                            if (File.Exists(ErrorLogger.OutputJsonFileName))
                            {
                                string errorinfo = File.ReadAllText(ErrorLogger.OutputJsonFileName);
                                Trc.Log(LogLevel.Info, ErrorLogger.OutputJsonFileName + " contents start here-----------------------------------------------------------------");
                                Trc.Log(LogLevel.Info, errorinfo);
                                Trc.Log(LogLevel.Info, ErrorLogger.OutputJsonFileName + " contents end here--------------------------------------------------------------------");
                            }
                            else
                            {
                                Trc.Log(LogLevel.Info, "The file {0} does not exist", ErrorLogger.OutputJsonFileName);
                            }
                        }
                        catch (Exception ex)
                        {
                            Trc.Log(LogLevel.Warn, "Failed to read error json file : " + ErrorLogger.OutputJsonFileName
                                + " with following exception " + ex);
                        }

                        DeleteSetupCreatedFiles();
                        EndOfAppDataLogging(returnValue);
                        if (PropertyBagDictionary.Instance.PropertyExists(
                            PropertyBagConstants.CombinedLogFile))
                        {
                            SetupReportData.MergeFiles(
                                PropertyBagDictionary.Instance.GetProperty<string>(
                                    PropertyBagConstants.CombinedLogFile));
                        }

                        if (PropertyBagDictionary.Instance.PropertyExists(
                            PropertyBagConstants.CombinedSummaryFile))
                        {
                            try
                            {
                                File.Copy(
                                    PropertyBagDictionary.Instance.GetProperty<string>(
                                        PropertyBagConstants.SummaryFileName),
                                    PropertyBagDictionary.Instance.GetProperty<string>(
                                        PropertyBagConstants.CombinedSummaryFile),
                                    true);

                                SetupHelper.MoveAgentSetupLogsToUploadedFolder();
                            }
                            catch (Exception ex)
                            {
                                Trc.LogErrorException(
                                    "Exception while copying Json file: {0}",
                                    ex);
                            }
                        }

                        if (!SetupHelper.IsCXInstalled() &&
                            PropertyBagDictionary.Instance.GetProperty<Platform>(
                                PropertyBagConstants.Platform) == Platform.VmWare &&
                            PropertyBagDictionary.Instance.GetProperty<SetupInvoker>(
                                PropertyBagConstants.SetupInvoker) == SetupInvoker.commandline)
                        {
                            SetupHelper.TransferAgentSetupLogsToCSPSSetup();
                        }
                    }
                    catch (Exception ex)
                    {
                        Trc.Log(LogLevel.Error,
                            "Failed to complete configuration with exception {0}",
                            ex);
                    }
                }

                mutex.ReleaseMutex();
            }

            return (int)returnValue;
        }

        /// <summary>
        /// Set default property bag properties
        /// </summary>
        private static void SetDefaultPropertyValues()
        {
            // Setup start time
            PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.SetupStartTime, DateTime.Now);

            // Default log file where all Microsoft Azure Site Recovery Provider logging will take place
            string logFilePath = SetupHelper.SetLogFilePath(
                UnifiedSetupConstants.UnifiedAgentConfiguratorLogFileName);
            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.DefaultLogName,
                logFilePath);

            // Add the file to the list of files to collect the data from.
            SetupReportData.AddLogFilePath(logFilePath);

            // Add the current path to the propertybag
            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagDictionary.SetupExePath,
                (new System.IO.FileInfo(
                    System.Reflection.Assembly.GetExecutingAssembly().Location)).DirectoryName);

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.WizardTitle,
                StringResources.AgentMessageBoxTitle);

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.CanClose,
                false);

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.RUNID,
                Guid.NewGuid().ToString());

            if (Environment.GetCommandLineArgs().Length > 1)
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.IsManualInstall,
                    false);
            }
            else
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.IsManualInstall,
                    true);
            }
        }

        /// <summary>
        /// Pre-init steps.
        /// </summary>
        private static void InitializeSetup()
        {
            string machineId = SetupHelper.GetMachineIdentifier();
            Guid machineIdentifier = Guid.Empty;
            if (!string.IsNullOrEmpty(machineId))
            {
                machineIdentifier = new Guid(machineId);
            }
            
            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.MachineIdentifier,
                machineIdentifier);

            Platform platform;
            string vmPlatform = SetupHelper.GetAgentInstalledPlatformFromDrScout();
            if(string.IsNullOrEmpty(vmPlatform.ToString()))
            {
                Trc.Log(
                    LogLevel.Always, 
                    "Performing VM Platform validation as platform value is null in drscout config.");
                platform = SetupHelper.IsAzureVirtualMachine() ? Platform.Azure : Platform.VmWare;
            }
            else
            {
                platform = vmPlatform.AsEnum<Platform>().Value;
            }

            Trc.Log(LogLevel.Always, "Platform is set to : " + platform);
            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.Platform,
                platform);

            string hostId;
            bool isCXInstalled = SetupHelper.IsCXInstalled();

            /* Get the host guid from amethyst config 
               if Configuration server is installed,
               else get it from Drscout config for agent upgrade 
               or generate a new Guid for fresh installation. */
            if (isCXInstalled)
            {
                hostId = SetupHelper.GetHostGuidFromAmethystConfig();
                Trc.Log(LogLevel.Always, "hostId from amethyst config: {0}", hostId);
            }
            else
            {
                ConfigureActionProcessor.GetHostId(out hostId);
            }

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.HostId,
                hostId);

            SetupHelper.RetrieveExistingProxySettings();
        }

        /// <summary>
        /// Checks if the current machine is a failover vm
        /// </summary>
        /// <returns>True if the current machine is failover vm</returns>
        private static bool IsFailedOverVm()
        {
            Platform platform = PropertyBagDictionary.Instance.GetProperty<Platform>(
                PropertyBagConstants.Platform);
            ConfigurationServerType csType = SetupHelper.GetCSTypeEnumFromDrScout();
            bool IsAzureVirtualMachine = SetupHelper.IsAzureVirtualMachine();

            if (platform == Platform.VmWare &&
                csType == ConfigurationServerType.CSPrime &&
                IsAzureVirtualMachine)
            {
                return true;
            }
            return false;
        }

        /// <summary>
        /// Parses command line and fills appropriate values.
        /// </summary>
        /// <returns>True is parsing succeeded, false otherwise.</returns>
        private static bool ParseCommandLine()
        {
            string[] arguments = Environment.GetCommandLineArgs();
            CommandlineParameters commandlineParameters = new CommandlineParameters();

            // Parse command line arguments
            if (!commandlineParameters.ParseCommandline(arguments))
            {
                return false;
            }

            // Store data in dictionary. We are not doing any validations
            // at this point to avoid UI lag.

            // VmWare params.
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.CSEndPoint))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CSEndpoint,
                    commandlineParameters.GetParameterValue(CommandlineParameterId.CSEndPoint).ToString());
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.CSPort))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CSPort,
                    commandlineParameters.GetParameterValue(CommandlineParameterId.CSPort).ToString());
            }
            else
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CSPort,
                    UnifiedSetupConstants.CSPort);
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.PassphraseFilePath))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.PassphraseFilePath,
                    commandlineParameters.GetParameterValue(CommandlineParameterId.PassphraseFilePath).ToString());
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.SourceConfigFilePath))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.SourceConfigFilePath,
                    commandlineParameters.GetParameterValue(CommandlineParameterId.SourceConfigFilePath).ToString());
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.CredentialLessDiscovery))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CredLessDiscovery,
                    commandlineParameters.GetParameterValue(CommandlineParameterId.CredentialLessDiscovery).ToString());
            }
            else
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CredLessDiscovery,
                    "false");
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.Unconfigure))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.Unconfigure,
                    commandlineParameters.GetParameterValue(CommandlineParameterId.Unconfigure).ToString());
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.Operation))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.Operation,
                    commandlineParameters.GetParameterValue(CommandlineParameterId.Operation).ToString());
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.LogFilePath))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CombinedLogFile,
                    commandlineParameters.GetParameterValue(CommandlineParameterId.LogFilePath).ToString());
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.SummaryFilePath))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CombinedSummaryFile,
                    commandlineParameters.GetParameterValue(
                        CommandlineParameterId.SummaryFilePath).ToString());
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.ValidationsOutputJsonFilePath))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ValidationsOutputJsonFilePath,
                    commandlineParameters.GetParameterValue(
                        CommandlineParameterId.ValidationsOutputJsonFilePath).ToString());
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.ExternalIP))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ExternalIP,
                    commandlineParameters.GetParameterValue(CommandlineParameterId.ExternalIP).ToString());
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.CSType))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CSType,
                    commandlineParameters.GetParameterValue(CommandlineParameterId.CSType).ToString());
            }
            else
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CSType,
                    ConfigurationServerType.CSLegacy.ToString());
            }

            if(commandlineParameters.IsParameterSpecified(CommandlineParameterId.ClientRequestId))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    key: PropertyBagConstants.ClientRequestId,
                    value: commandlineParameters.GetParameterValue(CommandlineParameterId.ClientRequestId).ToString());
            }

            // Azure params.
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.Credentials))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.RcmCredsPath,
                    commandlineParameters.GetParameterValue(CommandlineParameterId.Credentials).ToString());
            }

            // Agent setup invoker.
            SetupInvoker invoker = SetupInvoker.commandline;
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.Invoker))
            {
                if (!commandlineParameters.GetParameterValue(CommandlineParameterId.Invoker)
                    .ToString()
                    .TryAsEnum<SetupInvoker>(out invoker))
                {
                    Trc.Log(LogLevel.Always, "Parameter passed for invoker is invalid.");
                }
            }

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.SetupInvoker,
                invoker);

            // Proxy params.
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.ConfigureProxy))
            {
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagDictionary.Silent, true);
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ConfigureProxy, true);
            }
            else
            {
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ConfigureProxy, false);
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.ProxyAddress))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ProxyAddress,
                    commandlineParameters.GetParameterValue(CommandlineParameterId.ProxyAddress).ToString());
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.ProxyPort))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ProxyPort,
                    commandlineParameters.GetParameterValue(CommandlineParameterId.ProxyPort).ToString());
            }

            return true;
        }

        #region Silent Configuration

        /// <summary>
        /// Runs silent configuration.
        /// </summary>
        /// <returns>Exit code to be returned by configurator.</returns>
        private static SetupHelper.UASetupReturnValues SilentConfiguration()
        {
            ConsoleUtils.Log(LogLevel.Always, "Starting silent configuration.");

            Platform platform = PropertyBagDictionary.Instance.GetProperty<Platform>(
                PropertyBagConstants.Platform);

            ConsoleUtils.Log(LogLevel.Always, "Running configuration for " + platform.ToString());

            if (PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.ConfigureProxy))
            {
                return ConfigureProxy();
            }

            // A. Check for platform
            if ((platform == Platform.VmWare) && ValidationHelper.IsCsLegacy())
            {
                return SilentConfigurationForCS();
            }
            else if ((platform == Platform.VmWare) && ValidationHelper.IsCsPrime())
            {
                return SilentConfigurationForCSPrime();
            }
            else if (platform == Platform.Azure)
            {
                return SilentConfigurationForRCM();
            }

            return SetupHelper.UASetupReturnValues.Successful;
        }

        /// <summary>
        /// Configures proxy settings.
        /// </summary>
        /// <returns>Process exit code.</returns>
        private static SetupHelper.UASetupReturnValues ConfigureProxy()
        {
            ConsoleUtils.Log(LogLevel.Always, "Configuring proxy settings");
            OperationResult opResult = RegistrationOperations.ConfigureProxySettings();

            if (opResult.Status == OperationStatus.Failed)
            {
                SetupHelper.ShowErrorUA(opResult.Error);
            }

            return opResult.ProcessExitCode;
        }

        /// <summary>
        /// Performs unconfigure
        /// </summary>
        /// <returns>Return value from process.</returns>
        private static SetupHelper.UASetupReturnValues UnconfigureAgentWithCSPrime()
        {
            try
            {
                string installationDir = SetupHelper.GetAgentInstalledLocation();
                IniFile drScoutInfo = new IniFile(Path.Combine(
                    installationDir,
                    UnifiedSetupConstants.DrScoutConfigRelativePath));

                AgentInstallRole role = SetupHelper.GetAgentInstalledRole();
                if (role == AgentInstallRole.MS &&
                    String.Equals(drScoutInfo.ReadValue("vxagent", PropertyBagConstants.CSType),
                    ConfigurationServerType.CSPrime.ToString(),
                    StringComparison.OrdinalIgnoreCase))
                {
                    // Shutdown agent services.
                    if (!SetupHelper.ShutdownInMageAgentServices())
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.Configuration,
                            OpName.StopServices,
                            OpResult.Failed,
                            (int)SetupHelper.UASetupReturnValues.UnableToStopServices,
                            StringResources.FailedToStopServices);
                        return SetupHelper.UASetupReturnValues.UnconfigureAgentFailure;
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.Configuration,
                            OpName.StopServices,
                            OpResult.Success);
                    }

                    if (!RegistrationOperations.UnconfigureAgentCSPrime())
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.Configuration,
                            OpName.UnconfigureAgentWithCSPrime,
                            OpResult.Failed);
                        return SetupHelper.UASetupReturnValues.UnconfigureAgentFailure;
                    }

                    if (!Directory.Exists(installationDir))
                    {
                        Trc.Log(LogLevel.Error, "{0} directory doesn't exists.", installationDir);
                        return SetupHelper.UASetupReturnValues.UnconfigureAgentFailure;
                    }

                    string sourceConfigFilePath = Path.Combine(installationDir,
                        UnifiedSetupConstants.SourceConfigRelativePath);
                    string sourceConfigFilePathLock = sourceConfigFilePath + ".lck";

                    Trc.Log(LogLevel.Always, "Source config file path : {0}, Lock path : {1}",
                        sourceConfigFilePath, sourceConfigFilePathLock);
                    if (File.Exists(sourceConfigFilePath))
                    {
                        Trc.Log(LogLevel.Always, "Deleting file : {0}", sourceConfigFilePath);
                        File.Delete(sourceConfigFilePath);
                    }
                    if (File.Exists(sourceConfigFilePathLock))
                    {
                        Trc.Log(LogLevel.Always, "Deleting file : {0}", sourceConfigFilePathLock);
                        File.Delete(sourceConfigFilePathLock);
                    }

                    // Updating ResourceId.
                    drScoutInfo.WriteValue(
                        "vxagent",
                        "ResourceId",
                        "");
                    RegistrationOperations.UpdateDrScoutForAgentUnconfigure();

                    RegistrationOperations.UpdateRegistryForAgentUnconfigure();
                }

                SummaryFileWrapper.RecordOperation(
                    Scenario.Configuration,
                    OpName.UnconfigureAgentWithCSPrime,
                    OpResult.Success);

                return SetupHelper.UASetupReturnValues.Successful;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "CRITICAL: Hit exception during unconfigure agent {0}",
                    ex);
                return SetupHelper.UASetupReturnValues.UnconfigureAgentFailure;
            }
        }

        /// <summary>
        /// Performs configuration operation
        /// </summary>
        /// <returns>Return value from process.</returns>
        private static SetupHelper.UASetupReturnValues ConfigureOperation()
        {
            try
            {

                string operation = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.Operation);
                if (!operation.Equals(PropertyBagConstants.DiagnoseAndFix, StringComparison.OrdinalIgnoreCase))
                {
                    ConsoleUtils.Log(LogLevel.Error, "Invalid operation : "+ operation + " passed.");
                    return SetupHelper.UASetupReturnValues.DiagnoseAndFixFailure;
                }

                ConsoleUtils.Log(LogLevel.Info, "Diagnose and fix configuration.");
                string installationDir = SetupHelper.GetAgentInstalledLocation();
                IniFile drScoutInfo = new IniFile(Path.Combine(
                    installationDir,
                    UnifiedSetupConstants.DrScoutConfigRelativePath));

                // Shutdown agent services.
                if (!SetupHelper.ShutdownInMageAgentServices())
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Configuration,
                        OpName.StopServices,
                        OpResult.Failed,
                        (int)SetupHelper.UASetupReturnValues.UnableToStopServices,
                        StringResources.FailedToStopServices);

                    ConsoleUtils.Log(LogLevel.Error, "Failed to stop agent services.");
                    return SetupHelper.UASetupReturnValues.DiagnoseAndFixFailure;
                }
                else
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Configuration,
                        OpName.StopServices,
                        OpResult.Success);
                    ConsoleUtils.Log(LogLevel.Info, "Agent services stopped succesfully.");
                }

                if (!RegistrationOperations.DiagnoseAndFix())
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Configuration,
                        OpName.DiagnoseAndFix,
                        OpResult.Failed);

                    ConsoleUtils.Log(LogLevel.Error, "AzureRcmCli Diagnose and fix operation has failed.");
                }
                if (String.Equals(drScoutInfo.ReadValue("vxagent", PropertyBagConstants.CSType),
                    ConfigurationServerType.CSLegacy.ToString(),
                    StringComparison.OrdinalIgnoreCase))
                {
                    //Register agent for CSLegacy
                    if (!SetupHelper.RegisterAgent())
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.Configuration,
                            OpName.RegisterAgent,
                            OpResult.Failed);
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.Configuration,
                            OpName.RegisterAgent,
                            OpResult.Success);
                    }
                }

                // Start services.
                if (!SetupHelper.StartUAServices(false))
                {
                    SummaryFileWrapper.RecordOperation(
                            Scenario.Configuration,
                            OpName.BootServices,
                            OpResult.Failed);
                    ConsoleUtils.Log(LogLevel.Error, "Failed to start agent services.");
                    return SetupHelper.UASetupReturnValues.DiagnoseAndFixFailure;
                }
                else
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Installation,
                        OpName.BootServices,
                        OpResult.Success);
                    ConsoleUtils.Log(LogLevel.Info, "Agent services started succesfully.");
                }

                ConsoleUtils.Log(LogLevel.Info, "Diagnose and fix configuration succeeded.");
                return SetupHelper.UASetupReturnValues.Successful;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "CRITICAL: Hit exception during configuration operation {0}",
                    ex);
                return SetupHelper.UASetupReturnValues.DiagnoseAndFixFailure;
            }
        }

        /// <summary>
        /// Performs silent configuration for CS.
        /// </summary>
        /// <returns>Return value from process.</returns>
        private static SetupHelper.UASetupReturnValues SilentConfigurationForCS()
        {
            string csEndpoint;
            string passphraseFilePath;
            OperationResult opResult;

            ConsoleUtils.Log(
                LogLevel.Always,
                "Starting registration for VmWare platform");

            // Verify user provided CSEndpoint
            if (!PropertyBagDictionary.Instance.ContainsKey(PropertyBagConstants.CSEndpoint))
            {
                SetupHelper.ShowErrorUA("Configuration Server endpoint is missing!");
                return SetupHelper.UASetupReturnValues.InvalidPassphraseOrCSEndPoint;
            }
            else
            {
                csEndpoint = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.CSEndpoint);
            }

            ConsoleUtils.Log(
                LogLevel.Always,
                "CS endpoint for registration - " + csEndpoint);

            // Verify passphrase file.
            if (!PropertyBagDictionary.Instance.ContainsKey(PropertyBagConstants.PassphraseFilePath))
            {
                SetupHelper.ShowErrorUA("Passphrase file path is missing");
                return SetupHelper.UASetupReturnValues.InvalidPassphraseOrCSEndPoint;
            }
            else
            {
                passphraseFilePath = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.PassphraseFilePath);
            }

            ConsoleUtils.Log(
                LogLevel.Always,
                "Passphrase file for registration - " + passphraseFilePath);

            // Prepare for registration.
            ConsoleUtils.Log(LogLevel.Info, "Preparing for registration");
            opResult = RegistrationOperations.PrepareForCSRegistration(
                csEndpoint,
                passphraseFilePath);

            if (opResult.Status == OperationStatus.Failed)
            {
                SetupHelper.ShowErrorUA(opResult.Error);
                return opResult.ProcessExitCode;
            }

            // Configure Proxy Settings
            ConsoleUtils.Log(LogLevel.Info, "Configuring connection settings.");
            opResult = RegistrationOperations.ConfigureProxySettings();
            if (opResult.Status == OperationStatus.Failed)
            {
                SetupHelper.ShowErrorUA(opResult.Error);
                return opResult.ProcessExitCode;
            }

            // Register with CS.
            ConsoleUtils.Log(LogLevel.Info, "Registering with CS.");
            opResult = RegistrationOperations.RegisterWithCS();
            if (opResult.Status == OperationStatus.Failed)
            {
                SetupHelper.ShowErrorUA(opResult.Error);
                return opResult.ProcessExitCode;
            }

            // Check status.
            ConsoleUtils.Log(LogLevel.Info, "Validating configuration.");
            opResult = RegistrationOperations.ValidateCSRegistration();
            if (opResult.Status == OperationStatus.Warning || 
                opResult.Status == OperationStatus.Failed)
            {
                SetupHelper.ShowErrorUA(opResult.Error);
                return opResult.ProcessExitCode;
            }

            // Update configuration status as success.
            RegistrationOperations.UpdateConfigurationStatus(UnifiedSetupConstants.SuccessStatus);

            return SetupHelper.UASetupReturnValues.Successful;
        }

        /// <summary>
        /// Performs silent configuration for CSPrime.
        /// </summary>
        /// <returns>Return value from process.</returns>
        private static SetupHelper.UASetupReturnValues SilentConfigurationForCSPrime()
        {
            string sourceConfigFilePath;
            OperationResult opResult;

            ConsoleUtils.Log(
                LogLevel.Always,
                "Starting registration for CSPrime");

            // Verify source config file
            if (!PropertyBagDictionary.Instance.ContainsKey(PropertyBagConstants.SourceConfigFilePath))
            {
                SetupHelper.ShowErrorUA("Source config file path is missing!");
                return SetupHelper.UASetupReturnValues.InvalidSourceConfigFile;
            }
            else
            {
                sourceConfigFilePath = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.SourceConfigFilePath);
            }

            ConsoleUtils.Log(
                LogLevel.Always,
                "Source config file for registration - " + sourceConfigFilePath);

            // Prepare for registration.
            ConsoleUtils.Log(LogLevel.Info, "Preparing for registration");
            opResult = RegistrationOperations.PrepareForCSPrimeRegistration(
                sourceConfigFilePath);

            if (opResult.Status == OperationStatus.Failed)
            {
                SetupHelper.ShowErrorUA(opResult.Error);
                return opResult.ProcessExitCode;
            }

            // Configure Proxy Settings
            ConsoleUtils.Log(LogLevel.Info, "Configuring connection settings.");
            opResult = RegistrationOperations.ConfigureProxySettings();
            if (opResult.Status == OperationStatus.Failed)
            {
                SetupHelper.ShowErrorUA(opResult.Error);
                return opResult.ProcessExitCode;
            }

            // Register with CS.
            ConsoleUtils.Log(LogLevel.Info, "Registering with CS.");
            opResult = RegistrationOperations.RegisterWithCS();
            if (opResult.Status == OperationStatus.Failed)
            {
                SetupHelper.ShowErrorUA(opResult.Error);
                return opResult.ProcessExitCode;
            }

            // Check status.
            ConsoleUtils.Log(LogLevel.Info, "Validating configuration.");
            opResult = RegistrationOperations.ValidateCSRegistration();
            if (opResult.Status == OperationStatus.Warning ||
                opResult.Status == OperationStatus.Failed)
            {
                SetupHelper.ShowErrorUA(opResult.Error);
                return opResult.ProcessExitCode;
            }

            // Update configuration status as success.
            RegistrationOperations.UpdateConfigurationStatus(UnifiedSetupConstants.SuccessStatus);

            return SetupHelper.UASetupReturnValues.Successful;
        }

        /// <summary>
        /// Performs silent configuration for RCM.
        /// </summary>
        /// <returns>Returns value from process.</returns>
        private static SetupHelper.UASetupReturnValues SilentConfigurationForRCM()
        {
            ConsoleUtils.Log(
                LogLevel.Always,
                "Starting registration for Azure platform");

            if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.CSEndpoint) ||
                (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.PassphraseFilePath)))
            {
                SetupHelper.ShowErrorUA(StringResources.ConfigInValidParamInA2AMode);
                return SetupHelper.UASetupReturnValues.InvalidCommandLineArguments;
            }

            if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.RcmCredsPath))
            {
                string rcmCredsPath = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.RcmCredsPath);

                ConsoleUtils.Log(LogLevel.Always, "Using RCM Credentials as " + rcmCredsPath);

                // B. Prepare for registration.
                ConsoleUtils.Log(LogLevel.Info, "Prepairing to register with RCM.");
                OperationResult opResult = RegistrationOperations.PrepareForRCMRegistration(
                    rcmCredsPath);
                if (opResult.Status == OperationStatus.Failed)
                {
                    SetupHelper.ShowErrorUA(opResult.Error);
                    return opResult.ProcessExitCode;
                }

                // C. Configure Proxy Settings
                ConsoleUtils.Log(LogLevel.Info, "Configuring connection settings.");
                opResult = RegistrationOperations.ConfigureProxySettings();
                if (opResult.Status == OperationStatus.Failed)
                {
                    SetupHelper.ShowErrorUA(opResult.Error);
                    return opResult.ProcessExitCode;
                }

                // D. Register with RCM.
                ConsoleUtils.Log(LogLevel.Info, "Registering with RCM.");
                opResult = RegistrationOperations.RegisterWithRCM();
                if (opResult.Status == OperationStatus.Failed)
                {
                    SetupHelper.ShowErrorUA(opResult.Error);
                    return opResult.ProcessExitCode;
                }

                ConsoleUtils.Log(LogLevel.Info, "Registered with RCM.");
                
                // Update configuration status as success.
                RegistrationOperations.UpdateConfigurationStatus(UnifiedSetupConstants.SuccessStatus);
                return SetupHelper.UASetupReturnValues.Successful;
            }
            else
            {
                SetupHelper.ShowErrorUA("Credentials file missing.");
                return SetupHelper.UASetupReturnValues.InvalidCommandLineArguments;
            }
        }
        #endregion

        #region UI Run routines
        /// <summary>
        /// Run Setup in UI mode.
        /// </summary>
        private static void UiRun()
        {
            App app = new App();
            app.Startup += new StartupEventHandler(App_Startup);
            app.Run();
        }

        /// <summary>
        /// UI app startup.
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Startupevent Args.</param>
        private static void App_Startup(object sender, StartupEventArgs e)
        {
            // Load and initializes all pages 
            IPageHost host = LoadUiPages();

            // Jump off the landing page to the correct page track.
            PageNavigation.Instance.MoveToNextPage();

            // Launch the UI 
            host.Closed += new EventHandler(Host_Closed);
            host.Show();
        }

        /// <summary>
        /// Loads and initializes all UI pages.
        /// </summary>
        /// <returns>the newly created IPageHost interface object</returns>
        private static IPageHost LoadUiPages()
        {
            // Creates a page factory, a host page and set up page navigation 
            Factory factory = new WpfFactory();
            IPageHost host = factory.CreateHost();
            PageNavigation.Instance.Host = host;
            Dictionary<string, SidebarNavigation> setupStages =
                new Dictionary<string, SidebarNavigation>();

            // Add wait screen here if needed.
            XmlSerializer serializer = new XmlSerializer(typeof(Pages));

            // Parse Pages.xml and get pages.
            StringReader pagesXml = new StringReader(Properties.Resources.Pages);
            XmlReader pageReader = XmlReader.Create(pagesXml);
            Pages inputPages = (Pages)serializer.Deserialize(pageReader);

            foreach (PagesPage page in inputPages.Page)
            {
                Trc.Log(LogLevel.Debug, "Adding Page {0}", page.Id);
                if (string.IsNullOrEmpty(page.SetupStage.Value))
                {
                    Trc.Log(LogLevel.Debug, "Skipping stage addition. Stage Null", page.Id);
                }
                else
                {
                    if (setupStages.ContainsKey(page.SetupStage.Value))
                    {
                        Trc.Log(LogLevel.Debug, "Skipping stage addition. Stage already present", page.Id);
                    }
                    else
                    {
                        Trc.Log(LogLevel.Debug, "Adding {0} key to dictionary", page.SetupStage.Value);
                        setupStages.Add(
                            page.SetupStage.Value,
                            new SidebarNavigation(page.SetupStage.Value, page.Id));
                    }
                }

                Page workingPage = new Page(factory, host, page);

                // Add this page to our pages
                PageRegistry.Instance.RegisterPage(workingPage);
            }

            host.SetSetupStages(setupStages);

            return host;
        }

        /// <summary>
        /// Handles the Closed event of the Host control.
        /// </summary>
        /// <param name="sender">The source of the event.</param>
        /// <param name="e">The <see cref="System.EventArgs"/> instance containing the event data.</param>
        private static void Host_Closed(object sender, EventArgs e)
        {
        }
        #endregion

        #region Application Exit routines
        /// <summary>
        /// Files to be removed during crash.
        /// </summary>
        private static void DeleteSetupCreatedFiles()
        {
            try
            {
                string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);

                // Remove setup created files
                ValidationHelper.DeleteFile(Path.Combine(sysDrive, @"Temp\ASRSetup\Passphrase.txt"));
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
            }
        }

        /// <summary>
        /// Logs everything at the end of the application 
        /// </summary>
        /// <param name="returnValue">The application's return value</param>
        private static void EndOfAppDataLogging(SetupHelper.UASetupReturnValues returnValue)
        {
            // Logs the property bag
            Trc.Log(LogLevel.Always, "Application Ended");
            
            // Log return val only if Silent run was performed.
            if (Environment.GetCommandLineArgs().Length > 1)
            {
                Trc.Log(
                    LogLevel.Always,
                    "Exit code returned by unified agent configurator: {0}({1})",
                    (int)returnValue,
                    returnValue.ToString());
            }

            SetupHelper.LogPropertyBag();
        }
        #endregion
    }
}
