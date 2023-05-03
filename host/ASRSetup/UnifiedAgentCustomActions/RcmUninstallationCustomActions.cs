using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using ASRSetupFramework;
using Microsoft.Deployment.WindowsInstaller;

namespace UnifiedAgentCustomActions
{
    /// <summary>
    /// Actions to be performed when uninstallation is running
    /// and agent is connected to RCM service.
    /// </summary>
    public class RcmUninstallationCustomActions
    {
        /// <summary>
        /// Unregisters Source agent from RCM service.
        /// </summary>
        /// <param name="session">Msiexec session.</param>
        /// <returns>Result of the custom action.</returns>
        [CustomAction]
        public static ActionResult UnregisterSourceAgent(Session session)
        {
            try
            {
                // The action could be run both in CS or RCM perspective. We 
                // need to figure out if the mode is RCM or not.
                string cstype = SetupHelper.GetCSTypeFromDrScout();
                if (SetupHelper.GetAgentInstalledPlatform() == Platform.Azure || 
                    cstype.Equals(ConfigurationServerType.CSPrime.ToString(),StringComparison.OrdinalIgnoreCase))
                {
                    session.Log("Running CA for Azure platform");

                    if (SetupHelper.IsAgentConfiguredCompletely())
                    {
                        session.Log("Agent is configured, unregistering agent");
                        string installationLocation = SetupHelper.GetAgentInstalledLocation();
                        string rcmCliPath = Path.Combine(
                            installationLocation,
                            UnifiedSetupConstants.AzureRCMCliExeName);

                        for (int i = 0; i < 2; i++)
                        {
                            session.Log("Unregistersourceagent count : {0}" , i);
                            string output;
                            int returnCode = CommandExecutor.RunCommand(rcmCliPath, "--unregistersourceagent", out output);
                            session.Log("Unregistersourceagent command output : {0}", output);
                            session.Log("Unregistersourceagent command iteration: {0} return code : {1}", i, returnCode);
                            if (returnCode == 0)
                            {
                                session.Log("Unregistersourceagent executed successfully.");
                                break;
                            }
                            else
                            {
                                session.Log("Unregistersourceagent execution failed.");
                            }
                        }

                        return ActionResult.Success;
                    }
                    else
                    {
                        session.Log("Skipping Unregistersourceagent CA execution as agent is not registered");
                        return ActionResult.Success;
                    }
                }
                else
                {
                    session.Log("Skipping Unregistersourceagent CA execution as Platform is not Azure");
                    return ActionResult.Success;
                }
            }
            catch (Exception ex)
            {
                // Marking all errors as soft failures.
                session.Log(LogLevel.Error, "Hit exception while unregistering agent {0}", ex);
                return ActionResult.Success;
            }
        }

        /// <summary>
        /// Unregisters machine from RCM service.
        /// </summary>
        /// <param name="session">Msiexec session.</param>
        /// <returns>Result of the custom action.</returns>
        [CustomAction]
        public static ActionResult UnregisterMachine(Session session)
        {
            try
            {
                // The action could be run both in CS or RCM perspective. We 
                // need to figure out if the mode is RCM or not.
                string cstype = SetupHelper.GetCSTypeFromDrScout();
                if (SetupHelper.GetAgentInstalledPlatform() == Platform.Azure || 
                    cstype.Equals(ConfigurationServerType.CSPrime.ToString(),StringComparison.OrdinalIgnoreCase))
                {
                    session.Log("Running CA for Azure platform");

                    if (SetupHelper.IsAgentConfiguredCompletely())
                    {
                        session.Log("Agent is configured, unregistering machine");
                        string installationLocation = SetupHelper.GetAgentInstalledLocation();
                        string rcmCliPath = Path.Combine(
                            installationLocation,
                            UnifiedSetupConstants.AzureRCMCliExeName);

                        for (int i = 0; i < 2; i++)
                        {
                            session.Log("unregistermachine count : {0}", i);
                            string output;
                            int returnCode = CommandExecutor.RunCommand(rcmCliPath, "--unregistermachine", out output);
                            session.Log("unregistermachine command output : {0}", output);
                            session.Log("unregistermachine command iteration: {0} return code : {1}", i, returnCode);
                            if (returnCode == 0)
                            {
                                session.Log("unregistermachine executed successfully.");
                                break;
                            }
                            else
                            {
                                session.Log("unregistermachine execution failed.");
                            }
                        }

                        return ActionResult.Success;
                    }
                    else
                    {
                        session.Log("Skipping unregistermachine CA execution as agent is not registered");
                        return ActionResult.Success;
                    }
                }
                else
                {
                    session.Log("Skipping unregistermachine CA execution as Platform is not Azure");
                    return ActionResult.Success;
                }
            }
            catch (Exception ex)
            {
                // Marking all errors as soft failures.
                session.Log(LogLevel.Error, "Hit exception while unregistering machine {0}", ex);
                return ActionResult.Success;
            }
        }

        /// <summary>
        /// Removes AAD authentication certificate.
        /// </summary>
        /// <param name="session">Msiexec session.</param>
        /// <returns>Result of the custom action.</returns>
        [CustomAction]
        public static ActionResult RemoveAADAuthCertificate(Session session)
        {
            try
            {
                // The action could be run both in CS or RCM perspective. We 
                // need to figure out if the mode is RCM or not.
                if (SetupHelper.GetAgentInstalledPlatform() == Platform.Azure)
                {
                    session.Log("Running CA for Azure platform");

                    if (SetupHelper.IsAgentConfiguredCompletely())
                    {
                        session.Log("Agent is configured. Checks if cert need deletion");
                        string installationDir = SetupHelper.GetAgentInstalledLocation();

                        X509Store machineCertificateStore =
                            new X509Store(StoreName.My, StoreLocation.LocalMachine);
                        machineCertificateStore.Open(OpenFlags.ReadWrite);

                        IniFile drsoutConf = new IniFile(
                            Path.Combine(installationDir, @"Application Data\etc\drscout.conf"));
                        string rcmInfoPath = drsoutConf.ReadValue("vxagent", "RcmSettingsPath");
                        IniFile rcmConf = new IniFile(rcmInfoPath);
                        string certThumbprint = rcmConf.ReadValue("rcm.auth", "AADAuthCert");
                        X509Certificate2Collection aadCert = machineCertificateStore.Certificates.Find(
                            X509FindType.FindByThumbprint,
                            certThumbprint,
                            false);

                        // Since the search is by Thumbprint only at max one cert can be there.
                        if (aadCert.Count > 0)
                        {
                            session.Log("Deleting cert with thumbprint {0}", certThumbprint);
                            machineCertificateStore.Remove(aadCert[0]);
                        }

                        return ActionResult.Success;
                    }
                    else
                    {
                        session.Log("Skipping CA execution as agent is not registered");
                        return ActionResult.Success;
                    }
                }
                else
                {
                    session.Log("Skipping CA execution as Platform is not Azure");
                    return ActionResult.Success;
                }
            }
            catch (Exception ex)
            {
                // Marking all errors as soft failures.
                session.Log(LogLevel.Error, "Hit exception while deleting certs {0}", ex);
                return ActionResult.Success;
            }
        }
    }
}
