<#
.SYNOPSIS
Uninstall Unified Agent Setup
#>

Param(
	[Parameter(Mandatory=$False)]
    [string] $LogDir
)

# Global Variables
$LogDir = $Env:SystemDrive + "\ProgramData\ASRSetupLogs"
$LogFile = $LogDir + "\UninstallMSIAgent.log"

#
# Log messages to text file
#
function Logger ([string]$message)
{
	Add-Content $LogFile -Value [$([DateTime]::Now)]$message
}

function Main()
{
	New-Item -Type Directory -Path $LogDir -ErrorAction Ignore
	
	$app = Get-WmiObject -Class Win32_Product | Where-Object { $_.Name -match "Microsoft Azure Site Recovery Mobility Service/Master Target Server"}
	Logger "INFO : Agent info :: $app.IdentifyingNumber, $app.Version"

	$identityNumber = $app.IdentifyingNumber
	$identifier = $identityNumber.Substring(1,$identityNumber.Length-2)
	
	[ScriptBlock] $MSUninstallBlock = {
		$Process = (Start-Process -Wait -Passthru -FilePath "MsiExec.exe" -ArgumentList "/qn /x `{$identifier`} /L+*V $LogFile")
		return $Process.ExitCode;    
	}

	$returnCode = (Invoke-Command -ScriptBlock $MSUninstallBlock)
	if($?)
	{
		Logger "INFO : SUCCESS : ASRUnifiedAgent Uninstallation Succeeded with error code: $returnCode"
		Write-Output "Success"
	}
	Logger "INFO : ASRUnifiedAgent Uninstallation exited with return code: $returnCode"

	Switch -regex ($returnCode) {

		"0|1|1605" {
			Logger "INFO : SUCCESS : ASRUnifiedAgent Uninstallation Succeeded with error code: $returnCode"
			Write-Output "Success"
			Start-Sleep -Seconds 120
			exit 0
		}
			
		Default 
		{
			Logger "ERROR : FAIL : ASRUnifiedAgent Uninstallation failed with error code: $returnCode"
			Write-Output "FAIL"
			Start-Sleep -Seconds 120
			exit $returnCode
		}	
	}
	exit 0
}

Main