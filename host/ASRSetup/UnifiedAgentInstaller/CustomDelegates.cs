//---------------------------------------------------------------
//  <copyright file="CustomDelegates.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Logic to transition between pages.
//  </summary>
//
//  History:     04-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using ASRSetupFramework;

namespace UnifiedAgentInstaller
{
    public class CustomDelegates
    {
        #region Default Navigation Logic

        /// <summary>
        /// Null page handler.
        /// </summary>
        /// <param name="currentPage">The current page.</param>
        /// <returns>always null</returns>
        [System.Diagnostics.CodeAnalysis.SuppressMessage(
            "Microsoft.Usage", 
            "CA1801:ReviewUnusedParameters", 
            MessageId = "currentPage", 
            Justification = "Must match expected function parameters")]
        public static Page NullPageHandler(Page currentPage)
        {
            return null;
        }

        /// <summary>
        /// Returns the default next page.
        /// </summary>
        /// <param name="currentPage">The current page.</param>
        /// <returns>Page to move to</returns>
        public static Page DefaultNextPage(Page currentPage)
        {
            return DefaultPage(currentPage.NextPageArgument);
        }

        /// <summary>
        /// Returns the default previous page.
        /// </summary>
        /// <param name="currentPage">The current page.</param>
        /// <returns>Page to move to</returns>
        public static Page DefaultPreviousPage(Page currentPage)
        {
            return DefaultPage(currentPage.PreviousPageArgument);
        }
        #endregion Default Navigation Logic

        #region Private Helpers
        /// <summary>
        /// Returns the page in the registry indexed by the provided string or null.
        /// </summary>
        /// <param name="pageId">The page id.</param>
        /// <returns>
        /// the page in the registry indexed by the provided string.
        /// </returns>
        private static Page DefaultPage(string pageId)
        {
            return
                string.IsNullOrEmpty(pageId) ?
                    null : PageRegistry.Instance.GetPage(pageId);
        }

        #endregion // Private Helpers
    }
}
