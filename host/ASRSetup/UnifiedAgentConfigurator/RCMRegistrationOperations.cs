using ASRResources;
using ASRSetupFramework;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using Microsoft.Win32;
using UnifiedAgentConfigurator.Credentials;

namespace UnifiedAgentConfigurator
{
    /// <summary>
    /// Routines for RCM registration.
    /// </summary>
    internal partial class RegistrationOperations
    {
        /// <summary>
        /// Prepares for registration
        /// </summary>
        /// <param name="csIP">Configuration server IP.</param>
        /// <param name="csPassphraseFile">Passphrase file.</param>
        /// <returns>Operation result.</returns>
        internal static OperationResult PrepareForRCMRegistration(string credentialsPath)
        {
            try
            {
                Trc.Log(
                    LogLevel.Info,
                    "Preparaing RCM registration. Provided credentials file path '{0}'.",
                    credentialsPath);

                var opResult = new OperationResult
                {
                    Status = OperationStatus.Completed,
                    Error = string.Empty
                };

                if (!File.Exists(credentialsPath))
                {
                    opResult.Error = StringResources.CredentialsFileMissing;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.CredentialsFileMissing;
                    opResult.Status = OperationStatus.Failed;
                    return opResult;
                }

                Trc.Log(LogLevel.Debug, "RcmCredentials received: {0}", File.ReadAllText(credentialsPath));
                using (StreamReader streamReader = new StreamReader(credentialsPath))
                {
                    try
                    {                        
                        JsonSerializer jsonSerializer = new JsonSerializer();
                        var creds = jsonSerializer.Deserialize(
                            streamReader,
                            typeof(RcmCredentials)) as RcmCredentials;
                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.RcmCredentials, creds);
                    }
                    catch(OutOfMemoryException)
                    {
                        throw;
                    }
                    catch (Exception ex)
                    {
                        Trc.LogErrorException("Failed to deserialize RCM creds.", ex);
                        opResult.Error = StringResources.CorruptCredentialsFile;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.CorruptCredentialsFile;
                        opResult.Status = OperationStatus.Failed;
                        return opResult;
                    }
                }

                return opResult;
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "CRITICAL: Hit exception during prepare for RCM registration {0}",
                    ex);
                return new OperationResult
                {
                    Error = StringResources.AgentRegistrationFailed,
                    ProcessExitCode = SetupHelper.UASetupReturnValues.AgentRegistrationFailed,
                    Status = OperationStatus.Failed
                };
            }
        }

        /// <summary>
        /// Register machine with RCM.
        /// </summary>
        /// <returns>Operation result.</returns>
        internal static OperationResult RegisterWithRCM()
        {
            try
            {
                var opResult = new OperationResult
                {
                    Status = OperationStatus.Completed,
                    Error = string.Empty
                };

                // Change svagent and appservice startuptype to Manual.
                if (!SetupHelper.ChangeAgentServicesStartupType(
                    NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START))
                {
                    opResult.Error = StringResources.FailedToSetServiceStartupTypeToManual;
                    opResult.ProcessExitCode =
                        SetupHelper.UASetupReturnValues.FailedToSetServiceStartupType;
                    opResult.Status = OperationStatus.Failed;
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Configuration,
                        OpName.ChangeAgentServicesStartupTypeToManual,
                        OpResult.Failed,
                        (int)opResult.ProcessExitCode);
                    return opResult;
                }
                else
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Configuration,
                        OpName.ChangeAgentServicesStartupTypeToManual,
                        OpResult.Success);
                }

                // Stop InMage Services
                Trc.Log(LogLevel.Always, "Stopping InMage agent services.");
                if (!SetupHelper.ShutdownInMageAgentServices())
                {
                    opResult.Error = StringResources.FailedToStopServices;
                    opResult.Status = OperationStatus.Failed;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.UnableToStopServices;
                    return opResult;
                }

                string biosId = SetupHelper.GetBiosHardwareId();

                if (string.IsNullOrEmpty(biosId))
                {
                    opResult.Error = StringResources.UnableToDetermineBiosId;
                    opResult.Status = OperationStatus.Failed;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.BiosIdDetectionFailed;
                    return opResult;
                }

                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.BiosId,
                    biosId);

                X509Certificate2 authCert = GetAADCertificate();

                if (authCert.NotAfter < DateTime.Now)
                {
                    opResult.Error = StringResources.ExpiredCredentialsFile;
                    opResult.Status = OperationStatus.Failed;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.CredentialsFileExpired;
                    return opResult;
                }

                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.RcmAuthCertThumbprint,
                    authCert.Thumbprint);

                int exitCode;
                ConfigureForRCM(out exitCode);
                if (exitCode != 0)
                {
                    opResult.Error = StringResources.AzureRcmCliCleanupFailed;
                    opResult.Status = OperationStatus.Failed;

                    if (exitCode == 2)
                    {
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors;
                    }
                    else if (exitCode == 148)
                    {
                        opResult.ProcessExitCode = (SetupHelper.UASetupReturnValues)exitCode;
                    }
                    else
                    {
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.RCMRegistrationFailure;
                    }
                    
                    return opResult;
                }

                string installationPath = SetupHelper.GetAgentInstalledLocation();
                string rcmCliPath = Path.Combine(
                    installationPath,
                    UnifiedSetupConstants.AzureRCMCliExeName);

                if (!RegisterMachine(rcmCliPath, out exitCode))
                {
                    opResult.Error = StringResources.FailedToRegisterWithRCM;
                    opResult.Status = OperationStatus.Failed;
                    if (exitCode == 2)
                    {
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors;
                    }
                    else if (exitCode == 148)
                    {
                        opResult.ProcessExitCode = (SetupHelper.UASetupReturnValues)exitCode;
                    }
                    else
                    {
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.RCMRegistrationFailure;
                    }
                    return opResult;
                }

                if (!RegisterSourceAgent(rcmCliPath, out exitCode))
                {
                    opResult.Error = StringResources.FailedToRegisterWithRCM;
                    opResult.Status = OperationStatus.Failed;
                    if (exitCode == 2)
                    {
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors;
                    }
                    else if (exitCode == 148)
                    {
                        opResult.ProcessExitCode = (SetupHelper.UASetupReturnValues)exitCode;
                    }
                    else
                    {
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.RCMRegistrationFailure;
                    }
                    return opResult;
                }

                // Change svagent and appservice service startuptype to Automatic (Delayed Start).
                if (!SetupHelper.ChangeAgentServicesStartupType(
                    NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START))
                {
                    opResult.Error = StringResources.FailedToSetServiceStartupTypeToAutomatic;
                    opResult.ProcessExitCode =
                        SetupHelper.UASetupReturnValues.FailedToSetServiceStartupType;
                    opResult.Status = OperationStatus.Failed;
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Configuration,
                        OpName.ChangeAgentServicesStartupTypeToAutomatic,
                        OpResult.Failed,
                        (int)opResult.ProcessExitCode);
                    return opResult;
                }
                else
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Configuration,
                        OpName.ChangeAgentServicesStartupTypeToAutomatic,
                        OpResult.Success);
                }
                

                if (!ConfigureActionProcessor.BootServices())
                {
                    opResult.Error = StringResources.FailedToStartServices;
                    opResult.Status = OperationStatus.Failed;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.UnableToStartServices;
                    return opResult;
                }

                SetupReportData.AddLogFilePath(Path.Combine(installationPath, "AzureRcmCli.log"));

                return opResult;
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "CRITICAL: Hit exception during RCM registration {0}",
                    ex);
                return new OperationResult
                {
                    Error = StringResources.AgentRegistrationFailed,
                    ProcessExitCode = SetupHelper.UASetupReturnValues.AgentRegistrationFailed,
                    Status = OperationStatus.Failed
                };
            }
        }

        /// <summary>
        /// Unconfigure the agent.
        /// </summary>
        /// <returns>true if unconfigure succeeds, false otherwise</returns>
        internal static bool UnconfigureAgentCSPrime()
        {
            int exitCode = 0;
            string installationPath = SetupHelper.GetAgentInstalledLocation();
            string rcmCliPath = Path.Combine(
                installationPath,
                UnifiedSetupConstants.AzureRCMCliExeName);
            string procOutput, procError;

            // Unregister source agent.
            UnregisterSourceAgent(rcmCliPath);

            // Unregister machine.
            UnregisterMachine(rcmCliPath);

            // Clean up existing settings.
            exitCode = CommandExecutor.ExecuteCommand(
                rcmCliPath,
                out procOutput,
                out procError,
                "--cleanup " + UnifiedSetupConstants.AzureRcmCliErrorFileName + " \"" +
                PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath)
                + "\"");
            if (exitCode != 0)
            {
                Trc.Log(
                    LogLevel.Error,
                    "AzureRcmCli --cleanup operation has failed with exitcode: {0}",
                    exitCode);
            }

            Trc.Log(LogLevel.Always, "Unconfigure agent exitCode: {0}", exitCode);

            return (exitCode == 0);
        }
        /// <summary>
        /// Unconfigure the agent.
        /// </summary>
        /// <returns>true if unconfigure succeeds, false otherwise</returns>
        internal static bool DiagnoseAndFix()
        {
            int exitCode = 0;
            string installationPath = SetupHelper.GetAgentInstalledLocation();
            string rcmCliPath = Path.Combine(
                installationPath,
                UnifiedSetupConstants.AzureRCMCliExeName);
            string procOutput, procError;

            // Diagnose And Fix
            exitCode = CommandExecutor.ExecuteCommand(
                rcmCliPath,
                out procOutput,
                out procError,
                "--diagnoseandfix " + UnifiedSetupConstants.AzureRcmCliErrorFileName + " \"" +
                PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath)
                + "\"");
            if (exitCode != 0)
            {
                Trc.Log(
                    LogLevel.Error,
                    "AzureRcmCli --diagnoseandfix operation has failed with exitcode: {0}",
                    exitCode);
            }

            Trc.Log(LogLevel.Always, "DiagnoseAndFix agent exitCode: {0}", exitCode);

            return (exitCode == 0);
        }

        /// <summary>
        /// Updates drscout with hostid
        /// </summary>
        internal static void UpdateDrScoutForAgentUnconfigure()
        {
            Trc.Log(
               LogLevel.Always,
               "Updating drscout file for agent unconfigure.");

            string installationDir = SetupHelper.GetAgentInstalledLocation();
            IniFile drScoutInfo = new IniFile(Path.Combine(
                installationDir,
                UnifiedSetupConstants.DrScoutConfigRelativePath));

            // Updating hostId.
            drScoutInfo.WriteValue(
                "vxagent",
                "HostId",
                "");
        }

        /// <summary>
        /// Updates registry.
        /// </summary>
        internal static void UpdateRegistryForAgentUnconfigure()
        {
            Trc.Log(
               LogLevel.Always,
               "Updating Registry for agent unconfigure.");

            RegistryKey hklm = Registry.LocalMachine;

            using (RegistryKey inMageSvSys = hklm.OpenSubKey(UnifiedSetupConstants.SvSystemsRegistryHive, true))
            {
                inMageSvSys.SetValue(
                    "HostId",
                    "",
                    RegistryValueKind.String);
            }
        }

        #region private methods

        /// <summary>
        /// Gets AAD authentication certificate.
        /// </summary>
        /// <returns>Certificate object.</returns>
        private static X509Certificate2 GetAADCertificate()
        {
            RcmCredentials rcmCreds = PropertyBagDictionary.Instance.GetProperty<RcmCredentials>(
                PropertyBagConstants.RcmCredentials);

            X509Certificate2 registrationCert =
                new X509Certificate2(
                Convert.FromBase64String(rcmCreds.AADDetails.AuthCertificate),
                    (string)null,
                    X509KeyStorageFlags.MachineKeySet | X509KeyStorageFlags.PersistKeySet);

            return registrationCert;
        }

        /// <summary>
        /// Sets DR scout config for RCM.
        /// </summary>
        private static bool ConfigureForRCM(out int exitCode)
        {
            Trc.Log(
                LogLevel.Always,
                "Updating DrScout and Rcm config for RCM");

            exitCode = 0;
            string installationDir = SetupHelper.GetAgentInstalledLocation();
            string appData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
            string rcmInfoPath = Path.Combine(appData, UnifiedSetupConstants.RcmConfRelativePath);
            IniFile drScoutInfo = new IniFile(Path.Combine(
                installationDir, 
                UnifiedSetupConstants.DrScoutConfigRelativePath));
            IniFile rcmConf = new IniFile(rcmInfoPath);

            RcmCredentials rcmCreds = PropertyBagDictionary.Instance.GetProperty<RcmCredentials>(
                PropertyBagConstants.RcmCredentials);

            X509Store machineCertificateStore = null;
            machineCertificateStore = new X509Store(StoreName.My, StoreLocation.LocalMachine);
            machineCertificateStore.Open(OpenFlags.ReadWrite);
            X509Certificate2 registrationCert =
                new X509Certificate2(
                Convert.FromBase64String(rcmCreds.AADDetails.AuthCertificate),
                    (string)null,
                    X509KeyStorageFlags.MachineKeySet | X509KeyStorageFlags.PersistKeySet);

            if (registrationCert.NotAfter < DateTime.Now)
            {
                throw new Exception(StringResources.ExpiredCredentialsFile);
            }

            machineCertificateStore.Add(registrationCert);
            machineCertificateStore.Close();

            UnregisterAgentAndCleanUp(
                rcmCreds.ManagementId,
                rcmConf.ReadValue("rcm", "ManagementId"),
                rcmConf.ReadValue("rcm", "BiosId"),
                installationDir,
                out exitCode);
            if (exitCode != 0)
            {
                return false;
            }

            if (!rcmCreds.ClusterManagementId.IsNullOrWhiteSpace())
            {
                Trc.Log(LogLevel.Always, "ClusterManagementId: {0}", rcmCreds.ClusterManagementId);
                rcmConf.WriteValue(
                "rcm",
                "ClusterManagementId",
                rcmCreds.ClusterManagementId);
            }
            else
            {
                Trc.Log(LogLevel.Always, "ClusterManagementId is null or empty.");
            }

            rcmConf.WriteValue(
                "rcm",
                "ServiceUri",
                rcmCreds.RcmUri);
            rcmConf.WriteValue(
                "rcm",
                "ManagementId",
                rcmCreds.ManagementId);
            rcmConf.WriteValue(
                "rcm",
                "ClientRequestId",
                rcmCreds.ClientRequestId);
            rcmConf.WriteValue(
                "rcm",
                "ActivityId",
                rcmCreds.ActivityId);
            rcmConf.WriteValue(
                "rcm",
                "BiosId",
                PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.BiosId));

            //EndPointType.
            rcmConf.WriteValue(
                "rcm",
                "EndPointType",
                rcmCreds.VMNetworkTypeBasedOnPrivateEndpoint);

            // Authentication Details
            rcmConf.WriteValue(
                "rcm.auth",
                "AADUri",
                rcmCreds.AADDetails.ServiceUri);
            rcmConf.WriteValue(
                "rcm.auth",
                "AADAuthCert",
                PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.RcmAuthCertThumbprint));
            rcmConf.WriteValue(
                "rcm.auth",
                "AADTenantId",
                rcmCreds.AADDetails.TenantId);
            rcmConf.WriteValue(
                "rcm.auth",
                "AADClientId",
                rcmCreds.AADDetails.ClientId);
            rcmConf.WriteValue(
                "rcm.auth",
                "AADAudienceUri",
                rcmCreds.AADDetails.AudienceUri);

            // Relay details if present.
            if (rcmCreds.RelayDetails != null && !string.IsNullOrEmpty(rcmCreds.RelayDetails.RelayKeyPolicyName))
            {
                rcmConf.WriteValue(
                    "rcm.relay",
                    "RelayKeyPolicyName",
                    rcmCreds.RelayDetails.RelayKeyPolicyName);
                rcmConf.WriteValue(
                    "rcm.relay",
                    "RelaySharedKey",
                    rcmCreds.RelayDetails.RelaySharedKey);
                rcmConf.WriteValue(
                    "rcm.relay",
                    "RelayServicePathSuffix",
                    rcmCreds.RelayDetails.RelayServicePathSuffix);
                rcmConf.WriteValue(
                    "rcm.relay",
                    "ExpiryTimeoutInSeconds",
                    rcmCreds.RelayDetails.ExpiryTimeoutInSeconds.ToString());

                // Set MS agent to run in relay connection mode.
                rcmConf.WriteValue(
                    "rcm",
                    "ServiceConnectionMode",
                    "relay");
            }
            else
            {
                // Set MS agent to run in direct service connection mode.
                rcmConf.WriteValue(
                    "rcm",
                    "ServiceConnectionMode",
                    "direct");
            }

            // Update DRScout config to the RCMInfo file.
            drScoutInfo.WriteValue(
                "vxagent",
                "RcmSettingsPath",
                rcmInfoPath);

            // Updating hostId.
            drScoutInfo.WriteValue(
                "vxagent",
                "HostId",
                PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.HostId));
            drScoutInfo.WriteValue(
                "vxagent.transport",
                "Https",
                "1");

            return true;
        }

        /// <summary>
        /// Tries to Register the Machine.
        /// </summary>
        /// <param name="rcmCliPath">Path of RcmCli.exe</param>
        /// <out="exitCode">exitCode from RcmCli</out>
        /// <returns>true if registration succeeds, false otherwise</returns>
        private static bool RegisterMachine(string rcmCliPath, out int exitCode)
        {
            bool isMachineRegistered = false;
            exitCode = -1;
            string parameters;

            string installationDir = SetupHelper.GetAgentInstalledLocation();
            IniFile drScoutInfo = new IniFile(Path.Combine(
                installationDir,
                UnifiedSetupConstants.DrScoutConfigRelativePath));

            for (int i = 0; i < ConfigurationConstants.NetOpsRetryCount ; i++)
            {
                Trc.Log(LogLevel.Always, "Register machine count : {0}", i);

                if (String.Equals(drScoutInfo.ReadValue("vxagent", PropertyBagConstants.CSType),
                    ConfigurationServerType.CSPrime.ToString(),
                    StringComparison.OrdinalIgnoreCase))
                {
                    parameters = "--registermachine " + UnifiedSetupConstants.AzureRcmCliErrorFileName +
                        $" \"{PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath)}\" " +
                        UnifiedSetupConstants.AzureRcmCliLogFileName +
                        $" \"{PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.DefaultLogName)}\"";
                }
                else
                {
                    parameters = "--registermachine " + UnifiedSetupConstants.AzureRcmCliErrorFileName +
                        $" \"{PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath)}\"";
                }

                exitCode = CommandExecutor.RunCommand(rcmCliPath, parameters);
                Trc.Log(LogLevel.Always, "Register machine Iteration: {0} Exit code: {1}", i, exitCode);
                if (exitCode == 0)
                {
                    Trc.Log(LogLevel.Always, "Register machine executed successfully.");
                    isMachineRegistered = true;
                    break;
                }
                else if (exitCode == 148)
                {
                    Trc.Log(LogLevel.Always,
                        "Stopping service - {0}",
                        UnifiedSetupConstants.SvagentsServiceName);
                    if (!ServiceControlFunctions.StopService(
                            UnifiedSetupConstants.SvagentsServiceName))
                    {
                        ServiceControlFunctions.KillServiceProcess(
                            UnifiedSetupConstants.SvagentsServiceName);
                    }
                }
            }
            return isMachineRegistered;
        }

        /// <summary>
        /// Tries to Register the Source Agent.
        /// </summary>
        /// <param name="rcmCliPath">Path of RcmCli.exe</param>
        /// <out="exitCode">exitCode from RcmCli</out>
        /// <returns>true if registration succeeds, false otherwise</returns>
        private static bool RegisterSourceAgent(string rcmCliPath, out int exitCode)
        {
            bool isSourceAgentRegistered = false;
            exitCode = -1;
            string parameters;

            string installationDir = SetupHelper.GetAgentInstalledLocation();
            IniFile drScoutInfo = new IniFile(Path.Combine(
                installationDir,
                UnifiedSetupConstants.DrScoutConfigRelativePath));

            for (int i = 0; i < ConfigurationConstants.NetOpsRetryCount ; i++)
            {
                Trc.Log(LogLevel.Always, "Register source agent count : {0}", i);

                if (String.Equals(drScoutInfo.ReadValue("vxagent", PropertyBagConstants.CSType),
                    ConfigurationServerType.CSPrime.ToString(),
                    StringComparison.OrdinalIgnoreCase))
                {
                    parameters = "--registersourceagent " + UnifiedSetupConstants.AzureRcmCliErrorFileName +
                        $" \"{PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath)}\" " +
                        UnifiedSetupConstants.AzureRcmCliLogFileName +
                        $" \"{PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.DefaultLogName)}\"";
                }
                else
                {
                    parameters = "--registersourceagent " + UnifiedSetupConstants.AzureRcmCliErrorFileName +
                        $" \"{PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath)}\"";
                }

                exitCode = CommandExecutor.RunCommand(rcmCliPath, parameters);
                Trc.Log(LogLevel.Always, "Register source agent Iteration: {0} Exit code: {1}", i, exitCode);
                if (exitCode == 0)
                {
                    Trc.Log(LogLevel.Always, "Register source agent executed successfully.");
                    isSourceAgentRegistered = true;
                    break;
                }
                else if (exitCode == 148)
                {
                    Trc.Log(LogLevel.Always,
                        "Stopping service - {0}",
                        UnifiedSetupConstants.SvagentsServiceName);
                    if (!ServiceControlFunctions.StopService(
                            UnifiedSetupConstants.SvagentsServiceName))
                    {
                        ServiceControlFunctions.KillServiceProcess(
                            UnifiedSetupConstants.SvagentsServiceName);
                    }
                }
            }
            return isSourceAgentRegistered;
        }

        /// <summary>
        /// Tries to unregister the source agent.
        /// </summary>
        /// <param name="rcmCliPath">Path of RcmCli.exe</param>
        private static void UnregisterSourceAgent(string rcmCliPath)
        {
            string processOutput, processError;
            int exitCode = -1;
            for (int i = 0; i < ConfigurationConstants.NetOpsRetryCount; i++)
            {
                Trc.Log(LogLevel.Always, "Unregister source agent count : {0}", i);
                string parameters = "--unregistersourceagent " + UnifiedSetupConstants.AzureRcmCliErrorFileName +
                $" \"{PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath)}\"";
                exitCode = CommandExecutor.ExecuteCommand(
                    rcmCliPath,
                    out processOutput,
                    out processError,
                    parameters);

                if (exitCode == 0)
                {
                    Trc.Log(
                        LogLevel.Always, 
                        "Unregister source agent executed successfully.");
                    return;
                }
                else if (exitCode == 148)
                {
                    Trc.Log(LogLevel.Always,
                        "Stopping service - {0}",
                        UnifiedSetupConstants.SvagentsServiceName);
                    if (!ServiceControlFunctions.StopService(
                            UnifiedSetupConstants.SvagentsServiceName))
                    {
                        ServiceControlFunctions.KillServiceProcess(
                            UnifiedSetupConstants.SvagentsServiceName);
                    }
                }
            }

            Trc.Log(
                LogLevel.Always, 
                "Failed to unregister source agent. Exitcode : {0}", 
                exitCode);
        }

        /// <summary>
        /// Tries to unregister the machine.
        /// </summary>
        /// <param name="rcmCliPath">Path of RcmCli.exe</param>
        private static void UnregisterMachine(string rcmCliPath)
        {
            string processOutput, processError;
            int exitCode = -1;
            for (int i = 0; i < ConfigurationConstants.NetOpsRetryCount ; i++)
            {
                Trc.Log(
                    LogLevel.Always, 
                    "Unregister machine count : {0}", 
                    i);
                string parameters = "--unregistermachine " + UnifiedSetupConstants.AzureRcmCliErrorFileName +
                $" \"{PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath)}\"";
                exitCode = CommandExecutor.ExecuteCommand(
                    rcmCliPath,
                    out processOutput,
                    out processError,
                    parameters);

                if (exitCode == 0)
                {
                    Trc.Log(
                        LogLevel.Always, 
                        "Unregister machine executed successfully.");
                    return;
                }
                else if (exitCode == 148)
                {
                    Trc.Log(LogLevel.Always,
                        "Stopping service - {0}",
                        UnifiedSetupConstants.SvagentsServiceName);
                    if (!ServiceControlFunctions.StopService(
                            UnifiedSetupConstants.SvagentsServiceName))
                    {
                        ServiceControlFunctions.KillServiceProcess(
                            UnifiedSetupConstants.SvagentsServiceName);
                    }
                }
            }

            Trc.Log(
                LogLevel.Always, 
                "Failed to unregister machine. Exitcode : {0}", 
                exitCode);
        }

        /// <summary>
        /// Clean up existing settings, unregister machine/agent and generate new HostId 
        /// if ManagementId in RCMCredentials file is different from RCMInfo.conf. 
        /// </summary>
        /// <param name="RCMCredsManagementId">ManagementId in RCM creds file</param>
        /// <param name="RCMConfManagementId">ManagementId in RCM conf file</param>
        /// <param name="installationDir">Agent installation direxctory</param>
        /// <param name="exitCode">Exit code returned by AzureRcmCli.</param>        
        private static void UnregisterAgentAndCleanUp(
            string RCMCredsManagementId, 
            string RCMConfManagementId,
            string RCMConfBiosId,
            string installationDir,
            out int exitCode)
        {
            bool generateHostid = false;
            string systemBiosId = SetupHelper.GetBiosHardwareId();
            exitCode = 0;
            Trc.Log(LogLevel.Always, "RCMCredsManagementId: {0}", RCMCredsManagementId);
            Trc.Log(LogLevel.Always, "RCMConfManagementId: {0}", RCMConfManagementId);
            Trc.Log(LogLevel.Always, "RCMConfBiosId: {0}", RCMConfBiosId);
            Trc.Log(LogLevel.Always, "systemBiosId: {0}", systemBiosId);

            if (!string.IsNullOrEmpty(RCMConfManagementId))
            {
                if (!string.Equals(RCMCredsManagementId, RCMConfManagementId, StringComparison.OrdinalIgnoreCase))
                {
                    Trc.Log(LogLevel.Always, "RCMCredsManagementId and RCMConfManagementId are different.");
                    string rcmCliPath = Path.Combine(
                        installationDir,
                        UnifiedSetupConstants.AzureRCMCliExeName);
                    string procOutput, procError;

                    // Unregister source agent.
                    UnregisterSourceAgent(rcmCliPath);

                    // Unregister machine.
                    UnregisterMachine(rcmCliPath);

                    // Clean up existing settings.
                    exitCode = CommandExecutor.ExecuteCommand(
                        rcmCliPath,
                        out procOutput,
                        out procError,
                        "--cleanup " + UnifiedSetupConstants.AzureRcmCliErrorFileName + " \"" +
                PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath)
                + "\"");
                    if (exitCode != 0)
                    {
                        Trc.Log(
                            LogLevel.Error,
                            "AzureRcmCli --cleanup operation has failed with exitcode: {0}",
                            exitCode);
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always,
                        "Generating new HostId as RCMCredsManagementId and RCMConfManagementId are different.");
                        generateHostid = true;
                    }
                }
                else
                {
                    Trc.Log(LogLevel.Always, "RCMCredsManagementId and RCMConfManagementId are same.");
                    /* Return error, when ManagementID in RCM creds 
                    * and ManagementID in RCM Conf file are same (and) 
                    * system biosId doesn't match with biosid 
                    * in rcminfo.conf
                    */
                    if (!string.Equals(systemBiosId, RCMConfBiosId, StringComparison.OrdinalIgnoreCase))
                    {
                        Trc.Log(LogLevel.Error,
                            "ManagementID in RCM Credentials file and RCM Conf file are same, BiosId of the machine and biosId in the RCM Config are different.");
                        exitCode = (int)SetupHelper.UASetupReturnValues.BiosIdMismatchFound;
                    }
                }
            }
            else
            {
                generateHostid = true;
            }

            if (generateHostid)
            {
                Trc.Log(LogLevel.Always, "Generating new hostId.");
                string hostId = Guid.NewGuid().ToString();
                Trc.Log(LogLevel.Always, "HostId value: {0}", hostId);
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.HostId,
                    hostId);
            }
            Trc.Log(LogLevel.Always, "UnregisterAgentAndCleanUp exitCode: {0}", exitCode);
        }
        #endregion private methods
    }
}
