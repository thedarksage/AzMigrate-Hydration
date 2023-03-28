###
# Common functions
##

# ########### Header ###########
# Refer common library file
. $PSScriptRoot\CommonParams.ps1
. $PSScriptRoot\TestStatus.ps1
# #############################

<#
.SYNOPSIS
    Write messages to log file
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
    elseif ($LogType -eq [LogType]::Info) {
        $logLevel = "INFO"
        $color = "Green"
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

<#
.SYNOPSIS
    Create Log directory, if not exists.
#>
Function CreateLogDir() {
    if ( !$(Test-Path -Path $global:LogDir -PathType Container) ) {
        Write-Host "Creating the directory $global:LogDir"
        New-Item -Path $global:LogDir -ItemType Directory -Force >> $null

        if ( !$? ) {
            Write-Error "Error creating log directory $global:LogDir"
            exit 1
        }
    }
}

<#
.SYNOPSIS
    Waits for job completion
#>
Function WaitForJobCompletion { 
    param(
        [parameter(Mandatory = $True)] [string] $JobId,
        [int] $JobQueryWaitTimeInSeconds = 30
    )

    $isJobLeftForProcessing = $true
    LogMessage -Message ("Job - {0} is in-Progress" -f $JobId) -LogType ([LogType]::Info)

    do {
        $azJob = Get-AzRecoveryServicesAsrJob -Name $JobId
        $azJob
		
        if ($azJob.State -eq "InProgress" -or $azJob.State -eq "NotStarted") {
            $isJobLeftForProcessing = $true
        }
        else {
            $isJobLeftForProcessing = $false
        }

        if ($isJobLeftForProcessing) {
            LogMessage -Message ("{0} - {1} in-Progress...Wait: {2} Sec, ClientRequestId: {3}, JobID: {4}" -f `
                    $azJob.JobType, $azJob.DisplayName, $JobQueryWaitTimeInSeconds.ToString(), $azJob.ClientRequestId, $azJob.Name) -LogType ([LogType]::Info)
            Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
        }
        else {
            if ($null -eq $azJob.DisplayName -or $azJob.DisplayName -eq '') {
                $messageToDisplay = "NA"
            }
            else {
                $messageToDisplay = $azJob.DisplayName
            }

            if ($azJob.State -eq "Failed") {
				LogMessage -Message ("Provider error details: {0}" -f ($azJob.Errors.ProviderErrorDetails | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
                LogMessage -Message ("{0}, Status: {1}" -f $messageToDisplay, $azJob.State) -LogType ([LogType]::Info)
                LogMessage -Message ("Task Details: {0}" -f ($azJob.Tasks | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
                LogMessage -Message ("Error Details: {0}" -f ($azJob.Errors.ServiceErrorDetails | ConvertTo-json -Depth 1)) -LogType ([LogType]::Error)
                $errorMessage = ($azJob.Errors.ServiceErrorDetails | ConvertTo-json -Depth 1)
                throw $errorMessage
            }
            else {
                LogMessage -Message ("{0}, Status: {1}" -f $messageToDisplay, $azJob.State) -LogType ([LogType]::Info)
            }
			
        }
    }While ($isJobLeftForProcessing)
}

<#
.SYNOPSIS
    Waits for IR job completion
#>
Function WaitForIRCompletion { 
    param(
        [string] $VMName,
        [string] $ProtectionDirection = "AzureToVmware",
        [int] $JobQueryWaitTimeInSeconds = 60,
        [int] $MaxIRWaitTimeInSeconds = 60 * 60 * 10
    )

    $isProcessingLeft = $true
    $IRjobs = $null
    $StopWatch = New-Object -TypeName System.Diagnostics.Stopwatch
    $StopWatch.Start()
    LogMessage -Message ("IR in Progress for {0}" -f $VMName) -LogType ([LogType]::Info)
    
    $jobCount = 1
    if ($ProtectionDirection -eq "VmwareToAzure") {
        $jobCount = 2
    }
    LogMessage -Message ("Protection direction : {0}, jobcount : {1}" -f $ProtectionDirection, $jobCount) -LogType ([LogType]::Info)

    $waitTimeCounter = 0
    do {
        $IRjobs = Get-AzRecoveryServicesAsrJob `
        | Sort-Object StartTime -Descending `
        | Select-Object -First $jobCount `
        | Where-Object { $_.TargetObjectName -eq $VMName } `
        | Where-Object { $_.JobType -like "*IrCompletion" }
         
        LogMessage -Message ("Jobs Found:{0}" -f $IRjobs.Count) -LogType ([LogType]::Info)

        if ($null -eq $IRjobs -or $IRjobs.Count -lt $jobCount) {
            $isProcessingLeft = $true
        }
        else {
            $isProcessingLeft = $false
        }

        if ($isProcessingLeft) {
            LogMessage -Message ("Elapsed Time: {0}" -f $StopWatch.Elapsed.ToString()) -LogType ([LogType]::Info)
            $waitTimeCounter += $JobQueryWaitTimeInSeconds
            if ($waitTimeCounter -gt $MaxIRWaitTimeInSeconds) {
                $errorMsg = ("IR not completed after Waiting for '{0}' seconds for {1}. Failing test." -f `
                        $MaxIRWaitTimeInSeconds, $VMName)
                LogMessage -Message ($errorMsg) -LogType ([LogType]::Error)
                Throw $errorMsg
            }
            LogMessage -Message ("IR in Progress.Wait: {0} Sec" -f $JobQueryWaitTimeInSeconds.ToString()) -LogType ([LogType]::Info)
            Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
        }
    }While ($isProcessingLeft)
	
    LogMessage -Message ("Total IR Time: {0}" -f $StopWatch.Elapsed.ToString()) -LogType ([LogType]::Info)
    LogMessage -Message ("Finalize IR jobs, Job Count: {0}" -f $IRjobs.Count) -LogType ([LogType]::Info)

    $IRjobs
    foreach ($job in $IRjobs) {
        WaitForJobCompletion -JobId $job.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
    }
	
    LogMessage -Message ("Total IR + Finalize IR Time: {0} Sec for {1} VM" -f `
            $StopWatch.Elapsed.ToString(), $VMName) -LogType ([LogType]::Info)
    $StopWatch.Stop()
}

<#
.SYNOPSIS
    Wait till the replicated time reaches protected state
#>
Function WaitForProtectedItemState { 
    param(
        [string] $State = "Protected",
        [string] $VMName,
        [PSObject] $ContainerObject,
        [int] $JobQueryWaitTimeInSeconds = 60
    )

    LogMessage -Message ("Waiting for state:{0}" -f $State) -LogType ([LogType]::Info)

    # Adding a timeout of 10 hours
    $maxWaitTime = 10 * $JobQueryWaitTimeInSeconds * $JobQueryWaitTimeInSeconds
    $stateChanged = $true
	$retries = 10
    do {
        $protectedItemObject = Get-AzRecoveryServicesAsrReplicationProtectedItem `
            -FriendlyName $VMName `
            -ProtectionContainer $ContainerObject
		if ((!$protectedItemObject) -and ($retries -ne 0)) {
			LogMessage -Message ("Failed to fetch the protected item object for the VM {0}..retry after sleep." -f $VMName) -LogType ([LogType]::Error)
			$retries--
			Start-Sleep -Seconds 60
			continue
		}
        Assert-NotNull($protectedItemObject)
		$retries = 10

        LogMessage -Message ("Current state:{0}, Waiting for state:{1}" -f $protectedItemObject.ProtectionState, $State) -LogType ([LogType]::Info)
        if ($protectedItemObject.ProtectionState -eq $State) {
            $stateChanged = $false
        }
        else {
            $waitTimeCounter += $JobQueryWaitTimeInSeconds
            LogMessage -Message ("Waiting for: {0} sec before querying again." -f ($JobQueryWaitTimeInSeconds)) -LogType ([LogType]::Info)
            Start-Sleep -Seconds ($JobQueryWaitTimeInSeconds)
        }
    }While ($stateChanged -and ($waitTimeCounter -le $maxWaitTime))

    # Fail the job, if the replicaiton state is not changed after timeout
    if ($stateChanged -ne $false) {
        $errorMsg = ("Replication didn't move to protected state even after waiting for '{0}' seconds for {1}. Failing test." -f `
                $maxWaitTime, $VMName)
        LogMessage -Message ($errorMsg) -LogType ([LogType]::Error)
        Throw $errorMsg
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

<#
.SYNOPSIS
    Get Fabric details
#>
Function GetFabricInfo {
    # Get Replication Protected Item
    $fabric = Get-AzRecoveryServicesAsrFabric `
        -FriendlyName $global:CSPSName `
    | Sort-Object -Descending @{Expression = { $_.FabricSpecificDetails.LastHeartbeat } } | Select-Object -First 1
    Assert-NotNull($fabric)
	
    $protectionContainer = Get-AzRecoveryServicesAsrProtectionContainer `
        -Fabric $fabric `
        -FriendlyName $global:CSPSName
    Assert-NotNull($protectionContainer)

    $rpi = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $protectionContainer -Name $global:RpiName
    Assert-NotNull($rpi)

    $psAccount = $fabric.FabricSpecificDetails.ProcessServers `
    | Where-Object { $_.FriendlyName -match $global:CSPSName }
    $vmAccount = $fabric.FabricSpecificDetails.RunAsAccounts `
    | Where-Object { $_.AccountName -match $rpi.FriendlyName }
    $isLinux = $rpi.ProviderSpecificDetails `
    | Where-Object { $_.OSType -match "Linux" }
    if ($null -ne $isLinux) {
        $mtAccount = $fabric.FabricSpecificDetails.MasterTargetServers | where-object { $_.OsType -match "Linux" }
    }
    else {
        $mtAccount = $fabric.FabricSpecificDetails.MasterTargetServers | where-object { $_.OsType -match "Windows" }
    }
    
    [hashtable]$fabricInfo = @{ }
    $fabricInfo.Fabric = $fabric
    $fabricInfo.ProtectionContainer = $protectionContainer
    $fabricInfo.RPI = $rpi
    $fabricInfo.PSAccount = $psAccount
    $fabricInfo.VMAccount = $vmAccount
    $fabricInfo.MTAccount = $mtAccount
    
    # Fetch Azure PS details if it has to be used while reprotect
    if ($global:UseAzurePS -eq "True") {
        $azPSAccount = $fabric.FabricSpecificDetails.ProcessServers | Where-Object { $_.FriendlyName -match $global:AzPSName }
        $fabricInfo.AzPSAccount = $azPSAccount
    }

    return $fabricInfo
}

<#
.SYNOPSIS
    Wait for VM ProvisioningState=succeeded and VM PowerState=running 
#>
Function WaitForAzureVMProvisioningAndPowerState { 
    param(
        [string] $VMName,
        [string] $ResourceGroupName,
        [int] $JobQueryWaitTimeInSeconds = 30
    )
	
    $provisioningState = "ProvisioningState/succeeded"
    $powerState = "PowerState/running"
    # Setting the timeout to 60 mins.
    $maxWaitTime = 60 * $JobQueryWaitTime60Seconds
    LogMessage -Message ("Quering VM provisioning and power State for VM : {0}" -f $VMName) -LogType ([LogType]::Info)

    $stateChanged = $true
    $waitTimeCounter = 0
    do {
		try {
			$vm = Get-AzVM -Name $VMName -ResourceGroupName $ResourceGroupName -Status
			
			LogMessage -Message ("Current Vm Provisioning state:{0}, Power State:{1} , Expected Provisioning state:{2}, Power state:{3} , RG:{4}" -f `
					$vm.Statuses.Code[0], $vm.Statuses.Code[1], $provisioningState, $powerState, $ResourceGroupName) -LogType ([LogType]::Info)
			if ($vm.Statuses.Code[0] -eq $provisioningState -and $vm.Statuses.Code[1] -eq $powerState) {
				$stateChanged = $false
			}        
		}
		catch
		{
			LogMessage -Message ("ERROR:: {0}" -f ($Error | ConvertTo-json -Depth 1)) -LogType ([LogType]::Error)
		}
		
		if ($stateChanged) {
            $waitTimeCounter += $JobQueryWaitTimeInSeconds
            LogMessage -Message ("Waiting for: {0} sec before querying again." -f $JobQueryWaitTimeInSeconds) -LogType ([LogType]::Info)
            Start-Sleep -Seconds $JobQueryWaitTimeInSeconds
        }
    }While ($stateChanged -and ($waitTimeCounter -le $maxWaitTime))

    if ($stateChanged -ne $false) {
        $errorMsg = ("{0} didn't move to {1} state even after waiting for '{2}' seconds. Failing test." -f `
                $VMName, $powerState, $maxWaitTime)
        LogMessage -Message ($errorMsg) -LogType ([LogType]::Error)
        Throw $errorMsg
    }
}

<#
.SYNOPSIS
    Remove Azure VM
#>
Function RemoveAzureVM {
    param(
        [string] $ResourceGroupName,
        [string] $VMName
    )

    $vm = Get-AzVM -Name $VMName -ResourceGroupName $ResourceGroupName

    if ($null -ne $vm) {
        # Remove Azure VM
        LogMessage -Message ("Removing the Azure VM : {0}" -f $VMName) -LogType ([LogType]::Info)
        Remove-AzVM -ResourceGroupName $ResourceGroupName -Name $VMName -Force
        if ($?) {
            LogMessage -Message ("Deleted Azure VM : {0}" -f $VMName) -LogType ([LogType]::Info)

            # TODO: Add support for non-managed disks deletion
            # Remove OSDisk
            $osDisk = Get-AzDisk -ResourceGroupName $vm.ResourceGroupName -DiskName $vm.StorageProfile.OsDisk.Name
            if ($null -ne $osDisk) {
                $osDisk | Remove-AzDisk -Force
                if (!$?) {
                    LogMessage -Message ("Failed to delete os disk : {0}" -f $osDisk.Name) -LogType ([LogType]::Warning)
                }
            }

            # Remove DataDisks
            $dataDisks = $vm.StorageProfile.DataDisks
            foreach ($disk in $dataDisks) {
                $dataDisk = Get-AzDisk -ResourceGroupName $vm.ResourceGroupName -DiskName $disk.Name
                if ($null -ne $dataDisk) {
                    $dataDisk | Remove-AzDisk -Force
                    if (!$?) {
                        LogMessage -Message ("Failed to delete data disk : {0}" -f $dataDisk.Name) -LogType ([LogType]::Warning)
                    }
                }
            }

            # Delete the Virtual network interface, if exists
            foreach ($nicUri in $vm.NetworkProfile.NetworkInterfaces.Id) {
                $nic = Get-AzNetworkInterface -ResourceGroupName $vm.ResourceGroupName -Name $nicUri.Split('/')[-1]
                Remove-AzNetworkInterface -Name $nic.Name -ResourceGroupName $vm.ResourceGroupName -Force
                if (!$?) {
                    LogMessage -Message ("Failed to delete nic : {0}" -f $nic) -LogType ([LogType]::Warning)
                }

                foreach ($ipConfig in $nic.IpConfigurations) {
                    if ($null -ne $ipConfig.PublicIpAddress) {
                        Remove-AzPublicIpAddress -ResourceGroupName $vm.ResourceGroupName -Name $ipConfig.PublicIpAddress.Id.Split('/')[-1] -Force
                    }
                }
            }
        }
        else {
            LogMessage -Message ("Failed to delete Azure VM : {0}" -f $VMName) -LogType ([LogType]::Warning)
        }
    }
}

<#
.SYNOPSIS
    Fetch Latest recovery point
#>
Function GetLatestRecoveryPoint {
    param(
        [parameter(Mandatory = $True)][PSObject] $RecoveryPoints
    )
	
    if ($RecoveryPoints.Count -eq 1) {
        $recoveryPoint = $RecoveryPoints[0]
    }
    else {
        $recoveryPoint = $RecoveryPoints[$RecoveryPoints.Count - 1]
    }
	
    return [PSObject]$recoveryPoint
}

<#
.SYNOPSIS
    Fetch Recovery points
#>
Function GetRecoveryPoint {
    param(
        [parameter(Mandatory = $True)][PSObject] $ProtectedItemObject,
        [parameter(Mandatory = $True)][TagType] $TagType,
		[parameter(Mandatory = $False)][bool] $IgnoreTagType = $false
    )
	
    # Wait for recovery point to generate(10mins, for at least one CC)
    #Start-Sleep -Seconds 600
	
    do {
        $flag = $false
        LogMessage -Message ("Triggering Get RecoveryPoint.") -LogType ([LogType]::Info)
        $recoveryPoints = Get-AzRecoveryServicesAsrRecoveryPoint -ReplicationProtectedItem $ProtectedItemObject
        if ($recoveryPoints.Count -eq 0) {
            $flag = $true
            LogMessage -Message ("No Recovery Points Available. Waiting for: {0} sec before querying again." -f `
                ($JobQueryWaitTime60Seconds * 2)) -LogType ([LogType]::Info)
            Start-Sleep -Seconds ($JobQueryWaitTime60Seconds * 2)
        }
    }while ($flag)
    LogMessage -Message ("Number of RecoveryPoints found: {0}" -f $recoveryPoints.Count) -LogType ([LogType]::Info)
	$recoveryPoints | Out-File $global:LogName -Append
	
	if ($IgnoreTagType)
	{
		LogMessage -Message ("Ignoring Tag type") -LogType ([LogType]::Info)
		return
	}
	
    if ($TagType -eq [TagType]::ApplicationConsistent) {
        $recoveryPoints = $recoveryPoints | Where-Object { $_.RecoveryPointType -match [TagType]::ApplicationConsistent }
    }
    elseif ($TagType -eq [TagType]::CrashConsistent) {
        $recoveryPoints = $recoveryPoints | Where-Object { $_.RecoveryPointType -match [TagType]::CrashConsistent }
    }

    $recoveryPoints = $recoveryPoints | Sort-Object RecoveryPointTime -Descending
    $recoveryPoint = GetLatestRecoveryPoint -recoveryPoints $recoveryPoints
    LogMessage -Message ("Recovery point : {0}, Recovery point type : {1} ,Recovery point time : {2}" -f $recoveryPoint.Name, $recoveryPoint.RecoveryPointType, $recoveryPoint.RecoveryPointTime.ToString("MM/dd/yyyy hh:mm:ss tt")) -LogType ([LogType]::Info)
	
    return [PSObject]$recoveryPoint
}

<#
.SYNOPSIS
    Remove Azure Fabrics and Containers
#>
Function ASRCheckAndRemoveFabricsNContainers {
    # Check and remove fabric, if exists
    $fabricObjects = Get-AzRecoveryServicesAsrFabric
    if ($null -ne $fabricObjects) {
        # First DisableDR all VMs.
        foreach ($fabricObject in $fabricObjects) {
            $containerObjects = Get-AzRecoveryServicesAsrProtectionContainer -Fabric $fabricObject
            foreach ($containerObject in $containerObjects) {
                $protectedItems = Get-AzRecoveryServicesAsrReplicationProtectedItem -ProtectionContainer $containerObject
                # DisableDR all protected items
                foreach ($protectedItem in $protectedItems) {
                    LogMessage -Message ("Triggering DisableDR(Purge) for item: {0}" -f $protectedItem.Name) -LogType ([LogType]::Info)
                    $currentJob = Remove-AzRecoveryServicesAsrReplicationProtectedItem -InputObject $protectedItem -Force
                    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
                    LogMessage -Message ("DisableDR(Purge) completed") -LogType ([LogType]::Info)
                }
            }
        }

        # Remove all mappings
        foreach ($fabricObject in $fabricObjects) {
            if ($fabricObject.FriendlyName -eq "Microsoft Azure") {
                continue
            }

            $containerObjects = Get-AzRecoveryServicesAsrProtectionContainer `
                -Fabric $fabricObject
            foreach ($containerObject in $containerObjects) {
                $containerMappings = Get-AzRecoveryServicesAsrProtectionContainerMapping `
                    -ProtectionContainer $containerObject
                # Remove all Container Mappings
                foreach ($containerMapping in $containerMappings) {
                    LogMessage -Message ("Triggering Remove Container Mapping: {0}" -f $containerMapping.Name) -LogType ([LogType]::Info)
                    $currentJob = Remove-AzRecoveryServicesAsrProtectionContainerMapping -ProtectionContainerMapping $containerMapping
                    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
                    LogMessage -Message ("Removed Container Mappings") -LogType ([LogType]::Info)
                }

                try {
                    # Remove container
                    LogMessage -Message ("Triggering Remove Container: {0}" -f $containerObject.FriendlyName) -LogType ([LogType]::Info)
                    $currentJob = Remove-AzRecoveryServicesAsrProtectionContainer -InputObject $containerObject
                    WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
                    LogMessage -Message ("Removed Container") -LogType ([LogType]::Info)
                }
                catch {
                    LogMessage -Message ('Failed to remove protection container {0}. Ignoring the exception.' -f $containerObject.FriendlyName) -LogType ([LogType]::Warning)
                }

                # Remove Vcenter
                $vCenter = Get-AzRecoveryServicesAsrvCenter -Fabric $fabricObject -Name $global:VcenterName
                if ($null -ne $vCenter.FriendlyName) {
                    LogMessage -Message ("Removing Vcenter : {0}" -f $global:VcenterName) -LogType ([LogType]::Info)
                    $job = Remove-AzRecoveryServicesAsrvCenter -InputObject $vCenter
                    WaitForJobCompletion -JobId $job.name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
                    LogMessage -Message ("Removed Vcenter") -LogType ([LogType]::Info)
                }

                # Remove Fabric
                LogMessage -Message ("Triggering Remove Fabric: {0}" -f $fabricObject.FriendlyName) -LogType ([LogType]::Info)
                $currentJob = Remove-AzRecoveryServicesAsrFabric -InputObject $fabricObject -Force
                WaitForJobCompletion -JobId $currentJob.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
                LogMessage -Message ("Removed Fabric") -LogType ([LogType]::Info)
            }
        }
    }
}

<#
.SYNOPSIS
    Login to Azure
#>
Function LoginToAzure {
	LogMessage -Message ("Logging to Azure") -LogType ([LogType]::Info)
	
	$retry = 1
	$sleep = 0
	while ($retry -ge 0) {
		Start-Sleep -Seconds $sleep
		$cert = Get-ChildItem -path 'Cert:\CurrentUser\My' | where {$_.Subject -eq 'CN=Test123' }
		LogMessage -Message ("Logging using User: {0}" -f $env:username) -LogType ([LogType]::Info)
		Start-Sleep -Seconds $sleep
		$TPrint = $cert.ThumbPrint
		if (!$cert -or !$TPrint) {
			if ($retry -eq 0) {
				LogMessage -Message ("Failed to fetch the thumbprint") -LogType ([LogType]::Error)
				return $false
			}
			
			LogMessage -Message ("Failed to fetch the thumbprint..retrying") -LogType ([LogType]::Info)
			$retry = 0
			$sleep = 60
		} else {
			break
		}
     }

	LogMessage -Message ("ThumbPrint: {0}, ApplicationId: {1} and TenantId: {2}" -f $TPrint, $global:ApplicationId, $global:TenantId) -LogType ([LogType]::Info)
	Login-AzAccount -ServicePrincipal -CertificateThumbprint $TPrint -ApplicationId $global:ApplicationId -Tenant $global:TenantId
	if (!$?) {
		LogMessage -Message ("Unable to login to Azure account using Thumbprint: {0}, ApplicationId: {1} and TenantId: {2}" -f $TPrint, $global:ApplicationId, $global:TenantId) -LogType ([LogType]::Error)
		return $false
	}
	
	LogMessage -Message ("Login to Azure succeeded...Selecting subscription : {0}" -f $SubscriptionName) -LogType ([LogType]::Info)
	Get-AzSubscription -SubscriptionName  $SubscriptionName | Select-AzSubscription
	if (!$?) {
		LogMessage -Message ("Failed to select the subscription {0}" -f $SubscriptionName) -LogType ([LogType]::Error)
		return $false
	}

	return $true
}


<#
.SYNOPSIS
    Refresh ConfigServer
#>
Function RefreshCS {
    $azFabric = Get-AzRecoveryServicesAsrFabric -FriendlyName $global:CSPSName
    Assert-NotNull($azFabric)
    $azProvider = Get-AzRecoveryServicesAsrServicesProvider -Fabric $azFabric
    Assert-NotNull($azProvider)

    LogMessage -Message ("Refresh CS: {0}" -f $global:CSPSName) -LogType ([LogType]::Info)
    $job = Update-AzRecoveryServicesAsrServicesProvider -InputObject $azProvider
    WaitForJobCompletion -JobId $job.Name -JobQueryWaitTimeInSeconds $JobQueryWaitTimeInSeconds
    LogMessage -Message ("Refresh CS job is completed") -LogType ([LogType]::Info)
}

<#
.SYNOPSIS
    Initialize the consolidation report
#>
Function InitReporting {	
	$Logger = @{}
	echo "ID : $global:Id $global:Scenario $global:ReportingTableName"
	$Logger = TestStatusGetContext $global:Id $global:Scenario $global:ReportingTableName
	TestStatusInit $Logger
}

<#
.SYNOPSIS
    Updates the corresponding column for an operation in the colsidated reporting table
#>
Function ReportOperationStatus {
	param(
		[parameter(Mandatory=$True)][String] $OperationName
	)
	
	$Logger = TestStatusGetContext $global:Id $global:Scenario $global:ReportingTableName
	if (!$global:ReturnValue) {
		TestStatusLog $Logger $OperationName "FAILED" $global:LogName
		exit 1
	} else {
		TestStatusLog $Logger $OperationName "PASSED"
		exit 0
	}
}

#####
Write-Host "CommonFunctions.ps1 executed"