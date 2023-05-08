//-----------------------------------------------------------------------
// <copyright file="IPageHost.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary> Host interface for the pages.
// </summary>
//-----------------------------------------------------------------------
namespace ASRSetupFramework
{
    using System;
    using System.Collections.Generic;
    using System.Windows;

    /// <summary>
    /// The Interface host pages must implement.
    /// </summary>
    public interface IPageHost
    {
        /// <summary>
        /// The Host Page must handle the Closed Event 
        /// </summary>
        event EventHandler Closed;

        /// <summary>
        /// Sets the page.
        /// </summary>
        /// <param name="page">The UI page.</param>
        void SetPage(IPageUI page);

        /// <summary>
        /// Shows this instance.
        /// </summary>
        void Show();

        /// <summary>
        /// Closes this instance.
        /// </summary>
        void Close();


        /// <summary>
        /// Sets the state of the previous button.
        /// </summary>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        void SetPreviousButtonState(bool visibleState, bool enabledState);

        /// <summary>
        /// Sets the state of the previous button.
        /// </summary>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        /// <param name="buttonText">The button text.</param>
        void SetPreviousButtonState(bool visibleState, bool enabledState, string buttonText);

        /// <summary>
        /// Sets the state of the next button.
        /// </summary>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        void SetNextButtonState(bool visibleState, bool enabledState);

        /// <summary>
        /// Sets the state of the next button.
        /// </summary>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        /// <param name="buttonText">The button text.</param>
        void SetNextButtonState(bool visibleState, bool enabledState, string buttonText);

        /// <summary>
        /// Sets the state of the cancel button.
        /// </summary>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        void SetCancelButtonState(bool visibleState, bool enabledState);

        /// <summary>
        /// Sets the state of the cancel button.
        /// </summary>
        /// <param name="visibleState">if set to <c>true</c> [visible state].</param>
        /// <param name="enabledState">if set to <c>true</c> [enabled state].</param>
        /// <param name="buttonText">The button text.</param>
        void SetCancelButtonState(bool visibleState, bool enabledState, string buttonText);

        /// <summary>
        /// Sets the state of sidebar controls
        /// </summary>
        /// <param name="setupStages">Dictionary containing states</param>
        void SetSetupStages(Dictionary<string, SidebarNavigation> setupStages);

        /// <summary>
        /// Enable sidebar navigation for this page.
        /// </summary>
        /// <param name="setupStage">Stage to enable navigation for.</param>
        void EnableNavigation(string setupStage);

        /// <summary>
        /// Disables sidebar navigation for this page.
        /// </summary>
        /// <param name="setupStage">Stage to disable navigation for.</param>
        void DisableNavigation(string setupStage);

        /// <summary>
        /// Marks current page as active on navigation bar.
        /// </summary>
        /// <param name="setupStage">Stage to mark navigation active for.</param>
        void ActivateSetupStage(string setupStage);

        /// <summary>
        /// Marks current page as inactive on navigation bar.
        /// </summary>
        /// <param name="setupStage">Stage to mark navigation inactive for.</param>
        void DeactivateSetupStage(string setupStage);

        /// <summary>
        /// Disable navigation sidebar.
        /// </summary>
        void DisableGlobalSideBarNavigation();

        /// <summary>
        /// Enable navigation sidebar.
        /// </summary>
        void EnableGlobalSideBarNavigation();

        /// <summary>
        /// Marks current page as operation completed.
        /// </summary>
        /// <param name="setupStage">Stage to mark operation status for.</param>
        void MarkStageAsCompleted(string setupStage);

        /// <summary>
        /// Disable/Hide the Side bar stage.
        /// </summary>
        /// <param name="setupStage">Stage to disable in the Side bar.</param>
        void HideSidebarStage(string setupStage);

        /// <summary>
        /// Enable/Shows the Side bar stage.
        /// </summary>
        /// <param name="setupStage">Stage to disable in the Side bar.</param>
        void ShowSidebarStage(string setupStage);
    }
}
