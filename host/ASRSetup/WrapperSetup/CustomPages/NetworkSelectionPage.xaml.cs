//------------------------------------------------------------------------
//  <copyright file="NetworkSelectionPage.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  User has to choose Network Interface IP.
//  </summary>
//
//  History:     15-Oct-2015   bhayachi     Created
//-------------------------------------------------------------------------

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
using System.Net.NetworkInformation;
using System.Text.RegularExpressions;
using ASRSetupFramework;
using ASRResources;

namespace UnifiedSetup
{
    /// <summary>
    /// Interaction logic for NetworkSelectionPage.xaml
    /// </summary>
    public partial class NetworkSelectionPage : BasePageForWpfControls
    {
        public NetworkSelectionPage(ASRSetupFramework.Page page)
            : base(page, StringResources.network_selection, 3)
        {
            this.InitializeComponent();
            
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="NetworkSelectionPage"/> class.
        /// </summary>
        public NetworkSelectionPage()
        {
            InitializeComponent();
        }

        /// <summary>
        /// Enters the page.
        /// </summary>
        public override void EnterPage()
        {
            base.EnterPage();
            if (!string.IsNullOrEmpty(this.NetworkSelectionComboBox.Text) && !string.IsNullOrEmpty(this.NetworkSelectionPortTextbox.Text))
            {
                this.Page.Host.SetNextButtonState(true, true);
            }
            else
            {
                this.Page.Host.SetNextButtonState(true, false);
            }

            if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
            {
                this.NetworkSelectionNoteTextblock.Text = StringResources.NetworkSelectionCSNoteText;
                this.NetworkSelection443NoteTextblock.Visibility = System.Windows.Visibility.Visible;

                this.NetworkSelectionAzureTextblock.Visibility = System.Windows.Visibility.Visible;
                this.NetworkSelectionComboBoxLabelAzure.Visibility = System.Windows.Visibility.Visible;
                this.NetworkSelectionComboBoxAzure.Visibility = System.Windows.Visibility.Visible;

                if (!string.IsNullOrEmpty(this.NetworkSelectionComboBoxAzure.Text))
                {
                    this.Page.Host.SetNextButtonState(true, true);
                }
                else
                {
                    this.Page.Host.SetNextButtonState(true, false);
                }

            }
            else if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
            {
                this.NetworkSelectionNoteTextblock.Text = StringResources.NetworkSelectionPSNoteText;
                this.NetworkSelection443NoteTextblock.Visibility = System.Windows.Visibility.Collapsed;
                this.NetworkSelectionAzureTextblock.Visibility = System.Windows.Visibility.Collapsed;
                this.NetworkSelectionComboBoxLabelAzure.Visibility = System.Windows.Visibility.Collapsed;
                this.NetworkSelectionComboBoxAzure.Visibility = System.Windows.Visibility.Collapsed;
            }
        }

        /// <summary>
        /// Override default OnNext logic.
        /// </summary>
        /// <returns>True if next page can be shown, false otherwise.</returns>
        public override bool OnNext()
        {
            return this.ValidatePage(PageNavigation.Navigation.Next);
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
            if (!string.IsNullOrEmpty(this.NetworkSelectionComboBox.Text) && !string.IsNullOrEmpty(this.NetworkSelectionPortTextbox.Text))
            {
                if (this.NetworkSelectionPortTextbox.Text == "443")
                {
                    if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                    {
                        SetupHelper.ShowError(StringResources.Invalid443PortText);
                        isValidPage = false;
                    }                    
                }
                else
                {
                    if (!SetupHelper.IsPortValid(this.NetworkSelectionPortTextbox.Text))
                    {
                        isValidPage = false;
                    }
                    else
                    {
                        string Comboboxnicselected = NetworkSelectionComboBox.SelectedItem as string;
                        string psip = Regex.Match(Comboboxnicselected, @"\[([^)]*)\]", RegexOptions.RightToLeft).Groups[1].Value;

                        // Validating the PSIP.
                        if (ValidationHelper.ValidateIPAddress(psip))
                        {
                            SetupParameters.Instance().PSIP = psip;
                            Trc.Log(LogLevel.Always, "Set PSIP to {0}", SetupParameters.Instance().PSIP);

                            SetupParameters.Instance().DataTransferSecurePort = this.NetworkSelectionPortTextbox.Text;
                            Trc.Log(LogLevel.Always, "Set DataTransferSecurePort to {0}", SetupParameters.Instance().DataTransferSecurePort);
                        }
                        else
                        {
                            Trc.Log(LogLevel.Error, "Invalid PSIP: {0}", psip);
                            SetupHelper.ShowError("Unable to fetch the IP address associated with Network Interface Card.");
                            isValidPage = false;
                        }

                        // Validating the AZUREIP.
                        if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                        {
                            string ComboboxnicAzureselected = NetworkSelectionComboBoxAzure.SelectedItem as string;
                            string azureip = Regex.Match(ComboboxnicAzureselected, @"\[([^)]*)\]", RegexOptions.RightToLeft).Groups[1].Value;

                            if (ValidationHelper.ValidateIPAddress(azureip))
                            {
                                SetupParameters.Instance().AZUREIP = azureip;
                                Trc.Log(LogLevel.Always, "Set AZUREIP to {0}", SetupParameters.Instance().AZUREIP);
                            }
                            else
                            {
                                Trc.Log(LogLevel.Error, "Invalid AZUREIP: {0}", azureip);
                                SetupHelper.ShowError("Unable to fetch the AZURE IP address associated with Network Interface Card.");
                                isValidPage = false;
                            }
                        }
                    }
                }
            }

            return isValidPage;
        }

        /// <summary>
        /// Exits the page.
        /// </summary>
        public override void ExitPage()
        {
            base.ExitPage();
        }

        /// <summary>
        /// Load Combobox with NIC IP's.
        /// </summary>
        private void NetworkSelectionComboBox_Loaded(object sender, RoutedEventArgs e)
        {
            try
            {
                List<object> Comboboxnics = new List<object>();
                Comboboxnics.Add("Select");

                foreach (NetworkInterface nic in NetworkInterface.GetAllNetworkInterfaces())
                {
                    if (isValidNIC(nic))
                    {
                        IPInterfaceProperties ipProperties = nic.GetIPProperties();

                        //For each of the IPProperties process the informaiton 
                        foreach (IPAddressInformation ipAddr in ipProperties.UnicastAddresses)
                        {

                            string ipAddress = ipAddr.Address.ToString();
                            if (!string.IsNullOrEmpty(ipAddress))
                            {
                                //Only list Valid IPV4 address using regular expression comparision
                                string IPV4_PATTERN =
                                        "^([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.([01]?\\d\\d?|2[0-4]\\d|25[0-5])$";
                                if (Regex.IsMatch(ipAddress, IPV4_PATTERN))
                                {
                                    Comboboxnics.Add(nic.Name + " [" + ipAddress + "]");
                                }
                            }

                        }
                    }
                }

                // Get the ComboBox reference.
                var comboBox = sender as ComboBox;

                // Assign the ItemsSource to the List.
                comboBox.ItemsSource = Comboboxnics;

                if (Comboboxnics.Count == 1)
                {
                    Trc.Log(LogLevel.Always, "Selecting the item by default in combobox as it has only one value.");
                    comboBox.SelectedItem = Comboboxnics[1];
                    if (!string.IsNullOrEmpty(this.NetworkSelectionPortTextbox.Text))
                    {
                        this.Page.Host.SetNextButtonState(true, true);
                    }
                }
                else
                {
                    comboBox.SelectedItem = Comboboxnics[0];
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error, "Exception occurred: ", ex);
            }
        }

        /// <summary>
        ///  This method checks if the NIC could be selected - Only Ethernet and Wireless through WIFI is allowed for now.  
        /// </summary>
        /// <param name="nic"></param>
        /// <returns></returns>
        private Boolean isValidNIC(NetworkInterface nic)
        {
            Boolean result = false;

            //Check if the NIC supports IPV4 protocol
            if (nic.Supports(NetworkInterfaceComponent.IPv4) == true)
            {
                //Check if NIC interface type is either Ethernet based or Wireless(WIFI) based.
                switch (nic.NetworkInterfaceType)
                {
                    case NetworkInterfaceType.Ethernet:
                    case NetworkInterfaceType.Ethernet3Megabit:
                    case NetworkInterfaceType.FastEthernetFx:
                    case NetworkInterfaceType.FastEthernetT:
                    case NetworkInterfaceType.GigabitEthernet:
                    case NetworkInterfaceType.Wireless80211:
                        {
                            // Only show NIC cards, which are in operational state i.e. LAN Wire or WIFI is already connected.
                            if (nic.OperationalStatus == OperationalStatus.Up)
                            {
                                result = true;
                            }
                            break;
                        }
                }
            }
            return result;
        }

        /// <summary>
        /// Restricts the port input to numeric digits.
        /// </summary>
        private void NetworkSelectionPortTextbox_PreviewTextInput(object sender, TextCompositionEventArgs e)
        {
            Regex portRegEx = new Regex("^[0-9]");
            if (!portRegEx.IsMatch(e.Text))
            {
                e.Handled = true;
            }
            else
            {
                e.Handled = false;
            }
        }

        /// <summary>
        /// Next button state handler.
        /// </summary>
        private void updateNextbuttonstate()
        {
            if (!string.IsNullOrEmpty(this.NetworkSelectionPortTextbox.Text))
            {
                this.Page.Host.SetNextButtonState(true, true);
            }
            else
            {
                this.Page.Host.SetNextButtonState(true, false);
            }
        }

        /// <summary>
        /// Port textbox selection change handler.
        /// </summary>
        private void NetworkSelectionPortTextbox_SelectionChanged(object sender, RoutedEventArgs e)
        {
            updateNextbuttonstate();
        }

        /// <summary>
        /// Combobox selection change handler.
        /// </summary>
        private void NetworkSelectionComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            updateNextbuttonstate();
        }
    }
}
