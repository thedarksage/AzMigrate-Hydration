<#
.Synopsis  
  Creates Source Agent Driver Nuget packages.
  
.Parameter DriverVersion
    Driver version.
	
.Parameter BranchName
    Branch name.
	
.Parameter NugetCreationFolder
    Driver Nuget creation folder.

.Example  
    DriverNugetCreation.ps1 -DriverVersion <Driver version> -BranchName <Branch name> -NugetCreationFolder <Nuget creation folder>

#>

param(
    [Parameter(Mandatory=$True)] 
    [string]
    $DriverVersion,

    [Parameter(Mandatory=$True)] 
    [string]
    $BranchName,

    [Parameter(Mandatory=$True)] 
    [string]
    $NugetCreationFolder	
)

$TodaysDate =  Get-Date -format "dd_MMM_yyyy"
$BuildsDestDir = $DriverVersion
$BuildSharePath = "\\inmstagingsvr\DailyBuilds\Daily_Builds\$BuildsDestDir\HOST"
$TodaysBuildPath = "$BuildSharePath\$TodaysDate"
$DriverBinariesBuildPath = "$TodaysBuildPath\UnifiedAgent_Builds\release\InDskFlt\VistaRelease"
$DriverBinariesBuildPathx64 = "$TodaysBuildPath\UnifiedAgent_Builds\release\InDskFlt\VistaRelease\x64"
$SoftwareFolderName = "Software"
$CredentialProviderBundleName = "CredentialProviderBundle"
$CredentialProviderZipName = "CredentialProviderBundle.zip"
$SoftwareFolderPath = $NugetCreationFolder + "\" + $SoftwareFolderName
$NugetExePath = $NugetCreationFolder + "\" + $SoftwareFolderName + "\" + $CredentialProviderBundleName + "\NuGet.exe"
$CredentialProviderZipPath = $NugetCreationFolder + "\" + $SoftwareFolderName + "\" + $CredentialProviderZipName
$CredentialProviderBundlePath = $NugetCreationFolder + "\" + $SoftwareFolderName + "\" + $CredentialProviderBundleName
$CredentialProviderZipURI = "https://pkgs.dev.azure.com/hsdp/_apis/public/nuget/client/CredentialProviderBundle.zip"
$NugetTemplatespecPath = $NugetCreationFolder + "\" + "nuget_template.nuspec"
$TodaysNugetCreationFolder = $NugetCreationFolder + "\" + $TodaysDate
$DriverBinariesNugetCreationFolder = "$TodaysNugetCreationFolder\DriverBinaries"
$ASRDPSource = "https://msazure.pkgs.visualstudio.com/One/_packaging/ASRDPNugetFeed/nuget/v3/index.json"
$PackageNameSuffix = "VistaDrivers"
$DateTime = Get-Date -Format "yyyy-mm-ddThh-mm-ff"
$LogName = "NugetCreation" + $DateTime + ".log"
$LogPath = $NugetCreationFolder + "\" + $LogName

<#
.SYNOPSIS
Writes the output string to the console and also to a log file.
Usage:
	Log-Info ""
#>
function Log-Info([string] $OutputText)
{
    $OutputText | %{ Write-Output $_; Out-File -filepath $LogPath -inputobject $_ -append -encoding "ASCII" }
}

 
<#
.SYNOPSIS
Writes the error string to the console and also to a log file.
Usage:
	Log-Info ""
#>
function Log-Error([string] $OutputText)
{
    $OutputText | %{ Write-Error $_; Out-File -filepath $LogPath -inputobject $_ -append -encoding "ASCII" }
}

<#
.SYNOPSIS
Create Nuspec files.
Usage:
	CreateNuspecFiles -BuildsPath <Builds path> -PackagesPath <Packages to be copied path> -NugetVersion <Version of Nuget package> -BranchName <Branch name>
#>
function CreateNuspecFiles
{
    param(
        [string] $BuildsPath,
        [string] $PackagesPath,
        [string] $NugetVersion,
		[string] $BranchName
        )

    Log-Info "Creating Nuspec files."
    New-Item -Path $PackagesPath -type directory -Force

	Copy-Item -Path $BuildsPath\* -Destination $PackagesPath -Recurse 

	$PkgName = $PackageNameSuffix + "_" + $BranchName
	Log-Info "Creating nuspec file for $BranchName"

	$Nuspec = [xml](Get-Content $NugetTemplatespecPath)
	$nuspec.package.metadata.id = $PkgName
	$nuspec.package.metadata.description = $PkgName
	$Nuspec.package.metadata.version = $NugetVersion
	$filesElement = $Nuspec.CreateElement("files", $Nuspec.DocumentElement.NamespaceURI)

	# Add subdirectories and its files to nuspec file.
	$SubDir = Get-ChildItem $PackagesPath\* -Directory | Select-Object -ExpandProperty name 
	$SubDirCount = $SubDir | Measure-Object | %{$_.Count}
	Log-Info "SubDirCount $SubDirCount"
	if ($SubDirCount -gt 0)
	{
		foreach ($dir in $SubDir) {
			$SubDirFiles = Get-ChildItem $PackagesPath\$dir\* 
			$SubDirFilesCount = $SubDirFiles | Measure-Object | %{$_.Count}
			Log-Info "SubDirFilesCount $SubDirFilesCount"
			if ($SubDirFilesCount -gt 0)
			{
				foreach ($file in $SubDirFiles) {
					$SubDirFileName = [System.IO.Path]::GetFileName($file)

					Log-Info "Adding $SubDirFileName to Nuspec file."
					$fileElement = $Nuspec.CreateElement("file", $Nuspec.DocumentElement.NamespaceURI)
					$fileElement.SetAttribute("src","$dir\$SubDirFileName")
					$fileElement.SetAttribute("target","$dir\$SubDirFileName")
					$filesElement.AppendChild($fileElement)
				}
			}
		}
		$Nuspec.package.AppendChild($filesElement)
	}

	$NugetName = [string]::Join(".", @($PkgName,"nuspec"))
	$NugetSpecPath = [System.IO.Path]::Combine($PackagesPath, $NugetName)
	$Nuspec.Save($NugetSpecPath);
    
    Log-Info "Successfully created Nuspec files."
}

<#
.SYNOPSIS
Create Nupkg files.
Usage:
	CreateNupkgFiles -PackagesPath <Required packages available path>
#>
function CreateNupkgFiles
{
    param(
        [string] $PackagesPath
        )

    Log-Info "Creating Nupkg files."

    $files = [System.IO.Directory]::GetFiles("$PackagesPath", "*.nuspec")
    For ($i=0; $i -lt $files.Count; $i++) {
        $filePath = $files[$i]
        $Cmd = "$NugetExePath pack $filePath -OutputDirectory $PackagesPath"
        Log-Info "$cmd"
        Invoke-Expression $Cmd
    }

    Log-Info "Successfully created the Nupkg files."
}

<#
.SYNOPSIS
Push Nuget packages to feed.
Usage:
	PushNugetPackages -PackagesPath <Required packages available path>
#>
function PushNugetPackages
{
    param(
        [string] $PackagesPath
        )

    Log-Info "Pushing NuGet packages to feed."

    $files = [System.IO.Directory]::GetFiles("$PackagesPath", "*.nupkg")
    For ($i=0; $i -lt $files.Count; $i++) {
        $filePath = $files[$i]
        $cmd = "$NugetExePath push -Source $ASRDPSource -ApiKey az " + $filePath + "  -Timeout 1500"
        Log-Info "$cmd"
        Invoke-Expression $cmd
    }

    Log-Info "Successfully pushed the NuGet packages to feed."
}

<#
.SYNOPSIS
Create and Push Nuget packages to feed.
Usage:
	CreateAndPushNugetPackages -BuildsPath -PackagesPath <Required packages available path> -NugetVersion <Version of Nuget package> -BranchName <Branch name>
#>
function CreateAndPushNugetPackages
{
    param(
        [string] $BuildsPath,
        [string] $PackagesPath,
        [string] $NugetVersion,
		[string] $BranchName
        )

    CreateNuspecFiles -BuildsPath $BuildsPath -PackagesPath $PackagesPath -NugetVersion $NugetVersion -BranchName $BranchName
    CreateNupkgFiles -PackagesPath $PackagesPath
    PushNugetPackages -PackagesPath $PackagesPath
}

try
{
    # Check for today's build path.
    if(-Not(Test-Path $TodaysBuildPath))
    {
        $msg = "Today's build path is not available."
        Log-Error $msg
        throw $msg
    }

    # Get build version.
    $WindowsInstaller = Get-ChildItem $DriverBinariesBuildPathx64 | Where-Object {$_.Name -match ".*sys"}
    if($WindowsInstaller -eq $null)
    {
        $msg = "Today's driver binaries are not available."
        Log-Error $msg
        throw $msg
    }
	
	$MajorVersion = $WindowsInstaller.VersionInfo.FileMajorPart 
	$MinorVersion = $WindowsInstaller.VersionInfo.FileMinorPart 
	$BuildNumber = $WindowsInstaller.VersionInfo.FileBuildPart 
	$FilePartNumber = $WindowsInstaller.VersionInfo.FilePrivatePart
    $NugetVersion = "$MajorVersion.$MinorVersion.$BuildNumber.$FilePartNumber"
    
    # Delete stale folders and create required folders.
    if (Test-Path $SoftwareFolderPath) { Remove-Item $SoftwareFolderPath -Recurse -Force; }
    if (Test-Path $TodaysNugetCreationFolder) { Remove-Item $TodaysNugetCreationFolder -Recurse -Force; }
    New-Item -Path $TodaysNugetCreationFolder -type directory -Force

    # Download CredentialProvider Zip file and extract it.
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    New-Item -Path $SoftwareFolderPath -type directory -Force
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -URI $CredentialProviderZipURI -OutFile $CredentialProviderZipPath
    [System.IO.Compression.ZipFile]::ExtractToDirectory($CredentialProviderZipPath, $CredentialProviderBundlePath)

    # Create and Push NuGet packages.
    CreateAndPushNugetPackages -BuildsPath $DriverBinariesBuildPath -PackagesPath $DriverBinariesNugetCreationFolder -NugetVersion $NugetVersion -BranchName $BranchName

    Log-Info "The NuGets were successfully published."
}
catch
{
    Log-Error "Script execution failed with error $_.Exception.Message"
	throw "Script execution failed."
}