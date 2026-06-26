@{
    RootModule        = 'AzMigrate.Hydration.psm1'
    ModuleVersion     = '1.0.0'
    GUID              = 'b2e3c7a1-6f4d-4c2a-9d8e-2a1f0c5b9e44'
    Author            = 'Azure Site Recovery / Azure Migrate'
    CompanyName       = 'Microsoft'
    Copyright         = '(c) Microsoft Corporation. All rights reserved.'
    Description       = 'Az-module test harness for the AzureRecoveryUtil hydration tool, including dynamic recovery-info (.conf) generation.'
    PowerShellVersion = '7.2'
    RequiredModules   = @(
        @{ ModuleName = 'Az.Accounts'; ModuleVersion = '2.12.0' },
        @{ ModuleName = 'Az.Compute';  ModuleVersion = '5.0.0'  },
        @{ ModuleName = 'Az.Storage';  ModuleVersion = '5.0.0'  }
    )
    FunctionsToExport = @(
        'New-AzureRecoveryInfoFile',
        'Get-HydrationDiskLunMap',
        'New-HydrationStatusBlobSasToken',
        'Wait-HydrationStatus',
        'ConvertTo-IniBool'
    )
    CmdletsToExport   = @()
    VariablesToExport = @()
    AliasesToExport   = @()
    PrivateData       = @{
        PSData = @{
            Tags       = @('Azure', 'AzureMigrate', 'SiteRecovery', 'Hydration')
            ProjectUri = 'https://github.com/Azure/AzMigrate-Hydration'
        }
    }
}
