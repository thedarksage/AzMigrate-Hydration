Param
(
	[parameter(Mandatory=$false)]
	[String]$WorkingDir = "D:\TestBinRoot"
)

#
# Log messages to text file
#
function Logger ([string]$message)
{
	$logfile = $WorkingDir + "\DeployPS.log"
	Add-Content $logfile -Value [$([DateTime]::Now)]$message
}

#
# Run Sysprep on PS
#
function RunSysprep()
{
	Logger "INFO : Running Sysprep on ProcessServer"
	C:\Windows\System32\Sysprep\sysprep.exe /generalize /shutdown /oobe
	if (!$?)
	{
		Logger "ERROR : Failed to run sysprep on ProcessServer"
		Write-output "Failure"
		exit 1
	}
	Logger "INFO : Set sysprep on ProcessServer"
	Write-output "Success"
}

RunSysprep