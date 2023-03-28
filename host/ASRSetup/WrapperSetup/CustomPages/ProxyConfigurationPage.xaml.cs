//------------------------------------------------------------------------
//  <copyright file="ProxyConfigurationPage.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Validates all the Pre-requisites required for installation to proceed.
//  </summary>
//
//  History:     12-Oct-2015   bhayachi     Created
//-------------------------------------------------------------------------

using System.Diagnostics;
using System.Windows.Documents;

namespace UnifiedSetup
{
    using System;
    using System.Collections;
    using System.Collections.Generic;
    using System.IO;
    using System.Net;
    using System.Security;
    using System.Text;
    using System.Text.RegularExpressions;
    using System.Threading;
    using System.Windows;
    using System.Windows.Controls;
    using System.Windows.Media;
    using System.Windows.Threading;
    using Microsoft.Win32;
    using ASRResources;
    using ASRSetupFramework;
    using InMage.APIHelpers;
    using InMage.APIModel;
    using Microsoft.DisasterRecovery.Resources;
    using DRSetup = Microsoft.DisasterRecovery.SetupFramework;
    using IC = Microsoft.DisasterRecovery.IntegrityCheck;

    /// <summary>
    /// Interaction logic for ProxyConfigurationPage.xaml
    /// </summary>
    public partial class ProxyConfigurationPage : ASRSetupFramework.BasePageForWpfControls
    {
        /// <summary>
        /// Empty delegate to force UI refresh.
        /// </summary>
        private static Action emptyDelegate = delegate
        {
        };

        /// <summary>
        /// Private endpoint state.
        /// </summary>
        private string privateEndpointState;

        /// <summary>
        /// Resource Id.
        /// </summary>
        private string resourceId;

        #region Fields
        /// <summary>
        /// Stores current connection state using all different types of proxy.
        /// </summary>
        private static Dictionary<ProxyType, ProxyConnectionState> proxyTypeConnectionStateMap;

        /// <summary>
        /// If password for proxy was changed.
        /// </summary>
        private bool passwordChanged;

        /// <summary>
        /// Detailed Error of proxy connection failure.
        /// </summary>
        private string lastDetailedError = string.Empty;

        /// <summary>
        /// Type of navigation if validation succeeds.
        /// </summary>
        private ASRSetupFramework.PageNavigation.Navigation navType;

        /// <summary>
        /// Page to jump to if validation succeeds.
        /// </summary>
        private string pageId;

        /// <summary>
        /// List of endpoints to be validated
        /// </summary>
        private List<string> listofEndpoints;

        /// <summary>
        /// List of endpoints to be validated
        /// </summary>
        private List<string> listofPrivateEndpoints;
        #endregion

        #region Delegates
        /// <summary>
        /// The delegate used to navigate to next page.
        /// </summary>
        private delegate void NavigateDelegate();

        /// <summary>
        /// The delegate used to report progress
        /// </summary>
        private delegate void ProgressDelegate();
        #endregion Delegates

        #region Enums
        /// <summary>
        /// Current state of connected using associated proxy.
        /// </summary>
        private enum ProxyConnectionState
        {
            /// <summary>
            /// Connected to internet.
            /// </summary>
            Connected,

            /// <summary>
            /// Not connected to internet.
            /// </summary>
            NotConnected,

            /// <summary>
            /// Checking for internet connectivity.
            /// </summary>
            Checking,

            /// <summary>
            /// Pending check for internet connectivity.
            /// </summary>
            CheckingPending,

            /// <summary>
            /// Additional data required for checking connectivity to internet.
            /// </summary>
            DataRequired,

            /// <summary>
            /// Proxy password is required.
            /// </summary>
            PasswordRequired
        }

        /// <summary>
        /// Type of proxy.
        /// </summary>
        private enum ProxyType
        {
            /// <summary>
            /// Bypass existing system proxy.
            /// </summary>
            Bypass,

            /// <summary>
            /// Use custom proxy settings.
            /// </summary>
            Custom
        }
        #endregion

        public ProxyConfigurationPage(ASRSetupFramework.Page page)
            : base(page, ASRResources.StringResources.ConnectionSettings, 2)
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="PrerequisitesPage"/> class.
        /// </summary>
        public ProxyConfigurationPage()
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Enters the page.
        /// </summary>
        public override void EnterPage()
        {
            try
            {
                base.EnterPage();

                // Attach event handlers.
                this.textBoxAddress.TextChanged += this.TextBox_AddressChanged;
                this.textBoxPort.TextChanged += this.TextBox_PortChanged;

                // This is executed only once.
                if (proxyTypeConnectionStateMap == null)
                {
                    ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "Adding proxy type status to CheckingPending.");
                    proxyTypeConnectionStateMap = new Dictionary<ProxyType, ProxyConnectionState>();
                    proxyTypeConnectionStateMap.Add(
                        ProxyType.Bypass,
                        ProxyConnectionState.CheckingPending);
                    proxyTypeConnectionStateMap.Add(
                        ProxyType.Custom,
                        ProxyConnectionState.CheckingPending);

                    // Evaluate condition if custom proxy needs to be checked.
                    if (!string.IsNullOrEmpty(this.textBoxAddress.Text))
                    {
                        if (!string.IsNullOrEmpty(this.textBoxUserName.Text))
                        {
                            // If proxy address was specified but not Password ask user input
                            if (string.IsNullOrEmpty(this.passwordBoxPassword.Password))
                            {
                                proxyTypeConnectionStateMap[ProxyType.Custom] =
                                    ProxyConnectionState.PasswordRequired;
                            }
                        }
                    }
                    else
                    {
                        proxyTypeConnectionStateMap[ProxyType.Custom] = ProxyConnectionState.DataRequired;
                    }
                }

                this.Page.Host.SetCancelButtonState(true, true, ASRResources.StringResources.CancelButtonText);
                this.Page.Host.SetPreviousButtonState(true, true);
                this.Page.Host.DisableGlobalSideBarNavigation();

                if (this.radioButtonProxyServer.IsChecked.Value)
                {
                    if (proxyTypeConnectionStateMap[ProxyType.Custom] == ProxyConnectionState.Connected)
                    {
                        this.Page.Host.SetNextButtonState(true, true);
                    }
                    else
                    {
                        this.Page.Host.SetNextButtonState(true, false);
                    }
                }

                // Fetch ACS URL and Geo name from configuration server in case of process server installation.
                if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
                {
                    if (!InstallActionProcessess.FetchAcsUrlGeoName())
                    {
                        this.Page.Host.SetNextButtonState(true, false);
                    }
                }

                // Fetch the list of endpoints.
                this.UpdateEndPoints();

                // Add MySQL Url to list of end points in case of configuration server installation.
                if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                {
                    if (!InstallActionProcessess.IsPrivateEndpointStateEnabled())
                    {
                        if (!SetupHelper.IsMySQLInstalled(UnifiedSetupConstants.MySQL57Version))
                        {
                            listofEndpoints.Add(UnifiedSetupConstants.MySQLDownloadLink);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                ASRSetupFramework.Trc.LogException(ASRSetupFramework.LogLevel.Error, "On enter ProxyConfigurationPage", ex);
                ASRSetupFramework.SetupHelper.ShowError(ex.Message, this);
            }
        }

        /// <summary>
        /// Override default OnNext logic.
        /// </summary>
        /// <returns>True if next page can be shown, false otherwise.</returns>
        public override bool OnNext()
        {
            if (this.radioButtonBypassProxy.IsChecked.Value)
            {
                SetupParameters.Instance().ProxyType = UnifiedSetupConstants.BypassProxy;
            }
            else if (this.radioButtonProxyServer.IsChecked.Value)
            {
                SetupParameters.Instance().ProxyType = UnifiedSetupConstants.CustomProxy;
            }

            ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "Set ProxyType to {0}", SetupParameters.Instance().ProxyType);
            return this.ValidatePage(ASRSetupFramework.PageNavigation.Navigation.Next);
        }

        /// <summary>
        /// Validates the page.
        /// The default implementation always returns true for 'validated'.
        /// </summary>
        /// <param name="navType">Navigation type.</param>
        /// <param name="pageId">Page Id being navigated to.</param>
        /// <returns>true if valid</returns>
        public override bool ValidatePage(
            ASRSetupFramework.PageNavigation.Navigation navType,
            string pageId = null)
        {
            // Set variables for navigation.
            this.navType = navType;
            this.pageId = pageId;

            if (navType == ASRSetupFramework.PageNavigation.Navigation.Previous)
            {
                return true;
            }

            this.smallErrorImage.Visibility = System.Windows.Visibility.Collapsed;
            this.smallSuccessCheckImage.Visibility = System.Windows.Visibility.Collapsed;
            this.SetConnectionStatusText(ASRResources.StringResources.ProxyConfigurationPageCheckingInternet);
            this.SetNextButtonState(false);
            this.PersistProxySettings();

            ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "Invoking the Validate page internal.");
            proxyTypeConnectionStateMap[this.GetCurrentSelectedProxyType()] =
                ProxyConnectionState.CheckingPending;
            ThreadPool.QueueUserWorkItem(new WaitCallback(this.ValidatePageInternal));

            return false;
        }

        /// <summary>
        /// Exits the page.
        /// </summary>
        public override void ExitPage()
        {
            // Detach event handlers.
            this.textBoxAddress.TextChanged -= this.TextBox_AddressChanged;
            this.textBoxPort.TextChanged -= this.TextBox_PortChanged;
            ASRSetupFramework.PropertyBagDictionary.Instance.SafeRemove(ASRSetupFramework.PropertyBagConstants.ErrorMessage);
            base.ExitPage();
        }

        /// <summary>
        /// Validate all the internal checks and navigate to next page if success
        /// </summary>
        /// <param name="o">required by callback</param>
        private void ValidatePageInternal(object o)
        {
            try
            {
                this.Dispatcher.Invoke(new WaitCallback(this.SetProgressbarState), true);
                ProxyType proxyType = ProxyType.Bypass;
                this.Dispatcher.Invoke(new Action(() =>
                {
                    proxyType = this.GetCurrentSelectedProxyType();
                }));

                bool status = true;
                bool? imageStatus = true;
                object nextButtonStateVar = true;
                string statusText = ASRResources.StringResources.ConnectedToInternet;

                if (!this.CheckEndpointReachability(proxyType))
                {
                    ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "CheckEndpointReachability has failed.");
                    statusText =
                        this.GetConnectionStatusString(proxyTypeConnectionStateMap[proxyType], out imageStatus);

                    nextButtonStateVar = proxyType;
                    status = false;
                }

                // Internet status and image reflects only the internet connectivity check
                this.Dispatcher.Invoke(new WaitCallback(this.SetConnectionStatusText), statusText);
                this.Dispatcher.Invoke(new WaitCallback(this.SetConnectionStateImage), imageStatus);
                this.Dispatcher.Invoke(new WaitCallback(this.SetProgressbarState), false);
                this.Dispatcher.Invoke(new WaitCallback(this.SetNextButtonState), nextButtonStateVar);

                ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "status = {0}", status);
                if (status)
                {
                    bool bypassProxy = false;
                    if (proxyType == ProxyType.Bypass)
                    {
                        bypassProxy = true;
                    }

                    SetupHelper.UpdateProxySettingsInRegistry(
                        PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ProxyAddress),
                        PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ProxyPort),
                        PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ProxyUsername),
                        PropertyBagDictionary.Instance.GetProperty<SecureString>(PropertyBagConstants.ProxyPassword),
                        bypassProxy);

                    this.Dispatcher.BeginInvoke(new NavigateDelegate(this.NavigatePage));
                }
            }
            catch (Exception ex)
            {
                ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Error, "Exception occurred during ValidatePageInternal: {0}", ex.ToString());
            }
        }

        /// <summary>
        /// Check if able to connect endpoints
        /// </summary>
        /// <param name="proxyType">param to hold proxyType </param>
        /// <returns>returns true on success</returns>
        private bool CheckEndpointReachability(ProxyType proxyType)
        {
            bool status = true;

            switch (proxyTypeConnectionStateMap[proxyType])
            {
                case ProxyConnectionState.CheckingPending:
                    ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "ProxyConnectionState: CheckingPending");

                    ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "Checking connectivity to end points url's.");
                    this.CheckInternetConnectivity(proxyType);

                    if (proxyTypeConnectionStateMap[(ProxyType)proxyType] != ProxyConnectionState.Connected)
                    {
                        ASRSetupFramework.SetupHelper.ShowError(this.lastDetailedError, this);
                        this.lastDetailedError = string.Empty;
                        status = false;
                    }

                    break;
                case ProxyConnectionState.Connected:
                    status = true;
                    break;
                case ProxyConnectionState.DataRequired:
                case ProxyConnectionState.NotConnected:
                case ProxyConnectionState.PasswordRequired:
                    status = false;
                    break;
            }

            return status;
        }

        /// <summary>
        /// Navigate page based on last user input.
        /// </summary>
        private void NavigatePage()
        {
            if (this.navType == ASRSetupFramework.PageNavigation.Navigation.Next)
            {
                ASRSetupFramework.PageNavigation.Instance.MoveToNextPage(false);
            }
            else if (this.navType == ASRSetupFramework.PageNavigation.Navigation.Previous)
            {
                ASRSetupFramework.PageNavigation.Instance.MoveToPreviousPage(false);
            }
            else if (this.navType == ASRSetupFramework.PageNavigation.Navigation.Jump)
            {
                ASRSetupFramework.PageNavigation.Instance.MoveToPage(this.pageId);
            }
        }

        /// <summary>
        /// Update the list of endpoints to be verified for connectivity.
        /// </summary>
        private void UpdateEndPoints()
        {
            string vaultLocation = string.Empty;
            if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
            {
                vaultLocation =
                    DRSetup.PropertyBagDictionary.Instance.GetProperty<string>(
                        DRSetup.PropertyBagConstants.VaultLocation);
                this.resourceId = DRSetup.PropertyBagDictionary.Instance.GetProperty<string>(
                DRSetup.PropertyBagConstants.ServiceResourceId);
            }
            else if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
            {
                vaultLocation =
                    DRSetup.PropertyBagDictionary.Instance.GetProperty<string>(
                        ASRSetupFramework.PropertyBagConstants.VaultLocation);
                this.resourceId = DRSetup.PropertyBagDictionary.Instance.GetProperty<string>(
                ASRSetupFramework.PropertyBagConstants.ResourceId);
            }

            string aadEndpoint =
                DRSetup.PropertyBagDictionary.Instance.GetProperty<string>(
                DRSetup.PropertyBagConstants.AadEndpoint);

            string aadEndPointFromCS =
                DRSetup.PropertyBagDictionary.Instance.GetProperty<string>(
                ASRSetupFramework.PropertyBagConstants.AcsUrl);

            this.listofEndpoints = new List<string>();
            List<string> listofPrivateEndpoints = IC.IntegrityCheckWrapper.GetEndpoints(
                vaultLocation);

            this.privateEndpointState = DRSetup.PropertyBagDictionary.Instance.GetProperty<string>(
                DRSetup.PropertyBagConstants.PrivateEndpointState);

            if (InstallActionProcessess.IsPrivateEndpointStateEnabled())
            {
                listofPrivateEndpoints.Remove(DRSetup.DRSetupConstants.AppInsightsUrl);

                foreach (string endpoint in listofPrivateEndpoints)
                {
                    string url = DRSetup.RegistrationHelper.GetUrl(this.resourceId, endpoint, this.privateEndpointState);
                    listofEndpoints.Add(url);
                }
            }
            else
            {
                listofEndpoints.AddRange(listofPrivateEndpoints);
            }

            if (!string.IsNullOrEmpty(aadEndpoint))
            {
                listofEndpoints.Add(aadEndpoint);
            }

            if (!string.IsNullOrEmpty(aadEndPointFromCS))
            {
                listofEndpoints.Add(aadEndPointFromCS);
            }
        }
        /// <summary>
        /// Check for network connectivity based on proxyType and upgrade UI Elements accordingly.
        /// </summary>
        /// <param name="proxyType">Type of Proxy</param>
        private void CheckInternetConnectivity(object proxyType)
        {
            try
            {
                string detailedError = string.Empty;
                IC.CheckEndpointConnectivityInput input =
                    new IC.CheckEndpointConnectivityInput()
                    {
                        Endpoints = this.listofEndpoints,
                    };

                foreach (string url in this.listofEndpoints)
                {
                    ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "Endpoint URLs : {0}", url);
                }

                switch ((ProxyType)proxyType)
                {
                    case ProxyType.Bypass:
                        ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "Checking for Bypass settings");
                        proxyTypeConnectionStateMap[ProxyType.Bypass] = ProxyConnectionState.Checking;
                        bool connectionStateBypass = true;

                        if (!DRSetup.SettingsFileHelper.IsMock)
                        {
                            input.WebProxy = new System.Net.WebProxy();
                            IC.Response validationResponse =
                                IC.IntegrityCheckWrapper.Evaluate(IC.Validations.CheckEndpointsConnectivity, input);
                            IC.IntegrityCheckWrapper.RecordOperation(
                                IC.ExecutionScenario.PreInstallation,
                                EvaluateOperations.ValidationMappings[IC.Validations.CheckEndpointsConnectivity],
                                validationResponse.Result,
                                validationResponse.Message,
                                validationResponse.Causes,
                                validationResponse.Recommendation,
                                validationResponse.Exception
                                );

                            if (validationResponse.Result == IC.OperationResult.Failed)
                            {
                                connectionStateBypass = false;
                                detailedError = string.Format(
                                    "{0}{1}{1}{2}:{3}{4}{5}{5}{6}:{7}{8}",
                                    validationResponse.Message,
                                    Environment.NewLine,
                                    ASRResources.StringResources.Causes,
                                    Environment.NewLine,
                                    validationResponse.Causes,
                                    Environment.NewLine,
                                    ASRResources.StringResources.Recommendation,
                                    Environment.NewLine,
                                    validationResponse.Recommendation);
                            }
                        }

                        ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "connectionState for Bypass proxy: {0}", connectionStateBypass);
                        if (connectionStateBypass)
                        {
                            proxyTypeConnectionStateMap[ProxyType.Bypass] = ProxyConnectionState.Connected;
                            ASRSetupFramework.PropertyBagDictionary.Instance.SafeAdd(ASRSetupFramework.PropertyBagConstants.ProxyType, UnifiedSetupConstants.BypassProxy);
                        }
                        else
                        {
                            proxyTypeConnectionStateMap[ProxyType.Bypass] = ProxyConnectionState.NotConnected;
                        }

                        break;
                    case ProxyType.Custom:
                        if (proxyTypeConnectionStateMap[ProxyType.Custom] != ProxyConnectionState.DataRequired &&
                            proxyTypeConnectionStateMap[ProxyType.Custom] != ProxyConnectionState.PasswordRequired)
                        {
                            ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "Checking for Custom settings");
                            proxyTypeConnectionStateMap[ProxyType.Custom] = ProxyConnectionState.Checking;
                            bool connectionStateCustom = true;

                            if (!DRSetup.SettingsFileHelper.IsMock)
                            {
                                input.WebProxy = ASRSetupFramework.SetupHelper.BuildWebProxyFromPropertyBag();
                                IC.Response validationResponse =
                                    IC.IntegrityCheckWrapper.Evaluate(IC.Validations.CheckEndpointsConnectivity, input);
                                IC.IntegrityCheckWrapper.RecordOperation(
                                    IC.ExecutionScenario.PreInstallation,
                                    EvaluateOperations.ValidationMappings[IC.Validations.CheckEndpointsConnectivity],
                                    validationResponse.Result,
                                    validationResponse.Message,
                                    validationResponse.Causes,
                                    validationResponse.Recommendation,
                                    validationResponse.Exception
                                    );

                                if (validationResponse.Result == IC.OperationResult.Failed)
                                {
                                    connectionStateCustom = false;
                                    detailedError = string.Format(
                                        "{0}{1}{1}{2}:{3}{4}{5}{5}{6}:{7}{8}",
                                        validationResponse.Message,
                                        Environment.NewLine,
                                        ASRResources.StringResources.Causes,
                                        Environment.NewLine,
                                        validationResponse.Causes,
                                        Environment.NewLine,
                                        ASRResources.StringResources.Recommendation,
                                        Environment.NewLine,
                                        validationResponse.Recommendation);
                                }
                            }

                            if (connectionStateCustom)
                            {
                                proxyTypeConnectionStateMap[ProxyType.Custom] = ProxyConnectionState.Connected;
                                ASRSetupFramework.PropertyBagDictionary.Instance.SafeAdd(
                                    ASRSetupFramework.PropertyBagConstants.ProxyType,
                                    UnifiedSetupConstants.CustomProxy);
                            }
                            else
                            {
                                proxyTypeConnectionStateMap[ProxyType.Custom] = ProxyConnectionState.NotConnected;
                            }
                        }

                        break;
                }

                this.lastDetailedError = detailedError;
            }
            catch (Exception ex)
            {
                ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Error, "Exception occurred: ", ex);
                this.lastDetailedError = ASRResources.StringResources.InternalError;
            }
        }

        /// <summary>
        /// Write changed proxy settings to PropertyBag.
        /// </summary>
        private void PersistProxySettings()
        {
            string address = string.Empty;
            string port = string.Empty;
            string userName = string.Empty;
            string bypassProxyURLs = string.Empty;
            SecureString password = null;
            UnifiedSetupConstants.ProxyChangeAction proxyAction = UnifiedSetupConstants.ProxyChangeAction.NoChange;
            bool bypassDefaultProxy = false;

            // Set what option was last selected
            if (this.radioButtonBypassProxy.IsChecked.Value)
            {
                bypassDefaultProxy = true;
                proxyAction = UnifiedSetupConstants.ProxyChangeAction.Bypass;
            }
            else if (this.radioButtonProxyServer.IsChecked.Value)
            {
                proxyAction = UnifiedSetupConstants.ProxyChangeAction.Write;
                address = this.textBoxAddress.Text;
                port = this.textBoxPort.Text;
                bypassDefaultProxy = false;
                bypassProxyURLs = this.textBypassProxy.Text;
                if (this.checkBoxCredentials.IsChecked.HasValue &&
                                    this.checkBoxCredentials.IsChecked.Value)
                {
                    userName = this.textBoxUserName.Text;
                    password = this.passwordBoxPassword.SecurePassword;
                }
            }

            ASRSetupFramework.PropertyBagDictionary.Instance.SafeAdd(ASRSetupFramework.PropertyBagConstants.BypassDefaultProxy, bypassDefaultProxy);
            ASRSetupFramework.PropertyBagDictionary.Instance.SafeAdd(ASRSetupFramework.PropertyBagConstants.ProxyChanged, proxyAction);
            ASRSetupFramework.PropertyBagDictionary.Instance.SafeAdd(ASRSetupFramework.PropertyBagConstants.ProxyAddress, address);
            ASRSetupFramework.PropertyBagDictionary.Instance.SafeAdd(ASRSetupFramework.PropertyBagConstants.ProxyPort, port);
            ASRSetupFramework.PropertyBagDictionary.Instance.SafeAdd(ASRSetupFramework.PropertyBagConstants.ProxyUsername, userName);
            ASRSetupFramework.PropertyBagDictionary.Instance.SafeAdd(ASRSetupFramework.PropertyBagConstants.ProxyPassword, password);
            ASRSetupFramework.PropertyBagDictionary.Instance.SafeAdd(ASRSetupFramework.PropertyBagConstants.BypassProxyURLs, bypassProxyURLs);
        }

        /// <summary>
        /// Event handler for CheckBoxCredentialsChanged
        /// </summary>
        /// <param name="sender">Sender of event</param>
        /// <param name="e">Routed Event Args</param>
        private void CheckBoxCredentialsChanged(object sender, RoutedEventArgs e)
        {
            if (proxyTypeConnectionStateMap != null)
            {
                proxyTypeConnectionStateMap[ProxyType.Custom] = ProxyConnectionState.CheckingPending;

                if (!string.IsNullOrEmpty(this.textBoxUserName.Text)
                    && string.IsNullOrEmpty(this.passwordBoxPassword.Password)
                    && this.checkBoxCredentials.IsChecked.Value)
                {
                    proxyTypeConnectionStateMap[ProxyType.Custom] = ProxyConnectionState.PasswordRequired;
                }

                this.SetConnectionStatusElements(ProxyType.Custom);
                this.SetNextButtonState(ProxyType.Custom);
            }

            this.SetCredentialControlsState(true);
        }

        /// <summary>
        /// Event handler for RadioButtonBypassProxyChanged
        /// </summary>
        /// <param name="sender">Sender object</param>
        /// <param name="e">RoutedEvent Args</param>
        private void RadioButtonBypassProxyChanged(object sender, RoutedEventArgs e)
        {
            if (this.radioButtonBypassProxy.IsChecked.Value)
            {
                if (proxyTypeConnectionStateMap != null)
                {
                    this.SetConnectionStatusElements(ProxyType.Bypass);
                    this.SetNextButtonState(ProxyType.Bypass);
                }
            }
        }

        /// <summary>
        /// Event handler RadioButtonProxyServerChanged
        /// </summary>
        /// <param name="sender">Sender object</param>
        /// <param name="e">RoutedEvent Args</param>
        private void RadioButtonProxyServerChanged(object sender, RoutedEventArgs e)
        {
            if (this.radioButtonProxyServer.IsChecked.Value)
            {
                if (proxyTypeConnectionStateMap != null)
                {
                    this.SetConnectionStatusElements(ProxyType.Custom);
                    this.SetNextButtonState(ProxyType.Custom);
                }

                // Show UI controls.
                this.customProxyStackPanel.Visibility = System.Windows.Visibility.Visible;
                this.credentialStackPanel.Visibility = System.Windows.Visibility.Visible;
                this.checkBoxCredentials.Visibility = System.Windows.Visibility.Visible;
                this.bypassProxyStackPanel.Visibility = System.Windows.Visibility.Visible;
                this.bypassProxyRecommendationStackPanel.Visibility = System.Windows.Visibility.Visible;
            }
            else
            {
                // Hide UI controls.
                this.customProxyStackPanel.Visibility = System.Windows.Visibility.Hidden;
                this.credentialStackPanel.Visibility = System.Windows.Visibility.Hidden;
                this.checkBoxCredentials.Visibility = System.Windows.Visibility.Hidden;
                this.bypassProxyStackPanel.Visibility = System.Windows.Visibility.Hidden;
                this.bypassProxyRecommendationStackPanel.Visibility = System.Windows.Visibility.Hidden;
            }
        }

        /// <summary>
        /// Set the Proxy Controls state based on the proxy check box checked state
        /// </summary>
        private void SetProxyControlsState()
        {
            bool isProxyServerChecked = false;
            bool? val = this.radioButtonProxyServer.IsChecked;
            if (this.radioButtonProxyServer.IsChecked.HasValue)
            {
                isProxyServerChecked = this.radioButtonProxyServer.IsChecked.Value;
            }

            this.Label_1.IsEnabled = isProxyServerChecked;
            this.Label_2.IsEnabled = isProxyServerChecked;
            this.textBoxAddress.IsEnabled = isProxyServerChecked;
            this.textBoxPort.IsEnabled = isProxyServerChecked;
            this.checkBoxCredentials.IsEnabled = isProxyServerChecked;
            this.SetCredentialControlsState(isProxyServerChecked);
        }

        /// <summary>
        /// Set the Credential Controls state based on the credentials check box and proxy check box checked state
        /// </summary>
        /// <param name="isProxyServerChecked">proxy check box</param>
        private void SetCredentialControlsState(bool isProxyServerChecked)
        {
            bool isCredentialsChecked = false;

            if (!isProxyServerChecked)
            {
                this.checkBoxCredentials.IsChecked = false;
            }

            if (isProxyServerChecked && this.checkBoxCredentials.IsChecked.HasValue)
            {
                isCredentialsChecked = this.checkBoxCredentials.IsChecked.Value;
            }

            this.Label_3.IsEnabled = isCredentialsChecked;
            this.Label_4.IsEnabled = isCredentialsChecked;
            this.textBoxUserName.IsEnabled = isCredentialsChecked;
            this.passwordBoxPassword.IsEnabled = isCredentialsChecked;
        }

        /// <summary>
        /// Address text is changed.
        /// </summary>
        /// <param name="sender">The sender.</param>
        /// <param name="e">The <see cref="System.Windows.Controls.TextChangedEventArgs"/> instance containing the event data.</param>
        private void TextBox_AddressChanged(object sender, TextChangedEventArgs e)
        {
            if (proxyTypeConnectionStateMap != null)
            {
                proxyTypeConnectionStateMap[ProxyType.Custom] = ProxyConnectionState.CheckingPending;
                this.SetConnectionStatusElements(ProxyType.Custom);
                this.SetNextButtonState(ProxyType.Custom);

                // Disabling port text box and next button if address has https.
                if (ASRSetupFramework.SetupHelper.IsAddressHasHTTPS(this.textBoxAddress.Text))
                {
                    UpdateUI_WrongAddress();
                    this.AddressErrorTextblock.Text = ASRResources.StringResources.HttpsNotSupportedText;
                }
                // Disabling port text box and next button if address is not http.
                else if (!ASRSetupFramework.SetupHelper.IsAddressHasHTTP(this.textBoxAddress.Text))
                {
                    UpdateUI_WrongAddress();
                    this.AddressErrorTextblock.Text = ASRResources.StringResources.InvalidProxyAddressText;
                }
                else
                {
                    this.AddressErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                    this.AddressErrorTextblock.Visibility = System.Windows.Visibility.Collapsed;
                    this.textBoxPort.IsEnabled = true;
                    this.Page.Host.SetPreviousButtonState(true, true);
                    this.Page.Host.EnableGlobalSideBarNavigation();
                }
            }
        }

        private void UpdateUI_WrongAddress()
        {
            this.AddressErrorImage.Visibility = System.Windows.Visibility.Visible;
            this.AddressErrorTextblock.Visibility = System.Windows.Visibility.Visible;
            this.textBoxPort.IsEnabled = false;
            this.textBoxPort.Clear();
            this.Page.Host.SetNextButtonState(true, false);
            this.Page.Host.SetPreviousButtonState(true, false);
            this.Page.Host.DisableGlobalSideBarNavigation();
        }

        /// <summary>
        /// Port text is changed.
        /// </summary>
        /// <param name="sender">The sender.</param>
        /// <param name="e">The <see cref="System.Windows.Controls.TextChangedEventArgs"/> instance containing the event data.</param>
        private void TextBox_PortChanged(object sender, TextChangedEventArgs e)
        {
            if (proxyTypeConnectionStateMap != null)
            {
                proxyTypeConnectionStateMap[ProxyType.Custom] = ProxyConnectionState.CheckingPending;
                this.SetConnectionStatusElements(ProxyType.Custom);
                this.SetNextButtonState(ProxyType.Custom);
            }
        }

        /// <summary>
        /// User name text is changed.
        /// </summary>
        /// <param name="sender">The sender.</param>
        /// <param name="e">The <see cref="System.Windows.Controls.TextChangedEventArgs"/> instance containing the event data.</param>
        private void TextBox_UserNameChanged(object sender, TextChangedEventArgs e)
        {
            if (proxyTypeConnectionStateMap != null)
            {
                if (!string.IsNullOrEmpty(this.textBoxUserName.Text))
                {
                    proxyTypeConnectionStateMap[ProxyType.Custom] = ProxyConnectionState.PasswordRequired;
                }
                else
                {
                    proxyTypeConnectionStateMap[ProxyType.Custom] = ProxyConnectionState.DataRequired;
                }

                this.SetConnectionStatusElements(ProxyType.Custom);
                this.SetNextButtonState(ProxyType.Custom);
            }

            this.passwordBoxPassword.Clear();
        }

        /// <summary>
        /// Passwords is changed.
        /// </summary>
        /// <param name="sender">The sender.</param>
        /// <param name="e">The <see cref="System.Windows.RoutedEventArgs"/> instance containing the event data.</param>
        private void PasswordBoxChanged(object sender, RoutedEventArgs e)
        {
            if (proxyTypeConnectionStateMap != null)
            {
                proxyTypeConnectionStateMap[ProxyType.Custom] = ProxyConnectionState.CheckingPending;
                if (!string.IsNullOrEmpty(this.textBoxUserName.Text) && string.IsNullOrEmpty(this.passwordBoxPassword.Password))
                {
                    proxyTypeConnectionStateMap[ProxyType.Custom] = ProxyConnectionState.PasswordRequired;
                }

                this.SetConnectionStatusElements(ProxyType.Custom);
                this.SetNextButtonState(ProxyType.Custom);
            }

            ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Debug, "Password has been changed." + passwordChanged);
            this.passwordChanged = true;
        }

        /// <summary>
        /// Set the next button state
        /// </summary>
        /// <param name="parameter">parameter which is used to evaluate the next button state.</param>
        private void SetNextButtonState(object parameter)
        {
            bool isNextButtonEnabled = true;

            if (parameter is ProxyType)
            {
                if (this.radioButtonProxyServer.IsChecked.HasValue && this.radioButtonProxyServer.IsChecked.Value)
                {
                    if (String.IsNullOrEmpty(this.textBoxAddress.Text) || String.IsNullOrEmpty(this.textBoxPort.Text))
                    {
                        isNextButtonEnabled = false;
                    }
                    else
                    {
                        if (this.checkBoxCredentials.IsChecked.HasValue && this.checkBoxCredentials.IsChecked.Value)
                        {
                            if (String.IsNullOrEmpty(this.textBoxUserName.Text) ||
                                    (this.passwordBoxPassword.SecurePassword.Length == 0))
                            {
                                isNextButtonEnabled = false;
                            }
                        }
                    }
                }
            }
            else
            {
                isNextButtonEnabled = (bool)parameter;
            }

            this.Page.Host.SetNextButtonState(true, isNextButtonEnabled);
            this.Page.Host.SetPreviousButtonState(true, true);

            // Enable Proxy buttons during validation.
            this.radioButtonBypassProxy.IsEnabled = true;

            // Enable global navigation
            this.Page.Host.EnableGlobalSideBarNavigation();
        }

        /// <summary>
        /// Restricts the port input to numeric digits.
        /// </summary>
        /// <param name="sender">The sender</param>
        /// <param name="e"> /// <param name="e">The <see cref="System.Windows.Input.TextCompositionEventArgs"/> instance containing the event data.</param></param>
        private void TextBoxPort_PreviewTextInput(object sender, System.Windows.Input.TextCompositionEventArgs e)
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
        /// Sets connection status string and image based on ProxyType.
        /// </summary>
        /// <param name="proxyType">Type of proxy to update UI for.</param>
        private void SetConnectionStatusElements(ProxyType proxyType)
        {
            bool? imageStatus;
            this.internetStatus.Text = this.GetConnectionStatusString(proxyTypeConnectionStateMap[proxyType], out imageStatus);
            this.SetConnectionStateImage(imageStatus);
        }

        /// <summary>
        /// Sets status string.
        /// </summary>
        /// <param name="statusText">text to update UI for.</param>
        private void SetConnectionStatusText(object statusText)
        {
            this.internetStatus.Text = (string)statusText;
        }

        /// <summary>
        /// Gets connection status string based on connection state.
        /// </summary>
        /// <param name="connectionState">Connection state of proxy.</param>
        /// <param name="imageStatus">Image status to be shown.</param>
        /// <returns>Gets cnnection state based on connection state.</returns>
        private string GetConnectionStatusString(ProxyConnectionState connectionState, out bool? imageStatus)
        {
            if (connectionState == ProxyConnectionState.Checking)
            {
                imageStatus = null;
                return ASRResources.StringResources.CheckingInternetConnectivity;
            }
            else if (connectionState == ProxyConnectionState.CheckingPending)
            {
                imageStatus = null;
                return ASRResources.StringResources.ClickNextToCheckConnectivity;
            }
            else if (connectionState == ProxyConnectionState.Connected)
            {
                imageStatus = true;
                return ASRResources.StringResources.ConnectedToInternet;
            }
            else if (connectionState == ProxyConnectionState.DataRequired)
            {
                imageStatus = null;
                return ASRResources.StringResources.ProxyDataRequired;
            }
            else if (connectionState == ProxyConnectionState.PasswordRequired)
            {
                imageStatus = null;
                return ASRResources.StringResources.ProxyRequiresPassword;
            }
            else if (connectionState == ProxyConnectionState.NotConnected)
            {
                imageStatus = false;
                return ASRResources.StringResources.NotConnectedToInternet;
            }

            imageStatus = null;
            return null;
        }

        /// <summary>
        /// Set connection status image visibility based on imageStatus.
        /// </summary>
        /// <param name="imageStatus">Status to be shown.</param>
        private void SetConnectionStateImage(object imageStatus)
        {
            if (imageStatus == null)
            {
                this.smallErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                this.smallSuccessCheckImage.Visibility = System.Windows.Visibility.Collapsed;
                this.checkingConnectivity.Visibility = System.Windows.Visibility.Collapsed;
            }
            else
            {
                if ((bool)imageStatus)
                {
                    this.smallErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                    this.smallSuccessCheckImage.Visibility = System.Windows.Visibility.Visible;
                    this.checkingConnectivity.Visibility = System.Windows.Visibility.Collapsed;
                }
                else
                {
                    this.smallErrorImage.Visibility = System.Windows.Visibility.Visible;
                    this.smallSuccessCheckImage.Visibility = System.Windows.Visibility.Collapsed;
                    this.checkingConnectivity.Visibility = System.Windows.Visibility.Collapsed;
                }
            }
        }

        /// <summary>
        /// Show or Hide progress bar for connection checking.
        /// </summary>
        /// <param name="showProgressBar">IF progress bar is to be shown or not.</param>
        private void SetProgressbarState(object showProgressBar)
        {
            if ((bool)showProgressBar)
            {
                this.checkingConnectivity.Visibility = System.Windows.Visibility.Visible;
            }
            else
            {
                this.checkingConnectivity.Visibility = System.Windows.Visibility.Hidden;
            }
        }

        /// <summary>
        /// Sets connection state based on currently selected option.
        /// </summary>
        /// <returns>Current selected proxy type.</returns>
        private ProxyType GetCurrentSelectedProxyType()
        {
            if (this.radioButtonBypassProxy.IsChecked.Value)
            {
                return ProxyType.Bypass;
            }
            else
            {
                return ProxyType.Custom;
            }
        }

        private void HandleRequestNavigate(object sender, RoutedEventArgs e)
        {
            string navigateUri = ((Hyperlink)sender).NavigateUri.ToString();
            Process.Start(new ProcessStartInfo(navigateUri));
            e.Handled = true;
        }
    }
}
