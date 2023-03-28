//---------------------------------------------------------------
//  <copyright file="ReplicationApplianceDetailsPage.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Validates passphrase.
//  </summary>
//
//  History:     30-Sep-2015   bhayachi     Created
//----------------------------------------------------------------

namespace UnifiedSetup
{
    using System;
    using System.Collections.Generic;
    using System.Net;
    using System.Security;
    using System.Text.RegularExpressions;
    using System.Threading;
    using System.Windows;
    using System.Windows.Controls;
    using System.Windows.Threading;
    using System.IO;
    using ASRResources;
    using ASRSetupFramework;

    /// <summary>
    /// Interaction logic for ReplicationApplianceDetailsPage.xaml
    /// </summary>
    public partial class ReplicationApplianceDetailsPage : BasePageForWpfControls
    {
        #region Delegates
        /// <summary>
        /// The delegate used to navigate to next page.
        /// </summary>
        private delegate void NavigateDelegate();

        /// <summary>
        /// The delegate used to validate vSphereCLI.
        /// </summary>
        private delegate void CheckPassphraseDelegate();

        /// <summary>
        /// The delegate used to enable next button.
        /// </summary>
        private delegate void EnableButtonDelegate();

        /// <summary>
        /// The delegate used to update UI elements.
        /// </summary>
        private delegate void UpdateUIDelegate();
        #endregion

        #region Fields
        /// <summary>
        /// Get systemdrive.
        /// </summary>
        public string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);

        /// <summary>
        /// Type of navigation if validation succeeds.
        /// </summary>
        private PageNavigation.Navigation navType;

        /// <summary>
        /// Page to jump to if validation succeeds.
        /// </summary>
        private string pageId;
        #endregion

        public ReplicationApplianceDetailsPage(ASRSetupFramework.Page page)
            : base(page, StringResources.configuration_server_details, 3)
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="ReplicationApplianceDetailsPage"/> class.
        /// </summary>
        public ReplicationApplianceDetailsPage()
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Enters the page.
        /// </summary>
        public override void EnterPage()
        {
            base.EnterPage();
            updateNextbuttonstate();
            this.CSDetailsNoteTextblock.Text = string.Format(StringResources.CSDetailsNoteText, sysDrive);
            this.CSDetailsNoteTextblock.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Overrides OnNext.
        /// </summary>
        /// <returns>True if previous button is to be shown, false otherwise.</returns>
        public override bool OnPrevious()
        {
            return true;
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
        /// Trigger installation in a new non-UI thread.
        /// </summary>
        public void TriggerValidationInNewThread()
        {
            ThreadPool.QueueUserWorkItem(new WaitCallback(this.ValidatePassphrase));
        }

        /// <summary>
        /// Navigate page based on last user input.
        /// </summary>
        private void NavigatePage()
        {
            if (this.navType == PageNavigation.Navigation.Next)
            {
                PageNavigation.Instance.MoveToNextPage(false);
            }
            else if (this.navType == PageNavigation.Navigation.Previous)
            {
                PageNavigation.Instance.MoveToPreviousPage(false);
            }
            else if (this.navType == PageNavigation.Navigation.Jump)
            {
                PageNavigation.Instance.MoveToPage(this.pageId);
            }
        }

        /// <summary>
        /// Update UI when Passphrase validation succeeds.
        /// </summary>
        private void updatePassphraseCorrectUI()
        {
            this.CSDetailsValidatePassphraseTextblock.Visibility = System.Windows.Visibility.Collapsed;
            this.CSDetailsValidatePassphraseProgress.Visibility = System.Windows.Visibility.Collapsed;

            this.Page.Host.SetPreviousButtonState(true, true);
            this.Page.Host.SetNextButtonState(true, true);
            this.Page.Host.SetCancelButtonState(true, true);

            // Enable global navigation
            this.Page.Host.EnableGlobalSideBarNavigation();
        }

        /// <summary>
        /// Update UI when Passphrase validation fails.
        /// </summary>
        private void updatePassphraseInCorrectUI()
        {
            this.CSDetailsValidatePassphraseTextblock.Visibility = System.Windows.Visibility.Collapsed;
            this.CSDetailsValidatePassphraseProgress.Visibility = System.Windows.Visibility.Collapsed;
            this.CSDetailsIncorrectPassphraseTextblock.Visibility = System.Windows.Visibility.Visible;
            this.CSDetailsIncorrectPassphraseErrorImage.Visibility = System.Windows.Visibility.Visible;

            this.Page.Host.SetPreviousButtonState(true, true);
            this.Page.Host.SetCancelButtonState(true, true);
        }

        /// <summary>
        /// Validate Passphrase and update UI accordingly.
        /// </summary>
        private void ValidatePassphrase(object o)
        {
            string path = SetupParameters.Instance().PassphraseFilePath;
            string ip = SetupParameters.Instance().CSIP;
            string port = "443";

            if (ValidationHelper.ValidateConnectionPassphrase(path, ip, port))
            {
                Trc.Log(LogLevel.Always, "User has entered correct Configuration Server Passphrase.");
                Trc.Log(LogLevel.Always, "Invoke thread that calls updatePassphraseCorrectUI method.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UpdateUIDelegate(this.updatePassphraseCorrectUI));

                SetupParameters.Instance().IsPassphraseValid = "Yes";
                Trc.Log(LogLevel.Always, "Set IsPassphraseValid to {0}", SetupParameters.Instance().IsPassphraseValid);
                
                this.Dispatcher.BeginInvoke(new NavigateDelegate(this.NavigatePage));
            }
            else
            {
                Trc.Log(LogLevel.Always, "User has entered wrong Configuration Server Passphrase.");
                Trc.Log(LogLevel.Always, "Invoke thread that calls updatePassphraseInCorrectUI method.");
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UpdateUIDelegate(this.updatePassphraseInCorrectUI));
            }
        }

        /// <summary>
        /// Validates the page.
        /// The default implementation always returns true for 'validated'.
        /// </summary>
        /// <param name="navType">Navigation type.</param>
        /// <param name="pageId">Page Id being navigated to.</param>
        /// <returns>true if valid</returns>
        public override bool ValidatePage(
            PageNavigation.Navigation navType,
            string pageId = null)
        {
            bool isValidPage = true;

            this.navType = navType;
            this.pageId = pageId;

            if (!string.IsNullOrEmpty(this.CSIPDetailsIPTextbox.Text) && !string.IsNullOrEmpty(this.CSDetailsPassphrasePasswordbox.Password))
            {
                if (SetupParameters.Instance().IsPassphraseValid == "Yes")
                {
                    Trc.Log(LogLevel.Always, "Moving to next page as Passphrase is already validated.");
                }
                else
                {
                    try
                    {
                        string line = this.CSDetailsPassphrasePasswordbox.Password;
                        string ip = this.CSIPDetailsIPTextbox.Text;
                        string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);
                        string path = sysDrive + @"Temp\ASRSetup\Passphrase.txt";
                        string dir = sysDrive + @"Temp\ASRSetup";

                        Directory.CreateDirectory(dir);
                        File.WriteAllText(path, line);

                        SetupParameters.Instance().CSIP = ip;
                        Trc.Log(LogLevel.Always, "Set CSIP to {0}", SetupParameters.Instance().CSIP);

                        SetupParameters.Instance().PassphraseFilePath = path;
                        Trc.Log(LogLevel.Always, "Set PassphraseFilePath to {0}", SetupParameters.Instance().PassphraseFilePath);

                        // Disable global navigation
                        this.Page.Host.DisableGlobalSideBarNavigation();

                        this.CSDetailsIncorrectPassphraseTextblock.Visibility = System.Windows.Visibility.Collapsed;
                        this.CSDetailsIncorrectPassphraseErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                        this.CSDetailsValidatePassphraseTextblock.Visibility = System.Windows.Visibility.Visible;
                        this.CSDetailsValidatePassphraseProgress.Visibility = System.Windows.Visibility.Visible;

                        this.Page.Host.SetNextButtonState(true, false);
                        this.Page.Host.SetPreviousButtonState(true, false);
                        this.Page.Host.SetCancelButtonState(true, false);

                        this.Dispatcher.BeginInvoke(DispatcherPriority.Background, new CheckPassphraseDelegate(this.TriggerValidationInNewThread));
                        isValidPage = false;
                    }
                    catch (Exception e)
                    {
                        Trc.Log(LogLevel.Always, "Exception occured: {0}", e.ToString());
                        isValidPage = false;
                    }
                }
            }
            return isValidPage;
        }

        /// <summary>
        /// Next button state handler.
        /// </summary>
        private void updateNextbuttonstate()
        {
            this.CSDetailsIncorrectPassphraseTextblock.Visibility = System.Windows.Visibility.Collapsed;
            this.CSDetailsIncorrectPassphraseErrorImage.Visibility = System.Windows.Visibility.Collapsed;

            if (!string.IsNullOrEmpty(this.CSIPDetailsIPTextbox.Text) && !string.IsNullOrEmpty(this.CSDetailsPassphrasePasswordbox.Password))
            {
                // IP validation.
                if (ValidationHelper.ValidateIPAddress(this.CSIPDetailsIPTextbox.Text))
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

        /// <summary>
        /// Configuration Server IP textbox change handler.
        /// </summary>
        private void CSIPDetailsIPTextbox_TextChanged(object sender, TextChangedEventArgs e)
        {
            SetupParameters.Instance().IsPassphraseValid = "Not Validated";

            // Enable the next button only when both CS IP and Passphrase are entered.
            updateNextbuttonstate();
        }

        /// <summary>
        /// Configuration Server password change handler.
        /// </summary>
        private void CSDetailsPassphrasePasswordbox_PasswordChanged(object sender, RoutedEventArgs e)
        {
            SetupParameters.Instance().IsPassphraseValid = "Not Validated";
            Trc.Log(LogLevel.Always, "Set IsPassphraseValid to {0}", SetupParameters.Instance().IsPassphraseValid);

            // Enable the next button only when both CS IP and Passphrase are entered.
            updateNextbuttonstate();
        }
    }
}
