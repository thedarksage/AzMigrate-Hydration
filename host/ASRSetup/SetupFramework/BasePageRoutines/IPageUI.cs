//-----------------------------------------------------------------------
// <copyright file="IPageUI.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary> The UI interface of a page 
// </summary>
//-----------------------------------------------------------------------
namespace ASRSetupFramework
{
    /// <summary>
    /// The interface implementation classes associated to page classes belonging to the 
    /// ASRSetupFramework must comply with. 
    /// </summary>
    public interface IPageUI
    {
        /// <summary>
        /// Gets a value indicating whether this instance is initialized.
        /// </summary>
        /// <value>
        ///     <c>true</c> if this instance is initialized; otherwise, <c>false</c>.
        /// </value>
        bool IsInitialized { get; }

        /// <summary>
        /// Gets the title.
        /// </summary>
        /// <value>The title.</value>
        string Title { get; }

        /// <summary>
        /// Gets the progress phase the page belong to
        /// </summary>
        double ProgressPhase { get; }

        /// <summary>
        /// Validates the page.
        /// </summary>
        /// <param name="navType">Navigation type</param>
        /// <param name="pageId">Page name being called next.</param>
        /// <returns>true if valid</returns>
        bool ValidatePage(PageNavigation.Navigation navType, string pageId = null);

        /// <summary>
        /// Initializes the page.
        /// </summary>
        void InitializePage();

        /// <summary>
        /// Enters the page.
        /// </summary>
        void EnterPage();

        /// <summary>
        /// Exits the page.
        /// </summary>
        void ExitPage();
    }
}
