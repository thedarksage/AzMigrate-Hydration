<#
.Synopsis  
  Creates SLES12/15 kernel headers Nuget packages.
	
.Parameter NugetCreationFolder
    Nuget creation folder.

.Example  
    SLESNugetCreation.ps1 -NugetCreationFolder <Nuget creation folder>

#>

param(
    [Parameter(Mandatory=$True)] 
    [string]
    $NugetCreationFolder	
)

$TodaysDate =  Get-Date -format "dd_MMM_yyyy"
$BuildSharePath = "\\inmstagingsvr\DailyBuilds\Daily_Builds\SLESKernelHeaders"
$TodaysBuildPath = "$BuildSharePath\$TodaysDate"
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
$SLESNugetCreationFolder = "$TodaysNugetCreationFolder\SLES"
$ASRDPSource = "https://msazure.pkgs.visualstudio.com/One/_packaging/ASRDPNugetFeed/nuget/v3/index.json"
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
        $cmd = "$NugetExePath push -Source $ASRDPSource -ApiKey az " + $filePath + "  -Timeout 1500"
        Log-Info "$cmd"
        Invoke-Expression $cmd
    }

    Log-Info "Successfully pushed the Nuget packages to feed."
}

<#
.SYNOPSIS
Create and Push SLES kernel headers Nuget packages to feed.
Usage:
	CreateAndPushNugetPackages -NugetVersion <version>
#>
function CreateAndPushNugetPackages
{
    param(
        [string] $NugetVersion
        )

    New-Item -Path $SLESNugetCreationFolder -type directory -Force

    Copy-Item -Path $TodaysBuildPath\*.zip -Destination $SLESNugetCreationFolder

    $Files = Get-ChildItem $SLESNugetCreationFolder\* -Include *.zip $Files
    For ($i=0; $i -lt $Files.Count; $i++) {
        $FilePath = $Files[$i]
        if($FilePath.Length -eq 0kb) {
            Log-Info ("Skipping for $FilePath as file size is 0")
            continue
        }

        $FileName = [System.IO.Path]::GetFileName($FilePath)
        $PkgName = "ASR_KernelHeaders_" + $FileName.SubString(0,6)

        $Nuspec = [xml](Get-Content $NugetTemplatespecPath)
        $nuspec.package.metadata.id = $PkgName
        $Nuspec.package.metadata.version = $NugetVersion
        $Nuspec.package.metadata.description = "Kernel headers for " + $FileName.SubString(0,6)

        $filesElement = $Nuspec.CreateElement("files", $Nuspec.DocumentElement.NamespaceURI)
        Log-Info "Adding $FileName to Nuspec file."
        $fileElement = $Nuspec.CreateElement("file", $Nuspec.DocumentElement.NamespaceURI)
        $fileElement.SetAttribute("src","$FileName")
        $fileElement.SetAttribute("target","$FileName")
        $filesElement.AppendChild($fileElement)
        $Nuspec.package.AppendChild($filesElement)

        $NugetName = [string]::Join(".", @($PkgName,"nuspec"))
        $NugetSpecPath = [System.IO.Path]::Combine($SLESNugetCreationFolder, $NugetName)
        $Nuspec.Save($NugetSpecPath);
    }

    CreateNupkgFiles -PackagesPath $SLESNugetCreationFolder
    PushNugetPackages -PackagesPath $SLESNugetCreationFolder

    Log-info "The SLES kernel headers nugets succesfully published."
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

    $NugetVersion = Get-Date -format "yyyyMMdd"
    
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

    # create and push SLES kernel headers nuget packages
    CreateAndPushNugetPackages -NugetVersion $NugetVersion
    Log-Info "The nugets were succesfully published."
}
catch
{
    Log-Error "Script execution failed with error $_.Exception.Message"
	throw "Script execution failed."
}
