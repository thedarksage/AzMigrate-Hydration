<#
.SYNOPSIS
    Powershell script to update the configuration needed for deployment

.DESCRIPTION
    The script is going to capture the configuration of Azure, Source VMs, ESX/vCenter, CS and MT so that deployment can proceed to install the CS and creates an XML file which will be utilized by tests to perform end to end operations for V2A GQL.
#>

#
# Include common library files
#
$Error.Clear()
. $PSScriptRoot\CommonFunctions.ps1

# Initiating Log
CreateLogDir
$global:LogName = "$global:LogDir\InitializeReporting_{0}.log" -f (Get-Date -Format "MM-dd-yyyy HH.mm.ss")

Try
{
	$BranchName = $global:BranchName
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

	$xmlDoc = New-Object System.XML.XMLDocument
	$xmlRootNode = $xmlDoc.appendChild($xmlDoc.CreateElement("V2AGQLConfig"))
		
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