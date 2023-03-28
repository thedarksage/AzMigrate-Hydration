using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;
using Microsoft.Deployment.WindowsInstaller;
using Microsoft.Win32;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.IO;
using System.Threading;
using System.ComponentModel;
using System.Linq;
using ASRSetupFramework;
using ASRResources;
using System.Security.AccessControl;
using System.ServiceProcess;

namespace UnifiedAgentCustomActions
{

    public class CustomActions
    {
        //Detects Configuration/Process Server installation
        private static bool IsCXInstalled()
        {
            if ((Registry.GetValue(UnifiedSetupConstants.CSPSRegistryName, UnifiedSetupConstants.AgentProductVersion, null) != null) && ServiceControlFunctions.IsInstalled("tmansvc"))
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        //Sets value to CXINSTALLED property
        [CustomAction]
        public static ActionResult CXInstalled(Session session)
        {
            if ((Registry.GetValue(UnifiedSetupConstants.CSPSRegistryName, UnifiedSetupConstants.AgentProductVersion, null) != null) && ServiceControlFunctions.IsInstalled("tmansvc"))
            {
                // setting a property
                session["CXINSTALLED"] = "yes";
                session.Log("Configuration/Process Server is already installed on this machine.");
            }
            else
            {
                // setting a property
                session["CXINSTALLED"] = "no";
                session.Log("Configuration/Process Server is not installed on this machine.");
            }
            return ActionResult.Success;
        }

        // Deletes directory
        public static bool DeleteDir(string path, Session session)
        {
            bool returnCode = false;
            // Checks whether directory already exists
            if (Directory.Exists(path))
            {
                session.Log("\"" + path + "\" directory is exists. Deleting it.");
                try
                {
                    //deletes directory
                    Directory.Delete(path, true);
                    session.Log("Successfully deleted \"" + path + "\" directory.");
                    return true;
                }
                catch (Exception DeleteException)
                {
                    session.Log("Unable to Delete \"" + path + "\" directory.");
                    session.Log(DeleteException.ToString());
                }
            }
            else
            {
                session.Log("\"" + path + "\" directory is not available.");
                return true;
            }
            return returnCode;
        }

        //Deletes Install Directory
        [CustomAction]
        public static ActionResult DeleteDirectory(Session session)
        {
            try
            {
                session.Log("Begin DeleteDirectory");
                string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);
                string strProgramDataPath =
                            Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
                string InDskFltPath = strProgramDataPath + "\\Microsoft Azure Site Recovery\\InDskFlt";
                string MASRPath = strProgramDataPath + "\\Microsoft Azure Site Recovery";
                string InstallPath = session["INSTALLDIR"];
                string SRVPath = sysDrive + "SRV";

                DeleteDir(InDskFltPath, session);

                if (IsCXInstalled())
                {
                    session.Log("Not deleting the \"" + strProgramDataPath + "\\Microsoft Azure Site Recovery\" path as Configuration/Process Server is installed on this machine.");

                }
                else
                {
                    session.Log("Configuration/Process Server is not installed on this machine.");
                    DeleteDir(MASRPath, session);
                }

                DeleteDir(InstallPath, session);
                DeleteDir(SRVPath, session);
            }
            catch (Exception ex)
            {
                session.Log("Exception at DeleteDirectory: " + ex + "");
            }

            return ActionResult.Success;
        }

        //Stops Volume Replication service
        [CustomAction]
        public static ActionResult UninstallActions(Session session)
        {
            try
            {
                session.Log("Begin UninstallActions");
                ServiceController service = ServiceControlFunctions.IsServiceInstalled("svagents");
                if (null != service)
                {
                    session.Log("Volume Replication service installed.");
                    if (service.Status == ServiceControllerStatus.Running)
                    {
                        Platform installedPlatform;
                        string vmPlatform = SetupHelper.GetAgentInstalledPlatformFromDrScout();
                        installedPlatform = (string.IsNullOrEmpty(vmPlatform)) ?
                            SetupHelper.GetAgentInstalledPlatform() :
                            vmPlatform.AsEnum<Platform>().Value;
                        session.Log("Installed platform - {0}", installedPlatform);

                        session.Log("Volume Replication service running. Invoking taskkill command to stop Volume Replication service.");
                        ProcessStartInfo processStartInfo;
                        if (installedPlatform == Platform.VmWare)
                        {
                            processStartInfo = new ProcessStartInfo("cmd", "/C taskkill /im svagentsCS.exe /f /t");
                        }
                        else
                        {
                            processStartInfo = new ProcessStartInfo("cmd", "/C taskkill /im svagentsRCM.exe /f /t");
                        }

                        processStartInfo.CreateNoWindow = true;
                        processStartInfo.UseShellExecute = false;
                        processStartInfo.WindowStyle = ProcessWindowStyle.Hidden;

                        using (Process process = Process.Start(processStartInfo))
                        {
                            process.WaitForExit();
                            int exitCode = process.ExitCode;
                            if (exitCode == 0)
                            {
                                session.Log("Stopped the Volume Replication service successfully.");
                                return ActionResult.Success;
                            }
                            else
                            {
                                session.Log("Unable to stop Volume Replication Service.");
                                return ActionResult.Failure;
                            }
                        }
                    }

                }
            }
            catch (Exception ex)
            {
                session.Log("Exception at UninstallActions: " + ex + "");
            }

            return ActionResult.Success;
        }

        //Uninstall ServiceCtrl
        [CustomAction]
        public static ActionResult UninstallServiceCtrl(Session session)
        {
            try
            {
                session.Log("Begin UninstallServiceCtrl");

                session.Log("Invoking ServiceCtrl.exe");
                string InstallDirectory = (string)Registry.GetValue(@"HKEY_LOCAL_MACHINE\Software\InMage Systems\Installed Products\5", "InstallDirectory", null);
                string ServiceCtrlFile = InstallDirectory + "\\ServiceCtrl.exe";

                int ReturnCode = CommandExecutor.RunCommand(ServiceCtrlFile, "TrkWks restore");
                if (ReturnCode == 0)
                {
                    session.Log("uninstalled ServiceCtrl.exe successfully");
                }
                else
                {
                    session.Log("uninstallation of ServiceCtrl.exe failed");
                }
            }
            catch (Exception ex)
            {
                session.Log("Exception at UninstallServiceCtrl: " + ex + "");
            }

            return ActionResult.Success;
        }

        //Unregister VX and FX agents from CS.
        [CustomAction]
        public static ActionResult UnregisterAgent(Session session)
        {
            try
            {
                session.Log("Begin UnregisterAgent");
                string InstallDirectory = (string)Registry.GetValue(@"HKEY_LOCAL_MACHINE\Software\InMage Systems\Installed Products\5", "InstallDirectory", null);
                string UnregagentFileVX = InstallDirectory + "\\unregagent.exe";
                string UnregagentFileFX = InstallDirectory + "\\FileRep\\unregagent.exe";
                string output;

                session.Log("Invoking unregagent.exe.");
                int ReturnCode = CommandExecutor.ExecuteCommand(UnregagentFileVX, out output, "Vx Y");
                session.Log("unregagent VX exit code: {0}", ReturnCode);
                session.Log("unregagent binary output for VX: {0}", output);
                if (ReturnCode == 0)
                {
                    session.Log("VX agent unregistered successfully.");
                }
                else
                {
                    session.Log("Failed to unregister VX agent.");
                }

                session.Log("Invoking unregagent.exe.");
                int ResultCode = CommandExecutor.ExecuteCommand(UnregagentFileFX, out output, "filereplication Y");
                session.Log("unregagent FX exit code: {0}", ReturnCode);
                session.Log("unregagent binary output for FX: {0}", output);
                if (ResultCode == 0)
                {
                    session.Log("uninstalled unregagent.exe for fx successfully");
                }
                else
                {
                    session.Log("uninstallation of  unregagent.exe for fx failed");
                }
            }
            catch (Exception ex)
            {
                session.Log("Exception at UnregisterAgent: " + ex + "");
            }

            return ActionResult.Success;
        }

        //Invokes InDskFlt manifest file in agent uninstall
        [CustomAction]
        public static ActionResult InvokeIndskfltManifestUninstall(Session session)
        {
            try
            {
                session.Log("Begin InvokeIndskfltManifestUninstall");
                // Set variables
                string InstallDirectory = (string)Registry.GetValue(@"HKEY_LOCAL_MACHINE\Software\InMage Systems\Installed Products\5", "InstallDirectory", null);
                string ManifestFile = InstallDirectory + "\\DiskFltEvents.man";
                string SystemPath = Environment.GetFolderPath(Environment.SpecialFolder.System);
                string WevtutilFile = SystemPath + "\\wevtutil.exe";
                string output = "";

                session.Log("Uninstalling InDskFlt manifest.");
                int ReturnCode = CommandExecutor.RunCommand(WevtutilFile, "um \"" + ManifestFile + "\"", out output);

                session.Log("InDskFlt manifest uninstallation output.");
                session.Log("---------------------------------------");
                session.Log(output);
                session.Log("---------------------------------------");

                session.Log("InDskFlt manifest uninstallation Return Code:" + ReturnCode);
                if (ReturnCode == 0)
                {
                    session.Log("InDskFlt manifest uninstalled successfully.");
                }
                else
                {
                    session.Log("InDskFlt manifest uninstallation failed with error:" + ReturnCode);
                    return ActionResult.Failure;
                }
            }
            catch (Exception ex)
            {
                session.Log("Exception at InvokeIndskfltManifestUninstall: " + ex + "");
            }

            return ActionResult.Success;
        }

        //Removes InDskFlt eventlog registry
        [CustomAction]
        public static ActionResult RemoveInDskFltEventlogRegistry(Session session)
        {
            try
            {
                session.Log("Begin RemoveInDskFltEventlogRegistry");
                using (RegistryKey InDskFltKey = Registry.LocalMachine.OpenSubKey(@"System\CurrentControlSet\Services\EventLog\System\", true))
                {
                    if (InDskFltKey.OpenSubKey("InDskFlt") != null)
                    {
                        session.Log("Found references of InDskFlt eventlog from EventLog key. deleting them.");
                        InDskFltKey.DeleteSubKeyTree("InDskFlt");
                    }
                    else
                    {
                        session.Log("Not found references of InDskFlt registry from EventLog key.");
                    }
                }
            }
            catch (Exception ex)
            {
                session.Log("Error: " + ex + "");
            }

            return ActionResult.Success;
        }

        //Assigns Azure VM agent install status to isAzureVMAgentInstalled property
        [CustomAction]
        public static ActionResult AzureVMAgentInstalled(Session session)
        {
            session.Log("Begin AzureVMAgentInstalled");
            try
            {
                string azureVMAgentInstalled = (string)Registry.GetValue(
                    UnifiedSetupConstants.AgentRegistryName,
                    UnifiedSetupConstants.AzureVMAgentInstalled,
                    null);

                if (azureVMAgentInstalled == UnifiedSetupConstants.Yes)
                {
                    session["isAzureVMAgentInstalled"] = "yes";
                    session.Log("Azure VM agent is installed.");
                }
                else
                {
                    session["isAzureVMAgentInstalled"] = "no";
                    session.Log("Azure VM agent is not installed.");
                }
            }
            catch (Exception ex)
            {
                session["isAzureVMAgentInstalled"] = "no";
                session.Log("Exception at AzureVMAgentInstalled: " + ex + "");
            }

            return ActionResult.Success;
        }

        // Delete registries.
        [CustomAction]
        public static ActionResult RemoveRegistries(Session session)
        {
            session.Log("Begin RemoveRegistries.");
            try
            {
                string rootKeyName = UnifiedSetupConstants.RootRegKey;
                string strSubKey = @"SV Systems";
                string InMagesystemsSubKey = @"InMage Systems";

                RegistryKey regkeySoftware = Registry.LocalMachine.OpenSubKey(rootKeyName, true);
                regkeySoftware.DeleteSubKeyTree(strSubKey);
                regkeySoftware.DeleteSubKeyTree(InMagesystemsSubKey);
                regkeySoftware.Close();
            }
            catch (Exception ex)
            {
                session.Log("Exception at RemoveRegistries: " + ex + "");
            }

            return ActionResult.Success;
        }

        // Stop services.
        [CustomAction]
        public static ActionResult StopServices(Session session)
        {
            try
            {
                List<string> Services = new List<string>()
                {
                    "cxprocessserver",
                    "svagents",
                    "InMage Scout Application Service",
                };

                foreach (string service in Services)
                {
                    if (!ServiceControlFunctions.IsInstalled(service))
                    {
                        session.Log(service + " is not installed.");
                        continue;
                    }

                    if (!ServiceControlFunctions.IsEnabledAndRunning(service))
                    {
                        session.Log(service + " is not enabled or it is not running.");
                        continue;
                    }

                    if (service == "cxprocessserver" && IsCXInstalled())
                    {
                        session.Log("Not stopping cxprocessserver as Configuration/Process server is installed.");
                        continue;
                    }

                    if (ServiceControlFunctions.StopService(service))
                    {
                        session.Log("Successfully stopped " + service + " service.");
                    }
                    else
                    {
                        session.Log("Failed to stop " + service + " service.");
                        return ActionResult.Failure;
                    }
                }
            }
            catch (Exception ex)
            {
                session.Log("Exception at StopServices: " + ex + "");
                return ActionResult.Failure;
            }

            return ActionResult.Success;
        }

        // Uninstall services.
        [CustomAction]
        public static ActionResult UninstallServices(Session session)
        {
            try
            {
                List<string> Services = new List<string>()
                {
                    "svagents",
                    "InMage Scout Application Service",
                };

                foreach (string service in Services)
                {
                    if (!ServiceControlFunctions.IsInstalled(service))
                    {
                        session.Log(service + " is not installed.");
                        continue;
                    }

                    if (ServiceControlFunctions.UninstallService(service))
                    {
                        session.Log("Successfully uninstalled " + service + " service.");
                    }
                    else
                    {
                        session.Log("Failed to uninstall " + service + " service.");
                        return ActionResult.Failure;
                    }
                }
            }
            catch (Exception ex)
            {
                session.Log("Exception at UninstallServices: " + ex + "");
                return ActionResult.Failure;
            }

            return ActionResult.Success;
        }
    }
}
