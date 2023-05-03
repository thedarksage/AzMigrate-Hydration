Param
(
    [parameter(Mandatory=$true)]
    [String]
    $UserMSI,

    [parameter(Mandatory=$true)]
    [String]
    $DIToolPath,

    [parameter(Mandatory=$true)]
    [String]
    $Container                                      
)

# Churn(Load) Generation
# This tool generates a 4MB file each second and this is configurable by changing parameters in AutoFRLoadGen.xml
function Start-Churn ()
{
    $Time = (Get-Date -Format "dd_MM_yyyy_HH_mm_ss_tt")
    $output = [string] (& "sc.exe" @( "create", "Frload", "binpath=$DIToolPath\DITestBinRoot\FRGuestTestDir\FRLoadGenServer.exe") 2>&1)
    write-output "$Time - $output" | Out-File $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append
    net start Frload

    $output = [string] (& "$DIToolPath\DITestBinRoot\FRGuestTestDir\FRLoadGenClient.exe" @( "loadgenstart", "$DIToolPath\DITestBinRoot\FRGuestTestDir\AutoFRLoadGen.xml") 2>&1)
    write-output "$Time - $output" | Out-File $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append

    Start-Sleep 300
}

# Getting Data Disk for Data-Integrity
function Getting-DataDiskId ()
{
    $Time = (Get-Date -Format "dd_MM_yyyy_HH_mm_ss_tt")
    $Disk = Get-WmiObject -Class Win32_DiskDrive | Where-Object { $_.Size -le "5368709120" }
    if ($Disk.Count -eq 0)
    {
        Write-output "$Time - Error: DiskId not found"  | Out-File $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append
        return
    }
    else
    {
        $global:DiskId = $Disk.Signature
        $DiskId | Out-File $DIToolPath\DITestBinRoot\DITest\DiskId.txt
        write-output "$Time - DiskId = $DiskId" | Out-File $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append
    }

    Start-Sleep 10
}

# Issue vacp marker
function Issue-Vacp-Marker ()
{
    $Time = (Get-Date -Format "dd_MM_yyyy_HH_mm_ss_tt")
    $Error.Clear();
    $output = [string] (& "$DIToolPath\DITestBinRoot\DITestUtil\DITestUtil.exe" @( "-d", "$DIToolPath\DITestBinRoot\DITest", "-cc", "-disks", $DiskId) 2>&1)
    if ($Error[0].FullyQualifiedErrorId -eq "NativeCommandError" )
    {
        Write-Output "$Time - DITestUtil.exe Failed with: $output" | out-file $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append      
        return
    }
    else
    {
        write-output "$Time - $output" | Out-File $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append
    }

    Start-Sleep 300
}

# Stop churn
function Stop-Churn ()
{
    $Time = (Get-Date -Format "dd_MM_yyyy_HH_mm_ss_tt")
    net stop Frload
    $output = [string] (& "sc.exe" @( "delete", "Frload") 2>&1)
    write-output "$Time - $output" | Out-File $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append
          
    Start-Sleep 10
}

# Generating checksum for DataDisk
function Getting-Checksum ()
{
    $Time = (Get-Date -Format "dd_MM_yyyy_HH_mm_ss_tt")
    $Error.Clear();
    $output = [string] (& "$DIToolPath\DITestBinRoot\DITestUtil\DITestUtil.exe" @( "-d", "$DIToolPath\DITestBinRoot\DITest", "-md5sum", "-disks", $DiskId, "-b", 4194304, "-t") 2>&1)
    if ($Error[0].FullyQualifiedErrorId -eq "NativeCommandError" )
    {
        Write-Output "$Time - DITestUtil.exe Failed with: $output" | out-file $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append      
        return
    }
    else
    {
        write-output "$Time - $output" | Out-File $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append
    }
    
    Start-Sleep 120
}

# Get VmId from drscout.conf and redirect the output to DITest Folder
function Get-Sourceinfo()
{
   "DiskId=$DiskId" | Out-File $DIToolPath\DITestBinRoot\DITest\SourceVmInfo.txt -Append
    
    $Audience = Get-Content -Path "C:\ProgramData\Microsoft Azure Site Recovery\Config\RCMInfo.conf" | Where-Object { $_ -match 'AADAudienceUri'}
    $resource_container = $Audience.Split('/')

    if ($resource_container[0] -match "https:")
    {
        $resource = $resource_container[4]
        $container = $resource_container[5]
    }

    if ($resource_container[0] -match "api:")
    {
        $resource = $resource_container[5]
        $container = $resource_container[6]
    }
    
    "ResourceId=$resource" | Out-File $DIToolPath\DITestBinRoot\DITest\SourceVmInfo.txt -Append
    "ContainerId=$container" | Out-File $DIToolPath\DITestBinRoot\DITest\SourceVmInfo.txt -Append
    
    $VmId = Get-Content -Path "C:\Program Files (x86)\Microsoft Azure Site Recovery\agent\Application Data\etc\drscout.conf" | Where-Object { $_ -match 'HostId'}
    $VmId | Out-File $DIToolPath\DITestBinRoot\DITest\SourceVmInfo.txt -Append

    $MarkerId = Get-Content -Path "$DIToolPath\DITestBinRoot\DITest\cc_marker_id.txt"
    "MarkerId=$MarkerId" | Out-File $DIToolPath\DITestBinRoot\DITest\SourceVmInfo.txt -Append
}

# Copy the checksum and Marker files into StorageBlob for validation
function Upload-File()
{
    $Time = (Get-Date -Format "dd_MM_yyyy_HH_mm_ss_tt")
    $Command = "$DIToolPath\DITestBinRoot\azcopy.exe"
    $Parms = "login --identity --identity-client-id $UserMSI"
    $Prms = $Parms.Split(" ")
    $output = & "$Command" $Prms
    Write-Output $output | out-file $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append

    $Src = "$DIToolPath\DITestBinRoot\DITest"
    $Dest = "https://a2agqldi.blob.core.windows.net/$Container"
    Write-Output $Dest | out-file $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append
    
    $Parms = "copy $Src $Dest --recursive"
    $Prms = $Parms.Split(" ")
    $output = & "$Command" $Prms
    if ($?)
    {
        Write-Output "$Time - uploading files to storage container succeeded: $output" | out-file $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append
        return
    }
    else
    {
        Write-Output "$Time - uploading files to storage container failed: $output" | out-file $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append 
    }
}

#online the Disk
function Online-Disk ()
{
    $Time = (Get-Date -Format "dd_MM_yyyy_HH_mm_ss_tt")
    $Error.Clear();
    $output = [string] (& "$DIToolPath\DITestBinRoot\DITestUtil\DITestUtil.exe" @( "-d", "$DIToolPath\DITestBinRoot\DITest", "-online", "-disks", $DiskId) 2>&1)
    if ($Error[0].FullyQualifiedErrorId -eq "NativeCommandError" )
    {
        Write-Output "$Time - DITestUtil.exe Failed with: $output" | out-file $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append      
        return
    }
    else
    {
        write-output $output | Out-File $DIToolPath\DITestBinRoot\DITest\SourceVmCommand.txt -Append
    }
}

function Main ( )
{
    Start-Churn
    Getting-DataDiskId
    Stop-Churn
    # DI tool offlines the data disk before creating marker.
    Issue-Vacp-Marker
    Getting-Checksum
    Get-Sourceinfo
    Upload-File
    Online-Disk
}

Main