param(
[string]$argsparameter 
)
 
$global:LogFile = "C:\ProgramData\ASR\home\svsystems\var\passphrase_update.log"

if (!$argsparameter)
{
	write-output "Argument is null, Hence exiting.." | Out-File -Append $global:LogFile -encoding ASCII
	Exit 4
}

write-output "Argument is $argsparameter" | Out-File -Append $global:LogFile -encoding ASCII

function startService
{ 
	Param(
		[parameter(position=2)]
        $sleepTime,
        [parameter(position=1)]
        $maxAttempts,
        [parameter(position=0)]
        $ServiceName
        )
    $ServiceData = Get-Service -Name $ServiceName
	$startServicestatus = 0
	$startCounter = 0
	$maxAttempts
	if ($ServiceData.Status -eq 'Running')
	{
			$startServicestatus = 1;
	}
	else
	{
		while ($ServiceData.Status -ne 'Running')
		{
			if ($startCounter -lt $maxAttempts)
			{
				Start-Service $ServiceName
				write-output $ServiceData.status | Out-File -Append $global:LogFile -encoding ASCII
				write-output 'Service starting' | Out-File -Append $global:LogFile -encoding ASCII
				Start-Sleep -seconds $sleepTime
				$ServiceData.Refresh()
				if ($ServiceData.Status -eq 'Running')
				{
					$startServicestatus = 1
					Write-output 'Service is now Running' | Out-File -Append $global:LogFile -encoding ASCII
					break
				}
			}
			$startCounter = $startCounter + 1
			$startCounter
			if ($startCounter -eq $maxAttempts)
			{
				Write-output 'Max attempts Reached' | Out-File -Append $global:LogFile -encoding ASCII
				break
			}			
		}
	}
	return $startServicestatus
}


function stopService
{ 
	Param(
		[parameter(position=2)]
        $sleepTime,
        [parameter(position=1)]
        $maxAttempts,
        [parameter(position=0)]
        $ServiceName
        )
    $ServiceData = Get-Service -Name $ServiceName
	$stopServicesStatus = 0
	$startCounter = 0

	if ($ServiceData.Status -eq 'Stopped')
	{
			$stopServicesStatus = 1;
	}
	else
	{
		while ($ServiceData.Status -ne 'Stopped')
		{
			if ($startCounter -lt $maxAttempts)
			{
				Stop-Service $ServiceName -WarningAction silentlyContinue
				write-output $ServiceData.status | Out-File -Append $global:LogFile -encoding ASCII
				write-output 'Service stopping' | Out-File -Append $global:LogFile -encoding ASCII
				Start-Sleep -seconds $sleepTime
				$ServiceData.Refresh()
				if ($ServiceData.Status -eq 'Stopped')
				{
					$stopServicesStatus = 1
					write-output 'Service is now Stopping' | Out-File -Append $global:LogFile -encoding ASCII
					break
				}
			}
			$startCounter = $startCounter + 1
			if ($startCounter -eq $maxAttempts)
			{
				write-output 'Max attempts Reached' | Out-File -Append $global:LogFile -encoding ASCII
				break
			}			
		}
	}
	write-output "returned"  | Out-File -Append $global:LogFile -encoding ASCII
	return $stopServicesStatus
}


function stopServices
{
	param([string[]]$listOfServicesToStop)
	$returnValue = 0
	$listOfServicesStopped = New-Object System.Collections.ArrayList
	Foreach($servicename in $listOfServicesToStop)
    {	
		write-output $servicename  | Out-File -Append $global:LogFile -encoding ASCII
		[void]$listOfServicesStopped.Add($servicename)
		$stopstatus = stopService $servicename 5 20
		if ($stopstatus -eq 1)
		{
			$returnValue = 1
		}
		else
		{
			$listOfServicesStopped.Reverse();
			foreach($servicenametostart in $listOfServicesStopped)
			{
				write-output "starting order"  | Out-File -Append $global:LogFile -encoding ASCII
				write-output $servicenametostart  | Out-File -Append $global:LogFile -encoding ASCII
				$returnValue = 0
				$startstatus = startService $servicenametostart 5 20
			}
			break;
		}
    }
	return $returnValue
}

function startServicesBack
{
	param([string[]]$listOfServicesToStart)
	[array]::Reverse($listOfServicesToStart)
	$returnValue = 1
	foreach($servicenametostart in $listOfServicesToStart)
	{
		$ServiceData = Get-Service -Name $servicenametostart
		
		$ServiceData.Refresh()
		write-output $servicenametostart | Out-File -Append $global:LogFile -encoding ASCII
		if ($ServiceData.Status -eq 'Stopped')
		{
			write-output "It is already in stopped, Starting.." | Out-File -Append $global:LogFile -encoding ASCII
			$startstatus = startService $servicenametostart 5 20
			if ($startstatus -eq 0)
			{
				$returnValue = 0
			}
		}
		else
		{
			write-output "It is already running, Stopping.." | Out-File -Append $global:LogFile -encoding ASCII
			$stopstatus = stopService $servicenametostart 5 20 
			write-output "Stop status"$stopstatus | Out-File -Append $global:LogFile -encoding ASCII
			if ($stopstatus -eq 1)
			{
				write-output "Starting.." | Out-File -Append $global:LogFile -encoding ASCII
				$startstatus = startService $servicenametostart 5 20
				if ($startstatus -eq 0)
				{
					$returnValue = 0
				}
			}
			else
			{
				$returnValue = 0
			}
		}
	}
	return $returnValue
}


#Pre script
if ($argsparameter -eq 'pre')
{
	$listServicesStop = @("DRA","cxprocessserver","svagents","InMage Scout Application Service","InMage PushInstall","tmansvc","processservermonitor","processserver","W3SVC")
	Write-output "Pre script execution started" | Out-File -Append $global:LogFile -encoding ASCII
	$stopServicesStatus = stopServices $listServicesStop
	if ($stopServicesStatus -ne 1)
	{
		Write-output "stopServices failed" | Out-File -Append $global:LogFile -encoding ASCII
		Exit 2
	}
	Write-output "Passphrase pre script executed successfully." | Out-File -Append $global:LogFile -encoding ASCII
	Exit 1
}


#Post script
if ($argsparameter -eq 'post')
{
	###
	#Only starting below services in post script after health factors inserted, though pre script stopped multiple services. The reason is, starting all services can clear process
	#server and master target health factors. If those health factors cleared in-line, those health never exposed to end user in portal. The health factor need to expose because
	#user need to do vault re-register and master target registration explicitly to generate latest cloud container certificates related to MT and data path to continue to work.
	###
	$listServicesStart = @("DRA","processservermonitor","processserver","InMage Scout Application Service","InMage PushInstall","W3SVC")
	Write-output "Post script execution started" | Out-File -Append $global:LogFile -encoding ASCII
	
	# Set up references for both executable and script
	$PhpExe  = "php"
	$PhpFile = "C:\ProgramData\ASR\home\svsystems\admin\web\passphrase_update.php"

	$PhpArgs = '"{0}"' -f $PhpFile

	# Invoke script using the call operator
	$PhpOutput = & $PhpExe $PhpArgs
	Write-output "PHP SCRIPT OUTPUT::" | Out-File -Append $global:LogFile -encoding ASCII
	Write-output $PhpOutput | Out-File -Append $global:LogFile -encoding ASCII
	if (([string]::IsNullOrEmpty($PhpOutput)) -or $PhpOutput -eq '3')
	{
		Write-output "Database error occurred" | Out-File -Append $global:LogFile -encoding ASCII
		Exit 3
	}
	
	$startServicesStatus = startServicesBack $listServicesStart
	Write-output "startServicesBack STATUS::" | Out-File -Append $global:LogFile -encoding ASCII
	Write-output $startServicesStatus | Out-File -Append $global:LogFile -encoding ASCII
	if ($startServicesStatus -ne 1)
	{
		Write-output "startServicesBack failed" | Out-File -Append $global:LogFile -encoding ASCII
		Exit 2
	}
}
Write-output "Configuration server passphrase update done successfully, hence, exiting with code 1" | Out-File -Append $global:LogFile -encoding ASCII
Exit 1