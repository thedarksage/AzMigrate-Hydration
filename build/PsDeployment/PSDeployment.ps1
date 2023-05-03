# PS Gallery Image Creation
	
# Configuration
$Config = $PSScriptRoot + "\PSConfig.xml"
[xml]$ConfigXml = Get-Content $Config


# Parse the Azure Environment config details
$AzureEnvironment = $ConfigXml.Configuration.AzureEnvironment
$SubscriptionName = $AzureEnvironment.Subscription
$ResourceGroup = $AzureEnvironment.ResourceGroup
$StorageAccount = $AzureEnvironment.StorageAccount
$StorageType = $AzureEnvironment.StorageType
$VnetName = $AzureEnvironment.VnetName
$ContainerName = $AzureEnvironment.ContainerName
$VhdContainerName = $AzureEnvironment.VhdContainerName
$SasExpiryDuration = $AzureEnvironment.SasExpiryDuration
$ApplicationId = $AzureEnvironment.ApplicationId
$TenantId = $AzureEnvironment.TenantId

# Parse the Image Config Details
$ImageConfig = $ConfigXml.Configuration.ImageConfig
$ImageName = $ImageConfig.ImageName
$ImageVersion = $ImageConfig.ImageVersion
$ReplicationGeos = $ImageConfig.ReplicationGeos
$PublisherName = $ImageConfig.PublisherName
$OfferName = $ImageConfig.OfferName
$SkuName = $ImageConfig.SkuName
$VMSize = $ImageConfig.VMSize
$DiskSizeGb = $ImageConfig.DiskSizeGb
$DataDiskName = $ImageConfig.DataDiskName
$LunNumber = $ImageConfig.LunNumber

# Parse the ProcessServer Config details
$PSConfig = $ConfigXml.Configuration.PSConfig
$Location = $PSConfig.Location
$UserName = $ImageConfig.Username
$Password = $ImageConfig.Password
$PfxIssuer = $ImageConfig.PfxIssuer
$VMName = $PSConfig.VMName
$global:PSBuildVersion = $PSConfig.PSFullVersion
$TestBinRoot = $PSScriptRoot + "\TestBinRoot"
$OSDiskName = "$VMName-OSDisk"
$NicName = $VMName + "-Nic01"

# Global Variables
$LogPath = $PSScriptRoot + "\PSLogs"
$CustomScriptFilesList = [System.Collections.ArrayList]@()

#
# Create log directory, if it already exist delete it and then create a new folder.
#
function CreateLogDir()
{
	# Delete the existing log dir
    if ( $(Test-Path -Path $LogPath -PathType Container) )
    {
        Write-Host "Directory $LogPath already exist. Deleting it ..."

        Remove-Item $LogPath -Recurse -Force >> $null
    }

    # Create the directory
    Write-Host "Creating the directory $LogPath ..."

    New-Item -Path $LogPath -ItemType Directory -Force >> $null
	
	if ( !$? )
    {
        Write-Error "Error creating working directory $LogPath"
        exit 1
    }
}

#
# Log messages to text file
#
function Log ([string]$message)
{
	$logfile = $LogPath + "\DeployPS.log"
	Write-output [$([DateTime]::Now)]$message | out-file $logfile -Append
}

#
# Set Azure Subscription
#
function SetAzureSubscription()
{
	try
    {
		Log "INFO : Logging into the Azure"
	
		$retry = 1
		$sleep = 0
		while ($retry -ge 0) {
			Start-Sleep -Seconds $sleep
			$cert = Get-ChildItem -path 'Cert:\CurrentUser\My' | where {$_.Subject -match $PfxIssuer }
			Start-Sleep -Seconds $sleep
			$ThumbPrint = $cert.ThumbPrint
			if (!$cert -or !$ThumbPrint) {
				if ($retry -eq 0) {
					Log "ERROR : Failed to fetch the thumbprint"
					exit 1
				}
				
				Log "ERROR : Failed to fetch the thumbprint..retrying"
				$retry = 0
				$sleep = 60
			} else {
				break
			}
		 }
		
		Log "INFO : Successfully fetched cert thumbprint details. ThumbPrint: $ThumbPrint, ApplicationId: $ApplicationId and TenantId: $TenantId"
		Login-AzAccount -ServicePrincipal -CertificateThumbprint $ThumbPrint -ApplicationId $ApplicationId -Tenant $TenantId
		if (!$?) {
			Log "ERROR : Unable to login to Azure account using Thumbprint: $ThumbPrint, ApplicationId: $ApplicationId and TenantId: $TenantId" 
			exit 1
		}
		
		Log "INFO : Login to Azure succeeded. Selecting subscription : $SubscriptionName" 
		Get-AzSubscription -SubscriptionName  $SubscriptionName | Select-AzSubscription
		if (!$?) {
			Log "ERROR : Failed to select the subscription {0}" -f $SubscriptionName
			exit 1
		}
		
		Log "INFO : Subscription - $SubscriptionName selected successfully." 
	}
    catch
    {
        # Exception resulted
        Log "ERROR : SetAzureSubscription Exception caught - " + $_
        exit 1
    }
}

#
# Remove ProcessServer VM if it exists already.
#
function RemoveAzureVM()
{
	Log "INFO : Checking if $VMName VM already exists."

	$VMInfo = Get-AzVM -ResourceGroupName $ResourceGroup -Name $VMName
	if ( $VMInfo.Name -eq  $VMName)
	{
		Log "INFO : $VMName VM exists."
	}
	else
	{
		Log "INFO : $VMName VM does not exist."
		return
	}

	Log "INFO : Deleting the VM: $VMName"
	Remove-AzVM -ResourceGroupName $ResourceGroup -Name $VMName -Force
	if(!$?)
	{
		Log "ERROR : Failed to delete the VM: $VMName"
		exit 1
	}
	else
	{
		Log "INFO : Succesfully deleted the VM: $VMName"
	}
}

#
# Fetch ProcessServer VM details
#
function GetAzureVM()
{
	Log "INFO : Fetching VM: '$VMName' details on ResourceGroup : $ResourceGroup"

	$VMInfo = Get-AzVM -ResourceGroupName $ResourceGroup -Name $VMName
	if(!$?)
	{
		Log "ERROR : Failed to fetch VM info: $VMName"
		exit 1
	}
	
	return $VMInfo
}

#
# Create ProcessServer VM and attach data disk to it
#
function DeployVM()
{
	try
    {
		Log "INFO : Setting Azure Subscription : $SubscriptionName"
		Set-AzContext -Subscription $SubscriptionName | out-null
		if(!$?)
		{
			Log "ERROR : Failed to Set Azure Subscription : $SubscriptionName"
			exit 1
		}
		
		Log "INFO : Creating new VM: $VMName"
		$SecurePassword = ConvertTo-SecureString $Password -AsPlainText -Force
        $VMCredential = New-Object System.Management.Automation.PSCredential ($UserName, $SecurePassword)
		
        $NewVM = New-AzVMConfig -VMName $VMName -VMSize $VMSize
        $NewVM = Set-AzVMOperatingSystem -VM $NewVM -Windows -ComputerName $VMName -Credential $VMCredential
		$Images = Get-AzVMImage -Location $Location -PublisherName $PublisherName -Offer $OfferName -Skus $SkuName | Sort-Object -Descending -Property PublishedDate
		$NewVM = Set-AzVMSourceImage -VM $NewVM -PublisherName $Images[0].PublisherName -Offer $Images[0].Offer -Skus $Images[0].Skus -Version $Images[0].Version
        $NewVM = Set-AzVMOSDisk -VM $NewVM -Name $OSDiskName -StorageAccountType $StorageType -CreateOption FromImage -Caching ReadWrite
		$VnetObject = Get-AzVirtualNetwork -Name $VnetName -ResourceGroupName $ResourceGroup -ErrorAction SilentlyContinue
        $NicObject = New-AzNetworkInterface -Name $NicName -Location $Location -ResourceGroupName $ResourceGroup -SubnetId $VnetObject.Subnets[0].Id -Force
		$NewVM = Add-AzVMNetworkInterface -VM $NewVM -Id $NicObject.Id
		
        New-AzVM -ResourceGroupName $ResourceGroup -Location $Location -VM $NewVM -Verbose | Out-Null        
		if(!$?)
		{
			Log "ERROR : Failed to create vm : $VMName"
			exit 1
		}
		
		$NsgName = "ProcessServer-nsg"
        $Nsg = Get-AzNetworkSecurityGroup -Name $NsgName
		
		$NsgRuleID = (Get-AzNetworkSecurityGroup -Name $NsgName -ResourceGroupName $ResourceGroup | Get-AzNetworkSecurityRuleConfig -Name "https").Id
		if($NsgRuleID -eq $null)
		{
			Log "INFO : Updating NSG rule to allow 443 traffic"
			Get-AzNetworkSecurityGroup -Name $NsgName -ResourceGroupName $ResourceGroup | 
			Add-AzNetworkSecurityRuleConfig -Name 'https' -Description "Allow 443" -Access Allow -Protocol Tcp -Direction Inbound -SourceAddressPrefix * -SourcePortRange * -DestinationAddressPrefix * -DestinationPortRange 443 -Priority 120 | Set-AzNetworkSecurityGroup
			if(!$?)
			{
				Log "ERROR : Unable to craete NSG rule to allow traffic for VM: $VMName"
				exit 1
			}
			else
			{
				Log "INFO : Updated NSG rule to allow trafficfor VM: $VMName"
			}
		}
		else
		{
			Log "INFO : NSG rule already exists."
		}

		Log "INFO : Attaching disks to the VM : $VMName"
		$VM = Get-AzVM -ResourceGroupName $ResourceGroup -Name $VMName
		$DiskConfig = New-AzDiskConfig -Location $Location -DiskSizeGB $DiskSizeGb -AccountType $StorageType -CreateOption Empty
        $NewDisk = New-AzDisk -Diskname $DataDiskName -ResourceGroupName $ResourceGroup -Disk $DiskConfig
        $VM = Add-AzVMDataDisk -VM $VM -Name $NewDisk.Name -CreateOption Attach -ManagedDiskId $NewDisk.Id -Lun $LunNumber
        
		Update-AzVM -ResourceGroupName $ResourceGroup -VM $VM
		if(!$?)
		{
			Log "ERROR : Failed to attach disks and update VM : $VMName"
			exit 1
		}
		
		Log "INFO : Successfully created VM: $VMName"
	}
	catch
    {
        # Exception resulted
        Log "ERROR : DeployVM : Exception caught - " + $_.Exception.Message
        Log "ERROR : DeployVM : Failed Item - " + $_.Exception.ItemName
        exit 1
    }
}

#
# Create Zip Folder of PS Test Binaries
#
function CreateZip($Dir)
{
	$ZipFile = $TestBinRoot + "\PSTestBinaries.zip"
	Log "INFO : Creating Zip File : $ZipFile"
	
	If(Test-path $ZipFile) {Remove-item $ZipFile}
	
	Add-Type -assembly "system.io.compression.filesystem"
	[io.compression.zipfile]::CreateFromDirectory($Dir, $ZipFile)
	if(!$?)
	{
		Log "ERROR : Failed to create Zip File : $ZipFile"
		exit 1
	}
	Log "INFO : Successfully created zip file : $ZipFile"
}

#
# Copy PS Installer Bits from Release path
#
function CopyBits()
{
	try
    {
		$Destination = $PSScriptRoot + "\PSTestBinaries"
		
		$BuildDate = $PSConfig.BuildDate
		$PSVersion = $PSConfig.PSVersion
		$BuildsPath = "\\INMSTAGINGSVR\DailyBuilds\Daily_Builds\"
		$PSBuild = $BuildsPath + $PSVersion + "\HOST\" + $BuildDate + "\release\MicrosoftAzureSiteRecoveryUnifiedSetup.exe"
				
		Log "INFO : PS build path: $PSBuild"
		if(!(Test-Path $TestBinRoot)) { New-Item $TestBinRoot -Type Container }
		
		# Remove the existing file
		$PSInstaller = $Destination + "MicrosoftAzureSiteRecoveryUnifiedSetup.exe"

		if(Test-Path $PSInstaller)
		{
			Remove-Item $PSInstaller -Force -Confirm:$false >> $null
			if ($? -eq "True")
			{
				Log "INFO : PS Installer '$PSInstaller' deleted successfully"
			}
		}
		
		#$global:PSBuildVersion = (Get-Item $PSBuild).VersionInfo.ProductVersion;
		Log "INFO : PS Version is $global:PSBuildVersion"
		Log "INFO : Copying '$PSBuild' to '$Destination'"
		Copy-Item $PSBuild -Destination $Destination -Force >> $null
		if(!$?)
		{
			Log "ERROR : Failed to copy $PSBuild to $Destination"
			exit 1
		}
		Log "INFO : Successfully copied PS installer bits from $PSBuild to $Destination path "
			
		CreateZip ($Destination)
	}
    catch
    {
        # Exception resulted
        Log "ERROR : CopyBits Exception caught - " + $_.Exception.Message
        exit 1
    }
}

#
# Populate Custom Script files list
#
function GetFileList()
{
	if ( !$(Test-Path -Path $TestBinRoot -PathType Container) )
	{
		Log "ERROR : Invalid folder path: $TestBinRoot"
		exit 1
	}
		
	$Scripts = Get-ChildItem "$TestBinRoot\*"
	if ( !$? )
	{
		Log "ERROR : Script files not found"
		exit 1
	}
	
	# Upload DI utilities
	foreach ( $File in $Scripts )
	{
		Log "INFO : Adding $File in CustomScriptFilesList"
		$CustomScriptFilesList.Add("$($File.Name)")
	}
}

#
# Upload test binaries and installer bits to storage account
#
function UploadFiles()
{
    Log "INFO : Uploading files to storage account : $StorageAccount"
	try
	{	
		$Scripts = Get-ChildItem "$TestBinRoot\*"
		if ( !$? )
		{
			Log "ERROR : Script files not found"
			exit 1
		}
		
		$StorageAccountKey = (Get-AzStorageAccountKey -ResourceGroupName $ResourceGroup -Name $StorageAccount)
	    if ($null -eq $StorageAccountKey) {
			Log "ERROR : Failed to retrieve storage account keys for account with name $StorageAccount in resource group $ResourceGroup. Exiting..."
			exit 1
	    }
		
	    $StorageAccountKey = $StorageAccountKey[0].Value
	    if ($null -eq $StorageAccountKey) {
			Log "ERROR : Failed to retrieve storage account key from keys array for account with name $StorageAccount in resource group $ResourceGroup. Exiting..."
			exit 1
	    }

	    $StorageAccountContext = New-AzStorageContext -StorageAccountName $StorageAccount -StorageAccountKey $StorageAccountKey

	    if (!(Get-AzStorageContainer -Name $ContainerName -Context $StorageAccountContext)) {
			New-AzStorageContainer -Name $ContainerName -Context $StorageAccountContext
			if (!(Get-AzStorageContainer -Name $ContainerName -Context $StorageAccountContext)) {
				Log "ERROR : Could not retrieve $ContainerName container from storage account $StorageAccount even after attempting to create the account. Exiting..."
				exit 1
			}
	    }
		
		# Upload DI utilities
		foreach ( $file in $Scripts )
		{
			Log "INFO : Uploading file: $file to Azure"
			
			Set-AzStorageBlobContent -Container $ContainerName -File $file -Blob $file.Name -Context $StorageAccountContext -Force | Out-Null
			if ( !$? )
			{
				Log "ERROR : Failed to upload file : $file to Azure"
				
			}
			else
			{
				Log "INFO : Successfully uploaded file: $file to Azure"
			}
		}
	}
	Catch
	{
		Log "ERROR : Upload Files : Caught Exception - $_.Exception.Message"
	}
	
	Log "INFO : Successfully uploaded PS Installer bits"
}

#
# Run Custom Extenstion to install PS bits
#
function SetCustomScriptExtension
{
	param(
        [Parameter(Mandatory=$True)]
        [string]
        $ScriptName,
		
		[Parameter(Mandatory=$True)]
        [string]
        $Param
	)
	
	if ($VM = Get-AzVM -ResourceGroupName $ResourceGroup -Name $VMName -ErrorAction Stop)
    {
        if ($ExistingCustomScriptExtension = $VM.Extensions | Where-Object VirtualMachineExtensionType -eq "CustomScriptExtension")
        {
			Log "INFO : Removing AzVMCustomScriptExtension from VM: $VMName"
			Remove-AzVMCustomScriptExtension -ResourceGroupName $ResourceGroup -VMName $VMName -Name $ExistingCustomScriptExtension.Name -Force -ErrorAction Stop
			Log "INFO : Removed AzVMCustomScriptExtension from VM: $VMName"
        }
    }
	
	Log "INFO : Creating custom script extension from uploaded file: $ScriptName."
	
	$VM = Get-AzVM -ResourceGroupName $ResourceGroup -Name $VMName
	$AzureVMName = $VM.Name
	$VMLocation = $VM.Location
	$StorageAccountKeys = (Get-AzStorageAccountKey -ResourceGroupName $ResourceGroup -Name $StorageAccount)
	$StorageAccountKey = $StorageAccountKeys[0].Value
	Set-AzVMCustomScriptExtension -ResourceGroupName $ResourceGroup -VMName $AzureVMName -Name $ScriptName -Location $VMLocation -StorageAccountName $StorageAccount -StorageAccountKey $StorageAccountKey -FileName $customScriptFilesList -ContainerName $ContainerName -RunFile $ScriptName -Argument $Param | Out-Null
	
	if( !$? )
	{
		Log "ERROR : Failed to set custom script extention for file: $ScriptName"
		exit 1
	}
	else
	{
		Log "INFO : Successfully set custom script extention for file: $ScriptName"
	}
}

#
# Fetch the custom extension script status
#
function GetExtensionStatus()
{
	# Viewing the  script execution output.
	Log "INFO : Fetching Custom Extension script status"
	
	$extNotFound = $true
	$message = "Starting"
    do
    {
		$vm = Get-AzVM -ResourceGroupName $ResourceGroup -Name $VMName
		$status = ($vm.Extensions | Where-Object { $_.VirtualMachineExtensionType -eq 'CustomScriptExtension' }).ProvisioningState
		
		if($message -ilike "Failed")
		{
			Log "ERROR : Custom script execution failed"
			exit 1
		}
		elseif(($status -ilike "Succeeded") -Or ($message -ilike "*Succeeded*"))
		{
			Log "INFO : Custom script execution succeeded"
            $extNotFound = $false
		}
		else
		{
			Log "INFO : Custom script current status: $status"
		}
    } while ($extNotFound)
}

#
# Reboot the ProcessServer
#
function RebootPS()
{
	Log "INFO : Rebooting the VM: $VMName"
	
	$RestartStatus = Restart-AzVM -ResourceGroupName $ResourceGroup -Name $VMName
	Log "INFO : Status: $RestartStatus"
	if ($RestartStatus.Status -ine "Succeeded"){
		Log "ERROR : Failed to reboot the VM: $VMName"
		exit 1
	}

	Log "INFO : Sleeping..."
	Start-Sleep -Seconds 30
	
	# Wait for server to reboot
	$ProvisioningState = (Get-AzVM -ResourceGroupName $ResourceGroup -Name $VMName -Status).Statuses[1].Code
	 
	While ($ProvisioningState -ne "PowerState/running")
	{
		Log "INFO : Waiting for Power On VM: $VMName. Current Status: $ProvisioningState"
		Start-Sleep -Seconds 5
	 
		$ProvisioningState = (Get-AzVM -ResourceGroupName $ResourceGroup -Name $VMName -Status).Statuses[1].Code
	}
	
	Log "INFO : Successfully restarted the VM: $VMName"
}

#
# Install ProcessServer
#
function InstallPS()
{
	Log "INFO : Install Process Server"
	SetCustomScriptExtension -ScriptName 'InstallPS.ps1' -Param 'D:\TestBinRoot'
	
	Log "INFO : Sleeping.."
	
	GetExtensionStatus
	
	RebootPS
}

#
# Update Config Files in ProcessServer
#
function RunCustomizations()
{
	Log "INFO : Run PS customization on ProcessServer"
	SetCustomScriptExtension -ScriptName 'CustomizePS.ps1' -Param 'D:\TestBinRoot'
	
	Start-Sleep -s 60
	
	GetExtensionStatus
	
	Start-Sleep -s 60
}

#
# RunSysprep on ProcessServer
#
function RunSysprep()
{
	Log "INFO : RunSysprep on ProcessServer"
	SetCustomScriptExtension -ScriptName 'RunSysprep.ps1' -Param 'D:\TestBinRoot'
	GetExtensionStatus
	Start-Sleep -Seconds 900
}

#
# Capture PS Gallery Image
#
function CreatePSGalleryImage()
{	
	$IsVmStopped = $false
	while (!$IsVmStopped)
	{
		$VMstatus = Get-AzVM -ResourceGroupName $ResourceGroup -Name $VMName -Status
		foreach($Status in $VMstatus.Statuses)
		{
		   $Code = $Status.Code
		   Log "INFO : Ran sysprep..Waiting...Current Status = $Code"
		   if($Code.Contains("PowerState/stopped"))
		   {
				$IsVmStopped = $true
				break
			}
		}
		Start-Sleep -Seconds 10
	}
	
	Log "INFO : Stopping $VMName VM"
	Stop-AzVM -ResourceGroupName $ResourceGroup -Name $VMName -Force > $null
	if ( !$? )
	{
		Log "ERROR : Failed to stop $VMName VM"
	}
	
	$IsVmStopped = $false
	while (!$IsVmStopped)
	{
		$VMstatus = Get-AzVM -ResourceGroupName $ResourceGroup -Name $VMName -Status
		foreach($Status in $VMstatus.Statuses)
		{
		   $Code = $Status.Code
		   Log "INFO : Dellocating the VM...Current Status = $Code"
		   if($Code.Contains("PowerState/deallocated"))
		   {
				$IsVmStopped = $true
				break
			}
		}
		Start-Sleep -Seconds 10
	}
	Log "INFO : $VMName VM is successfully stopped"
	
	$VM = Get-AzVM -Name $VMName -ResourceGroupName $ResourceGroup
	$OSDisk = $VM.StorageProfile.OsDisk.Name
	$DataDisk = $VM.StorageProfile.DataDisks[0].Name
	$VHDCreationTime = Get-Date -Format "yyyy-MM-dd-HH-mm-ss"
	$OsDiskVHDFileName = "AzureSiteRecovery-" + $VMName + "-" + $VHDCreationTime + ".vhd"
	$DataDiskVHDFileName = "AzureSiteRecovery-" + $VMName + "-" + $DataDisk + $VHDCreationTime + ".vhd"
	$OSDiskSas = Grant-AzDiskAccess -ResourceGroupName $ResourceGroup -DiskName $OSDisk -DurationInSecond $SasExpiryDuration -Access Read
	$DataDiskSas = Grant-AzDiskAccess -ResourceGroupName $ResourceGroup -DiskName $DataDisk -DurationInSecond $SasExpiryDuration -Access Read
	
	$StorageAccountKey = (Get-AzStorageAccountKey -ResourceGroupName $ResourceGroup -Name $StorageAccount)
	if ($null -eq $StorageAccountKey) {
		Log "ERROR : Failed to retrieve storage account keys for account with name $StorageAccount in resource group $ResourceGroup. Exiting..."
		exit 1
	}
	
	$StorageAccountKey = $StorageAccountKey[0].Value
	if ($null -eq $StorageAccountKey) {
		Log "ERROR : Failed to retrieve storage account key from keys array for account with name $StorageAccount in resource group $ResourceGroup. Exiting..."
		exit 1
	}

	$StorageAccountContext = New-AzStorageContext -StorageAccountName $StorageAccount -StorageAccountKey $StorageAccountKey

	if (!(Get-AzStorageContainer -Name $VhdContainerName -Context $StorageAccountContext)) {
		New-AzStorageContainer -Name $VhdContainerName -Context $StorageAccountContext
		if (!(Get-AzStorageContainer -Name $VhdContainerName -Context $StorageAccountContext)) {
			Log "ERROR : Could not retrieve $VhdContainerName container from storage account $StorageAccount even after attempting to create the account. Exiting..."
			exit 1
		}
	}

	Start-AzStorageBlobCopy -AbsoluteUri $OSDiskSas.AccessSAS -DestContainer $VhdContainerName -DestContext $StorageAccountContext -DestBlob $OsDiskVHDFileName -Force
	if ( !$? )
	{
		Log "ERROR : Failed to copy $VMName OS disk to container : $VhdContainerName"
		exit 1
		
	}
	else
	{
		Log "INFO : Successfully copied $VMName OS disk to container: $VhdContainerName"
	}
	
	Start-AzStorageBlobCopy -AbsoluteUri $DataDiskSas.AccessSAS -DestContainer $VhdContainerName -DestContext $StorageAccountContext -DestBlob $DataDiskVHDFileName -Force
	if ( !$? )
	{
		Log "ERROR : Failed to copy $VMName Data disk to container : $VhdContainerName"
		exit 1
		
	}
	else
	{
		Log "INFO : Successfully copied $VMName Data disk to container: $VhdContainerName"
	}
	
	Set-AzVm -ResourceGroupName $ResourceGroup -Name $VMName -Generalized
	$Image = New-AzImageConfig -Location $Location -SourceVirtualMachineId $VM.Id
	New-AzImage -Image $Image -ImageName $ImageName -ResourceGroupName $ResourceGroup
	if ( !$? )
	{
		Log "ERROR : Failed to capture image: $ImageName for VM: $VMName"
		exit 1
	}
	Log "INFO : Sleeping.."
	Start-Sleep -Seconds 60
	
	Get-AzImage -ResourceGroupName $ResourceGroup -ImageName $ImageName
	if ( !$? )
	{
		Log "ERROR : Failed to fetch image: $ImageName"
		exit 1
	}
	Log "INFO : Successfully created image: $ImageName"
}

#
# Validate ProcessServer VM
#
function ValidatePS()
{
	Log "INFO : Validate bits on ProcessServer"
	
	Log "INFO : PSBuildVersion : $global:PSBuildVersion"
	SetCustomScriptExtension -ScriptName 'ValidatePS.ps1' $global:PSBuildVersion
	GetExtensionStatus
	
	Log "INFO : Validation is passed on ProcessServer"
}

#
# Deploy VM from ProcessServer Gallery Image
#
function DeployPSFromImage()
{
	$VM = "TestImage"
	$SubnetName = "default"
	$SecurePassword = ConvertTo-SecureString $Password -AsPlainText -Force
    $VMCredential = New-Object System.Management.Automation.PSCredential ($UserName, $SecurePassword)
		
	New-AzVm `
            -ResourceGroupName $ResourceGroup `
            -Name $VM `
            -Location $Location `
			-VirtualNetworkName $VnetName `
            -SubnetName $SubnetName `
            -ImageName $ImageName `
            -Credential $VMCredential | Out-Null
			
	if( !$? )
	{
		Log "ERROR : Failed to deploy $VM VM from $ImageName"
		exit 1
	}
	Log "INFO : Successfully deployed $VM from $ImageName"
	
	$ProvisioningState = (Get-AzVM -ResourceGroupName $ResourceGroup -Name $VM -Status).Statuses[1].Code
	 
	While ($ProvisioningState -ne "PowerState/running")
	{
		Log "INFO : Waiting for Power On VM: $VM. Current Status: $ProvisioningState"
		Start-Sleep -Seconds 5
	 
		$ProvisioningState = (Get-AzVM -ResourceGroupName $ResourceGroup -Name $VM -Status).Statuses[1].Code
	}
	
	$VMName = $VM
	ValidatePS
	Log "INFO : Successfully validated $VM"
	
	Start-Sleep -Seconds 120
	
	# Removing test VM deployed from image.
	Remove-AzVM -ResourceGroupName $ResourceGroup -Name $VM -Force
	DeleteNetworkInterface -ResourceGroup $ResourceGroup -NicName $VM
	DeletePublicIP -ResourceGroup $ResourceGroup -PublicIPName $VM
	DeleteNSG -ResourceGroup $ResourceGroup -NSGName $VM
}

#
# Publish PS Gallery Image
#
function PublishPSGalleryImage()
{
	$description = "The Microsoft Azure Site Recovery Process Server is used for caching, compression, and encryption of replication traffic in the Microsoft Azure Site Recovery scenario of settingup failback of virtual machines from Microsoft Azure back to an on-premises VMware site."
	$label = "Microsoft Azure Site Recovery Process Server V2"
	$date = Get-Date -Format g
	$version = $ImageVersion + ".00"
	
	import-module 'C:\Program Files (x86)\Microsoft SDKs\Azure\PowerShell\ServiceManagement\Azure\Compute\PIR.psd1'
	
	Update-AzureVMImage -ImageName $ImageName -Description $description -ImageFamily $label -Label $label -PrivacyUri "http://go.microsoft.com/fwlink/?LinkId=512132" -RecommendedVMSize "ExtraLarge" -DontShowInGui "True" -PublishedDate $date
	if ( !$? )
	{
		Log "ERROR : Failed to update $ImageName image properties"
		exit 1
	}
	
	Update-AzureVMImage -ImageName $ImageName -Label $label -Eula "http://go.microsoft.com/fwlink/?LinkId=525741"
	if ( !$? )
	{
		Log "ERROR : Failed to update $ImageName image eula"
		exit 1
	}
	
	if($Cloud -eq "MoonCake")
	{
		$geos = @('China East', 'China North')
	}
	elseif($Cloud -eq "FairFax")
	{
		$geos = @('USGov Iowa', 'USGov Virginia', 'USDoD Central', 'USDoD East', 'USGov Texas', 'USGov Arizona')
	}
	elseif($Cloud -eq "BlackForest")
	{
		$geos = @('Germany Central', 'Germany North East')
	}
	else
	{
		#$geos = @('East US', 'North Europe', 'West Europe', 'East Asia', 'Japan East','Japan West', 'Australia East', 'Australia Southeast', 'Southeast Asia', 'West US', 'Brazil South','Central India', 'South India', 'East US 2', 'Central US', 'North Central US', 'South Central US', 'West Central US', 'West US 2', 'UK West', 'UK South', 'Canada Central', 'Canada East', 'UK South 2', 'UK North', 'East US 2 EUAP', 'Central US EUAP')
		$geos = @('East Asia', 'Southeast Asia')
	}
	
	$PSIC = New-AzurePlatformComputeImageConfig -Offer "Process-Server" -Sku "Windows-2012-R2-Datacenter" -Version $version
	Set-AzurePlatformVMImage -ImageName $ImageName -ReplicaLocations $geos -ComputeImageConfig $PSIC
	if ( !$? )
	{
		Log "ERROR : Failed to replicate $ImageName image to $ReplicationGeos"
		exit 1
	}
	
	Set-AzurePlatformVMImage -ImageName $ImageName -Permission "Public"
	if ( !$? )
	{
		Log "ERROR : Failed to mark $ImageName as public"
		exit 1
	}
	
	$ReplicationStatus = GetReplicationStatus
	while($ReplicationStatus -ne "0")
	{
		Log "INFO : Waiting for the replication to be completed in all the geos..."
		Start-Sleep -Seconds 15
	 
		$ReplicationStatus = GetReplicationStatus
	}
	
	Log "INFO : $ImageName Replication is completed"
}

function GetReplicationStatus()
{
	# Wait for server to reboot
	$repArr = @()
	$repArr = Get-AzurePlatformVMImage -ImageName $ImageName | select –ExpandProperty "ReplicationProgress"
	Log "INFO : Replication Status : $repArr"
	
	$RepStatus = @()
	for ($i=0; $i -lt $repArr.length; $i++) 
	{
		$geo = $repArr[$i].Location
		$status = $repArr[$i].Progress
		if($status -ne '100')
		{
			Log "INFO : Replication is not completed in $geo, current status is $status %"
			$RepStatus += $status
		}
		else
		{
			Log "INFO : Replication finished in $geo"
		}
	}
	
	Log "Arr - " + $RepStatus
	if($RepStatus.count -gt 0)
	{
		if($RepStatus -notcontains "100")
		{
			Log "Replication not yet completed"
			return $false
		}
	}
	
	return $true
}

#Import certificate
function Import-PfxCertificate {

    param(
        [String]$PfxFilePath,
        [String]$Password)    

	$absolutePfxFilePath = Resolve-Path -Path $PfxFilePath
    Log "INFO : Importing store certificate '$absolutePfxFilePath'..."

	try	{
		Add-Type -AssemblyName System.Security
        $cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2
        $cert.Import($absolutePfxFilePath, $Password, [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]"PersistKeySet")
        $store = new-object system.security.cryptography.X509Certificates.X509Store -argumentlist "MY", CurrentUser
        $store.Open([System.Security.Cryptography.X509Certificates.OpenFlags]::"ReadWrite")
        $store.Add($cert)
        $store.Close()
		Log "INFO : Imported store certificate '$absolutePfxFilePath' successfully."
	}
	catch
	{
		Write-Error $_.ToString()
        Log "ERROR : Failed to import certificate"
		exit 1
	}
}

### Function to delete disks from Resource Group
function DeleteDisksFromRg
{
    param(
        [Parameter(Mandatory=$True)]
        [string]
        $ResourceGroup,
		
		[Parameter(Mandatory=$True)]
        [string]
        $DiskName
    )

    $Disk = Get-AzDisk -ResourceGroupName $ResourceGroup -DiskName $DiskName
	
	if ( $? )
	{
		if ($Disk.ManagedBy -eq $null -and $Disk.DiskState -notmatch "Active")
		{
			Log "INFO : Deleting the disk: $DiskName from the RG : $ResourceGroup"
			Remove-AzDisk -DiskName $DiskName -ResourceGroupName $ResourceGroup -Force

			if ( !$? )
			{
				Log "INFO : Deleted the disk: $DiskName from the RG : $ResourceGroup"
			}
			else 
			{
				Log "ERROR : Failed to delete the disk: $DiskName from the RG : $ResourceGroup"
			}
		}
		else
		{
			Log "INFO : Disk: $DiskName is in use. Not deleting."	
		}
	}
	else
	{
		Log "INFO : Disk: $DiskName not exists."
	}
}


### Function to delete Network Interface
function DeleteNetworkInterface
{
    param(
        [Parameter(Mandatory=$True)]
        [string]
        $ResourceGroup,

        [Parameter(Mandatory=$True)]
        [string]
        $NicName
    )

    $filterName = $NicName + "*";
    $NicObjectList = Get-AzNetworkInterface -ResourceGroupName $ResourceGroup | Where-Object {$_.Name -like $filterName}

    foreach ($NicObject in $NicObjectList)
    {
        Log "INFO : Removing network interface card from resource group:$ResourceGroup"
        Remove-AzNetworkInterface -Name $NicObject.Name -ResourceGroupName $ResourceGroup -Force
    }
}

### Function to delete Public IP address
function DeletePublicIP
{
    param(
        [Parameter(Mandatory=$True)]
        [string]
        $ResourceGroup,

        [Parameter(Mandatory=$True)]
        [string]
        $PublicIPName
    )

    $filterName = $PublicIPName + "*";
    $PublicIPList = Get-AzPublicIpAddress -ResourceGroupName $ResourceGroup | Where-Object {$_.Name -like $filterName}

    foreach ($PublicIP in $PublicIPList)
    {
        Log "INFO : Removing public IP address from resource group:$ResourceGroup"
		Remove-AzPublicIpAddress -Name $PublicIP.Name -ResourceGroupName $ResourceGroup -Force
    }
}

### Function to delete Network security group
function DeleteNSG
{
    param(
        [Parameter(Mandatory=$True)]
        [string]
        $ResourceGroup,

        [Parameter(Mandatory=$True)]
        [string]
        $NSGName
    )

    $filterName = $NSGName + "*";
    $NSGList = Get-AzNetworkSecurityGroup -ResourceGroupName $ResourceGroup | Where-Object {$_.Name -like $filterName}

    foreach ($NSG in $NSGList)
    {
        Log "INFO : Removing NSG from resource group:$ResourceGroup"
		Remove-AzNetworkSecurityGroup -Name $NSG.Name -ResourceGroupName $ResourceGroup -Force
    }
}


### Function to claean-up stale entries of vm, disks, nic from resource group
function CleanUpStaleEntriesFromRg
{
    param(
        [Parameter(Mandatory=$True)]
        [string]
        $ResourceGroup,

        [Parameter(Mandatory=$True)]
        [string]
        $NicName
    )

    DeleteDisksFromRg -ResourceGroup $ResourceGroup -DiskName $OSDiskName
	DeleteDisksFromRg -ResourceGroup $ResourceGroup -DiskName $DataDiskName
    DeleteNetworkInterface -ResourceGroup $ResourceGroup -NicName $NicName
}

#
# Main Function
#
function Main()
{
	# Set TLS-1.2 as security protocol
	[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

	Log "INFO : Is 64 bit process : "
	
	[Environment]::Is64BitProcess
	
	Set-ExecutionPolicy Unrestricted -Scope CurrentUser -Force

    CreateLogDir
	
	SetAzureSubscription

	CopyBits

	RemoveAzureVM
	
	CleanUpStaleEntriesFromRg -ResourceGroup $ResourceGroup -NicName $NicName
	
	DeployVM

	GetFileList
	
	UploadFiles
	
	InstallPS

	RunCustomizations
	
	RunSysprep
	
	CreatePSGalleryImage
	
	DeployPSFromImage
exit
	PublishPSGalleryImage
}

Main
