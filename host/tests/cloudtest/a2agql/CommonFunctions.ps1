###
# Common functions
##

# ########### Header ###########
# Refer common library file
. $PSScriptRoot\CommonParams.ps1
. $PSScriptRoot\TestStatus.ps1
# #############################

$global:LogDir = $env:LoggingDirectory
if ($global:LogDir -eq "" -Or $global:LogDir -eq $null)
{
    $global:LogDir = "$PSScriptRoot\A2AGQLLogs"
}
Write-Host "CommonFunctions : LoggingDirectory : $global:LogDir"

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

    Write-Host "logdir : $global:LogDir, subdir : $subdir"
    $DirName = Join-Path -Path $global:LogDir -ChildPath $subdir  

    if ( !$(Test-Path -Path $DirName -PathType Container) ) {
        Write-Host "Creating the directory $global:LogDir"
        New-Item -Path $DirName -ItemType Directory -Force >> $null

        if ( !$? ) {
            Write-Error "Error creating log directory $global:LogDir"
            throw "Error creating log directory $global:LogDir"
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

Function LogObject
{
    param(
		[PSObject] $Object
    )
    
    Write-Host ($Object | Format-List | Out-String)
}

# Reset log file for every operation
$global:LogName = $null

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
            $Job = Get-AzRecoveryServicesAsrJob -Name $JobId

            if($Job -eq $null)
            {
                LogMessage -Message ("Job fetched from RecoveryService was NULL for Jobid" -f $JobId) -LogType ([LogType]::Info1);
                continue;
            }

            if(($Error -ne $null) -and $Error[0].Exception.Message.Contains("ResourceNotFound") -and $Error[0].Exception.Message.Contains("vaults"))
            {
                LogMessage -Message ("Resource could not be found. Check if recovery services vault still exists.") -LogType ([LogType]::Error)
                $msg = "Resource could not be found. Check if recovery services vault still exists."
                LogMessage -Message ($msg) -LogType ([LogType]::Error)
                throw $msg
            }
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
            
            if ($Job.State -eq "InProgress" -or $Job.State -eq "NotStarted")
            {
                $isJobLeftForProcessing = $true
            }
            else
            {
                $isJobLeftForProcessing = $false
            }

            if ($isJobLeftForProcessing)
            {
                LogMessage -Message ("{0} - {1} in-Progress...Wait: {2} Sec, ClientRequestId: {3}, JobID: {4}" -f `
                $Job.JobType, $Job.DisplayName, $JobQueryWaitTimeInSeconds.ToString(),$Job.ClientRequestId,$Job.Name) -LogType ([LogType]::Info3);
                Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
            }
            else
            {
                ($Job | Format-Table $JobFormat -Wrap | Out-String).Trim() 
                if ($null -eq $Job.DisplayName -or $Job.DisplayName -eq '')
                {
                    $messageToDisplay = $Message;
                }
                else
                {
                    $messageToDisplay = $Job.DisplayName;
                }

                if ($Job.State -eq "Failed")
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
        if ($provider -eq "A2A")
        {
            $IRjobs = Get-AzRecoveryServicesAsrJob | Sort-Object StartTime -Descending | select -First 2 `
            | Where-Object{$_.JobType -like "*IrCompletion"} `
            | Where-Object{$_.TargetObjectName -eq $VM.TargetObjectName }
            LogMessage -Message ("Jobs Found:{0}" -f $IRjobs.Count) -LogType ([LogType]::Info1);
            $IRjobs| `
		    ForEach-Object{ 
			    $data = $_; 
			    LogMessage -Message ("JobDisplayName: {0}, JobName: {1}" -f $data.DisplayName,$data.Name) -LogType ([LogType]::Info1);
		     }
        }
        if ($null -eq $IRjobs -or $IRjobs.Count -lt 2)
        {
            $isProcessingLeft = $true
        }
        else
        {
            $isProcessingLeft = $false
        }

        if ($isProcessingLeft)
        {
            LogMessage -Message ("Elapsed Time: {0}" -f $StopWatch.Elapsed.ToString()) -LogType ([LogType]::Info3);
            $waitTimeCounter += $JobQueryWaitTimeInSeconds
            if ($waitTimeCounter -gt $MaxIRWaitTimeInSeconds)
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
        $protectedItemObject = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $containerObject -FriendlyName $VMName

        LogMessage -Message ("Current state:{0}, Waiting for state:{1}" -f $protectedItemObject.ProtectionState,$state) -LogType ([LogType]::Info1);
        if ($protectedItemObject.ProtectionState -eq $state)
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

    $MaxWaitTime = 30 * 60
    LogMessage -Message ("Quering VM Agent State") -LogType ([LogType]::Info1);

    $stateChanged = $true
    $waitTimeCounter = 0
    do
    {
        $vm = Get-AzVM -Name $vmName -ResourceGroupName $resourceGroupName -Status
        LogMessage -Message ("Current VM Agent state:{0}, Expected Agent state:{1}" -f $vm.VMAgent.Statuses.Code, $state) -LogType ([LogType]::Info1);
        if ($vm.VMAgent.Statuses.Code -eq $state)
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

    $vm = Get-AzVM -Name $vmName -ResourceGroupName $resourceGroupName -Status

    if ($vm.VMAgent.Statuses.Code -ne $state)
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
    $MaxWaitTime = 30 * 60
    LogMessage -Message ("Quering VM provisioning and power State") -LogType ([LogType]::Info1);

    $stateChanged = $true
    $waitTimeCounter = 0
    do
    {
        $vm = Get-AzVM -Name $vmName -ResourceGroupName $resourceGroupName -Status
        
		if ($null -ne $vm -and $null -ne $vm.Statuses -and $null -ne $vm.Statuses.Code[0] -and $null -ne $vm.Statuses.Code[1])
        {
            LogMessage -Message ("Current Vm Provisioning state:{0}, Power State:{1} ; Expected Provisioning state:{2}, Power state:{3} ; RG:{4}" -f $vm.Statuses.Code[0],$vm.Statuses.Code[1],$provisioningState,$powerState,$resourceGroupName) -LogType ([LogType]::Info1);
            if ($vm.Statuses.Code[0] -eq $provisioningState -and $vm.Statuses.Code[1] -eq $powerState)
            {
                LogMessage -Message ("VM Provisioning state:{0}, Power State:{1}" -f $vm.Statuses.Code[0],$vm.Statuses.Code[1]) -LogType ([LogType]::Info1);
                $stateChanged = $false
            }
        }
        else
        {
            $waitTimeCounter += $JobQueryWaitTimeInSeconds
            LogMessage -Message ("Waiting for: {0} sec before querying again." -f $JobQueryWaitTimeInSeconds) -LogType ([LogType]::Info1)
            Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
        }
    }While($stateChanged -and ($waitTimeCounter -le $MaxWaitTime))

    $vm = Get-AzVM -Name $vmName -ResourceGroupName $resourceGroupName -Status

    if ($vm.Statuses.Code[0] -ne $provisioningState -or $vm.Statuses.Code[1] -ne $powerState)
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

Function GetSecretFromKeyVault
{
    param(
        [Parameter(Mandatory=$True)]
        [string] $SecretName
        )

    try
    {
        LogMessage -Message ("Trying to fetch secret {0} from {1} key-vault" -f $SecretName, $global:SecretsKeyVaultName) -LogType ([LogType]::Info1)
        $secretText = Get-AzKeyVaultSecret -VaultName $global:SecretsKeyVaultName -name $SecretName -AsPlainText

        if (!$? -or $null -eq $secretText)
        {
            LogMessage -Message ("Failed to fetch secret from keyvault") -LogType ([LogType]::Error)
            Throw "Failed to fetch secret from keyvault"
        }
    }
    catch
    {
        $ErrorMessage = ('Failed to fetch secret from keyvault : {0}' -f $_.Exception.Message)
        LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
        Throw $ErrorMessage
    }

    return $secretText
}

Function LoginToAzureSubscription {
    try
    {
        LogMessage -Message ("Logging to Azure Subscription : {0}" -f $global:SubscriptionId) -LogType ([LogType]::Info1)
	
	    $retry = 1
	    $sleep = 0
	    while ($retry -ge 0) {
		    Start-Sleep -Seconds $sleep
		    $cert = Get-ChildItem -path 'cert:\LocalMachine\My' | Where-Object { $_.Subject.Contains($global:AgentSpnCertName) }
		    #LogMessage -Message ("User: {0}, Cert: {1}" -f $env:username, ($cert | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info1)
		    Start-Sleep -Seconds $sleep
		    $Thumbprint = $cert.ThumbPrint
		    if (!$cert -or !$Thumbprint) {
			    if ($retry -eq 0) {
				    LogMessage -Message ("Failed to fetch the thumbprint") -LogType ([LogType]::Error)
				    return $false
			    }
			
			    LogMessage -Message ("Failed to fetch the thumbprint..retrying") -LogType ([LogType]::Info1)
			    $retry = 0
			    $sleep = 60
		    } else {
			    break
		    }
        }

        ### Connect to the Azure Account
        LogMessage -Message ("ApplicationId: {0} and TenantId: {1}" -f $global:DrDatapPlaneAppClientId, $global:TenantId) -LogType ([LogType]::Info1)
	    Connect-AzAccount -ApplicationId $global:DrDatapPlaneAppClientId -Tenant $global:TenantId -CertificateThumbprint $Thumbprint  -Subscription $global:SubscriptionId
	    if (!$?) {
		    $ErrorMessage = ('Unable to login to Azure account using ApplicationId: {0} and TenantId: {1}' -f $global:DrDatapPlaneAppClientId, $global:TenantId)
            LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
            throw $ErrorMessage
	    }

        LogMessage -Message ("Login to Azure succeeded...Selecting subscription : {0}" -f $global:SubscriptionId ) -LogType ([LogType]::Info1)
	    Set-AzContext -SubscriptionId $global:SubscriptionId
	    if (!$?) {
		    $ErrorMessage = ('Failed to select the subscription {0}' -f $global:SubscriptionId)
            LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
            throw $ErrorMessage
	    }
    }
    catch
    {
        $ErrorMessage = ('Failed to login to Azure Subscription with error : {0}' -f $_.Exception.Message)
        LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
        throw $ErrorMessage
    }

	return $true
}

#
# Enable Protection without data disks
#
Function ASREnableProtectionWithoutDataDisks
{
    param(
        [parameter(Mandatory=$True)][PSObject] $ProtectionContainer,
        [parameter(Mandatory=$True)][PSObject] $ProtectionContainerMapping,
        [parameter(Mandatory=$True)][PSObject] $AzureArtifactsData
        )

    LogObject -Object $AzureArtifactsData
        
    $VmName = $AzureArtifactsData.Vm.Name
    $parameters = @{
        AzureVmId                   = $AzureArtifactsData.Vm.Id
        Name                        = $VmName
        ProtectionContainerMapping  = $ProtectionContainerMapping
        LogStorageAccountId         = $AzureArtifactsData.StagingStorageAcc.Id
        RecoveryResourceGroupId     = $AzureArtifactsData.RecResourceGroup.ResourceId
    }

    . {
        LogMessage -Message ("Enabling Protection for V2 Vm: {0}" -f $VmName) -LogType ([LogType]::Info1)
        LogObject -Object $parameters

        $replicationProtectedItem = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -FriendlyName $VmName -ErrorAction SilentlyContinue
        if ($null -ne $replicationProtectedItem)
        {
            LogMessage -Message ("Vm: {0} is already protected." -f $VmName) -LogType ([LogType]::Info1)
            return $replicationProtectedItem
        }

        # check if Vms which are to be protected, are in expected provisioning state and if the Vm agent is in expected state
        WaitForAzureVMReadyState -vmName $VmName -resourceGroupName $AzureArtifactsData.PriResourceGroup.ResourceGroupName

        $enableStartTime = Get-Date

        $currentJob = New-AzRecoveryServicesAsrReplicationProtectedItem `
                        -AzureToAzure `
                        @parameters

        LogObject -Object $currentJob      
        if ($currentJob)
        {
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds)
            $currentJob = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name
    
            $vmObject = New-Object PSObject -Property @{  
                Name                = $currentJob.TargetObjectId
                TargetObjectName    = $currentJob.TargetObjectName
                EnableDrTime        = $enableStartTime
            } 
            
            WaitForIRCompletion -VM $vmObject -ProtectionContainer $ProtectionContainer -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds) -provider A2A
            WaitForProtectedItemState -state "Protected" -containerObject $ProtectionContainer -vmName $VmName

            $replicationProtectedItem = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -FriendlyName $VmName
            LogMessage -Message ("Vm: {0} is protected now." -f $VmName) -LogType ([LogType]::Info1)
        }
        else
        {
            LogMessage -Message ("Error while enabling protection for Vm: {0} Aborting..." -f $VmName) -LogType ([LogType]::Error)
            Throw
        }

    } | Out-Null

    LogObject -Object $replicationProtectedItem
    return $replicationProtectedItem
}

#
# Enable Protection with data disks
#
Function ASREnableProtectionWithDataDisks
{
    param(
        [parameter(Mandatory=$True)][PSObject] $ProtectionContainer,
        [parameter(Mandatory=$True)][PSObject] $ProtectionContainerMapping,
        [parameter(Mandatory=$True)][PSObject] $AzureArtifactsData
        )

    LogObject -Object $AzureArtifactsData
        
    $VmName = $AzureArtifactsData.Vm.Name
    $enableDRName = "ER-"+$VmName
    
    . {
        LogMessage -Message ("Enabling Protection for V2 Vm: {0}" -f $VmName) -LogType ([LogType]::Info1)

        $replicationProtectedItem = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -FriendlyName $VmName -ErrorAction SilentlyContinue

        LogObject -Object $replicationProtectedItem
        if ($null -ne $replicationProtectedItem)
        {
            LogMessage -Message ("Vm: {0} is already protected. Skipping!!" -f $VmName) -LogType ([LogType]::Info1)
            return $replicationProtectedItem
        }

        # check if Vms which are to be protected, are in expected provisioning state and if the Vm agent is in expected state
        WaitForAzureVMReadyState -vmName $VmName -resourceGroupName $AzureArtifactsData.PriResourceGroup.ResourceGroupName

        # Azure artifacts
        $v2VmId = $AzureArtifactsData.Vm.Id
        $v2TargetSA = $AzureArtifactsData.RecStorageAcc.Id
        $v2StagingSA = $AzureArtifactsData.StagingStorageAcc.Id
        $recoveryResourceGroupId = $AzureArtifactsData.RecResourceGroup.ResourceId
        $enableStartTime = Get-Date

        if ($IsManagedDisk)
        {
            $disk1 = New-AzRecoveryServicesAsrAzureToAzureDiskReplicationConfig -DiskId $AzureArtifactsData.Vm.StorageProfile.OsDisk.ManagedDisk.Id -LogStorageAccountId $v2StagingSA `
                        -ManagedDisk -RecoveryReplicaDiskAccountType Standard_LRS -RecoveryResourceGroupId  $recoveryResourceGroupId -RecoveryTargetDiskAccountType Standard_LRS          

            $diskList = New-Object -TypeName 'System.Collections.ArrayList'
            $diskList.Add($disk1)

            foreach($dataDisk in $AzureArtifactsData.Vm.StorageProfile.DataDisks)
            {
                $disk1 = New-AzRecoveryServicesAsrAzureToAzureDiskReplicationConfig -DiskId $dataDisk.ManagedDisk.Id -LogStorageAccountId $v2StagingSA -ManagedDisk  `
                            -RecoveryReplicaDiskAccountType Standard_LRS -RecoveryResourceGroupId  $recoveryResourceGroupId -RecoveryTargetDiskAccountType Standard_LRS

                $diskList.Add($disk1)
            }

            LogMessage -Message ("Enabling Protection for V2 VM: $enableDRName") -LogType ([LogType]::Info1)

            $currentJob = New-AzRecoveryServicesAsrReplicationProtectedItem -Name $enableDRName -ProtectionContainerMapping $ProtectionContainerMapping `
                -AzureVmId $v2VmId -AzureToAzureDiskReplicationConfiguration $diskList -RecoveryResourceGroupId $recoveryResourceGroupId
        }
        else
        {
            $disk1 = New-AzRecoveryServicesAsrAzureToAzureDiskReplicationConfig -VhdUri $AzureArtifactsData.Vm.StorageProfile.OsDisk.Vhd.Uri `
                        -RecoveryAzureStorageAccountId $v2TargetSA -LogStorageAccountId $v2StagingSA

            $diskList = New-Object -TypeName 'System.Collections.ArrayList'
            $diskList.Add($disk1)

            foreach($dataDisk in $AzureArtifactsData.Vm.StorageProfile.DataDisks)
            {
                $disk1 = New-AzRecoveryServicesAsrAzureToAzureDiskReplicationConfig -VhdUri $dataDisk.Vhd.Uri -RecoveryAzureStorageAccountId $v2TargetSA -LogStorageAccountId $v2StagingSA

                $diskList.Add($disk1)
            }

            $currentJob = New-AzRecoveryServicesAsrReplicationProtectedItem -Name $enableDRName -ProtectionContainerMapping $ProtectionContainerMapping `
                            -AzureVmId $v2VmId -AzureToAzureDiskReplicationConfiguration $diskList -RecoveryResourceGroupId $recoveryResourceGroupId -RecoveryBootDiagStorageAccountId $v2TargetSA
        }

        LogObject -Object $currentJob      
        if ($currentJob)
        {
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTime60Seconds * 2)
            $job = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name
            
            LogMessage -Message ('Job target object id : {0}, target object name : {1}' -f $job.TargetObjectId, $job.TargetObjectName) -LogType ([LogType]::Info1)
            
            $vmObject = New-Object PSObject -Property @{            
                Name                = $job.TargetObjectId
                TargetObjectName    = $job.TargetObjectName
                EnableDrTime        = $enableStartTime
            }    
        
            LogMessage -Message ("Validate Deployment : {0}" -f $global:ValidateDeployment) -LogType ([LogType]::Info1)
            if ($global:ValidateDeployment)
            {
                LogMessage -Message ("Validating AgentVersion and ExtensionVersion") -LogType ([LogType]::Info1)

                ValidateAgentVersion

                ValidateExtensionVersion

                LogMessage -Message ("Successfully Validated AgentVersion and ExtensionVersion") -LogType ([LogType]::Info1)
            }

            GetExtensionVersion
            WaitForIRCompletion -VM $vmObject -ProtectionContainer $ProtectionContainer -JobQueryWaitTimeInSeconds ($JobQueryWaitTime60Seconds * 2) -provider A2A
            WaitForProtectedItemState -state "Protected" -containerObject $ProtectionContainer -vmName $VmName
            
            $replicationProtectedItem = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -FriendlyName $VmName
            LogMessage -Message ("Vm: {0} is protected now." -f $VmName) -LogType ([LogType]::Info2)
        }
        else
        {
            LogMessage -Message ("Error while enabling protection for Vm: {0} Aborting..." -f $VmName) -LogType ([LogType]::Error)
            Throw
        }

    } | Out-Null

    LogObject -Object $replicationProtectedItem
    return $replicationProtectedItem
}

#
# Test Failover
#
Function ASRTestFailover
{
    param(
            [parameter(Mandatory=$True)][PSObject] $ReplicationProtectedItem,
            [parameter(Mandatory=$True)][PSObject] $CurrentSourceResourceGroup,
            [parameter(Mandatory=$True)][PSObject] $CurrentTargetResourceGroup,
            [parameter(Mandatory=$True)][PSObject] $TestFailoverNetwork,
            [ValidateSet('PrimaryToRecovery','RecoveryToPrimary')]
            [parameter(Mandatory=$True)][String] $Direction,
            [parameter(Mandatory=$True)][PSObject] $RecoveryPoint,
                                        [int] $JobQueryWaitTimeInSeconds = 120
        )

    if ($null -eq $ReplicationProtectedItem -or $null -eq $CurrentSourceResourceGroup -or `
            $null -eq $CurrentTargetResourceGroup -or $null -eq $TestFailoverNetwork -or [string]::IsNullOrEmpty($Direction))
    {
        LogMessage -Message ("--ReplicationProtectedItem, CurrentSourceResourceGroup, CurrentTargetResourceGroup, TestFailoverNetwork can not be null for Vm...") -LogType ([LogType]::Error)
        throw
    }

    # check if Vms in current primary region which are to be failed over, are in expected provisioning state and if the Vm agent is in expected state
    WaitForAzureVMReadyState -vmName $ReplicationProtectedItem.FriendlyName -resourceGroupName $CurrentSourceResourceGroup

    # Test failover
    LogMessage -Message ("Triggering Test Failover for Vm: {0}, Id: {1}" -f $ReplicationProtectedItem.FriendlyName, $ReplicationProtectedItem.Name) -LogType ([LogType]::Info1)
    $currentJob = Start-AzRecoveryServicesAsrTestFailoverJob -ReplicationProtectedItem $ReplicationProtectedItem -Direction $Direction `
                    -RecoveryPoint $RecoveryPoint -AzureVMNetworkId $TestFailoverNetwork.Id 

    LogObject -Object $currentJob
    LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name,$currentJob.State) -LogType ([LogType]::Info1)

    if ($currentJob)
    {
        WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds)

        # check if test failover Vms created on target are in expected provisioning state and if the agent is in expected state
        WaitForAzureVMReadyState -vmName "$($ReplicationProtectedItem.FriendlyName)-test" -resourceGroupName $CurrentTargetResourceGroup

        LogMessage -Message ("*****Test Failover Cleanup required for Completion*****") -LogType ([LogType]::Info2)
    }
}

#
# Test Failover Cleanup
#
Function ASRTestFailoverCleanup
{
    param(
            [parameter(Mandatory=$True)][PSObject] $ReplicationProtectedItem,
                                        [int] $JobQueryWaitTimeInSeconds = 120
        )

    LogMessage -Message ("Triggering Test Failover Cleanup") -LogType ([LogType]::Info1)

	$currentJob = Start-AzRecoveryServicesAsrTestFailoverCleanupJob -ReplicationProtectedItem $ReplicationProtectedItem -Comment "Test Failover Cleanup"
    
    if ($currentJob)
    {
        LogObject -Object $currentJob
        LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name,$currentJob.State) -LogType ([LogType]::Info1)
        LogMessage -Message ("Waiting for: {0} seconds" -f (($JobQueryWaitTimeInSeconds))) -LogType ([LogType]::Info1)

        WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
        LogMessage -Message ("*****Test Failover Cleanup Completed*****") -LogType ([LogType]::Info2)
    }
    else
    {
        LogMessage -Message ("*****Test Failover Cleanup Failed*****") -LogType ([LogType]::Error)
    }
}

#
# Unplanned Failover With Recovery Point Option
#
Function ASRUnplannedFailoverWithRecoveryPoint
{
    param(
            [parameter(Mandatory=$True)][PSObject] $ReplicationProtectedItem,
            [parameter(Mandatory=$True)][PSObject] $CurrentSourceResourceGroup,
            [parameter(Mandatory=$True)][PSObject] $CurrentTargetResourceGroup,
            [ValidateSet('PrimaryToRecovery','RecoveryToPrimary')]
            [parameter(Mandatory=$True)][String] $Direction = "PrimaryToRecovery",
            [parameter(Mandatory=$True)][PSObject] $RecoveryPoint,
                                        [int] $JobQueryWaitTimeInSeconds = 120
        )

    # check if Vms in current primary region which are to be failed over, are in expected provisioning state and if the Vm agent is in expected state
    WaitForAzureVMReadyState -vmName $ReplicationProtectedItem.FriendlyName -resourceGroupName $CurrentSourceResourceGroup

    LogMessage -Message ("Triggering Unplanned Failover for Vm: {0}, Id: {1}" -f $ReplicationProtectedItem.FriendlyName,$ReplicationProtectedItem.Name) -LogType ([LogType]::Info1)
    LogObject -Object $RecoveryPoint
    $currentJob = Start-AzRecoveryServicesAsrUnPlannedFailoverJob -ReplicationProtectedItem $ReplicationProtectedItem -Direction $Direction -PerformSourceSideActions -RecoveryPoint $RecoveryPoint
    
    if ($currentJob)
    {
        LogObject -Object $currentJob
        LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name, $currentJob.State) -LogType ([LogType]::Info1)
        LogMessage -Message ("Waiting for: {0} seconds" -f (($JobQueryWaitTimeInSeconds))) -LogType ([LogType]::Info1)

        WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
        LogMessage -Message ("*****UFO finished*****") -LogType ([LogType]::Info2)
        
        # check if failover Vms created on target are in expected provisioning state and if the agent is in expected state
        WaitForAzureVMReadyState -vmName $ReplicationProtectedItem.RecoveryAzureVMName -resourceGroupName $CurrentTargetResourceGroup
        LogMessage -Message ("*****Validation finished on UFO VM*****") -LogType ([LogType]::Info2)

        LogMessage -Message ("*****Unplanned Failover Completed*****") -LogType ([LogType]::Info2)
    }
    else
    {
        LogMessage -Message ("*****Unplanned Failover Failed*****") -LogType ([LogType]::Error)
    }
}

#
# Unplanned Failover with Recovery Tag
#
Function ASRUnplannedFailoverWithRecoveryTag
{
    param(
            [parameter(Mandatory=$True)][PSObject] $ReplicationProtectedItem,
            [parameter(Mandatory=$True)][PSObject] $CurrentSourceResourceGroup,
            [parameter(Mandatory=$True)][PSObject] $CurrentTargetResourceGroup,
            [ValidateSet('PrimaryToRecovery','RecoveryToPrimary')]
            [parameter(Mandatory=$True)][String] $Direction = "PrimaryToRecovery",
            [ValidateSet('Latest', 'LatestAvailable', 'LatestAvailableApplicationConsistent', 'LatestAvailableCrashConsistent')]
            [parameter(Mandatory=$True)][string] $RecoveryTag = 'Latest',
                                        [int] $JobQueryWaitTimeInSeconds = 120
        )

    # check if Vms in current primary region which are to be failed over, are in expected provisioning state and if the Vm agent is in expected state
    WaitForAzureVMReadyState -vmName $ReplicationProtectedItem.FriendlyName -resourceGroupName $CurrentSourceResourceGroup

    LogMessage -Message ("Triggering Unplanned Failover for Vm: {0}, Id: {1} with {2}" -f $ReplicationProtectedItem.FriendlyName,$ReplicationProtectedItem.Name, $RecoveryTag) -LogType ([LogType]::Info1)
    $currentJob = Start-AzRecoveryServicesAsrUnPlannedFailoverJob -ReplicationProtectedItem $ReplicationProtectedItem -Direction $Direction -PerformSourceSideActions -RecoveryTag $RecoveryTag
    
    if ($currentJob)
    {
        LogObject -Object $currentJob
        LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name, $currentJob.State) -LogType ([LogType]::Info1)
        LogMessage -Message ("Waiting for: {0} seconds" -f (($JobQueryWaitTimeInSeconds))) -LogType ([LogType]::Info1)

        WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
        LogMessage -Message ("*****UFO finished*****") -LogType ([LogType]::Info2)
        
        # check if failover Vms created on target are in expected provisioning state and if the agent is in expected state
        WaitForAzureVMReadyState -vmName $ReplicationProtectedItem.RecoveryAzureVMName -resourceGroupName $CurrentTargetResourceGroup
        LogMessage -Message ("*****Validation finished on UFO VM*****") -LogType ([LogType]::Info2)

        LogMessage -Message ("*****Unplanned Failover Completed*****") -LogType ([LogType]::Info2)
    }
    else
    {
        LogMessage -Message ("*****Unplanned Failover Failed*****") -LogType ([LogType]::Error)
    }
}

#
# Commit Failover
#
Function CommitFailover
{
    param(
            [parameter(Mandatory=$True)][PSObject] $ReplicationProtectedItem,
                                        [int] $JobQueryWaitTimeInSeconds = 120
        )

    LogMessage -Message ("Triggering Commit after PFO for VM: {0}, Id: {1}" -f $ReplicationProtectedItem.FriendlyName, $ReplicationProtectedItem.Name) -LogType ([LogType]::Info1);
    $currentJob = Start-AzRecoveryServicesAsrCommitFailoverJob -ReplicationProtectedItem $ReplicationProtectedItem
    
    if ($currentJob)
    {
        LogObject -Object $currentJob
        LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name, $currentJob.State) -LogType ([LogType]::Info1)
        LogMessage -Message ("Waiting for: {0} seconds" -f (($JobQueryWaitTimeInSeconds))) -LogType ([LogType]::Info1)

        WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
        LogMessage -Message ("*****Commit failover Completed*****") -LogType ([LogType]::Info2)
    }
    else
    {
        LogMessage -Message ("*****Commit failover Failed*****") -LogType ([LogType]::Error)
    }
}

#
# Switch Protection
#
Function ASRSwitchProtection
{
    param(
        [parameter(Mandatory=$True)][PSObject] $ProtectionContainer,
        [parameter(Mandatory=$True)][PSObject] $ProtectionContainerMapping,
        [parameter(Mandatory=$True)][PSObject] $ReplicationProtectedItem,
        [ValidateSet('PrimaryToRecovery','RecoveryToPrimary')]
        [parameter(Mandatory=$True)][String] $Direction,
        [parameter(Mandatory=$True)][PSObject] $AzureArtifactsData
        )

    LogObject -Object $AzureArtifactsData

    $parameters = @{
        ProtectionContainerMapping  = $ProtectionContainerMapping
        ReplicationProtectedItem    = $ReplicationProtectedItem
    }

    if ($Direction -eq "PrimaryToRecovery")
    {
        $parameters.Add("LogStorageAccountId", $AzureArtifactsData.RecStorageAcc.Id)
        $parameters.Add("RecoveryResourceGroupId", $AzureArtifactsData.PriResourceGroup.ResourceId)
        $CurrentSourceResourceGroup = $AzureArtifactsData.RecResourceGroup.ResourceGroupName
    }
    else {
        $parameters.Add("LogStorageAccountId", $AzureArtifactsData.StagingStorageAcc.Id)
        $parameters.Add("RecoveryResourceGroupId", $AzureArtifactsData.RecResourceGroup.ResourceId)
        $CurrentSourceResourceGroup = $AzureArtifactsData.PriResourceGroup.ResourceGroupName
    }

    . {
        LogMessage -Message ("Reprotection for V2 Rpi: {0}" -f $ReplicationProtectedItem.FriendlyName) -LogType ([LogType]::Info1)
        LogObject -Object $parameters

        # this rpi check is on the current primary protection container
        $replicationProtectedItemAlreadyExist = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer `
                                                    -FriendlyName $ReplicationProtectedItem.RecoveryAzureVMName -ErrorAction SilentlyContinue
        if ($null -ne $replicationProtectedItemAlreadyExist)
        {
            LogMessage -Message ("Vm: {0} is already protected." -f $replicationProtectedItemAlreadyExist.FriendlyName) -LogType ([LogType]::Info1)
            return $replicationProtectedItemAlreadyExist
        }

        WaitForAzureVMReadyState -vmName $ReplicationProtectedItem.RecoveryAzureVMName -resourceGroupName $AzureArtifactsData.PriResourceGroup.ResourceGroupName

        $currentJob = Update-AzRecoveryServicesAsrProtectionDirection `
                        -AzureToAzure `
                        @parameters

        LogObject -Object $currentJob      
        if ($currentJob)
        {
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds)
            $currentJob = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name

            $vmObject = New-Object PSObject -Property @{            
                Name                = $currentJob.TargetObjectId
                TargetObjectName    = $currentJob.TargetObjectName
            }    
            
            WaitForIRCompletion -VM $vmObject -ProtectionContainer $ProtectionContainer -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds) -provider A2A
            WaitForProtectedItemState -state "Protected" -containerObject $ProtectionContainer -vmName $ReplicationProtectedItem.RecoveryAzureVMName

            $replicationProtectedItem = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -FriendlyName $ReplicationProtectedItem.RecoveryAzureVMName
            LogMessage -Message ("Vm: {0} is reprotected now." -f $replicationProtectedItem.FriendlyName) -LogType ([LogType]::Info1)
        }
        else
        {
            LogMessage -Message ("Error while reprotecting for Vm: {0} Aborting..." -f $replicationProtectedItem.FriendlyName) -LogType ([LogType]::Error)
            Throw
        }

    } | Out-Null

    LogObject -Object $replicationProtectedItem
    return $replicationProtectedItem
}

#
# Switch Protection with Data Disks
#
Function ASRSwitchProtectionWithDataDisks
{
    param(
        [parameter(Mandatory=$True)][PSObject] $ProtectionContainer,
        [parameter(Mandatory=$True)][PSObject] $ProtectionContainerMapping,
        [parameter(Mandatory=$True)][PSObject] $ReplicationProtectedItem,
        [ValidateSet('PrimaryToRecovery','RecoveryToPrimary')]
        [parameter(Mandatory=$True)][String] $Direction,
        [parameter(Mandatory=$True)][PSObject] $AzureArtifactsData
        )

    . {
        LogObject -Object $AzureArtifactsData

        LogMessage -Message ("Reprotection for V2 Rpi: {0}, direction - {1}" -f $ReplicationProtectedItem.FriendlyName, $Direction) -LogType ([LogType]::Info1)

        # This rpi check is on the current primary protection container
        $replicationProtectedItemAlreadyExist = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer `
                                                    -FriendlyName $ReplicationProtectedItem.RecoveryAzureVMName -ErrorAction SilentlyContinue
        if ($null -ne $replicationProtectedItemAlreadyExist)
        {
            LogMessage -Message ("Vm: {0} is already protected." -f $replicationProtectedItemAlreadyExist.FriendlyName) -LogType ([LogType]::Info1)
            LogObject -Object $replicationProtectedItemAlreadyExist
            return $replicationProtectedItemAlreadyExist
        }

        $azureVm = $AzureArtifactsData.Vm
        $v2VmId = $azureVm.Id
        # Azure artifacts
        if ($direction -eq "PrimaryToRecovery")
        {
            $v2TargetSA = $AzureArtifactsData.RecStorageAcc.Id
            $v2StagingSA = $AzureArtifactsData.StagingStorageAcc.Id
            $recoveryResourceGroupId = $AzureArtifactsData.RecResourceGroup.ResourceId
            $resourceGroupName = $AzureArtifactsData.PriResourceGroup.ResourceGroupName
        }
        else
        {
            $v2StagingSA = $AzureArtifactsData.RecStorageAcc.Id
            $v2TargetSA = $AzureArtifactsData.StagingStorageAcc.Id
            $recoveryResourceGroupId = $AzureArtifactsData.PriResourceGroup.ResourceId
            $resourceGroupName = $AzureArtifactsData.RecResourceGroup.ResourceGroupName
        }
        Write-Host "ResourceGroup : $resourceGroupName"
        Write-Host "v2TargetSA : $v2TargetSA"
        Write-Host "v2StagingSA : $v2StagingSA"
        Write-Host "recoveryResourceGroupId : $recoveryResourceGroupId"
        WaitForAzureVMReadyState -vmName $ReplicationProtectedItem.RecoveryAzureVMName -resourceGroupName $resourceGroupName

        LogMessage -Message ("For VM Id: {0}" -f $v2VmId) -LogType ([LogType]::Info1)

        if ($IsManagedDisk)
        {
            $diskDetails = New-AzRecoveryServicesAsrAzureToAzureDiskReplicationConfig -DiskId $azureVm.StorageProfile.OsDisk.ManagedDisk.Id `
                                -LogStorageAccountId $v2StagingSA -ManagedDisk -RecoveryReplicaDiskAccountType Standard_LRS `
                                -RecoveryResourceGroupId $recoveryResourceGroupId -RecoveryTargetDiskAccountType Standard_LRS

            $diskList = New-Object -TypeName 'System.Collections.ArrayList'
            $diskList.Add($diskDetails)

            foreach($dataDisk in $azureVm.StorageProfile.DataDisks)
            {
                $diskDetails = New-AzRecoveryServicesAsrAzureToAzureDiskReplicationConfig -DiskId $dataDisk.ManagedDisk.Id -LogStorageAccountId $v2StagingSA `
                                    -ManagedDisk -RecoveryReplicaDiskAccountType Standard_LRS -RecoveryResourceGroupId $recoveryResourceGroupId -RecoveryTargetDiskAccountType Standard_LRS
                $diskList.Add($diskDetails)
            }

            LogMessage -Message ("Reprotection with managed disks for V2 Rpi: {0}" -f $ReplicationProtectedItem.FriendlyName) -LogType ([LogType]::Info1)
            $currentJob = Update-AzRecoveryServicesAsrProtectionDirection -AzureToAzure -AzureToAzureDiskReplicationConfiguration $diskList `
                            -ProtectionContainerMapping $ProtectionContainerMapping -ReplicationProtectedItem $ReplicationProtectedItem -RecoveryResourceGroupId $recoveryResourceGroupId
        }
        else
        {
            # Populate disk info
            $disk1 = New-AzRecoveryServicesAsrAzureToAzureDiskReplicationConfig -VhdUri $AzureArtifactsData.Vm.StorageProfile.OsDisk.Vhd.Uri `
                        -RecoveryAzureStorageAccountId $v2TargetSA -LogStorageAccountId $v2StagingSA

            $diskList = New-Object -TypeName 'System.Collections.ArrayList'
            $diskList.Add($disk1)

            foreach($dataDisk in $AzureArtifactsData.Vm.StorageProfile.DataDisks)
            {
                $disk1 = New-AzRecoveryServicesAsrAzureToAzureDiskReplicationConfig -VhdUri $dataDisk.Vhd.Uri -RecoveryAzureStorageAccountId $v2TargetSA -LogStorageAccountId $v2StagingSA

                $diskList.Add($disk1)
            }

            $currentJob = New-AzRecoveryServicesAsrReplicationProtectedItem -Name $enableDRName -ProtectionContainerMapping $ProtectionContainerMapping `
                            -AzureVmId $v2VmId -AzureToAzureDiskReplicationConfiguration $diskList -RecoveryResourceGroupId $recoveryResourceGroupId -RecoveryBootDiagStorageAccountId $v2TargetSA

            LogMessage -Message ("Reprotection with unmanaged disks for V2 Rpi: {0}" -f $ReplicationProtectedItem.FriendlyName) -LogType ([LogType]::Info1)
            $spName = "SP-" + $replicationProtectedItemAlreadyExist.FriendlyName
            $currentJob = Switch-AzRecoveryServicesAsrReplicationProtectedItem -Name $spName -ProtectionContainerMapping $ProtectionContainerMapping `
                            -AzureToAzureDiskReplicationConfiguration $diskList -RecoveryResourceGroupId $recoveryResourceGroupId -ReplicationProtectedItem $ReplicationProtectedItem

            #$currentJob = Update-AzRecoveryServicesAsrProtectionDirection -AzureToAzure -ProtectionContainerMapping $ProtectionContainerMapping `
            #                -RecoveryResourceGroupId $recoveryResourceGroupId -ReplicationProtectedItem $ReplicationProtectedItem
        }

        LogObject -Object $currentJob      
        if ($currentJob)
        {
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTime60Seconds * 2)
            $job = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name

            $vmObject = New-Object PSObject -Property @{            
                Name       = $job.TargetObjectId
                TargetObjectName=$job.TargetObjectName
            }
                    
            WaitForIRCompletion -VM $vmObject -JobQueryWaitTimeInSeconds ($JobQueryWaitTime60Seconds * 2) -provider A2A
            # Wait for VM state to change to protected
            WaitForProtectedItemState -state "Protected" -containerObject $ProtectionContainer -vmName $ReplicationProtectedItem.RecoveryAzureVMName

            $replicationProtectedItem = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $ProtectionContainer -FriendlyName $ReplicationProtectedItem.RecoveryAzureVMName
            LogMessage -Message ("Vm: {0} is reprotected now." -f $replicationProtectedItem.FriendlyName) -LogType ([LogType]::Info1)
        }else
        {
            LogMessage -Message ("Error while reprotecting for Vm: {0} Aborting..." -f $replicationProtectedItem.FriendlyName) -LogType ([LogType]::Error)
            Throw
        }
    } | Out-Null

    LogObject -Object $replicationProtectedItem
    return $replicationProtectedItem
}

Function DisableProtectionForFabric
{
    param(
            [parameter(Mandatory=$True)][PSObject] $fabricName,
            [parameter(Mandatory=$True)][PSObject] $containerName
        )

    if ([string]::IsNullOrEmpty($fabricName) -or [string]::IsNullOrEmpty($containerName))
    {
        LogMessage -Message ("--Fabric Name and containerName can not be null...") -LogType ([LogType]::Error)
        throw "Fabric Name and containerName can not be null or empty."
    }

    . {
        LogMessage -Message ("Triggering Disable Protection for VM: {0}, fabric : {1}, container : {2}" -f $global:VMName,$fabricName, $containerName) -LogType ([LogType]::Info1)
        $protectedItemObject = GetProtectedItemObject -fabricName $fabricName -containerName $containerName -throwIfNotFound $false
        if ($null -eq $protectedItemObject)
        {
            LogMessage -Message ("VM: {0} is not in enabled list.Skipping disable for this VM" -f $global:VMName) -LogType ([LogType]::Warning)
        }
        else
        {
            DisableProtection -ReplicationProtectedItem $protectedItemObject
        }
    } | Out-Null
}

Function DisableProtection
{
    param(
            [parameter(Mandatory=$True)][PSObject] $ReplicationProtectedItem,
                                        [int]      $JobQueryWaitTimeInSeconds = 120
        )

    if ($null -eq $ReplicationProtectedItem)
    {
        LogMessage -Message ("Vm: {0} is not in enabled list. Skipping disable for this Vm" -f $ReplicationProtectedItem.FriendlyName) -LogType ([LogType]::Info1)
        return;
    }

    LogMessage -Message ("Triggering Disable Protection for Vm: {0}" -f $ReplicationProtectedItem.FriendlyName) -LogType ([LogType]::Info1)
    $currentJob = Remove-AzRecoveryServicesAsrReplicationProtectedItem -ReplicationProtectedItem $ReplicationProtectedItem -Force
    
    LogObject -Object $currentJob

    if ($currentJob)
    {
        LogMessage -Message ("JobId: {0}, JobStatus: {1}" -f $currentJob.Name,$currentJob.State) -LogType ([LogType]::Info1)
        WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
        LogMessage -Message ("*****Disable DR finished*****") -LogType ([LogType]::Info2)
    }
    else
    {
        LogMessage -Message ("Error while disabling protection for Vm: {0} Aborting..." -f $ReplicationProtectedItem.FriendlyName) -LogType ([LogType]::Error)
        Throw
    }
}

#
# Remove Fabrics and Containers
#
Function ASRCheckAndRemoveFabricsNContainers
{
    # Check and remove fabric if exist exist
    $fabricObjects = Get-AzRecoveryServicesAsrFabric
    if ($fabricObjects -ne $null)
    {
        # First DisableDR all VMs.
        foreach($fabricObject in $fabricObjects)
        {
            $containerObjects = Get-AzRecoveryServicesAsrProtectionContainer -Fabric $fabricObject
            foreach($containerObject in $containerObjects)
            {
                $protectedItems = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $containerObject
                # DisableDR all protected items
                foreach($protectedItem in $protectedItems)
                {
                    DisableProtection -ReplicationProtectedItem $protectedItem
                }
            }
        }

        # Remove all fabrics
        foreach($fabricObject in $fabricObjects)
        {
            if ($fabricObject.FriendlyName -eq "Microsoft Azure")
            {
                continue;
            }

            $containerObjects = Get-AzRecoveryServicesAsrProtectionContainer -Fabric $fabricObject
            foreach($containerObject in $containerObjects)
            {
                $containerMappings = Get-AzRecoveryServicesAsrProtectionContainerMapping -ProtectionContainer $containerObject 
                # Remove all Container Mappings
                foreach($containerMapping in $containerMappings)
                {
                    LogMessage -Message ("Triggering Remove Container Mapping: {0}" -f $containerMapping.Name) -LogType ([LogType]::Info1)
                    $currentJob = Remove-AzRecoveryServicesAsrProtectionContainerMapping -ProtectionContainerMapping $containerMapping
                    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
                }

                # Remove container
                LogMessage -Message ("Triggering Remove Container: {0}" -f $containerObject.FriendlyName) -LogType ([LogType]::Info1)
                $currentJob = Remove-AzRecoveryServicesAsrProtectionContainer -ProtectionContainer $containerObject
                WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
            }

            if ($null -ne $fabricObject)
            {
                # Remove Fabric
                DeleteFabric -Fabric $fabricObject
            }
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
        if (!$?)
        {
            LogMessage -Message ("Failed to fetch storage context") -LogType ([LogType]::Error)	
        }
        
        $containerObj = Get-AzureStorageContainer -Context $storageContext -Name $global:ContainerName -ErrorAction SilentlyContinue
        if ($containerObj -eq $null )
        {
            # Create new container
            LogMessage -Message ("Creating a new container : {0}" -f $global:ContainerName) -LogType ([LogType]::Info1)
            New-AzureStorageContainer -Name $global:ContainerName -Context $storageContext -Permission Container
            
            if (!$?)
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
        if ($Operation -eq "tfo") { $VMName = $TFOVMName }
        LogMessage -Message ("Setting Custom Extension Script for vm {0}" -f $VMName) -LogType ([LogType]::Info1)
        
        LogMessage -Message ("StorageAccount : {0}, StorageKey : {1}, Container : {2}, Args : {3}, ScriptName : {4}" `
        -f $storageAccountName, $storageKey, $global:ContainerName, $arguments, $scriptName) -LogType ([LogType]::Info1)
        
        Get-AzVMExtension -ResourceGroupName $resourceGroupName -Name $global:CustomExtensionName -VMName $VMName -ErrorAction SilentlyContinue	
        if ($? -eq "True")
        {
            LogMessage -Message ("Removing existing custom script extension") -LogType ([LogType]::Info1)
            Remove-AzVMExtension -ResourceGroupName $resourceGroupName -Name $global:CustomExtensionName -VMName $VMName -Force
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
        
        if ( $global:OSType -eq "windows")
        {
            Set-AzVMCustomScriptExtension -ResourceGroupName $resourceGroupName -VMName $VMName -Name $global:CustomExtensionName `
            -Location $location -StorageAccountName $storageAccountName -StorageAccountKey $storageKey -FileName $fileList `
            -ContainerName $global:ContainerName -RunFile $scriptName -Argument "$arguments"
        }
        else
        {
            $scriptBlobURL = "https://" + $storageAccountName + ".blob.core.windows.net/" + $global:ContainerName +"/"
            $scriptLocation = $scriptBlobURL + $scriptName
            $timestamp = (Get-Date).Ticks
            if ($useAzureExtensionForLinux)
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
                Set-AzVMExtension -ResourceGroupName $resourceGroupName -VMName $VMName -Location $location `
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
                ((Get-AzVM -Name $VMName -ResourceGroupName $resourceGroupName -Status).Extensions | Where-Object {$_.Name -eq $CustomExtensionName}).Substatuses
            }
        }
        
        if ( !$? )
        {
            LogMessage -Message ("Failed to set custom script extension for vm {0}" -f $VMName) -LogType ([LogType]::Error)
            throw "SetCustomScriptExtension failed."
        }
        else
        {
            LogMessage -Message ("Successfully set custom script extension for vm {0}" -f $VMName) -LogType ([LogType]::Info1)
        }
        
        $VM = Get-AzVM -ResourceGroupName $resourceGroupName -Name $VMName
        
        Update-AzVM -ResourceGroupName $resourceGroupName -VM $VM
        if ( !$? )
        {
            LogMessage -Message ("Failed to Update Azure RM VM for vm {0}" -f $VMName) -LogType ([LogType]::Info1)
            throw "SetCustomScriptExtension failed."
        }
        
        Start-Sleep -Seconds 1
        
        $result = $status = "fail"
        $vmex = Get-AzVMExtension -ResourceGroupName $resourceGroupName -VMName $VMName -Name $global:CustomExtensionName -Status
        
        if ( $global:OSType -eq "windows")
        {
            $extStatus = $vmex.SubStatuses
            
            if ($extStatus)
            {
                $result = $extStatus[0].Message	
                $status = $extStatus[0].DisplayStatus	
            }			
        }
        else
        {
            $extStatus = $vmex.Statuses
            if ($extStatus)
            {
                $result = $extStatus.Message
                $status = $extStatus.DisplayStatus
            }
        }
        
        LogMessage -Message ("Result : {0}, Status : {1}" -f $result, $status) -LogType ([LogType]::Info1)
        
        if ( $result -match "Success")
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
        if ($_ -match 'ResourceGroup')
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
    LogMessage -Message ("*******Removing Azure Artifacts**********" ) -LogType ([LogType]::Info1)

    DeleteResourceGroup -Name $global:RecResourceGroup -Location $global:RecoveryLocation
    DeleteResourceGroup -Name $global:VaultRGName -Location $global:VaultRGLocation
    DeleteResourceGroup -Name $global:PriResourceGroup -Location $global:PrimaryLocation

    $diagRgName = $global:VMName + "-DIAG"
    $diagRgName = $diagRgName -replace "-"
    $diagRgName = $diagRgName.ToLower()
    DeleteResourceGroup -Name $diagRgName -Location $global:PrimaryLocation
	LogMessage -Message ("*******Removed Azure Artifacts found(if any) as part of previous run**********" ) -LogType ([LogType]::Info1)
}

#
# Get Fabric objects
#
Function GetFabricObject
{
    param(
        # Fabric Name
        [Parameter(Mandatory=$True)][string]$fabricName,
        [parameter(Mandatory=$False)] [bool] $throwIfNotFound = $true
    )
    
    if ([string]::IsNullOrEmpty($fabricName))
    {
        LogMessage -Message ("--Fabric Name can not be null...") -LogType ([LogType]::Error)
        throw "Fabric Name can not be null or empty."
    }

    . {
        LogMessage -Message ("Getting Fabric: {0}" -f $fabricName) -LogType ([LogType]::Info1)
        $fabricObject = Get-AzRecoveryServicesAsrFabric -Name $fabricName -ErrorAction SilentlyContinue
        
        if ($null -eq $fabricObject)
        {
            LogMessage -Message ("Fabric: {0} does not exist..." -f $fabricName) -LogType ([LogType]::Info1)
            if ($throwIfNotFound -eq $true)
            {
                Throw
            }
        }
    } | Out-Null

    LogObject -Object $fabricObject
    return $fabricObject
}

#
# Get protected object
#
Function GetProtectedItemObject
{
    param
    (
        [parameter(Mandatory=$True)][string] $fabricName,
        [parameter(Mandatory=$True)][string] $containerName,
        [parameter(Mandatory=$False)] [bool] $throwIfNotFound = $true
    )
    
    if ([string]::IsNullOrEmpty($fabricName))
    {
        LogMessage -Message ("--Fabric Name can not be null...") -LogType ([LogType]::Error)
        throw "Fabric Name can not be null or empty."
    }

    . {
        $fabricObject = Get-AzRecoveryServicesAsrFabric -Name $fabricName
        if ($null -ne $fabricObject) {
            $containerObject = Get-AzRecoveryServicesAsrProtectionContainer -Fabric $fabricObject -FriendlyName $containerName
            LogMessage -Message ("Container: {0}, Role: {1}, AvailablePolicies: {2}" -f $containerObject.FriendlyName, `
                                    $containerObject.Role, $containerObject.AvailablePolicies.FriendlyName ) -LogType ([LogType]::Info1);
            
            if ($null -ne $containerObject) {
                $protectedItemObject = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $containerObject | where { $_.FriendlyName -eq $global:VMName }
                if ($null -eq $protectedItemObject)
                {
                    LogMessage -Message ("ProtectedItemObject: {0} does not exist..." -f $fabricName) -LogType ([LogType]::Info1)
                }
                else {
                    if ($null -ne $protectedItemObject.ProviderSpecificDetails.AgentVersion)
                    {
                        LogMessage -Message ("AgentVersion: {0}" -f $protectedItemObject.ProviderSpecificDetails.AgentVersion) -LogType ([LogType]::Info1)
                    }
                }
            }
        }
    } | Out-Null

    if ($throwIfNotFound -eq $true -and $null -eq $protectedItemObject)
    {
        Throw
    }

    LogObject -Object $protectedItemObject
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
    
    if ($recoveryPoints.Count -eq 1)
    {
        $recoveryPoint = $recoveryPoints[0]
    }
    else
    {
        $recoveryPoint = $recoveryPoints[$recoveryPoints.Count-1]
    }
    
    LogObject -Object $recoveryPoint

    return [PSObject]$recoveryPoint
}

#
# Fetch Recovery points
#
Function GetRecoveryPoint
{
    param(
        [parameter(Mandatory=$True)][PSObject] $protectedItemObject,
        [ValidateSet('Latest', 'LatestAvailable', 'LatestAvailableApplicationConsistent', 'LatestAvailableCrashConsistent')]
        [parameter(Mandatory=$True)][string] $tagType = 'Latest'
    )
    
    Start-Sleep -Seconds 600
    # Wait for recovery point to generate(10mins, at least one CC)
    do
    {
        $flag = $false;
        LogMessage -Message ("Triggering Get RecoveryPoint.") -LogType ([LogType]::Info1)
        $recoveryPoints = Get-AzRecoveryServicesAsrRecoveryPoint -ReplicationProtectedItem $protectedItemObject | Sort-Object RecoveryPointTime -Descending
        
        if ($recoveryPoints.Count -eq 0)
        {
            $flag = $true;
            LogMessage -Message ("No Recovery Points Available. Waiting for: {0} sec before querying again." -f ($JobQueryWaitTime60Seconds * 2)) -LogType ([LogType]::Info1);
            Start-Sleep -Seconds ($JobQueryWaitTime60Seconds * 2);
        }
    }while($flag)
    LogMessage -Message ("Number of RecoveryPoints found: {0}" -f $recoveryPoints.Count) -LogType ([LogType]::Info1);
    
    if ($tagType -eq "Latest")
    {
        $recoveryPoint = GetLatestTag -recoveryPoints $recoveryPoints
        LogMessage -Message ("Latest recovery point : type - {0}, time - {1}" -f $recoveryPoint.RecoveryPointType, $recoveryPoint.RecoveryPointTime) -LogType ([LogType]::Info1);
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
            if ($tsepoch -eq $timestamp) { $recoveryPoint = $ts }
        }
        #$recoveryPoint = $recoveryPoints | where { (Get-Date -Date $_.RecoveryPointTime.ToString("MM/dd/yyyy hh:mm:ss tt") -UFormat %s) -eq $timestamp}
        $recoveryPointName = $recoveryPoint.Name
        
        if (!$recoveryPointName)
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
    if (($global:Operation -eq 'SETVAULTCONTEXT') -or ($global:Operation -eq 'ER') -or ($global:Operation -eq 'RUN540'))
    {
        # Create Vault RG if not exists
        $rg = Get-AzResourceGroup -Name $global:VaultRGName -Location $global:VaultRGLocation -ErrorAction SilentlyContinue
        if ($rg -eq $null)
        {
            LogMessage -Message ('Creating Vault Resource Group: {0}' -f $global:VaultRGName) -LogType ([LogType]::Info1)
            $vaultRGObject = New-AzResourceGroup -Name $global:VaultRGName -Location $global:VaultRGLocation 
            LogMessage -Message ('Sleeping for {0} sec so that vault RG creation message can reach to ASR.' -f (60 * 2)) -LogType ([LogType]::Info1)
            Start-Sleep -Seconds (60 * 2)
        }
        else
        {
            LogMessage -Message ('Vault Resource Group: {0} already exist. Skipping this.' -f $global:VaultRGName) -LogType ([LogType]::Info1)
        }

        # Create Vault if not exists
        $Vault = Get-AzRecoveryServicesVault -ResourceGroupName $global:VaultRGName -Name $global:VaultName -ErrorAction SilentlyContinue
        if ($Vault -eq $null)
        {
            LogMessage -Message ('Creating Vault: {0}' -f $global:VaultName) -LogType ([LogType]::Info1)
            $Vault = New-AzRecoveryServicesVault -Name $global:VaultName -ResourceGroupName $global:VaultRGName -Location $global:VaultRGLocation
            LogMessage -Message ('Sleeping for {0} sec so that vault creation message can reach to ASR.' -f (60 * 3)) -LogType ([LogType]::Info1)
            Start-Sleep -Seconds (60 * 3)
        }
        else
        {
            LogMessage -Message ('Vault: {0} already exist. Skipping this.' -f $global:VaultName) -LogType ([LogType]::Info1)
        }
    } 
    else 
    {
        $rg = Get-AzResourceGroup -Name $global:VaultRGName -Location $global:VaultRGLocation -ErrorAction SilentlyContinue
        Assert-NotNull($rg)
        $Vault = Get-AzRecoveryServicesVault -ResourceGroupName $global:VaultRGLocation -Name $global:VaultName -ErrorAction SilentlyContinue
    }

    # Set Vault Context
    if ($Vault -ne $null)
    {
        LogObject -Object $Vault
        LogMessage -Message ('Set Vault Context : {0}' -f $global:VaultName) -LogType ([LogType]::Info1)
        Set-AzRecoveryServicesAsrVaultContext -Vault $Vault
        if (!$?) {
            $ErrorMessage = ('Failed to import vault : {0}' -f $global:VaultName)
            LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
            throw $ErrorMessage
        }
    }
}

Function AssignTagToRG
{
    param(
        [Parameter(Mandatory=$true)][string] $RGName
    )
    #Tagging Resource Groups
    $tag = @{"a2agql" = "true"}

    $rg = Get-AzResource -ResourceGroup $RGName -ErrorAction SilentlyContinue -Verbose
    $rg = Get-AzResourceGroup -ResourceGroupName $RGName -Verbose
    if ($rg)
    {
        New-AZTag -ResourceId $rg.ResourceId -Tag $tag -ErrorAction SilentlyContinue -Verbose

        if ($?)
        {
            LogMessage -Message ("Successfully applied tag {0}" -f $tag) -LogType ([LogType]::Info1)
        }
        else {
            LogMessage -Message ("Failed to apply tag {0}" -f $tag) -LogType ([LogType]::Info1)
        }
    }
}

Function CreateFabric
{
    param(
        [Parameter(Mandatory=$true)][string] $Name,
        [Parameter(Mandatory=$true)][string] $Location,
                                    [int] $JobQueryWaitTimeInSeconds = 5
    )

    if ([string]::IsNullOrEmpty($Name) -or [string]::IsNullOrEmpty($Location))
    {
        LogMessage -Message ("--Fabric Name and Location can not be null...") -LogType ([LogType]::Error)
        throw "Fabric Name and Location can not be null or empty."
    }

    . {
        LogMessage -Message ("Creating Fabric: {0}" -f $Name) -LogType ([LogType]::Info1)

        $fabric = Get-AzRecoveryServicesAsrFabric -Name $Name -ErrorAction SilentlyContinue
        if ($null -eq $fabric)
        {
            $currentJob = New-AzRecoveryServicesAsrFabric -Azure -Name $Name -Location $Location
            
            LogObject -Object $currentJob
            $currentJob = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds)
            $fabric = Get-AzRecoveryServicesAsrFabric -Name $Name -ErrorAction SilentlyContinue
        }
        else
        {
            LogMessage -Message ("--Fabric: {0} already exist. Skipping ..." -f $Name) -LogType ([LogType]::Info1)
        }

    } | Out-Null

    LogObject -Object $fabric
    return $fabric
}

Function CreateProtectionContainer
{
    param(
        # Container name
        [Parameter(Mandatory=$true)][string]$Name,
        # Fabric Object
        [Parameter(Mandatory=$true)][PSObject]$Fabric,
        [int] $JobQueryWaitTimeInSeconds = 5
    )
    
    if ([string]::IsNullOrEmpty($Name) -or $null -eq $Fabric)
    {
        LogMessage -Message ("--Container Name and Fabric can not be null...") -LogType ([LogType]::Error)
        return
    }

    . {
        LogMessage -Message ("--Container Name {0} and Fabric {1} to create container..." -f $Name, $Fabric.FriendlyName) -LogType ([LogType]::Info1)

        $containerObject = Get-AzRecoveryServicesAsrProtectionContainer -FriendlyName $Name -Fabric $Fabric -ErrorAction SilentlyContinue
        
        if ($null -eq $containerObject)
        {
            $currentJob = New-AzRecoveryServicesAsrProtectionContainer -Name $Name -Fabric $Fabric

            LogObject -Object $currentJob
            $currentJob = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds)

            $containerObject = Get-AzRecoveryServicesAsrProtectionContainer -FriendlyName $Name -Fabric $Fabric -ErrorAction SilentlyContinue
        }
        else
        {
            LogMessage -Message ("Protection Container: {0} already exist. Skipping creation..." -f $Name) -LogType ([LogType]::Info1)
        }

    } | Out-Null

    LogObject -Object $containerObject
    return $containerObject
}

Function CreateReplicationPolicy
{
    param(
        # Policy name
        [Parameter(Mandatory=$true)][string] $Name,
        
        # Recovery point retention in hours
                                    [int] $RecoveryPointRetentionInHours = 12,
                                    [int] $ApplicationConsistentSnapshotFrequencyInHours = 1,
                                    [ValidateSet('Enable','Disable')]
                                    [string] $MultiVmSyncStatus,
                                    [int] $JobQueryWaitTimeInSeconds = 5
    )
    
    if ([string]::IsNullOrEmpty($Name))
    {
        LogMessage -Message ("--Policy Name can not be null...") -LogType ([LogType]::Error)
        throw
    }

    . {
        LogMessage -Message ("Triggering Replication Protection Policy creation: {0}" -f $Name) -LogType ([LogType]::Info1)
        $policyObject = Get-AzRecoveryServicesAsrPolicy -Name $Name -ErrorAction SilentlyContinue

        if ($null -eq $policyObject)
        {        
            $currentJob = New-AzRecoveryServicesAsrPolicy -AzureToAzure -Name $Name -RecoveryPointRetentionInHours $recoveryPointRetentionInHours `
            -ApplicationConsistentSnapshotFrequencyInHours $applicationConsistentSnapshotFrequencyInHours -MultiVmSyncStatus $MultiVmSyncStatus

            LogObject -Object $currentJob
            $currentJob = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds)
            
            $policyObject = Get-AzRecoveryServicesAsrPolicy -Name $Name -ErrorAction SilentlyContinue
        }
        else
        {
            LogMessage -Message ("Replication Protection policy: {0} already exist. Skipping creation..." -f $Name) -LogType ([LogType]::Info1)
        }
    } | Out-Null

    LogObject -Object $policyObject
    return $policyObject
}

Function CreateProtectionContainerMapping
{
    param(
        # Container Mapping name
        [Parameter(Mandatory=$true)][string]$Name,

        # Primary Container Object
        [Parameter(Mandatory=$true)][PSObject]$PrimaryProtectionContainer,

        # Recovery Container Object
        [Parameter(Mandatory=$true)][PSObject]$RecoveryProtectionContainer,

        # Replication Policy Object
        [Parameter(Mandatory=$true)][PSObject]$Policy,

                                    [int] $JobQueryWaitTimeInSeconds = 5
    )
    
    if ([string]::IsNullOrEmpty($Name) -or $null -eq $PrimaryProtectionContainer -or $null -eq $RecoveryProtectionContainer -or $null -eq $Policy)
    {
        LogMessage -Message ("--Container Mapping Name, Primary Container, Recovery Container and Repication Policy can not be null...") -LogType ([LogType]::Error)
        throw
    }

    . {
        LogMessage -Message ("Triggering Protection Container Mapping") -LogType ([LogType]::Info1)

        $containerMappingObject = Get-AzRecoveryServicesAsrProtectionContainerMapping -Name $Name -ProtectionContainer $PrimaryProtectionContainer -ErrorAction SilentlyContinue 

        if ($null -eq $containerMappingObject)
        {
            $currentJob = New-AzRecoveryServicesAsrProtectionContainerMapping `
                            -Name $Name `
                            -Policy $Policy `
                            -PrimaryProtectionContainer $PrimaryProtectionContainer `
                            -RecoveryProtectionContainer $RecoveryProtectionContainer

            LogObject -Object $currentJob
            $currentJob = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds)
            
            $containerMappingObject = Get-AzRecoveryServicesAsrProtectionContainerMapping -Name $Name -ProtectionContainer $PrimaryProtectionContainer                                                                                     
        }
        else
        {
            LogMessage -Message ("Protection Container Mapping: {0} already exist. Skipping creation..." -f $Name) -LogType ([LogType]::Info1)
        }
    } | Out-Null

    LogObject -Object $containerMappingObject
    return $containerMappingObject
}

Function GetProtectionContainer
{
    param(
        # Container name
        [Parameter(Mandatory=$true)][string]$Name,
        
        # Fabric Object
        [Parameter(Mandatory=$true)][PSObject]$Fabric,
        [int] $JobQueryWaitTimeInSeconds = 5
    )
    
    if ([string]::IsNullOrEmpty($Name) -or $null -eq $Fabric)
    {
        LogMessage -Message ("--Container Name and Fabric can not be null...") -LogType ([LogType]::Error)
        return
    }

    . {
        LogMessage -Message ("Getting Protection Container: {0}" -f $Name) -LogType ([LogType]::Info1)
        $containerObject = Get-AzRecoveryServicesAsrProtectionContainer -Name $Name -Fabric $Fabric -ErrorAction SilentlyContinue
        
        if ($null -eq $ContainerObject)
        {
            LogMessage -Message ("Protection Container: {0} does not exist..." -f $Name) -LogType ([LogType]::Error)
            throw
        }

    } | Out-Null
    
    LogObject -Object $containerObject
    return $containerObject
}

# Create Network Mapping
Function CreateNetworkMapping
{
    param(
        # Network Mapping Name
        [Parameter(Mandatory=$true)][string]$Name,
        
        # Primary Network Object
        [Parameter(Mandatory=$true)][PSObject]$PrimaryAzureNetwork,

        # Primary Fabric Object
        [Parameter(Mandatory=$true)][PSObject]$PrimaryFabric,

        # Recovery Network Object
        [Parameter(Mandatory=$true)][PSObject]$RecoveryAzureNetwork,
        
        # Recovery Fabric Object
        [Parameter(Mandatory=$true)][PSObject]$RecoveryFabric,
                                    [int] $JobQueryWaitTimeInSeconds = 5
    )
    
    if ([string]::IsNullOrEmpty($Name) -or $null -eq $PrimaryFabric -or $null -eq $PrimaryAzureNetwork -or $null -eq $RecoveryAzureNetwork -or $null -eq $RecoveryFabric)
    {
        LogMessage -Message ("--Network Mapping Name, Primary Fabric, Primary Azure Network, Recovery Azure Network and Recovery Fabric can not be null...") -LogType ([LogType]::Error)
        throw
    }
    
    . {
        LogMessage -Message ("Triggering Network Pairing: {0}" -f $Name) -LogType ([LogType]::Info1)
        $networkMappingObject = Get-AzRecoveryServicesAsrNetworkMapping -Name $Name -PrimaryFabric $PrimaryFabric -ErrorAction SilentlyContinue 

        if ($null -eq $networkMappingObject)
        {
            $currentJob = New-AzRecoveryServicesAsrNetworkMapping   -AzureToAzure `
                                                                    -Name $Name `
                                                                    -PrimaryFabric $PrimaryFabric `
                                                                    -PrimaryAzureNetworkId $PrimaryAzureNetwork.Id `
                                                                    -RecoveryFabric $RecoveryFabric `
                                                                    -RecoveryAzureNetworkId $RecoveryAzureNetwork.Id `

            $currentJob = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds)
            
            $networkMappingObject = Get-AzRecoveryServicesAsrNetworkMapping -Name $Name -PrimaryFabric $PrimaryFabric -ErrorAction SilentlyContinue                                                                              
        }
    } | Out-Null

    LogObject -Object $networkMappingObject
    return $networkMappingObject
}

Function GetExtensionVersion
{
    LogMessage -Message ("Trying to fetch extension version") -LogType ([LogType]::Info1)

    if ($null -eq $DeploymentInfo.ExtensionType)
    {
        LogMessage -Message ("DeploymentInfo.ExtensionType is null") -LogType ([LogType]::Warning)
        return $null
    }

    $vmStatus = Get-AzVM -ResourceGroupName $global:PriResourceGroup -Name $global:VMName -Status
    if ($null -ne $vmStatus)
    {
        $extensions = $vmStatus | Select-Object -ExpandProperty Extensions

        if ($null -ne $extensions)
        {
            $extType = "SiteRecovery-" + $DeploymentInfo.ExtensionType
            LogMessage -Message ("extType : $extType") -LogType ([LogType]::Info1)
            $agentExt = $extensions | Where-Object { $_.Name -eq  $extType}

            if ($null -ne $agentExt)
            {
                LogObject -Object $agentExt

                $extVersion = $agentExt.TypeHandlerVersion

                if ($null -ne $extVersion)
                {
                    LogMessage -Message ("Installed Extension Version : $extVersion") -LogType ([LogType]::Info1)
                    return $extVersion
                }
            }
        }
    }
    LogMessage -Message ("Couldn't fetch ExtensionVersion") -LogType ([LogType]::Warning)
    return $null
}

Function ValidateAgentVersion
{
    LogMessage -Message ("Validating AgentVersion on VM : $global:VMName") -LogType ([LogType]::Info1)

    # Get protected VM Item
    $protectedItemObject = GetProtectedItemObject -fabricName $global:PrimaryFabricName -containerName $global:PrimaryContainerName

    if ($null -ne $protectedItemObject)
    {
        $currAgentVersion = $protectedItemObject.ProviderSpecificDetails.AgentVersion

        LogMessage -Message ("Current Agent Version : $currAgentVersion") -LogType ([LogType]::Info1)
        if ($null -eq $currAgentVersion -or $currAgentVersion -ne $global:AgentVersion)
        {
            $errMsg = "Found invalid agent version. Installed Agent Version : $currAgentVersion, Expected Agent Version : $global:AgentVersion"

            Write-Error "error : $errMsg"
            Throw $errMsg
        }

        LogMessage -Message ("Agent Bits $global:AgentVersion are successfully validated") -LogType ([LogType]::Info1)
    }
}

Function ValidateExtensionVersion
{
    LogMessage -Message ("Validating ExtensionVersion on VM : $global:VMName") -LogType ([LogType]::Info1)

    $extVersion = GetExtensionVersion

    if ($null -eq $extVersion -or $extVersion -ne $ExtensionVersion)
    {
        $errMsg = "Found invalid agent version. Installed Extension Version : $extVersion, Expected Extension Version : $ExtensionVersion"

        Write-Error "error : $errMsg"
        Throw $errMsg
    }

    LogMessage -Message ("Extension Version $ExtensionVersion is successfully validated") -LogType ([LogType]::Info1)
}

Function Invoke-RetryWithOutput
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
			$cmd | Invoke-Retry -Retry 61
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
		[UInt32]$Retry = 10,
	
		[Parameter(Mandatory = $false, Position = 3)]
		[ValidateRange(0, [UInt32]::MaxValue)]
		[UInt32]$Delay = 5,
	
		[Parameter(Mandatory = $false, Position = 4)]
		[Switch]$ShowError = $false
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
            $exceptionMessage = ""
			Try
			{
				Write-Verbose -Message ("Command [{0}] started. Retry: {1}" -f $command,($i+1)+'/'+$Retry) -Verbose
				$output = & $command
				$succeeded = $true
				Write-Verbose -Message ("Command [{0}] succeeded." -f $command) -Verbose
				return $output
			}
			Catch
			{
                Write-Verbose -Message ("Command [{0}] failed inside catch. Retrying in {1} seconds." -f $command, $Delay) -Verbose
				If ($ShowError)
				{
					Write-Verbose $_.Exception.Message
				}
                
                $exceptionMessage = $_.Exception.Message
				
				If ($Global:Error.Count -gt 0)
				{
					$Global:Error.RemoveAt(0)
				}
				$exception = $true
			}
			
			If ($exception)
			{
				Write-Verbose -Message ("Command [{0}] failed. Retrying in {1} seconds, exception message:{2}." -f $command, $Delay, $exceptionMessage) -Verbose
				Start-Sleep -Seconds $Delay
			}
		}
     
        if ($succeeded -eq $false)
        {
          throw "Command failed even after retries"
        }
	}
}

# =====================================
# Delete Operations
# =====================================


# Deletes a Resource Group
Function DeleteResourceGroup
{
    param(
        # Name of the Resource Group to be deleted
        [Parameter(Mandatory=$true)][string] $Name,

        # Location Name where Resource Group is present
        [Parameter(Mandatory=$true)][string] $Location
    )

    if ($null -eq $Name -or $null -eq $Location)
    {
        LogMessage -Message ("--Resource group name and location can not be null...") -LogType ([LogType]::Info1)
        return
    }

    LogMessage -Message ("Deleting Resource Group: {0}" -f $Name) -LogType ([LogType]::Info1)
    $rgObject = Get-AzResourceGroup -Name $Name -Location $Location -ErrorAction SilentlyContinue
    if ($null -eq $rgObject)
    {
        LogMessage -Message ("--Resource Group: {0} does not exist. Skipping ..." -f $Name) -LogType ([LogType]::Info1)
    }
    else
    {
        Remove-AzResourceGroup -Name $Name -Force
    }

    $rgObject = Get-AzResourceGroup -Name $Name -Location $Location -ErrorAction SilentlyContinue
    if ($null -eq $rgObject)
    {
        LogMessage -Message ("--Resource Group: {0} Deleted." -f $Name) -LogType ([LogType]::Info1)
    }
    else
    {
        LogMessage -Message ("--Resource Group: {0} Deletion failed." -f $Name) -LogType ([LogType]::Error)
    }
}

Function DeleteFabric
{
    param(
        # Fabric Object to be deleted
        [Parameter(Mandatory=$true)][PSObject] $Fabric
    )

    if ($null -eq $Fabric)
    {
        LogMessage -Message ("--Fabric can not be null...") -LogType ([LogType]::Info1)
        return
    }

    LogMessage -Message ("Deleting Fabric: {0}" -f $Fabric.Name) -LogType ([LogType]::Info1)

    $currentJob = Remove-AzRecoveryServicesAsrFabric -Fabric $Fabric -Force
    LogObject -Object $currentJob

    $currentJob = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds)

    $fabric = Get-AzRecoveryServicesAsrFabric -Name $Fabric.Name -ErrorAction SilentlyContinue
    if ($null -eq $fabric)
    {
        LogMessage -Message ("Fabric: {0} Deleted." -f $Name) -LogType ([LogType]::Info1)
    }
    else
    {
        LogMessage -Message ("--Fabric: {0} Deletion failed." -f $Name) -LogType ([LogType]::Error)
        Throw
    }

}


Function DeleteProtectionContainer
{
    param(
        [Parameter(Mandatory=$true)][string] $ContainerName,
        [Parameter(Mandatory=$true)][PSObject] $FabricObject
    )

    if ($null -eq $ContainerName -or $null -eq $FabricObject)
    {
        LogMessage -Message ("--Fabric Object or containerName can not be null...") -LogType ([LogType]::Info1)
        return
    }

    $protectionContainer = Get-AzRecoveryServicesAsrProtectionContainer -FriendlyName $ContainerName -Fabric $FabricObject -ErrorAction SilentlyContinue
        
    if ($null -ne $protectionContainer)
    {
        LogObject $protectionContainer

        LogMessage -Message ("Deleting Protection Container: {0}" -f $protectionContainer.Name) -LogType ([LogType]::Info1)
        LogObject -Object $protectionContainer

        $currentJob = Remove-AzRecoveryServicesAsrProtectionContainer -InputObject $protectionContainer

        LogObject -Object $currentJob
        $currentJob = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name
        WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds)
    
        LogMessage -Message ("Deleted Protection Container: {0}" -f $protectionContainer.Name) -LogType ([LogType]::Info1)
    }
}

Function DeleteProtectionContainerMapping
{
    param(
        [Parameter(Mandatory=$true)][string] $ContainerName,
        [Parameter(Mandatory=$true)][string] $ContainerMappingName,
        [Parameter(Mandatory=$true)][PSObject] $FabricObject
    )

    if ($null -eq $ContainerName -or $null -eq $FabricObject)
    {
        LogMessage -Message ("--Fabric Object or containerName can not be null...") -LogType ([LogType]::Info1)
        return
    }

    $containerObject = Get-AzRecoveryServicesAsrProtectionContainer -Fabric $FabricObject `
                                    -FriendlyName $ContainerName -ErrorAction SilentlyContinue

    if ($null -ne $containerObject) {
        LogObject $containerObject
        
        $containerMappingObject = Get-AzRecoveryServicesAsrProtectionContainerMapping -Name $ContainerMappingName -ProtectionContainer $containerObject -ErrorAction SilentlyContinue                           
        if ($null -ne $containerMappingObject) {
            LogObject $containerMappingObject

            LogMessage -Message ("Deleting Protection Container Mapping : {0}" -f $containerMappingObject.Name) -LogType ([LogType]::Info1)
            $currentJob = Remove-AzRecoveryServicesAsrProtectionContainerMapping -InputObject $containerMappingObject
            $currentJob = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name

            LogObject -Object $currentJob
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds)
            LogMessage -Message ("*****Containers Unpaired*****") -LogType ([LogType]::Info2)
        }else {
            LogMessage -Message ("***** Protection Container Mapping for {0} doesn't exists*****" -f $ContainerMappingName) -LogType ([LogType]::Info2)
        }
    }
}

Function DeleteReplicationPolicy
{
    param(
        [Parameter(Mandatory=$true)][string] $PolicyName
    )

    if ($null -eq $PolicyName)
    {
        LogMessage -Message ("--Replication Policy name can not be null...") -LogType ([LogType]::Info1)
        return
    }

    $policyObject = Get-AzRecoveryServicesAsrPolicy -Name $PolicyName -ErrorAction SilentlyContinue
    if ($null -ne $policyObject) {
        LogObject -Object $policyObject

        LogMessage -Message ("Deleting Replication Policy: {0}" -f $ReplicationPolicy.Name) -LogType ([LogType]::Info1)
        $currentJob = Remove-AzRecoveryServicesAsrPolicy -Policy $policyObject
        LogObject -Object $currentJob

        $currentJob = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name
        WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds)
        LogMessage -Message ("*****Policy Removed*****") -LogType ([LogType]::Info2)
    }
    else {
        LogMessage -Message ("***** Replication Policy doesn't exists*****") -LogType ([LogType]::Info2)
    }
}

Function DeleteNetworkMapping
{
    param(
        # Protection Container Mapping Object to be deleted
        [Parameter(Mandatory=$true)][PSObject] $FabricObject
    )
    if ($null -ne $FabricObject) {
        LogObject -Object $FabricObject
        LogMessage -Message ("Triggering Network Un-Mapping") -LogType ([LogType]::Info1)

        $networkMappingObject = Get-AzRecoveryServicesAsrNetworkMapping -PrimaryFabric $FabricObject
        if ($null -ne $networkMappingObject) {
            LogObject -Object $networkMappingObject
            
            LogMessage -Message ("Deleting Network Mapping: {0}" -f $networkMappingObject.Name) -LogType ([LogType]::Info1)
    
            $currentJob = Remove-AzRecoveryServicesAsrNetworkMapping -InputObject $networkMappingObject

            LogObject -Object $currentJob
            $currentJob = Get-AzRecoveryServicesAsrJob -Name $currentJob.Name
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTimeInSeconds)
            LogMessage -Message ("*****Network Un-Mapped*****") -LogType ([LogType]::Info2)
        } else {
            LogMessage -Message ("*****No Network Mapping*****") -LogType ([LogType]::Info1)
        }
    } else {
        LogMessage -Message ("*****No Fabric Object exists*****") -LogType ([LogType]::Info2)
    }
}

#####
Write-Host "CommonFunctions.ps1 executed"
