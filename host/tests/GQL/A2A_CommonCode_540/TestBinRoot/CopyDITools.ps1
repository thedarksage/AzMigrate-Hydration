Param
(
	[parameter(Mandatory=$false)]
	[String]$WorkingDir = "C:\TestBinRoot"
)

#
# Global Variables declaration
#
$global:LogFile = $WorkingDir + "\DITest.log"
$global:DIToolsZipFile = "DITestBinaries.zip"


#
# Create directory, if it already exist then delete it and then create a new.
#
function CreateDirectory()
{
    # Check whether dir exists and delete it
    if ( $(Test-Path -Path "$WorkingDir" -PathType Container) )
    {
        #Write-Host "Directory $WorkingDir already exist. Deleting it ..."
		Remove-Item $WorkingDir -Recurse -Force >> $null
    }

    # Create the directory
    #Write-Host "Creating the directory $WorkingDir ..."

    New-Item -Path "$WorkingDir" -ItemType Directory -Force >> $null
	
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
	$logfile = $WorkingDir + "\DItest.log"
	Add-Content $logfile -Value [$([DateTime]::Now)]$message
}

#
# Copy config files to working directory
#
function CopyFilesToWorkingDir ()
{
    Logger "INFO : Copying files to working directory ..."
	
	$files = @();
	$files = Get-ChildItem -Path $PWD
    foreach ( $file in $files )
    {
        $file = $file.Name
        if($file -ne "CopyDITools.ps1")
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
    }

    Logger "INFO : Successfuly copied all the files"
}

#
# Extract Executables to working directory
#
function ExtractExecutablesToDir ( )
{
    $zipfile = "$PWD\$DIToolsZipFile"
	
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
# Install Microsoft Storage Azure Tools
#
function InstallMSAzureTools()
{
	# Installing AzurePowershell in AzureVM(Source)
	$azCopyPath = "C:\Program Files (x86)\Microsoft SDKs\Azure\AzCopy\AzCopy.exe"
	
	if ( $(Test-Path "$azCopyPath" -PathType Leaf) )
	{
		Logger "INFO : AZCopy is already installed on source VM"
	}
	else
	{
		Logger "INFO : Install AzurePowershell in AzureVm(Source)from $WorkingDir\MicrosoftAzureStorageTools.msi"
		msiexec.exe /i $WorkingDir\MicrosoftAzureStorageTools.msi /qn
		
		Logger "INFO : Successfully installed AzureStorage tools"
		Start-Sleep(5)
	}
}


#
# Verify the list of files 
#
function VerifyDownloadedFiles ()
{
    # Prepare the downloaded files list and verify them
    $Files = "$PWD\$global:DIToolsZipFile"

    foreach ( $file in $Files )
    {
        Logger "INFO : Verifying the file $file"

        if ( !$(Test-Path "$file" -PathType Leaf) )
        {
            Write-Error "Error verifying file $file"

            exit 1
        }
    }

    Logger "INFO : All the files are verified"
}

#
# Extract Executables to working directory
#
function Main ()
{
    CreateDirectory
	
	CopyFilesToWorkingDir
	
    #VerifyDownloadedFiles
	
    ExtractExecutablesToDir
    
    InstallMSAzureTools  
}

Main
Write-Output "Success"