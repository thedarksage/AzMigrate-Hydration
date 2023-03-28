<#
#.SYNOPSIS
#
# This script creates a VM in the specified location. 
# OSName/Tag : W2K19, W2K16, W2K12R2, W2012, W2K8R2, W10, UB1804, UB1604, UB1404, SLES15, SLES12SP4, SLES11SP4, RHEL8, RHEL81, RHEL77, RHEL6, OEL81, OEL8, OEL7, OEL6, DEBIAN8
# Note : For any changes, update above OSName/Tag and as well WTT workflow so that it would be handy for users
#>

param(
    # Script params
    # Subscription ID
    [Parameter(Mandatory=$True)]
    [string] $SubId = "41b6b0c9-3e3a-4701-811b-92135df8f9e3",

    # Azure Location where the VM need to be created
    [Parameter(Mandatory=$True)]
    [string] $Location = "eastus2euap",

    # Asrtest credentials are used for login to Azure VM. These values are decrypted through WTT job
    [Parameter(Mandatory=$True)]
    [string]  $vmUserName,

    # Asrtest credentials are used for login to Azure VM. These values are decrypted through WTT job
    [Parameter(Mandatory=$True)]
    [string]  $vmPassword,

    # Uniquely idenetifiable string 
    [Parameter(Mandatory=$True)]
    [string]  $BranchRunPrefix = "DGQL",

    # OSName/TAG
    [Parameter(Mandatory=$True)]
    [string] $OSName = $Args[0],

    # Cheapest multi core
    [Parameter(Mandatory=$False)]
    [string]  $vmSize = "Standard_B2s",

    [Parameter(Mandatory=$False)]
    [string]  $storageType = "StandardLRS",

    # Number of Disks need to be added to this VM, size of the disk hardcoded	
    [Parameter(Mandatory=$False)]
    [string]  $nrDisks = 2
)

. $PSScriptRoot\CommonFunctions.ps1
. $PSScriptRoot\OS.ps1


$OpName = "CreateVM"
CreateLogFileName $global:VMName $OpName

 # TODO : This tag helps to easily manage resources in Azure
 # This need to be applied to all resources
 #$RGTag = @{"TestType" = "A2AGQL"; "$BranchRunPrefix" = $BranchRunPrefix}

Function CreateVM()
{
    param ([string] $rgName,
           [string] $distro,
           [string] $vmName
           )

    # Network
    $vnetName = $vmName + "-VNET"
    $snetName = $vmName + "-SNET"
    $nicName = $vmName + "-NIC"
    
    # Storage
    $osDiskName = $vmName + "-OD"
    $dataDiskPrefix = $vmName + "-DD"
    # Convert to lower case
    $diagSAName = $BranchRunPrefix + "-DIAG" 

    # OS
    $VmOSType = $distros[$distro][0]
    $pub = $distros[$distro][1]
    $offer = $distros[$distro][2]
    $sku = $distros[$distro][3]

    LogMessage -Message ("Location   : {0}" -f $Location) -LogType ([LogType]::Info1)
    LogMessage -Message ("Distro/OS Details : {0}-{1}-{2}" -f $pub, $offer, $sku) -LogType ([LogType]::Info1)

    LoginAzureWithServicePrincipal $SubId
    
    LogMessage -Message ("Checking Resource Group {0} exists?" -f $rgName) -LogType ([LogType]::Info1)
    $rg = Get-AzureRmResourceGroup -Name $rgName -Verbose
    if ($rg -eq $null) {
        LogMessage -Message ("Creating Resource Group {0}" -f $rgName) -LogType ([LogType]::Info1)
        New-AzureRmResourceGroup -Name $rgName -Location $Location -Verbose
    } else {
        LogMessage -Message ("Resource Group Name {0} exists" -f $rgName) -LogType ([LogType]::Warning)
        Throw "This Resource Group should have cleaned as part of previous run, triage this!!"
    }

    LogMessage -Message ("Creating VM Config") -LogType ([LogType]::Info1)
    $newVM = New-AzureRmVMConfig -Name $vmName -VMSize $vmSize -Verbose

    # Creds	
    $vmSecurePassword = ConvertTo-SecureString $vmPassword -AsPlainText -force 
    $vmCreds = New-Object System.Management.Automation.PSCredential($vmUserName, $vmSecurePassword)

    LogMessage -Message ("Setting OS : {0}" -f $VmOSType) -LogType ([LogType]::Info1)
    
    if ($VmOSType -eq "Linux") {
        $newVM = Set-AzureRmVMOperatingSystem -VM $newVM -Linux -ComputerName $vmName -Credential $vmCreds -Verbose
    } else {
        $newVM = Set-AzureRmVMOperatingSystem -VM $newVM -Windows -ComputerName $vmName -Credential $vmCreds -Verbose
    }

    LogMessage -Message ("Creating VM {0} resources" -f $vmName) -LogType ([LogType]::Info1)
  
    LogMessage -Message ("Configuring Storage") -LogType ([LogType]::Info1)
     
    LogMessage -Message ("Setting OsDisk") -LogType ([LogType]::Info1)
    $newVM = Set-AzureRmVMSourceImage -VM $newVM -PublisherName $pub -Offer $offer -Skus $sku -Version Latest -Verbose
    $newVM = Set-AzureRmVMOSDisk -VM $newVM -Name $osDiskName -StorageAccountType $storageType -CreateOption FromImage -Caching ReadWrite

    # Create data disks and attach to VM
    $lunList=@()
    for ($i=0; $i -lt $nrDisks; $i++) {
        $dataDiskName = $dataDiskPrefix + ($i+1).ToString();
        $diskSize = $i + 2

        $diskConfig = New-AzureRmDiskConfig -AccountType $storageType -Location $Location -CreateOption Empty -DiskSizeGB $diskSize -Verbose
        $dataDisk = New-AzureRmDisk -DiskName $dataDiskName -Disk $diskConfig -ResourceGroupName $rgName -Verbose
        
        do {
            $lun = Get-Random -minimum 2 -maximum 10
        } while ($lunList -contains $lun)
        
        $lunList += $lun
        
        LogMessage -Message ("Attached diskName {0}, disksize {1}GB, lun {2}" -f $dataDiskName, $diskSize, $lun) -LogType ([LogType]::Info1)
        $vm = Add-AzureRmVMDataDisk -VM $newVM -Name $dataDiskName -CreateOption Attach -ManagedDiskId $dataDisk.Id -Lun $lun -Verbose
    }
    
    $diagSAName = $diagSAName -replace "-"
    $diagSAName = $diagSAName.ToLower()
    $diagRgName = $diagSAName

    LogMessage -Message ("Checking and Creating Resource Group {0} for Diagnostics storage account" -f $diagRgName) -LogType ([LogType]::Info1)
    $diagRg = Get-AzureRmResourceGroup -Name $diagRgName -ErrorAction SilentlyContinue 
    if ($diagRg -eq $null) {
        $diagRg = New-AzureRmResourceGroup -Name $diagRgName -Location $Location
    } else {
        LogMessage -Message ("Resource Group {0} for Diagnostic storage account already exist" -f $diagRgName) -LogType ([LogType]::Info1)
    }

    if ($diagRg -and $diagRg.ResourceGroupName -eq $diagRgName) {
        LogMessage -Message ("Creating boot diagnostics storage account {0}, rgname {1}" -f $diagSAName,$diagRgName) -LogType ([LogType]::Info1)
        $diagSA = Get-AzureRmStorageAccount -Name $diagSAName -ResourceGroupName $diagRgName -ErrorAction SilentlyContinue -Verbose
        if ($diagSA -eq $null) {
            $storageType = "Standard_LRS"
            $diagSA = New-AzureRmStorageAccount -Name $diagSAName -ResourceGroupName $diagRgName -Type $storageType -Location $Location -Verbose
        } else {
            LogMessage -Message ("$diagSAName already exist" -f $diagSAName) -LogType ([LogType]::Info1)
        }

        if ($diagSA.StorageAccountName -eq $diagSAName) {
            Set-AzureRmVMBootDiagnostics -VM $newVM -Enable -ResourceGroupName $diagRgName -StorageAccountName $diagSAName -Verbose
        } else {
            LogMessage -Message ("Failed to set boot diagnostic storage account {0} {1} {2}" -f $diagSAName, $diagRgName, $diagSA) -LogType ([LogType]::Error)
        }

    } else {
        LogMessage -Message ("Failed to create Resource Group {0} for diagnostic storage account" -f $diagRgName) -LogType ([LogType]::Warning)
    }
   
    LogMessage -Message ("Configuring network infrastructure") -LogType ([LogType]::Info1)
    #$vNet = Get-AzureRmVirtualNetwork -Name $vnetName -ResourceGroupName $rgName -ErrorAction SilentlyContinue -Verbose

    LogMessage -Message ("Creating SubNet {0}" -f $snetName) -LogType ([LogType]::Info1)
    $sNet = New-AzureRmVirtualNetworkSubnetConfig -Name $snetName -AddressPrefix '10.0.0.0/24' -Verbose
    
    if ($sNet -ne $null) {
        LogMessage -Message ("Creating vNet {0}" -f $vnetName) -LogType ([LogType]::Info1)
        $vNet = New-AzureRmVirtualNetwork -Name $vnetName -ResourceGroupName $rgName -Location $Location -AddressPrefix "10.0.0.0/16" -Subnet $sNet -Verbose
        
        if ($vNet -ne $null) {
            LogMessage -Message ("Creating vNet {0} Succeeded" -f $vnetName) -LogType ([LogType]::Info1)
        } else {
            LogMessage -Message ("Creating vNet {0} failed" -f $vnetName) -LogType ([LogType]::Error)
            Throw("Creating Vnet failed")
        }
    } else {
        LogMessage -Message ("Creating Subnet {0} failed" -f $vnetName) -LogType ([LogType]::Error)
        Throw("Creating Snet failed")
    }

    LogMessage -Message ("Creating NIC {0}" -f $nicName) -LogType ([LogType]::Info1)
    $nic = New-AzureRmNetworkInterface -Name $nicName -Location $Location -ResourceGroupName $rgName -SubnetId $vNet.Subnets[0].Id -Verbose

    if ($nic -ne $null) {
        # Attach the NIC
        LogMessage -Message ("Attach NIC to VM") -LogType ([LogType]::Info1)
        $newVM = Add-AzureRmVMNetworkInterface -VM $newVM -Id $nic.Id -Verbose
    } else {
        LogMessage -Message ("Attaching NIC to VM failed" -f $nicName) -LogType ([LogType]::Error)
        Throw("Attaching NIC to VM failed")
    }

    LogMessage -Message ("Creating VM") -LogType ([LogType]::Info1)
    New-AzureRmVM -ResourceGroupName $rgName -Location $Location -VM $newVM
    
    $VerifyAzureVm = Get-AzureRmVM -ResourceGroupName $rgName -Name $vmName -ErrorAction SilentlyContinue -Verbose
    if ($VerifyAzureVm.Name -eq $vmName) {
        LogMessage -Message ("Azure VM {0} created" -f $vmName) -LogType ([LogType]::Info1)
    }  else {
        Throw "Create VM failed" 
    }
}

Function Main()
{
    if ($global:EnableMailReporting -eq "true" -and $global:ReportingTableName -ne $null) {
        ReportOperationStatusInProgress -VmName $global:VMName -ReportingTableName $global:ReportingTableName -OperationName $OpName
    }

    try
    {
       # Validate the tags refer to defined distros
       LogMessage -Message ("Create VM Operation started") -LogType ([LogType]::Info1)
       LogMessage -Message ("Selected OS : {0}" -f $OSName) -LogType ([LogType]::Info1)
       CreateVM $global:PriResourceGroup $OSName $global:VMName
       WaitForAzureVMReadyState -vmName $global:VMName -resourceGroupName $global:PriResourceGroup
       LogMessage -Message ("Create VM Operation Passed") -LogType ([LogType]::Info1)
    }
    catch
    {
        LogMessage -Message ("ERROR:: {0}" -f ($Error | ConvertTo-json -Depth 1)) -LogType ([LogType]::Error)
        LogMessage -Message ("Create VM script failed") -LogType ([LogType]::Error)

        $ErrorMessage = ('{0} operation failed' -f $OpName)

        LogMessage -Message $ErrorMessage -LogType ([LogType]::Error)

        throw $ErrorMessage

    }
    finally {

        if ($global:EnableMailReporting -eq "true" -and $global:ReportingTableName -ne $null) {
            ReportOperationStatus -VmName $global:VMName -ReportingTableName $global:ReportingTableName -OperationName $OpName
        }
    }

}
Main
