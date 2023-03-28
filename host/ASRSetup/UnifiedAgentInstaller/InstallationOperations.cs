using ASRResources;
using ASRSetupFramework;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using Newtonsoft.Json;

namespace UnifiedAgentInstaller
{
    /// <summary>
    /// Installation oprerations.
    /// </summary>
    public class InstallationOperations
    {

        #region PrepareForInstall

        /// <summary>
        /// Prepares for Installation or upgrade. Evaluate all pre-conditions and
        /// shut down running services.
        /// </summary>
        /// <returns>Final operation status.</returns>
        internal static OperationResult PrepareForInstall()
        {
            try
            {
                Trc.Log(LogLevel.Always, "Begin PrepareForInstall.");

                string installationLocation = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.InstallationLocation);
                AgentInstallRole role = PropertyBagDictionary.Instance.GetProperty<AgentInstallRole>(
                    PropertyBagConstants.InstallRole);
                SetupAction setupAction = PropertyBagDictionary.Instance.GetProperty<SetupAction>(
                    PropertyBagConstants.SetupAction);
                Platform platform = PropertyBagDictionary.Instance.GetProperty<Platform>(
                    PropertyBagConstants.Platform);
                string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);

                Trc.Log(LogLevel.Always, "installationLocation: {0}", installationLocation);
                Trc.Log(LogLevel.Always, "role: {0}", role);
                Trc.Log(LogLevel.Always, "setupAction: {0}", setupAction);
                Trc.Log(LogLevel.Always, "platform: {0}", platform);
                Trc.Log(LogLevel.Always, "sysDrive: {0}", sysDrive);               
                
                var opResult = new OperationResult();

                if (role == AgentInstallRole.MS)
                {
                    if (InstallActionProcessor.IsASRServerSetup())
                    {
                        opResult.Error = StringResources.AgentUpgradeOnASRServer;
                        opResult.Status = OperationStatus.Failed;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.BlockMSInstallationOnASRServerSetup;

                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.IsASRServerSetupInstalled,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode,
                            opResult.Error);

                        return opResult;
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.IsASRServerSetupInstalled,
                            OpResult.Success);
                    }
                }

                if (role == AgentInstallRole.MS)
                {
                    if (!SetupHelper.IsAzureVirtualMachine())
                    {
                        if (platform != Platform.VmWare)
                        {
                            Trc.Log(LogLevel.Always, "Blocking platform change from VmWare to Azure.");
                            opResult.Status = OperationStatus.Failed;
                            opResult.Error = StringResources.PlatformChange;
                            opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors;

                            ErrorLogger.LogInstallerError(
                                new InstallerException(
                                    UASetupErrorCode.ASRMobilityServiceInstallerPlatformMismatch.ToString(),
                                    null,

                                    string.Format(UASetupErrorCodeMessage.PlatformChangeFailure, platform.ToString())
                                    )
                                );

                            SummaryFileWrapper.RecordOperation(
                                Scenario.PreInstallation,
                                OpName.PlatformValidation,
                                OpResult.Failed,
                                (int)opResult.ProcessExitCode,
                                opResult.Error);

                            return opResult;
                        }
                        else
                        {
                            SummaryFileWrapper.RecordOperation(
                                Scenario.PreInstallation,
                                OpName.PlatformValidation,
                                OpResult.Success);
                        }
                    }
                }

                // Space check validation (performs for both fresh/upgrade)
                if (!PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagDictionary.SkipPrereqChecks))
                {
                    if (!ValidationHelper.ValidateInstallLocationUA(installationLocation))
                    {
                        opResult.Error = StringResources.InstallLocationErrorTextUASilent;
                        opResult.Status = OperationStatus.Failed;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.InvalidInstallLocation;

                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.InstallLocationValidation,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode,
                            opResult.Error);

                        return opResult;
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.InstallLocationValidation,
                            OpResult.Success);
                    }
                }

                if (setupAction == SetupAction.Upgrade)
                {
                    AgentInstallRole installedRole = SetupHelper.GetAgentInstalledRole();

                    if (installedRole != role)
                    {
                        opResult.Status = OperationStatus.Failed;
                        opResult.Error = StringResources.AgentRoleMismatch;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.AgentRoleChangeNotAllowed;

                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.RoleValidation,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode,
                            opResult.Error);

                        return opResult;
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.RoleValidation,
                            OpResult.Success);
                    }

                    Platform installedPlatform;
                    string vmPlatform = SetupHelper.GetAgentInstalledPlatformFromDrScout();
                                     
                    installedPlatform = (string.IsNullOrEmpty(vmPlatform)) ? 
                        SetupHelper.GetAgentInstalledPlatform() :
                        vmPlatform.AsEnum<Platform>().Value;
                    Trc.Log(LogLevel.Always,
                        "installedPlatform - {0}",
                        installedPlatform);

                    if (installedPlatform != platform)
                    {
                        if (installedPlatform == Platform.VmWare && platform == Platform.Azure)
                        {
                            Trc.Log(LogLevel.Always, "Allowing platform change from VmWare to Azure.");
                            PropertyBagDictionary.Instance.SafeAdd(PropertyBagDictionary.PlatformChange, true);
                            SummaryFileWrapper.RecordOperation(
                                Scenario.PreInstallation,
                                OpName.PlatformValidation,
                                OpResult.Success);
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "Blocking platform change from Azure to VmWare.");
                            opResult.Status = OperationStatus.Failed;
                            opResult.Error = StringResources.PlatformMismatch;
                            opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.PlatformChangeNotAllowed;

                            SummaryFileWrapper.RecordOperation(
                                Scenario.PreInstallation,
                                OpName.PlatformValidation,
                                OpResult.Failed,
                                (int)opResult.ProcessExitCode,
                                opResult.Error);

                            return opResult;
                        }
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.PlatformValidation,
                            OpResult.Success);
                    }
                }

                if (role == AgentInstallRole.MS)
                {
                    // Validate pre-requisites
                    string consoleOutput = String.Empty, msvOutputJsonPath, msvSummaryJsonPath, msvOutputLogPath = string.Empty;
                    int returnCode = InstallActionProcessor.ValidatePreRequisites(platform, out msvOutputJsonPath, out msvSummaryJsonPath, out msvOutputLogPath, out consoleOutput);
                    Trc.Log(LogLevel.Debug, "Mobility service validator console output is");
                    Trc.Log(LogLevel.Debug, "---------------------------------------Start-----------------------------------------------");
                    Trc.Log(LogLevel.Debug, consoleOutput);
                    Trc.Log(LogLevel.Debug, "----------------------------------------End------------------------------------------------");
                    //Appending mobility service validator logs to installer logs
                    if (!msvOutputLogPath.Equals(String.Empty, StringComparison.OrdinalIgnoreCase))
                    {
                        try
                        {
                            if (File.Exists(msvOutputLogPath))
                            {
                                string msvLogs = File.ReadAllText(msvOutputLogPath);
                                Trc.Log(LogLevel.Debug, "Mobility service validator output starts here");
                                Trc.Log(LogLevel.Debug, "----------------------------------------------");
                                Trc.Log(LogLevel.Debug, msvLogs);
                                Trc.Log(LogLevel.Debug, "Mobility service validator output ends here");
                                Trc.Log(LogLevel.Debug, "----------------------------------------------");
                            }
                            else
                            {
                                Trc.Log(LogLevel.Debug, "The file " + msvOutputLogPath + " does not exist");
                            }
                        }
                        catch(OutOfMemoryException)
                        {
                            throw;
                        }
                        catch (Exception ex)
                        {
                            Trc.Log(LogLevel.Warn, "Failed to read file " + msvOutputLogPath +
                                " with following exception " + ex);
                        }

                    }
                    if (returnCode == 0)
                    {
                        // Read MobilityServiceValidator json file entries and write them into installer summary json file.
                        WriteEntriesIntoInstallerSummaryJsonFile(msvSummaryJsonPath);

                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.PrechecksResult, UnifiedSetupConstants.InstallerPrechecksResult.Success);
                        Trc.Log(LogLevel.Debug, "Prechecks succeeded");

                        SummaryFileWrapper.RecordOperation(
                            Scenario.ProductPreReqChecks,
                            OpName.ProductPrereqsValidation,
                            OpResult.Success);
                    }
                    else if (returnCode == 1)
                    {
                        // Read MobilityServiceValidator json file entries and write them into installer summary json file.
                        WriteEntriesIntoInstallerSummaryJsonFile(msvSummaryJsonPath);

                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.PrechecksResult, UnifiedSetupConstants.InstallerPrechecksResult.Failure);
                        Trc.Log(LogLevel.Debug, "Prechecks failed");

                        opResult.Error = String.Format(
                            StringResources.MobilityServicePrereqsValidationError,
                            msvOutputJsonPath);
                        opResult.Status = OperationStatus.Failed;
                        opResult.ProcessExitCode =
                            SetupHelper.UASetupReturnValues.ProductPrereqsValidationFailed;

                        SummaryFileWrapper.RecordOperation(
                            Scenario.ProductPreReqChecks,
                            OpName.ProductPrereqsValidation,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode,
                            opResult.Error);

                        return opResult;
                    }
                    else if (returnCode == 2)
                    {
                        // Read MobilityServiceValidator json file entries and write them into installer summary json file.
                        WriteEntriesIntoInstallerSummaryJsonFile(msvSummaryJsonPath);

                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.PrechecksResult, UnifiedSetupConstants.InstallerPrechecksResult.Warning);
                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.InstallationStatus, UnifiedSetupConstants.InstallationStatus.Warning);
                        Trc.Log(LogLevel.Debug, "Prechecks succeeded with warnings");

                        SummaryFileWrapper.RecordOperation(
                            Scenario.ProductPreReqChecks,
                            OpName.ProductPrereqsValidation,
                            OpResult.Warn
                            );
                    }
                    else if (returnCode == 3)
                    {
                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.PrechecksResult, UnifiedSetupConstants.InstallerPrechecksResult.Failure);
                        Trc.Log(LogLevel.Debug, "Prechecks failed due to out of memory exception");

                        opResult.Error = StringResources.MobilityServicePrereqsValidationError;
                        opResult.Status = OperationStatus.Failed;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.ProductPrereqsValidationFailed;

                        SummaryFileWrapper.RecordOperation(
                            Scenario.ProductPreReqChecks,
                            OpName.ProductPrereqsValidation,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode,
                            opResult.Error);

                        ErrorLogger.LogInstallerError(
                                new InstallerException(
                                    UASetupErrorCode.ASRMobilityServiceInstallerPrecheckInternalError.ToString(),
                                    null,
                                    UASetupErrorCodeMessage.PrecheckCrash
                                    )
                                );

                        return opResult;
                    }
                    else
                    {
                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.PrechecksResult, UnifiedSetupConstants.InstallerPrechecksResult.Failure);
                        Trc.Log(LogLevel.Debug, "Prechecks failed due to internal error");

                        opResult.Error = StringResources.AgentInternalError;
                        opResult.Status = OperationStatus.Failed;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.ProductPrereqsValidationFailed;

                        SummaryFileWrapper.RecordOperation(
                            Scenario.ProductPreReqChecks,
                            OpName.ProductPrereqsValidation,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode,
                            opResult.Error);

                        return opResult;
                    }
                }

                if ((setupAction == SetupAction.Install) &&
                    !PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagDictionary.SkipPrereqChecks))
                {
                    // Don't allow interactive installation on Azure VMs.
                    if (platform == Platform.Azure &&
                        !PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagDictionary.Silent))
                    {
                        opResult.Error = StringResources.InteractiveInstallNotAllowedOnAzureVMs;
                        opResult.Status = OperationStatus.Failed;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.Failed;

                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.InteractiveInstallationOnAzureVMs,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode,
                            opResult.Error);
                        return opResult;
                    }

                    // Checks whether install location characters are within the ASCII characters range(32 to 126) or not.        
                    if (!ValidationHelper.IsAsciiCheck(installationLocation))
                    {
                        opResult.Error = StringResources.InstallLocationAsciiErrorTextUA;
                        opResult.Status = OperationStatus.Failed;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.InvalidInstallLocation;

                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.InstallLocationValidation,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode,
                            opResult.Error);

                        return opResult;
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.InstallLocationValidation,
                            OpResult.Success);
                    }

                    // Check validity of OS.
                    OSType operationSystem = InstallActionProcessor.CheckOSValidity(role);

                    if (operationSystem == OSType.Unsupported)
                    {
                        if (role == AgentInstallRole.MS)
                        {
                            opResult.Error = StringResources.UnsupportedOSMobilityService;
                        }
                        else
                        {
                            opResult.Error = StringResources.UnsupportedOSMasterTarget;
                        }

                        opResult.Status = OperationStatus.Failed;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.UnsupportedOS;

                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.OSValidation,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode,
                            opResult.Error);

                        return opResult;
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.OSValidation,
                            OpResult.Success);
                    }

                    // Check VMware Tools status when role is Master Target and platform is VMware.
                    if ((!ValidationHelper.PSExists()) && (platform == Platform.VmWare) && (role == AgentInstallRole.MT))
                    {
                        if (!(ValidationHelper.isVMwareToolsExists()))
                        {
                            opResult.Error = StringResources.EnvironmentDetailsPageVMwareToolsNotExistsText;
                            opResult.Status = OperationStatus.Failed;
                            opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.VMwareToolsAreNotAvailable;

                            SummaryFileWrapper.RecordOperation(
                                Scenario.PreInstallation,
                                OpName.VMwareToolsValidation,
                                OpResult.Failed,
                                (int)opResult.ProcessExitCode,
                                opResult.Error);

                            return opResult;
                        }
                        else
                        {
                            SummaryFileWrapper.RecordOperation(
                                Scenario.PreInstallation,
                                OpName.VMwareToolsValidation,
                                OpResult.Success);
                        }
                    }

                    // Check for a pending reboot on machine.
                    if (!ValidationHelper.PSExists())
                    {
                        string error;
                        bool rebootNotNeeded = ValidationHelper.RebootNotRequired(
                            out error,
                            checkForPendingRenamesAndMU: false);
                        if (rebootNotNeeded)
                        {
                            Trc.Log(LogLevel.Always, "System restart is not detected on this server.");

                            SummaryFileWrapper.RecordOperation(
                                Scenario.PreInstallation,
                                OpName.SystemRestartPendingValidation,
                                OpResult.Success);
                        }
                        else
                        {
                            PropertyBagDictionary.Instance.SafeAdd(PropertyBagDictionary.RebootRequired, true);
                            Trc.Log(LogLevel.Always, "System restart is detected on this server.");
                            opResult.Error = error;
                            opResult.Status = OperationStatus.Failed;
                            opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.SystemRestartPending;

                            SummaryFileWrapper.RecordOperation(
                                Scenario.PreInstallation,
                                OpName.SystemRestartPendingValidation,
                                OpResult.Failed,
                                (int)opResult.ProcessExitCode,
                                opResult.Error);

                            return opResult;
                        }
                    }
                }

                if ((role == AgentInstallRole.MS) && (SetupAction.Upgrade == setupAction))
                {
                    bool changeStartupType = SetupHelper.ChangeAgentServicesStartupType(NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START);

                    SummaryFileWrapper.RecordOperation(
                        Scenario.PreInstallation,
                        OpName.ChangeAgentServicesStartupTypeToManual,
                        (changeStartupType)? OpResult.Success : OpResult.Failed,
                        (int) ((changeStartupType) ? SetupHelper.UASetupReturnValues.Successful : SetupHelper.UASetupReturnValues.SucceededWithWarnings)
                        );
                }

                // Shutdown InMage services.
                if (!SetupHelper.ShutdownInMageAgentServices())
                {
                    opResult.Error = StringResources.FailedToStopServices;
                    opResult.Status = OperationStatus.Failed;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.UnableToStopServices;

                    SummaryFileWrapper.RecordOperation(
                        Scenario.PreInstallation,
                        OpName.StopServices,
                        OpResult.Failed,
                        (int)opResult.ProcessExitCode);

                    return opResult;
                }
                else
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.PreInstallation,
                        OpName.StopServices,
                        OpResult.Success);
                }

                if (SetupHelper.IsCXInstalled())
                {
                    SetupHelper.StopCXServices();
                }
                
                // Backup Exisiting Configuration.
                if (setupAction == SetupAction.Upgrade)
                {
                    if (!SetupHelper.BackupDrscout())
                    {
                        opResult.Error = StringResources.FailedToBackupConfiguration;
                        opResult.Status = OperationStatus.Failed;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors;
                        ErrorLogger.LogInstallerError(
                            new InstallerException(
                                UASetupErrorCode.ASRMobilityServiceInstallerConfigFilesBackupFailed.ToString(),
                                new Dictionary<string, string>()
                                {
                                    {UASetupErrorCodeParams.ErrorCode, ((int)opResult.ProcessExitCode).ToString()}
                                },
                                UASetupErrorCodeMessage.ConfigFilesBackupFailure
                                )
                            );
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.BackupConfigurationFiles,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode);

                        return opResult;
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PreInstallation,
                            OpName.BackupConfigurationFiles,
                            OpResult.Success);
                    }

                    // Backup cxps config.
                    SetupHelper.AgentConfigCopy(
                        UnifiedSetupConstants.CXPSConfigRelativePath,
                        UnifiedSetupConstants.CXPSConfigBackupRelativePath);
                    
                }

                // Remove Filter driver entry from services.
                if (setupAction == SetupAction.Install)
                {
                    //SetupHelper.DeleteInDskFltRegistry();
                }

                if (PropertyBagDictionary.Instance.GetProperty<int>(PropertyBagConstants.PrechecksResult) == (int)UnifiedSetupConstants.InstallerPrechecksResult.Warning)
                {
                    opResult.Error = String.Format(
                                StringResources.MobilityServicePrereqsValidationError,
                                ErrorLogger.OutputJsonFileName);
                    opResult.Status = OperationStatus.Warning;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.SucceededWithWarnings;
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
                Trc.Log(LogLevel.Error, "CRITICALEXCEPTION hit during prepare for install: {0}", ex);
                SummaryFileWrapper.RecordOperation(
                    Scenario.PreInstallation,
                    OpName.PrepareForInstall,
                    OpResult.Failed,
                    (int)SetupHelper.UASetupReturnValues.FailedWithErrors,
                    StringResources.AgentInstallationGenericError,
                    null,
                    null,
                    ex.ToString(),
                    null);
                ErrorLogger.LogInstallerError(
                    new InstallerException(
                        UASetupErrorCode.ASRMobilityServiceInstallerPrepareForInstallStepsFailed.ToString(),
                        null,
                        UASetupErrorCodeMessage.PrepareForInstallStepsFailed
                        )
                    );
                return new OperationResult
                {
                    Error = StringResources.AgentInstallationGenericError,
                    ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors,
                    Status = OperationStatus.Failed
                };
            }
        }

        #endregion PrepareForInstall

        #region InstallComponents
        /// <summary>
        /// Installs components on machine.
        /// </summary>
        /// <returns>Final operation status.</returns>
        internal static OperationResult InstallComponents()
        {
            try
            {
                Trc.Log(LogLevel.Always, "Begin InstallComponents.");

                string installationLocation = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.InstallationLocation);
                AgentInstallRole role = PropertyBagDictionary.Instance.GetProperty<AgentInstallRole>(
                    PropertyBagConstants.InstallRole);
                SetupAction setupAction = PropertyBagDictionary.Instance.GetProperty<SetupAction>(
                    PropertyBagConstants.SetupAction);
                Platform platform = PropertyBagDictionary.Instance.GetProperty<Platform>(
                    PropertyBagConstants.Platform);

                Trc.Log(LogLevel.Always, "installationLocation: {0}", installationLocation);
                Trc.Log(LogLevel.Always, "role: {0}", role);
                Trc.Log(LogLevel.Always, "setupAction: {0}", setupAction);
                Trc.Log(LogLevel.Always, "platform: {0}", platform);

                var opResult = new OperationResult();

                // Install\Upgrade agent.
                bool result = false;
                int exitCode = 1;
                if (setupAction == SetupAction.Install)
                {
                    result = InstallActionProcessor.InstallAgent(role, installationLocation, platform, out exitCode);
                }
                else
                {
                    result = InstallActionProcessor.UpgradeAgentInstallation(role, installationLocation, platform, out exitCode);
                }

                Trc.Log(LogLevel.Always, "Installation Status - " + (result ? "Succeeded" : "Failed"));

                // See if a reboot is needed.
                if (result)
                {
                    if (PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagDictionary.RebootRequired))
                    {
                        Trc.Log(LogLevel.Warn, "You must restart the server for some system changes to take effect.");
                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.InstallationStatus, UnifiedSetupConstants.InstallationStatus.Warning);
                        opResult.Error = StringResources.PostInstallationRebootRequired;
                        opResult.Status = OperationStatus.Warning;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.SucceededWithWarnings;
                        SummaryFileWrapper.RecordOperation(
                            Scenario.Installation,
                            OpName.InstallAgent,
                            OpResult.Warn);
                        return opResult;
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.Installation,
                            OpName.InstallAgent,
                            OpResult.Success);
                        return opResult;
                    }
                }
                else
                {
                    Dictionary<string, string> errorParams = new Dictionary<string, string>();
                    if (PropertyBagDictionary.Instance.ContainsKey(PropertyBagConstants.ErrorMessage))
                    {
                        opResult.Error = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ErrorMessage);
                    }
                    else
                    {
                        opResult.Error = StringResources.FailedToInstallUnifiedAgent;
                    }

                    string msiErrorCode = UASetupErrorCode.ASRMobilityServiceInstallerMSIInstallationFailure.ToString();
                    try
                    {
                        if (Enum.IsDefined(typeof(UAMSIErrorCodeList), exitCode))
                        {
                            msiErrorCode = UAInstallerErrorPrefixConstants.ASRMobilityServiceInstallerMSIInstallationFailure + ((UAMSIErrorCodeList)exitCode).ToString();
                        }
                    }
                    catch(Exception ex)
                    {
                        Trc.Log(LogLevel.Error, "Failed to check for known msi errors with exception {0}. So taking generic failure code.", ex );
                        msiErrorCode = UASetupErrorCode.ASRMobilityServiceInstallerMSIInstallationFailure.ToString();
                    }

                    if(exitCode == (int)UAMSIErrorCodeList.InstallationAlreadyRunning)
                    {
                        Trc.Log(LogLevel.Always, "Process id for msiexec process is : {0}", String.Join(";", SetupHelper.GetProcessID("msiexec.exe").Select( x => x.ToString()).ToArray()));
                    }

                    if(exitCode == (int)UAMSIErrorCodeList.InternalError)
                    {
                        int msiExitCode = (int)InstallActionProcessor.CheckMsiSubErrors1603(errorParams);

                        if(msiExitCode != (int)MSIError1603.Default)
                        {
                            msiErrorCode += ((MSIError1603)msiExitCode).ToString();
                        }
                    }

                    if(exitCode == (int)UAMSIErrorCodeList.InstallerNotAccessible)
                    {
                        int msiExitCode = (int)InstallActionProcessor.CheckMsiSubErrors1601(errorParams);
                        if (msiExitCode != (int)MSIError1601.Default)
                        {
                            msiErrorCode += ((MSIError1601)msiExitCode).ToString();
                        }
                    }

                    errorParams.Add(UASetupErrorCodeParams.ErrorCode, exitCode.ToString());

                    ErrorLogger.LogInstallerError(
                        new InstallerException(
                            msiErrorCode,
                            errorParams,
                            String.Format(UASetupErrorCodeMessage.MSIInstallationFailure, exitCode.ToString())
                            )
                        );

                    opResult.Status = OperationStatus.Failed;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors;

                    SummaryFileWrapper.RecordOperation(
                            Scenario.Installation,
                            OpName.InstallAgent,
                            OpResult.Failed,
                            exitCode,
                            opResult.Error);

                    return opResult;
                }
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "CRITICALEXCEPTION hit during install: {0}", ex);
                SummaryFileWrapper.RecordOperation(
                    Scenario.Installation,
                    OpName.InstallAgent,
                    OpResult.Failed,
                    (int)SetupHelper.UASetupReturnValues.FailedWithErrors,
                    StringResources.AgentInstallationGenericError,
                    null,
                    null,
                    ex.ToString(),
                    null);
                ErrorLogger.LogInstallerError(
                    new InstallerException(
                        UASetupErrorCode.ASRMobilityServiceInstallerInstallingComponentsFailedWithInternalError.ToString(),
                        null,
                        UASetupErrorCodeMessage.InstallingComponentsFailedWithInternalError
                        )
                    );
                return new OperationResult
                {
                    Error = StringResources.AgentInstallationGenericError,
                    ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors,
                    Status = OperationStatus.Failed
                };
            }
        }

        #endregion InstallComponents

        #region PostInstallationSteps

        /// <summary>
        /// Performs post installation steps.
        /// </summary>
        /// <returns>Final operation status.</returns>
        internal static OperationResult PostInstallationSteps()
        {
            try
            {
                Trc.Log(LogLevel.Always, "Begin PostInstallationSteps.");

                string installationLocation = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.InstallationLocation);
                AgentInstallRole role = PropertyBagDictionary.Instance.GetProperty<AgentInstallRole>(
                    PropertyBagConstants.InstallRole);
                Platform platform = PropertyBagDictionary.Instance.GetProperty<Platform>(
                    PropertyBagConstants.Platform);
                SetupAction setupAction = PropertyBagDictionary.Instance.GetProperty<SetupAction>(
                    PropertyBagConstants.SetupAction);
                bool isPostInstallActionsSucceeded = 
                    ValidationHelper.IsAgentPostInstallActionSuccess();

                Trc.Log(LogLevel.Always, "installationLocation: {0}", installationLocation);
                Trc.Log(LogLevel.Always, "role: {0}", role);
                Trc.Log(LogLevel.Always, "platform: {0}", platform);
                Trc.Log(LogLevel.Always, "setupAction: {0}", setupAction);

                var opResult = new OperationResult();

                bool isCXInstalled = SetupHelper.IsCXInstalled();

                // Update installation registry.
                InstallActionProcessor.SetInstallationRegistries(setupAction);

                // svagents service Symlink will be recreated in 3 conditions
                // 1. It is fresh install from command line
                // 2. There is platform change from V2A to A2A or, A2A to V2A
                // 3. If current svagents is not symbolic link. It is needed to handle cases
                //          of earlier releases when ASR shipped svagents which was actually
                //          a real file.
                if ((setupAction == SetupAction.Install) || 
                    PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagDictionary.PlatformChange) ||
                PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.PlatformUpgradeFromCSToCSPrime) ||
                !SetupHelper.IsSvagentsSymbolicLink(installationLocation))
                {
                    // Create symbolic links.
                    if (!SetupHelper.SetupSymbolicPaths(installationLocation, platform, OperationsPerformed.Installer))
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
                }
                else
                {
                    Trc.Log(LogLevel.Info, "Skipped creating symlink");
                }

                // Create services.
                if (!SetupHelper.CreateAndConfigureServices(installationLocation))
                {
                    opResult.Error = StringResources.FailedToInstallAndConfigureServices;
                    opResult.Status = OperationStatus.Failed;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.FailedToInstallAndConfigureServices;
                    SummaryFileWrapper.RecordOperation(
                        Scenario.PostInstallation,
                        OpName.CreateAndConfigureServices,
                        OpResult.Failed,
                        (int)opResult.ProcessExitCode,
                        opResult.Error);
                    return opResult;
                }
                else
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.PostInstallation,
                        OpName.CreateAndConfigureServices,
                        OpResult.Success);
                }

                // Merge DrScout config and replace cxps config backup file.
                if (setupAction == SetupAction.Upgrade)
                {
                    if (!SetupHelper.MergeDrscout())
                    {
                        opResult.Error = StringResources.FailedToMergeConfiguration;
                        opResult.Status = OperationStatus.Failed;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors;
                        ErrorLogger.LogInstallerError(
                            new InstallerException(
                                UASetupErrorCode.ASRMobilityServiceInstallerConfigFilesMergeFailed.ToString(),
                                null,
                                UASetupErrorCodeMessage.ConfigFilesMergeFailure
                                )
                            );
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PostInstallation,
                            OpName.MergeDrscoutConfig,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode,
                            opResult.Error);
                        return opResult;
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PostInstallation,
                            OpName.MergeDrscoutConfig,
                            OpResult.Success);
                    }
                }

                // Set Admin privileges.
                SetupHelper.ConfigureAdmin(installationLocation);

                // Perform Scout tuning.
                if (role == AgentInstallRole.MT && setupAction == SetupAction.Install)
                {
                    SetupHelper.ScoutTuning(installationLocation);
                }

                // Generate Certificates.
                if (setupAction == SetupAction.Install && !isCXInstalled && platform != Platform.Azure)
                {
                    SetupHelper.GenerateCert(installationLocation);
                }

                // Perform Heap change.
                if (role == AgentInstallRole.MT && setupAction == SetupAction.Install)
                {
                    SetupHelper.HeapChange();
                }

                // Start service controller.
                if (setupAction == SetupAction.Install)
                {
                    SetupHelper.InstallServiceCtrl(installationLocation);
                }

                // Replace runtime paths.
                SetupHelper.ReplaceRuntimeInstallPath();

                int productType = PropertyBagDictionary.Instance.GetProperty<int>(PropertyBagConstants.GuestOSProductType);
                OSType osType = PropertyBagDictionary.Instance.GetProperty<OSType>(PropertyBagConstants.GuestOSType);
                Trc.Log(LogLevel.Always, "OsType value: {0}.", osType);

                //Reregister the VSS provider
                bool vssProviderReregistrationSucceeded = true;
                if ((setupAction == SetupAction.Install ||
                    !isPostInstallActionsSucceeded) && 
                    role == AgentInstallRole.MS && osType != OSType.Vista)
                {
                    if (ServiceControlFunctions.IsInstalled(UnifiedSetupConstants.ASRVSSServiceName))
                    {
                        string agentInstallationLocation = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.InstallationLocation);
                        Trc.Log(LogLevel.Debug, string.Format("Reregistering VSS Provider from location {0}", agentInstallationLocation));

                        int exitCode = CommandExecutor.RunCommand("regsvr32", "/s \"" + agentInstallationLocation + @"\InMageVssProvider.dll" + "\"");
                        if (exitCode != 0)
                        {
                            vssProviderReregistrationSucceeded = false;
                            Trc.Log(LogLevel.Error, string.Format("VSS provider reregistration failed from location {0} with exit code {1}", agentInstallationLocation, exitCode.ToString()));
                            PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.InstallationStatus, UnifiedSetupConstants.InstallationStatus.Warning);

                            ErrorLogger.LogInstallerError(
                                new InstallerException(
                                    UASetupErrorCode.ASRMobilityServiceInstallerVSSProviderRegistrationFailed.ToString(),
                                    new Dictionary<string, string> ()
                                    {
                                        {UASetupErrorCodeParams.ErrorCode , exitCode.ToString() }
                                    },
                                     string.Format(UASetupErrorCodeMessage.VSSProviderRegistrationFailed,exitCode.ToString())
                                    )
                                );
                        }
                        else
                        {
                            Trc.Log(LogLevel.Debug, string.Format("VSS Provider reregistration succeeded from location {0}", agentInstallationLocation));
                        }
                    }
                    else
                    {
                        Trc.Log(LogLevel.Debug, "Azure Site Recovery VSS Provider service is not installed. So skipping provider reregistration");
                    }
                }
                

                // Create AppConf.
                if (!isCXInstalled && 
                    (setupAction == SetupAction.Install ||
                    !isPostInstallActionsSucceeded))
                {
                    if (!SetupHelper.CreateAPPConf())
                    {
                        opResult.Error = StringResources.FailedToCreateAppConf;
                        opResult.Status = OperationStatus.Failed;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors;
                        ErrorLogger.LogInstallerError(
                            new InstallerException(
                                UASetupErrorCode.ASRMobilityServiceInstallerConfigFilesCreationFailed.ToString(),
                                null,
                                UASetupErrorCodeMessage.ConfigFilesCreationFailure
                                )
                            );
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PostInstallation,
                            OpName.CreateAPPConfig,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode);
                        return opResult;
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PostInstallation,
                            OpName.CreateAPPConfig,
                            OpResult.Success);
                    }
                }

                // Start CX services.
                if (isCXInstalled)
                {
                    SetupHelper.StartCXServices();
                }

                // Set Frsvc to run on demand.
                SetupHelper.FrSvcSetOnDemand();
               
                // Set ACL on required folders.
                if (setupAction == SetupAction.Install ||
                    !isPostInstallActionsSucceeded)
                {
                    string permissionFiles;
                    string errorCode = String.Empty;
                    if (!SetupHelper.PermissionChanges(out permissionFiles, out errorCode))
                    {
                        opResult.Error = StringResources.FailedToSetPermissionOnDirectories;
                        opResult.Status = OperationStatus.Failed;
                        opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors;
                        ErrorLogger.LogInstallerError(
                            new InstallerException(
                                UASetupErrorCode.ASRMobilityServiceInstallerFailedToSetPermissions.ToString(),
                                new Dictionary<string, string>()
                                {
                                    {UASetupErrorCodeParams.PermissionsFiles, permissionFiles},
                                    {UASetupErrorCodeParams.ErrorCode, errorCode}
                                },
                                string.Format(UASetupErrorCodeMessage.FailedToSetPermissions, permissionFiles)
                                )
                            );
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PostInstallation,
                            OpName.PermissionChanges,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode);
                        return opResult;
                    }
                    else
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PostInstallation,
                            OpName.PermissionChanges,
                            OpResult.Success);
                    }
                }

                // Update config with new data.
                InstallActionProcessor.UpdateDrScoutConfig(installationLocation, setupAction, role);

                // Update CXPS config.
                InstallActionProcessor.UpdateCXPSConfig(installationLocation, setupAction, role);

                // Setup firewall exceptions.
                SetupHelper.InstallVXFirewall(installationLocation);

                // Check UpperFilter registry key value.
                if (role == AgentInstallRole.MS)
                {
                    if (!SetupHelper.CheckUpperFilterRegistryKeyValue())
                    {
                        SetupHelper.AddInDskFltToUpperFilterRegistry();
                        if (!SetupHelper.CheckUpperFilterRegistryKeyValue())
                        {
                            opResult.Error = StringResources.UpperFilterInDskFltNotUpdated;
                            opResult.Status = OperationStatus.Failed;
                            opResult.ProcessExitCode =
                                SetupHelper.UASetupReturnValues.IndskfltValueNotUpdatedInUpperFilterRegistry;
                            SummaryFileWrapper.RecordOperation(
                                Scenario.PostInstallation,
                                OpName.UpperFilterRegistryKeyValidation,
                                OpResult.Failed,
                                (int)opResult.ProcessExitCode);
                            return opResult;
                        }
                        else
                        {
                            SummaryFileWrapper.RecordOperation(
                                Scenario.PostInstallation,
                                OpName.UpperFilterRegistryKeyValidation,
                                OpResult.Success);
                        }
                    }
                }

                // Execute InDskFlt driver manifest file.
                if (role == AgentInstallRole.MS)
                {
                    if (!SetupHelper.InvokeIndskfltManifest())
                    {
                        Trc.Log(LogLevel.Error, "InDskFlt manifest installation failed.");
                    }

                    SummaryFileWrapper.RecordOperation(
                        Scenario.PostInstallation,
                        OpName.IndskfltManifestExecution,
                        OpResult.Success);
                }

                // Execute drvutil to populate OS version keys in upgrade.
                if (setupAction == SetupAction.Upgrade && role == AgentInstallRole.MS)
                {
                    Trc.Log(LogLevel.Always, "Executing drvutil to populate OS version keys in upgrade.");

                    string output;
                    CommandExecutor.ExecuteCommand(
                        Path.Combine(installationLocation, @"drvutil.exe"),
                        out output,
                        "--UpdateOSVersion");
                    Trc.Log(LogLevel.Always, "drvutil output: {0}", output);
                }

                // Set the post installation status.
                InstallActionProcessor.SetPostInstallActionStatus(UnifiedSetupConstants.SuccessStatus);

                // Install Azure-VM-Agent.
                if (Platform.VmWare == platform && !ASRSetupFramework.SetupHelper.IsAzureStackVirtualMachine())
                {
                    InstallActionProcessor.AzureVMAgentInstall();
                }

                // Start filter driver service.
                if (role == AgentInstallRole.MS)
                {
                    opResult = SetupHelper.LoadIndskFltDriver();
                    var operationName = (opResult.ProcessExitCode ==
                                        SetupHelper.UASetupReturnValues.FailedToInstallAndConfigureServices) ?
                                        OpName.CreateAndConfigureServices: OpName.RebootRequiredPostDiskFilterInstall;

                    SummaryFileWrapper.RecordOperation(
                        Scenario.PostInstallation,
                        operationName,
                        (opResult.ProcessExitCode == SetupHelper.UASetupReturnValues.Successful) ? 
                                OpResult.Success : OpResult.Failed,
                        (int)opResult.ProcessExitCode,
                        opResult.Error);

                    if (opResult.Status == OperationStatus.Failed)
                    {
                        return opResult;
                    }
                }

                if(vssProviderReregistrationSucceeded == false)
                {
                    opResult.Error = StringResources.AgentInternalError;
                    opResult.Status = OperationStatus.Warning;
                    opResult.ProcessExitCode = SetupHelper.UASetupReturnValues.SucceededWithWarnings;
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
                Trc.Log(LogLevel.Error, "CRITICALEXCEPTION hit during post install: {0}", ex);
                SummaryFileWrapper.RecordOperation(
                    Scenario.PostInstallation,
                    OpName.PostInstallActions,
                    OpResult.Failed,
                    (int)SetupHelper.UASetupReturnValues.FailedWithErrors,
                    StringResources.AgentInstallationGenericError,
                    null,
                    null,
                    ex.ToString(),
                    null);
                ErrorLogger.LogInstallerError(
                    new InstallerException(
                        UASetupErrorCode.ASRMobilityServiceInstallerPostInstallationStepsFailed.ToString(),
                        null,
                        UASetupErrorCodeMessage.PostInstallationStepsFailed
                        )
                    );
                return new OperationResult
                {
                    Error = StringResources.AgentInstallationGenericError,
                    ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors,
                    Status = OperationStatus.Failed
                };
            }
        }

        #endregion PostInstallationSteps

        #region Internal Methods
        /// <summary>
        /// Starts agent services. This action is run only when agent is
        /// already configured.
        /// </summary>
        /// <returns>Final operation status.</returns>
        internal static OperationResult StartAgentServices()
        {
            try
            {
                SetupAction setupAction = PropertyBagDictionary.Instance.GetProperty<SetupAction>(
                    PropertyBagConstants.SetupAction);

                var opResult = new OperationResult();

                // Change svagent and appservice service startuptype to Automatic (Delayed Start).
                if (!SetupHelper.ChangeAgentServicesStartupType(
                    NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START))
                {
                    opResult.Error = StringResources.FailedToSetServiceStartupTypeToAutomatic;
                    opResult.ProcessExitCode = 
                        SetupHelper.UASetupReturnValues.FailedToSetServiceStartupType;
                    opResult.Status = OperationStatus.Failed;
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Installation,
                        OpName.ChangeAgentServicesStartupTypeToAutomatic,
                        OpResult.Failed,
                        (int)opResult.ProcessExitCode);
                    return opResult;
                }
                else
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Installation,
                        OpName.ChangeAgentServicesStartupTypeToAutomatic,
                        OpResult.Success);
                }

                // Start services.
                if (!SetupHelper.StartUAServices(setupAction == SetupAction.Install))
                {
                    opResult.Error = StringResources.FailedToStartServices;
                    opResult.Status = OperationStatus.Failed;
                    opResult.ProcessExitCode = 
                        SetupHelper.UASetupReturnValues.UnableToStartServices;
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Installation,
                        OpName.BootServices,
                        OpResult.Failed,
                        (int)opResult.ProcessExitCode);
                    return opResult;
                }
                else
                {
                    SummaryFileWrapper.RecordOperation(
                        Scenario.Installation,
                        OpName.BootServices,
                        OpResult.Success);
                }

                return opResult;
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "CRITICALEXCEPTION hit during Start Agent services: {0}", ex);
                ErrorLogger.LogInstallerError(
                    new InstallerException(
                        UASetupErrorCode.ASRMobilityServiceInstallerAgentServicesStartFailedWithInternalError.ToString(),
                        null,
                        UASetupErrorCodeMessage.FailedToStartAgentServicesInternalError
                        )
                    );
                return new OperationResult
                {
                    Error = StringResources.AgentInstallationGenericError,
                    ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors,
                    Status = OperationStatus.Failed
                };
            }
        }

        /// <summary>
        /// Checks filter driver status.
        /// </summary>
        /// <returns>Checks filter driver status.</returns>
        internal static OperationResult CheckFilterDriverStatus()
        {
            var opResult = new OperationResult();

            try
            {
                if (SetupHelper.GetAgentInstalledRole().Equals(AgentInstallRole.MS))
                {
                    opResult = SetupHelper.LoadIndskFltDriver();

                    if (OperationStatus.Failed == opResult.Status)
                    {
                        SummaryFileWrapper.RecordOperation(
                            Scenario.PostInstallation,
                            OpName.CreateAndConfigureServices,
                            OpResult.Failed,
                            (int)opResult.ProcessExitCode,
                            opResult.Error);
                        return opResult;
                    }

                    SummaryFileWrapper.RecordOperation(
                    Scenario.PostInstallation,
                    OpName.FilterDriverStatusValidation,
                    OpResult.Success);
                }
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "CRITICALEXCEPTION hit during Driver status check: {0}", ex);
                SummaryFileWrapper.RecordOperation(
                    Scenario.PostInstallation,
                    OpName.FilterDriverStatusValidation,
                    OpResult.Failed,
                    (int)SetupHelper.UASetupReturnValues.FailedWithErrors,
                    StringResources.AgentInstallationGenericError);
                ErrorLogger.LogInstallerError(
                    new InstallerException(
                        UASetupErrorCode.ASRMobilityServiceInstallerReplicationEngineError.ToString(),
                        null,
                        UASetupErrorCodeMessage.ReplicationEngineFailure
                        )
                    );
                return new OperationResult
                {
                    Error = StringResources.AgentInstallationGenericError,
                    ProcessExitCode = SetupHelper.UASetupReturnValues.FailedWithErrors,
                    Status = OperationStatus.Failed
                };
            }

            return opResult;
        }

        /// <summary>
        /// Reads MobilityServiceValidator json file entries and writes them into installer summary json file.
        /// </summary>
        /// <param name="msvSummaryJsonPath">MobilityServiceValidator summary json file path</param>
        internal static void WriteEntriesIntoInstallerSummaryJsonFile(string msvSummaryJsonPath)
        {
            // Read MobilityServiceValidator json file entries and write them into installer summary json file.
            JsonSerializerSettings SerializerSettings = new JsonSerializerSettings()
            {
                Formatting = Formatting.Indented,
                NullValueHandling = NullValueHandling.Ignore
            };

            using (StreamReader sw = new StreamReader(msvSummaryJsonPath, false))
            using (JsonTextReader reader = new JsonTextReader(sw))
            {
                JsonSerializer summaryserializer = JsonSerializer.Create(SerializerSettings);
                List<OperationDetails> ops = summaryserializer.Deserialize<List<OperationDetails>>(reader);
                foreach (var op in ops)
                {
                    if (op.ErrorCode != 0)
                    {
                        op.ErrorCode = (int)SetupHelper.UASetupReturnValues.ProductPrereqsValidationFailed; 
                    }

                    SummaryFileWrapper.RecordOperation(
                        Scenario.ProductPreReqChecks,
                        op.OperationName,
                        op.Result,
                        op.ErrorCode,
                        op.Message,
                        op.Causes,
                        op.Recommendation,
                        op.Exception,
                        op.ErrorCodeName);
                }
            }
        }

        #endregion Internal Methods
    }
}
