@{
    # ---------------------------------------------------------------------
    # Sample configuration for Invoke-AzureHydration.ps1
    # Copy this file, fill in your values, and pass it via -ConfigPath.
    # ---------------------------------------------------------------------

    # Azure context
    SubscriptionId     = '00000000-0000-0000-0000-000000000000'
    ResourceGroupName  = 'rg-hydration-test'
    Location           = 'eastus2'

    # Storage account that holds the hydration artefacts + status blob.
    StorageAccountName = 'myhydrationstg'
    ContainerName      = 'hydration'

    # Hydration helper VM.
    VmName             = 'hydration-vm'
    VmSize             = 'Standard_D2s_v5'
    OsType             = 'Linux'            # 'Linux' or 'Windows'
    AdminUsername      = 'azureuser'
    AdminPassword      = 'REPLACE-with-a-secret-or-Key-Vault-reference'

    # Networking (must already exist in ResourceGroupName).
    VNetName           = 'vnet-hydration'
    SubnetName         = 'default'

    # Source managed disks to patch. LUNs are assigned automatically in order;
    # the [DiskMap] in the generated recovery-info file is built from these -
    # no manual GUID mapping required.
    #
    # SourceDiskId is OPTIONAL. Supply it only for 'recovery' scenarios where
    # the disk-id must match the host-info XML; otherwise the managed disk name
    # is used.
    DataDisks          = @(
        @{ DiskName = 'source-osdisk' }
        @{ DiskName = 'source-datadisk-1' }
        # @{ DiskName = 'source-datadisk-2'; SourceDiskId = '6000C29A-....' }
    )

    # Local folder containing AzureRecoveryTools.zip (and StartupScript.* /
    # optional host-info XML). The generated .conf is written here too.
    ArtifactsPath      = 'C:\hydration\artifacts'

    # Optional host-info XML (recovery scenario).
    # HostInfoPath     = 'C:\hydration\artifacts\hostinfo-<hostid>.xml'

    # Optional run flags.
    HostId             = ''        # blank => a new GUID is generated
    TestFailover       = $false
    EnableRDP          = $false

    # UEFI / GPT source disk (optional). When set, the OS [DiskMap] entry is
    # rewritten to the MBR disk signature, matching the production writer.
    # IsUEFI                         = $true
    # OsDiskId                       = 'source-osdisk'
    # DiskSignature                  = 305419896
    # ActivePartitionStartingOffset  = 1048576
}
