#
# V2A GQL Common Parameters
#

#region Logger Settings
if ($global:LogDir -eq $null)
{
   $global:LogDir = "$PSScriptRoot\V2AGQLLogs"
}
$global:LogName = $null
#endregion Logger Settings

#region Enums

# Enum for Tag Type
Add-Type -TypeDefinition @"
   public enum TagType
   {
      ApplicationConsistent,
      CrashConsistent,
      Latest
   }
"@

# Enum for LogType
Add-Type -TypeDefinition @"
   public enum LogType
   {
      Error,
      Info,
      Warning
   }
"@

#endregion Enums

# Read V2A GQL Configuration File Settings
$global:configFile = $PSScriptRoot + "\V2AGQLConfig.xml"
[xml]$configXml = Get-Content $global:configFile

#region AzureFabric
# Parse the AzureFabric config details
$AzureFabric = $configXml.V2AGQLConfig.AzureFabric
$global:VaultName = $AzureFabric.VaultName
$global:SubscriptionName = $AzureFabric.SubscriptionName
$global:SubscriptionId = $AzureFabric.SubscriptionId

# AAD App identity to login to Azure subscription
#
#
$global:DrDatapPlaneAppClientId = "7c07fb08-b554-4955-8baf-13d2f772a65b"
$global:TenantId = "72f988bf-86f1-41af-91ab-2d7cd011db47"
$global:AgentSpnCertName = "AgentSpnCert";
$global:SecretsKeyVaultName = 'asr-src-secrets-kv'
$global:AsrTestSecretName = 'asrtest'

$global:VaultFile = $AzureFabric.VaultFile
$global:RecResourceGroup = $AzureFabric.ResourceGroup
$global:RecResourceGroupId = $AzureFabric.ResourceGroupId
$global:RecStorageAccountId = $AzureFabric.StorageAccountId
$global:RecoveryVnetId = $AzureFabric.VnetId
$global:RecoverySubnet = $AzureFabric.Subnet
$global:UserName = $AzureFabric.UserName
$global:Password = $AzureFabric.Password
$global:ResourceGroup = $AzureFabric.ResourceGroup
#endregion AzureFabric

#region InMageFabric
$InMageFabric = $configXml.V2AGQLConfig.InMageFabric

#region ConfigServer
$ConfigServer = $InMageFabric.ConfigServer
$global:CSPSName = $ConfigServer.Name
$global:CSPSFQDN = $ConfigServer.FQDN
$global:CSIPAddress = $ConfigServer.IPAddress
$global:CSHttpsPort = $ConfigServer.HttpsPort
$global:Passphrase = $ConfigServer.Passphrase
$global:CSUserName = $ConfigServer.UserName
$global:CSPassword = $ConfigServer.Password
#endregion ConfigServer

#region AzureProcessServer
$AzProcessServer = $InMageFabric.AzureProcessServer
$global:UseAzurePS = $AzProcessServer.UseAzurePS
$global:AzPSName = $AzProcessServer.Name
#endregion AzureProcessServer

#region VCenter
$Vcenter = $InMageFabric.VCenter
$global:VcenterName = $Vcenter.Name
$global:VcenterIP = $Vcenter.IpAddress
$global:VcenterPort = $Vcenter.Port
$global:VcenterUserName = $Vcenter.UserName
$global:VcenterPassword = $Vcenter.Password
#endregion VCenter

#region Source
$global:source = $InMageFabric.SourceServer
$global:SourceFriendlyName = $source.FriendlyName
$global:SourceIPAddress = $source.IPAddress
$global:SourceOSType = $source.OsType
$global:SourceUsername = $source.UserName
$global:SourcePassword = $source.Password
#endregion Source

$global:SourceSeverList = $InMageFabric.SourceServerList.SourceServer

$Deployment = $configXml.V2AGQLConfig.Deployment
$global:Testbinroot = $Deployment.Testbinroot
$global:BuildLocation = $Deployment.BuildLocation
$global:BuildType = $Deployment.BuildType
$global:IsReportingEnabled = $Deployment.IsReportingEnabled
$global:BranchName = $Deployment.BranchName
$global:SharedLocation = $Deployment.SharedLocation
$global:LinuxMTUAFile = $Deployment.LinuxMTUAFile
$global:VaultFile = $Deployment.VaultFile
$global:ReturnValue = $false

#region MasterTarget
$global:MasterTargetList = $InMageFabric.MasterTargetServerList.MasterTarget
$MasterTargetServerList = $InMageFabric.MasterTargetServerList
$global:MTList = @()
foreach ($mt in $MasterTargetServerList.MasterTarget) {
    $friendlyName = $mt.FriendlyName
    $osType = $mt.OsType
    $datastore = $mt.Datastore
    $retention = $mt.Retention
    $username = $mt.UserName
    $password = $mt.Password

    $mtObj = new-object psobject -prop @{FriendlyName = $friendlyName; OSType = $osType; Datastore = $datastore; Retention = $retention; UserName = $username; Password = $password }
    $global:MTList += $mtObj
}
$global:MTList
#endregion MasterTarget
#endregion InMageFabric

#region ReplicationSettings
$global:AppConsistencySnapshotInHrs = 1
$global:RetentionWindowsInHrs = 24
$global:RPOThresholdInMins = 60
$global:ReplicationPolicy = $global:SourceFriendlyName + "-policy"
$global:ReverseReplicationPolicy = $global:ReplicationPolicy + "-failback"
$global:RpiName = $global:SourceFriendlyName + "-RPI"
$global:ForwardContainerMappingName = "PriConMap-" + $global:SourceFriendlyName
$global:ReverseContainerMappingName = "RecConMap-" + $global:SourceFriendlyName
#endregion ReplicationSettings

#region Other Settings
$global:JobQueryWaitTimeInSeconds = 30
$global:JobQueryWaitTime60Seconds = 60
#$global:TestBinRoot = $PSScriptRoot + "\TestBinRoot"
$global:IgnoreTagType = 0
#endregion Other Settings

Write-Host "CommonParams.ps1 executed"