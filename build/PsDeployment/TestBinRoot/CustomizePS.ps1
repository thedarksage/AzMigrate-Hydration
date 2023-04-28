Param
(
	[parameter(Mandatory=$false)]
	[String]$WorkingDir = "D:\TestBinRoot"
)

# Global Variables
$PSToolsZipFile = "PSTestBinaries.zip"

#
# Log messages to text file
#
function Logger ([string]$message)
{
	$logfile = $WorkingDir + "\DeployPS.log"
	Add-Content $logfile -Value [$([DateTime]::Now)]$message
}

#
# Modify execution path to current working directory
#
function SetExecutionPath()
{
    # Move to working directory
    Logger "INFO : Changing directory to $WorkingDir"
    cd $WorkingDir
    if( !$? )
    {
        Logger "ERROR : Can't change directory to $WorkingDir"
		Write-Output "Failure"
		exit 1
    }
}

#
# Stop tmansvc,Pushinstall and cspsconfig Services
#
function StopServices()
{
	$services = @("tmansvc", "InMage PushInstall", "cxprocessserver" , "ProcessServer" , "ProcessServerMonitor")
	#$services = @("tmansvc", "InMage PushInstall", "cxprocessserver")
	foreach ($service in $services)
	{
		$svc = Get-WmiObject -Class Win32_Service -Filter "Name='$service'"
		Logger "INFO : Azure VM Service : $service status : $svc"
		if($svc -eq $null)
		{
			Logger "INFO : Service : $service is not installed on server"
			Write-Output "Failure"
			exit 1
		}
		else
		{
			$startType = $svc.StartMode
			Logger "INFO : Azure VM Service Starttype : $startType"
			if($startType -eq 'Auto')
			{
				Set-Service $service -StartupType Manual
				if(!$?)
				{
					Logger "ERROR : Failed to modify service $service status to Manual"
					Write-Output "Failure"
					exit 1
				}
				Logger "INFO : Successfully set $service type to manual"
				
				Stop-Service $service
				if(!$?)
				{
					Logger "ERROR : Failed to stop service $service"
					Write-Output "Failure"
					exit 1
				}
				Logger "INFO : Successfully stopped $service"
			}
		}
	}
}

#
# Update Registry with cxpsconfigtool entry
#
function UpdateRegistry()
{
	$regFile = $WorkingDir + "\PsRunOnce.reg"
	Logger "INFO : Updating registry keys using $regFile"
	#To do : Add a check to see if the file exists or not before running
	regedit /S $regFile
	if(!$?)
	{
		Logger "ERROR : Failed to update registry with cspsconfigtool"
		Write-Output "Failure"
		exit 1
	}
	Logger "INFO : Successfully updated registry with cspsconfigtool"
}

function RecursiveDelete($dir) 
{
	Get-ChildItem $dir -recurse -force | ? { $_.PSIsContainer -ne $true } | % {Remove-Item -Path $_.FullName -Force }
	Remove-Item $dir -Recurse -Force
	
	<#
	$files = Get-ChildItem -Path $dir | where-object { $_.Attributes -eq "Directory"} |% {$_.FullName}
	foreach($file in $files) 
	{
		if ($file -ne $null) {
			RecursiveDelete $file
		}
	}
	Remove-Item $dir -recurse -force
	if(!$? -AND (Test-Path $dir))
	{
		Logger "ERROR : Failed to delete file : $dir"
		Start-Sleep -s 40
		Logger "INFO : Retrying delete operation for $dir"
		Remove-Item $dir -recurse -force
		if(!$? -AND (Test-Path $dir))
		{
			Logger "ERROR : Failed to delete file : $file"
			#exit 1
		}
	}
	
	$status = "success"
	Get-ChildItem -Path $dir -Recurse -force | Where-Object { -not ($_.psiscontainer) } | Remove-Item –Force
	if(!$?)
	{
		Log "Failed to remove files under $dir"
		$status = "fail"
	}
	else
	{
		Remove-Item -Recurse -Force $dir
		if(!$?)
		{
			Log "Failed to remove $dir"
			$status = "fail"
		}
	}
	Start-Sleep -s 40
	if(($status -eq "fail") -AND (Test-Path $dir) )
	{
		&cmd.exe /c rd /s /q $dir
		if(!$?)
		{
			Log "Failed to remove $dir"
			exit 1
		}
	}
	
	&cmd.exe /c rd /s /q $dir
	if(!$? -AND Test-Path $dir)
	{
		Logger "Failed to remove $dir"
		return $false
	}
	return $true
	#>
}

#
# Remove existing CS Entries
#
function DeleteFolders()
{
	$dir = "C:\ProgramData\ASR"
	
	# Remove ASR folder
	if(Test-Path $dir)
	{
		Logger "INFO : Directory $dir exists"
		cmd /c "rmdir $dir"
		if(!$?)
		{
			Logger "ERROR : Failed to delete $dir"
			exit 1
		}
		Logger "INFO : Successfully deleted $dir"
	}
	else
	{
		Logger "ERROR : $dir not found"
		exit 1
	}
	
	# Cleanup CS registration details
	$folders = @("C:\ProgramData\Microsoft Azure Site Recovery\certs",
						"C:\ProgramData\Microsoft Azure Site Recovery\fingerprints", "C:\ProgramData\Microsoft Azure Site Recovery\private")
	foreach ($dir in $folders)
	{
		if(Test-Path $dir)
		{
			Logger "INFO : Directory $dir exists"
			Remove-Item -Recurse -Force $dir
			if(!$?)
			{
				Logger "ERROR : Failed to delete $dir"
				exit 1
			}
			Logger "INFO : Successfully deleted $dir"
		}
	}
	Write-Output "Success"
}

#
# Modify conf params
#
function ModifyConf($conf, $key, $value)
{
	$path = "E:\Program Files (x86)\Microsoft Azure Site Recovery\home\svsystems\"
	$confFile = $path + $conf
	
	((Get-Content $confFile) | Foreach-Object {
		$line = $_
		if ($line -like "$key*") {
			$arr = $line -split "=", 2
			if ($arr[0].Trim() -eq $key.Trim()) {
				$arr[1] = $value
				$line = $arr -join "="
			}
		}
		$line
	})| Set-Content ($confFile)
}

#
# Update Config Files
#
function UpdateConfigFiles()
{
	Logger "INFO : Updating config files"
	$null_val = ""
	$value = ' ""'
	
	# Nullify Amethyst.conf values
	ModifyConf "etc\amethyst.conf" "DB_HOST " $value
	ModifyConf "etc\amethyst.conf" "HOST_GUID " $value
	ModifyConf "etc\amethyst.conf" "PS_IP " $value
	ModifyConf "etc\amethyst.conf" "PS_CS_IP " $value
	ModifyConf "etc\amethyst.conf" "PS_CS_PORT " $value
	
	# Nullify PushInstall.conf values
	ModifyConf "pushinstallsvc\pushinstaller.conf" "Hostid " $value
	ModifyConf "pushinstallsvc\pushinstaller.conf" "Port " $value
	ModifyConf "pushinstallsvc\pushinstaller.conf" "Hostname" $null_val
	
	# Nullify cspsconf values
	ModifyConf "transport\cxps.conf" "id " $null_val
	ModifyConf "transport\cxps.conf" "cs_ip_address " $null_val
	ModifyConf "transport\cxps.conf" "cs_ssl_port " $null_val
	
	Logger "INFO : Removed CS entries from all conf files"
}

#
# Move CSPSConfigTool to public desktop path
#
function MoveCSPSConfigTool
{
	Logger "INFO : Moving cspsconfigtool binary"
	
	$DesktopFile = [Environment]::GetFolderPath("Desktop") +"\cspsconfigtool.lnk"
	If(Test-path $DesktopFile)
	{
		Move-Item $DesktopFile "C:\Users\Public\Desktop"
		if (!$?)
		{
			Logger "ERROR : Failed to move cspsconfigtool.exe from desktop to Public Desktop path"
			Write-Output "Failure"
			exit 1
		}
	}
	
	If(Test-path "D:\Cspsconfigtool.lnk")
	{
		Move-Item "D:\Cspsconfigtool.lnk" "C:\Users\Public\Desktop"
		if (!$?)
		{
			Logger "ERROR : Failed to move cspsconfigtool.exe from D drive to Public Desktop path"
			Write-Output "Failure"
			exit 1
		}
	}
	Logger "INFO : Successfully moved cspsconfigtool binary"
}

#
# Run Sysprep on PS
#
function RunSysprep()
{
	Logger "INFO : Running Sysprep on ProcessServer"
	C:\Windows\System32\Sysprep\sysprep.exe /generalize /shutdown /oobe
	if (!$?)
	{
		Logger "ERROR : Failed to run sysprep on ProcessServer"
		Write-Output "Failure"
		exit 1
	}
	Logger "INFO : Set sysprep on ProcessServer"
	echo "Success"
}

function RegistryKeyValidation()
{
	$key = "hklm:SOFTWARE\Microsoft\Windows\CurrentVersion\RunOnce";
	$reginfo = Get-ItemProperty -Path $key;
	
	if(!$?)
	{
		Logger "Failed to access registry keys $key"
		Write-output "Failure"
		exit 1
	}
	
	$junctioncreation = $reginfo.junctioncreation;
	if(!$?)
	{
		Logger "Failed to access registry keys $key.$junctioncreation"
		Write-output "Failure"
		exit 1
	}
	
	$launchcspsconfigtool = $reginfo.launchcspsconfigtool;
	if(!$?)
	{
		Logger "Failed to access registry keys $key.$launchcspsconfigtool"
		Write-output "Failure"
		exit 1
	}
	
	if($junctioncreation -ne 'cmd /C mklink /J "C:\ProgramData\ASR" "E:\Program Files (x86)\Microsoft Azure Site Recovery"')
	{
		Logger "$junctioncreation key is incorrect"
		Write-output "Failure"
		exit 1
	}
	
	if($launchcspsconfigtool -ne 'E:\Program Files (x86)\Microsoft Azure Site Recovery\home\svsystems\bin\cspsconfigtool.exe')
	{
		Logger "$launchcspsconfigtool key is incorrect"
		Write-output "Failure"
		exit 1
	}
	
	Logger "Successfully validated registry keys"
}

#
# Main Function
#
function Main()
{
	SetExecutionPath
	StopServices
	UpdateRegistry
	#RegistryKeyValidation
	UpdateConfigFiles
	MoveCSPSConfigTool
	DeleteFolders
	#RunSysprep
}

Main