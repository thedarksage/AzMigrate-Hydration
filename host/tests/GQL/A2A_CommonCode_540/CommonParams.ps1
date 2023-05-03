#
# A2A GQL Common Parameters
#

# Logger Settings
$global:LogDir = "$PSScriptRoot\A2AGQLLogs"
$global:LogDLLPath = "$PSScriptRoot\LogsExtension\Log4Test.dll"
$global:LogDLLXmlFile = "$PSScriptRoot\LogsExtension\Log4Test.xml"
$Global:Logger = $null

## Enum for Operation Status
Add-Type -TypeDefinition @"
   public enum OperationStatus
   {
      Started,
      Succeeded,
      Failed
   }
"@

## Enum for Operation names
Add-Type -TypeDefinition @"
   public enum ASROperation
   {
      GetJob,
      ResumeJob,

      GetFabric,
      CreateFabric,
      DeleteFabric,
      DeleteVaultRG,
      DeleteRecoveryRG,

      EnableDR,
      DisableDR,
      IRCompletion,
      UpdateVMProperties,
      
      GetContainer,
      CreateContainer,
      DeleteContainer,

      GetPolicy,
      CreatePolicy,
      DeletePolicy,
      GetContainerMapping,
      AssociatePolicy,
      DissociatePolicy,

      GetProtectedItem,
      GetProtectableItem,

      TestFailover,
      CleanupTFO,
      Failover,
      CommitFailover,
      Failback,
      CommitFailback,
      ReverseReplicate,
      SwitchProtection,
      
      InstallDRA,
      RegisterDRA,
      UnregisterDRA,
      
      CreateRP,
      DeleteRP,
      UpdateRP,
      
      RPTestFailover,
      RPCleanupTFO,
      RPFailover,
      RPCommitFailover,
      RPFailback,
      RPCommitFailback,
      RPReverseReplicate,
     
      MapNetwork,
      UnMapNetwork,
     
      MapStorage,
      UnMapStorage,
      
      RefreshDRA,
      VMNicUpdate,
      GIP,
   }
"@

# Read A2A GQL Configuration File
$configFile = $PSScriptRoot + "\A2AGQLConfig.xml"
[xml]$configXml = Get-Content $configFile

# Parse the Azure Environment config details
$AzureEnvironment = $configXml.A2AGQLConfig.AzureEnvironment
$global:VMSubName = $AzureEnvironment.VMSubName
$global:VMSubId = $AzureEnvironment.VMSubId
$global:VMSubUserName = $AzureEnvironment.VMSubUserName
$global:VMSubPassword = $AzureEnvironment.VMSubPassword
$global:VaultName = $AzureEnvironment.VaultName
$global:VaultSubName = $AzureEnvironment.VaultSubName
$global:VaultSubId = $AzureEnvironment.VaultSubId
$global:VaultSubUserName = $AzureEnvironment.VaultSubUserName
$global:VaultSubPassword = $AzureEnvironment.VaultSubPassword
$global:VaultRGName = $AzureEnvironment.VaultRGName

# Parse the Source Target Environment config details
$AzureFabric = $configXml.A2AGQLConfig.AzureFabric
$global:OSType = $AzureFabric.OSType
$global:VMName = $AzureFabric.VMName
$global:TFOVMName = $AzureFabric.TFOVMName
$global:ReplicationPolicy = $AzureFabric.ReplicationPolicy
if( $AzureFabric.IsManagedDisk -eq "true" ) 
{ 
    $global:IsManagedDisk = $true 
} 
else 
{ 
    $global:IsManagedDisk = $false 
}

# Source Fabric and Container Settings
$PrimaryFabrics = $AzureFabric.PrimaryFabrics
$global:PrimaryLocation = $PrimaryFabrics.PrimaryLocation
$global:PriResourceGroup = $PrimaryFabrics.PrimaryResourceGroup
$global:PriStorageAccount = $PrimaryFabrics.PrimaryStorageAccount
$global:StagingStorageAcc = $PrimaryFabrics.StagingStorageAccount
$global:PrimaryVnet = $PrimaryFabrics.PrimaryVnet
$global:PrimaryFabricName = $PrimaryFabrics.PrimaryFabric
$global:PrimaryContainerName = $PrimaryFabrics.PrimaryContainer
$global:ForwardContainerMappingName = $PrimaryFabrics.ForwardContainerMap
$global:ForwardNetworkMappingName = $PrimaryFabrics.ForwardNetworkMap

# Target Fabric and Container Settings
$RecoveryFabrics = $AzureFabric.RecoveryFabrics
$global:RecResourceGroup = $RecoveryFabrics.RecoveryResourceGroup
$global:RecoveryLocation = $RecoveryFabrics.RecoveryLocation
$global:RecStorageAccount = $RecoveryFabrics.RecoveryStorageAccount
$global:RecoveryVnet = $RecoveryFabrics.RecoveryVnet
$global:RecoveryFabricName = $RecoveryFabrics.RecoveryFabric
$global:RecoveryContainerName = $RecoveryFabrics.RecoveryContainer
$global:ReverseContainerMappingName = $RecoveryFabrics.ReverseContainerMap
$global:ReverseNetworkMappingName = $RecoveryFabrics.ReverseNetworkMap

# Churn Config settings
$ChurnConfig = $configXml.A2AGQLConfig.ChurnConfig
$global:TotalChurnSize = $ChurnConfig.TotalChurnSize
$global:ChurnFileSize = $ChurnConfig.ChurnFileSize

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






