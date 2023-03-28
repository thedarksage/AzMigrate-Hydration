namespace UnifiedSetup
{
    using System.Diagnostics;
    using ASRSetupFramework;

    /// <summary>
    /// Custom Page Navigation Delegates
    /// </summary>
    public static class CustomDelegates
    {
        #region Default Navigation Logic

        /// <summary>
        /// Null page handler.
        /// </summary>
        /// <param name="currentPage">The current page.</param>
        /// <returns>always null</returns>
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA1801:ReviewUnusedParameters", MessageId = "currentPage", Justification = "Must match expected function parameters")]
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

        #region Custom Delegates
        /// <summary>
        /// Navigation logic to Next page for Configuration or Process Server flow.
        /// </summary>
        /// <param name="currentPage">Current Page</param>
        /// <returns>Next page to navigate to.</returns>
        public static Page CSorPSNextPage(Page currentPage)
        {
            string[] nextPageArgument = currentPage.NextPageArgument.Split(',');

            Debug.Assert(
                nextPageArgument.Length == 2,
                "Update Custom delegate for InstallationChoiceNextPage.");

            string inst_Type = SetupParameters.Instance().ServerMode;

            if (inst_Type == UnifiedSetupConstants.CSServerMode)
            {
                return PageRegistry.Instance.GetPage(nextPageArgument[0]);
            }
            else
            {
                return PageRegistry.Instance.GetPage(nextPageArgument[1]);
            }
        }

        /// <summary>
        /// Navigation logic to Previous page for Configuration or Process Server flow.
        /// </summary>
        /// <param name="currentPage">Current Page</param>
        /// <returns>Next page to navigate to.</returns>
        public static Page CSorPSPreviousPage(Page currentPage)
        {
            string[] previousPageArgument = currentPage.PreviousPageArgument.Split(',');

            Debug.Assert(
                previousPageArgument.Length == 2,
                "Update Custom delegate for InstallationChoiceNextPage.");

            string inst_Type = SetupParameters.Instance().ServerMode;

            if (inst_Type == UnifiedSetupConstants.CSServerMode)
            {
                return PageRegistry.Instance.GetPage(previousPageArgument[0]);
            }
            else
            {
                return PageRegistry.Instance.GetPage(previousPageArgument[1]);
            }
        }

        /// <summary>
        /// Navigation logic to next page from LaunchPage based on the type of installation.
        /// </summary>
        /// <param name="currentPage">Current Page</param>
        /// <returns>Next page to navigate to.</returns>
        public static Page NextFromLaunchPage(Page currentPage)
        {
            string[] nextPageArgument = currentPage.NextPageArgument.Split(',');

            Debug.Assert(
                nextPageArgument.Length == 4,
                "Update Custom delegate for NextFromLaunchPage.");

            // Upgrade
            if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade)
            {
                if ((SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode) && (PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.MySQL55Installed)))
                {
                    return PageRegistry.Instance.GetPage(nextPageArgument[3]);
                }
                else
                {
                    return PageRegistry.Instance.GetPage(nextPageArgument[0]);
                }
            }
            // Fresh installation.
            else
            {
                if (SetupParameters.Instance().ReInstallationStatus == UnifiedSetupConstants.Yes)
                {
                    if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                    {
                        return PageRegistry.Instance.GetPage(nextPageArgument[1]);
                    }
                    else
                    {
                        return PageRegistry.Instance.GetPage(nextPageArgument[0]);
                    }
                }
                else
                {
                    return PageRegistry.Instance.GetPage(nextPageArgument[2]);
                } 
            }
        }

        /// <summary>
        /// Navigation logic to next page for Thirdparty Software flow.
        /// </summary>
        /// <param name="currentPage">Current Page</param>
        /// <returns>Next page to navigate to.</returns>
        public static Page ThirdpartySoftwareNextPage(Page currentPage)
        {
            string[] nextPageArgument = currentPage.NextPageArgument.Split(',');

            Debug.Assert(
                nextPageArgument.Length == 2,
                "Update Custom delegate for ThirdpartySoftwareNextPage.");

            string inst_Type = SetupParameters.Instance().ServerMode;

            if ((SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade) && 
                (inst_Type == UnifiedSetupConstants.CSServerMode) && 
                (PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.MySQL55Installed)))
            {
                return PageRegistry.Instance.GetPage(nextPageArgument[0]);
            }
            else
            {
                return PageRegistry.Instance.GetPage(nextPageArgument[1]);
            }
        }

        /// <summary>
        /// Navigation logic to next page from ProxyConfigurationPage based on the type of installation.
        /// </summary>
        /// <param name="currentPage">Current Page</param>
        /// <returns>Next page to navigate to.</returns>
        public static Page NextFromProxyConfigurationPage(Page currentPage)
        {
            string[] nextPageArgument = currentPage.NextPageArgument.Split(',');

            Debug.Assert(
                nextPageArgument.Length == 2,
                "Update Custom delegate for NextFromProxyConfigurationPage.");

            if (SetupParameters.Instance().ReInstallationStatus == UnifiedSetupConstants.Yes)
            {
                return PageRegistry.Instance.GetPage(nextPageArgument[0]);
            }
            else
            {
                return PageRegistry.Instance.GetPage(nextPageArgument[1]);
            } 
            
        }

        /// <summary>
        /// Navigation logic to previous page from ProxyConfigurationPage based on the type of installation.
        /// </summary>
        /// <param name="currentPage">Current Page</param>
        /// <returns>previous page to navigate to.</returns>
        public static Page PreviousFromProxyConfigurationPage(Page currentPage)
        {
            string[] previousPageArgument = currentPage.PreviousPageArgument.Split(',');

            Debug.Assert(
                previousPageArgument.Length == 2,
                "Update Custom delegate for PreviousFromProxyConfigurationPage.");

            string inst_Type = SetupParameters.Instance().ServerMode;

            if ((SetupParameters.Instance().ReInstallationStatus == UnifiedSetupConstants.Yes) ||
                (inst_Type == UnifiedSetupConstants.CSServerMode))
            {
                return PageRegistry.Instance.GetPage(previousPageArgument[0]);
            }
            else
            {
                return PageRegistry.Instance.GetPage(previousPageArgument[1]);
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
