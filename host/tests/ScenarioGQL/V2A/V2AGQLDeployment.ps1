<#
.SYNOPSIS
    Powershell script to perform deployment for V2A

.DESCRIPTION
    The script is used to perform the following operations
	1. Disable all the protections
	2. Power-ON the source VMs
	3. Remove the resources VMs, network interfaces and disks from the resource group
	4. Un-install the agent bits from the source VMs and Linux MT
	5. De-register vcenter
	6. Uninstall the CS
	7. Copy builds and scrips and binaries to TDM and CS
	8. Install the CS
	9. Adds the user account to discover the vCenter
	10. Discovers vCenter
	11. Linux MT installation
	
.PARAMETER Operation
    Mandatory parameter, it indicates the operation to be performed on the VM.
    Allowed values for Operation are "INITREPORTING" "DISABLEALLPROTECTIONSANDREMOVEPOLICIES", "STARTSOURCEVMS", "CLEANUPRG", "UNINSTALLAGENTS", "UNINSTALLLINUXMT", "DELVCENTER", "UNINSTALLCS", "COPYBUILDTOTDM", "COPYSCRIPTSTOCS", "DOWNLOADVAULTFILE", "INSTALLCS", "ADDACCOUNTONCS", "ADDVCENTER", "INSTALLLINUXMT"
#>
param (
    [Parameter(Mandatory = $true)]
    [ValidateSet("INITREPORTING", "DISABLEALLPROTECTIONSANDREMOVEPOLICIES", "STARTSOURCEVMS", "CLEANUPRG", "UNINSTALLAGENTS", "UNINSTALLLINUXMT", "DELVCENTER", "UNINSTALLCS", "COPYBUILDTOTDM", "COPYSCRIPTSTOCS", "DOWNLOADVAULTFILE", "INSTALLCS", "ADDACCOUNTONCS", "ADDVCENTER", "INSTALLLINUXMT", IgnoreCase = $true)]
    [String] $Operation
	<#
	,
	
	[Parameter(Mandatory = $false)]
	[String] $BranchName = "develop",
	
	[Parameter(Mandatory = $false)]
	[String] $IsReportingEnabled = $false
	#>
)

#
# Include common library files
#
$Error.Clear()
. $PSScriptRoot\CommonFunctions.ps1

#region Global Variables
$global:Operation = $Operation.ToUpper()
#$global:BranchName = $BranchName.ToLower()

$global:Id = $global:BranchName
$global:Scenario = "V2A_GQL_DEPLOYMENT"
$todaydate = Get-Date -Format dd_MM_yyyy
$global:ReportingTableName = "Daily_" + $todaydate
$LoggerContext = TestStatusGetContext $global:Id $global:Scenario $global:ReportingTableName

# Read V2A GQL Configuration File Settings
$global:configFile = $PSScriptRoot + "\V2AGQLConfig.xml"
$global:xmlDoc = [System.Xml.XmlDocument](Get-Content $global:configFile)
#endregion Global Variables

<#
.SYNOPSIS
    Initializes the reporting and sets the unused columns as NA
#>
Function InitializeReporting()
{
	if (!$IsReportingEnabled) {
		LogMessage -Message ("Reporting is not enabled...") -LogType ([LogType]::Info)
		return;
	}

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

<#
.SYNOPSIS
    Disable all the protectrions if any exist
#>
Function DisableAllProtectionsAndRemovePolicies()
{
	LogMessage -Message ("Disabling of all protected VMs has started...") -LogType ([LogType]::Info)
	#if ((!$global:VaultFile) -or !(Test-Path $global:VaultFile)) {
	if (!$global:VaultFile) {
		LogMessage -Message ("There is no vault file in the configuration") -LogType ([LogType]::Info)
		$global:VaultFile = $global:Testbinroot + "\" + $global:VaultName + ".VaultCredentials"
		LogMessage -Message ("This operation will use the vault file {0}" -f $global:VaultFile) -LogType ([LogType]::Info)
	}
	
	if(!(Test-Path $global:VaultFile)) {
		LogMessage -Message ("The vault credential file doesn't exist") -LogType ([LogType]::Info)
		$global:ReturnValue = $true
		exit 0
	}
	
	LoginToAzure
	if (!$?) {
		LogMessage -Message ("Failed to login to Azure") -LogType ([LogType]::Error)
		exit 1
	}
	
	Import-AzRecoveryServicesAsrVaultSettingsFile -Path $global:VaultFile
	if (!$?) {
        LogMessage -Message ('Failed to import vault : {0}' -f $global:VaultFile) -LogType ([LogType]::Error)
		$global:ReturnValue = $true
        exit 0
    }
	
	$fabricObject = Get-AzRecoveryServicesAsrFabric -FriendlyName $global:CSPSName
	if (!$fabricObject) {
		LogMessage -Message ("The fabric {0} doesn't exist" -f $global:CSPSName) -LogType ([LogType]::Info)
		$global:ReturnValue = $true
		exit 0
	}
	
	LogMessage -Message ("The fabric {0} exists and fabric object is as follows: {1}" -f $global:CSPSName, ($fabricObject | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
	#$fabricObject | Out-File $global:LogName -Append
	
	$containerObjects = Get-AzRecoveryServicesAsrProtectionContainer -Fabric $fabricObject
    foreach ($containerObject in $containerObjects) {
        $protectedItems = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $containerObject
        # DisableDR all protected items
        foreach ($protectedItem in $protectedItems) {
            LogMessage -Message ("Triggering DisableDR(Purge) for item: {0}" -f $protectedItem.Name) -LogType ([LogType]::Info)
            $currentJob = Remove-AzRecoveryServicesAsrReplicationProtectedItem -InputObject $protectedItem
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
            LogMessage -Message ("DisableDR(Purge) completed") -LogType ([LogType]::Info)
        }
		
		$containerMappings = Get-AzRecoveryServicesAsrProtectionContainerMapping -ProtectionContainer $containerObject
		# Remove all Container Mappings
        foreach ($containerMapping in $containerMappings) {
            LogMessage -Message ("Triggering Remove Container Mapping: {0}" -f $containerMapping.Name) -LogType ([LogType]::Info)
            $currentJob = Remove-AzRecoveryServicesAsrProtectionContainerMapping -ProtectionContainerMapping $containerMapping
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
            LogMessage -Message ("Removed Container Mappings") -LogType ([LogType]::Info)
        }
    }
	
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Power-ON the source VMs
#>
Function StartSourceVms()
{
	LogMessage -Message ("Starting source VMs...") -LogType ([LogType]::Info)
	if($global:SourceSeverList -eq $null) {
		LogMessage -Message ('No source server found') -LogType ([LogType]::Info)
		$global:ReturnValue = $true
		return 0
	}
	
	# Adding snap in to read VMWare PowerClI Commands through power shell

	Add-PSSnapin "VMware.VimAutomation.Core" | Out-Null
<#
	if (!$?) {
		LogMessage -Message ('Unable to snap-in VMware.VimAutomation.Core to the current session') -LogType ([LogType]::Error)
		exit 1
	}
#>
	# Connecting to vCenter Server
	Connect-VIServer -Server $global:VcenterIP -User $global:VcenterUserName -Password $global:VcenterPassword -Port $global:VcenterPort
	if (!$global:DefaultVIServers.IsConnected) {
		LogMessage -Message ('Failed to connect to the vCenter server, IPAddress: {0}, User: {1}, Password: {2} and Port: {3}' -f $global:VcenterIP, $global:UserName, $global:Password, $global:VcenterPort) -LogType ([LogType]::Error)
		exit 1
	}
	
	LogMessage -Message ("Connection to the vCenter server {0} is successful" -f $global:VcenterIP) -LogType ([LogType]::Info)
	
	$ret = 0
	foreach($source in $global:SourceSeverList) {
		$vm = Get-VM -Name $source.Name
		if (!$?) {
			LogMessage -Message ("The source VM {0} doesn't exist" -f $VMName) -LogType ([LogType]::Error)
			continue
		}
		
		if ($vm.PowerState -match "PoweredOff") {
			LogMessage -Message ("Starting the VM {0}" -f $source.Name) -LogType ([LogType]::Info)
			Start-VM -VM $vm -Confirm:$false
			$ret = 1
		}
		
		if ($ret -eq 1) {
			LogMessage -Message ("Started one or more VMs and so waiting for those to come up") -LogType ([LogType]::Error)
			Start-Sleep -Seconds 300
		}
	}
	
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Uninstall Agent on both Windows and Linux VMs
#>
Function CleanupResourceGroup()
{
	LogMessage -Message ("Starting cleanup of resource group {0}" -f $global:ResourceGroup) -LogType ([LogType]::Info)
	if (!$global:RecResourceGroup) {
		LogMessage -Message ('No resource group found') -LogType ([LogType]::Info)
		$global:ReturnValue = $true
		return 0
	}
	
	LoginToAzure
	if (!$?) {
		LogMessage -Message ("Failed to login to Azure") -LogType ([LogType]::Error)
		exit 1
	}
	
	$res = Get-AzResource -ResourceType 'Microsoft.Compute/virtualMachines' -ResourceGroupName $global:ResourceGroup
	if($res) {
		 $i = 0
		 while($res[$i]) {
			Remove-AzResource -ResourceId $res[$i].ResourceId -Force
			$i++
		 }
	}
	 
	$res = Get-AzResource -ResourceType 'Microsoft.Compute/disks' -ResourceGroupName $global:ResourceGroup
	if($res) {
		 $i = 0
		 while($res[$i]) {
			Remove-AzResource -ResourceId $res[$i].ResourceId -Force
			$i++
		 }
	}
	 
	$res = Get-AzResource -ResourceType 'Microsoft.Network/networkInterfaces' -ResourceGroupName $global:ResourceGroup
	if($res) {
		 $i = 0
		 while($res[$i]) {
			Remove-AzResource -ResourceId $res[$i].ResourceId -Force
			$i++
		 }
	}
	
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Uninstall Agent on both Windows and Linux VMs
#>
Function UninstallAgents()
{
	if($global:SourceSeverList -eq $null) {
		LogMessage -Message ('No source server found') -LogType ([LogType]::Info)
		$global:ReturnValue = $true
		return 0
	}
	
	# Adding snap in to read VMWare PowerClI Commands through power shell
	Add-PSSnapin "VMware.VimAutomation.Core" | Out-Null
<#
	if (!$?) {
		LogMessage -Message ('Unable to snap-in VMware.VimAutomation.Core to the current session') -LogType ([LogType]::Error)
		exit 1
	}
#>

	# Connecting to vCenter Server
	Connect-VIServer -Server $global:VcenterIP -User $global:VcenterUserName -Password $global:VcenterPassword -Port $global:VcenterPort
	if (!$global:DefaultVIServers.IsConnected) {
		LogMessage -Message ('Failed to connect to the vCenter server, IPAddress: {0}, User: {1}, Password: {2} and Port: {3}' -f $global:VcenterIP, $global:VcenterUserName, $global:VcenterPassword, $global:VcenterPort) -LogType ([LogType]::Error)
		exit 1
	}
	
	LogMessage -Message ("Connection to the vCenter server {0} is successful" -f $global:VcenterIP) -LogType ([LogType]::Info)
	
	foreach($source in $global:SourceSeverList) {
		$VMName = $source.Name
		$Username = $source.UserName
		$Password = $source.Password
		
		LogMessage -Message ("Uninstalling agent on VM {0}" -f $VMName) -LogType ([LogType]::Info)
		
		$vm = Get-VM -Name $VMName
		if (!$?) {
			LogMessage -Message ("The source VM {0} doesn't exist" -f $VMName) -LogType ([LogType]::Error)
			$ret = 1
			continue
		}
		
		LogMessage -Message ("Power state for VM {0} is {1}" -f $VMName, $vm.PowerState) -LogType ([LogType]::Info)
		
		if ($source.OsType -match "Windows") {
			$command = "ls c:\"
			$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
			if (!$?) {
				LogMessage -Message ("Unable to login to the VM {0} using the creds {1}/{2}" -f $VMName, $Username, $Password) -LogType ([LogType]::Error)
				$ret = 1
				continue
			}
            
			LogMessage -Message ("The output of command {0} is as follows: {1}" -f $command, ($out.ScriptOutput | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
			LogMessage -Message ("Login to the VM {0} successful" -f $VMName) -LogType ([LogType]::Info)
			
			#$command = "Remove-Item C:\V2AGQL\CSPSUninstaller.ps1"
			$command = "Remove-Item C:\V2AGQL -Recurse -Force"
			Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
			$command = "mkdir C:\V2AGQL"
			Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
			Get-Item $($global:Testbinroot+"\CSPSUninstaller.ps1") | Copy-VMGuestFile -Destination "C:\V2AGQL" -VM $vm -LocalToGuest -GuestUser $Username -GuestPassword $Password
			
			$command = "powershell.exe C:\V2AGQL\CSPSUninstaller.ps1 -Role 'Agent' > C:\V2AGQL\output.txt"
			$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
			$ret=$out.ExitCode
			LogMessage -Message ("The return value from uninstaller for VM {0} is {1}" -f $VMName, $ret) -LogType ([LogType]::Info)
			Copy-VMGuestFile -VM $vm -GuestToLocal -GuestUser $Username -GuestPassword $Password -Source 'C:\V2AGQL\output.txt' -Destination $global:LogDir
			$output = Get-Content $($global:LogDir+"\output.txt")
			LogMessage -Message ("The un-installation output is as follows: {0}" -f ($output | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
			#$output | Out-File $global:LogName -Append
			Remove-Item $($global:LogDir+"\output.txt")
			LogMessage -Message ("The return value from uninstaller is {0}" -f $out.ExitCode) -LogType ([LogType]::Info)
			
			$results = Select-String "running scripts is disabled on this system" -InputObject $output
			if ($results) {
				LogMessage -Message ('The uninstall script failed to execute on the VM {0}' -f $VMName) -LogType ([LogType]::Error)
				$ret = 1
				continue
			}
			
			$results = Select-String "Microsoft ASR Mobility Service/Master Target Server is not installed" -InputObject $output
			if ($results) {
				LogMessage -Message ("Agent is not installed on {0}" -f $VMName) -LogType ([LogType]::Info)
				continue             
			}
			
			$results = Select-String "ASRUnifiedAgent Uninstallation exited with return code: 0" -InputObject $output
			if (!$results) {
				LogMessage -Message ('Agent uninstallation has failed for the VM {0}' -f $VMName) -LogType ([LogType]::Error)
				$ret = 1
				continue
			}
			
			LogMessage -Message ('Rebooting the VM {0}' -f $VMName) -LogType ([LogType]::Info)
			$command = "shutdown -r -f"
			Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		}
		
		if ($source.OsType -match "Linux") {
			$command = "ls /"
			$out = Invoke-VMScript -VM $vm -ScriptText $command -ScriptType Bash -GuestUser $Username -GuestPassword $Password
			if ($out.ExitCode -ne 0) {
				LogMessage -Message ("Failed to login to the VM {0}" -f $VMName) -LogType ([LogType]::Error)
				continue
			}
			
			LogMessage -Message ("Login to the VM {0} successful" -f $VMName) -LogType ([LogType]::Info)
			
			$command = "ls /usr/local/.vx_version"
			$out = Invoke-VMScript -VM $vm -ScriptText $command -ScriptType Bash -GuestUser $Username -GuestPassword $Password
			if ($out.ExitCode -ne 0) {
				LogMessage -Message ("Agent is not installed on {0}, skipping uninstallation..." -f $VMName) -LogType ([LogType]::Info)
				continue
			}
			
			LogMessage -Message ("Agent bits are installed on the VM {0}" -f $VMName) -LogType ([LogType]::Info)
			
			$command = "/usr/local/ASR/uninstall.sh -Y"
			$out = Invoke-VMScript -VM $vm -ScriptText $command -ScriptType Bash -GuestUser $Username -GuestPassword $Password
					
			LogMessage -Message ("The return value from uninstaller is {0} and console output is {1}" -f $out.ExitCode, $out.ScriptOutput) -LogType ([LogType]::Info)
			if ($out.ExitCode -ne 0) {
				$ret = 1
				LogMessage -Message ("Failed to uninstall the agent from VM {0}" -f $VMName) -LogType ([LogType]::Error)
				Copy-VMGuestFile -VM $vm -GuestToLocal -GuestUser $Username -GuestPassword $Password -Source '/var/log/ua_uninstall.log' -Destination $global:LogDir
				$output = Get-Content $($global:LogDir+"\ua_uninstall.log")
				LogMessage -Message ("The un-installation log is as follows: {0}" -f ($output | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
				#$output | Out-File $global:LogName -Append
			}
		}
	}
	
	if ($ret -eq 1) {
		LogMessage -Message ("Failed to uninstall the agent on one or more VMs" -f $VMName) -LogType ([LogType]::Error)
	}
	
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Uninstall Linux Mater Target
#>
Function UninstallLinuxMT()
{
	LogMessage -Message ('Uninstalling Linux MT...') -LogType ([LogType]::Info)
	
	if ($global:MasterTargetList -eq $null) {
		LogMessage -Message ('No Master Target found') -LogType ([LogType]::Info)
		$global:ReturnValue = $true
		return 0
	}
	
	# Adding snap in to read VMWare PowerClI Commands through power shell
	Add-PSSnapin "VMware.VimAutomation.Core" | Out-Null
<#
	if (!$?) {
		LogMessage -Message ('Unable to snap-in VMware.VimAutomation.Core to the current session') -LogType ([LogType]::Error)
		exit 1
	}
#>
	# Connecting to vCenter Server
	Connect-VIServer -Server $global:VcenterIP -User $global:VcenterUserName -Password $global:VcenterPassword -Port $global:VcenterPort
	if (!$global:DefaultVIServers.IsConnected) {
		LogMessage -Message ('Failed to connect to the vCenter server, IPAddress: {0}, User: {1}, Password: {2} and Port: {3}' -f $global:VcenterIP, $global:UserName, $global:Password, $global:VcenterPort) -LogType ([LogType]::Error)
		exit 1
	}
	
	LogMessage -Message ("Connection to the vCenter server {0} is successful" -f $global:VcenterIP) -LogType ([LogType]::Info)
	
	$ret = 0
	foreach($Mt in $global:MasterTargetList)
	{
		if ($Mt.OsType -ne "Linux") {
			continue
		}
		
		$VMName = $Mt.FriendlyName
		$Username = $Mt.UserName
		$Password = $Mt.Password
		
		$vm = Get-VM -Name $VMName
		if (!$?) {
			LogMessage -Message ("The MT VM {0} doesn't exist" -f $VMName) -LogType ([LogType]::Error)
			$ret = 1
			break
		}
		
		$command = "ls /usr/local/.vx_version"
		$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		if ($out.ExitCode -ne 0) {
			LogMessage -Message ("MT is not installed with agent bits, skipping uninstallation") -LogType ([LogType]::Info)
			break
		}
		
		$command = "/usr/local/ASR/uninstall.sh -Y"
		$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
				
		LogMessage -Message ("The return value from uninstaller is {0} and console output is {1}" -f $out.ExitCode, $out.ScriptOutput) -LogType ([LogType]::Info)
		if ($out.ExitCode -ne 0) {
			$ret = 1
			LogMessage -Message ("Failed to uninstall the Linux MT") -LogType ([LogType]::Error)
			Copy-VMGuestFile -VM $vm -GuestToLocal -GuestUser $Username -GuestPassword $Password -Source '/var/log/ua_uninstall.log' -Destination $global:LogDir
			break
		}
		
		break
	}
	
	Disconnect-VIServer -server  $global:DefaultVIServers -force -confirm:$false -ErrorAction SilentlyContinue
	LogMessage -Message ("Disconnected the vCenter") -LogType ([LogType]::Info)
	
	if ($ret -eq 1) {
		exit 1
	}
	
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Remove Vcenter associated with CS
#>
Function RemoveVcenter()
{
	LogMessage -Message ('Removing vCenter...') -LogType ([LogType]::Info)
	if (!$global:VaultFile) {
		$vaultFile = $global:Testbinroot + "\" + $global:VaultName + ".VaultCredentials"
		if (!(Test-Path $vaultFile)) {
			LogMessage -Message ('The vault credential file {0} is missing...Skipping vCenter removal' -f $vaultFile) -LogType ([LogType]::Info)
			$global:ReturnValue = $true
			return
		}
		
		$global:VaultFile = $vaultFile
	}
	
	LoginToAzure
	if (!$?) {
		LogMessage -Message ("Failed to login to Azure") -LogType ([LogType]::Error)
		exit 1
	}
	
	Import-AzRecoveryServicesAsrVaultSettingsFile -Path $global:VaultFile
	if (!$?) {
        LogMessage -Message ('Failed to import vault : {0}' -f $global:VaultFile) -LogType ([LogType]::Error)
        exit 1
    }
	
    $fabricObject = Get-AzRecoveryServicesAsrFabric -FriendlyName $global:CSPSName
	$fabricObject
    if ($null -ne $fabricObject) {
        # Get Vcenter details
        $vCenter = Get-AzRecoveryServicesAsrvCenter -Fabric $fabricObject -Name $global:VcenterName
		$vCenter

        if ($null -ne $vCenter.FriendlyName) {
            LogMessage -Message ("Removing Vcenter : {0}" -f $global:VcenterName) -LogType ([LogType]::Info)
            $job = Remove-AzRecoveryServicesAsrvCenter -InputObject $vCenter
            WaitForJobCompletion -JobId $job.name
            LogMessage -Message ("Removed Vcenter") -LogType ([LogType]::Info)
        }
    }
	
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Uninstallation of Configuration Server
#>
Function UninstallCS()
{
	LogMessage -Message ('Uninstalling CS...') -LogType ([LogType]::Info)
	
	if (!$global:VaultFile) {
		$vaultFile = $global:Testbinroot + "\" + $global:VaultName + ".VaultCredentials"
		if (!(Test-Path $vaultFile)) {
			LogMessage -Message ('The vault credential file {0} is missing...Skipping fabric removal' -f $vaultFile) -LogType ([LogType]::Info)
		} else {
			$global:VaultFile = $vaultFile
		}
	}
	
	if ($global:VaultFile) {
		LoginToAzure
		if (!$?) {
			LogMessage -Message ("Failed to login to Azure") -LogType ([LogType]::Error)
			exit 1
		}
	
		Import-AzRecoveryServicesAsrVaultSettingsFile -Path $global:VaultFile
		if ($?) {
			$fabricObject = Get-AzRecoveryServicesAsrFabric -FriendlyName $global:CSPSName
			if ($fabricObject -ne $null) {
				LogMessage -Message ('Found ASR Fabric {0}' -f $fabricObject) -LogType ([LogType]::Info)
				$job = Remove-AzRecoveryServicesAsrFabric -Fabric $fabricObject
				WaitForJobCompletion -JobId $job.name
				LogMessage -Message ("Removed Fabric") -LogType ([LogType]::Info)
			}
		}
	}
	
	$secpasswd = ConvertTo-SecureString $global:CSPassword -AsPlainText -Force
	$credential = New-Object System.Management.Automation.PSCredential($global:CSUserName, $secpasswd)
	$session = New-PSSession -ComputerName $global:CSIPAddress -Credential $credential
	if ($session -eq $null) {
		LogMessage -Message ('Unable to connect to CS {0} using the credentials of username {1}' -f $global:CSPSName, $global:CSUserName) -LogType ([LogType]::Error)
		LogMessage -Message ('Uninstallation of CS/PS has failed') -LogType ([LogType]::Error)
		exit 1
	}
	
	Invoke-Command -Session $session -ScriptBlock {
		powershell.exe "C:\V2AGQL\CSPSUninstaller.ps1" > C:\V2AGQL\output.txt
	}
	
	$output = Invoke-Command -Session $session -ScriptBlock {Get-Content "C:\V2AGQL\output.txt"}
	LogMessage -Message ('The output of uninstallation of CS/PS is as follows: {0}' -f ($output | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
	#$output | Out-File $global:LogName -Append
	$results = Select-String "failed" -InputObject $output
	If ($results) {
		LogMessage -Message ('Uninstallation of CS/PS has failed') -LogType ([LogType]::Error)
		exit 1
	}
	
	LogMessage -Message ('Uninstallation of CS/PS has completed') -LogType ([LogType]::Info)
	
	LogMessage -Message ('Rebooting the CS/PS VM {0}' -f $global:CSPSName) -LogType ([LogType]::Info)
	Restart-Computer -ComputerName $global:CSPSName -Credential $credential -Force
	
	$retries = 1
	do {
		LogMessage -Message ('Waiting for 30 seconds for the CS/PS VM {0} to come-up...retry - {1}' -f $global:CSPSName, $retries) -LogType ([LogType]::Info)
		Start-Sleep -Seconds 30
		$session = New-PSSession -ComputerName $global:CSIPAddress -Credential $credential
		if ($session) {
			break
		}
		
		$retries++;
	} while ($retries -le 10)
	
	if ($retries -gt 10) {
		LogMessage -Message ('Failed to reboot the CS/PS VM {0}' -f $global:CSPSName) -LogType ([LogType]::Error)
		exit 1
	} else {
		LogMessage -Message ('Successfully rebboted the CS/PS VM {0}' -f $global:CSPSName) -LogType ([LogType]::Info)
	}
	
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Copies unified setup, Linux unified agent for MT, MySQL password file, etc., to the TDM
#>
Function CopyBuildToTDM()
{
	LogMessage -Message ("Copying builds to TDM...") -LogType ([LogType]::Info)

	& NET USE Z: /D
	& NET USE Z: '\\INMSTAGINGSVR.corp.microsoft.com\DailyBuilds\Daily_Builds' /user:$global:FareastUser $global:FareastPass
	if (!$?) {
		LogMessage -Message ("Failed to access the shared path \\INMSTAGINGSVR.corp.microsoft.com") -LogType ([LogType]::Error)
		exit 1
	}

	if (!($BuildType -match "Custom")) {
		$global:BuildLocation = "Z:\9.*"
		
		$devBuild=0
		$relBuild=0
		Get-ChildItem $global:BuildLocation -Directory |
		Foreach-Object {
			$minorBuildNum = $_.BaseName.split(".")[1]
			if ($minorBuildNum -gt $devBuild) {
				$relBuild = $devBuild
				$devBuild = $minorBuildNum
			} else {
				if ($minorBuildNum -gt $relBuild) {
					$relBuild = $minorBuildNum
				}
			}
		}
		
		LogMessage -Message ("Dev Build = 9.{0} and Rel Build = 9.{1}" -f $devBuild, $relBuild) -LogType ([LogType]::Info)
		if ($BuildType -match "Develop") {
			$global:BuildLocation = "Z:\9." + $devBuild + "\Host"
		} else {
			$global:BuildLocation = "Z:\9." + $relBuild + "\Host"
		}
		
		$buildDate=$null
		Get-ChildItem $global:BuildLocation -Directory |
		Foreach-Object {
			$directory = $_.BaseName
			$date = Get-Date $directory.replace("_", "-")
			if (!$?) {
				LogMessage -Message ("Not a valid build directory {0}...skipping" -f $directory) -LogType ([LogType]::Info)
				return
			}

			if ($buildDate -eq $null) {
				$buildDate = $directory
			} else {
				$date1 = Get-Date $buildDate.replace("_", "-")
				$date2 = Get-Date $directory.replace("_", "-")
				
				$diff = New-TimeSpan -Start $date1 -end $date2
				if ($diff.Days -gt 0) {
					$buildDate = $directory
				}
			}
		}
		
		if ($buildDate -eq $null) {
			LogMessage -Message ("Build location {0} doesn't contain any valid build date directory" -f $global:BuildLocation) -LogType ([LogType]::Error)
			& NET USE Z: /D
			exit 1
		}
		
		$global:BuildLocation = $global:BuildLocation + "\" + $buildDate
	} else {
		$tokens = $global:BuildLocation.split("\")
		$buildDate = $tokens[$tokens.Count - 1]
	}
	
	if (!(Test-Path $global:BuildLocation)) {
		LogMessage -Message ("Build location {0} doesn't exist" -f $global:BuildLocation) -LogType ([LogType]::Error)
		& NET USE Z: /D
		exit 1
	}

	LogMessage -Message ( "Build location is {0} " -f $global:BuildLocation) -LogType ([LogType]::Info)
	
	if ($global:IsReportingEnabled -eq $true) {
		TestStatusLog $LoggerContext "BuildDate" $buildDate.Replace("_", "-")
	}

	$TDMDestLocation = $global:Testbinroot
	if ( !(Test-Path $TDMDestLocation)) {
		LogMessage -Message ("The location {0} doesn't exist on TDM" -f $TDMDestLocation) -LogType ([LogType]::Error)
		& NET USE Z: /D
		exit 1
	}

    Remove-Item $($TDMDestLocation+"\"+"MicrosoftAzureSiteRecoveryUnifiedSetup.exe")
	$file = $global:BuildLocation + "\release\" + "MicrosoftAzureSiteRecoveryUnifiedSetup.exe"
	$Files = @($file)
	$LoggerContext = TestStatusGetContext $global:Id $global:Scenario $global:ReportingTableName
	
	Remove-Item $($TDMDestLocation+"\"+$global:LinuxMTUAFile)
	$file = $global:BuildLocation + "\UnifiedAgent_Builds\release\" + $global:LinuxMTUAFile
	$Files += $file

	LogMessage -Message ("Copying following files to TDM location {0}: {1}" -f $TDMDestLocation, ($Files | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
	#$Files | Out-File $global:LogName -Append
	for ($i=0; $i -lt $Files.Count; $i++) {
		$file = $Files[$i]
		if ( !(Test-Path $file)) {
			LogMessage -Message ("The file {0} doesn't exist" -f $file) -LogType ([LogType]::Error)
			& NET USE Z: /D
			exit 1
		}
		
		Copy-Item $file -Destination $TDMDestLocation -ErrorAction stop -force
		$ret = $?
		LogMessage -Message ("Copy file {0} return value $ret" -f $file) -LogType ([LogType]::Info)
		if (!$ret) {
			LogMessage -Message ("Failed to copy the file {0} to {1}" -f $file, $TDMDestLocation) -LogType ([LogType]::Error)
			& NET USE Z: /D
			exit 1
		}
	}
	
	& NET USE Z: /D
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Copy scripts, binaries and build to CS
#>
Function CopyScriptsToCS()
{
	LogMessage -Message ("Copying of binaries CS...") -LogType ([LogType]::Info)
	$secureString = New-Object -TypeName System.Security.SecureString
	$global:CSPassword.ToCharArray() | ForEach-Object { $secureString.AppendChar($_) }
	$credential = New-Object -TypeName System.Management.Automation.PSCredential -argumentlist $global:CSUserName, $secureString
	
	$networkPath = "\\" + $global:CSIPAddress + "\c$"
	LogMessage -Message ("Network path : {0}" -f $networkPath) -LogType ([LogType]::Info)
	
	Remove-PSDrive -Name P
	New-PSDrive -Name P -PSProvider FileSystem -Persist -Root $networkPath -Credential $credential
	if (!$?) {
		LogMessage -Message ("The C drive on CS is not accessible for copying the scripts") -LogType ([LogType]::Error)
		exit 1
	}

	$Path = "P:\V2AGQL"
	Remove-Item $Path -Recurse -Force
	New-Item $Path -Type directory
	$Files = @("MySQLPasswordFile.txt", "csgetfingerprint.exe", "root.pfx", "VMware-PowerCLI-6.0.0-3056836.exe", "MicrosoftAzureSiteRecoveryUnifiedSetup.exe", "DownloadVaultFile.ps1", "CSPSUninstaller.ps1", "AddAccounts.exe")
	
	for ($i=0; $i -lt $Files.length; $i++) {
		$file = $global:Testbinroot + "\" + $Files[$i]
		if ( !(Test-Path $file)) {
			LogMessage -Message ("The file {0} doesn't exist" -f $file) -LogType ([LogType]::Error)
			exit 1
		}
		
		Copy-Item $file -Destination $Path -ErrorAction stop -force
		$ret = $?
		LogMessage -Message ("Copy file {0} return value $ret" -f $file) -LogType ([LogType]::Info)
		if (!$ret) {
			LogMessage -Message ("Failed to copy the file {0} to {1}" -f $file, $Path) -LogType ([LogType]::Error)
			exit 1
		}
	}
	
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Downalod the vault credential file
#>
Function DownloadVaultFile()
{
	LogMessage -Message ("Downloading of vault file has started...") -LogType ([LogType]::Info)
	$secpasswd = ConvertTo-SecureString $global:CSPassword -AsPlainText -Force
	$credential = New-Object System.Management.Automation.PSCredential($global:CSUserName, $secpasswd)
	$session = New-PSSession -ComputerName $global:CSIPAddress -Credential $credential
	if ($session -eq $null) {
		LogMessage -Message ('Unable to connect to CS {0} using the credentials of username {1}' -f $global:CSPSName, $global:CSUserName) -LogType ([LogType]::Error)
		LogMessage -Message ('Download of vault file failed') -LogType ([LogType]::Error)
		exit 1
	}
	
	$CSLocation = "C:\V2AGQL"
	$command = $CSLocation + "\DownloadVaultFile.ps1 " + " -SubscriptionName " + "'" + $global:SubscriptionName + "'" + " -VaultName " + $global:VaultName + " -ResourceGroupName " + $global:ResourceGroup + " -Testbinroot " + $CSLocation + " -LogFile DownloadVaultFile.log" + " -ApplicationId '" + $global:ApplicationId + "' -TenantId '" + $global:TenantId + "'"
	LogMessage -Message ("Command = $command") -LogType ([LogType]::Info)
	$output = Invoke-Command -Session $session -ScriptBlock {
	param(
		$command
	)
		powershell.exe $command
	}-ArgumentList $command
	
	LogMessage -Message ("The log from the download vault file operation is " -f ($output | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
	#$output | Out-File $global:LogName -Append

	$secureString = New-Object -TypeName System.Security.SecureString
	$global:CSPassword.ToCharArray() | ForEach-Object { $secureString.AppendChar($_) }
	$credential = New-Object -TypeName System.Management.Automation.PSCredential -argumentlist $global:CSUserName, $secureString
	
	$networkPath = "\\" + $global:CSIPAddress + "\c$"
	LogMessage -Message ("Network path : {0}" -f $networkPath) -LogType ([LogType]::Info)
	
	Remove-PSDrive -Name P
	New-PSDrive -Name P -PSProvider FileSystem -Persist -Root $networkPath -Credential $credential
	if (!$?) {
		LogMessage -Message ("The C drive on CS is not accessible for getting the logs of vault download") -LogType ([LogType]::Error)
		exit 1
	}

	$Path = "P:\V2AGQL"
	$output = Get-Content $($Path + "\DownloadVaultFile.log")
	LogMessage -Message ('The output of vault download is as follows: {0}' -f ($output | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
	#$output | Out-File $global:LogName -Append
	$results = Select-String "Successfully downloaded the vault credential file" -InputObject $output
	If (!$results) {
		LogMessage -Message ('Vault dowload has failed') -LogType ([LogType]::Error)
		Start-Sleep -s 30
		exit 1
	}
	
	$VaultFile = $global:Testbinroot + "\" + $global:VaultName + ".VaultCredentials"
	if (Test-Path $VaultFile) {
		Remove-Item $VaultFile
	}
	
	$file = $Path + "\" + $global:VaultName + ".VaultCredentials"
	Copy-Item $file -Destination $VaultFile -ErrorAction stop -force
	LogMessage -Message ("Vault file copied to {0}" -f $VaultFile) -LogType ([LogType]::Info)
	
	& NET USE Z: /D
	& NET USE Z: $global:SharedLocation /user:$global:FareastUser $global:FareastPass
	if (!$?) {
		LogMessage -Message ("Failed to access the shared path {0}" -f $global:SharedLocation) -LogType ([LogType]::Error)
		exit 1
	}
	Copy-Item $VaultFile -Destination 'Z:\' -ErrorAction stop -force
	& NET USE Z: /D
	
	$node = $global:xmlDoc.V2AGQLConfig.AzureFabric.SelectSingleNode("VaultFile")
	$node.AppendChild($global:xmlDoc.CreateTextNode($($global:SharedLocation+"\"+$global:VaultName+".VaultCredentials")))
	
	$node = $global:xmlDoc.V2AGQLConfig.Deployment.SelectSingleNode("VaultFile")
	$node.AppendChild($global:xmlDoc.CreateTextNode($VaultFile))

	$global:xmlDoc.Save($global:configFile)
	LogMessage -Message ("Successfully downloaded vault file") -LogType ([LogType]::Info)
	
	$global:ReturnValue = $true
}
	
<#
Function DownloadVaultFile()
{
	LogMessage -Message ("Downloading of vault file has started...") -LogType ([LogType]::Info)

	LoginToAzure
	if (!$?) {
		LogMessage -Message ("Failed to login to Azure") -LogType ([LogType]::Error)
		exit 1
	}

	LogMessage -Message ("Connecting to vault : {0}" -f $global:VaultName) -LogType ([LogType]::Info)
	$vault = Get-AzRecoveryServicesVault -Name $global:VaultName -ResourceGroupName $global:ResourceGroup
	LogMessage -Message ("Vault : {0}" -f $vault) -LogType ([LogType]::Info)

	if (!$vault) {
		LogMessage -Message ("Faied to connect to the vault using vault name {0} and resource group (1)" -f $global:VaultName, $global:ResourceGroup) -LogType ([LogType]::Error)
		exit 1
	}

	$dt = $(Get-Date).ToString("M-d-yyyy")
	$cert = New-SelfSignedCertificate -DnsName $($vault.Name+$global:SubscriptionId+'-'+$dt+'-vaultcredentials') -CertStoreLocation cert:\CurrentUser\My -Type 'DocumentEncryptionCert'
	if ($cert -eq $null) {
		LogMessage -Message ("Faied to generate the certificate") -LogType ([LogType]::Error)
		exit 1
	}
	
	LogMessage -Message ("cert: {0}" -f $cert) -LogType ([LogType]::Info)
	$certficate = [Convert]::ToBase64String($cert.RawData)
	LogMessage -Message ("certficate: {0}" -f $certficate) -LogType ([LogType]::Info)
	
	Set-AzRecoveryServicesAsrVaultContext -Vault $vault
	$file = Get-AzRecoveryServicesVaultSettingsFile -SiteRecovery -Vault $vault -Certificate $certficate.ToString()
	
	if ($cert -eq $null) {
		LogMessage -Message ("Faied to generate the certificate") -LogType ([LogType]::Error)
		exit 1
	}
	
	LogMessage -Message ("cert: {0}" -f $cert) -LogType ([LogType]::Info)
	$keyValue = [Convert]::ToBase64String($cert.RawData)
	LogMessage -Message ("certficate: {0}" -f $certficate) -LogType ([LogType]::Info)
	$file = Get-AzRecoveryServicesVaultSettingsFile -SiteRecovery -Vault $vault -Certificate $keyValue 
	if ((!$file) -or (!(Test-Path $file.FilePath))) {
		LogMessage -Message ("Faied to download the vault file") -LogType ([LogType]::Error)
		exit 1
	}
		
	$downloadPath = $file.FilePath
	LogMessage -Message ("Downloaded Vault file : {0}" -f $downloadPath)

	$VaultFile = $global:Testbinroot + "\" + $VaultName + ".VaultCredentials" 
	Remove-Item $VaultFile

	$xmlVaultFile = [System.Xml.XmlDocument](Get-Content $downloadPath)
	$xmlVaultFile.RSVaultAsrCreds.ChannelIntegrityKey

	Move $downloadPath -Destination $VaultFile -Force
	LogMessage -Message ("Vault file copied to {0}" -f $VaultFile) -LogType ([LogType]::Info)
	Copy-Item $VaultFile -Destination $global:SharedLocation -ErrorAction stop -force
	
	$node = $global:xmlDoc.V2AGQLConfig.AzureFabric.SelectSingleNode("VaultFile")
	$node.AppendChild($global:xmlDoc.CreateTextNode($($global:SharedLocation+"\"+$VaultName+".VaultCredentials")))
	
	$node = $global:xmlDoc.V2AGQLConfig.Deployment.SelectSingleNode("VaultFile")
	$node.AppendChild($global:xmlDoc.CreateTextNode($VaultFile))

	$global:xmlDoc.Save($global:configFile)
	LogMessage -Message ("Successfully downloaded vault file") -LogType ([LogType]::Info)
}
#>

<#
.SYNOPSIS
    Installs the unified setup on CS
#>
Function InstallCS()
{
	LogMessage -Message ("Installing CS...") -LogType ([LogType]::Info)
	$OutputFile = "C:\V2AGQL\out"
	$secpasswd = ConvertTo-SecureString $global:CSPassword -AsPlainText -Force
	$credential = New-Object System.Management.Automation.PSCredential($global:CSUserName, $secpasswd)
	$session = New-PSSession -ComputerName $global:CSIPAddress -Credential $credential
	
	$DraReg = 0
	do {
		Invoke-Command -Session $session -ScriptBlock  {
		param(
			$VaultName,
			$OutputFile
		)
			$ExtractionDir = "C:\V2AGQL"
			
			Remove-Item $OutputFile -Recurse -Force
			
			$cmd = $ExtractionDir + '\MicrosoftAzureSiteRecoveryUnifiedSetup.exe' + ' /q /x:' + $ExtractionDir
			$cmd | Out-File $OutputFile
			cmd.exe /c $cmd
			Start-Sleep -s 20
			
			$cmd = $ExtractionDir + "\UNIFIEDSETUP.EXE /AcceptThirdpartyEULA /Servermode 'CS' /InstallLocation 'C:\Program Files (x86)\Microsoft Azure Site Recovery' /MySQLCredsFilePath " + "'" + $($ExtractionDir + "\MySQLPasswordFile.txt") + "'" + ' /VaultCredsFilePath ' + "'" + $($ExtractionDir + "\" + $VaultName + ".VaultCredentials") + "'" + ' /EnvType "VMWare" /SkipSpaceCheck'
			$cmd | Out-File $OutputFile -Append
			powershell.exe $cmd >> $OutputFile
		}-ArgumentList $global:VaultName, $OutputFile

		$output = Invoke-Command -Session $session -ScriptBlock {Get-Content $OutputFile}
		LogMessage -Message ('The CS installation log is as follows: {0}' -f ($output | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
		#$output | Out-File $global:LogName -Append
		$results = Select-String "System has a pending restart" -InputObject $output
		if ($results)
		{
			LogMessage -Message ('Restarting the CS system as it has one pending reboot') -LogType ([LogType]::Info)
			Restart-Computer -ComputerName $global:CSPSName -Credential $credential -Force
	
			$retries = 1
			do {
				LogMessage -Message ('Waiting for 30 seconds for the CS/PS VM {0} to come-up...retry - {1}' -f $global:CSPSName, $retries) -LogType ([LogType]::Info)
				Start-Sleep -Seconds 30
				$session = New-PSSession -ComputerName $global:CSPSName -Credential $credential
				if ($session) {
					break
				}
				
				$retries++;
			} while ($retries -le 10)
			
			if ($retries -gt 10) {
				LogMessage -Message ('Failed to reboot the CS/PS VM {0}' -f $global:CSPSName) -LogType ([LogType]::Error)
			} else {
				LogMessage -Message ('Successfully rebboted the CS/PS VM {0}' -f $global:CSPSName) -LogType ([LogType]::Info)
				continue
			}
		}
		$results = Select-String "Failed to register with Site Recovery" -InputObject $output
		if (($results) -and ($DraReg -eq 0))
		{
			$DraReg = 1
			LogMessage -Message ('Retrying the CS installation as it fails to regoister with Site Recovery') -LogType ([LogType]::Info)
			continue
		}
		
		$results = Select-String "failed" -InputObject $output
		if ($results)
		{
			LogMessage -Message ('CS Builds installation failed') -LogType ([LogType]::Error)
			exit 1
		}
		
		break
	} while(1)
		
	LogMessage -Message ('CSPS installation has succeeded') -LogType ([LogType]::Info)

	$passphrase = Invoke-Command -Session $session -ScriptBlock {cmd.exe /c '"C:\Program Files (x86)\Microsoft Azure Site Recovery\home\svsystems\bin\genpassphrase.exe" -n'}
	if (!$passphrase) {
		LogMessage -Message ('Failed to fetch the passphrase') -LogType ([LogType]::Error)
		exit 1
	}
	
	LogMessage -Message ('Passphrase: {0}' -f $passphrase) -LogType ([LogType]::Info)
	$node = $global:xmlDoc.V2AGQLConfig.InMageFabric.ConfigServer.SelectSingleNode("Passphrase")
	$node.AppendChild($global:xmlDoc.CreateTextNode($passphrase))
	$global:xmlDoc.Save($global:configFile)
	LogMessage -Message ('Inserted passphrase') -LogType ([LogType]::Info)
	
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Adds an account either for adding vCenter or for protecting a VM
#>
Function AddAccountOnCS()
{
	LoginToAzure
	if (!$?) {
		LogMessage -Message ("Failed to login to Azure") -LogType ([LogType]::Error)
		exit 1
	}
	
    Import-AzRecoveryServicesAsrVaultSettingsFile -Path $global:VaultFile
	if (!$?) {
        LogMessage -Message ('Failed to import vault : {0}' -f $global:VaultFile) -LogType ([LogType]::Error)
        exit 1
    }
	
	$secpasswd = ConvertTo-SecureString $global:CSPassword -AsPlainText -Force
	$credential = New-Object System.Management.Automation.PSCredential($global:CSUserName, $secpasswd)
	$session = New-PSSession -ComputerName $global:CSIPAddress -Credential $credential
	if (!$session) {
		LogMessage -Message ("Failed to create a session with CS {0}" -f $global:CSPSName) -LogType ([LogType]::Error)
		exit 1
	}
	$session
	$command = "cd C:\V2AGQL; .\AddAccounts.exe --csip " + $global:CSIPAddress + " --csport " + $global:CSHttpsPort + " --friendlyname " + $global:VcenterName + " --user " + $global:VcenterUserName + " --pass " + $global:VcenterPassword
	LogMessage -Message ("Command to execute on CS for adding account for vCenter is {0}" -f $command) -LogType ([LogType]::Info)
	
	$out = Invoke-Command -Session $session -ScriptBlock  {
	param(
		$command
	)
		powershell.exe $command
	}-ArgumentList $command 

	LogMessage -Message ("The output from add account exe is {0}" -f $out) -LogType ([LogType]::Info)
	
	if (!($out.contains("Successfully added user"))) {
		LogMessage -Message ("Failed to add the account for vCenter") -LogType ([LogType]::Error)
		exit 1
	}
	
	LogMessage -Message ("Successfully added the account for vCenter") -LogType ([LogType]::Info)
	
	$retries = 1
	$found = 0
	do {
		$fabricObject = Get-AzRecoveryServicesAsrFabric -FriendlyName $global:CSPSName
		if ($fabricObject -eq $null) {
			break
		}
		
		if ($fabricObject.FabricSpecificDetails.RunAsAccounts.Count -ne 0) {
			$found = 1
			break
		}
		
		LogMessage -Message ("Refreshing CS..Retry {0}" -f $retries) -LogType ([LogType]::Info)
		RefreshCS
		#Start-Sleep -Seconds 30
		$retries++;
	} while ($retries -le 40)
	
	write-host "retries = $retries"
	
	if ($found -eq 0) {
		LogMessage -Message ("The added account is not getting listed in the fabric") -LogType ([LogType]::Error)
		exit 1
	}
	
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Vcenter discovery
#>
Function AddVcenter()
{
	LoginToAzure
	if (!$?) {
		LogMessage -Message ("Failed to login to Azure") -LogType ([LogType]::Error)
		exit 1
	}
	
	Import-AzRecoveryServicesAsrVaultSettingsFile -Path $global:VaultFile
	if (!$?) {
        LogMessage -Message ('Failed to import vault : {0}' -f $global:VaultFile) -LogType ([LogType]::Error)
        exit 1
    }
	
	$global:CSPSName
	$fabricObject = Get-AzRecoveryServicesAsrFabric -FriendlyName $global:CSPSName
	$fabricObject
    Assert-NotNull($fabricObject)
	Assert-NotNull($fabricObject.FabricSpecificDetails.RunAsAccounts)
    $vcenterAccount = $fabricObject.FabricSpecificDetails.RunAsAccounts | Where-Object { $_.AccountName -match $global:VcenterName }

    # Add Vcenter
    LogMessage -Message ("Adding Vcenter : {0}" -f $global:VcenterName) -LogType ([LogType]::Info)
    $job = New-AzRecoveryServicesAsrvCenter -Account $vcenterAccount -Fabric $fabricObject -IpOrHostName $global:VcenterIP -Name $global:VcenterName -Port $global:VcenterPort
    WaitForJobCompletion -JobId $job.name
	LogMessage -Message ("Added Vcenter") -LogType ([LogType]::Info)

    # Get Vcenter details
    $vCenter = Get-AzRecoveryServicesAsrvCenter -Fabric $fabricObject -Name $global:VcenterName
    Assert-NotNull($vCenter)
	
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Install Agent bits on Linux MT
#>
Function InstallLinuxMT()
{
	LogMessage -Message ('Installing Linux MT...') -LogType ([LogType]::Info)
	if ($global:MasterTargetList -eq $null) {
		LogMessage -Message ('No Master Target found') -LogType ([LogType]::Info)
		$global:ReturnValue = $true
		return 0
	}
	
	# Adding snap in to read VMWare PowerClI Commands through power shell
	Add-PSSnapin "VMware.VimAutomation.Core" | Out-Null
<#
	if (!$?) {
		LogMessage -Message ('Unable to snap-in VMware.VimAutomation.Core to the current session') -LogType ([LogType]::Error)
		exit 1
	}
#>	

	# Connecting to vCenter Server
	Connect-VIServer -Server $global:VcenterIP -User $global:VcenterUserName -Password $global:VcenterPassword -Port $global:VcenterPort
	if (!$global:DefaultVIServers.IsConnected) {
		LogMessage -Message ('Failed to connect to the vCenter server, IPAddress: {0}, User: {1}, Password: {2} and Port: {3}' -f $global:VcenterIP, $global:UserName, $global:Password, $global:VcenterPort) -LogType ([LogType]::Error)
		exit 1
	}
	
	LogMessage -Message ("Connection to the vCenter server {0} is successful" -f $global:VcenterIP) -LogType ([LogType]::Info)
	
	$ret = 0
	foreach($Mt in $global:MasterTargetList)
	{
		if ($Mt.OsType -ne "Linux") {
			continue
		}
		
		$VMName = $Mt.FriendlyName
		$Username = $Mt.UserName
		$Password = $Mt.Password
		
		$vm = Get-VM -Name $VMName
		if (!$?) {
			LogMessage -Message ("The MT VM {0} doesn't exist" -f $VMName) -LogType ([LogType]::Error)
			$ret = 1
			break
		}
		
		$VMBuildPath = "/home/mt_build"
		$passphraseFile = $VMBuildPath + "/passphrase.txt"

		$command = 'rm -f /var/log/ua_*'
		LogMessage -Message ('Invoking command {0}' -f $command) -LogType ([LogType]::Info)
		Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		
		$command = "rm -rf " + $VMBuildPath + "; mkdir -p " + $VMBuildPath
		LogMessage -Message ('Invoking command {0}' -f $command) -LogType ([LogType]::Info)
		Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		
		$command = 'echo ' + $global:Passphrase + '> ' + $passphraseFile
		LogMessage -Message ('Invoking command {0}' -f $command) -LogType ([LogType]::Info)
		$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		if ($out.ExitCode -ne 0) {
			$ret = 1
			LogMessage -Message ("The passphrase file {0} creation failed" -f $passphraseFile) -LogType ([LogType]::Error)
			break
		}
		
		$LinuxMTUAFile = $global:Testbinroot + "\" + $global:LinuxMTUAFile
		LogMessage -Message ('Copying {0} to {1}' -f $LinuxMTUAFile, $VMBuildPath) -LogType ([LogType]::Info)
		Copy-VMGuestFile -VM $vm -LocalToGuest -GuestUser $Username -GuestPassword $Password -Source $LinuxMTUAFile -Destination $VMBuildPath
		$command = "ls " + $VMBuildPath + "/" + $global:LinuxMTUAFile
		LogMessage -Message ('Invoking command {0}' -f $command) -LogType ([LogType]::Info)
		$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		if ($out.ExitCode -ne 0) {
			$ret = 1
			LogMessage -Message ("The copy of Linux Unified setup {0} to {1} failed with error {2} and output {3}" -f $global:LinuxMTUAFile, $VMBuildPath, $out.ExitCode, $out.ScriptOutput) -LogType ([LogType]::Error)
			break
		}
		
		$command = "cd " + $VMBuildPath + "; tar -xvf " + $global:LinuxMTUAFile
		LogMessage -Message ('Invoking command {0}' -f $command) -LogType ([LogType]::Info)
		$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		if ($out.ExitCode -ne 0) {
			$ret = 1
			LogMessage -Message ("The extraction of Linux Unified setup {0} has failed with error {1} and output {2}" -f $global:LinuxMTUAFile, $out.ExitCode, $out.ScriptOutput) -LogType ([LogType]::Error)
			break
		}
		
		$command = 'cd ' + $VMBuildPath + '; ./ApplyCustomChanges.sh <<<Y'
		LogMessage -Message ('Invoking command {0}' -f $command) -LogType ([LogType]::Info)
		$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		
		if ($out.ExitCode -ne 0) {
			$ret = 1
			LogMessage -Message ("ApplyCustomChanges.sh failed with the following output: {0}" -f $out.ScriptOutput) -LogType ([LogType]::Error)
			break
		}
		
		$command = 'reboot'
		LogMessage -Message ('Invoking command {0}' -f $command) -LogType ([LogType]::Info)
		$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		$retries = 10
		do {
			$command = "cd " + $VMBuildPath
			$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
			if ($out.ExitCode -eq 0) {
				break
			}
			LogMessage -Message ("Waiting for the MT {0} to come up" -f $VMName) -LogType ([LogType]::Info)
			Start-Sleep -Seconds 30
			$retries -= 1
		} While ($retries -gt 0)
		
		if ($retries -eq 0) {
			$ret = 1
			LogMessage -Message ("The Linux MT {0} is not accessible after reboot." -f $VMName) -LogType ([LogType]::Error)
			break
		}
		
		LogMessage -Message ("Installation of agent has started") -LogType ([LogType]::Info)
		$command = 'cd ' + $VMBuildPath + '; ./install -q -d /usr/local/ASR -r MT -v VmWare'
		LogMessage -Message ('Invoking command {0}' -f $command) -LogType ([LogType]::Info)
		$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		if ($out.ExitCode -ne 0) {
			$ret = 1
			LogMessage -Message ("Installation has failed with error code {0}. Installer log file ua_install.log is available at {1}" -f $out.ExitCode, $global:LogDir) -LogType ([LogType]::Error)
			Copy-VMGuestFile -VM $vm -GuestToLocal -GuestUser $Username -GuestPassword $Password -Source '/var/log/ua_install.log' -Destination $global:LogDir
			break
		}
		
		LogMessage -Message ("Configurator has started") -LogType ([LogType]::Info)
		$command = '/usr/local/ASR/Vx/bin/UnifiedAgentConfigurator.sh -i ' + $global:CSIPAddress + ' -P ' + $passphraseFile
		LogMessage -Message ('Invoking command {0}' -f $command) -LogType ([LogType]::Info)
		$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		if ($out.ExitCode -ne 0) {
			$ret = 1
			LogMessage -Message ("Configurator has failed with error code {0}. Log file ua_install.log is available at {1} {2}" -f $out.ExitCode, $global:LogDir, $out.ScriptOutput) -LogType ([LogType]::Error)
			Copy-VMGuestFile -VM $vm -GuestToLocal -GuestUser $Username -GuestPassword $Password -Source '/var/log/ua_install.log' -Destination $global:LogDir
			break
		}
		Start-Sleep -Seconds 1000
		break
	}
	
	Disconnect-VIServer -server  $global:DefaultVIServers -force -confirm:$false -ErrorAction SilentlyContinue
	LogMessage -Message ("Disconnected the vCenter") -LogType ([LogType]::Info)
	
	if ($ret -eq 1) {
		exit 1
	}
	
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
Main Block
#>
Function Main() {
	try {
		switch ($Operation.ToUpper()) {
			'INITREPORTING' { $op = $null; InitializeReporting; break }
			'DISABLEALLPROTECTIONSANDREMOVEPOLICIES' { $op = "DRPALL"; DisableAllProtectionsAndRemovePolicies; break }
			'STARTSOURCEVMS' { $op = "RebootSVMs"; StartSourceVms; break }
			'CLEANUPRG' { $op = "CleanupRG"; CleanupResourceGroup; break }
			'UNINSTALLAGENTS' { $op = "UninstallAgents"; UninstallAgents; break }
			'UNINSTALLLINUXMT' { $op = "UninstallMT"; UninstallLinuxMT; break }
			'DELVCENTER' { $op = "RemoveVCenter"; RemoveVcenter; break }
			'UNINSTALLCS' { $op = "UinstallCS"; UninstallCS; break }
			'COPYBUILDTOTDM' { $op = "CopyBuilds"; CopyBuildToTDM; break }
			'COPYSCRIPTSTOCS' { $op = "CopyCSScripts"; CopyScriptsToCS; break }
			'DOWNLOADVAULTFILE' { $op = "DownloadVault"; DownloadVaultFile; break }
			'INSTALLCS' { $op = "InstallCS"; InstallCS; break}
			'ADDACCOUNTONCS' { $op = "AddAccount"; AddAccountOnCS; break }
			'ADDVCENTER' { $op = "AddVCenter"; AddVcenter; break }
			'INSTALLLINUXMT' { $op = "InstallMT"; InstallLinuxMT; break }
			default { break }
		}
	}
	catch
	{
		LogMessage -Message ("ERROR:: {0}" -f ($Error | ConvertTo-json -Depth 1)) -LogType ([LogType]::Error)
		LogMessage -Message ("V2A GQL Deployment Failed") -LogType ([LogType]::Error)
		[DateTime]::Now.ToString() + "Exception caught - " + $_.Exception.Message | Out-File $global:LogName -Append
		[DateTime]::Now.ToString() + "Failed Item - " + $_.Exception.ItemName | Out-File $global:LogName -Append
		$errorMessage = "GQL DEPLOYMENT FAILED"
        throw $errorMessage      
    }
	finally {
		if (($global:IsReportingEnabled -eq $true) -and ($op)) {
			ReportOperationStatus -OperationName $op
		}
	}
}

CreateLogDir
$global:LogName = "$global:LogDir\$global:Operation`_{0}.log" -f (Get-Date -Format "MM-dd-yyyy HH.mm.ss")


Main