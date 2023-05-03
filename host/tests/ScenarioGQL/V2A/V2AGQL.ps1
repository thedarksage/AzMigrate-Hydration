param(
	[Parameter(Mandatory=$false)]
	[String] $ConfigFile = "default",
	
	[Parameter(Mandatory=$true)]
	[String] $Username,
	
	[Parameter(Mandatory=$true)]
	[String] $Password,
	
	[Parameter(Mandatory=$false)]
	[String] $TestBinRoot = "C:\V2AGQL"
)	

$LogFile = "V2AGQL_Main.log"

cd $TestBinRoot

Remove-Item $LogFile
Remove-Item "V2AGQLLogs" -Recurse -Force 


if ($ConfigFile -eq "default")
{
	$ConfigFile = "V2AGQLConfig_" + (Get-Date).DayOfWeek.ToString() + ".xml"
}

Function logger {
	param(
		[Parameter(Mandatory=$true)]
		[String] $msg
	)
	$dateTime = Get-Date -Format "MM-dd-yyyy HH.mm.ss"
	$msg = $dateTime + ' : ' + $msg
	Write-output $msg | out-file $LogFile -Append
}

logger "ConfigFile: $ConfigFile"
logger "Username: $Username"
logger "TestBinRoot: $TestBinRoot"

[xml]$configXml = Get-Content $ConfigFile
$SharedPath = $configXml.V2AGQLConfig.Deployment.SharedLocation
$ResourceGroup = $configXml.V2AGQLConfig.AzureFabric.ResourceGroup

$global:FareastUser = $Username
$global:FareastPass = $Password
$global:SrcServerList = $configXml.V2AGQLConfig.InMageFabric.SourceServerList.SourceServer

#if(!(Test-Path "V2AGQLConfig.xml")) {
	Copy-Item -Path $ConfigFile -Destination V2AGQLConfig.xml -Force
#}

# Copy the binaries from shared location
logger "Copying binaries from $SharedPath to $TestBinRoot"
& NET USE Z: /D
& NET USE Z: $SharedPath /user:$Username $Password
if (!$?) {
	logger "Failed to access the shared path $SharedPath"
	exit 1
}

#Copy-Item -Path 'Z:\*' -Destination $TestBinRoot -Force
if (!$?) {
	logger "Failed to copy the binaries from $SharedPath to $TestBinRoot"
	exit 1
}

& NET USE Z: /D

# Initialize Repoting
logger "Initializing the report"
.\InitializeReporting.ps1
if (!$?) {
	logger "Initilization of consolidation report is failed"
	exit 1
}

# Disable all protected VMs
logger "Disabling the protected VMs and removing the replication policies"
.\V2AGQLDeployment.ps1 'DISABLEALLPROTECTIONSANDREMOVEPOLICIES'
if (!$?) {
	logger "Failed to disable the protected VMs or remove the replication plocies"
	exit 1
}

# Cleanup the resources in resource group
logger "Cleaning up the resources from resource group $ResourceGroup"
.\V2AGQLDeployment.ps1 'CLEANUPRG'
if (!$?) {
	logger "Failed to cleanup the resources in resource group $ResourceGroup"
	exit 1
}

# Start the source VM if powered-off
logger "Power-on the source VMs"
.\V2AGQLDeployment.ps1 'STARTSOURCEVMS'
if (!$?) {
	logger "Failed to power-on the source VMs"
	exit 1
}

# Uninstall the agents from source VMs
logger "Uninstalling the agent from the source VMs"
.\V2AGQLDeployment.ps1 'UNINSTALLAGENTS'
if (!$?) {
	logger "Failed to uninstall the agents from source VMs"
	exit 1
}

# Uninstall the Linux MT bits
logger "Uninstalling the Linux MT bits"
.\V2AGQLDeployment.ps1 'UNINSTALLLINUXMT'
if (!$?) {
	logger "Failed to uninstall the Linux MT bits"
	exit 1
}

# Remove the vCenter
logger "Removing the vCenter"
.\V2AGQLDeployment.ps1 'DELVCENTER'
if (!$?) {
	logger "Failed to remove the vCenter"
	exit 1
}

# Uninstall CS
logger "Uninstalling CS"
.\V2AGQLDeployment.ps1 'UNINSTALLCS'
if (!$?) {
	logger "Failed to uninstall CS"
	exit 1
}

Copy-Item -Path $ConfigFile -Destination 'V2AGQLConfig.xml' -Force

# Copy builds to TDM
logger "Copoying the builds to TDM"
.\V2AGQLDeployment.ps1 'COPYBUILDTOTDM'
if (!$?) {
	logger "Failed to copy the builds to TDM"
	exit 1
}

# Copy builds to TDM
#logger "Copoying the builds to TDM"
#.\V2AGQLDeployment.ps1 'COPYBUILDTOTDM'
#if (!$?) {
#	logger "Failed to copy the builds to TDM"
#	exit 1
#}

# Copy scripts to CS
logger "Copoying the scripts to CS"
.\V2AGQLDeployment.ps1 'COPYSCRIPTSTOCS'
if (!$?) {
	logger "Failed to copy the scripts to CS"
	exit 1
}

# Download vault credential file
logger "Downloading vault credential file"
.\V2AGQLDeployment.ps1 'DOWNLOADVAULTFILE'
if (!$?) {
	logger "Failed to download vault credential file"
	exit 1
}

# Install CS
logger "Installing CS"
.\V2AGQLDeployment.ps1 'INSTALLCS'
if (!$?) {
	logger "Failed to install CS"
	exit 1
}

# Add vCenter account
logger "Adding vCenter account"
.\V2AGQLDeployment.ps1 'ADDACCOUNTONCS'
if (!$?) {
	logger "Failed to add vCenter account"
	exit 1
}

# Discover vCenter
logger "Discovering vCenter"
.\V2AGQLDeployment.ps1 'ADDVCENTER'
if (!$?) {
	logger "Failed to discover vCenter"
	exit 1
}

# Installing Linux MT
logger "Installing Linux MT"
.\V2AGQLDeployment.ps1 'INSTALLLINUXMT'
if (!$?) {
	logger "Failed to install Linux MT"
	exit 1
}

# Copy the configuration file V2AGQLConfig.xml to shared location
logger "Copying the configuration file V2AGQLConfig.xml to $SharedPath"
& NET USE Z: /D
& NET USE Z: $SharedPath /user:$global:FareastUser $global:FareastPass
if (!$?) {
	logger "Failed to access the shared path $SharedPath"
	exit 1
}

Copy-Item -Path '.\V2AGQLConfig.xml' -Destination 'Z:\' -Force
if (!$?) {
	logger "Failed to copy the file V2AGQLConfig.xml to $SharedPath"
	exit 1
}

& NET USE Z: /D

logger "V2A GQL deployment succeeded"

logger "Starting the test pass for all source VMs"
foreach($source in $global:SrcServerList) {
	$VMName = $source.Name
	logger "Starting tests for the VM $VMName"
	$dir = "VM_" + $source.Name
	Remove-Item $dir -Recurse -Force
	New-Item -Path $dir -ItemType Directory -Force >> $null
	if ( !$? ) {
        logger "Failed to create the directory $dir for VM $VMName"
        continue
    }
	
	Copy-Item .\AddAccounts.exe,.\CommonFunctions.ps1,.\CommonParams.ps1,.\CSPSUninstaller.ps1,.\TestStatus.ps1,.\UpdateSourceInfo.ps1,.\V2AGQLConfig.xml,.\*.VaultCredentials,.\V2AGQLTest.ps1 -Destination $dir -Force
	if (!$?) {
		logger "Failed to copy the scripts to the directory $dir for VM $VMName"
		continue
	}

	logger "Updating the source information for VM $VMName"
	cd $dir
	.\UpdateSourceInfo.ps1 -VMName $source.Name -IPAddress $source.IPAddess -OSMode $source.OsType -User $source.UserName -Passwd $source.Password
	cd ..

	logger "Forking the thread for VM $VMName for 504D"
	cd $dir
	Start-process powershell.exe -ArgumentList "-file", .\V2AGQLTest.ps1, 'RUN540'
	cd ..
	Start-Sleep -Seconds 300
}

logger "Forked test runs for all source VMs"
exit 0