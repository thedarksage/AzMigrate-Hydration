using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security;
using System.Threading;
using System.Windows;
using System.Xml;
using System.Xml.Serialization;
using ASRResources;
using ASRSetupFramework;
using IntegrityCheck = Microsoft.DisasterRecovery.IntegrityCheck;
using DRSetupFramework = Microsoft.DisasterRecovery.SetupFramework;

namespace UnifiedSetup
{
    /// <summary>
    /// This is the main class that control the Microsoft Azure Site Recovery Provider setup.exe
    /// </summary>
    public static class Program
    {
        # region Fields
        /// <summary>
        /// Blocks multiple instances of same exe.
        /// </summary>
        private static Mutex mutex;

        /// <summary>
        /// Error message.
        /// </summary>
        private static string detailedErrorMessage;

        /// <summary>
        /// Status variable for upgrade scenario.
        /// </summary>
        private static bool isUpgradeScenario;

        /// <summary>
        /// Stores the system directory.
        /// </summary>
        public static string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);

        # endregion

        /// <summary>
        /// Main entry point
        /// </summary>
        /// <returns>int a value from the SetupReturnValues</returns>
        [System.Diagnostics.CodeAnalysis.SuppressMessage(
            "Microsoft.Design",
            "CA1031:DoNotCatchGeneralExceptionTypes",
            Justification = "Setup should never crash on the user." +
            "By catching the general exception at this level, we keep that from happening.")]
        [SecurityCritical]
        [STAThread]
        public static int Main()
        {
            SetupHelper.SetupReturnValues returnValue = SetupHelper.SetupReturnValues.Failed;

            // Decide if we go Console mode or Windows mode
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
            mutex = new Mutex(true, "Microsoft Azure Site Recovery Setup", out createdNew);
            if (!createdNew)
            {
                SetupHelper.ShowError(StringResources.ProcessAlreadyLaunched);
                return (int)returnValue;
            }

            SetDefaultPropertyValues();
            Trc.Initialize(
                (LogLevel.Always | LogLevel.Debug | LogLevel.Error | LogLevel.Warn | LogLevel.Info),
                PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.DefaultLogName));
            Trc.Log(LogLevel.Always, "Application Started");

            // Check for .NET framework 4.6.2 and above.
            if (!InstallActionProcessess.CheckNETFramework462OrAbove())
            {
                SetupHelper.ShowError(StringResources.NetFramework462OrAboveErrorMessage);
                Environment.Exit(1);
            }

            DRSetupFramework.Trc.Initialize(
                DRSetupFramework.LogLevel.Always,
                PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.IntegrityCheckLogName));
            DRSetupFramework.Trc.Log(DRSetupFramework.LogLevel.Always, "IntegrityCheck Started");

            try
            {
                // Checks MySQL 5.5.37 installation status
                if (SetupHelper.IsMySQLInstalled(UnifiedSetupConstants.MySQL55Version))
                {
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.MySQL55Installed, true);
                }
                
                // Get the version of the current installer.
                Version currentBuildVersion = InstallActionProcessess.GetBuildVersion(
                    UnifiedSetupConstants.UnifiedSetupExeName);
                SetupParameters.Instance().CurrentBuildVersion = currentBuildVersion;
                Trc.Log(LogLevel.Always, "currentBuildVersion - {0}", currentBuildVersion);

                // Check for upgrade support.
                isUpgradeScenario = InstallActionProcessess.IsUpgradeSupported(out detailedErrorMessage);

                IntegrityCheck.IntegrityCheckWrapper.StartSummary(
                    UnifiedSetupConstants.UnifiedSetupProductName,
                    currentBuildVersion,
                    isUpgradeScenario ? IntegrityCheck.InstallationType.Upgrade : IntegrityCheck.InstallationType.New,
                    DRSetupFramework.DRSetupConstants.FabricAdapterType.InMageAdapter);

                // OVF image scenario.
                if (DRSetupFramework.RegistryUtils.IsRegistryKeyExists(UnifiedSetupConstants.OVFRegKey))
                {
                    // Set OVFImage status to true.
                    SetupParameters.Instance().OVFImage = true;
                }

                // Parse command line arguments.
                if (ParseCommandLine())
                {
                    // Creating machine identifier.
                    InstallActionProcessess.CreateMachineIdentifier();

                    if (!isUpgradeScenario)
                    {
                        Trc.Log(LogLevel.Always, "tls check value : {0}", SetupParameters.Instance().SkipTLSCheck);
                        // Perform TLS validation.
                        if (!SetupParameters.Instance().SkipTLSCheck)
                        {
                            string errorMessage = "";
                            IntegrityCheck.Response validationResponse = IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.CheckTLS10Status);
                            IntegrityCheck.IntegrityCheckWrapper.RecordOperation(
                                IntegrityCheck.ExecutionScenario.PreInstallation,
                                EvaluateOperations.ValidationMappings[IntegrityCheck.Validations.CheckTLS10Status],
                                validationResponse.Result,
                                validationResponse.Message,
                                validationResponse.Causes,
                                validationResponse.Recommendation,
                                validationResponse.Exception);

                            if (validationResponse.Result == IntegrityCheck.OperationResult.Failed)
                            {
                                errorMessage = string.Format(
                                    "{0} {1} {2}",
                                    validationResponse.Message,
                                    Environment.NewLine,
                                    validationResponse.Recommendation);
                                if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagDictionary.Silent))
                                {
                                    ConsoleUtils.Log(LogLevel.Error, errorMessage);
                                }
                                else
                                {
                                    SetupHelper.ShowError(errorMessage);
                                }

                                Environment.Exit(1);
                            }
                        }
                    }

                    // Silent installation.
                    if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagDictionary.Silent))
                    {
                        returnValue = SilentRun();

                        if (ServiceChecks())
                        {
                            if (returnValue == SetupHelper.SetupReturnValues.Successful)
                            {
                                SetupParameters.Instance().IsInstallationSuccess = true;
                                // Return 3010 exit code, if post installation reboot is required.
                                bool isRebootRequired;
                                if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade)
                                {
                                    isRebootRequired = (SetupParameters.Instance().RebootRequired == UnifiedSetupConstants.Yes);
                                }
                                else
                                {
                                    isRebootRequired =
                                        ((ValidationHelper.PostAgentInstallationRebootRequired()) ||
                                        (SetupParameters.Instance().RebootRequired == UnifiedSetupConstants.Yes));
                                }

                                if (isRebootRequired)
                                {
                                    ConsoleUtils.Log(LogLevel.Error, StringResources.PostInstallationRebootRequired);
                                    returnValue = SetupHelper.SetupReturnValues.SuccessfulNeedReboot;
                                }
                            }
                        }
                        else
                        {
                            returnValue = returnValue == SetupHelper.SetupReturnValues.Successful ?
                                SetupHelper.SetupReturnValues.ServicesAreNotRunning :
                                returnValue;
                        }

                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.InstallationFinished,
                            true);
                        IntegrityCheck.IntegrityCheckWrapper.EndSummary(
                            returnValue == SetupHelper.SetupReturnValues.Successful ?
                                IntegrityCheck.OperationResult.Success :
                                IntegrityCheck.OperationResult.Failed,
                            (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode) ?
                                UnifiedSetupConstants.ScaleOutUnitComponentName :
                                string.Empty);
                        IntegrityCheck.IntegrityCheckWrapper.DisposeSummary();
                        InstallActionProcessess.UploadOperationSummary();
                        InstallActionProcessess.ChangeLogUploadServiceToManual();
                    }
                    else
                    {
                        // Interactive installation.

                        // Upgrade supported scenario.
                        if (isUpgradeScenario)
                        {
                            // Upgrade is supported.
                            if (SetupHelper.ShowMessage(
                                StringResources.DoUpgrade,
                                MessageBoxButton.YesNo,
                                MessageBoxImage.Information) == true)
                            {
                                Trc.Log(LogLevel.Always, "User selected Yes button. Proceeding ahead with upgrade.");

                                // Validate upgrade pre-requisites.
                                if (InstallActionProcessess.UpgradePrereqs(out detailedErrorMessage))
                                {
                                    UiRun();
                                }
                                else
                                {
                                    SetupHelper.ShowError(detailedErrorMessage);
                                }
                            }
                            else
                            {
                                Trc.Log(LogLevel.Error, "User cancelled the setup - Upgrade required.");
                            }
                        }
                        // Re-installation.
                        else if ((SetupParameters.Instance().ReInstallationStatus == UnifiedSetupConstants.Yes) &&
                                (SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall))
                        {
                            if (SetupHelper.ShowMessage(
                                StringResources.CSPSinstalledMTNotInstalledText,
                                MessageBoxButton.YesNo,
                                MessageBoxImage.Information) == true)
                            {
                                UiRun();
                            }
                            else
                            {
                                Trc.Log(LogLevel.Error, "User cancelled the setup - Re-installation.");
                            }
                        }
                        // Fresh installation.
                        else if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall)
                        {
                            // Validate fresh installation pre-requisites.
                            if (InstallActionProcessess.FreshInstallationPrereqs(out detailedErrorMessage))
                            {
                                UiRun();
                            }
                            else
                            {
                                SetupHelper.ShowError(detailedErrorMessage);
                            }
                        }
                        // Upgrade unsupported scenario.
                        else
                        {
                            SetupHelper.ShowError(detailedErrorMessage);
                        }

                        // Set the application return value: Always success in UI mode
                        returnValue = SetupHelper.SetupReturnValues.Successful;
                    }
                }
                else
                {
                    // Could not parse the command line.
                    returnValue = SetupHelper.SetupReturnValues.InvalidCommandLine;
                }

                // Display suceess/failure message on the console.
                if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagDictionary.Silent))
                {
                    ConsoleUtils.Log(LogLevel.Error, "");
                    if (returnValue != SetupHelper.SetupReturnValues.Successful &
                        returnValue != SetupHelper.SetupReturnValues.SuccessfulNeedReboot)
                    {
                        ConsoleUtils.Log(LogLevel.Error, StringResources.InstallationFailed);
                    }
                    else
                    {
                        ConsoleUtils.Log(LogLevel.Always, StringResources.UnifiedSetupInstallationSucceeded);
                    }
                }
            }
            catch (Exception exception)
            {
                // If we get any other excpetions other than the ones we have handled we show them and then wizard closes.
                Trc.LogException(LogLevel.Always, "Uncaught Exception", exception);
                SetupHelper.ShowError(exception.Message);
            }
            finally
            {
                // If installation didn't finish, may be by aborting from user/exceptions
                if (!PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.InstallationFinished))
                {
                    if (PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.InstallationAborted))
                    {
                        IntegrityCheck.IntegrityCheckWrapper.EndSummary(
                            IntegrityCheck.OperationResult.Aborted,
                            (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode) ?
                                UnifiedSetupConstants.ScaleOutUnitComponentName :
                                string.Empty);
                    }
                    else
                    {
                        IntegrityCheck.IntegrityCheckWrapper.EndSummary(
                            IntegrityCheck.OperationResult.Failed,
                            (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode) ?
                                UnifiedSetupConstants.ScaleOutUnitComponentName :
                                string.Empty);
                    }

                    IntegrityCheck.IntegrityCheckWrapper.DisposeSummary();
                }

                EndOfAppDataLogging(returnValue);

                if (SetupParameters.Instance().ServerMode == 
                    UnifiedSetupConstants.PSServerMode)
                {
                    SetupHelper.TransferScaleOutUnitLogsToCSPSSetup(
                        SetupParameters.Instance().CSIP);
                }

                DeleteSetupCreatedFiles();
                mutex.ReleaseMutex();
            }

            return (int)returnValue;
        }


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
        /// Validates the type of installation and invokes installation methods.
        /// </summary>
        /// <returns>a non-zero error code for the failure and 0 on sucess</returns>
        private static SetupHelper.SetupReturnValues SilentRun()
        {
            // Upgrade supported scenario.
            if (isUpgradeScenario)
            {
                // Validate upgrade pre-requisites.
                if (InstallActionProcessess.UpgradePrereqs(out detailedErrorMessage))
                {
                    SetupHelper.SetupReturnValues setupReturnCode;
                    if (InstallComponents(out setupReturnCode))
                    {
                        return SetupHelper.SetupReturnValues.Successful;
                    }
                    else
                    {
                        return setupReturnCode;
                    }
                }
                else
                {
                    ConsoleUtils.Log(LogLevel.Error, detailedErrorMessage);
                    return SetupHelper.SetupReturnValues.Failed;
                }

            }
            // Re-installation.
            else if ((SetupParameters.Instance().ReInstallationStatus == UnifiedSetupConstants.Yes) &&
                    (SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall))
            {
                SetupHelper.SetupReturnValues setupReturnCode;
                if (InstallComponents(out setupReturnCode))
                {
                    return SetupHelper.SetupReturnValues.Successful;
                }
                else
                {
                    return setupReturnCode;
                }
            }
            // Fresh installation.
            else if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall)
            {
                // Validate fresh installation pre-requisites.
                if (InstallActionProcessess.FreshInstallationPrereqs(out detailedErrorMessage))
                {
                    // Check for all system pre-requisites.
                    if (CheckForPreRequisites())
                    {
                        Trc.Log(LogLevel.Always, "System pre-requisite checks have passed.");

                        // Clean up DRA related registries.
                        InstallActionProcessess.CallPurgeDRARegistryMethod();

                        SetupHelper.SetupReturnValues setupReturnCode;
                        if (InstallComponents(out setupReturnCode))
                        {
                            return SetupHelper.SetupReturnValues.Successful;
                        }
                        else
                        {
                            return setupReturnCode;
                        }
                    }
                    else
                    {
                        return SetupHelper.SetupReturnValues.PrerequisiteChecksFailed;
                    }
                }
                else
                {
                    ConsoleUtils.Log(LogLevel.Error, detailedErrorMessage);
                    return SetupHelper.SetupReturnValues.Failed;
                }
            }
            // Upgrade unsupported scenario.
            else
            {
                ConsoleUtils.Log(LogLevel.Error, detailedErrorMessage);
                return SetupHelper.SetupReturnValues.UpgradeNotSupported;
            }
        }

        /// <summary>
        /// Handles the Closed event of the Host control.
        /// </summary>
        /// <param name="sender">The source of the event.</param>
        /// <param name="e">The <see cref="System.EventArgs"/> instance containing the event data.</param>
        private static void Host_Closed(object sender, EventArgs e)
        {
            // Application.Exit();
        }

        /// <summary>
        /// Parses the command line.
        /// </summary>
        /// <returns>
        /// true if valid command line arguments are passed to installer - silent installation
        /// always true - Interactive installation.
        /// </returns>
        private static bool ParseCommandLine()
        {
            bool result = true;

            // Get command line arguments.
            String[] arguments = Environment.GetCommandLineArgs();

            // If command line arguments are more than 1,
            // set installation mode as silent and validate command line arguments.
            if (Environment.GetCommandLineArgs().Length > 1)
            {
                // Set installation mode to silent.
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagDictionary.Silent, true);

                // Parse command line arguments.
                CommandlineParameters commandlineParameters = new CommandlineParameters();
                commandlineParameters.ParseCommandline(arguments);

                // Validate command line arguments passed to the installer.
                if (!ValidateCommandLineArguments(commandlineParameters))
                {
                    return false;
                }
            }
            return result;
        }

        /// <summary>
        /// Validate command line arguments.
        /// </summary>
        /// <param name="commandlineParameters">commandlineParameters object</param>
        /// <returns>true if no errors</returns>
        private static bool ValidateCommandLineArguments(CommandlineParameters commandlineParameters)
        {
            bool result = true;
            int upgradeCommandlineLength;

            // Validate command line arguments and assign them to SetupParameters members.
            Trc.Log(LogLevel.Always, "Main : Current directory = {0}", Environment.CurrentDirectory);

            // Switch to upgrade workflow when /Upgrade switch is passed.
            if ((bool)commandlineParameters.GetParameterValue(CommandlineParameterId.Upgrade))
            {
                ConsoleUtils.ShowConsoleWindow();

                // Check for /SkipPrereqsCheck argument.
                if ((bool)commandlineParameters.GetParameterValue(CommandlineParameterId.SkipPrereqsCheck))
                {
                    Trc.Log(LogLevel.Always, "/SkipPrereqsCheck argument is passed to the installer.");
                    SetupParameters.Instance().SkipPrereqsCheck = true;
                }

                if ((PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.MySQL55Installed)) && (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode))
                {
                    // Display Thirdparty EULA when /ShowThirdpartyEULA argument is passed and bail out the installation.
                    if ((bool)commandlineParameters.GetParameterValue(CommandlineParameterId.ShowThirdpartyEULA))
                    {
                        ConsoleUtils.ShowConsoleWindow();
                        ConsoleUtils.Log(LogLevel.Info, StringResources.UnifiedSetupThirdpartyEULAText);
                        return false;
                    }

                    // Abort installation if /AcceptThirdpartyEULA argument is not passed.
                    if (!(bool)commandlineParameters.GetParameterValue(CommandlineParameterId.AcceptThirdpartyEULA))
                    {
                        ConsoleUtils.ShowConsoleWindow();
                        ConsoleUtils.Log(LogLevel.Error, "/AcceptThirdpartyEULA is a mandatory argument. Please pass the argument and retry the installation.");
                        ConsoleUtils.Log(LogLevel.Info, StringResources.CommandlineUsage);
                        return false;
                    }
                    upgradeCommandlineLength = 5;
                    // Fail installation when some other switches are also passed along with /Upgrade /AcceptThirdpartyEULA switch.
                    if (Environment.GetCommandLineArgs().Length >= upgradeCommandlineLength)
                    {
                        ConsoleUtils.Log(LogLevel.Error, "Some other switches are passed to the installer along with /Upgrade /AcceptThirdpartyEULA switch. Please re-try installation by passing /Upgrade /AcceptThirdpartyEULA switch alone.");
                        return false;
                    }
                }
                else
                {
                    upgradeCommandlineLength = 4;
                    // Fail installation when some other switches are also passed along with /Upgrade switch.
                    if (Environment.GetCommandLineArgs().Length >= upgradeCommandlineLength)
                    {
                        ConsoleUtils.Log(LogLevel.Error, "Some other switches are passed to the installer along with /Upgrade switch. Please re-try installation by passing /Upgrade switch alone.");
                        return false;
                    }
                }

                // Fail installation when all of the components are not installed but /Upgrade switch is passed.
                if ((SetupParameters.Instance().ProductISAlreadyInstalled != UnifiedSetupConstants.Yes))
                {
                    ConsoleUtils.Log(LogLevel.Error, "All the Unified Setup components are not installed on this server, but /Upgrade switch is passed. Please re-try installation by passing fresh installation arguments.");
                    return false;
                }
                return result;
            }
            else
            {
                // Fail installation when all components are already installed but /Upgrade switch is not passed.
                if ((SetupParameters.Instance().ProductISAlreadyInstalled == UnifiedSetupConstants.Yes))
                {
                    ConsoleUtils.Log(LogLevel.Error, "All the components of Unified Setup are already installed on this server. Pass /Upgrade switch to proceed with upgrade.");
                    return false;
                }
            }

            // Display usage when /? argument is passed and bail out the installation.
            if ((bool)commandlineParameters.GetParameterValue(CommandlineParameterId.Help))
            {
                ConsoleUtils.ShowConsoleWindow();
                ConsoleUtils.Log(LogLevel.Info, StringResources.CommandlineUsage);
                return false;
            }

            // Display Thirdparty EULA when /ShowThirdpartyEULA argument is passed and bail out the installation.
            if ((bool)commandlineParameters.GetParameterValue(CommandlineParameterId.ShowThirdpartyEULA))
            {
                ConsoleUtils.ShowConsoleWindow();
                ConsoleUtils.Log(LogLevel.Info, StringResources.UnifiedSetupThirdpartyEULAText);
                return false;
            }

            // Abort installation if /AcceptThirdpartyEULA argument is not passed.
            if (!(bool)commandlineParameters.GetParameterValue(CommandlineParameterId.AcceptThirdpartyEULA))
            {
                ConsoleUtils.ShowConsoleWindow();
                ConsoleUtils.Log(LogLevel.Error, "/AcceptThirdpartyEULA is a mandatory argument. Please pass the argument and retry the installation.");
                ConsoleUtils.Log(LogLevel.Info, StringResources.CommandlineUsage);
                return false;
            }

            // Check whether /SkipSpaceCheck argument is passed.
            if ((bool)commandlineParameters.GetParameterValue(CommandlineParameterId.SkipSpaceCheck))
            {
                Trc.Log(LogLevel.Always, "/SkipSpaceCheck argument is passed to the installer.");
                SetupParameters.Instance().SkipSpaceCheck = "true";
            }

            // Check for /SkipPrereqsCheck argument.
            if ((bool)commandlineParameters.GetParameterValue(CommandlineParameterId.SkipPrereqsCheck))
            {
                Trc.Log(LogLevel.Always, "/SkipPrereqsCheck argument is passed to the installer.");
                SetupParameters.Instance().SkipPrereqsCheck = true;
            }

            // Check for /SkipRegistration argument.
            if ((bool)commandlineParameters.GetParameterValue(CommandlineParameterId.SkipRegistration))
            {
                Trc.Log(LogLevel.Always, "/SkipRegistration argument is passed to the installer.");
                SetupParameters.Instance().SkipRegistration = true;
            }

            // Check for /SkipTLSCheck argument.
            if ((bool)commandlineParameters.GetParameterValue(CommandlineParameterId.SkipTLSCheck))
            {
                Trc.Log(LogLevel.Always, "/SkipTLSCheck argument is passed to the installer.");
                SetupParameters.Instance().SkipTLSCheck = true;
            }

            // Get Server Mode parameter value
            string serverMode = "";
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.ServerMode))
            {
                serverMode = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.ServerMode);
                Trc.Log(LogLevel.Always, "ServerMode was specified as {0}", serverMode);
            }
            else
            {
                ConsoleUtils.Log(LogLevel.Error, "/ServerMode is a mandatory argument. Please pass the argument and retry the installation.");
                ConsoleUtils.Log(LogLevel.Error, StringResources.CommandlineUsage);
                return false;
            }

            // Validate ServerMode parameter value
            if ((serverMode.ToUpper() != UnifiedSetupConstants.CSServerMode) &&
                (serverMode.ToUpper() != UnifiedSetupConstants.PSServerMode))
            {
                ConsoleUtils.Log(LogLevel.Error, "Invalid ServerMode argument value: " + serverMode);
                ConsoleUtils.Log(LogLevel.Error, StringResources.CommandlineUsage);
                return false;
            }
            else
            {
                Trc.Log(LogLevel.Always, "Successfully validated ServerMode argument value.");
                SetupParameters.Instance().ServerMode = serverMode.ToUpper();
            }

            // Check whether each mandatory argument is passed or not.
            bool isInstallLocationSpecified = commandlineParameters.IsParameterSpecified(CommandlineParameterId.InstallLocation);
            bool isMySQLCredsFilePathSpecified = commandlineParameters.IsParameterSpecified(CommandlineParameterId.MySQLCredsFilePath);
            bool isEnvTypeSpecified = commandlineParameters.IsParameterSpecified(CommandlineParameterId.EnvType);
            bool isCSIPSpecified = commandlineParameters.IsParameterSpecified(CommandlineParameterId.CSIP);
            bool isPassphraseFilePathSpecified = commandlineParameters.IsParameterSpecified(CommandlineParameterId.PassphraseFilePath);
            bool isVaultCredsFilePathSpecified = false;
            
            if (SetupParameters.Instance().SkipRegistration)
            {
                Trc.Log(LogLevel.Always, "Skipping Vault Credentials File Path validation as /SkipRegistration argument is passed to the installer.");
                isVaultCredsFilePathSpecified = true;
            }
            else
            {
                isVaultCredsFilePathSpecified = commandlineParameters.IsParameterSpecified(CommandlineParameterId.VaultCredsFilePath);
            }

            // Check whether all mandatory arguments are passed, based on the ServerMode value.
            if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
            {
                // Mandatory arguments for CS installation are InstallLocation, MysqlCredsFilePath, ValutCredsFilePath and EnvType.
                if (isInstallLocationSpecified && isMySQLCredsFilePathSpecified && isVaultCredsFilePathSpecified && isEnvTypeSpecified)
                {
                    Trc.Log(LogLevel.Always, "All mandatory arguments that are required for CS+PS installation are passed.");
                }
                else
                {
                    // Construct mandatory arguments dictionary
                    Dictionary<string, bool> mandatoryArguments = new Dictionary<string, bool>();
                    mandatoryArguments.Add("InstallLocation", isInstallLocationSpecified);
                    mandatoryArguments.Add("MySQLCredsFilePath", isMySQLCredsFilePathSpecified);
                    mandatoryArguments.Add("VaultCredsFilePath", isVaultCredsFilePathSpecified);
                    mandatoryArguments.Add("EnvType", isEnvTypeSpecified);

                    // Construct missing arguments string
                    string missingArguments = "";
                    foreach (KeyValuePair<string, bool> kvp in mandatoryArguments)
                    {
                        // If argument is not passed add it to the missingArguments string.
                        if (!kvp.Value)
                        {
                            missingArguments += kvp.Key + " ";
                        }
                    }
                    // Trim and replace space with a comma.
                    missingArguments = missingArguments.Trim().Replace(' ', ',');

                    ConsoleUtils.Log(LogLevel.Error, "Following arguments are missing which are mandatory for CS installation: " + missingArguments);
                    ConsoleUtils.Log(LogLevel.Error, StringResources.CommandlineUsage);
                    return false;
                }
            }
            else if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
            {
                // Mandatory arguments for PS installation are InstallLocation, EnvType, CSIP and PassphraseFilePath.
                if (isInstallLocationSpecified && isEnvTypeSpecified && isCSIPSpecified && isPassphraseFilePathSpecified)
                {
                    Trc.Log(LogLevel.Always, "All mandatory arguments that are required for PS installation are passed.");
                }
                else
                {
                    // Construct mandatory arguments dictonary
                    Dictionary<string, bool> mandatoryArguments = new Dictionary<string, bool>();
                    mandatoryArguments.Add("InstallLocation", isInstallLocationSpecified);
                    mandatoryArguments.Add("EnvType", isEnvTypeSpecified);
                    mandatoryArguments.Add("CSIP", isCSIPSpecified);
                    mandatoryArguments.Add("PassphraseFilePath", isPassphraseFilePathSpecified);

                    // Construct missing arguments string
                    string missingArguments = "";
                    foreach (KeyValuePair<string, bool> kvp in mandatoryArguments)
                    {
                        // If argument is not passed add it to the missingArguments string.
                        if (!kvp.Value)
                        {
                            missingArguments += kvp.Key + " ";
                        }
                    }
                    // Trim and replace space with a comma.
                    missingArguments = missingArguments.Trim().Replace(' ', ',');

                    ConsoleUtils.Log(LogLevel.Error, "Following arguments are missing which are mandatory for PS installation: " + missingArguments);
                    ConsoleUtils.Log(LogLevel.Error, StringResources.CommandlineUsage);
                    return false;
                }
            }

            // Validate Install Location Parameter regardless of ServerMode parameter.
            string installLocation = "";
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.InstallLocation))
            {
                installLocation = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.InstallLocation);
                Trc.Log(LogLevel.Always, "InstallLocation was specified as {0}", installLocation);
            }

            // Gets install drive from install location and checks whether installation drive is system drive or not.
            bool installLocationDriveValResult = false;
            installLocationDriveValResult = ValidationHelper.IsNonSystemDrive(installLocation);
            if (installLocationDriveValResult)
            {
                Trc.Log(LogLevel.Always, "Installation drive validation against system drive has succeeded.");
            }
            else
            {
                ConsoleUtils.Log(LogLevel.Error, "Install location(" + installLocation + ") either be a System Drive or relative path." + "\n");
                return false;
            }

            // Do not check for free space if /SkipSpaceCheck argument is passed and pefrom the rest of the checks like NTFS drive, fixed drive etc.
            bool InstallLocationValResult = false;
            if ((SetupParameters.Instance().SkipSpaceCheck == "true") || SetupParameters.Instance().SkipPrereqsCheck)
            {
                InstallLocationValResult = ValidationHelper.ValidateInstallLocation(installLocation, false);
                if (!InstallLocationValResult)
                {
                    ConsoleUtils.Log(LogLevel.Error, "Either install location(" + installLocation + ") does not exist (or) install location is not on a fixed drive (or)" +
                                                     " install location is not on a NTFS drive." + "\n");
                    return false;
                }
            }
            else
            {
                InstallLocationValResult = ValidationHelper.ValidateInstallLocation(installLocation, true);
                if (!InstallLocationValResult)
                {
                    ConsoleUtils.Log(LogLevel.Error, "Either install location(" + installLocation + ") does not exist (or) it does not have 600 GB free space" +
                                                     " (or) install location is not on a fixed drive (or) install location is not on a NTFS drive." + "\n");
                    return false;
                }
            }

            Trc.Log(LogLevel.Always, "Successfully validated InstallLocation argument value.");
            SetupParameters.Instance().CXInstallDir = installLocation + @"\home\svsystems";
            SetupParameters.Instance().AgentInstallDir = installLocation;

            // MySQL Credentials File Path Parameter if ServerMode is CS
            if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
            {
                // Validate MySQL Credentials File Path Parameter
                string mysqlCredsFilePath = "";
                if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.MySQLCredsFilePath))
                {
                    mysqlCredsFilePath = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.MySQLCredsFilePath);
                    Trc.Log(LogLevel.Always, "MySQLCredsFilePath was specified as {0}", mysqlCredsFilePath);
                }

                bool mysqlCredsFilePathValResult = ValidationHelper.ValidateMySQLCredsFilePath(mysqlCredsFilePath);
                if (!mysqlCredsFilePathValResult)
                {
                    ConsoleUtils.Log(LogLevel.Error, "Either MySQL credentials file(" + mysqlCredsFilePath + ") does not exist or it's contents are not valid." + "\n");
                    ConsoleUtils.Log(LogLevel.Error, StringResources.InvalidMySQLCredsFilePath);
                    return false;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Successfully validated MySQL credentials file and it's contents.");
                    SetupParameters.Instance().MysqlCredsFilePath = mysqlCredsFilePath;
                }
            }

            if (SetupParameters.Instance().SkipRegistration)
            {
                Trc.Log(LogLevel.Always, "Skipping Vault Credentials File Path validation as /SkipRegistration argument is passed to the installer.");
            }
            else
            {
                // Validate Vault Credentials File Path Parameter if ServerMode is CS
                if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                {
                    string vaultCredsFilePath = "";
                    if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.VaultCredsFilePath))
                    {
                        vaultCredsFilePath = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.VaultCredsFilePath);
                        Trc.Log(LogLevel.Always, "VaultCredsFilePath was specified as {0}", vaultCredsFilePath);
                    }

                    bool vaultCredsFilePathValResult = ValidationHelper.ValidateVaultCredsFilePath(vaultCredsFilePath);
                    if (!vaultCredsFilePathValResult)
                    {
                        ConsoleUtils.Log(LogLevel.Error, "Either vault credentials file(" + vaultCredsFilePath + ") does not exist or it does not have .VaultCredentials extension." + "\n");
                        return false;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Successfully validated VaultCredsFilePath value.");
                        SetupParameters.Instance().ValutCredsFilePath = vaultCredsFilePath;
                    }
                }
            }

            // Validate Environment Type Parameter regardless of ServerMode parameter.
            string envType = "";
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.EnvType))
            {
                envType = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.EnvType);
                Trc.Log(LogLevel.Always, "EnvType was specified as {0}", envType);
            }

            // Check whether VMWare or NonVMWare values are passed.
            if (envType.ToUpper() != "VMWARE" && envType.ToUpper() != "NONVMWARE")
            {
                ConsoleUtils.Log(LogLevel.Error, "Invalid EnvType argument value: " + envType);
                ConsoleUtils.Log(LogLevel.Error, StringResources.CommandlineUsage);
                return false;
            }

            // If envType is NonVMWare, no need to check for VMware Tools
            if (envType.ToUpper() == "NONVMWARE")
            {
                Trc.Log(LogLevel.Always, "Successfully validated EnvType value.");
                SetupParameters.Instance().EnvType = "NonVMWare";
            }
            else
            {
                // If envType is VMWare, check for VMware Tools and abort if it does not exist.
                if (!(ValidationHelper.isVMwareToolsExists()))
                {
                    ConsoleUtils.Log(LogLevel.Error, StringResources.EnvironmentDetailsPageVMwareToolsNotExistsText);
                    return false;
                }
            }

            // Validate Configuration Server IP Parameter if ServerMode is PS
            if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
            {
                string csIP = "";
                if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.CSIP))
                {
                    csIP = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.CSIP);
                    Trc.Log(LogLevel.Always, "CSIP was specified as {0}", csIP);
                }

                bool csIPValResult = ValidationHelper.ValidateIPAddress(csIP);
                if (!csIPValResult)
                {
                    ConsoleUtils.Log(LogLevel.Error, "CSIP passed to the installer(" + csIP + ") is not a valid IP address." + "\n");
                    return false;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Successfully validated CSIP value.");
                    SetupParameters.Instance().CSIP = csIP;
                }
            }

            // Validate Passphrase File Path Parameter only when PS does not exist on this server.
            if (!ValidationHelper.PSExists())
            {
                // Validate Passphrase File Path Parameter only when ServerMode is PS
                if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
                {
                    string passphraseFilePath = "";

                    if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.PassphraseFilePath))
                    {
                        passphraseFilePath = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.PassphraseFilePath);
                        Trc.Log(LogLevel.Always, "PassphraseFilePath was specified as {0}", passphraseFilePath);
                    }

                    bool passphraseFilePathValResult = ValidationHelper.ValidateConnectionPassphrase(passphraseFilePath, SetupParameters.Instance().CSIP, SetupParameters.Instance().CSPort);
                    if (!passphraseFilePathValResult)
                    {
                        ConsoleUtils.Log(LogLevel.Error, "Passphrase validation has failed. Please ensure that passphrase file(" + passphraseFilePath +
                                                         ") exists and it has correct passphrase. Also ensure that you are passing correct CSIP value." + "\n");
                        return false;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Successfully validated PassphraseFilePath value.");
                        SetupParameters.Instance().PassphraseFilePath = passphraseFilePath;
                    }
                }
            }
            else
            {
                Trc.Log(LogLevel.Always, "Skipping Passphrase File Path Parameter validation as PS already exists on this server.");
            }

            // Validate Process Server IP Parameter if it is passed, regardless of ServerMode parameter.
            string psIP = "";
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.PSIP))
            {
                psIP = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.PSIP);
                Trc.Log(LogLevel.Always, "PSIP was specified as {0}.", psIP);

                bool psIPValResult = ValidationHelper.ValidateIPAddress(psIP);
                if (!psIPValResult)
                {
                    ConsoleUtils.Log(LogLevel.Error, "PSIP passed to the installer(" + psIP + ") is not a valid IP address." + "\n");
                    return false;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Successfully validated CSIP value.");
                    SetupParameters.Instance().PSIP = psIP;
                }
            }
            else
            {
                Trc.Log(LogLevel.Always, "PSIP was not specified.");
            }

            // Validate Azure IP Parameter if it is passed, only on CS.
            if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
            {
                string azureIP = "";
                if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.AZUREIP))
                {
                    azureIP = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.AZUREIP);
                    Trc.Log(LogLevel.Always, "AZUREIP was specified as {0}.", azureIP);

                    bool azureIPValResult = ValidationHelper.ValidateIPAddress(azureIP);
                    if (!azureIPValResult)
                    {
                        ConsoleUtils.Log(LogLevel.Error, "AZUREIP passed to the installer(" + azureIP + ") is not a valid IP address." + "\n");
                        return false;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Successfully validated CSIP value.");
                        SetupParameters.Instance().AZUREIP = azureIP;
                    }
                }
                else
                {
                    Trc.Log(LogLevel.Always, "AZUREIP was not specified.");
                }
            }
            
            // Validate ProxySettingsFilePath argument .
            // Set ProxyType as Custom if /ProxySettingsFilePath argument is passed.
            string proxySettingsFilePath = "";
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.ProxySettingsFilePath))
            {
                proxySettingsFilePath = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.ProxySettingsFilePath);
                Trc.Log(LogLevel.Always, "ProxySettingsFilePath was specified as {0}", proxySettingsFilePath);

                bool proxySettingsFilePathValResult = ValidationHelper.ValidateProxySettingsFilePath(proxySettingsFilePath);
                if (!proxySettingsFilePathValResult)
                {
                    ConsoleUtils.Log(LogLevel.Error, "Either proxy settings file(" + proxySettingsFilePath + ") does not exist or it's contents are not valid." + "\n");
                    return false;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Successfully validated ProxySettingsFilePath value.");
                    Trc.Log(LogLevel.Always, "Setting ProxyType as Custom.");
                    SetupParameters.Instance().ProxyType = UnifiedSetupConstants.CustomProxy;
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ProxyType, UnifiedSetupConstants.CustomProxy);
                    SetupParameters.Instance().ProxySettingsFilePath = proxySettingsFilePath;
                }
            }
            // Set ProxyType as Bypass if /ProxySettingsFilePath argument is not passed.
            else
            {
                Trc.Log(LogLevel.Always, "ProxySettingsFilePath argument is not passed.Setting ProxyType as Bypass.");
                SetupParameters.Instance().ProxyType = UnifiedSetupConstants.BypassProxy;
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ProxyType, UnifiedSetupConstants.BypassProxy);
            }

            // Validate DataTransferSecurePort if it is passed, regardless of ServerMode parameter.
            string dataTransferSecurePort = "";
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.DataTransferSecurePort))
            {
                dataTransferSecurePort = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.DataTransferSecurePort);
                Trc.Log(LogLevel.Always, "DataTransferSecurePort was specified as {0}.", dataTransferSecurePort);

                bool dataTransferSecurePortValResult = SetupHelper.IsPortValid(dataTransferSecurePort);
                if (!dataTransferSecurePortValResult)
                {
                    ConsoleUtils.Log(LogLevel.Error, "DataTransferSecurePort passed to the installer(" + dataTransferSecurePort + ") is not a valid port." + "\n");
                    return false;
                }
                else
                {
                    if (dataTransferSecurePort == "443")
                    {
                        if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                        {
                            ConsoleUtils.Log(LogLevel.Error, "Port 443 is used to manage replication operations. Select a different port." + "\n");
                            return false;
                        }
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Successfully validated DataTransferSecurePort value.");
                        SetupParameters.Instance().DataTransferSecurePort = dataTransferSecurePort;
                    }
                }
            }
            else
            {
                Trc.Log(LogLevel.Always, "DataTransferSecurePort was not specified.");
            }

            return result;
        }

        /// <summary>
        /// Install/Upgrade all the Unified setup components.
        /// </summary>
        /// <out>sucess/failure return code</out>
        /// <returns>true if all the installations/registrations succeed, false otherwise</returns>
        private static bool InstallComponents(out SetupHelper.SetupReturnValues setupReturnCode)
        {
            bool installStatus = true;
            string privateEndpointState;

            try
            {
                setupReturnCode = SetupHelper.SetupReturnValues.Successful;

                // Fetch all the required information in case of Upgrade/Re-installation.
                if ((SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade) ||
                    (SetupParameters.Instance().ReInstallationStatus == UnifiedSetupConstants.Yes))
                {
                    if (!InstallActionProcessess.FetchInstallationDetails())
                    {
                        ConsoleUtils.Log(
                            LogLevel.Error,
                            "Unable to fetch the desired information from the server. Aborting the installation...");
                        setupReturnCode = SetupHelper.SetupReturnValues.Failed;
                        return false;
                    }
                }

                // Remove unused registry keys during upgrade.
                if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade)
                {
                    InstallActionProcessess.RemoveUnUsedRegistryKeys();
                }

                // Log all the setup parameter values.
                InstallActionProcessess.LogAllSetupParams();

                // Check whether latest CX_TP, CX, DRA, MT, MARS and Configuration Manager are already installed.
                bool isCXTPAlreadyInstalled =
                    InstallActionProcessess.IsCurrentComponentInstalled(
                    UnifiedSetupConstants.CXTPProductName,
                    UnifiedSetupConstants.CXTPExeName);

                bool isCXAlreadyInstalled =
                    InstallActionProcessess.IsCurrentComponentInstalled(
                    UnifiedSetupConstants.CSPSProductName,
                    UnifiedSetupConstants.CSPSExeName);

                bool isMTAlreadyInstalled =
                    (InstallActionProcessess.IsCurrentComponentInstalled(
                        UnifiedSetupConstants.MS_MTProductName,
                        UnifiedSetupConstants.MTExeName) &&
                    ValidationHelper.IsAgentPostInstallActionSuccess());

                bool isDRAAlreadyInstalled =
                    InstallActionProcessess.IsCurrentComponentInstalled(
                    UnifiedSetupConstants.AzureSiteRecoveryProductName,
                    UnifiedSetupConstants.DRAExeName);

                bool isMARSAlreadyInstalled =
                    InstallActionProcessess.IsCurrentComponentInstalled(
                    UnifiedSetupConstants.MARSAgentProductName,
                    UnifiedSetupConstants.MarsAgentExeName);

                bool isConfigManagerAlreadyInstalled =
                     InstallActionProcessess.IsCurrentComponentInstalled(
                     UnifiedSetupConstants.ConfigManagerProductName,
                     UnifiedSetupConstants.ConfigManagerMsiName);

                if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall)
                {
                    if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                    {
                        if (!SetupParameters.Instance().SkipRegistration)
                        {
                            // Parse command line arguments.
                            // Get command line arguments.
                            String[] arguments = Environment.GetCommandLineArgs();
                            CommandlineParameters commandlineParameters = new CommandlineParameters();
                            commandlineParameters.ParseCommandline(arguments);

                            string vaultCredsFilePath = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.VaultCredsFilePath);

                            /// Populate property bag with appropriate values from vault creds file and installs management certificate.
                            DRSetupFramework.SettingsFileHelper.ProcessSettingsFile(vaultCredsFilePath);
                            privateEndpointState =
                                DRSetupFramework.PropertyBagDictionary.Instance.GetProperty<string>(
                                    DRSetupFramework.PropertyBagConstants.PrivateEndpointState);
                        }
                    }
                    else if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
                    {
                        InstallActionProcessess.FetchAcsUrlGeoName();
                        privateEndpointState =
                            DRSetupFramework.PropertyBagDictionary.Instance.GetProperty<string>(
                                ASRSetupFramework.PropertyBagConstants.PrivateEndpointState);
                    }
                }

                // Download MySQL.
                if ((SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode) &&
                    ((SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall) || ((SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade) && (PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.MySQL55Installed)))))
                {
                    ConsoleUtils.Log(LogLevel.Always, "");


                    if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade)
                    {
                        SetupHelper.GetProxyDetails();
                    }

                    if ((PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.MySQL55Installed)) && (SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade))
                    {
                        ConsoleUtils.Log(LogLevel.Always, "MySQL 5.5.37 is already installed on the server. Downloading MySQL 5.5.37 setup file to uninstall 5.5.37.");
                        if (InstallActionProcessess.DownloadMySQLSetupFile(UnifiedSetupConstants.MySQL55Version))
                        {
                            ConsoleUtils.Log(
                                LogLevel.Always,
                                "MySQL 5.5.37 setup file is downloaded successfully.");
                        }
                        else
                        {
                            ConsoleUtils.Log(
                                LogLevel.Error,
                                "Failed to download MySQL 5.5.37 setup file. Aborting the installation...");
                            SetupParameters.Instance().IsMySQLDownloaded = false;
                            InstallActionProcessess.FailureRecordOperation(
                                IntegrityCheck.ExecutionScenario.Installation,
                                IntegrityCheck.OperationName.DownloadMySql,
                                string.Format(
                                    StringResources.RecordOperationFailureMessage,
                                    -1),
                                string.Format(
                                StringResources.RecordOperationRecommendationMessage,
                                sysDrive + UnifiedSetupConstants.ASRUnifiedSetupLog));
                            setupReturnCode = SetupHelper.SetupReturnValues.Failed;
                            return false;
                        }
                    }

                    if (SetupHelper.IsMySQLInstalled(UnifiedSetupConstants.MySQL57Version))
                    {
                        ConsoleUtils.Log(LogLevel.Always, "MySQL is already installed on the server.");
                        SetupParameters.Instance().IsMySQLDownloaded = true;
                        InstallActionProcessess.SucessRecordOperation(
                            IntegrityCheck.ExecutionScenario.Installation,
                            IntegrityCheck.OperationName.DownloadMySql);
                    }
                    else
                    {
                        if (InstallActionProcessess.IsPrivateEndpointStateEnabled())
                        {
                            ConsoleUtils.Log(
                                   LogLevel.Always,
                                   "Private endpoint is enabled.");

                            string MySQLInstallerPath = Path.Combine(Path.GetPathRoot(Environment.SystemDirectory), UnifiedSetupConstants.MySQLFileName);
                            if (System.IO.File.Exists(MySQLInstallerPath))
                            {
                                ConsoleUtils.Log(
                                    LogLevel.Always,
                                    string.Format(
                                    "MySQL 5.7.20 file exists on {0}. Proceeding with installation.",
                                    MySQLInstallerPath));
                                SetupParameters.Instance().IsMySQLDownloaded = true;
                                InstallActionProcessess.SucessRecordOperation(
                                    IntegrityCheck.ExecutionScenario.Installation,
                                    IntegrityCheck.OperationName.DownloadMySql);
                            }
                            else
                            {
                                // When private endpoint is enabled we do not download MySQL.
                                ConsoleUtils.Log(
                                        LogLevel.Error,
                                        StringResources.MySQLManualInstallText);

                                SetupParameters.Instance().IsMySQLDownloaded = false;
                                InstallActionProcessess.FailureRecordOperation(
                                    IntegrityCheck.ExecutionScenario.Installation,
                                    IntegrityCheck.OperationName.DownloadMySql,
                                    string.Format(
                                        StringResources.RecordOperationFailureMessage,
                                        -1),
                                    string.Format(
                                    StringResources.RecordOperationRecommendationMessage,
                                    sysDrive + UnifiedSetupConstants.ASRUnifiedSetupLog));
                                return false;
                            }
                        }
                        else
                        {
                            ConsoleUtils.Log(
                                LogLevel.Always,
                                "MySQL is not installed on the server. Downloading the setup file...");

                            if (InstallActionProcessess.DownloadMySQLSetupFile(UnifiedSetupConstants.MySQL57Version))
                            {
                                ConsoleUtils.Log(
                                    LogLevel.Always,
                                    "MySQL setup file is downloaded successfully.");
                                SetupParameters.Instance().IsMySQLDownloaded = true;
                                InstallActionProcessess.SucessRecordOperation(
                                    IntegrityCheck.ExecutionScenario.Installation,
                                    IntegrityCheck.OperationName.DownloadMySql);
                            }
                            else
                            {
                                ConsoleUtils.Log(
                                    LogLevel.Error,
                                    "Failed to download MySQL setup file. Aborting the installation...");
                                SetupParameters.Instance().IsMySQLDownloaded = false;
                                InstallActionProcessess.FailureRecordOperation(
                                    IntegrityCheck.ExecutionScenario.Installation,
                                    IntegrityCheck.OperationName.DownloadMySql,
                                    string.Format(
                                        StringResources.RecordOperationFailureMessage,
                                        -1),
                                    string.Format(
                                    StringResources.RecordOperationRecommendationMessage,
                                    sysDrive + UnifiedSetupConstants.ASRUnifiedSetupLog));
                                setupReturnCode = SetupHelper.SetupReturnValues.Failed;
                                return false;
                            }
                        }
                    }
                }

                ConsoleUtils.Log(LogLevel.Always, "");

                //Install Internet Information Services
                if ((SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode) &&
                    (SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall))
                {
                    if (InstallActionProcessess.IsIISInstalled())
                    {
                        ConsoleUtils.Log(LogLevel.Always, "Internet Information Services is already installed.");
                    }
                    else
                    {
                        // Update console that the IIS installation is in progress.
                        ConsoleUtils.Log
                            (LogLevel.Always,
                            StringResources.InstallationProgressCurrentOperationIISText);

                        // Initiate IIS installation.
                        int iisInstallStatus = InstallActionProcessess.InstallIIS();
                        if (iisInstallStatus == 0)
                        {
                            ConsoleUtils.Log(LogLevel.Always, "Internet Information Services is installed successfully.");
                        }
                        else if (iisInstallStatus == 3010)
                        {
                            ConsoleUtils.Log(LogLevel.Error, "Internet Information Services installation requires the computer to be rebooted. Reboot the machine and try install again.");
                            setupReturnCode = SetupHelper.SetupReturnValues.Failed;
                            return false;
                        }
                        else
                        {
                            ConsoleUtils.Log(LogLevel.Error, "Internet Information Services installation has failed. Aborting the installation...");
                            setupReturnCode = SetupHelper.SetupReturnValues.Failed;
                            return false;
                        }
                    }
                }

                ConsoleUtils.Log(LogLevel.Always, "");

                // Install CXTP.
                if (isCXTPAlreadyInstalled)
                {
                    ConsoleUtils.Log(
                        LogLevel.Always,
                        "Latest version of Configuration Server/Process Server Dependencies component is already installed on the server.");
                    InstallActionProcessess.SkipRecordOperation(
                        IntegrityCheck.ExecutionScenario.Installation,
                        IntegrityCheck.OperationName.InstallTp,
                        StringResources.ComponentAlreadyInstalled);
                    SetupParameters.Instance().IsCXTPInstalled = true;
                }
                else
                {
                    Trc.Log(
                        LogLevel.Always,
                        "Latest version of Configuration Server/Process Server Dependencies component is not installed on the server.");

                    // Update console that the CX_TP installation is in progress.
                    ConsoleUtils.Log
                        (LogLevel.Always,
                        StringResources.InstallationProgressCurrentOperationTPText);

                    // Initiate CX_TP installation.
                    if (InstallActionProcessess.InstallCXTP() == 0)
                    {
                        ConsoleUtils.Log(
                            LogLevel.Always,
                            "Configuration Server/Process Server Dependencies component is installed successfully.");
                    }
                    else
                    {
                        ConsoleUtils.Log(LogLevel.Error, "Configuration Server/Process Server Dependencies component installation has failed. Aborting the installation...");
                        setupReturnCode = SetupHelper.SetupReturnValues.Failed;
                        return false;
                    }
                }

                ConsoleUtils.Log(LogLevel.Always, "");

                // Install CX.
                if (isCXAlreadyInstalled)
                {
                    ConsoleUtils.Log(LogLevel.Always, "Latest version of Configuration Server/Process Server component is already installed on the server.");
                    SetupParameters.Instance().IsCSInstalled = true;

                    if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                    {
                        InstallActionProcessess.SkipRecordOperation(
                            IntegrityCheck.ExecutionScenario.Installation,
                            IntegrityCheck.OperationName.InstallCs,
                            StringResources.ComponentAlreadyInstalled);

                        InstallActionProcessess.SkipRecordOperation(
                            IntegrityCheck.ExecutionScenario.Installation,
                            IntegrityCheck.OperationName.InstallPs,
                            StringResources.ComponentAlreadyInstalled);
                    }
                    else
                    {
                        InstallActionProcessess.SkipRecordOperation(
                            IntegrityCheck.ExecutionScenario.Installation,
                            IntegrityCheck.OperationName.InstallPs,
                            StringResources.ComponentAlreadyInstalled);
                    }
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Latest version of Configuration Server/Process Server component is not installed on the server.");

                    string componentName = "";

                    // Update console that the CX installation is in progress.
                    if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                    {
                        ConsoleUtils.Log(LogLevel.Always, StringResources.InstallationProgressCurrentOperationCSPSText);
                        componentName = "Configuration Server/Process Server";
                    }
                    else if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
                    {
                        ConsoleUtils.Log(LogLevel.Always, StringResources.InstallationProgressCurrentOperationPSText);
                        componentName = "Process Server";
                    }

                    int cxInstallStatus = -1;
                    switch (SetupParameters.Instance().InstallType)
                    {
                        case UnifiedSetupConstants.FreshInstall:
                            Trc.Log(LogLevel.Always, "Invoking InstallCX.");
                            cxInstallStatus = InstallActionProcessess.InstallCX();
                            break;
                        case UnifiedSetupConstants.Upgrade:
                            Trc.Log(LogLevel.Always, "Invoking UpgradeCX.");
                            cxInstallStatus = InstallActionProcessess.UpgradeCX();
                            break;
                    }

                    if (cxInstallStatus == 0)
                    {
                        ConsoleUtils.Log(LogLevel.Always, "" + componentName + " component is installed successfully.");
                    }
                    else
                    {
                        ConsoleUtils.Log(LogLevel.Error, "" + componentName + " component installation has failed. Aborting the installation...");
                        setupReturnCode = SetupHelper.SetupReturnValues.Failed;
                        return false;
                    }
                }


                // Install DRA only in case of CS installation.
                if ((SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode) &&
                    (!SetupParameters.Instance().PSGalleryImage))
                {
                    ConsoleUtils.Log(LogLevel.Always, "");
                    if (isDRAAlreadyInstalled)
                    {
                        ConsoleUtils.Log(LogLevel.Always, "Latest version of Microsoft Azure Site Recovery Provider is already installed on the server.");
                        InstallActionProcessess.SkipRecordOperation(
                            IntegrityCheck.ExecutionScenario.Installation,
                            IntegrityCheck.OperationName.InstallDra,
                            StringResources.ComponentAlreadyInstalled);
                        SetupParameters.Instance().IsDRAInstalled = true;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Latest version of Microsoft Azure Site Recovery Provider is not installed on the server.");

                        // Update console that the DRA installation is in progress.
                        ConsoleUtils.Log(LogLevel.Always, StringResources.InstallationProgressCurrentOperationDRAText);

                        if (InstallActionProcessess.InstallDRA() == 0)
                        {
                            ConsoleUtils.Log(LogLevel.Always, "Microsoft Azure Site Recovery Provider is installed successfully.");
                        }
                        else
                        {
                            ConsoleUtils.Log(LogLevel.Error, "Microsoft Azure Site Recovery Provider installation has failed. Aborting the installation...");
                            setupReturnCode = SetupHelper.SetupReturnValues.DRAInstallationFailed;
                            return false;
                        }
                    }
                }

                ConsoleUtils.Log(LogLevel.Always, "");

                // Install Master target.
                if ((!SetupParameters.Instance().PSGalleryImage))
                {
                    if (isMTAlreadyInstalled)
                    {
                        ConsoleUtils.Log(LogLevel.Always, "Latest version of Master target is already installed on the server.");
                        InstallActionProcessess.SkipRecordOperation(
                            IntegrityCheck.ExecutionScenario.Installation,
                            IntegrityCheck.OperationName.InstallMt,
                            StringResources.ComponentAlreadyInstalled);
                        SetupParameters.Instance().IsMTInstalled = true;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Latest version of Master target is not installed on the server.");

                        // Update console that the MT installation is in progress.
                        ConsoleUtils.Log(LogLevel.Always, StringResources.InstallationProgressCurrentOperationMTText);

                        int mtInstallStatus = InstallActionProcessess.InstallMT();

                        if ((mtInstallStatus == 0) || (mtInstallStatus == 3010) || (mtInstallStatus == 1641) || (mtInstallStatus == 98))
                        {
                            ConsoleUtils.Log(LogLevel.Always, "Master Target component is installed successfully.");

                            // Set Rebootrequired value to Yes if Unified agent returns 1641/3010/98 exit code.
                            if ((mtInstallStatus == 3010) || (mtInstallStatus == 1641) || (mtInstallStatus == 98))
                            {
                                SetupParameters.Instance().RebootRequired = UnifiedSetupConstants.Yes;
                                Trc.Log(LogLevel.Always, "Reboot required parameter is set to : {0}", SetupParameters.Instance().RebootRequired);
                            }
                        }
                        else
                        {
                            ConsoleUtils.Log(LogLevel.Error, "Master Target installation has failed. Aborting the installation...");
                            setupReturnCode = SetupHelper.SetupReturnValues.Failed;
                            return false;
                        }
                    }
                }
                ConsoleUtils.Log(LogLevel.Always, "");

                // Configure Master target.
                if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall)
                {
                    if (InstallActionProcessess.isMTConfigured())
                    {
                        Trc.Log(LogLevel.Always, "MT is already configured on the server.");
                        InstallActionProcessess.SkipRecordOperation(
                            IntegrityCheck.ExecutionScenario.Configuration,
                            IntegrityCheck.OperationName.ConfigureMt,
                            StringResources.ComponentAlreadyInstalled);
                        SetupParameters.Instance().IsMTConfigured = true;
                    }
                    else
                    {
                        int mtConfigStatus = InstallActionProcessess.ConfigureMT();
                        if (mtConfigStatus == 0)
                        {
                            ConsoleUtils.Log(LogLevel.Always, "Master Target configuration is performed successfully.");
                        }
                        else
                        {
                            ConsoleUtils.Log(LogLevel.Always, "Failed to configure Master Target. Aborting the installation...");
                            setupReturnCode = SetupHelper.SetupReturnValues.Failed;
                            return false;
                        }
                    }
                }

                // Install MARS agent.
                if ((!SetupParameters.Instance().PSGalleryImage))
                {
                    if (isMARSAlreadyInstalled)
                    {
                        Trc.Log(LogLevel.Always, "Latest version of Microsoft Azure Recovery Services Agent is already installed on the server.");
                        InstallActionProcessess.SkipRecordOperation(
                            IntegrityCheck.ExecutionScenario.Installation,
                            IntegrityCheck.OperationName.InstallMars,
                            StringResources.ComponentAlreadyInstalled);
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Latest version of Microsoft Azure Recovery Services Agent is not installed on the server.");

                        // Update console that the MARS agent installation is in progress.
                        ConsoleUtils.Log(LogLevel.Always, StringResources.InstallationProgressCurrentOperationMARSText);

                        if (InstallActionProcessess.InstallMARS() == 0)
                        {
                            // MARS service sometimes take 10 secs to be in running state.
                            Thread.Sleep(10000);

                            ConsoleUtils.Log(LogLevel.Always, "Microsoft Azure Recovery Services Agent is installed successfully.");
                        }
                        else
                        {
                            ConsoleUtils.Log(LogLevel.Error, "Microsoft Azure Recovery Services Agent installation has failed. Aborting the installation...");
                            setupReturnCode = SetupHelper.SetupReturnValues.MARSAgentInstallationFailed;
                            return false;
                        }
                    }
                }

                ConsoleUtils.Log(LogLevel.Always, "");

                // Install Configuration Manager.
                if ((SetupParameters.Instance().OVFImage))
                {
                    if (isConfigManagerAlreadyInstalled)
                    {
                        ConsoleUtils.Log(LogLevel.Always, "Latest version of Configuration Manager is already installed on the server.");
                        InstallActionProcessess.SkipRecordOperation(
                            IntegrityCheck.ExecutionScenario.Installation,
                            IntegrityCheck.OperationName.InstallConfigManager,
                            StringResources.ComponentAlreadyInstalled);
                        SetupParameters.Instance().IsConfigManagerInstalled = true;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Latest version of Configuration Manager is not installed on the server.");

                        // Update console that the Configuration Manager installation is in progress.
                        ConsoleUtils.Log(LogLevel.Always, StringResources.InstallationProgressCurrentOperationConfigManagerText);

                        int configManagerInstallStatus = InstallActionProcessess.InstallConfigurationManager();

                        if (configManagerInstallStatus == 0)
                        {
                            ConsoleUtils.Log(LogLevel.Always, "Configuration Manager component is installed successfully.");
                        }
                        else
                        {
                            ConsoleUtils.Log(LogLevel.Error, "Configuration Manager installation has failed. Aborting the installation...");
                            setupReturnCode = SetupHelper.SetupReturnValues.ConfigManagerInstallationFailed;
                            return false;
                        }
                    }
                }

                // Components registration.
                if ((SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall) &&
                    (!SetupParameters.Instance().PSGalleryImage))
                {
                    ConsoleUtils.Log(LogLevel.Always, "");

                    // Update console that the components registration is in progress.
                    ConsoleUtils.Log(LogLevel.Always, StringResources.InstallationProgressCurrentOperationRegistrationText);

                    bool registrationStatus = false;
                    if (SetupParameters.Instance().SkipRegistration)
                    {
                        Trc.Log(LogLevel.Always, "Skipping registration as /SkipRegistration argument is passed to the installer.");
                    }
                    else
                    {
                        if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                        {
                            registrationStatus = InstallActionProcessess.CSComponentsRegistration(out setupReturnCode);
                        }
                        else
                        {
                            registrationStatus = InstallActionProcessess.PSComponentsRegistration(out setupReturnCode);
                        }

                        if (registrationStatus)
                        {
                            ConsoleUtils.Log(LogLevel.Always, "Successfully registered with Site Recovery.");
                        }
                        else
                        {
                            ConsoleUtils.Log(LogLevel.Error, "Failed to register with Site Recovery. Aborting the installation...");
                            return false;
                        }
                    }
                }
                ConsoleUtils.Log(LogLevel.Always, "");

                return installStatus;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Error, "Exception occurred while installing components: {0}", e.ToString());
                setupReturnCode = SetupHelper.SetupReturnValues.Failed;
                return false;
            }
        }

        /// <summary>
        /// Check for all pre-requisites.
        /// </summary>
        /// <returns>ture if all pre-requisites are met, otherwise false</returns>
        private static bool CheckForPreRequisites()
        {
            bool result = true;

            if (SetupParameters.Instance().SkipPrereqsCheck)
            {
                Trc.Log(LogLevel.Always, "Skipping pre-requisites check as /SkipPrereqsCheck argument is passed to the installer.");
            }
            else
            {
                List<EvaluateOperations.Prerequisites> fabricPrereqs = EvaluateOperations.GetPrereqs();

                foreach (EvaluateOperations.Prerequisites prereq in fabricPrereqs)
                {
                    if (prereq == EvaluateOperations.Prerequisites.IsFreeSpaceAvailable &&
                        SetupParameters.Instance().SkipSpaceCheck == "true")
                    {
                        Trc.Log(LogLevel.Always, "Skipping space check as /SkipSpaceCheck argument is passed to the installer.");
                        continue;
                    }

                    EvaluateOperations.UiPrereqMappings[prereq].ForEach(validation =>
                    {
                        IntegrityCheck.Response validationResponse = EvaluateOperations.EvaluatePrereq(validation);
                        IntegrityCheck.IntegrityCheckWrapper.RecordOperation(
                            IntegrityCheck.ExecutionScenario.PreInstallation,
                            EvaluateOperations.ValidationMappings[validation],
                            validationResponse.Result,
                            validationResponse.Message,
                            validationResponse.Causes,
                            validationResponse.Recommendation,
                            validationResponse.Exception
                            );

                        if (validationResponse.Result == IntegrityCheck.OperationResult.Success)
                        {
                            Trc.Log(LogLevel.Always, validationResponse.Message);
                        }
                        else if (validationResponse.Result == IntegrityCheck.OperationResult.Warn)
                        {
                            Trc.Log(LogLevel.Always, validationResponse.Message);
                        }
                        else if (validationResponse.Result == IntegrityCheck.OperationResult.Skip)
                        {
                            Trc.Log(LogLevel.Always, validationResponse.Message);
                        }
                        else
                        {
                            Trc.Log(
                                LogLevel.Error, "{0}{1}{2}",
                                validationResponse.Message,
                                Environment.NewLine,
                                validationResponse.Exception);
                            ConsoleUtils.Log(LogLevel.Error, "Pre-requisite check has failed.");
                            ConsoleUtils.Log(LogLevel.Error, validationResponse.Message);
                            ConsoleUtils.Log(LogLevel.Error, validationResponse.Causes);
                            ConsoleUtils.Log(LogLevel.Error, validationResponse.Recommendation);
                            result = false;
                        }
                    });
                }
            }

            return result;
        }

        /// <summary>
        /// Files to be removed during crash.
        /// </summary>
        private static void DeleteSetupCreatedFiles()
        {
            try
            {
                string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);

                // Remove setup created files
                ValidationHelper.DeleteFile(sysDrive + UnifiedSetupConstants.ProxySettingsFile);
                ValidationHelper.DeleteFile(sysDrive + UnifiedSetupConstants.PassphraseFile);
                ValidationHelper.DeleteFile(SetupHelper.SetLogFilePath(UnifiedSetupConstants.PrerequisiteStatusFileName));
                ValidationHelper.DeleteFile(sysDrive + UnifiedSetupConstants.MySQLCredentialsFile);
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred while deleting setup created files: {0}", e.ToString());
            }
        }

        /// <summary>
        /// Set default property bag properties
        /// </summary>
        private static void SetDefaultPropertyValues()
        {
            // Setup start time
            PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.SetupStartTime, DateTime.Now);

            // Default log file where all Microsoft Azure Site Recovery Provider logging will take place
            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.DefaultLogName,
                SetupHelper.SetLogFilePath(UnifiedSetupConstants.UnifiedSetupWizardLogFileName));

            // Logs related to IntegrityCheck library.
            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.IntegrityCheckLogName,
                SetupHelper.SetLogFilePath(UnifiedSetupConstants.IntegrityCheckFileName));

            // Add the current path to the propertybag
            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagDictionary.SetupExePath,
                (new System.IO.FileInfo(
                    System.Reflection.Assembly.GetExecutingAssembly().Location)).DirectoryName);

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.WizardTitle,
                StringResources.SetupMessageBoxTitle);

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.CanClose,
                false);

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.InstallationFinished,
                false);

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.InstallationAborted,
                false);

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.UnattendedUpgradeRunning,
                false);

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.RebootDrServiceAtEnd,
                false);

            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.DRAUTCLogName,
                string.Format(
                    UnifiedSetupConstants.SetupWizardLogFileNameFormat,
                    DateTime.UtcNow.Ticks));
        }

        /// <summary>
        /// Logs everything at the end of the application
        /// </summary>
        /// <param name="returnValue">The application's return value</param>
        private static void EndOfAppDataLogging(SetupHelper.SetupReturnValues returnValue)
        {
            // Logs the property bag
            Trc.Log(LogLevel.Always, "Application Ended");
            Trc.Log(LogLevel.Always, "Exit code returned by unified setup: {0}({1})", (int)returnValue, returnValue.ToString());
            DRSetupFramework.Trc.Log(DRSetupFramework.LogLevel.Always, "IntegrityCheck Ended");

            SetupHelper.LogPropertyBag();
        }

        /// <summary>
        /// Services to be validated post installation
        /// </summary>
        private static bool ServiceChecks()
        {
            bool result = true;
            IntegrityCheck.Response validationResponse;

            // Get the operation summary for the Installation to validate service checks
            List<IntegrityCheck.OperationDetails> installOperationDetails =
                IntegrityCheck.IntegrityCheckWrapper.GetOperationSummary().OperationDetails.Where(
                o => o.ScenarioName == IntegrityCheck.ExecutionScenario.Installation).ToList();

            // Get the list of services
            List<IntegrityCheck.Validations> serviceValidations =
                EvaluateOperations.GetServiceChecks(SetupParameters.Instance().ServerMode);

            foreach (IntegrityCheck.Validations validation in serviceValidations)
            {
                // Trying to find whether the install operation is done for the specific component
                // and this will be done only once.
                List<IntegrityCheck.OperationDetails> operationList = installOperationDetails.Where(
                    o => o.OperationName == EvaluateOperations.ServiceToOperationMappings[validation].ToString()).ToList();

                if (validation == IntegrityCheck.Validations.IsDraServiceRunning)
                {
                    if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall)
                    {
                        bool isDRARegistered = SetupParameters.Instance().IsDRARegistered;
                        if (!isDRARegistered)
                        {
                            Trc.Log(LogLevel.Always, "Skipping DRA service check");
                            continue;
                        }
                    }
                }

                if ((validation == IntegrityCheck.Validations.IsVxAgentServiceRunning) || (validation == IntegrityCheck.Validations.IsScoutApplicationServiceRunning))
                {
                    if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall)
                    {
                        bool isMTConfigured = SetupParameters.Instance().IsMTConfigured;
                        if (!isMTConfigured)
                        {
                            Trc.Log(LogLevel.Always, "Skipping {0} check.", validation);
                            continue;
                        }
                    }
                }

                if (operationList.Count != 0 && operationList.Single().Result == IntegrityCheck.OperationResult.Success)
                {
                    validationResponse = EvaluateOperations.EvaluateServiceChecks(validation);
                    IntegrityCheck.IntegrityCheckWrapper.RecordOperation(
                        IntegrityCheck.ExecutionScenario.PostInstallation,
                        EvaluateOperations.ValidationMappings[validation],
                        validationResponse.Result,
                        validationResponse.Message,
                        validationResponse.Causes,
                        validationResponse.Recommendation,
                        validationResponse.Exception);

                    if (validationResponse.Result == IntegrityCheck.OperationResult.Failed)
                    {
                        ConsoleUtils.Log(LogLevel.Error, validationResponse.Message);
                        result = false;
                    }
                }

                // Sleep for 1 sec during multiple service checks.
                Thread.Sleep(1000);
            }

            return result;
        }
    }
}