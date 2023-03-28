//---------------------------------------------------------------
//  <copyright file="ASRRegistrationPage.xaml.cs" company="Microsoft">
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
    using System.Windows.Input;
    using System.Windows.Forms;
    using System.IO;
    using ASRResources;
    using ASRSetupFramework;
    using System.Runtime.Serialization;
    using Microsoft.Azure.Portal.RecoveryServices.Models.Common;
    using DRSetup = Microsoft.DisasterRecovery.SetupFramework;
    using System.Security.Cryptography.X509Certificates;
    using IC = Microsoft.DisasterRecovery.IntegrityCheck;

    /// <summary>
    /// Interaction logic for ASRRegistrationPage.xaml.cs
    /// </summary>
    public partial class ASRRegistrationPage : ASRSetupFramework.BasePageForWpfControls
    {
        public ASRRegistrationPage(ASRSetupFramework.Page page)
            : base(page, StringResources.azure_site_recovery_registration, 3)
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="ProxyConfigurationPage"/> class.
        /// </summary>
        public ASRRegistrationPage()
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Enters the page.
        /// </summary>
        public override void EnterPage()
        {
            base.EnterPage();

            // Enable the next button if Vault Credentials file is selected.
            if (string.IsNullOrEmpty(this.ASRPageTextBox.Text))
            {
                this.Page.Host.SetNextButtonState(true, false);
            }
            else
            {
                this.Page.Host.SetNextButtonState(true, true);
            }

            // Hide previous button in case of reinstallation.
            if (SetupParameters.Instance().ReInstallationStatus == UnifiedSetupConstants.Yes)
            {
                this.Page.Host.SetPreviousButtonState(false, false);
            }
        }

        public override bool OnNext()
        {
            SetupParameters.Instance().ValutCredsFilePath = this.ASRPageTextBox.Text;
            ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "Set valutCredsFilePath to {0}", SetupParameters.Instance().ValutCredsFilePath);
            CopyVaultFile();

            return this.ValidatePage(ASRSetupFramework.PageNavigation.Navigation.Next);
        }

        /// <summary>
        /// Exits the page.
        /// </summary>
        public override void ExitPage()
        {
            base.ExitPage();
        }

        /// <summary>
        /// Copy vault file.
        /// </summary>
        private void CopyVaultFile()
        {
            try
            {
                string serviceLogDir =
                    Path.GetPathRoot(Environment.SystemDirectory) +
                    UnifiedSetupConstants.LogUploadServiceLogPath;
                if (!Directory.Exists(serviceLogDir))
                {
                    Directory.CreateDirectory(serviceLogDir);
                }

                string destFileName = Path.Combine(
                    serviceLogDir, 
                    UnifiedSetupConstants.VaultCredsFileName);

                File.Copy(
                    SetupParameters.Instance().ValutCredsFilePath, 
                    destFileName, 
                    true);
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at CopyVaultFile - {0}", ex);
            }
        }

        /// <summary>
        /// Click event handler for browsing vault credentials file.
        /// </summary>
        private void ASRKeyBrowserClick(object sender, RoutedEventArgs e)
        {
            try
            {
                System.Windows.Forms.OpenFileDialog asrbuttonbrowser = new System.Windows.Forms.OpenFileDialog();
                asrbuttonbrowser.Filter = "Vault Credentials Files (*.VaultCredentials)|*.VaultCredentials";
                asrbuttonbrowser.RestoreDirectory = true;

                DialogResult dialogResult = asrbuttonbrowser.ShowDialog();
                if (dialogResult == DialogResult.OK)
                {
                    // Get the filename extension.
                    string fileExt = Path.GetExtension(asrbuttonbrowser.FileName);
                    string requiredExt = ".VaultCredentials";

                    // Comparing the file extensions case-insensitively.
                    if (string.Equals(fileExt, requiredExt, StringComparison.OrdinalIgnoreCase))
                    {
                        try
                        {
                            // Process file, install certificate and even validates certificate expiry.
                            // Throws SettingFileException on certificate expiry.
                            // As install certificate call is idempotent no issue in calling the same by dra again.
                            DRSetup.SettingsFileHelper.ProcessSettingsFile(asrbuttonbrowser.FileName);

                            string vaultLocation =
                                DRSetup.PropertyBagDictionary.Instance.GetProperty<string>(
                                DRSetup.PropertyBagConstants.VaultLocation);

                            // If vault location is not empty, verify endpoints existence for the location.
                            if (!string.IsNullOrEmpty(vaultLocation))
                            {
                                if (IC.IntegrityCheckWrapper.GetEndpoints(vaultLocation).Count == 0)
                                {
                                    throw
                                        new DRSetup.SettingFileException(
                                            DRSetup.SettingFileException.SettingsFileError.DRAVersionNotSupported);
                                }
                            }

                            this.ASRPageTextBox.Text = asrbuttonbrowser.FileName;
                            ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "Enable the next button.");
                            this.Page.Host.SetNextButtonState(true, true);
                        }
                        catch (DRSetup.SettingFileException settingFileException)
                        {
                            SetupHelper.ShowError(
                                this.GetFriendlyCredentialFileError(settingFileException));
                        }
                        catch (Exception ex)
                        {
                            ASRSetupFramework.Trc.LogException(ASRSetupFramework.LogLevel.Error, "Exception occurred: ", ex);
                            System.Windows.MessageBox.Show(
                                StringResources.InternalError,
                                StringResources.SetupMessageBoxTitle,
                                MessageBoxButton.OK,
                                MessageBoxImage.Error);
                        }
                    }
                    else
                    {
                        ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Error, "Invalid Vault Credentials file has been selected.");
                        System.Windows.MessageBox.Show(
                            StringResources.InvalidFile,
                            StringResources.SetupMessageBoxTitle,
                            MessageBoxButton.OK,
                            MessageBoxImage.Error);
                    }
                }
            }
            catch (Exception ex)
            {
                System.Windows.MessageBox.Show(StringResources.ASRKeyBrowserClick_error , ex.Message);
            }
        }

        /// <summary>
        /// Gives friendly Error string for SettingFileException object.
        /// </summary>
        /// <param name="exp">SettingFileException object.</param>
        /// <returns>String detailing Error.</returns>
        private string GetFriendlyCredentialFileError(DRSetup.SettingFileException exp)
        {
            Trc.Log(LogLevel.Always, "SettingFileException object Status - {0}", exp.SettingsFileStatus);
            switch (exp.SettingsFileStatus)
            {
                case DRSetup.SettingFileException.SettingsFileError.CorruptFile:
                    return StringResources.SettingsFileCorrupt;

                case DRSetup.SettingFileException.SettingsFileError.DRAVersionNotSupported:
                    return StringResources.DraVersionNotSupported;

                case DRSetup.SettingFileException.SettingsFileError.FileExpired:
                    return StringResources.CredentialFileExpired;
            }

            return StringResources.InternalError;
        }
    }
}

