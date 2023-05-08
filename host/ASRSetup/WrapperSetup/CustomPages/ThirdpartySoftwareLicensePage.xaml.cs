//---------------------------------------------------------------
//  <copyright file="ThirdpartySoftwareLicensePage.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  UI page to show Thirdparty License Terms.
//  </summary>
//
//  History:     16-Sep-2015   bhayachi     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Diagnostics;
using ASRSetupFramework;
using ASRResources;

namespace UnifiedSetup
{
    /// <summary>
    /// Interaction logic for ThirdpartySoftwareLicensePage.xaml
    /// </summary>
    public partial class ThirdpartySoftwareLicensePage : BasePageForWpfControls
    {
        public ThirdpartySoftwareLicensePage(ASRSetupFramework.Page page)
            : base(page, StringResources.third_party_software_license, 3)
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="ThirdpartySoftwareLicensePage"/> class.
        /// </summary>
        public ThirdpartySoftwareLicensePage()
        {
            this.InitializeComponent();
        }

        public override void EnterPage()
        {
            base.EnterPage();

            if ((SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade) &&
                (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode) &&
                (PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.MySQL55Installed)))
            {
                // Hide Previous button
                this.Page.Host.SetPreviousButtonState(false, false);
            }
            /// <summary>
            /// Sets the next button state depending upon the checkbox checked state.
            /// </summary>
            if (this.ThirdpartySoftwareLicenseCheckbox.IsChecked == true)
            {
                this.Page.Host.SetNextButtonState(true, true);
            }
            else
            {
                this.Page.Host.SetNextButtonState(true, false);
            }
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
        /// Handles the request navigate.
        /// </summary>
        private void HandleRequestNavigate(object sender, RoutedEventArgs e)
        {
            string navigateUri = ((Hyperlink)sender).NavigateUri.ToString();
            Process.Start(new ProcessStartInfo(navigateUri));
            e.Handled = true;
        }

        /// <summary>
        /// Event handler for CheckBox Checked.
        /// </summary>
        private void ThirdpartySoftwareLicenseCheckbox_Checked(object sender, RoutedEventArgs e)
        {
            Trc.Log(LogLevel.Always, "Enabling the next button as ThirdpartySoftwareLicenseCheckbox has been checked.");
            this.Page.Host.SetNextButtonState(true, true);
        }

        /// <summary>
        /// Event handler for CheckBox Unchecked.
        /// </summary>
        private void ThirdpartySoftwareLicenseCheckbox_Unchecked(object sender, RoutedEventArgs e)
        {
            Trc.Log(LogLevel.Always, "Disabling the next button as ThirdpartySoftwareLicenseCheckbox has been Unchecked.");
            this.Page.Host.SetNextButtonState(true, false);
        }
    }
}
