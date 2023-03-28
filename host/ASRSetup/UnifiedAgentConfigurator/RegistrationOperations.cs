using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using ASRResources;
using ASRSetupFramework;
using Microsoft.Win32;

namespace UnifiedAgentConfigurator
{
    /// <summary>
    /// Routines common to both RCM and CS for registration.
    /// </summary>
    internal partial class RegistrationOperations
    {
        /// <summary>
        /// Configure proxy details.
        /// </summary>
        /// <returns>Operation result.</returns>
        internal static OperationResult ConfigureProxySettings()
        {
            try
            {
                Trc.Log(
                    LogLevel.Always,
                    "Updating proxy details.");

                string installationDir = SetupHelper.GetAgentInstalledLocation();
                string appData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
                string proxyInfoPath =
                    Path.Combine(appData, UnifiedSetupConstants.ProxyConfRelativePath);
                IniFile drScoutInfo =
                    new IniFile(Path.Combine(installationDir, UnifiedSetupConstants.DrScoutConfigRelativePath));
                IniFile proxyConf = new IniFile(proxyInfoPath);

                if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.ProxyAddress) &&
                    !string.IsNullOrEmpty(PropertyBagDictionary.Instance.GetProperty<string>(
                        PropertyBagConstants.ProxyAddress)))
                {
                    string proxyAddress = PropertyBagDictionary.Instance.GetProperty<string>(
                            PropertyBagConstants.ProxyAddress);
                    string proxyPort = PropertyBagDictionary.Instance.GetProperty<string>(
                            PropertyBagConstants.ProxyPort);
                    UInt16 intProxyPort = 0;
                    
                    if (Uri.IsWellFormedUriString(proxyAddress, UriKind.Absolute) &&
                        UInt16.TryParse(proxyPort, out intProxyPort))
                    {
                        proxyConf.WriteValue("proxy", "Address", proxyAddress);
                        proxyConf.WriteValue("proxy", "Port", proxyPort);
                    }
                    else
                    {
                        return new OperationResult
                        {
                            Error = StringResources.InvalidProxyDetails,
                            Status = OperationStatus.Failed,
                            ProcessExitCode = SetupHelper.UASetupReturnValues.InvalidProxyDetails
                        };
                    }
                }
                else
                {
                    proxyConf.RemoveKey("proxy", "Address");
                    proxyConf.RemoveKey("proxy", "Port");
                }

                // Update DRScout config to the Proxy info file.
                drScoutInfo.WriteValue(
                    "vxagent",
                    "ProxySettingsPath",
                    proxyInfoPath);

                return new OperationResult
                {
                    Status = OperationStatus.Completed,
                    ProcessExitCode = SetupHelper.UASetupReturnValues.Successful
                };
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Trc.LogErrorException("CRITICAL EXCEPTION in proxy updation", ex);

                return new OperationResult
                {
                    Status = OperationStatus.Failed,
                    ProcessExitCode = SetupHelper.UASetupReturnValues.ProxyConfigurationFailed,
                    Error = StringResources.ProxyConfiguratinFailureUnifiedAgent
                };
            }
        }

        /// <summary>
        /// Update configuration status in registry.
        /// </summary>
        internal static void UpdateConfigurationStatus(string status)
        {
            Trc.Log(
                LogLevel.Always,
                "Updating configuration status as {0} in registry.", status);

            RegistryKey hklm = Registry.LocalMachine;
            using (RegistryKey inMageVxAgent = hklm.OpenSubKey(UnifiedSetupConstants.LocalMachineMSMTRegName, true))
            {
                inMageVxAgent.SetValue(
                    UnifiedSetupConstants.AgentConfigurationStatus,
                    status,
                    RegistryValueKind.String);
            }
        }
    }
}
