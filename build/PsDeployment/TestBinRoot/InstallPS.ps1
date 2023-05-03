Param
(
	[parameter(Mandatory=$false)]
	[String]$WorkingDir = "D:\TestBinRoot"
)

# Global Variables
$PSToolsZipFile = "PSTestBinaries.zip"

#
# Create directory, if it already exist then delete it and then create a new.
#
function CreateDirectory()
{
	#
    # Check wheather dir exists
    #
    if ( $(Test-Path -Path "$WorkingDir" -PathType Container) )
    {
        Write-Host "Directory $WorkingDir already exist. Deleting it ..."

        Remove-Item $WorkingDir -Recurse -Force
    }

    #
    # Create the directory
    #
    Write-Host "Creating the directory $WorkingDir ..."

    New-Item -Path "$WorkingDir" -ItemType Directory -Force
	
	if ( !$? )
    {
        Write-Error "Error creating working directory $WorkingDir"
		Write-Output "Failure"
        exit 1
    }
}

#
# Log messages to text file
#
function Logger ([string]$message)
{
	$logfile = $WorkingDir + "\DeployPS.log"
	#Write-output [$([DateTime]::Now)]$message | out-file $logfile -Append
	Add-Content $logfile -Value [$([DateTime]::Now)]$message
}

#
# Create PS Installation Drive
#
function CreateInstallDir()
{	
	Logger "INFO : Entered CreateInstallDir"
	
	mountvol E:\  /D
	if(!$?)
	{
		Logger "ERROR : Failed to remove drive letter from CD Rom"
		Write-Output "Failure"
		exit 1
	}
	Logger "INFO : Removed 'E' drive letter from CD Rom drive"
	
	
	$disk = Get-Disk | where-object PartitionStyle -eq "RAW"
	if(!$?)
	{
		Logger "ERROR : Failed to fetch RAW disks"
		Write-Output "Failure"
		exit 1
	}
	
	Initialize-Disk -Number $disk.Number -PartitionStyle MBR -confirm:$false  
	if(!$?)
	{
		Logger "ERROR : Failed to intialize disk $disk.Number"
		Write-Output "Failure"
		exit 1
	}
	
	New-Partition -DiskNumber $disk.Number -UseMaximumSize -IsActive | Format-Volume -FileSystem NTFS -NewFileSystemLabel "HOME" -confirm:$False  
	if(!$?)
	{
		Logger "ERROR : Failed to create partition "
		Write-Output "Failure"
		exit 1
	}
	
	Set-Partition -DiskNumber $disk.Number -PartitionNumber 1 -NewDriveLetter E  
	if(!$?)
	{
		Logger "ERROR : Failed to assign E drive letter to PS installation drive"
		Write-Output "Failure"
		exit 1
	}
	Logger "INFO : Successfully created PS installation drive"
}

#
# Copy config files to working directory
#
function CopyFilesToWorkingDir ()
{
    Logger "INFO : Copying files to working directory ..."
	
	$files = "$PWD\$global:PSToolsZipFile"
    foreach ( $file in $files )
    {
        Logger "INFO : Copying $file --> $WorkingDir"

        Copy-Item -Path "$file" -Destination "$WorkingDir" -Force
        if( !$? )
        {
            Logger "ERROR : Failed to copy $file to $WorkingDir path"
			Write-Output "Failure"
			exit 1
        }
    }

    Logger "INFO : Successfuly copied all the files"
}

#
# Extract Executables to working directory
#
function ExtractExecutablesToDir ( )
{
    $zipfile = "$PWD\$PSToolsZipFile"

    Logger "INFO : Extracting $zipfile to $WorkingDir"

    $copyflags = [int](
                  4   +   # No progress dlg
                  16  +   # Yes to all
                  512 +   # No conformation for create dir
                  1024)   # No error dlg

    $shell = New-Object -ComObject shell.application

    $zipedFiles = $shell.NameSpace($zipfile)
    if ( $zipedFiles -eq $null )
    {
        Logger "ERROR : Could not open the zip file $zipfile"
		Write-Output "Failure"
        exit 1
    }

    foreach ( $file in $zipedFiles.items() )
    {
        $shell.NameSpace($WorkingDir).copyhere($file, $copyflags)
        if( !$? )
        {
            Logger "ERROR : Error extracting files from $zipfile"
			Write-Output "Failure"
            exit 1
        }
    }

    Logger "INFO : Successfuly extracted the files from $zipfile"
	
	#
    # Move to working directory
    #
    Logger "INFO : Changing directory to $WorkingDir"
    cd $WorkingDir
    if( !$? )
    {
        Logger "ERROR : Can't change directory to $WorkingDir"
		Write-Output "Failure"
		exit 1
    }
}

#
# Remove special permissions on Installation root directory
#
function RemovePermissions()
{
	$RootDir = "E:\"
	$ACL = Get-Acl $RootDir
	$AppendDataRule = New-Object System.Security.AccessControl.FileSystemAccessRule ("BUILTIN\Users", "AppendData", "ContainerInherit", "None", "Allow")
	$CreateFilesRule = New-Object System.Security.AccessControl.FileSystemAccessRule ("BUILTIN\Users", "CreateFiles", "ContainerInherit", "InheritOnly", "Allow")
	$ACL.RemoveAccessRule($AppendDataRule)
	Set-Acl -Path $RootDir -AclObject $ACL
	$ACL.RemoveAccessRule($CreateFilesRule)
	Set-Acl -Path $RootDir -AclObject $ACL
	
	Logger "INFO : Successfully removed AppendData and CreateFiles permissions for BUILTIN\Users"
}

#
# Install Microsoft Storage Azure Tools
#
function InstallMSAzureTools()
{
	# Installing AzurePowershell in AzureVM(Source)
	Logger "INFO : Install AzurePowershell in AzureVm(Source)"
	Logger "INFO : $WorkingDir\PSTestBinaries\MicrosoftAzureStorageTools.msi"
	msiexec.exe /i $WorkingDir\DITestBinaries\MicrosoftAzureStorageTools.msi /qn
	
	Logger "INFO : Successfully installed AzureStorage tools"
	Start-Sleep(5)
}

#
# Install ProcessServer on Azure VM
#
function InstallPS()
{
	Logger "INFO : Installing ProcessServer bits"
	
	$outFile = $WorkingDir + "\out"
	If(Test-path $outFile) { Remove-item $outFile -Force }
	$cmd = $WorkingDir + '\MicrosoftAzureSiteRecoveryUnifiedSetup.exe' + ' /q /x:' + $WorkingDir
	$cmd | Out-File $outFile -Append
	cmd.exe /c $cmd

	Start-Sleep -s 20
	$installLocation = "E:\Program Files (x86)\Microsoft Azure Site Recovery"
	$passphrasePath = $WorkingDir + "\connection.passphrase"
	$csIPAddress = "10.218.0.5"
	$serverMode = "PS"
	$envType = "NonVMWare"
	$installcmd = $WorkingDir + '\UnifiedSetup.exe' + ' /AcceptThirdpartyEULA /ServerMode "' + $serverMode + '" /InstallLocation ' + "'" + $installLocation + "'" + ' /CSIP ' + "'" + $CSIPAddress +"'" + ' /EnvType ' + "'" + $envType + "'" + ' /PassphraseFilePath ' + "'" + $passphrasePath + "'" + ' /SkipSpaceCheck' 

	$installcmd | Out-File $outFile -Append
	powershell.exe $installcmd >> $outFile
	
	$output = Get-Content $outFile

	Logger "INFO : PS Installation Output : $output"

	$results = Select-String "failed" -InputObject $output

	If ($results -eq $null)
	{
		Logger "INFO : Successfully installed ProcessServer bits"
	}
	else
	{
		Logger "ERROR : ProcessServer installation failed"
		Write-Output "Failure"
		exit 1
	}
	Start-Sleep -s 60
}

#
# Uninstall Master Target
#
function UninstallMT()
{
	Logger "INFO : Uninstalling MT and MARS"
	#$applications = @("Microsoft Azure Site Recovery Mobility Service/Master Target Server", "Microsoft Azure Recovery Services Agent")
	$applications = @("Microsoft Azure Site Recovery Mobility Service/Master Target Server")
	
	foreach ($app in $applications)
	{
		$agent = Get-WmiObject -Class Win32_Product -Filter "Name = '$app'"
		$agent.Uninstall()
		
		if( !$? )
		{
			Logger "ERROR : Failed to uninstall $app"
			Write-Output "Failure"
			exit 1
		}
		Start-Sleep -Seconds 10
		Logger "INFO : Successfully uninstalled $app"
	}

	Write-Output "Success"
}

#
# Main Function
#
function Main()
{
	CreateDirectory
	
	CreateInstallDir
	
	CopyFilesToWorkingDir
	
	ExtractExecutablesToDir
	
	RemovePermissions
	
	#InstallMSAzureTools
	
	InstallPS
	
	UninstallMT
}

Main