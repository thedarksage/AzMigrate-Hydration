#This function generates version.h file
function GenerateVersionFile 
{
Param(
    [Parameter(Mandatory=$true)]
        [String]$SourceCodePath,
    [Parameter(Mandatory=$true)]
        [String]$BranchName,
    [Parameter(Mandatory=$true)]
        [String]$MajorVersion,
    [Parameter(Mandatory=$true)]
        [String]$MinorVersion,
    [Parameter(Mandatory=$true)]
        [String]$DailyBuildNumber,
    [Parameter(Mandatory=$true)]
        [String]$Private,
    [Parameter(Mandatory=$true)]
        [String]$PatchVersion,
    [Parameter(Mandatory=$true)]
        [String]$PatchSetVersion,
    [Parameter(Mandatory=$true)]
        [String]$BuildQuality,
[Parameter(Mandatory=$true)]
        [String]$BuildPhase
    )#end param
    [Boolean]$retvalue=$true

    $mm = Get-Date -Format MM
    $mmint = [convert]::ToInt32($mm, 10)
    $month = (Get-Culture).DateTimeFormat.GetAbbreviatedMonthName($mmint)
    $date = Get-Date -Format dd
    $year = Get-Date -Format yyyy

    $tagstring="$BuildQuality`_$MajorVersion.$MinorVersion.$PatchSetVersion.$PatchVersion`_$BuildPhase`_$DailyBuildNumber`_$month`_$date`_$year"
    
    $versionContents="#define INMAGE_PRODUCT_VERSION_STR `"$tagstring`"`r`n"
    $versionContents=[string]$versionContents+ "#define INMAGE_PRODUCT_VERSION $MajorVersion,$MinorVersion,$DailyBuildNumber,$Private`r`n"
    $versionContents=[string]$versionContents+"#define INMAGE_PRODUCT_VERSION_MAJOR $MajorVersion`r`n"
    $versionContents=[string]$versionContents+"#define INMAGE_PRODUCT_VERSION_MINOR $MinorVersion`r`n"
    $versionContents=[string]$versionContents+ "#define INMAGE_PRODUCT_VERSION_BUILDNUM $DailyBuildNumber`r`n";
    $versionContents=[string]$versionContents+ "#define INMAGE_PRODUCT_VERSION_PRIVATE $Private`r`n";
    $versionContents=[string]$versionContents+ "#define INMAGE_HOST_AGENT_CONFIG_CAPTION `"${tagstring}`"`r`n";
    $versionContents=[string]$versionContents+ "#define PROD_VERSION `"$MajorVersion.$MinorVersion.$PatchSetVersion.$PatchVersion`"`r`n";
    $versionContents=[string]$versionContents+ "#define INMAGE_COPY_RIGHT `"(C) $year Microsoft Corp. All rights reserved.`"`r`n";
    $versionContents=[string]$versionContents+ "#define INMAGE_PRODUCT_NAME `"Microsoft Azure Site Recovery`"";

    Write-Host "Updating file I:\Source_Code\$Branch\InMage-Azure-SiteRecovery\host\common\version.h"
    Out-File -FilePath "I:\Source_Code\$Branch\InMage-Azure-SiteRecovery\host\common\version.h" -InputObject $versionContents -Encoding ASCII 
    Write-Host "SUCCEEDED: Updated file I:\Source_Code\$Branch\InMage-Azure-SiteRecovery\host\common\version.h"
    #Write-Host $versionContents

    return $retvalue;
}


#Declaring the date and time variables.
$date1 = Get-Date -Date "01/01/1970"
$date2 = Get-Date
$timeinsecs=(New-TimeSpan -Start $date1 -End $date2).TotalSeconds

#Declaring the branch and config variable and reading the values from scheduler.
$Branch=$args[0]
$Config=$args[1]
$DestBldFolder=$args[2]

$BldSrcFolder="I:\Source_Code\$Branch\InMage-Azure-SiteRecovery\build\scripts\automation"
$DriverSrcFolder="I:\Source_Code\$Branch\InMage-Azure-SiteRecovery\host\drivers\InVolFlt\windows\DiskFlt"

Copy-Item $BldSrcFolder\BuildConfig.bcf $BldSrcFolder\BuildConfig.bcf.orig

if ($Config -eq "release")
{
	( Get-Content $BldSrcFolder\BuildConfig.bcf ) | Foreach-Object {$_ -replace "Config_Value","release"} | set-content $BldSrcFolder\BuildConfig.bcf
}

if ($Config -eq "debug")
{ 
	( Get-Content $BldSrcFolder\BuildConfig.bcf ) | Foreach-Object {$_ -replace "Config_Value","debug"}  | set-content $BldSrcFolder\BuildConfig.bcf
} 

$hash = [ordered]@{}
$refsecs = [ordered]@{}
$smrrefsecs = [ordered]@{}

$refsecs.add("develop", 1104517800)
$refsecs.add("release", 1104517800)
$refsecs.add("master", 1104517800)

$smrrefsecs.add("develop", 1337193000)
$smrrefsecs.add("release", 1337193000)
$smrrefsecs.add("master", 1337193000)

#Read Build configuration
( Get-Content $BldSrcFolder\BuildConfig.bcf ) | Foreach-Object {
    
        # Write-Host "$_"
            $tokens = $_ -split '='
        if ($tokens.Count -eq 2) {
            $name = $tokens[0].Trim()
            $value = $tokens[1].Trim()
            $hash.add($name, $value)            
        }
}

# Calculate Build Number
$dlybldnum = 0
if ($refsecs.Contains($Branch)) {
    $dlybldnum=(($timeinsecs - $refsecs.Item($Branch))/86400);
    $dlybldnum=[math]::floor($dlybldnum)
}

GenerateVersionFile $hash.Item("srcpath") $hash.Item("branch") $hash.Item("majorversion") $hash.Item("minorversion") $dlybldnum 1 $hash.Item("patchversion") $hash.Item("patchsetversion") $hash.Item("buildquality") $hash.Item("buildphase") 

$DriverBldCommand = "$BldSrcFolder\vista_driverbld.cmd $DriverSrcFolder c:\test"
Write-Host "Building Driver $DriverBldCommand"
cmd.exe /C "$DriverBldCommand"

$mm = Get-Date -Format MM
$mmint = [convert]::ToInt32($mm, 10)
$month = (Get-Culture).DateTimeFormat.GetAbbreviatedMonthName($mmint)
$date = Get-Date -Format dd
$year = Get-Date -Format yyyy


$MajorVersion=$hash.Item("majorversion")
$MinorVersion=$hash.Item("minorversion")

$VistaX86BldFolder="$MajorVersion.$MinorVersion\HOST\$date`_$month`_$year\UnifiedAgent_Builds\release\InDskFlt\VistaRelease\x86\"
$VistaX64BldFolder="$MajorVersion.$MinorVersion\HOST\$date`_$month`_$year\UnifiedAgent_Builds\release\InDskFlt\VistaRelease\x64\"

Push-Location $DestBldFolder

#Cleanup earlier target locations
if (Test-Path -Path $VistaX86BldFolder) {
    Write-Host "Deleting Folder $VistaX86BldFolder"
    Remove-Item $VistaX86BldFolder -Recurse:$Recurse
}

if (Test-Path -Path $VistaX64BldFolder) {
    Write-Host "Deleting Folder $VistaX64BldFolder"
    Remove-Item $VistaX64BldFolder -Recurse:$Recurse
}

# Create Target Folders
New-Item -Path $VistaX86BldFolder -ItemType directory
New-Item -Path $VistaX64BldFolder -ItemType directory

#copy vista X86 driver binaries and symbols
Copy-Item "C:\test\VistaRelease\x86\indskflt.sys" -Destination  "$VistaX86BldFolder"
Copy-Item "C:\test\VistaRelease\x86\indskflt.pdb" -Destination  "$VistaX86BldFolder"

#copy vista X64 driver binaries and symbols
Copy-Item "C:\test\VistaRelease\x64\indskflt.sys" -Destination  "$VistaX64BldFolder"
Copy-Item "C:\test\VistaRelease\x64\indskflt.pdb" -Destination  "$VistaX64BldFolder"

Write-Host "Vista Driver Build copied"
Pop-Location

Write-Host "Vista Build Copied Exiting...."
