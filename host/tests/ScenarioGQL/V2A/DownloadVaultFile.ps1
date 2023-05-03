param(
    [Parameter(Mandatory=$true)]
    [string]$SubscriptionName,
    [Parameter(Mandatory=$true)]
    [string]$VaultName,
	[Parameter(Mandatory=$true)]
    [string]$ResourceGroupName,
    [Parameter(Mandatory=$true)]
    [string]$Testbinroot,
	[Parameter(Mandatory=$true)]
    [string]$LogFile,
	[Parameter(Mandatory=$true)]
    [string]$ApplicationId,
	[Parameter(Mandatory=$true)]
    [string]$TenantId
)

# Initiating Log 

$LogFile = $Testbinroot + "\" + $LogFile
if (Test-Path $LogFile) {
	Remove-Item $LogFile
}

function logger ([string]$message)
{
	write-output [$([DateTime]::Now)] $message | out-file $LogFile -Append
	write-host [$([DateTime]::Now)] $message
}

logger "------------------------------------------------------------------------"
logger "Inputs to the script are as follows:"
logger "Subscription Name : $SubscriptionName"
logger "Vault Name : $VaultName"
logger "Resource Group : $ResourceGroupName"
logger "Test Directory : $Testbinroot"
logger "Log File : $LogFile"
logger "ApplicationId : $ApplicationId"
logger "TenantId : $TenantId"
logger "------------------------------------------------------------------------"

try
{
	$cert = Get-ChildItem -path 'Cert:\CurrentUser\My' | where {$_.Subject -eq 'CN=Test123' }
	logger "User: $env:username"
	$TPrint = $cert.ThumbPrint
	if ($TPrint -eq $null) {
        logger "Failed to fetch the thumbprint"
		exit 1
    }
	
	$Error.Clear()
	Login-AzureRmAccount -ServicePrincipal -CertificateThumbprint $TPrint -ApplicationId $ApplicationId -Tenant $TenantId
	if (!$?) {
		logger "Error: $Error"
		logger "Login to Azure account failing"
		exit 1
	}

	logger "Login to subscription $SubscriptionName completed"

	logger "Selecting subscription : $SubscriptionName"
	$output = Get-AzureRmSubscription –SubscriptionName  $SubscriptionName | Select-AzureRmSubscription
	$output | Out-File $LogFile -Append

	logger "Connecting to vault : $VaultName"
	$vault = Get-AzureRmRecoveryServicesVault -Name $VaultName -ResourceGroupName $ResourceGroupName
	logger "Get-AzureRmRecoveryServicesVault: Return Value: $?"
	if (!$vault) {
		$vault = Get-AzureRmRecoveryServicesVault -Name $VaultName
		if (!$vault) {
			logger "Unable to fetch the vault information"
			exit 1
		}
	}
	
	$vault | Out-File $LogFile -Append

	#Set-AzureRmSiteRecoveryVaultSettings -ARSVault $vault
	$file = Get-AzureRmRecoveryServicesVaultSettingsFile -Vault $vault -Path $Testbinroot
	logger "Get-AzureRmRecoveryServicesVaultSettingsFile: return value $?"
	if ((!$file) -or (!$file.FilePath) -or (!(Test-Path $file.FilePath))) {
		logger "Failed to download the vault credentials file: Error = $Error"
		exit 1
	}
	$downloadPath = $file.FilePath
	logger "Downloaded Vault file : $downloadPath"

	$CredsFileName = $Testbinroot + "\" + $VaultName + ".VaultCredentials" 
	if (Test-Path $CredsFileName) {
		Remove-Item $CredsFileName
	}

	Move $file.FilePath -Destination $CredsFileName -Force
	logger "Vault file moved to : $CredsFileName"

	if ( Test-Path $CredsFileName)
	{
		logger "Successfully downloaded the vault credential file"
		exit 0
	}
	else
	{
		exit 1
	}
}
catch
{
	logger "ERROR:: $Error"
	
	[DateTime]::Now.ToString() + "Exception caught - " + $_.Exception.Message | Out-File $LogFile -Append
	[DateTime]::Now.ToString() + "Failed Item - " + $_.Exception.ItemName | Out-File $LogFile -Append
	$errorMessage = "VAULT DOWNLOAD FAILED"
    throw $errorMessage
	exit 1      
}