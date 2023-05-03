###
# Common functions
##

# ########### Header ###########
# Refer common library file
. $PSScriptRoot\CommonParams.ps1
# #############################

Function LogMessage
{
	param(
		[byte] $Type = 1,
		[string] $Message,
		[bool] $WriteToLogFile = $true
	)

	if($Global:Logger -eq $null)
	{
		$Global:Logger = InitializeLogger;
	}

	$TimeStamp = Get-Date -Format "dd.MMM.yyyy HH:mm:ss"
	$Time = Get-Date -Format "HH:mm:ss"

	if ($Type -eq 0)
	{
		#$msg = "[ERROR]`t{0}`t{1}" -f $Time, $Message
        $msg = "{0,-7}: {1,-8}: {2}" -f "ERROR",$Time, $Message
		Write-Host $msg -ForegroundColor Red

		if ($WriteToLogFile)
		{
			$Global:Logger.LogInfo($Message);
		}
	}
	elseif ($Type -eq 1)
	{
        $msg = "{0,-7}: {1,-8}: {2}" -f "INFO",$Time, $Message
		Write-Host $msg -ForegroundColor Green

		if (($WriteToLogFile))
		{
			$Global:Logger.LogInfo($Message);
		}
	}
	elseif ($Type -eq 2)
	{
        $msg = "{0,-7}: {1,-8}: {2}" -f "INFO",$Time, $Message
		Write-Host $msg -ForegroundColor DarkYellow -BackgroundColor Black

		if (($WriteToLogFile))
		{
			$Global:Logger.LogInfo($Message);
		}
	}
	elseif ($Type -eq 3)
	{
        $msg = "{0,-7}: {1,-8}: {2}" -f "INFO",$Time, $Message
		Write-Host $msg -ForegroundColor Cyan

		if (($WriteToLogFile))
		{
			$Global:Logger.LogInfo($Message);
		}
	}
	elseif ($Type -eq 4)
	{
        $msg = "{0,-7}: {1,-8}: {2}" -f "WARNING",$Time, $Message
		Write-Host $msg -ForegroundColor Yellow

		if (($WriteToLogFile))
		{
			$Global:Logger.LogInfo($Message);
		}
	}
	else
	{
		$msg = "{0,-7}: {1,-8}: {2}" -f "UNKNOWN",$Time, $Message
		Write-Host $msg -ForegroundColor Magenta

		if ($WriteToLogFile)
		{
			$Global:Logger.LogInfo($Message);
		}
	}
}

Function InitializeLogger
{
	[void][Reflection.Assembly]::LoadFile($Global:LogDLLPath)
	$Global:Logger = New-Object Microsoft.Common.Log4TestNonStatic
	$dateTime = "MM-dd-yyyy HH.mm.ss"
	$logFileName = "$Global:LogDir\A2AGQLLog`_{0}.log" -f (Get-Date -Format $dateTime)
	if(!(Test-Path $Global:LogDir))
	{
		New-Item -Path $Global:LogDir -ItemType directory | Out-Null
	}
    $Global:Logger.LoggerInitialize($Global:LogDLLXmlFile, $null, $logFileName) | Out-Null
	$Global:Logger
}

Function getRandom
{
	$rand = Get-Random -minimum 1 -maximum 10000 
	return $rand
}

Function WaitForJobCompletion
{ 
	param(
		[parameter(Mandatory=$True)] [string] $JobId,
        [parameter(Mandatory=$True)] [ASROperation] $OperationName,
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
                LogMessage -Message ("{0} - {1} in-Progress...Wait: {1} Sec, ClientRequestId: {2}, JobID: {3}" -f $Job.JobType, $Job.DisplayName, $JobQueryWaitTimeInSeconds.ToString(),$Job.ClientRequestId,$Job.Name) -Type 3;
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
                    LogMessage -Message ("{0}, Status: {1}" -f $messageToDisplay,$Job.State) -Type 1;
                    LogMessage -Message ("Task Details: {0}" -f ($Job.Tasks | ConvertTo-json -Depth 1)) -Type 3;
                    LogMessage -Message ("Error Details: {0}" -f ($Job.Errors.ServiceErrorDetails | ConvertTo-json -Depth 1)) -Type 0;
                    $errorMessage = ($Job.Errors.ServiceErrorDetails | ConvertTo-json -Depth 1)
					throw $errorMessage;
				}
                else 
                {
                    LogMessage -Message ("{0}, Status: {1}" -f $messageToDisplay,$Job.State) -Type 3;
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
    LogMessage -Message ("IR in Progress..." -f $messageToDisplay,$Job.State) -Type 1;
    $OperationName = "IRCompletion"
	
	$waitTimeCounter = 0
	do
	{
        if($provider -eq "E2E")
        {
		    $IRjobs = Get-AzureRMSiteRecoveryJob -TargetObjectId $VM.Name | Sort-Object StartTime -Descending | select -First 4 | Where-Object{$_.JobType -like "*IrCompletion"}
            LogMessage -Message ("Jobs Found:{0}" -f $IRjobs.Count) -Type 1;
        }
        elseif($provider -eq "A2A")
        {
            $IRjobs = Get-AzureRMSiteRecoveryJob | Sort-Object StartTime -Descending | select -First 2 `
            | Where-Object{$_.JobType -like "*IrCompletion"} `
            | Where-Object{$_.TargetObjectName -eq $VM.TargetObjectName }
            LogMessage -Message ("Jobs Found:{0}" -f $IRjobs.Count) -Type 1;
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
            LogMessage -Message ("Elapsed Time: {0}" -f $StopWatch.Elapsed.ToString()) -Type 3;
			$waitTimeCounter += $JobQueryWaitTimeInSeconds
			if($waitTimeCounter -gt $MaxIRWaitTimeInSeconds)
			{
				$errorMsg = ("IR not completed after Waiting for '{0}' seconds. Failing test." -f $MaxIRWaitTimeInSeconds)
				LogMessage -Message ($errorMsg) -Type 0;
                Throw $errorMsg
			}
            LogMessage -Message ("IR in Progress.Wait: {0} Sec" -f $JobQueryWaitTimeInSeconds.ToString()) -Type 3;
			Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
		}
	}While($isProcessingLeft)
	
    LogMessage -Message ("Total IR Time: {0}" -f $StopWatch.Elapsed.ToString()) -Type 3;
    LogMessage -Message ("Finalize IR jobs, Job Count: {0}" -f $IRjobs.Count) -Type 3;
	$IRjobs
	WaitForJobCompletion -JobId $IRjobs[0].Name -Message $IRjobs[0].DisplayName -OperationName ([ASROperation]::IRCompletion)
    WaitForJobCompletion -JobId $IRjobs[1].Name -Message $IRjobs[1].DisplayName -OperationName ([ASROperation]::IRCompletion)
	
    LogMessage -Message ("Total IR + Finalize IR Time: {0} Sec" -f $StopWatch.Elapsed.ToString()) -Type 3;
	$StopWatch.Stop()
}

Function WaitForProtectedItemState
{ 
	param(
        [string] $state = "Protected",
        [PSObject] $containerObject,
        [int] $JobQueryWaitTimeInSeconds = 60
        )

    LogMessage -Message ("Waiting for state:{0}" -f $state) -Type 1;
    $stateChanged = $true
	do
	{
		$protectedItemObject = Get-AzureRmSiteRecoveryReplicationProtectedItem -ProtectionContainer $containerObject -FriendlyName $VMName

        LogMessage -Message ("Current state:{0}, Waiting for state:{1}" -f $protectedItemObject.ProtectionState,$state) -Type 1;
		if($protectedItemObject.ProtectionState -eq $state)
		{
			$stateChanged = $false
		}
		else
		{
            LogMessage -Message ("Waiting for: {0} sec before querying again." -f ($JobQueryWaitTime60Seconds * 2)) -Type 1
			Start-Sleep -Seconds ($JobQueryWaitTime60Seconds * 2)
		}
	}While($stateChanged)
}

Function WaitForAzureVMAgentState
{ 
	param(
        [string] $state = "ProvisioningState/succeeded",
        [string] $vmName,
        [string] $resourceGroupName,
        [int] $JobQueryWaitTimeInSeconds = 30
        )

    $MaxWaitTime = 10 * 60
    LogMessage -Message ("Quering VM Agent State") -Type 1;

    $stateChanged = $true
    $waitTimeCounter = 0
	do
	{
        $vm = Get-AzureRmVM -Name $vmName -ResourceGroupName $resourceGroupName -Status
        LogMessage -Message ("Current state:{0}, Waiting for state:{1}" -f $vm.VMAgent.Statuses.Code, $state) -Type 1;
		if($vm.VMAgent.Statuses.Code -eq $state)
		{
			$stateChanged = $false
		}
		else
		{
            $waitTimeCounter += $JobQueryWaitTimeInSeconds
            LogMessage -Message ("Waiting for: {0} sec before querying again." -f $JobQueryWaitTimeInSeconds) -Type 1
			Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
		}
	}While($stateChanged -and ($waitTimeCounter -le $MaxWaitTime))
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
    $MaxWaitTime = 10 * 60
    LogMessage -Message ("Quering VM provisioning and power State") -Type 1;

    $stateChanged = $true
    $waitTimeCounter = 0
	do
	{
        $vm = Get-AzureRmVM -Name $vmName -ResourceGroupName $resourceGroupName -Status
        
        LogMessage -Message ("Current Vm Provisioning state:{0}, Power State:{1} ; Expected Provisioning state:{2}, Power state:{3} ; RG:{4}" -f $vm.Statuses.Code[0],$vm.Statuses.Code[1],$provisioningState,$powerState,$resourceGroupName) -Type 1;
		if($vm.Statuses.Code[0] -eq $provisioningState -and $vm.Statuses.Code[1] -eq $powerState)
		{
			$stateChanged = $false
		}
		else
		{
            $waitTimeCounter += $JobQueryWaitTimeInSeconds
            LogMessage -Message ("Waiting for: {0} sec before querying again." -f $JobQueryWaitTimeInSeconds) -Type 1
			Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
		}
	}While($stateChanged -and ($waitTimeCounter -le $MaxWaitTime))
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

Function SetSubscriptionContext
{ 
	param(
		[parameter(Mandatory=$True)] [string]$UserName,
		[parameter(Mandatory=$True)] [string]$Password,
		[parameter(Mandatory=$True)] [string]$AzureSubscriptionId,
		[ValidateSet("PROD", "BVT", "INT")] [string]$Env = "PROD"
		)

	$SecurePassword = ConvertTo-SecureString -AsPlainText $Password -Force
	$AzureOrgIdCredential = New-Object System.Management.Automation.PSCredential -ArgumentList $UserName, $securePassword
	
	# Check if current context is same then return it.
	$currentSub = $null;
    try
    {
	    $context = Get-AzureRmContext -ErrorAction SilentlyContinue
    }
    catch
    {
        LogMessage -Message ('AzureRmContext not set for user: {0}, Subscription: {1}. Login required...' -f $UserName,$AzureSubscriptionId) -Type 4;
    }

    if($context -ne $null -and ($context.Subscription -eq $null -or $context.Subscription.Id -ne $AzureSubscriptionId))
	{
		Import-AzureRmContext -Path "$PSScriptRoot\a2agqlprofile.json"
		Select-AzureRmSubscription -TenantId $context.Tenant.TenantId -SubscriptionId $AzureSubscriptionId -ErrorAction SilentlyContinue
		$context = Get-AzureRmContext -ErrorAction SilentlyContinue
	}
    else
    {
        return $currentSub;
    }

    if($context -ne $null -and $context.Subscription.Id -eq $AzureSubscriptionId)
	{
		return $currentSub;
	}

	# set subscription context if it is not set.
	if($Env -eq "PROD")
	{
		Login-AzureRmAccount -Credential $AzureOrgIdCredential -SubscriptionId $AzureSubscriptionId
		Select-AzureRmSubscription -TenantId $context.Tenant.TenantId -SubscriptionId $AzureSubscriptionId
	}
	elseif($Env -eq "BVT")
	{
		Add-AzureRmEnvironment -Name BVT `
			-PublishSettingsFileUrl 'https://hrmazureportal.cloudapp.net/publishsettings/index' `
			-ServiceEndpoint 'https://management-preview.core.windows-int.net/' `
			-ManagementPortalUrl 'https://hrmazureportal.cloudapp.net/' `
			-ActiveDirectoryEndpoint 'https://login.windows-ppe.net/' `
			-ActiveDirectoryServiceEndpointResourceId 'https://management.core.windows.net/' `
			-ResourceManagerEndpoint 'https://api-dogfood.resources.windows-int.net/' `
			-GalleryEndpoint 'https://df.gallery.azure-test.net/' `
			-GraphEndpoint 'https://graph.ppe.windows.net/'

		Add-AzureRmAccount -Environment BVT -Credential $AzureOrgIdCredential -SubscriptionId $AzureSubscriptionId | Out-Null
		
		Add-Type -Path "$PSScriptRoot\ASRDelegatingHandler\ASRDelegatingHandler.dll"
		$handler = New-Object -TypeName ASRDelegatingHandler.ASRDelegatingHandler("Microsoft.RecoveryServicesBVTD2");
		[Microsoft.Azure.Commands.Common.Authentication.AzureSession]::Instance.ClientFactory.AddHandler($handler);
	}
	elseif($Env -eq "INT")
	{
		Add-AzureRmEnvironment -Name INT `
			-PublishSettingsFileUrl 'https://srsazureportal.cloudapp.net/publishsettings/index' `
			-ServiceEndpoint 'https://management-preview.core.windows-int.net/' `
			-ManagementPortalUrl 'https://hrmazureportal.cloudapp.net/' `
			-ActiveDirectoryEndpoint 'https://login.windows-ppe.net/' `
			-ActiveDirectoryServiceEndpointResourceId 'https://management.core.windows.net/' `
			-ResourceManagerEndpoint 'https://api-dogfood.resources.windows-int.net/' `
			-GalleryEndpoint 'https://df.gallery.azure-test.net/' `
			-GraphEndpoint 'https://graph.ppe.windows.net/'

		Add-AzureRmAccount -Environment INT -Credential $AzureOrgIdCredential -SubscriptionId $AzureSubscriptionId | Out-Null
	}

	return $AzureSubscription;
}

#
# Set Current Azure Subscription
#
Function SetAzureSubscription
{
		param(
		[parameter(Mandatory=$True)] [string] $SubscriptionName,
		[parameter(Mandatory=$True)] [string] $SubscriptionId,
		[parameter(Mandatory=$True)] [string] $SubUserName,
		[parameter(Mandatory=$True)] [string] $SubPassword
	)
	
	# Add Azure Account
    LogMessage -Message ('Setting current Azure Subscription to {0}' -f $SubscriptionName) -Type 1
	
	$SecurePassword = ConvertTo-SecureString -AsPlainText $SubPassword -Force
	$AzureOrgIdCredential = New-Object System.Management.Automation.PSCredential -ArgumentList $SubUserName, $SecurePassword

	# Set Subscription Info
	SetSubscriptionContext -UserName $SubUserName -Password $SubPassword -AzureSubscriptionId $SubscriptionId -Env PROD

}

#
# SetAzureEnvironment
#
Function SetAzureEnvironment
{
	param(
        [parameter(Mandatory=$True)][string] $environment
    )
	
	if($environment -eq "PROD")
	{
		# SetAzureSubscription -SubscriptionName $global:VMSubName -SubscriptionId $global:VMSubId `
			# -SubUserName $global:VMSubUserName -SubPassword $global:VMSubPassword -Environment $environment
			
		SetAzureSubscription -SubscriptionName $global:VMSubName -SubscriptionId $global:VMSubId `
	-SubUserName $global:VMSubUserName -SubPassword $global:VMSubPassword
	
	# Create ResourceGroup if not exists
	LogMessage -Message ('Creating Vault Resource Group: {0}' -f $global:RecResourceGroup) -Type 1
	$rg = Get-AzureRmResourceGroup -Name $global:RecResourceGroup -Location $global:RecoveryLocation -ErrorAction SilentlyContinue
	if($rg -eq $null)
	{
		$vaultRGObject = New-AzureRmResourceGroup -Name $global:RecResourceGroup -Location $global:RecoveryLocation
	}
	else
	{
		LogMessage -Message ('Vault Resource Group: {0} already exist. Skipping this.' -f $global:RecResourceGroupe) -Type 1
	}

	# Create Vault if not exists
	LogMessage -Message ('Creating Vault: {0}' -f $global:VaultName) -Type 1
	$Vault = Get-AzureRMRecoveryServicesVault -ResourceGroupName $global:RecResourceGroup -Name $global:VaultName -ErrorAction SilentlyContinue
	if($Vault -eq $null)
	{
		$vault = New-AzureRMRecoveryServicesVault -Name $global:VaultName -ResourceGroupName $global:RecResourceGroup -Location $global:RecoveryLocation
		LogMessage -Message ('Sleeping for {0} sec so that vault creation message can reach to ASR.' -f (60 * 2)) -Type 1
		Start-Sleep -Seconds (60 * 2)
	}
	else
	{
		LogMessage -Message ('Vault: {0} already exist. Skipping this.' -f $global:VaultName) -Type 1
	}

	# Set Vault Context
	Set-AzureRmSiteRecoveryVaultSettings -ARSVault $Vault
	}
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
		
		$diskList =  New-Object System.Collections.ArrayList
		foreach ($disk in $diskIdArray)
		{
			$diskInfo=New-Object Microsoft.Azure.Commands.SiteRecovery.ASRAzureToAzureManagedDiskDetails 
			$diskInfo.DiskId = $disk
			$diskInfo.PrimaryStagingAzureStorageAccountId = $v2StagingSA
			$diskInfo.RecoveryAzureResourceGroupId = $recResourceGroupId
			$diskList.Add($diskInfo)
		}
		LogMessage -Message ("Enabling Protection for Managed V2 VM: $enableDRName") -Type 1;
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
		
		$diskList =  New-Object System.Collections.ArrayList
		foreach ($diskuri in $diskURIArray)
		{
			$diskInfo=New-Object  Microsoft.Azure.Commands.SiteRecovery.ASRAzureToAzureDiskDetails 
			$diskInfo.DiskUri = $diskuri
			$diskInfo.PrimaryStagingAzureStorageAccountId = $v2StagingSA
			$diskInfo.RecoveryAzureStorageAccountId = $v2TargetSA
			$diskList.Add($diskInfo)
		}
		
		LogMessage -Message ("Enabling Protection for V2 VM: $enableDRName") -Type 1;
		$currentJob = New-AzureRmSiteRecoveryReplicationProtectedItem -Name $enableDRName -ProtectionContainerMapping $containerMappingObject `
			-AzureVmId $v2VmId -AzureVmDiskDetails $diskList -RecoveryResourceGroupId $recResourceGroupId 
	}
	
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTime60Seconds * 2) -OperationName ([ASROperation]::EnableDR)
    $job = Get-AzureRmSiteRecoveryJob -Name $currentJob.Name
	
	LogMessage -Message ('Job target object id : {0}, target object name : {1}' -f $job.TargetObjectId, $job.TargetObjectName) -Type 1
	
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
		
		$diskList =  New-Object System.Collections.ArrayList
		foreach ($disk in $diskIdArray)
		{
			$diskInfo=New-Object Microsoft.Azure.Commands.SiteRecovery.ASRAzureToAzureManagedDiskDetails 
			$diskInfo.DiskId = $disk
			$diskInfo.PrimaryStagingAzureStorageAccountId = $stagingSA
			$diskInfo.RecoveryAzureResourceGroupId = $recResourceGroupId
			$diskList.Add($diskInfo)
		}
		
		LogMessage -Message ("Triggering Switch protection") -Type 1;
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
		
		$diskList =  New-Object System.Collections.ArrayList
		foreach ($diskuri in $diskURIArray)
		{
			$diskInfo=New-Object  Microsoft.Azure.Commands.SiteRecovery.ASRAzureToAzureDiskDetails 
			$diskInfo.DiskUri = $diskuri
			$diskInfo.PrimaryStagingAzureStorageAccountId = $stagingSA
			$diskInfo.RecoveryAzureStorageAccountId = $recoverySA
			$diskList.Add($diskInfo)
		}

    	LogMessage -Message ("Triggering Switch protection") -Type 1;
		$currentJob = Switch-AzureRmSiteRecoveryReplicationProtectedItem -Name $switchProtectionName -ProtectionContainerMapping $containerMappingObject `
			-AzureVmDiskDetails $diskList -RecoveryResourceGroupId $recResourceGroupId -ReplicationProtectedItem $protectedItemObject
    }
	
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds ($JobQueryWaitTime60Seconds * 2) -OperationName ([ASROperation]::SwitchProtection)
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
                    LogMessage -Message ("Triggering DisableDR for item: {0}" -f $protectedItem.Name) -Type 1
                    $currentJob = Remove-AzureRmSiteRecoveryReplicationProtectedItem -ReplicationProtectedItem $protectedItem -Force
                    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DisableDR)
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
                    LogMessage -Message ("Triggering Remove Container Mapping: {0}" -f $containerMapping.Name) -Type 1
                    $currentJob = Remove-AzureRmSiteRecoveryProtectionContainerMapping -ProtectionContainerMapping $containerMapping
                    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DissociatePolicy)
                }

                # Remove container
                LogMessage -Message ("Triggering Remove Container: {0}" -f $containerObject.FriendlyName) -Type 1
                $currentJob = Remove-AzureRmSiteRecoveryProtectionContainer -ProtectionContainer $containerObject
                WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DeleteContainer)
            }

            # Remove Fabric
            LogMessage -Message ("Triggering Remove Fabric: {0}" -f $fabricObject.FriendlyName) -Type 1
            $currentJob = Remove-AzureRmSiteRecoveryFabric -Fabric $fabricObject -Force
            WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DeleteFabric)
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
		LogMessage -Message ("Uploading GQL Binaries to storage account : {0}" -f $storageAccountName) -Type 1
		
		$VMInfo = GetVMDetails
		$resourceGroupName = $VMInfo.ResourceGroup
		$storageAccountName = $VMInfo.StorageAccount
		$storageAccountKey = $VMInfo.StorageKey
		
		LogMessage -Message ("StorageAccount : {0}, StorageKey : {1}, Container : {2}" -f $storageAccountName, $storageAccountKey, $global:ContainerName) -Type 1
		
		$storageContext = New-AzureStorageContext -StorageAccountName $storageAccountName -StorageAccountKey $storageAccountKey
		if(!$?)
		{
			LogMessage -Message ("Failed to fetch storage context") -Type 0	
		}
		
		$containerObj = Get-AzureStorageContainer -Context $storageContext -Name $global:ContainerName -ErrorAction SilentlyContinue
		if($containerObj -eq $null )
		{
			# Create new container
			LogMessage -Message ("Creating a new container : {0}" -f $global:ContainerName) -Type 1
			New-AzureStorageContainer -Name $global:ContainerName -Context $storageContext -Permission Container
			
			if(!$?)
			{
				LogMessage -Message ("Failed to create container") -Type 0
				exit 1
			}
		} 
		else 
		{ 
			LogMessage -Message ("Container : {0} already exists.. Skipping the creation" -f $global:ContainerName) -Type 1
		}
		
		# Upload GQL utilities
		LogMessage -Message ("Started uploading GQL Binaries to storage account : {0}" -f $storageAccountName) -Type 1
		$Scripts = Get-ChildItem "$PSScriptRoot\TestBinRoot\*"
		if ( !$? )
		{
			LogMessage -Message ("Config files not found") -Type 0
			exit 1
		}
		
		foreach ( $file in $Scripts )
		{
			LogMessage -Message ("Uploading file : {0}" -f $file) -Type 1	
			Set-AzureStorageBlobContent -Container $global:ContainerName -File $file -Blob $file.Name -Context $storageContext -force >> $null
			if ( !$? )
			{
				LogMessage -Message ("Failed to upload file : {0} to azure" -f $file.Name) -Type 0
				exit 1
			}
			else
			{
				LogMessage -Message ("Successfully uploaded file : {0} to azure" -f $file.Name) -Type 1
			}
		}
	}
	Catch
	{
		LogMessage -Message ("UploadFiles : ERROR : {0}" -f $_.Exception.Message) -Type 0
		exit 1
	}
	
	LogMessage -Message ("Successfully uploaded files to {0} storage account" -f $storageAccountName) -Type 1
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
		LogMessage -Message ("Setting Custom Extension Script for vm {0}" -f $VMName) -Type 1
		
		LogMessage -Message ("StorageAccount : {0}, StorageKey : {1}, Container : {2}, Args : {3}, ScriptName : {4}" `
		-f $storageAccountName, $storageKey, $global:ContainerName, $arguments, $scriptName) -Type 1
		
		Get-AzureRmVMExtension -ResourceGroupName $resourceGroupName -Name $global:CustomExtensionName -VMName $VMName -ErrorAction SilentlyContinue	
		if($? -eq "True")
		{
			LogMessage -Message ("Removing existing custom script extension") -Type 1
			Remove-AzureRmVMExtension -ResourceGroupName $resourceGroupName -Name $global:CustomExtensionName -VMName $VMName -Force
			if ( !$? )
			{
				LogMessage -Message ("ERROR : Failed to remove existing custom script extension on VM : $VMName") -Type 0
				#throw "SetCustomScriptExtension failed."
			}
			else
			{   
				LogMessage -Message ("Successfully removed the existing custom script extension from vm : $VMName, Extension : $global:CustomerExtensionName") -Type 1
			}
		}
        # Fix for extension removal issue
        # remove exception in case of failure to remove extension
        # create extension with random name and remove 
        $rand = Get-Random -minimum 1 -maximum 999
		$global:CustomExtensionName = "a2acustomscript" + $rand
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
			LogMessage -Message ("Failed to set custom script extension for vm {0}" -f $VMName) -Type 0
			throw "SetCustomScriptExtension failed."
		}
		else
		{
			LogMessage -Message ("Successfully set custom script extension for vm {0}" -f $VMName) -Type 1
		}
		
		$VM = Get-AzureRmVM -ResourceGroupName $resourceGroupName -Name $VMName
		
		Update-AzureRmVM -ResourceGroupName $resourceGroupName -VM $VM
		if( !$? )
		{
			LogMessage -Message ("Failed to Update Azure RM VM for vm {0}" -f $VMName) -Type 1
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
		
		LogMessage -Message ("Result : {0}, Status : {1}" -f $result, $status) -Type 1
		
		if( $result -match "Success")
		{
			LogMessage -Message ("Successfully set custom extension on VM {0}" -f $VMName) -Type 1
		}
		else
		{
			LogMessage -Message ("Failed to set custom extension") -Type 0
			throw "SetCustomScriptExtension failed."
		}

        #Newly added code
		Get-AzureRmVMExtension -ResourceGroupName $resourceGroupName -Name $global:CustomExtensionName -VMName $VMName -ErrorAction SilentlyContinue	
		if($? -eq "True")
		{
			LogMessage -Message ("[AfterSetExtension]Removing existing custom script extension") -Type 1
			Remove-AzureRmVMExtension -ResourceGroupName $resourceGroupName -Name $global:CustomExtensionName -VMName $VMName -Force
			if ( !$? )
			{
				LogMessage -Message ("[AfterSetExtension]ERROR : Failed to remove existing custom script extension on VM : $VMName") -Type 0
				throw "[AfterSetExtension]SetCustomScriptExtension failed."
			}
			else
			{   
				LogMessage -Message ("[AfterSetExtension]Successfully removed the existing custom script extension from vm : $VMName, Extension : $global:CustomerExtensionName") -Type 1
			}
		}

	}
	Catch
	{
			LogMessage -Message ("Failed with error - {0} for vm {1}" -f $_.Exception.Message, $VMName) -Type 0
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

	    LogMessage -Message ("Key:{0}, value:{1}" -f $_,$value) -Type $type
	}
}	
	
#
# Clean azure artifacts
#
Function CleanAzureArtifacts
{
    # removing below artifacts from recovery resource group
    LogMessage -Message ("*****Removing azure artifacts from recovery RG:{0}*****" -f $global:RecResourceGroup) -Type 1
    $vmsToRemove = Find-AzureRmResource -ResourceGroupNameContains $global:RecResourceGroup -ResourceType "Microsoft.Compute/virtualMachines"
    LogMessage -Message ("VMs Found:{0}, ResourceGroup:{1}, removing all" -f $vmsToRemove.Count, $global:RecResourceGroup) -Type 1
    $vmsToRemove | Remove-AzureRmVM -Force

    $nicsToRemove = Find-AzureRmResource -ResourceGroupNameContains $global:RecResourceGroup -ResourceType "Microsoft.Network/networkInterfaces"
    LogMessage -Message ("NICs Found:{0}, ResourceGroup:{1}, removing all" -f $nicsToRemove.Count, $global:RecResourceGroup) -Type 1
    $nicsToRemove | Remove-AzureRmNetworkInterface -Force

    $publicIpsToRemove = Find-AzureRmResource -ResourceGroupNameContains $global:RecResourceGroup -ResourceType "Microsoft.Network/publicIPAddresses"
    LogMessage -Message ("PublicIps Found:{0}, ResourceGroup:{1}, removing all" -f $publicIpsToRemove.Count,$global:RecResourceGroup) -Type 1
    $publicIpsToRemove | Remove-AzureRmPublicIpAddress -Force

    $vnetsToRemove = Find-AzureRmResource -ResourceGroupNameContains $global:RecResourceGroup -ResourceType "Microsoft.Network/virtualNetworks"
    LogMessage -Message ("Vnet Found:{0}, ResourceGroup:{1}, removing all" -f $vnetsToRemove.Count, $global:RecResourceGroup) -Type 1
    $vnetsToRemove | Remove-AzureRmVirtualNetwork -Force

    $nsgToRemove = Find-AzureRmResource -ResourceGroupNameContains $global:RecResourceGroup -ResourceType "Microsoft.Network/networkSecurityGroups"
    LogMessage -Message ("NSGs Found:{0}, ResourceGroup:{1}, removing all" -f $nsgToRemove.Count, $global:RecResourceGroup) -Type 1
    $nsgToRemove | Remove-AzureRmNetworkSecurityGroup -Force
    
    if(!$IsManagedDisk)
    {
        LogMessage -Message ("Removing Primary Storage Account : {0}" -f $global:PriStorageAccount) -Type 1
        #Remove-AzureRmStorageAccount -Name $global:PriStorageAccount -ResourceGroupName $global:PriResourceGroup -Force
    }
    else
    {
        LogMessage -Message ("Removing Staging Storage Account : {0}" -f $global:PriStorageAccount) -Type 1
        Remove-AzureRmStorageAccount -Name $global:StagingStorageAcc -ResourceGroupName $global:PriResourceGroup -Force
    }
    
    #LogMessage -Message ("Removing all the unused nics") -Type 1
    #Get-AzureRmNetworkInterface | where { -Not $_.VirtualMachine } | Remove-AzureRmNetworkInterface -Force
    
    LogMessage -Message ("Removing Recovery ResourgeGroup:{0} and all its artifacts" -f $global:RecResourceGroup) -Type 1
    $currentJob = Remove-AzureRmResourceGroup -Name $global:RecResourceGroup -Force
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DeleteRecoveryRG)

    # Remove Vault resource group and including vault
    LogMessage -Message ("Removing Vault ResourceGroup:{0}" -f $global:VaultRGName) -Type 1
    $currentJob = Remove-AzureRmResourceGroup -Name $global:VaultRGName -Force
    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds -OperationName ([ASROperation]::DeleteVaultRG)

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
	LogMessage -Message ("Container: {0}, Role: {1}, AvailablePolicies: {2}" -f $containerObject.FriendlyName, $containerObject.Role, $containerObject.AvailablePolicies.FriendlyName ) -Type 1;
	
	$protectedItemObject = Get-AzureRmSiteRecoveryReplicationProtectedItem -ProtectionContainer $containerObject | where { $_.FriendlyName -eq $global:VMName }
	LogMessage -Message ("Protected VM: {0} fetched successfully." -f $global:VMName) -Type 1;
	
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
		LogMessage -Message ("Triggering Get RecoveryPoint.") -Type 1
		$recoveryPoints = Get-AzureRmSiteRecoveryRecoveryPoint -ReplicationProtectedItem $protectedItemObject | Sort-Object RecoveryPointTime -Descending
		if($recoveryPoints.Count -eq 0)
		{
			$flag = $true;
			LogMessage -Message ("No Recovery Points Available. Waiting for: {0} sec before querying again." -f ($JobQueryWaitTime60Seconds * 2)) -Type 1;
			Start-Sleep -Seconds ($JobQueryWaitTime60Seconds * 2);
		}
	}while($flag)
	LogMessage -Message ("Number of RecoveryPoints found: {0}" -f $recoveryPoints.Count) -Type 1;
	
	if($tagType -eq "Latest")
	{
		$recoveryPoint = GetLatestTag -recoveryPoints $recoveryPoints
		LogMessage -Message ("Latest recovery point : {0}" -f $recoveryPoint) -Type 1;
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
			LogMessage -Message ("Picking latest tag as custom tag is null, rec point name:{0}" -f $recoveryPoint.Name) -Type 1;
		}
        else
        {
            LogMessage -Message ("Rec point name:{0}" -f $recoveryPointName) -Type 1;
		    #LogMessage -Message ("Rec point time:{0}" -f $recoveryPoint.RecoveryPointTime.ToString("MM/dd/yyyy hh:mm:ss tt")) -Type 1;
		
        }
	}
	
	return [PSObject]$recoveryPoint
}

if($Global:Logger -eq $null)
{
	$Global:Logger = InitializeLogger;
}

#####
Write-Host "CommonFunctions.ps1 executed"
