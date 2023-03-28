using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using ASRResources;
using ASRSetupFramework;

namespace UnifiedAgentConfigurator
{
    /// <summary>
    /// Interaction logic for ProxyConfigurationPage.xaml
    /// </summary>
    public partial class ProxyConfigurationPage : BasePageForWpfControls
    {
        /// <summary>
        /// If proxy address is valid.
        /// </summary>
        private bool validAddress = true;

        /// <summary>
        /// If proxy port is valid.
        /// </summary>
        private bool validPort = true;

        /// <summary>
        /// Initializes a new instance of the <see cref="ProxyConfigurationPage"/> class.
        /// </summary>
        public ProxyConfigurationPage()
        {
            InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="ProxyConfigurationPage"/> class.
        /// </summary>
        /// <param name="page">Setup framework base page.</param>
        public ProxyConfigurationPage(ASRSetupFramework.Page page)
            : base(page, StringResources.ProxyConfigurationPage, 1)
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Code executed when user enters this page.
        /// </summary>
        public override void EnterPage()
        {
            base.EnterPage();

            string proxyAddress = PropertyBagDictionary.Instance.GetProperty<string>(
                PropertyBagConstants.ProxyAddress);
            string proxyPort = PropertyBagDictionary.Instance.GetProperty<string>(
                PropertyBagConstants.ProxyPort);

            if (!string.IsNullOrEmpty(proxyAddress))
            {
                this.CustomProxyRadio.IsChecked = true;
                this.txtProxyAddress.Text = proxyAddress;
                this.txtProxyPort.Text = proxyPort;
            }
        }

        /// <summary>
        /// Change event handler for text change in proxy address box.
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Event arguments.</param>
        private void txtProxyAddress_TextChanged(object sender, TextChangedEventArgs e)
        {
            bool validUrl = false;
            // This code is executed before the value changes in page. So no
            // heavy code here or the UI will stall.
            if (Uri.IsWellFormedUriString((sender as TextBox).Text, UriKind.Absolute))
            {
                Uri url = new Uri((sender as TextBox).Text);

                // We only support HTTP.
                if (url.Scheme != ConfigurationConstants.UrlHttpScheme)
                {
                    validUrl = false;
                }
                else
                {
                    validUrl = true;
                }
            }

            if (validUrl)
            {
                Uri url = new Uri((sender as TextBox).Text);
                this.txtProxyPort.Text = url.Port.ToString();

                BitmapImage logo = new BitmapImage();
                logo.BeginInit();
                logo.UriSource = new Uri(
                    "pack://application:,,,/ASRSetupFramework;component/Images/smallGreenCheck.png");
                logo.EndInit();
                this.imgProxyAddress.Source = logo;
                this.validAddress = true;
            }
            else
            {
                BitmapImage logo = new BitmapImage();
                logo.BeginInit();
                logo.UriSource = new Uri(
                    "pack://application:,,,/ASRSetupFramework;component/Images/smallError.png");
                logo.EndInit();
                this.imgProxyAddress.Source = logo;
                this.validAddress = false;
            }
        }

        /// <summary>
        /// Change event handler for text change in proxy port.
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Event arguments.</param>
        private void txtProxyPort_TextChanged(object sender, TextChangedEventArgs e)
        {
            // This code is executed before the value changes in page. So no
            // heavy code here or the UI will stall.
            UInt16 portNumber = 0;
            if (UInt16.TryParse((sender as TextBox).Text, out portNumber))
            {
                BitmapImage logo = new BitmapImage();
                logo.BeginInit();
                logo.UriSource = new Uri(
                    "pack://application:,,,/ASRSetupFramework;component/Images/smallGreenCheck.png");
                logo.EndInit();
                this.imgProxyPort.Source = logo;
                this.validPort = true;
            }
            else
            {
                BitmapImage logo = new BitmapImage();
                logo.BeginInit();
                logo.UriSource = new Uri(
                    "pack://application:,,,/ASRSetupFramework;component/Images/smallError.png");
                logo.EndInit();
                this.imgProxyPort.Source = logo;
                this.validPort = false;
            }
        }

        /// <summary>
        /// Executed when customer clicks next button.
        /// </summary>
        /// <returns></returns>
        public override bool OnNext()
        {
            if (this.DefaultProxyRadio.IsChecked.Value || (this.validPort && this.validAddress))
            {
                return base.OnNext();
            }
            else
            {
                MessageBox.Show(
                    StringResources.CorrectErrorsOnPage, 
                    StringResources.ProxyConfigurationPage, 
                    MessageBoxButton.OKCancel, 
                    MessageBoxImage.Error, 
                    MessageBoxResult.OK);
                return false;
            }
        }

        /// <summary>
        /// Routine called on page exit.
        /// </summary>
        public override void ExitPage()
        {
            if (this.CustomProxyRadio.IsChecked.Value)
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ProxyAddress, 
                    this.txtProxyAddress.Text);
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ProxyPort, 
                    this.txtProxyPort.Text);
            }
            else
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ProxyAddress, 
                    string.Empty);
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ProxyPort, 
                    string.Empty);
            }

            base.ExitPage();
        }
    }
}
