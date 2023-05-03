//---------------------------------------------------------------
//  <copyright file="InstallationChoice.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Choice of installation.
//  </summary>
//
//  History:     22-Sep-2015   bhayachi     Created
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
    using System.Windows.Threading;
    using System.Collections;
    using System.Diagnostics;
    using System.IO;
    using System.Reflection;
    using System.Xml;
    using System.Xml.Serialization;
    using ASRResources;
    using ASRSetupFramework;


    /// <summary>
    /// Interaction logic for InstallationChoice.xaml
    /// </summary>
    public partial class InstallationChoice : BasePageForWpfControls
    {
        public InstallationChoice(ASRSetupFramework.Page page)
            : base(page, StringResources.before_you_begin,1)
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="ProxyConfigurationPage"/> class.
        /// </summary>
        public InstallationChoice()
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Enters the page.
        /// </summary>
        public override void EnterPage()
        {
            base.EnterPage();
        }

        /// <summary>
        /// Exits the page.
        /// </summary>
        public override void ExitPage()
        {
            base.ExitPage();
        }

        /// <summary>
        /// Overrides OnNext.
        /// </summary>
        /// <returns>True if next button is to be shown, false otherwise.</returns>
        public override bool OnNext()
        {
            if (this.CSPSRadioButton.IsChecked.Value)
            {
                Trc.Log(LogLevel.Always, "User clicked CSPS Radio button.");
                SetupParameters.Instance().ServerMode = UnifiedSetupConstants.CSServerMode;                
            }
            else if (this.PSRadioButton.IsChecked.Value)
            {
                Trc.Log(LogLevel.Always, "User clicked PS Radio button.");
                SetupParameters.Instance().ServerMode = UnifiedSetupConstants.PSServerMode;
            }

            Trc.Log(LogLevel.Always, "Set server mode to {0}", SetupParameters.Instance().ServerMode);

            HideSideBarStages();
            return this.ValidatePage(PageNavigation.Navigation.Next);
        }

        /// <summary>
        /// Hide/Show side bar stages based on installation choice (CS/PS).
        /// </summary>
        private void HideSideBarStages()
        {          
            if (this.CSPSRadioButton.IsChecked.Value)
            {
                // List of side bar stages to hide when CSPS radio button is selected.
                var pagesToHide = new List<string> { 
                    "configuration_server_details" };

                foreach (var page in pagesToHide)
                {
                    HideSidebarStageforPage(page);
                }

                // List of side bar stages to show if they are disabled.
                var pagesToShow = new List<string> { 
                    "third_party_software_license", 
                    "mysql_configuration", 
                    "azure_site_recovery_registration" };

                foreach (var page in pagesToShow)
                {
                    ShowSidebarStageforPage(page);
                }

            }
            else if (this.PSRadioButton.IsChecked.Value)
            {
                // List of side bar stages to hide when PS radio button is selected.
                var pagesToHide = new List<string> { 
                    "third_party_software_license", 
                    "mysql_configuration", 
                    "azure_site_recovery_registration" };

                foreach (var page in pagesToHide)
                {
                    HideSidebarStageforPage(page);
                }

                // List of side bar stages to show if they are disabled.
                var pagesToShow = new List<string> { 
                    "configuration_server_details" };

                foreach (var page in pagesToShow)
                {
                    ShowSidebarStageforPage(page);
                }
            }
        }
    }
}
