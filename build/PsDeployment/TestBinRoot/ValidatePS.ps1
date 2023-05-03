Param
(
	[parameter(Mandatory=$false)]
	[String]$PSBuildVersion = "9.12.0.0",
	[parameter(Mandatory=$false)]
	[String]$WorkingDir = "D:\TestBinRoot"
)

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
        #Write-Host "Directory $WorkingDir already exist. Deleting it ..."

        Remove-Item $WorkingDir -Recurse -Force > $null
    }

    #
    # Create the directory
    #
    #Write-Host "Creating the directory $WorkingDir ..."

    New-Item -Path "$WorkingDir" -ItemType Directory -Force > $null
	
	if ( !$? )
    {
        #Write-Error "Error creating working directory $WorkingDir"
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
	Add-Content $logfile -Value [$([DateTime]::Now)]$message
}

function ValidateJunction()
{
	if(!(Test-Path -Path "C:\ProgramData\ASR"))
	{
		Logger "The junction does not exist"
		Write-output "Failure"
		exit 1
	}
	
	if(!(Test-Path -Path "E:\Program Files (x86)\Microsoft Azure Site Recovery"))
	{
		Logger "The installation folder does not exist"
		Write-output "Failure"
		exit 1
	}
	
	$attributes = (Get-Item -Path "C:\ProgramData\ASR" -Force).Attributes.ToString();
	if (!($attributes -match "ReparsePoint"))
	{
		Logger "The junction does not exist"
		Write-output "Failure"
		exit 1
	}
}

#
# Verify Build Version on PS
#
function ValidatePS()
{
	CreateDirectory
	
	Logger "INFO : Fetching Build Version on ProcessServer"
	$key = "HKLM:\SOFTWARE\Wow6432Node\InMage Systems\Installed Products/9"; 
	$PSBuild = (Get-ItemProperty -Path $key).Product_Version; 
	
	if(!$PSBuild)
	{
		Logger "ERROR : PS is not installed : $PSBuild on VM"
		Write-output "Failure"
		exit 1
	}
	elseif($PSBuild -eq $PSBuildVersion)
	{
		Logger "INFO : $PSBuild : $PSBuildVersion"
		Logger "INFO : PS Deployment is successfull on VM"
	}
	else
	{
		Logger "ERROR : $PSBuild : $PSBuildVersion - Version Mismatch on VM"
		Write-output "Failure"
		exit 1
	}
	#ValidateJunction
	
	Logger "INFO : Successfully validated ProcessServer"
	Write-output "Success"
}

ValidatePS