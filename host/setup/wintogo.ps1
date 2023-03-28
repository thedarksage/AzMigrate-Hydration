#
# InMage WinToGO USB Creator Ver 1.03
# Date: Oct 30,2012
#
#
# Change Log
# 1.00 - First Version
# 1.01 - Add Support to Windows 8 Media ISO 
#        Auto detect media (Windows 2012 vs Window 8)  
#        Now works on both 32/64 bit (earlier vesion only supported 64bit)
# 1.02 - Resolve win7 32bit incompatibility issue
#        Windows 8 Support
#        Any new disk, which initialized with MBR
# 1.03  Corrected typo, which broke win7


#*=============================================================================
#* Function: ExitNow
#* Arguments:
#*=============================================================================
#* Purpose: Exit the program
#*=============================================================================
function ExitNow()
{
  exit
}


#*=============================================================================
#* Function: Drives
#* Arguments:  
#*=============================================================================
#* Purpose: Gets USB Drives Connected to System (more than 30G)
#*=============================================================================
function Drives()
{
   $drives = gwmi -query "Select DeviceID,Caption,Manufacturer,Model,serialnumber,Index,partitions,Size from Win32_DiskDrive where (interfacetype='USB' OR partitions=0) AND size > 32212254720"
   $drives
   return
}

#*=============================================================================
#* Function: PrintDisk
#* Arguments:
#*=============================================================================
#* Purpose: Print the disks for selection
#*=============================================================================
function PrintDisk()
{
 param($drives)
 $drives | sort-object -property index | format-table -property Index,DeviceID,Caption,Size -AutoSize
}


#*=============================================================================
#* Function:StartUSBProcess
#* Arguments:
#*=============================================================================
#* Purpose: Create USB Execution Process
#*=============================================================================
function StartUSBProcess()
{
  CreateUSB
}


#*=============================================================================
#* Function: isWAIKInstalled
#* Arguments:
#*=============================================================================
#* Purpose: Check if Windows AIK Kit is installed
#*=============================================================================
function isWAIKInstalled()
{
    $result = 0
    if($global:os -eq 8)
    {
      $global:waikinstallpath= "$env:programfiles (x86)\Windows Kits\8.0\Assessment and Deployment Kit\Deployment Tools\$arch"

      If((Test-Path("$global:waikinstallpath\DISM\imagex.exe")) -and (Test-Path("$global:waikinstallpath\BCDBoot\bcdboot.exe")))
      {
         $result =1
      }
      else{
        $global:waikinstallpath= "$env:programfiles\Windows Kits\8.0\Assessment and Deployment Kit\Deployment Tools\$arch"
        If((Test-Path("$global:waikinstallpath\DISM\imagex.exe")) -and (Test-Path("$global:waikinstallpath\BCDBoot\bcdboot.exe")))
        {
             $result =1
        }
      }
    }
    elseif($global:os -eq 7)
    {
       If((Test-Path("$global:waikinstallpath\imagex.exe")) -and (Test-Path("$global:waikinstallpath\bcdboot.exe")))
	    {
 	     $result =1
	    }
    }
	
	$result
    Return 
}

#*=============================================================================
#* Function: isCurrentOSOk
#* Arguments:
#*=============================================================================
#* Purpose: Check if current desktop OS is Windows 7 64bit
#*=============================================================================
 
function isCurrentOSOk()
{
   $result = 0
  
   #if([Environment]::OSVersion.Version.Minor -eq 1 -and [Environment]::OSVersion.Version.Major -eq 6 -and (gwmi -class Win32_OperatingSystem).OSArchitecture -eq '64-bit'){
   if(([Environment]::OSVersion.Version.Minor) -eq 1 -and ([Environment]::OSVersion.Version.Major -eq 6)){
        $result = 1
        $global:os=7
   } elseif(([Environment]::OSVersion.Version.Minor) -eq 2 -and ([Environment]::OSVersion.Version.Major -eq 6))
   {
        $result = 1
        $global:os=8
   }


   $result
   Return
}


#*=============================================================================
#* Function: isUSBConnected
#* Arguments: List of USB Drives greater 30GB
#*=============================================================================
#* Purpose: Checks if USB Drivers more than 30GB are connected
#*=============================================================================
function isUSBConnected()
{
  param($drives)
  $result = 0

  #Get USB Drives In the System 
  $myc = $drives | measure
  if($myc.Count -gt 0){
    $result = 1
  }
  $result
  return
}



#*=============================================================================
#* Function: isOSInstallationAvailable
#* Arguments: Installation Path of the Windows 2012 or Windows 8
#*=============================================================================
#* Purpose: Checks if the supplied path is correct OS installer path
#*=============================================================================
function isOSInstallationAvailable([String]$osinstallerpath)
{
	$result = 0

	$installwimpath = $OSInstallerPath + "\sources\install.wim"
	
	If(Test-Path($installwimpath))
	{
	  $result = 1
	}
	$result
	return
}




#*=============================================================================
#* Function: Check Required Pre-requisites
#* Arguments:
#*=============================================================================
#* Purpose:  Check Required Pre-Requisites
#*=============================================================================
function checkPrerequisites([System.Array]$drives, [String]$osinstallerpath)
{
 #  param(
  # )
  #param($osinstallerpath)
  
  $allOk = 1
  Write-Host "Verifying Prerequisites..."
  Write-Host "--------------------------"
  
  
  if(isCurrentOSOk -eq 1)
  {
    Write-Host("1. Desktop OS Windows $global:os, Ok!");
  }
  else
  {
    $allOk = 0
	Write-Host("1. Desktop OS Windows 7 or Windows 8, Not Ok!");
  }
  
  if(isWAIKInstalled -eq 1)
  {
    if($global:os -eq 7){
        Write-Host("2. Windows Automated Installation Kit (AIK) found, Ok!");
    }
    elseif($global:os -eq 8)
    {
       Write-Host("2. Windows Assessment and Deployment Kit (ADK) found, Ok!");
    }
  }
  else
  {
    $allOk = 0
    
    if($global:os -eq 7){
        Write-Host("2. Windows Automated Installation Kit Not found, Not Ok!");
        Write-Host("Download the installation kit from : http://www.microsoft.com/en-us/download/details.aspx?id=5753");
    }
    elseif($global:os -eq 8)
    {
        Write-Host("2. Windows Assessment and Deployment Kit (ADK) Not found, Not Ok!");
        Write-Host("Download the installation kit from : http://www.microsoft.com/en-us/download/details.aspx?id=30652");
    }

  }

 if((isUSBConnected $drives) -eq 1)
 {
   Write-Host("3. USB Hard Disk is connected, Ok!");
 }
 else
 {
   $allOk = 0
   Write-Host("3. USB Hard Disk is not available, Not Ok!");
   Write-Host("Connected external USB harddrive of atleast capacity 30GB");
 }
 
 if((isOSInstallationAvailable $osinstallerpath) -eq 1)
 {
   Write-Host("4. Window 2012 or 8 installer available, Ok!");
 }
 else
 {
   $allOk = 0
   Write-Host("4. Window 2012 or 8 installer not available, Not Ok!");
   Write-Host("Please provide full path to Windows 2012 or 8 installer path");
 }
 $allOk
 Return
}





#*=============================================================================
#* Function: Create USB Execution Process
#* Arguments:
#*=============================================================================
#* Purpose: Create USB Execution Process
#*=============================================================================
function CreateUSB()
{
  Write-Host "Starting USB Creation Process..."
}


#*=============================================================================
#* Function: PrepareDrive
#* Arguments:List of USB 30GB USB Drives
#*=============================================================================
#* Purpose: Prepares USB Drive Selected by user
#*=============================================================================
function PrepareDrive([String] $dl)
{
  
  Write-Host
  Write-Host "Disk Selection"
  Write-Host "--------------"
  
  $selectediskIndex = 0
  $selectedDeviceId = 0

  	$myc = $drives | measure
	PrintDisk $drives
	
	if($myc.Count -eq 1){
		foreach ($objItem in $drives) {
			$selectediskIndex = $objItem.Index
			$selectedDeviceId = $objItem.DeviceID
			break
		}
	}else
	{
	    Write-Host
		do{
			$selecteddiskIndex = Read-Host "Select USB Disk Index # "
			foreach ($objItem in $drives) {
				$selectediskIndex1 = $objItem.Index
				if($selectediskIndex1 -eq $selecteddiskIndex)
				{
				   $selectedDeviceId = $objItem.DeviceID
				   break
				}
		   }
		}
		until($selectedDeviceId -ne 0)
	}
	
	Write-Host
	$confirm = Read-Host "Disk" $selectedDeviceId "is selected,please confirm Y/N "
    if($confirm.ToUpper() -ne "Y")
	{
	  Write-Host "Aborted USB creation process"
	  ExitNow
	}
	else
	{
	  $diskid = $selectedDeviceId.Substring($selectedDeviceId.Length - 1)
	 
	  
	  $command= @"
	  select disk $diskid
	  clean
	  create partition primary size=30720
	  select partition 1
	  format fs=ntfs label="WinToGo" quick
	  active
	  assign LETTER=$dl
"@
	 
	  Write-Host
	  Write-Host "Preparing USB Drive"
	  Write-Host "-------------------"
	  $command | diskpart
	  Write-Host "USB boot volume is assigned letter: $dl"
  	}
}


#*=============================================================================
#* Function: InstallWinToGo
#* Arguments:USB Boot Volume Drive Letter
#*=============================================================================
#* Purpose: Install Windows 2012/8 to USB Harddrive
#*=============================================================================
function InstallWinToGo([String]$bootVol)
{
  Write-Host
  Write-Host "WinToGo Installation..."
  Write-Host "-----------------------"
  Write-Host "Installation Process will take 15-20 minutes, please don't abort the process"
  
  $tempPath = "c:\Wintogo64"
  
  If (!(Test-Path -path $tempPath))
  {
	 New-Item -path $tempPath -type directory
  }
  
 #Copy the Required Files to temp location
   Write-Host "Copying files required for installation,It might take few minutes (2-5 minutes)"
  
   if($os -eq 7)
   {
     Copy-Item $global:waikinstallpath\imagex.exe -Destination $tempPath  
     Copy-Item $global:waikinstallpath\bcdboot.exe -Destination $tempPath  
   }
   elseif($os -eq 8) {
     Copy-Item $global:waikinstallpath\DISM\imagex.exe -Destination $tempPath  
     Copy-Item $global:waikinstallpath\BCDBooT\bcdboot.exe -Destination $tempPath  
   }
     Copy-Item $osinstallerpath\sources\install.wim -Destination $tempPath 
  
  
  #Installation
  Write-Host
  Write-Host "This process will take 10-15 minutes, don't abort"

  #Determining right Image to be used from install media for Windows 8 (i.e. 1) and Windows 2012 (i.e. 2)
  Write-Host "Determing the right OS image to install"
  [String] $info =  &"$temppath\imagex.exe" /INFO "$temppath\install.wim" 
  
  $startStr_1 = "Image Count:"
  $endStr_1 = "Compression:"
  $startStr_2 = "<NAME>"
  $endStr_2 = "</NAME>"
  
  $idx1 = $info.IndexOf($startStr_1,0)+$startStr_1.Length
  $idx2 = $info.IndexOf($endStr_1,$idx1)
  $imageidx = 0
  $OSStr = ""
  if (($idx1 -ge 0) -and ($idx2 -ge 0))
  {
	$imageidx = $info.Substring($idx1,($idx2-$idx1)).Trim()
	if($imageidx -gt 1)
	{
	  $imageidx = 2
	}
	Write-Host "Selected OS Image Index : "  $imageidx
	#Getting The Operating System Image
	$idx1 = 0
	for($i=0; $i -lt $imageidx; $i++)
	{
	 $idx1 = $info.IndexOf($startStr_2,$idx1)+$startStr_2.Length
	}
	$idx2 = $info.IndexOf($endStr_2,$idx1)

	if (($idx1 -ge 0) -and ($idx2 -ge 0))
	{
 	 $OSStr = $info.Substring($idx1,($idx2-$idx1)).Trim()
	 Write-Host "Operating System Image Selected :" $OSStr
	}
  }
  else
  {
	Write-Host "Not a valid OS install.wim, use a new install media"
	ExitNow
  }
   
  
  #Installing Windows Files to the USB
  Write-Host "Installing Windows on the USB Drive"
  $driveletter = $bootVol+":"
  $command= "$tempPath\imagex.exe /APPLY $tempPath\install.wim $imageidx " + $driveletter 
  $command | CMD
  
  #Making the USB Drive bootable 
  Write-Host "Making the USB Drive bootable"
  $command = "$tempPath\bcdboot.exe $driveletter\windows /s " + $driveletter 
  $command | CMD
  
  
  #Cleanup temporary installation directory
  Write-Host "Cleaning up temporary installation directories"
  If ((Test-Path -path $tempPath))
  {
     Remove-Item -Path $tempPath -Recurse -Force 
  }
}


#*=============================================================================
#* Function: UnusedDrive
#* Arguments:
#*=============================================================================
#* Purpose: Returns Unused Drive Letter
#*=============================================================================
function UnusedDrive()
{
	for($j=67;gdr([char]$j)-ea 0){$j++};
	[char]$j
	Return 
}

#*=============================================================================
#*								 MAIN EXECUTION
#*=============================================================================

cls
Write-Host "#=============================================|"
Write-Host "|     InMage WinToGo USB Creator Ver 1.01     |"
Write-Host "#=============================================|"
 

Write-Host
Write-Host "Note:This process will take 15-20 minutes, don't abort in between"
Write-Host


#Get Drives
[wmi] $global:drives = Drives
[string] $global:os =0
[string] $global:arch =""

#Take all required Inputs from the user
Write-Host "Please Specify, the following"
Write-Host "-----------------------------"

$osinstallerpath = Read-Host "Installer path(extract iso or mount iso) for Windows 2012 or Windows 8 "
#$osinstallerpath = 'C:\WinToGo\win2012install'
if((gwmi -class win32_processor).Manufacturer.toUpper().indexOf("INTEL") -ge 0)
{
	$arch = "x86"
}
else
{
  $arch = "amd"
}
$global:waikinstallpath = "$env:programfiles\Windows AIK\Tools\$arch"
$global:imageidx=0
$global:installOSName=""
 
Write-Host

#Do Required Pre-Requisite Checks
if((checkPrerequisites $drives $osinstallerpath) -eq 1)
{
	$bootvol= UnusedDrive
	try{
    PrepareDrive $bootvol
	InstallWinToGo $bootVol
    Write-Host "WinToGo Sucessfully installed, you can unplug USB and boot through USB Now, -ThankQ"
    } catch [System.Exception]
    {
       ExitNow
    }
		 
}
else{
 ExitNow
}