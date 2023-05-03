//-----------------------------------------------------------------------
// <copyright file="Factory.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary> Factory class for Pages
// </summary>
//-----------------------------------------------------------------------
namespace ASRSetupFramework
{
    using System;

    /// <summary>
    /// Abstract class for creating host and UI pages
    /// </summary>
    public abstract class Factory
    {
        /// <summary>
        /// Creates a window that will host pages 
        /// </summary>
        /// <returns>a IPageHost compliant window</returns>
        public abstract IPageHost CreateHost();

        /// <summary>
        /// Creates the page.
        /// </summary>
        /// <param name="page">The page object for UI creation.</param>
        /// <param name="type">The Type object for UI creation.</param>
        /// <returns>an IPageUI compliant wizard page</returns>
        public abstract IPageUI CreatePage(Page page, Type type);
    }
}
