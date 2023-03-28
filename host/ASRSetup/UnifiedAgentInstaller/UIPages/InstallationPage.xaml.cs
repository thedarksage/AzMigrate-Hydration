//---------------------------------------------------------------
//  <copyright file="InstallationPage.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Page to perform installation\upgrade operations.
//  </summary>
//
//  History:     03-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using ASRResources;
using ASRSetupFramework;
using System.Diagnostics;

namespace UnifiedAgentInstaller
{
    /// <summary>
    /// Interaction logic for InstallationPage.xaml
    /// </summary>
    public partial class InstallationPage : BasePageForWpfControls
    {
        /// <summary>
        /// Collection of operations.
        /// </summary>
        ObservableCollection<Operation> operations;

        /// <summary>
        /// Gets or sets a value indicating whether install has succeeded or not.
        /// </summary>
        private bool InstallSucceeded { get; set; }

        /// <summary>
        /// Initializes a new instance of the <see cref="InstallationPage"/> class.
        /// </summary>
        public InstallationPage()
        {
            InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="InstallationPage"/> class.
        /// </summary>
        /// <param name="page">Setup framework base page.</param>
        public InstallationPage(ASRSetupFramework.Page page)
            : base(page, StringResources.installation_progress, 2)
        {
            this.InitializeComponent();
            this.operations = new ObservableCollection<Operation>();
            this.operationListView.ItemsSource = this.operations;
        }

        /// <summary>
        /// Executes actions on entering the page.
        /// </summary>
        public override void EnterPage()
        {
            base.EnterPage();
            this.Page.Host.SetNextButtonState(true, true, StringResources.Install);
            this.Page.Host.SetCancelButtonState(true, true, StringResources.CancelButtonText);

            if (PropertyBagDictionary.Instance.GetProperty<SetupAction>(
                    PropertyBagConstants.SetupAction) == SetupAction.Install &&
               PropertyBagDictionary.Instance.GetProperty<Platform>(
                    PropertyBagConstants.Platform) == Platform.VmWare)
            {
                this.Page.Host.SetPreviousButtonState(true, true);
            }
            else
            {
                this.Page.Host.SetPreviousButtonState(false, false);
            }

            this.Page.Host.SetPreviousButtonState(false, false);
            this.txtInstallLocation.Text = PropertyBagDictionary.Instance.GetProperty<string>(
                PropertyBagConstants.InstallationLocation);

            this.lblinstallComponentText.Text = string.Format(
                StringResources.InstallationChoiceText,
                PropertyBagDictionary.Instance.GetProperty<SetupAction>(
                    PropertyBagConstants.SetupAction) == SetupAction.Upgrade ? 
                        StringResources.UpgradeAction : 
                        StringResources.InstallAction,
                PropertyBagDictionary.Instance.GetProperty<AgentInstallRole>(
                    PropertyBagConstants.InstallRole) == AgentInstallRole.MS ?
                        StringResources.MobilityServerName :
                        StringResources.MasterTargetServer);

            if (PropertyBagDictionary.Instance.GetProperty<SetupAction>(
                PropertyBagConstants.SetupAction) != SetupAction.Install)
            {
                this.buttonBrowse.IsEnabled = false;
            }
        }

        /// <summary>
        /// Starts installation process.
        /// </summary>
        /// <returns>false since we don't want to proceed to next page.</returns>
        public override bool OnNext()
        {
            this.Page.Host.SetNextButtonState(true, false);

            if (!this.InstallSucceeded)
            {
                this.PerformInstallation();
            }
            else
            {
                this.StartRegistration();
            }
            return false;
        }

        /// <summary>
        /// Performs operation related to installation and upgrade of the product.
        /// </summary>
        private void PerformInstallation()
        {
            Thread performOps = new Thread(new ThreadStart(new Action(() =>
                {
                    // Disable any navigation.
                    this.txtInstallLocation.Dispatcher.Invoke(
                        new Action(() =>
                        {
                            this.Page.Host.SetCancelButtonState(true, false);
                            this.Page.Host.SetNextButtonState(true, false);
                        }));

                    /* Add all the steps to be executed during installation.
                     * There is a bug in .net3.5 which causes list to not 
                     * updated if the collection elements are changed. Hence we 
                     * need to refresh the listview everytime a change elements
                     * values.
                     */
                    this.operationListView.Dispatcher.Invoke(
                        new Action(() =>
                            {
                                this.operations.Clear();

                                this.operations.Add(new Operation
                                    {
                                        Action = new Operation.PerformOperation(this.PrepareForInstall),
                                        Component = StringResources.PrepareForInstallStep,
                                        Error = string.Empty,
                                        Status = OperationStatus.Waiting
                                    });

                                this.operationListView.Items.Refresh();
                            }),
                            System.Windows.Threading.DispatcherPriority.Render);

                    // Invoke all tasks one by one.
                    // Using for loop to allow Tasks to queue more tasks in case required.
                    bool preReqPassed = true;
                    for (int i = 0; i < operations.Count; i++)
                    {
                        if (!preReqPassed)
                        {
                            this.operationListView.Dispatcher.Invoke(
                                new Action(() =>
                                {
                                    operations[i].Status = OperationStatus.Skipped;
                                    this.operationListView.Items.Refresh();
                                }));

                            continue;
                        }

                        // Set the operation status to inprogress and invoke it.
                        this.operationListView.Dispatcher.Invoke(
                            new Action(() =>
                            {
                                operations[i].Status = OperationStatus.InProgress;
                                this.operationListView.Items.Refresh();
                            }),
                            System.Windows.Threading.DispatcherPriority.Render);

                        OperationResult opStatus = null;

                        try
                        {
                            opStatus = operations[i].Action.Invoke();
                        }
                        catch (OutOfMemoryException)
                        {
                            opStatus = new OperationResult
                            {
                                Status = OperationStatus.Failed,
                                Error = StringResources.OutOfMemory,
                                ProcessExitCode = SetupHelper.UASetupReturnValues.OutOfMemoryException
                            };
                        }
                        catch (Exception ex)
                        {
                            Trc.LogException(
                                LogLevel.Error,
                                "Failed to execute Task " + operations[i].Component,
                                ex);

                            opStatus = new OperationResult
                            {
                                Status = OperationStatus.Failed,
                                Error = StringResources.InternalError,
                                ProcessExitCode = SetupHelper.UASetupReturnValues.Failed
                            };
                        }

                        // Update operation status.
                        this.operationListView.Dispatcher.Invoke(
                            new Action(() =>
                            {
                                operations[i].Status = opStatus.Status;
                                operations[i].Error = opStatus.Error;
                                if (!string.IsNullOrEmpty(opStatus.Error))
                                {
                                    SetupHelper.ShowErrorUA(opStatus.Error);

                                    if (opStatus.Status == OperationStatus.Failed)
                                    {
                                        preReqPassed = false;
                                    }
                                }

                                this.operationListView.Items.Refresh();
                            }),
                            System.Windows.Threading.DispatcherPriority.Render);
                        Trc.Log(LogLevel.Always, "opStatus.Status - {0}", opStatus.Status);
                        Trc.Log(LogLevel.Always, "opStatus.ProcessExitCode - {0}", opStatus.ProcessExitCode);
                        SetupHelper.UASetupReturnValues exitCode = opStatus.ProcessExitCode;
                        //Handle cases where previous tasks returned warning and current task returns success
                        if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.ExitCode))
                        {
                            if (PropertyBagDictionary.Instance.GetProperty<int>(PropertyBagConstants.ExitCode) == (int)SetupHelper.UASetupReturnValues.SucceededWithWarnings)
                                exitCode = SetupHelper.UASetupReturnValues.SucceededWithWarnings;
                        }
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ExitCode,
                            opStatus.Status == OperationStatus.Failed ? opStatus.ProcessExitCode : exitCode);
                    }

                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.CanClose, true);
                    PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.InstallationFinished,
                        true);

                    if (!preReqPassed)
                    {
                        this.operationListView.Dispatcher.Invoke(new Action(() =>
                            {
                                this.Page.Host.SetNextButtonState(false, false);
                                this.Page.Host.SetCancelButtonState(true, true, StringResources.CancelButtonText);
                                this.lblNextStep.Content = string.Format(
                                    StringResources.InstallationPageErrorLogLocation, 
                                    SetupHelper.SetLogFilePath(
                                        PropertyBagDictionary.Instance.GetProperty<string>(
                                            PropertyBagConstants.DefaultLogName)));
                            }));
                    }
                    else
                    {
                        // If install\upgrade worked and agent is not configured. Let
                        // user configure.
                        this.InstallSucceeded = true;
                        if (!SetupHelper.IsAgentConfigured())
                        {
                            this.lblNextStep.Dispatcher.Invoke(new Action(() =>
                                {
                                    this.lblNextStep.Content = StringResources.ConfigureAgent;
                                    this.lblNextStep.Visibility = System.Windows.Visibility.Visible;
                                    this.Page.Host.SetNextButtonState(true, true, StringResources.ProceedToConfiguration);
                                    this.Page.Host.SetCancelButtonState(true, false);
                                }));
                        }
                        else
                        {
                            this.lblNextStep.Dispatcher.Invoke(new Action(() =>
                                {
                                    this.lblNextStep.Content = StringResources.AgentAlreadyConfigured;
                                    this.lblNextStep.Visibility = System.Windows.Visibility.Visible;
                                    this.Page.Host.SetNextButtonState(false, false);
                                    this.Page.Host.SetCancelButtonState(true, true, StringResources.FinishButtonText);
                                }));
                        }
                    }
                })));

            performOps.Start();
        }

        /// <summary>
        /// Starts registration and exits this process.
        /// </summary>
        private void StartRegistration()
        {
            Process.Start(
                Path.Combine(
                    PropertyBagDictionary.Instance.GetProperty<string>(
                        PropertyBagConstants.InstallationLocation),
                    UnifiedSetupConstants.AgentConfiguratorExeName));
            this.Page.Host.Close();
        }

        /// <summary>
        /// Prepares for Installation or upgrade. Evaluate all pre-conditions and
        /// shut down running services.
        /// </summary>
        /// <returns>Final operation status.</returns>
        private OperationResult PrepareForInstall()
        {
            // A. Set values for next tasks to come.
            SetupAction setupAction = PropertyBagDictionary.Instance.GetProperty<SetupAction>(
                PropertyBagConstants.SetupAction);

            if (!ValidationHelper.IsCsPrime())
            {
                // this function always returns a value, so no need to add additional checks for setup action
                ConfigurationServerType csType = SetupHelper.GetCSTypeEnumFromDrScout();

                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CSType,
                    csType);
            }

            if (setupAction == SetupAction.CheckFilterDriver)
            {
                this.operationListView.Dispatcher.Invoke(
                    new Action(() =>
                        {
                            this.operations.Add(new Operation
                            {
                                Action = new Operation.PerformOperation(InstallationOperations.CheckFilterDriverStatus),
                                Component = StringResources.CheckDriverStatusStep,
                                Error = string.Empty,
                                Status = OperationStatus.Waiting
                            });

                            this.operationListView.Items.Refresh();
                        }));

                return new OperationResult
                {
                    Status = OperationStatus.Completed
                };
            }
            else if (setupAction == SetupAction.ExecutePostInstallSteps)
            {
                // Set setup action to install/upgrade.
                setupAction = InstallActionProcessor.GetAgentInstallAction();
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.SetupAction, setupAction);

                this.operationListView.Dispatcher.Invoke(
                    new Action(() =>
                    {
                        this.operations.Add(new Operation
                        {
                            Action = new Operation.PerformOperation(InstallationOperations.PostInstallationSteps),
                            Component = StringResources.PostInstallActionsStep,
                            Error = string.Empty,
                            Status = OperationStatus.Waiting
                        });

                        this.operationListView.Items.Refresh();
                    }));

                return new OperationResult
                {
                    Status = OperationStatus.Completed
                };
            }
            else
            {
                this.operationListView.Dispatcher.Invoke(
                    new Action(() =>
                    {

                        this.operations.Add(new Operation
                        {
                            Action = new Operation.PerformOperation(InstallationOperations.InstallComponents),
                            Component = StringResources.InstallationStep,
                            Error = string.Empty,
                            Status = OperationStatus.Waiting
                        });

                        this.operations.Add(new Operation
                        {
                            Action = new Operation.PerformOperation(InstallationOperations.PostInstallationSteps),
                            Component = StringResources.PostInstallationStep,
                            Error = string.Empty,
                            Status = OperationStatus.Waiting
                        });

                    }));
            }

            // At this point we should only get install or upgrade. Anything else
            // we won't be handling.
            this.txtInstallLocation.Dispatcher.Invoke(
                new Action(() =>
                {
                    PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.InstallationLocation, 
                        this.txtInstallLocation.Text);
                }));

            OperationResult opResult = InstallationOperations.PrepareForInstall();

            if (opResult.Status == OperationStatus.Failed)
            {
                return opResult;
            }

            if (setupAction == SetupAction.Upgrade && 
                SetupHelper.IsAgentConfigured())
            {
                this.operationListView.Dispatcher.Invoke(
                    new Action(() =>
                    {
                        this.operations.Add(new Operation
                        {
                            Action = new Operation.PerformOperation(InstallationOperations.StartAgentServices),
                            Component = StringResources.StartServicesStep,
                            Error = string.Empty,
                            Status = OperationStatus.Waiting
                        });

                        this.operationListView.Items.Refresh();
                    }));
            }

            return opResult;
        }

        /// <summary>
        /// Shows error for the failed step.
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Event arguments.</param>
        private void ShowErrorDetails(object sender, RoutedEventArgs e)
        {
            Point position = (sender as Button).PointToScreen(new Point(0d, 0d)),
            controlPosition = this.PointToScreen(new Point(0d, 0d));

            AutoHidingDialog dialog = new AutoHidingDialog(
                (int)position.Y,
                (int)position.X,
                ((sender as Button).DataContext as Operation).Error);
            dialog.ShowInTaskbar = false;
            dialog.Show();
        }

        /// <summary>
        /// Handles click event for browse button.
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Event arguments.</param>
        private void buttonBrowse_Click(object sender, RoutedEventArgs e)
        {
            var instlocButtonBrowser = new System.Windows.Forms.FolderBrowserDialog();
            System.Windows.Forms.DialogResult dialogResult = instlocButtonBrowser.ShowDialog();
            if (dialogResult == System.Windows.Forms.DialogResult.OK)
            {
                this.txtInstallLocation.Text = Path.Combine(
                    instlocButtonBrowser.SelectedPath, 
                    UnifiedSetupConstants.UnifiedAgentDirectorySuffix);
            }
        }
    }
}
