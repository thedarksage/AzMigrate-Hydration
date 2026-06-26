# Hydration Test Runner

Az PowerShell harness for exercising the **AzureRecoveryUtil** hydration tool
end to end against real Azure managed disks.

This replaces the legacy classic-ASM (`Add-AzureDisk` / `New-AzureVM` /
`Set-AzureVMCustomScriptExtension`) `TestRunners.ps1` harness, which was retired
along with the Azure Service Management cmdlets.

## What it does

It reproduces, for test purposes, the production hydration workflow performed by
`ProtSvcTeeImplementation`:

1. Create a hydration VM and attach the source data disks at deterministic LUNs.
2. **Dynamically generate the recovery-info (`azurerecovery-<id>.conf`) file**
   from the live VM — including the `[DiskMap]` disk-id → LUN section — so no
   disk GUIDs are hand-typed.
3. Upload `AzureRecoveryTools.zip` + the recovery-info (+ optional host-info XML)
   and run them through the Custom Script Extension, which invokes
   `AzureRecoveryUtil --operation <recovery|migration|genconversion>`.
4. Poll the status blob until completion, then optionally build the recovered VM
   and tear down the hydration resources.

## Files

| File | Purpose |
|------|---------|
| `AzMigrate.Hydration.psm1` / `.psd1` | Module: `New-AzureRecoveryInfoFile`, `Get-HydrationDiskLunMap`, `New-HydrationStatusBlobSasToken`, `Wait-HydrationStatus`. |
| `Invoke-AzureHydration.ps1` | End-to-end orchestrator (config-driven). |
| `HydrationConfig.sample.psd1` | Sample run configuration — copy and fill in. |
| `AzMigrate.Hydration.Tests.ps1` | Pester tests (offline) for the dynamic recovery-info generation. |

## The fix: dynamic recovery-info generation

The legacy harness required operators to hand-author a `VHD_DISK_ID_MAPPING`
string mapping every VHD to its source disk GUID, then manually edit the
generated `.conf`. That manual step was the long-standing blocker.

`Get-HydrationDiskLunMap` now derives the disk-id → LUN map directly from the
hydration VM's attached data disks, and `New-AzureRecoveryInfoFile` writes the
full recovery-info contract (matching
`ProtSvcTeeImplementation.PrepareAzureRecoveryConfigFile` and
`AzureRecoveryLib/config/RecoveryConfig.h`). For `recovery` scenarios where the
disk-id must line up with the host-info XML, supply `SourceDiskId` per disk in
the config.

## Usage

```powershell
Connect-AzAccount
Copy-Item ./HydrationConfig.sample.psd1 ./my-run.psd1   # then edit values
./Invoke-AzureHydration.ps1 -ConfigPath ./my-run.psd1 -Operation migration -Verbose
```

Useful switches: `-WhatIf` (dry run), `-SkipRecoveredVm`, `-KeepResources`.

## Running the tests

```powershell
Invoke-Pester -Path ./AzMigrate.Hydration.Tests.ps1
```

The tests are fully offline (no Azure calls) and validate the recovery-info file
contract.
