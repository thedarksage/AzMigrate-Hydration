//---------------------------------------------------------------
//  <copyright file="PrerequisiteUIHelper.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Routines to allow dynamic addition of UI elements associated
//  with Prereqs on to WPF controls.
//  </summary>
//
//  History:     30-Sep-2014   Ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using WpfResources;

namespace Microsoft.DisasterRecovery.SetupFramework
{
    /// <summary>
    /// PrerequisiteUIHelper class for adding PrerequisiteHelper to UI, updating UI.
    /// </summary>
    [System.Diagnostics.CodeAnalysis.SuppressMessage(
        "Microsoft.StyleCop.CSharp.MaintainabilityRules",
        "SA1402:FileMayOnlyContainASingleClass",
        Justification = "All prereq helpers in one class")]
    public class PrerequisiteUIHelper
    {
        #region UI Constants for arranging buttons
        /// <summary>
        /// Image width.
        /// </summary>
        private const int ImageWidth = 16;

        /// <summary>
        /// Hyperlink button width.
        /// </summary>
        private const int HyperLinkButtonWidth = 150;

        /// <summary>
        /// Label width showing status.
        /// </summary>
        private const int BoolLabelWidth = 100;
        #endregion

        #region Private Members
        /// <summary>
        /// Panel length of panel to add UI elements to.
        /// </summary>
        private double panelLength = 0;

        /// <summary>
        /// Panel to add UI elements to.
        /// </summary>
        private Panel panel;

        /// <summary>
        /// Prereq text box.
        /// </summary>
        private TextBlock preReqNameTextBlock;

        /// <summary>
        /// Prereq status label.
        /// </summary>
        private Label preReqStatusLabel;

        /// <summary>
        /// Prereq status image.
        /// </summary>
        private Image preReqStatusImage;

        /// <summary>
        /// More info button for preqreq
        /// </summary>
        private Button preReqHelpLinkButton;

        /// <summary>
        /// Dialog to be opened on clicking help button.
        /// </summary>
        private DialogHelper helpLink;
        #endregion

        /// <summary>
        /// Initializes a new instance of the PrerequisiteUIHelper class.
        /// </summary>
        /// <param name="panelLengthArgs">Total length of Panel to which UI elements will be added.</param>
        /// <param name="panelArgs">Panel object tp which UI elements will be added.</param>
        private PrerequisiteUIHelper(double panelLengthArgs, Panel panelArgs)
        {
            this.panelLength = panelLengthArgs;
            this.panel = panelArgs;
        }

        /// <summary>
        /// Returns a new object of PrerequisiteUIHelper.
        /// </summary>
        /// <param name="panelLength">Panel Length.</param>
        /// <param name="panel">Panel for adding UI elements.</param>
        /// <returns>UI handler object.</returns>
        public static PrerequisiteUIHelper PreRequisiteUIHandler(double panelLength, Panel panel)
        {
            return new PrerequisiteUIHelper(panelLength, panel);
        }

        /// <summary>
        /// Adds UI controls for preReq object.
        /// </summary>
        /// <param name="preReq">PrerequisiteHelper object to be added to UI.</param>
        public void AddPreRequisiteToUI(PrerequisiteHelper preReq)
        {
            if (preReq == null)
            {
                return;
            }

            Trc.Log(LogLevel.Debug, "Adding PreReq to UI for PreReq " + preReq.PreRequisiteName);
            this.preReqNameTextBlock = this.PrepareTextBlock(preReq.PreRequisiteName, this.panelLength - (ImageWidth + HyperLinkButtonWidth + BoolLabelWidth));
            this.preReqStatusImage = this.PrepareImage(preReq.Passed, ImageWidth);
            this.preReqStatusLabel = this.PrepareLabel(WPFResourceDictionary.Status_Checking, BoolLabelWidth);
            this.preReqHelpLinkButton = this.PrepareButton(WPFResourceDictionary.ViewDetails, HyperLinkButtonWidth, true);

            preReq.AssociateUIHandler(this);
            this.panel.Children.Add(this.preReqNameTextBlock);
            this.panel.Children.Add(this.preReqStatusImage);
            this.panel.Children.Add(this.preReqStatusLabel);
            this.panel.Children.Add(this.preReqHelpLinkButton);

            this.helpLink = preReq.HelpLink;
        }

        /// <summary>
        /// Update PrerequisiteHelper object values in UI.
        /// </summary>
        /// <param name="preReq">PrerequisiteHelper object for which UI elements need to be updated.</param>
        public void UpdatePreRequisitesInUI(PrerequisiteHelper preReq)
        {
            // Update Image
            Trc.Log(LogLevel.Debug, "Starting update of UI for PreReq " + preReq.PreRequisiteName);
            BitmapImage imageSrc = new BitmapImage();
            imageSrc.BeginInit();

            if (preReq.Passed.HasValue)
            {
                if (preReq.Passed.Value)
                {
                    imageSrc.UriSource = new Uri("pack://application:,,,/DRSetupFramework;component/Images/smallGreenCheck.png");
                }
                else
                {
                    imageSrc.UriSource = new Uri("pack://application:,,,/DRSetupFramework;component/Images/smallError.png");
                }
            }
            else
            {
                imageSrc.UriSource = new Uri("pack://application:,,,/DRSetupFramework;component/Images/warning.png");
            }

            // Update label marking status of PreReq
            if (preReq.Passed.HasValue)
            {
                if (preReq.Passed.Value)
                {
                    this.preReqStatusLabel.Content = WPFResourceDictionary.Status_Passed;
                }
                else
                {
                    this.preReqStatusLabel.Content = WPFResourceDictionary.Status_Failed;
                }
            }
            else
            {
                this.preReqStatusLabel.Content = WPFResourceDictionary.Status_Skipped;
            }

            imageSrc.EndInit();
            this.preReqStatusImage.Source = imageSrc;
            this.preReqStatusImage.Visibility = Visibility.Visible;

            // Visibility depending on failure\pass
            if (!preReq.Passed.Value)
            {
                this.preReqHelpLinkButton.Visibility = System.Windows.Visibility.Visible;
            }
        }

        /// <summary>
        /// Returns new button object to be added to UI.
        /// </summary>
        /// <param name="content">Text to be shown.</param>
        /// <param name="width">Width of button.</param>
        /// <param name="isLink">If button is a hyperlink.</param>
        /// <returns>Button object.</returns>
        private Button PrepareButton(string content, int width, bool isLink = false)
        {
            Button button = new Button();
            button.Content = content;
            button.Style = WPFResourceDictionary.ButtonHyperLinkStyle;
            button.Foreground = new SolidColorBrush(Colors.Blue);

            button.Click += this.ViewDetailsButtonClick;
            button.Visibility = System.Windows.Visibility.Hidden;
            return button;
        }

        /// <summary>
        /// Click event for button.
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Event arguments.</param>
        private void ViewDetailsButtonClick(object sender, RoutedEventArgs e)
        {
            this.helpLink.ShowDialog();
        }

        /// <summary>
        /// Returns new label object to be added to UI.
        /// </summary>
        /// <param name="content">String to be shown.</param>
        /// <param name="width">Width of label.</param>
        /// <returns>Label object.</returns>
        private Label PrepareLabel(string content, double width)
        {
            Label preReqLabel = new Label();
            preReqLabel.Width = width;
            preReqLabel.Content = content;
            return preReqLabel;
        }

        /// <summary>
        /// Returns new textblock object to be added to UI.
        /// </summary>
        /// <param name="content">String to be shown.</param>
        /// <param name="width">Width of label.</param>
        /// <returns>TextBlock object.</returns>
        private TextBlock PrepareTextBlock(string content, double width)
        {
            TextBlock preReqTextBlock = new TextBlock();
            preReqTextBlock.Margin = new Thickness(0, 5, 0, 0);
            preReqTextBlock.Width = width;
            preReqTextBlock.Text = content;
            preReqTextBlock.TextWrapping = TextWrapping.Wrap;
            return preReqTextBlock;
        }

        /// <summary>
        /// Image to be added to UI.
        /// </summary>
        /// <param name="passed">State based on which Image will be shown.</param>
        /// <param name="width">Width of image.</param>
        /// <returns>Image object.</returns>
        private Image PrepareImage(bool? passed, int width)
        {
            Image preReqImage = new Image();
            BitmapImage imageSrc = new BitmapImage();
            imageSrc.BeginInit();
            imageSrc.UriSource = new Uri("pack://application:,,,/DRSetupFramework;component/Images/warning.png");
            imageSrc.EndInit();
            preReqImage.Source = imageSrc;
            preReqImage.Width = width;
            preReqImage.Height = width;
            preReqImage.Visibility = Visibility.Hidden;
            return preReqImage;
        }
    }

    /// <summary>
    /// Event generation controller for Prereqs
    /// </summary>
    [System.Diagnostics.CodeAnalysis.SuppressMessage(
        "Microsoft.StyleCop.CSharp.MaintainabilityRules",
        "SA1402:FileMayOnlyContainASingleClass",
        Justification = "All prereq helpers in one class")]
    public class PrerequisiteEventGenerator : EventArgs
    {
        /// <summary>
        /// Delegate for raising event when all PreReqs are checked.
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Event generator object.</param>
        public delegate void PreReqEventHandler(object sender, PrerequisiteEventGenerator e);
    }
}
