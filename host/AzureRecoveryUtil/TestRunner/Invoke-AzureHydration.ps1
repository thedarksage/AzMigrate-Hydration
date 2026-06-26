#Requires -Version 7.2
#Requires -Modules Az.Accounts, Az.Compute, Az.Storage

<#
.SYNOPSIS
    End-to-end Azure Migrate / Site Recovery hydration test runner (Az module).

.DESCRIPTION
    Modern replacement for the legacy classic-ASM TestRunners.ps1. Drives the
    AzureRecoveryUtil hydration tool against real, customer-supplied managed
    disks:

        1. Create the hydration VM in a dedicated resource group and attach the
           source data disks at deterministic LUNs.
        2. DYNAMICALLY generate the recovery-info (.conf) file from the live VM
           (no hand-typed disk GUIDs) via Get-HydrationDiskLunMap +
           New-AzureRecoveryInfoFile.
        3. Upload AzureRecoveryTools.zip + the recovery-info (+ optional host
           info) and run them through the Custom Script Extension, which invokes
           AzureRecoveryUtil with the requested operation.
        4. Wait for completion via the status blob, then optionally build the
           recovered VM and tear down the hydration resources.

    All configuration is supplied through a single -ConfigPath .psd1 data file;
    see HydrationConfig.sample.psd1.

.PARAMETER ConfigPath
    Path to a PowerShell data file (.psd1) describing the run. See the sample.

.PARAMETER Operation
    AzureRecoveryUtil operation to run: recovery, migration or genconversion.

.PARAMETER SkipRecoveredVm
    Stop after hydration completes; do not build the recovered VM.

.PARAMETER KeepResources
    Do not delete the hydration resource group / artefacts on completion.

.EXAMPLE
    ./Invoke-AzureHydration.ps1 -ConfigPath ./my-run.psd1 -Operation migration

.NOTES
    Requires an authenticated Az context (Connect-AzAccount) with rights to the
    target subscription, resource group and storage account.
#>
[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$ConfigPath,

    [ValidateSet('recovery', 'migration', 'genconversion')]
    [string]$Operation = 'migration',

    [switch]$SkipRecoveredVm,

    [switch]$KeepResources
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'AzMigrate.Hydration.psd1') -Force

function Resolve-StorageContext {
    param([string]$ResourceGroupName, [string]$StorageAccountName)
    $account = Get-AzStorageAccount -ResourceGroupName $ResourceGroupName -Name $StorageAccountName
    return $account.Context
}

# ---------------------------------------------------------------------------
# Load and validate configuration
# ---------------------------------------------------------------------------
$cfg = Import-PowerShellDataFile -LiteralPath $ConfigPath
foreach ($required in 'SubscriptionId', 'ResourceGroupName', 'Location', 'StorageAccountName', 'ContainerName', 'VmName', 'OsType', 'DataDisks', 'ArtifactsPath') {
    if (-not $cfg.ContainsKey($required)) {
        throw "Configuration '$ConfigPath' is missing required key '$required'."
    }
}

Write-Information "Selecting subscription $($cfg.SubscriptionId)." -InformationAction Continue
Set-AzContext -Subscription $cfg.SubscriptionId | Out-Null

# Linux hydration only supports recovery / migration (see StartupScript.sh).
if ($cfg.OsType -eq 'Linux' -and $Operation -eq 'genconversion') {
    throw "Operation 'genconversion' is not supported for Linux hydration; use 'recovery' or 'migration'."
}

$ctx = Resolve-StorageContext -ResourceGroupName $cfg.ResourceGroupName -StorageAccountName $cfg.StorageAccountName
$hostId = if ($cfg.ContainsKey('HostId') -and $cfg.HostId) { $cfg.HostId } else { [guid]::NewGuid().ToString() }
$vmSize = if ($cfg.ContainsKey('VmSize') -and $cfg.VmSize) { $cfg.VmSize } else { 'Standard_D2s_v5' }

# ---------------------------------------------------------------------------
# 1. Create the hydration VM and attach the source disks
# ---------------------------------------------------------------------------
function New-HydrationVm {
    [Diagnostics.CodeAnalysis.SuppressMessageAttribute(
        'PSAvoidUsingConvertToSecureStringWithPlainText', '',
        Justification = 'Test harness: the admin password is an operator-supplied secret, sourced from the HYDRATION_ADMIN_PASSWORD environment variable (preferred) or the config file, and must be converted to a credential for the local helper VM.')]
    [CmdletBinding(SupportsShouldProcess)]
    param()

    $vmConfig = New-AzVMConfig -VMName $cfg.VmName -VMSize $vmSize

    # A throwaway OS disk for the hydration helper VM itself (a stock marketplace
    # image); the *source* disks are attached as data disks for patching.
    $image = if ($cfg.OsType -eq 'Windows') {
        @{ PublisherName = 'MicrosoftWindowsServer'; Offer = 'WindowsServer'; Skus = '2022-datacenter-azure-edition'; Version = 'latest' }
    }
    else {
        @{ PublisherName = 'Canonical'; Offer = '0001-com-ubuntu-server-jammy'; Skus = '22_04-lts-gen2'; Version = 'latest' }
    }

    # Prefer the secret from the environment so it never has to live in the
    # config file; fall back to the config value for convenience.
    $plainPassword = if ($env:HYDRATION_ADMIN_PASSWORD) { $env:HYDRATION_ADMIN_PASSWORD }
                     elseif ($cfg.ContainsKey('AdminPassword') -and $cfg.AdminPassword) { $cfg.AdminPassword }
                     else { throw 'No admin password supplied. Set $env:HYDRATION_ADMIN_PASSWORD or AdminPassword in the config.' }

    $cred = [pscredential]::new(
        $cfg.AdminUsername,
        (ConvertTo-SecureString $plainPassword -AsPlainText -Force))

    if ($cfg.OsType -eq 'Windows') {
        $vmConfig = Set-AzVMOperatingSystem -VM $vmConfig -Windows -ComputerName $cfg.VmName -Credential $cred -ProvisionVMAgent
    }
    else {
        $vmConfig = Set-AzVMOperatingSystem -VM $vmConfig -Linux -ComputerName $cfg.VmName -Credential $cred
    }
    $vmConfig = Set-AzVMSourceImage -VM $vmConfig @image

    # Network plumbing
    $vnet = Get-AzVirtualNetwork -ResourceGroupName $cfg.ResourceGroupName -Name $cfg.VNetName
    $subnetId = ($vnet.Subnets | Where-Object Name -eq $cfg.SubnetName).Id
    $nic = New-AzNetworkInterface -ResourceGroupName $cfg.ResourceGroupName -Location $cfg.Location `
        -Name "$($cfg.VmName)-nic" -SubnetId $subnetId -Force
    $vmConfig = Add-AzVMNetworkInterface -VM $vmConfig -Id $nic.Id

    # Attach the source data disks at deterministic LUNs starting at 0.
    $lun = 0
    foreach ($disk in $cfg.DataDisks) {
        $managed = Get-AzDisk -ResourceGroupName $cfg.ResourceGroupName -DiskName $disk.DiskName
        $vmConfig = Add-AzVMDataDisk -VM $vmConfig -ManagedDiskId $managed.Id -Lun $lun -CreateOption Attach
        Write-Verbose "Attached '$($disk.DiskName)' at LUN $lun."
        $lun++
    }

    if ($PSCmdlet.ShouldProcess($cfg.VmName, 'Create hydration VM')) {
        New-AzVM -ResourceGroupName $cfg.ResourceGroupName -Location $cfg.Location -VM $vmConfig | Out-Null
    }
    return Get-AzVM -ResourceGroupName $cfg.ResourceGroupName -Name $cfg.VmName
}

# ---------------------------------------------------------------------------
# 2. Generate the recovery-info file dynamically and stage artefacts
# ---------------------------------------------------------------------------
function Publish-HydrationArtifact {
    [CmdletBinding(SupportsShouldProcess)]
    param([Parameter(Mandatory)]$Vm)

    # SourceDiskId overrides let recovery scenarios pin ids to the host-info XML.
    $overrides = @{}
    foreach ($disk in $cfg.DataDisks) {
        if ($disk.ContainsKey('SourceDiskId') -and $disk.SourceDiskId) {
            $overrides[$disk.DiskName] = $disk.SourceDiskId
        }
    }

    $diskLunMap = Get-HydrationDiskLunMap -Vm $Vm -SourceDiskId $overrides
    $statusSas = New-HydrationStatusBlobSasToken -Context $ctx -ContainerName $cfg.ContainerName

    $confName = "azurerecovery-$hostId.conf"
    $confPath = Join-Path $cfg.ArtifactsPath $confName

    $recoveryArgs = @{
        Path             = $confPath
        StatusBlobSasUri = $statusSas
        DiskLunMap       = $diskLunMap
        HostId           = $hostId
        TestFailover     = [bool]($cfg['TestFailover'])
        EnableRDP        = [bool]($cfg['EnableRDP'])
    }
    if ($cfg.ContainsKey('OsDiskId') -and $cfg.OsDiskId) {
        $recoveryArgs.IsUEFI = [bool]($cfg['IsUEFI'])
        $recoveryArgs.OsDiskId = $cfg.OsDiskId
        if ($cfg.ContainsKey('DiskSignature')) { $recoveryArgs.DiskSignature = [uint32]$cfg.DiskSignature }
        if ($cfg.ContainsKey('ActivePartitionStartingOffset')) { $recoveryArgs.ActivePartitionStartingOffset = [long]$cfg.ActivePartitionStartingOffset }
    }

    New-AzureRecoveryInfoFile @recoveryArgs | Out-Null
    Write-Information "Generated recovery-info file '$confPath' for $($diskLunMap.Count) disk(s)." -InformationAction Continue

    # Upload artefacts (recovery-info, tools zip, optional host-info XML).
    $uploads = [System.Collections.Generic.List[string]]::new()
    $uploads.Add($confPath)
    $uploads.Add((Join-Path $cfg.ArtifactsPath 'AzureRecoveryTools.zip'))
    if ($cfg.ContainsKey('HostInfoPath') -and $cfg.HostInfoPath) { $uploads.Add($cfg.HostInfoPath) }

    foreach ($file in $uploads) {
        if (-not (Test-Path -LiteralPath $file)) { throw "Artefact not found: $file" }
        if ($PSCmdlet.ShouldProcess($file, 'Upload artefact')) {
            Set-AzStorageBlobContent -File $file -Container $cfg.ContainerName -Blob (Split-Path $file -Leaf) `
                -Context $ctx -Force | Out-Null
        }
    }
    return @{ ConfName = $confName; ConfPath = $confPath }
}

# ---------------------------------------------------------------------------
# 3. Push the Custom Script Extension that runs AzureRecoveryUtil
# ---------------------------------------------------------------------------
function Set-HydrationCustomScriptExtension {
    [CmdletBinding(SupportsShouldProcess)]
    param([Parameter(Mandatory)][string]$ConfName)

    $containerUri = "$($ctx.BlobEndPoint)$($cfg.ContainerName)"
    $sasToken = New-AzStorageContainerSASToken -Name $cfg.ContainerName -Permission r `
        -ExpiryTime (Get-Date).AddHours(12) -Context $ctx

    if ($cfg.OsType -eq 'Windows') {
        $fileUris = @(
            "$containerUri/StartupScript.ps1$sasToken",
            "$containerUri/AzureRecoveryTools.zip$sasToken",
            "$containerUri/$ConfName$sasToken"
        )
        $settings = @{ fileUris = $fileUris }
        $protected = @{ commandToExecute = "powershell -ExecutionPolicy Unrestricted -File StartupScript.ps1 $hostId $Operation" }
        $params = @{
            ResourceGroupName  = $cfg.ResourceGroupName
            VMName             = $cfg.VmName
            Name               = 'HydrationCustomScript'
            Publisher          = 'Microsoft.Compute'
            ExtensionType      = 'CustomScriptExtension'
            TypeHandlerVersion = '1.10'
            Settings           = $settings
            ProtectedSettings  = $protected
            Location           = $cfg.Location
        }
    }
    else {
        $fileUris = @(
            "$containerUri/StartupScript.sh$sasToken",
            "$containerUri/AzureRecoveryTools.zip$sasToken",
            "$containerUri/$ConfName$sasToken"
        )
        $settings = @{ fileUris = $fileUris }
        # NB: StartupScript.sh takes its arguments in the REVERSE order of the
        # Windows StartupScript.ps1 - scenario first, then host-id.
        $protected = @{ commandToExecute = "sh StartupScript.sh $Operation $hostId" }
        $params = @{
            ResourceGroupName  = $cfg.ResourceGroupName
            VMName             = $cfg.VmName
            Name               = 'HydrationCustomScript'
            Publisher          = 'Microsoft.Azure.Extensions'
            ExtensionType      = 'CustomScript'
            TypeHandlerVersion = '2.1'
            Settings           = $settings
            ProtectedSettings  = $protected
            Location           = $cfg.Location
        }
    }

    if ($PSCmdlet.ShouldProcess($cfg.VmName, 'Set hydration Custom Script Extension')) {
        Set-AzVMExtension @params | Out-Null
    }
}

# ---------------------------------------------------------------------------
# 4. Teardown
# ---------------------------------------------------------------------------
function Remove-HydrationVm {
    [CmdletBinding(SupportsShouldProcess)]
    param()
    if ($PSCmdlet.ShouldProcess($cfg.VmName, 'Remove hydration VM')) {
        Remove-AzVM -ResourceGroupName $cfg.ResourceGroupName -Name $cfg.VmName -Force | Out-Null
    }
}

# ---------------------------------------------------------------------------
# Orchestration
# ---------------------------------------------------------------------------
$artefacts = $null
try {
    Write-Information "=== Hydration run: operation '$Operation', VM '$($cfg.VmName)' ===" -InformationAction Continue

    $vm = New-HydrationVm
    $artefacts = Publish-HydrationArtifact -Vm $vm
    Set-HydrationCustomScriptExtension -ConfName $artefacts.ConfName

    Wait-HydrationStatus -Context $ctx -ContainerName $cfg.ContainerName | Out-Null
    Write-Information 'Hydration completed successfully.' -InformationAction Continue

    if (-not $SkipRecoveredVm) {
        Write-Information 'Detaching disks from hydration VM before building recovered VM.' -InformationAction Continue
        Remove-HydrationVm
        # The recovered-VM build re-uses the now-patched managed disks; callers
        # typically promote the OS disk to a new VM here. Left as an explicit,
        # scenario-specific step rather than a hidden default.
        Write-Information 'Patched disks are ready. Build the recovered VM from the OS disk as needed.' -InformationAction Continue
    }
}
finally {
    if (-not $KeepResources) {
        Write-Information 'Cleaning up hydration artefacts.' -InformationAction Continue
        $confBlob = if ($artefacts) { $artefacts.ConfName } else { $null }
        foreach ($blob in @('AzureRecoveryTools.zip', $confBlob, 'recoveryutiltestrunner.status')) {
            if ($blob) {
                Remove-AzStorageBlob -Container $cfg.ContainerName -Blob $blob -Context $ctx -Force -ErrorAction SilentlyContinue
            }
        }
    }
}
