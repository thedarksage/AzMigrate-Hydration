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
Function V2AWaitForIRCompletion
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
