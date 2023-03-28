using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.ServiceProcess;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using Microsoft.Win32;

using ASRResources;

namespace ASRSetupFramework
{
    /// <summary>
    /// Contains custom actions ported from InMage MSI custom actions.
    /// </summary>
    public partial class SetupHelper
    {
        #region External Kernel32 Imports
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool Wow64DisableWow64FsRedirection(ref IntPtr ptr);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool Wow64RevertWow64FsRedirection(IntPtr ptr);
        #endregion

        /// <summary>
        /// Deletes Filter driver registry.
        /// </summary>
        public static void DeleteInDskFltRegistry()
        {
            Trc.Log(LogLevel.Always, "Begin DeleteInDskFltRegistry");

            using (RegistryKey InDskFltKey = Registry.LocalMachine.OpenSubKey(@"System\CurrentControlSet\Services\", true))
            {

                if (InDskFltKey.OpenSubKey("InDskFlt") != null)
                {
                    Trc.Log(LogLevel.Always, "Deleting InDskFlt registry from services key.");
                    InDskFltKey.DeleteSubKeyTree("InDskFlt");
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Didn't find references of InDskFlt registry from services key.");
                }
            }
        }

        /// <summary>
        /// Registers CDPCLI agent.
        /// </summary>
        /// <returns>True if registration succeeded, false otherwise.</returns>
        public static bool RegisterAgent()
        {
            try
            {
                Trc.Log(LogLevel.Info, "Begin RegisterAgent");

                string installationPath = GetAgentInstalledLocation();
                string cdpcliPath = Path.Combine(installationPath, "cdpcli.exe");
                string drScoutPath = Path.Combine(
                    installationPath, 
                    UnifiedSetupConstants.DrScoutConfigRelativePath);
                string cdpcliLogFile = installationPath + "\\cdpcli.log";
                string output = "";
                string args = "--registerhost --loglevel=7";

                bool retVal = false;

                //Attempt registation
                Trc.Log(LogLevel.Info, "Running cdpcli {0} command", args);

                for (int i = 0; i < 3; i++)
                {
                    int cdpcliReturnCode = CommandExecutor.RunCommand(cdpcliPath, args, out output);

                    Trc.Log(LogLevel.Info, "cdpcli {0} command output.", args);
                    Trc.Log(LogLevel.Info, "---------------------------------------");
                    Trc.Log(LogLevel.Info, output);
                    Trc.Log(LogLevel.Info, "---------------------------------------");

                    if (cdpcliReturnCode == 0)
                    {
                        Trc.Log(LogLevel.Info, "cdpcli {0} has succeeded.", args);
                        Registry.SetValue(
                            UnifiedSetupConstants.AgentRegistryName,
                            UnifiedSetupConstants.CdpcliRegStatusRegKeyName,
                            UnifiedSetupConstants.SuccessStatus);
                        retVal = true;
                        break;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Info, "cdpcli {0} has failed.", args);
                        Registry.SetValue(
                            UnifiedSetupConstants.AgentRegistryName,
                            UnifiedSetupConstants.CdpcliRegStatusRegKeyName,
                            UnifiedSetupConstants.FailedStatus);
                    }

                    //sleep 30 secs.
                    Thread.Sleep(30000);
                    Trc.Log(LogLevel.Info, "Re-trying the registration. Re-try count: {0}", i);
                }

                // Append cdpcli.log content.
                SetupReportData.AppendLogFileContent(cdpcliLogFile);

                return retVal;
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Error while registering CDPCLI : " + ex + "");
                return false;
            }
        }

        public static void HeapChange()
        {
            Trc.Log(LogLevel.Always, "Begin HeapChange");
            Trc.Log(LogLevel.Always, "Disabling 8.3 file name creation on NTFS partitions");
            string installationPath = GetAgentInstalledLocation();
            string fsUtilPath = Path.Combine(installationPath, "fsutil.exe");
            if (IntPtr.Size == 8)
            {
                IntPtr wow64Value = IntPtr.Zero;
                if (Wow64DisableWow64FsRedirection(ref wow64Value))
                {
                    ProcessStartInfo processStartInfo = new ProcessStartInfo(fsUtilPath, "behavior set disable8dot3 1");
                    processStartInfo.CreateNoWindow = true;
                    processStartInfo.WindowStyle = ProcessWindowStyle.Hidden;
                    processStartInfo.UseShellExecute = false;
                    Process.Start(processStartInfo).WaitForExit(); ;
                    Wow64RevertWow64FsRedirection(wow64Value);
                }
            }
            else
            {
                ProcessStartInfo processStartInfo = new ProcessStartInfo(fsUtilPath, "behavior set disable8dot3 1");
                processStartInfo.CreateNoWindow = true;
                processStartInfo.WindowStyle = ProcessWindowStyle.Hidden;
                processStartInfo.UseShellExecute = false;
                Process.Start(processStartInfo).WaitForExit();
            }

            Trc.Log(LogLevel.Always, "Changing the Heap value to 4096");
            HeapValueChange(-1, -1, 4096);
        }

        private static void HeapValueChange(int Firstalue, int SecondValue, int ThirdValue)
        {
            RegistryKey key = Registry.LocalMachine.OpenSubKey("System\\CurrentControlSet\\Control\\Session Manager\\SubSystems", true);
            key.SetValue("Windows", sharedvalue(Firstalue, SecondValue, ThirdValue, key.GetValue("Windows").ToString()));
        }

        private static string sharedvalue(int Firstalue, int SecondValue, int ThirdValue, string WindowsRegValue)
        {
            var result = Regex.Match(WindowsRegValue, @"SharedSection=(\d+),(\d+),(\d+)");
            var HeapValue = int.Parse(result.Groups[3].Value);

            Func<int, string, string> setValue = delegate(int ConversionInt, string ConversionStr)
            {
                return (ConversionInt == -1) ? ConversionStr : ConversionInt.ToString();
            };
            if (HeapValue < 4096)
            {
                Registry.SetValue(UnifiedSetupConstants.AgentRegistryName, UnifiedSetupConstants.RebootRequiredReg, UnifiedSetupConstants.Yes);
                return Regex.Replace(WindowsRegValue, @"SharedSection=(\d+),(\d+),(\d+)", delegate(Match m)
                {
                    return string.Format("SharedSection={0},{1},{2}", setValue(Firstalue, m.Groups[1].Value), setValue(SecondValue, m.Groups[2].Value), setValue(ThirdValue, m.Groups[3].Value));
                }, RegexOptions.IgnoreCase);
            }
            else
            {
                Registry.SetValue(UnifiedSetupConstants.AgentRegistryName, UnifiedSetupConstants.RebootRequiredReg, UnifiedSetupConstants.No);
                return WindowsRegValue;
            }
        }

        public static void StopCXServices()
        {
            try
            {
                Trc.Log(LogLevel.Always, "Begin StopCXServices");
                Trc.Log(LogLevel.Always, "Stopping services");
                string csInstallDirectory = GetCSInstalledLocation();
                string GUIDFile = csInstallDirectory + @"\etc\amethyst.conf";
                string CXType = GrepUtils.GetKeyValueFromFile(GUIDFile, "CX_TYPE");

                if (ServiceControlFunctions.IsInstalled("InMage PushInstall"))
                {
                    ServiceControlFunctions.StopService("InMage PushInstall");
                }
                if (CXType == "3")
                {
                    ProcessStartInfo processStartInfo = new ProcessStartInfo("cmd", "/C net stop WAS /Y");
                    processStartInfo.CreateNoWindow = true;
                    processStartInfo.WindowStyle = ProcessWindowStyle.Hidden;
                    processStartInfo.UseShellExecute = false;
                    Process.Start(processStartInfo);
                    ServiceControlFunctions.StopService("MySQL");
                }
                ServiceControlFunctions.StopService("tmansvc");

                if (ServiceControlFunctions.IsInstalled(
                    UnifiedSetupConstants.LogUploadServiceName))
                {
                    ServiceControlFunctions.StopService(
                        UnifiedSetupConstants.LogUploadServiceName);
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Error, "Exception Occured while StopCXServices: {0}", e.ToString());
            }
        }

        public static bool MergeDrscout()
        {
            Trc.Log(LogLevel.Info, "Begin MergeDrscout");

            string InstallDirectory = GetAgentInstalledLocation();
            string configMergeTool = InstallDirectory + "\\configmergetool.exe";
            string backupDrScout = InstallDirectory + "\\Application Data\\etc\\drscout_upgrade_backup.conf";
            string newDrScout = InstallDirectory + "\\Application Data\\etc\\drscout.conf.new";
            string tempDrScout = InstallDirectory + "\\Application Data\\etc\\drscout_temp.conf";
            string drScoutPath = InstallDirectory + "\\Application Data\\etc\\drscout.conf";

            string createNewFile = string.Format("--oldfilepath \"{0}\" --newfilepath \"{1}\" --operation merge", backupDrScout, newDrScout);
            string overwriteOldFile = string.Format("--oldfilepath \"{0}\" --overwritefilepath \"{1}\" --operation merge", backupDrScout, tempDrScout);

            int MergeReturnCode = CommandExecutor.RunCommand(configMergeTool, createNewFile);
            if (MergeReturnCode == 0)
            {
                Trc.Log(LogLevel.Info, "configmergetool with --oldfilepath, --newfilepath and --operation merge options executed successfully.");
            }
            else
            {
                Trc.Log(LogLevel.Info, "Unable to execute configmergetool with --oldfilepath, --newfilepath and --operation merge options.");
                return false;
            }

            System.Threading.Thread.Sleep(10000);

            MergeReturnCode = CommandExecutor.RunCommand(configMergeTool, overwriteOldFile);
            if (MergeReturnCode == 0)
            {
                Trc.Log(LogLevel.Info, "configmergetool with --oldfilepath, --overwritefilepath and --operation merge options executed successfully.");
            }
            else
            {
                Trc.Log(LogLevel.Info, "Unable to execute configmergetool with --oldfilepath, --overwritefilepath and --operation merge options.");
                return false;
            }

            // We don't make changes to the actual DRScout conf but instead to a backup one and then replace it to the original.
            File.Copy(backupDrScout, drScoutPath, true);

            return true;
        }

        public static void ConfigureAdmin(string installationDirectory)
        {
            Trc.Log(LogLevel.Always, "Begin ConfigureAdmin.");
            CommandExecutor.RunCommand(
                Path.Combine(installationDirectory, @"configureadmin.bat"),
                string.Format(
                    "\"{0}\" \"{1}\"",
                    Path.Combine(installationDirectory, @"FileRep\hostconfigwxcommon.exe"),
                    Path.Combine(installationDirectory, @"hostconfigwxcommon.exe")));
        }

        public static void ScoutTuning(string installationDirectory)
        {
            Trc.Log(LogLevel.Always, "Begin ScoutTuning.");
            string installedVersion = SetupHelper.GetAgentInstalledVersion().ToString();

            CommandExecutor.RunCommand(
                Path.Combine(installationDirectory, @"ScoutTuning.exe"),
                string.Format(
                    "-version {0} -role vconmt",
                    installedVersion));
        }

        public static void GenerateCert(string installationDirectory)
        {
			Trc.Log(LogLevel.Always, "Begin GenerateCert.");
            string output;
            CommandExecutor.ExecuteCommand(
                Path.Combine(installationDirectory, @"gencert.exe"),
                out output,
                "-n ps --dh");
            Trc.Log(LogLevel.Always, "Generate Cert output: {0}", output);
        }

        public static void InstallServiceCtrl(string installationDirectory)
        {
			Trc.Log(LogLevel.Always, "Begin InstallServiceCtrl.");
            string output;
            CommandExecutor.ExecuteCommand(
                Path.Combine(installationDirectory, @"ServiceCtrl.exe"),
                out output,
                "TrkWks stop force");
            Trc.Log(LogLevel.Always, "Install ServiceCtrl output: {0}", output);
        }

        public static void InstallVXFirewall(string installationDirectory)
        {
            Trc.Log(LogLevel.Always, "Begin InstallVXFirewall.");
            CommandExecutor.RunCommand(
                Path.Combine(installationDirectory, @"vxfirewall.bat"),
                "");
        }

        public static int InstallVSSProvider(string installationDirectory)
        {
            int returnCode = -1;
            try
            {
                Trc.Log(LogLevel.Always, "Begin InstallVSSProvider.");
                string output;
                returnCode = CommandExecutor.ExecuteCommand(
                    Path.Combine(installationDirectory, "InMageVSSProvider_Install.cmd"), out output);
                Trc.Log(LogLevel.Always, "InMageVSSProvider_Install.cmd output: {0}", output);

                if (returnCode != 0)
                {
                    Trc.Log(LogLevel.Error, "VSSProvider installation has failed with error code: {0}.", returnCode);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "VSSProvider installation completed successfully.");
                }
            }
            catch(Exception ex)
            {
                Trc.Log(LogLevel.Error, "VSS installation failed with exception " + ex);
            }

            return returnCode;
        }

        public static bool FrSvcSetOnDemand()
        {
            Trc.Log(LogLevel.Always, "Begin FrSvcSetOnDemand.");
            return ServiceControlFunctions.ChangeServiceStartupType(
                                "frsvc", 
                                NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START);
        }

        public static void StartCXServices()
        {
            try
            {
                Trc.Log(LogLevel.Always, "Begin StartCXServices");
                string csInstallDirectory = GetCSInstalledLocation();
                string GUIDFile = csInstallDirectory + @"\etc\amethyst.conf";
                string CXType = GrepUtils.GetKeyValueFromFile(GUIDFile, "CX_TYPE");

                if (ServiceControlFunctions.IsInstalled("InMage PushInstall"))
                {
                    ServiceControlFunctions.StartService("InMage PushInstall");
                }

                ServiceControlFunctions.StartService("tmansvc");
                if (CXType == "3")
                {
                    ProcessStartInfo processStartInfo = new ProcessStartInfo("cmd", "/C net start WAS /Y");
                    processStartInfo.CreateNoWindow = true;
                    processStartInfo.WindowStyle = ProcessWindowStyle.Hidden;
                    processStartInfo.UseShellExecute = false;
                    Process.Start(processStartInfo);
                    ServiceControlFunctions.StartService("W3SVC");
                    ServiceControlFunctions.StartService("MySQL");
                }

                if (ServiceControlFunctions.IsInstalled(
                    UnifiedSetupConstants.LogUploadServiceName))
                {
                    ServiceControlFunctions.StartService(
                        UnifiedSetupConstants.LogUploadServiceName);
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Error, "Exception Occured while StartCXServices: {0}", e.ToString());
            }
        }

        public static bool StartUAServices(bool isFreshInstall)
        {
            Trc.Log(LogLevel.Always,"Begin StartUAServices");

            string uaInstallDirectory = GetAgentInstalledLocation();
            string ConfFile = uaInstallDirectory + @"\Application Data\etc\drscout.conf";
            string RoleType = GrepUtils.GetKeyValueFromFile(ConfFile, "Role");

            string RebootRequired = (string)Registry.GetValue(UnifiedSetupConstants.AgentRegistryName, UnifiedSetupConstants.RebootRequiredReg, null);
            if ((RebootRequired == UnifiedSetupConstants.Yes) && (RoleType == "Agent") && isFreshInstall)
            {
                Trc.Log(LogLevel.Always, "Reboot is required for Agent installation. Not starting the services.");
                return true;
            }
            else
            {
                Trc.Log(LogLevel.Always,"Starting the services.");

                if (ServiceControlFunctions.IsInstalled("InMage Scout Application Service"))
                {
                    Trc.Log(LogLevel.Always, "InMage Scout Application Service is installed. Statring InMage Scout Application Service.");
                    if (ServiceControlFunctions.StartService("InMage Scout Application Service"))
                    {
                        Trc.Log(LogLevel.Always, "InMage Scout Application Service started successfully.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Unable to start InMage Scout Application Service.");
                        SetupHelper.ShutdownInMageAgentServices();
                        return false;
                    }
                }
                else
                {
                    Trc.Log(LogLevel.Always, "InMage Scout Application Service is not installed.");
                }                

                if (ServiceControlFunctions.IsInstalled("svagents"))
                {
                    Trc.Log(LogLevel.Always, "svagents service is installed. Statring svagents service.");

                    if (ServiceControlFunctions.StartService("svagents"))
                    {
                        Trc.Log(LogLevel.Always, "svagents service started successfully.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Unable to start svagents service.");
                        SetupHelper.ShutdownInMageAgentServices();
                        return false;
                    }
                }
                else
                {
                    Trc.Log(LogLevel.Always, "svagents service is not installed.");
                }

                return true;
            }
        }

        /// <summary>
        /// Executes InDskFlt driver manifest file.
        /// </summary>
        /// <returns>True if manifest execution succeeded, false otherwise.</returns>
        public static bool InvokeIndskfltManifest()
        {
            Trc.Log(LogLevel.Always, "Begin InvokeIndskfltManifest");

            string InstallDirectory = GetAgentInstalledLocation();
            string SystemPath = Environment.GetFolderPath(Environment.SpecialFolder.System);
            string WevtutilFile = SystemPath + "\\wevtutil.exe";
            string output = "";

            string manifestArgs = string.Format("im \"{0}\"", Path.Combine(InstallDirectory, "DiskFltEvents.man"));

            Trc.Log(LogLevel.Always, "Installing InDskFlt manifest file.");
            int ReturnCode = CommandExecutor.RunCommand(WevtutilFile, manifestArgs, out output);

            Trc.Log(LogLevel.Always, "InDskFlt manifest installation output.");
            Trc.Log(LogLevel.Always, "---------------------------------------");
            Trc.Log(LogLevel.Always, output);
            Trc.Log(LogLevel.Always, "---------------------------------------");

            Trc.Log(LogLevel.Always, "InDskFlt manifest installation Return Code:" + ReturnCode);
            if (ReturnCode == 0)
            {
                Trc.Log(LogLevel.Always, "InDskFlt manifest installation executed successfully.");
            }
            else
            {
                Trc.Log(LogLevel.Always, "InDskFlt manifest installation failed with error:" + ReturnCode);
                return false;
            }

            const string LogName = "Microsoft-InMage-InDiskFlt/telemetryOperational";
            const int MaxLogSize = 1 * 1024 * 1024; // 1 MB

            output = "";

            Trc.Log(LogLevel.Always, "Setting InDskFlt telemetry channel max log size to {0}.", MaxLogSize);
            string telemetryChannelSetArgs = string.Format("sl {0} /maxsize:{1}", LogName, MaxLogSize);
            ReturnCode = CommandExecutor.RunCommand(WevtutilFile, telemetryChannelSetArgs, out output);

            Trc.Log(LogLevel.Always, "-----------------------------------------------------------------------------------------------------");
            Trc.Log(LogLevel.Always, output);
            Trc.Log(LogLevel.Always, "-----------------------------------------------------------------------------------------------------");

            Trc.Log(LogLevel.Always, "Setting InDskFlt telemetry channel limit parameters Return Code:" + ReturnCode);
            if (ReturnCode == 0)
            {
                Trc.Log(LogLevel.Always, "Setting InDskFlt telemetry channel limit parameters executed successfully.");
            }
            else
            {
                Trc.Log(LogLevel.Always, "Setting InDskFlt telemetry channel limit parameters failed with error:" + ReturnCode);
                // Ignore the failure of setting the telemetry channel parameters, since this could only cause some telemetry
                // events dropped/incomplete in corner cases, say when service stops, system restarts, etc.
            }

            return true;
        }

        public static bool StartInDskFltService()
        {
            Trc.Log(LogLevel.Always, "Starting the InDskFlt service");

            if (ServiceControlFunctions.StartService(serviceName: "InDskFlt"))
            {
                Trc.Log(LogLevel.Always, "InDskFlt service is started successfully");
                return true;
            }

            Trc.Log(LogLevel.Always, "Unable to start InDskFlt service");
            return false;
        }

        public static bool InstallDriver()
        {
            Trc.Log(LogLevel.Debug, "Begin InstallDriver");
            try
            {
                Trc.Log(LogLevel.Always, "Installing InDskFlt service");

                string InstallDirectory = GetAgentInstalledLocation();
                string InDskFltFile = Path.Combine(Environment.GetEnvironmentVariable("windir"),
                                                    @"System32\drivers\InDskFlt.sys");

                if (!File.Exists(InDskFltFile))
                {
                    Trc.Log(LogLevel.Error, "File {0} is not copied. Skipping driver installation", InDskFltFile);
                    return false;
                }

                //Install InDskFlt service
                bool isInstalled = ServiceControlFunctions.InstallDriver(
                                            driverName: UnifiedSetupConstants.IndskFltDriverName,
                                            displayName: UnifiedSetupConstants.IndskFltDisplayName,
                                            serviceBootFlag: NativeMethods.SERVICE_START_TYPE.SERVICE_BOOT_START,
                                            fileName: InDskFltFile,
                                            loadOrderGroup: "Pnp Filter"
                                            );

                if (!isInstalled)
                {
                    Trc.Log(LogLevel.Always, "Unable to install InDskFlt service");
                    return false;
                }

            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Exception{0} encountered while installing driver", ex);
                return false;
            }

            Trc.Log(LogLevel.Info, "indskflt driver installed successfully");
            return true;
        }
        /// <summary>
        /// This function loads indskflt driver and starts it
        /// It executes drvuitl with stop filtering all option to check
        /// if disks are enumerated.
        /// returns: OperationResult
        /// </summary>
        public static OperationResult LoadIndskFltDriver()
        {
            Trc.Log(LogLevel.Debug, "Begin LoadIndskFltDriver");
            OperationResult operationResult = new OperationResult();

            try
            {
                bool isInstalled = ServiceControlFunctions.IsInstalled("indskflt");
                if (!isInstalled)
                {
                    if (!InstallDriver())
                    {
                        Trc.Log(LogLevel.Error, "Failed to install indskflt driver");
                        operationResult.Error = StringResources.FailedToInstallAndConfigureServices;
                        operationResult.Status = OperationStatus.Failed;
                        operationResult.ProcessExitCode = UASetupReturnValues.FailedToInstallAndConfigureServices;
                        return operationResult;
                    }
                }

                Trc.Log(LogLevel.Always, "Starting InDskflt driver");
                if (!StartInDskFltService() ||
                    SetupHelper.IsRebootRequiredPostDiskFilterInstall())
                {
                    Trc.Log(LogLevel.Always, "Reboot is required for driver changes to take effect");
                    operationResult.Error = StringResources.PostInstallationRebootRequired;
                    operationResult.Status = OperationStatus.Failed;
                    operationResult.ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors;

                    ErrorLogger.LogInstallerError(
                    new InstallerException(
                        UASetupErrorCode.ASRMobilityServiceInstallerSuccessfulMandatoryReboot.ToString(),
                        null,
                        UASetupErrorCodeMessage.SuccessfulMandatoryReboot
                    ));

                    return operationResult;
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "LoadIndskFltDriver Exception encounter: {0}", ex);
                operationResult.Error = StringResources.FailedToInstallAndConfigureServices;
                operationResult.Status = OperationStatus.Failed;
                operationResult.ProcessExitCode = UASetupReturnValues.FailedToInstallAndConfigureServices;
            }

            return operationResult;
        }

        public static bool PermissionChanges(out string permissionFiles, out string returnValue)
        {
            Trc.Log(LogLevel.Always, "Begin PermissionChanges");
            string InstallDirectory = GetAgentInstalledLocation(); ;
            Trc.Log(LogLevel.Always, "Setting ACLs on " + InstallDirectory + " directory");
            permissionFiles = string.Empty;
            if (SetFolderPermissions(InstallDirectory, out returnValue))
            {
                Trc.Log(LogLevel.Always, "Successfully set ACLs on " + InstallDirectory + " directory");
            }
            else
            {
                Trc.Log(LogLevel.Always, "Unable to set ACLs on " + InstallDirectory + " directory");
                permissionFiles = InstallDirectory;
                return false;
            }

            if (IsCXInstalled())
            {
                Trc.Log(LogLevel.Always, "Configuration/Process Server is already installed on this machine. Not setting ACLs on private Directory as Configuration/Process Server installation has already set the ACLs on private directory.");
            }
            else
            {
                Trc.Log(LogLevel.Always, "Configuration/Process Server is not installed on this machine.");
                string programDataPath = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
                string privateDirectory = programDataPath + "\\Microsoft Azure Site Recovery\\private";
                Trc.Log(LogLevel.Always, "Setting ACLs on " + privateDirectory + " directory");
                if (SetFolderPermissions(privateDirectory, out returnValue))
                {
                    Trc.Log(LogLevel.Always, "Successfully set ACLs on " + privateDirectory + " directory");
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Unable to set ACLs on " + privateDirectory + " directory");
                    permissionFiles = privateDirectory;
                    return false;
                }
            }

            return true;
        }

        private static bool SetFolderPermissions(string path, out string ReturnCode)
        {
            ReturnCode = string.Empty;
            try
            {
                if (Directory.Exists(path))
                {
                    Trc.Log(LogLevel.Always, path + " directory is available. Setting ACLs.");
                    var security = new DirectorySecurity();
                    security.AddAccessRule(
                        new FileSystemAccessRule(
                            new System.Security.Principal.SecurityIdentifier("S-1-5-32-544"),               // sid for account "Administrators"
                            FileSystemRights.FullControl,
                            InheritanceFlags.ContainerInherit | InheritanceFlags.ObjectInherit,
                            PropagationFlags.None,
                            AccessControlType.Allow));
                    security.AddAccessRule(
                        new FileSystemAccessRule(
                            new System.Security.Principal.SecurityIdentifier("S-1-5-18"),                   // sid for account "Local System"
                            FileSystemRights.FullControl,
                            InheritanceFlags.ContainerInherit | InheritanceFlags.ObjectInherit,
                            PropagationFlags.None,
                            AccessControlType.Allow));
                    security.SetAccessRuleProtection(isProtected: true, preserveInheritance: false);
                    Directory.SetAccessControl(path, security);
                    return true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, path + " directory is not available.");
                    ReturnCode = "Directory not found";
                    return false;
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Always, "Failed to set ACLs on " + path + " directory. Exception - " + ex.ToString());
                ReturnCode = ex.GetType().ToString();
                return false;
            }
        }

        public static bool CreateAPPConf()
        {
            try
            {
                Trc.Log(LogLevel.Always, "Begin CreateAPPConf.");
                string installDir = GetAgentInstalledLocation();
                string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);
                string configPath = sysDrive + "ProgramData\\Microsoft Azure Site Recovery\\Config";
                string appConf = configPath + "\\App.conf";

                if (File.Exists(appConf))
                {
                    Trc.Log(LogLevel.Always, "App.conf file already exists. Overwriting installation directory content.");
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Creating App.conf and appending installation directory content into it.");
                }

                string reqString = installDir.Remove(installDir.LastIndexOf(@"\"), 6);
                Trc.Log(LogLevel.Always, "reqString = " + reqString + "");
                string line = @"INSTALLATION_PATH = """ + reqString + "\"";
                Directory.CreateDirectory(configPath);
                File.WriteAllText(appConf, line);
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Error: " + ex + "");
                return false;
            }

            return true;
        }

        public static bool BackupDrscout()
        {
            Trc.Log(LogLevel.Debug, "Entering BackupDrscout");
            try
            {
                Trc.Log(LogLevel.Always, "Begin BackupDrscout");
                string InstallDirectory = GetAgentInstalledLocation();
                string sourceFile = Path.Combine(InstallDirectory, @"Application Data\etc\drscout.conf");
                string destFile = Path.Combine(InstallDirectory, @"Application Data\etc\drscout_upgrade_backup.conf");

                Trc.Log(LogLevel.Debug, "Backing up source file {0} to {1}", sourceFile, destFile);

                try
                {
                    File.Delete(destFile);
                }
                catch(DirectoryNotFoundException)
                {
                    Trc.Log(LogLevel.Debug, "File {0} doesnt exist", destFile);
                }
                File.Copy(sourceFile, destFile, true);
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "BackupDrscout encountered Exception: {0}", ex);
                return false;
            }

            return true;
        }

        /// <summary>
        /// Copying config file from source to destination path.
        /// </summary>
        /// <returns>true if copy succeeds, false otherwise</returns>
        public static bool AgentConfigCopy(string sourceConfPath, string destConfPath)
        {
            try
            {
                Trc.Log(LogLevel.Always, "Copying {0} as {1}", sourceConfPath, destConfPath);
                string installDir = (string)Registry.GetValue(
                    UnifiedSetupConstants.AgentRegistryName,
                    UnifiedSetupConstants.InstDirRegKeyName,
                    null);
                string sourceFile = Path.Combine(installDir, sourceConfPath);
                string destFile = Path.Combine(installDir, destConfPath);
                File.Delete(destFile);
                File.Copy(sourceFile, destFile, true);
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Always, "Error: " + ex + "");
                return false;
            }

            return true;
        }

        //Replace RUNTIME_INSTALL_PATH string with Install directory
        public static void ReplaceRuntimeInstallPath()
        {
            Trc.Log(LogLevel.Always, "Begin ReplaceRuntimeInstallPath");
            //Read InstallDirectory from registry
            string InstallDirectory = GetAgentInstalledLocation();
            string FileName = InstallDirectory + "\\Application Data\\etc\\drscout.conf";
            string InitialString = "RUNTIME_INSTALL_PATH";

            Trc.Log(LogLevel.Always, "Replacing RUNTIME_INSTALL_PATH with Install directory : {0}", InstallDirectory);
            StringReplace(FileName, InitialString, InstallDirectory);
        }

        private static void StringReplace(string FileName, string InitialString, string ReplaceString)
        {
            //Read all conetent from filename
            string text = File.ReadAllText(FileName);
            //Replaces strings
            text = text.Replace(InitialString, ReplaceString);
            //Write replaced content to file
            File.WriteAllText(FileName, text);
        }

        /// <summary>
        /// Check for UpperFilter registry key.
        /// </summary>
        /// <returns>true if UpperFilters registry key is 
        /// updated with InDskFlt value, false otherwise</returns>
        public static bool CheckUpperFilterRegistryKeyValue()
        {
            RegistryKey UpperFiltersKey = 
                Registry.LocalMachine.OpenSubKey(
                    UnifiedSetupConstants.UpperFilterRegHive);

            string[] UpperFilters =
                (string[])UpperFiltersKey.GetValue(
                    UnifiedSetupConstants.UpperFilterRegKey);
            if (UpperFilters == null || UpperFilters.Length == 0)
            {
                Trc.Log(LogLevel.Error, 
                    "UpperFilters registry key has empty value.");
                return false;
            }
            else
            {
                // Validates UpperFilters first element.
                if (UpperFilters.First().Equals(
                    UnifiedSetupConstants.InDskFltName))
                {
                    Trc.Log(LogLevel.Always, 
                        "UpperFilters registry key is updated with InDskFlt value.");
                }
                else
                {
                    Trc.Log(LogLevel.Error, 
                        "UpperFilters registry key is not updated with InDskFlt value.");
                    return false;
                }
            }
            return true;
        }

        /// <summary>
        /// Add InDskFlt value to UpperFilter registry key.
        /// </summary>
        public static void AddInDskFltToUpperFilterRegistry()
        {
            RegistryKey UpperFiltersKey = 
                Registry.LocalMachine.OpenSubKey(
                    UnifiedSetupConstants.UpperFilterRegHive, 
                    true);

            string[] UpperFilters = 
                (string[])UpperFiltersKey.GetValue(
                    UnifiedSetupConstants.UpperFilterRegKey);
            Trc.Log(LogLevel.Always, 
                "UpperFilters registry key value:");
            foreach (string val in UpperFilters)
            {
                Trc.Log(LogLevel.Always, 
                    "{0}", val);
            }

            string[] Indskflt = 
                { UnifiedSetupConstants.InDskFltName };
            string[] UpdatedUpperFilters = 
                new string[Indskflt.Length + UpperFilters.Length];
            Indskflt.CopyTo(
                UpdatedUpperFilters, 
                0);
            UpperFilters.CopyTo(
                UpdatedUpperFilters, 
                Indskflt.Length);

            Trc.Log(LogLevel.Always, 
                "UpperFilters registry key new value:");
            foreach (string val in UpdatedUpperFilters)
            {
                Trc.Log(LogLevel.Always, 
                    "{0}", val);
            }

            UpperFiltersKey.SetValue(
                UnifiedSetupConstants.UpperFilterRegKey, 
                UpdatedUpperFilters);
        }
    }
}