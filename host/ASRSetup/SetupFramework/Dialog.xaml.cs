using System;
using System.Windows;
using System.Windows.Media.Imaging;
using ASRResources;

namespace ASRSetupFramework
{
    /// <summary>
    /// Interaction logic for Dialog.xaml
    /// </summary>
    [System.Diagnostics.CodeAnalysis.SuppressMessage(
        "Microsoft.StyleCop.CSharp.MaintainabilityRules",
        "SA1402:FileMayOnlyContainASingleClass",
        Justification = "All prereq helpers in one class")]
    public partial class Dialog : Window
    {
        /// <summary>
        /// Link associated with primary info.
        /// </summary>
        private string primaryLink;

        /// <summary>
        /// Link associated with secondary info.
        /// </summary>
        private string extraInfoLink;

        /// <summary>
        /// Initializes a new instance of the Dialog class
        /// </summary>
        /// <param name="primaryError">Main error to be shown.</param>
        /// <param name="primaryLinkText">Documentation link text for primaryError.</param>
        /// <param name="primaryLink">Documentation link for primaryError.</param>
        /// <param name="extraInfoError">Additional info for the error shown.</param>
        /// <param name="extraInfoLink">Documentation link text for extraInfoError.</param>
        public Dialog(string primaryError, string primaryLinkText, string primaryLink, string extraInfoError, string extraInfoLink)
        {
            Trc.Log(LogLevel.Debug, "Adding dialog");
            this.InitializeComponent();
            this.dialogWindow.Title = StringResources.SetupMessageBoxTitle;
            
            BitmapImage imageSrc = new BitmapImage();
            imageSrc.BeginInit();
            imageSrc.UriSource = new Uri("pack://application:,,,/ASRSetupFramework;component/Images/HyperVRecoveryManager.ico");
            imageSrc.EndInit();

            this.dialogWindow.Icon = imageSrc;
            this.txtBlock_PrimaryError.Text = primaryError;
            this.button_PrimaryError.Content = primaryLinkText;
            if (string.IsNullOrEmpty(primaryLink))
            {
                this.button_PrimaryError.Visibility = System.Windows.Visibility.Collapsed;
            }

            if (!string.IsNullOrEmpty(extraInfoError))
            {
                this.txtBlock_ExtraInfo.Text = extraInfoError;
            }
            else
            {
                this.txtBlock_ExtraInfo.Visibility = System.Windows.Visibility.Collapsed;
            }

            if (string.IsNullOrEmpty(extraInfoLink))
            {
                this.button_ExtraInfo.Visibility = System.Windows.Visibility.Collapsed;
            }
            else
            {
                this.button_ExtraInfo.Visibility = System.Windows.Visibility.Visible;
                this.button_ExtraInfo.Content = StringResources.LearnMore;
            }

            this.primaryLink = primaryLink;
            this.extraInfoLink = extraInfoLink;
        }

        /// <summary>
        /// Ok button click handler
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Event arguments.</param>
        private void OKButton_Click(object sender, System.Windows.RoutedEventArgs e)
        {
            DialogResult = true;
        }

        /// <summary>
        /// Primary link button click handler.
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Event arguments.</param>
        private void Button_Click(object sender, RoutedEventArgs e)
        {
            System.Diagnostics.Process.Start(this.primaryLink);
            DialogResult = true;
        }

        /// <summary>
        /// Extra info button click handler.
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Event arguments.</param>
        private void Button_ExtraInfo_Click(object sender, RoutedEventArgs e)
        {
            System.Diagnostics.Process.Start(this.extraInfoLink);
            DialogResult = true;
        }
    }

    /// <summary>
    /// Dialog helper class.
    /// </summary>
    [System.Diagnostics.CodeAnalysis.SuppressMessage(
        "Microsoft.StyleCop.CSharp.MaintainabilityRules",
        "SA1402:FileMayOnlyContainASingleClass",
        Justification = "All prereq helpers in one class")]
    public class DialogHelper
    {
        /// <summary>
        /// Primary information
        /// </summary>
        private string primaryError;

        /// <summary>
        /// Primary info documentation link text.
        /// </summary>
        private string primaryLinkText;

        /// <summary>
        /// Primary info documentation link
        /// </summary>
        private string primaryLink;

        /// <summary>
        /// Extra information.
        /// </summary>
        private string extraInfoError;

        /// <summary>
        /// Extra info documentation link.
        /// </summary>
        private string extraInfoLink;

        /// <summary>
        /// Initializes a new instance of the DialogHelper class.
        /// </summary>
        /// <param name="primaryError"> Primary information.</param>
        /// <param name="primaryLinkText">Primary info documentation link text.</param>
        /// <param name="primaryLink">Primary info documentation link.</param>
        /// <param name="extraInfoError">Extra information.</param>
        /// <param name="extraInfoLink">Extra info documentation link.</param>
        public DialogHelper(string primaryError, string primaryLinkText, string primaryLink, string extraInfoError, string extraInfoLink)
        {
            this.primaryError = primaryError;
            this.primaryLinkText = primaryLinkText;
            this.primaryLink = primaryLink;
            this.extraInfoError = extraInfoError;
            this.extraInfoLink = extraInfoLink;
        }

        /// <summary>
        /// Show the UI dialog.
        /// </summary>
        /// <returns>Result of the dialog.</returns>
        public bool? ShowDialog()
        {
            return new Dialog(this.primaryError, this.primaryLinkText, this.primaryLink, this.extraInfoError, this.extraInfoLink).ShowDialog();
        }
    }
}
