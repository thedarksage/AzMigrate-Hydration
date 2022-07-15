# Script that generates various version files.
Param(
[Parameter(Mandatory=$true)]
    [String]$SourcesRootDirectory
)

# This function generates version.h file
function GenerateVersionDotH 
{
    Param(
    [Parameter(Mandatory=$true)]
        [String]$FilePath
    )

    $VersionContents="#define INMAGE_PRODUCT_VERSION_STR `"$BuildTag`"`r`n"
    $VersionContents=[string]$VersionContents + "#define INMAGE_PRODUCT_VERSION $MajorVersion,$MinorVersion,$BuildNumber,1`r`n"
    $VersionContents=[string]$VersionContents + "#define INMAGE_PRODUCT_VERSION_MAJOR $MajorVersion`r`n"
    $VersionContents=[string]$VersionContents + "#define INMAGE_PRODUCT_VERSION_MINOR $MinorVersion`r`n"
    $VersionContents=[string]$VersionContents + "#define INMAGE_PRODUCT_VERSION_BUILDNUM $BuildNumber`r`n";
    $VersionContents=[string]$VersionContents + "#define INMAGE_PRODUCT_VERSION_PRIVATE 1`r`n";
    $VersionContents=[string]$VersionContents + "#define INMAGE_HOST_AGENT_CONFIG_CAPTION `"${BuildTag}`"`r`n";
    $VersionContents=[string]$VersionContents + "#define PROD_VERSION `"$ProductVersion`"`r`n";
    $VersionContents=[string]$VersionContents + "#define INMAGE_COPY_RIGHT `"\xa9 $year Microsoft Corp. All rights reserved.`"`r`n";
    $VersionContents=[string]$VersionContents + "#define INMAGE_PRODUCT_NAME `"Microsoft Azure Site Recovery`"";

    Out-File -FilePath $FilePath -InputObject $VersionContents -Encoding ASCII
    Write-Host "`r`nContents of $FilePath"
    Write-Host "--------------------------------------------"
    Get-Content -Path $FilePath
}

# This function generates version.cs file
function GenerateVersionDotCs 
{
    Param(
    [Parameter(Mandatory=$true)]
        [String]$FilePath
    )    

    $VersionContents="using System.Reflection;`r`n`r`n"
    $VersionContents=[string]$VersionContents + "[assembly: AssemblyFileVersion(`"$MajorVersion.$MinorVersion.$BuildNumber.1`")]`r`n"
    $VersionContents=[string]$VersionContents + "[assembly: AssemblyCompany(`"Microsoft Corporation`")]`r`n"
    $VersionContents=[string]$VersionContents + "[assembly: AssemblyProduct(`"Microsoft Azure Site Recovery`")]`r`n"
    $VersionContents=[string]$VersionContents + "[assembly: AssemblyInformationalVersion(`"$ProductVersion`")]`r`n"
    $VersionContents=[string]$VersionContents + "[assembly: AssemblyCopyright(`"\u00A9 2021 Microsoft Corp. All rights reserved.`")]`r`n"
    $VersionContents=[string]$VersionContents + "[assembly: AssemblyTrademark(`"Microsoft\u00AE is a registered trademark of Microsoft Corporation.`")]`r`n"

    Out-File -FilePath $FilePath -InputObject $VersionContents -Encoding ASCII
    Write-Host "`r`nContents of $FilePath"
    Write-Host "---------------------------------------------"
    Get-Content -Path $FilePath
}

# This function generates version.wxi file
function GenerateVersionDotWxi 
{
    Param(
    [Parameter(Mandatory=$true)]
        [String]$FilePath
    )    

    $VersionContents="<Include>`r`n"
    $VersionContents=[string]$VersionContents + "<?define BuildVersion = `"$MajorVersion.$MinorVersion.$BuildNumber.1`" ?>`r`n"
    $VersionContents=[string]$VersionContents + "<?define ProductVersion = `"$ProductVersion`" ?>`r`n"
    $VersionContents=[string]$VersionContents + "<?define BuildPhase = `"$BuildPhase`" ?>`r`n"
    $VersionContents=[string]$VersionContents + "<?define BuildTag = `"$BuildTag`" ?>`r`n"
    $VersionContents=[string]$VersionContents + "<?define BuildNumber = `"$BuildNumber`" ?>`r`n"
    $VersionContents=[string]$VersionContents + "<?define CompanyName = `"Microsoft Corporation`" ?> `r`n"
    $VersionContents=[string]$VersionContents + "</Include>`r`n"
   
    Out-File -FilePath $FilePath -InputObject $VersionContents -Encoding ASCII
    Write-Host "`r`nContents of $FilePath"
    Write-Host "------------------------------------------------------------------"
    Get-Content -Path $FilePath
}

# This function generates version.iss file
function GenerateVersionDotIss
{
    Param(
    [Parameter(Mandatory=$true)]
        [String]$FilePath
    )

    $VersionContents="#define VERSION `"$ProductVersion`"`r`n"
    $VersionContents=[string]$VersionContents + "#define PRODUCTVERSION `"$ProductVersion`"`r`n"
    $VersionContents=[string]$VersionContents + "#define APPVERSION `"$MajorVersion.$MinorVersion.$BuildNumber.1`"`r`n"
    $VersionContents=[string]$VersionContents + "#define BUILDTAG `"${BuildTag}_InMage`"`r`n";
    $VersionContents=[string]$VersionContents + "#define BUILDNUMBER `"${BuildNumber}`"`r`n";
    $VersionContents=[string]$VersionContents + "#define BUILDPHASE `"${BuildPhase}`"`r`n";
    $VersionContents=[string]$VersionContents + "#define PARTNERCODE `"1`"`r`n";
    $VersionContents=[string]$VersionContents + "#define BUILDDATE `"${BuildDate}`"`r`n";
    $VersionContents=[string]$VersionContents + "#define COPYRIGHT `"\xa9 $year Microsoft Corp. All rights reserved.`"`r`n";

    Out-File -FilePath $FilePath -InputObject $VersionContents -Encoding ASCII
    Write-Host "`r`nContents of $FilePath"
    Write-Host "--------------------------------------------"
    Get-Content -Path $FilePath
}

# This function generates version file
function GenerateVersionFile 
{
    Param(
    [Parameter(Mandatory=$true)]
        [String]$FilePath
    )    

    $VersionContents="$BuildTag`r`n"
    $VersionContents=[string]$VersionContents + "$ProductVersion`_$BuildPhase`_$BuildNumber`r`n"
    $VersionContents=[string]$VersionContents + "VERSION=$ProductVersion`r`n"
    $VersionContents=[string]$VersionContents + "PROD_VERSION=$ProductVersion`r`n"
   
    Out-File -FilePath $FilePath -InputObject $VersionContents -Encoding ASCII
    Write-Host "`r`nContents of $FilePath"
    Write-Host "----------------------------------------"
    Get-Content -Path $FilePath
}

# This function generates manifest.txt file
function GenerateManifestDotTxt
{
    Param(
    [Parameter(Mandatory=$true)]
        [String]$FilePath
    )

    $ManifestContents="Microsoft-ASR_UA_${ProductVersion}_Windows_${BuildPhase}_${BuildDate}_release.exe,1,Windows,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_RHEL5-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,RHEL5-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_RHEL6-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,RHEL6-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_RHEL7-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,RHEL7-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_RHEL8-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,RHEL8-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_OL6-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,OL6-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_OL7-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,OL7-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_OL8-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,OL8-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_SLES11-SP3-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,SLES11-SP3-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_SLES11-SP4-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,SLES11-SP4-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_SLES12-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,SLES12-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_SLES15-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,SLES15-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_UBUNTU-14.04-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,UBUNTU-14.04-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_UBUNTU-16.04-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,UBUNTU-16.04-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_UBUNTU-18.04-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,UBUNTU-18.04-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_UBUNTU-20.04-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,UBUNTU-20.04-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_DEBIAN7-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,DEBIAN7-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_DEBIAN8-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,DEBIAN8-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_DEBIAN9-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,DEBIAN9-64,,${ProductVersion},Upgrade,no`r`n"
    $ManifestContents=[string]$ManifestContents + "Microsoft-ASR_UA_${ProductVersion}_DEBIAN10-64_${BuildPhase}_${BuildDate}_release.tar.gz,2,DEBIAN10-64,,${ProductVersion},Upgrade,no`r`n"

    Out-File -FilePath $FilePath -InputObject $ManifestContents -Encoding ASCII
    Write-Host "`r`nContents of $FilePath"
    Write-Host "--------------------------------------------"
    Get-Content -Path $FilePath
}

# This function generates VersionData.txt file
function GenerateVersionDataDotTxtFile 
{
    Param(
    [Parameter(Mandatory=$true)]
        [String]$FilePath
    )    

    $VersionContents="BuildVersion=$MajorVersion.$MinorVersion.$BuildNumber.1`r`n"
	$VersionContents=[string]$VersionContents + "AgentInstallerName=Microsoft-ASR_UA_${ProductVersion}_Windows_${BuildPhase}_${BuildDate}_release.exe`r`n"
   
    Out-File -FilePath $FilePath -InputObject $VersionContents -Encoding ASCII
    Write-Host "`r`nContents of $FilePath"
    Write-Host "----------------------------------------"
    Get-Content -Path $FilePath
}

try
{
    # Set timezone to IST.
    Write-Host ("Current time:", (Get-Date))
    Write-Host ("Current time zone:", (Get-Timezone | Select-Object -Property Id))
    Set-TimeZone -Name "India Standard Time"
    if (!$?)
    {
        Write-Host ('Failed to set time zone to IST.')
        exit 1
    }
    else
    {
        Write-Host ('Sucessfully set time zone to IST.')
    }
    Write-Host ("Current time:", (Get-Date))
    Write-Host ("Current time zone:", (Get-Timezone | Select-Object -Property Id))

    # Read build configuration.
    $BuildConfigFile = $SourcesRootDirectory + "\.pipelines\BuildConfig.bcf"
    $BuildConfig = [ordered]@{}
    ( Get-Content $BuildConfigFile ) | Foreach-Object {
        $Tokens = $_ -split '='
        if ($Tokens.Count -eq 2) {
            $ConfigName = $Tokens[0].Trim()
            $ConfigValue = $Tokens[1].Trim()
            $BuildConfig.add($ConfigName, $ConfigValue)            
        }
    }

    # Define build configuration varaibles.
    $BuildPhase = $BuildConfig.Item("BuildPhase")
    $BuildQuality = $BuildConfig.Item("BuildQuality")
    $MajorVersion = $BuildConfig.Item("MajorVersion")
    $MinorVersion = $BuildConfig.Item("MinorVersion")
    $PatchsetVersion = $BuildConfig.Item("PatchsetVersion")
    $PatchVersion = $BuildConfig.Item("PatchVersion")
    $ProductVersion = $MajorVersion + "." + $MinorVersion + "." + $PatchSetVersion + "." + $PatchVersion

    # Calculate build number.
    $StartDate = Get-Date -Date "01/01/1970"
    $EndDate = Get-Date
    $TimeInSecs=(New-TimeSpan -Start $StartDate -End $EndDate).TotalSeconds
    $BuildNumber=[math]::floor(($TimeInSecs - 1104517800)/86400);
    
    # Generate build tag.
    $Month = (Get-Culture).DateTimeFormat.GetAbbreviatedMonthName([convert]::ToInt32((Get-Date -Format MM)))
    $Date = Get-Date -Format dd
    $Year = Get-Date -Format yyyy
    $BuildDate = $Date + $Month + $Year
    $BuildTag = $BuildQuality + "_" + $ProductVersion + "_" + $BuildPhase + "_" + $BuildNumber + "_" + $Month + "_" + $Date + "_" + $Year

    # Define version file vaiables.
    $VersonDotHFile = $SourcesRootDirectory + "\host\common\version.h"
    $VersonDotCsFile = $SourcesRootDirectory + "\host\common\version.cs"
    $VersonDotWxiFileAgent = $SourcesRootDirectory + "\host\ASRSetup\UnifiedAgentMSI\version.wxi"
    $VersonDotWxiFilePS = $SourcesRootDirectory + "\server\windows\ProcessServerMSI\Version.wxi"
    $VersonDotIssFile = $SourcesRootDirectory + "\host\setup\version.iss"
    $VersonFileServer = $SourcesRootDirectory + "\server\tm\version"
    $ManifestDotTxtFile = $SourcesRootDirectory + "\host\setup\manifest.txt"
	$VersionDataDotTxtFile = $SourcesRootDirectory + "\VersionData.txt"
    
    # Generate version.h file.
    GenerateVersionDotH $VersonDotHFile
    
    # Generate version.cs file.
    GenerateVersionDotCs $VersonDotCsFile
    
    # Generate Agent version.wxi file.
    GenerateVersionDotWxi $VersonDotWxiFileAgent
    
    # Generate PS version.wxi file.
    GenerateVersionDotWxi $VersonDotWxiFilePS
    
    # Generate version.iss file.
    GenerateVersionDotIss $VersonDotIssFile

    # Generate version file.
    GenerateVersionFile $VersonFileServer

    # Generate manifest.txt file.
    GenerateManifestDotTxt $ManifestDotTxtFile
	
	# Generate VersionData.txt file.
	GenerateVersionDataDotTxtFile $VersionDataDotTxtFile
}
catch
{
    Write-Host "Hit below excecption."
    Write-Host $_.Exception | Format-List 
    exit 1
}