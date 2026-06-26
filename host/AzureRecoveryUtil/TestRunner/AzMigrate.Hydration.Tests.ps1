#Requires -Modules @{ ModuleName = 'Pester'; ModuleVersion = '5.0.0' }
<#
    Unit tests for the dynamic recovery-info generation in AzMigrate.Hydration.
    These run fully offline (no Azure) and validate the .conf contract consumed
    by AzureRecoveryUtil (AzureRecoveryLib/config/RecoveryConfig.h).
#>

BeforeAll {
    $modulePath = Join-Path $PSScriptRoot 'AzMigrate.Hydration.psd1'
    Import-Module $modulePath -Force
}

Describe 'New-AzureRecoveryInfoFile' {

    BeforeEach {
        $script:confPath = Join-Path ([System.IO.Path]::GetTempPath()) ("azurerecovery-{0}.conf" -f [guid]::NewGuid())
    }

    AfterEach {
        if (Test-Path -LiteralPath $script:confPath) { Remove-Item -LiteralPath $script:confPath -Force }
    }

    It 'writes all single-line keys before the [DiskMap] section' {
        $map = [ordered]@{ 'disk-a' = 0; 'disk-b' = 1 }
        New-AzureRecoveryInfoFile -Path $script:confPath -StatusBlobSasUri 'https://x/y?sas' -DiskLunMap $map -HostId '11111111-1111-1111-1111-111111111111' | Out-Null

        $lines = Get-Content -LiteralPath $script:confPath
        $diskMapIndex = [array]::IndexOf($lines, '[DiskMap]')

        $diskMapIndex | Should -BeGreaterThan 0
        ($lines[0..($diskMapIndex - 1)] -join "`n") | Should -Match 'PreRecoveryExecutionBlobSasUri=https://x/y\?sas'
        $lines[0..($diskMapIndex - 1)] | Should -Contain 'HostId=11111111-1111-1111-1111-111111111111'
        $lines[0..($diskMapIndex - 1)] | Should -Contain 'TestFailover=false'
    }

    It 'emits the disk-id -> LUN map under [DiskMap] without manual editing' {
        $map = [ordered]@{ '6000C29A' = 0; '6000C30B' = 1; '6000C41C' = 2 }
        New-AzureRecoveryInfoFile -Path $script:confPath -StatusBlobSasUri 'https://x?sas' -DiskLunMap $map | Out-Null

        $lines = Get-Content -LiteralPath $script:confPath
        $lines | Should -Contain '6000C29A=0'
        $lines | Should -Contain '6000C30B=1'
        $lines | Should -Contain '6000C41C=2'
    }

    It 'uses key=value with no surrounding spaces (production contract)' {
        $map = [ordered]@{ 'd' = 5 }
        New-AzureRecoveryInfoFile -Path $script:confPath -StatusBlobSasUri 'sas' -DiskLunMap $map | Out-Null

        (Get-Content -LiteralPath $script:confPath) | Should -Not -Match '\s=\s'
    }

    It 'renders booleans as lower-case true/false' {
        $map = [ordered]@{ 'd' = 0 }
        New-AzureRecoveryInfoFile -Path $script:confPath -StatusBlobSasUri 'sas' -DiskLunMap $map -TestFailover $true -EnableRDP $true | Out-Null

        $lines = Get-Content -LiteralPath $script:confPath
        $lines | Should -Contain 'TestFailover=true'
        $lines | Should -Contain 'EnableRDP=true'
        $lines | Should -Contain 'IsUEFI=false'
    }

    It 'replaces the OS disk-id with the disk signature for UEFI disks' {
        $map = [ordered]@{ 'os-disk-id' = 0; 'data-disk-id' = 1 }
        New-AzureRecoveryInfoFile -Path $script:confPath -StatusBlobSasUri 'sas' -DiskLunMap $map `
            -IsUEFI $true -DiskSignature ([uint32]305419896) -OsDiskId 'os-disk-id' | Out-Null

        $lines = Get-Content -LiteralPath $script:confPath
        $lines | Should -Contain 'IsUEFI=true'
        $lines | Should -Contain '305419896=0'      # OS entry rewritten to signature
        $lines | Should -Contain 'data-disk-id=1'   # data entry untouched
        $lines | Should -Not -Contain 'os-disk-id=0'
    }

    It 'defaults HostId to a GUID when not supplied' {
        $map = [ordered]@{ 'd' = 0 }
        New-AzureRecoveryInfoFile -Path $script:confPath -StatusBlobSasUri 'sas' -DiskLunMap $map | Out-Null

        $hostLine = (Get-Content -LiteralPath $script:confPath | Where-Object { $_ -like 'HostId=*' })
        $guid = $hostLine -replace '^HostId=', ''
        [guid]::TryParse($guid, [ref]([guid]::Empty)) | Should -BeTrue
    }

    It 'throws when DiskLunMap is empty' {
        { New-AzureRecoveryInfoFile -Path $script:confPath -StatusBlobSasUri 'sas' -DiskLunMap @{} } |
            Should -Throw -ExpectedMessage '*at least one*'
    }
}

Describe 'Get-HydrationDiskLunMap' {

    It 'derives an ordered disk-id -> LUN map from the VM data disks' {
        $vm = [pscustomobject]@{
            Name           = 'hydration-vm'
            StorageProfile = [pscustomobject]@{
                DataDisks = @(
                    [pscustomobject]@{ Name = 'osdisk';   Lun = 0 },
                    [pscustomobject]@{ Name = 'datadisk1'; Lun = 1 },
                    [pscustomobject]@{ Name = 'datadisk2'; Lun = 2 }
                )
            }
        }

        $map = Get-HydrationDiskLunMap -Vm $vm
        $map['osdisk']    | Should -Be 0
        $map['datadisk1'] | Should -Be 1
        $map['datadisk2'] | Should -Be 2
    }

    It 'honours SourceDiskId overrides (recovery scenarios)' {
        $vm = [pscustomobject]@{
            Name           = 'hydration-vm'
            StorageProfile = [pscustomobject]@{
                DataDisks = @( [pscustomobject]@{ Name = 'osdisk'; Lun = 0 } )
            }
        }

        $map = Get-HydrationDiskLunMap -Vm $vm -SourceDiskId @{ 'osdisk' = '6000C-source-guid' }
        $map.Keys | Should -Contain '6000C-source-guid'
        $map['6000C-source-guid'] | Should -Be 0
    }

    It 'feeds straight into New-AzureRecoveryInfoFile (end-to-end, offline)' {
        $vm = [pscustomobject]@{
            Name           = 'hydration-vm'
            StorageProfile = [pscustomobject]@{
                DataDisks = @(
                    [pscustomobject]@{ Name = 'osdisk';    Lun = 0 },
                    [pscustomobject]@{ Name = 'datadisk1'; Lun = 1 }
                )
            }
        }
        $confPath = Join-Path ([System.IO.Path]::GetTempPath()) ("e2e-{0}.conf" -f [guid]::NewGuid())
        try {
            $map = Get-HydrationDiskLunMap -Vm $vm
            New-AzureRecoveryInfoFile -Path $confPath -StatusBlobSasUri 'https://s?sas' -DiskLunMap $map | Out-Null

            $lines = Get-Content -LiteralPath $confPath
            $lines | Should -Contain 'osdisk=0'
            $lines | Should -Contain 'datadisk1=1'
        }
        finally {
            if (Test-Path -LiteralPath $confPath) { Remove-Item -LiteralPath $confPath -Force }
        }
    }
}
