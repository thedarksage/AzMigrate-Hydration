//---------------------------------------------------------------
//  <copyright file="Program.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Application entry point and silent installation logic.
//  </summary>
//
//  History:     02-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Security;
using System.Threading;
using System.Windows;
using System.Xml;
using System.Xml.Serialization;
using ASRResources;
using ASRSetupFramework;
using System.Windows.Threading;

namespace UnifiedAgentInstaller
{
    /// <summary>
    /// This is the main class that control the Microsoft Azure Site Recovery Provider setup.exe
    /// </summary>
    public static class Program
    {
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
            mutex = new Mutex(true, "Microsoft Azure Site Recovery Unified Agent", out createdNew);
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
                // Generate machine identifier.
                SetupHelper.GenerateMachineIdentifier();

                // Get the version of the current installer.
                Version currentBuildVersion = ValidationHelper.GetBuildVersion(
                    UnifiedSetupConstants.UnifiedAgentInstallerExeName);

                if (ParseCommandLine())
                {
                    string jsonFilename = string.Empty;
                    if(PropertyBagDictionary.Instance.PropertyExists(
                        PropertyBagConstants.ValidationsOutputJsonFilePath))
                    {
                        jsonFilename = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath);
                    }
                    if(!ErrorLogger.InitializeInstallerOutputJson(jsonFilename, true))
                    {
                        Trc.Log(LogLevel.Error,"Failed to Initialise the output json file");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Info,string.Format("Successfully initialised the output json file {0}",ErrorLogger.OutputJsonFileName));
                    }

                    SetupAction setupAction;

                    if(!PrechecksSetupHelper.PreInstallationPrechecks())
                    {
                        Trc.Log(LogLevel.Error, "PreInstallation prechecks failed");
                        setupAction = InstallActionProcessor.GetSetupAction();
                        PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.SetupAction, setupAction);
                        SummaryFileWrapper.StartSummary(
                        UnifiedSetupConstants.UnifiedAgentSetupLogsComponentName,
                        currentBuildVersion,
                        (setupAction == SetupAction.Upgrade) ?
                            InstallType.Upgrade :
                            InstallType.New,
                        AdapterType.InMageAdapter);
                        returnValue = SetupHelper.UASetupReturnValues.FailedWithErrors;
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ExitCode, (int)returnValue);
                        PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.InstallationFinished,
                        true);
                        throw new Exception("PreInstallation prechecks failed. Please check the file " + ErrorLogger.OutputJsonFileName + " for details");
                    }

                    // Figure out the Setup action.
                    setupAction = InstallActionProcessor.GetSetupAction();
                    PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.SetupAction, setupAction);

                    SummaryFileWrapper.StartSummary(
                        UnifiedSetupConstants.UnifiedAgentSetupLogsComponentName,
                        currentBuildVersion,
                        (setupAction == SetupAction.Upgrade) ?
                            InstallType.Upgrade :
                            InstallType.New,
                        AdapterType.InMageAdapter);

                    // Do these checks only for command line installation
                    if(!PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.IsManualInstall) && 
                        PropertyBagDictionary.Instance.GetProperty<AgentInstallRole>(PropertyBagConstants.InstallRole) == AgentInstallRole.MS)
                    {
                        SetupHelper.UASetupReturnValues returnvalue;
                        string errorString;
                        if(!InstallActionProcessor.InstallationTypeCheck(out returnvalue, out errorString))
                        {
                            returnValue = returnvalue;
                            Trc.Log(LogLevel.Always, "The exit code for installation type precheck is {0}", returnValue);
                            PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ExitCode, (int)returnValue);
                            PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.InstallationFinished,
                            true);
                            throw new Exception(errorString);
                        }
                    }
                    

                    if (PropertyBagDictionary.Instance.GetProperty<bool>(
                        PropertyBagDictionary.Silent))
                    {
                        returnValue = SilentInstallation();
                        Trc.Log(LogLevel.Always, "returnValue - {0}", returnValue);
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ExitCode,
                            (int)returnValue);
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.InstallationFinished,
                            true);
                    }
                    else if ((Environment.GetCommandLineArgs().Length == 3) &&
                        (PropertyBagDictionary.Instance.GetProperty<ConfigurationServerType>(PropertyBagConstants.CSType) == ConfigurationServerType.CSPrime))
                    {
                        UiRun();
                    }
                    else
                    {
                        UiRun();
                    }

                    //if installation succeeded with warnings then check if reboot required
                    //this is done in main to avoid sending the same warning more than once.
                    //Do not comapre with returnvalue as UI does not set the return value.
                    if ((int)SetupHelper.UASetupReturnValues.SucceededWithWarnings == PropertyBagDictionary.Instance.GetProperty<int>(PropertyBagConstants.ExitCode))
                    {
                        if(PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagDictionary.RebootRequired))
                        {
                            if (PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.MandatoryRebootRequired))
                            {
                                Trc.Log(LogLevel.Debug,"Mandatory reboot required. Please restart your machine");
                                ErrorLogger.LogInstallerError(
                                new InstallerException(
                                    UASetupErrorCode.ASRMobilityServiceInstallerSuccessfulMandatoryReboot.ToString(),
                                    null,
                                    UASetupErrorCodeMessage.SuccessfulMandatoryReboot
                                    )
                                );
                            }
                            else
                            {
                                Trc.Log(LogLevel.Debug, "Recommended reboot required. Please restart your machine");
                                ErrorLogger.LogInstallerError(
                                new InstallerException(
                                    UASetupErrorCode.ASRMobilityServiceInstallerSuccessfulRecommendedReboot.ToString(),
                                    null,
                                    UASetupErrorCodeMessage.SuccessfulRecommendedReboot
                                    )
                                );
                            }
                            
                        }
                    }

                }
                else
                {
                    // Figure out the Setup action.
                    SetupAction setupAction = InstallActionProcessor.GetSetupAction();
                    PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.SetupAction, setupAction);

                    SummaryFileWrapper.StartSummary(
                        UnifiedSetupConstants.UnifiedAgentSetupLogsComponentName,
                        currentBuildVersion,
                        (setupAction == SetupAction.Upgrade) ?
                            InstallType.Upgrade :
                            InstallType.New,
                        AdapterType.InMageAdapter);
                    
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
                        Trc.Log(LogLevel.Always, "Finally block.");

                        SummaryFileWrapper.EndSummary(
                            PropertyBagDictionary.Instance.GetProperty<int>(
                                PropertyBagConstants.ExitCode) == 0 ?
                                    OpResult.Success :
                                    OpResult.Failed,
                            PropertyBagDictionary.Instance.GetProperty<Guid>(
                                PropertyBagConstants.MachineIdentifier),
                            UnifiedSetupConstants.WindowsOs,
                            PropertyBagDictionary.Instance.GetProperty<string>(
                            PropertyBagConstants.osName),
                            SetupHelper.GetHostName(),
                            SetupHelper.GetAssetTag(),
                            PropertyBagDictionary.Instance.GetProperty<string>(
                                PropertyBagConstants.Platform),
                            PropertyBagDictionary.Instance.GetProperty<string>(
                                PropertyBagConstants.InstallRole),
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
                            PropertyBagDictionary.Instance.GetProperty<string>(
                            PropertyBagConstants.BiosId),
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
                            "Failed to complete installation with exception {0}",
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
                UnifiedSetupConstants.UnifiedAgentInstallerLogFileName);
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
                PropertyBagConstants.InstallationFinished,
                false);

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagDictionary.RebootRequired, 
                false);

            // Set default installation directory.
            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.InstallationLocation,
                Path.Combine(
                    Path.Combine(
                        Path.GetPathRoot(Environment.SystemDirectory),
                        UnifiedSetupConstants.AgentDefaultInstallDir),
                        UnifiedSetupConstants.UnifiedAgentDirectorySuffix));

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.RUNID,
                Guid.NewGuid().ToString());

            //Set default result of prechecks as success
            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.PrechecksResult,
                UnifiedSetupConstants.InstallerPrechecksResult.Success);

            //Set default installation status as success.
            //This property is checked at the last to determine if installation succeeded with warnings.
            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.InstallationStatus,
                UnifiedSetupConstants.InstallationStatus.Success);

            //Set Mandatory Reboot Flaf as false
            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.MandatoryRebootRequired,
                false);

            if(Environment.GetCommandLineArgs().Length > 1)
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
        /// Parses command line and fills appropriate values.
        /// </summary>
        /// <returns>True if parsing succeeded, false otherwise.</returns>
        private static bool ParseCommandLine()
        {
            // Get command line arguments and decide if setup is to be Unattended
            string[] arguments = Environment.GetCommandLineArgs();
            CommandlineParameters commandlineParameters = new CommandlineParameters();

            // Parse command line arguments
            if (!commandlineParameters.ParseCommandline(arguments))
            {
                return false;
            }

            // Parse platform param.
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.Platform))
            {
                Platform platform = Platform.VmWare;
                bool isAzureStackHub = false;
                if (!commandlineParameters.GetParameterValue(CommandlineParameterId.Platform)
                    .ToString()
                    .TryAsEnum<Platform>(out platform))
                {
                    PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.ErrorMessage,
                        string.Format(
                            StringResources.InvalidValueForCommandLineParam,
                            CommandlineParameterId.Platform.ToString()));

                    return false;
                }

                if (platform == Platform.AzureStackHub)
                {
                    platform = Platform.VmWare;
                    isAzureStackHub = true;

                }

                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.Platform,
                    platform);
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.IsAzureStackHubVm,
                    isAzureStackHub);
        }

            // Parse Role param.
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.Role))
            {
                AgentInstallRole role;
                if (!commandlineParameters.GetParameterValue(CommandlineParameterId.Role)
                    .ToString()
                    .TryAsEnum<AgentInstallRole>(out role))
                {
                    PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.ErrorMessage,
                        string.Format(
                            StringResources.InvalidValueForCommandLineParam,
                            CommandlineParameterId.Role.ToString()));

                    return false;
                }

                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.InstallRole,
                    role);
            }

            // Parse param Installation Type
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.InstallationType))
            {
                InstallationType instlType;
                if (!commandlineParameters.GetParameterValue(CommandlineParameterId.InstallationType)
                    .ToString()
                    .TryAsEnum<InstallationType>(out instlType))
                {
                    PropertyBagDictionary.Instance.SafeAdd(
                       PropertyBagConstants.ErrorMessage,
                       string.Format(
                           StringResources.InvalidValueForCommandLineParam,
                           CommandlineParameterId.InstallationType.ToString()));
                    Trc.Log(LogLevel.Always, "Failed to parse installation type");

                    return false;
                }
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.InstallationType,
                    instlType);
                Trc.Log(LogLevel.Always, "Installation Type : {0}", instlType.ToString());
            }
            else
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.InstallationType,
                    InstallationType.Install
                    );
                Trc.Log(LogLevel.Always, "Install Type : {0}", InstallationType.Install.ToString());
            }

            // Parse param CSType
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.CSType))
            {
                ConfigurationServerType csType;
                if (! commandlineParameters.GetParameterValue(CommandlineParameterId.CSType)
                    .ToString()
                    .TryAsEnum<ConfigurationServerType>(out csType))
                {
                    PropertyBagDictionary.Instance.SafeAdd(
                       PropertyBagConstants.ErrorMessage,
                       string.Format(
                           StringResources.InvalidValueForCommandLineParam,
                           CommandlineParameterId.CSType.ToString()));
                    Trc.Log(LogLevel.Always, "Failed to parse CS type");
                    return false;
                }
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CSType,
                    csType);
                Trc.Log(LogLevel.Always, "CS Type : {0}", csType.ToString());
            }
            else
            {
                // Incase CSType parameter is not specified, we consider it as CSLegacy
                // for backward compatibility
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CSType,
                    ConfigurationServerType.CSLegacy);
                Trc.Log(LogLevel.Always, "CS Type : {0}", ConfigurationServerType.CSLegacy.ToString());
            }

            // Parse installation location.
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.InstallLocation))
            {
                string installationLocation =
                    commandlineParameters.GetParameterValue(
                    CommandlineParameterId.InstallLocation).ToString();

                installationLocation = installationLocation + "\\agent";
                Trc.Log(LogLevel.Always, "Install location : {0}", installationLocation);

                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.InstallationLocation,
                    installationLocation);
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

            // Parse Silent installation argument.
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.Silent))
            {
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagDictionary.Silent, true);
            }
            else
            {
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagDictionary.Silent, false);
            }

            // Parse SkipPrereqChecks argument.
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.SkipPrereqChecks))
            {
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagDictionary.SkipPrereqChecks, true);
            }
            else
            {
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagDictionary.SkipPrereqChecks, false);
            }

            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.LogFilePath))
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CombinedLogFile,
                    commandlineParameters.GetParameterValue(
                        CommandlineParameterId.LogFilePath).ToString());
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

            return true;
        }

        #region Silent Installation

        /// <summary>
        /// Runs silent installation.
        /// </summary>
        /// <returns>Exit code to be returned by setup.</returns>
        private static SetupHelper.UASetupReturnValues SilentInstallation()
        {
            ConsoleUtils.Log(LogLevel.Always, "Starting silent installation.");

            string platformInfo = PropertyBagDictionary.Instance.GetProperty<bool>(
                PropertyBagConstants.IsAzureVm) ?
                "The VM is running on Azure" :
                "The VM is not running on Azure";
            Trc.Log(LogLevel.Always, platformInfo);

            SetupAction setupAction = PropertyBagDictionary.Instance.GetProperty<SetupAction>(
                    PropertyBagConstants.SetupAction);

            Trc.Log(LogLevel.Always, "Installation action - " + setupAction);

            /* If user passes different role apart from already installed role,
               return AgentRoleChangeNotAllowed exit code. */
            AgentInstallRole selectedRole = 
                PropertyBagDictionary.Instance.GetProperty<AgentInstallRole>(
                    PropertyBagConstants.InstallRole);
            if ((setupAction == SetupAction.CheckFilterDriver) ||
                (setupAction == SetupAction.ExecutePostInstallSteps))
            {
                AgentInstallRole installedRole = 
                    SetupHelper.GetAgentInstalledRole();
                if (installedRole != selectedRole)
                {
                    SetupHelper.ShowErrorUA(
                        StringResources.AgentRoleMismatch);
                    SummaryFileWrapper.RecordOperation(
                        Scenario.PreInstallation,
                        OpName.RoleValidation,
                        OpResult.Failed,
                        (int)SetupHelper.UASetupReturnValues.AgentRoleChangeNotAllowed,
                        StringResources.AgentRoleMismatch);
                    return SetupHelper.UASetupReturnValues.AgentRoleChangeNotAllowed;
                }
            }

            if (setupAction == SetupAction.CheckFilterDriver)
            {
                OperationResult result = InstallationOperations.CheckFilterDriverStatus();

                if (!string.IsNullOrEmpty(result.Error))
                {
                    ConsoleUtils.Log(LogLevel.Error, result.Error);
                }

                return result.ProcessExitCode;
            }
            else if (setupAction == SetupAction.ExecutePostInstallSteps)
            {
                // Set setup action to install/upgrade.
                setupAction = InstallActionProcessor.GetAgentInstallAction();
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.SetupAction, setupAction);
                InstallActionProcessor.SetPlatform(setupAction);

                if (setupAction == SetupAction.Upgrade)
                {
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.InstallRole,
                        SetupHelper.GetAgentInstalledRole());
                    PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.InstallationLocation,
                        SetupHelper.GetAgentInstalledLocation());
                }

                OperationResult result = InstallationOperations.PostInstallationSteps();

                if (result.Status == OperationStatus.Failed)
                {
                    SetupHelper.ShowErrorUA(result.Error);
                    return result.ProcessExitCode;
                }

                int instllStatus = PropertyBagDictionary.Instance.GetProperty<int>(PropertyBagConstants.InstallationStatus);
                if (instllStatus == (int)UnifiedSetupConstants.InstallationStatus.Warning)
                {
                    Trc.Log(LogLevel.Info, "Exiting silent Installation");
                    return SetupHelper.UASetupReturnValues.SucceededWithWarnings;
                }
                else
                {
                    ConsoleUtils.Log(LogLevel.Info, "Post Install actions executed successfully.");
                    return SetupHelper.UASetupReturnValues.Successful;
                }             
            }
            else if (setupAction == SetupAction.InvalidOperation)
            {
                SetupHelper.ShowErrorUA(StringResources.UpgradeUnSupportedHigherVersionText);
                SummaryFileWrapper.RecordOperation(
                    Scenario.PreInstallation,
                    OpName.PrepareForInstall,
                    OpResult.Failed,
                    (int)SetupHelper.UASetupReturnValues.DowngradeNotSupported,
                    StringResources.UpgradeUnSupportedHigherVersionText);
                return SetupHelper.UASetupReturnValues.DowngradeNotSupported;
            }
            else if (setupAction == SetupAction.UnSupportedUpgrade)
            {
                SetupHelper.ShowErrorUA(StringResources.UpgradeUnSupportedText);
                SummaryFileWrapper.RecordOperation(
                    Scenario.PreInstallation,
                    OpName.PrepareForInstall,
                    OpResult.Failed,
                    (int)SetupHelper.UASetupReturnValues.UpgradeNotSupported);
                return SetupHelper.UASetupReturnValues.UpgradeNotSupported;
            }

            ConsoleUtils.Log(LogLevel.Always, "Starting installation with action as - " + setupAction);

            // B. Figure out the installation location.
            if (setupAction == SetupAction.Upgrade)
            {
                // In case of upgrade we blindly overwrite with the place where the installation
                // happened last.
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.InstallationLocation,
                    SetupHelper.GetAgentInstalledLocation());
            }

            ConsoleUtils.Log(
                LogLevel.Always,
                "Starting installation with location as - " +
                    PropertyBagDictionary.Instance.GetProperty<string>(
                        PropertyBagConstants.InstallationLocation));

            // D. Check install role status.
            if (setupAction == SetupAction.Upgrade &&
                !PropertyBagDictionary.Instance.ContainsKey(PropertyBagConstants.InstallRole))
            {
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.InstallRole,
                    SetupHelper.GetAgentInstalledRole());
            }

            // E. Set backing platform.
            InstallActionProcessor.SetPlatform(setupAction);

            bool startAgentServicesAtEnd = setupAction == SetupAction.Upgrade &&
                SetupHelper.IsAgentConfigured();            

            // G. Prepare for installation.
            ConsoleUtils.Log(LogLevel.Info, "Preparing for installation...");
            OperationResult opResult = InstallationOperations.PrepareForInstall();

            if (opResult.Status == OperationStatus.Failed)
            {
                SetupHelper.ShowErrorUA(opResult.Error);
                return opResult.ProcessExitCode;
            }

            // H. Install/Upgrade
            ConsoleUtils.Log(LogLevel.Info, "Installing components....");
            opResult = InstallationOperations.InstallComponents();

            if (opResult.Status == OperationStatus.Failed)
            {
                // In case of upgrade failures.. restart agent services
                if (startAgentServicesAtEnd)
                {
                    InstallationOperations.StartAgentServices();
                }

                SetupHelper.ShowErrorUA(opResult.Error);
                return opResult.ProcessExitCode;
            }
            else if (opResult.Status == OperationStatus.Warning)
            {
                ConsoleUtils.Log(LogLevel.Warn, opResult.Error);
            }

            //I. Post installation steps.
            ConsoleUtils.Log(LogLevel.Info, "Performing post installation steps...");
            opResult = InstallationOperations.PostInstallationSteps();

            if (opResult.Status == OperationStatus.Failed)
            {
                SetupHelper.ShowErrorUA(opResult.Error);
                return opResult.ProcessExitCode;
            }
            else if (opResult.Status == OperationStatus.Warning)
            {
                ConsoleUtils.Log(LogLevel.Warn, opResult.Error);
            }

            // Start services.
            bool platformChange = PropertyBagDictionary.Instance.GetProperty<bool>(
                PropertyBagDictionary.PlatformChange) ||
                PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.PlatformUpgradeFromCSToCSPrime);
            Trc.Log(LogLevel.Always, "PlatformChange value - {0}", platformChange);
            Trc.Log(LogLevel.Always, "There is a platform change from vmware to azure or from cs to csprime. So agent services will not be started.");
            if (startAgentServicesAtEnd && !platformChange)
            {
                ConsoleUtils.Log(LogLevel.Info, "Starting services....");
                opResult = InstallationOperations.StartAgentServices();

                if (opResult.Status == OperationStatus.Failed)
                {
                    SetupHelper.ShowErrorUA(opResult.Error);
                    return opResult.ProcessExitCode;
                }
            }
            else
            {
                Trc.Log(LogLevel.Always, "Not starting agent services.");
            }

            string installationLocation = SetupHelper.GetAgentInstalledLocation();
            string rcmCliPath = Path.Combine(
                installationLocation,
                UnifiedSetupConstants.AzureRCMCliExeName);
            string output;

            if ((PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSType) ==
                ConfigurationServerType.CSPrime.ToString()) &&
                (PropertyBagDictionary.Instance.GetProperty<SetupInvoker>(
                    PropertyBagConstants.SetupInvoker) == SetupInvoker.commandline))
            {

                if (ValidationHelper.GetAgentConfigInput(rcmCliPath, out output))
                {
                    Console.WriteLine("Agent Config Input: {0}", output);
                }
                else
                {
                    ConsoleUtils.Log(LogLevel.Error, string.Format("Please run {0} {1} to generate agent config input manually.", rcmCliPath, UnifiedSetupConstants.AzureRcmCliAgentConfigInput));
                    return SetupHelper.UASetupReturnValues.AgentConfigInputFailure;
                }
            }

            string instlStatus = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.InstallationStatus);
            if (instlStatus.Equals(UnifiedSetupConstants.InstallationStatus.Warning.ToString(), StringComparison.OrdinalIgnoreCase))
            {
                Trc.Log(LogLevel.Info, "Exiting silent Installation");
                return SetupHelper.UASetupReturnValues.SucceededWithWarnings;
            }
            else
            {
                ConsoleUtils.Log(LogLevel.Info, "Post Install actions executed successfully.");
                return SetupHelper.UASetupReturnValues.Successful;
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
                    "Exit code returned by unified agent: {0}({1})",
                    (int)returnValue,
                    returnValue.ToString());
            }

            SetupHelper.LogPropertyBag();
        }
        #endregion
    }
}
