param(
    [Parameter(Mandatory=$True)]
    [string] $OSName,
    [Parameter(Mandatory=$True)]
    [string] $RequiredKernel,
    [Parameter(Mandatory=$True)]
    [string] $RootPwd
)

. $PSScriptRoot\CommonFunctions.ps1
. $PSScriptRoot\OS.ps1

$Error.Clear()

Write-Host "$global:LogDir"
$global:LogName = "$global:LogDir\UpdateKernel.log"

$offer = $distros[$OSName][2]
if($offer.ToLower().Contains('ubuntu'))
{
    $Distro = "UBUNTU"
}
elseif($offer.ToLower().StartsWith('sles'))
{
    $Distro = "SLES"
}
elseif($offer.ToLower().StartsWith('debian'))
{
    $Distro = "DEBIAN"
}
else
{
    Write-Host "Found $OSName. Skipping InstallKernel operation!!"
    exit 0
}

LogMessage -Message ("rgName : {0}, offer : {1}, vmName : {2}, OSName : {3}" -f $global:PriResourceGroup, $offer, $global:VMName, $OSName) -LogType ([LogType]::Info1)

Function RunCommand
{
    param (
        [parameter(Mandatory=$True)][PSObject] $Params
    )

    $result = Invoke-AzVMRunCommand -ResourceGroupName $PriResourceGroup -Name $VMName -CommandId 'RunShellScript' -ScriptPath 'InstallKernel.sh' -Parameter $Params

    LogObject $result
    LogObject $result.Value
    If ($result.Status -ne "Succeeded" -or ($result.Value[0].Message.Contains("Failed")))
    {
        LogMessage -Message ("Issue encountered while validating Kernel Version: $result.Value[0].Message") -LogType ([LogType]::Error)
        Throw "Issue encountered while validating Kernel Version: $result.Value[0].Message"
    }

    return $result
}

Function InstallKernel
{
    LogMessage -Message ("Installing Kernel $RequiredKernel on $OSName : $Distro") -LogType ([LogType]::Info1)
    
    $result = RunCommand -Params @{"Distro" = $Distro; "RequiredKernel" = $RequiredKernel; "RootPassword" = $RootPwd; "Validate" = "false"}
    
    LogMessage -Message ("Installation succeded. $result.Value[0].Message") -LogType ([LogType]::Info1)

    LogObject $result.Value[0]
    if ($result.Value[0].Message.Contains("Already Updated to Required Kernel Version"))
    {
        Write-Host "Successfully updated the Kernel"
        exit 0
    }
}

Function RebootVM
{
    LogMessage -Message ("Rebooting {0}" -f $global:VMName) -LogType ([LogType]::Info1) 
    Restart-AzVM -ResourceGroupName $global:PriResourceGroup -Name $global:VMName
    if ( !$? )
    {
        $ErrorMessage = ('Failed to restart VM : {0} under RG {1}' -f $global:VMName, $global:PriResourceGroup)
        LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
        Throw $ErrorMessage
    }
    LogMessage -Message ("Successfully Rebooted VM") -LogType ([LogType]::Info1)
}

Function ValidateKernel
{
    LogMessage -Message ("Validating Kernel $RequiredKernel on $global:VMName") -LogType ([LogType]::Info1)
    
    $result = RunCommand -Params @{"Distro" = $Distro; "RequiredKernel" = $RequiredKernel; "RootPassword" = $RootPwd; "Validate" = "true"}

    LogMessage -Message ("Kernel Validation succededed. $result.Value[0].Message") -LogType ([LogType]::Info1)
}

try {
    Write-Host "Attempting to update kernel"
    InstallKernel

    RebootVM

    ValidateKernel

    Write-Host "Successfully updated the Kernel"

    exit 0
}
catch {
    Write-Error "Failed to update the kernel version on $global:VMName"
    Write-Error "ERROR Message:  $_.Exception.Message"
    Write-Error "ERROR:: $Error | ConvertTo-json -Depth 1"
    Throw
}
