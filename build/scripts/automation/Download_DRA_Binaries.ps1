param (
    [string]$organization = "msazure",
    [string]$project = "one",
    [string]$definitionId = "215888",
    [string]$branch = "develop",
    [string]$artifactName = "drop_build_retail_amd64",
    [Parameter(Mandatory=$true)][string]$targetPath
)

#Declaring the variables for log name.
$Date = Get-Date -Format dd_MMM_yyyy
$DRALog_Name = "Copy_DRA_Binaries_"+$(Get-Date -Format dd_MMM_yyyy_hh_mm_tt)+".txt"
$DRALog_File = "H:\BUILDS\Daily_Builds\$branch\HOST\$Date\$DRALog_Name"

Function CreateLogFile() 
{
  $DRALogFileExist = Test-Path -Path $DRALog_File
  if($DRALogFileExist -eq 'True') 
  {
    Remove-Item $DRALog_File
  }
  New-Item -ItemType "file" -Force -Path $DRALog_File
}

Function RemoveAndCreateArtifactsDir()
{
    write "Removing already exist directory structure : $targetPath" >> $DRALog_File
    Remove-Item $targetPath -Recurse >> $DRALog_File
    write "Deleted directory structure : $targetPath" >> $DRALog_File  
    New-Item -ItemType Directory -Force -Path $targetPath >> $DRALog_File
    write "Created directory structure - $targetPath" >> $DRALog_File
}

Function GetArtifact($Name, $Uri, $OutFile, [hashtable]$Headers, [int]$Retries = 3, [int]$SecondsDelay = 10)
{
    $retryCount = 1
    $completed = $false
    
    Write "Request to download artifact:$Name , url: $Uri , outfile: $OutFile, MaxRetries: $Retries" >> $DRALog_File
    
    while ( -not $completed ) {
        Write "Attempt to download artifact:$Name, RetryCount:$retryCount" >> $DRALog_File
        try 
        {
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            $ProgressPreference = 'SilentlyContinue'
            iwr -UseBasicParsing $Uri -Headers $Headers -OutFile $OutFile -TimeoutSec 900
            $completed = $true
        }
        catch [Exception]
        {
            $FileExist = Test-Path -Path $OutFile
            if($FileExist -eq 'True') 
            {
                Remove-Item $OutFile
            }
            
            if ($retrycount -ge $Retries)
            {
                write "Request to download artifact:$Name , url: $Uri failed on the $retryCount attempt." >> $DRALog_File
                throw
            } 
            else
            {
                write "Request to download artifact:$Name , url: $Uri failed. Retrying in $SecondsDelay seconds." >> $DRALog_File
                Start-Sleep $SecondsDelay
                $retrycount++
            }
        }
    }
    
    Write-Host "Downloaded $Name. OK." >> $DRALog_File
}

Function DownloadArtifacts() {
    
        $targets = @(
        @{name="VMware.Interfaces.resources.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2Fen%2FVMware.Interfaces.resources.dll"; outFile="\VMware.Interfaces.resources.dll"},
        @{name="AccessControl2.S2S.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FAccessControl2.S2S.dll"; outFile="\AccessControl2.S2S.dll"},
        @{name="AsyncInterface.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FAsyncInterface.dll"; outFile="\AsyncInterface.dll"},
        @{name="AutoMapper.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FAutoMapper.dll"; outFile="\AutoMapper.dll"},
        @{name="CatalogCommon.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FCatalogCommon.dll"; outFile="\CatalogCommon.dll"},
        @{name="CloudCommonInterface.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FCloudCommonInterface.dll"; outFile="\CloudCommonInterface.dll"},
        @{name="CloudSharedInfra.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FCloudSharedInfra.dll"; outFile="\CloudSharedInfra.dll"},
        @{name="DRResources.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FDRResources.dll"; outFile="\DRResources.dll"},
        @{name="EndpointsConfig.xml";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FEndpointsConfig.xml"; outFile="\EndpointsConfig.xml"},
        @{name="ErrorCodeUtils.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FErrorCodeUtils.dll"; outFile="\ErrorCodeUtils.dll"},
        @{name="IdMgmtApiClientLib.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FIdMgmtApiClientLib.dll"; outFile="\IdMgmtApiClientLib.dll"},
        @{name="IdMgmtInterface.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FIdMgmtInterface.dll"; outFile="\IdMgmtInterface.dll"},
        @{name="InMageDiscovery.exe";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FInMageDiscovery.exe"; outFile="\InMageDiscovery.exe"},
        @{name="IntegrityCheck.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FIntegrityCheck.dll"; outFile="\IntegrityCheck.dll"},
        @{name="Microsoft.ApplicationInsights.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FMicrosoft.ApplicationInsights.dll"; outFile="\Microsoft.ApplicationInsights.dll"},
        @{name="Microsoft.Identity.Client.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FMicrosoft.Identity.Client.dll"; outFile="\Microsoft.Identity.Client.dll"},
        @{name="Newtonsoft.Json.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FNewtonsoft.Json.dll"; outFile="\Newtonsoft.Json.dll"},
        @{name="Polly.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FPolly.dll"; outFile="\Polly.dll"},
        @{name="SetupFramework.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FSetupFramework.dll"; outFile="\SetupFramework.dll"},
        @{name="SrsRestApiClientLib.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FSrsRestApiClientLib.dll"; outFile="\SrsRestApiClientLib.dll"},
        @{name="STSService.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FSTSService.dll"; outFile="\STSService.dll"},
        @{name="TelemetryInterface.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FTelemetryInterface.dll"; outFile="\TelemetryInterface.dll"},
        @{name="Vim25Service.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FVim25Service.dll"; outFile="\Vim25Service.dll"},
        @{name="VMware.Interfaces.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FVMware.Interfaces.dll"; outFile="\VMware.Interfaces.dll"},
        @{name="VMware.VSphere.Management.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FASRAdapterFiles%2FVMware.VSphere.Management.dll"; outFile="\VMware.VSphere.Management.dll"},
        @{name="Microsoft.IdentityModel.dll";   downloadFile="format=file&subPath=%2Fretail-amd64%2FInMageFabricExtension%2FMicrosoft.IdentityModel.dll"; outFile="\Microsoft.IdentityModel.dll"},
        @{name="AzureSiteRecoveryConfigurationManager.msi";   downloadFile="format=file&subPath=%2Fretail-amd64%2FAzureSiteRecoveryConfigurationManager.msi"; outFile="\AzureSiteRecoveryConfigurationManager.msi"},
        @{name="AzureSiteRecoveryProvider.exe";   downloadFile="format=file&subPath=%2Fretail-amd64%2FAzureSiteRecoveryProvider.exe"; outFile="\AzureSiteRecoveryProvider.exe"}
    )
    
    $PATPath = "C:\DRAConfig\dra_pat_config.json"
    $PATContent = Get-Content -Raw -Path $PATPath | ConvertFrom-Json
    $PAT = $PATContent.PAT
    $tokenWithUserName = ":" + $PAT
    $tokenBytes = [Text.Encoding]::ASCII.GetBytes($tokenWithUserName)
    $encoded = [Convert]::ToBase64String($tokenBytes)

    $headers = @{
        Authorization = "Basic " + $encoded
    }

    # List all APIM-AP builds
    $runs = iwr -UseBasicParsing "https://dev.azure.com/$organization/$project/_apis/build/builds?definitions=$definitionId&api-version=5.1" -Headers $headers -ErrorAction SilentlyContinue
    if ($runs.StatusCode -ne 200) {
        write-Error "Can't get successful response from DevOps api: $($runs.StatusCode)" >> $DRALog_File
    }

    $runs = $runs | ConvertFrom-Json
    write "Found $($runs.count) runs total" >> $DRALog_File

    # Find most recent from $branch branch
    $branchRef = "refs/heads/" + $branch
    $runs = $runs.value | ?{ $_.status -eq "completed" -and ($_.result -eq "succeeded" -or $_.result -eq "partiallySucceeded") -and $_.sourceBranch -eq $branchRef } | sort -Descending startTime
    write "Found $($runs.count) completed successful runs for branch '$branch' runs" >> $DRALog_File

    if ($runs.count -eq 0) {
        write "##[warning]Can't find last successful Apim-Ap run for '$branch' branch" >> $DRALog_File
        exit 1
    }

    $run = $runs[0]
    $buildId = $run.id
    write "Using run $buildId started at $($run.startTime): https://dev.azure.com/$organization/$project/_build/results?buildId=$buildId" >> $DRALog_File

    $drop = iwr -UseBasicParsing "$($runs[0].url)/artifacts?artifactName=$artifactName&api-version=5.1" -Headers $headers | ConvertFrom-Json
    $downloadUrl = $drop.resource.downloadUrl

    foreach ($t in $targets)
    {
        $targetUrl = $downloadUrl -replace "format=zip", $t.downloadFile
        $outFile = $($targetPath + $t.outFile)
        try
        {
          GetArtifact -Name $t.name -Uri $targetUrl -OutFile $outFile -Headers $headers
        }
        catch [Exception]
        {
          write $_.Exception | format-list -force >> $DRALog_File
          exit 1
        }
    }
    write "Successfully downloaded the artifacts: $artifactName" >> $DRALog_File
    
    #Logging DRA version
    Get-ItemProperty -Path "$targetPath\AzureSiteRecoveryProvider.exe" | Format-list -Property * -Force >> $DRALog_File
}

CreateLogFile
RemoveAndCreateArtifactsDir
DownloadArtifacts
exit 0