﻿using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Threading;
using System.Windows;
using System.Windows.Documents;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using ASRResources;
using ASRSetupFramework;
using IntegrityCheck = Microsoft.DisasterRecovery.IntegrityCheck;
using Microsoft.Win32;
using DRSetupFramework = Microsoft.DisasterRecovery.SetupFramework;

namespace UnifiedSetup
{
    /// <summary>
    /// Interaction logic for InstallationProgress.xaml
    /// </summary>
    public partial class InstallationProgress : BasePageForWpfControls
    {
        public InstallationProgress(ASRSetupFramework.Page page)
            : base(page, StringResources.installation_progress, 4)
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="InstallationProgress"/> class.
        /// </summary>
        public InstallationProgress()
        {
            InitializeComponent();
        }

        # region Delegates
        /// <summary>
        /// Delegate to install all the components.
        /// </summary>
        private delegate void InstallComponentsDelegate();

        /// <summary>
        /// Delegate to update UI.
        /// </summary>
        private delegate void UIUpdateDelegate();
        # endregion

        # region Fields
        /// <summary>
        /// Stores the system directory.
        /// </summary>
        public string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);

        /// <summary>
        /// Stores installation status of components.
        /// </summary>
        private static Dictionary<ComponentType, InstallationStatus> ComponentTypeInstallationStatus;
        # endregion

        # region Enums
        /// <summary>
        /// Various components
        /// </summary>
        public enum ComponentType
        {
            /// <summary>
            /// MySQL
            /// </summary>
            MySQL,

            /// <summary>
            /// Internet Information Services
            /// </summary>
            IIS,

            /// <summary>
            /// Thirdparty components
            /// </summary>
            CXTP,

            /// <summary>
            /// Configuration/process server
            /// </summary>
            CX,

            /// <summary>
            /// Microsoft Azure Site Recovery Provider
            /// </summary>
            DRA,

            /// <summary>
            /// Install Master target
            /// </summary>
            InstallMT,

            /// <summary>
            /// Configure Master target
            /// </summary>
            ConfigureMt,

            /// <summary>
            /// Microsoft Azure Recovery Services Agent
            /// </summary>
            MARS,

            /// <summary>
            /// Azure Site Recovery Configuration Manager
            /// </summary>
            ConfigurationManager,

            /// <summary>
            /// Components registration
            /// </summary>
            Registration
        }

        /// <summary>
        /// Status of installation
        /// </summary>
        public enum InstallationStatus
        {
            /// <summary>
            /// Installation Succeeded
            /// </summary>
            Success,

            /// <summary>
            /// Installation Failed
            /// </summary>
            Failure,

            /// <summary>
            /// Installation Failed
            /// </summary>
            Skip,

            /// <summary>
            /// Checking installation
            /// </summary>
            Checking

        }
        #endregion

        /// <summary>
        /// Enters the page.
        /// </summary>
        public override void EnterPage()
        {
            base.EnterPage();

            Trc.Log(LogLevel.Always, "Disabling sidebar navigation and finish button.");

            // Disable global navigation
            this.Page.Host.DisableGlobalSideBarNavigation();

            // Hide Previous button and disable Next and Cancel buttons
            this.Page.Host.SetPreviousButtonState(false, false);
            this.Page.Host.SetNextButtonState(false, false);
            this.Page.Host.SetCancelButtonState(true, false, StringResources.FinishButtonText);

            // Add component installation status as checking to dictionary.
            ComponentTypeInstallationStatus = new Dictionary<ComponentType, InstallationStatus>();
            ComponentTypeInstallationStatus.Add(ComponentType.MySQL, InstallationStatus.Checking);
            ComponentTypeInstallationStatus.Add(ComponentType.IIS, InstallationStatus.Checking);
            ComponentTypeInstallationStatus.Add(ComponentType.CXTP, InstallationStatus.Checking);
            ComponentTypeInstallationStatus.Add(ComponentType.CX, InstallationStatus.Checking);
            ComponentTypeInstallationStatus.Add(ComponentType.DRA, InstallationStatus.Checking);
            ComponentTypeInstallationStatus.Add(ComponentType.InstallMT, InstallationStatus.Checking);
            ComponentTypeInstallationStatus.Add(ComponentType.ConfigureMt, InstallationStatus.Checking);
            ComponentTypeInstallationStatus.Add(ComponentType.MARS, InstallationStatus.Checking);
            ComponentTypeInstallationStatus.Add(ComponentType.ConfigurationManager, InstallationStatus.Checking);
            ComponentTypeInstallationStatus.Add(ComponentType.Registration, InstallationStatus.Checking);

            /// <summary>
            /// Update UI based on Fresh/Upgrade installation.
            /// </summary>

            // Upgrade
            if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade)
            {
                Trc.Log(LogLevel.Always, "UI changes: Upgrade");
                ChangesDuringUpgrade();
            }
            // Fresh installation.
            else
            {
                Trc.Log(LogLevel.Always, "UI changes: Fresh");

                // Reinstallation scenario.
                if (SetupParameters.Instance().ReInstallationStatus == UnifiedSetupConstants.Yes)
                {
                    // Fetch all the required installation information.
                    InstallActionProcessess.FetchInstallationDetails();
                }

                FreshInstallUIChanges();
            }

            this.InstallationProgressCurrentOperationTextBlock.Visibility = System.Windows.Visibility.Visible;
            this.InstallationShowProgress.Visibility = System.Windows.Visibility.Visible;

            this.Dispatcher.BeginInvoke(DispatcherPriority.Background, new InstallComponentsDelegate(this.TriggerInstallationInNewThread));
        }

        /// <summary>
        /// Trigger installation in a new non-UI thread.
        /// </summary>
        public void TriggerInstallationInNewThread()
        {
            ThreadPool.QueueUserWorkItem(new WaitCallback(this.InstallComponents));
        }

        /// <summary>
        /// Overrides OnNext.
        /// </summary>
        /// <returns>True if next button is to be shown, false otherwise.</returns>
        public override bool OnNext()
        {
            return this.ValidatePage(PageNavigation.Navigation.Next);
        }

        /// <summary>
        /// Exits the page.
        /// </summary>
        public override void ExitPage()
        {
            base.ExitPage();
        }

        /// <summary>
        /// Actions performed on Finish button click .
        /// </summary>
        public override bool OnCancel()
        {
            try
            {
                // Upgrade case - Show reboot required message if Master target returns 3010 exit code.
                if ((SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade) &&
                    (SetupParameters.Instance().RebootRequired == UnifiedSetupConstants.Yes))
                {
                    Trc.Log(LogLevel.Always, "Upgrade reboot case.");
                    SetupHelper.ShowError(StringResources.PostInstallationRebootRequired);
                }
                // Fresh installation
                else if ((SetupParameters.Instance().InstallStatus == UnifiedSetupConstants.SuccessStatus) &&
                    (SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall))
                {
                    // Dispaly a message box, if post installation reboot is required.
                    if ((ValidationHelper.PostAgentInstallationRebootRequired()) ||
                        (SetupParameters.Instance().RebootRequired == UnifiedSetupConstants.Yes))
                    {
                        SetupHelper.ShowError(StringResources.PostInstallationRebootRequired);
                    }

                    // Passphrase popup is launched only in case of Configuration Server installation.
                    if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                    {
                        // Show Passphrase.
                        string passphrasefile = sysDrive + UnifiedSetupConstants.PassphrasePath;
                        var passphrase = File.ReadAllText(passphrasefile);
                        var msg = "Configuration Server Connection Passphrase: " + passphrase + Environment.NewLine + "You will be asked for the connection passphrase when installing agents on source and target environment." + Environment.NewLine + Environment.NewLine + "Do you want to copy passphrase to clipboard?";
                        if (File.Exists(passphrasefile))
                        {
                            if (MessageBox.Show(msg, StringResources.PassphrasePopupTitle, MessageBoxButton.YesNo, MessageBoxImage.Information) == MessageBoxResult.Yes)
                            {
                                Trc.Log(LogLevel.Always, "Passphrase is copied to clipboard");
                                Clipboard.SetText(passphrase);
                            }
                            else
                            {
                                Trc.Log(LogLevel.Always, "Passphrase is not copied to clipboard");
                            }
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "Passphrase file " + passphrasefile + " is not available.");
                        }

                        // Launching Account Management Dialog.
                        Trc.Log(LogLevel.Always, "Launching Account Management Dialog.");
                        Process.Start(
                            Path.Combine(
                            SetupParameters.Instance().CXInstallDir,
                            UnifiedSetupConstants.CspsconfigtoolExePath));
                    }
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Error, "Exception occurred while cancel button click: {0}", e.ToString());
            }

            return true;
        }

        #region ComponentInstallationMethods
        /// <summary>
        /// Install/Upgrade all the Unified setup components.
        /// </summary>
        public void InstallComponents(object o)
        {
            try
            {
                // Log all the setup parameter values.
                InstallActionProcessess.LogAllSetupParams();

                // Check whether CX_TP, CX, DRA, MT, MARS and ConfigurationManager are already installed.
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

                // Download MySQL.
                if ((SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode) && 
                    ((SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall) || 
                    ((SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade) && 
                    (PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.MySQL55Installed)))))
                {
                    if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade)
                    {
                        SetupHelper.GetProxyDetails();
                    }

                    if (SetupHelper.IsMySQLInstalled(UnifiedSetupConstants.MySQL57Version))
                    {
                        Trc.Log(LogLevel.Always, "MySQL is already installed on the server.");
                        this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MySQLAlreadyInstalled));
                        SetupParameters.Instance().IsMySQLDownloaded = true;
                        ComponentTypeInstallationStatus[ComponentType.MySQL] = InstallationStatus.Success;
                        InstallActionProcessess.SucessRecordOperation(
                            IntegrityCheck.ExecutionScenario.Installation,
                            IntegrityCheck.OperationName.DownloadMySql);
                    }
                    else
                    {
                        if (InstallActionProcessess.IsPrivateEndpointStateEnabled())
                        {
                            Trc.Log(
                                    LogLevel.Always,
                                    "Private endpoint is enabled.");

                            string MySQLInstallerPath = Path.Combine(Path.GetPathRoot(Environment.SystemDirectory), UnifiedSetupConstants.MySQLFileName);
                            if (System.IO.File.Exists(MySQLInstallerPath))
                            {
                                Trc.Log(
                                    LogLevel.Always,
                                    string.Format(
                                    "MySQL 5.7.20 file exists on {0}. Proceeding with installation.",
                                    MySQLInstallerPath));
                                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MySQLSuccess));
                                SetupParameters.Instance().IsMySQLDownloaded = true;
                                ComponentTypeInstallationStatus[ComponentType.MySQL] = InstallationStatus.Skip;
                                InstallActionProcessess.SucessRecordOperation(
                                    IntegrityCheck.ExecutionScenario.Installation,
                                    IntegrityCheck.OperationName.DownloadMySql);
                            }
                            else
                            {
                                // When private endpoint is enabled we do not download MySQL.
                                Trc.Log(
                                    LogLevel.Always,
                                    StringResources.MySQLManualInstallText);
                                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MySQLFailure));
                                SetupParameters.Instance().IsMySQLDownloaded = false;
                                ComponentTypeInstallationStatus[ComponentType.MySQL] = InstallationStatus.Failure;
                                InstallActionProcessess.FailureRecordOperation(
                                    IntegrityCheck.ExecutionScenario.Installation,
                                    IntegrityCheck.OperationName.DownloadMySql,
                                    string.Format(
                                        StringResources.RecordOperationFailureMessage,
                                        -1),
                                    string.Format(
                                    StringResources.RecordOperationRecommendationMessage,
                                    sysDrive + UnifiedSetupConstants.ASRUnifiedSetupLog));
                            }
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "MySQL is not installed on the server.");
                            MySQLDownload();
                        }
                    }
                }

                // Install Internet Information Services.
                if ((SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode) &&
                    (SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall))
                {
                    InstallationStatus mySQLInstStatus = ComponentTypeInstallationStatus[ComponentType.MySQL];
                    Trc.Log(LogLevel.Always, "mySQLInstStatus : {0}", mySQLInstStatus);
                    if (mySQLInstStatus == InstallationStatus.Success || mySQLInstStatus == InstallationStatus.Skip)
                    {
                        if (InstallActionProcessess.IsIISInstalled())
                        {
                            Trc.Log(LogLevel.Always, "Internet Information Services is already installed.");
                            Trc.Log(LogLevel.Always, "Update UI : IISAlreadyInstalled.");
                            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.IISAlreadyInstalled));
                            ComponentTypeInstallationStatus[ComponentType.IIS] = InstallationStatus.Success;
                            InstallActionProcessess.SkipRecordOperation(
                                IntegrityCheck.ExecutionScenario.Installation,
                                IntegrityCheck.OperationName.InstallIIS,
                                StringResources.ComponentAlreadyInstalled);
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "Installing Internet Information Services.");
                            InvokeIIS();
                        }
                    }
                }
              
                // Install CXTP.
                InstallationStatus iisInstStatus = ComponentTypeInstallationStatus[ComponentType.IIS];
                Trc.Log(LogLevel.Always, "iisInstStatus : {0}", iisInstStatus);
                bool invokeCxtp = false;
                if ((PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.MySQL55Installed)) && (SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade))
                {
                    if (ComponentTypeInstallationStatus[ComponentType.MySQL] == InstallationStatus.Success || ComponentTypeInstallationStatus[ComponentType.MySQL] == InstallationStatus.Skip)
                    {
                        invokeCxtp = true;
                    }
                }
                else if (iisInstStatus == InstallationStatus.Success || iisInstStatus == InstallationStatus.Skip)
                {
                    invokeCxtp = true;
                }
                if (invokeCxtp)
                {
                    if (isCXTPAlreadyInstalled)
                    {
                        Trc.Log(LogLevel.Always, "Same version of CX_TP is already installed on the server.");
                        this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.CXTPAlreadyInstalled));
                        ComponentTypeInstallationStatus[ComponentType.CXTP] = InstallationStatus.Success;
                        InstallActionProcessess.SkipRecordOperation(
                            IntegrityCheck.ExecutionScenario.Installation,
                            IntegrityCheck.OperationName.InstallTp,
                            StringResources.ComponentAlreadyInstalled);
                        SetupParameters.Instance().IsCXTPInstalled = true;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Same version of CX_TP is not installed on the server.");
                        InvokeCXTP();
                    }
                }

                // Install CX only if CXTP installation succeeds.
                InstallationStatus cxtpInstStatus = ComponentTypeInstallationStatus[ComponentType.CXTP];
                Trc.Log(LogLevel.Always, "cxtpInstStatus : {0}", cxtpInstStatus);
                if (cxtpInstStatus == InstallationStatus.Success)
                {
                    if (isCXAlreadyInstalled)
                    {
                        Trc.Log(LogLevel.Always, "Same version of CX is already installed on the server.");
                        this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.CXAlreadyInstalled));
                        ComponentTypeInstallationStatus[ComponentType.CX] = InstallationStatus.Success;
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
                        Trc.Log(LogLevel.Always, "Same version of CX is not installed on the server.");
                        InvokeCX();
                    }
                }

                // Install DRA only in case of CS installation.
                InstallationStatus cxInstStatus = ComponentTypeInstallationStatus[ComponentType.CX];
                Trc.Log(LogLevel.Always, "cxInstStatus : {0}", cxInstStatus);
                if ((SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode) &&
                    (!SetupParameters.Instance().PSGalleryImage))
                {
                    if (cxInstStatus == InstallationStatus.Success)
                    {
                        if (isDRAAlreadyInstalled)
                        {
                            Trc.Log(LogLevel.Always, "Same version of DRA is already installed on the server.");
                            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.DRAAlreadyInstalled));
                            ComponentTypeInstallationStatus[ComponentType.DRA] = InstallationStatus.Success;
                            InstallActionProcessess.SkipRecordOperation(
                                IntegrityCheck.ExecutionScenario.Installation,
                                IntegrityCheck.OperationName.InstallDra,
                                StringResources.ComponentAlreadyInstalled);
                            SetupParameters.Instance().IsDRAInstalled = true;
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "Same version of DRA is not installed on the server.");
                            InvokeDRA();
                        }
                    }
                }

                // Install Master target.
                if ((cxInstStatus == InstallationStatus.Success) &&
                    (!SetupParameters.Instance().PSGalleryImage))
                {
                    InstallationStatus draInstStatus = ComponentTypeInstallationStatus[ComponentType.DRA];
                    Trc.Log(LogLevel.Always, "draInstStatus : {0}", draInstStatus);
                    if (draInstStatus == InstallationStatus.Success || draInstStatus == InstallationStatus.Skip)
                    {
                        if (isMTAlreadyInstalled)
                        {
                            Trc.Log(LogLevel.Always, "Same version of MT is already installed on the server.");
                            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MTAlreadyInstalled));
                            ComponentTypeInstallationStatus[ComponentType.InstallMT] = InstallationStatus.Success;
                            InstallActionProcessess.SkipRecordOperation(
                                IntegrityCheck.ExecutionScenario.Installation,
                                IntegrityCheck.OperationName.InstallMt,
                                StringResources.ComponentAlreadyInstalled);
                            SetupParameters.Instance().IsMTInstalled = true;
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "Same version of MT is not installed on the server.");
                            InvokeMT();
                        }
                    }
                }

                // Configure Master target in case of fresh installation only.
                InstallationStatus mtInstStatus = ComponentTypeInstallationStatus[ComponentType.InstallMT];
                Trc.Log(LogLevel.Always, "mtInstStatus : {0}", mtInstStatus);
                if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall)
                {
                    if (mtInstStatus == InstallationStatus.Success)
                    {
                        if (InstallActionProcessess.isMTConfigured())
                        {
                            Trc.Log(LogLevel.Always, "MT is already configured on the server.");
                            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MTAlreadyConfigured));
                            ComponentTypeInstallationStatus[ComponentType.ConfigureMt] = InstallationStatus.Success;
                            InstallActionProcessess.SkipRecordOperation(
                                IntegrityCheck.ExecutionScenario.Configuration,
                                IntegrityCheck.OperationName.ConfigureMt,
                                StringResources.ComponentAlreadyInstalled);
                            SetupParameters.Instance().IsMTConfigured = true;
                        }
                        else
                        {
                            InvokeConfigureMT();
                        }
                    }
                }

                // Install MARS.
                if ((mtInstStatus == InstallationStatus.Success) &&
                    (!SetupParameters.Instance().PSGalleryImage))
                {
                    InstallationStatus mtConfigStatus = ComponentTypeInstallationStatus[ComponentType.ConfigureMt];
                    Trc.Log(LogLevel.Always, "mtConfigStatus : {0}", mtConfigStatus);
                    if (mtConfigStatus == InstallationStatus.Success || mtConfigStatus == InstallationStatus.Skip)
                    {
                        if (isMARSAlreadyInstalled)
                        {
                            Trc.Log(LogLevel.Always, "Same version of MARS is already installed on the server.");
                            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MARSAlreadyInstalled));
                            ComponentTypeInstallationStatus[ComponentType.MARS] = InstallationStatus.Success;
                            InstallActionProcessess.SkipRecordOperation(
                                IntegrityCheck.ExecutionScenario.Installation,
                                IntegrityCheck.OperationName.InstallMars,
                                StringResources.ComponentAlreadyInstalled);
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "Same version of MARS is not installed on the server.");
                            InvokeMARS();
                        }
                    }
                }

                // Install Configuration Manager.
                InstallationStatus marsInstStatus = ComponentTypeInstallationStatus[ComponentType.MARS];
                Trc.Log(LogLevel.Always, "marsInstStatus : {0}", marsInstStatus);
                if ((marsInstStatus == InstallationStatus.Success) &&
                    (SetupParameters.Instance().OVFImage))
                {
                    if (isConfigManagerAlreadyInstalled)
                    {
                        Trc.Log(LogLevel.Always, "Same version of Configuration Manager is already installed on the server.");
                        this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.ConfigManagerAlreadyInstalled));
                        ComponentTypeInstallationStatus[ComponentType.ConfigurationManager] = InstallationStatus.Success;
                        SetupParameters.Instance().IsConfigManagerInstalled = true;

                        InstallActionProcessess.SkipRecordOperation(
                            IntegrityCheck.ExecutionScenario.Installation,
                            IntegrityCheck.OperationName.InstallConfigManager,
                            StringResources.ComponentAlreadyInstalled);
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Same version of Configuration Manager is not installed on the server.");
                        InvokeConfigurationManager();
                    }
                }

                // Components registration.
                bool invokeRegistration = false;
                if (SetupParameters.Instance().OVFImage)
                {
                    InstallationStatus configManagerInstStatus = ComponentTypeInstallationStatus[ComponentType.ConfigurationManager];
                    Trc.Log(LogLevel.Always, "configManagerInstStatus : {0}", configManagerInstStatus);
                    if (configManagerInstStatus == InstallationStatus.Success)
                    {
                        invokeRegistration = true;
                    }
                }
                else
                {
                    marsInstStatus = ComponentTypeInstallationStatus[ComponentType.MARS];
                    Trc.Log(LogLevel.Always, "marsInstStatus : {0}", marsInstStatus);
                    if ((marsInstStatus == InstallationStatus.Success) &&
                    (!SetupParameters.Instance().PSGalleryImage))
                    {
                        invokeRegistration = true;
                    }
                }
                if(invokeRegistration)
                {
                    switch (SetupParameters.Instance().InstallType)
                    {
                        case UnifiedSetupConstants.FreshInstall:
                            Trc.Log(LogLevel.Always, "Invoking components registration.");
                            InvokeRegistration();
                            break;
                        case UnifiedSetupConstants.Upgrade:
                            ComponentTypeInstallationStatus[ComponentType.Registration] = InstallationStatus.Skip;
                            break;
                    }
                }

                // Update server configuration status.
                Trc.Log(LogLevel.Always, "Invoking ServerConfiguration.");
                this.InvokeServerConfiguration();

                //Update UI after installation.
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.AfterInstallUIUpdate));
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Error, "Exception occurred while installing components: {0}", e.ToString());
            }
        }

        /// <summary>
        /// Download MySQL setup file.
        /// </summary>
        private void MySQLDownload()
        {
            bool ismysql55Downloaded = true;
            if ((PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.MySQL55Installed)) && (SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade))
            {
                ismysql55Downloaded = InstallActionProcessess.DownloadMySQLSetupFile(UnifiedSetupConstants.MySQL55Version);
            }
            bool ismysql57Downloaded = InstallActionProcessess.DownloadMySQLSetupFile(UnifiedSetupConstants.MySQL57Version);
            if (ismysql57Downloaded && ismysql55Downloaded)
            {
                Trc.Log(LogLevel.Always, "Update UI : MySQL success");
                SetupParameters.Instance().IsMySQLDownloaded = true;
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MySQLSuccess));
                ComponentTypeInstallationStatus[ComponentType.MySQL] = InstallationStatus.Success;
                InstallActionProcessess.SucessRecordOperation(
                    IntegrityCheck.ExecutionScenario.Installation,
                    IntegrityCheck.OperationName.DownloadMySql);

            }
            else
            {
                Trc.Log(LogLevel.Always, "Update UI : MySQL failed. Aborting the installation.");
                SetupParameters.Instance().IsMySQLDownloaded = false;
                
                if (!ismysql55Downloaded)
                {
                    SetupParameters.Instance().MysqlVersion = UnifiedSetupConstants.MySQL55Version;
                }
                else if (!ismysql57Downloaded)
                {
                    SetupParameters.Instance().MysqlVersion = UnifiedSetupConstants.MySQL57Version;
                }
                
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MySQLFailure));
                
                ComponentTypeInstallationStatus[ComponentType.MySQL] = InstallationStatus.Failure;
                InstallActionProcessess.FailureRecordOperation(
                    IntegrityCheck.ExecutionScenario.Installation,
                    IntegrityCheck.OperationName.DownloadMySql,
                    string.Format(
                        StringResources.RecordOperationFailureMessage,
                        -1),
                    string.Format(
                    StringResources.RecordOperationRecommendationMessage,
                    sysDrive + UnifiedSetupConstants.ASRUnifiedSetupLog));
            }
        }

        /// <summary>
        /// Install Internet Information Services.
        /// </summary>
        private void InvokeIIS()
        {
            // Update UI that Internet Information Services installation is in progress
            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.IISProgress));

            int iisInstallStatus = InstallActionProcessess.InstallIIS();
            if (iisInstallStatus == 0)
            {
                Trc.Log(LogLevel.Always, "Update UI : IIS success.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.IISSuccess));
                ComponentTypeInstallationStatus[ComponentType.IIS] = InstallationStatus.Success;
            }
            else if (iisInstallStatus == 3010)
            {
                Trc.Log(LogLevel.Always, "Update UI : Internet Information Services installation requires the computer to be rebooted. Reboot machine and try install again.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.IISFailure));
                ComponentTypeInstallationStatus[ComponentType.IIS] = InstallationStatus.Failure;
            }
            else
            {
                Trc.Log(LogLevel.Always, "Update UI : IIS installation failed. Aborting the installation.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.IISFailure));
                ComponentTypeInstallationStatus[ComponentType.IIS] = InstallationStatus.Failure;
            }
        }

        /// <summary>
        /// Install thirdparty components.
        /// </summary>
        private void InvokeCXTP()
        {
            // Update UI that CX-TP installation is in progress
            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.CXTPProgress));

            Trc.Log(LogLevel.Always, "Installing configuration/process server dependencies.");
            int cxtpInstallStatus = InstallActionProcessess.InstallCXTP();
            if (cxtpInstallStatus == 0)
            {
                Trc.Log(LogLevel.Always, "Update UI : CXTP success.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.CXTPSuccess));
                ComponentTypeInstallationStatus[ComponentType.CXTP] = InstallationStatus.Success;
            }
            else
            {
                Trc.Log(LogLevel.Always, "Update UI : CXTP failed. Aborting the installation.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.CXTPFailure));
                ComponentTypeInstallationStatus[ComponentType.CXTP] = InstallationStatus.Failure;
            }
        }

        /// <summary>
        /// Install configuration/process server.
        /// </summary>
        private void InvokeCX()
        {
            // Update UI that CX installation is in progress
            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.CXProgress));

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
                Trc.Log(LogLevel.Always, "Update UI : CX success.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.CXSuccess));
                ComponentTypeInstallationStatus[ComponentType.CX] = InstallationStatus.Success;
            }
            else
            {
                Trc.Log(LogLevel.Always, "Update UI : CX failed. Aborting the installation.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.CXFailure));
                ComponentTypeInstallationStatus[ComponentType.CX] = InstallationStatus.Failure;
            }
        }

        /// <summary>
        /// Install DRA.
        /// </summary>
        private void InvokeDRA()
        {
            // Update UI that DRA installation is in progress.
            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.DRAProgress));

            int draInstallStatus = -1;
            draInstallStatus = InstallActionProcessess.InstallDRA();
            if (draInstallStatus == 0)
            {
                Trc.Log(LogLevel.Always, "Update UI : DRA installation successful.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.DRASuccess));
                ComponentTypeInstallationStatus[ComponentType.DRA] = InstallationStatus.Success;
            }
            else
            {
                Trc.Log(LogLevel.Always, "Update UI : DRA installation failed. Aborting the installation.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.DRAFailure));
                ComponentTypeInstallationStatus[ComponentType.DRA] = InstallationStatus.Failure;
            }
        }

        /// <summary>
        /// Install master target.
        /// </summary>
        private void InvokeMT()
        {
            // Update UI that MT installation is in progress.
            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MTProgress));

            int mtInstallStatus = InstallActionProcessess.InstallMT();

            // Treat 0 & 3010 exit codes as success case.
            if ((mtInstallStatus == 0) || (mtInstallStatus == 3010) || (mtInstallStatus == 1641) || (mtInstallStatus == 98))
            {
                Trc.Log(LogLevel.Always, "Update UI : MT success.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MTSuccess));
                ComponentTypeInstallationStatus[ComponentType.InstallMT] = InstallationStatus.Success;

                // Set Rebootrequired value to Yes if Unified agent returns 1641/3010/98 exit code.
                if ((mtInstallStatus == 3010) || (mtInstallStatus == 1641) || (mtInstallStatus == 98))
                {
                    SetupParameters.Instance().RebootRequired = UnifiedSetupConstants.Yes;
                    Trc.Log(LogLevel.Always, "Reboot required parameter is set to : {0}", SetupParameters.Instance().RebootRequired);
                }
            }
            else
            {
                Trc.Log(LogLevel.Always, "Update UI : MT failed. Aborting the installation.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MTFailure));
                ComponentTypeInstallationStatus[ComponentType.InstallMT] = InstallationStatus.Failure;
            }
        }

        /// <summary>
        /// Configure Master target.
        /// </summary>
        private void InvokeConfigureMT()
        {
            // Update UI that MT configuration is in progress.
            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MTConfigProgress));

            int mtConfigStatus = InstallActionProcessess.ConfigureMT();
            if (mtConfigStatus == 0)
            {
                Trc.Log(LogLevel.Always, "Update UI : MT configuration success.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MTConfigSuccess));
                ComponentTypeInstallationStatus[ComponentType.ConfigureMt] = InstallationStatus.Success;
            }
            else
            {
                Trc.Log(LogLevel.Always, "Update UI : MT configuration failed. Aborting the installation.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MTConfigFailure));
                ComponentTypeInstallationStatus[ComponentType.ConfigureMt] = InstallationStatus.Failure;
            }
        }

        /// <summary>
        /// Install MARS.
        /// </summary>
        private void InvokeMARS()
        {
            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MARSProgress));

            Trc.Log(LogLevel.Always, "Invoking InstallMARS.");
            int marsInstallStatus = InstallActionProcessess.InstallMARS();
            if (marsInstallStatus == 0)
            {
                // MARS service sometimes take 10 secs to be in running state.
                Thread.Sleep(10000);
                Trc.Log(LogLevel.Always, "Update UI : MARS installation successful.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MARSSuccess));
                ComponentTypeInstallationStatus[ComponentType.MARS] = InstallationStatus.Success;
            }
            else
            {
                Trc.Log(LogLevel.Always, "Update UI : MARS installation failed. Aborting the installation.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.MARSFailure));
                ComponentTypeInstallationStatus[ComponentType.MARS] = InstallationStatus.Failure;
            }
        }

        /// <summary>
        /// Install Configuration Manager.
        /// </summary>
        private void InvokeConfigurationManager()
        {
            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.ConfigManagerProgress));

            Trc.Log(LogLevel.Always, "Invoking InstallConfigurationManager.");
            int configManagerInstallStatus = InstallActionProcessess.InstallConfigurationManager();
            if (configManagerInstallStatus == 0)
            {
                Trc.Log(LogLevel.Always, "Update UI : Configuration Manager installation successful.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.ConfigManagerSuccess));
                ComponentTypeInstallationStatus[ComponentType.ConfigurationManager] = InstallationStatus.Success;
            }
            else
            {
                Trc.Log(LogLevel.Always, "Update UI : Configuration Manager installation failed. Aborting the installation.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.ConfigManagerFailure));
                ComponentTypeInstallationStatus[ComponentType.ConfigurationManager] = InstallationStatus.Failure;
            }
        }

        /// <summary>
        /// Perform Vault, conatiner and MT registration.
        /// </summary>
        private void InvokeRegistration()
        {
            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.RegistrationProgress));
            bool registrationStatus = false;
            SetupHelper.SetupReturnValues setupReturnCode;
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
                Trc.Log(LogLevel.Always, "Update UI : Registration success.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.RegistrationSuccess));
                ComponentTypeInstallationStatus[ComponentType.Registration] = InstallationStatus.Success;
                SetupParameters.Instance().InstallStatus = UnifiedSetupConstants.SuccessStatus;
            }
            else
            {
                Trc.Log(LogLevel.Always, "Update UI : Registration failed. Aborting the installation.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.RegistrationFailure));
                ComponentTypeInstallationStatus[ComponentType.Registration] = InstallationStatus.Failure;
                SetupParameters.Instance().InstallStatus = UnifiedSetupConstants.FailedStatus;
            }
        }

        /// <summary>
        /// validate server configuration.
        /// </summary>
        private void InvokeServerConfiguration()
        {
            try
            {
                // Update UI that Configuration of server is in progress
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.ConfigureServerProgress));

                // Validate after installation.
                this.ValidateServerConfiguration();

                // Create summary presentation;
                this.CreateServerConfigurationStatusFile();

                // Get resultant status.
                bool finalStatus = ServerConfigurationOperationStatus();

                if (finalStatus)
                {
                    SetupParameters.Instance().IsInstallationSuccess = true;
                }

                // Post installation steps. Update summary and upload telemetry logs.
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.InstallationFinished, true);
                IntegrityCheck.IntegrityCheckWrapper.EndSummary(
                    finalStatus ? 
                        IntegrityCheck.OperationResult.Success : 
                        IntegrityCheck.OperationResult.Failed,
                    (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode) ?
                        UnifiedSetupConstants.ScaleOutUnitComponentName :
                        string.Empty);
                IntegrityCheck.IntegrityCheckWrapper.DisposeSummary();

                // Upload summary logs.
                InstallActionProcessess.UploadOperationSummary();

                InstallActionProcessess.ChangeLogUploadServiceToManual();

                    // After post installation steps, update UI based on entire operations result.
                if (finalStatus)
                {
                    Trc.Log(LogLevel.Always, "Update UI : Server configuration success.");
                    this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.ConfigureServerSuccess));
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Update UI : Server configuration failed.");
                    this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UIUpdateDelegate(this.ConfigureServerFailure));
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
            }
        }

        /// <summary>
        /// Validates services post installation.
        /// </summary>
        private bool ValidateServices()
        {
            bool result = true;
            IntegrityCheck.Response validationResponse;

            // Get the operation summary for the Installation to validate service checks
            List<IntegrityCheck.OperationDetails> installOperationDetails =
                IntegrityCheck.IntegrityCheckWrapper.GetOperationSummary().OperationDetails.Where(
                o => o.ScenarioName == IntegrityCheck.ExecutionScenario.Installation).ToList();

            // Get the services to be validated
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

                // Check for the service running status, only if the install operation is success.
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
                        result = false;
                    }
                }

                // Sleep for 1 sec during multiple service checks.
                Thread.Sleep(1000);
            }

            return result;
        }

        /// <summary>
        /// Validate server configuration post installation.
        /// </summary>
        /// <returns></returns>
        private bool ValidateServerConfiguration()
        {
            // Currently checking only services.
            return this.ValidateServices();
        }

        /// <summary>
        /// Validate server configuration operation status.
        /// </summary>
        /// <returns>true if all the operations succeed, false otherwise</returns>
        private bool ServerConfigurationOperationStatus()
        {
            bool operationResult = true;

            // Fetch operations list.
            List<IntegrityCheck.OperationDetails> OperationDetailsList =
                IntegrityCheck.IntegrityCheckWrapper.GetOperationSummary().OperationDetails.Where(
                o => o.ScenarioName != IntegrityCheck.ExecutionScenario.PreInstallation).ToList();

            foreach (IntegrityCheck.OperationDetails operation in OperationDetailsList)
            {
                if (operation.Result == IntegrityCheck.OperationResult.Failed)
                {
                    Trc.Log(LogLevel.Always,
                        "operation: {0}  result: {1}", operation.OperationName, operation.Result);
                    operationResult = false;
                    break;
                }
            }

            return operationResult;
        }

        #endregion

        #region LogCreationMethods

        /// <summary>
        /// Create server configuration status file for user readability
        /// </summary>
        private void CreateServerConfigurationStatusFile()
        {
            try
            {
                string filepath = SetupHelper.SetLogFilePath(UnifiedSetupConstants.SummaryStatusFileName);
                List<string> lines = new List<string>();
                List<IntegrityCheck.OperationDetails> operationList = new List<IntegrityCheck.OperationDetails>();
                List<IntegrityCheck.ExecutionScenario> displayScenarios = new List<IntegrityCheck.ExecutionScenario>
                {
                    IntegrityCheck.ExecutionScenario.PreInstallation,
                    IntegrityCheck.ExecutionScenario.Installation,
                    IntegrityCheck.ExecutionScenario.Configuration,
                    IntegrityCheck.ExecutionScenario.PostInstallation
                };

                // Summmary title
                lines.Add(this.FrameHeader(ASRResources.StringResources.SummaryLogTitle));

                // Display start time.
                lines.Add(string.Format(
                    "{0}\t: {1}",
                    ASRResources.StringResources.SummaryStartTime,
                    Convert.ToDateTime(
                    IntegrityCheck.IntegrityCheckWrapper.GetOperationSummary().StartTime).ToString(
                    ASRResources.StringResources.SummaryTimeFormat)));
                lines.Add(Environment.NewLine);

                // Iterate over the scenarios.
                displayScenarios.ForEach(scenario =>
                    {
                        lines.Add(this.ParseAndDisplay(scenario));
                    });

                // Display end time.
                lines.Add(string.Format(
                    "{0}\t: {1}",
                    ASRResources.StringResources.SummaryEndTime,
                    Convert.ToDateTime(
                    IntegrityCheck.IntegrityCheckWrapper.GetOperationSummary().EndTime).ToString(
                    ASRResources.StringResources.SummaryTimeFormat)));

                // Write to file.
                File.WriteAllLines(filepath, lines, Encoding.UTF8);
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error, "Create server configuration status file failed.", ex);
            }
        }

        /// <summary>
        /// Extract the summary for the respective scenario and frame to a user readable format.
        /// </summary>
        /// <param name="scenarioName">Name of the scenario to be viewed.</param>
        /// <returns>Returns display string.</returns>
        private string ParseAndDisplay(IntegrityCheck.ExecutionScenario scenarioName)
        {
            List<IntegrityCheck.OperationDetails> operationDetails =
                IntegrityCheck.IntegrityCheckWrapper.GetOperationSummary().OperationDetails.Where(
                o => o.ScenarioName == scenarioName).ToList();
            List<IntegrityCheck.OperationDetails> operationList = new List<IntegrityCheck.OperationDetails>();
            List<string> lines = new List<string>();

            // Maintain only the latest and display to the user.
            operationDetails.ForEach(operation =>
                {
                    if (operationList.Where(o => o.OperationName == operation.OperationName).Count() != 0)
                    {
                        operationList.RemoveAll(o => o.OperationName == operation.OperationName);
                    }

                    operationList.Add(operation);
                });

            // Frame header
            lines.Add(this.FrameHeader(EvaluateOperations.ScenarioFriendlyNames[scenarioName]));

            foreach (IntegrityCheck.OperationDetails operation in operationList.Where(o => o.ScenarioName == scenarioName))
            {
                IntegrityCheck.OperationName operationName = 
                    (IntegrityCheck.OperationName)Enum.Parse(
                        typeof(IntegrityCheck.OperationName),
                        operation.OperationName);
				lines.Add(this.FrameOperation(
                    EvaluateOperations.OperationFriendlyNames[operationName],
                    operation.Result.ToString(),
                    operation.Message,
                    operation.Causes,
                    operation.Recommendation));

                lines.Add(Environment.NewLine);
            }

            return String.Join(Environment.NewLine, lines);
        }

        /// <summary>
        /// Frame summary header.
        /// </summary>
        /// <param name="headerName">Header name.</param>
        /// <returns>Returns the formatted header.</returns>
        private string FrameHeader(string headerName)
        {
            const char delimiter = '=';
            const int delimiterCount = 22;
            List<string> lines = new List<string>();

            return string.Format(
                "{0}{1}{2}",
                headerName,
                Environment.NewLine,
                new String(delimiter, delimiterCount));
        }

        /// <summary>
        /// Frame operation.
        /// </summary>
        /// <param name="name">Name of the operation.</param>
        /// <param name="result">Result.</param>
        /// <param name="message">Message to be displayed</param>
        /// <param name="causes">Possible causes if any</param>
        /// <param name="recommendedAction">Recommended action if any</param>
        /// <returns>Returns the formatted string.</returns>
        private string FrameOperation(
            string name,
            string result,
            string message,
            string causes,
            string recommendedAction)
        {
            List<string> lines = new List<string>();

            lines.Add(string.Format(
                "{0}\t\t\t: {1}",
                ASRResources.StringResources.SummaryOperationName,
                name));
            lines.Add(string.Format(
                "{0}\t\t\t: {1}",
                ASRResources.StringResources.SummaryOperationResult,
                result));

            // If any message.
            if (!string.IsNullOrEmpty(message))
            {
                lines.Add(string.Format(
                    "{0}\t\t\t: {1}",
                    ASRResources.StringResources.SummaryOperationMessage,
                    message));
            }

            // If any possible causes
            if (!string.IsNullOrEmpty(causes))
            {
                lines.Add(string.Format(
                    "{0}\t\t: {1}",
                    ASRResources.StringResources.SummaryOperationCauses,
                    causes));
            }

            // If any recommended action
            if (!string.IsNullOrEmpty(recommendedAction))
            {
                lines.Add(string.Format(
                    "{0}\t: {1}",
                    ASRResources.StringResources.SummaryOperationRecommendation,
                    recommendedAction));
            }

            return String.Join(Environment.NewLine, lines);
        }

        /// <summary>
        /// Create status file.
        /// </summary>
        private void CreateStatusLog(string fileName, string info)
        {
            try
            {
                string path = sysDrive + fileName;

                if (File.Exists(path))
                {
                    Trc.Log(LogLevel.Always, "Deleting the " + path + " file.");
                    File.Delete(path);
                }

                Trc.Log(LogLevel.Always, "Creating "+ path +" file.");
                File.WriteAllText(path, info);
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error, "Exception occurred while creating status log: ", ex);
            }
        }
        #endregion

        #region UpdateUIMethods

        /// <summary>
        /// Update UI to collapse a row.
        /// </summary>
        private void CollapseRow(
            System.Windows.Controls.TextBox imageTextbox,
            System.Windows.Controls.TextBox componentTextbox,
            System.Windows.Controls.TextBox statusTextbox)
        {
            imageTextbox.Visibility = System.Windows.Visibility.Collapsed;
            componentTextbox.Visibility = System.Windows.Visibility.Collapsed;
            statusTextbox.Visibility = System.Windows.Visibility.Collapsed;
        }

        /// <summary>
        /// Changes during upgrade.
        /// </summary>
        private void ChangesDuringUpgrade()
        {
            try
            {
                if ((SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode) || 
                    ((SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode) && 
                    (!PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.MySQL55Installed))))
                {
                    // Collapse MySQL row.
                    CollapseRow(
                        InstProgressMySQLImageTextbox,
                        InstProgressMySQLTextbox,
                        InstProgressMySQLStatusPending);
                    this.InstProgressTPStatusPending.Visibility = System.Windows.Visibility.Collapsed;
                    this.InstProgressTPStatusInstalling.Visibility = System.Windows.Visibility.Visible;
                    ComponentTypeInstallationStatus[ComponentType.MySQL] = InstallationStatus.Skip;
                }
                else
                {
                    this.InstallationProgressCurrentOperationTextBlock.Text = StringResources.InstallationProgressCurrentOperationMySQLText;
                    this.InstProgressMySQLStatusPending.Visibility = System.Windows.Visibility.Collapsed;
                    this.InstProgressMySQLStatusDownloading.Visibility = System.Windows.Visibility.Visible;
                }

                // Collapse IIS row.
                CollapseRow(
                    InstProgressIISImageTextbox,
                    InstProgressIISTextbox,
                    InstProgressIISStatusPending);
				
				// Collapse Configure master target row.
                CollapseRow(
                    ConfigureMTImageTextbox,
                    ConfigureMTTextbox,
                    ConfigureMTStatusPending);

                // Collapse Registration row.
                CollapseRow(
                    InstProgressRegistrationImageTextbox,
                    InstProgressRegistrationTextbox,
                    InstProgressRegistrationStatusPending);

                if (SetupParameters.Instance().PSGalleryImage)
                {
                    // Collapse MT row.
                    CollapseRow(
                        InstProgressImageTextbox3,
                        InstProgressMTTextbox,
                        InstProgressMTStatusPending);

                    // Collapse MARS row.
                    CollapseRow(
                        InstProgressMARSImageTextbox,
                        InstProgressMARSTextbox,
                        InstProgressMARSStatusPending);
                }

                if (!(SetupParameters.Instance().OVFImage))
                {
                    ComponentTypeInstallationStatus[ComponentType.ConfigurationManager] = InstallationStatus.Skip;
                    // Collapse Configure Manager row.
                    CollapseRow(
                        InstProgressConfigManagerImageTextbox,
                        InstProgressConfigManagerTextbox,
                        InstProgressConfigManagerStatusPending);
                }

                ComponentTypeInstallationStatus[ComponentType.IIS] = InstallationStatus.Skip;
				ComponentTypeInstallationStatus[ComponentType.ConfigureMt] = InstallationStatus.Skip;
                ComponentTypeInstallationStatus[ComponentType.Registration] = InstallationStatus.Skip;

                // Fetch all the required installation information.
                InstallActionProcessess.FetchInstallationDetails();

                if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                {
                    this.InstProgressCSPSTextbox.Visibility = System.Windows.Visibility.Visible;
                }
                else
                {
                    this.InstProgressPSTextbox.Visibility = System.Windows.Visibility.Visible;
                    ComponentTypeInstallationStatus[ComponentType.DRA] = InstallationStatus.Skip;

                    // Collapse DRA row.
                    CollapseRow(
                        InstProgressDRAImageTextbox,
                        InstProgressDRATextbox,
                        InstProgressDRAStatusPending);
                }

                // Remove unused registry keys.
                InstallActionProcessess.RemoveUnUsedRegistryKeys();
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error, "Exception occurred while UI changes during upgrade: ", ex);
            }
        }

        /// <summary>
        /// UI changes during fresh installation.
        /// </summary>
        private void FreshInstallUIChanges()
        {
            if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
            {
                this.InstProgressCSPSTextbox.Visibility = System.Windows.Visibility.Visible;
                this.InstallationProgressCurrentOperationTextBlock.Text = StringResources.InstallationProgressCurrentOperationMySQLText;
                this.InstProgressMySQLStatusPending.Visibility = System.Windows.Visibility.Collapsed;
                this.InstProgressMySQLStatusDownloading.Visibility = System.Windows.Visibility.Visible;

                this.InstProgressIISStatusPending.Visibility = System.Windows.Visibility.Visible;
                this.InstProgressIISStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            }
            // Collapse MySQL row in case of PS+MT installation.
            else if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
            {
                this.InstProgressPSTextbox.Visibility = System.Windows.Visibility.Visible;
                this.InstProgressTPStatusPending.Visibility = System.Windows.Visibility.Collapsed;
                this.InstProgressTPStatusInstalling.Visibility = System.Windows.Visibility.Visible;
                ComponentTypeInstallationStatus[ComponentType.MySQL] = InstallationStatus.Skip;
                ComponentTypeInstallationStatus[ComponentType.IIS] = InstallationStatus.Skip;
                ComponentTypeInstallationStatus[ComponentType.DRA] = InstallationStatus.Skip;

                // Collapse MySQL row.
                CollapseRow(
                    InstProgressMySQLImageTextbox,
                    InstProgressMySQLTextbox,
                    InstProgressMySQLStatusPending);

                // Collapse IIS row.
                CollapseRow(
                    InstProgressIISImageTextbox,
                    InstProgressIISTextbox,
                    InstProgressIISStatusPending);

                // Collapse DRA row.
                CollapseRow(
                    InstProgressDRAImageTextbox,
                    InstProgressDRATextbox,
                    InstProgressDRAStatusPending);
            }

            if (!(SetupParameters.Instance().OVFImage))
            {
                ComponentTypeInstallationStatus[ComponentType.ConfigurationManager] = InstallationStatus.Skip;
                // Collapse Configure Manager row.
                CollapseRow(
                    InstProgressConfigManagerImageTextbox,
                    InstProgressConfigManagerTextbox,
                    InstProgressConfigManagerStatusPending);
            }
        }

        /// <summary>
        /// Update UI after installation of all components.
        /// </summary>
        private void AfterInstallUIUpdate()
        {
            // Delete setup related files.
            if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall)
            {
                DeleteSetupFiles();
            }

            // Enable Finish button.
            this.Page.Host.SetCancelButtonState(true, true, StringResources.FinishButtonText);

            // Hide Installation Progress bar
            this.InstallationShowProgress.Visibility = System.Windows.Visibility.Hidden;

            // Hide current operation text box
            this.InstallationProgressCurrentOperationTextBlock.Visibility = System.Windows.Visibility.Hidden;

            // Suppress Cancel text.
            PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.CanClose, true);

            // Installation finished.
            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.InstallationFinished,
                true);
        }

        /// <summary>
        /// Update UI if MySQL is already installed.
        /// </summary>
        private void MySQLAlreadyInstalled()
        {
            CreateStatusLog(UnifiedSetupConstants.MySQLDownloadLog, string.Format(StringResources.MySQLDownloadAlreadyInstalledText, sysDrive));
            this.InstProgressMySQLStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMySQLStatusDownloading.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMySQLStatusAlreadyInstalled.Visibility = System.Windows.Visibility.Visible;
            this.MySQLSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI if MySQL installation is successful.
        /// </summary>
        private void MySQLSuccess()
        {
            CreateStatusLog(UnifiedSetupConstants.MySQLDownloadLog, string.Format(StringResources.MySQLDownloadSuccessText, sysDrive));
            this.InstProgressMySQLStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMySQLStatusDownloading.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMySQLStatusDone.Visibility = System.Windows.Visibility.Visible;
            this.MySQLSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI if MySQL installation failed.
        /// </summary>
        private void MySQLFailure()
        {
            if (SetupParameters.Instance().MysqlVersion == UnifiedSetupConstants.MySQL55Version)
            {
                CreateStatusLog(UnifiedSetupConstants.MySQLDownloadLog, string.Format(StringResources.MySQL55DownloadFailureText, sysDrive));
            }
            else if (SetupParameters.Instance().MysqlVersion == UnifiedSetupConstants.MySQL57Version)
            {
                CreateStatusLog(UnifiedSetupConstants.MySQLDownloadLog, string.Format(StringResources.MySQLDownloadFailureText, sysDrive));
            }
            this.InstProgressMySQLStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMySQLStatusDownloading.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMySQLStatusFailed.Visibility = System.Windows.Visibility.Visible;
            this.MySQLErrorImage.Visibility = System.Windows.Visibility.Visible;
            this.InstProgressTPStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressCSPSStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressDRAStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressMTStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressMARSStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressConfigManagerStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressRegistrationStatusPending.Text = StringResources.SkippedStatusText;
        }
       
        /// <summary>
        /// Update UI if IIS is already installed.
        /// </summary>
        private void IISAlreadyInstalled()
        {
            this.InstProgressIISStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressIISStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressIISStatusAlreadyInstalled.Visibility = System.Windows.Visibility.Visible;
            this.IISSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI when IIS installation is in progress.
        /// </summary>
        private void IISProgress()
        {
            this.InstProgressIISStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressIISStatusInstalling.Visibility = System.Windows.Visibility.Visible;
            this.InstallationProgressCurrentOperationTextBlock.Text = StringResources.InstallationProgressCurrentOperationIISText;
        }

        /// <summary>
        /// Update UI if IIS installation is successful.
        /// </summary>
        private void IISSuccess()
        {
            this.InstProgressIISStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressIISStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressIISStatusDone.Visibility = System.Windows.Visibility.Visible;
            this.IISSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI if IIS installation failed.
        /// </summary>
        private void IISFailure()
        {
            this.InstProgressIISStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressIISStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressIISStatusFailed.Visibility = System.Windows.Visibility.Visible;
            this.IISErrorImage.Visibility = System.Windows.Visibility.Visible;
            this.InstProgressTPStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressCSPSStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressDRAStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressMTStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressMARSStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressConfigManagerStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressRegistrationStatusPending.Text = StringResources.SkippedStatusText;
        }

        /// <summary>
        /// Update UI if CXTP is already installed.
        /// </summary>
        private void CXTPAlreadyInstalled()
        {
            this.InstProgressTPStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressTPStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressTPStatusAlreadyInstalled.Visibility = System.Windows.Visibility.Visible;
            this.SuccessImage1.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI when CXTP installation is in progress.
        /// </summary>
        private void CXTPProgress()
        {
            this.InstProgressTPStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressTPStatusInstalling.Visibility = System.Windows.Visibility.Visible;
            this.InstallationProgressCurrentOperationTextBlock.Text = StringResources.InstallationProgressCurrentOperationTPText;
        }

        /// <summary>
        /// Update UI if CXTP installation is successful.
        /// </summary>
        private void CXTPSuccess()
        {
            this.InstProgressTPStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressTPStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressTPStatusDone.Visibility = System.Windows.Visibility.Visible;
            this.SuccessImage1.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI if CXTP installation failed.
        /// </summary>
        private void CXTPFailure()
        {
            this.InstProgressTPStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressTPStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressTPStatusFailed.Visibility = System.Windows.Visibility.Visible;
            this.ErrorImage1.Visibility = System.Windows.Visibility.Visible;
            this.InstProgressCSPSStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressDRAStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressMTStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressMARSStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressConfigManagerStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressRegistrationStatusPending.Text = StringResources.SkippedStatusText;
        }

        /// <summary>
        /// Update UI if CX is already installed.
        /// </summary>
        private void CXAlreadyInstalled()
        {
            this.InstProgressCSPSStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressCSPSStatusAlreadyInstalled.Visibility = System.Windows.Visibility.Visible;
            this.SuccessImage2.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI when CX installation is in progress.
        /// </summary>
        private void CXProgress()
        {
            this.InstProgressCSPSStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressCSPSStatusInstalling.Visibility = System.Windows.Visibility.Visible;

            if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
            {
                this.InstallationProgressCurrentOperationTextBlock.Text = StringResources.InstallationProgressCurrentOperationCSPSText;
            }
            else if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
            {
                this.InstallationProgressCurrentOperationTextBlock.Text = StringResources.InstallationProgressCurrentOperationPSText;
            }
        }

        /// <summary>
        /// Update UI if CX installation is successful.
        /// </summary>
        private void CXSuccess()
        {
            this.InstProgressCSPSStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressCSPSStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressCSPSStatusDone.Visibility = System.Windows.Visibility.Visible;
            this.SuccessImage2.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI if CX installation failed.
        /// </summary>
        private void CXFailure()
        {
            this.InstProgressCSPSStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressCSPSStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressCSPSStatusFailed.Visibility = System.Windows.Visibility.Visible;
            this.ErrorImage2.Visibility = System.Windows.Visibility.Visible;
            this.InstProgressDRAStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressMTStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressMARSStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressConfigManagerStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressRegistrationStatusPending.Text = StringResources.SkippedStatusText;
        }

        /// <summary>
        /// Update UI if DRA is already installed.
        /// </summary>
        private void DRAAlreadyInstalled()
        {
            this.InstProgressDRAStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressDRAStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressDRAStatusAlreadyInstalled.Visibility = System.Windows.Visibility.Visible;
            this.DRASuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI when DRA installation is in progress.
        /// </summary>
        private void DRAProgress()
        {
            this.InstProgressDRAStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressDRAStatusInstalling.Visibility = System.Windows.Visibility.Visible;
            this.InstallationProgressCurrentOperationTextBlock.Text = StringResources.InstallationProgressCurrentOperationDRAText;
        }

        /// <summary>
        /// Update UI if DRA installation is successful.
        /// </summary>
        private void DRASuccess()
        {
            this.InstProgressDRAStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressDRAStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressDRAStatusDone.Visibility = System.Windows.Visibility.Visible;
            this.DRASuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI if DRA installation failed.
        /// </summary>
        private void DRAFailure()
        {
            this.InstProgressDRAStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressDRAStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressDRAStatusFailed.Visibility = System.Windows.Visibility.Visible;
            this.DRAErrorImage.Visibility = System.Windows.Visibility.Visible;
            this.InstProgressMTStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressMARSStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressConfigManagerStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressRegistrationStatusPending.Text = StringResources.SkippedStatusText;
        }

        /// <summary>
        /// Update UI if MT is already installed.
        /// </summary>
        private void MTAlreadyInstalled()
        {
            this.InstProgressMTStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMTStatusAlreadyInstalled.Visibility = System.Windows.Visibility.Visible;
            this.SuccessImage3.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI when MT installation is in progress.
        /// </summary>
        private void MTProgress()
        {
            this.InstProgressMTStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMTStatusInstalling.Visibility = System.Windows.Visibility.Visible;
            this.InstallationProgressCurrentOperationTextBlock.Text = StringResources.InstallationProgressCurrentOperationMTText;
        }

        /// <summary>
        /// Update UI if MT installation is successful.
        /// </summary>
        private void MTSuccess()
        {
            this.InstProgressMTStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMTStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMTStatusDone.Visibility = System.Windows.Visibility.Visible;
            this.SuccessImage3.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI if MT installation failed.
        /// </summary>
        private void MTFailure()
        {
            this.InstProgressMTStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMTStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMTStatusFailed.Visibility = System.Windows.Visibility.Visible;
            this.ErrorImage3.Visibility = System.Windows.Visibility.Visible;
            this.ConfigureMTStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressMARSStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressConfigManagerStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressRegistrationStatusPending.Text = StringResources.SkippedStatusText;
        }


        /// <summary>
        /// Update UI if MT is already configured.
        /// </summary>
        private void MTAlreadyConfigured()
        {
            this.ConfigureMTStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.ConfigureMTStatusAlreadyConfigured.Visibility = System.Windows.Visibility.Visible;
            this.ConfigureMTSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI when MT configuration is in progress.
        /// </summary>
        private void MTConfigProgress()
        {
            this.ConfigureMTStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.ConfigureMTStatusConfiguring.Visibility = System.Windows.Visibility.Visible;
            this.InstallationProgressCurrentOperationTextBlock.Text = StringResources.CurrentOperationMTConfigurationText;
        }

        /// <summary>
        /// Update UI if MT configuration is successful.
        /// </summary>
        private void MTConfigSuccess()
        {
            this.ConfigureMTStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.ConfigureMTStatusConfiguring.Visibility = System.Windows.Visibility.Collapsed;
            this.ConfigureMTStatusDone.Visibility = System.Windows.Visibility.Visible;
            this.ConfigureMTSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI if MT configuration failed.
        /// </summary>
        private void MTConfigFailure()
        {
            this.ConfigureMTStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.ConfigureMTStatusConfiguring.Visibility = System.Windows.Visibility.Collapsed;
            this.ConfigureMTStatusFailed.Visibility = System.Windows.Visibility.Visible;
            this.ConfigureMTErrorImage.Visibility = System.Windows.Visibility.Visible;
            this.InstProgressMARSStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressConfigManagerStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressRegistrationStatusPending.Text = StringResources.SkippedStatusText;
        }

        /// <summary>
        /// Update UI if MARS is already installed.
        /// </summary>
        private void MARSAlreadyInstalled()
        {
            CreateStatusLog(UnifiedSetupConstants.MARSStatusLog, string.Format(StringResources.MARSInstStatusAlreadyInstalled, sysDrive));
            this.InstProgressMARSStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMARSStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMARSStatusAlreadyInstalled.Visibility = System.Windows.Visibility.Visible;
            this.MARSSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI when MARS installation is in progress.
        /// </summary>
        private void MARSProgress()
        {
            this.InstProgressMARSStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMARSStatusInstalling.Visibility = System.Windows.Visibility.Visible;
            this.InstallationProgressCurrentOperationTextBlock.Text = StringResources.InstallationProgressCurrentOperationMARSText;
        }

        /// <summary>
        /// Update UI if MARS installation is successful.
        /// </summary>
        private void MARSSuccess()
        {
            CreateStatusLog(UnifiedSetupConstants.MARSStatusLog, string.Format(StringResources.MARSInstStatusSuccess, sysDrive));
            this.InstProgressMARSStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMARSStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMARSStatusDone.Visibility = System.Windows.Visibility.Visible;
            this.MARSSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI if MARS installation failed.
        /// </summary>
        private void MARSFailure()
        {
            CreateStatusLog(UnifiedSetupConstants.MARSStatusLog, string.Format(StringResources.MARSInstStatusFailed, sysDrive));
            this.InstProgressMARSStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMARSStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressMARSStatusFailed.Visibility = System.Windows.Visibility.Visible;
            this.MARSErrorImage.Visibility = System.Windows.Visibility.Visible;
            this.InstProgressConfigManagerStatusPending.Text = StringResources.SkippedStatusText;
            this.InstProgressRegistrationStatusPending.Text = StringResources.SkippedStatusText;
        }

        /// <summary>
        /// Update UI if Configuration Manager is already installed.
        /// </summary>
        private void ConfigManagerAlreadyInstalled()
        {
            CreateStatusLog(UnifiedSetupConstants.ConfigManagerStatusLog, string.Format(StringResources.ConfigManagerInstStatusAlreadyInstalled, sysDrive));
            this.InstProgressConfigManagerStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressConfigManagerStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressConfigManagerStatusAlreadyInstalled.Visibility = System.Windows.Visibility.Visible;
            this.ConfigManagerSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI when Configuration Manager installation is in progress.
        /// </summary>
        private void ConfigManagerProgress()
        {
            this.InstProgressConfigManagerStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressConfigManagerStatusInstalling.Visibility = System.Windows.Visibility.Visible;
            this.InstallationProgressCurrentOperationTextBlock.Text = StringResources.InstallationProgressCurrentOperationConfigManagerText;
        }

        /// <summary>
        /// Update UI if Configuration Manager installation is successful.
        /// </summary>
        private void ConfigManagerSuccess()
        {
            CreateStatusLog(UnifiedSetupConstants.ConfigManagerStatusLog, string.Format(StringResources.ConfigManagerInstStatusSuccess, sysDrive));
            this.InstProgressConfigManagerStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressConfigManagerStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressConfigManagerStatusDone.Visibility = System.Windows.Visibility.Visible;
            this.ConfigManagerSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI if Configuration Manager installation failed.
        /// </summary>
        private void ConfigManagerFailure()
        {
            CreateStatusLog(UnifiedSetupConstants.ConfigManagerStatusLog, string.Format(StringResources.ConfigManagerInstStatusFailed, sysDrive));
            this.InstProgressConfigManagerStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressConfigManagerStatusInstalling.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressConfigManagerStatusFailed.Visibility = System.Windows.Visibility.Visible;
            this.ConfigManagerErrorImage.Visibility = System.Windows.Visibility.Visible;
            this.InstProgressRegistrationStatusPending.Text = StringResources.SkippedStatusText;
        }

        /// <summary>
        /// Update UI if Registration is already done.
        /// </summary>
        private void ComponentsAlreadyRegistered()
        {
            this.InstProgressRegistrationStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressRegistrationStatusRegistering.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressComponentsAlreadyRegistered.Visibility = System.Windows.Visibility.Visible;
            this.RegistrationSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI when Registration is in progress.
        /// </summary>
        private void RegistrationProgress()
        {
            this.InstProgressRegistrationStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressRegistrationStatusRegistering.Visibility = System.Windows.Visibility.Visible;
            this.InstallationProgressCurrentOperationTextBlock.Text = StringResources.InstallationProgressCurrentOperationRegistrationText;
        }

        /// <summary>
        /// Update UI if Registration is successful.
        /// </summary>
        private void RegistrationSuccess()
        {
            this.InstProgressRegistrationStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressRegistrationStatusRegistering.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressRegistrationStatusDone.Visibility = System.Windows.Visibility.Visible;
            this.RegistrationSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI if Registration failed.
        /// </summary>
        private void RegistrationFailure()
        {
            this.InstProgressRegistrationStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressRegistrationStatusRegistering.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressRegistrationStatusFailed.Visibility = System.Windows.Visibility.Visible;
            this.RegistrationErrorImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI when server configuration is in progress.
        /// </summary>
        private void ConfigureServerProgress()
        {
            this.InstProgressConfiguringServerStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressConfiguringServerStatusConfiguring.Visibility = System.Windows.Visibility.Visible;
            this.InstallationProgressCurrentOperationTextBlock.Text = StringResources.ServerConfigurationCurrentOperationText;
        }

        /// <summary>
        /// Update UI if server configuration is successful.
        /// </summary>
        private void ConfigureServerSuccess()
        {
            this.InstProgressConfiguringServerStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressConfiguringServerStatusConfiguring.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressConfiguringServerStatusDone.Visibility = System.Windows.Visibility.Visible;
            this.ConfiguringServerSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI if server configuration failed.
        /// </summary>
        private void ConfigureServerFailure()
        {
            this.InstProgressConfiguringServerStatusPending.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressConfiguringServerStatusConfiguring.Visibility = System.Windows.Visibility.Collapsed;
            this.InstProgressConfiguringServerStatusFailed.Visibility = System.Windows.Visibility.Visible;
            this.ConfiguringServerErrorImage.Visibility = System.Windows.Visibility.Visible;
            this.IncompleteOperationsErrorImage.Visibility = System.Windows.Visibility.Visible;
            this.InstProgressIncompleteOperationsTextBlock.Visibility = System.Windows.Visibility.Visible;
        }
        #endregion

        #region MouseDoubleClickEventHandlersOnTextbox
        /// <summary>
        /// Mouse double click event handler on CXTP status textbox.
        /// </summary>
        private void InstProgressCXTP_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            ValidationHelper.OpenFile(sysDrive + UnifiedSetupConstants.CXTPInstallLog);
        }

        /// <summary>
        /// Mouse double click event handler on CSPS status textbox.
        /// </summary>
        private void InstProgressCSPS_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            ValidationHelper.OpenFile(sysDrive + UnifiedSetupConstants.CXInstallLog);
        }

        /// <summary>
        /// Mouse double click event handler on MT status textbox.
        /// </summary>
        private void InstProgressMT_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            ValidationHelper.OpenFile(sysDrive + UnifiedSetupConstants.ASRUnifiedAgentInstallLog);
        }

        /// <summary>
        /// Mouse double click event handler on configure MT status textbox.
        /// </summary>
        private void InstProgressConfigureMT_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            ValidationHelper.OpenFile(sysDrive + UnifiedSetupConstants.ASRUnifiedAgentConfiguratorLog);
        }

        /// <summary>
        /// Mouse double click event handler on MySQL status textbox.
        /// </summary>
        private void InstProgressMySQL_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            ValidationHelper.OpenFile(sysDrive + UnifiedSetupConstants.MySQLDownloadLog);
        }

        /// <summary>
        /// Mouse double click event handler on MySQL status textbox.
        /// </summary>
        private void InstProgressIIS_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            ValidationHelper.OpenFile(sysDrive + UnifiedSetupConstants.ASRUnifiedSetupLog);
        }

        /// <summary>
        /// Mouse double click event handler on DRA status textbox.
        /// </summary>
        private void InstProgressDRA_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            string defaultDRALogFileName =
                        PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.DRAUTCLogName);
            ValidationHelper.OpenFile(sysDrive + UnifiedSetupConstants.DRALogFolder + defaultDRALogFileName);
        }

        /// <summary>
        /// Mouse double click event handler on MARS status textbox.
        /// </summary>
        private void InstProgressMARS_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            ValidationHelper.OpenFile(sysDrive + UnifiedSetupConstants.MARSStatusLog);
        }

        /// <summary>
        /// Mouse double click event handler on Configuration Manager status textbox.
        /// </summary>
        private void InstProgressConfigManager_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            ValidationHelper.OpenFile(sysDrive + UnifiedSetupConstants.ConfigManagerStatusLog);
        }

        /// <summary>
        /// Mouse double click event handler on Registration status textbox.
        /// </summary>
        private void InstProgressRegistration_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            string defaultDRALogFileName =
                        PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.DRAUTCLogName);
            ValidationHelper.OpenFile(sysDrive + UnifiedSetupConstants.DRALogFolder + defaultDRALogFileName);
        }

        /// <summary>
        /// Mouse double click event handler on Configuring Server status textbox.
        /// </summary>
        private void InstProgressConfiguringServer_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            ValidationHelper.OpenFile(SetupHelper.SetLogFilePath(UnifiedSetupConstants.SummaryStatusFileName));
        }
        #endregion

        #region OtherMethods
        /// <summary>
        /// Files to be removed at the end of installation.
        /// </summary>
        private void DeleteSetupFiles()
        {
            // Files to be removed at the end of installation.
            List<string> filestoDelete = new List<string>()
                {
                        SetupParameters.Instance().MysqlCredsFilePath,
                        SetupParameters.Instance().ProxySettingsFilePath,
                        SetupParameters.Instance().PassphraseFilePath,
                        SetupHelper.SetLogFilePath(UnifiedSetupConstants.PrerequisiteStatusFileName)
                };

            filestoDelete.ForEach(ValidationHelper.DeleteFile);
        }
        #endregion
    }
}