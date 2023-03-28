using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.IO;
using Microsoft.Win32;
using System.Text.RegularExpressions;
using ASRSetupFramework;

namespace RemoveUnifiedSetupConfiguration
{
    public static class RemoveConfiguration
    {

        # region Fields

        /// <summary>
        /// Stores the system directory.
        /// </summary>
        public static string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);

        # endregion Fields

        /// <summary>
        /// List of remove config operations.
        /// </summary>
        public enum RemoveConfigOperations
        {
            StopUnifiedSetupServices,
            ChangeUnifiedSetupServicesStartupTypeToManual,
            UninstallMySQL,
            RemoveDataTransferSecurePort,
            RemoveCSPSCertificates,
            RemoveCSPSConfig
        };

        /// <summary>
        /// Removes Unified Setup configuration.
        /// </summary>
        /// <returns>true if configuration removed successfully, false otherwise</returns>
        public static bool RemoveUnifiedSetupConfig()
        {
            bool status = true;

            try
            {
                string csInstallLocation = (string)Registry.GetValue(
                    UnifiedSetupConstants.CSPSRegistryName,
                    UnifiedSetupConstants.InstDirRegKeyName,
                    string.Empty);
                string csType =
                    GrepUtils.GetKeyValueFromFile(
                    Path.Combine(csInstallLocation, UnifiedSetupConstants.AmethystFile),
                    UnifiedSetupConstants.ConfigurationType);
                Trc.Log(LogLevel.Always, "csType: {0}", csType);

                // Evaluates all the operations and returns false if any operation fails, else returns success.
                List<RemoveConfigOperations> operations =
                    new List<RemoveConfigOperations>();
                operations.Add(RemoveConfigOperations.StopUnifiedSetupServices);
                operations.Add(RemoveConfigOperations.ChangeUnifiedSetupServicesStartupTypeToManual);
                operations.Add(RemoveConfigOperations.UninstallMySQL);
                operations.Add(RemoveConfigOperations.RemoveDataTransferSecurePort);
                operations.Add(RemoveConfigOperations.RemoveCSPSCertificates);
                operations.Add(RemoveConfigOperations.RemoveCSPSConfig);

                foreach (RemoveConfigOperations operation in operations)
                {
                    if (operation == RemoveConfigOperations.UninstallMySQL && csType == "2")
                    {
                        Trc.Log(LogLevel.Always, "Skipping MySQL configuration removal as Process Server installed on this machine.");
                        continue;
                    }

                    if (!EvaluateRemoveConfigOperations(operation))
                    {
                        status = false;
                        break;
                    }
                }
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while removing Unified Setup configuration : {0}", ex.ToString());
                status = false;
            }

            return status;
        }

        /// <summary>
        /// Evaluate the given operations.
        /// </summary>
        /// <param name="operationName">Remove configuration check to be evaluated.</param>
        /// <returns>Status of the operation.</returns>
        private static bool EvaluateRemoveConfigOperations(
            RemoveConfigOperations operationName)
        {
            switch (operationName)
            {
                case RemoveConfigOperations.StopUnifiedSetupServices:
                    return StopUnifiedSetupServices();
                case RemoveConfigOperations.ChangeUnifiedSetupServicesStartupTypeToManual:
                    return ChangeUnifiedSetupServicesStartupTypeToManual();
                case RemoveConfigOperations.UninstallMySQL:
                    return UninstallMySQL();
                case RemoveConfigOperations.RemoveDataTransferSecurePort:
                    return RemoveDataTransferSecurePort();
                case RemoveConfigOperations.RemoveCSPSCertificates:
                    return RemoveCSPSCertificates();
                case RemoveConfigOperations.RemoveCSPSConfig:
                    return RemoveCSPSConfig();
                default:
                    throw new Exception("Un-supported Remove configuration function.");
            }
        }

        /// <summary>
        /// Stop Services.
        /// </summary>
        /// <returns>true if services stopped successfully, false otherwise</returns>
        public static bool StopUnifiedSetupServices()
        {
            try
            {
                // Always stop ProcessServerMonitor service first to avoid
                // generating alerts if other services are stopped during
                // install/upgrade/uninstall.
                List<string> serviceNames = new List<string>() 
                {
                    "ProcessServerMonitor",
                    "ProcessServer",
                    "cxprocessserver",
                    "InMage PushInstall",
                    "InMage Scout Application Service",
                    "svagents",
                    "tmansvc",
                    "obengine",
                    "dra",
                    "LogUpload"
                };

                Trc.Log(LogLevel.Always, "Stopping services.");
                foreach (var serviceName in serviceNames)
                {
                    if (ServiceControlFunctions.IsInstalled(serviceName))
                    {
                        if (!ServiceControlFunctions.StopService(serviceName))
                        {
                            Trc.Log(
                                LogLevel.Error,
                                "Stop service - {0} failed. Please stop it manually.", serviceName);
                            return false;
                        }

                        Trc.Log(
                            LogLevel.Always,
                            "Service - {0} stopped successfully.", serviceName);
                    }
                }

                return true;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while stopping services : {0}", ex.ToString());
                return false;
            }
        }

        /// <summary>
        /// Change Services startup type.
        /// </summary>
        /// <returns>true if services startup type changed, false otherwise</returns>
        public static bool ChangeUnifiedSetupServicesStartupTypeToManual()
        {
            try
            {
                List<string> serviceNames = new List<string>() 
                {
                    "ProcessServerMonitor",
                    "ProcessServer",
                    "cxprocessserver",
                    "InMage PushInstall",
                    "InMage Scout Application Service",
                    "svagents",
                    "tmansvc",
                    "obengine",
                    "dra",
                    "LogUpload"
                };
                string startupType = "Manual";

                Trc.Log(LogLevel.Always, "Changing services startup type.");
                foreach (var serviceName in serviceNames)
                {

                    if (ServiceControlFunctions.IsInstalled(serviceName))
                    {
                        if (!ServiceControlFunctions.ChangeServiceStartupType(serviceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START))
                        {
                            Trc.Log(
                                LogLevel.Error,
                                "Startup type change failed for service - {0}.", serviceName);
                            return false;
                        }

                        Trc.Log(
                            LogLevel.Always,
                            "Startup type change for service - {0} is successfully.", serviceName);
                    }
                }

                return true;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while changing services startup type : {0}", ex.ToString());
                return false;
            }
        }

        /// <summary>
        /// Uninstall MySQL and delete stale entries.
        /// </summary>
        /// <returns>true if MySQL uninstall, false otherwise</returns>
        public static bool UninstallMySQL()
        {
            try
            {
                string mySQLFilePath = Path.Combine(sysDrive, UnifiedSetupConstants.MySQLMsiPath);
                string mySQLInstallerFilePath = Path.Combine(sysDrive, UnifiedSetupConstants.MySQLFileName);
                string LogsPath = Path.Combine(
                        Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
                        UnifiedSetupConstants.LogFolder);
                string mySQLUninstallLog = "MySQLUninstall.log";
                string mySQLUninstallLogPath = Path.Combine(LogsPath, mySQLUninstallLog);
                string msiexec = "msiexec.exe";
                string UninstallCommandLineParams = "/x \"" + mySQLFilePath + "\" /q /norestart /L*V " + mySQLUninstallLogPath;
                string UninstallMySQLInstallerParams = "/x \"" + mySQLInstallerFilePath + "\" /q /norestart /L*V " + mySQLUninstallLogPath;
                string output = "";
                string errorMsg = "";
                string location = "Location";
                string dataLocation = "DataLocation";

                // Get MySQL installation directory from the registry.
                string mySQLInstallDir = (string)Registry.GetValue(
                    UnifiedSetupConstants.MySQLRegistryLMHive,
                    location,
                    string.Empty);
                string mySQLDataDir = (string)Registry.GetValue(
                    UnifiedSetupConstants.MySQLRegistryLMHive,
                    dataLocation,
                    string.Empty);

                Trc.Log(LogLevel.Always, "MySQL Install dir: " + mySQLInstallDir);
                Trc.Log(LogLevel.Always, "MySQL data dir: " + mySQLDataDir);

                Trc.Log(LogLevel.Always, "Stopping MySQL service.");

                if (!ServiceControlFunctions.StopService(UnifiedSetupConstants.MySQLProductName))
                {
                    Trc.Log(
                        LogLevel.Error,
                        "Stop service - {0} failed. Please stop it manually.", UnifiedSetupConstants.MySQLProductName);
                    return false;
                }

                Trc.Log(
                    LogLevel.Always,
                    "Service - {0} stopped successfully.", UnifiedSetupConstants.MySQLProductName);


                Trc.Log(LogLevel.Always, "Uninstalling MySQL.");

                int mySQLReturnCode =
                    CommandExecutor.ExecuteCommand(
                    msiexec,
                    out output,
                    out errorMsg,
                    UninstallCommandLineParams);

                if (mySQLReturnCode == 0)
                {
                    Trc.Log(LogLevel.Always, "MySQL uninstallation is successful.");
                }
                else
                {
                    Trc.Log(LogLevel.Error, "Failed to uninstall MySQL.");
                    return false;
                }

                int mySQLInstallerReturnCode =
                    CommandExecutor.ExecuteCommand(
                    msiexec,
                    out output,
                    out errorMsg,
                    UninstallMySQLInstallerParams);

                if (mySQLInstallerReturnCode == 0)
                {
                    Trc.Log(LogLevel.Always, "MySQL installer uninstallation is successful.");
                }
                else
                {
                    Trc.Log(LogLevel.Error, "Failed to uninstall MySQL installer.");
                    return false;
                }

                File.Delete(mySQLInstallerFilePath);
                Directory.Delete(mySQLDataDir, true);

                Trc.Log(LogLevel.Always, "MySQL data directory deleted.");

                string mySQLEnvString = string.Format("{0}lib;{0}bin;", mySQLInstallDir);
                string strPath = System.Environment.GetEnvironmentVariable("Path", EnvironmentVariableTarget.Machine);
                string result = strPath.Replace(mySQLEnvString, string.Empty);
                // Replace the current system path with strNewSystemPath.
                System.Environment.SetEnvironmentVariable("Path", result, EnvironmentVariableTarget.Machine);

                return true;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while uninstalling MySQL : {0}", ex.ToString());
                return false;
            }
        }


        /// <summary>
        /// Remove DataTransferSecurePort from firewall allow list.
        /// </summary>
        /// <returns>true if DataTransferSecurePort removed, false otherwise</returns>
        public static bool RemoveDataTransferSecurePort()
        {
            try
            {
                string csInstallDir = (string)Registry.GetValue(
                        UnifiedSetupConstants.CSPSRegistryName,
                        UnifiedSetupConstants.InstDirRegKeyName,
                        string.Empty);

                string cxpsConfPath = Path.Combine(csInstallDir, UnifiedSetupConstants.CXPSConfigRelativePath);
                string portValue = GrepUtils.GetKeyValueFromFile(cxpsConfPath, "ssl_port");
                string output = "";
                string errorMsg = "";

                string InstallCommandLineParams = "advfirewall firewall delete rule name=\"Open Port " + portValue + "\"";
                int firewallReturnCode =
                    CommandExecutor.ExecuteCommand(
                    "netsh",
                    out output,
                    out errorMsg,
                    InstallCommandLineParams);

                if (firewallReturnCode == 0)
                {
                    Trc.Log(LogLevel.Always, "DataTransferSecurePort removed successful.");
                }
                else
                {
                    Trc.Log(LogLevel.Error, "Failed to remove DataTransferSecurePort.");
                    return false;
                }

                return true;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while removing secured port from firewall allowed apps : {0}", ex.ToString());
                return false;
            }
            
        }

        /// <summary>
        /// Remove certificates.
        /// </summary>
        /// <returns>true if certificates removed, false otherwise</returns>
        public static bool RemoveCSPSCertificates()
        {
            string certIssuer = "Scout";
            try
            {
                string csInstallLocation = (string)Registry.GetValue(
                    UnifiedSetupConstants.CSPSRegistryName,
                    UnifiedSetupConstants.InstDirRegKeyName,
                    string.Empty);
                string csType =
                    GrepUtils.GetKeyValueFromFile(
                    Path.Combine(csInstallLocation, UnifiedSetupConstants.AmethystFile),
                    UnifiedSetupConstants.ConfigurationType);
                Trc.Log(LogLevel.Always, "csType: {0}", csType);

                // Removes certificates from local store only when Configuration and Process Server installed.
                if (csType == "3")
                {
                    Trc.Log(LogLevel.Always, "Removing Certificate from local store.");
                    if (ValidationHelper.RemoveCertificateFromStore(certIssuer))
                    {
                        Trc.Log(LogLevel.Always, "Certificate removed successfully from local store.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Error, "Failed to remove certificate from local store.");
                        return false;
                    }
                }

                string strProgramDataPath =
                        Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);

                string certsPath = Path.Combine(strProgramDataPath, @"Microsoft Azure Site Recovery\certs");
                string fingerprintsPath = Path.Combine(strProgramDataPath, @"Microsoft Azure Site Recovery\fingerprints");
                string privatePath = Path.Combine(strProgramDataPath, @"Microsoft Azure Site Recovery\private");

                // Delete certs and fingerprints
                Directory.Delete(certsPath, true);
                Directory.Delete(fingerprintsPath, true);
                Directory.Delete(privatePath, true);

                return true;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while removing certificates : {0}", ex.ToString());
                return false;
            }
        }

        /// <summary>
        /// Remove configuration.
        /// </summary>
        /// <returns>true if configuration removed, false otherwise</returns>
        public static bool RemoveCSPSConfig()
        {
            string ConfigNullValue = string.Empty;
            try
            {
                string agentInstallLocation = (string)Registry.GetValue(
                    UnifiedSetupConstants.AgentRegistryName,
                    UnifiedSetupConstants.InstDirRegKeyName,
                    string.Empty);
                string csInstallLocation = (string)Registry.GetValue(
                    UnifiedSetupConstants.CSPSRegistryName,
                    UnifiedSetupConstants.InstDirRegKeyName,
                    string.Empty);
                string csType =
                    GrepUtils.GetKeyValueFromFile(
                    Path.Combine(csInstallLocation, UnifiedSetupConstants.AmethystFile),
                    UnifiedSetupConstants.ConfigurationType);
                Trc.Log(LogLevel.Always, "csType: {0}", csType);

                IniFile cxpsConfigAgent = new IniFile(Path.Combine(agentInstallLocation, UnifiedSetupConstants.CXPSConfigRelativePath));
                cxpsConfigAgent.WriteValue("cxps", "cs_ip_address", ConfigNullValue);

                IniFile drScoutInfo = new IniFile(Path.Combine(
                    agentInstallLocation,
                    UnifiedSetupConstants.DrScoutConfigRelativePath));

                // VXAgent.Transport Section
                drScoutInfo.WriteValue("vxagent.transport", "HostName", ConfigNullValue);

                // VXAgent section
                drScoutInfo.WriteValue("vxagent", "ResourceId", ConfigNullValue);
                drScoutInfo.WriteValue("vxagent", "HostId", ConfigNullValue);
                drScoutInfo.WriteValue("vxagent", "SourceGroupId", ConfigNullValue);
                drScoutInfo.WriteValue("vxagent", "SystemUUID", ConfigNullValue);

                IniFile cxpsConfigCS = new IniFile(Path.Combine(csInstallLocation, UnifiedSetupConstants.CXPSConfigRelativePath));
                cxpsConfigCS.WriteValue("cxps", "id", ConfigNullValue);
                cxpsConfigCS.WriteValue("cxps", "cs_ip_address", ConfigNullValue);
                cxpsConfigCS.WriteValue("cxps", "ssl_port", ConfigNullValue);

                IniFile pushInstallConfig = new IniFile(Path.Combine(csInstallLocation, UnifiedSetupConstants.PushInstallConfigRelativePath));
                pushInstallConfig.WriteValue("PushInstaller", "Hostid", ConfigNullValue);
                pushInstallConfig.WriteValue("PushInstaller.transport", "Hostname", ConfigNullValue);

                string amethystConfig = Path.Combine(csInstallLocation, UnifiedSetupConstants.AmethystFile);
                RegistryKey inMageSystemsKey = Registry.LocalMachine.OpenSubKey(UnifiedSetupConstants.InmageRegistryHive, true);
                RegistryKey svSystemsKey = Registry.LocalMachine.OpenSubKey(UnifiedSetupConstants.SvSystemsRegistryHive, true);

                inMageSystemsKey.SetValue(UnifiedSetupConstants.MachineIdentifierRegKeyName, ConfigNullValue);
                svSystemsKey.SetValue(UnifiedSetupConstants.HostIdRegKeyName, ConfigNullValue);
                svSystemsKey.SetValue(UnifiedSetupConstants.ServernameRegKeyName, ConfigNullValue);

                if (csType == "3")
                {
                    ValidationHelper.ReplaceLineinFile(amethystConfig, "DB_PASSWD =", "DB_PASSWD = ");
                    ValidationHelper.ReplaceLineinFile(amethystConfig, "DB_ROOT_PASSWD =", "DB_ROOT_PASSWD = ");
                    ValidationHelper.ReplaceLineinFile(amethystConfig, "MYSQL_PATH =", "MYSQL_PATH = ");
                    ValidationHelper.ReplaceLineinFile(amethystConfig, "MYSQL_DATA_PATH =", "MYSQL_DATA_PATH = ");
                    ValidationHelper.ReplaceLineinFile(amethystConfig, "CS_IP =", "CS_IP = ");
                    ValidationHelper.ReplaceLineinFile(amethystConfig, "IP_ADDRESS_FOR_AZURE_COMPONENTS =", "IP_ADDRESS_FOR_AZURE_COMPONENTS = ");
                    ValidationHelper.ReplaceLineinFile(amethystConfig, "AZURE_NIC_DESC =", "AZURE_NIC_DESC = ");
                }
                if (csType == "2")
                {
                    ValidationHelper.ReplaceLineinFile(amethystConfig, "DB_HOST =", "DB_HOST = ");
                    svSystemsKey.SetValue(UnifiedSetupConstants.ServernameRegKeyName, ConfigNullValue);
                }
                ValidationHelper.ReplaceLineinFile(amethystConfig, "BPM_NTW_DEVICE =", "BPM_NTW_DEVICE = ");
                ValidationHelper.ReplaceLineinFile(amethystConfig, "PS_IP =", "PS_IP = ");
                ValidationHelper.ReplaceLineinFile(amethystConfig, "PS_CS_IP =", "PS_CS_IP = ");
                ValidationHelper.ReplaceLineinFile(amethystConfig, "HOST_GUID =", "HOST_GUID = ");

                return true;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while removing configuration : {0}", ex.ToString());
                return false;
            }
        }
    }
}
