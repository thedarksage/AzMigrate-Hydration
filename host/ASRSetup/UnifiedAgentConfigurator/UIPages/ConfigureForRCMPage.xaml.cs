using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
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
    /// Interaction logic for ConfigureForRCMPage.xaml
    /// </summary>
    public partial class ConfigureForRCMPage : BasePageForWpfControls
    {
        /// <summary>
        /// Collection of operations.
        /// </summary>
        private ObservableCollection<Operation> operations;

        /// <summary>
        /// Initializes a new instance of the <see cref="ConfigureForRCMPage"/> class.
        /// </summary>
        public ConfigureForRCMPage()
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="ConfigureForRCMPage"/> class.
        /// </summary>
        /// <param name="page">Setup framework base page.</param>
        public ConfigureForRCMPage(ASRSetupFramework.Page page)
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
            this.Page.Host.SetNextButtonState(true, true, StringResources.Register);
            this.Page.Host.SetCancelButtonState(true, true, StringResources.CancelButtonText);
        }

        /// <summary>
        /// Starts registration process.
        /// </summary>
        /// <returns>false since we don't want to proceed to next page.</returns>
        public override bool OnNext()
        {
            this.Page.Host.SetNextButtonState(true, false);

            if (string.IsNullOrEmpty(this.txtCredentialsPath.Text))
            {
                return false;
            }

            this.PerformOperations();
            return false;
        }

        /// <summary>
        /// Performs operation related to installation and upgrade of the product.
        /// </summary>
        private void PerformOperations()
        {
            Thread performOps = new Thread(new ThreadStart(new Action(() =>
            {
                // Disable any navigation.
                this.txtCredentialsPath.Dispatcher.Invoke(
                    new Action(() =>
                    {
                        this.Page.Host.SetCancelButtonState(true, false);
                        this.Page.Host.SetNextButtonState(true, false);
                        this.Page.Host.SetPreviousButtonState(true, false);
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

                        this.operations.Add(new Operation
                        {
                            Action = new Operation.PerformOperation(RegistrationOperations.ConfigureProxySettings),
                            Component = StringResources.ConfigureProxyStep,
                            Error = string.Empty,
                            Status = OperationStatus.Waiting
                        });

                        this.operations.Add(new Operation
                        {
                            Action = new Operation.PerformOperation(RegistrationOperations.RegisterWithRCM),
                            Component = StringResources.RegistrationStep,
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
                    catch (Exception ex)
                    {
                        Trc.LogException(
                            LogLevel.Error,
                            "Failed to execute Task " + operations[i].Component,
                            ex);

                        opStatus = new OperationResult
                        {
                            Status = OperationStatus.Failed,
                            Error = StringResources.InternalError
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
                }

                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.CanClose, true);

                if (PropertyBagDictionary.Instance.GetProperty<bool>(
                    PropertyBagDictionary.RebootRequired) && preReqPassed)
                {
                    this.operationListView.Dispatcher.Invoke(
                        new Action(() =>
                        {
                            this.txtNextSteps.Text = StringResources.PostInstallationRebootRequired;
                            this.txtNextSteps.Visibility = System.Windows.Visibility.Visible;
                        }));

                }

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
            string credsPath = string.Empty;

            this.Dispatcher.Invoke(new Action(() =>
                credsPath = this.txtCredentialsPath.Text));
            return RegistrationOperations.PrepareForRCMRegistration(
                credsPath);
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
        /// Browse for credentials file.
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Event arguments.</param>
        private void BrowseCredentialsFile(object sender, RoutedEventArgs e)
        {
            var instlocButtonBrowser = new System.Windows.Forms.OpenFileDialog();
            instlocButtonBrowser.CheckFileExists = true;
            instlocButtonBrowser.Multiselect = false;
            instlocButtonBrowser.Title = StringResources.CredentialsPath;
            System.Windows.Forms.DialogResult dialogResult = instlocButtonBrowser.ShowDialog();
            if (dialogResult == System.Windows.Forms.DialogResult.OK)
            {
                this.txtCredentialsPath.Text = instlocButtonBrowser.FileName;
            }
        }
    }
}
