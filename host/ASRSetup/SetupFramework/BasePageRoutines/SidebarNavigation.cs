//---------------------------------------------------------------
//  <copyright file="SidebarNavigation.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Sidebar handling for DR Setup and Registration
//  </summary>
//
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Shapes;

namespace ASRSetupFramework
{
    /// <summary>
    /// Class to control sidebar navigation.
    /// </summary>
    public class SidebarNavigation
    {
        #region Private members

        /// <summary>
        /// If sidebar navigation is allowed or not. If false no click event of sidebar will work.
        /// </summary>
        private static bool globalNavigationEnabled = true;
        #endregion

        #region Constructors
        /// <summary>
        /// Initializes a new instance of the SidebarNavigation class.
        /// </summary>
        /// <param name="setupStage">String associated with the stage.</param>
        /// <param name="navigationButton">Button associated with the stage.</param>
        /// <param name="progressEllipse">Ellipse associated with the stage.</param>
        /// <param name="isEnabled">If click event enabled for Navigation button.</param>
        /// <param name="pageName">Associated page to jump to on click.</param>
        public SidebarNavigation(string setupStage, Button navigationButton, Ellipse progressEllipse, bool isEnabled, string pageName)
        {
            this.SetupStage = setupStage;
            this.NavigationButton = navigationButton;
            this.ProgressEllipse = progressEllipse;
            this.IsEnabled = isEnabled;
            this.PageName = pageName;

            this.NavigationButton.IsEnabled = this.IsEnabled;
            this.UpdateUIElementState();
        }

        /// <summary>
        /// Initializes a new instance of the SidebarNavigation class.
        /// </summary>
        /// <param name="setupStage">string associated with the stage.</param>
        /// <param name="pageName">Associated page to jump to on click.</param>
        public SidebarNavigation(string setupStage, string pageName)
        {
            this.SetupStage = setupStage;
            this.PageName = pageName;
        }
        #endregion

        #region Properties
        /// <summary>
        /// Gets setup stage string associated with the stage.
        /// </summary>
        public string SetupStage { get; private set; }
        
        /// <summary>
        /// Gets navigation button associated with the stage.
        /// </summary>
        public Button NavigationButton { get; private set; }

        /// <summary>
        /// Gets progress ellipse associated with the stage.
        /// </summary>
        public Ellipse ProgressEllipse { get; private set; }

        /// <summary>
        /// Gets the border associated with navigation sidebar.
        /// </summary>
        public Border ContainerBorder { get; private set; }

        /// <summary>
        /// Gets a value indicating whether click event is enabled for Navigation button.
        /// </summary>
        public bool IsEnabled { get; private set; }

        /// <summary>
        /// Gets the page to navigate to.
        /// </summary>
        public string PageName { get; private set; }
        #endregion

        /// <summary>
        /// Disable navigation using sidebar.
        /// </summary>
        public static void DisableGlobalSideBarNavigation()
        {
            globalNavigationEnabled = false;
        }

        /// <summary>
        /// Enable navigation using sidebar.
        /// </summary>
        public static void EnableGlobalNavigation()
        {
            globalNavigationEnabled = true;
        }

        /// <summary>
        /// Associated UI elements with this SideBarNavigation object.
        /// </summary>
        /// <param name="navigationButton">Navigation button.</param>
        /// <param name="progressEllipse">Progress ellipse.</param>
        /// <param name="containerBorder">Container broder for navigation.</param>
        public void SetUIElements(Button navigationButton, Ellipse progressEllipse, Border containerBorder)
        {
            this.NavigationButton = navigationButton;
            this.ProgressEllipse = progressEllipse;
            this.ContainerBorder = containerBorder;

            this.UpdateUIElementState();
            this.ProgressEllipse.Fill = Brushes.Blue;
        }

        /// <summary>
        /// Enable clicking of this sidebar control.
        /// </summary>
        public void EnableSideBarNavigation()
        {
            this.IsEnabled = true;
            this.NavigationButton.Background = new SolidColorBrush();
            this.UpdateUIElementState();
        }

        /// <summary>
        /// Disable clicking of this sidebar control.
        /// </summary>
        public void DisableSideBarNavigation()
        {
            this.IsEnabled = false;
            this.UpdateUIElementState();
        }

        /// <summary>
        /// Mark current active stage on Sidebar.
        /// </summary>
        public void ActiveSideBarNavigation()
        {
            this.NavigationButton.Background = new SolidColorBrush(Colors.DeepSkyBlue);
            this.ContainerBorder.Background = new SolidColorBrush(Colors.DeepSkyBlue);
        }

        /// <summary>
        /// Mark current stage as inactive on Sidebar.
        /// </summary>
        public void InactiveSideBarNavigation()
        {
            this.NavigationButton.Background = new SolidColorBrush();
            this.ContainerBorder.Background = new SolidColorBrush();
        }

        /// <summary>
        /// Mark current stage as operation completed. This can be used if you want the
        /// stage as compeleted without changing page.
        /// </summary>
        public void MarkStageAsCompleted()
        {
            this.ProgressEllipse.Fill = Brushes.Green;
        }

        /// <summary>
        /// Hide the sidebar stage elements.
        /// </summary>
        public void HideSidebarStage(string stage)
        {
            this.SetupStage = stage;

            this.NavigationButton.Visibility = System.Windows.Visibility.Collapsed;
            this.ProgressEllipse.Visibility = System.Windows.Visibility.Collapsed;
            this.ContainerBorder.Visibility = System.Windows.Visibility.Collapsed;
        }

        /// <summary>
        /// Show the sidebar stage elements.
        /// </summary>>
        public void ShowSidebarStage(string stage)
        {
            this.SetupStage = stage;

            this.NavigationButton.Visibility = System.Windows.Visibility.Visible;
            this.ProgressEllipse.Visibility = System.Windows.Visibility.Visible;
            this.ContainerBorder.Visibility = System.Windows.Visibility.Visible;
        }

        /// <summary>
        /// Set element states based on IsEnabled state.
        /// </summary>
        private void UpdateUIElementState()
        {
            if (!string.IsNullOrEmpty(this.SetupStage))
            {
                if (this.IsEnabled)
                {
                    this.ProgressEllipse.Fill = Brushes.Green;
                    this.NavigationButton.Click += this.JumpToPage;
                }
                else
                {
                    try
                    {
                        this.NavigationButton.Click -= this.JumpToPage;
                    }
                    catch 
                    {
                        // If no event handler is associated just consume exception.
                    }
                }
            }
        }

        /// <summary>
        /// Click event for button.
        /// </summary>
        /// <param name="sender">Sender of event.</param>
        /// <param name="e">Event arguments.</param>
        private void JumpToPage(object sender, RoutedEventArgs e)
        {
            if (globalNavigationEnabled)
            {
                PageNavigation.Instance.MoveToPage(this.PageName, true);
            }

            e.Handled = true;
        }
    }
}
