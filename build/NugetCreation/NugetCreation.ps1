<#
.Synopsis  
  Creates Agents, Pushclients and other required binaries Nuget packages.
  
.Parameter BranchName
    Branch name.

.Parameter BuildsDestDir
    Builds destination directory in staging server.
	
.Parameter NugetCreationFolder
    Nuget creation folder.

.Example  
    NugetCreation.ps1 -BranchName <Branch name> -BuildsDestDir <Builds destination directory> -NugetCreationFolder <Nuget creation folder>

#>

param(
    [Parameter(Mandatory=$True)] 
    [string]
    $BranchName,

    [Parameter(Mandatory=$True)] 
    [string]
    $BuildsDestDir,

    [Parameter(Mandatory=$True)] 
    [string]
    $NugetCreationFolder	
)

$TodaysDate =  Get-Date -format "dd_MMM_yyyy"
$BuildSharePath = "\\inmstagingsvr\DailyBuilds\Daily_Builds\$BuildsDestDir\HOST"
$TodaysBuildPath = "$BuildSharePath\$TodaysDate"
$AgentsBuildPath = "$TodaysBuildPath\UnifiedAgent_Builds\release"
$PushInstallBinariesBuildPath = "$TodaysBuildPath\PushInstallBinaries"
$DataProtectionBinariesBuildPath = "$TodaysBuildPath\DataProtectionBinaries"
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
$AgentsNugetCreationFolder = "$TodaysNugetCreationFolder\UnifiedAgent"
$PushInstallBinariesNugetCreationFolder = "$TodaysNugetCreationFolder\PushInstallBinaries"
$DataProtectionBinariesNugetCreationFolder = "$TodaysNugetCreationFolder\DataProtectionBinaries"
$RecoveryServicesSource = "https://microsoft.pkgs.visualstudio.com/DefaultCollection/_packaging/RecoveryServices.Internal/nuget/v3/index.json"
$PackageNameSuffix = "ASR"
$DateTime = Get-Date -Format "yyyy-mm-ddThh-mm-ff"
$LogName = "NugetCreation" + $DateTime + ".log"
$LogPath = $NugetCreationFolder + "\" + $LogName
$PushBinariesPackageName = "PI_BINARIES"
$DataProtectionBinariesPackageName = "DP_BINARIES"

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
	CreateNuspecFiles -BuildsPath <Builds path> -PackagesPath <Packages to be copied path> -NugetVersion <Version of Nuget package> -PackageName <Package name to be created>
#>
function CreateNuspecFiles
{
    param(
        [string] $BuildsPath,
        [string] $PackagesPath,
        [string] $NugetVersion,
        [string] $PackageName
        )

    Log-Info "Creating Nuspec files."
    New-Item -Path $PackagesPath -type directory -Force

	Copy-Item -Path $BuildsPath\* -Destination $PackagesPath -Recurse 
	$Files = Get-ChildItem $PackagesPath\* -Include *.*
	$FileCount = $Files | Measure-Object | %{$_.Count}
	Log-Info "FileCount: $FileCount"

	if ($FileCount -gt 0)
	{
		$PkgName = $PackageNameSuffix + "_" + $PackageName + "_" + $BranchName
		Log-Info "Creating nuspec file for $PackageName"

		$Nuspec = [xml](Get-Content $NugetTemplatespecPath)
		$nuspec.package.metadata.id = $PkgName
		$nuspec.package.metadata.description = $PkgName
		$Nuspec.package.metadata.version = $NugetVersion
		$filesElement = $Nuspec.CreateElement("files", $Nuspec.DocumentElement.NamespaceURI)

		# Create nuspec files for all files present in the folder.
		For ($i=0; $i -lt $FileCount; $i++) {
			$FilePath = $Files[$i]
			$FileName = [System.IO.Path]::GetFileName($filePath)

			Log-Info "Adding $FileName to Nuspec file."
			$fileElement = $Nuspec.CreateElement("file", $Nuspec.DocumentElement.NamespaceURI)
			$fileElement.SetAttribute("src","$FileName")
			$fileElement.SetAttribute("target","$FileName")
			$filesElement.AppendChild($fileElement)
		}
		$Nuspec.package.AppendChild($filesElement)

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
	}
    
    Log-Info "Sucessfully created Nuspec files."
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

    Log-Info "Pushing Nuget packages to feed."

    $files = [System.IO.Directory]::GetFiles("$PackagesPath", "*.nupkg")
    For ($i=0; $i -lt $files.Count; $i++) {
        $filePath = $files[$i]
        $cmd = "$NugetExePath push -Source $RecoveryServicesSource -ApiKey VSTS " + $filePath + "  -Timeout 1500"
        Log-Info "$cmd"
        Invoke-Expression $cmd
    }

    Log-Info "Successfully pushed the Nuget packages to feed."
}

<#
.SYNOPSIS
Create and Push Nuget packages to feed.
Usage:
	CreateAndPushNugetPackages -BuildsPath -PackagesPath <Required packages available path>
#>
function CreateAndPushNugetPackages
{
    param(
        [string] $BuildsPath,
        [string] $PackagesPath,
        [string] $NugetVersion,
        [string] $PackageName
        )

    CreateNuspecFiles -BuildsPath $BuildsPath -PackagesPath $PackagesPath -NugetVersion $NugetVersion -PackageName $PackageName
    CreateNupkgFiles -PackagesPath $PackagesPath
    PushNugetPackages -PackagesPath $PackagesPath
}

<#
.SYNOPSIS
Create and Push agent Nuget packages to feed.
Usage:
	CreateAndPushAgentNugetPackages -NugetVersion <version>
#>
function CreateAndPushAgentNugetPackages
{
    param(
        [string] $NugetVersion
        )

    New-Item -Path $AgentsNugetCreationFolder -type directory -Force

    Copy-Item -Path $AgentsBuildPath\*.tar.gz -Destination $AgentsNugetCreationFolder
    Copy-Item -Path $AgentsBuildPath\*.exe -Destination $AgentsNugetCreationFolder

    $Files = Get-ChildItem $AgentsNugetCreationFolder\* -Include *.gz,  *.exe; $Files
    For ($i=0; $i -lt $Files.Count; $i++) {
        $FilePath = $Files[$i]
        if($FilePath.Length -eq 0kb) {
            Log-Info ("Skipping for $FilePath as file size is 0")
            continue
        }

        $FileName = [System.IO.Path]::GetFileName($FilePath)
        $PkgName = "ASR_UA_" + $FileName.Split('_')[3] + "_" + $BranchName

        $Nuspec = [xml](Get-Content $NugetTemplatespecPath)
        $nuspec.package.metadata.id = $PkgName
        $Nuspec.package.metadata.version = $NugetVersion
        $Nuspec.package.metadata.description = "ASR Mobility Agent for " + $FileName.Split('_')[3]

        $filesElement = $Nuspec.CreateElement("files", $Nuspec.DocumentElement.NamespaceURI)
        Log-Info "Adding $FileName to Nuspec file."
        $fileElement = $Nuspec.CreateElement("file", $Nuspec.DocumentElement.NamespaceURI)
        $fileElement.SetAttribute("src","$FileName")
        $fileElement.SetAttribute("target","$FileName")
        $filesElement.AppendChild($fileElement)
        $Nuspec.package.AppendChild($filesElement)

        $NugetName = [string]::Join(".", @($PkgName,"nuspec"))
        $NugetSpecPath = [System.IO.Path]::Combine($AgentsNugetCreationFolder, $NugetName)
        $Nuspec.Save($NugetSpecPath);
    }

    CreateNupkgFiles -PackagesPath $AgentsNugetCreationFolder
    PushNugetPackages -PackagesPath $AgentsNugetCreationFolder

    Log-info "The Unified agent nugets succesfully published."
}

############ -- MAIN -- ############

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
    $WindowsInstaller = Get-ChildItem $AgentsBuildPath | Where-Object {$_.Name -match ".*exe"}
    if($WindowsInstaller -eq $null)
    {
        $msg = "Today's Windows build is not available."
        Log-Error $msg
        throw $msg
    }

    $NugetVersion = $WindowsInstaller.VersionInfo.FileVersion
    
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

    # Create and Push Nuget packages.
    CreateAndPushNugetPackages -BuildsPath $PushInstallBinariesBuildPath -PackagesPath $PushInstallBinariesNugetCreationFolder -NugetVersion $NugetVersion -PackageName $PushBinariesPackageName
    CreateAndPushNugetPackages -BuildsPath $DataProtectionBinariesBuildPath -PackagesPath $DataProtectionBinariesNugetCreationFolder -NugetVersion $NugetVersion -PackageName $DataProtectionBinariesPackageName

    # create and push agent nuget packages
    CreateAndPushAgentNugetPackages -NugetVersion $NugetVersion
    Log-Info "The nugets were succesfully published."
}
catch
{
    Log-Error "Script execution failed with error $_.Exception.Message"
	throw "Script execution failed."
}
