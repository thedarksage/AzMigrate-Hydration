//---------------------------------------------------------------
//  <copyright file="CustomDelegates.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Logic for transitioning between pages.
//  </summary>
//
//  History:     05-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using ASRSetupFramework;

namespace UnifiedAgentConfigurator
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

        #region Custom Navigation
        /// <summary>
        /// Returns the default next page from proxy page.
        /// </summary>
        /// <param name="currentPage">The current page.</param>
        /// <returns>Page to move to</returns>
        public static Page NextFromProxyPage(Page currentPage)
        {
            string[] nextPageArgument = currentPage.NextPageArgument.Split(',');

            Debug.Assert(
                nextPageArgument.Length == 2,
                "Update Custom delegate for jumping from Proxy page..");

            Platform platform = PropertyBagDictionary.Instance.GetProperty<Platform>(
                PropertyBagConstants.Platform);
            string csType = SetupHelper.GetCSTypeFromDrScout();

            if ((platform == Platform.VmWare) && (csType == ConfigurationServerType.CSLegacy.ToString()))
            {
                PageRegistry.Instance.GetPage(nextPageArgument[1]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[1]).SetupStage);
                PageRegistry.Instance.GetPage(nextPageArgument[2]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[2]).SetupStage);
                return PageRegistry.Instance.GetPage(nextPageArgument[0]);
            }
            else if ((platform == Platform.VmWare) && (csType == ConfigurationServerType.CSPrime.ToString()))
            {
                PageRegistry.Instance.GetPage(nextPageArgument[0]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[0]).SetupStage);
                PageRegistry.Instance.GetPage(nextPageArgument[1]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[1]).SetupStage);
                return PageRegistry.Instance.GetPage(nextPageArgument[2]);
            }
            else
            {
                PageRegistry.Instance.GetPage(nextPageArgument[0]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[0]).SetupStage);
                PageRegistry.Instance.GetPage(nextPageArgument[2]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[2]).SetupStage);
                return PageRegistry.Instance.GetPage(nextPageArgument[1]);
            }
        }

        /// <summary>
        /// Returns the default next page from Launch page
        /// </summary>
        /// <param name="currentPage">The current page.</param>
        /// <returns>Page to move to</returns>
        public static Page NextFromLaunchPage(Page currentPage)
        {
            string[] nextPageArgument = currentPage.NextPageArgument.Split(',');

            Debug.Assert(
                nextPageArgument.Length == 3,
                "Update Custom delegate for jumping to Proxy page..");

            Platform platform = PropertyBagDictionary.Instance.GetProperty<Platform>(
                PropertyBagConstants.Platform);
            string csType = SetupHelper.GetCSTypeFromDrScout();

            if ((platform == Platform.VmWare) && (csType == ConfigurationServerType.CSLegacy.ToString()))
            {
                PageRegistry.Instance.GetPage(nextPageArgument[1]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[1]).SetupStage);
                PageRegistry.Instance.GetPage(nextPageArgument[2]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[2]).SetupStage);
                PageRegistry.Instance.GetPage(nextPageArgument[3]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[3]).SetupStage);

                return PageRegistry.Instance.GetPage(nextPageArgument[0]);
            }
            else if ((platform == Platform.VmWare) && (csType == ConfigurationServerType.CSPrime.ToString()))
            {
                PageRegistry.Instance.GetPage(nextPageArgument[0]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[0]).SetupStage);
                PageRegistry.Instance.GetPage(nextPageArgument[1]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[1]).SetupStage);
                PageRegistry.Instance.GetPage(nextPageArgument[2]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[2]).SetupStage);
                return PageRegistry.Instance.GetPage(nextPageArgument[3]);
            }
            else
            {
                PageRegistry.Instance.GetPage(nextPageArgument[0]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[0]).SetupStage);
                PageRegistry.Instance.GetPage(nextPageArgument[3]).Host.HideSidebarStage(
                    PageRegistry.Instance.GetPage(nextPageArgument[3]).SetupStage);

                return PageRegistry.Instance.GetPage(nextPageArgument[2]);
            }
        }
        #endregion

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
