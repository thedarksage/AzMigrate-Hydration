using ASRResources;
using ASRSetupFramework;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace UnifiedAgentConfigurator
{
    /// <summary>
    /// Routines for CS registration.
    /// </summary>
    internal partial class RegistrationOperations
    {
        #region internal methods

        /// <summary>
        /// Prepares for registration
        /// </summary>
        /// <param name="csIP">Configuration server IP.</param>
        /// <param name="csPassphraseFile">Passphrase file.</param>
        /// <returns>Operation result.</returns>
        internal static OperationResult PrepareForCSRegistration(
            string csIP,
            string csPassphraseFile)
        {
            try
            {
                var opResult = new OperationResult
                {
                    Status = OperationStatus.Completed,
                    Error = string.Empty
                };

                bool? isAgentPointedToSameCS = IsAgentPointedToSameCS(csIP);
                if (isAgentPointedToSameCS == true)
                {
                    Trc.Log(LogLevel.Always, StringResources.AgentPointingToSameCS);
                }
                else if (isAgentPointedToSameCS == false)
                {
                    opResult.Error = StringResources.AgentPointingToDifferentCS;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.AgentPointingToDifferentCS;
                    opResult.Status = OperationStatus.Failed;

                    SummaryFileWrapper.RecordOperation(
                        Scenario.PreConfiguration,
                        OpName.AgentPointingToDifferentCS,
                        OpResult.Failed,
                        (int)opResult.ProcessExitCode,
                        opResult.Error);

                    return opResult;
                }

                // Validate external IP.
                if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.ExternalIP))
                {
                    string externalIP = PropertyBagDictionary.Instance.GetProperty<string>(
                        PropertyBagConstants.ExternalIP);
                    Trc.Log(LogLevel.Always, "externalIP : {0}", externalIP);

                    if (!string.IsNullOrEmpty(externalIP))
                    {
                        if (!ValidationHelper.ValidateIPAddress(externalIP))
                        {
                            opResult.Error = StringResources.InvalidIPAddress;
                            opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.InvalidCommandLineArguments;
                            opResult.Status = OperationStatus.Failed;
                            SummaryFileWrapper.RecordOperation(
                                Scenario.PreConfiguration,
                                OpName.ExternalIPValidation,
                                OpResult.Failed,
                                (int)opResult.ProcessExitCode);
                            return opResult;
                        }
                        else
                        {
                            SummaryFileWrapper.RecordOperation(
                                Scenario.PreConfiguration,
                                OpName.ExternalIPValidation,
                                OpResult.Success);
                        }
                    }
                }

                UpdateDrScout();

                IniFile drScoutInfo = new IniFile(Path.Combine(
                        SetupHelper.GetAgentInstalledLocation(),
                        UnifiedSetupConstants.DrScoutConfigRelativePath));

                if (!ValidationHelper.PSExists())
                {
                    Trc.Log(
                        LogLevel.Always,
                        "Starting validation of CSPassphrase.");

                    // Validate passphrase.
                    if (!ValidationHelper.ValidateConnectionPassphrase(
                        csPassphraseFile,
                        csIP,
                        PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSPort)))
                    {
                        drScoutInfo.WriteValue(
                            "vxagent.transport",
                            "CSIPforTelemetry",
                            "");
                        opResult.Error = StringResources.CSDetailsIncorrectPassphraseText;
                        opResult.ProcessExitCode =
                            SetupHelper.UASetupReturnValues.InvalidPassphraseOrCSEndPoint;
                        opResult.Status = OperationStatus.Failed;
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreConfiguration,
                            OpName.ConnectionPassphraseValidation,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode);
                        return opResult;
                    }
                    else
                    {
                        drScoutInfo.WriteValue(
                            "vxagent.transport",
                            "CSIPforTelemetry",
                            csIP);
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreConfiguration,
                            OpName.ConnectionPassphraseValidation,
                            OpResult.Success);
                    }
                }

                // Dump data into propertybag dictionary.
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.CSEndpoint,
                    csIP);

                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.PassphraseFilePath,
                    csPassphraseFile);

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
                    "CRITICAL: Hit exception during prepare for CS registration {0}",
                    ex);
                SummaryFileWrapper.RecordOperation(
                    Scenario.PreConfiguration,
                    OpName.PrerapeForRegistration,
                    OpResult.Failed,
                    (int)SetupHelper.UASetupReturnValues.AgentRegistrationFailed,
                    StringResources.AgentRegistrationFailed);
                return new OperationResult
                {
                    Error = StringResources.AgentRegistrationFailed,
                    ProcessExitCode = SetupHelper.UASetupReturnValues.AgentRegistrationFailed,
                    Status = OperationStatus.Failed
                };
            }
        }

        /// <summary>
        /// Prepares for registration
        /// </summary>
        /// <param name="sourceConfigFilePath">Source config file.</param>
        /// <returns>Operation result.</returns>
        internal static OperationResult PrepareForCSPrimeRegistration(
            string sourceConfigFilePath)
        {
            try
            {
                var opResult = new OperationResult
                {
                    Status = OperationStatus.Completed,
                    Error = string.Empty
                };

                if(IsAgentPointedToCS())
                {
                    opResult.Error = StringResources.AgentPointingToDifferentCS;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.AgentPointingToDifferentCS;
                    opResult.Status = OperationStatus.Failed;

                    SummaryFileWrapper.RecordOperation(
                        Scenario.PreConfiguration,
                        OpName.AgentPointingToDifferentCS,
                        OpResult.Failed,
                        (int)opResult.ProcessExitCode,
                        opResult.Error);

                    return opResult;
                }

                string biosId = SetupHelper.GetBiosHardwareId();
                if (string.IsNullOrEmpty(biosId) || biosId.Trim().Length == 0)
                {
                    opResult.Error = StringResources.UnableToDetermineBiosId;
                    opResult.Status = OperationStatus.Failed;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.BiosIdDetectionFailed;
                    return opResult;
                }
                else
                {
                    PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.BiosId, biosId);
                }

                UpdateRcmConf();
                UpdateDrScoutForCSPrime();

                Trc.Log(
                        LogLevel.Always,
                        "Starting validation of Source config file.");

                int exitCode;
                // Validate source config file.
                if (!ValidationHelper.ValidateSoureConfigFileForCSPrime(sourceConfigFilePath, out exitCode))
                {
                    opResult.Error = StringResources.IncorrectSourceConfigFile;
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
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.AgentRegistrationFailed;
                    }

                    SummaryFileWrapper.RecordOperation(
                        Scenario.PreConfiguration,
                        OpName.SourceConfigFileValidation,
                        OpResult.Failed,
                        (int)opResult.ProcessExitCode);
                    return opResult;
                }
                else
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.PreConfiguration,
                        OpName.SourceConfigFileValidation,
                        OpResult.Success);
                }

                SetupHelper.UpdateDrScout(ConfigurationServerType.CSPrime);

                return opResult;
            }
            catch (OutOfMemoryException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "CRITICAL: Hit exception during prepare for CSPrime registration {0}",
                    ex);
                SummaryFileWrapper.RecordOperation(
                    Scenario.PreConfiguration,
                    OpName.PrerapeForRegistration,
                    OpResult.Failed,
                    (int)SetupHelper.UASetupReturnValues.AgentRegistrationFailed,
                    StringResources.AgentRegistrationFailed);
                return new OperationResult
                {
                    Error = StringResources.AgentRegistrationFailed,
                    ProcessExitCode = SetupHelper.UASetupReturnValues.AgentRegistrationFailed,
                    Status = OperationStatus.Failed
                };
            }
        }

        /// <summary>
        /// Performs registration
        /// </summary>
        /// <returns>Operation result.</returns>
        internal static OperationResult RegisterWithCS()
        {
            try
            {
                var opResult = new OperationResult
                {
                    Status = OperationStatus.Completed,
                    Error = string.Empty
                };

                // A. Shutdown CX services if needed.
                bool isCXInstalled = SetupHelper.IsCXInstalled();

                if (isCXInstalled)
                {
                    SetupHelper.StopCXServices();
                }

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

                // B. Shutdown InMage agent services.
                if (!SetupHelper.ShutdownInMageAgentServices())
                {
                    opResult.Error = StringResources.FailedToStopServices;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.UnableToStopServices;
                    opResult.Status = OperationStatus.Failed;
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Configuration,
                        OpName.StopServices,
                        OpResult.Failed,
                        (int)opResult.ProcessExitCode,
                        opResult.Error);
                    return opResult;
                }
                else
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Configuration,
                        OpName.StopServices,
                        OpResult.Success);
                }

                UpdateDrScoutConfigForCS();

                // D. Update CXPS config with CS details.
                UpdateCXPSConfigForCS();

                // E. Update registry
                UpdateRegistryForPSConfiguration();

                // F. Boot Services.
                if (isCXInstalled)
                {
                    SetupHelper.StartCXServices();
                }

                // G. Register agent for CSLegacy/CSPrime
                string installationDir = SetupHelper.GetAgentInstalledLocation();
                IniFile drScoutInfo = new IniFile(Path.Combine(
                    installationDir,
                    UnifiedSetupConstants.DrScoutConfigRelativePath));

                AgentInstallRole role = SetupHelper.GetAgentInstalledRole();
                if(role == AgentInstallRole.MS &&
                    String.Equals(drScoutInfo.ReadValue("vxagent", PropertyBagConstants.CSType),
                    ConfigurationServerType.CSPrime.ToString(),
                    StringComparison.OrdinalIgnoreCase))
                {
                    //Register agent for CSPrime
                    int exitCode;
                    string installationPath = SetupHelper.GetAgentInstalledLocation();
                    string rcmCliPath = Path.Combine(
                        installationPath,
                        UnifiedSetupConstants.AzureRCMCliExeName);

                    if (!RegisterMachine(rcmCliPath, out exitCode))
                    {
                        if (exitCode == (int)SetupHelper.UASetupReturnValues.RcmProxyRegistrationFailureExitCode)
                        {
                            Trc.Log(LogLevel.Always, "Register machine exit code is " + (int)SetupHelper.UASetupReturnValues.RcmProxyRegistrationFailureExitCode
                                + ". Saving the register machine state in drscout.conf");
                            PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.CloudPairingStatus, false);
                            drScoutInfo.WriteValue("vxagent", PropertyBagConstants.CloudPairingStatus, "Incomplete");
                        }
                        else
                        {
                            CleanHostnameAndExternalIp();
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

                    }

                    // Register sourceagent will work only if registermachine is successful
                    if (exitCode == 0)
                    {
                        if (!RegisterSourceAgent(rcmCliPath, out exitCode))
                        {
                            CleanHostnameAndExternalIp();
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
                    }

                    bool exitConfigurator;
                    if (!GetNonCachedSettings(rcmCliPath, out exitCode))
                    {
                        if (exitCode == 1)
                        {
                            opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.SucceededWithWarnings;
                            opResult.Status = OperationStatus.Warning;
                            exitConfigurator = false;
                        }
                        else if (exitCode == 2)
                        {
                            opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors;
                            opResult.Error = StringResources.FailedToRegisterWithRCM;
                            opResult.Status = OperationStatus.Failed;
                            exitConfigurator = true;
                        }
                        else if (exitCode == 148)
                        {
                            opResult.ProcessExitCode = (SetupHelper.UASetupReturnValues)exitCode;
                            opResult.Error = StringResources.FailedToRegisterWithRCM;
                            opResult.Status = OperationStatus.Failed;
                            exitConfigurator = true;
                        }
                        else
                        {
                            opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.RCMRegistrationFailure;
                            opResult.Error = StringResources.FailedToRegisterWithRCM;
                            opResult.Status = OperationStatus.Failed;
                            exitConfigurator = true;
                        }

                        if(exitConfigurator)
                        {
                            return opResult;
                        }

                    }
                }
                else
                {
                    //Register agent for CSLegacy
                    if (!SetupHelper.RegisterAgent())
                    {
                        CleanHostnameAndExternalIp();
                        opResult.Error = StringResources.FailedToRegisterCDPCLI;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.AgentRegistrationFailed;
                        opResult.Status = OperationStatus.Failed;
                        SummaryFileWrapper.RecordOperation(
                            Scenario.Configuration,
                            OpName.RegisterAgent,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode,
                            opResult.Error);
                        return opResult;
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.Configuration,
                            OpName.RegisterAgent,
                            OpResult.Success);
                    }
                }

                string agentInstallDir = (string)Registry.GetValue(
                        UnifiedSetupConstants.AgentRegistryName,
                        UnifiedSetupConstants.InMageInstallPathReg,
                        string.Empty);
                Platform platform = PropertyBagDictionary.Instance.GetProperty<Platform>(
                    PropertyBagConstants.Platform);

                // Create symbolic links.
                // No-op for A2A
                if (!SetupHelper.SetupSymbolicPaths(agentInstallDir, platform, OperationsPerformed.Configurator))
                {
                    opResult.Error = StringResources.FailedToCreateSymlinks;
                    opResult.Status = OperationStatus.Failed;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.FailedToCreateSymlinks;
                    SummaryFileWrapper.RecordOperation(
                        Scenario.PostInstallation,
                        OpName.CreateSymLinks,
                        OpResult.Failed,
                        (int)opResult.ProcessExitCode,
                        opResult.Error);
                    return opResult;
                }
                else
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.PostInstallation,
                        OpName.CreateSymLinks,
                        OpResult.Success);
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

                // H. Start services.
                if (!ConfigureActionProcessor.BootServices())
                {
                    opResult.Error = StringResources.FailedToStartServices;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.UnableToStartServices;
                    opResult.Status = OperationStatus.Failed;
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Configuration,
                        OpName.BootServices,
                        OpResult.Failed,
                        (int)opResult.ProcessExitCode);
                    return opResult;
                }
                else
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Configuration,
                        OpName.BootServices,
                        OpResult.Success);
                }

                if(PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.CloudPairingStatus)
                    && PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.CloudPairingStatus) == false)
                {
                    opResult.Error = StringResources.CloudPairingIncomplete;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.Successful;
                    opResult.Status = OperationStatus.Warning;
                    return opResult;
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
                    "CRITICAL: Hit exception during CS registration {0}",
                    ex);
                SummaryFileWrapper.RecordOperation(
                    Scenario.Configuration,
                    OpName.Registration,
                    OpResult.Failed,
                    (int)SetupHelper.UASetupReturnValues.AgentRegistrationFailed,
                    StringResources.AgentRegistrationFailed,
                    null,
                    null,
                    ex.ToString(),
                    null);
                return new OperationResult
                {
                    Error = StringResources.AgentRegistrationFailed,
                    ProcessExitCode = SetupHelper.UASetupReturnValues.AgentRegistrationFailed,
                    Status = OperationStatus.Failed
                };
            }
        }

        /// <summary>
        /// Validates registration was successful.
        /// </summary>
        /// <returns>Operation result.</returns>
        internal static OperationResult ValidateCSRegistration()
        {
            try
            {
                var opResult = new OperationResult();

                if (ConfigureActionProcessor.RebootRequired())
                {
                    RegistrationOperations.UpdateConfigurationStatus(UnifiedSetupConstants.SuccessStatus);

                    opResult.Status = OperationStatus.Warning;
                    opResult.Error = StringResources.PostInstallationRebootRequired;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.SuccessfulRecommendedReboot;
                    SummaryFileWrapper.RecordOperation(
                        Scenario.PostConfiguration,
                        OpName.RebootRequired,
                        OpResult.Warn,
                        (int)opResult.ProcessExitCode,
                        opResult.Error);
                    return opResult;
                }

                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.InstallationFinished,
                    true);

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
                    "CRITICAL: Hit exception during Validate CS registration {0}",
                    ex);
                SummaryFileWrapper.RecordOperation(
                    Scenario.PostConfiguration,
                    OpName.ValidateRegistration,
                    OpResult.Failed,
                    (int)SetupHelper.UASetupReturnValues.AgentRegistrationFailed,
                    StringResources.AgentRegistrationFailed,
                    null,
                    null,
                    ex.ToString(),
                    null);
                return new OperationResult
                {
                    Error = StringResources.AgentRegistrationFailed,
                    ProcessExitCode = SetupHelper.UASetupReturnValues.AgentRegistrationFailed,
                    Status = OperationStatus.Failed
                };
            }
        }

        #endregion internal methods

        #region private methods

        /// <summary>
        /// Updates drscout with hostid and
        /// Rcminfo file path
        /// </summary>
        private static void UpdateDrScout()
        {
            string installationDir = SetupHelper.GetAgentInstalledLocation();
            IniFile drScoutInfo = new IniFile(Path.Combine(
                installationDir,
                UnifiedSetupConstants.DrScoutConfigRelativePath));

            // Updating hostId.
            drScoutInfo.WriteValue(
                "vxagent",
                "HostId",
                PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.HostId));
        }

        /// <summary>
        /// Updates drscout with hostid and
        /// Rcminfo file path
        /// </summary>
        private static void UpdateDrScoutForCSPrime()
        {
            string installationDir = SetupHelper.GetAgentInstalledLocation();
            IniFile drScoutInfo = new IniFile(Path.Combine(
                installationDir,
                UnifiedSetupConstants.DrScoutConfigRelativePath));

            string appData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
            string rcmInfoPath = Path.Combine(appData, UnifiedSetupConstants.RcmConfRelativePath);
            IniFile rcmConf = new IniFile(rcmInfoPath);

            int credentialLessDiscoveryValue = (PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CredLessDiscovery).Equals(PropertyBagConstants.TrueValue, StringComparison.OrdinalIgnoreCase)) ? 1 : 0;

            // Updating hostId.
            drScoutInfo.WriteValue(
            "vxagent",
            "HostId",
            PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.HostId));

            drScoutInfo.WriteValue(
                "vxagent",
                "RcmSettingsPath",
                rcmInfoPath);

            drScoutInfo.WriteValue(
                "vxagent",
                "IsCredentialLessDiscovery",
                credentialLessDiscoveryValue.ToString());
        }

        /// <summary>
        /// Updates DrScout config with CS information.
        /// </summary>
        private static void UpdateDrScoutConfigForCS()
        {
            Trc.Log(
                LogLevel.Always,
                "Updating DrScout config for CS");

            string installationDir = SetupHelper.GetAgentInstalledLocation();
            IniFile drScoutInfo = new IniFile(Path.Combine(
                installationDir,
                UnifiedSetupConstants.DrScoutConfigRelativePath));
            string appData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
            string rcmInfoPath = Path.Combine(appData, UnifiedSetupConstants.RcmConfRelativePath);

            // Updating hostId.
            if (PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSType).Equals(ConfigurationServerType.CSLegacy.ToString(), StringComparison.OrdinalIgnoreCase))
            {
                drScoutInfo.WriteValue(
                    "vxagent.transport",
                    "Hostname",
                    PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSEndpoint));

                drScoutInfo.WriteValue(
                "vxagent.transport",
                "Port",
                PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSPort));

                if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.ExternalIP))
                {
                    string externalIP = PropertyBagDictionary.Instance.GetProperty<string>(
                        PropertyBagConstants.ExternalIP);
                    Trc.Log(LogLevel.Always, "Adding externalIP : {0} to drscout config.", externalIP);

                    if (!string.IsNullOrEmpty(externalIP))
                    {
                        drScoutInfo.WriteValue(
                            "vxagent",
                            "ExternalIpAddress",
                            externalIP);
                    }
                }
            }

            drScoutInfo.WriteValue(
                "vxagent.transport",
                "Https",
                "1");
        }

        /// <summary>
        /// Update RcmInfo.conf with bios id and clientrequestid
        /// </summary>
        private static void UpdateRcmConf()
        {
            string appData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
            string rcmInfoPath = Path.Combine(appData, UnifiedSetupConstants.RcmConfRelativePath);
            IniFile rcmConf = new IniFile(rcmInfoPath);

            rcmConf.WriteValue(
                    "rcm",
                    "BiosId",
                    PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.BiosId));
            if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.ClientRequestId))
            {
                rcmConf.WriteValue(
                    "rcm",
                    "ClientRequestId",
                    PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ClientRequestId));
            }
        }

        /// <summary>
        /// Updates CXPS config for CS.
        /// </summary>
        /// <param name="installationDir">Installation directory.</param>
        /// <param name="setupAction">Setup action.</param>
        /// <param name="role">Installed agent role.</param>
        private static void UpdateCXPSConfigForCS()
        {
            Trc.Log(
                LogLevel.Always,
                "Updating CXPS config for CS");

            bool isCXInstalled = SetupHelper.IsCXInstalled();
            AgentInstallRole role = SetupHelper.GetAgentInstalledRole();
            string installationDir = SetupHelper.GetAgentInstalledLocation();
            IniFile cxpsConfig = new IniFile(Path.Combine(installationDir, UnifiedSetupConstants.CXPSConfigRelativePath));

            if (role == AgentInstallRole.MT)
            {
                if (!isCXInstalled)
                {
                    cxpsConfig.WriteValue(
                        "cxps",
                        "id",
                        PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.HostId));
                }

                cxpsConfig.WriteValue(
                    "cxps",
                    "cs_ip_address",
                    PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSEndpoint));

                cxpsConfig.WriteValue(
                    "cxps",
                    "cs_ssl_port",
                    PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSPort));
            }
        }

        /// <summary>
        /// Updates registry for CS.
        /// </summary>
        private static void UpdateRegistryForPSConfiguration()
        {
            Trc.Log(
               LogLevel.Always,
               "Updating Registry for CS configuration");

            RegistryKey hklm = Registry.LocalMachine;

            using (RegistryKey inMageSvSys = hklm.OpenSubKey(UnifiedSetupConstants.SvSystemsRegistryHive, true))
            {
                if (PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSType).Equals(ConfigurationServerType.CSLegacy.ToString(), StringComparison.OrdinalIgnoreCase))
                {
                    inMageSvSys.SetValue(
                        "ServerName",
                        PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSEndpoint),
                        RegistryValueKind.String);

                    inMageSvSys.SetValue(
                        "ServerHttpPort",
                        PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSPort),
                        RegistryValueKind.DWord);
                }

                inMageSvSys.SetValue(
                    "HostId",
                    PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.HostId),
                    RegistryValueKind.String);
            }
        }

        /// <summary>
        /// Validation for agent pointing to Configuration server.
        /// </summary>
        /// <param name="csIP">User provided Configuration server IP</param>
        /// <returns>true if latest version of the agent present, false otherwise</returns>
        private static bool? IsAgentPointedToSameCS(string csIP)
        {
            string agentInstallDir = (string)Registry.GetValue(
                UnifiedSetupConstants.AgentRegistryName,
                UnifiedSetupConstants.InstDirRegKeyName,
                string.Empty);
            Trc.Log(LogLevel.Always, "Agent installation directory: {0}", agentInstallDir);

            string DRScoutcsIP = GrepUtils.GetKeyValueFromFile(
                Path.Combine(agentInstallDir, UnifiedSetupConstants.DrScoutConfigRelativePath),
                "Hostname");
            string csType = GrepUtils.GetKeyValueFromFile(
                Path.Combine(agentInstallDir, UnifiedSetupConstants.DrScoutConfigRelativePath),
                "CSType");

            Trc.Log(LogLevel.Always, "Configuration server IP in drscout.conf: {0}", DRScoutcsIP);

            if (string.IsNullOrEmpty(DRScoutcsIP) || DRScoutcsIP == "0.0.0.0")
            {
                Trc.Log(LogLevel.Always, "Agent is not pointing to any CS.");
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.AgentConfigured, false);
                return null;
            }
            else if (csIP == DRScoutcsIP)
            {
                Trc.Log(LogLevel.Always, "Agent is pointing to same CS.");
                return true;
            }
            else if(csType == "CSLegacy" &&
                PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSType).Equals(ConfigurationServerType.CSPrime.ToString(),StringComparison.OrdinalIgnoreCase))
            {
                Trc.Log(LogLevel.Always, "Agent upgrade from cs to csprime");
                return true;
            }
            else
            {
                Trc.Log(LogLevel.Always, "Agent is pointing to differnt CS.");
                return false;
            }
        }

        /// <summary>
        /// Validation for agent pointing to Configuration server.
        /// </summary>
        /// <returns>true if agent pointing to Configuration Server, false otherwise</returns>
        private static bool IsAgentPointedToCS()
        {
            bool isAgentPointedToCS = false;
            string agentInstallDir = (string)Registry.GetValue(
                UnifiedSetupConstants.AgentRegistryName,
                UnifiedSetupConstants.InstDirRegKeyName,
                string.Empty);
            Trc.Log(LogLevel.Always, "Agent installation directory: {0}", agentInstallDir);

            string DRScoutcsIP = GrepUtils.GetKeyValueFromFile(
                Path.Combine(agentInstallDir, UnifiedSetupConstants.DrScoutConfigRelativePath),
                "Hostname");
            string csType = GrepUtils.GetKeyValueFromFile(
                Path.Combine(agentInstallDir, UnifiedSetupConstants.DrScoutConfigRelativePath),
                "CSType");

            Trc.Log(LogLevel.Always, "Configuration server IP in drscout.conf: {0}", DRScoutcsIP);
            Trc.Log(LogLevel.Always, "CSType in drscout.conf: {0}", csType);

            if (csType == "CSLegacy")
            {
                if (string.IsNullOrEmpty(DRScoutcsIP) || DRScoutcsIP == "0.0.0.0")
                {
                    Trc.Log(LogLevel.Always, "Agent is not pointing to any Configuration Server.");
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Agent is already pointing to Configuration Server - {0}.", DRScoutcsIP);
                    isAgentPointedToCS = true;
                }
            }
            else
            {
                Trc.Log(LogLevel.Always, "Agent is not pointing to any Configuration Server.");
            }

            return isAgentPointedToCS;
        }

        private static void CleanHostnameAndExternalIp()
        {
            IniFile drScoutInfo = new IniFile(Path.Combine(
            SetupHelper.GetAgentInstalledLocation(),
            UnifiedSetupConstants.DrScoutConfigRelativePath));
            drScoutInfo.WriteValue(
            "vxagent.transport",
            "Hostname",
            "");
            drScoutInfo.RemoveKey(
            "vxagent",
            "ExternalIpAddress");
        }

        // <summary>
        /// Get non cached settings.
        /// </summary>
        /// <param name="rcmCliPath">Path of RcmCli.exe</param>
        /// <out="exitCode">exitCode from RcmCli</out>
        /// <returns>true if non cached settings are fetched, false otherwise</returns>
        private static bool GetNonCachedSettings(string rcmCliPath, out int exitCode)
        {
            bool isNonCachedSettingsFetch = false;
            exitCode = -1;

            string parameters = "--getnoncachedsettings " + UnifiedSetupConstants.AzureRcmCliErrorFileName +
                $" \"{PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath)}\"";
            exitCode = CommandExecutor.RunCommand(rcmCliPath, parameters);
            Trc.Log(LogLevel.Always, "Get non cached settings exit code: {0}", exitCode);
            if (exitCode == 0)
            {
                Trc.Log(LogLevel.Always, "Fetched non cached settings successfully.");
                isNonCachedSettingsFetch = true;
            }

            return isNonCachedSettingsFetch;
        }

        # endregion private methods
    }
}
