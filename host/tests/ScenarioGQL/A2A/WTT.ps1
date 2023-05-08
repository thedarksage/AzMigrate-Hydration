param(
    [string] $global:BranchRunPrefix = $args[0],
	[string] $EnableMailReporting = $args[1],
	[string] $global:OSName = $args[2],
	[string] $OSType = $args[3],
	[string] $PrimaryLocation = $args[4],
	[string] $RecoveryLocation = $args[5],
	[string] $SubscriptionId = $args[6],
	[string] $SubscriptionName = $args[7],
	[string] $VaultRGLocation = $args[8],
	[string] $LabAccntUserName = $args[9],
	[string] $LabAccntPassword = $args[10]
)	

# Enable mulitenant support when running through this script
$global:MT = $true

$host.ui.RawUI.WindowTitle = $OSName
$TestBinRoot = $PSScriptRoot
Write-Host "Test: $TestBinRoot"

$global:configFile = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLConfig.xml"

Function CopyLogFileExit()
{
    #$LogFile = $Script "LOGNAME
	exit 1
}


Function UpdateConfigFile()
{
    $Script = Join-Path -Path $PSScriptRoot -ChildPath "UpdateConfig.ps1"

    .$Script -TestBinRoot $TestBinRoot -SubscriptionName "$SubscriptionName" -SubscriptionId $SubscriptionId -PrimaryLocation $PrimaryLocation -RecoveryLocation $RecoveryLocation -VaultRGLocation $VaultRGLocation -OSType $OSType -BranchRunPrefix $BranchRunPrefix -OSName $OSName -EnableMailReporting $EnableMailReporting
    if (!$?) {
        CopyLogFileExit
    }
}

Function Login()
{
    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    Write-Host "Test: $PSScriptRoot"

    .$Script "LOGIN"
    if (!$?) {
        CopyLogFileExit
    }
}

Function DisableCleanAzureResources()
{
    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    .$Script "CLEANPREVIOUSRUN"
}

Function CreateVM() {
    param (
        [Parameter(Mandatory=$true)][string] $SubscriptionId,
        [Parameter(Mandatory=$true)][string] $PrimaryLocation,
        [Parameter(Mandatory=$true)][string] $BranchRunPrefix,
        [Parameter(Mandatory=$true)][string] $OSName
    )

    $Script = Join-Path -Path $PSScriptRoot -ChildPath "CreateNewVm.ps1"

    #TODO: Account.eaf

    .$Script -SubId $SubscriptionId -Location $PrimaryLocation -VmUserName $LabAccntUserName -VmPassword $LabAccntPassword -BranchRunPrefix $BranchRunPrefix -OSName $OSName
    if (!$?) {
        CopyLogFileExit
    }
}

Function EnableReplication()
{
    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    .$Script "ER"
    if (!$?) {
        CopyLogFileExit
    }
}

Function IssueCrashConsistencyTag()
{
    param (
        [Parameter(Mandatory=$true)][string] $ResourceGroupType
    )

    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    .$Script "ISSUETAG" $ResourceGroupType
}

Function TestFailover()
{
    param (
        [Parameter(Mandatory=$true)][string] $ResourceGroupType
    )

    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    .$Script "TFO" $ResourceGroupType
    if (!$?) {
        CopyLogFileExit
    }
}

Function Reboot()
{
    param (
        [Parameter(Mandatory=$true)][string] $ResourceGroupType
    )

    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    .$Script "REBOOT" $ResourceGroupType
    if (!$?) {
        CopyLogFileExit
    }
}

Function UnplannedFailover()
{
    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    .$Script "UFO"
    if (!$?) {
        CopyLogFileExit
    }
}

Function SwitchProtection()
{
    param (
        [Parameter(Mandatory=$true)][string] $ResourceGroupType
    )

    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    .$Script "SP" $ResourceGroupType
    if (!$?) {
        CopyLogFileExit
    }
}

Function FailBack()
{
    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    .$Script "FB"
    if (!$?) {
        CopyLogFileExit
    }
}

Function ReverseSwitchProtection()
{
    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    .$Script "RSP"
    if (!$?) {
        CopyLogFileExit
    }
}

Function DisableReplication()
{
    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    .$Script "DR"
    if (!$?) {
        CopyLogFileExit
    }
}

Function CleanupResources()
{
    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    .$Script "CR"
    if (!$?) {
        CopyLogFileExit
    }
}

Write-Host "UpdateConfigFile"
UpdateConfigFile
Write-Host "Login"
Login 
Write-Host "DisableCleanAzureResources"
DisableCleanAzureResources
Write-Host "CreateVM"
CreateVM $SubscriptionId $PrimaryLocation $BranchRunPrefix $OSName
Write-Host "EnableReplication"
EnableReplication
Write-Host "IssueCrashConsistencyTag"
IssueCrashConsistencyTag "Primary"
Write-Host "TestFailover"
TestFailover "Recovery"
Write-Host "Reboot"
Reboot "Primary"
Write-Host "IssueCrashConsistencyTag"
IssueCrashConsistencyTag "Primary"
Write-Host "UnplannedFailover"
UnplannedFailover
Write-Host "SwitchProtection"
SwitchProtection "Recovery"
Write-Host "IssueCrashConsistencyTag"
IssueCrashConsistencyTag "Recovery"
Write-Host "FailBack"
FailBack
Write-Host "ReverseSwitchProtection"
ReverseSwitchProtection 
Write-Host "DisableReplication"
DisableReplication
Write-Host "CleanupResources" 
CleanupResources
Write-Host "Done"
