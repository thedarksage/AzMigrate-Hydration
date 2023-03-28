#
# A2A GQL Common Parameters
#

# Logger Settings
$global:LogDir = $env:LoggingDirectory
if ($global:LogDir -eq "" -Or $global:LogDir -eq $null)
{
    $global:LogDir = "$PSScriptRoot\A2AGQLLogs"
}
Write-Host "CommonParams : LoggingDirectory : $global:LogDir"

## Enum for Operation Status
Add-Type -TypeDefinition @"
   public enum OperationStatus
   {
      Started,
      Succeeded,
      Failed
   }
"@

# Just for referece from legacy scripts to differentiate VM operation vs ASR operation. Currently ASR operations are working. 
# VM operations are disabled as part of WTT workflow as these operations are not reliable due to remote connectivity "Extensions" are not stable and keep blocking tests
#$ASROperationArr = @('ER', 'TFO', 'UFO', 'SP', 'FB', 'RSP', 'DR', 'CR')
#$VMOperationArr = @('REBOOT', 'COPY', 'UPLOAD', 'APPLYLOAD', 'ISSUETAG', 'VERIFYLOAD', 'UNINSTALL', 'ATTACHDISK', 'CREATEDISKLAYOUT')


# For any Info messages, use Info1
# For adding Info messages in WaitForJobCompletion and WaitForIRCompletion functions, use Info3
# Afer wait completion for WaitForJobCompletion and WaitForIRCompletion, use Info2

Add-Type -TypeDefinition @"
   public enum LogType
   {
      Error,
      Info1,
      Info2,
      Info3,
      Warning,
      Unknown
   }
"@

#
# AAD App identity to login to Azure subscription
#
#
$global:DrDatapPlaneAppClientId = "7c07fb08-b554-4955-8baf-13d2f772a65b"
$global:TenantId = "72f988bf-86f1-41af-91ab-2d7cd011db47"
$global:AgentSpnCertName = "AgentSpnCert"
$global:SecretsKeyVaultName = 'asr-src-secrets-kv'
$global:SecretName = 'VMPassword'

# Read A2A GQL Configuration File
# For MT , config files are created as part of script
$global:configFile = $PSScriptRoot + "\A2AGQLConfig.xml"

$ConfigFile = $global:configFile 
if (!$(Test-Path -Path $ConfigFile)) {
    $ConfigFile = $PSScriptRoot + "\A2AGQLConfig.xml"
    Write-Host "Using default $ConfigFile"
}

Write-Host "Using Config: $ConfigFile"
[xml]$configXml = Get-Content $ConfigFile

# Parse the Azure Environment config details
$AzureEnvironment = $configXml.A2AGQLConfig.AzureEnvironment
$global:SubscriptionName = $AzureEnvironment.SubscriptionName
# This variable is already set in CreateVM script
$global:SubscriptionId = $AzureEnvironment.SubscriptionId
$global:VMSubUserName = $subUserName
$global:VMSubPassword = $subPassword
$global:VaultName = $AzureEnvironment.VaultName
$global:VaultRGName = $AzureEnvironment.VaultRGName
$global:VaultRGLocation = $AzureEnvironment.VaultRGLocation
$global:IsPremiumDR = $AzureEnvironment.IsPremiumDR


# Parse the Source Target Environment config details
$AzureFabric = $configXml.A2AGQLConfig.AzureFabric
$global:VMName = $AzureFabric.VMName
$global:OSType = $AzureFabric.OSType
$global:IsManagedDisk = $AzureFabric.IsManagedDisk
$global:TFOVMName = $AzureFabric.TFOVMName
$global:ReplicationPolicy = $AzureFabric.ReplicationPolicy

# Source Fabric and Container Settings
$PrimaryFabrics = $AzureFabric.PrimaryFabrics
$global:PrimaryLocation = $PrimaryFabrics.PrimaryLocation
$global:PriResourceGroup = $PrimaryFabrics.PrimaryResourceGroup
#$global:PriStorageAccount = $PrimaryFabrics.PrimaryStorageAccount
$global:StagingStorageAcc = $PrimaryFabrics.StagingStorageAccount
$global:PrimaryVnet = $PrimaryFabrics.PrimaryVnet
$global:PrimaryFabricName = $PrimaryFabrics.PrimaryFabric
$global:PrimaryContainerName = $PrimaryFabrics.PrimaryContainer
$global:ForwardContainerMappingName = $PrimaryFabrics.ForwardContainerMap
$global:ForwardNetworkMappingName = $PrimaryFabrics.ForwardNetworkMap

# Target Fabric and Container Settings
$RecoveryFabrics = $AzureFabric.RecoveryFabrics
$global:RecoveryLocation = $RecoveryFabrics.RecoveryLocation
$global:RecResourceGroup = $RecoveryFabrics.RecoveryResourceGroup
$global:RecStorageAccount = $RecoveryFabrics.RecoveryStorageAccount
$global:RecoveryVnet = $RecoveryFabrics.RecoveryVnet
$global:RecoveryFabricName = $RecoveryFabrics.RecoveryFabric
$global:RecoveryContainerName = $RecoveryFabrics.RecoveryContainer
$global:ReverseContainerMappingName = $RecoveryFabrics.ReverseContainerMap
$global:ReverseNetworkMappingName = $RecoveryFabrics.ReverseNetworkMap

# Not in use, this code can be reused - Churn Config settings
#$ChurnConfig = $configXml.A2AGQLConfig.ChurnConfig

# A2A Src Params
$global:EnableMailReporting = $configXml.A2AGQLConfig.A2ASrcParams.EnableMailReporting
$global:ReportingTableName = $configXml.A2AGQLConfig.A2ASrcParams.ReportingTableName

# Deployment Info
$DeploymentInfo = $configXml.A2AGQLConfig.DeploymentInfo
$global:AgentVersion = $DeploymentInfo.AgentVersion
$global:ExtensionVersion = $DeploymentInfo.ExtensionVersion
$global:ExtensionType = $DeploymentInfo.ExtensionType
$global:ValidateDeployment = $DeploymentInfo.ValidateDeployment

##### Other Settings #####
$global:JobQueryWaitTimeInSeconds = 1 * 30;
$global:JobQueryWaitTime60Seconds = 60;
$global:TestBinRoot = $PSScriptRoot + "\TestBinRoot"
$global:CustomExtensionName = "CustomScriptExtension"
if( $OSType -eq "linux")
{
    $global:CustomExtensionName = "ConfigureLinuxMachine"
}

Write-Host "CommonParams.ps1 executed"
