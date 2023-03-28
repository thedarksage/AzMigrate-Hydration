<#
.SYNOPSIS
    Powershell script to perform End To End Testing for V2A

.DESCRIPTION
    The script is used to perform EnableProtection, TFO, UFO, SwitchProtection, Failback, DisableProtection operations for V2A.

.PARAMETER Operation
    Mandatory parameter, it indicates the operation to be performed on the VM.
    Allowed values for Operation are 'CLEANUP', 'DELFABRIC', 'DELVCENTER', 'VALIDATE' , 'ADDVCENTER', 'ER', 'TFO', 'UFO', 'SP', 'FB', 'RSP', 'DISABLE', 'DRP', 'UPDATE', 'RUN540', 'RUN360', 'RUN180'

.EXAMPLE
./V2AGQLTest.ps1 "ER"
    ER(EnableReplication) operation performs Enable Protection on the On-Prem VM

.EXAMPLE
./V2AGQLTest.ps1 "TFO"
    TFO(TestFailover) operation triggers test failover on the Primary VM

.EXAMPLE
./V2AGQLTest.ps1 "UFO"
    UFO(UnplannedFailover) operation triggers failover on the Primary VM

.EXAMPLE
./V2AGQLTest.ps1 "SP"
    SP(SwitchProtection) operation triggers SwitchProtection on the UFO VM

.EXAMPLE
./V2AGQLTest.ps1 "FB"
    FB(Failback) operation triggers Failback

.EXAMPLE
./V2AGQLTest.ps1 "DISABLE"
    DISABLE operation disables the protection on the Primary VM and removes all the azure artifacts created in the earlier operations.

.EXAMPLE
./V2AGQLTest.ps1 "DRP"
    DRP(DisableReplicationPolicy) operation removes all the mappings and replicaiton policies associated with the vault

.EXAMPLE
./V2AGQLTest.ps1 "RSP"
    RSP(ReverseSwitchProtection) operation triggers Reverse Switch Protection from Primary -> Recovery

.EXAMPLE
./V2AGQLTest.ps1 "DELFABRIC"
    DELFABRIC(DeleteFabric) opearation removes the fabrics associated with the vault

.EXAMPLE
./V2AGQLTest.ps1 "DELVCENTER"
    DELVCENTER(DeleteVCenter) operation removes the Vcenter associated with the CS

.EXAMPLE
./V2AGQLTest.ps1 "VALIDATE"
    VALIDATE operation validates the fabric associated to the vault. Always run this operation before triggering ER operation to validate if the fabric has all the mappings or not.

.EXAMPLE
./V2AGQLTest.ps1 ADDACCOUNTONCS
	ADDACCOUNTONCS operation adds the user account

.EXAMPLE
./V2AGQLTest.ps1 "ADDVCENTER"
    ADDVCENTER operation performs Vcenter discovery

.EXAMPLE
./V2AGQLTest.ps1 "CLEANUP"
    CLEANUP operation unmaps all the existing fabrics and containers from the vault

.EXAMPLE
./V2AGQLTest.ps1 "UPDATE"
    UPDATE operation will upgrade the mobility agent on the source vm

.EXAMPLE
./V2AGQLTest.ps1 "RUN540"
    Run540 operation will trigger 540 V2A GQL run on the Primary VM.

.EXAMPLE
./V2AGQLTest.ps1 "RUN360"
    Run360 operation will trigger 360 V2A GQL run on the Primary VM.

.EXAMPLE
./V2AGQLTest.ps1 "RUN180"
    Run180 operation will trigger 180 V2A GQL run on the Primary VM.

.NOTES
    Below are the prerequisites required to run any operation using this script. These steps(1-6) will be done as part of deployment workflow from WTT.
        1. Install CS, On-Prem PS(s), Azure PS, Linux MT.
        2. Add account for Vcenter/ESX discovery with vcenter/esx name as account name.
        3. Add account for source VM with source hostname as account name.
        4. Add Vcenter
        5. Copy/downlaod the vault file to local path on TDM
        6. Update V2AGQLConfig.xml with valid on-prem(CS, PS, Linux MT), Azure fabric details(Subscription, ResourceGroup, Vault, Vnet) and vault file path.
        7. Run 'VALIDATE' operation to check if all the prerequisites are met or not.
    Each operation can be run independently.
#>

# Input parameters
param (
    [Parameter(Mandatory = $true)]
    [ValidateSet("ER", "TFO", "UFO", "SP", "FB", "DISABLE", "RSP", "CR", "DRP", "VALIDATE", "ADDACCOUNTONCS", "ADDVCENTER", "DELVCENTER", "CLEANUP", "UPDATE", "RUN540", 'RUN360', 'RUN180', IgnoreCase = $true)]
    [String] $Operation
)

#
# Include common library files
#
$Error.Clear()
. $PSScriptRoot\CommonFunctions.ps1

#region Global Variables
$global:Operation = $Operation.ToUpper()

$global:Id = $global:SourceFriendlyName
$global:Scenario = "V2A_GQL"
$todaydate = Get-Date -Format dd_MM_yyyy
$global:ReportingTableName = "Daily_" + $todaydate
$Op = $null
#endregion Global Variables

<#
.SYNOPSIS
    Validates if the agent is installed or not
#>
Function IsAgentUninstalled() {
	if($global:source -eq $null) {
		LogMessage -Message ('No source server found') -LogType ([LogType]::Error)
		Throw "No source server found"
	}
	
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
		Throw "Failed to connect to the vCenter server"
	}
	
	LogMessage -Message ("Connection to the vCenter server {0} is successful" -f $global:VcenterIP) -LogType ([LogType]::Info)
	
	$VMName = $global:SourceFriendlyName
	$Username = $global:SourceUsername
	$Password = $global:SourcePassword
	$OsType = $global:SourceOSType
		
	LogMessage -Message ("Checking if agent on VM {0} is uninstalled or not" -f $VMName) -LogType ([LogType]::Info)
		
	$vm = Get-VM -Name $VMName
	if (!$?) {
		LogMessage -Message ("The source VM {0} doesn't exist" -f $VMName) -LogType ([LogType]::Error)
		exit 1
	}
	
	LogMessage -Message ("Power state for VM {0} is {1}" -f $VMName, $vm.PowerState) -LogType ([LogType]::Info)
	
	if ($OsType -match "Windows") {
		$command = "ls c:\"
		$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		if (!$?) {
			LogMessage -Message ("Unable to login to the VM {0} using the creds {1}/{2}" -f $VMName, $Username, $Password) -LogType ([LogType]::Error)
			exit 1
		}
           
		LogMessage -Message ("The output of command {0} is as follows: {1}" -f $command, ($out.ScriptOutput | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
		LogMessage -Message ("Login to the VM {0} successful" -f $VMName) -LogType ([LogType]::Info)
		
		$command = "Remove-Item C:\V2AGQL\CSPSUninstaller.ps1"
		Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		$command = "mkdir C:\V2AGQL"
		Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		Get-Item $($global:Testbinroot+"\CSPSUninstaller.ps1") | Copy-VMGuestFile -Destination "C:\V2AGQL" -VM $vm -LocalToGuest -GuestUser $Username -GuestPassword $Password
		
		$command = "powershell.exe C:\V2AGQL\CSPSUninstaller.ps1 -Role 'Agent' -AbortIfAgentInstalled $true > C:\V2AGQL\output.txt"
		$out = Invoke-VMScript -VM $vm -ScriptText $command -GuestUser $Username -GuestPassword $Password
		$ret=$out.ExitCode
		LogMessage -Message ("The return value from uninstaller for VM {0} is {1}" -f $VMName, $ret) -LogType ([LogType]::Info)
		Copy-VMGuestFile -VM $vm -GuestToLocal -GuestUser $Username -GuestPassword $Password -Source 'C:\V2AGQL\output.txt' -Destination $global:LogDir
		$output = Get-Content $($global:LogDir+"\output.txt")
		LogMessage -Message ("The output is as follows: {0}" -f ($output | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
		#$output | Out-File $global:LogName -Append
		Remove-Item $($global:LogDir+"\output.txt")
		LogMessage -Message ("The return value is {0}" -f $out.ExitCode) -LogType ([LogType]::Info)
			
		$results = Select-String "running scripts is disabled on this system" -InputObject $output
		if ($results) {
			LogMessage -Message ('The uninstall script failed to execute on the VM {0}' -f $VMName) -LogType ([LogType]::Error)
			exit 1
		}
			
		$results = Select-String "Microsoft ASR Mobility Service/Master Target Server is not installed" -InputObject $output
		if ($results) {
			LogMessage -Message ("Agent is not installed on {0}" -f $VMName) -LogType ([LogType]::Info)
			return
		}
		
		return
#		LogMessage -Message ("Uninstall the agent on VM {0}" -f $VMName) -LogType ([LogType]::Error)
#		exit 1
	}
		
	if ($OsType -match "Linux") {
		$command = "ls /"
		$out = Invoke-VMScript -VM $vm -ScriptText $command -ScriptType Bash -GuestUser $Username -GuestPassword $Password
		if ($out.ExitCode -ne 0) {
			LogMessage -Message ("Failed to login to the VM {0}" -f $VMName) -LogType ([LogType]::Error)
			exit 1
		}
			
		LogMessage -Message ("Login to the VM {0} successful" -f $VMName) -LogType ([LogType]::Info)
			
		$command = "ls /usr/local/.vx_version"
		$out = Invoke-VMScript -VM $vm -ScriptText $command -ScriptType Bash -GuestUser $Username -GuestPassword $Password
		if ($out.ExitCode -ne 0) {
			LogMessage -Message ("Agent is not installed on {0}, skipping uninstallation..." -f $VMName) -LogType ([LogType]::Info)
			return
		}
		
		LogMessage -Message ("Uninstall the agent on VM {0}" -f $VMName) -LogType ([LogType]::Error)
		exit 1
	}
	
	LogMessage -Message ("OS type of VM {0} is {1}, expecting either Windows or Linux" -f $VMName, $OsType) -LogType ([LogType]::Error)
	exit 1
}

Function ValidateIfAgentUninstalled
{
    # Read V2A GQL Configuration File Settings
    $global:configFile = $PSScriptRoot + "\V2AGQLConfig.xml"
    $global:xmlDoc = [System.Xml.XmlDocument](Get-Content $global:configFile)

    $node = $global:xmlDoc.V2AGQLConfig.InMageFabric.SourceServerList | Select-Object -ExpandProperty childnodes | Where-Object {$_.Name -eq $global:SourceFriendlyName}
    LogObject $node
    if ($node.UninstallStatus -ne "Pass")
    {
        Throw "Older agent exists on Source VM, Please uninstall it and retry the operation!!!!" 
    }
}

<#
.SYNOPSIS
    Site Recovery Fabric Validation
#>
Function ValidateFabric() {
	$Op = "ValidateFabric"
    # Validate Azure fabric details
    $fabric = Get-AzRecoveryServicesAsrFabric -FriendlyName $global:CSPSName
    Assert-NotNull($fabric)
    Assert-NotNull($fabric.FriendlyName)
    Assert-NotNull($fabric.name)
    Assert-NotNull($fabric.ID)
    Assert-NotNull($fabric.FabricSpecificDetails)
    LogObject -Object $fabric
    
    $fabricDetails = $fabric.FabricSpecificDetails
    Assert-NotNull($fabricDetails.HostName)
    Assert-NotNull($fabricDetails.IpAddress)
    Assert-NotNull($fabricDetails.AgentVersion)
    Assert-NotNull($fabricDetails.ProtectedServers)
    Assert-NotNull($fabricDetails.LastHeartbeat)
    Assert-NotNull($fabricDetails.ProcessServers)
    Assert-NotNull($fabricDetails.MasterTargetServers)
    Assert-NotNull($fabricDetails.RunAsAccounts)
    Assert-NotNull($fabricDetails.IpAddress)
    LogObject -Object $fabricDetails
    
    # Validate ProcessServer details
    $processServers = $fabricDetails.ProcessServers
    foreach ($ps in $processServers) {
        Assert-NotNull($ps.FriendlyName)
        Assert-NotNull($ps.HostId)
        Assert-NotNull($ps.IpAddress)
        LogMessage -Message ("ProcessServer = {0}, Version= {1}" -f $ps.FriendlyName, $ps.AgentVersion) -LogType ([LogType]::Info)
        LogObject -Object $ps
    }

    # Validate MasterTarget details
    $masterTargets = $fabricDetails.MasterTargetServers
    foreach ($mt in $masterTargets) {
        Assert-NotNull($mt.Name)
        Assert-NotNull($mt.Datastores)
        Assert-NotNull($mt.Id)
        Assert-NotNull($mt.IpAddress)
        Assert-NotNull($mt.OsType)
        Assert-NotNull($mt.RetentionVolumes)
        LogMessage -Message ("MT = {0}, Version = {1}" -f $mt.Name, $mt.AgentVersion) -LogType ([LogType]::Info)
        LogObject -Object $mt
    }

    # Validate RunAccounts
    $vcenterAccount = $fabricDetails.RunAsAccounts | Where-Object { $_.AccountName -match $global:VcenterName }
    Assert-NotNull($vcenterAccount.AccountId)
    Assert-NotNull($vcenterAccount.AccountName)
    LogObject -Object $vcenterAccount

    $sourceAccount = $fabricDetails.RunAsAccounts | Where-Object { $_.AccountName -match $global:SourceFriendlyName }
    Assert-NotNull($sourceAccount.AccountId)
    Assert-NotNull($sourceAccount.AccountName)
    LogObject -Object $sourceAccount

    $protectionContainer = Get-AzRecoveryServicesAsrProtectionContainer -Fabric $fabric -FriendlyName $global:CSPSName
    Assert-NotNull($protectionContainer)
    Assert-NotNull($protectionContainer.id)
    LogObject -Object $protectionContainer
    
    if ($protectionContainer.FabricType -ne "VMware") {
        LogMessage -Message ("Invalid FabricType {0} is found" -f $protectionContainer.FabricType) -LogType ([LogType]::Error)
        Throw "Invalid fabrictype found"
    }
	
	# Validate if the agent on source system is uninstalled or not
	ValidateIfAgentUninstalled
	
    LogMessage -Message ("Fabric {0} validation PASSED" -f $global:CSPSName) -LogType ([LogType]::Info)
	$global:ReturnValue = $true
	if (($global:IsReportingEnabled -eq $true) -and ($op)) {
		ReportOperationStatus -OperationName $Op
	}
}

<#
.SYNOPSIS
    Remove Vcenter associated with CS
#>
Function AddAccountOnCS()
{
	$Op = "AddAccount"
	$secpasswd = ConvertTo-SecureString $global:CSPassword -AsPlainText -Force
	$credential = New-Object System.Management.Automation.PSCredential($global:CSUserName, $secpasswd)
	$session = New-PSSession -ComputerName $global:CSIPAddress -Credential $credential
	if (!$session) {
		LogMessage -Message ("Failed to create a session with CS {0}" -f $global:CSPSName) -LogType ([LogType]::Error)
		exit 1
	}
	$session
	$command = "cd C:\V2AGQL; .\AddAccounts.exe --csip " + $global:CSIPAddress + " --csport " + $global:CSHttpsPort + " --friendlyname " + $global:SourceFriendlyName + " --user " + $global:SourceUsername + " --pass " + $global:SourcePassword
	LogMessage -Message ("Command to execute on CS for adding account for source VM is {0}" -f $command) -LogType ([LogType]::Info)
	
	$out = Invoke-Command -Session $session -ScriptBlock  {
	param(
		$command
	)
		powershell.exe $command
	}-ArgumentList $command 

	LogMessage -Message ("The output from add account exe is:") -LogType ([LogType]::Info)
	$out | Out-File $global:LogName -Append
	if (!($out.contains("Successfully added user"))) {
		LogMessage -Message ("Failed to add the account for VM {0}" -f $global:SourceFriendlyName) -LogType ([LogType]::Error)
		exit 1
	}
	
	LogMessage -Message ("Successfully added the account for VM {0}" -f $global:SourceFriendlyName) -LogType ([LogType]::Info)
	
	$retries = 1
	$found = 0
	do {
		$fabricObject = Get-AzRecoveryServicesAsrFabric -FriendlyName $global:CSPSName
		if ($fabricObject -eq $null) {
			break
		}
		
		$fabricObject.FabricSpecificDetails.RunAsAccounts
		$output = $fabricObject.FabricSpecificDetails.RunAsAccounts | Where-Object { $_.AccountName -match $global:SourceFriendlyName }
		if ($output) {
			$found = 1
			break
		}
		
		LogMessage -Message ("Refreshing CS..Retry {0}" -f $retries) -LogType ([LogType]::Info)
		RefreshCS
		Start-Sleep -Seconds 300
		$retries++;
	} while ($retries -le 3)
	
	write-host "retries = $retries"
	
	if ($found -eq 0) {
		LogMessage -Message ("The added account is not getting listed in the fabric") -LogType ([LogType]::Error)
		exit 1
	}
	
	$global:ReturnValue = $true
	if (($global:IsReportingEnabled -eq $true) -and ($op)) {
		ReportOperationStatus -OperationName $Op
	}
}

<#
.SYNOPSIS
    Remove Vcenter associated with CS
#>
Function RemoveVcenter() {
    $fabricObject = Get-AzRecoveryServicesAsrFabric -FriendlyName $global:CSPSName

    if ($null -ne $fabricObject) {
        # Get Vcenter details
        $vCenter = Get-AzRecoveryServicesAsrvCenter -Fabric $fabricObject -Name $global:VcenterName

        if ($null -ne $vCenter.FriendlyName) {
            LogMessage -Message ("Removing Vcenter : {0}" -f $global:VcenterName) -LogType ([LogType]::Info)
            $job = Remove-AzRecoveryServicesAsrvCenter -InputObject $vCenter
            WaitForJobCompletion -JobId $job.name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
            LogMessage -Message ("Removed Vcenter") -LogType ([LogType]::Info)
        }
    }
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Remove Fabric mappings from vault
#>
Function RemoveFabric() {
    $fabricObject = Get-AzRecoveryServicesAsrFabric -FriendlyName $global:CSPSName

    if ($null -ne $fabricObject.FriendlyName) {
        LogMessage -Message ("Removing Fabric : {0}" -f $fabricObject.FriendlyName) -LogType ([LogType]::Info)
        $job = Remove-AzRecoveryServicesAsrFabric -InputObject $fabricObject -Force
        WaitForJobCompletion -JobId $job.name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
        LogMessage -Message ("Removed Fabric") -LogType ([LogType]::Info)
    }
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Vcenter discovery
#>
Function AddVcenter() {
    $fabricObject = Get-AzRecoveryServicesAsrFabric -FriendlyName $global:CSPSName
    Assert-NotNull($fabricObject)

    $vcenterAccount = $fabricObject.FabricSpecificDetails.RunAsAccounts | Where-Object { $_.AccountName -match $global:VcenterName }

    # Add Vcenter
    LogMessage -Message ("Adding Vcenter : {0}" -f $global:VcenterName) -LogType ([LogType]::Info)
    $job = New-AzRecoveryServicesAsrvCenter `
        -Account $vcenterAccount `
        -Fabric $fabricObject `
        -IpOrHostName $global:VcenterIP `
        -Name $global:VcenterName `
        -Port $global:VcenterPort
    WaitForJobCompletion -JobId $job.name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
    LogMessage -Message ("Added Vcenter") -LogType ([LogType]::Info)

    # Get Vcenter details
    $vCenter = Get-AzRecoveryServicesAsrvCenter -Fabric $fabricObject -Name $global:VcenterName
    Assert-NotNull($vCenter)
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Enable Protection
#>
Function EnableReplication() {
	$Op = "ER_180"
    $fabricObject = Get-AzRecoveryServicesAsrFabric -FriendlyName $global:CSPSName
    Assert-NotNull($fabricObject)

    $protContainerObject = Get-AzRecoveryServicesAsrProtectionContainer -Fabric $fabricObject -FriendlyName $global:CSPSName
    Assert-NotNull($protContainerObject)

    # Before starting run try to Unpair Container from earlier run if exist.
    $containerMappingObject = Get-AzRecoveryServicesAsrProtectionContainerMapping `
        -ProtectionContainer $protContainerObject `
    | Where-Object { $_.PolicyFriendlyName -match $global:ReplicationPolicy }
LogMessage -Message ("Found {0} protection mappings." -f $containerMappingObject.Count) -LogType ([LogType]::Info)
foreach ($pcMapping in $containerMappingObject) {
    LogMessage -Message ("Started Container Unpairing before run starts.") -LogType ([LogType]::Info)
    $currentJob = Remove-AzRecoveryServicesAsrProtectionContainerMapping -InputObject $pcMapping
    LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name, $currentJob.State) -LogType ([LogType]::Info)
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
    LogMessage -Message ("*****Containers: {0} Unpaired before run starts*****" -f $pcMapping.Name) -LogType ([LogType]::Info)
}

# Removing replication policy from earlier run if exist.
$policyObject = Get-AzRecoveryServicesAsrPolicy -Name $global:ReplicationPolicy -ErrorAction SilentlyContinue
    
if ($policyObject.Count -gt 0) {
    LogMessage -Message ("Removing Replication policy: {0} before run starts." -f $policyObject.Name) -LogType ([LogType]::Info)
    $currentJob = Remove-AzRecoveryServicesAsrPolicy -InputObject $policyObject
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
    LogMessage -Message ("*****Policy Removed*****") -LogType ([LogType]::Info)
}

# Create Policy
LogMessage -Message ("Triggering replication policy creation: {0}" -f $global:ReplicationPolicy) -LogType ([LogType]::Info)
$currentJob = New-AzRecoveryServicesAsrPolicy `
    -Name $global:ReplicationPolicy `
    -RecoveryPointRetentionInHours $global:RetentionWindowsInHrs `
    -RPOWarningThresholdInMinutes $global:RPOThresholdInMins `
    -VMwareToAzure `
    -ApplicationConsistentSnapshotFrequencyInHours $global:AppConsistencySnapshotInHrs
LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name, $currentJob.State) -LogType ([LogType]::Info)
WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
LogMessage -Message ("*****Replication policy created*****") -LogType ([LogType]::Info)

$policyObject = Get-AzRecoveryServicesAsrPolicy -Name $global:ReplicationPolicy
Assert-NotNull($policyObject)
LogMessage -Message ("Replication policy: {0} fetched successfully." -f $policyObject.Name) -LogType ([LogType]::Info)

# Create container mapping(Cloud Pairing) [primary -> recovery]
LogMessage -Message ("Triggering Container Pairing: {0}" -f $global:ForwardContainerMappingName) -LogType ([LogType]::Info)
$currentJob = New-AzRecoveryServicesAsrProtectionContainerMapping `
    -Name $global:ForwardContainerMappingName `
    -Policy $policyObject `
    -PrimaryProtectionContainer $protContainerObject
LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name, $currentJob.State) -LogType ([LogType]::Info)
WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
LogMessage -Message ("*****Container paired*****") -LogType ([LogType]::Info)

# Update Protection container objects after Container pairing
$protContainerObject = Get-AzRecoveryServicesAsrProtectionContainer -Fabric $fabricObject
Assert-NotNull($protContainerObject)
LogMessage -Message ("Primary Container: {0}, Role: {1}, AvailablePolicies: {2}" -f 
    $protContainerObject.FriendlyName,
    $protContainerObject.Role, 
    $protContainerObject.AvailablePolicies.FriendlyName ) -LogType ([LogType]::Info)

$forwardContainerMappingObject = Get-AzRecoveryServicesAsrProtectionContainerMapping `
    -Name $global:ForwardContainerMappingName `
    -ProtectionContainer $protContainerObject `
| Where-Object { $_.PolicyFriendlyName -match $global:ReplicationPolicy }
Assert-NotNull($forwardContainerMappingObject)
LogMessage -Message ("Container Mapping: {0}, State: {1}" -f `
        $forwardContainerMappingObject.Name, $forwardContainerMappingObject.State) -LogType ([LogType]::Info)

### Enable Replication
LogMessage -Message ("Triggering Enable Protection: {0} for VM: {1}" -f `
        $enableDRName, $global:SourceFriendlyName) -LogType ([LogType]::Info)
$psAccount = $fabricObject.FabricSpecificDetails.ProcessServers `
| Where-Object { $_.FriendlyName -match $global:CSPSName }
$vmAccount = $fabricObject.FabricSpecificDetails.RunAsAccounts `
| Where-Object { $_.AccountName -eq $global:SourceFriendlyName }

$pi = Get-AzRecoveryServicesAsrProtectableItem `
    -ProtectionContainer $protContainerObject `
    -FriendlyName $global:SourceFriendlyName `
    | Where-Object { $_.FabricSpecificVMDetails.DiscoveryType -match "vCenter" }
Assert-NotNull($pi)

$currentJob = New-AzRecoveryServicesAsrReplicationProtectedItem `
    -VmwareToAzure `
    -ProtectableItem $pi `
    -Name $global:RpiName `
    -ProtectionContainerMapping $forwardContainerMappingObject `
    -RecoveryAzureStorageAccountId $global:RecStorageAccountId `
    -RecoveryResourceGroupId  $global:RecResourceGroupId `
    -ProcessServer $psAccount `
    -Account $vmAccount `
    -RecoveryAzureNetworkId $global:RecoveryVnetId `
    -RecoveryAzureSubnetName $global:RecoverySubnet

WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTime60Seconds

$job = Get-AzRecoveryServicesAsrJob -Job $currentJob
LogMessage -Message ('Job target object id : {0}, target object name : {1}' -f `
        $job.TargetObjectId, $job.TargetObjectName) -LogType ([LogType]::Info)

$ProtectedItems = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $protContainerObject
LogMessage -Message ('Protected VMs information is as follows:') -LogType ([LogType]::Info)
$ProtectedItems | Out-File $global:LogName -Append
		
#WaitForIRCompletion -VMName $job.TargetObjectName -ProtectionDirection "VmwareToAzure"-JobQueryWaitTimeInSeconds $JobQueryWaitTime60Seconds

# Wait for VM state to change to protected
WaitForProtectedItemState -state "Protected" -VMName $global:SourceFriendlyName -containerObject $protContainerObject

LogMessage -Message ("Enable Protection completed successfully") -LogType ([LogType]::Info)
$global:ReturnValue = $true
if (($global:IsReportingEnabled -eq $true) -and ($op)) {
	ReportOperationStatus -OperationName $Op
}
}

<#
.SYNOPSIS
    Update mobility agent on source vm
#>
Function UpdateMobilityAgent {
    # Get Replication Protected Item
    $fabricInfo = GetFabricInfo
    $rpi = $fabricInfo.RPI
    
    LogMessage -Message ("Updating Mobility Agent on : {0}" -f $global:SourceFriendlyName) -LogType ([LogType]::Info)
    $job = Update-AzRecoveryServicesAsrMobilityService -ReplicationProtectedItem $rpi -Account $fabricInfo.VMAccount
    WaitForJobCompletion -JobId $Job.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTime60Seconds
    LogMessage -Message ("Update mobility agent is completed successfully") -LogType ([LogType]::Info)
	$global:ReturnValue = $true
}

<#
.SYNOPSIS
    Triggers Test Failover for source VM
#>
Function TestFailover {
	$Op = "TFO"
    # Get Replication Protected Item
    $fabricInfo = GetFabricInfo
    $rpi = $fabricInfo.RPI
	
    # Get tag
    #TODO: Remove the default latest crash tag recovery and make it configurable
    $recoveryPoint = GetRecoveryPoint `
        -protectedItemObject $rpi `
        -tagType ([TagType]::CrashConsistent)

    # TFO
    LogMessage -Message ("Triggering TFO for VM: {0}, Id: {1}" -f $rpi.FriendlyName, $rpi.Name) -LogType ([LogType]::Info)
    $currentJob = Start-AzRecoveryServicesAsrTestFailoverJob `
        -AzureVMNetworkId $global:RecoveryVnetId `
        -Direction PrimaryToRecovery `
        -ReplicationProtectedItem $rpi `
        -RecoveryPoint $recoveryPoint
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTime60Seconds
    LogMessage -Message ("*****TFO Completed*****") -LogType ([LogType]::Info)

    # Check VM Status
    $VMName = $rpi.FriendlyName + "-test"
    WaitForAzureVMProvisioningAndPowerState -vmName $VMName -resourceGroupName $global:RecResourceGroup

    # Complete TFO
    LogMessage -Message ("Triggering TFO Cleanup") -LogType ([LogType]::Info)
    $cleanupJob = Start-AzRecoveryServicesAsrTestFailoverCleanupJob -ReplicationProtectedItem $rpi -Comment "TFO cleanup"
    WaitForJobCompletion -JobId $cleanupJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTime60Seconds
    LogMessage -Message ("*****TFO Cleanup Completed*****") -LogType ([LogType]::Info)
	$global:ReturnValue = $true
	if (($global:IsReportingEnabled -eq $true) -and ($op)) {
		ReportOperationStatus -OperationName $Op
	}
}

<#
.SYNOPSIS
    Triggers Unplanned Failover for source VM
#>
Function UnPlannedFailover {
	$Op = "FO_180"
    # Get Replication Protected Item
    $fabricInfo = GetFabricInfo
    $rpi = $fabricInfo.RPI

    # UFO
    #TODO: Remove the default latest crash tag recovery and make it configurable
    LogMessage -Message ("Triggering UFO for VM: {0}, Id: {1}" -f $rpi.FriendlyName, $rpi.Name) -LogType ([LogType]::Info)
    $currentJob = Start-AzRecoveryServicesAsrUnplannedFailoverJob `
        -Direction PrimaryToRecovery `
        -RecoveryTag LatestAvailableCrashConsistent `
        -ReplicationProtectedItem $rpi `
        -PerformSourceSideAction
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTime60Seconds

    # Check Recovered VM Status
    WaitForAzureVMProvisioningAndPowerState -vmName $rpi.FriendlyName -resourceGroupName $global:RecResourceGroup
    LogMessage -Message ("*****Validation finished on UFO VM*****") -LogType ([LogType]::Info)

    LogMessage -Message ("Triggering Commit after UFO for VM: {0}, Id: {1}" -f $rpi.FriendlyName, $rpi.Name) -LogType ([LogType]::Info)
    $currentJob = Start-AzRecoveryServicesAsrCommitFailoverJob -ReplicationProtectedItem $rpi
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTime60Seconds
    LogMessage -Message ("*****UFO finished*****") -LogType ([LogType]::Info)
	$global:ReturnValue = $true
	if (($global:IsReportingEnabled -eq $true) -and ($op)) {
		ReportOperationStatus -OperationName $Op
	}
}

<#
.SYNOPSIS
    Triggers Switch Protection
#>
Function SwitchProtection {
	$Op = "ER_360"
    # Create Policy
    LogMessage -Message ("Triggering reverse replication policy creation: {0}" -f $global:ReverseReplicationPolicy) -LogType ([LogType]::Info)
    $currentJob = New-AzRecoveryServicesAsrPolicy `
        -Name $global:ReverseReplicationPolicy `
        -RecoveryPointRetentionInHours $global:RetentionWindowsInHrs `
        -RPOWarningThresholdInMinutes $global:RPOThresholdInMins `
        -AzureToVmware `
        -ApplicationConsistentSnapshotFrequencyInHours $global:AppConsistencySnapshotInHrs
    LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name, $currentJob.State) -LogType ([LogType]::Info)
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
    LogMessage -Message ("*****Replication policy created*****") -LogType ([LogType]::Info)

    # Sleeping for 45mins, so that VM status will be updated
    LogMessage -Message ("Sleeping for {0} sec" -f ($JobQueryWaitTime60Seconds * 45)) -LogType ([LogType]::Info)
    Start-Sleep -Seconds ($JobQueryWaitTime60Seconds * 45)
    
    # Get Fabric object
    $fabricInfo = GetFabricInfo
    $rpi = $fabricInfo.RPI
    $protectionContainer = $fabricInfo.ProtectionContainer
    $mtAccount = $fabricInfo.mtAccount
	
    $policyObject = Get-AzRecoveryServicesAsrPolicy -Name $global:ReverseReplicationPolicy
    Assert-NotNull($policyObject)
    LogMessage -Message ("Replication policy: {0} fetched successfully." -f $policyObject.Name) -LogType ([LogType]::Info)

    # Create Reverse Container Mapping(Cloud Pairing) [recovery -> primary]
    LogMessage -Message ("Triggering Reverse Container Mapping(Cloud Pairing) [recovery -> primary]:: {0}" -f `
            $global:ReverseContainerMappingName) -LogType ([LogType]::Info)
    $currentJob = New-AzRecoveryServicesAsrProtectionContainerMapping `
        -Name $global:ReverseContainerMappingName `
        -Policy $policyObject `
        -PrimaryProtectionContainer $protectionContainer `
        -RecoveryProtectionContainer $protectionContainer
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds

    # Get reverse ContainerMapping Object
    $reverseContainerMappingObject = Get-AzRecoveryServicesAsrProtectionContainerMapping `
        -Name $global:ReverseContainerMappingName `
        -ProtectionContainer $protectionContainer
    Assert-NotNull($reverseContainerMappingObject)

    $mtObj = $global:MTList | Where-Object { $_.FriendlyName -eq $mtAccount.Name }
    $mtDataStore = $mtAccount.DataStores | Where-Object { $_.SymbolicName -eq $mtObj.DataStore }
    $retVolume = $mtAccount.RetentionVolumes | Where-Object { $_.volumeName -eq $mtObj.Retention }
    LogMessage -Message ("Artifacts : MTName : {0}, MTDatastore : {1}, Retention Volume : {2}" -f $mtAccount.Name, $mtDataStore.SymbolicName, $retVolume.VolumeName) -LogType ([LogType]::Info)
    
    if ($null -ne $fabricInfo.azPSAccount) {
        $psAccount = $fabricInfo.azPSAccount
    }
    else {
        $psAccount = $fabricInfo.psAccount
    }
    LogMessage -Message ("Using PS {0} for reprotect" -f $psAccount.FriendlyName)

    LogMessage -Message ("Triggering Switch protection") -LogType ([LogType]::Info)	
    $currentJob = Update-AzRecoveryServicesAsrProtectionDirection `
        -AzureToVmware `
        -Account $fabricInfo.vmAccount `
        -DataStore $mtDataStore  `
        -Direction RecoveryToPrimary `
        -MasterTarget $mtAccount `
        -ProcessServer $psAccount `
        -ProtectionContainerMapping $reverseContainerMappingObject `
        -ReplicationProtectedItem $rpi `
        -RetentionVolume $retVolume

    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTime60Seconds

    $job = Get-AzRecoveryServicesAsrJob -Job $currentJob
    LogMessage -Message ('Job target object id : {0}, target object name : {1}' -f `
            $job.TargetObjectId, $job.TargetObjectName) -LogType ([LogType]::Info)
			
	$ProtectedItems = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $protectionContainer
	LogMessage -Message ('Protected VMs information is as follows:') -LogType ([LogType]::Info)
	$ProtectedItems | Out-File $global:LogName -Append

    #WaitForIRCompletion -VMName $job.TargetObjectName -ProtectionDirection "AzureToVmware"-JobQueryWaitTimeInSeconds $JobQueryWaitTime60Seconds

    # Wait for VM state to change to protected
	WaitForProtectedItemState -state "Protected" -VMName $rpi.FriendlyName -containerObject $protectionContainer

    LogMessage -Message ("Switch Protection completed successfully") -LogType ([LogType]::Info)
	$global:ReturnValue = $true
	if (($global:IsReportingEnabled -eq $true) -and ($op)) {
		ReportOperationStatus -OperationName $Op
	}
}

<#
.SYNOPSIS
    Triggers Failback (Reverse UFO)
#>
Function Failback {
	$Op = "FO_360"
    # Get Replication Protected Item
    $fabricInfo = GetFabricInfo
    $rpi = $fabricInfo.RPI

	# Get tag
    # Will use the latest available one
	$recoveryPoint = GetRecoveryPoint `
        -protectedItemObject $rpi `
        -tagType ([TagType]::CrashConsistent) `
		-IgnoreTagType $true
	
    # TODO : Remove the default latest crash tag recovery and make it configurable
    # Trigger failback
    LogMessage -Message ("Triggering UFO for VM: {0}, Id: {1}" -f $rpi.FriendlyName, $rpi.Name) -LogType ([LogType]::Info)
    $currentJob = Start-AzRecoveryServicesAsrUnplannedFailoverJob `
        -Direction PrimaryToRecovery `
        -RecoveryTag LatestAvailable `
        -ReplicationProtectedItem $rpi
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
    LogMessage -Message ("*****Failback finished*****") -LogType ([LogType]::Info)

    LogMessage -Message ("Triggering Commit after doing failback for VM: {0}, Id: {1}" -f $rpi.FriendlyName, $rpi.Name) -LogType ([LogType]::Info)
    $job = Start-AzRecoveryServicesAsrCommitFailoverJob -ReplicationProtectedItem $rpi
    WaitForJobCompletion -JobId $Job.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
    LogMessage -Message ("*****Commit after Failback finished*****") -LogType ([LogType]::Info)
	$global:ReturnValue = $true
	if (($global:IsReportingEnabled -eq $true) -and ($op)) {
		ReportOperationStatus -OperationName $Op
	}
}

<#
.SYNOPSIS
    Triggers Reverse Switch Protection
#>
Function ReverseSwitchProtection {
	$Op = "ER_540"
    # Sleeping for 20mins, so that VM status will be updated
    LogMessage -Message ("Sleeping for {0} sec" -f ($JobQueryWaitTime60Seconds * 35)) -LogType ([LogType]::Info)
    Start-Sleep -Seconds ($JobQueryWaitTime60Seconds * 35)

    # Get Fabric object
    $fabricInfo = GetFabricInfo
    $rpi = $fabricInfo.RPI
    $protectionContainer = $fabricInfo.ProtectionContainer
    LogMessage -Message ("Primary Container: {0}, Role: {1}, AvailablePolicies: {2}" -f `
            $protectionContainer.FriendlyName,
        $protectionContainer.Role,
        $protectionContainer.AvailablePolicies.FriendlyName ) -LogType ([LogType]::Info)

    $forwardContainerMappingObject = Get-AzRecoveryServicesAsrProtectionContainerMapping `
        -Name $global:ForwardContainerMappingName `
        -ProtectionContainer $protectionContainer  `
    | Where-Object { $_.PolicyFriendlyName -match $global:ReplicationPolicy }
Assert-NotNull($forwardContainerMappingObject)
LogMessage -Message ("Container Mapping: {0}, State: {1}" -f `
        $forwardContainerMappingObject.Name, $forwardContainerMappingObject.State) -LogType ([LogType]::Info)

LogMessage -Message ("Triggering Reverse Switch protection") -LogType ([LogType]::Info)

$currentJob = Update-AzRecoveryServicesAsrProtectionDirection `
    -VmwareToAzure `
    -Account $fabricInfo.vmAccount `
    -Direction RecoveryToPrimary `
    -ProcessServer $fabricInfo.psAccount `
    -ProtectionContainerMapping $forwardContainerMappingObject `
    -ReplicationProtectedItem $rpi

WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTime60Seconds

$job = Get-AzRecoveryServicesAsrJob -Job $currentJob
LogMessage -Message ('Job target object id : {0}, target object name : {1}' -f `
        $job.TargetObjectId, $job.TargetObjectName) -LogType ([LogType]::Info)

#WaitForIRCompletion -VMName $job.TargetObjectName -ProtectionDirection "VmwareToAzure"-JobQueryWaitTimeInSeconds $JobQueryWaitTime60Seconds

$ProtectedItems = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $protectionContainer
LogMessage -Message ('Protected VMs information is as follows:') -LogType ([LogType]::Info)
$ProtectedItems | Out-File $global:LogName -Append

# Wait for VM state to change to protected
WaitForProtectedItemState -state "Protected" -VMName $rpi.FriendlyName -containerObject $protectionContainer

LogMessage -Message ("Reverse Switch Protection completed successfully") -LogType ([LogType]::Info)
$global:ReturnValue = $true
if (($global:IsReportingEnabled -eq $true) -and ($op)) {
	ReportOperationStatus -OperationName $Op
}
}

<#
.SYNOPSIS
    Triggers Disable Replication
#>
Function DisableReplication {
	$Op= "DisableRep"
    # Get Replication Protected Item
    $fabricInfo = GetFabricInfo
    $rpi = $fabricInfo.RPI
	
    # Disable Protection(Forward)
    if ($null -eq $rpi) {
        LogMessage -Message ("VM: {0} is not in enabled list.Skipping disable for this VM" -f $global:SourceFriendlyName) -LogType 4
    }
    else {
        LogMessage -Message ("Triggering Disable Protection for VM: {0}" -f $rpi.FriendlyName) -LogType ([LogType]::Info)
        $currentJob = Remove-AzRecoveryServicesAsrReplicationProtectedItem -InputObject $rpi
        LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name, $currentJob.State) -LogType ([LogType]::Info)
        WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
        LogMessage -Message ("*****Disable DR finished*****") -LogType ([LogType]::Info)
    }
	$global:ReturnValue = $true
	if (($global:IsReportingEnabled -eq $true) -and ($op)) {
		ReportOperationStatus -OperationName $Op
	}
}

<#
.SYNOPSIS
    Dissociate and delete replication policy
#>
Function DeleteReplicationPolicy {
	$Op = "DelRepPolicy"
    $fabricObject = Get-AzRecoveryServicesAsrFabric -FriendlyName $global:CSPSName
    Assert-NotNull($fabricObject)

    $protContainerObject = Get-AzRecoveryServicesAsrProtectionContainer -Fabric $fabricObject -FriendlyName $global:CSPSName
    Assert-NotNull($protContainerObject)

    # Remove forward protection conatiner mapping
    $forwardContainerMappingObject = Get-AzRecoveryServicesAsrProtectionContainerMapping `
        -Name $global:ForwardContainerMappingName `
        -ProtectionContainer $protContainerObject `
        -ErrorAction SilentlyContinue `
    | Where-Object { $_.PolicyFriendlyName -match $global:ReplicationPolicy }

if ($null -ne $forwardContainerMappingObject.Name) {
    LogMessage -Message ("Container Mapping: {0}, State: {1}" -f `
            $forwardContainerMappingObject.Name, $forwardContainerMappingObject.State) -LogType ([LogType]::Info)
    $job = Remove-AzRecoveryServicesAsrProtectionContainerMapping -ProtectionContainerMapping $forwardContainerMappingObject
    WaitForJobCompletion -JobId $job.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
    LogMessage -Message ("Removed forward protection container mapping") -LogType ([LogType]::Info)
}

# Remove reverse container mapping
$reverseContainerMappingObject = Get-AzRecoveryServicesAsrProtectionContainerMapping `
    -Name $global:ReverseContainerMappingName `
    -ProtectionContainer $protContainerObject `
    -ErrorAction SilentlyContinue `
| Where-Object { $_.PolicyFriendlyName -match $global:ReverseReplicationPolicy }

if ($null -ne $reverseContainerMappingObject.Name) {
    LogMessage -Message ("Container Mapping: {0}, State: {1}" -f `
            $reverseContainerMappingObject.Name, $reverseContainerMappingObject.State) -LogType ([LogType]::Info)
    $job = Remove-AzRecoveryServicesAsrProtectionContainerMapping -ProtectionContainerMapping $reverseContainerMappingObject
    WaitForJobCompletion -JobId $job.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
    LogMessage -Message ("Removed reverse protection container mapping") -LogType ([LogType]::Info)
}

# Remove Replication policies
$policyObject = Get-AzRecoveryServicesAsrPolicy -Name $global:ReplicationPolicy -ErrorAction SilentlyContinue
if ($null -ne $policyObject.FriendlyName) {
    LogMessage -Message ("Triggering Remove Policy: {0}" -f $policyObject.FriendlyName) -LogType ([LogType]::Info)
    $currentJob = Remove-AzRecoveryServicesAsrPolicy -InputObject $policyObject
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
    LogMessage -Message ("*****Policy Removed*****") -LogType ([LogType]::Info)
}

$revPolicyObject = Get-AzRecoveryServicesAsrPolicy -Name $global:ReverseReplicationPolicy  -ErrorAction SilentlyContinue
if ($null -ne $revPolicyObject.FriendlyName) {
    LogMessage -Message ("Triggering Remove Policy: {0}" -f $revPolicyObject.FriendlyName) -LogType ([LogType]::Info)
    $currentJob = Remove-AzRecoveryServicesAsrPolicy -InputObject $revPolicyObject
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
    LogMessage -Message ("*****Failback Policy Removed*****") -LogType ([LogType]::Info)
}

$global:ReturnValue = $true
if (($global:IsReportingEnabled -eq $true) -and ($op)) {
	ReportOperationStatus -OperationName $Op
}
}

<#
.SYNOPSIS
    Delete the Azure Failover VM
#>
Function DeleteFailoverVM {
    LogMessage -Message ("Removing the Failover VM : {0}" -f $VMName) -LogType ([LogType]::Info)
    Remove-AzVM -ResourceGroupName $global:RecResourceGroup -VMName $global:SourceFriendlyName
    LogMessage -Message ("Removed the Failover VM : {0}" -f $VMName) -LogType ([LogType]::Info)
}


<#
.SYNOPSIS
Main Block
#>
Function Main() {
    try {
		LoginToAzure
		if (!$?) {
			LogMessage -Message ("Failed to login to Azure") -LogType ([LogType]::Error)
			Throw "Failed to login to Azure"
		}
		
        $vaultPath = "$PSScriptRoot\$global:VaultName.VaultCredentials"
        Import-AzRecoveryServicesAsrVaultSettingsFile -Path $vaultPath
        if (!$?) {
            LogMessage -Message ('Failed to import vault : {0}' -f $vaultPath) -LogType ([LogType]::Error)
            Throw "Failed to import Vault Settings File"
        }
        
        switch ($Operation.ToUpper()) {
            'CLEANUP' { ASRCheckAndRemoveFabricsNContainers; break }
            'DELFABRIC' { RemoveFabric; break }
            'DELVCENTER' { RemoveVcenter; break }
            'VALIDATE' { $Op = "ValidateFabric"; ValidateFabric; break }
			'ADDACCOUNTONCS' { $Op = "AddAccount"; AddAccountOnCS; break }
            'ADDVCENTER' { AddVcenter; break }
            'ER' {	$Op = "ER_180"; EnableReplication; break	}
            'TFO' { $Op = "TFO"; TestFailover; break	}
            'UFO' { $Op = "FO_180"; UnplannedFailover; break	}
            'SP' { $Op = "ER_360"; SwitchProtection; break	}
            'FB' { $Op = "FO_360"; Failback; break	}
            'RSP' { $Op = "ER_540"; ReverseSwitchProtection; break	}
            'DISABLE' { $Op= "DisableRep"; DisableReplication; break	}
            'DRP' { $Op = "DelRepPolicy"; DeleteReplicationPolicy; break	}
            'UPDATE' { UpdateMobilityAgent; break }
            'RUN540' {
                ValidateFabric
                EnableReplication
                TestFailover
                UnplannedFailover
                SwitchProtection
                Failback
                ReverseSwitchProtection
                DisableReplication
                DeleteReplicationPolicy
                LogMessage -Message ("V2A GQL RUN SUCCEEDED") -LogType ([LogType]::Info)
                break
            }
            'RUN360' {
                ValidateFabric
                EnableReplication
                TestFailover
                UnplannedFailover
                SwitchProtection
                Failback
                DisableReplication
                DeleteReplicationPolicy
                break
            }
            'RUN180' {
                ValidateFabric
                EnableReplication
                TestFailover
                UnplannedFailover
                DisableReplication
                DeleteReplicationPolicy
                DeleteFailoverVM
                break
            }
            default { break	}
        }

        LogMessage -Message ("V2A GQL TEST PASSED") -LogType ([LogType]::Info)
    }
    catch {
        LogMessage -Message ("ERROR:: {0}" -f ($Error | ConvertTo-json -Depth 1)) -LogType ([LogType]::Error)

        LogMessage -Message ("V2A GQL Test Failed") -LogType ([LogType]::Error)
        $errorMessage = "GQL TEST FAILED"
        throw $errorMessage
    }
	finally {
		if (($global:IsReportingEnabled -eq $true) -and ($op)) {
			ReportOperationStatus -OperationName $Op
		}
	}
}

# Set logName
if ($null -eq $global:LogName) {
    CreateLogDir
    $global:LogName = "$global:LogDir\$global:Operation.log"
    if ($(Test-Path -Path $global:LogName))
    {
        $bkplog = "$global:LogDir\$global:Operation`_{0}.log" -f (Get-Date -Format "MM-dd-yyyy HH.mm.ss")
        Move-Item -Path $global:LogName -Destination $bkplog -Force -ErrorAction SilentlyContinue
    }
}

Main
