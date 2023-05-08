###
# Common functions
##

# ########### Header ###########
# Refer common library file
. $PSScriptRoot\CommonParams.ps1
. $PSScriptRoot\TestStatus.ps1
# #############################


# Get Context Vm A2A Table
# Init logger
# Log logger column value <logs>  // for error cases

# <#
# .SYNOPSIS
#     Write messages to log file
#>
 Function LogMessage {
     param(
        [string] $Message,
        [LogType] $LogType = [LogType]::Info,
        [bool] $WriteToLogFile = $true
    )

    if ($LogType -eq [LogType]::Error) {
        $logLevel = "ERROR"
        $color = "Red"
    }
    elseif ($LogType -eq [LogType]::Info1) {
        $logLevel = "INFO"
        $color = "Green"
    }
    elseif ($LogType -eq [LogType]::Info2) {
        $logLevel = "INFO"
        $color = "DarkYellow"
    }
    elseif ($LogType -eq [LogType]::Info3) {
        $logLevel = "INFO"
        $color = "Cyan"
    }
    elseif ($LogType -eq [LogType]::Warning) {
        $logLevel = "WARNING"
        $color = "Yellow"
    }
    else {
        $logLevel = "Unknown"
        $color = "Magenta"
    }

    $format = "{0,-7} : {1,-8} : {2}"
    $dateTime = Get-Date -Format "MM-dd-yyyy HH.mm.ss"
    $msg = $format -f $logLevel, $dateTime, $Message
    Write-Host $msg -ForegroundColor $color

    if ($WriteToLogFile) {
        $message = $dateTime + ' : ' + $logLevel + ' : ' + $Message
        Write-output $message | out-file $global:LogName -Append
    }
}

# <#
# .SYNOPSIS
#    Create Log directory, if not exists.
#>
Function CreateLogDir() {

    param (
        [string] $subdir
    )

    $DirName = Join-Path -Path $global:LogDir -ChildPath $subdir  

    if ( !$(Test-Path -Path $DirName -PathType Container) ) {
        Write-Host "Creating the directory $global:LogDir"
        New-Item -Path $DirName -ItemType Directory -Force >> $null

        if ( !$? ) {
            Write-Error "Error creating log directory $global:LogDir"
            exit 1
        }
    }
}

<#
.SYNOPSIS
    Create name of the log file with appropriate operation name
#>
Function CreateLogFileName()
{
   param (
       [string] $vmname,
       [string] $opname
   )

   CreateLogDir $vmname
   $global:LogName = Join-Path -Path "$global:LogDir" -ChildPath "$vmname"
   $global:LogName = Join-Path -Path "$global:LogName" -ChildPath ("$opname" + ".log")
   if ($(Test-Path -Path $global:LogName)) {
       Clear-Content -Path $global:LogName 
   }
}

# Reset log file for every operation
$global:LogName = $null

Function getRandom
{
    $rand = Get-Random -minimum 1 -maximum 10000 
    return $rand
}

Function WaitForJobCompletion
{ 
    param(
        [parameter(Mandatory=$True)] [string] $JobId,
        [int] $JobQueryWaitTimeInSeconds = 30,
        [string] $Message = "NA"
        )

        $isJobLeftForProcessing = $true;

        do
        {
            $Job = Get-AzureRMSiteRecoveryJob -Name $JobId
            $Global:colIndex = 0
            $JobFormat = `
            @{Expression={$Global:colIndex;$Global:colIndex+=1};Label="SrNo";width=4;Alignment="left";}, `
            @{Expression={$_.Name};Label="Name";width=15;Alignment="left";}, `
            @{Expression={$_.JobType};Label="JobType";width=10;Alignment="left";}, `
            @{Expression={$_.DisplayName};Label="DisplayName";width=12;Alignment="left";}, `
            @{Expression={$_.ClientRequestId};Label="ClientRequestId";width=30;Alignment="left";}, `
            @{Expression={$_.StartTime};Label="StartTime";width=10;Alignment="left";}, `
            @{Expression={$_.EndTime};Label="EndTime";width=10;Alignment="left";}, `
            @{Expression={$_.State};Label="State";width=10;Alignment="left";}, `
            @{Expression={$_.StateDescription};Label="StateDescription";width=17;Alignment="left";}
            
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
                LogMessage -Message ("{0} - {1} in-Progress...Wait: {2} Sec, ClientRequestId: {3}, JobID: {4}" -f $Job.JobType, $Job.DisplayName, $JobQueryWaitTimeInSeconds.ToString(),$Job.ClientRequestId,$Job.Name) -LogType ([LogType]::Info3);
                Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
            }
            else
            {
                ($Job | Format-Table $JobFormat -Wrap | Out-String).Trim() 
                if($Job.DisplayName -eq $null -or $Job.DisplayName -eq '')
                {
                    $messageToDisplay = $Message;
                }
                else
                {
                    $messageToDisplay = $Job.DisplayName;
                }

                if($Job.State -eq "Failed")
                {
                    LogMessage -Message ("{0}, Status: {1}" -f $messageToDisplay,$Job.State) -LogType ([LogType]::Info1);
                    LogMessage -Message ("Task Details: {0}" -f ($Job.Tasks | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info3);
                    LogMessage -Message ("Error Details: {0}" -f ($Job.Errors.ServiceErrorDetails | ConvertTo-json -Depth 1)) -LogType ([LogType]::Error);
                    $errorMessage = ($Job.Errors.ServiceErrorDetails | ConvertTo-json -Depth 1)
                    throw $errorMessage;
                }
                else 
                {
                    LogMessage -Message ("{0}, Status: {1}" -f $messageToDisplay,$Job.State) -LogType ([LogType]::Info3);
                }
                
            }
        }While($isJobLeftForProcessing)
}

Function WaitForIRCompletion
{ 
    param(
        [PSObject] $VM,
        [int] $JobQueryWaitTimeInSeconds = 60,
        [string] $provider = "A2A",
        [int] $MaxIRWaitTimeInSeconds = 60 * 60 * 10
        )

    $isProcessingLeft = $true
    $IRjobs = $null
    $StopWatch = New-Object -TypeName System.Diagnostics.Stopwatch
    $StopWatch.Start()
    LogMessage -Message ("IR in Progress..." -f $messageToDisplay,$Job.State) -LogType ([LogType]::Info1);
    
    $waitTimeCounter = 0
    do
    {
        if($provider -eq "E2E")
        {
            $IRjobs = Get-AzureRMSiteRecoveryJob -TargetObjectId $VM.Name | Sort-Object StartTime -Descending | select -First 4 | Where-Object{$_.JobType -like "*IrCompletion"}
            LogMessage -Message ("Jobs Found:{0}" -f $IRjobs.Count) -LogType ([LogType]::Info1);
        }
        elseif($provider -eq "A2A")
        {
            $IRjobs = Get-AzureRMSiteRecoveryJob | Sort-Object StartTime -Descending | select -First 2 `
            | Where-Object{$_.JobType -like "*IrCompletion"} `
            | Where-Object{$_.TargetObjectName -eq $VM.TargetObjectName }
            LogMessage -Message ("Jobs Found:{0}" -f $IRjobs.Count) -LogType ([LogType]::Info1);
        }
        if($IRjobs -eq $null -or $IRjobs.Count -lt 2)
        {
            $isProcessingLeft = $true
        }
        else
        {
            $isProcessingLeft = $false
        }

        if($isProcessingLeft)
        {
            LogMessage -Message ("Elapsed Time: {0}" -f $StopWatch.Elapsed.ToString()) -LogType ([LogType]::Info3);
            $waitTimeCounter += $JobQueryWaitTimeInSeconds
            if($waitTimeCounter -gt $MaxIRWaitTimeInSeconds)
            {
                $errorMsg = ("IR not completed after Waiting for '{0}' seconds. Failing test." -f $MaxIRWaitTimeInSeconds)
                LogMessage -Message ($errorMsg) -LogType ([LogType]::Error);
                Throw $errorMsg
            }
            LogMessage -Message ("IR in Progress.Wait: {0} Sec" -f $JobQueryWaitTimeInSeconds.ToString()) -LogType ([LogType]::Info3);
            Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
        }
    }While($isProcessingLeft)
    
    LogMessage -Message ("Total IR Time: {0}" -f $StopWatch.Elapsed.ToString()) -LogType ([LogType]::Info3);
    LogMessage -Message ("Finalize IR jobs, Job Count: {0}" -f $IRjobs.Count) -LogType ([LogType]::Info3);
    $IRjobs
    WaitForJobCompletion -JobId $IRjobs[0].Name -Message $IRjobs[0].DisplayName
    WaitForJobCompletion -JobId $IRjobs[1].Name -Message $IRjobs[1].DisplayName
    
    LogMessage -Message ("Total IR + Finalize IR Time: {0} Sec" -f $StopWatch.Elapsed.ToString()) -LogType ([LogType]::Info3);
    $StopWatch.Stop()
}

Function WaitForProtectedItemState
{ 
    param(
        [string] $state = "Protected",
        [PSObject] $containerObject,
        [int] $JobQueryWaitTimeInSeconds = 60
        )

    LogMessage -Message ("Waiting for state:{0}" -f $state) -LogType ([LogType]::Info1);
    $stateChanged = $true
    do
    {
        $protectedItemObject = Get-AzureRmSiteRecoveryReplicationProtectedItem -ProtectionContainer $containerObject -FriendlyName $VMName

        LogMessage -Message ("Current state:{0}, Waiting for state:{1}" -f $protectedItemObject.ProtectionState,$state) -LogType ([LogType]::Info1);
        if($protectedItemObject.ProtectionState -eq $state)
        {
            $stateChanged = $false
        }
        else
        {
            LogMessage -Message ("Waiting for: {0} sec before querying again." -f ($JobQueryWaitTime60Seconds * 2)) -LogType ([LogType]::Info1)
            Start-Sleep -Seconds ($JobQueryWaitTime60Seconds * 2)
        }
    }While($stateChanged)
}

#
# WaitForAzureVMAgentState - VM agent is required for enabling and executing Azure VM extensions.
# TODO : This state is not ready, sometime reboot may solve. Reboot may solve. 
# For more details : https://docs.microsoft.com/en-us/azure/virtual-machines/extensions/agent-windows
#


Function WaitForAzureVMAgentState
{ 
    param(
        [string] $state = "ProvisioningState/succeeded",
        [string] $vmName,
        [string] $resourceGroupName,
        [int] $JobQueryWaitTimeInSeconds = 30
        )

    $MaxWaitTime = 10 * 60
    LogMessage -Message ("Quering VM Agent State") -LogType ([LogType]::Info1);

    $stateChanged = $true
    $waitTimeCounter = 0
    do
    {
        $vm = Get-AzureRmVM -Name $vmName -ResourceGroupName $resourceGroupName -Status
        LogMessage -Message ("Current VM Agent state:{0}, Expected Agent state:{1}" -f $vm.VMAgent.Statuses.Code, $state) -LogType ([LogType]::Info1);
        if($vm.VMAgent.Statuses.Code -eq $state)
        {
            LogMessage -Message ("VM Agent state:{0}" -f $vm.VMAgent.Statuses.Code) -LogType ([LogType]::Info1);
            $stateChanged = $false
        }
        else
        {
            $waitTimeCounter += $JobQueryWaitTimeInSeconds
            LogMessage -Message ("Waiting for: {0} sec before querying again." -f $JobQueryWaitTimeInSeconds) -LogType ([LogType]::Info1)
            Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
        }
    }While($stateChanged -and ($waitTimeCounter -le $MaxWaitTime))

    $vm = Get-AzureRmVM -Name $vmName -ResourceGroupName $resourceGroupName -Status

    if($vm.VMAgent.Statuses.Code -ne $state)
    {
            LogMessage -Message ("VM Agent state:{0}" -f $vm.VMAgent.Statuses.Code) -LogType ([LogType]::Info1);
            $errorMsg = ("VM agent state is not in desired state even after wait time of '{0}' seconds. Failing test." -f $MaxWaitTime)
            Throw $errorMsg
    }
}

Function WaitForAzureVMProvisioningAndPowerState
{ 
    param(
        [string] $vmName,
        [string] $resourceGroupName,
        [int] $JobQueryWaitTimeInSeconds = 30
        )
    
    $provisioningState = "ProvisioningState/succeeded"
    $powerState = "PowerState/running"
    $MaxWaitTime = 20 * 60
    LogMessage -Message ("Quering VM provisioning and power State") -LogType ([LogType]::Info1);

    $stateChanged = $true
    $waitTimeCounter = 0
    do
    {
        $vm = Get-AzureRmVM -Name $vmName -ResourceGroupName $resourceGroupName -Status
        
        LogMessage -Message ("Current Vm Provisioning state:{0}, Power State:{1} ; Expected Provisioning state:{2}, Power state:{3} ; RG:{4}" -f $vm.Statuses.Code[0],$vm.Statuses.Code[1],$provisioningState,$powerState,$resourceGroupName) -LogType ([LogType]::Info1);
        if($vm.Statuses.Code[0] -eq $provisioningState -and $vm.Statuses.Code[1] -eq $powerState)
        {
            LogMessage -Message ("VM Provisioning state:{0}, Power State:{1}" -f $vm.Statuses.Code[0],$vm.Statuses.Code[1]) -LogType ([LogType]::Info1);
            $stateChanged = $false
        }
        else
        {
            $waitTimeCounter += $JobQueryWaitTimeInSeconds
            LogMessage -Message ("Waiting for: {0} sec before querying again." -f $JobQueryWaitTimeInSeconds) -LogType ([LogType]::Info1)
            Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
        }
    }While($stateChanged -and ($waitTimeCounter -le $MaxWaitTime))

    $vm = Get-AzureRmVM -Name $vmName -ResourceGroupName $resourceGroupName -Status

    if($vm.Statuses.Code[0] -ne $provisioningState -or $vm.Statuses.Code[1] -ne $powerState)
    {
            LogMessage -Message ("Current Vm Provisioning state:{0}, Power State:{1} ; Expected Provisioning state:{2}, Power state:{3} ; RG:{4}" -f $vm.Statuses.Code[0],$vm.Statuses.Code[1],$provisioningState,$powerState,$resourceGroupName) -LogType ([LogType]::Info1);
            $errorMsg = ("One of the VM state is not desired state even after wait time of '{0}' seconds. Failing test." -f $MaxWaitTime)
            Throw $errorMsg
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

    WaitForAzureVMProvisioningAndPowerState -vmName $VMName -resourceGroupName $resourceGroupName
    WaitForAzureVMAgentState -vmName $VMName -resourceGroupName $resourceGroupName

}

# Generic Retry logic functions
Function RetryCommand
{
    param (
    [Parameter(Mandatory=$true)][string]$command,
    [Parameter(Mandatory=$true)][hashtable]$args,
    [Parameter(Mandatory=$false)][int]$retries = 5,
    [Parameter(Mandatory=$false)][int]$secondsDelay = 20
    )
    
    # Usage: RetryCommand -Command 'Get-Process' -Args @{ Id = "8156" } -Verbose
    
    # Setting ErrorAction to Stop is important. This ensures any errors that occur in the command are 
    # treated as terminating errors, and will be caught by the catch block.
    $args.ErrorAction = "Stop"
    
    $retrycount = 0
    $completed = $false

    while (-not $completed) {
        try {
            & $command @args
            Write-Verbose ("Command [{0}] succeeded." -f $command)
            $completed = $true
        } catch {
            if ($retrycount -ge $retries) {
                Write-Verbose ("Command [{0}] failed the maximum number of {1} times." -f $command, $retrycount)
                throw
            } else {
                Write-Verbose ("Command [{0}] failed. Retrying in {1} seconds." -f $command, $secondsDelay)
                Start-Sleep $secondsDelay
                $retrycount++
            }
        }
    }
}

Function InvokeRetry
{
    <#
        .SYNOPSIS
            Generic retry logic
    
        .DESCRIPTION
            This command will perform the action specified until the action generates no errors, unless the retry limit has been reached.
    
        .PARAMETER command
            Accepts a ScriptBlock object.
            You can create a script block by enclosing your script within curly braces.		
    
        .PARAMETER Retry
            Number of retries to attempt.
            If set to 5, the action specified will only be performed once.
    
        .PARAMETER Delay
            The maximum delay (in seconds) between each attempt. The default is 1 second.

        .PARAMETER ShowError
            Does not suppress errors.
    
        .EXAMPLE
            $cmd = { If ((Get-Date) -lt (Get-Date -Second 59)) { Get-Object foo } Else { Write-Host 'ok' } }
            $cmd | InvokeRetry -Retry 61
            ok
    
            Description
            -----------
            The script block $cmd will generate an error when run, except when the time has reached the last second of the current minute.
    
            This command tries to invoke $cmd until there is no error, up to 60 times. So depending on when this snipplet is run, it will always succeed after some time.
    #>
    
    [CmdletBinding()]
    Param
    (
        [Parameter(Mandatory = $true, Position = 1, ValueFromPipeline = $true)]
        [ScriptBlock]$command,
    
        [Parameter(Mandatory = $false, Position = 2)]
        [ValidateRange(0, [UInt32]::MaxValue)]
        [UInt32]$Retry = 5,
    
        [Parameter(Mandatory = $false, Position = 3)]
        [ValidateRange(0, [UInt32]::MaxValue)]
        [UInt32]$Delay = 5,
    
        [Parameter(Mandatory = $false, Position = 4)]
        [Switch]$ShowError = $true
    )
<#
    Begin
    {
        $rng = New-Object 'Random'
        
        If ($MinDelay -gt $Delay)
        {
            $Delay = $MinDelay
            Write-Warning "'Delay' is increased to $MinDelay because 'MinDelay' is greater than 'Delay'."
        }
    }
#>
    Process
    {
        $succeeded = $false
        
        For ($i = 0; $i -le $Retry; $i++)
        {
            $exception = $false
            Try
            {
                Write-Verbose -Message ("Command [{0}] started. Retry: {1}" -f $command,($i+1)+'/'+$Retry) -Verbose
                & $command
                $succeeded = $true
                Write-Verbose -Message ("Command [{0}] succeeded." -f $command) -Verbose
                Break
            }
            Catch
            {
                If ($ShowError)
                {
                    Write-Error $_.Exception.Message
                }
                
                If ($Global:Error.Count -gt 0)
                {
                    $Global:Error.RemoveAt(0)
                }
                $exception = $true
            }
            
            If ($exception)
            {
                Write-Verbose -Message ("Command [{0}] failed. Retrying in {1} seconds." -f $command, $Delay) -Verbose
                Start-Sleep -Seconds $Delay
            }
        }
        
        Return $succeeded
    }
}

#
# Set Current Azure Subscription to A2A Source GQLs canary
# Note that Service Principal is created for specific to A2A Source GQLs Canary
#
 Function LoginAzureWithServicePrincipal
 {
     param(
    # Subscription ID
    [Parameter(Mandatory=$False)]
     # A2A Source GQLs Canary
    [string] $SubId = "41b6b0c9-3e3a-4701-811b-92135df8f9e3"
    )

     LogMessage -Message ('Connecting to A2A Source GQLs canary with Service Principal') -LogType ([LogType]::Info1)

     $cert = Get-ChildItem -path 'Cert:\CurrentUser\My' | where {$_.Subject -eq 'CN=SrcGqlApp1'}
     $TPrint = $cert.ThumbPrint

     if ($TPrint -eq $null) {
        $cert = Get-ChildItem -path 'Cert:\CurrentUser\My' | where {$_.Subject -eq 'CN=SrcGqlApp1'}
        Start-Sleep -Seconds 60
        $TPrint = $cert.ThumbPrint
     }
     

    if ($TPrint -ne $null) {
        Add-AzureRmAccount -ServicePrincipal -CertificateThumbprint $TPrint -ApplicationId $global:ApplicationId -Tenant $global:TenantId
        if ($?) {
            LogMessage -Message ('Able to Login to Azure through Service Principal') -LogType ([LogType]::Info1)
        } else {
            LogMessage -Message ('Unable to Login to Azure through Service Principal') -LogType ([LogType]::Error)
        }
    } else {
       LogMessage -Message ('Failed to retrieve thump print') -LogType ([LogType]::Error)
    }

    if ( !$? ) {
        $ErrorMessage = 'Unable to Login to Azure through Service Principal'
        throw $ErrorMessage
    }
 }

#
# Create Vault RG and Vault for only WTT-less "LOGIN" operation and WTT-first operation "ER"
# Also Set Azure Environment and Set Vault context
#
Function SetAzureEnvironment
{
	if ($global:Operation -eq 'CLEANPREVIOUSRUN') {
		# This is special case, this might be no-op based on the state of previous run with same configuration, handled in main function
		return
	}

	# Vault RG and Vault is expected to created a part of first operation either WTT/WTT-less run 
    if (($global:Operation -eq 'LOGIN') -or ($global:Operation -eq 'ER'))
    {
        # Create Vault RG if not exists
        $rg = Get-AzureRmResourceGroup -Name $global:VaultRGName -Location $global:VaultRGLocation -ErrorAction SilentlyContinue
        if($rg -eq $null)
        {
            LogMessage -Message ('Creating Vault Resource Group: {0}' -f $global:VaultRGName) -LogType ([LogType]::Info1)
            $vaultRGObject = New-AzureRmResourceGroup -Name $global:VaultRGName -Location $global:VaultRGLocation 
            LogMessage -Message ('Sleeping for {0} sec so that vault RG creation message can reach to ASR.' -f (60 * 2)) -LogType ([LogType]::Info1)
            Start-Sleep -Seconds (60 * 2)
        }
        else
        {
            LogMessage -Message ('Vault Resource Group: {0} already exist. Skipping this.' -f $global:VaultRGName) -LogType ([LogType]::Info1)
        }

        # Create Vault if not exists
        $Vault = Get-AzureRMRecoveryServicesVault -ResourceGroupName $global:VaultRGName -Name $global:VaultName -ErrorAction SilentlyContinue
        if($Vault -eq $null)
        {
            LogMessage -Message ('Creating Vault: {0}' -f $global:VaultName) -LogType ([LogType]::Info1)
            $Vault = New-AzureRMRecoveryServicesVault -Name $global:VaultName -ResourceGroupName $global:VaultRGName -Location $global:VaultRGLocation
            LogMessage -Message ('Sleeping for {0} sec so that vault creation message can reach to ASR.' -f (60 * 2)) -LogType ([LogType]::Info1)
            Start-Sleep -Seconds (60 * 2)
        }
        else
        {
            LogMessage -Message ('Vault: {0} already exist. Skipping this.' -f $global:VaultName) -LogType ([LogType]::Info1)
        }
    } 
    else 
    {
        $rg = Get-AzureRmResourceGroup -Name $global:VaultRGName -Location $global:VaultRGLocation -ErrorAction SilentlyContinue
        Assert-NotNull($rg)
        $Vault = Get-AzureRMRecoveryServicesVault -ResourceGroupName $global:VaultRGName -Name $global:VaultName -ErrorAction SilentlyContinue
    }

    # Set Vault Context
    LogMessage -Message ('Set Vault Context : {0}' -f $global:VaultName) -LogType ([LogType]::Info1)
    Set-AzureRmSiteRecoveryVaultSettings -ARSVault $Vault
}

#
# Enable Protection
#
Function ASRA2AEnableDR
{
    param(
        [parameter(Mandatory=$True)][string] $enableDRName,
        [parameter(Mandatory=$True)][PSObject] $containerMappingObject,
        [parameter(Mandatory=$True)][PSObject] $azureArtifacts
        )

    # Azure artifacts
    $v2VmId = $azureArtifacts.Vm.Id
    $v2TargetSA = $azureArtifacts.RecStorageAcc.Id
    $v2StagingSA = $azureArtifacts.StagingStorageAcc.Id
    $recResourceGroupId = $azureArtifacts.RecResourceGroup.ResourceId
    
    # Enable protection
    if($IsManagedDisk)
    {
        $diskIdArray = New-Object System.Collections.ArrayList 
        $diskIdArray.Add($azureArtifacts.Vm.StorageProfile.OsDisk.ManagedDisk.Id)
        $dataDisks = $azureArtifacts.Vm.StorageProfile.DataDisks
        foreach ($disk in $dataDisks)
        {
            $diskIdArray.Add($disk.ManagedDisk.Id)
        }
        
        $diskList = New-Object System.Collections.ArrayList
        foreach ($disk in $diskIdArray)
        {
            $diskInfo=New-Object Microsoft.Azure.Commands.SiteRecovery.ASRAzureToAzureManagedDiskDetails 
            $diskInfo.DiskId = $disk
            $diskInfo.PrimaryStagingAzureStorageAccountId = $v2StagingSA
            $diskInfo.RecoveryAzureResourceGroupId = $recResourceGroupId
            $diskList.Add($diskInfo)
        }
        LogMessage -Message ("Enabling Protection for Managed V2 VM: $enableDRName") -LogType ([LogType]::Info1);
        $currentJob = New-AzureRmSiteRecoveryReplicationProtectedItem -Name $enableDRName -ProtectionContainerMapping $containerMappingObject `
            -AzureVmId $v2VmId -AzureVmManagedDiskDetails $diskList -RecoveryResourceGroupId $recResourceGroupId
    }
    else
    {
        $diskURIArray = New-Object System.Collections.ArrayList 
        $diskURIArray.Add($azureArtifacts.Vm.StorageProfile.OsDisk.Vhd.Uri)
        $dataDisks = $azureArtifacts.Vm.StorageProfile.DataDisks
        foreach ($disk in $dataDisks)
        {
            $diskURIArray.Add($disk.Vhd.Uri)
        }
        
        $diskList = New-Object System.Collections.ArrayList
        foreach ($diskuri in $diskURIArray)
        {
            $diskInfo=New-Object  Microsoft.Azure.Commands.SiteRecovery.ASRAzureToAzureDiskDetails 
            $diskInfo.DiskUri = $diskuri
            $diskInfo.PrimaryStagingAzureStorageAccountId = $v2StagingSA
            $diskInfo.RecoveryAzureStorageAccountId = $v2TargetSA
            $diskList.Add($diskInfo)
        }
        
        LogMessage -Message ("Enabling Protection for V2 VM: $enableDRName") -LogType ([LogType]::Info1);
        $currentJob = New-AzureRmSiteRecoveryReplicationProtectedItem -Name $enableDRName -ProtectionContainerMapping $containerMappingObject `
            -AzureVmId $v2VmId -AzureVmDiskDetails $diskList -RecoveryResourceGroupId $recResourceGroupId 
    }
    
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTime60Seconds * 2)
    $job = Get-AzureRmSiteRecoveryJob -Name $currentJob.Name
    
    LogMessage -Message ('Job target object id : {0}, target object name : {1}' -f $job.TargetObjectId, $job.TargetObjectName) -LogType ([LogType]::Info1)
    
    $vmObject = New-Object PSObject -Property @{            
        Name       = $job.TargetObjectId
        TargetObjectName=$job.TargetObjectName
    }    
    
    WaitForIRCompletion -VM $vmObject -JobQueryWaitTimeInSeconds ($JobQueryWaitTime60Seconds * 2) -provider A2A -containerObject 
}

#
# Switch Protection
#
Function ASRA2ASwitchProtection
{
    param(
        [parameter(Mandatory=$True)][string] $switchProtectionName,
        [parameter(Mandatory=$True)][PSObject] $containerMappingObject,
        [parameter(Mandatory=$True)][PSObject] $protectedItemObject,
        [ValidateSet('PrimaryToRecovery','RecoveryToPrimary')]
        [parameter(Mandatory=$True)][String] $direction = "PrimaryToRecovery",
        [parameter(Mandatory=$True)][PSObject] $azureArtifacts
    )

    # Azure artifacts
    if($direction -eq "PrimaryToRecovery")
    {
        $azureVm = $azureArtifacts.Vm
        $recoverySA = $azureArtifacts.RecStorageAcc.Id
        $stagingSA = $azureArtifacts.StagingStorageAcc.Id
        $recResourceGroupId = $azureArtifacts.RecResourceGroup.ResourceId
    }
    else
    {
        $stagingSA = $azureArtifacts.RecStorageAcc.Id
        $recoverySA = $azureArtifacts.StagingStorageAcc.Id
        $recResourceGroupId = $azureArtifacts.PriResourceGroup.ResourceId
        $azureVm = $azureArtifacts.Vm
        #$azureVm = Get-AzureRmVM -ResourceGroupName $azureArtifacts.RecResourceGroup.ResourceGroupName -Name $VMName
    }
    $v2VmId = $azureVm.Id
    
    # Switch protection
    if($IsManagedDisk)
    {
        $diskIdArray = New-Object System.Collections.ArrayList 
        $diskIdArray.Add($azureVm.StorageProfile.OsDisk.ManagedDisk.Id)
        $dataDisks = $azureVm.StorageProfile.DataDisks
        foreach ($disk in $dataDisks)
        {
            $diskIdArray.Add($disk.ManagedDisk.Id)
        }
        
        $diskList = New-Object System.Collections.ArrayList
        foreach ($disk in $diskIdArray)
        {
            $diskInfo=New-Object Microsoft.Azure.Commands.SiteRecovery.ASRAzureToAzureManagedDiskDetails 
            $diskInfo.DiskId = $disk
            $diskInfo.PrimaryStagingAzureStorageAccountId = $stagingSA
            $diskInfo.RecoveryAzureResourceGroupId = $recResourceGroupId
            $diskList.Add($diskInfo)
        }
        
        LogMessage -Message ("Triggering Switch protection") -LogType ([LogType]::Info1);
        $currentJob = Switch-AzureRmSiteRecoveryReplicationProtectedItem -Name $switchProtectionName -ProtectionContainerMapping $containerMappingObject `
            -AzureVmManagedDiskDetails $diskList -RecoveryResourceGroupId $recResourceGroupId -ReplicationProtectedItem $protectedItemObject
    }
    else
    {
        # Populate disk info
        $diskURIArray = New-Object System.Collections.ArrayList
        $diskURIArray.Add($azureVm.StorageProfile.OsDisk.Vhd.Uri)
        $dataDisks = $azureVm.StorageProfile.DataDisks
        foreach ($disk in $dataDisks)
        {
            $diskURIArray.Add($disk.Vhd.Uri)
        }
        
        $diskList = New-Object System.Collections.ArrayList
        foreach ($diskuri in $diskURIArray)
        {
            $diskInfo=New-Object  Microsoft.Azure.Commands.SiteRecovery.ASRAzureToAzureDiskDetails 
            $diskInfo.DiskUri = $diskuri
            $diskInfo.PrimaryStagingAzureStorageAccountId = $stagingSA
            $diskInfo.RecoveryAzureStorageAccountId = $recoverySA
            $diskList.Add($diskInfo)
        }

        LogMessage -Message ("Triggering Switch protection") -LogType ([LogType]::Info1);
        $currentJob = Switch-AzureRmSiteRecoveryReplicationProtectedItem -Name $switchProtectionName -ProtectionContainerMapping $containerMappingObject `
            -AzureVmDiskDetails $diskList -RecoveryResourceGroupId $recResourceGroupId -ReplicationProtectedItem $protectedItemObject
    }
    
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTime60Seconds * 2)
    $job = Get-AzureRmSiteRecoveryJob -Name $currentJob.Name
    
    $vmObject = New-Object PSObject -Property @{            
        Name       = $job.TargetObjectId
        TargetObjectName = $job.TargetObjectName
    }    
    WaitForIRCompletion -VM $vmObject -JobQueryWaitTimeInSeconds ($JobQueryWaitTime60Seconds * 2) -provider A2A
}

#
# Remove Fabrics and Containers
#
Function ASRCheckAndRemoveFabricsNContainers
{
    # Check and remove fabric if exist exist
    $fabricObjects = Get-AzureRmSiteRecoveryFabric
    if($fabricObjects -ne $null)
    {
        # First DisableDR all VMs.
        foreach($fabricObject in $fabricObjects)
        {
            $containerObjects = Get-AzureRmSiteRecoveryProtectionContainer -Fabric $fabricObject
            foreach($containerObject in $containerObjects)
            {
                $protectedItems = Get-AzureRmSiteRecoveryReplicationProtectedItem -ProtectionContainer $containerObject
                # DisableDR all protected items
                foreach($protectedItem in $protectedItems)
                {
                    LogMessage -Message ("Triggering DisableDR for item: {0}" -f $protectedItem.Name) -LogType ([LogType]::Info1)
                    $currentJob = Remove-AzureRmSiteRecoveryReplicationProtectedItem -ReplicationProtectedItem $protectedItem -Force
                    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
                }
            }
        }

        # Remove all fabrics
        foreach($fabricObject in $fabricObjects)
        {
            if($fabricObject.FriendlyName -eq "Microsoft Azure")
            {
                continue;
            }

            $containerObjects = Get-AzureRmSiteRecoveryProtectionContainer -Fabric $fabricObject
            foreach($containerObject in $containerObjects)
            {
                $containerMappings = Get-AzureRmSiteRecoveryProtectionContainerMapping -ProtectionContainer $containerObject 
                # Remove all Container Mappings
                foreach($containerMapping in $containerMappings)
                {
                    LogMessage -Message ("Triggering Remove Container Mapping: {0}" -f $containerMapping.Name) -LogType ([LogType]::Info1)
                    $currentJob = Remove-AzureRmSiteRecoveryProtectionContainerMapping -ProtectionContainerMapping $containerMapping
                    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
                }

                # Remove container
                LogMessage -Message ("Triggering Remove Container: {0}" -f $containerObject.FriendlyName) -LogType ([LogType]::Info1)
                $currentJob = Remove-AzureRmSiteRecoveryProtectionContainer -ProtectionContainer $containerObject
                WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
            }

            # Remove Fabric
            LogMessage -Message ("Triggering Remove Fabric: {0}" -f $fabricObject.FriendlyName) -LogType ([LogType]::Info1)
            $currentJob = Remove-AzureRmSiteRecoveryFabric -Fabric $fabricObject -Force
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
        }
    }
}

#
# Upload files to azure storage container
#
function UploadFiles()
{
    try
    {
        LogMessage -Message ("Uploading GQL Binaries to storage account : {0}" -f $storageAccountName) -LogType ([LogType]::Info1)
        
        $VMInfo = GetVMDetails
        $resourceGroupName = $VMInfo.ResourceGroup
        $storageAccountName = $VMInfo.StorageAccount
        $storageAccountKey = $VMInfo.StorageKey
        
        LogMessage -Message ("StorageAccount : {0}, Container : {1}" -f $storageAccountName, $global:ContainerName) -LogType ([LogType]::Info1)
        
        $storageContext = New-AzureStorageContext -StorageAccountName $storageAccountName -StorageAccountKey $storageAccountKey
        if(!$?)
        {
            LogMessage -Message ("Failed to fetch storage context") -LogType ([LogType]::Error)	
        }
        
        $containerObj = Get-AzureStorageContainer -Context $storageContext -Name $global:ContainerName -ErrorAction SilentlyContinue
        if($containerObj -eq $null )
        {
            # Create new container
            LogMessage -Message ("Creating a new container : {0}" -f $global:ContainerName) -LogType ([LogType]::Info1)
            New-AzureStorageContainer -Name $global:ContainerName -Context $storageContext -Permission Container
            
            if(!$?)
            {
                LogMessage -Message ("Failed to create container") -LogType ([LogType]::Error)
                exit 1
            }
        } 
        else 
        { 
            LogMessage -Message ("Container : {0} already exists.. Skipping the creation" -f $global:ContainerName) -LogType ([LogType]::Info1)
        }
        
        # Upload GQL utilities
        LogMessage -Message ("Started uploading GQL Binaries to storage account : {0}" -f $storageAccountName) -LogType ([LogType]::Info1)
        $Scripts = Get-ChildItem "$PSScriptRoot\TestBinRoot\*"
        if ( !$? )
        {
            LogMessage -Message ("Config files not found") -LogType ([LogType]::Error)
            exit 1
        }
        
        foreach ( $file in $Scripts )
        {
            LogMessage -Message ("Uploading file : {0}" -f $file) -LogType ([LogType]::Info1)	
            Set-AzureStorageBlobContent -Container $global:ContainerName -File $file -Blob $file.Name -Context $storageContext -force >> $null
            if ( !$? )
            {
                LogMessage -Message ("Failed to upload file : {0} to azure" -f $file.Name) -LogType ([LogType]::Error)
                exit 1
            }
            else
            {
                LogMessage -Message ("Successfully uploaded file : {0} to azure" -f $file.Name) -LogType ([LogType]::Info1)
            }
        }
    }
    Catch
    {
        LogMessage -Message ("UploadFiles : ERROR : {0}" -f $_.Exception.Message) -LogType ([LogType]::Error)
        exit 1
    }
    
    LogMessage -Message ("Successfully uploaded files to {0} storage account" -f $storageAccountName) -LogType ([LogType]::Info1)
}

#
# Run custom script on Azure VM
#
function SetCustomScriptExtension()
{
    param (
        [parameter(Mandatory=$True)] [string]$resourceGroupName,
        [parameter(Mandatory=$True)] [string]$storageAccountName,
        [parameter(Mandatory=$True)] [string]$storageKey,
        [parameter(Mandatory=$True)] [string]$location,
        [parameter(Mandatory=$True)] [string]$scriptName,
        [parameter(Mandatory=$False)] [string]$arguments
    )
    
    try
    {
        if($Operation -eq "tfo") { $VMName = $TFOVMName }
        LogMessage -Message ("Setting Custom Extension Script for vm {0}" -f $VMName) -LogType ([LogType]::Info1)
        
        LogMessage -Message ("StorageAccount : {0}, StorageKey : {1}, Container : {2}, Args : {3}, ScriptName : {4}" `
        -f $storageAccountName, $storageKey, $global:ContainerName, $arguments, $scriptName) -LogType ([LogType]::Info1)
        
        Get-AzureRmVMExtension -ResourceGroupName $resourceGroupName -Name $global:CustomExtensionName -VMName $VMName -ErrorAction SilentlyContinue	
        if($? -eq "True")
        {
            LogMessage -Message ("Removing existing custom script extension") -LogType ([LogType]::Info1)
            Remove-AzureRmVMExtension -ResourceGroupName $resourceGroupName -Name $global:CustomExtensionName -VMName $VMName -Force
            if ( !$? )
            {
                LogMessage -Message ("ERROR : Failed to remove existing custom script extension on VM : $VMName") -LogType ([LogType]::Error)
                throw "SetCustomScriptExtension failed."
            }
            else
            {
                LogMessage -Message ("Successfully removed the existing custom script extension from vm : $VMName") -LogType ([LogType]::Info1)
            }
        }
        
        [string[]]$fileList = @()
        (Get-ChildItem -Path $TestBinRoot ).Name | foreach { $fileList += $_ }
        
        if( $global:OSType -eq "windows")
        {
            Set-AzureRmVMCustomScriptExtension -ResourceGroupName $resourceGroupName -VMName $VMName -Name $global:CustomExtensionName `
            -Location $location -StorageAccountName $storageAccountName -StorageAccountKey $storageKey -FileName $fileList `
            -ContainerName $global:ContainerName -RunFile $scriptName -Argument "$arguments"
        }
        else
        {
            $scriptBlobURL = "https://" + $storageAccountName + ".blob.core.windows.net/" + $global:ContainerName +"/"
            $scriptLocation = $scriptBlobURL + $scriptName
            $timestamp = (Get-Date).Ticks
            if($useAzureExtensionForLinux)
            {
                $extensionType = 'CustomScript'
                $publisher = 'Microsoft.Azure.Extensions'  
                $version = '2.0'
                $PublicConfiguration = @{"fileUris" = [Object[]]"$scriptLocation";"timestamp" = $timestamp}
            }
            else
            {
                $extensionType = 'CustomScriptForLinux'
                $publisher = 'Microsoft.OSTCExtensions'  
                $version = '1.5'
                $PublicConfiguration = @{"fileUris" = [Object[]]"$scriptLocation";"timestamp" = "$timestamp"}
            }
            

            $PrivateConfiguration = @{"storageAccountName" = "$storageAccountName";"storageAccountKey" = "$storageKey";"commandToExecute" = "sh $scriptName $arguments"} 
            
            Try
            {
                Set-AzureRmVMExtension -ResourceGroupName $resourceGroupName -VMName $VMName -Location $location `
                -Name $global:CustomExtensionName -Publisher $publisher -ExtensionType $extensionType -TypeHandlerVersion $version `
                -Settings $PublicConfiguration -ProtectedSettings $PrivateConfiguration
            }
            Catch
            {
                Throw $_
                throw "SetCustomScriptExtension failed."
            }
            Finally
            {
                ((Get-AzureRmVM -Name $VMName -ResourceGroupName $resourceGroupName -Status).Extensions | Where-Object {$_.Name -eq $CustomExtensionName}).Substatuses
            }
        }
        
        if( !$? )
        {
            LogMessage -Message ("Failed to set custom script extension for vm {0}" -f $VMName) -LogType ([LogType]::Error)
            throw "SetCustomScriptExtension failed."
        }
        else
        {
            LogMessage -Message ("Successfully set custom script extension for vm {0}" -f $VMName) -LogType ([LogType]::Info1)
        }
        
        $VM = Get-AzureRmVM -ResourceGroupName $resourceGroupName -Name $VMName
        
        Update-AzureRmVM -ResourceGroupName $resourceGroupName -VM $VM
        if( !$? )
        {
            LogMessage -Message ("Failed to Update Azure RM VM for vm {0}" -f $VMName) -LogType ([LogType]::Info1)
            throw "SetCustomScriptExtension failed."
        }
        
        Start-Sleep -Seconds 1
        
        $result = $status = "fail"
        $vmex = Get-AzureRmVMExtension -ResourceGroupName $resourceGroupName -VMName $VMName -Name $global:CustomExtensionName -Status
        
        if( $global:OSType -eq "windows")
        {
            $extStatus = $vmex.SubStatuses
            
            if($extStatus)
            {
                $result = $extStatus[0].Message	
                $status = $extStatus[0].DisplayStatus	
            }			
        }
        else
        {
            $extStatus = $vmex.Statuses
            if($extStatus)
            {
                $result = $extStatus.Message
                $status = $extStatus.DisplayStatus
            }
        }
        
        LogMessage -Message ("Result : {0}, Status : {1}" -f $result, $status) -LogType ([LogType]::Info1)
        
        if( $result -match "Success")
        {
            LogMessage -Message ("Successfully set custom extension on VM {0}" -f $VMName) -LogType ([LogType]::Info1)
        }
        else
        {
            LogMessage -Message ("Failed to set custom extension") -LogType ([LogType]::Error)
            throw "SetCustomScriptExtension failed."
        }
    }
    Catch
    {
            LogMessage -Message ("Failed with error - {0} for vm {1}" -f $_.Exception.Message, $VMName) -LogType ([LogType]::Error)
            throw "SetCustomScriptExtension failed."
    }
}

#
# Display azure artifacts
#
Function DisplayAzureArtifacts
{
    param(
        [parameter(Mandatory=$True)][PSObject] $azureArtifacts,
        [int] $type = 3
    )
    
    $azureArtifacts.Keys | % {
        $value = $azureArtifacts.$_.Id
        if($_ -match 'ResourceGroup')
        {
            $value = $azureArtifacts.$_.ResourceId
        }

        LogMessage -Message ("Key:{0}, value:{1}" -f $_,$value) -LogType ([LogType]::Info1)
    }
}	
    
#
# Clean azure artifacts
#
Function CleanAzureArtifacts
{
    # removing below artifacts from recovery resource group
    LogMessage -Message ("*****Removing azure artifacts from recovery RG:{0}*****" -f $global:RecResourceGroup) -LogType ([LogType]::Info1)
    $vmsToRemove = Find-AzureRmResource -ResourceGroupNameContains $global:RecResourceGroup -ResourceType "Microsoft.Compute/virtualMachines"
    LogMessage -Message ("VMs Found:{0}, ResourceGroup:{1}, removing all" -f $vmsToRemove.Count, $global:RecResourceGroup) -LogType ([LogType]::Info1)
    $vmsToRemove | Remove-AzureRmVM -Force

    $nicsToRemove = Find-AzureRmResource -ResourceGroupNameContains $global:RecResourceGroup -ResourceType "Microsoft.Network/networkInterfaces"
    LogMessage -Message ("NICs Found:{0}, ResourceGroup:{1}, removing all" -f $nicsToRemove.Count, $global:RecResourceGroup) -LogType ([LogType]::Info1)
    $nicsToRemove | Remove-AzureRmNetworkInterface -Force

    $publicIpsToRemove = Find-AzureRmResource -ResourceGroupNameContains $global:RecResourceGroup -ResourceType "Microsoft.Network/publicIPAddresses"
    LogMessage -Message ("PublicIps Found:{0}, ResourceGroup:{1}, removing all" -f $publicIpsToRemove.Count,$global:RecResourceGroup) -LogType ([LogType]::Info1)
    $publicIpsToRemove | Remove-AzureRmPublicIpAddress -Force

    $vnetsToRemove = Find-AzureRmResource -ResourceGroupNameContains $global:RecResourceGroup -ResourceType "Microsoft.Network/virtualNetworks"
    LogMessage -Message ("Vnet Found:{0}, ResourceGroup:{1}, removing all" -f $vnetsToRemove.Count, $global:RecResourceGroup) -LogType ([LogType]::Info1)
    $vnetsToRemove | Remove-AzureRmVirtualNetwork -Force

    $nsgToRemove = Find-AzureRmResource -ResourceGroupNameContains $global:RecResourceGroup -ResourceType "Microsoft.Network/networkSecurityGroups"
    LogMessage -Message ("NSGs Found:{0}, ResourceGroup:{1}, removing all" -f $nsgToRemove.Count, $global:RecResourceGroup) -LogType ([LogType]::Info1)
    $nsgToRemove | Remove-AzureRmNetworkSecurityGroup -Force
    
    LogMessage -Message ("Removing Staging Storage Account : {0}" -f $global:StagingStorageAcc) -LogType ([LogType]::Info1)
    Remove-AzureRmStorageAccount -Name $global:StagingStorageAcc -ResourceGroupName $global:PriResourceGroup -Force
    
    LogMessage -Message ("Removing Recovery ResourgeGroup:{0} and all its artifacts, Recovery Location : {1} " -f $global:RecResourceGroup, $global:RecoveryLocation) -LogType ([LogType]::Info1)
    Remove-AzureRmResourceGroup -Name $global:RecResourceGroup -Force
    
    if ($global:VaultRGLocation -ne $global:RecoveryLocation) {
        # Removing Vault resource group and Vault
        LogMessage -Message ("Removing Vault ResourceGroup:{0} Vault ResourceGroup Location : {1}" -f $global:VaultRGName, $global:VaultRGLocation) -LogType ([LogType]::Info1)
        Remove-AzureRmResourceGroup -Name $global:VaultRGName -Force
    }

    LogMessage -Message ("Removing Primary ResourgeGroup:{0} and all its artifacts, Primary Location : {1} " -f $global:PriResourceGroup, $global:PrimaryLocation) -LogType ([LogType]::Info1)
    Remove-AzureRmResourceGroup -Name $global:PriResourceGroup -Force

	LogMessage -Message ("*******Removed Azure Artifacts found(if any) as part of previous run**********" ) -LogType ([LogType]::Info1)
}

#
# Get Fabric objects
#
Function GetFabricObject
{
    param
    (
        [parameter(Mandatory=$True)][string] $fabricName
    )
    # Get Primary fabric object
    $primaryFabricObject = Get-AzureRmSiteRecoveryFabric -Name $fabricName
    
    return $primaryFabricObject
}

#
# Get protected object
#
Function GetProtectedItemObject
{
    param
    (
        [parameter(Mandatory=$True)][string] $fabricName,
        [parameter(Mandatory=$True)][string] $containerName
    )
    
    $fabricObject = Get-AzureRmSiteRecoveryFabric -Name $fabricName
    
    $containerObject = Get-AzureRMSiteRecoveryProtectionContainer -Fabric $fabricObject -FriendlyName $containerName
    LogMessage -Message ("Container: {0}, Role: {1}, AvailablePolicies: {2}" -f $containerObject.FriendlyName, $containerObject.Role, $containerObject.AvailablePolicies.FriendlyName ) -LogType ([LogType]::Info1);
    
    $protectedItemObject = Get-AzureRmSiteRecoveryReplicationProtectedItem -ProtectionContainer $containerObject | where { $_.FriendlyName -eq $global:VMName }
    LogMessage -Message ("Protected VM: {0} fetched successfully." -f $global:VMName) -LogType ([LogType]::Info1);
    
    return $protectedItemObject
}

#
# Fetch Latest recovery point
#
Function GetLatestTag
{
    param(
        [parameter(Mandatory=$True)][PSObject] $recoveryPoints
    )
    
    if($recoveryPoints.Count -eq 1)
    {
        $recoveryPoint = $recoveryPoints[0]
    }
    else
    {
        $recoveryPoint = $recoveryPoints[$recoveryPoints.Count-1]
    }
    
    return [PSObject]$recoveryPoint
}

#
# Fetch Recovery points
#
Function GetRecoveryPoint
{
    param(
        [parameter(Mandatory=$True)][PSObject] $protectedItemObject,
        [parameter(Mandatory=$True)][String] $tagType = "Latest"
    )
    
    Start-Sleep -Seconds 600
    # Wait for recovery point to generate(10mins, at least one CC)
    do
    {
        $flag = $false;
        LogMessage -Message ("Triggering Get RecoveryPoint.") -LogType ([LogType]::Info1)
        $recoveryPoints = Get-AzureRmSiteRecoveryRecoveryPoint -ReplicationProtectedItem $protectedItemObject | Sort-Object RecoveryPointTime -Descending
        if($recoveryPoints.Count -eq 0)
        {
            $flag = $true;
            LogMessage -Message ("No Recovery Points Available. Waiting for: {0} sec before querying again." -f ($JobQueryWaitTime60Seconds * 2)) -LogType ([LogType]::Info1);
            Start-Sleep -Seconds ($JobQueryWaitTime60Seconds * 2);
        }
    }while($flag)
    LogMessage -Message ("Number of RecoveryPoints found: {0}" -f $recoveryPoints.Count) -LogType ([LogType]::Info1);
    
    if($tagType -eq "Latest")
    {
        $recoveryPoint = GetLatestTag -recoveryPoints $recoveryPoints
        LogMessage -Message ("Latest recovery point : {0}" -f $recoveryPoint) -LogType ([LogType]::Info1);
    }
    else
    {
        $recoveryPoints > "$PSScriptRoot\Tags.txt"
        $timestamp = Get-Content -Path "$TestBinRoot\taginserttimestamp.txt"
        foreach ($ts in $recoveryPoints)
        {
            $recPointTime = $ts.RecoveryPointTime
            $epoch = [timezone]::CurrentTimeZone.ToLocalTime([datetime]'1/1/1970')
            $tsepoch = (New-TimeSpan -Start $epoch -End $recPointTime).TotalSeconds
            if($tsepoch -eq $timestamp) { $recoveryPoint = $ts }
        }
        #$recoveryPoint = $recoveryPoints | where { (Get-Date -Date $_.RecoveryPointTime.ToString("MM/dd/yyyy hh:mm:ss tt") -UFormat %s) -eq $timestamp}
        $recoveryPointName = $recoveryPoint.Name
        
        if(!$recoveryPointName)
        {
            $recoveryPoint = GetLatestTag -recoveryPoints $recoveryPoints
            LogMessage -Message ("Picking latest tag as custom tag is null, rec point name:{0}" -f $recoveryPoint.Name) -LogType ([LogType]::Info1);
        }
        else
        {
            LogMessage -Message ("Rec point name:{0}" -f $recoveryPointName) -LogType ([LogType]::Info1);
            #LogMessage -Message ("Rec point time:{0}" -f $recoveryPoint.RecoveryPointTime.ToString("MM/dd/yyyy hh:mm:ss tt")) -LogType ([LogType]::Info1);
        
        }
    }
    
    return [PSObject]$recoveryPoint
}
#
# This function creates and initiazes a row for a given reporting table name and VM name key
# 
#

Function InitReporting
{
        param(
            [parameter(Mandatory=$True)][String] $VmName,
            [parameter(Mandatory=$False)][String] $Scenario = "A2A_GQL",
            [parameter(Mandatory=$True)][String] $ReportingTableName
        )

    $Logger = @{}
    $Logger = TestStatusGetContext $VmNameNew $Scenario $ReportingTableName
    TestStatusInit $Logger
}

#
# This function gets the context of the row of a VM for specified Table, updates the status and log files.
# This is applicable for error operations only
#
Function ReportErrorLogs
{
        param(
            [parameter(Mandatory=$True)][String] $VmName,
            [parameter(Mandatory=$False)][String] $Scenario = "A2A_GQL",
            [parameter(Mandatory=$True)][String] $ReportingTableName,
            [parameter(Mandatory=$True)][String] $OperationName,
            [parameter(Mandatory=$False)][String] $LogName
        )

    $testErrlogs = @()
    $testErrlogs += $LogName
    $LoggerContext = TestStatusGetContext $VmName $Scenario $ReportingTableName
    TestStatusLog $LoggerContext $OperationName "FAILED" $testErrlogs
}

#
# This function gets the context of the row of a VM for specified Table, updates the status and log files.
# This is applicable for error operations only
#
Function ReportSuccess
{
        param(
            [parameter(Mandatory=$True)][String] $VmName,
            [parameter(Mandatory=$False)][String] $Scenario = "A2A_GQL",
            [parameter(Mandatory=$True)][String] $ReportingTableName,
            [parameter(Mandatory=$True)][String] $OperationName,
            [parameter(Mandatory=$False)][String] $LogName
		)

    $LoggerContext = TestStatusGetContext $VmName $Scenario $ReportingTableName

    if ($LogName.Length -eq 0) {
		TestStatusLog $LoggerContext $OperationName "PASSED"
	} else {
		$testErrlogs = @()
		$testErrlogs += $LogName
		TestStatusLog $LoggerContext $OperationName "PASSED" $testErrlogs
	}
}

#
# This is wrapper around reporting either error or success. Always use throw exception in the 'catch',
#  so that 'finally' receives error and respective operation is logged 
# Note : The GQL operation may be different from the Reporting column. For any new operation, create mapping as part of $ReportOperationMap
#


Function ReportOperationStatus
{
        param(
            [parameter(Mandatory=$True)][String] $VmName,
            [parameter(Mandatory=$False)][String] $Scenario = "A2A_GQL",
            [parameter(Mandatory=$True)][String] $ReportingTableName,
            [parameter(Mandatory=$True)][String] $OperationName,
            [parameter(Mandatory=$False)][String] $LogName
        )

    if (!$?) {
        ReportErrorLogs -VmName $VmName -ReportingTableName $ReportingTableName -OperationName $OperationName -LogName $global:LogName
        exit 1
    } else {	
	    if ($LogName.Length -eq 0) {
            ReportSuccess -VmName $VmName -ReportingTableName $ReportingTableName -OperationName $OperationName
			exit 0
		} else {
			LogMessage -Message ("Report success with logfile") -LogType ([LogType]::Info1);
			ReportSuccess -VmName $VmName -ReportingTableName $ReportingTableName -OperationName $OperationName -LogName $LogName
            exit 0
		}
    }
}

Function ReportOperationStatusInProgress
{
    param(
        [parameter(Mandatory=$True)][String] $VmName,
        [parameter(Mandatory=$False)][String] $Scenario = "A2A_GQL",
        [parameter(Mandatory=$True)][String] $ReportingTableName,
        [parameter(Mandatory=$True)][String] $OperationName
    )

	$LoggerContext = TestStatusGetContext $VmName $Scenario $ReportingTableName
	TestStatusLog $LoggerContext $OperationName "IN_PROGRESS"
}


Function CopyFile()
{
    param (
        [Parameter(Mandatory=$true)][string] $Src,
        [Parameter(Mandatory=$true)][string] $Dest
    )

    LogMessage -Message ("Copying $Src -> $Dest") -LogType ([LogType]::Info1);
    Copy-Item -Path $Src -Destination $Dest -Recurse 
}

Function GetExecLogs()
{
    LogMessage -Message ("Generating execution log") -LogType ([LogType]::Info1);
    $LogPath = Join-Path -Path $global:LogDir -ChildPath $VmName
	CreateLogFileName $VmName "Execution"
	Get-ChildItem $LogPath | Sort-Object LastWriteTime |
    Foreach-Object {
	    if ($_.FullName -ne "Execution.log") {
            Get-Content $_.FullName | Out-File $global:LogName -Append
        }
    }
}

<#
.SYNOPSIS
    Assert that a value is not $null.
#>
function Assert-NotNull {
    [CmdletBinding()] 
    Param( 
        #The value to assert. 
        [Parameter(Mandatory = $true, ValueFromPipeline = $false, Position = 0)]
        [AllowNull()] 
        [AllowEmptyCollection()] 
        [System.Object] 
        $Value
    ) 
 
    $ErrorActionPreference = [System.Management.Automation.ActionPreference]::Stop 
    if (-not $PSBoundParameters.ContainsKey('Verbose')) { 
        $VerbosePreference = $PSCmdlet.GetVariableValue('VerbosePreference') -as [System.Management.Automation.ActionPreference] 
    } 
    if (-not $PSBoundParameters.ContainsKey('Debug')) { 
        $DebugPreference = $PSCmdlet.GetVariableValue('DebugPreference') -as [System.Management.Automation.ActionPreference] 
    } 
 
    $fail = $null -eq $Value 
 
    if ($VerbosePreference -or $fail) { 
        $message = 'Assertion {0}: {1}, file {2}, line {3}' -f @( 
            $(if ($fail) { 'failed' } else { 'passed' }), 
            $MyInvocation.Line.Trim(), 
            $MyInvocation.ScriptName, 
            $MyInvocation.ScriptLineNumber 
        ) 
 
        Write-Verbose -Message $message 
 
        if ($fail) { 
            Write-Debug -Message $message 
            LogMessage -Message ('Validation failed with error : {0}' -f $message) -LogType ([LogType]::Error)
            throw New-Object -TypeName 'System.Exception' -ArgumentList @($message) 
        } 
    } 
}

#####
Write-Host "CommonFunctions.ps1 executed"
