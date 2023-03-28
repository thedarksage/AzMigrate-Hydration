//---------------------------------------------------------------
//  <copyright file="ConfigureActionProcessor.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Routines specific to configuration.
//  </summary>
//
//  History:     05-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using ASRSetupFramework;
using Microsoft.Win32;

namespace UnifiedAgentConfigurator
{
    /// <summary>
    /// Routines specific to configuration.
    /// </summary>
    public class ConfigureActionProcessor
    {
        /// <summary>
        /// Boot Agent services.
        /// </summary>
        public static bool BootServices()
        {
            Trc.Log(
               LogLevel.Always,
               "Starting Agent services.");

            bool isAgentConfigured = SetupHelper.IsAgentConfigured();
            RegistryKey hklm = Registry.LocalMachine;

            if (SetupHelper.GetLastAgentSetupAction() == SetupAction.Install
                && !isAgentConfigured)
            {
                Trc.Log(
                   LogLevel.Always,
                   "Starting Agent services with Fresh install flag as True.");
                return SetupHelper.StartUAServices(true);
            }
            else
            {
                Trc.Log(
                   LogLevel.Always,
                   "Starting Agent services with Fresh install flag as False.");
                return SetupHelper.StartUAServices(false);
            }
        }

        /// <summary>
        /// Returns a value indicating is a reboot is needed.
        /// </summary>
        /// <returns>True if reboot is required, false otherwise.</returns>
        public static bool RebootRequired()
        {
            SetupAction lastInstallAction = SetupHelper.GetLastAgentSetupAction();
            bool isFreshInstall = false;
            Trc.Log(
                LogLevel.Always,
                "Last installation action value is - " + lastInstallAction);
            if (lastInstallAction == SetupAction.Install)
            {
                isFreshInstall = true;
            }

            string uaInstallDirectory = Registry.LocalMachine.OpenSubKey(
                UnifiedSetupConstants.InMageVXRegistryPath)
                .GetValue(UnifiedSetupConstants.InMageInstallPathReg, null).ToString();

            IniFile drScoutConf = new IniFile(Path.Combine(
                SetupHelper.GetAgentInstalledLocation(),
                UnifiedSetupConstants.DrScoutConfigRelativePath));

            if (SetupHelper.IsRebootRequiredPostDiskFilterInstall() &&
                drScoutConf.ReadValue("vxagent", "Role")
                    .Equals("Agent", StringComparison.InvariantCultureIgnoreCase) && 
                isFreshInstall)
            {
                Trc.Log(LogLevel.Always, "Reboot is required for Agent.");
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagDictionary.RebootRequired, true);
                return true;
            }
            else
            {
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagDictionary.RebootRequired, false);
                Trc.Log(LogLevel.Always, "Reboot is NOT required for Agent.");
                return false;
            }
        }

        /// <summary>
        /// Fetch the hostID from DRScout config and 
        /// validates it according to VmWare/Azure platform requirement.
        /// </summary>
        /// <out>hostId</out>
        public static void GetHostId(out string hostId)
        {
            Trc.Log(LogLevel.Always, "Begin GetHostId");
            bool generateHostid = true;
            string drScoutHostId = SetupHelper.FetchHostIDFromDrScout();
            
            // Generate new hostId, when hostId value in drscout.conf is null.
            if (!string.IsNullOrEmpty(drScoutHostId))
            {
                generateHostid = false;
            }

            if (generateHostid)
            {
                Trc.Log(LogLevel.Always, "Generating new hostId.");
                hostId = Guid.NewGuid().ToString();
            }
            else
            {
                Trc.Log(LogLevel.Always, "Using DRScout config hostId.");
                hostId = drScoutHostId;
            }

            Trc.Log(LogLevel.Always, "hostId value: {0}", hostId);
        }
    }
}
