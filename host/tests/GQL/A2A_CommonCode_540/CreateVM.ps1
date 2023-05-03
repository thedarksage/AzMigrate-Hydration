param(
	# Script params
	[string] $createTag = $(if (Args -eq $null) { $null } else { Args[0] }),
	[int] $Debug = 0,
	
	# Sub params
    [string] $Location = "CentralUSEuap",
    [string] $SubId = '41b6b0c9-3e3a-4701-811b-92135df8f9e3',
	[string] $subUserName = "idcdlslb@microsoft.com",
    [string] $subPassword = '',
    [string] $vmUserName = "azureuser", 
    [string] $vmPassword = "",
	
	# VM Params
    # Prefix for all the resources created
	[string] $resPrefix = "AGQL-$(Get-Date -Format ddMM)",
	# Cheapest multi core
    [string] $vmSize = "Standard_B2s",
	[string] $storageType = "Standard_LRS",
    # Number of data disks
    [int] $nrDisks = 2
)

$distros = @{
	# Found using
	# az vm image list --output table --all
	
	# Key = @("OsType", "PublisherName", "Offer", "Sku")
	#RHEL8 = @("Linux", "RedHat", "RHEL", "8")
	RHEL7 = @("Linux", "RedHat", "RHEL", "7.6")
	RHEL6 = @("Linux", "RedHat", "RHEL", "6.10")
	DEB8 = @("Linux", "credativ", "Debian", "8")
	UBU18 = @("Linux", "Canonical", "UbuntuServer", "18.04-LTS")
	UBU16 = @("Linux", "Canonical", "UbuntuServer", "16.04-LTS")
	UBU14 = @("Linux", "Canonical", "UbuntuServer", "14.04.5-LTS")
	OL7 = @("Linux", "Oracle", "Oracle-Linux", "7.6")
	OL6 = @("Linux", "Oracle", "Oracle-Linux", "6.10")
	SLES12 = @("Linux", "SUSE", "SLES-BYOS", "12-SP4")
	SLES11 = @("Linux", "SUSE", "SLES-BYOS", "11-SP4")
	W2K19 = @("Windows", "ALL")
	W2K16 = @("Windows", "ALL")
}

# All tags should be upper case
$tags = @{
	HOTFIX = @("UBU18", "UBU16", "UBU14", "SLES12")
}

Function LogError()
{
	param([string] $msg)
	
	Write-Host "ERROR: $msg" -Foregroundcolor Red
	
	if ($Fatal) {
		Write-Host "Exit"
		exit 1
	}
}

Function LogErrorExit()
{
	param([string] $msg)
	
	Write-Host "FATAL: $msg" -Foregroundcolor Red
	exit 1
}

Function LogWarn()
{
	param([string] $msg)
	
	Write-Host "WARN: $msg" -Foregroundcolor Yellow
}


Function LogDebug()
{
	param([string] $msg)
	
	if ($Debug -eq 1) {
		Write-Host "DEBUG: $msg" -Foregroundcolor Yellow
    }
}

Function Log()
{
	param([string] $msg)
	
	Write-Host "$msg"
}


Function CreateVM()
{
	param ([string] $distro) 
	
	$rgName = $resPrefix + "-RG"
	$vmName = $resPrefix + "-" + "$distro"
	
	# Network
	$vnetName = $resPrefix + "-VNET"
	$snetName = $resPrefix + "-SNET"
	$nicName = $vmName + "-NIC"
	
	# Storage
	$osDiskName = $vmName + "-OD"
	$dataDiskPrefix = $vmName + "-DD"
	$diagSAName = $resPrefix + "-DIAG" # Converted to lower case
	
	# OS
	$osType = $distros[$distro][0]
	$pub = $distros[$distro][1]
	$offer = $distros[$distro][2]
	$sku = $distros[$distro][3]
	
    Log "OS->$pub->$offer->$sku"

    #$AzureOrgIdCredential = New-Object System.Management.Automation.PSCredential -ArgumentList $subUserName, $subPassword
	#Connect-AzAccount -Credential $sCreds

    Set-AzureRmContext -SubscriptionId $SubId 
	
	$azureVm = Get-AzureRmVM -ResourceGroupName $rgName -Name $vmName -ErrorAction SilentlyContinue
	if ($azureVm -ne $null) {
		LogWarn "VM already exists"
        return 0
    }

	Log "Creating VM Config"
	$newVM = New-AzureRmVMConfig -Name $vmName -VMSize $vmSize

	# Creds	
	$vmSecurePassword = ConvertTo-SecureString $vmPassword -AsPlainText -force 
	$vmCreds = New-Object System.Management.Automation.PSCredential($vmUserName, $vmSecurePassword)

	Log "Setting OS -> $osType"
	
    if ($osType -eq "Linux") {
	    $newVM = Set-AzureRmVMOperatingSystem -VM $newVM -Linux -ComputerName $vmName -Credential $vmCreds 
    } else {
        $newVM = Set-AzureRmVMOperatingSystem -VM $newVM -ComputerName $vmName -Credential $vmCreds
    }

	Log "Creating VM $vmName resources"

    Log "Create Resource Group $rgName"
    $rg = Get-AzureRmResourceGroup -Name $rgName -ErrorAction SilentlyContinue
    if ($rg -eq $null) {
        New-AzureRmResourceGroup -Name $rgName -Location $Location
    } else {
        Log "Resource Group $rgName already present"
    }
	
	Log "Configuring Storage"
		
	Log "Setting OsDisk = $osDiskName"
	$newVM = Set-AzureRmVMSourceImage -VM $newVM -PublisherName $pub -Offer $offer -Skus $sku -Version Latest
    $newVM = Set-AzureRmVMOSDisk -VM $newVM -Name $osDiskName -StorageAccountType $storageType -CreateOption FromImage -Caching ReadWrite

    # Create data disks and attach to VM
	$lunList=@()
    for ($i=0; $i -lt $nrDisks; $i++) {
        $dataDiskName = $dataDiskPrefix + ($i+1).ToString();
        $diskSize = $i + 2

		$diskConfig = New-AzureRmDiskConfig -AccountType $storageType -Location $Location -CreateOption Empty -DiskSizeGB $diskSize
		$dataDisk = New-AzureRmDisk -DiskName $dataDiskName -Disk $diskConfig -ResourceGroupName $rgName
		
        do {
			$lun = Get-Random -minimum 2 -maximum 10
		} while ($lunList -contains $lun)
		
		$lunList += $lun
		
		Log "Attache $dataDiskName ($diskSize GB) to Lun $lun"
        $vm = Add-AzureRmVMDataDisk -VM $newVM -Name $dataDiskName -CreateOption Attach -ManagedDiskId $dataDisk.Id -Lun $lun
    }
	
    $diagSAName = $diagSAName -replace "-"
    $diagSAName = $diagSAName.ToLower()

	Log "Creating boot diagnostics storage account $diagSAName"
	$diagSA = Get-AzureRmStorageAccount -Name $diagSAName -ResourceGroupName $rgName -ErrorAction SilentlyContinue
    if ($diagSA -eq $null) {
		$diagSA = New-AzureRmStorageAccount -Name $diagSAName -ResourceGroupName $rgName -Type $storageType -Location $Location
    } else {
		Log "$diagSAName already exists"
	}
	
	Set-AzureRmVMBootDiagnostics -VM $newVM -Enable -ResourceGroupName $rgName -StorageAccountName $diagSAName
	
	Log "Configuring network infrastructure"
	$vNet = Get-AzureRmVirtualNetwork -Name $vnetName -ResourceGroupName $rgName -ErrorAction SilentlyContinue
	if ($vnet -ne $null) {
		Log "vNet $vnetName already exists"
	} else {
		Log "Creating SubNet $snetName"
        $sNet = New-AzureRmVirtualNetworkSubnetConfig -Name $snetName -AddressPrefix '10.0.0.0/24'
		
		Log "Creating vNet $vnetName"
        $vNet = New-AzureRmVirtualNetwork -Name $vnetName -ResourceGroupName $rgName -Location $Location -AddressPrefix "10.0.0.0/16" -Subnet $sNet
	}

	Log "Creating NIC $nicName"
	$nic = New-AzureRmNetworkInterface -Name $nicName -Location $Location -ResourceGroupName $rgName -SubnetId $vNet.Subnets[0].Id

	# Attach the NIC
	Log "Attach NIC to VM"
	$newVM = Add-AzureRmVMNetworkInterface -VM $newVM -Id $nic.Id
	
	Log "Create VM"
    New-AzureRmVM -ResourceGroupName $rgName -Location $Location -VM $newVM -Verbose
}

Function Init()
{
	# Validate the tags refer to defined distros
	foreach ($tag in $tags.keys) {
		foreach ($distro in $tags[$tag]) {
			if ($distros.ContainsKey($distro)) {
				continue
			}
					
			LogErrorExit "Distro $distro referred by tag $tag not defined"
		}		
	}

    # Add ALL tag
	$tags.add("ALL", $distros.keys)

    # Add LINUX and WINDOWS tags
    $llist = @()  
    $wlist = @()
    # Add individual distro based on OS type to LINUX/WINDOWS tags
    foreach ($distro in $distros.keys ) {
        if ($distros[$distro][0] -eq "Linux") {
            $llist += $distro
        } else {
            $wlist += $distro
        }
    }

    $tags.add("LINUX", $llist)
    $tags.add("WINDOWS", $wlist)
  

	# Add individual distros as tags
	foreach ($distro in $distros.keys ) {
		$key = $distro | % ToString
		$key = $key.ToUpper()
		$tags.add($key, $key)
	}

    if ($createTag -eq "") {
	    Log "$($tags | Out-String)"
        Log "USAGE: <Script> <TAG NAME>"
        LogErrorExit "Enter valid TAG NAME"
    }
}

Init

$createTag = $createTag.ToUpper()
$dlist = @()

LogDebug "Searching $createTag"

# Check for all distros matching the input tag
foreach ($tag in $tags.keys) {
	$key = $tag | % ToString
	if ($key -match "$createTag") {
		LogDebug "$key matched"
		foreach ($distro in $tags[$tag]) {
			# Add each distro only once
			if ($dlist -contains $distro) {
				continue
			}
				
			$dlist += $distro
		}
	}		
}

if ($dlist.count -eq 0) {
	LogErrorExit "Cannot find any distro matching the tag"
}

Write-Host "Selected Distros: $dlist"

if ($subPassword -eq "" ) {
    $subPassword = Read-Host -prompt "Please enter your login password"
}

if ($vmPassword -eq "" ) {
    $vmPassword = Read-Host -prompt "Please enter your VM password"
}

# Add individual distros as tags
foreach ($distro in $dlist) {
	CreateVM $distro
}



