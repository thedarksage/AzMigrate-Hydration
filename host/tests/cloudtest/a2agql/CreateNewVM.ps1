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

    # Credentials are used for login to Azure VM.
    [Parameter(Mandatory=$True)]
    [string]  $vmUserName,

    # Uniquely idenetifiable string 
    [Parameter(Mandatory=$True)]
    [string]  $BranchRunPrefix = "DGQL",

    # OSName/TAG
    [Parameter(Mandatory=$True)]
    [string] $OSName = $Args[0],

    # OSType
    [Parameter(Mandatory=$True)]
    [string] $OSType,

	# Cheapest multi core
    [Parameter(Mandatory=$False)]
    [string]  $vmSize = "Standard_B2s",

    [Parameter(Mandatory=$False)]
    [string]  $storageType = "Standard_LRS",

    # Number of Disks need to be added to this VM, size of the disk hardcoded	
    [Parameter(Mandatory=$False)]
    [string]  $nrDisks = 2,

    [Parameter(Mandatory=$False)]
    [string] $RequiredKernel
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
           [string] $vmName,
           [string] $vmPassword
           )

    # Network
    $vnetName = $global:PrimaryVnet
    $snetName = $vmName + "-SNET"
    $nicName = $vmName + "-NIC"
    
    # Storage
    $osDiskName = $vmName + "-OD"
    $dataDiskPrefix = $vmName + "-DD"
    # Convert to lower case
    $diagSAName = $vmName + "-DIAG"

    # OS
    $VmOSType = $distros[$distro][0]
    $pub = $distros[$distro][1]
    $offer = $distros[$distro][2]
    $sku = $distros[$distro][3]

    LogMessage -Message ("rgName : {0}, distro : {1}, vmName : {2}" -f $rgName, $distro, $vmName) -LogType ([LogType]::Info1)
    LogMessage -Message ("Location   : {0}" -f $Location) -LogType ([LogType]::Info1)
    LogMessage -Message ("Distro/OS Details : {0}-{1}-{2}" -f $pub, $offer, $sku) -LogType ([LogType]::Info1)

    LoginToAzureSubscription

    LogMessage -Message ("Checking Resource Group {0} exists?" -f $rgName) -LogType ([LogType]::Info1)
    $rg = Get-AzResourceGroup -Name $rgName -Verbose -ErrorAction SilentlyContinue
    if ($rg -eq $null) {
        LogMessage -Message ("Creating Resource Group {0}" -f $rgName) -LogType ([LogType]::Info1)
        New-AzResourceGroup -Name $rgName -Location $Location -Verbose

        AssignTagToRG -RGName $rgName
    } else {
        LogMessage -Message ("Resource Group Name {0} exists. Skipping!!" -f $rgName) -LogType ([LogType]::Warning)
    }

    # Create Primary Network.
    LogMessage -Message ("Configuring network infrastructure") -LogType ([LogType]::Info1)
    $vNet = Get-AzVirtualNetwork -Name $vnetName -ResourceGroupName $rgName -ErrorAction SilentlyContinue -Verbose
    if ($vNet -eq $null)
    {
        LogMessage -Message ("Creating SubNet {0}" -f $snetName) -LogType ([LogType]::Info1)
        $sNet = New-AzVirtualNetworkSubnetConfig -Name $snetName -AddressPrefix '10.0.0.0/24' -Verbose
        
        if ($sNet -ne $null) {
            LogMessage -Message ("Creating vNet {0}" -f $vnetName) -LogType ([LogType]::Info1)
            $vNet = New-AzVirtualNetwork -Name $vnetName -ResourceGroupName $rgName -Location $Location -AddressPrefix "10.0.0.0/16" -Subnet $sNet -Verbose
            
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
        $nic = New-AzNetworkInterface -Name $nicName -Location $Location -ResourceGroupName $rgName -SubnetId $vNet.Subnets[0].Id -Verbose
    }
    else {
        $nic = Get-AzNetworkInterface -Name $nicName -ResourceGroupName $rgName -ErrorAction SilentlyContinue
        if ($nic -eq $null)
        {
	        LogMessage -Message ("Create a network interface card:{0} in location:{1}" -f $nicName,$Location) -LogType ([LogType]::Info1)	
			$nic = New-AzNetworkInterface -Name $nicName -Location $Location -ResourceGroupName $rgName -SubnetId $vNet.Subnets[0].Id
        }
    }

    # Create VM
    LogMessage -Message ("***Creating Virtual Machine***") -LogType ([LogType]::Info1)
    $azureVm = Get-AzVM -ResourceGroupName $rgName -Name $vmName -ErrorAction SilentlyContinue
    if($azureVm -eq $null)
    {
        LogMessage -Message ("Creating VM Config") -LogType ([LogType]::Info1)
        $newVM = New-AzVMConfig -VMName $vmName -VMSize $vmSize -Verbose

        # Creds
        $vmSecurePassword = ConvertTo-SecureString $vmPassword -AsPlainText -force 
        $vmCreds = New-Object System.Management.Automation.PSCredential($vmUserName, $vmSecurePassword)

        LogMessage -Message ("Setting OS : {0}" -f $VmOSType) -LogType ([LogType]::Info1)
        
        LogObject -Object $AutoPatchSupportedDistros

        if ($VmOSType -eq "Linux") {
            if ($AutoPatchSupportedDistros.Contains($distro))
            {
                LogMessage -Message ("Enabling Auto Patching for VM : {0}, distro : {1}" -f $vmName, $distro) -LogType ([LogType]::Info1)
                $newVM = Set-AzVMOperatingSystem -VM $newVM -Linux -ComputerName $vmName -Credential $vmCreds -PatchMode "AutomaticByPlatform" -Verbose
            }
            else {
                $newVM = Set-AzVMOperatingSystem -VM $newVM -Linux -ComputerName $vmName -Credential $vmCreds -Verbose
            }
        } else {
            if ($AutoPatchSupportedDistros.Contains($distro))
            {
                LogMessage -Message ("Enabling Auto Patching for Windows VM : {0}, distro : {1}" -f $vmName, $distro) -LogType ([LogType]::Info1)
                $newVM = Set-AzVMOperatingSystem -VM $newVM -Windows -ComputerName $vmName -Credential $vmCreds -ProvisionVMAgent -EnableAutoUpdate -PatchMode "AutomaticByPlatform" -Verbose
            }
            else {
                $newVM = Set-AzVMOperatingSystem -VM $newVM -Windows -ComputerName $vmName -Credential $vmCreds -Verbose
            }
        }

        LogMessage -Message ("Creating VM {0} resources" -f $vmName) -LogType ([LogType]::Info1)
    
        LogMessage -Message ("Configuring Storage") -LogType ([LogType]::Info1)
        
        LogMessage -Message ("Setting OsDisk : {0}" -f $osDiskName) -LogType ([LogType]::Info1)
        $newVM = Set-AzVMSourceImage -VM $newVM -PublisherName $pub -Offer $offer -Skus $sku -Version Latest -Verbose
        $newVM = Set-AzVMOSDisk -VM $newVM -Name $osDiskName -StorageAccountType $storageType -CreateOption FromImage -Caching ReadWrite

        if ($nic -ne $null) {
            # Attach the NIC
            LogMessage -Message ("Attach NIC to VM") -LogType ([LogType]::Info1)
            $newVM = Add-AzVMNetworkInterface -VM $newVM -Id $nic.Id -Verbose
        } else {
            LogMessage -Message ("Attaching NIC to VM failed" -f $nicName) -LogType ([LogType]::Error)
            Throw("Attaching NIC to VM failed")
        }
        
        # Create data disks and attach to VM
        $lunList=@()
        for ($i=0; $i -lt $nrDisks; $i++) {
            $dataDiskName = $dataDiskPrefix + ($i+1).ToString();
            $diskSize = $i + 2

            $diskConfig = New-AzDiskConfig -AccountType $storageType -Location $Location -CreateOption Empty -DiskSizeGB $diskSize -Verbose
            $dataDisk = New-AzDisk -DiskName $dataDiskName -Disk $diskConfig -ResourceGroupName $rgName -Verbose
            
            do {
                $lun = Get-Random -minimum 2 -maximum 10
            } while ($lunList -contains $lun)
            
            $lunList += $lun
            
            LogMessage -Message ("Attached diskName {0}, disksize {1}GB, lun {2}" -f $dataDiskName, $diskSize, $lun) -LogType ([LogType]::Info1)
            $vm = Add-AzVMDataDisk -VM $newVM -Name $dataDiskName -CreateOption Attach -ManagedDiskId $dataDisk.Id -Lun $lun -Verbose
        }
        
        $diagSAName = $diagSAName -replace "-"
        $diagSAName = $diagSAName.ToLower()
        $diagRgName = $diagSAName

        LogMessage -Message ("Checking and Creating Resource Group {0} for Diagnostics storage account" -f $diagRgName) -LogType ([LogType]::Info1)
        $diagRg = Get-AzResourceGroup -Name $diagRgName -ErrorAction SilentlyContinue 
        if ($diagRg -eq $null) {
            $diagRg = New-AzResourceGroup -Name $diagRgName -Location $Location

            AssignTagToRG -RGName $diagRgName
        } else {
            LogMessage -Message ("Resource Group {0} for Diagnostic storage account already exist. Skipping!!" -f $diagRgName) -LogType ([LogType]::Info1)
        }

        if ($diagRg -and $diagRg.ResourceGroupName -eq $diagRgName) {
            LogMessage -Message ("Creating boot diagnostics storage account {0}, rgname {1}" -f $diagSAName,$diagRgName) -LogType ([LogType]::Info1)
            $diagSA = Get-AzStorageAccount -Name $diagSAName -ResourceGroupName $diagRgName -ErrorAction SilentlyContinue -Verbose
            if ($diagSA -eq $null) {
                $storageType = "Standard_LRS"
                $diagSA = New-AzStorageAccount -Name $diagSAName -ResourceGroupName $diagRgName -Type $storageType -Location $Location -Verbose
            } else {
                LogMessage -Message ("$diagSAName already exist. Skipping!!" -f $diagSAName) -LogType ([LogType]::Info1)
            }

            if ($diagSA.StorageAccountName -eq $diagSAName) {
                Set-AzVMBootDiagnostic -Enable -VM $newVM -ResourceGroupName $diagRgName -StorageAccountName $diagSAName -Verbose

            } else {
                LogMessage -Message ("Failed to set boot diagnostic storage account {0} {1} {2}" -f $diagSAName, $diagRgName, $diagSA) -LogType ([LogType]::Error)
            }

        } else {
            LogMessage -Message ("Failed to create Resource Group {0} for diagnostic storage account" -f $diagRgName) -LogType ([LogType]::Warning)
        }
        
        LogMessage -Message ("Creating VM") -LogType ([LogType]::Info1)
        New-AzVM -ResourceGroupName $rgName -Location $Location -VM $newVM

        $VerifyAzureVm = Get-AzVM -ResourceGroupName $rgName -Name $vmName -ErrorAction SilentlyContinue -Verbose
        if ($VerifyAzureVm.Name -eq $vmName) {
            LogMessage -Message ("Azure VM {0} created" -f $vmName) -LogType ([LogType]::Info1)
        }  else {
            Throw "Create VM failed"
        }

        if ($OSType.ToUpper() -eq "WINDOWS")
        {
            Write-Host "Running InitDisks.ps1 script to initialize disks"
            Invoke-AzVMRunCommand -ResourceGroupName $rgName -Name $vmName -CommandId 'RunPowerShellScript' -ScriptPath InitDisks.ps1
            Write-Host "Initialized disks"

            LogMessage -Message ("Invoking Install Patch for Azure VM {0}" -f $vmName) -LogType ([LogType]::Info1)
            Invoke-AzVmInstallPatch -ResourceGroupName $global:PriResourceGroup -VMName $vmName -MaximumDuration "PT2H" -RebootSetting "Always" -Windows -ClassificationToIncludeForWindows "Critical", "Security"
            WaitForAzureVMReadyState -vmName $vmName -resourceGroupName $global:PriResourceGroup
            LogMessage -Message ("Successfully installed patches on the Azure VM {0}" -f $vmName) -LogType ([LogType]::Info1)
        }
        else {
            LogMessage -Message ("Invoking Install Patch for Azure VM {0}" -f $vmName) -LogType ([LogType]::Info1)
            Invoke-AzVmInstallPatch -ResourceGroupName $global:PriResourceGroup -VMName $vmName -MaximumDuration "PT2H" -RebootSetting "Always" -Linux -ClassificationToIncludeForLinux  "Critical", "Security"
            WaitForAzureVMReadyState -vmName $vmName -resourceGroupName $global:PriResourceGroup
            LogMessage -Message ("Successfully installed patches on the Azure VM {0}" -f $vmName) -LogType ([LogType]::Info1)
        }
    }
    else {
        LogMessage -Message ("Azure VM {0} already exists. Skipping!!" -f $vmName) -LogType ([LogType]::Info1)
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
        $vmPassword = GetSecretFromKeyVault -SecretName $global:SecretName
        CreateVM $global:PriResourceGroup $OSName $global:VMName $vmPassword
        WaitForAzureVMReadyState -vmName $global:VMName -resourceGroupName $global:PriResourceGroup
        LogMessage -Message ("Create VM Operation Passed") -LogType ([LogType]::Info1)

        LogMessage -Message ("OSType : {0}, ValidateDeployment : {1}" -f $OSType, $global:ValidateDeployment) -LogType ([LogType]::Info1)
        if ($OSType.ToUpper() -eq "LINUX" -and $global:ValidateDeployment)
        {
            $Script = Join-Path -Path $PSScriptRoot -ChildPath "UpdateKernel.ps1"
            .$Script -OSName $OSName -RequiredKernel $RequiredKernel -RootPwd $vmPassword
            if (!$?) {
                Throw
            }
            LogMessage -Message ("UpdateKernel operation succeeded!!") -LogType ([LogType]::Info1)
        }
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
