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

    [parameter(Mandatory=$False)]
    [string]
    $AgentVersion,

    [parameter(Mandatory=$False)]
    [string]
    $ExtensionVersion,

    [parameter(Mandatory=$False)]
    [string]
    $ExtensionType,

    [Parameter(Mandatory=$False)]
    [string]
    $ValidateDeployment="false",

    [Parameter(Mandatory=$False)]
    [string]
    $IsPremiumDR="false",

    # Reporting should be set to false for any testing purposes to avoid sending reports to wide audiance
    # Note that logs are available as part of A2AGQLLog. If you missed the session, you can right-click "Browse Jobs Logs" on respective WTT jobs
    [Parameter(Mandatory=$True)]
    [string]  $EnableMailReporting = "false"
)

# Include common library files
. $PSScriptRoot\CommonFunctions.ps1

$OpName = "CreateConfig"

# TODO : Update the list with recent Geo Names
##### GeoName-GeoShortName mapping #####
$GeoNameGeoShortNameMap = @{"SoutheastAsia" = "sea"}
$GeoNameGeoShortNameMap.Set_Item("australiaeast","ae")
$GeoNameGeoShortNameMap.Set_Item("australiasoutheast","ase")
$GeoNameGeoShortNameMap.Set_Item("brazilsouth","brs")
$GeoNameGeoShortNameMap.Set_Item("centralus","cus")
$GeoNameGeoShortNameMap.Set_Item("eastasia","ea")
$GeoNameGeoShortNameMap.Set_Item("eastus","eus")
$GeoNameGeoShortNameMap.Set_Item("eastus2","eus2")
$GeoNameGeoShortNameMap.Set_Item("centralindia","inc")
$GeoNameGeoShortNameMap.Set_Item("japaneast","jpe")
$GeoNameGeoShortNameMap.Set_Item("japanwest","jpw")
$GeoNameGeoShortNameMap.Set_Item("northcentralus","ncus")
$GeoNameGeoShortNameMap.Set_Item("northeurope","ne")
$GeoNameGeoShortNameMap.Set_Item("southcentralus","scus")
$GeoNameGeoShortNameMap.Set_Item("southindia","ins")
$GeoNameGeoShortNameMap.Set_Item("westeurope","we")
$GeoNameGeoShortNameMap.Set_Item("westus","wus")
$GeoNameGeoShortNameMap.Set_Item("canadaeast","cne")
$GeoNameGeoShortNameMap.Set_Item("canadacentral","cnc")
$GeoNameGeoShortNameMap.Set_Item("westcentralus","wcus")
$GeoNameGeoShortNameMap.Set_Item("westus2","wus2")
$GeoNameGeoShortNameMap.Set_Item("uksouth","uks")
$GeoNameGeoShortNameMap.Set_Item("ukwest","ukw")
$GeoNameGeoShortNameMap.Set_Item("centraluseuap","ccy")
$GeoNameGeoShortNameMap.Set_Item("eastus2euap","ecy")
$GeoNameGeoShortNameMap.Set_Item("koreacentral","krc")
$GeoNameGeoShortNameMap.Set_Item("koreasouth","krs")
$GeoNameGeoShortNameMap.Set_Item("francecentral","frc")
$GeoNameGeoShortNameMap.Set_Item("francesouth","frs")
$GeoNameGeoShortNameMap.Set_Item("australiacentral","acl")
$GeoNameGeoShortNameMap.Set_Item("australiacentral2","acl2")
$GeoNameGeoShortNameMap.Set_Item("southafricawest","saw")
$GeoNameGeoShortNameMap.Set_Item("southafricanorth","san")
$GeoNameGeoShortNameMap.Set_Item("uaecentral","uac")
$GeoNameGeoShortNameMap.Set_Item("uaenorth","uan")
$GeoNameGeoShortNameMap.Set_Item("westindia","inw")
$GeoNameGeoShortNameMap.Set_Item("germanynorth","gn")
$GeoNameGeoShortNameMap.Set_Item("germanywestcentral","gwc")
$GeoNameGeoShortNameMap.Set_Item("switzerlandnorth","szn")
$GeoNameGeoShortNameMap.Set_Item("switzerlandwest","szw")
$GeoNameGeoShortNameMap.Set_Item("norwayeast","nwe")
$GeoNameGeoShortNameMap.Set_Item("norwaywest","nww")
$GeoNameGeoShortNameMap.Set_Item("swedencentral","sdc")
$GeoNameGeoShortNameMap.Set_Item("swedensouth","sds")
$GeoNameGeoShortNameMap.Set_Item("usnateast","exe")
$GeoNameGeoShortNameMap.Set_Item("usnatwest","exw")
$GeoNameGeoShortNameMap.Set_Item("usseceast","rxe")
$GeoNameGeoShortNameMap.Set_Item("ussecwest","rxw")
$GeoNameGeoShortNameMap.Set_Item("brazilsoutheast","bse")
$GeoNameGeoShortNameMap.Set_Item("westus3","wus3")
$GeoNameGeoShortNameMap.Set_Item("jioindiacentral","jic")
$GeoNameGeoShortNameMap.Set_Item("jioindiawest","jiw")
##########
    
    
try
{
    $PrimaryGeo = $GeoNameGeoShortNameMap[$PrimaryLocation]
    $RecoveryGeo = $GeoNameGeoShortNameMap[$RecoveryLocation]
    $VaultGeo = $GeoNameGeoShortNameMap[$VaultRGLocation]

    $VmNameNew = $BranchRunPrefix + "-" + $OSName
    CreateLogFileName $VmNameNew $OpName

    LogMessage -Message ("Update config Started") -LogType ([LogType]::Info1)

    # $global:configFile is set by CommonParams based
    # on multitenant ($MT) functionality
    $ConfigFile = $TestBinRoot + "\A2AGQLConfig.xml"
    <#if ($ConfigFile -ne $global:configFile) {
        CopyFile $ConfigFile $global:configFile
        $ConfigFile = $global:configFile
    }#>

    #sleep 120
    Write-Host "Config File: $ConfigFile"

    $xmlDoc = [System.Xml.XmlDocument](Get-Content $ConfigFile);
    
    $PrimaryResourceGroup = $VmNameNew + "-" + $PrimaryGeo + "-rg"
    $PrimaryVnet = $VmNameNew + "-" + "VNET"+ "-" + $PrimaryGeo

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
    $AzureElement.IsPremiumDR = $IsPremiumDR.ToLower()
	Write-Host "IsPremiumDR=$IsPremiumDR"
        
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

    if( $IsPremiumDR -eq "true")
	{
		$StagingStorageAccount = "pbb45st" + $BranchRunPrefix + $OSName + $PrimaryGeo
		Write-Host "IsPremiumDR, StagingStorageAccount=$StagingStorageAccount"
	}
	else
    {
		$StagingStorageAccount = "st" + $BranchRunPrefix + $OSName + $PrimaryGeo
		Write-Host "StagingStorageAccount=$StagingStorageAccount"		
	}
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
    if( $IsPremiumDR -eq "true")
	{
		$RecoveryStorageAccount = "pbb45st" + $BranchRunPrefix + $OSName + $RecoveryGeo
		Write-Host "IsPremiumDR, RecoveryStorageAccount=$RecoveryStorageAccount"
	}
	else
    {
		$RecoveryStorageAccount = "st" + $BranchRunPrefix + $OSName + $RecoveryGeo
		Write-Host "RecoveryStorageAccount=$RecoveryStorageAccount"
	}
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

    $DeploymentInfo = $xmlDoc.A2AGQLConfig.DeploymentInfo    
    $DeploymentInfo.ExtensionType = $ExtensionType
    if ($ValidateDeployment -eq "true")
    {
        if ($null -eq $AgentVersion -or $null -eq $ExtensionVersion -or $null -eq $ExtensionType)
        {
            Throw "One of the required params is null. AgentVersion = $AgentVersion : ExtensionVersion = $ExtensionVersion : ExtensionType = $ExtensionType"
        }

        $DeploymentInfo.ValidateDeployment = $ValidateDeployment
        $DeploymentInfo.AgentVersion = $AgentVersion
        $DeploymentInfo.ExtensionVersion = $ExtensionVersion
    }

    Write-Host "Saving $ConfigFile"
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
