//---------------------------------------------------------------
//  <copyright file="InstallLocationPage.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Location to install the Binaries.
//  </summary>
//
//  History:     22-Sep-2015   bhayachi     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Security;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;
using System.Windows.Forms;
using System.IO;
using ASRResources;
using ASRSetupFramework;

namespace UnifiedSetup
{
    /// <summary>
    /// Interaction logic for InstallLocationPage.xaml
    /// </summary>
    public partial class InstallLocationPage : BasePageForWpfControls
    {
        public static string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);
        const string DRIVE_FORMAT = "NTFS";
        
        public InstallLocationPage(ASRSetupFramework.Page page)
            : base(page, StringResources.install_location, 5)
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="InstallLocationPage"/> class.
        /// </summary>
        public InstallLocationPage()
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Enters the page.
        /// </summary>
        public override void EnterPage()
        {
            base.EnterPage();

            // Display largest drive installation path in InstallLocationTextBox if user does not select custom folder.
            string installLocation = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.InstallationLocation);
            if (installLocation != "Modified")
            {
                this.InstallLocationTextBox.Text = LargestNTFSDrive() + @"Program Files (x86)\Microsoft Azure Site Recovery";
                Trc.Log(LogLevel.Always, "Enter Page: Displaying {0} in InstallLocationTextBox.", InstallLocationTextBox.Text);
                if (!string.IsNullOrEmpty(LargestNTFSDrive()))
                {
                    long freeSpace = ReturnFreeSpaceAvail(LargestNTFSDrive());
                    if ((freeSpace >= 600))
                    {
                        
                        this.InstallLocationErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                        this.InstallLocationWarningImage.Visibility = System.Windows.Visibility.Collapsed;
                        this.InstallLocationSuccessImage.Visibility = System.Windows.Visibility.Visible;
                        this.InstallLocationSpaceWarningImage.Visibility = System.Windows.Visibility.Collapsed;
                        this.InstallLocationSpaceErrorImage.Visibility = System.Windows.Visibility.Collapsed;
						this.InstallLocationSpaceErrorTextblock.Visibility = System.Windows.Visibility.Collapsed;
                        this.InstallLocationSpaceWarningTextblock.Visibility = System.Windows.Visibility.Collapsed;
                        this.Page.Host.SetNextButtonState(true, true);
                        Trc.Log(LogLevel.Always, "Enabled next button as the drive selected "+InstallLocationTextBox.Text+" has more than 600 GB free space.");
                        
                    }
                    else if((freeSpace >= 5))
                    {
                        this.InstallLocationErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                        this.InstallLocationWarningImage.Visibility = System.Windows.Visibility.Visible;
                        this.InstallLocationSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                        this.InstallLocationSpaceWarningImage.Visibility = System.Windows.Visibility.Visible;
                        this.InstallLocationSpaceErrorImage.Visibility = System.Windows.Visibility.Collapsed;
						this.InstallLocationSpaceErrorTextblock.Visibility = System.Windows.Visibility.Collapsed;
                        this.InstallLocationSpaceWarningTextblock.Visibility = System.Windows.Visibility.Visible;
                        this.InstallLocationSpaceWarningTextblock.Text = string.Format(StringResources.InstallLocationSpaceWarningText);
                        this.Page.Host.SetNextButtonState(true, true);
                        Trc.Log(LogLevel.Always, "Enabled next button as the drive selected " + InstallLocationTextBox.Text + " has more than 5 GB free space.");
                    }                    
                    else
                    {

                        this.InstallLocationWarningImage.Visibility = System.Windows.Visibility.Collapsed;
                        this.InstallLocationErrorImage.Visibility = System.Windows.Visibility.Visible;
                        this.InstallLocationSuccessImage.Visibility = System.Windows.Visibility.Collapsed;                        
                        this.InstallLocationSpaceWarningImage.Visibility = System.Windows.Visibility.Collapsed;
                        this.InstallLocationSpaceErrorImage.Visibility = System.Windows.Visibility.Visible;
                        this.InstallLocationSpaceErrorTextblock.Visibility = System.Windows.Visibility.Visible;
						this.InstallLocationSpaceWarningTextblock.Visibility = System.Windows.Visibility.Collapsed;
						this.InstallLocationSpaceErrorTextblock.Text = string.Format(StringResources.MandatorySpaceCheckText);	                        
                        SetupParameters.Instance().CXInstallDir = null;
                        this.Page.Host.SetNextButtonState(true, false);
                        Trc.Log(LogLevel.Always, "Disabled next button as the drive selected " + InstallLocationTextBox.Text + " has less than 5 GB free space.");
                    }
                }
                else
                {
                    this.Page.Host.SetNextButtonState(true, false);
                }				
            } 
            else
            {
                try
                {
                    if (this.InstallLocationTextBox.Text == sysDrive)
                    {
                        UpdateUI_SystemDriveLocation();
                    }
                    else
                    {
                        // Enable global navigation
                        this.Page.Host.EnableGlobalSideBarNavigation();

                        // Splitting the argument path to get the drive letter.
                        string[] drivePathArray = InstallLocationTextBox.Text.Split(new char[] { ':' });
                        string driveSelected = drivePathArray[0];

                        string drvpath = driveSelected + @":\";
                        Trc.Log(LogLevel.Always, "Validating free space for {0} drive.", drvpath);
                        long freeSpace = ReturnFreeSpaceAvail(drvpath);

                        UpdateUI_freespace(freeSpace);
                    }

                    Trc.Log(LogLevel.Always, "Adding installationLocation value as Modified in PropertyBagConstants.");
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.InstallationLocation, "Modified");                    
                }
                catch (Exception ex)
                {
                    System.Windows.MessageBox.Show(StringResources.ASRKeyBrowserClick_error, ex.Message);
                }
            }
        }

        /// <summary>
        /// Overrides OnNext.
        /// </summary>
        /// <returns>True if next button is to be shown, false otherwise.</returns>
        public override bool OnNext()
        {
            SetupParameters.Instance().CXInstallDir = InstallLocationTextBox.Text + @"\home\svsystems";
            Trc.Log(LogLevel.Always, "Set CXInstallDir to {0}", SetupParameters.Instance().CXInstallDir);

            SetupParameters.Instance().AgentInstallDir = InstallLocationTextBox.Text;
            Trc.Log(LogLevel.Always, "Set AgentInstallDir to {0}", SetupParameters.Instance().AgentInstallDir);

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

            if (!isDriveNTFS(InstallLocationTextBox.Text))
            {
                Trc.Log(LogLevel.Error, "The drive path selected : {0} is not a NTFS drive path.", InstallLocationTextBox.Text);
                SetupHelper.ShowError(StringResources.DriveSelectionErrorText);
                isValidPage = false;
            }
                
            return isValidPage;
        }

        /// <summary>
        /// Click event handler for browsing the installation path.
        /// </summary>
        private void InstallLocationBrowserClick(object sender, RoutedEventArgs e)
        {
            System.Windows.Forms.FolderBrowserDialog instlocButtonBrowser = new System.Windows.Forms.FolderBrowserDialog();
            DialogResult dialogResult = instlocButtonBrowser.ShowDialog();
            if (dialogResult == DialogResult.OK)
            {
                try
                {
                    this.InstallLocationTextBox.Text = instlocButtonBrowser.SelectedPath;

                    if (this.InstallLocationTextBox.Text == sysDrive)
                    {
                        UpdateUI_SystemDriveLocation();
                    }
                    else
                    {
                        // Enable global navigation
                        this.Page.Host.EnableGlobalSideBarNavigation();

                        // Splitting the argument path to get the drive letter.
                        string[] drivePathArray = InstallLocationTextBox.Text.Split(new char[] { ':' });
                        string driveSelected = drivePathArray[0];

                        string drvpath = driveSelected + @":\";
                        Trc.Log(LogLevel.Always, "Validating free space for {0} drive.", drvpath);
                        long freeSpace = ReturnFreeSpaceAvail(drvpath);

                        UpdateUI_freespace(freeSpace);
                    }
                    Trc.Log(LogLevel.Always, "Adding installationLocation value as Modified in PropertyBagConstants.");
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.InstallationLocation, "Modified");                    
                }
                catch (Exception ex)
                {
                    System.Windows.MessageBox.Show(StringResources.ASRKeyBrowserClick_error, ex.Message);
                }

            }
        }

        /// <summary>
        /// Update UI when system drive is selected as install location.
        /// </summary>
        private void UpdateUI_SystemDriveLocation()
        {
            this.Page.Host.DisableGlobalSideBarNavigation();
            this.InstallLocationSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
            this.InstallLocationWarningImage.Visibility = System.Windows.Visibility.Collapsed;
            this.InstallLocationSpaceWarningImage.Visibility = System.Windows.Visibility.Collapsed;
            this.InstallLocationSpaceWarningTextblock.Visibility = System.Windows.Visibility.Collapsed;
            this.InstallLocationErrorImage.Visibility = System.Windows.Visibility.Visible;
            this.InstallLocationSpaceErrorImage.Visibility = System.Windows.Visibility.Visible;
            this.InstallLocationSpaceErrorTextblock.Visibility = System.Windows.Visibility.Visible;
            this.InstallLocationSpaceErrorTextblock.Text = "Installation cannot be done directly under system drive.";
            this.Page.Host.SetNextButtonState(true, false);
        }

        /// <summary>
        /// Update UI depending on freespace available in the drive selected.
        /// </summary>
        private void UpdateUI_freespace(long freeSpace)
        {
            // Validation for 600 GB free space.
            if (freeSpace >= 600)
            {
                this.InstallLocationErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                this.InstallLocationWarningImage.Visibility = System.Windows.Visibility.Collapsed;
                this.InstallLocationSuccessImage.Visibility = System.Windows.Visibility.Visible;
                this.InstallLocationSpaceErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                this.InstallLocationSpaceErrorTextblock.Visibility = System.Windows.Visibility.Collapsed;
                this.InstallLocationSpaceWarningImage.Visibility = System.Windows.Visibility.Collapsed;
                this.InstallLocationSpaceWarningTextblock.Visibility = System.Windows.Visibility.Collapsed;
                this.Page.Host.SetNextButtonState(true, true);
                Trc.Log(LogLevel.Always, "Enabled next button as the drive selected " + InstallLocationTextBox.Text + " has more than 600 GB free space.");
            }
            // Validation for 5 GB free space.
            else if (freeSpace >= 5)
            {
                this.InstallLocationErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                this.InstallLocationWarningImage.Visibility = System.Windows.Visibility.Visible;
                this.InstallLocationSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                this.InstallLocationSpaceWarningImage.Visibility = System.Windows.Visibility.Visible;
                this.InstallLocationSpaceErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                this.InstallLocationSpaceErrorTextblock.Visibility = System.Windows.Visibility.Collapsed;
                this.InstallLocationSpaceWarningTextblock.Visibility = System.Windows.Visibility.Visible;
                this.InstallLocationSpaceWarningTextblock.Text = string.Format(StringResources.InstallLocationSpaceWarningText);
                this.Page.Host.SetNextButtonState(true, true);
                Trc.Log(LogLevel.Always, "Enabled next button as the drive selected " + InstallLocationTextBox.Text + " has more than 5 GB free space.");
            }
            else
            {
                this.InstallLocationWarningImage.Visibility = System.Windows.Visibility.Collapsed;
                this.InstallLocationErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.InstallLocationSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                this.InstallLocationSpaceWarningImage.Visibility = System.Windows.Visibility.Collapsed;
                this.InstallLocationSpaceErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.InstallLocationSpaceWarningTextblock.Visibility = System.Windows.Visibility.Collapsed;
                this.InstallLocationSpaceErrorTextblock.Visibility = System.Windows.Visibility.Visible;
                this.InstallLocationSpaceErrorTextblock.Text = string.Format(StringResources.MandatorySpaceCheckText);
                SetupParameters.Instance().CXInstallDir = null;
                this.Page.Host.SetNextButtonState(true, false);
                Trc.Log(LogLevel.Always, "Disabled next button as the drive selected " + InstallLocationTextBox.Text + " has less than 5 GB free space.");
            }
        }

        /// <summary>
        /// Returns the drive space available.
        /// </summary>
        public long ReturnFreeSpaceAvail(string driveName)
        {
            DriveInfo driveinfo = new DriveInfo(driveName);
            if (driveinfo.IsReady && driveinfo.Name == driveName)
            {
                // Convert drive space to GB.
                long space_avail = driveinfo.TotalFreeSpace / (1024 * 1024 * 1024);
                return space_avail;
            }
            return -1;
        }

        /// <summary>
        /// Returns the NTFS drive with maximum space available.
        /// </summary>
        public string LargestNTFSDrive()
        {
            string output = "";
            Dictionary<DriveInfo, long> driveSpaceDictionary = new Dictionary<DriveInfo, long>();
            try
            {
                // Get information of all drives
                DriveInfo[] allDrives = DriveInfo.GetDrives();
                foreach (DriveInfo driveInfo in allDrives)
                {
                    if (driveInfo.DriveType == DriveType.Fixed && driveInfo.DriveFormat.Equals("NTFS", StringComparison.OrdinalIgnoreCase))
                    {
                        // Get the free space available on the drive.
                        long freeSpace = driveInfo.TotalFreeSpace / (1024 * 1024 * 1024);
                        Trc.Log(LogLevel.Always, "{0} has {1} GB free space.", driveInfo, freeSpace);

                        // Adding key-value pair to the dictionary.
                        driveSpaceDictionary.Add(driveInfo, freeSpace);
                    }
                }

                // Get the drive with maximum space.
                DriveInfo largestDrive = driveSpaceDictionary.Aggregate((l, r) => l.Value > r.Value ? l : r).Key;
                output = largestDrive.ToString();
                Trc.Log(LogLevel.Always, "The largest NTFS drive available in the system is {0}.", output);
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
            }
            return output;
        }

        /// <summary>
        /// Validates whether the drive is a NTFS drive or not.
        /// </summary>
        private bool isDriveNTFS(string path)
        {
            bool retval = false;

            try
            {
                // Splitting the argument path to get the drive letter.
                string[] drivePathArray = path.Split(new char[] { ':' });
                string driveSelected = drivePathArray[0];

                string drvpath = driveSelected + @":\";
                Trc.Log(LogLevel.Always, "Validating free space for {0} drive.", drvpath);
                long freeSpace = ReturnFreeSpaceAvail(drvpath);
                Trc.Log(LogLevel.Always, "The freespace available in {0} is {1} GB.", drvpath, freeSpace);
                if (freeSpace >= 5)
                {
                    DriveInfo[] allDrives = DriveInfo.GetDrives();
                    foreach (DriveInfo driveInfo in allDrives)
                    {
                        if (driveInfo.DriveType == DriveType.Fixed && driveInfo.DriveFormat.Equals("NTFS", StringComparison.OrdinalIgnoreCase))
                        {
                            // Splitting the NTFS drive to get the drive letter.
                            string[] driveInfoArray = driveInfo.ToString().Split(new char[] { ':' });
                            string driveInfodrive = driveInfoArray[0];

                            Trc.Log(LogLevel.Debug, "Comparing user selected drive: {0} with NTFS drive: {1} available in the system.", driveSelected, driveInfodrive);
                            if (driveSelected == driveInfodrive)
                            {
                                Trc.Log(LogLevel.Always, "The drive path selected : {0} is a NTFS drive path.", driveSelected);
                                retval = true;
                            }
                        }
                    }
                }
                else
                {
                    SetupHelper.ShowError(StringResources.MandatorySpaceCheckText);
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
            }
            return retval;
        }
    }
}
