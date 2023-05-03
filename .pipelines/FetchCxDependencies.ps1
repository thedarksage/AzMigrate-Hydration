Param(
    [Parameter(Mandatory=$true)]
        [String]$SourcesRootDirectory
)

function FetchDraDependendencies {
    
	Copy-Item $TargetDirDra\retail-amd64\AzureSiteRecoveryProvider.exe -Destination $TargetDirDra -Force -passthru
	Copy-Item $TargetDirDra\retail-amd64\AzureSiteRecoveryConfigurationManager.msi -Destination $TargetDirDra -Force -passthru
	Copy-Item $TargetDirDra\retail-amd64\InMageFabricExtension\*.* -Destination $TargetDirDra -Force -passthru
	Copy-Item $TargetDirDra\retail-amd64\ASRAdapterFiles\*.* -Destination $TargetDirDra -Force -passthru
	Copy-Item $TargetDirDra\retail-amd64\ASRAdapterFiles\en\VMware.Interfaces.resources.dll -Destination $TargetDirDra -Force -passthru
	Copy-Item $TargetDirMars\target\retail\amd64\release\MARSAgentInstaller.exe -Destination $TargetDirMars -Force -passthru

    "`r`nDRA build details" | Tee-Object -FilePath $CxDepsLogFile -Append | Write-Host
    "-----------------" | Tee-Object -FilePath $CxDepsLogFile -Append | Write-Host
    Get-ItemProperty -Path "$TargetDirDra\AzureSiteRecoveryProvider.exe" | Format-list -Property * -Force | Out-String `
                     | ForEach-Object { $_.Trim() } | Tee-Object -FilePath $CxDepsLogFile -Append
					 
	"`r`MARS build details" | Tee-Object -FilePath $CxDepsLogFile -Append | Write-Host
    "-----------------" | Tee-Object -FilePath $CxDepsLogFile -Append | Write-Host
    Get-ItemProperty -Path "$TargetDirMars\MARSAgentInstaller.exe" | Format-list -Property * -Force | Out-String `
                     | ForEach-Object { $_.Trim() } | Tee-Object -FilePath $CxDepsLogFile -Append
}

function GenerateUnixLEOSDetails {
    $SourceFile = $SourcesRootDirectory + "build\scripts\general\Unix_LE_OS_details.sh"
    $TargetFile = $SourcesRootDirectory + "server\windows\Unix_LE_OS_details.sh"
    try
    {
        $text = [IO.File]::ReadAllText($SourceFile) -replace "`r`n", "`n"
        [IO.File]::WriteAllText($TargetFile, $text)
        Write-Host ("Successfully generated OS_details.sh with Unix line endings.")
    }
    catch
    {
        Write-Host ("Failed to generate OS_details.sh with Unix line endings.")
        Write-Host ("Hit below excecption.")
        Write-Host $_.Exception | Format-List
        exit 1
    }
}

$TargetDirDra = $SourcesRootDirectory + "host\setup\DRA"
$TargetDirMars = $SourcesRootDirectory + "host\setup\MARS"
$CxDepsLogFile = $SourcesRootDirectory + "host\setup\" + "CxDependenciesDetails.log"

# Fetch DRA dependendencies
FetchDraDependendencies

# Generate OS_detaills.sh with Unix line endings
GenerateUnixLEOSDetails