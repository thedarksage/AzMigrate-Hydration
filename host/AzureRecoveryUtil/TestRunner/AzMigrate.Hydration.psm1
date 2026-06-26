<#
.SYNOPSIS
    Azure Migrate / Site Recovery hydration test-runner module.

.DESCRIPTION
    Modern, Az-module based orchestration for exercising the AzureRecoveryUtil
    hydration tool end to end:

        create managed disks  -> create hydration VM + attach disks
        -> push Custom Script Extension (which runs AzureRecoveryUtil)
        -> wait for completion -> create the recovered VM -> clean up.

    This replaces the legacy classic-ASM (Add-AzureDisk / New-AzureVM /
    Set-AzureVMCustomScriptExtension) harness.

    The centrepiece is New-AzureRecoveryInfoFile, which DYNAMICALLY generates
    the azurerecovery-<id>.conf recovery-info file (including the [DiskMap]
    disk-id -> LUN section) from the live hydration VM, removing the old manual
    "VHD_DISK_ID_MAPPING" blocker where operators had to hand-type every source
    disk GUID.

.NOTES
    Mirrors the production recovery-info contract produced by
    ProtSvcTeeImplementation.TaskCommonOperations.PrepareAzureRecoveryConfigFile.
#>

Set-StrictMode -Version Latest

# Recovery-info file keys. Kept in sync with
# AzureRecoveryLib/config/RecoveryConfig.h (RecoveryConfigKey) and the
# production PrepareAzureRecoveryConfigFile writer.
$script:RecoveryConfigKey = @{
    StatusBlobUri                 = 'PreRecoveryExecutionBlobSasUri'
    TestFailover                  = 'TestFailover'
    HostId                        = 'HostId'
    EnableRDP                     = 'EnableRDP'
    IsUEFI                        = 'IsUEFI'
    ActivePartitionStartingOffset = 'ActivePartitionStartingOffset'
    DiskSignature                 = 'DiskSignature'
    LogLevel                      = 'LogLevel'
    DiskMapSection                = 'DiskMap'
}

function ConvertTo-IniBool {
    <#
    .SYNOPSIS
        Renders a boolean the way the native recovery-config parser expects
        ("true" / "false", lower-case) so values round-trip through boost ini.
    #>
    [CmdletBinding()]
    [OutputType([string])]
    param([Parameter(Mandatory)][bool]$Value)

    return $Value.ToString().ToLowerInvariant()
}

function New-AzureRecoveryInfoFile {
    <#
    .SYNOPSIS
        Generates an AzureRecoveryUtil recovery-info (.conf) file from a
        disk-id -> LUN map.

    .DESCRIPTION
        Writes the recovery-info INI consumed by
        "AzureRecoveryUtil --recoveryinfofile". The [DiskMap] section is built
        from the supplied DiskLunMap, so callers never hand-edit disk GUIDs.

        Use Get-HydrationDiskLunMap to derive DiskLunMap automatically from a
        live hydration VM.

    .PARAMETER Path
        Full path of the .conf file to (over)write.

    .PARAMETER StatusBlobSasUri
        Read/write SAS URI of the status (page) blob the tool reports progress
        to. Emitted as PreRecoveryExecutionBlobSasUri.

    .PARAMETER DiskLunMap
        Ordered map of source-disk-id -> SCSI LUN. Emitted under [DiskMap].

    .PARAMETER HostId
        Source host id. Defaults to a new GUID when not supplied (valid for
        migration / gen-conversion scenarios that carry no host info).

    .PARAMETER TestFailover
        Whether this is a test failover. Default: $false.

    .PARAMETER EnableRDP
        Force-enable RDP on the recovered Windows VM. Default: $false.

    .PARAMETER IsUEFI
        Source disk is UEFI/GPT. When set, ActivePartitionStartingOffset and
        DiskSignature describe the MBR-converted OS disk. Default: $false.

    .PARAMETER ActivePartitionStartingOffset
        Byte offset of the active partition (UEFI scenarios only).

    .PARAMETER DiskSignature
        MBR disk signature of the converted OS disk (UEFI scenarios only). When
        non-zero it replaces the OS disk-id key in [DiskMap], matching the
        production writer.

    .PARAMETER OsDiskId
        Disk-id (key in DiskLunMap) of the OS disk. Required only when IsUEFI
        and DiskSignature are set, so the OS entry can be rewritten.

    .PARAMETER LogLevel
        Native tool log verbosity (0-7). Default: 4.

    .OUTPUTS
        System.String. The path written.

    .EXAMPLE
        New-AzureRecoveryInfoFile -Path .\azurerecovery-abc.conf `
            -StatusBlobSasUri $sas -DiskLunMap @{ '6000C29...' = 0; '6000C30...' = 1 }
    #>
    [CmdletBinding(SupportsShouldProcess)]
    [OutputType([string])]
    param(
        [Parameter(Mandatory)]
        [ValidateNotNullOrEmpty()]
        [string]$Path,

        [Parameter(Mandatory)]
        [ValidateNotNullOrEmpty()]
        [string]$StatusBlobSasUri,

        [Parameter(Mandatory)]
        [ValidateNotNull()]
        [System.Collections.IDictionary]$DiskLunMap,

        [ValidateNotNullOrEmpty()]
        [string]$HostId = [guid]::NewGuid().ToString(),

        [bool]$TestFailover = $false,

        [bool]$EnableRDP = $false,

        [bool]$IsUEFI = $false,

        [long]$ActivePartitionStartingOffset = 0,

        [uint32]$DiskSignature = 0,

        [string]$OsDiskId,

        [ValidateRange(0, 7)]
        [int]$LogLevel = 4
    )

    if ($DiskLunMap.Count -eq 0) {
        throw [System.ArgumentException]::new(
            'DiskLunMap must contain at least one disk-id -> LUN entry.', 'DiskLunMap')
    }

    $k = $script:RecoveryConfigKey
    $sb = [System.Text.StringBuilder]::new()

    # Single-line settings first; the native parser treats everything after the
    # [DiskMap] header as disk entries, so these must precede it.
    [void]$sb.AppendLine("$($k.StatusBlobUri)=$StatusBlobSasUri")
    [void]$sb.AppendLine("$($k.TestFailover)=$(ConvertTo-IniBool $TestFailover)")
    [void]$sb.AppendLine("$($k.HostId)=$HostId")
    [void]$sb.AppendLine("$($k.EnableRDP)=$(ConvertTo-IniBool $EnableRDP)")
    [void]$sb.AppendLine("$($k.IsUEFI)=$(ConvertTo-IniBool $IsUEFI)")
    [void]$sb.AppendLine("$($k.ActivePartitionStartingOffset)=$ActivePartitionStartingOffset")
    [void]$sb.AppendLine("$($k.DiskSignature)=$DiskSignature")
    [void]$sb.AppendLine("$($k.LogLevel)=$LogLevel")

    [void]$sb.AppendLine("[$($k.DiskMapSection)]")
    foreach ($entry in $DiskLunMap.GetEnumerator()) {
        $diskId = [string]$entry.Key
        $lun = [int]$entry.Value

        # For UEFI, the original OS disk-id is replaced by the MBR disk
        # signature of the converted disk (matches the production writer).
        if ($IsUEFI -and $DiskSignature -ne 0 -and
            -not [string]::IsNullOrEmpty($OsDiskId) -and
            [string]::Equals($diskId, $OsDiskId, [System.StringComparison]::OrdinalIgnoreCase)) {
            Write-Verbose "Rewriting OS disk-id '$OsDiskId' as disk signature '$DiskSignature'."
            [void]$sb.AppendLine("$DiskSignature=$lun")
        }
        else {
            [void]$sb.AppendLine("$diskId=$lun")
        }
    }

    if ($PSCmdlet.ShouldProcess($Path, 'Write recovery-info file')) {
        $dir = Split-Path -Path $Path -Parent
        if ($dir -and -not (Test-Path -LiteralPath $dir)) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
        }
        # ASCII, no BOM, LF-free trailing handled by Set-Content; the native
        # boost ini parser is tolerant of CRLF.
        Set-Content -LiteralPath $Path -Value $sb.ToString() -Encoding ascii -NoNewline -Force
        Write-Verbose "Wrote recovery-info file '$Path' with $($DiskLunMap.Count) disk(s)."
    }

    return $Path
}

function Get-HydrationDiskLunMap {
    <#
    .SYNOPSIS
        Derives the source-disk-id -> LUN map automatically from a hydration VM.

    .DESCRIPTION
        Reads the data disks currently attached to the VM and returns an ordered
        map keyed by disk-id. By default the disk-id is the managed disk name;
        callers can override per disk via SourceDiskId (for recovery scenarios
        where the id must match the host-info XML).

        This is the dynamic replacement for the legacy hand-typed
        VHD_DISK_ID_MAPPING string.

    .PARAMETER Vm
        The hydration VM object (from Get-AzVM) whose data disks are inspected.

    .PARAMETER SourceDiskId
        Optional hashtable mapping managed-disk-name -> desired source-disk-id.
        Names not present fall back to the managed disk name.

    .OUTPUTS
        System.Collections.Specialized.OrderedDictionary. disk-id -> LUN.
    #>
    [CmdletBinding()]
    [OutputType([System.Collections.Specialized.OrderedDictionary])]
    param(
        [Parameter(Mandatory)]
        [ValidateNotNull()]
        [psobject]$Vm,

        [hashtable]$SourceDiskId = @{}
    )

    $map = [ordered]@{}

    $dataDisks = @($Vm.StorageProfile.DataDisks) | Sort-Object Lun
    if ($dataDisks.Count -eq 0) {
        Write-Warning "VM '$($Vm.Name)' has no attached data disks."
        return $map
    }

    foreach ($disk in $dataDisks) {
        $name = $disk.Name
        $diskId = if ($SourceDiskId.ContainsKey($name)) { $SourceDiskId[$name] } else { $name }
        $map[$diskId] = [int]$disk.Lun
        Write-Verbose "[DiskMap] '$diskId' -> LUN $($disk.Lun) (disk '$name')."
    }

    return $map
}

function New-HydrationStatusBlobSasToken {
    <#
    .SYNOPSIS
        Creates (if needed) the status page blob and returns a read/write SAS URI.

    .DESCRIPTION
        AzureRecoveryUtil reports progress by updating the metadata of this blob;
        the harness polls it via Wait-HydrationStatus.

    .PARAMETER Context
        Azure Storage context (from New-AzStorageContext / (Get-AzStorageAccount).Context).

    .PARAMETER ContainerName
        Blob container that holds the hydration artefacts.

    .PARAMETER BlobName
        Status blob name. Default: recoveryutiltestrunner.status.

    .PARAMETER TtlHours
        SAS validity window in hours. Default: 12.
    #>
    [CmdletBinding(SupportsShouldProcess)]
    [OutputType([string])]
    param(
        [Parameter(Mandatory)][ValidateNotNull()][object]$Context,
        [Parameter(Mandatory)][ValidateNotNullOrEmpty()][string]$ContainerName,
        [string]$BlobName = 'recoveryutiltestrunner.status',
        [int]$TtlHours = 12
    )

    $existing = Get-AzStorageBlob -Container $ContainerName -Blob $BlobName -Context $Context -ErrorAction SilentlyContinue
    if (-not $existing -and $PSCmdlet.ShouldProcess($BlobName, 'Create status blob')) {
        $tmp = New-TemporaryFile
        try {
            Set-AzStorageBlobContent -File $tmp.FullName -Container $ContainerName -Blob $BlobName `
                -BlobType Page -Context $Context -Force | Out-Null
        }
        finally {
            Remove-Item -LiteralPath $tmp.FullName -Force -ErrorAction SilentlyContinue
        }
    }

    $sas = New-AzStorageBlobSASToken -Container $ContainerName -Blob $BlobName -Permission rw `
        -ExpiryTime (Get-Date).AddHours($TtlHours) -Context $Context -FullUri
    return $sas
}

function Wait-HydrationStatus {
    <#
    .SYNOPSIS
        Blocks until the status blob reports Success or Failed.

    .PARAMETER Context
        Azure Storage context.

    .PARAMETER ContainerName
        Container holding the status blob.

    .PARAMETER BlobName
        Status blob name. Default: recoveryutiltestrunner.status.

    .PARAMETER PollIntervalSeconds
        Seconds between polls. Default: 30.

    .PARAMETER TimeoutMinutes
        Overall timeout. Default: 120.
    #>
    [CmdletBinding()]
    [OutputType([string])]
    param(
        [Parameter(Mandatory)][ValidateNotNull()][object]$Context,
        [Parameter(Mandatory)][ValidateNotNullOrEmpty()][string]$ContainerName,
        [string]$BlobName = 'recoveryutiltestrunner.status',
        [ValidateRange(5, 600)][int]$PollIntervalSeconds = 30,
        [ValidateRange(1, 1440)][int]$TimeoutMinutes = 120
    )

    $deadline = (Get-Date).AddMinutes($TimeoutMinutes)
    while ($true) {
        $blob = Get-AzStorageBlob -Container $ContainerName -Blob $BlobName -Context $Context -ErrorAction Stop
        $metadata = $blob.ICloudBlob.Metadata
        $status = if ($metadata.ContainsKey('ExecutionStatus')) { $metadata['ExecutionStatus'] } else { 'Pending' }

        switch ($status) {
            'Success' {
                Write-Verbose 'Hydration execution succeeded.'
                return $status
            }
            'Failed' {
                $detail = ($metadata.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join '; '
                throw "Hydration execution failed. Status metadata: $detail"
            }
            default {
                if ((Get-Date) -gt $deadline) {
                    throw "Timed out after $TimeoutMinutes minute(s) waiting for hydration (last status '$status')."
                }
                Write-Information "Hydration status: $status" -InformationAction Continue
                Start-Sleep -Seconds $PollIntervalSeconds
            }
        }
    }
}

Export-ModuleMember -Function @(
    'New-AzureRecoveryInfoFile',
    'Get-HydrationDiskLunMap',
    'New-HydrationStatusBlobSasToken',
    'Wait-HydrationStatus',
    'ConvertTo-IniBool'
)
