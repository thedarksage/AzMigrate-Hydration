<#

.SYNOPSIS
Powershell script to perform End To End Testing for A2A

.DESCRIPTION
The script is used to perform EnableProtection, TFO, UFO, SwitchProtection, DisableProtection operations on Azure VM for A2A.

.PARAMETER Operation
Mandatory parameter, it indicates the operation to be performed on the VM. Allowed values for Operation are "ER", "TFO", "UFO", "SP", "FB", "DR", "RSP", "CR", "ALL", "Reboot", "APPLYLOAD", "ISSUETAG", "VERIFYLOAD", "HELP"

.EXAMPLE
./A2AGQLTest.ps1 "ER"
It performs Enable Protection on the Primary VM

.EXAMPLE
./A2AGQLTest.ps1 "TFO"
It triggers Test Failover on the Primary VM

./A2AGQLTest.ps1 "Reboot"
It reboots the Source VM

.EXAMPLE
./A2AGQLTest.ps1 "UFO"
It triggers UnplannedFailover on the Primary VM

.EXAMPLE
./A2AGQLTest.ps1 "SP"
It triggers ReverseProtection on the UFO VM

.EXAMPLE
./A2AGQLTest.ps1 "DR"
It disables the protection on the Primary VM and removes all the azure artifacts created in the earlier operations.

.EXAMPLE
./A2AGQLTest.ps1 "CR"
Removes all the mappings and azure recovery resources

.EXAMPLE
./A2AGQLTest.ps1 "RSP"
It triggers Reverse Switch Protection ( Primary -> Recovery )

.EXAMPLE
./A2AGQLTest.ps1 "APPLYLOAD"
Generates churn on agent

.EXAMPLE
./A2AGQLTest.ps1 "ISSUETAG"
Issues custom tag on agent and gets tag insert timestamp.

.EXAMPLE
./A2AGQLTest.ps1 "VERIFYLOAD"
Validates the generated data on agent machine and returns the status.

.EXAMPLE
./A2AGQLTest.ps1 "Uninstall"
Uninstalls the mobility agent from source VM and reboots it.

.EXAMPLE
./A2AGQLTest.ps1 "UPLOAD"
Uploads the required DI tool utilities to azure storage account

.EXAMPLE
./A2AGQLTest.ps1 "COPY"
Copies the required DI tool utilities to source machine

.NOTES
Make sure you update the A2AGQLConfig.xml file before running this script. Update the config file with valid Azure Artifacts like Primary Resource Group and Storage accounts.

#>

# Input parameters
param (
	[Parameter(Mandatory=$true)]
	[ValidateSet("COPY", "ER", "TFO", "UFO", "SP", "FB", "DR", "RSP", "CR", "ALL", "REBOOT", "UNINSTALL", "APPLYLOAD", "ISSUETAG", "VERIFYLOAD", "UPLOAD","ATTACHDISK","CREATEDISKLAYOUT", "HELP")] 
    [String] $Operation,
	
	[Parameter(Mandatory=$false)]
    [String] $ResourceGroupType="Primary",
	
	[Parameter(Mandatory=$false)]
    [String] $IsStaticVM="True",
	
	[parameter(Mandatory=$False)]
	[string]$useAzureExtensionForLinux = "true"
)

#
# Include common library files
#
if($Operation.ToLower() -ne 'help')
{
	$Error.Clear()
	. $PSScriptRoot\CommonFunctions.ps1
}

# Global Variables
$global:ContainerName = "scripts"
$global:CustomScriptFilesList = [System.Collections.ArrayList]@()
$global:Operation = $Operation.ToUpper()

#
# Prepare Azure Environment and Enable Protection
#
Function EnableReplication()
{
    # Check and remove fabric if exist exist
	ASRCheckAndRemoveFabricsNContainers
	
	# Create fabrics (primary & recovery)
	LogMessage -Message ("Creating Primary Fabric: $global:PrimaryFabricName") -Type 1;
	$currentJob = New-AzureRmSiteRecoveryFabric -Name $global:PrimaryFabricName -Type 'Azure' -Location $global:PrimaryLocation
	
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::CreateFabric)
	LogMessage -Message ("Creating Recovery Fabric: $global:RecoveryFabricName") -Type 1;
	$currentJob = New-AzureRmSiteRecoveryFabric -Name $global:RecoveryFabricName -Type 'Azure' -Location $global:RecoveryLocation
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::CreateFabric)

	# Get fabric objects
	$primaryFabricObject = GetFabricObject -fabricName $global:PrimaryFabricName
	$recoveryFabricObject = GetFabricObject -fabricName $global:RecoveryFabricName

	# Create Containers primary and recovery
	LogMessage -Message ("Creating Container for Primary Geo: $global:PrimaryContainerName") -Type 1;
	$currentJob = New-AzureRmSiteRecoveryProtectionContainer -Name $global:PrimaryContainerName -Fabric $primaryFabricObject
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::CreateContainer)
	LogMessage -Message ("Creating Container for Recovery Geo: $global:RecoveryContainerName") -Type 1;
	$currentJob = New-AzureRmSiteRecoveryProtectionContainer -Name $global:RecoveryContainerName -Fabric $recoveryFabricObject
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::CreateContainer)

	# Get container objects
	$primaryContainerObject = Get-AzureRmSiteRecoveryProtectionContainer -Fabric $primaryFabricObject -Name $global:PrimaryContainerName
	$recoveryContainerObject = Get-AzureRmSiteRecoveryProtectionContainer -Fabric $recoveryFabricObject -Name $global:RecoveryContainerName
	
	# Enumerate registered servers
	$fabrics = Get-AzureRmSiteRecoveryFabric
	$primaryFabricObject = GetFabricObject -fabricName $global:PrimaryFabricName
	$recoveryFabricObject = GetFabricObject -fabricName $global:RecoveryFabricName
	
	# Get Source VM Details
	$azureArtifacts = GetAzureArtifacts
	$VMName = $azureArtifacts.Vm.Name
	$VMId = $azureArtifacts.Vm.Id
	
	# Get Protection containers
	$ProtectionContainers = $fabrics | Get-AzureRMSiteRecoveryProtectionContainer
	LogMessage -Message ("Containers found : {0}" -f $ProtectionContainers.Count) -Type 1;
	LogMessage -Message ("Containers info:") -Type 1
	$ProtectionContainers| `
		%{ 
			$data = $_; 
			LogMessage -Message (" -> Name: {0}, Id: {1}" -f $data.FriendlyName,$data.Name) -Type 3;
		 }

	$primaryContainerObject = $ProtectionContainers | where { $_.FriendlyName -eq $global:PrimaryContainerName }
	LogMessage -Message ("Primary Container: {0}, Role: {1}, AvailablePolicies: {2}" -f $primaryContainerObject.FriendlyName,$primaryContainerObject.Role, $primaryContainerObject.AvailablePolicies.FriendlyName ) -Type 1;
	$RecoveryContainerObject = $ProtectionContainers | where { $_.FriendlyName-eq $RecoveryContainerName }
	LogMessage -Message ("Recovery Container: {0}, Role: {1}, AvailablePolicies: {2}" -f $RecoveryContainerObject.FriendlyName,$RecoveryContainerObject.Role, $RecoveryContainerObject.AvailablePolicies.FriendlyName ) -Type 1;

	# Before starting run try to Unpair Container from earlier run if exist.
	$containerMappingObject = Get-AzureRmSiteRecoveryProtectionContainerMapping -ProtectionContainer $primaryContainerObject 

	LogMessage -Message ("Found {0} protection mappings. Going to remove all." -f $containerMappingObject.Count) -Type 1;
	foreach($pcMapping in $containerMappingObject)
	{
		LogMessage -Message ("Started Container Unpairing before run starts.") -Type 1
		$currentJob = Remove-AzureRmSiteRecoveryProtectionContainerMapping -ProtectionContainerMapping $pcMapping
		LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name,$currentJob.State) -Type 1;
		WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DissociatePolicy)
		LogMessage -Message ("*****Containers: {0} Unpaired before run starts*****" -f $pcMapping.Name) -Type 2;
	}

	# Create Policy
	LogMessage -Message ("Triggering replication policy creation: {0}" -f $global:ReplicationPolicy) -Type 1;
	$currentJob = New-AzureRmSiteRecoveryPolicy -Name $global:ReplicationPolicy -ReplicationProvider 'A2A' -RecoveryPointHistory 1440 -CrashConsistentFrequencyInMinutes 5 -AppConsistentFrequencyInMinutes 60
	LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name,$currentJob.State) -Type 1;
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::CreatePolicy)
	LogMessage -Message ("*****Replication policy created*****") -Type 2;

	$policyObject = Get-AzureRmSiteRecoveryPolicy -Name $global:ReplicationPolicy
	LogMessage -Message ("Replication policy: {0} fetched successfully." -f $policyObject.Name) -Type 1;

	# Create container mapping(Cloud Pairing) [primary -> recovery]
	LogMessage -Message ("Triggering Container Pairing: {0}" -f $global:ForwardContainerMappingName) -Type 1;
	$currentJob = New-AzureRmSiteRecoveryProtectionContainerMapping -Name $global:ForwardContainerMappingName -Policy $policyObject -PrimaryProtectionContainer $primaryContainerObject -RecoveryProtectionContainer $RecoveryContainerObject
	LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name,$currentJob.State) -Type 1;
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::AssociatePolicy)
	LogMessage -Message ("*****Container paired*****") -Type 2;

	# Update Protection container objects after Container pairing
	$primaryContainerObject = Get-AzureRMSiteRecoveryProtectionContainer -Fabric $primaryFabricObject -FriendlyName $global:PrimaryContainerName
	LogMessage -Message ("Primary Container: {0}, Role: {1}, AvailablePolicies: {2}" -f $primaryContainerObject.FriendlyName,$primaryContainerObject.Role, $primaryContainerObject.AvailablePolicies.FriendlyName ) -Type 1;

	$forwardContainerMappingObject = Get-AzureRmSiteRecoveryProtectionContainerMapping -Name $global:ForwardContainerMappingName -ProtectionContainer $primaryContainerObject
	LogMessage -Message ("Container Mapping: {0}, State: {1}" -f $forwardContainerMappingObject.Name,$forwardContainerMappingObject.State) -Type 1;

	# Network Mapping [primary -> recovery]
	LogMessage -Message ("Triggering Network Mapping: {0}" -f $global:ForwardNetworkMappingName) -Type 1;
	$currentJob = New-AzureRmSiteRecoveryNetworkMapping -Name $global:ForwardNetworkMappingName -PrimaryFabric $primaryFabricObject -PrimaryAzureNetworkId $azureArtifacts.PriVnet.Id -RecoveryFabric $recoveryFabricObject -RecoveryAzureNetworkId $azureArtifacts.RecVnet.Id
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::MapNetwork)
	LogMessage -Message ("*****Network Mapped*****") -Type 2

	# Enable Protection
	$enableDRName = "ER-" + $global:VMName
	WaitForAzureVMReadyState -vmName $VMName -resourceGroupName $azureArtifacts.PriResourceGroup.ResourceGroupName
	LogMessage -Message ("Triggering Enable Protection: {0} for VM Id: {1}" -f $enableDRName,$VmID) -Type 1;
	ASRA2AEnableDR -enableDRName $enableDRName -containerMappingObject $forwardContainerMappingObject -azureArtifacts $azureArtifacts
}

#
# Triggers Test Failover for source VM
#
Function TestFailover
{
	# Get Source VM details
	$azureArtifacts = GetAzureArtifacts
	
	# Get fabric and protected VM objects
	$primaryFabricObject = GetFabricObject -fabricName $global:PrimaryFabricName
	$recoveryFabricObject = GetFabricObject -fabricName $global:RecoveryFabricName
	$protectedItemObject = GetProtectedItemObject -fabricName $global:PrimaryFabricName -containerName $global:PrimaryContainerName
	
	# Get Custom tag
	[PSObject]$recoveryPoint = GetRecoveryPoint -protectedItemObject $protectedItemObject -tagType "Latest"
	
	# TFO
	LogMessage -Message ("Triggering TFO for VM: {0}, Id: {1}" -f $protectedItemObject.FriendlyName, $protectedItemObject.Name) -Type 1;
	$currentJob = Start-AzureRmSiteRecoveryTestFailover -ReplicationProtectedItem $protectedItemObject -Direction PrimaryToRecovery -RecoveryPoint $recoveryPoint -AzureVMNetworkId $azureArtifacts.RecVnet.Id -CloudServiceCreationOption AutoCreateCloudService -SkipTestFailoverCleanup
	LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name, $currentJob.State) -Type 1;
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTime60Seconds * 2) -OperationName ([ASROperation]::TestFailover)
	LogMessage -Message ("*****TFO cleanup required for Completion*****") -Type 2
	
	# Check TFO VM Status
	WaitForAzureVMReadyState -vmName $TFOVMName -resourceGroupName $global:RecResourceGroup
	
	# Complete TFO
	LogMessage -Message ("Triggering TFO Cleanup") -Type 1;
	$currentJob = Start-AzureRmSiteRecoveryTestFailoverCleanup -ReplicationProtectedItem $protectedItemObject -Comments "TFO cleanup"
	LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name,$currentJob.State) -Type 1;
	LogMessage -Message ("Waiting for: {0} seconds" -f (($JobQueryWaitTime60Seconds * 2))) -Type 1;
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTime60Seconds -OperationName ([ASROperation]::ResumeJob)
	LogMessage -Message ("*****TFO Cleanup Completed*****") -Type 2
}

#
# Trigger Unplanned Failover on Source VM
#
Function UnPlannedFailover
{
	# Get Protected Item
	$protectedItemObject = GetProtectedItemObject -fabricName $global:PrimaryFabricName -containerName $global:PrimaryContainerName
	
	# Get Custom tag
	[PSObject]$recoveryPoint = GetRecoveryPoint -protectedItemObject $protectedItemObject -tagType "Latest"
	
	# UnPlanned Failover (UFO)
	WaitForAzureVMReadyState -vmName $global:VMName -resourceGroupName $global:PriResourceGroup
	$protectedItemObject = GetProtectedItemObject -fabricName $global:PrimaryFabricName -containerName $global:PrimaryContainerName
	LogMessage -Message ("Triggering UFO for VM: {0}, Id: {1}" -f $protectedItemObject.FriendlyName, $protectedItemObject.Name) -Type 1
	$currentJob = Start-AzureRmSiteRecoveryUnplannedFailover -ReplicationProtectedItem $protectedItemObject -Direction PrimaryToRecovery -PerformSourceSideActions -RecoveryPoint $recoveryPoint
	LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name,$currentJob.State) -Type 1
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTime60Seconds * 2) -OperationName ([ASROperation]::Failover)
	LogMessage -Message ("*****UFO finished*****") -Type 2
	
	# Check Recovered VM Status
	WaitForAzureVMReadyState -vmName $global:VMName -resourceGroupName $global:RecResourceGroup
	LogMessage -Message ("*****Validation finished on UFO VM*****") -Type 2
	
	LogMessage -Message ("Triggering Commit after PFO for VM: {0}, Id: {1}" -f $protectedItemObject.FriendlyName, $protectedItemObject.Name) -Type 1;
	$currentJob = Start-AzureRmSiteRecoveryCommitFailover -ReplicationProtectedItem $protectedItemObject
	LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name,$currentJob.State) -Type 1;
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::CommitFailover)
	LogMessage -Message ("*****Commit after PFO finished*****") -Type 2
}

#
# Trigger Switch Protection
#
Function SwitchProtection
{
	# Get Source VM details
	$azureArtifacts = GetAzureArtifacts
	
	# Commit after UFO
	$protectedItemObject = GetProtectedItemObject -fabricName $global:PrimaryFabricName -containerName $global:PrimaryContainerName

	# Create Reverse Container Mapping(Cloud Pairing) [recovery -> primary]
	LogMessage -Message ("Triggering Reverse Container Mapping(Cloud Pairing) [recovery -> primary]::{0}" -f $global:ReverseContainerMappingName) -Type 1;
	$policyObject = Get-AzureRmSiteRecoveryPolicy -Name $global:ReplicationPolicy
	LogMessage -Message ("Replication policy: {0} fetched successfully." -f $policyObject.Name) -Type 1;
	$primaryFabricObject = GetFabricObject -fabricName $global:PrimaryFabricName
	$recoveryFabricObject = GetFabricObject -fabricName $global:RecoveryFabricName
	$recoveryContainerObject = Get-AzureRmSiteRecoveryProtectionContainer -Fabric $recoveryFabricObject -Name $global:RecoveryContainerName
	$primaryContainerObject = Get-AzureRmSiteRecoveryProtectionContainer -Fabric $primaryFabricObject -Name $global:PrimaryContainerName
	$currentJob = New-AzureRmSiteRecoveryProtectionContainerMapping -Name $global:ReverseContainerMappingName -Policy $policyObject -PrimaryProtectionContainer $recoveryContainerObject -RecoveryProtectionContainer $primaryContainerObject
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::AssociatePolicy)
	
	# Get reverse ContainerMapping Object
	$reverseContainerMappingObject = Get-AzureRmSiteRecoveryProtectionContainerMapping -Name $global:ReverseContainerMappingName -ProtectionContainer $recoveryContainerObject

	# Reverse Network Mapping [recovery -> primary]
	LogMessage -Message ("Triggering Reverse Network Mapping: {0}" -f $global:ReverseContainerMappingName) -Type 1
	$currentJob = New-AzureRmSiteRecoveryNetworkMapping -Name $global:ReverseContainerMappingName -PrimaryFabric $recoveryFabricObject -PrimaryAzureNetworkId $azureArtifacts.RecVnet.Id -RecoveryFabric $primaryFabricObject -RecoveryAzureNetworkId $azureArtifacts.PriVnet.Id
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::MapNetwork)
	LogMessage -Message ("*****Reverse Network Mapped*****") -Type 2

	# Switch protection [recovery -> primary]
	# Wait for VM agent ProvisioningState=succeeded, VM ProvisioningState= Succeeded and VM PowerState=running
	WaitForAzureVMReadyState -vmName $azureArtifacts.Vm.Name -resourceGroupName $azureArtifacts.RecResourceGroup.ResourceGroupName

	Start-Sleep -Seconds ($JobQueryWaitTime60Seconds * 5)
	
	$forwardSwitchProtection = "SP-" + $global:VMName
	LogMessage -Message ("Triggering Switch protection (recovery -> primary): {0}" -f $forwardSwitchProtection) -Type 1;
	ASRA2ASwitchProtection -switchProtectionName $forwardSwitchProtection -containerMappingObject $reverseContainerMappingObject `
	-protectedItemObject $protectedItemObject -direction RecoveryToPrimary -azureArtifacts $azureArtifacts
	
	# Wait for VM state to change to protected
	WaitForProtectedItemState -state "Protected" -containerObject $recoveryContainerObject
}

#
# Trigger Failback (Reverse UFO)
#
Function Failback
{
	# Wait for VM agent ProvisioningState=succeeded, VM ProvisioningState= ucceeded and VM PowerState=running
	WaitForAzureVMReadyState -vmName $global:VMName -resourceGroupName $global:RecResourceGroup
	
	# Get reverse protected object
	$reverseProtectedItemObject = GetProtectedItemObject -fabricName $global:RecoveryFabricName -containerName $global:RecoveryContainerName
	
	# Get Custom tag
	[PSObject]$recoveryPoint = GetRecoveryPoint -protectedItemObject $reverseProtectedItemObject -tagType "Latest"
	
	# Trigger Failback
	LogMessage -Message ("Triggering Reverse UFO for VM: {0}, Id: {1}" -f $reverseProtectedItemObject.FriendlyName,$reverseProtectedItemObject.Name) -Type 1
	$currentJob = Start-AzureRmSiteRecoveryUnplannedFailover -ReplicationProtectedItem $reverseProtectedItemObject -Direction PrimaryToRecovery -PerformSourceSideActions -RecoveryPoint $recoveryPoint
	LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name,$currentJob.State) -Type 1
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTime60Seconds * 2) -OperationName ([ASROperation]::Failback)
	LogMessage -Message ("*****Reverse UFO finished*****") -Type 2
	
	WaitForAzureVMReadyState -vmName $global:VMName -resourceGroupName $global:PriResourceGroup
	LogMessage -Message ("*****Validation finished on Failback VM*****") -Type 2
}

#
# Triggers Reverse Switch Protection
#
Function ReverseSwitchProtection
{
	# Get Source VM details
	$azureArtifacts = GetAzureArtifacts
	
	# Wait for VM agent ProvisioningState=succeeded, VM ProvisioningState= ucceeded and VM PowerState=running
	WaitForAzureVMReadyState -vmName $azureArtifacts.Vm.Name -resourceGroupName $azureArtifacts.PriResourceGroup.ResourceGroupName
	
	LogMessage -Message ("Sleeping for {0} sec" -f ($JobQueryWaitTime60Seconds * 2)) -Type 1;
	Start-Sleep -Seconds ($JobQueryWaitTime60Seconds * 2)
	
	$reverseSwitchProtection = "RSP-" + $global:VMName
	$primaryFabricObject = GetFabricObject -fabricName $global:PrimaryFabricName
	$primaryContainerObject = Get-AzureRMSiteRecoveryProtectionContainer -Fabric $primaryFabricObject -FriendlyName $global:PrimaryContainerName
	LogMessage -Message ("Primary Container: {0}, Role: {1}, AvailablePolicies: {2}" -f $primaryContainerObject.FriendlyName,$primaryContainerObject.Role, $primaryContainerObject.AvailablePolicies.FriendlyName ) -Type 1;
	$forwardContainerMappingObject = Get-AzureRmSiteRecoveryProtectionContainerMapping -Name $global:ForwardContainerMappingName -ProtectionContainer $primaryContainerObject
	
	# Get reverse protected object
	$reverseProtectedItemObject = GetProtectedItemObject -fabricName $global:RecoveryFabricName -containerName $global:RecoveryContainerName
	
	LogMessage -Message ("Triggering Switch protection (primary -> recovery): {0}" -f $reverseSwitchProtection) -Type 1;
	ASRA2ASwitchProtection -switchProtectionName $reverseSwitchProtection -containerMappingObject $forwardContainerMappingObject `
	-protectedItemObject $reverseProtectedItemObject -direction PrimaryToRecovery -azureArtifacts $azureArtifacts
	
	# Wait for VM state to change to protected
	WaitForProtectedItemState -state "Protected" -containerObject $primaryContainerObject
}

#
# Triggers Disable Replication 
#
Function DisableReplication
{
	# Remove Networkmapping
	LogMessage -Message ("Triggering Primary->Recovery Network Un-Mapping") -Type 1
	$primaryFabricObject = GetFabricObject -fabricName $global:PrimaryFabricName
	$forwardNetworkMappingObject = Get-AzureRmSiteRecoveryNetworkMapping -PrimaryAzureFabric $primaryFabricObject
	$currentJob = Remove-AzureRmSiteRecoveryNetworkMapping -NetworkMapping $forwardNetworkMappingObject
	LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name,$currentJob.State) -Type 1
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::UnMapNetwork)
	LogMessage -Message ("*****Primary->Recovery Network Un-Mapped*****") -Type 2

	LogMessage -Message ("Triggering Recovery->Primary Network Un-Mapping") -Type 1
	$recoveryFabricObject = GetFabricObject -fabricName $global:RecoveryFabricName
	$reverseNetworkMappingObject = Get-AzureRmSiteRecoveryNetworkMapping -PrimaryAzureFabric $recoveryFabricObject
	$currentJob = Remove-AzureRmSiteRecoveryNetworkMapping -NetworkMapping $reverseNetworkMappingObject
	LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name,$currentJob.State) -Type 1
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::UnMapNetwork)
	LogMessage -Message ("*****Recovery->Primary Network Un-Mapped*****") -Type 2

	# Disable Protection
	$primaryContainerObject = Get-AzureRMSiteRecoveryProtectionContainer -Fabric $primaryFabricObject -FriendlyName $global:PrimaryContainerName
	
	$VMList = Get-AzureRmSiteRecoveryReplicationProtectedItem -ProtectionContainer $primaryContainerObject
	$protectedItemObject = $VMList | where { $_.FriendlyName -eq $global:VMName }
	if($protectedItemObject -eq $null)
	{
		LogMessage -Message ("VM: {0} is not in enabled list.Skipping disable for this VM" -f $global:VMName) -Type 4
	}
	else
	{
		# Wait for VM agent ProvisioningState=succeeded, VM ProvisioningState= ucceeded and VM PowerState=running
		WaitForAzureVMReadyState -vmName $global:VMName -resourceGroupName $global:PriResourceGroup

		#$protectedItemObject = Get-AzureRmSiteRecoveryReplicationProtectedItem -ProtectionContainer $primaryContainerObject | where { $_.FriendlyName -eq $VMName }
		LogMessage -Message ("Triggering Disable Protection for VM: {0}" -f $protectedItemObject.FriendlyName) -Type 1
		$currentJob = Remove-AzureRmSiteRecoveryReplicationProtectedItem -ReplicationProtectedItem $protectedItemObject
		LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name,$currentJob.State) -Type 1
		WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DisableDR)
		LogMessage -Message ("*****Disable DR finished*****") -Type 2
	}
}

#
# Cleanup Azure Artifacts
#
Function CleanupResources
{
	# Unpair Container
	$primaryFabricObject = GetFabricObject -fabricName $global:PrimaryFabricName
	$primaryContainerObject = Get-AzureRMSiteRecoveryProtectionContainer -Fabric $primaryFabricObject -FriendlyName $global:PrimaryContainerName
	$forwardContainerMappingObject = Get-AzureRmSiteRecoveryProtectionContainerMapping -Name $global:ForwardContainerMappingName -ProtectionContainer $primaryContainerObject
	LogMessage -Message ("Triggering Primary->Recovery Container Un-Pairing: {0}" -f $forwardContainerMappingObject.Name) -Type 1
	$currentJob = Remove-AzureRmSiteRecoveryProtectionContainerMapping -ProtectionContainerMapping $forwardContainerMappingObject
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DissociatePolicy)
	LogMessage -Message ("*****Primary->Recovery Containers Unpaired*****") -Type 2
	
	$recoveryFabricObject = GetFabricObject -fabricName $global:RecoveryFabricName
	$recoveryContainerObject = Get-AzureRmSiteRecoveryProtectionContainer -Fabric $recoveryFabricObject -Name $recoveryContainerName
	$reverseContainerMappingObject = Get-AzureRmSiteRecoveryProtectionContainerMapping -Name $global:ReverseContainerMappingName -ProtectionContainer $recoveryContainerObject
	LogMessage -Message ("Triggering Recovery->Primary Container Un-Pairing: {0}" -f $reverseContainerMappingObject.Name) -Type 1
	$currentJob = Remove-AzureRmSiteRecoveryProtectionContainerMapping -ProtectionContainerMapping $reverseContainerMappingObject
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DissociatePolicy)
	LogMessage -Message ("*****Recovery->Primary Containers Unpaired*****") -Type 2

	# Remove Container
	$primaryContainerObject = Get-AzureRmSiteRecoveryProtectionContainer -Fabric $primaryFabricObject -Name $primaryContainerName
	$recoveryContainerObject = Get-AzureRmSiteRecoveryProtectionContainer -Fabric $recoveryFabricObject -Name $recoveryContainerName

	LogMessage -Message ("Triggering Remove Primary Container: {0}" -f $primaryContainerObject.FriendlyName) -Type 1
	$currentJob = Remove-AzureRmSiteRecoveryProtectionContainer -ProtectionContainer $primaryContainerObject
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DeleteContainer)
	LogMessage -Message ("Triggering Remove Recovery Container: {0}" -f $recoveryContainerObject.FriendlyName) -Type 1
	$currentJob = Remove-AzureRmSiteRecoveryProtectionContainer -ProtectionContainer $recoveryContainerObject
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DeleteContainer)

	# Remove Policy
	$policyObject = Get-AzureRmSiteRecoveryPolicy -Name $global:ReplicationPolicy
	LogMessage -Message ("Triggering Remove Policy: {0}" -f $policyObject.FriendlyName) -Type 1
	$currentJob = Remove-AzureRMSiteRecoveryPolicy -Policy $policyObject
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DeletePolicy)
	LogMessage -Message ("*****Policy Removed*****") -Type 2

	# Remove Fabric
	$primaryFabricObject = Get-AzureRmSiteRecoveryFabric -Name $global:primaryFabricName
	$recoveryFabricObject = Get-AzureRmSiteRecoveryFabric -Name $global:recoveryFabricName

	LogMessage -Message ("Triggering Remove Primary Fabric: {0}" -f $primaryFabricObject.FriendlyName) -Type 1
	$currentJob = Remove-AzureRmSiteRecoveryFabric -Fabric $primaryFabricObject -Force
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DeleteFabric)
	
	LogMessage -Message ("Triggering Remove Recovery Fabric: {0}" -f $recoveryFabricObject.FriendlyName) -Type 1
	$currentJob = Remove-AzureRmSiteRecoveryFabric -Fabric $recoveryFabricObject -Force
	WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DeleteFabric)
	
	if($OSType -ne "windows")
	{
		# DetachDataDisk
		# RebootAzureVM
	}
}

#
# Restart Azure VM
#
Function RebootAzureVM
{
	$rgName = $global:PriResourceGroup
	if($ResourceGroupType.ToLower() -ne "primary")
	{
		$rgName = $global:RecResourceGroup
	}
	
	Restart-AzureRmVM -ResourceGroupName $rgName -Name $global:VMName
	if ( !$? )
	{
		LogMessage -Message ("Failed to restart VM : {0} under RG {1}" -f $global:VMName, $rgName) -Type 0
		exit 1
	}
	
	# Check Source VM Status
	WaitForAzureVMReadyState -vmName $global:VMName -resourceGroupName $rgName
	
	LogMessage -Message ("Successfully rebooted VM : {0}" -f $global:VMName) -Type 1
}

#
# Fetch Azure Artifacts
#
Function GetAzureArtifacts
{
	SetAzureEnvironment -environment "PROD"
				
	# Get VM Details
	if(($ResourceGroupType.ToLower() -eq "recovery") -AND ($Operation -ne "TFO"))
	{
		$azureVm = Get-AzureRmVM -ResourceGroupName $global:RecResourceGroup -Name $VMName
	}
	else
	{
		$azureVm = Get-AzureRmVM -ResourceGroupName $global:PriResourceGroup -Name $VMName
	}
	LogMessage -Message ("Azure VM :{0}" -f $azureVm) -Type 1 | Out-Null

	$priRgObject = Get-AzureRmResourceGroup -Name $global:PriResourceGroup -Location $global:PrimaryLocation 
	LogMessage -Message ("Primary Resource Group Object:{0}" -f $priRgObject) -Type 1 | Out-Null

	$recRgObject = Get-AzureRmResourceGroup -Name $global:RecResourceGroup -Location $global:RecoveryLocation -ErrorAction SilentlyContinue
	if($recRgObject -eq $null)
	{
		$recRgObject = New-AzureRmResourceGroup -Name $global:RecResourceGroup -Location $global:RecoveryLocation
	}
	LogMessage -Message ("Recovery Resource Group Object:{0}" -f $recRgObject) -Type 1 | Out-Null

	$priSaObject = Get-AzureRmStorageAccount -Name $global:PriStorageAccount -ResourceGroupName $global:PriResourceGroup
	LogMessage -Message ("Primary Storage Account Object:{0}" -f $priSaObject) -Type 1 | Out-Null


	$recSaObject = Get-AzureRmStorageAccount -Name $global:RecStorageAccount -ResourceGroupName $global:RecResourceGroup -ErrorAction SilentlyContinue
	if($recSaObject -eq $null)
	{
		LogMessage -Message ("Creating Recovery Storage Account Object") -Type 1 | Out-Null
		$recSaObject = New-AzureRmStorageAccount -Name $global:RecStorageAccount -ResourceGroupName $global:RecResourceGroup -Type 'Standard_LRS' -Location $global:RecoveryLocation
	}
	else
	{
		LogMessage -Message ("Recovery Storage Account : {0} already exist. Skipping this." -f $global:RecStorageAccount)
	}
	LogMessage -Message ("Recovery Storage Account Object:{0}" -f $recSaObject) -Type 1 | Out-Null
	
	$stagingSaObject = Get-AzureRmStorageAccount -Name $global:StagingStorageAcc -ResourceGroupName $global:PriResourceGroup -ErrorAction SilentlyContinue
	if($stagingSaObject -eq $null)
	{
		LogMessage -Message ("Creating Staging Storage Account Object") -Type 1 | Out-Null
		$stagingSaObject = New-AzureRmStorageAccount -Name $global:StagingStorageAcc -ResourceGroupName $global:PriResourceGroup -Type 'Standard_LRS' -Location $global:PrimaryLocation
	}
	else
	{
		LogMessage -Message ("Staging Storage Account : {0} already exist. Skipping this." -f $global:StagingStorageAcc)
	}
	LogMessage -Message ("Staging Storage Account Object:{0}" -f $stagingSaObject) -Type 1 | Out-Null
	
	$subnetRange = '10.0.0.0/24'
	$range = '10.0.0.0/16'
	$priVnetObject = Get-AzureRmVirtualNetwork -Name $global:PrimaryVnet -ResourceGroupName $global:PriResourceGroup
	LogMessage -Message ("Primary Vnet Object:{0}" -f $priVnetObject) -Type 1 | Out-Null

	# Get VNet
	$recVnetObject  = Get-AzureRmVirtualNetwork -Name $global:RecoveryVnet -ResourceGroupName $global:RecResourceGroup -ErrorAction SilentlyContinue
	if($RecVnetObject -eq $null)
	{
		$recSubnetConfig = New-AzureRmVirtualNetworkSubnetConfig -Name 'default' -AddressPrefix $subnetRange
		$recVnetObject = New-AzureRmVirtualNetwork -Name $global:RecoveryVnet -ResourceGroupName $global:RecResourceGroup -Location $global:RecoveryLocation -AddressPrefix $range -Subnet $recSubnetConfig
		LogMessage -Message ("Recovery Vent Object:{0}" -f $recVnetObject) -Type 1 | Out-Null
	}
	else
	{
		LogMessage -Message ("vnet: {0} already exist. Skipping this." -f $global:RecoveryVnet)
	}
	
    [hashtable]$script:AzureArtifactsData = @{}
	$AzureArtifactsData.Vm = $azureVm;
	$AzureArtifactsData.PriResourceGroup = $priRgObject;
	$AzureArtifactsData.RecResourceGroup = $recRgObject;
	$AzureArtifactsData.PriStorageAcc = $priSaObject;
	$AzureArtifactsData.RecStorageAcc = $recSaObject;
	$AzureArtifactsData.StagingStorageAcc = $stagingSaObject;
	$AzureArtifactsData.PriVnet = $priVnetObject;
	$AzureArtifactsData.RecVnet = $recVnetObject;
	
    DisplayAzureArtifacts -azureArtifacts $AzureArtifactsData
	
	SetAzureEnvironment -environment "PROD"
			
	return $AzureArtifactsData
}

#
# Create Zip Folder of PS Test Binaries
#
function CreateZip()
{
	$dir = $PSScriptRoot + "\DITestBinaries"
	$zipFile = $TestBinRoot + "\DITestBinaries.zip"
	LogMessage -Message ("Creating Zip File : $zipFile") -Type 1
	If(Test-path $zipFile) {Remove-item $zipFile}
	Add-Type -assembly "system.io.compression.filesystem"
	[io.compression.zipfile]::CreateFromDirectory($dir, $zipFile)
	if(!$?)
	{
		LogMessage -Message ("Failed to create Zip File : $zipFile") -Type 0
		exit 1
	}
	LogMessage -Message ("Successfully created zip file : $zipFile") -Type 1
}

#
# Downloads the blob from azure storage account and delete it from azure
#
function DownloadResults
{
	param (
		[parameter(Mandatory=$True)] [string]$storageAccountName,
		[parameter(Mandatory=$True)] [string]$storageKey,
		[parameter(Mandatory=$True)] [string]$blobName
	)
	
	$resultFile = $TestBinRoot + "\" + $blobName 
		
	if(Test-Path $resultFile)
	{
		[string]$directory = [System.IO.Path]::GetDirectoryName($resultFile);
		[string]$strippedFileName = [System.IO.Path]::GetFileNameWithoutExtension($resultFile);
		[string]$extension = [System.IO.Path]::GetExtension($resultFile);
		[string]$newFileName = $strippedFileName + "-" + [DateTime]::Now.ToString("yyyyMMdd-HHmmss") + $extension;
		[string]$newFilePath = [System.IO.Path]::Combine($LogDir, $newFileName);

		Move-Item -LiteralPath $resultFile -Destination $newFilePath;
		if ($? -eq "True")
		{
			LogMessage -Message ("Renamed resultFile to $newFilePath successfully") -Type 1
		}
		else
		{
			LogMessage -Message ("Failed to rename $resultFile to $newFilePath") -Type 0
			exit 1
		}
	}
	
	$storageContext = New-AzureStorageContext -StorageAccountName $storageAccountName -StorageAccountKey $storageKey
	
	Get-AzureStorageBlobContent -Blob $blobName -Container $ContainerName -Destination $TestBinRoot -Context $storageContext
	if( !$? )
	{
		LogMessage -Message ("Failed to download blob $blobName from azure storage account") -Type 0
		exit 1
	}
	
	LogMessage -Message ("Successfully downloaded $blobName of VM : $VMName") -Type 1
	
	Start-Sleep -seconds 60
	
	if(Test-Path $blobName)
	{
		Remove-AzureStorageBlob -Blob $blobName -Container $ContainerName -Context $storageContext
		if( !$? )
		{
			LogMessage -Message ("Failed to delete blob : $blobName from azure storage account") -Type 0
		}
	}
}

#
# Copy all the test executables to source machine
#
Function CopyDITools()
{
	try
	{
		# create zip file for di test tools
		CreateZip
		
		UploadFiles
		
		# Run scripts on VM
		$VMInfo = GetVMDetails
		if( $global:OSType -eq "windows")
		{
			$scriptName = "CopyDITools.ps1"
			$args = $Env:SystemDrive + "\TestBinRoot"
					SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
		-storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName -arguments $args
		}
	}
	Catch
	{
		LogMessage -Message ("Failed to copy di test binaries to source VM {0}" -f $VMName) -Type 0
		LogMessage -Message ("CopyDITools : ERROR : {0}" -f $_.Exception.Message) -Type 0
		exit 1
	}
}

#
# Generate churn on source machine
#
Function GenerateChurn()
{
	try
	{
		$VMInfo = GetVMDetails
		[string] $key = $VMInfo.StorageKey
		if( $global:OSType -eq "windows")
		{
			$scriptName = "ChurnGenerator.ps1"
			$blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
			$args = "$blobUrl $key ChurnData $false $TotalChurnSize $ChurnFileSize"
		}else
		{
			$scriptName = "GenerateTestTree.sh"
			$blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
			$args = "/mnt/lv3G/data,/mnt/lv4G/data 2 300 0"
			LogMessage -Message ("GenerateChurn : INFO : {0}" -f $args) -Type 1
		}
		
		# Run GenerateChurn.ps1 on Source VM and upload the tag inserted timestamp to storage account
		SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
		-storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName -arguments $args
	}
	Catch
	{
		LogMessage -Message ("Failed to generate data on VM {0}" -f $VMName) -Type 0
		LogMessage -Message ("GenerateChurn : ERROR : {0}" -f $_.Exception.Message) -Type 0
		exit 1
	}
}

Function CreateDiskLayout()
{
	try
	{
		$VMInfo = GetVMDetails
		[string] $key = $VMInfo.StorageKey
		if( $global:OSType -eq "windows")
		{
			# $scriptName = "ChurnGenerator.ps1"
			# $blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
			# $args = "$blobUrl $key ChurnData $false $TotalChurnSize $ChurnFileSize"
		}else
		{
			$scriptName = "partitioningcmd.sh"
			$blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
		}
		
		# Run partitioningcmd.sh on Source VM to create VM layout
		SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
		-storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName
	}
	Catch
	{
		LogMessage -Message ("Failed to CreateDiskLayout on VM {0}" -f $VMName) -Type 0
		LogMessage -Message ("CreateDiskLayout : ERROR : {0}" -f $_.Exception.Message) -Type 0
		exit 1
	}
}

#
# Issue tag on Source VM and upload the tag timestamp to blob
#
Function IssueTag()
{
	try
	{
		$VMInfo = GetVMDetails
		[string] $key = $VMInfo.StorageKey
			
		if( $global:OSType -eq "windows")
		{
			$scriptName = "IssueTag.ps1"
			$blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
			$args = "$blobUrl $key app"
		}
		else
		 {
			 $scriptName = "IssuetagAndUploadToStorage.sh"
			 $blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
			 $strName = $VMInfo.StorageAccount
			 LogMessage -Message ("IssueTag script is downloading from str: INFO : {0}" -f $strName) -Type 1
			 $args = "MyCustomTag $strName $key $global:ContainerName"
			 LogMessage -Message ("IssueTag : INFO : {0}" -f $args) -Type 1
		 }
		
		# Run IssueTag.ps1 on Source VM and upload the tag inserted timestamp to storage account
		SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
		-storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName -arguments $args
		
		# Download the insertedtagtimestamp file from storage account
		DownloadResults -storageAccountName $VMInfo.StorageAccount -storageKey $VMInfo.StorageKey -blobName 'taginserttimestamp.txt'
	}
	Catch
	{
		LogMessage -Message ("Failed to issue tag on VM {0}" -f $VMName) -Type 0
		LogMessage -Message ("IssueTag : ERROR : {0}" -f $_.Exception.Message) -Type 0
		exit 1
	}
}

#
# Validate churn
#
Function ValidateChurn()
{
	try
	{
		$VMInfo = GetVMDetails
		[string] $key = $VMInfo.StorageKey
			
		if( $global:OSType -eq "windows")
		{
			$scriptName = "ChurnGenerator.ps1"
			$blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
			$args = "$blobUrl $key ChurnData $true"
		}
		else
		{
			$scriptName = "VerifyLoad.sh"
			$blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
			$args = "/mnt/lv3G/data,/mnt/lv4G/data"
		}
		
		$retrycount = 1
		$result = "false"
		while($retrycount -le 3 -and $result -eq "false")
		{
			try
			{
				LogMessage -Message ("Started validate data on VM {0} , retrycount:{1}" -f $VMName, $retrycount) -Type 1
				# Run ValidateChurn.ps1 on Source VM and upload the results to storage account
				SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
				-storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName -arguments $args
				
				$result = "true"
			}
			catch
			{
				LogMessage -Message ("Failed to validate data on VM {0} on retrycount:{1}" -f $VMName, $retrycount) -Type 0
				$retrycount=$retrycount + 1
			}
			
		}
		if($result -eq "false")
		{
			LogMessage -Message ("Failed to validate data on VM {0} on retrycount:{1}. Exiting" -f $VMName, $retrycount) -Type 0
			exit 1
		}
		# Download the DI test results file from storage account
		#DownloadResults -storageAccountName $VMInfo.StorageAccount -storageKey $VMInfo.StorageKey -blobName 'DITestResult.txt'
	}
	Catch
	{
		LogMessage -Message ("Failed to validate data on VM {0}" -f $VMName) -Type 0
		LogMessage -Message ("ValidateChurn : ERROR : {0}" -f $_.Exception.Message) -Type 0
		exit 1
	}
}

function MountAttachedDataDisks
{
	try
	{
		$VMInfo = GetVMDetails
		[string] $key = $VMInfo.StorageKey
			
		if( $global:OSType -eq "windows")
		{
			# $scriptName = "ChurnGenerator.ps1"
			# $blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
			# $args = "$blobUrl $key ChurnData $true"
		}
		else
		{
			$scriptName = "MountLinuxDataDisks.sh"
			$blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
		}
		
		# Run ValidateChurn.ps1 on Source VM and upload the results to storage account
		SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
		-storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName
	}
	Catch
	{
		LogMessage -Message ("Failed to mount data datadisks on VM {0}" -f $VMName) -Type 0
		LogMessage -Message ("MountAttachedDataDisks : ERROR : {0}" -f $_.Exception.Message) -Type 0
		exit 1
	}
}
function AttachDataDisk
{
	try
	{
		UploadFiles
		
		$VMInfo = GetVMDetails
		$rgName = $VMInfo.ResourceGroup
		$vmName = $VMName
		$location = $VMInfo.Location 
		$VirtualMachine = Get-AzureRmVM -ResourceGroupName $VMInfo.ResourceGroup -Name $VMName
		if($IsManagedDisk -ne "true")
		{
			$storAccName = $VirtualMachine.StorageProfile.OsDisk.Vhd.Uri.Split("/")[2].Split(".")[0]
			$storacct = Get-AzureRmStorageAccount -ResourceGroupName $rgName -StorageAccountName $storAccName
			LogMessage -Message ("INFO:Attaching data disks on non-manageddisk type VM:{0} by fetching the data disk from StorageAccount:{1}, ResourceGroup:{2}, Location:{3}" -f $VMName, $storAccName, $rgName, $location) -Type 1
		}
		else
		{
			LogMessage -Message ("INFO:Attaching data disks on ManagedDisk type VM:{0} by fetching the data disk from ResourceGroup:{1}, Location:{2}" -f $VMName, $rgName, $location) -Type 1
		}
		
		$index = 10
		$diskSizes = (7,8)
		for($i=1; $i -le 2; $i++)
		{
			$disknameData = $vmName + '-' + 'DataDisk' + '-' + $i
			$pos = $i - 1
			if($IsManagedDisk -ne "true")
			{
				$dataDiskVhdUri = $storacct.PrimaryEndpoints.Blob.ToString() + "vhds/$disknameData.vhd"
				LogMessage -Message ("INFO: Starting attaching data disk:{0} " -f $dataDiskVhdUri) -Type 1
				$newVM = Add-AzureRmVMDataDisk -VM $VirtualMachine -VhdUri $dataDiskVhdUri -Lun $index -CreateOption Attach -Caching ReadWrite -Name $disknameData -DiskSizeInGB $diskSizes[$pos]
				LogMessage -Message ("INFO: Successfully attached data disk:{0} " -f $dataDiskVhdUri) -Type 1
			}
			else
			{
				LogMessage -Message ("INFO: Fetching data disks:{0} " -f $disknameData) -Type 1
				$disk = Get-AzureRmDisk -ResourceGroupName $rgName -DiskName $disknameData 
				LogMessage -Message ("INFO: Successfully fetched datadisk:{0}" -f $disknameData) -Type 1
				LogMessage -Message ("INFO: Starting attaching Managed data disk of ID:{0} " -f $disk.Id) -Type 1
				$newVM = Add-AzureRmVMDataDisk -CreateOption Attach -Lun $index -VM $VirtualMachine -ManagedDiskId $disk.Id -Caching ReadWrite
				LogMessage -Message ("INFO: Successfully attached Managed data disk of ID:{0} " -f $disk.Id) -Type 1
			}
			$index = $index + 1
		}
		
		$diskSizes2 = (9,10)
		$index = 0
		for($i=3; $i -le 4; $i++)
		{
			$pos = $i - 3
			$disknameData = $vmName + '-' + 'DataDisk' + '-' + $i
			if($IsManagedDisk -ne "true")
			{
				$dataDiskVhdUri = $storacct.PrimaryEndpoints.Blob.ToString() + "vhds/$disknameData.vhd"
				LogMessage -Message ("INFO: Starting attaching data disk:{0} " -f $dataDiskVhdUri) -Type 1
				$newVM = Add-AzureRmVMDataDisk -VM $VirtualMachine -VhdUri $dataDiskVhdUri -Lun $index -CreateOption Attach -Caching ReadWrite -Name $disknameData -DiskSizeInGB $diskSizes2[$pos]
				LogMessage -Message ("INFO: Successfully attached data disk:{0} " -f $dataDiskVhdUri) -Type 1
			}
			else
			{
				LogMessage -Message ("INFO: Fetching data disks:{0} " -f $disknameData) -Type 1
				$disk = Get-AzureRmDisk -ResourceGroupName $rgName -DiskName $disknameData 
				LogMessage -Message ("INFO: Successfully fetched datadisk:{0}" -f $disknameData) -Type 1
				LogMessage -Message ("INFO: Starting attaching Managed data disk of ID:{0} " -f $disk.Id) -Type 1
				$newVM = Add-AzureRmVMDataDisk -CreateOption Attach -Lun $index -VM $VirtualMachine -ManagedDiskId $disk.Id -Caching ReadWrite
				LogMessage -Message ("INFO: Successfully attached Managed data disk of ID:{0} " -f $disk.Id) -Type 1
			}
			$index = $index + 1
		}
		
		LogMessage -Message ("INFO: Updating Azure VM:{0} after attaching the data disks." -f $VMName) -Type 1
		Update-AzureRmVM -VM $VirtualMachine -ResourceGroupName $rgName
		if ( !$? )
		{
			LogMessage -Message ("ERROR : Failed to Update-AzureRmVM on VM : $VMName") -Type 0
			exit 1
		}
		else
		{
			LogMessage -Message ("INFO: Successfully updated Azure VM:{0} after attaching the data disks." -f $VMName) -Type 1
		}
		LogMessage -Message ("INFO: Mounting after attaching the data disks.") -Type 1
		MountAttachedDataDisks
		LogMessage -Message ("INFO: Successfully mounted after attaching the data disks.") -Type 1
	}
	catch
	{
		LogMessage -Message ("Failed to attach data disk on VM {0}" -f $VMName) -Type 0
		LogMessage -Message ("AttachDataDisk : ERROR : {0}" -f $_.Exception.Message) -Type 0
		exit 1
	}
}
#
# Wait for VMAgent service and VM Provisioning State 
#
Function WaitForAzureVMReadyState
{
    param(
        [parameter(Mandatory=$True)] [string] $vmName,
        [parameter(Mandatory=$True)] [string] $resourceGroupName
    )
	
	SetAzureEnvironment -environment "PROD"
		
	WaitForAzureVMProvisioningAndPowerState -vmName $VMName -resourceGroupName $resourceGroupName
	WaitForAzureVMAgentState -vmName $VMName -resourceGroupName $resourceGroupName
	
	if($global:Operation -eq "TFO")
	{
		UploadFiles

		ValidateChurn
	}
	
	if($global:Operation -ne "UNINSTALL")
	{
		SetAzureEnvironment -environment "PROD"
	}
}

#
# Uninstall Mobility Agent and Reboot the agent
#
Function UninstallAgent
{
	try
	{
		CleanAzureArtifacts
		
		$VMInfo = GetVMDetails
		
		$scriptName = "UninstallAgent.sh"
		$args = "/var/log"
		if( $global:OSType -eq "windows")
		{
			$scriptName = "UninstallAgent.ps1"
			$logDir = $Env:SystemDrive + "\ProgramData\ASRSetupLogs"
			$args = $logDir
		}
		
		# UninstallAgent on Source VM and reboot the machine
		SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
		-storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName -arguments $args
		
		RebootAzureVM
	}
	Catch
	{
		LogMessage -Message ("Failed to  uninstall Agent on VM {0}" -f $VMName) -Type 0
		LogMessage -Message ("UninstallAgent : ERROR : {0}" -f $_.Exception.Message) -Type 0
		exit 1
	}
}

#
# Get VM Information
#
Function GetVMDetails
{
    if($global:Operation -eq "TFO") { $VMName = $TFOVMName }
	
	if($ResourceGroupType.ToLower() -ne "primary")
	{
		$resourceGroupName = $global:RecResourceGroup
		$storageAccountName = $global:RecStorageAccount
		$location = $global:RecoveryLocation
	}
	else
	{
		$resourceGroupName = $PriResourceGroup
		$storageAccountName = $global:PriStorageAccount
		$location = $global:PrimaryLocation
	}
	
	if(!$IsManagedDisk)
	{
		$VM = Get-AzureRMVM -ResourceGroupName $resourceGroupName -VMName $VMName
		$storageAccountName = $VM.StorageProfile.OsDisk.Vhd.Uri.Split("/")[2].Split(".")[0]
	}
	
	$storageAccKey = GetStorageKey -resourceGroupName $resourceGroupName -storageAccountName $storageAccountName
	
	[hashtable]$VMInfo = @{}
	$VMInfo.ResourceGroup = $resourceGroupName
	$VMInfo.StorageAccount = $storageAccountName
	$VMInfo.StorageKey = $storageAccKey
	$VMInfo.Location = $location
	
	return $VMInfo
}

#
# Get storage account key
#
Function GetStorageKey
{
	param (
		[parameter(Mandatory=$True)] [string]$resourceGroupName,
		[parameter(Mandatory=$True)] [string]$storageAccountName
	)
	
	$key = Get-AzureRmStorageAccountKey -Name $storageAccountName -ResourceGroupName $resourceGroupName
	try
	{
		$storageKey = $key.value[0]
	}
	catch
	{}
	finally
	{
		if($storageKey -eq '')
		{
			$storageKey = $key.Key1
		}
	}
	
	return [string]$storageKey
}

#
# Main Block
#
Function Main()
{
	try
	{
		LogMessage -Message ("Operation : {0}" -f $global:Operation) -Type 1
		
		$ASROperationArr = @('ER', 'TFO', 'UFO', 'SP', 'FB', 'RSP', 'DR', 'CR')
		$VMOperationArr = @('REBOOT', 'COPY', 'UPLOAD', 'APPLYLOAD', 'ISSUETAG', 'VERIFYLOAD', 'UNINSTALL', 'ATTACHDISK', 'CREATEDISKLAYOUT')
		
		if($ASROperationArr.Contains($global:Operation))
		{
			SetAzureEnvironment -environment "PROD"
		}
		elseif($VMOperationArr.Contains($global:Operation))
		{
			SetAzureEnvironment -environment "PROD"
		}
		
		switch ($Operation.ToUpper()) 
		{
			'ER' 			{	EnableReplication; break	}
			'TFO'  			{ 	TestFailover; break	}
			'REBOOT'		{ 	RebootAzureVM; break	}
			'UFO'  			{ 	UnplannedFailover; break	}
			'SP'			{ 	SwitchProtection; break	}
			'FB'			{ 	Failback; break	}
			'RSP'			{ 	ReverseSwitchProtection; break	}
			'DR'			{ 	DisableReplication; break	}
			'CR'			{ 	CleanupResources; break	}
			'COPY'			{ 	CopyDITools; break	}
			'UPLOAD'		{ 	UploadFiles; break	}
			'APPLYLOAD'		{ 	GenerateChurn; break	}
			'ISSUETAG'		{ 	IssueTag; break	}
			'VERIFYLOAD' 	{ 	ValidateChurn;  break	}
			'UNINSTALL'		{ 	UninstallAgent; break	}
			'ATTACHDISK'	{ 	AttachDataDisk; break	}
			'CREATEDISKLAYOUT' {CreateDiskLayout; break }
			'ALL'			{
								UploadFiles
								CopyDITools
								EnableReplication
								GenerateChurn
								IssueTag
								TestFailover
								ValidateChurn
								UnplannedFailover
								ValidateChurn
								SwitchProtection
								GenerateChurn
								IssueTag
								Failback
								ValidateChurn
								ReverseSwitchProtection
								DisableReplication
								CleanupResources
								UninstallAgent
								break
							}
			'HELP' 			{ 	
								get-help $MyInvocation.ScriptName; 
								break 
							}
			default 		{ 	break	}
		}
		
		if($Operation -ne 'help')
		{
			LogMessage -Message ("A2A GQL TEST PASSED") -Type 1
		}
	}
	catch
	{
		LogMessage -Message ("ERROR:: {0}" -f ($Error | ConvertTo-json -Depth 1)) -Type 0

		LogMessage -Message ("A2A GQL Test Failed") -Type 1
		$errorMessage = "GIP TEST FAILED"
		throw $errorMessage;
	}
}

Main
