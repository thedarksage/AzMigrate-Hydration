//---------------------------------------------------------------
//  <copyright file="ConfigureForCSPage.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Page for configuring MS with MT.
//  </summary>
//
//  History:     04-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
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

namespace UnifiedAgentConfigurator
{
    /// <summary>
    /// Interaction logic for ConfigureForCS.xaml
    /// </summary>
    public partial class ConfigureForCSPage : BasePageForWpfControls
    {
        /// <summary>
        /// Collection of operations.
        /// </summary>
        private ObservableCollection<Operation> operations;

        /// <summary>
        /// Gets or sets the path of passphrase file path.
        /// </summary>
        private string PassphraseFilePath { get; set; }

        /// <summary>
        /// Gets or sets the Configuration server IP address.
        /// </summary>
        private string CSIP { get; set; }

        /// <summary>
        /// Gets or sets the CS passphrase.
        /// </summary>
        private string CSPassphrase { get; set; }

        /// <summary>
        /// Gets or sets the CS Port
        /// </summary>
        private string CSPort { get; set; }

        /// <summary>
        /// Initializes a new instance of the <see cref="ConfigureForCSPage"/> class.
        /// </summary>
        public ConfigureForCSPage()
        {
            InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="ConfigureForCSPage"/> class.
        /// </summary>
        /// <param name="page">Setup framework base page.</param>
        public ConfigureForCSPage(ASRSetupFramework.Page page)
            : base(page, StringResources.registration, 2)
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

            Platform platform = PropertyBagDictionary.Instance.GetProperty<Platform>(
                PropertyBagConstants.Platform);

            this.Page.Host.SetNextButtonState(true, true, StringResources.Register);
            this.Page.Host.SetCancelButtonState(true, true, StringResources.CancelButtonText);

            if (platform == Platform.VmWare)
            {
                this.Page.Host.SetPreviousButtonState(false, false);
            }

            this.PassphraseFilePath = Path.Combine(
                Path.GetPathRoot(Environment.SystemDirectory),
                ConfigurationConstants.TempPassphrasePath);
        }

        /// <summary>
        /// Starts registration process.
        /// </summary>
        /// <returns>false since we don't want to proceed to next page.</returns>
        public override bool OnNext()
        {
            this.Page.Host.SetNextButtonState(true, false);
            this.PerformOperations();
            return false;
        }

        /// <summary>
        /// Actions performed on cancel button click .
        /// </summary>
        /// <returns>true always</returns>
        public override bool OnCancel()
        {
            return true;
        }

        /// <summary>
        /// Performs operation related to installation and upgrade of the product.
        /// </summary>
        private void PerformOperations()
        {
            Platform platform = PropertyBagDictionary.Instance.GetProperty<Platform>(
                PropertyBagConstants.Platform);

            Thread performOps = new Thread(new ThreadStart(new Action(() =>
            {
                // Disable any navigation.
                this.txtCSIP.Dispatcher.Invoke(
                    new Action(() =>
                    {
                        this.Page.Host.SetCancelButtonState(true, false);
                        this.Page.Host.SetNextButtonState(true, false);
                        this.Page.Host.SetPreviousButtonState(false, false);
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
                            Action = new Operation.PerformOperation(this.PrepareForRegistration),
                            Component = StringResources.PrepairingForRegistrationStep,
                            Error = string.Empty,
                            Status = OperationStatus.Waiting
                        });

                        if (platform != Platform.VmWare)
                        {
                            this.operations.Add(new Operation
                            {
                                Action = new Operation.PerformOperation(RegistrationOperations.ConfigureProxySettings),
                                Component = StringResources.ConfigureProxyStep,
                                Error = string.Empty,
                                Status = OperationStatus.Waiting
                            });
                        }

                        this.operations.Add(new Operation
                        {
                            Action = new Operation.PerformOperation(RegistrationOperations.RegisterWithCS),
                            Component = StringResources.RegistrationStep,
                            Error = string.Empty,
                            Status = OperationStatus.Waiting
                        });

                        this.operations.Add(new Operation
                        {
                            Action = new Operation.PerformOperation(RegistrationOperations.ValidateCSRegistration),
                            Component = StringResources.PostRegistrationActions,
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
                    PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.ExitCode,
                        opStatus.ProcessExitCode);
                }

                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.CanClose, true);

                if (PropertyBagDictionary.Instance.GetProperty<bool>(
                    PropertyBagDictionary.RebootRequired))
                {
                    this.operationListView.Dispatcher.Invoke(
                        new Action(() =>
                            {
                                this.txtNextSteps.Text = StringResources.PostInstallationRebootRequired;
                                this.txtNextSteps.Visibility = System.Windows.Visibility.Visible;
                            }));
                    
                }

                // To do:
                // Check if we need additional error code for cloud pairing incomplete during manual install
                // This error code will appear only during manual install
                /*
                if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.CloudPairingStatus)
                    && PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.CloudPairingStatus) == false)
                {
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ExitCode, "NEW ERROR CODE FOR CLOUD PARING FAILURE");
                }
                */

                this.operationListView.Dispatcher.Invoke(
                    new Action(() =>
                        this.Page.Host.SetCancelButtonState(true, true, StringResources.FinishButtonText)));

                if (!preReqPassed)
                {
                    // Update configuration status as failed.
                    RegistrationOperations.UpdateConfigurationStatus(UnifiedSetupConstants.FailedStatus);
                    this.operationListView.Dispatcher.Invoke(new Action(() =>
                    {
                        this.txtNextSteps.Text = string.Format(
                            StringResources.RegistrationFailureLogLocation,
                            SetupHelper.SetLogFilePath(
                                PropertyBagDictionary.Instance.GetProperty<string>(
                                    PropertyBagConstants.DefaultLogName)));
                        this.txtNextSteps.Visibility = System.Windows.Visibility.Visible;
                    }));
                }
                else
                {
                    // Update configuration status as success.
                    RegistrationOperations.UpdateConfigurationStatus(UnifiedSetupConstants.SuccessStatus);
                }
            })));

            performOps.Start();
        }

        /// <summary>
        /// Prepares for registration
        /// </summary>
        /// <returns>Operation result.</returns>
        private OperationResult PrepareForRegistration()
        {
            // A. Fetch details from UI.
            this.txtCSIP.Dispatcher.Invoke(
                new Action(() =>
                {
                    this.CSIP = this.txtCSIP.Text;
                    this.CSPassphrase = this.txtCSPassphrase.Password;
                    this.CSPort = this.txtCSPORT.Text;
                }));

            // B. Dump passpharse into file.
            Directory.CreateDirectory(Path.GetDirectoryName(this.PassphraseFilePath));

            File.WriteAllText(this.PassphraseFilePath, this.CSPassphrase);

            // C. Add CSPort to Property Bag Dictionary
            if(!(string.IsNullOrEmpty(this.CSPort) || this.CSPort.Trim().Length == 0))
            {
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.CSPort, this.CSPort);
            }
            else
            {
                // Add default value
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.CSPort, UnifiedSetupConstants.CSPort);
            }

            // D. Validate passphrase.
            return RegistrationOperations.PrepareForCSRegistration(
                this.CSIP,
                this.PassphraseFilePath);
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
        /// Next button state handler.
        /// </summary>
        private void UpdateNextbuttonstate()
        {
            if (!string.IsNullOrEmpty(this.txtCSIP.Text))
            {
                bool isIPAddessValid = true;

                if (isIPAddessValid)
                {
                    this.successImageCSIP.Visibility = System.Windows.Visibility.Visible;
                    this.errorImageCSIP.Visibility = System.Windows.Visibility.Hidden;
                }
                else
                {
                    this.successImageCSIP.Visibility = System.Windows.Visibility.Hidden;
                    this.errorImageCSIP.Visibility = System.Windows.Visibility.Visible;
                }

                if (!string.IsNullOrEmpty(this.txtCSPassphrase.Password))
                {
                    if (isIPAddessValid)
                    {
                        this.Page.Host.SetNextButtonState(true, true);
                    }
                    else
                    {
                        this.Page.Host.SetNextButtonState(true, false);
                    }
                }
                else
                {
                    this.Page.Host.SetNextButtonState(true, false);
                }
            }
        }

        /// <summary>
        /// Configuration Server IP textbox change handler.
        /// </summary>
        private void txtCSIP_TextChanged(object sender, TextChangedEventArgs e)
        {
            this.ClearUI();
            this.UpdateNextbuttonstate();
        }

        /// <summary>
        /// Configuration Server passphrase change handler.
        /// </summary>
        private void txtCSPassphrase_PasswordChanged(object sender, RoutedEventArgs e)
        {
            this.ClearUI();
            this.UpdateNextbuttonstate();
        }

        /// <summary>
        /// Configuration server port change handler
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void txtPORT_TextChanged(object sender, TextChangedEventArgs e)
        {
            this.ClearUI();
            this.UpdateNextbuttonstate();
        }

        /// <summary>
        /// Clear the UI when Ipaddress/passphrase is modified.
        /// </summary>
        private void ClearUI()
        {
            this.operations.Clear();
            this.txtNextSteps.Text = string.Empty;
        }
    }
}
