using System;
using System.Collections.Generic;
using System.Configuration.Install;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace ProcessServerMonitor
{
    /// <summary>
    /// Performs service installation/uninstallation.
    /// </summary>
    internal static class InstallHelper
    {
        # region Fields

        /// <summary>
        /// Execution assemble exe path.
        /// </summary>
        private static readonly string exePath =
            Assembly.GetExecutingAssembly().Location;

        #endregion Fields

        # region Methods

        /// <summary>
        /// Install the service.
        /// </summary>
        internal static void InstallService()
        {
            ManagedInstallerClass.InstallHelper(
                new string[] { exePath });
        }

        /// <summary>
        /// Uninstall the service.
        /// </summary>
        internal static void UninstallService()
        {
            ManagedInstallerClass.InstallHelper(
                new string[] { "/u", exePath });
        }

        #endregion Methods
    }
}