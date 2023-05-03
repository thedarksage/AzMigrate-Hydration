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

#Create the Policy For Protection from Vmaware to Azure
$Job = New-AzureRmRecoveryServicesAsrPolicy -Name $policyName1 -VmwareToAzure -RecoveryPointRetentionInHours 40  -RPOWarningThresholdInMinutes 5 -ApplicationConsistentSnapshotFrequencyInHours 15
WaitForJobCompletion -JobId $Job.Name