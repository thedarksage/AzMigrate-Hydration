<#
.SYNOPSIS
    Powershell script to update the configuration needed for deployment

.DESCRIPTION
    The script is going to capture the configuration of Azure, Source VMs, ESX/vCenter, CS and MT so that deployment can proceed to install the CS and creates an XML file which will be utilized by tests to perform end to end operations for V2A GQL.
#>

param(
    [Parameter(Mandatory=$false)]
    [string]$VaultName = "v2agql-sea-vault",
	
	[Parameter(Mandatory=$true)]
	[string]$SubscriptionName,
    
	[Parameter(Mandatory=$true)]
    [string]$ResourceGroup,
	
	[Parameter(Mandatory=$true)]
    [string]$ResourceGroupId,
	
	[Parameter(Mandatory=$true)]
    [string]$StorageAccountId,
	
	[Parameter(Mandatory=$true)]
    [string]$VnetId,
	
	[Parameter(Mandatory=$true)]
    [string]$Subnet,
	
	[Parameter(Mandatory=$true)]
    [string]$CSName,
	
	[Parameter(Mandatory=$true)]
    [string]$CSIPAddress,
	
	[Parameter(Mandatory=$true)]
    [string]$CSPassword,
	
	[Parameter(Mandatory=$true)]
    [string]$CSUsername,
	
	[Parameter(Mandatory=$false)]
    [string]$CSHttpsPort = "443",
	
	[Parameter(Mandatory=$false)]
    [string]$UseAzurePS = "NO",
	
	[Parameter(Mandatory=$false)]
    [string]$AzurePSName = $null,
	
	[Parameter(Mandatory=$true)]
    [string]$VCenterName,
		
	[Parameter(Mandatory=$true)]
    [string]$VCenterIPAddress,
	
	[Parameter(Mandatory=$false)]
    [string]$VCenterPort = "443",
	
	[Parameter(Mandatory=$true)]
    [string]$VCenterUsername,
	
	[Parameter(Mandatory=$true)]
    [string]$VCenterPassword,
	
	[Parameter(Mandatory=$false)]
    [string]$WindowsSourceVMInfo = $null,
	
	[Parameter(Mandatory=$false)]
    [string]$LinuxSourceVMInfo = $null,
	
	[Parameter(Mandatory=$true)]
    [string]$LinuxMTVMInfo,
	
	[Parameter(Mandatory=$false)]
    [string]$LinuxMTUAFile = $null,
	
	[Parameter(Mandatory=$false)]
    [string]$MTDatastore = $null,
	
	[Parameter(Mandatory=$false)]
    [string]$MTRetentionDrive = $null,
	
	[Parameter(Mandatory=$false)]
    [string]$SharedLocation = $null,

	[Parameter(Mandatory=$false)]
    [string]$Testbinroot = $env:SystemDrive + "\V2AGQL",
	
	[Parameter(Mandatory=$False)]
    [string]$BuildLocation = $null,
	
	[Parameter(Mandatory=$True)]
    [string]$BuildType,
	
	[Parameter(Mandatory=$false)]
    [string]$BranchName = "develop",
	
	[Parameter(Mandatory=$True)]
    [string]$ApplicationId,
	
	[Parameter(Mandatory=$True)]
    [string]$TenantId,
	
	[Parameter(Mandatory = $false)]
	[String] $IsReportingEnabled = $false
)

#
# Include common library files
#
$Error.Clear()
. $PSScriptRoot\CommonFunctions.ps1

# Initiating Log
CreateLogDir
$global:LogName = "$global:LogDir\UpdateV2AGQLConfig_{0}.log" -f (Get-Date -Format "MM-dd-yyyy HH.mm.ss")

LogMessage -Message ("------------------------------------------------------------------------") -LogType ([LogType]::Info)
LogMessage -Message ("Inputs to the script are as follows:") -LogType ([LogType]::Info)
LogMessage -Message ("VaultName: {0}" -f $VaultName) -LogType ([LogType]::Info)
LogMessage -Message ("SubscriptionName: {0}" -f $SubscriptionName) -LogType ([LogType]::Info)
LogMessage -Message ("ResourceGroup: {0}" -f $ResourceGroup) -LogType ([LogType]::Info)
LogMessage -Message ("ResourceGroupId: {0}" -f $ResourceGroupId) -LogType ([LogType]::Info)
LogMessage -Message ("StorageAccountId: {0}" -f $StorageAccountId) -LogType ([LogType]::Info)
LogMessage -Message ("ApplicationId: {0}" -f $ApplicationId) -LogType ([LogType]::Info)
LogMessage -Message ("TenantId: {0}" -f $TenantId) -LogType ([LogType]::Info)
LogMessage -Message ("VnetId: {0}" -f $VnetId) -LogType ([LogType]::Info)
LogMessage -Message ("Subnet: {0}" -f $Subnet) -LogType ([LogType]::Info)
LogMessage -Message ("CSName: {0}" -f $CSName) -LogType ([LogType]::Info)
LogMessage -Message ("CSIPAddress: {0}" -f $CSIPAddress) -LogType ([LogType]::Info)
LogMessage -Message ("CSUsername: {0}" -f $CSUsername) -LogType ([LogType]::Info)
LogMessage -Message ("CSPassword: {0}" -f $CSPassword) -LogType ([LogType]::Info)
LogMessage -Message ("CSHttpsPort: {0}" -f $CSHttpsPort) -LogType ([LogType]::Info)
LogMessage -Message ("UseAzurePS: {0}" -f $UseAzurePS) -LogType ([LogType]::Info)
LogMessage -Message ("AzurePSName: {0}" -f $AzurePSName) -LogType ([LogType]::Info)
LogMessage -Message ("VCenterName: {0}" -f $VCenterName) -LogType ([LogType]::Info)
LogMessage -Message ("VCenterIPAddress: {0}" -f $VCenterIPAddress) -LogType ([LogType]::Info)
LogMessage -Message ("VCenterPort: {0}" -f $VCenterPort) -LogType ([LogType]::Info)
LogMessage -Message ("VCenterUsername: {0}" -f $VCenterUsername) -LogType ([LogType]::Info)
LogMessage -Message ("VCenterPassword: {0}" -f $VCenterPassword) -LogType ([LogType]::Info)
LogMessage -Message ("WindowsSourceVMInfo: {0}" -f $WindowsSourceVMInfo) -LogType ([LogType]::Info)
LogMessage -Message ("LinuxSourceVMInfo: {0}" -f $LinuxSourceVMInfo) -LogType ([LogType]::Info)
LogMessage -Message ("LinuxMTVMInfo: {0}" -f $LinuxMTVMInfo) -LogType ([LogType]::Info)
LogMessage -Message ("LinuxMTUAFile: {0}" -f $LinuxMTUAFile) -LogType ([LogType]::Info)
LogMessage -Message ("MTDatastore: {0}" -f $MTDatastore) -LogType ([LogType]::Info)
LogMessage -Message ("MTRetentionDrive: {0}" -f $MTRetentionDrive) -LogType ([LogType]::Info)
LogMessage -Message ("SharedLocation: {0}" -f $SharedLocation) -LogType ([LogType]::Info)
LogMessage -Message ("Testbinroot: {0}" -f $Testbinroot) -LogType ([LogType]::Info)
LogMessage -Message ("BuildLocation: {0}" -f $BuildLocation) -LogType ([LogType]::Info)
LogMessage -Message ("BuildType: {0}" -f $BuildType) -LogType ([LogType]::Info)
LogMessage -Message ("BranchName: {0}" -f $BranchName) -LogType ([LogType]::Info)
LogMessage -Message ("IsReportingEnabled: {0}" -f $IsReportingEnabled) -LogType ([LogType]::Info)
LogMessage -Message ("------------------------------------------------------------------------") -LogType ([LogType]::Info)

Try
{
	$ConfigFile = $Testbinroot + "\V2AGQLConfig.xml"
	if (Test-Path $ConfigFile) {
		Remove-Item $ConfigFile
	}

	$global:Id = $BranchName.ToLower()
	$global:Scenario = "V2A_GQL_DEPLOYMENT"
	$todaydate = Get-Date -Format dd_MM_yyyy
	$global:ReportingTableName = "Daily_" + $todaydate
	$OperationName = "UpdateConfig"
		
	if ($IsReportingEnabled -eq $false) {
		LogMessage -Message ("Reporting is not enabled...") -LogType ([LogType]::Info)
	} else {

		LogMessage -Message ("Initialize reporting...") -LogType ([LogType]::Info)
		# Initialize reporting
		InitReporting
		
		# Mark unsupported fields as "NA"
		$LoggerContext = @{}
		$LoggerContext = TestStatusGetContext $global:Id $global:Scenario $global:ReportingTableName
		TestStatusLog $LoggerContext "CS" "NA"
		TestStatusLog $LoggerContext "PS" "NA"
		TestStatusLog $LoggerContext "AzurePS" "NA"
		TestStatusLog $LoggerContext "MARS" "NA"
	}

	if (($LinuxMTVMInfo -ne $null) -and ($LinuxMTUAFile -eq $null)) {
		LogMessage -Message ("The Linux Unified Agent file name is not provided") -LogType ([LogType]::Error)
		
		if ($IsReportingEnabled -eq $true) {
			TestStatusLog $LoggerContext $OperationName "FAILED" $global:LogName
		}
		
		exit 1
	}

	if (!($BuildType -match "Custom") -and !($BuildType -match "Develop") -and !($BuildType -match "Release")) {
		LogMessage -Message ("The build type has to match from (Custom, Develop or Release)") -LogType ([LogType]::Error)
		
		if ($IsReportingEnabled -eq $true) {
			TestStatusLog $LoggerContext $OperationName "FAILED" $global:LogName
		}
		
		exit 1
	}

	$global:ApplicationId = $ApplicationId
	$global:TenantId = $TenantId

	$xmlDoc = New-Object System.XML.XMLDocument
	$xmlRootNode = $xmlDoc.appendChild($xmlDoc.CreateElement("V2AGQLConfig"))

	LoginToAzure
	if (!$?) {
		LogMessage -Message ("Failed to login to Azure") -LogType ([LogType]::Error)
		if ($IsReportingEnabled -eq $true) {
			TestStatusLog $LoggerContext $OperationName "FAILED" $global:LogName
		}
		
		exit 1
	}
	
	# Add AzureFabric Node
	$azureFabricNode = $xmlRootNode.appendChild($xmlDoc.CreateElement("AzureFabric"))

	$newXmlNameElement = $azureFabricNode.AppendChild($xmlDoc.CreateElement("VaultName"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VaultName))

	$newXmlNameElement = $azureFabricNode.AppendChild($xmlDoc.CreateElement("SubscriptionName"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($SubscriptionName))
	
	$newXmlNameElement = $azureFabricNode.AppendChild($xmlDoc.CreateElement("ApplicationId"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($ApplicationId))
	
	$newXmlNameElement = $azureFabricNode.AppendChild($xmlDoc.CreateElement("TenantId"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($TenantId))

	$newXmlNameElement = $azureFabricNode.AppendChild($xmlDoc.CreateElement("SubscriptionId"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($subscription.id))

	$newXmlNameElement = $azureFabricNode.AppendChild($xmlDoc.CreateElement("VaultFile"))

	$newXmlNameElement = $azureFabricNode.AppendChild($xmlDoc.CreateElement("ResourceGroup"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($ResourceGroup))

	$newXmlNameElement = $azureFabricNode.AppendChild($xmlDoc.CreateElement("ResourceGroupId"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($ResourceGroupId))

	$newXmlNameElement = $azureFabricNode.AppendChild($xmlDoc.CreateElement("StorageAccountId"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($StorageAccountId))

	$newXmlNameElement = $azureFabricNode.AppendChild($xmlDoc.CreateElement("VnetId"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VnetId))

	$newXmlNameElement = $azureFabricNode.AppendChild($xmlDoc.CreateElement("Subnet"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($Subnet))

	# Add InMage Fabric Node
	$inmageFabricNode = $xmlRootNode.appendChild($xmlDoc.CreateElement("InMageFabric"))

	# Add Configuration Server Node
	$csNode = $inmageFabricNode.appendChild($xmlDoc.CreateElement("ConfigServer"))

	$newXmlNameElement = $csNode.AppendChild($xmlDoc.CreateElement("Name"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($CSName))

	$fqdn = $CSName + ".fareast.corp.microsoft.com"
	$newXmlNameElement = $csNode.AppendChild($xmlDoc.CreateElement("FQDN"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($fqdn))

	$newXmlNameElement = $csNode.AppendChild($xmlDoc.CreateElement("IPAddress"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($CSIPAddress))

	$newXmlNameElement = $csNode.AppendChild($xmlDoc.CreateElement("Passphrase"))

	$newXmlNameElement = $csNode.AppendChild($xmlDoc.CreateElement("HttpsPort"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($CSHttpsPort))

	$newXmlNameElement = $csNode.AppendChild($xmlDoc.CreateElement("UserName"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($CSUsername))

	$newXmlNameElement = $csNode.AppendChild($xmlDoc.CreateElement("Password"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($CSPassword))

	# Add Azure Process Server Node
	$azurePSNode = $inmageFabricNode.appendChild($xmlDoc.CreateElement("AzureProcessServer"))

	$newXmlNameElement = $azurePSNode.AppendChild($xmlDoc.CreateElement("UseAzurePS"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($UseAzurePS))

	$newXmlNameElement = $azurePSNode.AppendChild($xmlDoc.CreateElement("Name"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($AzurePSName))

	# Add VCenter Node
	$vCenterNode = $inmageFabricNode.appendChild($xmlDoc.CreateElement("VCenter"))

	$newXmlNameElement = $vCenterNode.AppendChild($xmlDoc.CreateElement("Name"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VCenterName))

	$newXmlNameElement = $vCenterNode.AppendChild($xmlDoc.CreateElement("FriendlyName"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VCenterName))

	$newXmlNameElement = $vCenterNode.AppendChild($xmlDoc.CreateElement("IpAddress"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VCenterIPAddress))

	$newXmlNameElement = $vCenterNode.AppendChild($xmlDoc.CreateElement("Port"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VCenterPort))

	$fqdn = $VCenterName + ".fareast.corp.microsoft.com"
	$newXmlNameElement = $vCenterNode.AppendChild($xmlDoc.CreateElement("FQDN"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($fqdn))

	$newXmlNameElement = $vCenterNode.AppendChild($xmlDoc.CreateElement("UserName"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VCenterUsername))

	$newXmlNameElement = $vCenterNode.AppendChild($xmlDoc.CreateElement("Password"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VCenterPassword))

	# Add Master Target Server List Node
	$masterTargetServerListNode = $inmageFabricNode.appendChild($xmlDoc.CreateElement("MasterTargetServerList"))
	
	# Add Master Target Node for Windows
	$masterTargetNode = $masterTargetServerListNode.appendChild($xmlDoc.CreateElement("MasterTarget"))
	
	$newXmlNameElement = $masterTargetNode.AppendChild($xmlDoc.CreateElement("FriendlyName"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($CSName))

	$newXmlNameElement = $masterTargetNode.AppendChild($xmlDoc.CreateElement("OsType"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode("Windows"))

	$newXmlNameElement = $masterTargetNode.AppendChild($xmlDoc.CreateElement("Datastore"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($MTDatastore))

	$newXmlNameElement = $masterTargetNode.AppendChild($xmlDoc.CreateElement("Retention"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($MTRetentionDrive))

	$newXmlNameElement = $masterTargetNode.AppendChild($xmlDoc.CreateElement("UserName"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($CSUsername))

	$newXmlNameElement = $masterTargetNode.AppendChild($xmlDoc.CreateElement("Password"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($CSPassword))

	# Add Master Target Node for Linux
	if ($LinuxMTVMInfo -ne $null) {
		$masterTargetNode = $masterTargetServerListNode.appendChild($xmlDoc.CreateElement("MasterTarget"))
		
		$MTInfo = $LinuxMTVMInfo.split(",")
		
		$newXmlNameElement = $masterTargetNode.AppendChild($xmlDoc.CreateElement("FriendlyName"))
		$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($MTInfo[0]))

		$newXmlNameElement = $masterTargetNode.AppendChild($xmlDoc.CreateElement("OsType"))
		$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode("Linux"))

		$newXmlNameElement = $masterTargetNode.AppendChild($xmlDoc.CreateElement("Datastore"))
		$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($MTInfo[1]))

		$newXmlNameElement = $masterTargetNode.AppendChild($xmlDoc.CreateElement("Retention"))
		$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($MTInfo[2]))

		$newXmlNameElement = $masterTargetNode.AppendChild($xmlDoc.CreateElement("UserName"))
		$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($MTInfo[3]))

		$newXmlNameElement = $masterTargetNode.AppendChild($xmlDoc.CreateElement("Password"))
		$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($MTInfo[4]))
	}
	
	# Add Source VMs Information
	$sourceServerListNode = $inmageFabricNode.appendChild($xmlDoc.CreateElement("SourceServerList"))
	$OSType = "Windows"
	$VMinfo = $WindowsSourceVMInfo
	do {
		if ($VMInfo) {
			$VMList = $VMInfo.Split(";")
			for ($i = 0; $i -lt $VMList.Length; $i++) {
				$VM = $VMList[$i].Split(",")
				$sourceServerNode = $sourceServerListNode.appendChild($xmlDoc.CreateElement("SourceServer"))
				
				$newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("Name"))
				$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VM[0]))
				
				$newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("IPAddess"))
				$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VM[1]))
				
				$newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("OsType"))
				$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($OSType))
				
				$newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("UserName"))
				$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VM[2]))
				
				$newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("Password"))
				$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VM[3]))
			}
		}
		if ($OSType -match "Linux") {
			break
		}
		
		$OSType = "Linux"
		$VMinfo = $LinuxSourceVMInfo
	} while(1)
		
	# Add Deployment Node
	$deploymentNode = $xmlRootNode.appendChild($xmlDoc.CreateElement("Deployment"))
	
	if ($LinuxMTVMInfo -ne $null) {
		$newXmlNameElement = $deploymentNode.AppendChild($xmlDoc.CreateElement("LinuxMTUAFile"))
		$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($LinuxMTUAFile))
	}
	
	$newXmlNameElement = $deploymentNode.AppendChild($xmlDoc.CreateElement("SharedLocation"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($SharedLocation))
	
	$newXmlNameElement = $deploymentNode.AppendChild($xmlDoc.CreateElement("Testbinroot"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($Testbinroot))
	
	$newXmlNameElement = $deploymentNode.AppendChild($xmlDoc.CreateElement("BuildLocation"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($BuildLocation))
	
	$newXmlNameElement = $deploymentNode.AppendChild($xmlDoc.CreateElement("BuildType"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($BuildType))
	
	$newXmlNameElement = $deploymentNode.AppendChild($xmlDoc.CreateElement("BranchName"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($BranchName))
	
	$newXmlNameElement = $deploymentNode.AppendChild($xmlDoc.CreateElement("IsReportingEnabled"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($IsReportingEnabled))
	
	$newXmlNameElement = $deploymentNode.AppendChild($xmlDoc.CreateElement("VaultFile"))
	
	$xmlDoc.Save($ConfigFile)
	
	if ($IsReportingEnabled -eq $true) {
		TestStatusLog $LoggerContext $OperationName "PASSED"
	}
	
	exit 0
}
Catch
{
	LogMessage -Message ("ERROR:: {0}" -f ($Error | ConvertTo-json -Depth 1)) -LogType ([LogType]::Error)
    LogMessage -Message ("V2A GQL Deployment has Failed") -LogType ([LogType]::Error)
	[DateTime]::Now.ToString() + "Exception caught - " + $_.Exception.Message | Out-File $LogName -Append
	[DateTime]::Now.ToString() + "Failed Item - " + $_.Exception.ItemName | Out-File $LogName -Append
	
	if ($IsReportingEnabled) {
		TestStatusLog $LoggerContext $OperationName "FAILED" $global:LogName
	}
	
	exit 1
}