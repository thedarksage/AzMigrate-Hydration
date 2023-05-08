namespace ASRSetupFramework
{
    using System;
    using System.Collections.Generic;
    using System.Globalization;
    using System.Windows;
    using System.Windows.Controls;
    using System.Windows.Media;
    using System.Windows.Shapes;
    using ASRResources;
    using WpfResources;

    /// <summary>
    /// Interaction logic for the window hosting the wizard's pages
    /// </summary>
    public partial class WpfFormsHostPage : Window, IPageHost
    {
        #region Private Members
        /// <summary>
        /// Setup stages in sidebar.
        /// </summary>
        private Dictionary<string, SidebarNavigation> setupStages;
        #endregion

        #region Public Members
        /// <summary>
        /// Initializes a new instance of the <see cref="WpfFormsHostPage"/> class.
        /// </summary>
        public WpfFormsHostPage()
        {
            this.InitializeComponent();
            this.Window_1.Title = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.WizardTitle);
            this.setupPhaseProgressBar.Maximum = 5;
        }

        #region Button States Methods

        /// <summary>
        /// Sets the state of the previous button.
        /// </summary>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        public void SetPreviousButtonState(bool visibleState, bool enabledState)
        {
            this.SetButtonState(this.buttonPrevious, visibleState, enabledState);
        }

        /// <summary>
        /// Sets the state of the previous button.
        /// </summary>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        /// <param name="buttonText">The button text.</param>
        public void SetPreviousButtonState(bool visibleState, bool enabledState, string buttonText)
        {
            this.SetButtonState(this.buttonPrevious, visibleState, enabledState, buttonText);
        }

        /// <summary>
        /// Sets the state of the next button.
        /// </summary>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        public void SetNextButtonState(bool visibleState, bool enabledState)
        {
            this.SetButtonState(this.buttonNext, visibleState, enabledState);
        }

        /// <summary>
        /// Sets the state of the next button.
        /// </summary>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        /// <param name="buttonText">The button text.</param>
        public void SetNextButtonState(bool visibleState, bool enabledState, string buttonText)
        {
            this.SetButtonState(this.buttonNext, visibleState, enabledState, buttonText);
        }

        /// <summary>
        /// Sets the state of the cancel button.
        /// </summary>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        public void SetCancelButtonState(bool visibleState, bool enabledState)
        {
            this.SetButtonState(this.buttonCancel, visibleState, enabledState);
        }

        /// <summary>
        /// Sets the state of the cancel button.
        /// </summary>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        /// <param name="buttonText">The button text.</param>
        public void SetCancelButtonState(bool visibleState, bool enabledState, string buttonText)
        {
            this.SetButtonState(this.buttonCancel, visibleState, enabledState, buttonText);
        }

        #endregion Button States Methods

        #region Sidebar Navigation methods
        /// <summary>
        /// Sets the state of sidebar controls
        /// </summary>
        /// <param name="setupStages">Dictionary containing states</param>
        public void SetSetupStages(Dictionary<string, SidebarNavigation> setupStages)
        {
            int stageIndex = 6;
            foreach (KeyValuePair<string, SidebarNavigation> keyValuePair in setupStages)
            {
                Button stageButton = new Button();
                if (!string.IsNullOrEmpty(keyValuePair.Value.SetupStage))
                {
                    Border colorBorder = new Border();

                    stageButton.Content = RawResources.GetPageResource<string>(keyValuePair.Value.SetupStage);
                    stageButton.Background = new SolidColorBrush();
                    stageButton.HorizontalContentAlignment = System.Windows.HorizontalAlignment.Left;
                    stageButton.HorizontalAlignment = System.Windows.HorizontalAlignment.Stretch;
                    stageButton.BorderThickness = new Thickness(0, 0, 0, 0);
                    stageButton.Height = 30;

                    Ellipse stageEllipse = new Ellipse();
                    stageEllipse.Height = 10;
                    stageEllipse.Width = 10;

                    this.SideBarGrid.Children.Add(colorBorder);
                    this.SideBarGrid.Children.Add(stageButton);
                    this.SideBarGrid.Children.Add(stageEllipse);

                    Grid.SetColumn(stageButton, 1);
                    Grid.SetColumn(stageEllipse, 0);
                    Grid.SetColumn(colorBorder, 0);
                    Grid.SetColumnSpan(colorBorder, 2);
                    Grid.SetRow(stageButton, stageIndex);
                    Grid.SetRow(stageEllipse, stageIndex);
                    Grid.SetRow(colorBorder, stageIndex);

                    keyValuePair.Value.SetUIElements(stageButton, stageEllipse, colorBorder);
                    stageIndex++;
                }
            }

            this.setupStages = setupStages;
        }

        /// <summary>
        /// Enable sidebar navigation for this page.
        /// </summary>
        /// <param name="setupStage">Stage to enable navigation for.</param>
        public void EnableNavigation(string setupStage)
        {
            Trc.Log(LogLevel.Debug, "Enabling clickable navigation for {0}", setupStage);
            if (!string.IsNullOrEmpty(setupStage))
            {
                this.setupStages[setupStage].EnableSideBarNavigation();
            }
        }

        /// <summary>
        /// Hide/Disable Side bar stage.
        /// </summary>
        /// <param name="setupStage">Stage to enable navigation for.</param>
        public void HideSidebarStage(string stage)
        {
            if (!string.IsNullOrEmpty(stage))
            {
                this.setupStages[stage].HideSidebarStage(stage);
            }
        }

        /// <summary>
        /// Show/Enable Side bar stage.
        /// </summary>
        public void ShowSidebarStage(string stage)
        {
            if (!string.IsNullOrEmpty(stage))
            {
                this.setupStages[stage].ShowSidebarStage(stage);
            }
        }

        /// <summary>
        /// Disables sidebar navigation for this page.
        /// </summary>
        /// <param name="setupStage">Stage to disable navigation for.</param>
        public void DisableNavigation(string setupStage)
        {
            Trc.Log(LogLevel.Debug, "Disabling clickable navigation for {0}", setupStage);
            if (!string.IsNullOrEmpty(setupStage))
            {
                this.setupStages[setupStage].DisableSideBarNavigation();
            }
        }

        /// <summary>
        /// Marks current page is active in navigation bar.
        /// </summary>
        /// <param name="setupStage">Stage to mark navigation active for.</param>
        public void ActivateSetupStage(string setupStage)
        {
            Trc.Log(LogLevel.Debug, "Active navigation for {0}", setupStage);
            if (!string.IsNullOrEmpty(setupStage))
            {
                this.setupStages[setupStage].ActiveSideBarNavigation();
            }
        }

        /// <summary>
        /// Marks current page as inactive on navigation bar.
        /// </summary>
        /// <param name="setupStage">Stage to mark navigation inactive for.</param>
        public void DeactivateSetupStage(string setupStage)
        {
            Trc.Log(LogLevel.Debug, "Inactive navigation for {0} ", setupStage);
            if (!string.IsNullOrEmpty(setupStage))
            {
                this.setupStages[setupStage].InactiveSideBarNavigation();
            }
        }

        /// <summary>
        /// Disable navigation sidebar.
        /// </summary>
        public void DisableGlobalSideBarNavigation()
        {
            SidebarNavigation.DisableGlobalSideBarNavigation();
        }

        /// <summary>
        /// Enable navigation sidebar.
        /// </summary>
        public void EnableGlobalSideBarNavigation()
        {
            SidebarNavigation.EnableGlobalNavigation();
        }

        /// <summary>
        /// Marks current page as operation completed.
        /// </summary>
        /// <param name="setupStage">Stage to mark operation status for.</param>
        public void MarkStageAsCompleted(string setupStage)
        {
            this.setupStages[setupStage].MarkStageAsCompleted();
        }

        #endregion Sidebar Navigation methods

        #endregion Public Members

        #region IPageHost Members

        /// <summary>
        /// Sets the page.
        /// </summary>
        /// <param name="page">The page to be set.</param>
        public void SetPage(IPageUI page)
        {
            Control control = page as Control;
            if (control != null)
            {
                this.border.Child = control;
                this.border.Background = null;
                control.Background = null;
                this.pageLabel.Text = page.Title;
                this.setupPhaseProgressBar.Value = page.ProgressPhase;
            }
        }



        #endregion

        #region Protected Members

        /// <summary>
        /// Raises the <see cref="E:System.Windows.Window.Closing"/> event.
        /// </summary>
        /// <param name="e">A <see cref="T:System.ComponentModel.CancelEventArgs"/> that contains the event data.</param>
        protected override void OnClosing(System.ComponentModel.CancelEventArgs e)
        {
            // By default, OnClosing will have cancel event as false
            if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.CanClose))
            {
                bool canClose = PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.CanClose);

                if (!canClose)
                {
                    // Make sure the user wants to cancel
                    if (VerifyUserClose())
                    {
                        // User choose to cancel
                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.InstallationAborted, true);
                        Trc.Log(LogLevel.Info, "The user canceled setup");
                    }
                    else
                    {
                        // Cancel the event as user not opted to close.
                        e.Cancel = true;
                    }
                }
            }
            else
            {
                // Make sure we are have a next page... if we don't
                // we don't need to warn about closing as that is what should happen
                if (PageNavigation.Instance.HasNextPage)
                {
                    // Cast because CanClose is not a member of IPageUI
                    BasePageForWpfControls currentPage =
                        PageNavigation.Instance.CurrentPage.PageUI as BasePageForWpfControls;
                    if (currentPage.CanClose())
                    {
                        // Make sure the user wants to cancel
                        if (VerifyUserClose())
                        {
                            // User choose to cancel
                            Trc.Log(LogLevel.Info, "The user canceled setup");
                        }
                        else
                        {
                            // Cancel the event as user not opted to close.
                            e.Cancel = true;
                        }
                    }
                    else
                    {
                        // Cancel the event as current page not allowed to close.
                        e.Cancel = true;
                    }
                }
            }

            // If cancel event not cancelled, then update installation aborted to true.
            if (e.Cancel == false)
            {
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.InstallationAborted, true);
            }

            base.OnClosing(e);
        }

        #endregion

        #region Statics

        /// <summary>
        /// Verifies the user close.
        /// </summary>
        /// <returns>true if the user selects the yes button, false otherwise.</returns>
        private static bool VerifyUserClose()
        {
            return
                MessageBoxResult.Yes == System.Windows.MessageBox.Show(
                    StringResources.CancelText,
                    StringResources.SetupMessageBoxTitle,
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Question);
        }

        #endregion  Statics

        #region Private Members

        #region Button State Private Helper Methods

        /// <summary>
        /// Sets the state of the given button.
        /// </summary>
        /// <param name="button">the button to set.</param>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        private void SetButtonState(Button button, bool visibleState, bool enabledState)
        {
            if (visibleState)
            {
                button.Visibility = Visibility.Visible;
            }
            else
            {
                button.Visibility = Visibility.Hidden;
            }

            button.IsEnabled = enabledState;
        }

        /// <summary>
        /// Sets the state of the given button.
        /// </summary>
        /// <param name="button">the button to set.</param>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        /// <param name="buttonText">The button text.</param>
        private void SetButtonState(Button button, bool visibleState, bool enabledState, string buttonText)
        {
            this.SetButtonState(button, visibleState, enabledState);
            if (!string.IsNullOrEmpty(buttonText))
            {
                button.Content = buttonText;
            }
        }

        #endregion // Button State Private Helper Methods

        /// <summary>
        /// Handles the Click event of the buttonPrevious control.
        /// </summary>
        /// <param name="sender">The source of the event.</param>
        /// <param name="e">The <see cref="System.Windows.RoutedEventArgs"/> instance containing the event data.</param>
        private void ButtonPreviousClick(object sender, RoutedEventArgs e)
        {
            // Cast because OnPrevious is not a member of IPageUI.
            BasePageForWpfControls currentPage = PageNavigation.Instance.CurrentPage.PageUI as BasePageForWpfControls;
            if (currentPage.OnPrevious())
            {
                PageNavigation.Instance.MoveToPreviousPage(false);
            }
        }

        /// <summary>
        /// Handles the Click event of the buttonNext control.
        /// </summary>
        /// <param name="sender">The source of the event.</param>
        /// <param name="e">The <see cref="System.Windows.RoutedEventArgs"/> instance containing the event data.</param>
        private void ButtonNextClick(object sender, RoutedEventArgs e)
        {
            // Cast because OnNext is not a member of IPageUI.
            BasePageForWpfControls currentPage = PageNavigation.Instance.CurrentPage.PageUI as BasePageForWpfControls;
            if (currentPage.OnNext())
            {
                PageNavigation.Instance.MoveToNextPage();
            }
        }

        /// <summary>
        /// Handles the Click event of the buttonCancel control.
        /// </summary>
        /// <param name="sender">The source of the event.</param>
        /// <param name="e">The <see cref="System.Windows.RoutedEventArgs"/> instance containing the event data.</param>
        private void ButtonCancelClick(object sender, RoutedEventArgs e)
        {
            // Cast because OnCancel is not a member of IPageUI.
            BasePageForWpfControls currentPage = PageNavigation.Instance.CurrentPage.PageUI as BasePageForWpfControls;
            if (currentPage.OnCancel())
            {
                this.Close();
            }
        }

        #endregion // Private
    }
}
