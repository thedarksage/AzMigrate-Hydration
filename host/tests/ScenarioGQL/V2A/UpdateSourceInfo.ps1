<#
.SYNOPSIS
    Powershell script to update the configuration for a source VM

.DESCRIPTION
    The script is going to capture the configuration for a source to be protected like Name of the VM, IP address, Credentials and OS Type.
#>
param(
    [Parameter(Mandatory=$true)]
    [string]$VMName,
	[Parameter(Mandatory=$true)]
    [string]$IPAddress,
	[Parameter(Mandatory=$true)]
    [string]$OSMode,
	[Parameter(Mandatory=$true)]
    [string]$User,
	[Parameter(Mandatory=$true)]
    [string]$Passwd
)

#
# Include common library files
#
$Error.Clear()
. $PSScriptRoot\CommonFunctions.ps1

# Initiating Log
CreateLogDir
$global:LogName = "$global:LogDir\UpdateSourceInfo_{0}.log" -f (Get-Date -Format "MM-dd-yyyy HH.mm.ss")

LogMessage -Message ("------------------------------------------------------------------------") -LogType ([LogType]::Info)
LogMessage -Message (" You had provided the following inputs:") -LogType ([LogType]::Info)
LogMessage -Message (" VM Name : {0}" -f $VMName) -LogType ([LogType]::Info)
LogMessage -Message (" IP Address : {0}" -f $IPAddress) -LogType ([LogType]::Info)
LogMessage -Message (" OS Type : {0}" -f $OSMode) -LogType ([LogType]::Info)
LogMessage -Message (" UserName : {0}" -f $User) -LogType ([LogType]::Info)
LogMessage -Message (" Password : {0}" -f $Passwd) -LogType ([LogType]::Info)
LogMessage -Message ("------------------------------------------------------------------------") -LogType ([LogType]::Info)

$configFile = $PSScriptRoot + "\V2AGQLConfig.xml"

$global:Id = $VMName
$global:Scenario = "V2A_GQL"
$todaydate = Get-Date -Format dd_MM_yyyy
$global:ReportingTableName = "Daily_" + $todaydate
$OperationName = "UpdateConfig"

try
{
	if ($global:IsReportingEnabled -eq $false) {
		LogMessage -Message ("Reporting is not enabled...") -LogType ([LogType]::Info)
	} else {
		LogMessage -Message ("Initialie reporting...") -LogType ([LogType]::Info)
		
		# Initialize reporting
		InitReporting

		# Mark unsupported fields as "NA"
		$LoggerContext = @{}
		$LoggerContext = TestStatusGetContext $global:Id $global:Scenario $global:ReportingTableName
		TestStatusLog $LoggerContext "Kernel" "NA"
		TestStatusLog $LoggerContext "Agent" "NA"
		TestStatusLog $LoggerContext "OS" "NA"
		TestStatusLog $LoggerContext "Reboot" "NA"
		TestStatusLog $LoggerContext "DI_180" "NA"
		TestStatusLog $LoggerContext "DI_360" "NA"
	}

	$xmlDoc = [System.Xml.XmlDocument](Get-Content $configFile)
	
	$sourceServerNode = $xmlDoc.V2AGQLConfig.InMageFabric.AppendChild($xmlDoc.CreateElement("SourceServer"))

	$newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("FriendlyName"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VMName))
	
	$newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("IPAddress"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($IPAddress))
	
	$newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("OsType"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($OSMode))
	
	$newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("UserName"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($User))
	
	$newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("Password"))
	$newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($Passwd))
	
	$xmlDoc.Save($ConfigFile)
	
	if ($global:IsReportingEnabled -eq $true) {
		TestStatusLog $LoggerContext "Branch" $global:BranchName.ToLower()
		TestStatusLog $LoggerContext "OSType" "$OSMode"
		TestStatusLog $LoggerContext $OperationName "PASSED"
	}
	
	exit 0
}
Catch
{
	LogMessage -Message ("ERROR:: {0}" -f ($Error | ConvertTo-json -Depth 1)) -LogType ([LogType]::Error)
    LogMessage -Message ("V2A GQL Deployment has Failed") -LogType ([LogType]::Error)
	[DateTime]::Now.ToString() + "Exception caught - " + $_.Exception.Message | Out-File $LogFile -Append
	[DateTime]::Now.ToString() + "Failed Item - " + $_.Exception.ItemName | Out-File $LogFile -Append
	
	if ($IsReportingEnabled) {
		TestStatusLog $LoggerContext $OperationName "FAILED" $global:LogName
	}
	
	exit 1
}