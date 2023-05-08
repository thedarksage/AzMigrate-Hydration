//---------------------------------------------------------------
//  <copyright file="EnvironmentDetailsPage.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Confirm whether user wants to protect VMware VMs.
//  </summary>
//
//  History:     28-Sep-2015   bhayachi     Created
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
    using System.Windows.Documents;
    using System.Diagnostics;
    using System.ComponentModel;
    using ASRResources;
    using ASRSetupFramework;

    /// <summary>
    /// Interaction logic for EnvironmentDetailsPage.xaml
    /// </summary>
    public partial class EnvironmentDetailsPage : BasePageForWpfControls
    {
        #region Delegates
        /// <summary>
        /// The delegate used to navigate to next page.
        /// </summary>
        private delegate void NavigateDelegate();

        /// <summary>
        /// The delegate used to validate vSphereCLI.
        /// </summary>
        private delegate void CheckvSphereCLIDelegate();

        /// <summary>
        /// The delegate used to enable next button.
        /// </summary>
        private delegate void EnableButtonDelegate();

        /// <summary>
        /// The delegate used to update UI elements.
        /// </summary>
        private delegate void UpdateUIDelegate();
        #endregion 

        /// <summary>
        /// Trigger installation in a new non-UI thread.
        /// </summary>
        public void TriggerValidationInNewThread()
        {
            ThreadPool.QueueUserWorkItem(new WaitCallback(this.ValidateVMwareTools));
        }

        /// <summary>
        /// Interaction logic for EnvironmentDetailsPage.xaml
        /// </summary>
        public EnvironmentDetailsPage(ASRSetupFramework.Page page)
            : base(page, StringResources.environment_details, 4)
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="EnvironmentDetailsPage"/> class.
        /// </summary>
        public EnvironmentDetailsPage()
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Enters the page.
        /// </summary>
        public override void EnterPage()
        {
            base.EnterPage();
            IsNextButtonEnabled();
        }

        /// <summary>
        /// Next button state handler.
        /// </summary>
        private void IsNextButtonEnabled()
        {
            if ((this.EnvironmentDetailsPageNoRadioButton.IsChecked.Value) || (this.EnvironmentDetailsPageYesRadioButton.IsChecked.Value))
            {
                this.Page.Host.SetNextButtonState(true, true);
            }
            else
            {
                this.Page.Host.SetNextButtonState(true, false);
            }
        }
        
        /// <summary>
        /// Update UI when VMware Tools exists.
        /// </summary>
        private void updateVMwareToolsExistsUI()
        {
            this.EnvironmentDetailsPageVMwareToolsExistsTextBlock.Visibility = System.Windows.Visibility.Visible;
            this.EnvPageVMwareToolsSuccessImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI when VMware Tools does not exist.
        /// </summary>
        private void updateVMwareToolsNotExistsUI()
        {
            this.EnvironmentDetailsPageVMwareToolsNotExistsTextBlock.Visibility = System.Windows.Visibility.Visible;
            this.EnvPageVMwareToolsErrorImage.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Update UI after VMware Tools validation.
        /// </summary>
        private void UpdateUIAfterValidation()
        {
            this.EnvironmentDetailsPageNoRadioButton.IsEnabled = true;
            this.EnvironmentDetailsPageFetchCLITextBlock.Visibility = System.Windows.Visibility.Collapsed;
            this.EnvironmentDetailsPageFetchCLIProgress.Visibility = System.Windows.Visibility.Collapsed;
        }

        // <summary>
        /// Update UI when VMware Tools exist.
        /// </summary>
        private void UpdateUIValidationSucess()
        {
            this.Page.Host.SetPreviousButtonState(true, true);
			this.Page.Host.EnableGlobalSideBarNavigation();
            this.Page.Host.SetCancelButtonState(true, true);
            PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.CanClose, false);
            this.Page.Host.SetNextButtonState(true, true);
        }

        // <summary>
        /// Update UI when VMware Tools not exist.
        /// </summary>
        private void UpdateUIValidationFailure()
        {
            this.Page.Host.SetPreviousButtonState(true, false);
			PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.CanClose, true);
            this.Page.Host.SetCancelButtonState(true, true, StringResources.ExitButtonText);
        }

        /// <summary>
        /// Handles the request navigate.
        /// </summary>
        private void HandleRequestNavigate(object sender, RoutedEventArgs e)
        {
            string navigateUri = ((Hyperlink)sender).NavigateUri.ToString();
            Process.Start(new ProcessStartInfo(navigateUri));
            e.Handled = true;
        }

        /// <summary>
        /// Invoke threads depending on VMware Tools existence.
        /// </summary>
        private void ValidateVMwareTools(object o)
        {
            bool isVMwareToolsExists = ValidationHelper.isVMwareToolsExists();
            if (isVMwareToolsExists)
            {
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UpdateUIDelegate(this.updateVMwareToolsExistsUI));
            }
            else
            {
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UpdateUIDelegate(this.updateVMwareToolsNotExistsUI));
            }

            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UpdateUIDelegate(this.UpdateUIAfterValidation));
            if (isVMwareToolsExists)
            {
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UpdateUIDelegate(this.UpdateUIValidationSucess));
            }
            else
            {
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new UpdateUIDelegate(this.UpdateUIValidationFailure));
            }
        }

        /// <summary>
        /// Overrides OnNext.
        /// </summary>
        /// <returns>True if next button is to be shown, false otherwise.</returns>
        public override bool OnNext()
        {
            if (this.EnvironmentDetailsPageYesRadioButton.IsChecked.Value)
            {
                SetupParameters.Instance().EnvType = "VMWare";
            }
            else if (this.EnvironmentDetailsPageNoRadioButton.IsChecked.Value)
            {
                SetupParameters.Instance().EnvType = "NonVMWare";
            }
            Trc.Log(LogLevel.Always, "Set EnvType to {0}", SetupParameters.Instance().EnvType);

            return this.ValidatePage(PageNavigation.Navigation.Next);
        }

        /// <summary>
        /// Exits the page.
        /// </summary>
        public override void ExitPage()
        {
            base.ExitPage();
        }

        public override bool OnCancel()
        {
            return true;
        }

        /// <summary>
        /// Event handler for YesRadioButton_Checked
        /// </summary>
        private void YesRadioButton_Checked(object sender, RoutedEventArgs e)
        {
            Trc.Log(LogLevel.Always, "User has selected the 'Yes' radio button for protecting VMware virtual machines.");

            // Disable global navigation
            this.Page.Host.DisableGlobalSideBarNavigation();

            Trc.Log(LogLevel.Always, "Display VMware Tools validation text and Progress bar.");
            this.EnvironmentDetailsPageFetchCLITextBlock.Visibility = System.Windows.Visibility.Visible;
            this.EnvironmentDetailsPageFetchCLIProgress.Visibility = System.Windows.Visibility.Visible;

            Trc.Log(LogLevel.Always, "Disable Previous, Next and Cancel buttons during VMware Tools validation.");
            this.Page.Host.SetNextButtonState(true, false);
            this.Page.Host.SetPreviousButtonState(true, false);
            this.Page.Host.SetCancelButtonState(true, false);

            this.EnvironmentDetailsPageNoRadioButton.IsEnabled = false;

            this.Dispatcher.BeginInvoke(DispatcherPriority.Background, new CheckvSphereCLIDelegate(this.TriggerValidationInNewThread));
        }

        /// <summary>
        /// Event handler for NoRadioButton_Checked
        /// </summary>
        private void NoRadioButton_Checked(object sender, RoutedEventArgs e)
        {
            Trc.Log(LogLevel.Always, "User has selected the 'No' radio button for protecting VMware virtual machines.");

            // Enable global navigation
            this.Page.Host.EnableGlobalSideBarNavigation();

            // Enable Cancel text.
            PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.CanClose, false);

            this.EnvironmentDetailsPageVMwareToolsExistsTextBlock.Visibility = System.Windows.Visibility.Collapsed;
            this.EnvironmentDetailsPageVMwareToolsNotExistsTextBlock.Visibility = System.Windows.Visibility.Collapsed;
            this.EnvPageVMwareToolsSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
            this.EnvPageVMwareToolsErrorImage.Visibility = System.Windows.Visibility.Collapsed;

            Trc.Log(LogLevel.Always, "Enable Previous, Next and Cancel buttons.");
            this.Page.Host.SetPreviousButtonState(true, true);
            this.Page.Host.SetNextButtonState(true, true);
            this.Page.Host.SetCancelButtonState(true, true, StringResources.CancelButtonText);
        }
    }
}
