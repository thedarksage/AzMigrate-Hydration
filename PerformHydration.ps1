<#+--------------------------------------------------------------------------------------------------+
File         :   PerformHydration.ps1

Description  :  This script is independently capable of raising a hydration virtual machine,
                attaching customer disks to it, performing customizations and recreating a VM through
                the changed disks. The user is required to give all the necessary inputs as parameters
                when running in the beginning itself. Refer to the HelpMessages of the parameters
                wherever necessary.
+---------------------------------------------------------------------------------------------------+#>
[CmdletBinding(DefaultParametersetName='Default')] 
param
(
    [parameter(mandatory=$true, HelpMessage="Enter Your Resource Group Name...")]
    [string]$ResourceGroupName,

    [parameter(mandatory=$true, HelpMessage="Enter Your Location... Eg: centraluseuap")]
    [string]$Location,

    [parameter(mandatory=$true, HelpMessage="Enter Your Subscription Id...")]
    [string]$SubscriptionId,

    [parameter(mandatory=$true, HelpMessage="Mention the type of Operating System: Enter 0 for Windows and 1 for Linux. (Linux is set as default)")]
    [int]$OSType,

    [parameter(mandatory=$true, HelpMessage="Enter the name of the Source OS Disk")]
    [string]$OSDiskName,

    [parameter(ParameterSetName="DataDisk", mandatory=$false)]
    [switch]$AttachDataDisks,

    [parameter(ParameterSetName="DataDisk", mandatory=$true, HelpMessage="Enter the number of source data disks")]
    [int]$NoOfDataDisks=0,
    
    [parameter(ParameterSetName="DataDisk", mandatory=$true, HelpMessage="Enter the name of all the Source Data Disks")]
    [string[]]$DataDisksName,

    [parameter(mandatory=$false, HelpMessage="Enter the name of the GitHub branch")]
    [string]$GithubBranch="main",

    [parameter(ParameterSetName="CustomConfigSettings", mandatory=$false)]
    [switch]$AddCustomConfigSettings,

    [parameter(ParameterSetName="CustomConfigSettings", mandatory=$true, HelpMessage="Enter 1 for Yes and 0 for No")]
    [int]$EnableDHCP,
    
    [parameter(ParameterSetName="CustomConfigSettings", mandatory=$true, HelpMessage="Enter 1 for Yes and 0 for No")]
    [int]$EnableGA,

    [parameter(mandatory=$true, HelpMessage="Enter the name of the virtual network for the target Virtual Machine")]
    [string]$TargetVM_VirtualNetworkName,

    [parameter(mandatory=$true, HelpMessage="Enter the address of the virtual network for the target Virtual Machine")]
    [string]$TargetVM_VirtualNetworkAddress,

    [parameter(mandatory=$true, HelpMessage="Enter the name of the subnet for the target Virtual Machine")]
    [string]$TargetVM_SubnetName,

    [parameter(mandatory=$true, HelpMessage="Enter the address of the subnet for the target Virtual Machine")]
    [string]$TargetVM_SubnetAddress,

    [parameter(mandatory=$true, HelpMessage="Enter the name of the network interface for the target Virtual Machine")]
    [string]$TargetVM_NICName,

    [parameter(mandatory=$true, HelpMessage="Enter the name of the network security group for the target Virtual Machine")]
    [string]$TargetVM_NSGName,

    [parameter(mandatory=$true, HelpMessage="Enter the name of the target Virtual Machine")]
    [string]$TargetVM_VirtualMachineName   
)

#Random String Generation
[String]$RandomString = -join ( (48..57)  | Get-Random -Count 5 | % {[char]$_}) 

#Variables/Constants used in Creation of Virtual Network 
[string]$HydVM_AddressPrefix      = "10.255.248.0/22"
[string]$HydVM_VirtualNetworkName = "HydVM-VirtualNetwork" + $RandomString
[String]$HydVM_SubnetName         = "HydVM-Subnet"

#Variable used in Creation of NIC
[string]$HydVM_NICName = "HydVM-NIC" + $RandomString

#Variable used in Creation of NSG
[string]$HydVM_NSGName = "HydVM-NSG" + $RandomString

#Variables used in the Creation of Hydration Virtual Machine
[string]$HydVM_Name       = "HydVM" + $RandomString
[string]$HydVM_OSDiskName = "HydOsDisk" + $RandomString

#Variables/Constants used in Creation of Hydration Virtual Machine- Windows
[string]$HydVM_PublisherNameWindows      = "MicrosoftWindowsServer"
[string]$HydVM_OfferWindows              = "WindowsServer"
[string]$HydVM_SkusWindows               = "2016-Datacenter-smalldisk"
[string]$HydVM_VersionWindows            = "latest"
[string]$HydVM_StorageAccountTypeWindows = "Standard_LRS"

#Variables/Constants used in Creation of Hydration Virtual Machine- Linux
[string]$HydVM_PublisherNameLinux      = "Canonical"
[string]$HydVM_OfferLinux              = "UbuntuServer"
[string]$HydVM_SkusLinux               = "16.04-LTS"
[string]$HydVM_VersionLinux            = "latest"
[string]$HydVM_StorageAccountTypeLinux = "Standard_LRS"

#Variable used in attaching of source disk function
$DiskMap = [ordered]@{}

#Variables/constants used to fetch files from a github branch
$HydVM_CustomScriptExtensionName = "HydrationCustomScriptExtension"
$BaseUriApi                      = "https://api.github.com/repos/thedarksage/AzMigrate-Hydration/git/trees/"+$GithubBranch+"?recursive=1"
$PrefixFileUri                   = "https://github.com/thedarksage/AzMigrate-Hydration/raw/$GithubBranch/"
$script:FileUris                 =  @();

#Variables used to validate the status of operations for/on Hydration VM
[bool]$script:LoginSuccessStatus                              = $false
[bool]$script:SubscriptionSuccessStatus                       = $false
[bool]$script:HydVM_VirtualNetworkSuccessStatus               = $false
[bool]$script:HydVM_NICSuccessStatus                          = $false
[bool]$script:HydVM_NSGSuccessStatus                          = $false
[bool]$script:HydVM_VirtualMachineSuccessStatus               = $false
[bool]$script:HydVM_AttachOSDiskSuccessStatus                 = $false
[bool]$script:HydVM_AttachDataDisksSuccessStatus              = $true
[bool]$script:HydVM_AttachCSESuccessStatus                    = $false
[bool]$script:HydVM_FetchHydCompFromGithubSuccessStatus       = $false

#Variables used to validate the status of operations for/on Target VM
[bool]$script:TargetVM_VirtualNetworkSuccessStatus = $false
[bool]$script:TargetVM_NICSuccessStatus            = $false
[bool]$script:TargetVM_NSGSuccessStatus            = $false
[bool]$script:TargetVM_VirtualMachineSuccessStatus = $false

function Login 
{
    Write-Host "Login to your azure account in the pop-up below" -ForeGroundColor Green
    $LoginCheckStatus = Connect-AzAccount

    if(-not ([string]::IsNullOrEmpty($LoginCheckStatus)))
    {
        Write-Host "Successful Login" -ForegroundColor Green      
        $script:LoginSuccessStatus = $true
    }
    else 
    {
        Write-Error "Unsuccessful Login!"
    }

    $SubscriptionCheckStatus = Set-AzContext -Subscription $SubscriptionId

    if(-not ([string]::IsNullOrEmpty($SubscriptionCheckStatus)))
    {
        $script:SubscriptionSuccessStatus = $true
    }
    else 
    {
        Write-Error "Invalid SubscriptionId"
    }
}

function HydVM_CreateVirtualNetwork
{
    Write-Host "Creating the Virtual Network for Hydration Virtual Machine..." -ForegroundColor Green

    $CreatedSubnet = New-AzVirtualNetworkSubnetConfig -Name $HydVM_SubnetName -AddressPrefix $HydVM_AddressPrefix
    $VirtualNetworkCheckStatus = New-AzVirtualNetwork -Name $HydVM_VirtualNetworkName -ResourceGroupName $ResourceGroupName -Location $Location -AddressPrefix $HydVM_AddressPrefix -Subnet $CreatedSubnet 
    if(-not ([string]::IsNullOrEmpty($VirtualNetworkCheckStatus)))
    {
        Write-Host "Creation of Virtual Network for Hydration Virtual Machine Successful!" -ForegroundColor Green      
        $script:HydVM_VirtualNetworkSuccessStatus = $true
    }
    else 
    {
        Write-Error "Creation of Virtual Network for Hydration Virtual Machine Unsuccessful!"
    }   
}

function HydVM_CreateNIC
{
    Write-Host "Creating the Network Interface for Hydration Virtual Machine..." -ForegroundColor Green

    $CreatedVirtualNetwork = Get-AzVirtualNetwork -ResourceGroupName $ResourceGroupName -Name $HydVM_VirtualNetworkName
    $CreatedSubnet = Get-AzVirtualNetworkSubnetConfig -Name $HydVM_SubnetName -VirtualNetwork $CreatedVirtualNetwork
    $NICCheckStatus = New-AzNetworkInterface -Name $HydVM_NICName -ResourceGroupName $ResourceGroupName -Location $Location -Subnet $CreatedSubnet
    if(-not ([string]::IsNullOrEmpty($NICCheckStatus)))
    {
        Write-Host "Creation of Network Interface for Hydration Virtual Machine Successful!" -ForegroundColor Green      
        $script:HydVM_NICSuccessStatus = $true
    }
    else 
    {
        Write-Error "Creation of Network Interface Unsuccessful!"
    }
}

function HydVM_CreateNSG
{
    Write-Host "Creating Network Security Group for Hydration Virtual Machine..." -ForegroundColor Green

    $CreatedNSG = New-AzNetworkSecurityGroup -Name $HydVM_NSGName -ResourceGroupName $ResourceGroupName  -Location  $Location
    if(-not ([string]::IsNullOrEmpty($CreatedNSG)))
    {
        $CreatedVirtualNetwork = Get-AzVirtualNetwork -ResourceGroupName $ResourceGroupName -Name $HydVM_VirtualNetworkName
        Set-AzVirtualNetworkSubnetConfig -Name $HydVM_SubnetName -VirtualNetwork $CreatedVirtualNetwork -AddressPrefix $HydVM_AddressPrefix -NetworkSecurityGroup $CreatedNSG
        Set-AzVirtualNetwork -VirtualNetwork $CreatedVirtualNetwork
        Write-Host "Creation of Network Security Group for Hydration Virtual Machine Successful!" -ForegroundColor Green      
        $script:HydVM_NSGSuccessStatus = $true
    }
    else 
    {
        Write-Error "Creation of Network Security Group for Hydration Virtual Machine Unsuccessful!"  
    }  
}

function HydVM_CreateVirtualMachineWindows 
{
    #This command gives the ability of setting the username and password of the Hydration VM to the user.
    #$Credentials=Get-Credential -Message "Enter a username and password for the virtual machine."

    #These commands are used for random generation of username and password adhering to the password rules
    $Username = $HydVM_Name
    $Password = -join ( (33..126)  | Get-Random -Count 10 | % {[char]$_}) 
    $Password = "HydVM@"+$Password | ConvertTo-SecureString -Force -AsPlainText
    $Credentials = New-Object -TypeName PSCredential -ArgumentList ($Username, $Password)
    
    #HardwareProfile
    $NoOfSourceDisks = $NoOfDataDisks + 1 # 1 for source OS Disk
    $VMSize = Get-AzVMSize -Location $Location | Where {($_.NumberOfCores -gt '2') -and ($_.MemoryInMB -gt '2048') -and ($_.MaxDataDiskCount -gt $NoOfSourceDisks) -and ($_.ResourceDiskSizeInMB -ne 0)}
    $VirtualMachine = New-AzVMConfig -VMName $HydVM_Name -VMSize $VMSize[0].Name

    #StorageProfile
    Set-AzVMOSDisk -Name $HydVM_OSDiskName -VM $VirtualMachine -CreateOption FromImage -StorageAccountType $HydVM_StorageAccountTypeWindows -Caching ReadWrite
    
    #OSProfile
    Set-AzVMOperatingSystem -VM $VirtualMachine -Windows -ComputerName $HydVM_Name -Credential $Credentials -ProvisionVMAgent -EnableAutoUpdate
    Set-AzVMSourceImage -VM $VirtualMachine -PublisherName $HydVM_PublisherNameWindows -Offer $HydVM_OfferWindows -Skus $HydVM_SkusWindows -Version $HydVM_VersionWindows
    
    #Network Profile
    $NICObject = Get-AzNetworkInterface -Name $HydVM_NICName
    $VirtualMachine = Add-AzVMNetworkInterface -VM $VirtualMachine -Id $NICObject.Id
    
    Write-Host "Creating the Hydration Virtual Machine..." -ForegroundColor Green

    $VirtualMachineCheckStatus = New-AzVM -ResourceGroupName $ResourceGroupName -Location $Location -VM $VirtualMachine

    if(-not ([string]::IsNullOrEmpty($VirtualMachineCheckStatus)))
    {
        #GetStatusofVM
        Get-AzVM -ResourceGroupName $ResourceGroupName -Name $HydVM_Name -Status
        Write-Host "Creation of Hydration Virtual Machine Successful!" -ForegroundColor Green      
        $script:HydVM_VirtualMachineSuccessStatus = $true
    }
    else 
    {
        Write-Error "Creation of Hydration Virtual Machine Unsuccessful!"
    }
}

function HydVM_CreateVirtualMachineLinux
{
    #This command gives the ability of setting the username and password of the Hydration VM to the user.
    #$Credentials=Get-Credential -Message "Enter a username and password for the virtual machine."

    #These commands are used for random generation of username and password adhering to the password rules
    $Username = $HydVM_Name
    $Password = -join ( (33..126)  | Get-Random -Count 20 | % {[char]$_}) 
    $Password = $Password+ "HydVM@" | ConvertTo-SecureString -Force -AsPlainText
    $Credentials = New-Object -TypeName PSCredential -ArgumentList ($Username, $Password)

    #HardwareProfile 
    $NoOfSourceDisks = $NoOfDataDisks + 1 # 1 for source OS Disk
    $VMSize = Get-AzVMSize -Location $Location | Where {($_.NumberOfCores -gt '2') -and ($_.MemoryInMB -gt '2048') -and ($_.MaxDataDiskCount -gt $NoOfSourceDisks) -and ($_.ResourceDiskSizeInMB -ne 0)}
    $VirtualMachine = New-AzVMConfig -VMName $HydVM_Name -VMSize $VMSize[0].Name

    #StorageProfile
    Set-AzVMOSDisk -Name $HydVM_OSDiskName -VM $VirtualMachine -CreateOption FromImage -StorageAccountType $HydVM_StorageAccountTypeLinux -Caching ReadWrite
    
    #OSProfile
    Set-AzVMOperatingSystem -VM $VirtualMachine -Linux -ComputerName $HydVM_Name -Credential $Credentials 
    Set-AzVMSourceImage -VM $VirtualMachine -PublisherName $HydVM_PublisherNameLinux -Offer $HydVM_OfferLinux -Skus $HydVM_SkusLinux -Version $HydVM_VersionLinux

    #NetworkProfile
    $NICObject = Get-AzNetworkInterface -Name $HydVM_NICName
    $VirtualMachine = Add-AzVMNetworkInterface -VM $VirtualMachine -Id $NICObject.Id
    
    Write-Host "Creating the Hydration Virtual Machine..." -ForegroundColor Green
    $VirtualMachineCheckStatus = New-AzVM -ResourceGroupName $ResourceGroupName -Location $Location -VM $VirtualMachine

    if(-not ([string]::IsNullOrEmpty($VirtualMachineCheckStatus)))
    {
        #GetStatusofVM
        Get-AzVM -ResourceGroupName $ResourceGroupName -Name $HydVM_Name -Status
        Write-Host "Creation of Hydration Virtual Machine Successful!" -ForegroundColor Green
        $script:HydVM_VirtualMachineSuccessStatus = $true
    }
    else 
    {
        Write-Error "Creation of Hydration Virtual Machine Unsuccessful!"
    }   
}

Function HydVM_AttachSourceDisks
{
    Write-Host "Attaching Source OS Disk to the Hydration Virtual Machine..." -ForegroundColor Green

    $Disk = Get-AzDisk -ResourceGroupName $ResourceGroupName -DiskName $OSDiskName
    $VirtualMachine = Get-AzVM -Name $HydVM_Name -ResourceGroupName $ResourceGroupName
    $VirtualMachine = Add-AzVMDataDisk -CreateOption Attach -Lun 0 -VM $VirtualMachine -ManagedDiskId $Disk.Id
    $SourceDiskCheckStatus = Update-AzVM -VM $VirtualMachine -ResourceGroupName $ResourceGroupName
    If(-not [string]::IsNullOrEmpty($SourceDiskCheckStatus))
    {
        Write-Host "Attachment of Source OS disk to Hydration Virtual Machine Successful!" -ForegroundColor Green
        $DiskMap.Add($Disk.Id,0)
        $script:HydVM_AttachOSDiskSuccessStatus = $true
    }
    else 
    {
        Write-Error "Attachment of Source OS disk to Hydration Virtual Machine Unsuccessful!"
    }
    
    If($OSType -ne 0 -and $AttachDataDisks.IsPresent)
    {
        Write-Host "Attaching Source Data Disks to the Hydration Virtual Machine..." -ForegroundColor Green
        for($i = 0; $i -lt $NoOfDataDisks; $i++)
        {
            $Disk = Get-AzDisk -ResourceGroupName $ResourceGroupName -DiskName $DataDisksName[$i]
            $VirtualMachine = Get-AzVM -Name $HydVM_Name -ResourceGroupName $ResourceGroupName
            $VirtualMachine = Add-AzVMDataDisk -CreateOption Attach -Lun ($i+1) -VM $VirtualMachine -ManagedDiskId $Disk.Id
            $DataDiskCheckStatus = Update-AzVM -VM $VirtualMachine -ResourceGroupName $ResourceGroupName
            If([string]::IsNullOrEmpty($DataDiskCheckStatus))
            {
                Write-Error "Attachment of Source Data disk to Hydration Virtual Machine Unsuccessful!"
                $script:AttachDataDiskSuccessStatus = $false
            }
            $DiskMap.Add($Disk.Id, ($i+1))   
        } 
    } 
}

function HydVM_FetchHydComponentsFromGithub
{
    Write-Host "Fetching Hydration Components from $GithubBranch..." -ForegroundColor Green
    $Information = Invoke-WebRequest -Uri $BaseUriApi -UseBasicParsing
    if($null -eq $Information)
    {
        Write-Error "Invalid Github Branch Name"
    }
    else
    {   
        $Objects = $Information.Content | ConvertFrom-Json
        $Files = $Objects | Select -exp tree
        [string[]]$FileNames=$Files.path
        $CheckMandatoryFiles = @($false,$false,$false,$false)
        for($i=0;$i -lt $FileNames.Length;$i++)
        {
            if($FileNames[$i] -eq "AzureRecoveryTools.zip")
            {
                $CheckMandatoryFiles[0]=$true
            }
            if($FileNames[$i] -eq "StartupScript.ps1")
            {
                $CheckMandatoryFiles[1]=$true
            }
            if($FileNames[$i] -eq "StartupScript.sh")
            {
                $CheckMandatoryFiles[2]=$true
            }
            if($FileNames[$i] -eq "azurerecovery-azmigrate-dummy-guid.conf")
            {
                $CheckMandatoryFiles[3]=$true
            }
        }

        $AllMandatoryFilesPresent_Windows = $true
        $AllMandatoryFilesPresent_Linux = $true
        if(-not $CheckMandatoryFiles[0])
        {
            Write-Error "Mandatory component: AzureRecoveryTools.zip missing from '$GithubBranch'. Suggestion: Use main branch and re-run the script"
            $AllMandatoryFilesPresent_Windows=$false
            $AllMandatoryFilesPresent_Linux=$false
        }
        if(-not $CheckMandatoryFiles[1] -and $OSType -eq 0)
        {
            Write-Error "Mandatory component: StartupScript.ps1 missing from '$GithubBranch'. Suggestion: Use main branch and re-run the script"
            $AllMandatoryFilesPresent_Windows=$false
        }
        if(-not $CheckMandatoryFiles[2] -and $OSType -ne 0)
        {
            Write-Error "Mandatory component: StartupScript.sh missing from '$GithubBranch'. Suggestion: Use main branch and re-run the script"
            $AllMandatoryFilesPresent_Linux=$false
        }
        if(-not $CheckMandatoryFiles[3])
        {
            Write-Error "Mandatory component: azurerecovery-azmigrate-dummy-guid.conf missing from '$GithubBranch'. Suggestion: Use main branch and re-run the script"
            $AllMandatoryFilesPresent_Windows=$false
            $AllMandatoryFilesPresent_Linux=$false
        }
        if($OSType -eq 0 -and $AllMandatoryFilesPresent_Windows)
        { 
            for($i=0;$i -lt $FileNames.Length;$i++)
            {
                $FileUri=$PrefixFileUri+$FileNames[$i]
                $script:FileUris+=$FileUri            
            }
            Write-Host "Fetching Hydration Components from $GithubBranch Successful! " -ForegroundColor Green
            $script:HydVM_FetchHydCompFromGithubSuccessStatus = $true
        }
        elseif ($OSType -ne 0 -and $AllMandatoryFilesPresent_Linux) 
        {
            for($i=0;$i -lt $FileNames.Length;$i++)
            {
                $FileUri=$PrefixFileUri+$FileNames[$i]
                $script:FileUris+=$FileUri
            }
            Write-Host "Fetching Hydration Components from $GithubBranch Successful! " -ForegroundColor Green
            $script:HydVM_FetchHydCompFromGithubSuccessStatus = $true  
        }
        else 
        {
            Write-Error "Fetching Files from $GithubBranch Unsuccessful!"
        }
    }    
}

function HydVM_AttachCustomScriptExtensionWindows
{
    Write-Host "Attaching custom Script Extension to the Hydration Virtual Machine..." -ForegroundColor Green
    
    if(-not $AddCustomConfigSettings)
    {
        $AttachCSECheckStatus = Set-AzVMCustomScriptExtension -ResourceGroupName $ResourceGroupName -VMName $HydVM_Name -Location $Location -FileUri $script:FileUris -Run 'StartupScript.ps1 azmigrate-dummy-guid migration -HydrationConfigSettings EnableWindowsGAInstallation:true' -Name $HydVM_CustomScriptExtensionName  
        if(-not [string]::IsNullOrEmpty($AttachCSECheckStatus))
        {
            Write-Host "Attachment of custom Script Extension Successful!" -ForegroundColor Green
            $script:HydVM_AttachCSESuccessStatus = $true
        }
        else 
        {
            Write-Error "Attachment of custom Script Extension Unsuccessful!"
        }
    }
    else 
    {
        $CustomConfigSettings="";
        if($EnableDHCP -eq 1)
        {
            $CustomConfigSettings+="DHCP:true;"
        }
        else 
        {
            $CustomConfigSettings+="DHCP:false;"
        }
        if($EnableGA -eq 1)
        {
            $CustomConfigSettings+="EnableGA:true"
        }
        else 
        {
            $CustomConfigSettings+="EnableGA:false"
        }

        $AttachCSECheckStatus = Set-AzVMCustomScriptExtension -ResourceGroupName $ResourceGroupName -VMName $HydVM_Name -Location $Location -FileUri $script:FileUris -Run "StartupScript.ps1 azmigrate-dummy-guid migration -HydrationConfigSettings EnableWindowsGAInstallation:true -CustomConfigSettings $CustomConfigSettings" -Name $HydVM_CustomScriptExtensionName  
        if(-not [string]::IsNullOrEmpty($AttachCSECheckStatus))
        {
            Write-Host "Attachment of Custom Script Extension Successful!" -ForegroundColor Green      
            $script:HydVM_AttachCSESuccessStatus = $true
        }
        else 
        {
            Write-Error "Attachment of custom Script Extension Unsuccessful!"
        }
    }   
}

function HydVM_AttachCustomScriptExtensionLinux
{
    Write-Host "Attaching custom Script Extension to the Hydration Virtual Machine..." -ForegroundColor Green

    if(-not $AddCustomConfigSettings)
    {
        $Settings = @{"fileUris" = $script:FileUris; "commandToExecute" = "bash StartupScript.sh migration azmigrate-dummy-guid EnableLinuxGAInstallation:true "};
        $AttachCSECheckStatus = Set-AzVMExtension -ResourceGroupName $ResourceGroupName -Location $Location -VMName $HydVM_Name -Name "HydCustomScriptExtensionName" -Type "CustomScript" -Settings $Settings -TypeHandlerVersion "2.1" -Publisher "Microsoft.Azure.Extensions"
        if(-not [string]::IsNullOrEmpty($AttachCSECheckStatus))
        {
            Write-Host "Attachment of Custom Script Extension Successful!" -ForegroundColor Green
            $script:HydVM_AttachCSESuccessStatus = $true
        }
        else 
        {
            Write-Error "Attachment of Custom Script Extension Unsuccessful!"
        }
    }
    else 
    {
        $CustomConfigSettings="";
        if($EnableDHCP -eq 1)
        {
            $CustomConfigSettings+="DHCP:true;"
        }
        else 
        {
            $CustomConfigSettings+="DHCP:false;"
        }
        if($EnableGA -eq 1)
        {
            $CustomConfigSettings+="EnableGA:true"
        }
        else 
        {
            $CustomConfigSettings+="EnableGA:false"
        } 

        $Settings = @{"fileUris" = $script:FileUris; "commandToExecute" = "bash StartupScript.sh migration azmigrate-dummy-guid EnableLinuxGAInstallation:true $CustomConfigSettings"};
        $AttachCSECheckStatus = Set-AzVMExtension -ResourceGroupName $ResourceGroupName -Location $Location -VMName $HydVM_Name -Name "HydCustomScriptExtensionName" -Type "CustomScript" -Settings $Settings -TypeHandlerVersion "2.1" -Publisher "Microsoft.Azure.Extensions"
        if(-not [string]::IsNullOrEmpty($AttachCSECheckStatus))
        {
            Write-Host "Attachment of Custom Script Extension Successful!" -ForegroundColor Green      
            $script:HydVM_AttachCSESuccessStatus = $true
        }
        else 
        {
            Write-Error "Attachment of Custom Script Extension Unsuccessful!"
        }
    }    
}

function HydVM_DeleteVirtualMachine
{
    Write-Host "Deleting the Hydration Virtual Machine..." -ForegroundColor Green
    Remove-AzVM -ResourceGroupName $ResourceGroupName -Name $HydVM_Name -Force
}

function HydVM_DeleteOSDisk
{
    Write-Host "Deleting the Hydration VM's OS Disk..." -ForegroundColor Green
    Remove-AzDisk -ResourceGroupName $ResourceGroupName -DiskName $HydVM_OSDiskName -Force
}

function HydVM_DeleteNIC
{
    Write-Host "Deleting the Hydration VM's Network Interface..." -ForegroundColor Green
    Remove-AzNetworkInterface -Name $HydVM_NICName -ResourceGroup $ResourceGroupName -Force   
}

function HydVM_DeleteVirtualNetwork 
{
    Write-Host "Deleting the Hydration VM's Virtual Network..." -ForegroundColor Green
    Remove-AzVirtualNetwork -Name $HydVM_VirtualNetworkName -ResourceGroupName $ResourceGroupName -Force
}

function HydVM_DeleteNSG
{
    Write-Host "Deleting the Hydration VM's Network Security Group..." -ForegroundColor Green
    Remove-AzNetworkSecurityGroup -Name $HydVM_NSGName -ResourceGroupName $ResourceGroupName -Force
}

function TargetVM_CreateVirtualNetwork
{
    Write-Host "Creating Virtual Network for Target Virtual Machine..." -ForegroundColor Green
        
    $CreatedSubnet = New-AzVirtualNetworkSubnetConfig -Name $TargetVM_SubnetName -AddressPrefix $TargetVM_SubnetAddress
    $VirtualNetworkCheckStatus = New-AzVirtualNetwork -Name $TargetVM_VirtualNetworkName -ResourceGroupName $ResourceGroupName -Location $Location -AddressPrefix $TargetVM_VirtualNetworkAddress -Subnet $CreatedSubnet 
    if(-not ([string]::IsNullOrEmpty($VirtualNetworkCheckStatus)))
    {
        Write-Host "Creation of Virtual Network for Target Virtual Machine Successful!" -ForegroundColor Green      
        $script:TargetVM_VirtualNetworkSuccessStatus = $true
    }
    else 
    {
        Write-Error "Creation of Virtual Network for Target Virtual Machine Unsuccessful!"
    }
}

function TargetVM_CreateNIC
{
    Write-Host "Creating Network Interface for Target Virtual Machine..." -ForegroundColor Green

    $CreatedVirtualNetwork = Get-AzVirtualNetwork -ResourceGroupName $ResourceGroupName -Name $TargetVM_VirtualNetworkName
    $CreatedSubnet = Get-AzVirtualNetworkSubnetConfig -Name $TargetVM_SubnetName -VirtualNetwork $CreatedVirtualNetwork
    $NICCheckStatus = New-AzNetworkInterface -Name $TargetVM_NICName -ResourceGroupName $ResourceGroupName -Location $Location -Subnet $CreatedSubnet
    if(-not ([string]::IsNullOrEmpty($NICCheckStatus)))
    {
        Write-Host "Creation of Network Interface for Target Virtual Machine Successful!" -ForegroundColor Green      
        $script:TargetVM_NICSuccessStatus = $true
    }
    else 
    {
        Write-Error "Creation of Network Interface for Target Virtual Machine Unsuccessful!"
    }
}

function TargetVM_CreateNSG
{
    Write-Host "Creating Network Security Group for Target Virtual Machine..." -ForegroundColor Green

    $CreatedNSG = New-AzNetworkSecurityGroup -Name $TargetVM_NSGName -ResourceGroupName $ResourceGroupName  -Location  $Location
    if(-not ([string]::IsNullOrEmpty($CreatedNSG)))
    {
        $CreatedVirtualNetwork = Get-AzVirtualNetwork -ResourceGroupName $ResourceGroupName -Name $TargetVM_VirtualNetworkName
        Set-AzVirtualNetworkSubnetConfig -Name $TargetVM_SubnetName -VirtualNetwork $CreatedVirtualNetwork -AddressPrefix $TargetVM_SubnetAddress -NetworkSecurityGroup $CreatedNSG
        Set-AzVirtualNetwork -VirtualNetwork $CreatedVirtualNetwork
        Write-Host "Creation of Network Security Group for Target Virtual Machine Successful!" -ForegroundColor Green      
        $script:HydVM_NSGSuccessStatus = $true
    }
    else 
    {
        Write-Error "Creation of Network Security Group for Target Virtual Machine Unsuccessful!"  
    }  
}

function TargetVM_CreateVirtualMachineLinux
{
    #HardwareProfile 
    $NoOfSourceDisks = $NoOfDataDisks # 1 for source OS Disk
    $VMSize = Get-AzVMSize -Location $Location | Where {($_.NumberOfCores -gt '2') -and ($_.MemoryInMB -gt '2048') -and ($_.MaxDataDiskCount -gt $NoOfSourceDisks) -and ($_.ResourceDiskSizeInMB -ne 0)}
    $VirtualMachine = New-AzVMConfig -VMName $TargetVM_VirtualMachineName -VMSize $VMSize[0].Name

    $Disk = Get-AzDisk -ResourceGroupName $ResourceGroupName -DiskName $OSDiskName
    Set-AzVMOSDisk -VM $VirtualMachine -ManagedDiskId $Disk.Id -Name $OSDiskName -CreateOption "Attach" -Linux

    $NICObject = Get-AzNetworkInterface -Name $TargetVM_NICName
    $VirtualMachine = Add-AzVMNetworkInterface -VM $VirtualMachine -Id $NICObject.Id
    
    Write-Host "Creating the Virtual Machine..." -ForegroundColor Green
    New-AzVM -ResourceGroupName $ResourceGroupName -Location $Location -VM $VirtualMachine
    
}

function Main
{
    Start-Transcript
    Login

    if($script:LoginSuccessStatus -and $script:SubscriptionSuccessStatus)
    {
        HydVM_CreateVirtualNetwork
    }
    
    if($script:HydVM_VirtualNetworkSuccessStatus)
    {
        HydVM_CreateNIC
    }

    if($script:HydVM_NICSuccessStatus)
    {
        HydVM_CreateNSG
    }

    if($script:HydVM_NSGSuccessStatus)
    {
        if($OSType -eq 0)
        {
            HydVM_CreateVirtualMachineWindows
        }
        else 
        {
            HydVM_CreateVirtualMachineLinux
        }
    }

    if($script:HydVM_VirtualMachineSuccessStatus)
    {
        HydVM_AttachSourceDisks
    }
    
    if($script:HydVM_AttachOSDiskSuccessStatus -and $script:HydVM_AttachDataDisksSuccessStatus)
    {
        HydVM_FetchHydComponentsFromGithub
        if($script:HydVM_FetchHydCompFromGithubSuccessStatus)
        {
            if($OSType -eq 0)
            {
                HydVM_AttachCustomScriptExtensionWindows
            }
            else 
            {
                HydVM_AttachCustomScriptExtensionLinux 
            }
        }
        
    }

    if($script:HydVM_VirtualMachineSuccessStatus)
    {
        HydVM_DeleteVirtualMachine
        HydVM_DeleteOSDisk
    }

    if($script:HydVM_NICSuccessStatus)
    {
        HydVM_DeleteNIC
    }

    if($script:HydVM_VirtualNetworkSuccessStatus)
    {
        HydVM_DeleteVirtualNetwork
    }

    if($script:HydVM_NSGSuccessStatus)
    {
        HydVM_DeleteNSG
    }

    if($script:HydVM_AttachCSESuccessStatus)
    {
        TargetVM_CreateVirtualNetwork
        if($script:TargetVM_VirtualNetworkSuccessStatus)
        {
            TargetVM_CreateNIC
        }
        if($script:TargetVM_NICSuccessStatus)
        {
            TargetVM_CreateNSG
        }
        if($script:TargetVM_NICSuccessStatus)
        {
            if($OSType -eq 0)
            {
                TargetVM_CreateVirtualMachineWindows
            }
            else 
            {
                TargetVM_CreateVirtualMachineLinux
            }
        }    
    }
    
    Stop-Transcript  
}

#Entry-Point
Main
