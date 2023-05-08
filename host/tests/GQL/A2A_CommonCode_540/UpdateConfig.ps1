Param(
    [Parameter(Mandatory=$True)]
    [string]
    $ConfigDirectory,

    [Parameter(Mandatory=$False)]
    [string]
    $VMSubUserName = "idcdlslb@microsoft.com",

    [Parameter(Mandatory=$False)]
    [string]
    $VMSubPassword = "*@zure+!DCfun/2018*",
    
    [Parameter(Mandatory=$True)]
    [string]
    $VMSubName,

    [Parameter(Mandatory=$True)]
    [string]
    $VMSubId,   
	
    [Parameter(Mandatory=$True)]
    [string]
    $OSType,

    [Parameter(Mandatory=$True)]
    [string]
    $VMName,
	
    [Parameter(Mandatory=$True)]
    [string]
    $PrimaryResourceGroup,
	
    [Parameter(Mandatory=$True)]
    [string]
    $PrimaryLocation,

    [Parameter(Mandatory=$True)]
    [string]
    $PrimaryStorageAccount,

    [Parameter(Mandatory=$True)]
    [string]
    $PrimaryVnet,  

    [Parameter(Mandatory=$True)]
    [string]
    $IsManagedDisk,

    [Parameter(Mandatory=$True)]
    [string]
    $RecoveryLocation,
	
    [Parameter(Mandatory=$True)]
    [string]
    $VaultSubName,
	
    [Parameter(Mandatory=$True)]
    [string]
    $VaultSubId,

    [Parameter(Mandatory=$True)]
    [string]
    $TotalChurnSize,
	
    [Parameter(Mandatory=$True)]
    [string]
    $ChurnFileSize,
	
    [Parameter(Mandatory=$False)]
    [string]
    $VaultSubUserName = "idcdlslb@microsoft.com",
    
    [Parameter(Mandatory=$False)]
    [string]
    $VaultSubPassword = "*@zure+!DCfun/2018*",
	
    [Parameter(Mandatory=$False)]
    [string]
    $VaultRGName = "VaultRG",

    [Parameter(Mandatory=$false)]
    [String]
    $osSKU
)

# Include common library files
. $PSScriptRoot\CommonFunctions.ps1

##### GeoName-GeoShortName mapping #####
$GeoNameGeoShortNameMap = @{"SoutheastAsia" = "sea"}
$GeoNameGeoShortNameMap.Set_Item("EastAsia", "ea")
$GeoNameGeoShortNameMap.Set_Item("SouthIndia", "ins")
$GeoNameGeoShortNameMap.Set_Item("CentralIndia", "inc")
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
	SetAzureSubscription -SubscriptionName $VMSubName -SubscriptionId $VMSubId -SubUserName $VMSubUserName -SubPassword $VMSubPassword 
	
	$ConfigFile = $ConfigDirectory + "\A2AGQLConfig.xml"
	$xmlDoc = [System.Xml.XmlDocument](Get-Content $ConfigFile);
	$rand = Get-Random -minimum 1 -maximum 999
	
	# Azure Environment
	$AzureElement = $xmlDoc.A2AGQLConfig.AzureEnvironment
	$AzureElement.VMSubName = $VMSubName
	$AzureElement.VMSubId = $VMSubId
	$AzureElement.VMSubUserName = $VMSubUserName
	$AzureElement.VMSubPassword = $VMSubPassword
	$AzureElement.VaultSubName = $VaultSubName
	$AzureElement.VaultSubId = $VaultSubId
	$AzureElement.VaultSubUserName = $VaultSubUserName
	$AzureElement.VaultSubPassword = $VaultSubPassword
	
	$VaultRGName = $VMName + "-vlt-" + $rand + "-rg"
	$VaultName = $VMName + "-" + $rand
	$AzureElement.VaultName = $VaultName
	$AzureElement.VaultRGName = $VaultRGName
	
	# Azure Fabric
	$FabricElement = $xmlDoc.A2AGQLConfig.AzureFabric
	$FabricElement.OSType = $OSType.ToLower()
	
	$FabricElement.IsManagedDisk = $IsManagedDisk
	$FabricElement.VMName = $VMName
	$FabricElement.TFOVMName = $VMName + "-test"
	$FabricElement.ReplicationPolicy = "ReplicationPol-" + $VMName
	
	# Primary Fabric
	$PrimaryGeo = $GeoNameGeoShortNameMap[$PrimaryLocation]
	$RecoveryGeo = $GeoNameGeoShortNameMap[$RecoveryLocation]
	$PrimaryFabElement = $FabricElement.PrimaryFabrics
	$PrimaryFabElement.PrimaryLocation = $PrimaryLocation
	$PrimaryFabElement.PrimaryResourceGroup = $PrimaryResourceGroup
	$VM = Get-AzureRMVM -ResourceGroupName $PrimaryResourceGroup -VMName $VMName
	$storageAccountName = $PrimaryStorageAccount
	if($IsManagedDisk -ne "true")
	{
		$storageAccountName = $VM.StorageProfile.OsDisk.Vhd.Uri.Split("/")[2].Split(".")[0]
	}
	$PrimaryFabElement.PrimaryStorageAccount = $storageAccountName
	$PrimaryFabElement.StagingStorageAccount = $PrimaryStorageAccount + $rand
	$PrimaryFabElement.PrimaryVnet = $PrimaryVnet
	$PrimaryFabElement.PrimaryFabric = "PriFabric-" + $VMName + "-" + $PrimaryGeo
	$PrimaryFabElement.PrimaryContainer = "PriCon-" + $VMName + "-" + $PrimaryGeo
	$PrimaryFabElement.ForwardContainerMap = "PriConMap-" + $VMName + "-" + $PrimaryGeo
	$PrimaryFabElement.ForwardNetworkMap = "PriNetMap-" + $VMName + "-" + $PrimaryGeo
	
	# Primary Fabric
	$PrimaryFabElement = $FabricElement.RecoveryFabrics
	$PrimaryFabElement.RecoveryLocation = $RecoveryLocation
	
	$RecoveryResourceGroup = $VMName + "-" + $RecoveryGeo + "-rg"
	$PrimaryFabElement.RecoveryResourceGroup = $RecoveryResourceGroup
	$RecoveryStorageAccount = $VMName + $rand + "str"
	$PrimaryFabElement.RecoveryStorageAccount = [Regex]::Replace($RecoveryStorageAccount.ToLower(), '[^(a-z0-9)]', '')
	$PrimaryFabElement.RecoveryVnet = $VMName + "-" + $RecoveryGeo + "-" + $rand + "-vnet"
	$PrimaryFabElement.RecoveryFabric = "RecFabric-"+ $VMName + "-" + $RecoveryGeo
	$PrimaryFabElement.RecoveryContainer = "RecCon-" + $VMName + "-" + $RecoveryGeo
	$PrimaryFabElement.ReverseContainerMap = "RecConMap-" + $VMName + "-" + $RecoveryGeo
	$PrimaryFabElement.ReverseNetworkMap = "RecNetMap-" + $VMName + "-" + $RecoveryGeo
	
	# Churn Config
	$ChurnConfigElement = $xmlDoc.A2AGQLConfig.ChurnConfig
	$ChurnConfigElement.TotalChurnSize = $TotalChurnSize
	$ChurnConfigElement.ChurnFileSize = $ChurnFileSize
	
    $xmlDoc.Save($ConfigFile);
}
catch
{
	LogMessage -Message ('Exception caught - {0}' -f $_.Exception.Message) -Type 3
	LogMessage -Message ('ERROR : @line : {0}' -f $_.InvocationInfo.ScriptLineNumber) -Type 3
	LogMessage -Message ('Failed item - {0}' -f $_.Exception.ItemName) -Type 3
	exit 1
}
LogMessage -Message ("Updating the config completed") -Type 1

exit 0