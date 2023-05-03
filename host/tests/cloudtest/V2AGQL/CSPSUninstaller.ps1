# This script un-installs the CS/PS components from CS
param (
    [Parameter(Mandatory = $false)]
	[string]$Role = "CSPS",
	
	[Parameter(Mandatory = $false)]
	[string]$AbortIfAgentInstalled = $false
)

try {
	Set-ExecutionPolicy Unrestricted -Force -Scope CurrentUser
	write-output "Role = $Role"
	write-output "AbortIfAgentInstalled = $AbortIfAgentInstalled"
	
	# Agent Uninstallation
	write-output "Uninstalling the Agent"
	$app = Get-WmiObject -Class Win32_Product | Where-Object {
		$_.Name -match "Microsoft Azure Site Recovery Mobility Service/Master Target Server"
	}

	if ($app) {
		Write-Output $app
		if ($AbortIfAgentInstalled -eq $true) {
			write-output "Aborting without uninstalling the agent"
			exit 0
		}

		$identityNumber = $app.IdentifyingNumber
		$identifier = $identityNumber.Substring(1,$identityNumber.Length-2)
		$msiUninstallLog = $Env:SystemDrive + "\ProgramData\ASRSetupLogs\UnifiedAgentMSIUninstall.log"
		[ScriptBlock] $MSUninstallBlock = {
			$Process = (Start-Process -Wait -Passthru -FilePath "MsiExec.exe" -ArgumentList "/qn /x `{$identifier`} /L+*V $msiUninstallLog")
			return $Process.ExitCode;    
		}

		$returnCode = (Invoke-Command -ScriptBlock $MSUninstallBlock)
		Write-Output "ASRUnifiedAgent Uninstallation exited with return code: $returnCode"
		if ($returnCode) {
			Write-Output "The uninstallation of agent has failed"
			Throw "The uninstallation of agent has failed"
		}
		
		Write-Output "Successfully removed Microsoft ASR Mobility Service/Master Target Server"
		Start-Sleep -Seconds 120
	} else {
		Write-Output "Microsoft ASR Mobility Service/Master Target Server is not installed"
	}

	if ($Role -match "Agent") {
		exit 0
	}

	$app = Get-WmiObject -Class Win32_Product | Where-Object {
		$_.Name -match "Microsoft Azure Recovery Services Agent"
	}
		
	if ($app) {
		Write-Output "Microsoft Azure Recovery Services Agent is not removed completely"
		Write-Output $app
		
		$identityNumber = $app.IdentifyingNumber
		$identifier = $identityNumber.Substring(1,$identityNumber.Length-2)
		$msiUninstallLog = $Env:SystemDrive + "\ProgramData\ASRSetupLogs\UnifiedAgentMSIUninstall.log"
		[ScriptBlock] $MSUninstallBlock = {
			$Process = (Start-Process -Wait -Passthru -FilePath "MsiExec.exe" -ArgumentList "/qn /x `{$identifier`} /L+*V $msiUninstallLog")
			return $Process.ExitCode;    
		}
		
		$returnCode = (Invoke-Command -ScriptBlock $MSUninstallBlock)
		Write-Output "$(Get-Date -Format "yyyy-MM-dd HH:mm:ss") INFO : Microsoft Azure Recovery Services Agent Uninstallation exited with return code: $returnCode"
		if ($returnCode) {
			Write-Output "The uninstallation of agent has failed"
			Throw "The uninstallation of agent has failed"
		}
		
		Write-Output "Successfully removed Microsoft Azure Recovery Services Agent"
	} else {
		Write-Output "Not found Microsoft Azure Recovery Services Agent"
	}


	# CS/PS Uninstallation
	write-output "Uninstalling the CS/PS"
	$cs=Get-ItemProperty "hklm:\SOFTWARE\Wow6432Node\InMage Systems\Installed Products\9"
	if (($cs) -and ($cs.InstallDirectory)) {
		$path = $cs.InstallDirectory + "\unins000.exe"
		if (!(Test-Path $path)) {
			Write-Output "The CS/PS uninstller $path doesn't exist"
			Write-Output "The uninstallation of CS/PS has failed"
			Throw "The uninstallation of CS/PS has failed, $path doesn't exist"
		}
		#$command = "cmd.exe /C '$path' /verysilent /suppressmsgboxes"
		#Invoke-Expression -Command:"$command"
		
		[ScriptBlock] $CSPSUninstallBlock = {
			$CSProcess = (Start-Process -Wait -Passthru -FilePath $path -ArgumentList "/verysilent /suppressmsgboxes /NORESTART")
			$CSProcess.ExitCode
		}

		$CSUninstallReturnCode = (Invoke-Command -ScriptBlock $CSPSUninstallBlock)
		"$(Get-Date -Format "yyyy-MM-dd HH:mm:ss") INFO : CS uninstallation Return Code         : $CSUninstallReturnCode "
		if ($CSUninstallReturnCode) {
			Write-Output "The uninstallation of CS/PS has failed"
			Throw "The uninstallation of CS/PS has failed"
		}
		Start-Sleep -s 15
	} else {
		Write-Output "CS/PS is not installed"
	}


	#DRA Uninstall
	$msiUninstallLog = $Env:SystemDrive + "\ProgramData\ASRSetupLogs\DRAUninstall.log"
	write-output "Uninstalling the DRA Agent"
	$app = Get-WmiObject -Class Win32_Product | Where-Object {
		$_.Name -match "Microsoft Azure Site Recovery Provider"}
	$identityNumber = $app.IdentifyingNumber
	if (($app) -and ($identityNumber)) {
		write-output "Found DRA installer $app"
		write-output $identityNumber >> $msiUninstallLog
		$identifier = $identityNumber.Substring(1,$identityNumber.Length-2)
		[ScriptBlock] $DRAUninstallBlock = {
			$Process = (Start-Process -Wait -Passthru -FilePath "MsiExec.exe" -ArgumentList "/qn /x `{$identifier`} /L+*V $msiUninstallLog")
			return $Process.ExitCode; 
		}

		$returnCode = (Invoke-Command -ScriptBlock $DRAUninstallBlock)
		Write-Output "$(Get-Date -Format "yyyy-MM-dd HH:mm:ss") INFO : Microsoft Azure Site Recovery Provider Uninstallation exited with return code: $returnCode"
		if ($returnCode) {
			write-output "DRA uninstallation has failed"
			Throw "DRA uninstallation has failed"
		}
			
		Start-Sleep -s 15
		write-output "Successfully removed DRA"
	} else {
		write-output "Not found DRA"
	}


	# Third party
	write-output "Uninstalling the TP"
	$tpToolsPath = $Env:SystemDrive + "\thirdparty\unins000.exe"
	if (Test-Path $tpToolsPath ) {
		[ScriptBlock] $CXTPUninstallBlock= {
			$CXTPProcess = (Start-Process -Wait -Passthru -FilePath $tpToolsPath -ArgumentList "/verysilent /suppressmsgboxes /NORESTART")
			return $CXTPProcess.ExitCode
		}
		
		$CXTPUninstallReturnCode = (Invoke-Command -ScriptBlock $CXTPUninstallBlock)
		write-output "$(Get-Date -Format "yyyy-MM-dd HH:mm:ss") INFO : CXTP uninstallation Return Code         : $CXTPUninstallReturnCode "
		if ($CXTPUninstallReturnCode) {
			write-output "CX TP uninstallation has failed"
			Throw "CX TP uninstallation has failed"
		}
		
		write-output "Successfully removed CX TP"
		Start-Sleep -s 10
	} else {
		write-output "CX TP is not installed"
	}
	
	write-output "Removing stale folders"
	Remove-Item C:\ProgramData\ASR-Backup\* -Recurse -Force
	Move-Item -Path C:\ProgramData\ASR*Logs -Destination C:\ProgramData\ASR-Backup\ -Force -ErrorAction SilentlyContinue

	write-output "Removing the registries"
	Remove-Item -Path 'hklm:\SOFTWARE\Microsoft\Azure Site Recovery' -Recurse -Force

	exit 0
} catch {
	write-output "Exception caught - $_.Exception.Message"
	write-output "failed Item - $_.Exception.ItemName"
	Throw
}