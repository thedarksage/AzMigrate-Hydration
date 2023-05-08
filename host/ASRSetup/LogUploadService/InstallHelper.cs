//---------------------------------------------------------------
//  <copyright file="InstallHelper.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Helps with Service installation/uninstallation.
//  </summary>
//
//  History:     26-Sep-2017   bhayachi     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Configuration.Install;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using ASRSetupFramework;

namespace Microsoft.ASRSetup.LogUploadService
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
            Trc.Log(LogLevel.Always, "Installing the service");

            ManagedInstallerClass.InstallHelper(
                new string[] { exePath });
        }

        /// <summary>
        /// Uninstall the service.
        /// </summary>
        internal static void UninstallService()
        {
            Trc.Log(LogLevel.Always, "Uninstalling the service");

            ManagedInstallerClass.InstallHelper(
                new string[] { "/u", exePath });
        }

        #endregion Methods
    }
}
