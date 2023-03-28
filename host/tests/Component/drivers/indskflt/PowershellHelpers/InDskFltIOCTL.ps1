# USAGE: PS C:\> . InDskFltIOCTL.ps1  Microsoft.Test.IOCTLs.dll

param (
    [Parameter(Mandatory=$true)]
    $Microsoft_Test_IOCTLs_dll_path
    )

$Microsoft_Test_IOCTLs_dll_path = (Get-Item $Microsoft_Test_IOCTLs_dll_path).FullName

$TellDllAssembly = [System.Reflection.Assembly]::LoadFile($Microsoft_Test_IOCTLs_dll_path)
if (-not $TellDllAssembly)
{
    throw "Failed to load the test dll : $Microsoft_Test_IOCTLs_dll_path"
}

Add-Type -AssemblyName $TellDllAssembly.FullName -ErrorAction Stop

$IoctlController = New-Object Microsoft.Test.IOCTLs.IoctlController
if (-not $IoctlController)
{
    throw "Failed to create the Ioctl controller object"
}

<#
Add-Type -WarningAction SilentlyContinue @"
//using Mti = Microsoft.Test.IOCTLs;
using IoctlCodes = Microsoft.Test.IOCTLs.IoControlCodes;
using IoctlStructs = Microsoft.Test.IOCTLs.IOCTLStructures;
"@ -ReferencedAssemblies $TellDllAssembly
#>

<#
$SendIoctl = $IoctlController.GetType().GetMethods() | ? { $_.Name -eq "SendIOCTL" }

function script:Send-Ioctl
{
param(
    [Parameter(Mandatory=$true)]
    [uint32]$IoctlCode,
    [System.Object]$Input,
    [ref][System.Object]$Output
    )

    $OutputType = $InputType = [type]"Object"
    
    if ($Input) { $InputType = $Input.GetType() }
    if ($Output) { $OutputType = $Output.GetType() }

    $GenMethod = $SendIoctl.MakeGenericMethod($InputType, $OutputType)

    [object[]]$Arguments = @($IoctlCode, $Input, $Output)

    $GenMethod.Invoke($IoctlController, $Arguments)

    $Output = $Arguments[2]
}
#>

function Get-InDskFltDriverVersion
{
    $DrvVersion = New-Object Microsoft.Test.IOCTLs.IOCTLStructures+DRIVER_VERSION

    $IoctlController.SendIoctl([Microsoft.Test.IOCTLs.IoControlCodes]::IOCTL_INMAGE_GET_VERSION, $null, [ref]$DrvVersion)
    if (-not $?)
    {
        throw "Failed to get the driver version"
    }

    return $DrvVersion
}

function Notify-InDskFltSystemState
{
param(
    [Parameter(Mandatory=$true)]
    [bool]$AreBitmapFilesEncryptedOnDisk
    )

    $in = New-Object Microsoft.Test.IOCTLs.IOCTLStructures+NOTIFY_SYSTEM_STATE_INPUT

    $in.Flags = 0
    if ($AreBitmapFilesEncryptedOnDisk) {
        $in.Flags = $in.Flags -bor [Microsoft.Test.IOCTLs.IOCTLStructures+NOTIFY_SYSTEM_STATE_FLAGS]::ssFlagsAreBitmapFilesEncryptedOnDisk;
    }

    $out = New-Object Microsoft.Test.IOCTLs.IOCTLStructures+NOTIFY_SYSTEM_STATE_OUTPUT

    $IoctlController.SendIoctl([Microsoft.Test.IOCTLs.IoControlCodes]::IOCTL_INMAGE_NOTIFY_SYSTEM_STATE, $in, [ref]$out)
    if (-not $?)
    {
        throw "Failed to notify the system state"
    }

    return $out
}