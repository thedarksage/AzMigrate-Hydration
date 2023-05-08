. "./CommonASR.ps1"
# import common functions

$JobQueryWaitTimeInSeconds = 60

<#
.SYNOPSIS
Wait for job completion
Usage:
    WaitForJobCompletion -JobId $Job.ID
    WaitForJobCompletion -JobId $Job.ID -NumOfSecondsToWait 10
#>
function WaitForJobCompletion
{ 
    param(
        [string] $JobId,
        [int] $JobQueryWaitTimeInSeconds =$JobQueryWaitTimeInSeconds
        )
        $isJobLeftForProcessing = $true;
        do
        {
            $Job = Get-AzureRmRecoveryServicesAsrJob -Name $JobId
            $Job

            if($Job.State -eq "InProgress" -or $Job.State -eq "NotStarted")
            {
                $isJobLeftForProcessing = $true
            }
            else
            {
                $isJobLeftForProcessing = $false
            }

            if($isJobLeftForProcessing)
            {
                Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
            }
        }While($isJobLeftForProcessing)
}

<#
.SYNOPSIS
Wait for IR job completion
Usage:
    WaitForJobCompletion -VM $VM
    WaitForJobCompletion -VM $VM -NumOfSecondsToWait 10
#>
Function WaitForIRCompletion
{ 
    param(
        [PSObject] $TargetObjectId,
        [int] $JobQueryWaitTimeInSeconds = $JobQueryWaitTimeInSeconds
        )
        $isProcessingLeft = $true
        $IRjobs = $null

        do
        {
            $IRjobs = Get-AzureRmRecoveryServicesAsrJob -TargetObjectId $TargetObjectId | Sort-Object StartTime -Descending | select -First 1 | Where-Object{$_.JobType -eq "IrCompletion"}
            if($IRjobs -eq $null -or $IRjobs.Count -lt 1)
            {
                $isProcessingLeft = $true
            }
            else
            {
                $isProcessingLeft = $false
            }

            if($isProcessingLeft)
            {
                Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
            }
        }While($isProcessingLeft)

        $IRjobs
        WaitForJobCompletion -JobId $IRjobs[0].Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
}

$suffix = "Test"

$PrimaryFabricName = "V2APoc-CSPSMT"
$policyName1 = "V2aPoc" + $suffix
$policyName2 = "V2aPoc" + $suffix +"-failback"
$PrimaryProtectionContainerMapping = "pcmmapping" + $suffix
$reverseMapping = "reverseMap" + $suffix

$RecoveryAzureStorageAccountId = "/subscriptions/7c943c1b-5122-4097-90c8-861411bdd574/resourceGroups/sswcusrg/providers/Microsoft.Storage/storageAccounts/sswcusstr" 
$RecoveryResourceGroupId  = "/subscriptions/7c943c1b-5122-4097-90c8-861411bdd574/resourcegroups/sswcusrg"
$AzureVmNetworkId = "/subscriptions/7c943c1b-5122-4097-90c8-861411bdd574/resourceGroups/ERNetwork/providers/Microsoft.Network/virtualNetworks/WCUSExpressRoute-CORP-WCUS-VNET-2637"

$vCenterIpOrHostName = "10.150.208.139"
$vCenterName = "BCDR"
$Subnet = "Subnet-1"

$piNameList = "CentOS6U6"
$vmIp = "10.150.50.107"
$VmNameList = "CentOS6U6"
$piName = "CentOS6U6"


$VaultName = "V2APSTestVault"
$rgName = "sswcusrg"
$SubscriptionId = '7c943c1b-5122-4097-90c8-861411bdd574'
$UserName = "idcdlslb@microsoft.com"
$Password = '%GoldFinger-BCDR098%'
$SecPassword = ConvertTo-SecureString -AsPlainText $Password -Force
$AzureOrgIdCredential = New-Object System.Management.Automation.PSCredential -ArgumentList $UserName, $SecPassword
Login-AzureRmAccount -Credential $AzureOrgIdCredential
$context = Get-AzureRmContext
Select-AzureRmSubscription -TenantId $context.Tenant.TenantId -SubscriptionId $SubscriptionId
$Vault = Get-AzureRMRecoveryServicesVault -ResourceGroupName $rgName -Name $VaultName
########################################################################################################

Set-ASRVaultContext -vault $vault

$fabric =  Get-AsrFabric -FriendlyName $PrimaryFabricName
### vCenter 

$job = New-ASRvCenter -Fabric $fabric -Name $vCenterName -IpOrHostName $vCenterIporHostName -Port 443 -Account $fabric.FabricSpecificDetails.RunAsAccounts[0]
WaitForJobCompletion -JobId $job.name

# List all vCenter
$vCenter = Get-ASRvCenter -Fabric $fabric -Name $vCenterName 

# Update vCenter if needed
# $updateJob = Update-AzureRmRecoveryServicesAsrvCenter -InputObject $vCenter -Port 444
# WaitForJobCompletion -JobId $updatejob.name

# $updateJob = Update-AzureRmRecoveryServicesAsrvCenter -InputObject $vCenter -Account  $fabric.fabricSpecificDetails.RunAsAccounts[1]
# WaitForJobCompletion -JobId $updatejob.name

# ## Revert to original.
# $updateJob = Update-AzureRmRecoveryServicesAsrvCenter -InputObject $vCenter -Account  $fabric.fabricSpecificDetails.RunAsAccounts[0]  -Port 443
# WaitForJobCompletion -JobId $updatejob.name

### Policy

$ProtectionContainerList =  Get-ASRProtectionContainer -Fabric $fabric
$ProtectionContainer = $ProtectionContainerList[0]

#Create the Policy For Protection from Vmaware to Azure
$Job = New-AzureRmRecoveryServicesAsrPolicy -Name $policyName1 -VmwareToAzure -RecoveryPointRetentionInHours 40  -RPOWarningThresholdInMinutes 5 -ApplicationConsistentSnapshotFrequencyInHours 15
WaitForJobCompletion -JobId $Job.Name
# Get a profile created (with name ppAzure)
$Policy1 = Get-AzureRmRecoveryServicesAsrPolicy -Name $PolicyName1

#Create the Policy For Protection from Vmaware to Azure
$Job = New-AzureRmRecoveryServicesAsrPolicy -Name $policyName2 -AzureToVmware -RecoveryPointRetentionInHours 40  -RPOWarningThresholdInMinutes 5 -ApplicationConsistentSnapshotFrequencyInHours 15
WaitForJobCompletion -JobId $Job.Name

# Get a profile created (with name ppAzure)
$Policy2 = Get-AzureRmRecoveryServicesAsrPolicy -Name $PolicyName2


### Discover Protected Item by uncommenting below and Not needed if VM is auto discovered

# $job = New-AzureRmRecoveryServicesAsrProtectableItem -IPAddress $vmIp -Friendlyname $piName -OSType Linux -ProtectionContainer $ProtectionContainer
# WaitForJobCompletion -JobId $job.Name 

### Create Mapping
$pcmjob =  New-AzureRmRecoveryServicesAsrProtectionContainerMapping -Name $PrimaryProtectionContainerMapping -policy $Policy1 -PrimaryProtectionContainer $ProtectionContainer
WaitForJobCompletion -JobId $pcmjob.Name 

$pcm  = Get-AzureRmRecoveryServicesAsrProtectionContainerMapping -Name $PrimaryProtectionContainerMapping -ProtectionContainer $ProtectionContainer
$piNameList = $piNameList.split(",")

### Enable VMware Vm Replication to Azure Vm
foreach( $piName in $piNameList)
{
	$rpiName = $piName+"-RPI"
	$pi = Get-ASRProtectableItem -ProtectionContainer $ProtectionContainer -FriendlyName $piName
	$EnableDRjob = New-AzureRmRecoveryServicesAsrReplicationProtectedItem -vmwaretoazure -ProtectableItem $pi -Name $rpiName -ProtectionContainerMapping $pcm -RecoveryAzureStorageAccountId $RecoveryAzureStorageAccountId -RecoveryResourceGroupId  $RecoveryResourceGroupId -ProcessServer $fabric.fabricSpecificDetails.ProcessServers[0] -Account $fabric.fabricSpecificDetails.RunAsAccounts[0] -RecoveryAzureNetworkId $AzureVmNetworkId -RecoveryAzureSubnetName $Subnet

}

$rpiNameList = "-RPI,CentOS6U6-RPI"
$rpiNameList = $rpiNameList.split(",")

## Wait for all enable DR to complete. 
WaitForJobCompletion -JobId $EnableDRjob.Name 
 
V2AWaitForIRCompletion  -TargetObjectId $job.TargetObjectId

### UPdate RPI ##

## update Recovery VmName
$updateRPI = get-AzureRmRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -Name "CentOS6U6-RPI"
$job = Set-AzureRmRecoveryServicesAsrReplicationProtectedItem -InputObject $updateRPI -Name "Recovery1"
WaitForJobCompletion -JobId $job.Name

$updateRPI = get-AzureRmRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -Name "CentOS6U6-RPI"
$job = Set-AzureRmRecoveryServicesAsrReplicationProtectedItem -InputObject $updateRPI -RecoveryResourceGroupId $RecoveryResourceGroupId
WaitForJobCompletion -JobId $job.Name

$updateRPI = get-AzureRmRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -Name "CentOS6U6-RPI"
$updateRPI.NicDetailsList
$v = $updateRPI
Write-Host $("Started Update VM Properties") -ForegroundColor Green
$currentJob = Set-AzureRmRecoveryServicesAsrReplicationProtectedItem -ReplicationProtectedItem $v -PrimaryNic $v.NicDetailsList[0].NicId -RecoveryNetworkId $AzureVmNetworkID -RecoveryNicSubnetName $Subnet 
$currentJob
WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
Write-Host $("****************Updated VM Properties****************") -ForegroundColor Green
 $currentJob = Set-AzureRmRecoveryServicesAsrReplicationProtectedItem -ReplicationProtectedItem $v -UseManagedDisk $true
$currentJob

##################################################################################################

$rpi = get-AzureRmRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -Name $rpiName

###Test Failover

do
    {
        $rPoints = Get-ASRRecoveryPoint -ReplicationProtectedItem $rpi
        if($rpoints -and  $rpoints.count  -eq 0) {		
			#timeout 60
		}		
		else
		{
			break
		}
	}while ($rpoints.count -lt 0)
	
$tfoJob = Start-AzureRmRecoveryServicesAsrTestFailoverJob -ReplicationProtectedItem $rpi -Direction PrimaryToRecovery -AzureVMNetworkId  $AzureVMNetworkId -RecoveryPoint $rpoints[0]
WaitForJobCompletion -JobId $tfoJob.Name

	
$cleanupJob = Start-AzureRmRecoveryServicesAsrTestFailoverCleanupJob -ReplicationProtectedItem $rpi -Comments "testing done"
WaitForJobCompletion -JobId $cleanupJob.Name

### Failover
$foJob = Start-AzureRmRecoveryServicesAsrUnPlannedFailoverJob -ReplicationProtectedItem $rpi -Direction PrimaryToRecovery
WaitForJobCompletion -JobId $foJob.Name
$commitJob = Start-AzureRmRecoveryServicesAsrCommitFailoverJob -ReplicationProtectedItem $rpi 
WaitForJobCompletion -JobId $commitJob.Name

### Reprotect

# reverse mapping
$pcmjob =  New-AzureRmRecoveryServicesAsrProtectionContainerMapping -Name $reverseMapping -policy $Policy2 -PrimaryProtectionContainer $ProtectionContainer -RecoveryProtectionContainer $ProtectionContainer
WaitForJobCompletion -JobId $pcmjob.Name
$pcm = Get-ASRProtectionContainerMapping -Name $reverseMapping -ProtectionContainer $ProtectionContainer

$dataStore = "datastore1 (3)"
$dataStoreObject = New-Object Microsoft.Azure.Management.RecoveryServices.SiteRecovery.Models.DataStore -ArgumentList $dataStore
$job = Update-AzureRmRecoveryServicesAsrProtectionDirection `-AzureToVmware	-Account $fabric.FabricSpecificDetails.RunAsAccounts[2]	-DataStore $dataStoreObject -Direction RecoveryToPrimary -MasterTarget $fabric.FabricSpecificDetails.MasterTargetServers[0] -ProcessServer $fabric.FabricSpecificDetails.ProcessServers[0] -ProtectionContainerMapping $pcm `-ReplicationProtectedItem $RPI -RetentionVolume $fabric.FabricSpecificDetails.MasterTargetServers[0].RetentionVolumes[0]
WaitForJobCompletion -JobId $job.Name

### Failback
$rpi = get-AzureRmRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -Name $rpiName
$job = Start-AzureRmRecoveryServicesAsrUnPlannedFailoverJob -ReplicationProtectedItem $rpi -Direction PrimaryToRecovery
# recovery Tag  Latest is latestTime 
# default is latestTime
# "LatestTag"  is LatestAvailableApplicationConsistent

WaitForJobCompletion -JobId $Job.Name


$job = Start-AzureRmRecoveryServicesAsrCommitFailoverJob -ReplicationProtectedItem $rpi 
WaitForJobCompletion -JobId $Job.Name

$pcm = Get-ASRProtectionContainerMapping -Name $PrimaryProtectionContainerMapping -ProtectionContainer $ProtectionContainer

$job = Update-AzureRmRecoveryServicesAsrProtectionDirection -VmwareToAzure -Account $fabric.FabricSpecificDetails.RunAsAccounts[1] -Direction RecoveryToPrimary -ProcessServer $fabric.FabricSpecificDetails.ProcessServers[0] -ProtectionContainerMapping $pcm -ReplicationProtectedItem $rpi
WaitForJobCompletion -JobId $Job.Name			
			
#### After Enable DR Create RP
 $recoveryPlanName = "RPV2ACentOs6U6"
 
$rpi1 = get-AzureRmRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -Name $rpiNameList[1]
$job = New-AzureRmRecoveryServicesAsrRecoveryPlan -Name $recoveryPlanName -PrimaryFabric $Fabric -Azure -FailoverDeploymentModel ResourceManager -ReplicationProtectedItem $rpi1
$RP = Get-AsrRecoveryPlan -Name $RecoveryPlanName
$RP = Edit-ASRRecoveryPlan -RecoveryPlan $RP -AppendGroup

## Add item in RP in new Group

# $rpi2 = get-AzureRmRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -Name $rpiNameList[2]
# $RP = Edit-ASRRecoveryPlan -RecoveryPlan $RP -Group $RP.Groups[3] -AddProtectedItems $rpi2


# $rpi3 = get-AzureRmRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -Name $rpiNameList[3]
# $RP = Edit-ASRRecoveryPlan -RecoveryPlan $RP -Group $RP.Groups[3] -AddProtectedItems $rpi3

Update-ASRRecoveryPlan -RecoveryPlan $RP
### RP Test Failover
$currentJob = Start-ASRTestFailoverJob -RecoveryPlan $RP -Direction PrimaryToRecovery -AzureVMNetworkId $AzureVmNetworkId 

## Default / LatestAvailable  Recovery Tag is "LatestProcessed"
## Latest is Latest  and  LatestAvailableApplicationConsistent is "LatestApplicationConsistent"  

WaitForJobCompletion -JobId $currentJob.Name
$currentJob = Start-ASRTestFailoverCleanupJob -RecoveryPlan $RP
WaitForJobCompletion -JobId $currentJob.Name

### RP Failover
$currentJob = Start-AzureRmRecoveryServicesAsrUnPlannedFailoverJob -RecoveryPlan $RP -Direction PrimaryToRecovery
WaitForJobCompletion -JobId $currentJob.Name 

$currentJob = Start-AzureRmRecoveryServicesAsrCommitFailoverJob -RecoveryPlan $RP
$currentJob
WaitForJobCompletion -JobId $currentJob.Name

### RP Reprotect
## Not Supported


#Disable Replication
$currentJob = Remove-AzureRmRecoveryServicesAsrReplicationProtectedItem -ReplicationProtectedItem $rpi1 -Force
WaitForJobCompletion -JobId $currentJob.Name

#Remove RecoveryPlan
$currentJob = Remove-AzureRmRecoveryServicesAsrRecoveryPlan -RecoveryPlan $RP
WaitForJobCompletion -JobId $currentJob.Name