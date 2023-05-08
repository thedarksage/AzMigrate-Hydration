<#
#.SYNOPSIS
#
# This script updates the empty A2AGQLConfig.xml file copied as part of "copy scripts" to TDM box
# The naming convention in this script is aligned with "create vm" script, ensure both updateconfig.ps1 and CreateNewVM.ps1 scripts are modified accordingly
# .\UpdateConfig.ps1 -TestBinRoot '[TestBinRoot]' -SubscriptionName '[SubscriptionName]' -SubscriptionId '[SubscriptionId]' -PrimaryLocation '[PrimaryLocation]' -RecoveryLocation '[RecoveryLocation]' -VaultRGLocation '[VaultRGLocation]' -OSType '[OSType]' -VMNamePrefix '[VMNamePrefix]' -OSName '[OSName]'
# Replace brackets with appropriate values
#>


Param(
    # Directory path on the TDM for scripts
    [Parameter(Mandatory=$True)]
    [string]
    $TestBinRoot,

    [Parameter(Mandatory=$True)]
    [string]
    $SubscriptionName,

    [Parameter(Mandatory=$True)]
    [string]
    $SubscriptionId,

    [Parameter(Mandatory=$True)]
    [string]
    $PrimaryLocation,

    [Parameter(Mandatory=$True)]
    [string]
    $RecoveryLocation,

    [Parameter(Mandatory=$True)]
    [string]
    $VaultRGLocation,
    
    [Parameter(Mandatory=$True)]
    [string]
    $OSType,

    [Parameter(Mandatory=$True)]
    [string]
    $BranchRunPrefix,

    [Parameter(Mandatory=$True)]
    [string]
    $OSName,

    # Reporting should be set to false for any testing purposes to avoid sending reports to wide audiance
    # Note that logs are available as part of A2AGQLLog. If you missed the session, you can right-click "Browse Jobs Logs" on respective WTT jobs
    [Parameter(Mandatory=$True)]
    [string]  $EnableMailReporting = "true"
)

# Include common library files
. $PSScriptRoot\CommonFunctions.ps1

$OpName = "CreateConfig"

# TODO : Update the list with recent Geo Names
##### GeoName-GeoShortName mapping #####
$GeoNameGeoShortNameMap = @{"SoutheastAsia" = "sea"}
$GeoNameGeoShortNameMap.Set_Item("EastAsia", "ea")
$GeoNameGeoShortNameMap.Set_Item("SouthIndia", "ins")
$GeoNameGeoShortNameMap.Set_Item("CentralIndia", "inc")
$GeoNameGeoShortNameMap.Set_Item("WestIndia", "inw")
$GeoNameGeoShortNameMap.Set_Item("NorthCentralUS", "ncus")
$GeoNameGeoShortNameMap.Set_Item("NorthEurope", "ne")
$GeoNameGeoShortNameMap.Set_Item("WestEurope", "we")
$GeoNameGeoShortNameMap.Set_Item("EastUS", "eus")
$GeoNameGeoShortNameMap.Set_Item("WestUS", "wus")
$GeoNameGeoShortNameMap.Set_Item("SouthCentralUS", "scus")
$GeoNameGeoShortNameMap.Set_Item("CentralUS", "cus")
$GeoNameGeoShortNameMap.Set_Item("EastUS2", "eus2")
$GeoNameGeoShortNameMap.Set_Item("JapanEast", "jpe")
$GeoNameGeoShortNameMap.Set_Item("JapanWest", "jpw")
$GeoNameGeoShortNameMap.Set_Item("BrazilSouth", "brs")
$GeoNameGeoShortNameMap.Set_Item("AustraliaEast", "ae")
$GeoNameGeoShortNameMap.Set_Item("AustraliaSoutheast", "ase")
$GeoNameGeoShortNameMap.Set_Item("CanadaCentral", "cnc")
$GeoNameGeoShortNameMap.Set_Item("CanadaEast", "cne")
$GeoNameGeoShortNameMap.Set_Item("WestCentralUS", "wcus")
$GeoNameGeoShortNameMap.Set_Item("WestUS2", "wus2")
$GeoNameGeoShortNameMap.Set_Item("UKWest", "ukw")
$GeoNameGeoShortNameMap.Set_Item("UKSouth", "uks")
$GeoNameGeoShortNameMap.Set_Item("eastus2euap", "ecy")
$GeoNameGeoShortNameMap.Set_Item("centraluseuap", "ccy")
##########
    
    
try
{
    $VmNameNew = $BranchRunPrefix + "-" + $OSName
    CreateLogFileName $VmNameNew $OpName
    
    LogMessage -Message ("Update config Started") -LogType ([LogType]::Info1)

    # $global:configFile is set by CommonParams based
    # on multitenant ($MT) functionality
    $ConfigFile = $TestBinRoot + "\A2AGQLConfig.xml"
    if ($ConfigFile -ne $global:configFile) {
        CopyFile $ConfigFile $global:configFile
        $ConfigFile = $global:configFile
    }

    Write-Host "Config File: $ConfigFile"

    $xmlDoc = [System.Xml.XmlDocument](Get-Content $ConfigFile);
    
    $RgName = $VmNameNew
    $PrimaryResourceGroup = $RgName
    
    $PrimaryVnet = $VmNameNew + "-" + "VNET" #DGQL-OEL7-VNET
    
    $PrimaryGeo = $GeoNameGeoShortNameMap[$PrimaryLocation]
    $RecoveryGeo = $GeoNameGeoShortNameMap[$RecoveryLocation]
    $VaultGeo = $GeoNameGeoShortNameMap[$VaultRGLocation]

    # Azure Environment
    $AzureElement = $xmlDoc.A2AGQLConfig.AzureEnvironment
    $AzureElement.SubscriptionName = $SubscriptionName
    $AzureElement.SubscriptionId = $SubscriptionId

    $VaultName = "rsvlt-" + $VMNameNew
    $AzureElement.VaultName = $VaultName

    # Assuming all Location Names are valid here
    if ($VaultRGLocation -eq $RecoveryLocation) {
        $IsVaultRGandRecoveryRGSame = "True"
        $AzureElement.VaultName = $AzureElement.VaultName + "-$RecoveryGeo"
    } else {
        # Name Vault and Vault RG differently
        $IsVaultRGandRecoveryRGSame = "False"
        $AzureElement.VaultName = $AzureElement.VaultName + "-$VaultGeo"
        $VaultRGName =  "rg-" + $VMNameNew + "-vlt-" + $VaultGeo
        $AzureElement.VaultRGName = $VaultRGName
    }
    
    $AzureElement.VaultRGLocation = $VaultRGLocation
        
    # Azure Fabric
    $FabricElement = $xmlDoc.A2AGQLConfig.AzureFabric
    $FabricElement.VMName = $VmNameNew
    $FabricElement.OSType = $OSType.ToLower()

    # GQLs support only Managed disk
    $FabricElement.IsManagedDisk = "True"
    $FabricElement.TFOVMName = $VmNameNew + "-test"
    $FabricElement.ReplicationPolicy = "ReplicationPol-" + $VMNameNew
    
    # Primary Fabric
    $PrimaryFabElement = $FabricElement.PrimaryFabrics
    $PrimaryFabElement.PrimaryLocation = $PrimaryLocation
    $PrimaryFabElement.PrimaryResourceGroup = $PrimaryResourceGroup
    
    # This is not in use, can be used for any operation with in the guest VM
    #$PrimaryFabElement.PrimaryStorageAccount = $BranchRunPrefix + $OSName + $PrimaryGeo + "psa"

    $StagingStorageAccount = "st" + $BranchRunPrefix + $OSName + $PrimaryGeo
    $PrimaryFabElement.StagingStorageAccount = [Regex]::Replace($StagingStorageAccount.ToLower(), '[^(a-z0-9)]', '')
    $PrimaryFabElement.PrimaryVnet = $PrimaryVnet
    $PrimaryFabElement.PrimaryFabric = "PriFabric-" + $VmNameNew + "-" + $PrimaryGeo
    $PrimaryFabElement.PrimaryContainer = "PriCon-" + $VmNameNew + "-" + $PrimaryGeo
    $PrimaryFabElement.ForwardContainerMap = "PriConMap-" + $VmNameNew + "-" + $PrimaryGeo
    $PrimaryFabElement.ForwardNetworkMap = "PriNetMap-" + $VmNameNew + "-" + $PrimaryGeo
    
    # Recovery Fabric
    $RecoveryFabElement = $FabricElement.RecoveryFabrics
    $RecoveryFabElement.RecoveryLocation = $RecoveryLocation
    
    $RecoveryResourceGroup = $VmNameNew + "-" + $RecoveryGeo + "-rg"
    $RecoveryFabElement.RecoveryResourceGroup = $RecoveryResourceGroup
    if ($IsVaultRGandRecoveryRGSame -eq "True") {
        $AzureElement.VaultRGName = $RecoveryResourceGroup
    }
    $RecoveryStorageAccount = "st" + $BranchRunPrefix + $OSName + $RecoveryGeo
    $RecoveryFabElement.RecoveryStorageAccount = [Regex]::Replace($RecoveryStorageAccount.ToLower(), '[^(a-z0-9)]', '')
    $RecoveryFabElement.RecoveryVnet = "vnet-" + $VmNameNew + "-" + $RecoveryGeo
    $RecoveryFabElement.RecoveryFabric = "RecFabric-"+ $VmNameNew + "-" + $RecoveryGeo
    $RecoveryFabElement.RecoveryContainer = "RecCon-" + $VmNameNew + "-" + $RecoveryGeo
    $RecoveryFabElement.ReverseContainerMap = "RecConMap-" + $VmNameNew + "-" + $RecoveryGeo
    $RecoveryFabElement.ReverseNetworkMap = "RecNetMap-" + $VmNameNew + "-" + $RecoveryGeo
    
    # A2A Src Params
    if ($EnableMailReporting -eq "true") {
        $A2ASrcParams = $xmlDoc.A2AGQLConfig.A2ASrcParams
        $A2ASrcParams.EnableMailReporting = "true"
        $todaydate = Get-Date -Format dd_MM_yyyy
        switch -regex ($BranchRunPrefix) {
            "^D" {$Branch = "Develop"; break}
            "^R" {$Branch = "Release"; break}
            "^H" {$Branch = "Hotfix"; break}
            "^E" {$Branch = "Escrow"; break}
            default {$Branch = $BranchRunPrefix; break}
        }
        $ReportingTableName = $Branch + "_" + $todaydate
        $A2ASrcParams.ReportingTableName = $ReportingTableName
    }

    $xmlDoc.Save($ConfigFile)

    if ($EnableMailReporting -eq "true") {
        # Initialize reporting Table for this VM key
        InitReporting -VmName $VmNameNew -ReportingTableName $ReportingTableName 

        # Get the Table context
        $LoggerContext = @{}
        $LoggerContext = TestStatusGetContext $VmNameNew "A2A_GQL" $ReportingTableName

        # Mark unsupported fields as "NA"
        TestStatusLog $LoggerContext "DI_180" "NA"
        TestStatusLog $LoggerContext "DI_360" "NA"
        TestStatusLog $LoggerContext "DI_540" "NA"
        TestStatusLog $LoggerContext "Reboot" "NA"

        $OSDetails = $OSType + "-" + $OSName

        # Set the known fields here
        TestStatusLog $LoggerContext "OS" $OSDetails
    }

    LogMessage -Message ("Update config Passed/Completed") -LogType ([LogType]::Info1)


}
catch
{
    LogMessage -Message ('Exception caught - {0}' -f $_.Exception.Message) -LogType ([LogType]::Error)
    LogMessage -Message ('ERROR : @line : {0}' -f $_.InvocationInfo.ScriptLineNumber) -LogType ([LogType]::Error)
    LogMessage -Message ('Failed item - {0}' -f $_.Exception.ItemName) -LogType ([LogType]::Error)

    $ErrorMessage = ('{0} operation failed' -f $OpName)

    LogMessage -Message $ErrorMessage -LogType ([LogType]::Error)
    
    throw $ErrorMessage
}

finally {

    if ($A2ASrcParams.EnableMailReporting -eq "true" -and $A2ASrcParams.ReportingTableName -ne $null) {
        ReportOperationStatus -VmName $VmNameNew -ReportingTableName $ReportingTableName -OperationName $OpName
    }
}
