//---------------------------------------------------------------
//  <copyright file="OperationNames.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Provides list of operations.
//  </summary>
//
//  History:     28-Aug-2017   bhayachi     Created
//----------------------------------------------------------------

using System;

namespace ASRSetupFramework
{
    /// <summary>
    /// List of operation names.
    /// </summary>
    public enum OpName
    {
        #region Agent Operations
        
        RoleValidation,

        PlatformValidation,

        InstallLocationValidation,

        OSValidation,

        VMwareToolsValidation,

        IsASRServerSetupInstalled,

        SystemRestartPendingValidation,

        VSSProviderPrereqValidation,

        BootDriverValidation,

        InteractiveInstallationOnAzureVMs,

        StopServices,

        BackupConfigurationFiles,

        PermissionChanges,

        UpperFilterRegistryKeyValidation,

        IndskfltManifestExecution,

        RebootRequiredPostDiskFilterInstall,

        AgentPointingToDifferentCS,

        CSIPValidation,

        ExternalIPValidation,

        ConnectionPassphraseValidation,

        InstallAgent,

        PrepareForInstall,

        PostInstallActions,

        FilterDriverStatusValidation,

        MergeDrscoutConfig,

        InstallVSSProvider,

        CreateAPPConfig,

        RegisterAgent,

        ChangeAgentServicesStartupTypeToManual,

        ChangeAgentServicesStartupTypeToAutomatic,

        BootServices,

        RebootRequired,

        PrerapeForRegistration,

        Registration,

        ValidateRegistration,

        CredentialFileValidation,

        SourceConfigFileValidation,

        GetBiosHardwareId,

        CertificateValidation,

        UnregisterAgentAndCleanUp,

        RegisterMachine,

        RegisterSourceAgent,

        UnconfigureAgentWithCSPrime,

        DiagnoseAndFix,

        ProductPrereqsValidation,

        PreCheckBootUEFIValidation,

        PrecheckBootDriversValidation,

        PreCheckBootAndSystemDiskValidation,

        PreCheckActivePartitionsValidation,

        PreCheckCriticalServicesValidation,

        CreateSymLinks,

        CreateAndConfigureServices,

        PreCheckVSSInstallationValidation,

        PreCheckFilePathValidation,

        PreCheckCommandExecutionValidation,

        PreCheckSHA2CompatibilityValidation,

        PreCheckAgentMandatoryFilesValidation,

        PreCheckMachineMemoryValidation,

        PreCheckInvolFltDriverValidation,

        InvalidOperation = Int32.MaxValue
        #endregion Agent Operations
    }
}
