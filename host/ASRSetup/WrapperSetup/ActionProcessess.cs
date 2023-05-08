
using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Configuration;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Management;
using System.Management.Automation;
using System.Management.Automation.Runspaces;
using System.Net;
using System.Net.Security;
using System.Net.Sockets;
using System.Security;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows;
using ASRResources;
using ASRSetupFramework;
using IWshRuntimeLibrary;
using InMage.APIHelpers;
using InMage.APIModel;
using Microsoft.DisasterRecovery.IntegrityCheck;
using Microsoft.Win32;
using DefaultPowerShell = System.Management.Automation.PowerShell;
using DRSetup = Microsoft.DisasterRecovery.SetupFramework;

namespace UnifiedSetup
{
    /// <summary>
    /// Class for installation/registration actions.
    /// </summary>
    class InstallActionProcessess
    {
        #region Fields
        /// <summary>
        /// Stores the system directory.
        /// </summary>
        public static string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);

        #endregion

        #region Enums
        /// <summary>
        /// Build version installed on the server.
        /// </summary>
        public enum InstalledBuildVersion
        {
            /// <summary>
            /// Older version build
            /// </summary>
            Older,

            /// <summary>
            /// Same build
            /// </summary>
            Same,

            /// <summary>
            /// Higher version build
            /// </summary>
            Higher,

            /// <summary>
            /// None
            /// </summary>
            None,

            /// <summary>
            /// Skip
            /// </summary>
            Skip,

            /// <summary>
            /// Unsupported
            /// </summary>
            Unsupported
        }
        #endregion

        #region IISInstallationMethod
        /// <summary>
        /// Install Internet Information Services(IIS).
        /// </summary>
        /// <returns>Exit code</returns>
        public static int InstallIIS()
        {
            int returnCode = -1;
            try
            {
                //IIS features names
                var featureNames = new[]
                {
                    "PowerShell-ISE,",
                    "WAS,",
                    "WAS-Process-Model,",
                    "WAS-Config-APIs,",
                    "Web-Server,",
                    "Web-WebServer,",
                    "Web-Mgmt-Service,",
                    "Web-Request-Monitor,",
                    "Web-Common-Http,",
                    "Web-Static-Content,",
                    "Web-Default-Doc,",
                    "Web-Dir-Browsing,",
                    "Web-Http-Errors,",
                    "Web-App-Dev,",
                    "Web-CGI,",
                    "Web-Health,",
                    "Web-Http-Logging,",
                    "Web-Log-Libraries,",
                    "Web-Security,",
                    "Web-Filtering,",
                    "Web-Performance,",
                    "Web-Stat-Compression,",
                    "Web-Mgmt-Tools,",
                    "Web-Mgmt-Console,",
                    "Web-Scripting-Tools,",
                    "Web-Asp-Net45,",
                    "Web-Net-Ext45,",
                    "Web-Http-Redirect,",
                    "Web-Windows-Auth,",
                    "Web-Url-Auth",
                };

                ArrayList powershellErrors;
                string errorMessage = string.Empty;
                string successValue = string.Empty;
                string restartNeededValue = string.Empty;

                string psScript = string.Format(
                    "Add-WindowsFeature {0}",
                    string.Join(
                    " ",
                    featureNames.Select(name => string.Format("{0}", name))));

                Collection<PSObject> installIIS = DRSetup.SetupHelper.ExecuteScript(
                    new string[] { UnifiedSetupConstants.ServerManagerPowershellModule },
                    psScript,
                    out powershellErrors);

                if (powershellErrors != null && powershellErrors.Count > 0)
                {
                    foreach (object errors in powershellErrors)
                    {
                        errorMessage = string.Format(
                            "{0}{1}{2}",
                            errorMessage,
                            Environment.NewLine,
                            errors.ToString());
                    }

                    Trc.Log(
                        LogLevel.Error,
                        "Setup is unable to install IIS. {0}",
                        errorMessage);

                    Registry.SetValue(
                            UnifiedSetupConstants.InstalledProductsRegName,
                            UnifiedSetupConstants.IISInstallationStatusRegKeyName,
                            UnifiedSetupConstants.FailedStatus);
                }
                else
                {
                    foreach (PSObject result in installIIS)
                    {
                        successValue = result.Properties["Success"].Value.ToString();
                        restartNeededValue = result.Properties["RestartNeeded"].Value.ToString();
                    }

                    if ((successValue == "True") && (restartNeededValue == "No"))
                    {
                        Trc.Log(LogLevel.Always, "IIS installation completed successfully.");
                        Registry.SetValue(
                            UnifiedSetupConstants.InstalledProductsRegName,
                            UnifiedSetupConstants.IISInstallationStatusRegKeyName,
                            UnifiedSetupConstants.SuccessStatus);
                        returnCode = 0;
                    }
                    else if ((successValue == "True") && (restartNeededValue == "Yes"))
                    {
                        Trc.Log(LogLevel.Always, "Internet Information Services installation requires the computer to be rebooted. Reboot machine and try install again.");
                        Registry.SetValue(
                            UnifiedSetupConstants.InstalledProductsRegName,
                            UnifiedSetupConstants.IISInstallationStatusRegKeyName,
                            UnifiedSetupConstants.FailedStatus);
                        returnCode = 3010;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "IIS installation failed.");
                        Registry.SetValue(
                            UnifiedSetupConstants.InstalledProductsRegName,
                            UnifiedSetupConstants.IISInstallationStatusRegKeyName,
                            UnifiedSetupConstants.FailedStatus);
                    }
                }

                // Record operation status to summary log.
                RecordOperation(OperationName.InstallIIS, returnCode);

                return returnCode;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while installing Internet Information Services(IIS): {0}",
                    ex.ToString());
                Registry.SetValue(
                            UnifiedSetupConstants.InstalledProductsRegName,
                            UnifiedSetupConstants.IISInstallationStatusRegKeyName,
                            UnifiedSetupConstants.FailedStatus);

                // Record operation status to summary log.
                RecordOperation(OperationName.InstallIIS, returnCode, "", ex.ToString());

                return returnCode;
            }
            finally
            {
                // Set IIS install status to false if exit code is non-zero.
                if (returnCode == 0)
                {
                    SetupParameters.Instance().IsIISInstall = true;
                }
                else
                {
                    SetupParameters.Instance().IsIISInstall = false;
                }
            }
        }
        #endregion

       	#region CXTPInstallationMethod
        /// <summary>
        /// Install/Upgrade Configuration/Process Server dependencies.
        /// </summary>
        /// <returns>Exit code</returns>
        public static int InstallCXTP()
        {
            int returnCode = -1;
            try
            {
                returnCode =
                    CommandExecutor.RunCommand(
                    UnifiedSetupConstants.CXTPExeName,
                    SetupParameters.Instance().CommonParameters);

                // Record operation status to summary log.
                RecordOperation(OperationName.InstallTp, returnCode);

                return returnCode;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while installing Configuration/Process Server dependencies: {0}",
                    ex.ToString());
                // Record operation status to summary log.
                RecordOperation(OperationName.InstallTp, returnCode);

                return returnCode;
            }
            finally
            {
                // Set registration status to false if exit code is non-zero.
                if (returnCode == 0)
                {
                    SetupParameters.Instance().IsCXTPInstalled = true;
                }
                else
                {
                    SetupParameters.Instance().IsCXTPInstalled = false;
                }
            }
        }

        /// <summary>
        /// Creates machine identifier which is an idempotent call.
        /// </summary>
        public static void CreateMachineIdentifier()
        {
            RegistryKey key = null;

            try
            {
                Guid machineIdentifier = Guid.Empty;

                // Open Inmage hive in write mode
                key = Registry.LocalMachine.OpenSubKey(UnifiedSetupConstants.InmageRegistryHive, true);

                // If hive found then try to get value else create the key.
                if (key != null)
                {
                    string machineIdentifierStr = key.GetValue(
                        UnifiedSetupConstants.MachineIdentifierRegKeyName,
                        string.Empty).ToString();
                    Guid.TryParse(machineIdentifierStr, out machineIdentifier);
                }
                else
                {
                    key = Registry.LocalMachine.CreateSubKey(UnifiedSetupConstants.InmageRegistryHive);
                }

                // If value not found/empty, generate it.
                if (machineIdentifier == Guid.Empty)
                {
                    machineIdentifier = Guid.NewGuid();
                    key.SetValue(UnifiedSetupConstants.MachineIdentifierRegKeyName, machineIdentifier);

                    Trc.Log(LogLevel.Always, "Created machine identifier as {0}.", machineIdentifier);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Found machine identifier as {0}.", machineIdentifier);
                }

                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.MachineIdentifier,
                    machineIdentifier);
            }
            catch (Exception e)
            {
                Trc.LogException(LogLevel.Error, "Create machine identifier failed with exception", e);
                throw;
            }
            finally
            {
                if (key != null)
                {
                    key.Close();
                }
            }
        }
        #endregion

        #region CXInstallationMethods
        /// <summary>
        /// Install Configuration/Process Server.
        /// </summary>
        /// <returns>Exit code</returns>
        public static int InstallCX()
        {
            string CommandLineParameters = "";
            string psIPOption = "";
            string azureIPOption = "";
            int returnCode = -1;
            string privateEndpointState = string.Empty;

            try
            {
                // PSIP is an optional parameter in silent installation.
                if (!string.IsNullOrEmpty(SetupParameters.Instance().PSIP))
                {
                    psIPOption = " /PSIP " + "\"" + SetupParameters.Instance().PSIP + "\"";
                }

                // AZUREIP is an optional parameter in silent installation.
                if (!string.IsNullOrEmpty(SetupParameters.Instance().AZUREIP))
                {
                    azureIPOption = " /AZUREIP " + "\"" + SetupParameters.Instance().AZUREIP + "\"";
                }

                // Construct command line parameters.
                if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                {
                    CommandLineParameters = SetupParameters.Instance().CommonParameters +
                                                   " /ServerMode " + "\"" + SetupParameters.Instance().ServerMode + "\"" +
                                                   " /DIR=" + "\"" + SetupParameters.Instance().CXInstallDir + "\"" +
                                                   " /MySQLPasswordFile " + "\"" + SetupParameters.Instance().MysqlCredsFilePath + "\"" +
                                                   " /EnvType " + "\"" + SetupParameters.Instance().EnvType + "\"" +
                                                   " /DataTransferSecurePort " + "\"" + SetupParameters.Instance().DataTransferSecurePort + "\"" +
                                                   " /NORESTART" +
                                                   psIPOption +
                                                   azureIPOption;
                }
                else if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
                {
                    CommandLineParameters = SetupParameters.Instance().CommonParameters +
                                                   " /ServerMode " + "\"" + SetupParameters.Instance().ServerMode + "\"" +
                                                   " /DIR=" + "\"" + SetupParameters.Instance().CXInstallDir + "\"" +
                                                   " /CSIP " + "\"" + SetupParameters.Instance().CSIP + "\"" +
                                                   " /CSPort " + "\"" + SetupParameters.Instance().CSPort + "\"" +
                                                   " /PassphrasePath " + "\"" + SetupParameters.Instance().PassphraseFilePath + "\"" +
                                                   " /EnvType " + "\"" + SetupParameters.Instance().EnvType + "\"" +
                                                   " /DataTransferSecurePort " + "\"" + SetupParameters.Instance().DataTransferSecurePort + "\"" +
                                                   " /NORESTART" +
                                                   psIPOption;
                }

                CommandLineParameters = CommandLineParameters + CSPSAdditionalCommandLineParams();

                // Invoke CX installer with the command line parameters.
                returnCode = 
                    CommandExecutor.RunCommand(
                    UnifiedSetupConstants.CSPSExeName,
                    CommandLineParameters);


                if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                {
                    // Return code from CS installer cannot determine whether its a failure of CS / PS.
                    // So assigning the same return code to CS and PS operation summary logs.
                    // Record operation status of CS to summary log.
                    RecordOperation(OperationName.InstallCs, returnCode);

                    if (returnCode == 0)
                    {
                        // Record operation status of PS to summary log.
                        RecordOperation(OperationName.InstallPs, returnCode);
                    }
                }
                else
                {
                    // Record operation status of PS to summary log.
                    RecordOperation(OperationName.InstallPs, returnCode);
                }


                if (returnCode == 0)
                {
                    // Create Cspsconfigtool desktop shortcut.
                    CreateCspsConfigtoolShortcut();

                    // Adding vault location information to registry.
                    string vaultLocation =
                        DRSetup.PropertyBagDictionary.Instance.GetProperty<string>(
                        ASRSetupFramework.PropertyBagConstants.VaultLocation);
                    if (!string.IsNullOrEmpty(vaultLocation))
                    {
                        Trc.Log(LogLevel.Always, "Adding vaultLocation: {0} to registry.", vaultLocation);
                        Registry.SetValue(
                            UnifiedSetupConstants.CSPSRegistryName,
                            UnifiedSetupConstants.VaultGeoRegKeyName,
                            vaultLocation);
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Vault location is empty.");
                    }

                    if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                    {
                        privateEndpointState =
                            DRSetup.PropertyBagDictionary.Instance.GetProperty<string>(
                                DRSetup.PropertyBagConstants.PrivateEndpointState);
                    }
                    else if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
                    {
                        privateEndpointState =
                            DRSetup.PropertyBagDictionary.Instance.GetProperty<string>(
                                ASRSetupFramework.PropertyBagConstants.PrivateEndpointState);
                    }

                    Registry.SetValue(
                        UnifiedSetupConstants.CSPSRegistryName,
                        UnifiedSetupConstants.PrivateEndpointState,
                        privateEndpointState ?? DRSetup.DRSetupConstants.PrivateEndpointState.None.ToString());


                    // Adding Proxy type to registry.
                    Trc.Log(LogLevel.Always,
                        "Adding proxy type: {0} to registry.", SetupParameters.Instance().ProxyType);
                    Registry.SetValue(
                        UnifiedSetupConstants.CSPSRegistryName,
                        UnifiedSetupConstants.ProxyRegKeyName,
                        SetupParameters.Instance().ProxyType);
                }

                return returnCode;
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception occurred while installing Configuration/Process Server: {0}", ex.ToString());
                return returnCode;
            }
            finally
            {
                // Set install status to false if exit code is non-zero.
                if (returnCode == 0)
                {
                    SetupParameters.Instance().IsCSInstalled = true;
                }
                else
                {
                    SetupParameters.Instance().IsCSInstalled = false;
                }
            }
        }

        /// <summary>
        /// Upgrades Configuration/Process Server component
        /// </summary>
        /// <returns>Exit code</returns>
        public static int UpgradeCX()
        {
            string CommandLineParameters = "";
            int returnCode = -1;

            try
            {
                // Construct command line parameters.
                CommandLineParameters = SetupParameters.Instance().CommonParameters + " /UPGRADE";
                CommandLineParameters = CommandLineParameters + CSPSAdditionalCommandLineParams();

                // Invoke CX installer with the command line parameters.
                returnCode =
                    CommandExecutor.RunCommand(
                    UnifiedSetupConstants.CSPSExeName,
                    CommandLineParameters);

                // Re-try installation if CS installer returns 5 exit code.
                // 5 exit code referes to some files in use.
                if (returnCode == 5)
                {
                    Trc.Log(LogLevel.Always, "Sleeping for 2 minutes before retrying installation.");
                    Thread.Sleep(120000);
                    returnCode =
                        CommandExecutor.RunCommand(
                        UnifiedSetupConstants.CSPSExeName,
                        CommandLineParameters);
                }

                if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                {
                    // Record operation status of CS to summary log.
                    RecordOperation(OperationName.InstallCs, returnCode);

                    if (returnCode == 0)
                    {
                        // Record operation status of PS to summary log.
                        RecordOperation(OperationName.InstallPs, returnCode);

                        // Create Cspsconfigtool desktop shortcut.
                        CreateCspsConfigtoolShortcut();
                    }
                }
                else
                {
                    // Record operation status of PS to summary log.
                    RecordOperation(OperationName.InstallPs, returnCode);
                }
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while upgrading Configuration/Process Server: {0}", ex.ToString());
                // Record operation status to summary log.
                RecordOperation(OperationName.InstallCs, returnCode);
            }
            finally
            {
                // Set install status to false if exit code is non-zero.
                SetupParameters.Instance().IsCSInstalled =
                    (returnCode == 0) ? true : false;

                // Delete CSPSConfigTool shortcut on OVF VMs.
                if(SetupParameters.Instance().OVFImage)
                {
                    DeleteShortCut(UnifiedSetupConstants.CSPSConfigtoolShortcutName);
                }
            }

            return returnCode;
        }

        /// <summary>
        /// Additional CommandLine params to Configuration/process server install/upgrade.
        /// </summary>
        private static string CSPSAdditionalCommandLineParams()
        {
            string CommandLineParams = string.Empty;
            string msVC2015Version = string.Empty;
            string msVC2013Version = string.Empty;

            // Skip Microsoft Visual C++ 2015 Redistributable (x86) installation. 
            if (ValidationHelper.IsSoftwareInstalledWow6432(
                UnifiedSetupConstants.MSVC2015Redistributable,
                out msVC2015Version))
            {
                CommandLineParams = CommandLineParams +
                    UnifiedSetupConstants.SkipMSVC2015Installation;
            }

            // Skip Microsoft Visual C++ 2013 Redistributable (x86) installation.
            if (ValidationHelper.IsSoftwareInstalledWow6432(
                UnifiedSetupConstants.MSVC2013Redistributable,
                out msVC2013Version))
            {
                CommandLineParams = CommandLineParams +
                    UnifiedSetupConstants.SkipMSVC2013Installation;
            }

            return CommandLineParams;
        }

        #endregion

        #region DRAInstallationMethod
        /// <summary>
        /// Install/Upgrade DRA.
        /// </summary>
        /// <returns>Exit code</returns>
        public static int InstallDRA()
        {
            int draReturnCode = -1;
            string output = string.Empty; ;
            string errorMsg = string.Empty; ;
            try
            {
                string draDestFolder =
                    Path.Combine(sysDrive, UnifiedSetupConstants.ExtractDRAFolder);
                
                // Delete {system drive}\Temp\DRASETUP folder if it exists already.
                if (Directory.Exists(draDestFolder))
                {
                    Trc.Log(
                        LogLevel.Always,
                        "Deleting {0} directory as it already exists in the setup.", draDestFolder);
                    Directory.Delete(draDestFolder, true);
                }

                Trc.Log(LogLevel.Always, "Creating {0} directory.", draDestFolder);
                Directory.CreateDirectory(draDestFolder);

                // copy DRA build to {system drive}\Temp\DRASETUP.
                string draDestFile =
                    Path.Combine(draDestFolder, UnifiedSetupConstants.DRAExeName);
                string currDir =
                    new System.IO.FileInfo(System.Reflection.Assembly.GetExecutingAssembly().Location).DirectoryName;
                string sourceFile = 
                    Path.Combine(currDir, UnifiedSetupConstants.DRAExeName);
                Trc.Log(LogLevel.Always, "Copying {0} to {1}.", sourceFile, draDestFile);
                System.IO.File.Copy(sourceFile, draDestFile, true);

                Trc.Log(LogLevel.Always, "Extracting DRA.");
                string Extract_CommandLineParameters = " /x:" + draDestFolder + " /q";
                int extractReturnCode =
                    CommandExecutor.RunCommand(
                    draDestFile,
                    Extract_CommandLineParameters);

                // Installing DRA, if it's extraction succeeds.
                if (extractReturnCode == 0)
                {
                    Trc.Log(LogLevel.Always, "Installing DRA.");
                    string setupDRFile =
                        Path.Combine(draDestFolder, UnifiedSetupConstants.SETUPDRExeName);
                    string defaultDRALogFileName =
                        PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.DRAUTCLogName);

                    string InstallCommandLineParams = " /i /LogFileName " + defaultDRALogFileName + "";
                    draReturnCode =
                        CommandExecutor.ExecuteCommand(
                        setupDRFile,
                        out output,
                        out errorMsg,
                        InstallCommandLineParams);

                    if (draReturnCode == 0)
                    {
                        Trc.Log(LogLevel.Always, "DRA installation is successful.");

                        // Delete DRA extracted folder ({system drive}\Temp\DRASETUP).
                        Trc.Log(LogLevel.Always, "Deleting {0}.", draDestFolder);
                        Directory.Delete(draDestFolder, true);
                    }
                    else
                    {
                        Trc.Log(LogLevel.Error, "Failed to install DRA.");
                    }
                }
                else
                {
                    Trc.Log(LogLevel.Error, "Failed to extract DRA.");
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception occurred while installing DRA: {0}", ex.ToString());    
            }
            finally
            {
                // Set install status to false if exit code is non-zero.
                SetupParameters.Instance().IsDRAInstalled =
                    (draReturnCode == 0) ? true : false;

                // Record operation status to summary log.
                RecordOperation(OperationName.InstallDra, draReturnCode, output, errorMsg);
            }

            return draReturnCode;
        }
        #endregion

        #region MTInstallationMethods
        /// <summary>
        /// Install Master target.
        /// </summary>
        /// <returns>Exit code</returns>
        public static int InstallMT()
        {
            string processOutput, processError;
            int mtReturnCode = -1;
            try
            {
                string CommandLineParameters = "";
                string connectionPassphraseFile =
                    Path.Combine(sysDrive, UnifiedSetupConstants.PassphrasePath);

                // Fetch all the required information in case of Fresh/Re-installation.
                if ((SetupParameters.Instance().InstallType == UnifiedSetupConstants.FreshInstall) &&
                    (SetupParameters.Instance().ReInstallationStatus == UnifiedSetupConstants.Yes))
                {
                    if (!FetchInstallationDetails())
                    {
                        Trc.Log(LogLevel.Error, "Unable to fetch installation details.");
                        return mtReturnCode;
                    }
                }
                else if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade)
                {
                    Trc.Log(LogLevel.Always, "Fetch Agent installation directory during upgrade.");
                    // Get agent installation directory from the registry.
                    string agentInstallDir = (string)Registry.GetValue(
                        UnifiedSetupConstants.AgentRegistryName,
                        UnifiedSetupConstants.InstDirRegKeyName,
                        string.Empty);
                    Trc.Log(LogLevel.Always, "Agent installation directory: {0}", agentInstallDir);
                    SetupParameters.Instance().AgentInstallDir = agentInstallDir;
                }

                // Construct command line parameters.
                CommandLineParameters = "/Role " + "\"" + SetupParameters.Instance().AgentRole + "\"" +
                                        " /Silent " +
                                        " /InstallLocation " + "\"" + SetupParameters.Instance().AgentInstallDir + "\"" +
                                        " /Platform " + "\"" + Platform.VmWare + "\"";

                mtReturnCode =
                    CommandExecutor.ExecuteCommand(
                    UnifiedSetupConstants.MTExeName,
                    out processOutput,
                    out processError,
                    CommandLineParameters);

                Trc.Log(LogLevel.Always, "MT Installation output: {0}", processOutput);
                Trc.Log(LogLevel.Always, "MT Installation error: {0}", processError);
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while installing Master target: {0}", ex.ToString());
            }
            finally
            {
                // Record operation status to summary log.
                RecordOperation(OperationName.InstallMt, mtReturnCode);

                // Set install status to false if exit code is non-zero.
                if ((mtReturnCode == 0) || (mtReturnCode == 1641) || (mtReturnCode == 3010) || (mtReturnCode == 98))
                {
                    SetupParameters.Instance().IsMTInstalled = true;
                }
                else
                {
                    SetupParameters.Instance().IsMTInstalled = false;
                }

                // Delete hostconfig shortcut on OVF VMs.
                if (SetupParameters.Instance().OVFImage)
                {
                    DeleteShortCut(UnifiedSetupConstants.HostConfigShortcutName);
                }
            }

            return mtReturnCode;
        }
        #endregion

        #region MTConfigurationMethod

        /// <summary>
        /// Configure Master target.
        /// </summary>
        /// <returns>Exit code</returns>
        public static int ConfigureMT()
        {
            string processOutput, processError;
            int configMTRetCode = -1;
            try
            {
                string connectionPassphraseFile =
                    Path.Combine(sysDrive, UnifiedSetupConstants.PassphrasePath);

                // Fetch CS IP.
                if (string.IsNullOrEmpty(SetupParameters.Instance().CSIP))
                {
                    // Read PS_CS_IP from amethst.conf.
                    string amethystFilePath =
                        Path.Combine(
                        SetupParameters.Instance().CXInstallDir,
                        UnifiedSetupConstants.AmethystFile);
                    Trc.Log(LogLevel.Always, "Fetch CS IP from: {0}", amethystFilePath);
                    SetupParameters.Instance().CSIP =
                        GrepUtils.GetKeyValueFromFile(
                        amethystFilePath,
                        UnifiedSetupConstants.PSCSIP_AmethystConf);
                }

                // Construct command line parameters.
                string CommandLineParameters = " /CSEndPoint " + "\"" + SetupParameters.Instance().CSIP + "\"" +
                                        " /PassphraseFilePath " + "\"" + connectionPassphraseFile + "\"";

                // Get agent installation directory from the registry.
                string agentInstallDir = (string)Registry.GetValue(
                    UnifiedSetupConstants.AgentRegistryName,
                    UnifiedSetupConstants.InstDirRegKeyName,
                    string.Empty);

                string ConfiguratorExePath =
                    Path.Combine(agentInstallDir, UnifiedSetupConstants.ConfiguratorExeName);
                configMTRetCode =
                    CommandExecutor.ExecuteCommand(
                    ConfiguratorExePath,
                    out processOutput,
                    out processError,
                    CommandLineParameters);

                Trc.Log(LogLevel.Always, "MT Configuration output: {0}", processOutput);
                Trc.Log(LogLevel.Always, "MT Configuration error: {0}", processError);
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while configuring Master target: {0}", ex.ToString());
            }
            finally
            {
                // Record operation status to summary log.
                RecordOperation(OperationName.ConfigureMt, configMTRetCode);

                // Set configuration status to false if exit code is non-zero.
                if (configMTRetCode == 0)
                {
                    SetupParameters.Instance().IsMTConfigured = true;
                }
                else
                {
                    SetupParameters.Instance().IsMTConfigured = false;
                }
            }

            return configMTRetCode;
        }
        #endregion

        #region MARSInstallationMethod
        /// <summary>
        /// Install/Upgrade MARS agent.
        /// </summary>
        /// <returns>Exit code</returns>
        public static int InstallMARS()
        {
            int marsReturnCode = -1;
            try
            {
                string CmdLineParams = " /q /nu";

                Trc.Log(LogLevel.Always, "Installing MARS agent.");
                marsReturnCode =
                    CommandExecutor.RunCommand(
                    UnifiedSetupConstants.MarsAgentExeName,
                    CmdLineParams);

                // Retry MARS installation when exception occurs in process execution.
                if (marsReturnCode == -1)
                {
                    Thread.Sleep(10000);
                    marsReturnCode =
                        CommandExecutor.RunCommand(
                        UnifiedSetupConstants.MarsAgentExeName,
                        CmdLineParams);
                }

                if (marsReturnCode == 0)
                {
                    Trc.Log(LogLevel.Always, "MARS agent installation is successful.");

                    // Restart the ProcessServerMonitor service after MARS installation, as it is
                    // not able to detect the custom perf counters created by MARS.
                    ServiceControlFunctions.StopService(UnifiedSetupConstants.ProcessServerMonitorServiceName);
                    ServiceControlFunctions.StartService(UnifiedSetupConstants.ProcessServerMonitorServiceName);
                }
                else
                {
                    Trc.Log(LogLevel.Error, "Failed to install MARS agent.");
                }   
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while installing MARS agent: {0}", ex.ToString());
            }
            finally
            {
                // Set MARS status to false if exit code is non-zero.
                SetupParameters.Instance().IsMARSInstalled =
                    (marsReturnCode == 0) ? true : false;

                // Record operation status to summary log.
                RecordOperation(OperationName.InstallMars, marsReturnCode);

                // Delete MARS agent shortcut on OVF VMs.
                if (SetupParameters.Instance().OVFImage)
                {
                    DeleteShortCut(UnifiedSetupConstants.MarsAgentShortcutName);
                }
            }

            return marsReturnCode;
        }
        #endregion

        #region ConfigurationManagerInstallationMethods
        /// <summary>
        /// Install Azure Site Recovery Configuration Manager.
        /// </summary>
        /// <returns>Exit code</returns>
        public static int InstallConfigurationManager()
        {
            int configManagerReturnCode = -1;
            try
            {
                string msiArgument = " /norestart ";
                string currentPath = (new System.IO.FileInfo(System.Reflection.Assembly.GetExecutingAssembly().Location)).DirectoryName;
                int exitCode;
                bool result = false;

                Trc.Log(
                    LogLevel.Always,
                    string.Format(
                        "Calling MSIexec for '{0}' msi with arguments '{1}'",
                        UnifiedSetupConstants.ConfigManagerMsiName,
                        msiArgument));

                // Install the Configuration Manager MSI
                result = SetupHelper.MSIHelper(
                    true,
                    MsiOperation.Install,
                    currentPath,
                    UnifiedSetupConstants.ConfigManagerMsiName,
                    UnifiedSetupConstants.ConfigManagerMsiLog,
                    msiArgument,
                    StringResources.MsiexexUnknownExitCode,
                    out exitCode);

                if (result)
                {
                    Trc.Log(LogLevel.Always, "Azure Site Recovery Configuration Manager installation is successful.");
                    configManagerReturnCode = 0;
                }
                else
                {
                    Trc.Log(LogLevel.Error, "Failed to install Azure Site Recovery Configuration Manager with exit code: {0}.",exitCode);
                }
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while installing Azure Site Recovery Configuration Manager: {0}", ex.ToString());
            }
            finally
            {
                // Record operation status to summary log.
                RecordOperation(OperationName.InstallConfigManager, configManagerReturnCode);

                // Set install status to false if exit code is non-zero.
                if (configManagerReturnCode == 0)
                {
                    SetupParameters.Instance().IsConfigManagerInstalled = true;
                }
                else
                {
                    SetupParameters.Instance().IsConfigManagerInstalled = false;
                }
            }

            return configManagerReturnCode;
        }
        #endregion

        #region RegistrationMethods
        /// <summary>
        /// Performs Container, Vault and MT registration.
        /// Sets proxy for MARS agent.
        /// </summary>
        /// <returns>true if all the registrations succeed, false otherwise</returns>
        public static bool CSComponentsRegistration(out SetupHelper.SetupReturnValues setupReturnCode)
        {
            bool isRegSucess = false;
            int regRetVal;
            setupReturnCode = SetupHelper.SetupReturnValues.Successful;
            try
            {
                string drConfiguratorFile =
                    Path.Combine(sysDrive, UnifiedSetupConstants.DRConfiguratorExePath);
                string hostName = GetHostName(true);
                string vaultCredFilePath = SetupParameters.Instance().ValutCredsFilePath;
                string proxyType = SetupParameters.Instance().ProxyType;
                string proxyAddress = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.ProxyAddress);
                string proxyPort = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.ProxyPort);
                string proxyUsername = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.ProxyUsername);
                SecureString proxyPassword = PropertyBagDictionary.Instance.GetProperty<SecureString>(
                    PropertyBagConstants.ProxyPassword);
                string bypassProxyURLs = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.BypassProxyURLs);
                string exportLocation =
                    Path.Combine(sysDrive, UnifiedSetupConstants.ExportPath);
                string passphraseFilePath =
                    Path.Combine(sysDrive, UnifiedSetupConstants.PassphrasePath);
                SecureString passphraseVal =
                    SecureStringHelper.StringToSecureString(
                    System.IO.File.ReadAllText(passphraseFilePath));
                string defaultDRALogFileName = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.DRAUTCLogName);

                // Performing container registration.
                Trc.Log(LogLevel.Always, "Performing container registration.");
                regRetVal = ConatinerRegistration(
                                drConfiguratorFile,
                                proxyType,
                                vaultCredFilePath,
                                exportLocation,
                                passphraseVal,
                                proxyAddress,
                                proxyPort,
                                proxyUsername,
                                proxyPassword,
                                defaultDRALogFileName,
                                bypassProxyURLs);

                // Container registration succeeded.
                if (regRetVal == 0)
                {
                    Trc.Log(LogLevel.Always,
                        "Configuration server registration with Azure container has succeeded.");

                    // Performing vault registration.
                    Trc.Log(LogLevel.Always, "Performing vault registration.");
                    regRetVal = VaultRegistration(
                                    drConfiguratorFile,
                                    proxyType,
                                    hostName,
                                    vaultCredFilePath,
                                    proxyAddress,
                                    proxyPort,
                                    proxyUsername,
                                    proxyPassword,
                                    defaultDRALogFileName,
                                    bypassProxyURLs);

                    // Vault registration succeeded.
                    if (regRetVal == 0)
                    {
                        Trc.Log(LogLevel.Always,
                            "Configuration server registration with Azure vault has succeeded.");

                        // Check for Master target registration and MARS agent proxy.
                        if (PSComponentsRegistration(out setupReturnCode))
                        {
                            isRegSucess = true;
                        }
                    }
                    // Vault registration failed.
                    else
                    {
                        Trc.Log(LogLevel.Error, "Configuration server registration with Azure vault has failed.");
                        setupReturnCode = SetupHelper.SetupReturnValues.VaultRegistrationFailed;
                    }
                }
                // Container registration failed.
                else
                {
                    Trc.Log(LogLevel.Error, "Configuration server registration with Azure container has failed.");
                    setupReturnCode = SetupHelper.SetupReturnValues.ContainerRegistrationFailed;
                }

                return isRegSucess;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while CS components registration: {0}", ex.ToString());
                setupReturnCode = SetupHelper.SetupReturnValues.VaultRegistrationFailed;
                return isRegSucess;
            }
        }

        /// <summary>
        /// Performs MT registration and sets MARS agent proxy.
        /// </summary>
        /// <returns>true if all the registrations succeed, false otherwise</returns>
        public static bool PSComponentsRegistration(out SetupHelper.SetupReturnValues setupReturnCode)
        {
            bool isRegSucess = false;
            setupReturnCode = SetupHelper.SetupReturnValues.Successful;
            try
            {
                string cdpcliPath =
                    Path.Combine(
                    SetupParameters.Instance().AgentInstallDir,
                    UnifiedSetupConstants.CdpcliExePath);
                string proxyType = SetupParameters.Instance().ProxyType;

                Trc.Log(LogLevel.Always, "Performing Master target registration.");

                // Master target registration succeeded.
                if (MTRegistration(cdpcliPath))
                {
                    // Set MARS agent proxy if internet is connected through proxy server (custom proxy).
                    if (proxyType == UnifiedSetupConstants.CustomProxy)
                    {
                        if (SetMARSAgentProxy())
                        {
                            isRegSucess = true;
                        }
                        else
                        {
                            setupReturnCode = SetupHelper.SetupReturnValues.MARSAgentProxyIsNotSet;
                        }
                    }
                    // MARS agent proxy is not required.
                    else
                    {
                        isRegSucess = true;
                    }
                }
                else
                {
                    setupReturnCode = SetupHelper.SetupReturnValues.MTContainerRegistrationFailed;
                }

                return isRegSucess;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while PS components registration: {0}", ex.ToString());
                setupReturnCode = SetupHelper.SetupReturnValues.MTContainerRegistrationFailed;
                return isRegSucess;
            }
        }

        /// <summary>
        /// Vault registration.
        /// </summary>
        /// <returns>true if the registration succeeds, false otherwise</returns>
        public static int VaultRegistration(
            string drConfiguratorFile,
            string proxyType,
            string hostName,
            string vaultCredFilePath,
            string proxyAddress,
            string proxyPort,
            string proxyUsername,
            SecureString proxyPassword,
            string defaultDRALogFileName,
            string bypassProxyURLs)
        {
            int returnCode = -1;
            string proxyPwd = "";
            string output = "";
            string errorMsg = "";
            try
            {
                if (!SecureString.Equals(proxyPassword, null))
                {
                    proxyPwd = SecureStringHelper.SecureStringToString(proxyPassword);
                }

                // Bypass proxy parameters for vault registration.
                string BypassParams =
                    " /r " +
                    " /LogFileName " + "\"" + defaultDRALogFileName + "\"" +
                    " /friendlyname " + "\"" + hostName + "\"" +
                    " /Credentials " + "\"" + vaultCredFilePath + "\"";

                // Custom proxy without authentication parameters for container registration.
                string CustomParams =
                    BypassParams +
                    " /proxyaddress " + "\"" + proxyAddress + "\"" +
                    " /proxyport " + "\"" + proxyPort + "\"";

                if (!string.IsNullOrWhiteSpace(bypassProxyURLs))
                {
                    CustomParams =
                        CustomParams +
                        " /AddBypassUrls " + "\"" + bypassProxyURLs + "\"";
                }

                // Custom proxy with authentication parameters for container registration.
                string CustomAuthParams =
                    CustomParams +
                    " /proxyusername " + "\"" + proxyUsername + "\"" +
                    " /proxypassword " + "\"" + proxyPwd + "\"" ;

                switch (proxyType)
                {
                    case UnifiedSetupConstants.BypassProxy:
                        Trc.Log(
                            LogLevel.Always,
                            "Performing vault registration by Bypass proxy arguments.");
                        returnCode =
                            CommandExecutor.ExecuteCommand(
                            drConfiguratorFile,
                            out output,
                            out errorMsg,
                            BypassParams);
                        break;
                    case UnifiedSetupConstants.CustomProxy:
                        if (string.IsNullOrEmpty(proxyUsername) || string.IsNullOrEmpty(proxyPwd))
                        {
                            Trc.Log(
                                LogLevel.Always,
                                "Performing vault registration by Custom proxy without authentication arguments.");
                            returnCode =
                                CommandExecutor.ExecuteCommand(
                                drConfiguratorFile,
                                out output,
                                out errorMsg,
                                CustomParams);
                        }
                        else
                        {
                            Trc.Log(
                                LogLevel.Always,
                                "Performing vault registration by Custom proxy with authentication arguments .");
                            returnCode =
                                CommandExecutor.RunCommandNoArgsLogging(
                                drConfiguratorFile,
                                CustomAuthParams,
                                out output,
                                out errorMsg);
                        }
                        break;
                    default:
                        Trc.Log(
                            LogLevel.Always,
                            "Default case: Performing vault registration by Bypass proxy arguments.");
                        returnCode =
                            CommandExecutor.ExecuteCommand(
                            drConfiguratorFile,
                            out output,
                            out errorMsg,
                            BypassParams);
                        break;
                }

                Trc.Log(LogLevel.Always, "Vault Registration command output: {0}", output);

                // Record operation status to summary log.
                RecordOperation(OperationName.RegisterDra, returnCode, output);

                return returnCode;
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Exception occurred while vault registration: {0}", ex.ToString());
                // Record operation status to summary log.
                RecordOperation(OperationName.RegisterDra, returnCode, output, errorMsg);

                return returnCode;
            }
            finally
            {
                // Set registration status to false if exit code is non-zero.
                if (returnCode == 0)
                {
                    SetupParameters.Instance().IsDRARegistered = true;
                }
                else
                {
                    SetupParameters.Instance().IsDRARegistered = false;
                }
            }
        }

        /// <summary>
        /// Conatiner registration.
        /// </summary>
        /// <returns>true if the registration succeeds, false otherwise</returns>
        public static int ConatinerRegistration(
            string drConfiguratorFile,
            string proxyType,
            string vaultCredFilePath,
            string exportLocation,
            SecureString passphraseVal,
            string proxyAddress,
            string proxyPort,
            string proxyUsername,
            SecureString proxyPassword,
            string defaultDRALogFileName,
            string bypassProxyURLs)
        {
            int returnCode = -1;
            string proxyPwd = "";
            string passphraseString = "";
            string output = "";
            string errorMsg = "";
            try
            {
                if (!SecureString.Equals(proxyPassword, null))
                {
                    proxyPwd = SecureStringHelper.SecureStringToString(proxyPassword);
                }

                if (!SecureString.Equals(passphraseVal, null))
                {
                    passphraseString = SecureStringHelper.SecureStringToString(passphraseVal);
                }

                // Bypass proxy parameters for container registration.
                string BypassParams =
                    " /configureCloud " +
                    " /LogFileName " + "\"" + defaultDRALogFileName + "\"" +
                    " /Credentials " + "\"" + vaultCredFilePath + "\"" +
                    " /ExportLocation " + "\"" + exportLocation + "\"" +
                    " /CertificatePassword " + "\"" + passphraseString + "\"";

                // Custom proxy without authentication parameters for container registration.
                string CustomParams =
                    BypassParams +
                    " /proxyaddress " + "\"" + proxyAddress + "\"" +
                    " /proxyport " + "\"" + proxyPort + "\"";

                if (!string.IsNullOrWhiteSpace(bypassProxyURLs))
                {
                    CustomParams =
                        CustomParams +
                        " /AddBypassUrls " + "\"" + bypassProxyURLs + "\"";
                }

                // Custom proxy with authentication parameters for container registration.
                string CustomAuthParams =
                    CustomParams +
                    " /proxyusername " + "\"" + proxyUsername + "\"" +
                    " /proxypassword " + "\"" + proxyPwd + "\"";

                switch (proxyType)
                {
                    case UnifiedSetupConstants.BypassProxy:
                        Trc.Log(
                            LogLevel.Always,
                            "Performing container registration by Bypass proxy arguments.");
                        returnCode =
                            CommandExecutor.RunCommandNoArgsLogging(
                            drConfiguratorFile,
                            BypassParams,
                            out output,
                            out errorMsg);
                        break;
                    case UnifiedSetupConstants.CustomProxy:
                        if (string.IsNullOrEmpty(proxyUsername) || string.IsNullOrEmpty(proxyPwd))
                        {
                            Trc.Log(
                                LogLevel.Always,
                                "Performing container registration by Custom proxy without authentication arguments.");
                            returnCode =
                                CommandExecutor.RunCommandNoArgsLogging(
                                drConfiguratorFile,
                                CustomParams,
                                out output,
                                out errorMsg);
                        }
                        else
                        {
                            Trc.Log(
                                LogLevel.Always,
                                "Performing container registration by Custom proxy with authentication arguments .");
                            returnCode =
                                CommandExecutor.RunCommandNoArgsLogging(
                                drConfiguratorFile,
                                CustomAuthParams,
                                out output,
                                out errorMsg);
                        }
                        break;
                    default:
                        Trc.Log(
                            LogLevel.Always,
                            "Default case: Performing container registration by Bypass proxy arguments.");
                        returnCode =
                            CommandExecutor.RunCommandNoArgsLogging(
                            drConfiguratorFile,
                            BypassParams,
                            out output,
                            out errorMsg);
                        break;
                }

                Trc.Log(LogLevel.Always, "Conatiner Registration command output: {0}", output);

                // Record operation status to summary log.
                RecordOperation(OperationName.RegisterCloudContainer, returnCode, output);

                return returnCode;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while container registration: {0}", ex.ToString());
                // Record operation status to summary log.
                RecordOperation(OperationName.RegisterCloudContainer, returnCode, output, errorMsg);

                return returnCode;
            }
            finally
            {
                // Set registration status to false if exit code is non-zero.
                if (returnCode == 0)
                {
                    SetupParameters.Instance().IsDRARegistered = true;
                }
                else
                {
                    SetupParameters.Instance().IsDRARegistered = false;
                }
            }
        }

        /// <summary>
        /// Mater target registration.
        /// </summary>
        /// <returns>true if the registration succeeds, false otherwise</returns>
        public static bool MTRegistration(string cdpcliPath)
        {
            string output = "";
            string errorMsg = "";
            int cdpcliReturnCode = -1;
            try
            {
                // Register Master target by executing cdpcli --registermt command.
                cdpcliReturnCode =
                        CommandExecutor.ExecuteCommand(
                        cdpcliPath,
                        out output,
                        out errorMsg,
                        " --registermt");
                Trc.Log(LogLevel.Always, "cdpcli --registermt command output : {0}{1}{2}", output, Environment.NewLine, errorMsg);

                // Record operation status to summary log.
                RecordOperation(OperationName.RegisterMt, cdpcliReturnCode);

                if (cdpcliReturnCode == 0)
                {
                    Trc.Log(LogLevel.Always, "Master Target registration has succeeded.");
                    return true;
                }
                else
                {
                    Trc.Log(LogLevel.Error, "Master Target registration has failed.");
                    return false;
                }
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while Master target registration: {0}", ex.ToString());
                // Record operation status to summary log.
                RecordOperation(OperationName.RegisterMt, cdpcliReturnCode);

                return false;
            }
            finally
            {
                // Set MT registration status to false if exit code is non-zero.
                if (cdpcliReturnCode == 0)
                {
                    SetupParameters.Instance().IsMTRegistered = true;
                }
                else
                {
                    SetupParameters.Instance().IsMTRegistered = false;
                }
            }
        }

        /// <summary>
        /// Set MARS agent proxy.
        /// </summary>
        /// <returns>true if proxy is set successfully, false otherwise</returns>
        public static bool SetMARSAgentProxy()
        {
            bool isMARSAgentProxySet = false;
            try
            {
                Trc.Log(LogLevel.Always, "Setting up the proxy for the MARS agent.");
                int marsProxyStatus = SetMarsProxy();
                if (marsProxyStatus == 0)
                {
                    Trc.Log(LogLevel.Always, "MARS agent proxy has been set successfully.");

                    // Stop and start the MARS agent service.
                    ServiceControlFunctions.StopService(UnifiedSetupConstants.MARSServiceName);
                    ServiceControlFunctions.StartService(UnifiedSetupConstants.MARSServiceName);

                    isMARSAgentProxySet = true;
                }
                else
                {
                    Trc.Log(LogLevel.Error, "Failed to set MARS agent proxy.");
                }

                // Record operation status to summary log.
                RecordOperation(OperationName.SetMarsProxy, marsProxyStatus);

                return isMARSAgentProxySet;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while setting MARS agent proxy : {0}", ex.ToString());
                return isMARSAgentProxySet;
            }
        }

        /// <summary>
       /// Executes MARS agent cmdlet to set proxy.
       /// </summary>
       public static int SetMarsProxy()
       {
           int returnCode = -1;
           try
           {
               string address, port, username, password = "", bypassURLs;
               string CommandLineParameters, output = "";
               SecureString spassword;

               address =
                   PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ProxyAddress);
               port =
                   PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ProxyPort);
               username =
                   PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ProxyUsername);

               bypassURLs =
                    PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.BypassProxyURLs);

                if (!string.IsNullOrEmpty(username))
               {
                   spassword =
                       PropertyBagDictionary.Instance.GetProperty<SecureString>(
                       PropertyBagConstants.ProxyPassword);
                   password = SecureStringHelper.SecureStringToString(spassword);
               }

               // Adding http:// to address.
               if (!(address.Contains("http://")))
               {
                   address = "http://" + address;
               }

               // Construct command line parameters.
               if (!string.IsNullOrEmpty(username))
               {
                   CommandLineParameters = "\"" + address + "\"" + " " + "\"" + port + "\"" + " " + "\"" + username + "\"" + " " + "\"" + password + "\"";
               }
               else
               {
                   CommandLineParameters = "\"" + address + "\"" + " " + "\"" + port + "\"";
               }

                if (!string.IsNullOrWhiteSpace(bypassURLs))
                {
                    CommandLineParameters =
                        CommandLineParameters + " " + "\"" + bypassURLs + "\"";
                }

                // Invoking setMarsProxy.exe with command line parameters.
                returnCode =
                   CommandExecutor.RunCommandNoArgsLogging(
                   UnifiedSetupConstants.SetMarsProxyExeName,
                   CommandLineParameters,
                   out output);
               Trc.Log(LogLevel.Always, "Powershell command line output : {0}", output);
               return returnCode;
           }
           catch (Exception exp)
           {
               Trc.LogException(
                   LogLevel.Error,
                   "Failed to configure MARS proxy. Exception occurred : {0}", exp);
               return returnCode;
           }
       }

        /// <summary>
        /// Validate Vault, container and MT registrations.
        /// </summary>
        /// <returns>true if all the registrations succeed, false otherwise</returns>
        public static bool AreAllComponentsRegistered()
        {
            bool areAllComponentsRegistered = false;
            try
            {
                Microsoft.DisasterRecovery.IntegrityCheck.Response draRegResponse =
                    IntegrityCheckWrapper.Evaluate(Validations.IsDraRegistered);
                Microsoft.DisasterRecovery.IntegrityCheck.Response containerRegResponse =
                    IntegrityCheckWrapper.Evaluate(Validations.IsCloudContainerRegistered);
                Microsoft.DisasterRecovery.IntegrityCheck.Response mtRegResponse =
                    IntegrityCheckWrapper.Evaluate(Validations.IsMtRegistered);
                Trc.Log(LogLevel.Always, "DRA registration : {0}", draRegResponse.Result);
                Trc.Log(LogLevel.Always, "Container registration : {0}", containerRegResponse.Result);
                Trc.Log(LogLevel.Always, "MT registration : {0}", mtRegResponse.Result);

                if ((draRegResponse.Result == Microsoft.DisasterRecovery.IntegrityCheck.OperationResult.Success) &&
                    (containerRegResponse.Result == Microsoft.DisasterRecovery.IntegrityCheck.OperationResult.Success) &&
                    (mtRegResponse.Result == Microsoft.DisasterRecovery.IntegrityCheck.OperationResult.Success))
                {
                    areAllComponentsRegistered = true;
                }

                return areAllComponentsRegistered;
            }
            catch (Exception exp)
            {
                Trc.LogException(
                    LogLevel.Error,
                    "Exception occurred while validating components registration: {0}", exp);
                return areAllComponentsRegistered;
            }
        }
        #endregion

        #region ValidationMethods
        /// <summary>
        /// Check for IIS installation on the server.
        /// </summary>
        /// <returns>true if IISInstallationStatus regkey is set to succeeded, false otherwise</returns>
        public static bool IsIISInstalled()
        {
            bool isIISInstalled = false;
            try
            {
                string iisInstallationStatus =
                            (string)Registry.GetValue(
                                UnifiedSetupConstants.InstalledProductsRegName,
                                UnifiedSetupConstants.IISInstallationStatusRegKeyName,
                                null);

                Trc.Log(LogLevel.Always, "IIS installation status registry key is set to : {0}", iisInstallationStatus);

                if (iisInstallationStatus == UnifiedSetupConstants.SuccessStatus)
                {
                    isIISInstalled = true;
                }
            }
            catch (Exception e)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception Occured while IsIISInstalled validation: {0}", e.ToString());
            }
            return isIISInstalled;
        }

        /// <summary>
        /// Check for component installation on the server.
        /// </summary>
        /// <returns>true if component is installed, false otherwise</returns>
        public static bool IsComponentInstalled(string componentName)
        {
            bool iscomponentInstalled = false;
            try
            {
                string installedVersion = "";

                // Validate the component installation on the server.
                if ((componentName == UnifiedSetupConstants.CXTPProductName) ||
                    (componentName == UnifiedSetupConstants.CSPSProductName))
                {
                    iscomponentInstalled =
                        ValidationHelper.IsSoftwareInstalledWow6432(
                        componentName,
                        out installedVersion);
                }
                else
                {
                    iscomponentInstalled =
                        SetupHelper.IsInstalled(
                        componentName,
                        out installedVersion);
                }

                if (iscomponentInstalled)
                {
                    Trc.Log(LogLevel.Always, "{0} is installed on the server.", componentName);
                    iscomponentInstalled = true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "{0} is not installed on the server.", componentName);
                }
                return iscomponentInstalled;
            }
            catch (Exception e)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception Occured while IsComponentInstalled validation: {0}", e.ToString());
                return iscomponentInstalled;
            }
        }

        /// <summary>
        /// Reads CX dependencies and CX components installed version from Registry.
        /// </summary>
        public static void CXandCXTPInstalledVersion(string componentName, out string BuildVersion)
        {
            BuildVersion = "";
            try
            {
                // Get the version of the installed installer.
                if (componentName == UnifiedSetupConstants.CXTPProductName)
                {
                    // Fetch CX TP installed version from registry.
                    Trc.Log(LogLevel.Always, "Fetching CX TP installed version.");
                    BuildVersion =
                        (string)Registry.GetValue(
                        UnifiedSetupConstants.CXTPRegistryName,
                        UnifiedSetupConstants.BuildVersionRegKeyName,
                        null);
                }
                else
                {
                    // Fetch CX installed version from registry.
                    Trc.Log(LogLevel.Always, "Fetching CX installed version.");
                    BuildVersion =
                        (string)Registry.GetValue(
                        UnifiedSetupConstants.CSPSRegistryName,
                        UnifiedSetupConstants.BuildVersionRegKeyName,
                        null);
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception Occured: {0}", e.ToString());
            }
        }

        /// <summary>
        /// Check whether current component is installed on the server.
        /// </summary>
        /// <returns>true if current component is installed, false otherwise</returns>
        public static bool IsCurrentComponentInstalled(string componentName, string componentExeName)
        {
            bool isCurrentComponentInstalled = false;
            try
            {
                // Validate the component installation on the server.
                string installedVersion = "";
                string buildVersion = "";
                bool iscomponentInstalled = false;
                if ((componentName == UnifiedSetupConstants.CXTPProductName) ||
                    (componentName == UnifiedSetupConstants.CSPSProductName))
                {
                    iscomponentInstalled =
                        ValidationHelper.IsSoftwareInstalledWow6432(
                        componentName,
                        out installedVersion);
                }
                else
                {
                    iscomponentInstalled =
                        SetupHelper.IsInstalled(
                        componentName,
                        out installedVersion);
                }

                if (iscomponentInstalled)
                {
                    Trc.Log(LogLevel.Always, "" + componentName + " is installed on the server.");

                    // Get absolute path of component exe.
                    string currDir =
                        new System.IO.FileInfo(System.Reflection.Assembly.GetExecutingAssembly().Location).DirectoryName;
                    string componentPath = Path.Combine(currDir, componentExeName);

                    // Get the version of the current installer.
                    FileVersionInfo fvi = FileVersionInfo.GetVersionInfo(componentPath);
                    Version currentBuildVersion = new Version(fvi.FileVersion);

                    Version installedBuildVersion = new Version();
                    if ((componentName == UnifiedSetupConstants.CXTPProductName) ||
                        (componentName == UnifiedSetupConstants.CSPSProductName))
                    {
                        CXandCXTPInstalledVersion(componentName, out buildVersion);
                        // Convert string to version format.
                        installedBuildVersion = new Version(buildVersion);
                    }
                    else
                    {
                        // Convert string to version format.
                        installedBuildVersion = new Version(installedVersion);
                    }

                    Trc.Log(LogLevel.Always, "Current build version : " + currentBuildVersion + "");
                    Trc.Log(LogLevel.Always, "Installed build version : " + installedBuildVersion + "");

                    int result = currentBuildVersion.CompareTo(installedBuildVersion);
                    if (result == 0)
                    {
                        isCurrentComponentInstalled = true;
                    }
                }
                return isCurrentComponentInstalled;
            }
            catch (Exception e)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception Occured while IsCurrentComponentInstalled validation: {0}", e.ToString());
                return isCurrentComponentInstalled;
            }
        }

        /// <summary>
        /// Gets the version of the specified build.
        /// </summary>
        /// <param name="buildName">Name of the build to retrive version.</param>
        /// <returns>Returns the version info.</returns>
        public static Version GetBuildVersion(string buildName)
        {
            // Get absolute path of component exe.
            string currDir =
                new System.IO.FileInfo(System.Reflection.Assembly.GetExecutingAssembly().Location).DirectoryName;
            string componentPath = Path.Combine(currDir, buildName);

            // Get the version of the current installer.
            FileVersionInfo fvi = FileVersionInfo.GetVersionInfo(componentPath);
            Version currentBuildVersion = new Version(fvi.FileVersion);

            return currentBuildVersion;
        }

        /// <summary>
        /// Validates the component installation on the server.
        /// </summary>
        /// <returns>true if component is installed, false otherwise</returns>
        /// <out>installed component version</out>
        public static bool ValidateComponentInstallation(string componentName, string componentExeName, out InstalledBuildVersion buildinstalled)
        {
            bool isComponentInstalled = false;
            buildinstalled = InstalledBuildVersion.None;
            string installedVersion = "";
            string buildVersion = "";
            try
            {
                bool isInstalled;

                // Validate the component installation on the server.
                if ((componentName == UnifiedSetupConstants.CXTPProductName) ||
                    (componentName == UnifiedSetupConstants.CSPSProductName))
                {
                    isInstalled =
                        ValidationHelper.IsSoftwareInstalledWow6432(
                        componentName,
                        out installedVersion);
                }
                else
                {
                    isInstalled =
                        SetupHelper.IsInstalled(
                        componentName,
                        out installedVersion);
                }

                // Component is installed.
                if (isInstalled)
                {
                    Trc.Log(LogLevel.Always, "" + componentName + " is installed on the server.");

                    // Get the version of the current installer.
                    Version currentBuildVersion = GetBuildVersion(componentExeName);

                    Version installedBuildVersion = new Version();
                    if ((componentName == UnifiedSetupConstants.CXTPProductName) ||
                        (componentName == UnifiedSetupConstants.CSPSProductName))
                    {
                        CXandCXTPInstalledVersion(componentName, out buildVersion);
                        // Convert string to version format.
                        installedBuildVersion = new Version(buildVersion);
                    }
                    else
                    {
                        // Convert string to version format.
                        installedBuildVersion = new Version(installedVersion);
                    }

                    Trc.Log(LogLevel.Always, "Current build version : " + currentBuildVersion + "");
                    Trc.Log(LogLevel.Always, "Installed build version : " + installedBuildVersion + "");

                    // Comparing current build version with installed build version.
                    int result = currentBuildVersion.CompareTo(installedBuildVersion);
                    if (result > 0)
                    {
                        if ((componentName == UnifiedSetupConstants.CXTPProductName) ||
                            (componentName == UnifiedSetupConstants.CSPSProductName))
                        {
                            int installedMinorVersion = installedBuildVersion.Minor;
                            int currentMinorVersion = currentBuildVersion.Minor;
                            Trc.Log(LogLevel.Always, 
                                "installedMinorVersion - {0}", 
                                installedMinorVersion);
                            Trc.Log(LogLevel.Always, 
                                "currentMinorVersion - {0}", 
                                currentMinorVersion);

                            int resultMinorVersion =
                                currentMinorVersion - installedMinorVersion;
                            buildinstalled = (resultMinorVersion > 4) ?
                                InstalledBuildVersion.Unsupported :
                                InstalledBuildVersion.Older;
                        }
                        else
                        {
                            Trc.Log(
                                LogLevel.Always,
                                "Older version of " + componentName + " is installed on the server.");
                            buildinstalled = InstalledBuildVersion.Older;
                        }
                    }
                    else if (result == 0)
                    {
                        Trc.Log(
                            LogLevel.Always,
                            "Same version of " + componentName + " is installed on the server.");
                        buildinstalled = InstalledBuildVersion.Same;
                    }
                    else if (result < 0)
                    {
                        Trc.Log(
                            LogLevel.Always,
                            "Higher version of " + componentName + " is installed on the server.");
                        buildinstalled = InstalledBuildVersion.Higher;
                    }

                    return true;
                }
                // Component is not installed.
                else
                {
                    Trc.Log(LogLevel.Always, "" + componentName + " is not installed on the server.");
                    return isComponentInstalled;
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Error,
                    "Exception Occured while validating component installation: {0}", e.ToString());
                return isComponentInstalled;
            }
        }

        /// <summary>
        /// Validates the MT configuration status.
        /// </summary>
        /// <returns>true if status is succeeded, false otherwise</returns>
        public static bool isMTConfigured()
        {
            try
            {
                string getMTConfigurationStatus = (string)Registry.GetValue(
                    UnifiedSetupConstants.AgentRegistryName,
                    UnifiedSetupConstants.AgentConfigurationStatus,
                    null);
                Trc.Log(LogLevel.Always, "getMTConfigurationStatus : {0}", getMTConfigurationStatus);

                Trc.Log(
                LogLevel.Always,
                "Checking DrScout config to see agent is already configured");

                IniFile drScoutInfo = new IniFile(Path.Combine(
                    SetupHelper.GetAgentInstalledLocation(),
                    UnifiedSetupConstants.DrScoutConfigRelativePath));

                string hostId = drScoutInfo.ReadValue("vxagent", "HostId");

                Trc.Log(
                    LogLevel.Always,
                    "HostId value in DRScout config is : " + hostId);

                return (((getMTConfigurationStatus == UnifiedSetupConstants.SuccessStatus) && (!string.IsNullOrEmpty(hostId))) ? true : false);
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception Occured while fetching MT configuration status from registry: {0}", ex.ToString());
                return false;
            }
        }

        /// <summary>
        /// Validates the upgrade supportability.
        /// </summary>
        /// <returns>true if upgrade is supported, false otherwise</returns>
        /// <returns>error message, if upgrade is not supported</returns>
        public static bool IsUpgradeSupported(out string error)
        {
            bool isUpgradeSupported = false;
            error = "";
            try
            {
                InstalledBuildVersion cxtpBuildInstalled = InstalledBuildVersion.None;
                InstalledBuildVersion cxBuildInstalled = InstalledBuildVersion.None;
                InstalledBuildVersion draBuildInstalled = InstalledBuildVersion.None;
                InstalledBuildVersion mtBuildInstalled = InstalledBuildVersion.None;
                InstalledBuildVersion marsBuildInstalled = InstalledBuildVersion.None;

                bool isCXTPInstalled =
                    ValidateComponentInstallation(
                    UnifiedSetupConstants.CXTPProductName,
                    UnifiedSetupConstants.CXTPExeName,
                    out cxtpBuildInstalled);

                bool isCXInstalled =
                    ValidateComponentInstallation(
                    UnifiedSetupConstants.CSPSProductName,
                    UnifiedSetupConstants.CSPSExeName,
                    out cxBuildInstalled);

                // Check for CX-TP and CX installation.
                if (isCXTPInstalled && isCXInstalled)
                {
                    // Allow upgrade only from last 4 released versions.
                    if ((cxtpBuildInstalled == InstalledBuildVersion.Unsupported) ||
                        (cxBuildInstalled == InstalledBuildVersion.Unsupported))
                    {
                        error = StringResources.UnsupportedInstalledVersionForUpgrade;
                        return isUpgradeSupported;
                    }

                    bool areAllComponentsInstalled = false;

                    bool isMTInstalled =
                            ValidateComponentInstallation(
                            UnifiedSetupConstants.MS_MTProductName,
                            UnifiedSetupConstants.MTExeName,
                            out mtBuildInstalled);

                    bool isMARSInstalled =
                            ValidateComponentInstallation(
                            UnifiedSetupConstants.MARSAgentProductName,
                            UnifiedSetupConstants.MarsAgentExeName,
                            out marsBuildInstalled);

                    // Fetch server mode.
                    if (ValidationHelper.IsCSInstalled())
                    {
                        SetupParameters.Instance().ServerMode = UnifiedSetupConstants.CSServerMode;
                    }
                    else
                    {
                        SetupParameters.Instance().ServerMode = UnifiedSetupConstants.PSServerMode;
                    }

                    // PS gallery image scenario.
                    if ((SetupHelper.IsAzureVirtualMachine()) &&
                        (isCXTPInstalled) &&
                        (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode) &&
                        !(isMTInstalled))
                    {
                        // Set PSGalleryImage status to true.
                        SetupParameters.Instance().PSGalleryImage = true;
                        areAllComponentsInstalled = true;
                    }
                    // Check for DRA, MT and MARS installation in case of CS installation.
                    else if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                    {
                        bool isDRAInstalled =
                            ValidateComponentInstallation(
                            UnifiedSetupConstants.AzureSiteRecoveryProductName,
                            UnifiedSetupConstants.DRAExeName,
                            out draBuildInstalled);

                        bool areAllRegistrationsSucceeded =
                            AreAllComponentsRegistered();

                        areAllComponentsInstalled = (isDRAInstalled && isMTInstalled && isMARSInstalled && areAllRegistrationsSucceeded);
                    }
                    // Check for MT and MARS installation in case of PS installation.
                    else if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
                    {
                        bool isMTRegistrationSucceeded = false;
                        Microsoft.DisasterRecovery.IntegrityCheck.Response mtRegResponse =
                            IntegrityCheckWrapper.Evaluate(Validations.IsMtRegistered);
                        Trc.Log(LogLevel.Always, "MT registration : {0}", mtRegResponse.Result);
                        if (mtRegResponse.Result == Microsoft.DisasterRecovery.IntegrityCheck.OperationResult.Success)
                        {
                            isMTRegistrationSucceeded = true;
                        }
                        draBuildInstalled = InstalledBuildVersion.Skip;

                        areAllComponentsInstalled = (isMTInstalled && isMARSInstalled && isMTRegistrationSucceeded);
                    }

                    Trc.Log(LogLevel.Always, "Are all components installed : {0}", areAllComponentsInstalled);

                    // All components are installed.
                    // Upgrade scenario.
                    if (areAllComponentsInstalled)
                    {
                        // Set productISAlreadyInstalled parameter to Yes.
                        SetupParameters.Instance().ProductISAlreadyInstalled = UnifiedSetupConstants.Yes;

                        // Upgrade is not supported, when atleast one component installed is of higher version.
                        if ((cxtpBuildInstalled == InstalledBuildVersion.Higher) ||
                            (cxBuildInstalled == InstalledBuildVersion.Higher) ||
                            (draBuildInstalled == InstalledBuildVersion.Higher) ||
                            (mtBuildInstalled == InstalledBuildVersion.Higher) ||
                            (marsBuildInstalled == InstalledBuildVersion.Higher))
                        {
                            Trc.Log(LogLevel.Always, "Higher version of components are installed in the setup.");
                            error = StringResources.UpgradeUnSupportedHigherVersionText;
                        }
                        // Upgrade is supported from older version.
                        else if ((cxtpBuildInstalled == InstalledBuildVersion.Older) ||
                            (cxBuildInstalled == InstalledBuildVersion.Older) ||
                            (draBuildInstalled == InstalledBuildVersion.Older) ||
                            (mtBuildInstalled == InstalledBuildVersion.Older) ||
                            (marsBuildInstalled == InstalledBuildVersion.Older))
                        {
                            Trc.Log(LogLevel.Always, "Older version of components are installed in the setup.");

                            // Set installation type to Upgrade.
                            SetupParameters.Instance().InstallType = UnifiedSetupConstants.Upgrade;

                            isUpgradeSupported = true;
                        }
                        // Latest version of components installed.
                        else if ((cxtpBuildInstalled == InstalledBuildVersion.Same) &&
                            (cxBuildInstalled == InstalledBuildVersion.Same) &&
                            (mtBuildInstalled == InstalledBuildVersion.Same) &&
                            (marsBuildInstalled == InstalledBuildVersion.Same) ||
                            (draBuildInstalled == InstalledBuildVersion.Same))
                        {
                            Trc.Log(LogLevel.Always, "Same version of components are installed in the setup.");
                            error = StringResources.UpgradeUnSupportedSameVersionText;
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always,
                                "Upgrade is not supported from the components version installed in the setup.");
                            error = StringResources.UpgradeUnSupportedText;
                        }

                    }
                    // All components are not installed.
                    // Re-installation scenario.
                    else
                    {
                        // Latest version of CX_TP and CX are installed.
                        // Re-installation scenario.
                        if ((cxtpBuildInstalled == InstalledBuildVersion.Same) ||
                            (cxBuildInstalled == InstalledBuildVersion.Same))
                        {
                            Trc.Log(LogLevel.Always, "Re-installation scenario.");

                            // Set installation type to Fresh and reinstallation status to yes.
                            SetupParameters.Instance().InstallType = UnifiedSetupConstants.FreshInstall;
                            SetupParameters.Instance().ReInstallationStatus = UnifiedSetupConstants.Yes;
                        }
                        // Latest version of CX_TP and CX are not installed
                        // and upgrade is not supported if all components are not installed.
                        // User has to uninstall existing version of CX_TP and CX and retry the installation.
                        else
                        {
                            error = StringResources.Uninstall_CXTP_CX;
                        }
                    }
                }
                // Fresh installation.
                else
                {
                    Trc.Log(LogLevel.Always, "Fresh installation scenario.");

                    // Set installation type to Fresh.
                    SetupParameters.Instance().InstallType = UnifiedSetupConstants.FreshInstall;
                }

                return isUpgradeSupported;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Error,
                    "Exception Occured while trying to validate Upgrade supportability: {0}", e.ToString());
                return isUpgradeSupported;
            }
        }

        /// <summary>
        /// Validates the pre-requisites for fresh installation.
        /// </summary>
        /// <returns>true if pre-requisites succeeds, false otherwise</returns>
        public static bool FreshInstallationPrereqs(out string error)
        {
            error = "";
            string cultureType;
            string agentType;

            // MySQL 5.5 validation
            if (SetupHelper.IsMySQLInstalled(UnifiedSetupConstants.MySQL55Version))
            {
                error = string.Format(StringResources.MySQLErrorText);
                return false;
            }
            // English OS validation.
            if (!ValidationHelper.IsOSEnglish(out cultureType))
            {
                if (cultureType == "NonEnglishOSCulture")
                {
                    error = StringResources.NonEnglishOS;
                }
                else
                {
                    error = StringResources.NonEnglishCulture;
                }
                return false;
            }

            // Validate ASR server install on agent
            if (ValidationHelper.IsAgentOnlyInstalled(out agentType))
            {
                if (agentType == "MT")
                {
                    error = StringResources.ASRServerInstallOnMT;
                }
                else
                {
                    error = StringResources.ASRServerInstallOnMS;
                }
                return false;
            }

            // Remove junction directory, if it exists.
            if (!InstallActionProcessess.IsJunctionDirectoryRemoved())
            {
                error = string.Format(StringResources.JunctionDirectoryRemoveErrorText, sysDrive);
                return false;
            }
            else
            {
                return true;
            }
        }

        /// <summary>
        /// Validate endpoint URL connectivity.
        /// </summary>
        /// <returns>true if connected, false otherwise</returns>
        public static bool ValidateEndPointconnectivity(
            out string detailedError)
        {
            bool result = true;
            string proxyAddress = null;
            string proxyPort = null;
            string proxyUserName = null;
            string bypassUrls = null;
            SecureString proxyPassword = null;
            bool bypassDefault = true;
            bool bypassProxyOnLocal = false;
            detailedError = string.Empty;
            
            try
            {
                Trc.Log(LogLevel.Always, "Begin ValidateEndPointconnectivity.");

                List<string> listofProxyEndpoints = new List<string>();

                string privateEndpointState = (string)Registry.GetValue(
                        UnifiedSetupConstants.CSPSRegistryName,
                        UnifiedSetupConstants.PrivateEndpointState,
                        null);
                Trc.Log(LogLevel.Always, "Private endpoint state : {0}", privateEndpointState);

                if (privateEndpointState == DRSetup.DRSetupConstants.PrivateEndpointState.Enabled.ToString())
                {
                    Trc.Log(LogLevel.Always, "Private endpoint is enabled");

                    if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
                    {
                        listofProxyEndpoints.Add((string)Registry.GetValue(
                            UnifiedSetupConstants.RegistrationRegistryHive,
                            UnifiedSetupConstants.SRSUrlRegKeyName,
                            null));
                    }

                    listofProxyEndpoints.Add((string)Registry.GetValue(
                            UnifiedSetupConstants.InmageMTregHive,
                            UnifiedSetupConstants.IdMgmtUrlRegKeyName,
                            null));
                    listofProxyEndpoints.Add((string)Registry.GetValue(
                            UnifiedSetupConstants.InmageMTregHive,
                            UnifiedSetupConstants.ProtSvcUrlRegKeyName,
                            null));
                    listofProxyEndpoints.Add((string)Registry.GetValue(
                            UnifiedSetupConstants.InmageMTregHive,
                            UnifiedSetupConstants.TelemetryUrlRegKeyName,
                            null));
                    listofProxyEndpoints.Add((string)Registry.GetValue(
                            UnifiedSetupConstants.InmageMTregHive,
                            UnifiedSetupConstants.AADUrlRegKeyName,
                            null));
                }
                else
                {
                    string vaultLocation =
                    DRSetup.RegistryOperations.GetVaultLocation();

                    Trc.Log(LogLevel.Always,
                        "vaultLocation : {0}",
                        vaultLocation);

                    // If Vault location is empty in DRA registry, fetch it from CSPS registry.
                    if (string.IsNullOrEmpty(vaultLocation))
                    {
                        vaultLocation =
                            SetupHelper.GetVaultLocationFromCSPSRegistry();
                    }

                    if (!string.IsNullOrEmpty(vaultLocation))
                    {
                        listofProxyEndpoints.AddRange(
                            IntegrityCheckWrapper.GetEndpoints(vaultLocation));
                    }

                    if ((SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode) &&
                        (PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.MySQL55Installed)))
                    {
                        listofProxyEndpoints.Add(UnifiedSetupConstants.MySQLLink);
                        listofProxyEndpoints.Add(UnifiedSetupConstants.MySQL55Link);
                        listofProxyEndpoints.Add(UnifiedSetupConstants.MySQLDownloadLink);
                    }
                }

                CheckEndpointConnectivityInput input =
                    new CheckEndpointConnectivityInput()
                    {
                        Endpoints = listofProxyEndpoints,
                    };

                foreach (string url in listofProxyEndpoints)
                {
                    Trc.Log(LogLevel.Always, "Endpoint URLs : {0}", url);
                }

                // Fetch proxy details from DRA registry for CSPSMT and 
                // CSPS registry for PSMT.
                if (SetupParameters.Instance().ServerMode ==
                    UnifiedSetupConstants.CSServerMode)
                {
                    DRSetup.RegistryOperations.GetProxySettingsFromRegistry(
                        out proxyAddress,
                        out proxyPort,
                        out proxyUserName,
                        out bypassUrls,
                        out proxyPassword,
                        out bypassDefault,
                        out bypassProxyOnLocal);
                }
                else
                {
                    DRSetup.RegistryOperations.GetProxySettingsFromCSPSRegistry(
                        out proxyAddress,
                        out proxyPort,
                        out proxyUserName,
                        out proxyPassword,
                        out bypassDefault);
                }

                input.WebProxy = (bypassDefault) ?
                    new System.Net.WebProxy() :
                    SetupHelper.BuildWebProxyFromPropertyBag(
                        proxyAddress, 
                        proxyPort, 
                        proxyUserName, 
                        proxyPassword);

                Microsoft.DisasterRecovery.IntegrityCheck.Response validationResponse = 
                    IntegrityCheckWrapper.Evaluate(
                        Validations.CheckEndpointsConnectivity, 
                        input);
                IntegrityCheckWrapper.RecordOperation(
                    ExecutionScenario.PreInstallation,
                    EvaluateOperations.ValidationMappings[Validations.CheckEndpointsConnectivity],
                    validationResponse.Result,
                    validationResponse.Message,
                    validationResponse.Causes,
                    validationResponse.Recommendation,
                    validationResponse.Exception
                    );

                if (validationResponse.Result == 
                    Microsoft.DisasterRecovery.IntegrityCheck.OperationResult.Failed)
                {
                    result = false;
                    detailedError = string.Format(
                        "{0}{1}{1}{2}:{3}{4}{5}{5}{6}:{7}{8}",
                        validationResponse.Message,
                        Environment.NewLine,
                        ASRResources.StringResources.Causes,
                        Environment.NewLine,
                        validationResponse.Causes,
                        Environment.NewLine,
                        ASRResources.StringResources.Recommendation,
                        Environment.NewLine,
                        validationResponse.Recommendation);
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Exception occurred: ", ex);
                result = false;
            }

            return result;
        }

        /// <summary>
        /// Validates the pre-requisites for upgrade.
        /// </summary>
        /// <returns>true if pre-requisites succeeds, false otherwise</returns>
        public static bool UpgradePrereqs(out string error)
        {
            error = string.Empty;
            string cultureType;
            int minimumSpaceInGB = UnifiedSetupConstants.RequiredFreeSpaceInGB;
            string cSInstallLocation = SetupHelper.GetCSInstalledLocation();
            string installDrive = Path.GetPathRoot(cSInstallLocation);

            Trc.Log(LogLevel.Always, 
                "SkipPrereqsCheck - {0}", 
                SetupParameters.Instance().SkipPrereqsCheck);
            if (!SetupParameters.Instance().SkipPrereqsCheck)
            {
                // Validate endpoint connectivity.
                if (!ValidateEndPointconnectivity(out error))
                {
                    return false;
                }

                // Kill cspsconfigtool.exe process.
                ValidationHelper.KillProcess(
                    UnifiedSetupConstants.CSPSConfigtoolName);

                // Validate space on installation drive.
                if (!ValidationHelper.ValidateSpaceOnDrive(
                    installDrive, 
                    minimumSpaceInGB))
                {
                    error = string.Format(
                        StringResources.InSufficientSpace, 
                        installDrive);
                    return false;
                }

                // Validate space on system drive.
                if (!ValidationHelper.ValidateSpaceOnDrive
                    (sysDrive, 
                    minimumSpaceInGB))
                {
                    error = string.Format(
                        StringResources.InSufficientSpace, 
                        sysDrive);
                    return false;
                }

                // English OS validation.
                if (!ValidationHelper.IsOSEnglish(out cultureType))
                {
                    error = (cultureType == UnifiedSetupConstants.NonEnglishOSCulture) ?
                        StringResources.NonEnglishOS :
                        StringResources.NonEnglishCulture;
                    return false;
                }

                // Kill frsvc service.
                ValidationHelper.KillService(
                    UnifiedSetupConstants.FrsvcServiceName);
            }

            return true;
        }

        /// <summary>
        /// Fetch installation information.
        /// </summary>
        public static bool FetchInstallationDetails()
        {
            bool status = false;
            try
            {
                // Fetch CS install directory from registry.
                Trc.Log(LogLevel.Always, "Fetching CX installation directory.");
                string csInstallDirectory =
                    (string)Registry.GetValue(
                    UnifiedSetupConstants.CSPSRegistryName,
                    UnifiedSetupConstants.InstDirRegKeyName,
                    null);
                SetupParameters.Instance().CXInstallDir = csInstallDirectory;

                // Fetch server mode (CS/PS).
                // Get CX_TYPE value from amethyst.conf.
                Trc.Log(LogLevel.Always, "Fetching Server mode.");
                string csType =
                    GrepUtils.GetKeyValueFromFile(
                    Path.Combine(csInstallDirectory, UnifiedSetupConstants.AmethystFile),
                    UnifiedSetupConstants.ConfigurationType);
                Trc.Log(LogLevel.Always, "csType: {0}", csType);
                if (csType == "3")
                {
                    SetupParameters.Instance().ServerMode = UnifiedSetupConstants.CSServerMode;
                }
                else if (csType == "2")
                {
                    SetupParameters.Instance().ServerMode = UnifiedSetupConstants.PSServerMode;
                }

                // Fetch CS IP.
                // Read PS_CS_IP from amethst.conf.
                string amethystFilePath = Path.Combine(csInstallDirectory, UnifiedSetupConstants.AmethystFile);
                Trc.Log(LogLevel.Always, "Fetch CS IP from: {0}", amethystFilePath);
                SetupParameters.Instance().CSIP =
                    GrepUtils.GetKeyValueFromFile(
                    amethystFilePath,
                    UnifiedSetupConstants.PSCSIP_AmethystConf);

                // Fetch agent installation directory.
                // Fetch install dir from app.conf.
                if (!SetupParameters.Instance().PSGalleryImage)
                {
                    Trc.Log(LogLevel.Always, "Fetching agent installation directory.");
                    string agentInstDir =
                        GrepUtils.GetKeyValueFromFile(
                        sysDrive + UnifiedSetupConstants.AppConfFile,
                        UnifiedSetupConstants.InstPath_AppConf);
                    SetupParameters.Instance().AgentInstallDir = agentInstDir;
                }

                return true;
            }
            catch (Exception e)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while trying to fetch installation details: {0}", e.ToString());
                return status;
            }
        }

        /// <summary>
        /// Checks whether the junction directory exists or not, remove if it's exists.
        /// </summary>
        /// <returns>true if junction directory is removed, false otherwise</returns>
        public static bool IsJunctionDirectoryRemoved()
        {
            bool returnCode = false;
            try
            {
                string strProgramDataPath =
                    Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
                string junctionDirectoryPath =
                    Environment.ExpandEnvironmentVariables(strProgramDataPath) + "\\ASR\\";

                if (Directory.Exists(junctionDirectoryPath))
                {
                    Trc.Log(
                        LogLevel.Always,
                        "Attempting to delete a junction directory ({0}) ", junctionDirectoryPath);
                    Directory.Delete(junctionDirectoryPath, true);
                    Trc.Log(LogLevel.Always, "The junction directory is deleted successfully.");
                    return true;
                }
                else
                {
                    Trc.Log(
                        LogLevel.Always,
                        "The junction directory ({0}) doesn't exists.", junctionDirectoryPath);
                    return true;
                }
            }
            catch (Exception e)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while deleting junction directory: {0}", e.ToString());
            }
            return returnCode;
        }

        /// <summary>
        /// Creates shortcut on public desktop.
        /// </summary>
        /// <params> desktop shortcut link name and desktop shortcut actual exe path.
        public static void CreateShortCut(string linkName, string targetExePath)
        {
            try
            {
                Trc.Log(LogLevel.Always, "linkName: {0} ", linkName);
                Trc.Log(LogLevel.Always, "targetExePath: {0} ", targetExePath);

                WshShell wshshell = new WshShell();
                IWshShortcut shortCut = wshshell.CreateShortcut(
                    Environment.GetFolderPath(Environment.SpecialFolder.CommonDesktopDirectory) + "\\"+linkName+"") as IWshShortcut;
                shortCut.TargetPath = "" + targetExePath + "";
                shortCut.Save();
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception Occured while creating shortcut: {0}", e.ToString());
            }
        }

        /// <summary>
        /// Deletes shortcut from current user's desktop and public desktop.
        /// </summary>
        /// <param name="shortcutName">Shortcut name.</param>
        public static void DeleteShortCut(string shortcutName)
        {
            // Delete shortcut from user's desktop.
            string userDesktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
            if (System.IO.File.Exists(Path.Combine(userDesktopPath, shortcutName)))
            {
                System.IO.File.Delete(Path.Combine(userDesktopPath, shortcutName));
                Trc.Log(LogLevel.Always, "Shortcut {0} exists. Removed it successfully.", Path.Combine(userDesktopPath, shortcutName));
            }
            else
            {
                Trc.Log(LogLevel.Always, "Shortcut {0} didn't exist.", Path.Combine(userDesktopPath, shortcutName));
            }

            // Delete shortcut from public desktop.
            string publicDesktopPath = Environment.GetFolderPath(Environment.SpecialFolder.CommonDesktopDirectory);
            if (System.IO.File.Exists(Path.Combine(publicDesktopPath, shortcutName)))
            {
                System.IO.File.Delete(Path.Combine(publicDesktopPath, shortcutName));
                Trc.Log(LogLevel.Always, "Shortcut {0} exists. Removed it successfully.", Path.Combine(publicDesktopPath, shortcutName));
            }
            else
            {
                Trc.Log(LogLevel.Always, "Shortcut {0} didn't exist.", Path.Combine(publicDesktopPath, shortcutName));
            }
        }

        /// <summary>
        /// Create CspsConfigtool desktop shortcut.
        /// </summary>
        private static void CreateCspsConfigtoolShortcut()
        {
            try
            {
                Trc.Log(LogLevel.Always, "Creating Cspsconfigtool desktop shortcut.");
                string targetExePath =
                    Path.Combine(
                    SetupParameters.Instance().CXInstallDir,
                    UnifiedSetupConstants.CspsconfigtoolExePath);
                CreateShortCut(
                    UnifiedSetupConstants.CSPSConfigtoolShortcutName,
                    targetExePath);
            }
            catch (Exception ex)
            {
                Trc.LogException(
                    LogLevel.Error,
                    "Exception occurred while creating cspsconfigtool desktop shortcut.", ex);
            }
        }

        /// <summary>
        /// Remove unused registry keys during upgrade.
        /// </summary>
        public static void RemoveUnUsedRegistryKeys()
        {
            try
            {
                // List of registry keys to be removed.
                List<string> registryList = new List<string>
                {
                    UnifiedSetupConstants.DRAInstStatusRegKeyName,
                    UnifiedSetupConstants.DRARegStatusRegKeyName,
                    UnifiedSetupConstants.DRAServiceStatusRegKeyName,
                    UnifiedSetupConstants.MARSInstStatusRegKeyName,
                    UnifiedSetupConstants.RegisterMTStatusRegKeyName,
                    UnifiedSetupConstants.MARSServiceStatusRegKeyName,
                    UnifiedSetupConstants.MARSProxyStatusRegKeyName,
                    UnifiedSetupConstants.ContainerRegStatusRegKeyName
                };

                // Looping through the registry key list.
                foreach (string regKey in registryList)
                {
                    RemoveRegistryKey(UnifiedSetupConstants.LocalMachineCSPSRegName, regKey);
                }

                RemoveRegistryKey(
                    UnifiedSetupConstants.LocalMachineMSMTRegName,
                    UnifiedSetupConstants.RebootRequiredRegKeyName);
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while removing unused registry keys: {0}", ex.ToString());
            }
        }

        /// <summary>
        /// Remove registry key.
        /// </summary>
        /// <param name="registryName">Name of registry where required key exists</param>
        /// <param name="KeyName">Registry key to be deleted</param>
        private static bool RemoveRegistryKey(string registryName, string KeyName)
        {
            try
            {
                RegistryKey regKey = Registry.LocalMachine.OpenSubKey(registryName, true);

                if (Registry.GetValue(regKey.ToString(), KeyName, null) == null)
                {
                    Trc.Log(LogLevel.Always, "{0} registry entry doesn't exist.", KeyName);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Deleting {0} registry key.", KeyName);
                    regKey.DeleteValue(KeyName);
                }
                regKey.Close();
                return true;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while removing the {0} registry key: {1}",
                    KeyName,
                    ex.ToString());
                return false;
            }
        }

        /// <summary>
        /// Logs all the setup parameter values.
        /// </summary>
        public static void LogAllSetupParams()
        {
            Trc.Log(
                LogLevel.Always,
                "Agent installation directory: {0}", SetupParameters.Instance().AgentInstallDir);
            Trc.Log(
                LogLevel.Always,
                "Agent role: {0}", SetupParameters.Instance().AgentRole);
            Trc.Log(
                LogLevel.Always,
                "Communication mode: {0}", SetupParameters.Instance().CommMode);
            Trc.Log(
                LogLevel.Always,
                "CS IP: {0}", SetupParameters.Instance().CSIP);
            Trc.Log(
                LogLevel.Always,
                "CS Port: {0}", SetupParameters.Instance().CSPort);
            Trc.Log(
                LogLevel.Always,
                "CX installation directory: {0}", SetupParameters.Instance().CXInstallDir);
            Trc.Log(
                LogLevel.Always,
                "Environment type: {0}", SetupParameters.Instance().EnvType);
            Trc.Log(
                LogLevel.Always,
                "Installation type: {0}", SetupParameters.Instance().InstallType);
            Trc.Log(
                LogLevel.Always,
                "Mysql file path: {0}", SetupParameters.Instance().MysqlCredsFilePath);
            Trc.Log(
                LogLevel.Always,
                "Passphrase file path: {0}", SetupParameters.Instance().PassphraseFilePath);
            Trc.Log(
                LogLevel.Always,
                "Source config file path: {0}", SetupParameters.Instance().SourceConfigFilePath);
            Trc.Log(
                LogLevel.Always,
                "Proxy settings file path: {0}", SetupParameters.Instance().ProxySettingsFilePath);
            Trc.Log(
                LogLevel.Always,
                "Proxy type: {0}", SetupParameters.Instance().ProxyType);
            Trc.Log(
                LogLevel.Always,
                "PS IP: {0}", SetupParameters.Instance().PSIP);
            Trc.Log(
                LogLevel.Always,
                "AZURE IP: {0}", SetupParameters.Instance().AZUREIP);
            Trc.Log(
                LogLevel.Always,
                "Re installation status: {0}", SetupParameters.Instance().ReInstallationStatus);
            Trc.Log(
                LogLevel.Always, "Server mode: {0}", SetupParameters.Instance().ServerMode);
            Trc.Log(
                LogLevel.Always,
                "Skip space check: {0}", SetupParameters.Instance().SkipSpaceCheck);
            Trc.Log(
                LogLevel.Always,
                "Valut file path: {0}", SetupParameters.Instance().ValutCredsFilePath);
        }

        /// <summary>
        /// Call Purge DRA registry method to clean up DRA related registries.
        /// </summary>
        public static void CallPurgeDRARegistryMethod()
        {
            try
            {
                Trc.Log(LogLevel.Always, "Invoking PurgeDraRegistry method.");
                DRSetup.RegistryOperations.PurgeDraRegistry();
            }
            catch (Exception ex)
            {
                Trc.LogException(
                    LogLevel.Error,
                    "Exception occurred at CallPurgeDRARegistryMethod : {0}",
                    ex);
            }
        }

        /// <summary>
        /// Check for .NET Framework 4.6.2 and above.
        /// </summary>
        /// <returns>true if available, false otherwise.</returns>
        public static bool CheckNETFramework462OrAbove()
        {
            const string NetFrameworkSubkey = UnifiedSetupConstants.NETFrameworkRegkey;
            bool returnValue = false;

            try
            {
                using (RegistryKey ndpKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32).OpenSubKey(NetFrameworkSubkey))
                {
                    if (ndpKey != null && ndpKey.GetValue("Release") != null)
                    {
                        Trc.Log(LogLevel.Always, ".NET Framework Version {0} installed", ndpKey.GetValue("Version"));

                        if ((int)ndpKey.GetValue("Release") >= 394802)
                        {
                            Trc.Log(LogLevel.Error, ".NET Framework Version 4.6.2 or later is detected.");
                            returnValue = true;
                        }
                    }
                    else
                    {
                        Trc.Log(LogLevel.Error, ".NET Framework Version 4.5 or later is not detected.");
                    }
                }
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred at CheckforNETFramework462AndAbove : {0} {1}",
                    ex.StackTrace,
                    ex.Message);
            }

            return returnValue;
        }

        /// <summary>
        /// Download MySQL setup file and returns true on successful download or required file already exist.
        /// </summary>
        public static bool DownloadMySQLSetupFile(string version)
        {
            bool returnCode = false;
            try
            {
                ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;
                System.Net.WebClient webClient = new System.Net.WebClient();
                string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);
                string dir = sysDrive + @"Temp\ASRSetup";
                Directory.CreateDirectory(dir);
                string proxyType = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.ProxyType);
                string mySqlSetupHashData = string.Empty;
                string sourceFilePath = string.Empty;
                string destinationFilePath = string.Empty;

                if (version == UnifiedSetupConstants.MySQL57Version)
                {
                    mySqlSetupHashData = "54D6526FBFA9EDA6BB680B960D570B99";
                    sourceFilePath = "https://cdn.mysql.com/archives/mysql-installer/mysql-installer-community-5.7.20.0.msi";
                    destinationFilePath = sysDrive + @"Temp\ASRSetup\mysql-installer-community-5.7.20.0.msi";
                }
                else if (version == UnifiedSetupConstants.MySQL55Version)
                {
                    mySqlSetupHashData = "CEA61C5BC73C0ACC4E95E7B884B26C17";
                    sourceFilePath = "https://dev.mysql.com/get/archives/mysql-5.5/mysql-5.5.37-win32.msi";
                    destinationFilePath = sysDrive + @"Temp\ASRSetup\mysql-5.5.37-win32.msi";
                }

                switch (proxyType)
                {
                    case "Bypass":
                        // For CS downloading MySQL Setup file and keeping under <SystemDrive>:\Temp\ASRSetup\mysql-installer-community-5.7.20.0.msi
                        // Not downloading if MySQL already installed or MySQL Setup file already exists in <SystemDrive>:\Temp\ASRSetup path.
                        Trc.Log(LogLevel.Always, "Using Bypass proxy settings...");
                        Trc.Log(LogLevel.Always, "Is MySql setup file - {0} already exists: {1}",
                            version, (System.IO.File.Exists(destinationFilePath)).ToString());
                        for (int i = 1; i <= 3; i++)
                        {
                            if (!(System.IO.File.Exists(destinationFilePath)) || 
                                !(SetupHelper.ValidateMySQLMD5HashData(destinationFilePath, mySqlSetupHashData)))
                            {
                                Trc.Log(LogLevel.Always, "Downloading MySQL file.");
                                //Downloading MySQL Setup file if MD5 hash data did not match
                                if (System.IO.File.Exists(destinationFilePath))
                                {
                                    System.IO.File.Delete(destinationFilePath);
                                }

                                webClient.Proxy = null;
                                webClient.DownloadFile(sourceFilePath, destinationFilePath);
                                webClient.Dispose();

                                if ((System.IO.File.Exists(destinationFilePath)) && 
                                    SetupHelper.ValidateMySQLMD5HashData(destinationFilePath, mySqlSetupHashData))
                                {
                                    Trc.Log(LogLevel.Always, "MySQL setup file - {0} is successfully downloaded.", version);
                                    returnCode = true;
                                    break;
                                }
                                else
                                {
                                    Trc.Log(LogLevel.Always, "Unable to download MySQL setup file - {0}. re-trying count {1}", version, i);
                                    continue;
                                }
                            }
                            else
                            {
                                Trc.Log(LogLevel.Always, "MySQL setup file - {0} already exists.", version);
                                returnCode = true;
                            }
                        }
                        break;

                    case "Custom":
                        // For CS downloading MySQL Setup file and keeping under <SystemDrive>:\Temp\ASRSetup\mysql-installer-community-5.7.20.0.msi
                        // Not downloading if MySQL already installed or MySQL Setup file already exists in <SystemDrive>:\Temp\ASRSetup path.
                        //string isMySqlSetupFileDownloaded = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.mysqlsetupfileexists);
                        Trc.Log(LogLevel.Always, "Using Custom proxy settings...");
                        Trc.Log(LogLevel.Always, "Is MySql setup file already exists: {0}", 
                            (System.IO.File.Exists(destinationFilePath)).ToString());

                        for (int i = 1; i <= 3; i++)
                        {
                            if (!(System.IO.File.Exists(destinationFilePath)) || 
                                !(SetupHelper.ValidateMySQLMD5HashData(destinationFilePath, mySqlSetupHashData)))
                            {
                                Trc.Log(LogLevel.Always, "Downloading MySQL file.");

                                //Downloading MySQL Setup file, if setup file does not exists or MD5SUM did not match
                                if (System.IO.File.Exists(destinationFilePath))
                                {
                                    System.IO.File.Delete(destinationFilePath);
                                }
                                WebProxy customProxy = SetupHelper.BuildWebProxyFromPropertyBag();
                                webClient.Proxy = customProxy;
                                webClient.DownloadFile(sourceFilePath, destinationFilePath);
                                webClient.Dispose();

                                if ((System.IO.File.Exists(destinationFilePath)) &&
                                    SetupHelper.ValidateMySQLMD5HashData(destinationFilePath, mySqlSetupHashData))
                                {
                                    Trc.Log(LogLevel.Always, "MySQL setup file - {0} is successfully downloaded.", version);
                                    returnCode = true;
                                    break;
                                }
                                else
                                {
                                    Trc.Log(LogLevel.Always, "Unable to download MySQL setup file - {0}. re-trying count {0}", version, i);
                                    continue;
                                }
                            }
                            else
                            {
                                Trc.Log(LogLevel.Always, "MySQL setup file - {0} already exists.", version);
                                returnCode = true;
                                break;
                            }
                        }
                        break;
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(
                    LogLevel.Always,
                    string.Format("Unable to download required MySQL version."),
                    ex);
            }

            return returnCode;
        }

        /// <summary>
        /// Fetch ACS URL and Geo name from configuration server using API.
        /// </summary>
        public static bool FetchAcsUrlGeoName()
        {
            bool status = false;

            try
            {
                ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "Fetch the ACS URL and Geo name from configuration server.");

                FunctionRequest funcReq = new FunctionRequest();
                funcReq.Name = UnifiedSetupConstants.Get_AcsUrl_API;
                funcReq.Id = "1";
                funcReq.Include = false;

                // Add the FunctionRequest instance to ArraryList.
                ArrayList funcReqs = new ArrayList();
                funcReqs.Add(funcReq);

                // Post the request.
                string csIP = SetupParameters.Instance().CSIP;
                int csPort = UnifiedSetupConstants.csSecurePort;
                string protocol = UnifiedSetupConstants.protocol;
                string hostGUID = UnifiedSetupConstants.InMageProfilerhostGUID;

                InMage.APIModel.Response res = CXAPI.post(funcReqs, csIP, csPort, protocol, hostGUID);
                ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "Return code from configuration server :{0}", res.Returncode);

                // Return code value of response object.
                if (res.Returncode == "0")
                {
                    Function func = ResponseParser.GetFunction(res, funcReq.Name);
                    ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "Function return code from API:{0}", func.Returncode);
                    if (func.Returncode == "0")
                    {
                        foreach (Object paramObj in func.FunctionResponse.ChildList)
                        {
                            if (paramObj is Parameter)
                            {
                                Parameter param = paramObj as Parameter;
                                ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Always, "{0}: {1}", param.Name, param.Value);
                                if (String.Equals(param.Name, "AcsUrl", StringComparison.OrdinalIgnoreCase))
                                {
                                    DRSetup.PropertyBagDictionary.Instance.SafeAdd(
                                        ASRSetupFramework.PropertyBagConstants.AcsUrl,
                                        param.Value);
                                }
                                else if (String.Equals(param.Name, "GeoName", StringComparison.OrdinalIgnoreCase))
                                {
                                    DRSetup.PropertyBagDictionary.Instance.SafeAdd(
                                        ASRSetupFramework.PropertyBagConstants.VaultLocation,
                                        param.Value);
                                }
                                else if (String.Equals(param.Name, "PrivateEndpointState", StringComparison.OrdinalIgnoreCase))
                                {
                                    DRSetup.PropertyBagDictionary.Instance.SafeAdd(
                                        ASRSetupFramework.PropertyBagConstants.PrivateEndpointState,
                                        param.Value);
                                }
                                else if (String.Equals(param.Name, "ResourceId", StringComparison.OrdinalIgnoreCase))
                                {
                                    DRSetup.PropertyBagDictionary.Instance.SafeAdd(
                                        ASRSetupFramework.PropertyBagConstants.ResourceId,
                                        param.Value);
                                }
                            }
                        }

                        status = true;
                    }
                    else
                    {
                        ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Error, "CXAPI function call failed with the error: {0}", func.Message);
                        ASRSetupFramework.SetupHelper.ShowError(ASRResources.StringResources.AcsUrlAPIFailureResponse);
                    }
                }
                // All other failures.
                else
                {
                    ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Error, "CXAPI request failed with the error: {0}", res.Message);
                    ASRSetupFramework.SetupHelper.ShowError(ASRResources.StringResources.AcsUrlAPIFailureResponse);
                }

            }
            catch (Exception ex)
            {
                ASRSetupFramework.Trc.Log(ASRSetupFramework.LogLevel.Error, "Exception occurred: ", ex);
                ASRSetupFramework.SetupHelper.ShowError(ASRResources.StringResources.AcsUrlAPIFailureResponse);
            }

            return status;
        }

        /// <summary>
        /// Checks private endpoint status.
        /// </summary>
        /// <returns>true if private endpoint enabled, false otherwise</returns>
        public static bool IsPrivateEndpointStateEnabled()
        {
            bool result = false;

            string isPrivateEndpointEnabled =
                DRSetup.PropertyBagDictionary.Instance.GetProperty<string>(
                    DRSetup.PropertyBagConstants.PrivateEndpointState);

            if (!string.IsNullOrWhiteSpace(isPrivateEndpointEnabled) &&
                string.Equals(
                    isPrivateEndpointEnabled,
                    DRSetup.DRSetupConstants.PrivateEndpointState.Enabled.ToString(),
                    StringComparison.InvariantCultureIgnoreCase))
            {
                result = true;
            }

            return result;
        }

        // <summary>
        /// Changes the log upload service type to manual.
        /// </summary>
        public static void ChangeLogUploadServiceToManual()
        {
            string privateEndpointState = (string)Registry.GetValue(
                        UnifiedSetupConstants.CSPSRegistryName,
                        UnifiedSetupConstants.PrivateEndpointState,
                        null);

            if (privateEndpointState == DRSetup.DRSetupConstants.PrivateEndpointState.Enabled.ToString())
            {
                if (ServiceControlFunctions.IsInstalled(DRSetup.DRSetupConstants.LogUploadServiceName))
                {
                    if (ServiceControlFunctions.StopService(DRSetup.DRSetupConstants.LogUploadServiceName))
                    {
                        ServiceControlFunctions.ChangeServiceStartupType(
                            DRSetup.DRSetupConstants.LogUploadServiceName,
                            NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START);
                    }
                }
            }
        }

        #endregion

        #region RecordOperationMethods
        /// <summary>
        /// Record component installation status.
        /// </summary>
        private static void RecordOperation(OperationName operationName, int returnCode, string output = "", string exceptionMsg = "")
        {
            switch(operationName)
            {
                // Record MySQL download status.
                case OperationName.DownloadMySql:
                    if (returnCode == 0)
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.DownloadMySql);
                    }
                    else
                    {
                        FailureRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.DownloadMySql,
                            string.Format(
                                StringResources.RecordOperationFailureMessage,
                                returnCode),
                            string.Format(
                            StringResources.RecordOperationRecommendationMessage,
                            sysDrive + UnifiedSetupConstants.ASRUnifiedSetupLog),
                            errorCode: returnCode);
                    }
                    break;


                // Record Internet Information Services installation status.
                case OperationName.InstallIIS:
                    if (returnCode == 0)
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallIIS);
                    }
                    else if (returnCode == 3010)
                    {
                        FailureRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallIIS,
                            string.Format(
                                StringResources.RecordOperationFailureMessage,
                                returnCode),
                            StringResources.IIS3010RebootRequired,
                            errorCode: returnCode);
                    }
                    else
                    {
                        FailureRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallIIS,
                            string.Format(
                                StringResources.RecordOperationFailureMessage,
                                returnCode),
                            string.Format(
                            StringResources.RecordOperationRecommendationMessage,
                            sysDrive + UnifiedSetupConstants.ASRUnifiedSetupLog),
                            exceptionMsg,
                            errorCode: returnCode);
                    }
                    break;

                // Record configuration/process server dependencies installation status.
                case OperationName.InstallTp:
                    if (returnCode == 0)
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallTp);
                    }
                    else
                    {
                        FailureRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallTp,
                            string.Format(
                                StringResources.RecordOperationFailureMessage,
                                returnCode),
                            string.Format(
                            StringResources.RecordOperationRecommendationMessage,
                            sysDrive + UnifiedSetupConstants.CXTPInstallLog),
                            errorCode: returnCode);
                    }
                    break;

                // Record configuration/process server installation status.
                case OperationName.InstallCs:
                case OperationName.InstallPs:
                    if (returnCode == 0)
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Installation,
                            operationName);
                    }
                    else
                    {
                        FailureRecordOperation(
                            ExecutionScenario.Installation,
                            operationName,
                            string.Format(
                                StringResources.RecordOperationFailureMessage,
                                returnCode),
                            string.Format(
                            StringResources.RecordOperationRecommendationMessage,
                            sysDrive + UnifiedSetupConstants.CXInstallLog),
                            errorCode: returnCode);
                    }
                    break;

                // Record DRA installation status.
                case OperationName.InstallDra:
                    if (returnCode == 0)
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallDra);
                    }
                    else
                    {
                        string errorCodeName = ParseIntToSetupReturnValue(returnCode);

                        FailureRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallDra,
                            string.Format(
                                StringResources.RecordOperationFailureMessage,
                                returnCode),
                            string.Format(
                            StringResources.RecordOperationRecommendationMessage,
                            sysDrive + UnifiedSetupConstants.DRASetupLog),
                            exceptionMsg,
                            returnCode,
                            errorCodeName);
                    }
                    break;

                // Record Master target installation status.
                case OperationName.InstallMt:
                    if (returnCode == 0)
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallMt);
                    }
                    else if ((returnCode == 3010) || (returnCode == 1641) || (returnCode == 98))
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallMt,
                            StringResources.RecordOperationRebootMessage);
                    }
                    else
                    {
                        FailureRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallMt,
                            string.Format(
                                StringResources.RecordOperationFailureMessage,
                                returnCode),
                            string.Format(
                            StringResources.RecordOperationRecommendationMessage,
                            sysDrive + UnifiedSetupConstants.ASRUnifiedAgentInstallLog),
                            errorCode: returnCode);
                    }
                    break;

                // Record Master target configuration status.
                case OperationName.ConfigureMt:
                    if (returnCode == 0)
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Configuration,
                            OperationName.ConfigureMt);
                    }
                    else
                    {
                        FailureRecordOperation(
                            ExecutionScenario.Configuration,
                            OperationName.ConfigureMt,
                            string.Format(
                                StringResources.RecordOperationFailureMessage,
                                returnCode),
                            string.Format(
                            StringResources.RecordOperationRecommendationMessage,
                            sysDrive + UnifiedSetupConstants.ASRUnifiedAgentConfiguratorLog),
                            errorCode: returnCode);
                    }
                    break;

                // Record MARS installation status.
                case OperationName.InstallMars:
                    if (returnCode == 0)
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallMars);
                    }
                    else
                    {
                        FailureRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallMars,
                            string.Format(
                                StringResources.RecordOperationFailureMessage,
                                returnCode),
                            string.Format(
                            StringResources.RecordOperationRecommendationMessage,
                            sysDrive + UnifiedSetupConstants.MARSStatusLog),
                            errorCode: returnCode);
                    }
                    break;

                // Record Configuration Manager installation status.
                case OperationName.InstallConfigManager:
                    if (returnCode == 0)
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallConfigManager);
                    }
                    else
                    {
                        FailureRecordOperation(
                            ExecutionScenario.Installation,
                            OperationName.InstallConfigManager,
                            string.Format(
                                StringResources.RecordOperationFailureMessage,
                                returnCode),
                            string.Format(
                            StringResources.RecordOperationRecommendationMessage,
                            sysDrive + UnifiedSetupConstants.ConfigManagerStatusLog),
                            errorCode: returnCode);
                    }
                    break;

                // Record DRA registration status.
                case OperationName.RegisterDra:
                    if (returnCode == 0)
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Configuration,
                            OperationName.RegisterDra);
                    }
                    else
                    {
                        string errorCodeName = ParseIntToSetupReturnValue(returnCode);

                        FailureRecordOperation(
                            ExecutionScenario.Configuration,
                            OperationName.RegisterDra,
                            string.Format(
                                StringResources.RecordOperationFailureMessagewithOutput,
                                returnCode,
                                output),
                            string.Format(
                            StringResources.RecordOperationRecommendationMessage,
                            sysDrive + UnifiedSetupConstants.DRASetupLog),
                            exceptionMsg,
                            returnCode,
                            errorCodeName);
                    }
                    break;

                // Record Container registration status.
                case OperationName.RegisterCloudContainer:
                    if (returnCode == 0)
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Configuration,
                            OperationName.RegisterCloudContainer);
                    }
                    else
                    {
                        string errorCodeName = ParseIntToSetupReturnValue(returnCode);

                        FailureRecordOperation(
                            ExecutionScenario.Configuration,
                            OperationName.RegisterCloudContainer,
                            string.Format(
                                StringResources.RecordOperationFailureMessagewithOutput,
                                returnCode,
                                output),
                            string.Format(
                            StringResources.RecordOperationRecommendationMessage,
                            sysDrive + UnifiedSetupConstants.DRASetupLog),
                            exceptionMsg,
                            returnCode,
                            errorCodeName);
                    }
                    break;

                // Record MT registration status.
                case OperationName.RegisterMt:
                    if (returnCode == 0)
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Configuration,
                            OperationName.RegisterMt);
                    }
                    else
                    {
                        FailureRecordOperation(
                            ExecutionScenario.Configuration,
                            OperationName.RegisterMt,
                            string.Format(
                                StringResources.RecordOperationFailureMessage,
                                returnCode),
                            string.Format(
                            StringResources.RecordOperationRecommendationMessage,
                            sysDrive + UnifiedSetupConstants.ASRUnifiedSetupLog),
                            errorCode: returnCode);
                    }
                    break;

                // Record setting MARS proxy status.
                case OperationName.SetMarsProxy:
                    if (returnCode == 0)
                    {
                        SucessRecordOperation(
                            ExecutionScenario.Configuration,
                            OperationName.SetMarsProxy);
                    }
                    else
                    {
                        FailureRecordOperation(
                            ExecutionScenario.Configuration,
                            OperationName.SetMarsProxy,
                            string.Format(
                                StringResources.RecordOperationFailureMessage,
                                returnCode),
                            string.Format(
                            StringResources.RecordOperationRecommendationMessage,
                            sysDrive + UnifiedSetupConstants.ASRUnifiedSetupLog),
                            errorCode: returnCode);
                    }
                    break;

                // Un-supported Operation.
                default:
                    Trc.Log(LogLevel.Error, "Un-supported operation name.");
                    break;
            }
        }

        /// <summary>
        /// Record component installation status as success.
        /// </summary>
        /// <param name="executionScenario">Pre/Mid/Post installation.</param>
        /// <param name="operationName">name of the operation.</param>
        public static void SucessRecordOperation(
            ExecutionScenario executionScenario,
            OperationName operationName,
            string successMsg = null,
            string recommendationMsg = null)
        {
            IntegrityCheckWrapper.RecordOperation(
                executionScenario,
                operationName,
                Microsoft.DisasterRecovery.IntegrityCheck.OperationResult.Success,
                successMsg,
                null,
                recommendationMsg,
                null);
        }

        /// <summary>
        /// Record component installation status as failed.
        /// </summary>
        /// <param name="executionScenario">Pre/Mid/Post installation.</param>
        /// <param name="operationName">name of the operation.</param>
        /// <param name="errorMsg">The error message.</param>
        /// <param name="recommendationMsg">Recommendation message to the user.</param>
        public static void FailureRecordOperation(
            ExecutionScenario executionScenario,
            OperationName operationName,
            string errorMsg,
            string recommendationMsg,
            string exceptionMsg = null,
            int errorCode = 0,
            string errorCodeName = "")
        {
            IntegrityCheckWrapper.RecordOperation(
                executionScenario,
                operationName,
                Microsoft.DisasterRecovery.IntegrityCheck.OperationResult.Failed,
                errorMsg,
                null,
                recommendationMsg,
                exceptionMsg,
                errorCode,
                errorCodeName);
        }

        /// <summary>
        /// Record component installation status as skip.
        /// </summary>
        /// <param name="executionScenario">Pre/Mid/Post installation.</param>
        /// <param name="operationName">name of the operation.</param>
        /// <param name="message">message to be appended.</param>
        public static void SkipRecordOperation(
            ExecutionScenario executionScenario,
            OperationName operationName,
            string message)
        {
            IntegrityCheckWrapper.RecordOperation(
                executionScenario,
                operationName,
                Microsoft.DisasterRecovery.IntegrityCheck.OperationResult.Skip,
                message,
                null,
                null,
                null);
        }

        /// <summary>
        /// Gets the hostname of the system. On error, returns emtpy.
        /// </summary>
        /// <returns></returns>
        public static string GetHostName(bool thowExpOnFailure = false)
        {
            try
            {
                return Dns.GetHostName();
            }
            catch(SocketException)
            {
                if (thowExpOnFailure)
                {
                    throw;
                }
                else
                {
                    return string.Empty;
                }
            }
        }

        /// <summary>
        /// Integrity check upload summary invocation.
        /// </summary>
        public static void UploadOperationSummary()
        {
            try
            {
                Trc.Log(LogLevel.Always, "Uploading operation summary logs.");

                // TODO: Hanlde for Scale out unit required. As of now not handling.
                if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
                {
                    return;
                }

                if (IsPrivateEndpointStateEnabled())
                {
                    // When private endpoint is enabled we do not upload logs to telemtry.
                    Trc.Log(LogLevel.Always, "Not uploading telemetry as private endpoint is enabled.");
                    return;
                }

                // Get the version of the current installer.
                Version currentBuildVersion = GetBuildVersion(UnifiedSetupConstants.UnifiedSetupExeName);

                SetupProperties setupProperties = new SetupProperties();
                setupProperties.HostName = GetHostName();
                setupProperties.ProductName = UnifiedSetupConstants.UnifiedSetupProductName;
                setupProperties.MachineIdentifier = PropertyBagDictionary.Instance.GetProperty<Guid>(
                        PropertyBagConstants.MachineIdentifier);
                setupProperties.CSPSMTVersion = SetupParameters.Instance().CurrentBuildVersion.ToString();

                if (string.IsNullOrEmpty(sysDrive))
                {
                    sysDrive = Path.GetPathRoot(Environment.SystemDirectory);
                }

                if (string.IsNullOrEmpty(SetupParameters.Instance().CXInstallDir))
                {
                    SetupParameters.Instance().CXInstallDir = (string)Registry.GetValue(
                        UnifiedSetupConstants.CSPSRegistryName,
                        UnifiedSetupConstants.InstDirRegKeyName,
                        string.Empty);
                }

                // Check for test environment
                string environment = ConfigurationManager.AppSettings.Get(UnifiedSetupConstants.Environment);
                if (!string.IsNullOrWhiteSpace(environment)
                    && environment.Equals(UnifiedSetupConstants.TestEnvironment, StringComparison.InvariantCultureIgnoreCase))
                {
                    // Skip server certificate validation.
                    Trc.Log(LogLevel.Always, "Environment found: {0}. Skipping Cert validation", environment);
                    ServicePointManager.ServerCertificateValidationCallback +=
                                new RemoteCertificateValidationCallback(RemoteCertValidate);
                }

                string iisLogFileName;
                if (Directory.Exists(sysDrive + UnifiedSetupConstants.WorldWideWebServiceLogPath))
                {
                    DirectoryInfo iisDirectory = new DirectoryInfo(sysDrive + UnifiedSetupConstants.WorldWideWebServiceLogPath);
                    iisLogFileName = iisDirectory.GetFiles().OrderByDescending(f => f.LastWriteTime).First().ToString();
                }
                else
                {
                    iisLogFileName = "";
                }

                // Component Status.
                bool isCXTPInstalled = SetupParameters.Instance().IsCXTPInstalled;
                bool isCSInstalled = SetupParameters.Instance().IsCSInstalled;
                bool isMTInstalled = SetupParameters.Instance().IsMTInstalled;
                bool isMTConfigured = SetupParameters.Instance().IsMTConfigured;
                bool isDRAInstalled = SetupParameters.Instance().IsDRAInstalled;
                bool isDRARegistered = SetupParameters.Instance().IsDRARegistered;
                bool isIISInstalled = SetupParameters.Instance().IsIISInstall;
                bool isMySQLDownloaded = SetupParameters.Instance().IsMySQLDownloaded;
                bool isMARSInstalled = SetupParameters.Instance().IsMARSInstalled;
                bool isMTRegistered = SetupParameters.Instance().IsMTRegistered;
                bool isDRAServiceRunning = ServiceControlFunctions.IsEnabledAndRunning(
                    UnifiedSetupConstants.DRServiceName);
                bool isWorldWideWebServiceRunning = ServiceControlFunctions.IsEnabledAndRunning(
                    UnifiedSetupConstants.WorldWideWebServiceName);
                bool isTmansvcServiceRunning = ServiceControlFunctions.IsEnabledAndRunning(
                    UnifiedSetupConstants.TmansvcServiceName);
                bool isPushInstallServiceRunning = ServiceControlFunctions.IsEnabledAndRunning(
                    UnifiedSetupConstants.PushInstallServiceName);
                bool isCxPsServiceRunning = ServiceControlFunctions.IsEnabledAndRunning(
                    UnifiedSetupConstants.CxPsServiceName);
                bool isMarsServiceRunning = ServiceControlFunctions.IsEnabledAndRunning(
                    UnifiedSetupConstants.MARSServiceName);
                bool isInstallationSuccess = SetupParameters.Instance().IsInstallationSuccess;

                bool isConfigManagerInstalled = true;
                if (SetupParameters.Instance().OVFImage)
                {
                    isConfigManagerInstalled = SetupParameters.Instance().IsConfigManagerInstalled;
                }

                // Component log-names.
                string defaultDRALogFileName = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.DRAUTCLogName);
                string cxtpLogFile = sysDrive + UnifiedSetupConstants.CXTPInstallLog;
                string csLogFile = sysDrive + UnifiedSetupConstants.CXInstallLog;
                string mtInstLogFile = sysDrive + UnifiedSetupConstants.ASRUnifiedAgentInstallLog;
                string mtMSILogFile = sysDrive + UnifiedSetupConstants.ASRUnifiedAgentMSILogFile;
                string mtConfiguratorLogFile = sysDrive + UnifiedSetupConstants.ASRUnifiedAgentConfiguratorLog;
                string unifiedSetupLogFile = sysDrive + UnifiedSetupConstants.ASRUnifiedSetupInstallLog;
                string marsMSILogFile = sysDrive + UnifiedSetupConstants.MARSMsiLog;
                string marsMSIPatchLogFile = sysDrive + UnifiedSetupConstants.MARSPatchLog;
                string marsInstallLogFile = sysDrive + UnifiedSetupConstants.MARSInstallLog;
                string marsManagedInstallLogFile = sysDrive + UnifiedSetupConstants.MARSManagedInstallLog;
                string marsServiceLogFile = sysDrive + UnifiedSetupConstants.MARSServiceLog;
                string configManagerMSILogFile = sysDrive + UnifiedSetupConstants.ConfigManagerStatusLog;
                string unifiedSetupLogsPath = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
                    UnifiedSetupConstants.LogFolder);
                string worldWideWebServiceSystemLog = sysDrive + string.Format(
                    UnifiedSetupConstants.WorldWideWebServiceLog,
                    iisLogFileName);
                string worldWideWebServiceLogFile = Path.Combine(
                    unifiedSetupLogsPath,
                    iisLogFileName);
                string tmansvcServiceLogFile = Path.Combine(
                    SetupParameters.Instance().CXInstallDir,
                    UnifiedSetupConstants.TmansvcServiceLog);
                string pushInstallServiceLogFile = Path.Combine(
                    SetupParameters.Instance().CXInstallDir,
                    UnifiedSetupConstants.PushInstallServiceLog);
                string pushInstallConfFile = Path.Combine(
                    SetupParameters.Instance().CXInstallDir,
                    UnifiedSetupConstants.PushInstallConfFile);
                string cxpsErrorServiceLogFile = Path.Combine(SetupParameters.Instance().CXInstallDir,
                    UnifiedSetupConstants.CxPsErrorServiceLog);
                string cxpsMonitorServiceLogFile = Path.Combine(
                    SetupParameters.Instance().CXInstallDir,
                    UnifiedSetupConstants.CxPsMonitorServiceLog);
                string cxpsXferServiceLogFile = Path.Combine(
                    SetupParameters.Instance().CXInstallDir,
                    UnifiedSetupConstants.CxPsXferServiceLog);
                string dbUpgradeLogFile = Path.Combine(
                    SetupParameters.Instance().CXInstallDir,
                    UnifiedSetupConstants.DBUpgradeLog);
                string dbUpgradeErrorLogFile = Path.Combine(
                    SetupParameters.Instance().CXInstallDir,
                    UnifiedSetupConstants.DBUpgradeErrorLog);

                List<string> UnifiedSetupServiceLogs = GetUnifiedSetupServiceLogs();

                // Log upload flow.
                if (SetupParameters.Instance().InstallType == UnifiedSetupConstants.Upgrade)
                {
                    setupProperties.VaultLocation = (string)Registry.GetValue(
                        UnifiedSetupConstants.CSPSRegistryName,
                        UnifiedSetupConstants.VaultGeoRegKeyName,
                        string.Empty);

                    IntegrityCheckWrapper.UploadSummaryUsingDraRegistration(setupProperties);

                    setupProperties.ProductName = UnifiedSetupConstants.UnifiedSetupProductName;
                    setupProperties.FileName = Path.GetFileName(unifiedSetupLogFile);
                    Trc.Log(LogLevel.Always,
                        "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                        unifiedSetupLogFile);
                    IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                        setupProperties,
                        unifiedSetupLogFile);
                    
                    // All components installed successfully.
                    if (isInstallationSuccess)
                    {
                        setupProperties.ProductName = UnifiedSetupConstants.CSPSProductName;
                        setupProperties.FileName = Path.GetFileName(csLogFile);
                        Trc.Log(LogLevel.Always,
                                "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                csLogFile);
                        IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                            setupProperties,
                            csLogFile);

                        setupProperties.ProductName = UnifiedSetupConstants.MS_MTProductName;
                        setupProperties.FileName = Path.GetFileName(mtInstLogFile);
                        Trc.Log(LogLevel.Always,
                                        "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                        mtInstLogFile);
                        IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                            setupProperties,
                            mtInstLogFile);

                        setupProperties.FileName = Path.GetFileName(mtMSILogFile);
                        Trc.Log(LogLevel.Always,
                            "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                            mtMSILogFile);
                        IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                            setupProperties,
                            mtMSILogFile);
                    }
                    // Thirdparty install failure.
                    if (!isCXTPInstalled)
                    {
                        setupProperties.ProductName = UnifiedSetupConstants.CXTPProductName;
                        setupProperties.FileName = Path.GetFileName(cxtpLogFile);
                        Trc.Log(LogLevel.Always,
                            "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                            cxtpLogFile);
                        IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                            setupProperties,
                            cxtpLogFile);

                        setupProperties.ProductName = UnifiedSetupConstants.UnifiedSetupProductName;
                        setupProperties.FileName = Path.GetFileName(unifiedSetupLogFile);
                        Trc.Log(LogLevel.Always,
                            "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                            unifiedSetupLogFile);
                        IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                            setupProperties,
                            unifiedSetupLogFile);
                    }
                    // Thirdparty install success.
                    else
                    {
                        // CS install failure.
                        if (!isCSInstalled)
                        {
                            setupProperties.ProductName = UnifiedSetupConstants.CSPSProductName;
                            setupProperties.FileName = Path.GetFileName(csLogFile);
                            Trc.Log(LogLevel.Always,
                                "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                csLogFile);
                            IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                setupProperties,
                                csLogFile);

                            setupProperties.FileName = Path.GetFileName(dbUpgradeLogFile);
                            Trc.Log(LogLevel.Always,
                                "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                dbUpgradeLogFile);
                            IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                setupProperties,
                                dbUpgradeLogFile);

                            setupProperties.FileName = Path.GetFileName(dbUpgradeErrorLogFile);
                            Trc.Log(LogLevel.Always,
                                "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                dbUpgradeErrorLogFile);
                            IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                setupProperties,
                                dbUpgradeErrorLogFile);
                        }
                        // CS install success.
                        else
                        {
                            // DRA install failure.
                            if (!isDRAInstalled)
                            {
                                setupProperties.ProductName = UnifiedSetupConstants.AzureSiteRecoveryProductName;
                                setupProperties.FileName = Path.GetFileName(defaultDRALogFileName);
                                Trc.Log(LogLevel.Always,
                                    "Invoking UploadDraLogsUsingDraRegistration with file : {0}",
                                    defaultDRALogFileName);
                                IntegrityCheckWrapper.UploadDraLogsUsingDraRegistration(
                                    setupProperties,
                                    defaultDRALogFileName);
                            }
                            // DRA install success.
                            else
                            {
                                // MT install failure.
                                if (!isMTInstalled)
                                {
                                    setupProperties.ProductName = UnifiedSetupConstants.MS_MTProductName;
                                    setupProperties.FileName = Path.GetFileName(mtInstLogFile);
                                    Trc.Log(LogLevel.Always,
                                        "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                        mtInstLogFile);
                                    IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                        setupProperties,
                                        mtInstLogFile);

                                    setupProperties.FileName = Path.GetFileName(mtMSILogFile);
                                    Trc.Log(LogLevel.Always,
                                        "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                        mtMSILogFile);
                                    IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                        setupProperties,
                                        mtMSILogFile);
                                }
                                // MT install success.
                                else
                                {
                                    // MARS install failure.
                                    if (!isMARSInstalled)
                                    {
                                        setupProperties.ProductName = UnifiedSetupConstants.MARSAgentProductName;
                                        setupProperties.FileName = Path.GetFileName(marsMSILogFile);
                                        Trc.Log(LogLevel.Always,
                                            "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                            marsMSILogFile);
                                        IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                            setupProperties,
                                            marsMSILogFile);

                                        setupProperties.FileName = Path.GetFileName(marsMSIPatchLogFile);
                                        Trc.Log(LogLevel.Always,
                                            "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                            marsMSIPatchLogFile);
                                        IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                            setupProperties,
                                            marsMSIPatchLogFile);

                                        setupProperties.FileName = Path.GetFileName(marsInstallLogFile);
                                        Trc.Log(LogLevel.Always,
                                            "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                            marsInstallLogFile);
                                        IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                            setupProperties,
                                            marsInstallLogFile);

                                        setupProperties.FileName = Path.GetFileName(marsManagedInstallLogFile);
                                        Trc.Log(LogLevel.Always,
                                            "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                            marsManagedInstallLogFile);
                                        IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                            setupProperties,
                                            marsManagedInstallLogFile);
                                    }
                                    // MARS install success.
                                    else
                                    {
                                        // Configuration Manager install failure.
                                        if (!isConfigManagerInstalled)
                                        {
                                            setupProperties.ProductName = UnifiedSetupConstants.ConfigManagerProductName;
                                            setupProperties.FileName = Path.GetFileName(configManagerMSILogFile);
                                            Trc.Log(LogLevel.Always,
                                                "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                                configManagerMSILogFile);
                                            IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                                setupProperties,
                                                configManagerMSILogFile);
                                        }
                                        // Configuration Manager install success.
                                        else
                                        {
                                            // DRA service not running.
                                            if (!isDRAServiceRunning)
                                            {
                                                setupProperties.ProductName = UnifiedSetupConstants.DRServiceName;
                                                setupProperties.FileName = Path.GetFileName(defaultDRALogFileName);
                                                Trc.Log(LogLevel.Always,
                                                    "Invoking UploadDraLogsUsingDraRegistration with file : {0}",
                                                    defaultDRALogFileName);
                                                IntegrityCheckWrapper.UploadDraLogsUsingDraRegistration(
                                                    setupProperties,
                                                    defaultDRALogFileName);
                                            }

                                            // World Wide Web service not running.
                                            if (!isWorldWideWebServiceRunning)
                                            {
                                                setupProperties.ProductName = UnifiedSetupConstants.WorldWideWebServiceName;
                                                System.IO.File.Copy(worldWideWebServiceSystemLog, worldWideWebServiceLogFile, true);
                                                setupProperties.FileName = Path.GetFileName(worldWideWebServiceLogFile);
                                                Trc.Log(LogLevel.Always,
                                                    "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                                    worldWideWebServiceLogFile);
                                                IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                                    setupProperties,
                                                    worldWideWebServiceLogFile);
                                            }

                                            // Tmansvc service not running.
                                            if (!isTmansvcServiceRunning)
                                            {
                                                setupProperties.ProductName = UnifiedSetupConstants.TmansvcServiceName;
                                                setupProperties.FileName = Path.GetFileName(tmansvcServiceLogFile);
                                                Trc.Log(LogLevel.Always,
                                                    "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                                    tmansvcServiceLogFile);
                                                IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                                    setupProperties,
                                                    tmansvcServiceLogFile);
                                            }

                                            // Push Install service not running.
                                            if (!isPushInstallServiceRunning)
                                            {
                                                setupProperties.ProductName = UnifiedSetupConstants.PushInstallServiceName;
                                                setupProperties.FileName = Path.GetFileName(pushInstallServiceLogFile);
                                                Trc.Log(LogLevel.Always,
                                                    "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                                    pushInstallServiceLogFile);
                                                IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                                    setupProperties,
                                                    pushInstallServiceLogFile);

                                                setupProperties.FileName = Path.GetFileName(pushInstallConfFile);
                                                Trc.Log(LogLevel.Always,
                                                    "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                                    pushInstallConfFile);
                                                IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                                    setupProperties,
                                                    pushInstallConfFile);
                                            }

                                            // CxProcessServer service not running.
                                            if (!isCxPsServiceRunning)
                                            {
                                                setupProperties.ProductName = UnifiedSetupConstants.CxPsServiceName;
                                                setupProperties.FileName = Path.GetFileName(cxpsErrorServiceLogFile);
                                                Trc.Log(LogLevel.Always,
                                                    "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                                    cxpsErrorServiceLogFile);
                                                IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                                    setupProperties,
                                                    cxpsErrorServiceLogFile);

                                                setupProperties.FileName = Path.GetFileName(cxpsMonitorServiceLogFile);
                                                Trc.Log(LogLevel.Always,
                                                    "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                                    cxpsMonitorServiceLogFile);
                                                IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                                    setupProperties,
                                                    cxpsMonitorServiceLogFile);

                                                setupProperties.FileName = Path.GetFileName(cxpsXferServiceLogFile);
                                                Trc.Log(LogLevel.Always,
                                                    "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                                    cxpsXferServiceLogFile);
                                                IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                                    setupProperties,
                                                    cxpsXferServiceLogFile);
                                            }

                                            // Azure Recovery Services Agent service not running.
                                            if (!isMarsServiceRunning)
                                            {
                                                setupProperties.ProductName = UnifiedSetupConstants.MARSServiceName;
                                                setupProperties.FileName = Path.GetFileName(marsServiceLogFile);
                                                Trc.Log(LogLevel.Always,
                                                    "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                                                    marsServiceLogFile);
                                                IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                                                    setupProperties,
                                                    marsServiceLogFile);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Upload service logs for upgrade.
                    foreach (string logFile in UnifiedSetupServiceLogs)
                    {
                        setupProperties.ProductName = UnifiedSetupConstants.UnifiedSetupServiceLogsComponentName;
                        setupProperties.FileName = Path.GetFileName(logFile);
                        Trc.Log(
                            LogLevel.Always,
                            "Invoking UploadCustomLogsUsingDraRegistration with file : {0}",
                            logFile);
                         if (IntegrityCheckWrapper.UploadCustomLogsUsingDraRegistration(
                            setupProperties,
                            logFile))
                         {
                             System.IO.File.Delete(logFile);
                         }
                    }
                }
                else
                {
                    bool bypassProxy = true;
                    string proxyAddress = string.Empty;
                    string proxyPort = string.Empty;
                    string proxyUsername = string.Empty;
                    SecureString proxyPassword = null;

                    if (SetupParameters.Instance().ProxyType == UnifiedSetupConstants.CustomProxy)
                    {
                        bypassProxy = false;

                        proxyAddress = PropertyBagDictionary.Instance.GetProperty<string>(
                            PropertyBagConstants.ProxyAddress);
                        proxyPort = PropertyBagDictionary.Instance.GetProperty<string>(
                            PropertyBagConstants.ProxyPort);
                        proxyUsername = PropertyBagDictionary.Instance.GetProperty<string>(
                            PropertyBagConstants.ProxyUsername);
                        proxyPassword = PropertyBagDictionary.Instance.GetProperty<SecureString>(
                            PropertyBagConstants.ProxyPassword);
                    }

                    IntegrityCheckWrapper.UploadSummary(
                        SetupParameters.Instance().ValutCredsFilePath,
                        setupProperties,
                        bypassProxy,
                        proxyAddress,
                        proxyPort,
                        proxyUsername,
                        proxyPassword);

                    setupProperties.ProductName = UnifiedSetupConstants.UnifiedSetupProductName;
                    setupProperties.FileName = Path.GetFileName(unifiedSetupLogFile);
                    Trc.Log(LogLevel.Always,
                        "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                        unifiedSetupLogFile);
                    IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                        SetupParameters.Instance().ValutCredsFilePath,
                        setupProperties,
                        unifiedSetupLogFile,
                        1024,
                        bypassProxy,
                        proxyAddress,
                        proxyPort,
                        proxyUsername,
                        proxyPassword);

                    // All components installed successfully.
                    if (isInstallationSuccess)
                    {
                        setupProperties.ProductName = UnifiedSetupConstants.CSPSProductName;
                        setupProperties.FileName = Path.GetFileName(csLogFile);
                        Trc.Log(LogLevel.Always,
                            "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                            csLogFile);
                        IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                            SetupParameters.Instance().ValutCredsFilePath,
                            setupProperties,
                            csLogFile,
                            2048,
                            bypassProxy,
                            proxyAddress,
                            proxyPort,
                            proxyUsername,
                            proxyPassword);

                        setupProperties.ProductName = UnifiedSetupConstants.MS_MTProductName;
                        setupProperties.FileName = Path.GetFileName(mtInstLogFile);
                        Trc.Log(LogLevel.Always,
                            "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                            mtInstLogFile);
                        IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                            SetupParameters.Instance().ValutCredsFilePath,
                            setupProperties,
                            mtInstLogFile,
                            2048,
                            bypassProxy,
                            proxyAddress,
                            proxyPort,
                            proxyUsername,
                            proxyPassword);

                        setupProperties.FileName = Path.GetFileName(mtMSILogFile);
                        Trc.Log(LogLevel.Always,
                            "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                            mtMSILogFile);
                        IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                            SetupParameters.Instance().ValutCredsFilePath,
                            setupProperties,
                            mtMSILogFile,
                            2048,
                            bypassProxy,
                            proxyAddress,
                            proxyPort,
                            proxyUsername,
                            proxyPassword);
                    }
                    // MySQL download failure.
                    if (!isMySQLDownloaded)
                    {
                        setupProperties.ProductName = UnifiedSetupConstants.MySQLProductName;
                        setupProperties.FileName = Path.GetFileName(unifiedSetupLogFile);
                        Trc.Log(LogLevel.Always,
                            "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                            unifiedSetupLogFile);
                        IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                            SetupParameters.Instance().ValutCredsFilePath,
                            setupProperties,
                            unifiedSetupLogFile,
                            2048,
                            bypassProxy,
                            proxyAddress,
                            proxyPort,
                            proxyUsername,
                            proxyPassword);
                    }
                    // MySQL download success.
                    else
                    {
                        // IIS install failure.
                        if (!isIISInstalled)
                        {
                            setupProperties.ProductName = UnifiedSetupConstants.IisProductName;
                            setupProperties.FileName = Path.GetFileName(unifiedSetupLogFile);
                            Trc.Log(LogLevel.Always,
                                "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                unifiedSetupLogFile);
                            IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                SetupParameters.Instance().ValutCredsFilePath,
                                setupProperties,
                                unifiedSetupLogFile,
                                2048,
                                bypassProxy,
                                proxyAddress,
                                proxyPort,
                                proxyUsername,
                                proxyPassword);
                        }
                        // IIS install success.
                        else
                        {
                            // Thirdparty install failure.
                            if (!isCXTPInstalled)
                            {
                                setupProperties.ProductName = UnifiedSetupConstants.CXTPProductName;
                                setupProperties.FileName = Path.GetFileName(cxtpLogFile);
                                Trc.Log(LogLevel.Always,
                                    "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                    cxtpLogFile);
                                IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                    SetupParameters.Instance().ValutCredsFilePath,
                                    setupProperties,
                                    cxtpLogFile,
                                    2048,
                                    bypassProxy,
                                    proxyAddress,
                                    proxyPort,
                                    proxyUsername,
                                    proxyPassword);

                                setupProperties.ProductName = UnifiedSetupConstants.UnifiedSetupProductName;
                                setupProperties.FileName = Path.GetFileName(unifiedSetupLogFile);
                                Trc.Log(LogLevel.Always,
                                    "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                    unifiedSetupLogFile);
                                IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                    SetupParameters.Instance().ValutCredsFilePath,
                                    setupProperties,
                                    unifiedSetupLogFile,
                                    2048,
                                    bypassProxy,
                                    proxyAddress,
                                    proxyPort,
                                    proxyUsername,
                                    proxyPassword);
                            }
                            // Thirdparty install success.
                            else
                            {
                                // CS install failure.
                                if (!isCSInstalled)
                                {
                                    setupProperties.ProductName = UnifiedSetupConstants.CSPSProductName;
                                    setupProperties.FileName = Path.GetFileName(csLogFile); 
                                    Trc.Log(LogLevel.Always,
                                        "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                        csLogFile);
                                    IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                        SetupParameters.Instance().ValutCredsFilePath,
                                        setupProperties,
                                        csLogFile,
                                        2048,
                                        bypassProxy,
                                        proxyAddress,
                                        proxyPort,
                                        proxyUsername,
                                        proxyPassword);
                                }
                                // CS install success.
                                else
                                {
                                    // DRA install failure.
                                    if (!isDRAInstalled)
                                    {
                                        setupProperties.ProductName = UnifiedSetupConstants.AzureSiteRecoveryProductName;
                                        setupProperties.FileName = Path.GetFileName(defaultDRALogFileName);
                                        Trc.Log(LogLevel.Always,
                                            "Invoking UploadDraLogsUsingVaultCreds with file : {0}",
                                            defaultDRALogFileName);
                                        IntegrityCheckWrapper.UploadDraLogsUsingVaultCreds(
                                            SetupParameters.Instance().ValutCredsFilePath,
                                            setupProperties,
                                            defaultDRALogFileName,
                                            bypassProxy,
                                            proxyAddress,
                                            proxyPort,
                                            proxyUsername,
                                            proxyPassword);
                                    }
                                    // DRA install success.
                                    else
                                    {
                                        // MT install failure.
                                        if (!isMTInstalled)
                                        {
                                            setupProperties.ProductName = UnifiedSetupConstants.MS_MTProductName;
                                            setupProperties.FileName = Path.GetFileName(mtInstLogFile);
                                            Trc.Log(LogLevel.Always,
                                                "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                mtInstLogFile);
                                            IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                SetupParameters.Instance().ValutCredsFilePath,
                                                setupProperties,
                                                mtInstLogFile,
                                                2048,
                                                bypassProxy,
                                                proxyAddress,
                                                proxyPort,
                                                proxyUsername,
                                                proxyPassword);

                                            setupProperties.FileName = Path.GetFileName(mtMSILogFile);
                                            Trc.Log(LogLevel.Always,
                                                "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                mtMSILogFile);
                                            IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                SetupParameters.Instance().ValutCredsFilePath,
                                                setupProperties,
                                                mtMSILogFile,
                                                2048,
                                                bypassProxy,
                                                proxyAddress,
                                                proxyPort,
                                                proxyUsername,
                                                proxyPassword);
                                        }

                                        // MT configuration failure.
                                        if (!isMTConfigured)
                                        {
                                            setupProperties.ProductName = UnifiedSetupConstants.MS_MTProductName;
                                            setupProperties.FileName = Path.GetFileName(mtConfiguratorLogFile);
                                            Trc.Log(LogLevel.Always,
                                                "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                mtConfiguratorLogFile);
                                            IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                SetupParameters.Instance().ValutCredsFilePath,
                                                setupProperties,
                                                mtConfiguratorLogFile,
                                                2048,
                                                bypassProxy,
                                                proxyAddress,
                                                proxyPort,
                                                proxyUsername,
                                                proxyPassword);
                                        }
                                        // MT configuration success.
                                        else
                                        {
                                            // MARS install failed.
                                            if (!isMARSInstalled)
                                            {
                                                setupProperties.ProductName = UnifiedSetupConstants.MARSAgentProductName;
                                                setupProperties.FileName = Path.GetFileName(marsMSILogFile);
                                                Trc.Log(LogLevel.Always,
                                                "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                marsMSILogFile);
                                                IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                    SetupParameters.Instance().ValutCredsFilePath,
                                                    setupProperties,
                                                    marsMSILogFile,
                                                    2048,
                                                    bypassProxy,
                                                    proxyAddress,
                                                    proxyPort,
                                                    proxyUsername,
                                                    proxyPassword);

                                                setupProperties.FileName = Path.GetFileName(marsMSIPatchLogFile);
                                                Trc.Log(LogLevel.Always,
                                                    "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                    marsMSIPatchLogFile);
                                                IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                    SetupParameters.Instance().ValutCredsFilePath,
                                                    setupProperties,
                                                    marsMSIPatchLogFile,
                                                    2048,
                                                    bypassProxy,
                                                    proxyAddress,
                                                    proxyPort,
                                                    proxyUsername,
                                                    proxyPassword);

                                                setupProperties.FileName = Path.GetFileName(marsInstallLogFile);
                                                Trc.Log(LogLevel.Always,
                                                    "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                    marsInstallLogFile);
                                                IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                    SetupParameters.Instance().ValutCredsFilePath,
                                                    setupProperties,
                                                    marsInstallLogFile,
                                                    2048,
                                                    bypassProxy,
                                                    proxyAddress,
                                                    proxyPort,
                                                    proxyUsername,
                                                    proxyPassword);

                                                setupProperties.FileName = Path.GetFileName(marsManagedInstallLogFile);
                                                Trc.Log(LogLevel.Always,
                                                    "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                    marsManagedInstallLogFile);
                                                IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                    SetupParameters.Instance().ValutCredsFilePath,
                                                    setupProperties,
                                                    marsManagedInstallLogFile,
                                                    2048,
                                                    bypassProxy,
                                                    proxyAddress,
                                                    proxyPort,
                                                    proxyUsername,
                                                    proxyPassword);
                                            }
                                            // MARS install success.
                                            else
                                            {
                                                // Configuration Manager install failed.
                                                if (!isConfigManagerInstalled)
                                                {
                                                    setupProperties.ProductName = UnifiedSetupConstants.ConfigManagerProductName;
                                                    setupProperties.FileName = Path.GetFileName(configManagerMSILogFile);
                                                    Trc.Log(LogLevel.Always,
                                                    "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                    configManagerMSILogFile);
                                                    IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                        SetupParameters.Instance().ValutCredsFilePath,
                                                        setupProperties,
                                                        configManagerMSILogFile,
                                                        2048,
                                                        bypassProxy,
                                                        proxyAddress,
                                                        proxyPort,
                                                        proxyUsername,
                                                        proxyPassword);
                                                }
                                                // Configuration Manager install success.
                                                else
                                                {
                                                    // DRA service not running and Vault/Container registration failure.
                                                    if ((!isDRARegistered) || (!isDRAServiceRunning))
                                                    {
                                                        setupProperties.ProductName = UnifiedSetupConstants.AzureSiteRecoveryProductName;
                                                        setupProperties.FileName = Path.GetFileName(defaultDRALogFileName);
                                                        Trc.Log(LogLevel.Always,
                                                            "Invoking UploadDraLogsUsingVaultCreds with file : {0}",
                                                            defaultDRALogFileName);
                                                        IntegrityCheckWrapper.UploadDraLogsUsingVaultCreds(
                                                            SetupParameters.Instance().ValutCredsFilePath,
                                                            setupProperties,
                                                            defaultDRALogFileName,
                                                            bypassProxy,
                                                            proxyAddress,
                                                            proxyPort,
                                                            proxyUsername,
                                                            proxyPassword);

                                                    }

                                                    // MT registration failure.
                                                    if (!isMTRegistered)
                                                    {
                                                        setupProperties.ProductName = UnifiedSetupConstants.UnifiedSetupProductName;
                                                        setupProperties.FileName = Path.GetFileName(unifiedSetupLogFile);
                                                        Trc.Log(LogLevel.Always,
                                                            "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                            unifiedSetupLogFile);
                                                        IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                            SetupParameters.Instance().ValutCredsFilePath,
                                                            setupProperties,
                                                            unifiedSetupLogFile,
                                                            2048,
                                                            bypassProxy,
                                                            proxyAddress,
                                                            proxyPort,
                                                            proxyUsername,
                                                            proxyPassword);

                                                    }
                                                    else
                                                    {
                                                        // World Wide Web service not running.
                                                        if (!isWorldWideWebServiceRunning)
                                                        {
                                                            setupProperties.ProductName = UnifiedSetupConstants.WorldWideWebServiceName;
                                                            System.IO.File.Copy(worldWideWebServiceSystemLog, worldWideWebServiceLogFile, true);
                                                            setupProperties.FileName = Path.GetFileName(worldWideWebServiceLogFile);
                                                            Trc.Log(LogLevel.Always,
                                                             "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                             worldWideWebServiceLogFile);
                                                            IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                                SetupParameters.Instance().ValutCredsFilePath,
                                                                setupProperties,
                                                                worldWideWebServiceLogFile,
                                                                2048,
                                                                bypassProxy,
                                                                proxyAddress,
                                                                proxyPort,
                                                                proxyUsername,
                                                                proxyPassword);

                                                        }

                                                        // Tmansvc service not running.
                                                        if (!isTmansvcServiceRunning)
                                                        {
                                                            setupProperties.ProductName = UnifiedSetupConstants.TmansvcServiceName;
                                                            setupProperties.FileName = Path.GetFileName(tmansvcServiceLogFile);
                                                            Trc.Log(LogLevel.Always,
                                                             "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                             tmansvcServiceLogFile);
                                                            IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                                SetupParameters.Instance().ValutCredsFilePath,
                                                                setupProperties,
                                                                tmansvcServiceLogFile,
                                                                2048,
                                                                bypassProxy,
                                                                proxyAddress,
                                                                proxyPort,
                                                                proxyUsername,
                                                                proxyPassword);

                                                        }

                                                        // Push Install service not running.
                                                        if (!isPushInstallServiceRunning)
                                                        {
                                                            setupProperties.ProductName = UnifiedSetupConstants.PushInstallServiceName;
                                                            setupProperties.FileName = Path.GetFileName(pushInstallServiceLogFile);
                                                            Trc.Log(LogLevel.Always,
                                                             "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                             pushInstallServiceLogFile);
                                                            IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                                SetupParameters.Instance().ValutCredsFilePath,
                                                                setupProperties,
                                                                pushInstallServiceLogFile,
                                                                2048,
                                                                bypassProxy,
                                                                proxyAddress,
                                                                proxyPort,
                                                                proxyUsername,
                                                                proxyPassword);

                                                            setupProperties.FileName = Path.GetFileName(pushInstallConfFile);
                                                            Trc.Log(LogLevel.Always,
                                                             "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                             pushInstallConfFile);
                                                            IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                                SetupParameters.Instance().ValutCredsFilePath,
                                                                setupProperties,
                                                                pushInstallConfFile,
                                                                2048,
                                                                bypassProxy,
                                                                proxyAddress,
                                                                proxyPort,
                                                                proxyUsername,
                                                                proxyPassword);

                                                        }

                                                        // CxProcessServer service not running.
                                                        if (!isCxPsServiceRunning)
                                                        {
                                                            setupProperties.ProductName = UnifiedSetupConstants.CxPsServiceName;
                                                            setupProperties.FileName = Path.GetFileName(cxpsErrorServiceLogFile);
                                                            Trc.Log(LogLevel.Always,
                                                             "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                             cxpsErrorServiceLogFile);
                                                            IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                                SetupParameters.Instance().ValutCredsFilePath,
                                                                setupProperties,
                                                                cxpsErrorServiceLogFile,
                                                                2048,
                                                                bypassProxy,
                                                                proxyAddress,
                                                                proxyPort,
                                                                proxyUsername,
                                                                proxyPassword);

                                                            setupProperties.FileName = Path.GetFileName(cxpsMonitorServiceLogFile);
                                                            Trc.Log(LogLevel.Always,
                                                             "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                             cxpsMonitorServiceLogFile);
                                                            IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                                SetupParameters.Instance().ValutCredsFilePath,
                                                                setupProperties,
                                                                cxpsMonitorServiceLogFile,
                                                                2048,
                                                                bypassProxy,
                                                                proxyAddress,
                                                                proxyPort,
                                                                proxyUsername,
                                                                proxyPassword);

                                                            setupProperties.FileName = Path.GetFileName(cxpsXferServiceLogFile);
                                                            Trc.Log(LogLevel.Always,
                                                             "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                             cxpsXferServiceLogFile);
                                                            IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                                SetupParameters.Instance().ValutCredsFilePath,
                                                                setupProperties,
                                                                cxpsXferServiceLogFile,
                                                                2048,
                                                                bypassProxy,
                                                                proxyAddress,
                                                                proxyPort,
                                                                proxyUsername,
                                                                proxyPassword);

                                                        }

                                                        // Azure Recovery Services Agent service not running.
                                                        if (!isMarsServiceRunning)
                                                        {
                                                            setupProperties.ProductName = UnifiedSetupConstants.MARSServiceName;
                                                            setupProperties.FileName = Path.GetFileName(marsServiceLogFile);
                                                            Trc.Log(LogLevel.Always,
                                                             "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                                                             marsServiceLogFile);
                                                            IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                                                SetupParameters.Instance().ValutCredsFilePath,
                                                                setupProperties,
                                                                marsServiceLogFile,
                                                                2048,
                                                                bypassProxy,
                                                                proxyAddress,
                                                                proxyPort,
                                                                proxyUsername,
                                                                proxyPassword);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Upload service logs for fresh install.
                    foreach (string logFile in UnifiedSetupServiceLogs)
                    {
                        setupProperties.ProductName = UnifiedSetupConstants.UnifiedSetupServiceLogsComponentName;
                        setupProperties.FileName = Path.GetFileName(logFile);
                        Trc.Log(
                            LogLevel.Always,
                            "Invoking UploadCustomLogsUsingVaultCreds with file : {0}",
                            logFile);
                        if (IntegrityCheckWrapper.UploadCustomLogsUsingVaultCreds(
                                SetupParameters.Instance().ValutCredsFilePath,
                                setupProperties,
                                logFile,
                                1024,
                                bypassProxy,
                                proxyAddress,
                                proxyPort,
                                proxyUsername,
                                proxyPassword))
                        {
                            System.IO.File.Delete(logFile);
                        }
                    }

                }
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error, "Upload operation summary failed.", ex);
            }
        }

        /// <summary>
        /// List of Service logs in Unified setup.
        /// </summary>
        /// <returns>list of service logs</returns>
        public static List<string> ServiceLogs()
        {
            string csInstallDir =
                string.IsNullOrWhiteSpace(SetupParameters.Instance().CXInstallDir) ?
                SetupHelper.GetCSInstalledLocation() :
                SetupParameters.Instance().CXInstallDir;
            string agentInstallDir =
                string.IsNullOrWhiteSpace(SetupParameters.Instance().AgentInstallDir) ?
                SetupHelper.GetAgentInstalledLocation() :
                Path.Combine(
                    SetupParameters.Instance().AgentInstallDir,
                    UnifiedSetupConstants.UnifiedAgentDirectorySuffix);
            string unifiedSetupLogsFolder =
                Path.GetPathRoot(Environment.SystemDirectory) +
                UnifiedSetupConstants.UnifiedSetupLogFolder;
            string tmansvcServiceLogFile = Path.Combine(
                csInstallDir,
                UnifiedSetupConstants.TmansvcServiceLog);
            string pushInstallServiceLogFile = Path.Combine(
                csInstallDir,
                UnifiedSetupConstants.PushInstallServiceLog);
            string pushInstallConfFile = Path.Combine(
                csInstallDir,
                UnifiedSetupConstants.PushInstallConfFile);
            string cxpsErrorServiceLogFile = Path.Combine(
                csInstallDir,
                UnifiedSetupConstants.CxPsErrorServiceLog);
            string cxpsMonitorServiceLogFile = Path.Combine(
                csInstallDir,
                UnifiedSetupConstants.CxPsMonitorServiceLog);
            string cxpsXferServiceLogFile = Path.Combine(
                csInstallDir,
                UnifiedSetupConstants.CxPsXferServiceLog);
            string marsServiceLogFile = Path.Combine(
                sysDrive, 
                UnifiedSetupConstants.MARSServiceLog);
            string draServiceLogFile = 
                PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.DRAUTCLogName);

            Trc.Log(LogLevel.Always, "CS Install Dir - {0}", csInstallDir);
            Trc.Log(LogLevel.Always, "Agent Install Dir - {0}", agentInstallDir);

            // Add service logs to list.
            List<string> ServiceLogs = new List<string>();
            ServiceLogs.Add(tmansvcServiceLogFile);
            ServiceLogs.Add(pushInstallServiceLogFile);
            ServiceLogs.Add(cxpsErrorServiceLogFile);
            ServiceLogs.Add(cxpsMonitorServiceLogFile);
            ServiceLogs.Add(cxpsXferServiceLogFile);
            ServiceLogs.Add(marsServiceLogFile);
            ServiceLogs.Add(draServiceLogFile);

            // Add appservice logs.
            string[] appserviceLogs = Directory.GetFiles(agentInstallDir, "appservice*.log");
            foreach (string logFile in appserviceLogs)
            {
                Trc.Log(LogLevel.Always, "Adding file - {0}", logFile);
                ServiceLogs.Add(logFile);
            }

            // Add svagents logs.
            string[] svagentsLogs = Directory.GetFiles(agentInstallDir, "svagents*.log");
            foreach (string logFile in svagentsLogs)
            {
                Trc.Log(LogLevel.Always, "Adding file - {0}", logFile);
                ServiceLogs.Add(logFile);
            }

            // IIS log files.
            if (Directory.Exists(sysDrive + UnifiedSetupConstants.WorldWideWebServiceLogPath))
            {
                DirectoryInfo iisDirectory = new DirectoryInfo(sysDrive + UnifiedSetupConstants.WorldWideWebServiceLogPath);
                string iisLogFileName = iisDirectory.GetFiles().OrderByDescending(f => f.LastWriteTime).First().ToString();
                ServiceLogs.Add(iisLogFileName);
            }
            
            return ServiceLogs;
        }

        /// <summary>
        /// Lists Unified setup service logs after copying to ASRSetuplogs folder.
        /// </summary>
        /// <returns>list of Unified setup service logs upload path</returns>
        public static List<string> GetUnifiedSetupServiceLogs()
        {
            List<string> UnifiedSetupServiceLogs = new List<string>();
            string unifiedSetupLogsPath = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
                UnifiedSetupConstants.LogFolder);

            try
            {
                foreach (string logFile in ServiceLogs())
                {
                    if (System.IO.File.Exists(logFile))
                    {
                        string destFile = Path.Combine(unifiedSetupLogsPath, Path.GetFileName(logFile));
                        Trc.Log(LogLevel.Always, "Copying file - {0} to {1}", logFile, destFile);
                        System.IO.File.Copy(logFile, destFile, true);
                        UnifiedSetupServiceLogs.Add(destFile);
                    }
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error, "Exception at GetUnifiedSetupServiceLogs", ex);
            }

            return UnifiedSetupServiceLogs;
        }

        /// <summary>
        /// Callback to validate the remote certificate
        /// </summary>
        /// <param name="sender">Sender object</param>
        /// <param name="cert">Certificate to be validated.</param>
        /// <param name="chain">Certificate chain</param>
        /// <param name="error">SSL policy errors</param>
        /// <returns>Valid or not as a boolean flag</returns>
        private static bool RemoteCertValidate(
            object sender,
            X509Certificate cert,
            X509Chain chain,
            SslPolicyErrors error)
        {
            return true;
        }

        /// <summary>
        /// Parses the error code to SetupReturnValue enum.
        /// </summary>
        /// <param name="int">Return code</param>
        /// <returns>errorCodeName</returns>
        private static string ParseIntToSetupReturnValue(int returnCode)
        {
            string errorCodeName = "";

            try
            {
                if (returnCode >= 0)
                {
                    DRSetup.SetupHelper.SetupReturnValues returnVal = (Microsoft.DisasterRecovery.SetupFramework.SetupHelper.SetupReturnValues)returnCode;
                    errorCodeName = returnVal.ToString();
                    Trc.Log(LogLevel.Always, "errorCodeName : {0}", errorCodeName);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Return code value is less than 0. Value : {0}", returnCode);
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(
                    LogLevel.Error,
                    "Exception occurred at CallPurgeDRARegistryMethod : {0}",
                    ex);
            }
            return errorCodeName;
        }

        #endregion
    }
}
