<#

.SYNOPSIS
Powershell script to perform End To End Testing for A2A

.DESCRIPTION
The script is used to perform EnableProtection, TFO, UFO, SwitchProtection, DisableProtection operations on Azure VM for A2A.

.PARAMETER Operation
Mandatory parameter, it indicates the operation to be performed on the VM. Allowed values for Operation are "ER", "TFO", "UFO", "SP", "FB", "DR", "RSP", "CR", "ALL", "Reboot", "APPLYLOAD", "ISSUETAG", "VERIFYLOAD","RUN540", "CLEANPREVIOUSRUN","SETVAULTCONTEXT", "HELP"

.EXAMPLE
./A2AGQLTest.ps1 "ER"
It performs Enable Protection on the Primary VM

.EXAMPLE
./A2AGQLTest.ps1 "TFO"
It triggers Test Failover on the Primary VM

./A2AGQLTest.ps1 "Reboot"
It reboots the Source VM

.EXAMPLE
./A2AGQLTest.ps1 "UFO"
It triggers UnplannedFailover on the Primary VM

.EXAMPLE
./A2AGQLTest.ps1 "SP"
It triggers ReverseProtection on the UFO VM

.EXAMPLE
./A2AGQLTest.ps1 "DR"
It disables the protection on the Primary VM and removes all the azure artifacts created in the earlier operations.

.EXAMPLE
./A2AGQLTest.ps1 "CR"
Removes all the mappings and azure recovery resources

.EXAMPLE
./A2AGQLTest.ps1 "RSP"
It triggers Reverse Switch Protection ( Primary -> Recovery )

.EXAMPLE
./A2AGQLTest.ps1 "APPLYLOAD"
Generates churn on agent

.EXAMPLE
./A2AGQLTest.ps1 "ISSUETAG"
Issues custom tag on agent and gets tag insert timestamp.

.EXAMPLE
./A2AGQLTest.ps1 "VERIFYLOAD"
Validates the generated data on agent machine and returns the status.

.EXAMPLE
./A2AGQLTest.ps1 "Uninstall"
Uninstalls the mobility agent from source VM and reboots it.

.EXAMPLE
./A2AGQLTest.ps1 "UPLOAD"
Uploads the required DI tool utilities to azure storage account

.EXAMPLE
./A2AGQLTest.ps1 "COPY"
Copies the required DI tool utilities to source machine

.EXAMPLE
./A2AGQLTest.ps1 "CLEANPREVIOUSRUN"
# This operation disables and remove the protection left(if any) at the start of the run for the configuration being passed

.NOTES
Make sure you update the A2AGQLConfig.xml file before running this script. Update the config file with valid Azure Artifacts like Primary Resource Group and Storage accounts.
#>

# Input parameters
param (
    [Parameter(Mandatory=$true)]
    [ValidateSet("COPY", "LOGIN", "ER", "TFO", "UFO", "SP", "FB", "DR", "RSP", "CR", "ALL", "REBOOT", "UNINSTALL", "APPLYLOAD", "ISSUETAG", "VERIFYLOAD", "UPLOAD","ATTACHDISK","CREATEDISKLAYOUT", "CLEANPREVIOUSRUN", "RUN540", "SETVAULTCONTEXT", "HELP")] 
    [String] $Operation,
    
    [Parameter(Mandatory=$false)]
    [String] $ResourceGroupType="Primary"
    
    # This is used for any operations with in the Azure guest VM
    #[parameter(Mandatory=$False)]
    #[string]$useAzureExtensionForLinux = "true"
)

#
# Mapping between A2A GQL "operation" to "column" seen in the reporting table 
#
$ReportOperationMap = @{"ER" = "ER_180"}
$ReportOperationMap.Set_Item("UFO", "FO_180")
$ReportOperationMap.Set_Item("TFO", "TFO")
$ReportOperationMap.Set_Item("SP", "ER_360")
$ReportOperationMap.Set_Item("FB", "FO_360")
$ReportOperationMap.Set_Item("RSP", "ER_540")
$ReportOperationMap.Set_Item("REBOOT", "Reboot")
$ReportOperationMap.Set_Item("DR", "DisableRep")
$ReportOperationMap.Set_Item("CR", "CleanRep")
$ReportOperationMap.Set_Item("CLEANPREVIOUSRUN", "CleanPreviousRun")
#
# Include common library files
#
if($Operation.ToLower() -ne 'help')
{
    $Error.Clear()
    . $PSScriptRoot\CommonFunctions.ps1
}

# Global Variables
$global:ContainerName = "scripts"
$global:CustomScriptFilesList = [System.Collections.ArrayList]@()
$global:Operation = $Operation.ToUpper()

#
# Prepare Azure Environment and Enable Protection
#
Function EnableReplication()
{
    try
    {
        LogMessage -Message ("Running Enable Replication Job on VM : $global:VMName") -LogType ([LogType]::Info1);
        
        # Wait for VM agent ProvisioningState=succeeded, VM ProvisioningState= Succeeded and VM PowerState=running
        WaitForAzureVMReadyState -vmName $global:VMName -resourceGroupName $global:PriResourceGroup

        # Create fabrics (primary & recovery)
        LogMessage -Message ("Creating Primary Fabric: $global:PrimaryFabricName") -LogType ([LogType]::Info1);
        $primaryFabricObject = CreateFabric -Name $global:PrimaryFabricName -Location $global:PrimaryLocation
        LogMessage -Message ("Creating Recovery Fabric: $global:RecoveryFabricName") -LogType ([LogType]::Info1);
        $recoveryFabricObject = CreateFabric -Name $global:RecoveryFabricName -Location $global:RecoveryLocation

        # Create Containers primary and recovery
        LogMessage -Message ("Creating Container for Primary Geo: $global:PrimaryContainerName") -LogType ([LogType]::Info1);
        $primaryContainerObject = CreateProtectionContainer -Name $global:PrimaryContainerName -Fabric $primaryFabricObject
        LogMessage -Message ("Creating Container for Recovery Geo: $global:RecoveryContainerName") -LogType ([LogType]::Info1);
        $recoveryContainerObject = CreateProtectionContainer -Name $global:RecoveryContainerName -Fabric $recoveryFabricObject
        
        # Create Replication Policy
        LogMessage -Message ("Triggering replication policy creation: {0}" -f $global:ReplicationPolicy) -LogType ([LogType]::Info1);
        $policyObject = CreateReplicationPolicy -Name $global:ReplicationPolicy -MultiVmSyncStatus 'Enable'
        
        # Create container mapping(Cloud Pairing) [primary -> recovery]
        LogMessage -Message ("Triggering Container Pairing: {0}" -f $global:ForwardContainerMappingName) -LogType ([LogType]::Info1);
        $forwardContainerMappingObject = CreateProtectionContainerMapping -Name $global:ForwardContainerMappingName `
                            -PrimaryProtectionContainer $primaryContainerObject -RecoveryProtectionContainer $RecoveryContainerObject -Policy $policyObject
        
        # Get updated container objects after container mapping
        $primaryContainerObject = GetProtectionContainer -Name $global:PrimaryContainerName -Fabric $primaryFabricObject
        $recoveryContainerObject = GetProtectionContainer -Name $global:RecoveryContainerName -Fabric $recoveryFabricObject
        
        # Get Source VM Details
        $azureArtifacts = GetAzureArtifacts
        $VMName = $azureArtifacts.Vm.Name
        $VMId = $azureArtifacts.Vm.Id
        
        # Network Mapping [primary -> recovery]
        LogMessage -Message ("Triggering Network Mapping: {0}" -f $global:ForwardNetworkMappingName) -LogType ([LogType]::Info1);
        $forwardNetworkMapping = CreateNetworkMapping -Name $global:ForwardNetworkMappingName -PrimaryAzureNetwork $azureArtifacts.PriVnet `
                                    -PrimaryFabric $primaryFabricObject -RecoveryAzureNetwork $azureArtifacts.RecVnet -RecoveryFabric $recoveryFabricObject
        LogMessage -Message ("*****Network Mapped*****") -LogType ([LogType]::Info2)

        # Enable Protection
        LogMessage -Message ("Triggering Enable Protection: {0} for VM Id: {1}" -f $VMName,$VmID) -LogType ([LogType]::Info1);

        
        ASREnableProtectionWithDataDisks -ProtectionContainer $primaryContainerObject  `
                            -ProtectionContainerMapping $forwardContainerMappingObject  `
                            -AzureArtifactsData $azureArtifacts
        
        LogMessage -Message ("****** Completed enabling protection for VMName : $VMName") -LogType ([LogType]::Info1);
    }
    catch
    {
        LogMessage -Message ("{0}:Enable failed for vm object rgname: {1}" -f $global:VMName, $global:PriResourceGroup) -LogType ([LogType]::Error)
        LogMessage -Message ("{0}: Exception string : {1}" -f $global:VMName, $_.Exception) -LogType ([LogType]::Error)
        Throw
    }
}

#
# Triggers Test Failover for source VM
#
Function TestFailover
{
    LogMessage -Message ("Running Test Failover Job on VM : $global:VMName") -LogType ([LogType]::Info1);
    
    # Get protected VM Item
    $protectedItemObject = GetProtectedItemObject -fabricName $global:PrimaryFabricName -containerName $global:PrimaryContainerName

    # Get Source VM details
    $azureArtifacts = GetAzureArtifacts

    # Get recovery point
    [PSObject]$recoveryPoint = GetRecoveryPoint -protectedItemObject $protectedItemObject -tagType "Latest"

    # Test Failover
    ASRTestFailover -ReplicationProtectedItem $protectedItemObject `
                    -CurrentSourceResourceGroup $global:PriResourceGroup `
                    -CurrentTargetResourceGroup $global:RecResourceGroup `
                    -TestFailoverNetwork $azureArtifacts.RecVnet `
                    -Direction "PrimaryToRecovery" `
                    -RecoveryPoint $recoveryPoint
                    

    # Test Failover Cleanup
    ASRTestFailoverCleanup -ReplicationProtectedItem $protectedItemObject
}

#
# Trigger Unplanned Failover on Source VM
#
Function UnPlannedFailover
{
    LogMessage -Message ("Running Unplanned Failover Job on VM : $global:VMName") -LogType ([LogType]::Info1);

    # Get Protected Item
    $protectedItemObject = GetProtectedItemObject -fabricName $global:PrimaryFabricName -containerName $global:PrimaryContainerName
    
    # Get recovery point
    [PSObject]$recoveryPoint = GetRecoveryPoint -protectedItemObject $protectedItemObject -tagType "Latest"
    
    LogObject -Object $recoveryPoint

    <#
    # UnPlanned Failover (UFO)
    LogMessage -Message ("Triggering UFO for VM: {0}, Id: {1}" -f $protectedItemObject.FriendlyName, $protectedItemObject.Name) -LogType ([LogType]::Info1)
    
    ASRUnplannedFailoverWithRecoveryPoint -ReplicationProtectedItem $protectedItemObject `
                                            -CurrentSourceResourceGroup $global:PriResourceGroup `
                                            -CurrentTargetResourceGroup $global:RecResourceGroup `
                                            -Direction "PrimaryToRecovery" `
                                            -RecoveryPoint $recoveryPoint
    #>

    # UnPlanned Failover (UFO)
    LogMessage -Message ("Triggering UFO for VM: {0}, Id: {1}" -f $protectedItemObject.FriendlyName, $protectedItemObject.Name) -LogType ([LogType]::Info1)
    ASRUnplannedFailoverWithRecoveryTag -ReplicationProtectedItem $protectedItemObject `
                            -CurrentSourceResourceGroup $global:PriResourceGroup `
                            -CurrentTargetResourceGroup $global:RecResourceGroup `
                            -Direction "PrimaryToRecovery" `
                            -RecoveryTag "LatestAvailableCrashConsistent"

    CommitFailover -ReplicationProtectedItem $protectedItemObject
    
    LogMessage -Message ("***** UFO Completed *****") -LogType ([LogType]::Info2)
}

#
# Trigger Switch Protection
#
Function SwitchProtection
{
    LogMessage -Message ("Triggering Switch Protection Job on VM : $global:VMName") -LogType ([LogType]::Info1);

    # Get Source VM details
    $azureArtifacts = GetAzureArtifacts
    
    # Commit after UFO
    $protectedItemObject = GetProtectedItemObject -fabricName $global:PrimaryFabricName -containerName $global:PrimaryContainerName

    # Create Reverse Container Mapping(Cloud Pairing) [recovery -> primary]
    LogMessage -Message ("Triggering Reverse Container Mapping(Cloud Pairing) [recovery -> primary]::{0}" -f $global:ReverseContainerMappingName) -LogType ([LogType]::Info1);
    $policyObject = Get-AzRecoveryServicesAsrPolicy -Name $global:ReplicationPolicy
    LogMessage -Message ("Replication policy: {0} fetched successfully." -f $policyObject.Name) -LogType ([LogType]::Info1);

    # Get Primary and Recovery Fabrics
    $primaryFabricObject = GetFabricObject -fabricName $global:PrimaryFabricName
    $recoveryFabricObject = GetFabricObject -fabricName $global:RecoveryFabricName
    
    # Get updated container objects after container mapping
    $primaryContainerObject = GetProtectionContainer -Name $global:PrimaryContainerName -Fabric $primaryFabricObject
    $recoveryContainerObject = GetProtectionContainer -Name $global:RecoveryContainerName -Fabric $recoveryFabricObject
    
    # Get reverse ContainerMapping Object
    LogMessage -Message ("Triggering Reverse Container Pairing: {0}" -f $global:ReverseContainerMappingName) -LogType ([LogType]::Info1);
    $reverseContainerMappingObject = CreateProtectionContainerMapping -Name $global:ReverseContainerMappingName `
                                        -PrimaryProtectionContainer $recoveryContainerObject -RecoveryProtectionContainer $primaryContainerObject -Policy $policyObject

    # Reverse Network Mapping [recovery -> primary]
    LogMessage -Message ("Triggering Reverse Network Mapping: {0}" -f $global:ReverseContainerMappingName) -LogType ([LogType]::Info1)
    $reverseNetworkMapping = CreateNetworkMapping -Name $global:ReverseContainerMappingName -PrimaryAzureNetwork $azureArtifacts.RecVnet `
                                -PrimaryFabric $recoveryFabricObject -RecoveryAzureNetwork $azureArtifacts.PriVnet -RecoveryFabric $primaryFabricObject
    LogMessage -Message ("*****Reverse Network Mapped*****") -LogType ([LogType]::Info2)

    Start-Sleep -Seconds ($JobQueryWaitTime60Seconds * 5)
    
    LogMessage -Message ("Triggering Switch protection (recovery -> primary): vm : {0}" -f $global:VMName) -LogType ([LogType]::Info1);
    ASRSwitchProtectionWithDataDisks -ProtectionContainer $recoveryContainerObject `
                                        -ProtectionContainerMapping $reverseContainerMappingObject  `
                                        -ReplicationProtectedItem $protectedItemObject  `
                                        -AzureArtifactsData $azureArtifacts `
                                        -Direction RecoveryToPrimary
    LogMessage -Message ("***** Switch Protection Completed *****") -LogType ([LogType]::Info2)
}

#
# Trigger Failback (Reverse UFO)
#
Function Failback
{
    LogMessage -Message ("Running Failback Job on VM : $global:VMName") -LogType ([LogType]::Info1);

    # Get reverse protected object
    $reverseProtectedItemObject = GetProtectedItemObject -fabricName $global:RecoveryFabricName -containerName $global:RecoveryContainerName
    LogObject -Object $reverseProtectedItemObject

    # Get recovery point
    [PSObject]$recoveryPoint = GetRecoveryPoint -protectedItemObject $reverseProtectedItemObject -tagType "Latest"
    
    LogObject -Object $recoveryPoint

    # Reverse UnPlanned Failover (UFO)
    LogMessage -Message ("Triggering Failback for VM: {0}, Id: {1}" -f $reverseProtectedItemObject.FriendlyName, $reverseProtectedItemObject.Name) -LogType ([LogType]::Info1)
    ASRUnplannedFailoverWithRecoveryTag -ReplicationProtectedItem $reverseProtectedItemObject `
                            -CurrentSourceResourceGroup $global:RecResourceGroup `
                            -CurrentTargetResourceGroup $global:PriResourceGroup `
                            -Direction "PrimaryToRecovery" `
                            -RecoveryTag "LatestAvailableApplicationConsistent"

    LogMessage -Message ("***** Failback Completed *****") -LogType ([LogType]::Info2)
}

#
# Triggers Reverse Switch Protection
#
Function ReverseSwitchProtection
{
    # Get Source VM details
    $azureArtifacts = GetAzureArtifacts
    
    LogMessage -Message ("Sleeping for {0} sec" -f ($JobQueryWaitTime60Seconds * 2)) -LogType ([LogType]::Info1);
    Start-Sleep -Seconds ($JobQueryWaitTime60Seconds * 2)
    
    $primaryFabricObject = GetFabricObject -fabricName $global:PrimaryFabricName
    
    $primaryContainerObject = GetProtectionContainer -Name $global:PrimaryContainerName -Fabric $primaryFabricObject
    LogMessage -Message ("Primary Container: {0}, Role: {1}, AvailablePolicies: {2}" -f $primaryContainerObject.FriendlyName, `
                            $primaryContainerObject.Role, $primaryContainerObject.AvailablePolicies.FriendlyName ) -LogType ([LogType]::Info1);

    # Get reverse ContainerMapping Object
    LogMessage -Message ("Fetching Primary Container Pairing: {0}" -f $global:ForwardContainerMappingName) -LogType ([LogType]::Info1);
    $forwardContainerMappingObject = Get-AzRecoveryServicesAsrProtectionContainerMapping -Name $global:ForwardContainerMappingName -ProtectionContainer $primaryContainerObject
    
    # Get reverse protected object
    $reverseProtectedItemObject = GetProtectedItemObject -fabricName $global:RecoveryFabricName -containerName $global:RecoveryContainerName
    
    LogMessage -Message ("Triggering Reverse Switch protection (primary -> recovery): {0}" -f $reverseSwitchProtection) -LogType ([LogType]::Info1);
    ASRSwitchProtectionWithDataDisks -ProtectionContainer $primaryContainerObject  `
                                        -ProtectionContainerMapping $forwardContainerMappingObject  `
                                        -ReplicationProtectedItem $reverseProtectedItemObject  `
                                        -AzureArtifactsData $azureArtifacts `
                                        -Direction PrimaryToRecovery
    LogMessage -Message ("***** Reverse Switch Protection Completed *****") -LogType ([LogType]::Info2)
}

#
# Triggers Disable Replication 
#
Function DisableReplication
{
    # Remove Networkmapping
    LogMessage -Message ("Triggering Primary->Recovery Network Un-Mapping") -LogType ([LogType]::Info1)
    $primaryFabricObject = GetFabricObject -fabricName $global:PrimaryFabricName -throwIfNotFound $false
    if ($null -ne $PrimaryFabricObject) {
        DeleteNetworkMapping -FabricObject $primaryFabricObject
    }

    LogMessage -Message ("Triggering Recovery->Primary Network Un-Mapping") -LogType ([LogType]::Info1)
    $recoveryFabricObject = GetFabricObject -fabricName $global:RecoveryFabricName -throwIfNotFound $false
    if ($null -ne $PrimaryFabricObject) {
        DeleteNetworkMapping -FabricObject $recoveryFabricObject
    }

    if ($null -ne $PrimaryFabricObject) {
        # Disable Protection(Forward)
        LogMessage -Message ("Triggering Disable Protection(Forward) for VM: {0}, RG : {1}" -f $global:VMName,$global:PriResourceGroup) -LogType ([LogType]::Info1)
        DisableProtectionForFabric -fabricName $global:PrimaryFabricName -containerName $global:PrimaryContainerName
    }

    if ($null -ne $recoveryFabricObject) {
        # Disable Protection(Switch Protection)
        LogMessage -Message ("Triggering Disable Protection(Switch Protection) for VM: {0}, RG : {1}" -f $global:VMName,$global:RecResourceGroup) -LogType ([LogType]::Info1)
        DisableProtectionForFabric -fabricName $global:RecoveryFabricName -containerName $global:RecoveryContainerName
    }
}

#
# Cleanup Azure Artifacts
#
Function CleanupResources
{
    LogMessage -Message ("*****Removing ASR resources*****") -LogType ([LogType]::Info1)

    $primaryFabricObject = GetFabricObject -fabricName $global:PrimaryFabricName -throwIfNotFound $false -ErrorAction SilentlyContinue 
    # Remove Protection Container Mappings
    if ($null -ne $primaryFabricObject) {
        DeleteProtectionContainerMapping -ContainerName $global:PrimaryContainerName -ContainerMappingName $global:ForwardContainerMappingName -FabricObject $primaryFabricObject
    }
    $recoveryFabricObject = GetFabricObject -fabricName $global:RecoveryFabricName -throwIfNotFound $false -ErrorAction SilentlyContinue
    if ($null -ne $recoveryFabricObject) {
        DeleteProtectionContainerMapping -ContainerName $global:RecoveryContainerName -ContainerMappingName $global:ReverseContainerMappingName -FabricObject $recoveryFabricObject
    }

    #$primaryFabricObject = GetFabricObject -fabricName $global:PrimaryFabricName -throwIfNotFound $false -ErrorAction SilentlyContinue
    # Remove Protection Containers
    if ($null -ne $primaryFabricObject) {
        DeleteProtectionContainer -ContainerName $global:PrimaryContainerName -FabricObject $primaryFabricObject
    }

    #$recoveryFabricObject = GetFabricObject -fabricName $global:RecoveryFabricName -throwIfNotFound $false -ErrorAction SilentlyContinue
    if ($null -ne $recoveryFabricObject) {
        DeleteProtectionContainer -ContainerName $global:RecoveryContainerName -FabricObject $recoveryFabricObject
    }

    # Remove Policy
    DeleteReplicationPolicy -PolicyName $global:ReplicationPolicy

    # Remove Fabrics
    if ($null -ne $PrimaryFabricObject) {
        DeleteFabric -Fabric $primaryFabricObject
    }

    if ($null -ne $recoveryFabricObject) {
        DeleteFabric -Fabric $recoveryFabricObject
    }
    
    LogMessage -Message ("*****Removed ASR resources*****") -LogType ([LogType]::Info1)

    CleanAzureArtifacts
}

#
# Restart Azure VM
#
Function RebootAzureVM
{
    $rgName = $global:PriResourceGroup
    if($ResourceGroupType.ToLower() -ne "primary")
    {
        $rgName = $global:RecResourceGroup
    }
    
    Restart-AzVM -ResourceGroupName $rgName -Name $global:VMName
    if ( !$? )
    {
        $ErrorMessage = ('Failed to restart VM : {0} under RG {1}' -f $global:VMName, $rgName)
        LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
        throw $ErrorMessage
    }
    
    # Check Source VM Status
    WaitForAzureVMReadyState -vmName $global:VMName -resourceGroupName $rgName
    
    LogMessage -Message ("Successfully rebooted VM : {0}" -f $global:VMName) -LogType ([LogType]::Info1)
}

#
# Disable protection and delete Azure artifacts
# Set vault context only if actual disable protection started
#
Function DisableAndDeleteAzureArtifacts
{
    $isVaultPresent = $false
    $Vault = $null

    #Check whether recovery services vault exists or not
	LogMessage -Message ("DisableAndDeleteAzureArtifacts: VaultRGName={0}, VaultRGLocation={1}" -f $global:VaultRGName, $global:VaultRGLocation) -LogType ([LogType]::Info1)
    $rg = Get-AzResourceGroup -Name $global:VaultRGName -Location $global:VaultRGLocation -ErrorAction SilentlyContinue
    if ($rg -ne $null)
    { 
        # Vault RG exists
        $Vault = Get-AzRecoveryServicesVault -ResourceGroupName $global:VaultRGName -Name $global:VaultName -ErrorAction SilentlyContinue
        if ($vault -ne $null)
        { 
            # Vault exists
            LogMessage -Message ('Vault: {0} exists.' -f $global:VaultName) -LogType ([LogType]::Info1)
            $isVaultPresent = $true
        } 
        else
        {
            LogMessage -Message ('Vault: {0} does not exist.' -f $global:VaultName) -LogType ([LogType]::Info1)
        }
    }

    if ((!$isVaultPresent) -AND ($rg -ne $null)) 
    {
        LogMessage -Message ('Removing any Azure Artifacts created as part of previous run' -f $global:VaultName) -LogType ([LogType]::Info1)
        CleanAzureArtifacts
    }
    else 
    {
        # Set vault context.
        $vault = Get-AzRecoveryServicesVault -ResourceGroupName $global:VaultRGName -Name $global:VaultName
        if ($null -ne $vault)
        {
            LogObject -Object $vault
            Set-AzRecoveryServicesAsrVaultContext -Vault $vault
            if (!$?) {
                LogMessage -Message ('Failed to import vault : {0}' -f $global:VaultName) -LogType ([LogType]::Error)
                Throw "Failed to import vault $global:VaultName"
            }
            LogMessage -Message ('Removing both ASR and Azure Artifacts left as part of last run(if any)' -f $global:VaultName) -LogType ([LogType]::Info1)
            ASRCheckAndRemoveFabricsNContainers
            DisableReplication
            CleanupResources
        }
        else {
            LogMessage -Message ('Vault : {0} object is null' -f $global:VaultName) -LogType ([LogType]::Error)
        }
    }
}


#
# Fetch Azure Artifacts
#
Function GetAzureArtifacts
{
    # Get VM Details
    if(($ResourceGroupType.ToLower() -eq "recovery") -AND ($Operation -ne "TFO"))
    {
        $azureVm = Get-AzVM -ResourceGroupName $global:RecResourceGroup -Name $VMName
    }
    else
    {
        $azureVm = Get-AzVM -ResourceGroupName $global:PriResourceGroup -Name $VMName
    }
    LogMessage -Message ("Azure VM :{0}" -f $azureVm) -LogType ([LogType]::Info1)

    $priRgObject = Get-AzResourceGroup -Name $global:PriResourceGroup -Location $global:PrimaryLocation 
    LogMessage -Message ("Primary Resource Group Object:{0}" -f $priRgObject) -LogType ([LogType]::Info1)

    $recRgObject = Get-AzResourceGroup -Name $global:RecResourceGroup -Location $global:RecoveryLocation -ErrorAction SilentlyContinue
    if($recRgObject -eq $null)
    {
        $recRgObject = New-AzResourceGroup -Name $global:RecResourceGroup -Location $global:RecoveryLocation
        AssignTagToRG -RGName $global:RecResourceGroup
    }
    LogMessage -Message ("Recovery Resource Group Object:{0}" -f $recRgObject) -LogType ([LogType]::Info1)

    $recSaObject = Get-AzStorageAccount -Name $global:RecStorageAccount -ResourceGroupName $global:RecResourceGroup -ErrorAction SilentlyContinue
    if($recSaObject -eq $null)
    {
        LogMessage -Message ("Creating Recovery Storage Account Object") -LogType ([LogType]::Info1)
        if($global:IsPremiumDR.ToLower() -eq "true")
        {
            $recSaObject = New-AzStorageAccount -Name $global:RecStorageAccount -ResourceGroupName $global:RecResourceGroup -Location $global:RecoveryLocation -Kind "BlockBlobStorage" -SkuName "Premium_LRS"
        }
        else
        {
            $recSaObject = New-AzStorageAccount -Name $global:RecStorageAccount -ResourceGroupName $global:RecResourceGroup -Type 'Standard_LRS' -Location $global:RecoveryLocation
        }
    }
    else
    {
        LogMessage -Message ("Recovery Storage Account : {0} already exist. Skipping this." -f $global:RecStorageAccount) -LogType ([LogType]::Info1)
    }
    LogMessage -Message ("Recovery Storage Account Object:{0}" -f $recSaObject) -LogType ([LogType]::Info1)
    
    $stagingSaObject = Get-AzStorageAccount -Name $global:StagingStorageAcc -ResourceGroupName $global:PriResourceGroup -ErrorAction SilentlyContinue
    if($stagingSaObject -eq $null)
    {
        LogMessage -Message ("Creating Staging Storage Account Object") -LogType ([LogType]::Info1)
        if($global:IsPremiumDR.ToLower() -eq "true")
        {
            $stagingSaObject = New-AzStorageAccount -Name $global:StagingStorageAcc -ResourceGroupName $global:PriResourceGroup -Location $global:PrimaryLocation -Kind "BlockBlobStorage" -SkuName "Premium_LRS"
        }
        else
        {
            $stagingSaObject = New-AzStorageAccount -Name $global:StagingStorageAcc -ResourceGroupName $global:PriResourceGroup -Type 'Standard_LRS' -Location $global:PrimaryLocation
        }
    }
    else
    {
        LogMessage -Message ("Staging Storage Account : {0} already exist. Skipping this." -f $global:StagingStorageAcc) -LogType ([LogType]::Info1)
    }
    LogMessage -Message ("Staging Storage Account Object:{0}" -f $stagingSaObject) -LogType ([LogType]::Info1)
    
    $subnetRange = '10.0.0.0/24'
    $range = '10.0.0.0/16'
    $priVnetObject = Get-AzVirtualNetwork -Name $global:PrimaryVnet -ResourceGroupName $global:PriResourceGroup
    LogMessage -Message ("Primary Vnet Object:{0}" -f $priVnetObject) -LogType ([LogType]::Info1)

    # Get VNet
    $recVnetObject  = Get-AzVirtualNetwork -Name $global:RecoveryVnet -ResourceGroupName $global:RecResourceGroup -ErrorAction SilentlyContinue
    if($RecVnetObject -eq $null)
    {
        $recSubnetConfig = New-AzVirtualNetworkSubnetConfig -Name 'default' -AddressPrefix $subnetRange
        $recVnetObject = New-AzVirtualNetwork -Name $global:RecoveryVnet -ResourceGroupName $global:RecResourceGroup -Location $global:RecoveryLocation -AddressPrefix $range -Subnet $recSubnetConfig
        LogMessage -Message ("Recovery Vent Object:{0}" -f $recVnetObject) -LogType ([LogType]::Info1)
    }
    else
    {
        LogMessage -Message ("vnet: {0} already exist. Skipping this." -f $global:RecoveryVnet) -LogType ([LogType]::Info1)
    }
    
    LogObject -Object $priRgObject

    LogObject -Object $recRgObject

    LogObject -Object $priSaObject

    LogObject -Object $recSaObject

    LogObject -Object $stagingSaObject

    LogObject -Object $priVnetObject

    LogObject -Object $recVnetObject

    [hashtable]$AzureArtifactsData = @{}
    $AzureArtifactsData.Vm = $azureVm;
    $AzureArtifactsData.PriResourceGroup = $priRgObject;
    $AzureArtifactsData.RecResourceGroup = $recRgObject;
    $AzureArtifactsData.PriStorageAcc = $priSaObject;
    $AzureArtifactsData.RecStorageAcc = $recSaObject;
    $AzureArtifactsData.StagingStorageAcc = $stagingSaObject;
    $AzureArtifactsData.PriVnet = $priVnetObject;
    $AzureArtifactsData.RecVnet = $recVnetObject;
    
    DisplayAzureArtifacts -azureArtifacts $AzureArtifactsData

    LogObject -Object $AzureArtifactsData

    return $AzureArtifactsData
}

#
# Create Zip Folder of PS Test Binaries
#
function CreateZip()
{
    $dir = $PSScriptRoot + "\DITestBinaries"
    $zipFile = $TestBinRoot + "\DITestBinaries.zip"
    LogMessage -Message ("Creating Zip File : $zipFile") -LogType ([LogType]::Info1)
    If(Test-path $zipFile) {Remove-item $zipFile}
    Add-Type -assembly "system.io.compression.filesystem"
    [io.compression.zipfile]::CreateFromDirectory($dir, $zipFile)
    if(!$?)
    {
        LogMessage -Message ("Failed to create Zip File : $zipFile") -LogType ([LogType]::Error)
        exit 1
    }
    LogMessage -Message ("Successfully created zip file : $zipFile") -LogType ([LogType]::Info1)
}

#
# Downloads the blob from azure storage account and delete it from azure
#
function DownloadResults
{
    param (
        [parameter(Mandatory=$True)] [string]$storageAccountName,
        [parameter(Mandatory=$True)] [string]$storageKey,
        [parameter(Mandatory=$True)] [string]$blobName
    )
    
    $resultFile = $TestBinRoot + "\" + $blobName 
        
    if(Test-Path $resultFile)
    {
        [string]$directory = [System.IO.Path]::GetDirectoryName($resultFile);
        [string]$strippedFileName = [System.IO.Path]::GetFileNameWithoutExtension($resultFile);
        [string]$extension = [System.IO.Path]::GetExtension($resultFile);
        [string]$newFileName = $strippedFileName + "-" + [DateTime]::Now.ToString("yyyyMMdd-HHmmss") + $extension;
        [string]$newFilePath = [System.IO.Path]::Combine($LogDir, $newFileName);

        Move-Item -LiteralPath $resultFile -Destination $newFilePath;
        if ($? -eq "True")
        {
            LogMessage -Message ("Renamed resultFile to $newFilePath successfully") -LogType ([LogType]::Info1)
        }
        else
        {
            LogMessage -Message ("Failed to rename $resultFile to $newFilePath") -LogType ([LogType]::Error)
            exit 1
        }
    }
    
    $storageContext = New-AzureStorageContext -StorageAccountName $storageAccountName -StorageAccountKey $storageKey
    
    Get-AzureStorageBlobContent -Blob $blobName -Container $ContainerName -Destination $TestBinRoot -Context $storageContext
    if( !$? )
    {
        LogMessage -Message ("Failed to download blob $blobName from azure storage account") -LogType ([LogType]::Error)
        exit 1
    }
    
    LogMessage -Message ("Successfully downloaded $blobName of VM : $VMName") -LogType ([LogType]::Info1)
    
    Start-Sleep -seconds 60
    
    if(Test-Path $blobName)
    {
        Remove-AzureStorageBlob -Blob $blobName -Container $ContainerName -Context $storageContext
        if( !$? )
        {
            LogMessage -Message ("Failed to delete blob : $blobName from azure storage account") -LogType ([LogType]::Error)
        }
    }
}

#
# Copy all the test executables to source machine
#
Function CopyDITools()
{
    try
    {
        # create zip file for di test tools
        CreateZip
        
        UploadFiles
        
        # Run scripts on VM
        $VMInfo = GetVMDetails
        if( $global:OSType -eq "windows")
        {
            $scriptName = "CopyDITools.ps1"
            $args = $Env:SystemDrive + "\TestBinRoot"
                    SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
        -storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName -arguments $args
        }
    }
    Catch
    {
        LogMessage -Message ("Failed to copy di test binaries to source VM {0}" -f $VMName) -LogType ([LogType]::Error)
        LogMessage -Message ("CopyDITools : ERROR : {0}" -f $_.Exception.Message) -LogType ([LogType]::Error)
        exit 1
    }
}

#
# Generate churn on source machine
#
#Function GenerateChurn()
#{
#    try
    #{
    #   $VMInfo = GetVMDetails
    #   [string] $key = $VMInfo.StorageKey
    #   if( $global:OSType -eq "windows")
    #   {
    #       $scriptName = "ChurnGenerator.ps1"
    #       $blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
    #       $args = "$blobUrl $key ChurnData $false $TotalChurnSize $ChurnFileSize"
    #   }else
    #   {
    #       $scriptName = "GenerateTestTree.sh"
    #       $blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
    #       $args = "/mnt/lv3G/data,/mnt/lv4G/data 2 300 0"
    #       LogMessage -Message ("GenerateChurn : INFO : {0}" -f $args) -LogType ([LogType]::Info1)
    #   }
    #
    #   # Run GenerateChurn.ps1 on Source VM and upload the tag inserted timestamp to storage account
    #   SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
    #   -storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName -arguments $args
    #}
    #Catch
    #{
    #   LogMessage -Message ("Failed to generate data on VM {0}" -f $VMName) -LogType ([LogType]::Error)
    #   LogMessage -Message ("GenerateChurn : ERROR : {0}" -f $_.Exception.Message) -LogType ([LogType]::Error)
    #   exit 1
    #}
#}

Function CreateDiskLayout()
{
    try
    {
        $VMInfo = GetVMDetails
        [string] $key = $VMInfo.StorageKey
        if( $global:OSType -eq "windows")
        {
            # $scriptName = "ChurnGenerator.ps1"
            # $blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
            # $args = "$blobUrl $key ChurnData $false $TotalChurnSize $ChurnFileSize"
        }else
        {
            $scriptName = "partitioningcmd.sh"
            $blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
        }
        
        # Run partitioningcmd.sh on Source VM to create VM layout
        SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
        -storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName
    }
    Catch
    {
        LogMessage -Message ("Failed to CreateDiskLayout on VM {0}" -f $VMName) -LogType ([LogType]::Error)
        LogMessage -Message ("CreateDiskLayout : ERROR : {0}" -f $_.Exception.Message) -LogType ([LogType]::Error)
        exit 1
    }
}

#
# Issue tag on Source VM and upload the tag timestamp to blob
#
Function IssueTag()
{
    try
    {
        $VMInfo = GetVMDetails
        [string] $key = $VMInfo.StorageKey
            
        if( $global:OSType -eq "windows")
        {
            $scriptName = "IssueTag.ps1"
            $blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
            $args = "$blobUrl $key app"
        }
        else
         {
             $scriptName = "IssuetagAndUploadToStorage.sh"
             $blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
             $strName = $VMInfo.StorageAccount
             LogMessage -Message ("IssueTag script is downloading from str: INFO : {0}" -f $strName) -LogType ([LogType]::Info1)
             $args = "MyCustomTag $strName $key $global:ContainerName"
             LogMessage -Message ("IssueTag : INFO : {0}" -f $args) -LogType ([LogType]::Info1)
         }
        
        # Run IssueTag.ps1 on Source VM and upload the tag inserted timestamp to storage account
        SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
        -storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName -arguments $args
        
        # Download the insertedtagtimestamp file from storage account
        DownloadResults -storageAccountName $VMInfo.StorageAccount -storageKey $VMInfo.StorageKey -blobName 'taginserttimestamp.txt'
    }
    Catch
    {
        $ErrorMessage = ('Failed to issue tag on VM {0} with error {1}' -f $VMName, $_.Exception.Message)
        LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
        throw $ErrorMessage
    }
}

#
# Validate churn
#
Function ValidateChurn()
{
    try
    {
        $VMInfo = GetVMDetails
        [string] $key = $VMInfo.StorageKey
            
        if( $global:OSType -eq "windows")
        {
            $scriptName = "ChurnGenerator.ps1"
            $blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
            $args = "$blobUrl $key ChurnData $true"
        }
        else
        {
            $scriptName = "VerifyLoad.sh"
            $blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
            $args = "/mnt/lv3G/data,/mnt/lv4G/data"
        }
        
        $retrycount = 1
        $result = "false"
        while($retrycount -le 3 -and $result -eq "false")
        {
            try
            {
                LogMessage -Message ("Started validate data on VM {0} , retrycount:{1}" -f $VMName, $retrycount) -LogType ([LogType]::Info1)
                # Run ValidateChurn.ps1 on Source VM and upload the results to storage account
                SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
                -storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName -arguments $args
                
                $result = "true"
            }
            catch
            {
                LogMessage -Message ("Failed to validate data on VM {0} on retrycount:{1}" -f $VMName, $retrycount) -LogType ([LogType]::Error)
                $retrycount=$retrycount + 1
            }
            
        }
        if($result -eq "false")
        {
            LogMessage -Message ("Failed to validate data on VM {0} on retrycount:{1}. Exiting" -f $VMName, $retrycount) -LogType ([LogType]::Error)
            exit 1
        }
        # Download the DI test results file from storage account
        #DownloadResults -storageAccountName $VMInfo.StorageAccount -storageKey $VMInfo.StorageKey -blobName 'DITestResult.txt'
    }
    Catch
    {
        LogMessage -Message ("Failed to validate data on VM {0}" -f $VMName) -LogType ([LogType]::Error)
        LogMessage -Message ("ValidateChurn : ERROR : {0}" -f $_.Exception.Message) -LogType ([LogType]::Error)
        exit 1
    }
}

function MountAttachedDataDisks
{
    try
    {
        $VMInfo = GetVMDetails
        [string] $key = $VMInfo.StorageKey
            
        if( $global:OSType -eq "windows")
        {
            # $scriptName = "ChurnGenerator.ps1"
            # $blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
            # $args = "$blobUrl $key ChurnData $true"
        }
        else
        {
            $scriptName = "MountLinuxDataDisks.sh"
            $blobURL = "https://" + $VMInfo.StorageAccount + ".blob.core.windows.net/" + $global:ContainerName
        }
        
        # Run ValidateChurn.ps1 on Source VM and upload the results to storage account
        SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
        -storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName
    }
    Catch
    {
        LogMessage -Message ("Failed to mount data datadisks on VM {0}" -f $VMName) -LogType ([LogType]::Error)
        LogMessage -Message ("MountAttachedDataDisks : ERROR : {0}" -f $_.Exception.Message) -LogType ([LogType]::Error)
        exit 1
    }
}
function AttachDataDisk
{
    try
    {
        UploadFiles
        
        $VMInfo = GetVMDetails
        $rgName = $VMInfo.ResourceGroup
        $vmName = $VMName
        $location = $VMInfo.Location 
        $VirtualMachine = Get-AzVM -ResourceGroupName $VMInfo.ResourceGroup -Name $VMName
        if($IsManagedDisk -ne "true")
        {
            $storAccName = $VirtualMachine.StorageProfile.OsDisk.Vhd.Uri.Split("/")[2].Split(".")[0]
            $storacct = Get-AzStorageAccount -ResourceGroupName $rgName -StorageAccountName $storAccName
            LogMessage -Message ("Attaching data disks on non-manageddisk type VM:{0} by fetching the data disk from StorageAccount:{1}, ResourceGroup:{2}, Location:{3}" -f $VMName, $storAccName, $rgName, $location) -LogType ([LogType]::Info1)
        }
        else
        {
            LogMessage -Message ("Attaching data disks on ManagedDisk type VM:{0} by fetching the data disk from ResourceGroup:{1}, Location:{2}" -f $VMName, $rgName, $location) -LogType ([LogType]::Info1)
        }
        
        $index = 10
        $diskSizes = (7,8)
        for($i=1; $i -le 2; $i++)
        {
            $disknameData = $vmName + '-' + 'DataDisk' + '-' + $i
            $pos = $i - 1
            if($IsManagedDisk -ne "true")
            {
                $dataDiskVhdUri = $storacct.PrimaryEndpoints.Blob.ToString() + "vhds/$disknameData.vhd"
                LogMessage -Message ("Starting attaching data disk:{0} " -f $dataDiskVhdUri) -LogType ([LogType]::Info1)
                $newVM = Add-AzVMDataDisk -VM $VirtualMachine -VhdUri $dataDiskVhdUri -Lun $index -CreateOption Attach -Caching ReadWrite -Name $disknameData -DiskSizeInGB $diskSizes[$pos]
                LogMessage -Message ("Successfully attached data disk:{0} " -f $dataDiskVhdUri) -LogType ([LogType]::Info1)
            }
            else
            {
                LogMessage -Message ("Fetching data disks:{0} " -f $disknameData) -LogType ([LogType]::Info1)
                $disk = Get-AzDisk -ResourceGroupName $rgName -DiskName $disknameData 
                LogMessage -Message ("Successfully fetched datadisk:{0}" -f $disknameData) -LogType ([LogType]::Info1)
                LogMessage -Message ("Starting attaching Managed data disk of ID:{0} " -f $disk.Id) -LogType ([LogType]::Info1)
                $newVM = Add-AzVMDataDisk -CreateOption Attach -Lun $index -VM $VirtualMachine -ManagedDiskId $disk.Id -Caching ReadWrite
                LogMessage -Message ("Successfully attached Managed data disk of ID:{0} " -f $disk.Id) -LogType ([LogType]::Info1)
            }
            $index = $index + 1
        }
        
        $diskSizes2 = (9,10)
        $index = 0
        for($i=3; $i -le 4; $i++)
        {
            $pos = $i - 3
            $disknameData = $vmName + '-' + 'DataDisk' + '-' + $i
            if($IsManagedDisk -ne "true")
            {
                $dataDiskVhdUri = $storacct.PrimaryEndpoints.Blob.ToString() + "vhds/$disknameData.vhd"
                LogMessage -Message ("Starting attaching data disk:{0} " -f $dataDiskVhdUri) -LogType ([LogType]::Info1)
                $newVM = Add-AzVMDataDisk -VM $VirtualMachine -VhdUri $dataDiskVhdUri -Lun $index -CreateOption Attach -Caching ReadWrite -Name $disknameData -DiskSizeInGB $diskSizes2[$pos]
                LogMessage -Message ("Successfully attached data disk:{0} " -f $dataDiskVhdUri) -LogType ([LogType]::Info1)
            }
            else
            {
                LogMessage -Message ("Fetching data disks:{0} " -f $disknameData) -LogType ([LogType]::Info1)
                $disk = Get-AzDisk -ResourceGroupName $rgName -DiskName $disknameData 
                LogMessage -Message ("Successfully fetched datadisk:{0}" -f $disknameData) -LogType ([LogType]::Info1)
                LogMessage -Message ("Starting attaching Managed data disk of ID:{0} " -f $disk.Id) -LogType ([LogType]::Info1)
                $newVM = Add-AzVMDataDisk -CreateOption Attach -Lun $index -VM $VirtualMachine -ManagedDiskId $disk.Id -Caching ReadWrite
                LogMessage -Message ("Successfully attached Managed data disk of ID:{0} " -f $disk.Id) -LogType ([LogType]::Info1)
            }
            $index = $index + 1
        }
        
        LogMessage -Message ("Updating Azure VM:{0} after attaching the data disks." -f $VMName) -LogType ([LogType]::Info1)
        Update-AzVM -VM $VirtualMachine -ResourceGroupName $rgName
        if ( !$? )
        {
            LogMessage -Message ("ERROR : Failed to Update-AzVM on VM : $VMName") -LogType ([LogType]::Error)
            exit 1
        }
        else
        {
            LogMessage -Message ("Successfully updated Azure VM:{0} after attaching the data disks." -f $VMName) -LogType ([LogType]::Info1)
        }
        LogMessage -Message ("Mounting after attaching the data disks.") -LogType ([LogType]::Info1)
        MountAttachedDataDisks
        LogMessage -Message ("Successfully mounted after attaching the data disks.") -LogType ([LogType]::Info1)
    }
    catch
    {
        LogMessage -Message ("Failed to attach data disk on VM {0}" -f $VMName) -LogType ([LogType]::Error)
        LogMessage -Message ("AttachDataDisk : ERROR : {0}" -f $_.Exception.Message) -LogType ([LogType]::Error)
        exit 1
    }
}

#
# Uninstall Mobility Agent and Reboot the agent
#
Function UninstallAgent
{
    try
    {
        #CleanAzureArtifacts
        
        $VMInfo = GetVMDetails
        
        $scriptName = "UninstallAgent.sh"
        $args = "/var/log"
        if( $global:OSType -eq "windows")
        {
            $scriptName = "UninstallAgent.ps1"
            $logDir = $Env:SystemDrive + "\ProgramData\ASRSetupLogs"
            $args = $logDir
        }
        
        # UninstallAgent on Source VM and reboot the machine
        SetCustomScriptExtension -resourceGroupName $VMInfo.ResourceGroup -storageAccountName $VMInfo.StorageAccount `
        -storageKey $VMInfo.StorageKey -location $VMInfo.Location -scriptName $scriptName -arguments $args
        
        RebootAzureVM
    }
    Catch
    {
        LogMessage -Message ("Failed to  uninstall Agent on VM {0}" -f $VMName) -LogType ([LogType]::Error)
        LogMessage -Message ("UninstallAgent : ERROR : {0}" -f $_.Exception.Message) -LogType ([LogType]::Error)
        exit 1
    }
}

#
# Get VM Information
#
Function GetVMDetails
{
    if($global:Operation -eq "TFO") { $VMName = $TFOVMName }
    
    if($ResourceGroupType.ToLower() -ne "primary")
    {
        $resourceGroupName = $global:RecResourceGroup
        $storageAccountName = $global:RecStorageAccount
        $location = $global:RecoveryLocation
    }
    else
    {
        $resourceGroupName = $PriResourceGroup
        #$storageAccountName = $global:PriStorageAccount
        $location = $global:PrimaryLocation
    }
    
    if(!$IsManagedDisk)
    {
        $VM = Get-AzVM -ResourceGroupName $resourceGroupName -VMName $VMName
        $storageAccountName = $VM.StorageProfile.OsDisk.Vhd.Uri.Split("/")[2].Split(".")[0]
    }
    
    $storageAccKey = GetStorageKey -resourceGroupName $resourceGroupName -storageAccountName $storageAccountName
    
    [hashtable]$VMInfo = @{}
    $VMInfo.ResourceGroup = $resourceGroupName
    $VMInfo.StorageAccount = $storageAccountName
    $VMInfo.StorageKey = $storageAccKey
    $VMInfo.Location = $location
    
    return $VMInfo
}

#
# Get storage account key
#
Function GetStorageKey
{
    param (
        [parameter(Mandatory=$True)] [string]$resourceGroupName,
        [parameter(Mandatory=$True)] [string]$storageAccountName
    )
    
    $key = Get-AzStorageAccountKey -Name $storageAccountName -ResourceGroupName $resourceGroupName
    try
    {
        $storageKey = $key.value[0]
    }
    catch
    {}
    finally
    {
        if($storageKey -eq '')
        {
            $storageKey = $key.Key1
        }
    }
    
    return [string]$storageKey
}

#
# Main Block
#
Function Main()
{
    try
    {
        CreateLogFileName $global:VMName $global:Operation

        LogMessage -Message ("Operation : {0}" -f $global:Operation) -LogType ([LogType]::Info1)
        
        if ($global:EnableMailReporting -eq "true" -and $global:ReportingTableName -ne $null -and $ReportOperationMap[$global:Operation] -ne $null) {
            ReportOperationStatusInProgress -VmName $global:VMName -ReportingTableName $global:ReportingTableName -OperationName $ReportOperationMap[$global:Operation]
        }

        switch ($Operation.ToUpper()) 
        {
            'LOGIN'                 {  
                                        LoginToAzureSubscription
                                        break
                                    }
            'SETVAULTCONTEXT'       {
                                        SetAzureEnvironment
                                        break
                                    }
            'ER'                    {   EnableReplication; break    }
            'TFO'                   {   TestFailover; break    }
            'REBOOT'                {   RebootAzureVM; break    }
            'UFO'                   {   UnplannedFailover; break    }
            'SP'                    {   SwitchProtection; break    }
            'FB'                    {   Failback; break    }
            'RSP'                   {   ReverseSwitchProtection; break    }
            'DR'                    {   DisableReplication; break    }
            'CR'                    {   CleanupResources; break    }
            'COPY'                  {   CopyDITools; break    }
            'UPLOAD'                {   UploadFiles; break    }
            'APPLYLOAD'             {   GenerateChurn; break    }
            'ISSUETAG'              {   IssueTag; break    }
            'VERIFYLOAD'            {   ValidateChurn;  break    }
            'UNINSTALL'             {   UninstallAgent; break    }
            'ATTACHDISK'            {   AttachDataDisk; break    }
            'CREATEDISKLAYOUT'      {   CreateDiskLayout; break }
            'CLEANPREVIOUSRUN' 
                                    {  DisableAndDeleteAzureArtifacts; break }
            'RUN540'                {
                                        LoginToAzureSubscription
                                        SetAzureEnvironment
                                        EnableReplication
                                        TestFailover
                                        UnplannedFailover
                                        SwitchProtection
                                        Failback
                                        ReverseSwitchProtection
                                        DisableReplication
                                        CleanupResources
                                        break
                                    }
            'HELP'                  {
                                        get-help $MyInvocation.ScriptName; 
                                        break 
                                    }
            default                 { break }
        }
        
        if($Operation -ne 'help')
        {
            LogMessage -Message ('{0} operation succeeded' -f $global:Operation) -LogType ([LogType]::Info1)
        }
        exit 0
    }
    catch
    {
        LogObject -Object $Error
        LogMessage -Message ("A2AGQLTest ERROR:: {0}" -f ($Error | ConvertTo-json -Depth 1)) -LogType ([LogType]::Error)
        Throw
    }
    finally {
        if ($global:EnableMailReporting -eq "true" -and $global:ReportingTableName -ne $null -and $ReportOperationMap[$global:Operation] -ne $null) {
            if ($Operation.ToUpper() -eq 'DR') {
                GetExecLogs
                ReportOperationStatus -VmName $global:VMName -ReportingTableName $global:ReportingTableName -OperationName $ReportOperationMap[$global:Operation] -LogName $global:LogName
            } else {
                ReportOperationStatus -VmName $global:VMName -ReportingTableName $global:ReportingTableName -OperationName $ReportOperationMap[$global:Operation]
            }

            ReportOperationStatus -VmName $global:VMName -ReportingTableName $global:ReportingTableName -OperationName $ReportOperationMap[$global:Operation]
        }
    }
}

Main
