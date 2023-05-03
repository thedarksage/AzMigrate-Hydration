using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.ServiceProcess;
using Microsoft.Deployment.WindowsInstaller;
using Microsoft.Win32;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System.Threading;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core;

namespace ProcessServerCustomActions
{
    public class CustomActions
    {
        // process server services servicenames
        private static readonly string[] processServerServices = new[]
                {
                    "cxprocessserver",
                    "ProcessServerMonitor",
                    "ProcessServer"
                };

        private static readonly string[] processServerInputParams = new[]
                {
                    MODE
                };

        private const string PS_REGKEY_ROOT = @"SOFTWARE\Microsoft\";
        private const string PS_SUBKEY = @"Azure Site Recovery Process Server";
        private const string PS_REGKEY_PATH = @"SOFTWARE\Microsoft\Azure Site Recovery Process Server";
        private const string DOTNET_REGKEY_PATH = @"SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full\";
        private const string DOTNET_REGKEY = @"Release";
        private const string PS_REGKEY_VALUE = @"Installed On UTC";
        private const string BUILD_VERSION = @"Build_Version";
        private const string INSTALL_LOCATION = @"Install Location";
        private const string MODE = @"MODE";
        private const string CSMODE = @"CS Mode";
        private const string CACHE_LOCATION = @"CACHELOCATION";
        private const string TELEMETRY_LOCATION = @"TELEMETRYLOCATION";
        private const string REQDEF_LOCATION = @"REQDEFLOCATION";
        private const string LOG_FOLDER_PATH = @"Log Folder Path";
        private const string TELEMETRY_FOLDER_PATH = @"Telemetry Folder Path";
        private const string REQDEF_FOLDER_PATH = @"Request Default Folder Path";
        private const string DEFAULT_INSTALL_LOCATION = @"Microsoft Azure Site Recovery Process Server";
        private const string PROCESS_SERVER = @"ProcessServer";
        private const string MSAL_DLL_FULLNAME = @"Microsoft.Identity.Client.dll";
        private const string MSAL_DLL_NAME = @"Microsoft.Identity.Client";
        private const int DOTNET_MIN_VERSION = 394802;      // .Net version 4.6.2
        private const int CXPS_SUCC_EXIT_CODE = -1;

        private static int RunCommand(Session session, string filePath, string arguments = "")
        {
            int exitCode = -2;
            session.Log("Running command {0} with argument {1}", filePath, arguments);
            try
            {
                ProcessStartInfo processStartInfo = new ProcessStartInfo();

                //Set the defaults
                processStartInfo.FileName = "\"" + filePath + "\"";
                processStartInfo.Arguments = arguments;
                processStartInfo.CreateNoWindow = true;
                processStartInfo.UseShellExecute = false;
                processStartInfo.Verb = "runas";
                processStartInfo.WindowStyle = ProcessWindowStyle.Hidden;

                session.Log("Executing the following command: {0} {1}", filePath, arguments);

                using (Process process = Process.Start(processStartInfo))
                {
                    process.WaitForExit();
                    exitCode = process.ExitCode;
                    session.Log("Exit code : {0}", exitCode);
                    return exitCode;
                }
            }
            catch (Exception ex)
            {
                session.Log("Exception occured: {0}", ex.ToString());
                return exitCode;
            }
        }

        private static bool InstallationFolderCheck(Session session)
        {
            string installationLocation = session["INSTALLLOCATION"];
            if (Directory.Exists(installationLocation))
            {
                session.Log("Process Server installation path already exists");
                return false;
            }
            return true;
        }

        private static bool RegistryKeyCheck(Session session)
        {
            try
            {
                using (RegistryKey hklm = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
                {
                    string psRegKeyPath = PS_REGKEY_PATH;
                    using (RegistryKey psRegKey = hklm.OpenSubKey(psRegKeyPath))
                    {
                        if (psRegKey != null)
                        {
                            session.Log("Process Server registration key already exists");
                            return false;
                        }
                        session.Log("Process Server registration key does not exists");
                        return true;
                    }
                }
            }
            catch(Exception)
            {
                session.Log("Exception occured while accessing registry key {0}", @"HKLM\" + PS_REGKEY_PATH);
                return false;
            }
        }

        private static bool DotNetCompatibilityCheck(Session session)
        {
            // Refer MSDN link https://docs.microsoft.com/en-us/dotnet/framework/migration-guide/how-to-determine-which-versions-are-installed for details
            bool isDotNetVersionCompatible = false;
            try
            {
                using (RegistryKey hklm = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32))
                {
                    string dotnetRegKeyPath = DOTNET_REGKEY_PATH;
                    using (RegistryKey psRegKey = hklm.OpenSubKey(dotnetRegKeyPath))
                    {
                        if (psRegKey == null)
                        {
                            session.Log(@".net v4.6.2 or later versions are not installed");
                            isDotNetVersionCompatible = false;
                        }
                        else
                        {
                            var psKeyValue = psRegKey.GetValue(DOTNET_REGKEY);
                            if (psKeyValue == null)
                            {
                                session.Log(@".net v4.6.2 or later versions are not installed");
                                isDotNetVersionCompatible = false;
                            }
                            else
                            {
                                if ((int)psKeyValue >= DOTNET_MIN_VERSION)
                                {
                                    session.Log(@".net v4.6.2 or later version is installed");
                                    isDotNetVersionCompatible = true;
                                }
                                else
                                {
                                    session.Log(@".net v4.6.2 or later version is not installed");
                                    isDotNetVersionCompatible = false;
                                }
                            }
                        }
                    }
                }
            }
            catch(Exception)
            {
                session.Log("Exception occured while accessing registry key {0}", @"HKLM\" + DOTNET_REGKEY_PATH);
                isDotNetVersionCompatible = false;
            }
            return isDotNetVersionCompatible;
        }

        private static bool ProcessServerServicesCheck(Session session)
        {
            bool serviceAlreadyRunningFlag = false;
            foreach (var currSvcName in processServerServices)
            {
                if (ServiceController.GetServices().Any(s => String.Equals(s.ServiceName, currSvcName, StringComparison.OrdinalIgnoreCase)))
                {
                    session.Log("The service {0} already exists", currSvcName);
                    serviceAlreadyRunningFlag = true;
                }
            }
            return serviceAlreadyRunningFlag ? false : true;
        }

        private static bool ProcessServerInputParametersCheck(Session session)
        {
            List<string> paramsNotPresent = new List<string>();
            foreach (var parameter in processServerInputParams)
            {
                string param = session[parameter];
                if (String.IsNullOrWhiteSpace(param))
                {
                    paramsNotPresent.Add(parameter);
                }
            }
            if(paramsNotPresent.Any())
            {
                session.Log("Following parameters are not present in input {0}", String.Join(",", paramsNotPresent));
                return false;
            }
            return true;
        }

        [CustomAction]
        public static ActionResult PSPrechecks(Session session)
        {
            if (InstallationFolderCheck(session) &&
                RegistryKeyCheck(session) &&
                DotNetCompatibilityCheck(session) &&
                ProcessServerServicesCheck(session) &&
                ProcessServerInputParametersCheck(session))
            {
                return ActionResult.Success;
            }
            else
            {
                return ActionResult.Failure;
            }
        }

        [CustomAction]
        public static ActionResult UnregisterPS(Session session)
        {
            session.Log("Inside CA UnregisterPS");
            // Unregister requires version 5.0.5.0 of ActiveDirectory dll. By default clr will load
            // ActiveDirectory dll that ships with corresponding .net version. This custom action is
            // loaded by Rundll32.exe, and as such we can't modify the config file for binding redirection.
            // So we load the assembly in runtime using a resolve handler.
            AppDomain currDomain = AppDomain.CurrentDomain;
            currDomain.AssemblyResolve += CurrDomain_AssemblyResolve;
            RcmOperationContext rcmOperationContext = new RcmOperationContext();
            try
            {
                RcmApiFactory.GetProcessServerRcmApiStubs(PSConfiguration.Default)
                    .UnregisterProcessServerAsync(rcmOperationContext, PSConfiguration.Default.HostId, CancellationToken.None).Wait();
            }
            catch(Exception ex)
            {
                session.Log("Failed to unregister ps with exception {0}", ex);
                session.Log($"Activity Id : {rcmOperationContext.ActivityId}, Clientrequest Id : {rcmOperationContext.ClientRequestId}");
            }
            finally
            {
                currDomain.AssemblyResolve -= CurrDomain_AssemblyResolve;
            }
            // this is a soft failure so we don't fail the uninstallation
            // the failure can be debugged using ActivityId and ClientRequestId logged above
            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult UnconfigurePS(Session session)
        {
            session.Log("Inside CA UnconfigurePS");

            try
            {
                RcmOperationContext rcmOperationContext = new RcmOperationContext();
                var exePath = Path.Combine(session["BIN"], "PS_Configurator.ps1");
                var args = $"-Unconfigure -ClientRequestId \"{rcmOperationContext.ClientRequestId}\" -ActivityId \"{rcmOperationContext.ActivityId}\"";
                if (RunCommand(session, exePath, args) != 0)
                {
                    session.Log("Failed to unconfigure Process Server");
                    session.Log($"Activity Id : {rcmOperationContext.ActivityId}, Clientrequest Id : {rcmOperationContext.ClientRequestId}");
                    //Don't return from here as this is not a hard failure
                }
            }
            catch (Exception ex)
            {
                // Ignoring the exception inorder to unblock uninstallation
                session.Log("Failed to unconfigure Process Server with exception " + ex);
            }

            // this is a soft failure so we don't fail the uninstallation
            // the failure can be debugged using ActivityId and ClientRequestId logged above
            return ActionResult.Success;
        }

        /// <summary>
        /// This resolve handler is added during UnregisterPS custom action.
        /// If the runtime is not able to find any assembly, it will try to 
        /// use this function to get the correct assembly
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="args"></param>
        /// <returns>The resolved assembly if found, null otherwise</returns>
        private static Assembly CurrDomain_AssemblyResolve(object sender, ResolveEventArgs args)
        {
            string assemblyPath = null;
            try
            {
                using (RegistryKey hklm = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
                {
                    using (RegistryKey psRegKey = hklm.OpenSubKey(PS_REGKEY_PATH))
                    {
                        if (psRegKey != null)
                        {
                            var psInstallPathKeyValue = psRegKey.GetValue(INSTALL_LOCATION);
                            string[] paths = new string[] {
                                (string)psInstallPathKeyValue,
                                Settings.Default.Rcm_SubPath_Root,
                                Settings.Default.CommonMode_SubPath_Bin,
                                PROCESS_SERVER, MSAL_DLL_FULLNAME };
                            string resolvedPath = Path.Combine(paths);
                            if (File.Exists(resolvedPath))
                            {
                                assemblyPath = resolvedPath;
                            }
                        }
                    }
                }
            }
            catch (Exception) { /* No op*/}

            AssemblyName assemblyName = new AssemblyName(args.Name);
            if (assemblyPath != null && assemblyName.Name == MSAL_DLL_NAME)
            {
                Assembly assembly = Assembly.LoadFrom(assemblyPath);
                return assembly;
            }

            return null;
        }

        [CustomAction]
        public static ActionResult GetCurrentDateTime(Session session)
        {
            session["DATETIME"] = DateTime.UtcNow.ToString();
            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult DeleteDirectory(Session session)
        {
            session.Log("Inside CA DeleteDirectory");

            var foldersToDelete = new[]
            {
                // Deleted in this order. Resilient to failures.
                new Tuple<string, bool>(REQDEF_LOCATION, true),
                new Tuple<string, bool>(CACHE_LOCATION, true),
                new Tuple<string, bool>(TELEMETRY_LOCATION, true),
                new Tuple<string, bool>("INSTALLLOCATION", false)
            };

            foreach (var currFolderTuple in foldersToDelete)
            {
                string property = currFolderTuple.Item1;
                bool ignoreIfNotSet = currFolderTuple.Item2;

                var currFolderPath = session[property];

                int retryCount = 0;
                const int MAX_RETRY_COUNT = 3;
                bool failed;

                do
                {
                    failed = false;

                    try
                    {
                        if (ignoreIfNotSet && string.IsNullOrWhiteSpace(currFolderPath))
                        {
                            session.Log("{0} is not configured on this PS", property);
                        }
                        else if (Directory.Exists(currFolderPath))
                        {
                            session.Log(
                                "{0} - {1} exists, deleting it. Attempt {2}",
                                property, currFolderPath, retryCount);

                            Directory.Delete(currFolderPath, true);
                        }
                        else
                        {
                            session.Log("{0} - {1} is not present", property, currFolderPath);
                        }

                        break;
                    }
                    catch (Exception ex)
                    {
                        // since this happens during uninstallation, we don't fail
                        // uninstall if the folder is not deleted.
                        session.Log(
                            "Deletion of directory {0} ({1}) failed with exception {2}",
                            currFolderPath, property, ex);

                        failed = true;
                    }
                } while (failed && retryCount++ < MAX_RETRY_COUNT);
            }

            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult GetInstallDirectory(Session session)
        {
            session.Log("Inside CA GetInstallDirectory");
            try
            {
                using (RegistryKey hklm = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
                {
                    using (RegistryKey psRegKey = hklm.OpenSubKey(PS_REGKEY_PATH))
                    {
                        if (psRegKey == null)
                        {
                            session.Log("Failed to open registry key {0}", @"HKLM\" + PS_REGKEY_PATH);
                            return ActionResult.Failure;
                        }
                        else
                        {
                            var psInstLocKeyValue = psRegKey.GetValue(INSTALL_LOCATION);
                            var psModeKeyValue = psRegKey.GetValue(CSMODE);
                            var psLogLocationKeyValue = psRegKey.GetValue(LOG_FOLDER_PATH);
                            var psTelLocationKeyValue = psRegKey.GetValue(TELEMETRY_FOLDER_PATH);
                            var psReqDefLocationKeyValue = psRegKey.GetValue(REQDEF_FOLDER_PATH);
                            if (psInstLocKeyValue == null)
                            {
                                session.Log("Failed to get registry value for key {0}", INSTALL_LOCATION);
                                return ActionResult.Failure;
                            }
                            else if (psModeKeyValue == null)
                            {
                                session.Log("Failed to get registry value for key {0}", CSMODE);
                                return ActionResult.Failure;
                            }
                            // NOTE-SanKumar-1910: CACHE_LOCATION, TELEMETRY_FOLDER_PATH and
                            // REQDEF_LOCATION could be null/empty, if the PS was never configured
                            // (or) if it has been unconfigured.
                            else
                            {
                                session["INSTALLLOCATION"] = (string)psInstLocKeyValue;
                                session["MODE"] = (string)psModeKeyValue;
                                session[CACHE_LOCATION] = (string)psLogLocationKeyValue;
                                session[TELEMETRY_LOCATION] = (string)psTelLocationKeyValue;
                                session[REQDEF_LOCATION] = (string)psReqDefLocationKeyValue;
                                return ActionResult.Success;
                            }
                        }
                    }
                }
            }
            catch(Exception ex)
            {
                session.Log("Exception {0} encountered while getting installation location", ex);
                return ActionResult.Failure;
            }
        }

        [CustomAction]
        public static ActionResult SetRegistry(Session session)
        {
            session.Log("Inside CA SetRegistry");
            try
            {
                using (RegistryKey hklm = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
                {
                    string psRegKeyPath = PS_REGKEY_PATH;
                    using (RegistryKey psRegKey = hklm.OpenSubKey(psRegKeyPath, true))
                    {
                        if (psRegKey != null)
                        {
                            psRegKey.SetValue(PS_REGKEY_VALUE, session["DATETIME"], RegistryValueKind.String);
                            session.Log("Successfully set registry value {0}", PS_REGKEY_VALUE);
                        }
                        else
                        {
                            session.Log("The registry key {0} does not exists. Ignoring.", @"HKLM\" + PS_REGKEY_PATH);
                        }
                    }
                }
            }
            catch(Exception ex)
            {
                session.Log("Exception {0} occured while setting registry value {1} for registry key {2}", ex, PS_REGKEY_VALUE, @"HKLM\" + PS_REGKEY_PATH);
            }
            return ActionResult.Success;
        }

        // This custom action is optional.
        // Windows installer will automatically delete the registry key PS_REGKEY_PATH with all its values,
        // so this CA should actually never be required.
        // Added it for future proofing, if we add subkeys inside this registry key
        [CustomAction]
        public static ActionResult DeleteRegistry(Session session)
        {
            session.Log("Inside CA DeleteRegistry");
            try
            {
                using (RegistryKey hklm = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
                {
                    using (RegistryKey psRegKey = hklm.OpenSubKey(PS_REGKEY_ROOT, true))
                    {
                        psRegKey.DeleteSubKeyTree(PS_SUBKEY);
                    }
                    session.Log("Successfully removed PS Registry key {0}", @"HKLM\" + PS_REGKEY_PATH);
                }
                
            }
            catch(Exception ex)
            {
                session.Log("Exception {0} occured while removing registry key {1}. Please remove it manually", ex, @"HKLM\" + PS_REGKEY_PATH);
                //Since this is an soft failure we don't fail the uninstall step
            }
            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult VersionCheck(Session session)
        {
            session.Log("Inside CA VersionCheck");

            Assembly assembly = Assembly.GetExecutingAssembly();
            FileVersionInfo fvi = FileVersionInfo.GetVersionInfo(assembly.Location);
            Version installerVersion = new Version(fvi.FileVersion);
            session.Log("Present Installer Version: {0}", installerVersion);

            try
            {
                using (RegistryKey hklm = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
                {
                    string psRegKeyPath = PS_REGKEY_PATH;
                    using (RegistryKey psRegKey = hklm.OpenSubKey(psRegKeyPath))
                    {
                        if (psRegKey != null)
                        {
                            session.Log("Process Server registration key already exists");
                            Version installedVersion = new Version(psRegKey.GetValue(BUILD_VERSION).ToString());

                            session.Log("Installer version : " + installerVersion);
                            session.Log("Installed version : " + installedVersion);

                            if (installedVersion > installerVersion)
                            {
                                session.Log("A newer version of process server is already installed. Downgrade is not allowed.");
                                return ActionResult.Failure;
                            }
                            else
                            {
                                session.Log("Installed version is less than or equal to installer version.");
                                return ActionResult.Success;
                            }
                        }
                        else
                        {
                            session.Log("Fresh installation of process server");
                            return ActionResult.Success;
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                session.Log("Exception {0} occured while accessing registry key {1}", ex, @"HKLM\" + PS_REGKEY_PATH);
                return ActionResult.Failure;
            }
        }

        [CustomAction]
        public static ActionResult InstallServices(Session session)
        {
            try
            {
                var exePath = Path.Combine(session["TRANSPORT"], "cxps.exe");
                var args = $"-c \"{Path.Combine(session["TRANSPORT"], "cxps.conf")}\" install";
                if(RunCommand(session, exePath, args) != CXPS_SUCC_EXIT_CODE)
                {
                    session.Log("Failed to install cxps service");
                    return ActionResult.Failure;
                }

                exePath = Path.Combine(session["BIN"], "ProcessServer.exe");
                args = "/i";
                if(RunCommand(session, exePath, args) != 0)
                {
                    session.Log("Failed to install Process Server service");
                    return ActionResult.Failure;
                }

                exePath = Path.Combine(Environment.SystemDirectory, "sc.exe");
                args = "failure \"ProcessServer\" actions= restart/300000/// reset= 0";
                if(RunCommand(session, exePath, args) != 0)
                {
                    session.Log("Failed to set failure action for Process Server service");
                    //Don't return from here as this is not a hard failure
                }

                exePath = Path.Combine(session["BIN"], "ProcessServerMonitor.exe");
                args = "/i";
                if(RunCommand(session, exePath, args) != 0)
                {
                    session.Log("Failed to install Process Server Monitor service");
                    return ActionResult.Failure;
                }

                exePath = Path.Combine(Environment.SystemDirectory, "sc.exe");
                // TODO: Move values to Settings
                args = "failure \"ProcessServerMonitor\" actions= restart/120000/// reset= 0";
                if(RunCommand(session, exePath, args) != 0)
                {
                    session.Log("Failed to set failure action for Process Server Monitor service");
                    //Don't return from here as this is not a hard failure
                }

                return ActionResult.Success;
            }
            catch(Exception ex)
            {
                session.Log("Service installation failed with exception " + ex);
                return ActionResult.Failure;
            }
        }

        [CustomAction]
        public static ActionResult StopAndDeleteServices(Session session)
        {
            try
            {
                var exePath = Path.Combine(session["TRANSPORT"], "cxps.exe");
                var args = $"-c \"{Path.Combine(session["TRANSPORT"], "cxps.conf")}\" uninstall";
                if(RunCommand(session, exePath, args) != CXPS_SUCC_EXIT_CODE)
                {
                    session.Log("Failed to stop and delete cxps service");
                    return ActionResult.Failure;
                }

                exePath = Path.Combine(session["BIN"], "ProcessServer.exe");
                args = "/u";
                if(RunCommand(session, exePath, args) != 0)
                {
                    session.Log("Failed to stop and delete Process Server service");
                    return ActionResult.Failure;
                }

                exePath = Path.Combine(session["BIN"], "ProcessServerMonitor.exe");
                args = "/u";
                if(RunCommand(session, exePath, args) != 0)
                {
                    session.Log("Failed to stop and delete Process Server Monitor service");
                    return ActionResult.Failure;
                }
            }
            catch (Exception ex)
            {
                session.Log("Service delete failed with exception " + ex);
                return ActionResult.Failure;
            }
            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult StopServices(Session session)
        {
            foreach (var service in processServerServices)
            {
                try
                {
                    if (!ServiceUtils.IsServiceInstalled(service))
                    {
                        session.Log(service + " is not installed.");
                        continue;
                    }

                    ServiceUtils.StopService(service, CancellationToken.None);
                    session.Log("Successfully stopped " + service + " service.");
                }
                catch (Exception ex)
                {
                    session.Log($"Failed to stop process server services with exception {ex}");
                    return ActionResult.Failure;
                }
            }
            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult StartServices(Session session)
        {
            foreach (var service in processServerServices)
            {
                try
                {
                    if (!ServiceUtils.IsServiceInstalled(service))
                    {
                        session.Log(service + " is not installed.");
                        continue;
                    }

                    ServiceUtils.StartService(service, CancellationToken.None);
                    session.Log("Successfully started " + service + " service.");
                }
                catch (Exception ex)
                {
                    session.Log("Failed to start service {0} due to exception {1}", service, ex);
                    return ActionResult.Failure;
                }
            }
            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult InputValidation(Session session)
        {
            string mode = session[MODE];
            CSMode csmode;
            if(Enum.TryParse<CSMode>(mode, true, out csmode))
            {
                return ActionResult.Success;
            }
            else
            {
                session.Log($"CS mode {mode} is invalid");
                return ActionResult.Failure;
            }
        }

        [CustomAction]
        public static ActionResult DeleteFirewallRules(Session session)
        {
            try
            {
                FirewallUtils.RemoveFirewallRules(Settings.Default.AsrPSFwGroupName);
            }
            catch(Exception ex)
            {
                session.Log("Failed to remove firewall rules with exception {0}", ex);
            }
            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult CreateFolder(Session session)
        {
            try
            {
                if(String.IsNullOrWhiteSpace(session["INSTALLLOCATION"]))
                {
                    session["INSTALLLOCATION"] = Environment.ExpandEnvironmentVariables("%ProgramFiles%\\" + DEFAULT_INSTALL_LOCATION);
                    session.Log("The value of INSTALLLOCATION parameter is {0}", session["INSTALLLOCATION"]);
                }

                session.Log($"Creating directory {session["INSTALLLOCATION"]}");
                FSUtils.CreateAdminsAndSystemOnlyDir(session["INSTALLLOCATION"]);
            }
            catch(Exception ex)
            {
                session.Log($"Failed to create directory {session["INSTALLLOCATION"]} with exception {ex}");
                return ActionResult.Failure;
            }
            return ActionResult.Success;
        }
    }
}
