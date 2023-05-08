//---------------------------------------------------------------
//  <copyright file="InstallActionProcessor.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Routines to faciliate installation operations.
//  </summary>
//
//  History:     03-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using SystemRequirementValidator;
using ASRResources;
using ASRSetupFramework;
using Microsoft.Win32;

using WindowsInstaller;

namespace UnifiedAgentInstaller
{
    /// <summary>
    /// Operation to be performed by msiexec.
    /// </summary>
    public enum MsiOperation
    {
        /// <summary>
        /// Install a MSI
        /// </summary>
        Install,

        /// <summary>
        /// Upgrade existing bits.
        /// </summary>
        Upgrade,

        /// <summary>
        /// Uninstall a MSI
        /// </summary>
        Uninstall
    }

    /// <summary>
    /// Class to facilitate installation.
    /// </summary>
    public static class InstallActionProcessor
    {
        /// <summary>
        /// Gets the action to be executed.
        /// </summary>
        /// <returns>Setup action.</returns>
        public static SetupAction GetSetupAction()
        {
            Assembly assembly = Assembly.GetExecutingAssembly();
            FileVersionInfo fvi = FileVersionInfo.GetVersionInfo(assembly.Location);
            Version installerVersion = new Version(fvi.FileVersion);
            Version minRequireVersion = new Version(9, 6, 0, 0);

            Trc.Log(LogLevel.Always, "Present Installer Version: {0}", installerVersion);

            if (PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.IsProductInstalled))
            {
                Version installedVersion = new Version(PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.AlreadyInstalledVersion));
                Trc.Log(LogLevel.Always, "Already Installed Version: {0}", installedVersion);

                if (installedVersion < minRequireVersion)
                {
                    return SetupAction.UnSupportedUpgrade;
                }
                else if (installerVersion > installedVersion)
                {
                    return SetupAction.Upgrade;
                }
                else if (installerVersion == installedVersion)
                {
                    // Set setup action as Upgrade when platform passed to the installer
                    // and installed platfrom are not matching.
                    Platform platform = PropertyBagDictionary.Instance.GetProperty<Platform>(
                        PropertyBagConstants.Platform);
                    Platform installedPlatform;
                    string vmPlatform = SetupHelper.GetAgentInstalledPlatformFromDrScout();
                    installedPlatform = (string.IsNullOrEmpty(vmPlatform)) ?
                        SetupHelper.GetAgentInstalledPlatform() :
                        vmPlatform.AsEnum<Platform>().Value;
                    Trc.Log(LogLevel.Always, "Platform passed to the setup - {0}", platform);
                    Trc.Log(LogLevel.Always, "Installed platform - {0}", installedPlatform);

                    if (installedPlatform != platform)
                    {
                        Trc.Log(LogLevel.Always, "Platform passed to the setup and installed platform are not matching. Setting setup action as Upgrade.");
                        return SetupAction.Upgrade;
                    }
                    else
                    {
                        // Set setup action either as CheckFilterDriver or ExecutePostInstallSteps
                        // when platform passed to the seutp and installed platfrom are matching.
                        Trc.Log(LogLevel.Always, "Platform passed to the setup and installed platform are matching.");
                        if (ValidationHelper.IsAgentPostInstallActionSuccess())
                        {
                            return SetupAction.CheckFilterDriver;
                        }
                        else
                        {
                            return SetupAction.ExecutePostInstallSteps;
                        }
                    }
                }
                else
                {
                    return SetupAction.InvalidOperation;
                }
            }
            else
            {
                return SetupAction.Install;
            }
        }

        /// <summary>
        /// Check OS validity for installation.
        /// </summary>
        /// <param name="role">Role to be installed.</param>
        /// <returns>Operating system type.</returns>
        public static OSType CheckOSValidity(AgentInstallRole role)
        {
            int productType = PropertyBagDictionary.Instance.GetProperty<int>(PropertyBagConstants.GuestOSProductType);
            const int ProductTypeServer = 3;

            OSType operationSystem = PropertyBagDictionary.Instance.GetProperty<OSType>(PropertyBagConstants.GuestOSType);

            if (role == AgentInstallRole.MS)
            {
                // TODO: tmp change to block only x86 client OS
                if ((!PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.Is64BitOS))
                    && (operationSystem != OSType.Vista))
                {
                    Trc.Log(LogLevel.Error, "ASR source agent can not be installed on x86 Desktop machine");
                    return OSType.Unsupported;
                }
            }
            else
            {
                if (productType != ProductTypeServer)
                {
                    Trc.Log(LogLevel.Error, "MT cant be installed on non-Server machine");
                    return OSType.Unsupported;
                }

                if (operationSystem != OSType.Win81)
                {
                    Trc.Log(LogLevel.Error, "MT can be installed only on 2012R2 Machines.");
                    return OSType.Unsupported;
                }
            }

            return operationSystem;
        }

        /// <summary>
        /// Validates pre-requisites.
        /// </summary>
        /// <returns>Error code thrown by MobilityServiceValidator.exe</returns>
        public static int ValidatePreRequisites(Platform platform, out string msvOutputJsonPath, out string msvSummaryJsonPath, out string msvOutputLogPath, out string output)
        {
            int returnCode = -1;
            msvOutputJsonPath = string.Empty;
            msvSummaryJsonPath = string.Empty;
            msvOutputLogPath = string.Empty;
            output = String.Empty;

            try
            {
                // Construct command line parameters.
                string currDir =
                    new System.IO.FileInfo(System.Reflection.Assembly.GetExecutingAssembly().Location).DirectoryName;
                string msvExe = Path.Combine(currDir, UnifiedSetupConstants.MobilityServiceValidatorExe);
                string msvConfigName = platform == Platform.Azure ?
                    UnifiedSetupConstants.MobilityServiceValidatorA2AConfigFile :
                    UnifiedSetupConstants.MobilityServiceValidatorV2AConfigFile;
                string msvConfigPath = Path.Combine(currDir, msvConfigName);
                msvOutputLogPath = Path.Combine(
                    Path.Combine(
                        Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
                        UnifiedSetupConstants.LogFolder),
                    UnifiedSetupConstants.MobilityServiceValidatorLogFile);
                msvSummaryJsonPath = Path.Combine(
                    Path.Combine(
                        Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
                        UnifiedSetupConstants.LogFolder),
                    UnifiedSetupConstants.MobilityServiceValidatorSummaryJsonFile);

                // Construct validations output json file path when /ValidationsOutputFilePath is not passed.

                msvOutputJsonPath = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath);

                string commandLineParameters =
                    UnifiedSetupConstants.MobilityServiceValidatorConfigSwitch + " \"" + msvConfigPath + "\" " +
                    UnifiedSetupConstants.MobilityServiceValidatorOutSwitch + " \"" + msvOutputJsonPath + "\" " +
                    UnifiedSetupConstants.MobilityServiceValidatorLogSwitch + " \"" + msvOutputLogPath + "\" " +
                    UnifiedSetupConstants.MobilityServiceValidatorSummaryJsonSwitch + " \"" + msvSummaryJsonPath + "\" " +
                    UnifiedSetupConstants.MobilityServiceValidatorActionJsonSwitch + " \"" +
                    ((InstallActionProcessor.GetSetupAction() == SetupAction.Upgrade) ? InstallAction.Upgrade : InstallAction.Install) + "\" ";

                return CommandExecutor.RunCommand(msvExe, commandLineParameters, out output);
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Always,
                    "Exception occured while validating pre-requisites : {0}", ex.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Checks for COM+ application related pre-requisites.
        /// </summary>
        /// <returns>True if all the pre-reqs succeed, false otherwise.</returns>
        public static bool CheckVSSProviderStatus()
        {
            Trc.Log(LogLevel.Always, "Begin CheckVSSProviderStatus.");
            bool serviceEnableStatus = true, serviceStartStatus = true, comAppEnumerationStatus;
            string serviceStatus;

            try
            {
                List<string> services = new List<string>() 
                {  
                    "VSS", 
                    "COMSysApp", 
                    "MSDTC"
                };

                // Check whether VSS/COMSysApp/MSTDC service startup type is set to disabled.                
                foreach (string service in services)
                {
                    if (ValidationHelper.IsServiceStartupTypeDisabled(service))
                    {
                        Trc.Log(LogLevel.Always, "{0} service startup type is set to 'Disabled'. So, not attempting to start it.", service);
                        serviceEnableStatus = false;
                    }
                    else
                    {
                        // Try to start if VSS/COMSysApp/MSTDC service startup type is not set to disabled.
                        serviceStatus = ValidationHelper.GetServiceStatus(service);
                        if (serviceStatus != "Running")
                        {
                            Trc.Log(LogLevel.Always, "{0} service is not running. Attempting to start it.", service);
                            if (ServiceControlFunctions.StartService(service))
                            {
                                Trc.Log(LogLevel.Always, "Successfully started {0} service.", service);
                            }
                            else
                            {
                                Trc.Log(LogLevel.Always, "Failed to start {0} service.", service);
                                serviceStartStatus = false;
                            }
                        }
                    }
                }

                // Check for COM+ applications enumeration status.
                if (!ValidationHelper.COMPlusApplicationsEnumeration())
                {
                    comAppEnumerationStatus = false;
                }
                else
                {
                    comAppEnumerationStatus = true;
                }

                // Return true if all the pre-reqs succeed.
                if (serviceEnableStatus && serviceStartStatus && comAppEnumerationStatus)
                {
                    Trc.Log(LogLevel.Always, "All InMage VSS Provider pre-requisites are met.");
                    return true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "One or more InMage VSS Provider pre-requisites are not met.");
                    return false;
                }
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception Occured: {0}", e.ToString());
                return false;
            }
        }

        /// <summary>
        /// Returns the list of features to be installed.
        /// </summary>
        /// <param name="role">Installation role.</param>
        /// <returns>Operating system type.</returns>
        public static List<string> GetFeaturesToInstall(
            AgentInstallRole role, 
            OSType osType, 
            Platform platform)
        {
            var featuresToInstall = new List<string>() { "ProductFeature" };

            if (PropertyBagDictionary.Instance.GetProperty<bool>(
                PropertyBagConstants.Is64BitOS))
            {
                Trc.Log(LogLevel.Debug, "Product feature being installed: ProductFeature64");
                featuresToInstall.Add("ProductFeature64");
            }
            else
            {
                Trc.Log(LogLevel.Debug, "Product feature being installed: ProductFeature32");
                featuresToInstall.Add("ProductFeature32");
            }

            switch (osType)
            {
                case OSType.Win7:
                    featuresToInstall.Add("ProductFeatureWin7");
                    break;
                case OSType.Win8:
                    featuresToInstall.Add("ProductFeatureWin8");
                    break;
                case OSType.Win81:
                    featuresToInstall.Add("ProductFeatureWin81");
                    break;
                default:
                    // We should never hit this.
                    throw new Exception("Unsupported OS!");
            }

            if (platform == Platform.Azure)
            {
                featuresToInstall.Add("ProductFeatureRCM");
            }
            else
            {
                featuresToInstall.Add("ProductFeatureCS");
            }

            if (role == AgentInstallRole.MS)
            {
                featuresToInstall.Add("ProductFeatureMobilityAgent");
            }

            return featuresToInstall;
        }

        /// <summary>
        /// Installs list of features from MSI to the installation location.
        /// </summary>
        /// <param name="role">Role (MS/MT)</param>
        /// <param name="installationLocation">Location to install MSI at.</param>
        /// <param name="platform">VM Platform (Azure/VmWare)</param>
        /// <out="exitCode">exit code from MSI</out>
        /// <returns>True if installation succeeded, false othewise.</returns>
        public static bool InstallAgent(AgentInstallRole role, string installationLocation, Platform platform, out int exitCode)
        {
            string currentPath = PropertyBagDictionary.Instance.GetProperty<string>(
                PropertyBagDictionary.SetupExePath);

            string msiArgument = string.Empty;
            if (!string.IsNullOrEmpty(installationLocation))
            {
                // InMage components don't understand \ at the end of paths and Wix always puts the \ at then 
                // so we pass one more variable to the setup with ending \ removed.
                msiArgument = string.Format(" INSTALLDIR=\"{0}\" INSTALLPATH=\"{0}\" ", installationLocation);
            }

            if (PropertyBagDictionary.Instance.ContainsKey(PropertyBagConstants.Platform))
            {
                msiArgument = msiArgument + string.Format(
                    " PLATFORM=\"{0}\" ", 
                    platform);
            }

            if (PropertyBagDictionary.Instance.ContainsKey(PropertyBagConstants.InstallRole))
            {
                msiArgument = msiArgument + string.Format(
                    " ROLE=\"{0}\" ",
                    role);
            }

            msiArgument = msiArgument + " /norestart ";

            if (!PropertyBagDictionary.Instance.GetProperty<bool>(
                PropertyBagConstants.Is64BitOS))
            {
                currentPath = currentPath + "\\x86\\";
            }

            Trc.Log(
                LogLevel.Always,
                string.Format(
                    "Calling MSIexec for '{0}' msi with arguments '{1}'",
                    UnifiedSetupConstants.UnifiedAgentMsiName,
                    msiArgument));

            // Install the MS\MT MSI
            return MSIHelper(
                true,
                MsiOperation.Install,
                currentPath,
                UnifiedSetupConstants.UnifiedAgentMsiName,
                UnifiedSetupConstants.UnifiedAgentMSILog,
                msiArgument,
                StringResources.MsiexexUnknownExitCode,
                out exitCode,
                UnifiedSetupConstants.TimeoutInMs);
        }

        /// <summary>
        /// Upgrades exisiting installation.
        /// </summary>
        /// <param name="role">Role (MS/MT)</param>
        /// <param name="installationLocation">Location to install MSI at.</param>
        /// <param name="platform">VM Platform (Azure/VmWare)</param>
        /// <out="exitCode">exit code from MSI</out>
        /// <returns>True if installation succeeded, false otherwise.</returns>
        public static bool UpgradeAgentInstallation(AgentInstallRole role, string installationLocation, Platform platform, out int exitCode)
        {
            string currentPath = PropertyBagDictionary.Instance.GetProperty<string>(
                PropertyBagDictionary.SetupExePath);
            
            string msiArgument = string.Empty;
            if (!string.IsNullOrEmpty(installationLocation))
            {
                // InMage components don't understand \ at the end of paths and Wix always puts the \ at then 
                // so we pass one more variable to the setup with ending \ removed.
                msiArgument = string.Format(" INSTALLPATH=\"{0}\" ", installationLocation);
            }

            if (PropertyBagDictionary.Instance.ContainsKey(PropertyBagConstants.Platform))
            {
                msiArgument = msiArgument + string.Format(
                    " PLATFORM=\"{0}\" ",
                    platform);
            }

            if (PropertyBagDictionary.Instance.ContainsKey(PropertyBagConstants.InstallRole))
            {
                msiArgument = msiArgument + string.Format(
                    " ROLE=\"{0}\" ",
                    role);
            }

            msiArgument = msiArgument + " /norestart ";

            msiArgument = msiArgument + " MSIRMSHUTDOWN=2 REINSTALL=ALL REINSTALLMODE=vomus";

            if (!PropertyBagDictionary.Instance.GetProperty<bool>(
                PropertyBagConstants.Is64BitOS))
            {
                currentPath = currentPath + "\\x86\\";
            }

            Trc.Log(
                LogLevel.Always, 
                string.Format(
                    "Calling MSIexec for '{0}' msi with arguments '{1}'",
                    UnifiedSetupConstants.UnifiedAgentMsiName, 
                    msiArgument));

            // Upgrade the MS\MT MSI
            return MSIHelper(
                true,
                MsiOperation.Install,
                currentPath,
                UnifiedSetupConstants.UnifiedAgentMsiName,
                UnifiedSetupConstants.UnifiedAgentMSILog,
                msiArgument,
                StringResources.MsiexexUnknownExitCode,
                out exitCode,
                UnifiedSetupConstants.TimeoutInMs);
        }

        /// <summary>
        /// Updates the DrScout config with latest information.
        /// </summary>
        /// <param name="installationDir">Installation directory.</param>
        /// <param name="setupAction">Setup action.</param>
        /// <param name="role">Installed agent role.</param>
        public static void UpdateDrScoutConfig(
            string installationDir,
            SetupAction setupAction,
            AgentInstallRole role)
        {
            Trc.Log(LogLevel.Always, "Begin UpdateDrScoutConfig.");
            IniFile drScoutInfo = new IniFile(Path.Combine(
                installationDir, 
                UnifiedSetupConstants.DrScoutConfigRelativePath));
            string installedVersion = SetupHelper.GetAgentInstalledVersion().ToString();
            Platform platform = PropertyBagDictionary.Instance.GetProperty<Platform>(
                    PropertyBagConstants.Platform);
            ConfigurationServerType csType = PropertyBagDictionary.Instance.GetProperty<ConfigurationServerType>(PropertyBagConstants.CSType);
            bool isAzureStackHubVm = PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.IsAzureStackHubVm);

            // VXAgent.Transport Section
            drScoutInfo.WriteValue("vxagent.transport", "SslClientFile", Path.Combine(installationDir, @"transport\client.pem"));
            drScoutInfo.WriteValue("vxagent.transport", "SslKeyPathname", Path.Combine(installationDir, @"transport\newreq.pem"));
            drScoutInfo.WriteValue("vxagent.transport", "SslCertificatePathname", Path.Combine(installationDir, @"transport\newcert.pem"));

            // VXAgent section
            drScoutInfo.WriteValue("vxagent", "LogPathname", installationDir + "\\");
            drScoutInfo.WriteValue("vxagent", "CacheDirectory", Path.Combine(installationDir, @"Application Data"));
            drScoutInfo.WriteValue("vxagent", "VsnapConfigPathname", Path.Combine(installationDir, @"Application Data\etc\vsnap"));
            drScoutInfo.WriteValue("vxagent", "VsnapPendingPathname", Path.Combine(installationDir, @"Application Data\etc\pendingvsnap"));
            drScoutInfo.WriteValue("vxagent", "DiffTargetCacheDirectoryPrefix", Path.Combine(installationDir, @"Application Data"));
            drScoutInfo.WriteValue("vxagent", "InstallDirectory", installationDir);
            drScoutInfo.WriteValue("vxagent", "Version", installedVersion);
            drScoutInfo.WriteValue("vxagent", "PROD_VERSION", installedVersion);
            drScoutInfo.WriteValue("vxagent", "TargetChecksumsDir", Path.Combine(installationDir, @"cksums"));
            drScoutInfo.WriteValue("vxagent", "CdpcliExePathname", Path.Combine(installationDir, @"cdpcli.exe"));
            drScoutInfo.WriteValue("vxagent", "CacheMgrExePathname", Path.Combine(installationDir, @"cachemgr.exe"));
            drScoutInfo.WriteValue("vxagent", "CdpMgrExePathname", Path.Combine(installationDir, @"cdpmgr.exe"));
#if DEBUG
            drScoutInfo.WriteValue("vxagent", "LogLevel", "7");
#else
            drScoutInfo.WriteValue("vxagent", "LogLevel", "3");
#endif
            drScoutInfo.WriteValue("vxagent", "VolpackDriverAvailable", "0");
            drScoutInfo.WriteValue("vxagent", "VsnapDriverAvailable", "0");
            drScoutInfo.WriteValue("vxagent", "Role", "Agent");
            drScoutInfo.WriteValue("vxagent", "VmPlatform", platform.ToString());
            drScoutInfo.WriteValue("vxagent", "CSType", csType.ToString());
            drScoutInfo.WriteValue("vxagent", "IsAzureStackHubVm", isAzureStackHubVm ? "1" : "0");

            SetupHelper.UpdateS2AndDpPaths(platform.ToString(), csType, drScoutInfo, installationDir);

            // Additional entries for Master target only.
            if (role == AgentInstallRole.MT)
            {
                drScoutInfo.WriteValue("vxagent", "RegisterHostInterval", "180");
                drScoutInfo.WriteValue("vxagent", "FilterDriverAvailable", "0");
                drScoutInfo.WriteValue("vxagent", "Role", "MasterTarget");
            }

            // AppAgent section
            drScoutInfo.WriteValue("appagent", "ApplicationSettingsCachePath", Path.Combine(installationDir, @"Application Data\etc\AppAgentCache.dat"));
            drScoutInfo.WriteValue("appagent", "ApplicationSchedulerCachePath", Path.Combine(installationDir, @"Application Data\etc\SchedulerCache.dat"));

            // Add additional details if Upgrade is running.
            if (setupAction == SetupAction.Upgrade)
            {
                drScoutInfo.WriteValue("vxagent.upgrade", "UpdatedCX", "0");
                drScoutInfo.WriteValue("vxagent.upgrade", "UpgradePHP", @"/compatibility_update.php");
            }
        }

        /// <summary>
        /// Updates CXPS config.
        /// </summary>
        /// <param name="installationDir">Installation directory.</param>
        /// <param name="setupAction">Setup action.</param>
        /// <param name="role">Installed agent role.</param>
        public static void UpdateCXPSConfig(
            string installationDir,
            SetupAction setupAction,
            AgentInstallRole role)
        {
            Trc.Log(LogLevel.Always, "Begin UpdateCXPSConfig.");
            bool isCXInstalled = SetupHelper.IsCXInstalled();

            IniFile cxpsConfig = new IniFile(Path.Combine(installationDir, UnifiedSetupConstants.CXPSConfigRelativePath));

            if (role == AgentInstallRole.MT)
            {
                cxpsConfig.WriteValue("cxps", "cs_use_secure", "yes");

                if (!isCXInstalled && setupAction == SetupAction.Install)
                {
                    cxpsConfig.WriteValue("cxps", "cfs_mode", "yes");
                }
            }

            if (setupAction == SetupAction.Install && !isCXInstalled)
            {
                cxpsConfig.WriteValue("cxps", "install_dir", Path.Combine(installationDir, @"transport"));
            }
        }

        /// <summary>
        /// Sets installation registries.
        /// </summary>
        public static void SetInstallationRegistries(SetupAction setupAction)
        {
            RegistryKey hklm = Registry.LocalMachine;

            using (RegistryKey inMageSvSys = hklm.OpenSubKey(UnifiedSetupConstants.SvSystemsRegistryHive, true))
            {
#if DEBUG
                inMageSvSys.SetValue("DebugLogLevel", 7, RegistryValueKind.DWord);
#else
                inMageSvSys.SetValue("DebugLogLevel", 3, RegistryValueKind.DWord);
#endif
            }

            using (RegistryKey imageKey = hklm.OpenSubKey(UnifiedSetupConstants.InMageVXRegistryPath, true))
            {
                Trc.Log(LogLevel.Always, "Set {0} registry value : {1}", 
                    UnifiedSetupConstants.InMageInstallActionKey, 
                    setupAction.ToString());
                imageKey.SetValue(UnifiedSetupConstants.InMageInstallActionKey, setupAction.ToString());
                imageKey.SetValue(UnifiedSetupConstants.PostInstallActionsStatusKey, string.Empty);
            }
        }

        /// <summary>
        /// Sets post installation status registry.
        /// </summary>
        public static void SetPostInstallActionStatus(string status)
        {
            RegistryKey hklm = Registry.LocalMachine;

            using (RegistryKey imageKey = hklm.OpenSubKey(UnifiedSetupConstants.InMageVXRegistryPath, true))
            {
                Trc.Log(LogLevel.Always, "Set {0} registry value : {1}", UnifiedSetupConstants.PostInstallActionsStatusKey, status);
                imageKey.SetValue(UnifiedSetupConstants.PostInstallActionsStatusKey, status);
            }
        }

        /// <summary>
        /// Gets post installation status registry.
        /// </summary>
        public static SetupAction GetAgentInstallAction()
        {
            SetupAction setupAction = SetupAction.Install;
            RegistryKey hklm = Registry.LocalMachine;

            using (RegistryKey imageKey = hklm.OpenSubKey(UnifiedSetupConstants.InMageVXRegistryPath, true))
            {
                string installAction = (string)imageKey.GetValue(UnifiedSetupConstants.InMageInstallActionKey, string.Empty);
                Trc.Log(LogLevel.Always, "InstallAction value in registry : {0}", installAction);

                if (string.Equals(
                        installAction,
                        UnifiedSetupConstants.Upgrade,
                        StringComparison.InvariantCultureIgnoreCase))
                {
                    setupAction = SetupAction.Upgrade;
                }
            }
            return setupAction;
        }

        /// <summary>
        /// Performs MSI operations.
        /// </summary>
        /// <param name="silent">True if operations are to be done in silent manner.</param>
        /// <param name="operation">Operation to perform.</param>
        /// <param name="workingDirectory">Working directory.</param>
        /// <param name="strFileName">Name of file to perform operations on.</param>
        /// <param name="strLogFileName">Log file name.</param>
        /// <param name="msiArguments">Arguments for msi.</param>
        /// <param name="customError">Custom error to append.</param>
        /// <returns>True if installation succeeded, false otherwise.</returns>
        private static bool MSIHelper(
            bool silent, 
            MsiOperation operation, 
            string workingDirectory, 
            string strFileName, 
            string strLogFileName, 
            string msiArguments, 
            string customError,
            out int exitCode,
            int timeout = System.Threading.Timeout.Infinite)
        {
            try
            {
                Trc.Log(
                    LogLevel.Always,
                    "{0} {1}",
                    (operation == MsiOperation.Install) ? "Installating" :
                    (operation == MsiOperation.Upgrade) ? "Upgrading" : 
                    "Uninstalling",
                    strFileName);

                ProcessStartInfo startInfo = new ProcessStartInfo();
                startInfo.UseShellExecute = false;
                startInfo.RedirectStandardError = true;
                startInfo.RedirectStandardOutput = true;
                startInfo.Verb = "runas";
                startInfo.FileName = "msiexec.exe";
                startInfo.Arguments = string.Format(
                    "/q{0} /{1} \"{2}\\{3}\" {4} /L*v+ \"{5}\" /norestart",
                    silent ? "n" : "f",
                    (operation == MsiOperation.Install) ? "i" : (operation == MsiOperation.Upgrade) ? "update" : "x",
                    workingDirectory,
                    strFileName,
                    msiArguments,
                    SetupHelper.SetLogFilePath(strLogFileName));

                Trc.Log(LogLevel.Always,
                    "Starting MSI with arguments - {0}",
                    startInfo.Arguments);

                Process process = new Process();
                process.StartInfo = startInfo;
                process.Start();

                string error = GetUnicodeEncoding(process.StandardError);
                string output = GetUnicodeEncoding(process.StandardOutput);

                bool process_completed = process.WaitForExit(timeout);
                if (process_completed)
                {
                    Trc.Log(LogLevel.Always, "MSI execution completed within given timeout period {0} ms.", timeout);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "UA Msi installation did not complete within given timeout period {0}. Trying to kill the process.", timeout);
                    try
                    {
                        process.Kill();
                        Trc.Log(LogLevel.Always, "UA Msi installation did not complete within given timeout period {0}. So process was killed.", timeout);
                    }
                    catch(OutOfMemoryException)
                    {
                        throw;
                    }
                    catch (Exception ex)
                    {
                        Trc.Log(LogLevel.Always, "Failed to kill the process with exception " + ex);
                    }
                    System.Threading.Thread.Sleep(1000);
                }

                // Mark that MSI logs needs to be collected.
                SetupReportData.AddLogFilePath(
                    SetupHelper.SetLogFilePath(strLogFileName));
                exitCode = process.ExitCode;
                Trc.Log(LogLevel.Always, "Exit code from MSI : {0}", exitCode);

                switch (process.ExitCode)
                {
                    case UnifiedSetupConstants.Success:
                        Trc.Log(
                            LogLevel.Always,
                            "{0} of {1} successful",
                            operation.ToString(),
                            strFileName);
                        return true;
                    case UnifiedSetupConstants.SuccessWithRebootRequired:
                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagDictionary.RebootRequired, true);
                        Trc.Log(
                            LogLevel.Always,
                            "{0} of {1} successful, reboot is required",
                            operation.ToString(),
                            strFileName);
                        return true;
                    case UnifiedSetupConstants.FailureUninstallInstalledProduct:
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ErrorMessage, 
                            StringResources.IncompatibleProductInstalled);
                        Trc.Log(
                            LogLevel.Always,
                            "{0} of {1} failed with error code {2}, invalid product version installed",
                            operation.ToString(),
                            strFileName,
                            process.ExitCode);
                        return false;
                    case UnifiedSetupConstants.InvalidVersionNumber:
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ErrorMessage, 
                            StringResources.IncompatibleProductInstalled);
                        Trc.Log(
                            LogLevel.Always,
                            "{0} of {1} failed with error code {2}, invalid product version installed",
                            operation.ToString(),
                            strFileName,
                            process.ExitCode);
                        return false;
                    default:
                        string errorString = process.ExitCode.ToString();

                        if (string.IsNullOrEmpty(error))
                        {
                            if (!string.IsNullOrEmpty(output))
                            {
                                errorString = string.Format("{0}: {1}", process.ExitCode.ToString(), output);
                            }
                        }
                        else
                        {
                            errorString = string.Format("{0}: {1}", process.ExitCode.ToString(), error);
                        }

                        Trc.Log(
                            LogLevel.Always,
                            "Exception in MSI {0} {1}",
                            operation.ToString(),
                            errorString);
                        PropertyBagDictionary.Instance.SafeAdd(
                            PropertyBagConstants.ErrorMessage, 
                            AppendCustomError(errorString, customError));
                        return false;
                }
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Trc.LogException(
                    LogLevel.Always,
                    string.Format("Exception while MSI {0} ", operation.ToString()),
                    ex);
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.ErrorMessage, 
                    AppendCustomError(ex.Message, customError));
                exitCode = 1;
            }

            return false;
        }

        /// <summary>
        /// Get the unicode string
        /// </summary>
        /// <param name="sr">Stream reader</param>
        /// <returns>unicode string</returns>
        private static string GetUnicodeEncoding(StreamReader sr)
        {
            string output = sr.ReadToEnd();
            Encoding encoding = sr.CurrentEncoding;

            UnicodeEncoding unicodeEncoding = new UnicodeEncoding();
            return unicodeEncoding.GetString(encoding.GetBytes(output));
        }

        /// <summary>
        /// Append the second string to the first string if second string is not null or emty
        /// </summary>
        /// <param name="errorString">first string</param>
        /// <param name="appendString">second string</param>
        /// <returns>Appended string</returns>
        private static string AppendCustomError(string errorString, string appendString)
        {
            if (string.IsNullOrEmpty(appendString))
            {
                return errorString;
            }

            return string.Format("{0}\r\n{1}", errorString, appendString);
        }

        /// <summary>
        /// Install Azure VM Agent.
        /// </summary>
        /// <returns>Exit code</returns>
        public static int InstallAzureVMAgent()
        {
            int returnCode = -1;
            try
            {
                string azureVMMsiLog = SetupHelper.SetLogFilePath(PropertyBagConstants.azureVMMsiLog);

                // Get absolute path of WindowsAzureVmAgent.msi.
                string currDir = 
                    new System.IO.FileInfo(System.Reflection.Assembly.GetExecutingAssembly().Location).DirectoryName;
                string msiPath = currDir + @"\WindowsAzureVmAgent.msi";

                // Construct command line parameters.
                string CommandLineParameters = "/i " + "\"" + msiPath + "\"" + " /qn" +
                                        " /norestart " +
                                        " /L+*V " + "\"" + azureVMMsiLog + "\"";

                returnCode = 
                    CommandExecutor.RunCommand("msiexec.exe", CommandLineParameters, UnifiedSetupConstants.TimeoutInMs);
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Always,
                    "Exception occured while InstallAzureVMAgent : {0}", ex.ToString());
            }
            return returnCode;
        }

        /// <summary>
        /// Uninstall Azure VM Agent.
        /// </summary>
        /// <returns>Exit code</returns>
        public static int UninstallAzureVMAgent()
        {
            int returnCode = -1;
            try
            {
                string azureVMUninstallLog = SetupHelper.SetLogFilePath(PropertyBagConstants.azureVMUninstallLog);

                // Construct command line parameters.
                string CommandLineParameters = "/qn /x {5CF4D04A-F16C-4892-9196-6025EA61F964} " +
                    " /log " + "\"" + azureVMUninstallLog + "\"";

                returnCode =
                    CommandExecutor.ExecuteCommand("msiexec.exe", CommandLineParameters);
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Always,
                    "Exception occured while AzureVMAgent uninstall: {0}", ex.ToString());
            }
            return returnCode;
        }

        /// <summary>
        /// Get the MSI file property value.
        /// </summary>
        /// <returns>Returns specific property value</returns>
        public static string GetMsiFilePropertyValue(string msiFilePath, string propertyName)
        {
            string returnValue = string.Empty;

            // Create an Installer instance
            Type windowsInstallerType = Type.GetTypeFromProgID("WindowsInstaller.Installer");
            Object windowsInstallerObj = Activator.CreateInstance(windowsInstallerType);
            Installer windowsInstaller = windowsInstallerObj as Installer;

            // Open the msi file for reading
            // 0 - Read, 1 - Read/Write
            Database windowsInstallerDatabase = windowsInstaller.OpenDatabase(msiFilePath, 0);
            string propertyQuery = String.Format(
               "SELECT Value FROM Property WHERE Property='{0}'", propertyName);
            View databaseView = windowsInstallerDatabase.OpenView(propertyQuery);
            databaseView.Execute(null);
            Record databaseRecord = databaseView.Fetch();
            if (databaseRecord != null)
                returnValue = databaseRecord.get_StringData(1);

            return returnValue;
        }


        /// <summary>
        /// Install Azure VM agent on Mobility service when
        /// 1) .Net Framework is greater than or equal to 4.0
        /// 2) Azure VM agent is not installed on the machine 
        /// <returns>true if installation is successful, false otherwise</returns>
        /// </summary>
        public static bool AzureVMAgentInstall()
        {
            bool returnCode = false;
            try
            {
                int azureVMAgentReturnCode = -1;
                int azureVMAgentUninstallReturnCode = -1;
                AgentInstallRole role = PropertyBagDictionary.Instance.GetProperty<AgentInstallRole>(
                    PropertyBagConstants.InstallRole);
                Trc.Log(LogLevel.Always, "AgentInstallRole : {0}", role);

                if (role == AgentInstallRole.MS)
                {
                    string installedVersion;
                    bool installAzureVMAgent = false;
                    bool isInstalled = false;

                    bool azureAgentStatus = PrechecksSetupHelper.QueryInstallationStatus(
                        PropertyBagConstants.WindowsAzureVMAgentGUID, 
                        out installedVersion, out isInstalled
                        );

                    if (!azureAgentStatus)
                    {
                        Trc.Log(LogLevel.Error,
                            "Failed to query if Azure agent is already installed on this machine.");
                    }

                    if (isInstalled)
                    {
                        Trc.Log(LogLevel.Always, 
                            "Azure VM agent version - {0} is already installed on this machine.", 
                            installedVersion);

                        string currentDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
                        string AzureVMagentPath = Path.Combine(currentDirectory, UnifiedSetupConstants.AzureVMAgentInstallerMsiName);
                        string currentMsiVersion = GetMsiFilePropertyValue(AzureVMagentPath, UnifiedSetupConstants.ProductVersionProperty);
                        Version msiversion = new Version(currentMsiVersion);
                        Version version = new Version(installedVersion);
                        Trc.Log(LogLevel.Always, "current Azure VM agent build version - {0}", currentMsiVersion);

                        if (version < msiversion)
                        {
                            Trc.Log(LogLevel.Always, "Older version of Azure VM agent - {0} installed. Uninstalling  it.", version);
                            azureVMAgentUninstallReturnCode = UninstallAzureVMAgent();

                            if (azureVMAgentUninstallReturnCode == 0)
                            {
                                Trc.Log(LogLevel.Always, "Azure VM agent uninstallation has succeeded. ");
                                installAzureVMAgent = true;
                            }
                            else
                            {
                                Trc.Log(LogLevel.Always, "Azure VM agent uninstallation has failed.");
                                return returnCode;
                            }
                        }
                        else
                        {
                            //Disable Guest Agent services.
                            DisableGuestAgentServices();
                        }
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Azure VM agent is not installed.");

                        // Checking for .Net Framework 4.0 or higher.
                        if ((ValidationHelper.GetDotNetVersionFromRegistry("4.0")) || 
                            (ValidationHelper.GetDotNetVersionFromRegistry("4")))
                        {
                            Trc.Log(LogLevel.Always,
                                "Installing Azure VM agent as .Net Framework 4.0 or higher is available.");
                            installAzureVMAgent = true;
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always,
                                "Unable to proceed with Azure VM agent installation as .Net Framework 4.0 or higher version is not available.");
                        }
                    }

                    if (installAzureVMAgent)
                    {
                        azureVMAgentReturnCode = InstallAzureVMAgent();

                        if ((azureVMAgentReturnCode == 0) || (azureVMAgentReturnCode == 3010))
                        {
                            Trc.Log(LogLevel.Always, "Azure VM agent installation has succeeded. ");

                            //Disable Guest Agent services.
                            DisableGuestAgentServices();

                            if (azureVMAgentReturnCode == 3010)
                            {
                                PropertyBagDictionary.Instance.SafeAdd(
                                    PropertyBagDictionary.RebootRequired, true);
                            }
                            Registry.SetValue(
                                UnifiedSetupConstants.AgentRegistryName,
                                UnifiedSetupConstants.AzureVMAgentInstalled,
                                UnifiedSetupConstants.Yes);
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "Azure VM agent installation has failed.");
                            return returnCode;
                        }
                    }
                }
                return true;
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Always, 
                    "Exception Occured while AzureVMAgentInstall: {0}", 
                    ex.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Check whether the setup is an ASR server (CXTP/CS/PS installed).
        /// </summary>
        /// <returns>true if setup is ASR server, false otherwise</returns>
        public static bool IsASRServerSetup()
        {
            if (ValidationHelper.IsComponentInstalled(UnifiedSetupConstants.CXTPProductName) ||
                ValidationHelper.IsComponentInstalled(UnifiedSetupConstants.CSPSProductName))
            {
                Trc.Log(LogLevel.Always, "Setup is an ASR server as CXTP/CS/PS is installed.");
                return true;
            }

            return false;
        }

        /// <summary>
        /// Stop and disable WindowsAzureGuestAgent and WindowsAzureTelemetryService services
        /// </summary>        
        public static void DisableGuestAgentServices()
        {
            UnifiedSetupConstants.AzureGuestAgentServices.ForEach(service =>
            {
                if (ServiceControlFunctions.StopService(service))
                {
                    Trc.Log(LogLevel.Always, "Stopped {0} service.", service);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Failed to stop service {0}.", service);
                }

                if (ServiceControlFunctions.ChangeServiceStartupType(service, 
                                            NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED))
                {
                    Trc.Log(LogLevel.Always, "{0} disabled successfully.", service);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Failed to disable {0}.", service);
                }
            });
        }

        /// <summary>
        /// Set Platform value.
        /// </summary>
        /// <param name=setupAction>install/upgrade</param>
        internal static void SetPlatform(SetupAction setupAction)
        {
            if (!PropertyBagDictionary.Instance.ContainsKey(PropertyBagConstants.Platform))
            {
                Platform platform = Platform.VmWare;
                string vmPlatform = string.Empty;

                Trc.Log(LogLevel.Always, "Determining Platform during - {0}",
                    setupAction.ToString());

                switch (setupAction)
                {
                    /* A2A has platform value in drscout.conf from public preview.
                       Prior to 9.9, V2A doesn't have platform value in drscout.conf.
                       During upgrade, if platform value in drscout.conf is empty, 
                       VmWare is treated as default value. */
                    case SetupAction.Upgrade:
                        vmPlatform = SetupHelper.GetAgentInstalledPlatformFromDrScout();
                        platform = (string.IsNullOrEmpty(vmPlatform)) ?
                            Platform.VmWare :
                            vmPlatform.AsEnum<Platform>().Value;
                        break;
                    case SetupAction.ExecutePostInstallSteps:
                        vmPlatform = SetupHelper.GetAgentInstalledPlatformFromDrScout();
                        if (string.IsNullOrEmpty(vmPlatform))
                        {
                            goto case SetupAction.Install;
                        }
                        else
                        {
                            platform = vmPlatform.AsEnum<Platform>().Value;
                        }                  
                        break;
                    case SetupAction.Install:
                    default:
                        platform = (PropertyBagDictionary.Instance.GetProperty<bool>(
                            PropertyBagConstants.IsAzureVm)) ?
                            Platform.Azure :
                            Platform.VmWare;
                        break;
                }

                Trc.Log(LogLevel.Always, "Platform is set to - {0}",
                    platform.ToString());
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.Platform,
                    platform);
            }
        }

        internal static bool InstallationTypeCheck(out SetupHelper.UASetupReturnValues returnValue, out string errorString)
        {
            returnValue = SetupHelper.UASetupReturnValues.Failed;
            errorString = String.Empty;
            SetupAction setupAction = PropertyBagDictionary.Instance.GetProperty<SetupAction>(PropertyBagConstants.SetupAction);
            if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.InstallationType))
            {
                InstallationType installationType = PropertyBagDictionary.Instance
                    .GetProperty<InstallationType>(PropertyBagConstants.InstallationType);
                ConfigurationServerType csType = PropertyBagDictionary.Instance.
                    GetProperty<ConfigurationServerType>(PropertyBagConstants.CSType);

                // Upgrade case from CSLegacy to CSPrime
                // Upgrade from CS to CSPrime occurs in following conditions
                // 1. Fresh Installation flag is passed to the installer
                // 2. Upgrade is detected by installer
                // 3. Agent for CS Legacy is installed on vm
                // 4. The CSType passed to the installer is CSPrime
                // 5. Platform is V2A
                // 6. It is a command line installation
                if (InstallationType.Install == installationType &&
                    setupAction == SetupAction.Upgrade &&
                    !PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.IsAzureVm) &&
                    String.Equals(SetupHelper.GetCSTypeFromDrScout(), ConfigurationServerType.CSLegacy.ToString(), StringComparison.OrdinalIgnoreCase) &&
                    csType == ConfigurationServerType.CSPrime)
                {
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.SetupAction, SetupAction.Upgrade);
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.PlatformUpgradeFromCSToCSPrime, true);
                    Trc.Log(LogLevel.Always, "Upgrading platform from CS to CSPrime");
                    return true;
                }
                // check if setup action detected and that passed to the installer are same or not
                else if (setupAction == SetupAction.Install && installationType != InstallationType.Install)
                {
                    Trc.Log(LogLevel.Error, "The installation type passed to installer and that detected by installer do not match. Aborting Installation.");
                    Trc.Log(LogLevel.Error, string.Format("Installation type passed : {0} ; Installation type detected : {1}",
                        installationType.ToString(),
                        setupAction.ToString()));
                    ErrorLogger.LogInstallerError(
                        new InstallerException(
                            UASetupErrorCode.ASRMobilityServiceInstallerInstallationTypeMismatch.ToString(),
                            new Dictionary<string, string>()
                            {
                                            {UASetupErrorCodeParams.InstallationTypePassed,  installationType.ToString()},
                                            {UASetupErrorCodeParams.InstallationTypeDetected, setupAction.ToString()}
                            },
                            string.Format(UASetupErrorCodeMessage.InstallationTypeMismatch, installationType.ToString(), setupAction.ToString())
                            )
                        );
                    returnValue = SetupHelper.UASetupReturnValues.FailedWithErrors;
                    errorString = "The installation type passed to installer and that detected by installer do not match. Aborting Installation.";
                    return false;
                }
                // For upgrade in V2A check if CSType detected and CSType passed are same or not
                else if (!PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.IsAzureVm) &&
                    setupAction == SetupAction.Upgrade &&
                    csType.ToString() != SetupHelper.GetCSTypeFromDrScout())
                {
                    Trc.Log(LogLevel.Error, "The CSType passed to installer and that detected by installer do not match. Aborting Installation.");
                    Trc.Log(LogLevel.Error, string.Format("CSType passed : {0} ; CSType detected : {1}",
                        csType.ToString(),
                        SetupHelper.GetCSTypeFromDrScout()));
                    ErrorLogger.LogInstallerError(
                        new InstallerException(
                            UASetupErrorCode.ASRMobilityServiceInstallerCSTypeMismatch.ToString(),
                            new Dictionary<string, string>()
                            {
                                            {UASetupErrorCodeParams.CSTypePassed, csType.ToString()},
                                            {UASetupErrorCodeParams.CSTypeDetected, SetupHelper.GetCSTypeFromDrScout()}
                            },
                            string.Format(UASetupErrorCodeMessage.CSTypeMismatch, csType.ToString(), SetupHelper.GetCSTypeFromDrScout())
                            )
                        );
                    returnValue = SetupHelper.UASetupReturnValues.FailedWithErrors;
                    errorString = "The CSType passed to installer and that detected by installer do not match. Aborting Installation.";
                    return false;
                }
            }
            // Removing mandatory check for installation type parameter
            // The default value will be install if nothing is passed
            /*else
            {
                Trc.Log(LogLevel.Error, "Mandatory command line parameter " + PropertyBagConstants.InstallationType
                    + " not found. Aborting installation");
                ErrorLogger.LogInstallerError(
                    new InstallerException(
                        UASetupErrorCode.ASRMobilityServiceInstallerCommandLineParametersMissingFailure.ToString(),
                        new Dictionary<string, string>
                        {
                                        { UASetupErrorCodeParams.MissingCmdArguments, PropertyBagConstants.InstallationType }
                        },
                        string.Format(UASetupErrorCodeMessage.MissingCommandLineArguments, PropertyBagConstants.InstallationType)
                    ));
                returnValue = SetupHelper.UASetupReturnValues.FailedWithErrors;
                errorString = "Mandatory command line parameters not found. Aborting installation";
                return false;
            }*/
            return true;
        }

        /// <summary>
        /// Function to handle various suberrors for msi error code 1603.
        /// It searches for patterns in log files, and if the pattern exists,
        /// returns specific error for msi error 1603
        /// </summary>
        /// <param name="errorParams">Dictionary to store error params</param>
        /// <returns>Suberrors for msi error 1603</returns>
        internal static MSIError1603 CheckMsiSubErrors1603(IDictionary<string, string> errorParams)
        {
            //Illegal operation attempted on a registry key that has been marked for deletion.
            const string SystemError1018 = "System error 1018";

            //Insufficient system resources exist to complete the requested service.
            const string SystemError1450 = "System error 1450";

            string msiLogPath = SetupHelper.SetLogFilePath(UnifiedSetupConstants.UnifiedAgentMSILog);

            try
            {
                FileInfo msiFileInfo = new FileInfo(msiLogPath);

                if (!msiFileInfo.Exists)
                {
                    Trc.Log(LogLevel.Error, "Could not find the msi log file {0}", msiLogPath);
                    return MSIError1603.Default;
                }

                if (msiFileInfo.Length > UnifiedSetupConstants.MSILogReadMaxFileSize)
                {
                    Trc.Log(LogLevel.Always, "The msi log file {0} is too large {1} bytes to parse. So returning the default exit code",
                        msiLogPath, msiFileInfo.Length);
                    return MSIError1603.Default;
                }

                using (StreamReader sr = msiFileInfo.OpenText())
                {
                    string str = string.Empty;
                    str = sr.ReadToEnd();
                    if (str.IsNullOrWhiteSpace())
                    {
                        Trc.Log(LogLevel.Always, "The msi log {0} is empty", msiLogPath);
                        return MSIError1603.Default;
                    }

                    Regex reg = new Regex("-- Error([^\r\n]+)");
                    Match mat = reg.Match(str);
                    if (mat.Success)
                    {
                        if (mat.ToString().IndexOf(SystemError1018, StringComparison.OrdinalIgnoreCase) >= 0)
                        {
                            Trc.Log(LogLevel.Always, "MSI Error : {0}", SystemError1018);
                            return MSIError1603.KeyDeleted;
                        }
                        else if (mat.ToString().IndexOf(SystemError1450, StringComparison.OrdinalIgnoreCase) >= 0)
                        {
                            Trc.Log(LogLevel.Always, "MSI Error : {0}", SystemError1450);
                            return MSIError1603.NoSystemResource;
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "MSI Error : {0}", mat.ToString());
                            errorParams.Add(UASetupErrorCodeParams.ErrorMessage, mat.ToString());
                            return MSIError1603.GenericError;
                        }
                    }
                }
            }
            catch(Exception ex)
            {
                Trc.Log(LogLevel.Error, "Could not parse the msi log file {0} with exception {1}",
                    msiLogPath, ex);
            }

            return MSIError1603.Default;
        }

        /// <summary>
        /// Function to handle various suberrors for msi error code 1601.
        /// It searches for patterns in log files, and if the pattern exists,
        /// returns specific error for msi error 1603
        /// </summary>
        /// <param name="errorParams">Dictionary to store error params</param>
        /// <returns>Sub error for msi error 1601</returns>
        internal static MSIError1601 CheckMsiSubErrors1601(IDictionary<string, string> errorParams)
        {
            string msiLogPath = SetupHelper.SetLogFilePath(UnifiedSetupConstants.UnifiedAgentMSILog);

            try
            {
                FileInfo msiFileInfo = new FileInfo(msiLogPath);

                if (!msiFileInfo.Exists)
                {
                    Trc.Log(LogLevel.Error, "Could not find the msi log file {0}", msiLogPath);
                    return MSIError1601.Default;
                }

                if (msiFileInfo.Length > UnifiedSetupConstants.MSILogReadMaxFileSize)
                {
                    Trc.Log(LogLevel.Always, "The msi log file {0} is too large {1} bytes to parse. So returning the default exit code",
                        msiLogPath, (double)msiFileInfo.Length / (1024 * 1024));
                    return MSIError1601.Default;
                }

                using (StreamReader sr = msiFileInfo.OpenText())
                {
                    string str = string.Empty;
                    str = sr.ReadToEnd();
                    if (str.IsNullOrWhiteSpace())
                    {
                        Trc.Log(LogLevel.Always, "The msi log {0} is empty", msiLogPath);
                        return MSIError1601.Default;
                    }

                    Regex reg = new Regex("Failed to connect to server([^\r\n]+)");
                    Match mat = reg.Match(str);

                    if (mat.Success)
                    {
                        Trc.Log(LogLevel.Always, "MSI Error : {0}", mat.ToString());
                        return MSIError1601.ConnectToServerFailed;
                    }
                }
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Could not parse the msi log file {0} with exception {1}",
                    msiLogPath, ex);
            }

            return MSIError1601.Default;
        }
    }
}
