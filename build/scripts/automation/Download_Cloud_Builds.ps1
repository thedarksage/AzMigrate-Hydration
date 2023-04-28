param (
    [string]$branch = "develop"
)

if ($branch -eq "release")
{
    $version = "9.54"
}
elseif ($branch -eq "master")
{
    $version = "9.53"
}
else
{
    $version = "9.55"
}

# Note: this script takes a dependency on the current date for downloading cloud builds
# to download a different date build, change the date
$Date = Get-Date -Format dd_MMM_yyyy
$BuildNumber = (New-TimeSpan -Start '2005-01-01' -End $(Get-Date)).Days
$TargetPath = "J:\DailyBuilds\Daily_Builds\$version\HOST\$Date"
$LogName = "Download_Cloud_Builds_"+$(Get-Date -Format dd_MMM_yyyy_hh_mm_tt)+".txt"
$BuildDate = Get-Date -Format ddMMMyyyy

$LogFile = "C:\\Tools\DownloadCloudBuildLogs\$LogName"

$CloudBuildDirName = "\CloudBuildDir\"
$CloudBuildDir = $TargetPath + $CloudBuildDirName

$AgentBuildDirName = "\UnifiedAgent_Builds\release\"
$AgentBuildDir = $TargetPath + $AgentBuildDirName

$UnifiedSetupDirName = "\release\"
$UnifiedSetupDir = $TargetPath + $UnifiedSetupDirName

$PushInstallClientsDirName = "\PushInstallClients\"
$PushInstallClientsDir = $TargetPath + $PushInstallClientsDirName

$PushInstallClientsRcmDirName = "\PushInstallClientsRcm\"
$PushInstallClientsRcmDir = $TargetPath + $PushInstallClientsRcmDirName

$SupportedKernelsDirName = "\UnifiedAgent_Builds\release\supported_kernels\"
$SupportedKernelsDir = $TargetPath + $SupportedKernelsDirName

$AgentBuildLogDirName = "\UnifiedAgent_Builds\release\Logs\"
$AgentBuildLogDir = $TargetPath + $AgentBuildLogDirName

$AgentVersionDirName = "\UnifiedAgent_Builds\release\agent_versions\"
$AgentVersionDir = $TargetPath + $AgentVersionDirName

$LinuxCvtDirName = "\LinuxCVT\"
$LinuxCvtDir = $TargetPath + $LinuxCvtDirName

Function CreateLogFile()
{

    $LogFileExist = Test-Path -Path $LogFile
    if($LogFileExist -eq 'True')
    {
        Remove-Item $LogFile
    }
    New-Item -ItemType "file" -Force -Path $LogFile
}

Function CreateBuildDir()
{
    New-Item -ItemType Directory -Force -Path $TargetPath >> $LogFile
    New-Item -ItemType Directory -Force -Path $CloudBuilDdir >> $LogFile
    New-Item -ItemType Directory -Force -Path $AgentBuildDir >> $LogFile
    New-Item -ItemType Directory -Force -Path $UnifiedSetupDir >> $LogFile
    New-Item -ItemType Directory -Force -Path $PushInstallClientsDir >> $LogFile
    New-Item -ItemType Directory -Force -Path $PushInstallClientsRcmDir >> $LogFile
    New-Item -ItemType Directory -Force -Path $SupportedKernelsDir >> $LogFile
    New-Item -ItemType Directory -Force -Path $AgentBuildLogDir >> $LogFile
    New-Item -ItemType Directory -Force -Path $AgentVersionDir >> $LogFile
    New-Item -ItemType Directory -Force -Path $LinuxCvtDir >> $LogFile
    write "Created directory structure - $TargetPath" >> $LogFile
}

Function DownloadBuilds() {

    write "Build number: $buildNumber Date: $Date Path: $TargetPath" >> $LogFile

    $PATPath = "C:\tools\pat_config.json"
    $PATContent = Get-Content -Raw -Path $PATPath | ConvertFrom-Json
    $PAT = $PATContent.PAT
    $tokenWithUserName = ":" + $PAT
    $tokenBytes = [Text.Encoding]::ASCII.GetBytes($tokenWithUserName)
    $encoded = [Convert]::ToBase64String($tokenBytes)

    $headers = @{
        Authorization = "Basic " + $encoded
    }

    $agentTargets = @(
        @{name = "Microsoft-ASR_UA"; outDir = $agentBuildDirName; buildDetails = "true"},
        @{name = "pushinstallclient"; outDir = $PushInstallClientsRcmDirName; buildDetails = "true"}
        @{name = "supported_kernels"; outDir = $SupportedKernelsDirName; buildDetails = "true"}
        @{name = "AgentBuild"; outDir = $agentBuildLogDirName; buildDetails = "true"}
        @{name = "AgentVersion"; outDir = $agentVersionDirName; buildDetails = "true"}
        @{name = "indskflt_ct"; inDir = "TEST"; outDir = $linuxCvtDirName; buildDetails = "false"; skipFailure = "true"}
    )

    $csTargets = @(
        @{name = "MicrosoftAzureSiteRecoveryUnifiedSetup.exe"; outDir = $unifiedsetupDirName; buildDetails = "false"}
        @{name = "Windows_pushinstallclient.exe"; outDir = $PushInstallClientsRcmDirName; buildDetails = "false"}
    )

    $psTargets = @(
        @{name = "ProcessServer.msi";  outDir = $unifiedsetupDirName; buildDetails = "false"}
    )

    $buildDefinitions = @(
    @{id = "217108"; buildDetails = "true"; targets = $agentTargets; name = "WindowsAgent"},
    @{id = "233071"; buildDetails = "true"; targets = $agentTargets; name = "DEBIAN7-64"},
    @{id = "233306"; buildDetails = "true"; targets = $agentTargets; name = "DEBIAN8-64"},
    @{id = "233307"; buildDetails = "true"; targets = $agentTargets; name = "DEBIAN9-64"},
    @{id = "233069"; buildDetails = "true"; targets = $agentTargets; name = "DEBIAN10-64"},
    @{id = "301282"; buildDetails = "true"; targets = $agentTargets; name = "DEBIAN11-64"},
    @{id = "233308"; buildDetails = "true"; targets = $agentTargets; name = "OL6-64"},
    @{id = "233309"; buildDetails = "true"; targets = $agentTargets; name = "OL7-64"},
    @{id = "233310"; buildDetails = "true"; targets = $agentTargets; name = "OL8-64"},
    @{id = "308970"; buildDetails = "true"; targets = $agentTargets; name = "OL9-64"},
    @{id = "233311"; buildDetails = "true"; targets = $agentTargets; name = "RHEL6-64"},
    @{id = "233312"; buildDetails = "true"; targets = $agentTargets; name = "RHEL7-64"},
    @{id = "233313"; buildDetails = "true"; targets = $agentTargets; name = "RHEL8-64"},
    @{id = "308971"; buildDetails = "true"; targets = $agentTargets; name = "RHEL9-64"},
    @{id = "233314"; buildDetails = "true"; targets = $agentTargets; name = "UBUNTU-14.04-64"},
    @{id = "233315"; buildDetails = "true"; targets = $agentTargets; name = "UBUNTU-16.04-64"},
    @{id = "233316"; buildDetails = "true"; targets = $agentTargets; name = "UBUNTU-18.04-64"},
    @{id = "233061"; buildDetails = "true"; targets = $agentTargets; name = "UBUNTU-20.04-64"},
    @{id = "301283"; buildDetails = "true"; targets = $agentTargets; name = "UBUNTU-22.04-64"},
    @{id = "305929"; buildDetails = "true"; targets = $agentTargets; name = "SLES12-64"},
    @{id = "305931"; buildDetails = "true"; targets = $agentTargets; name = "SLES15-64"},
    @{id = "226623"; buildDetails = "false"; targets = $psTargets; name = "ProcessServer"},
    @{id = "226622"; buildDetails = "false"; targets = $csTargets; name = "UnifiedSetup"}
    )

    foreach ($definition in $buildDefinitions)
    {
        Get-AgentBuild -definition $definition -headers $headers
    }
}

Function Get-AgentBuild {
[CmdletBinding()]

    param (
        [Parameter(Mandatory = $true)]
        [hashtable[]] $definition,
        [Parameter(Mandatory = $true)]
        [hashtable]$headers
    )

    Begin {

    }

    Process {
        try {

            # List all builds
            $definitionId = $definition.id
            $runs = iwr -UseBasicParsing "https://dev.azure.com/msazure/one/_apis/build/builds?definitions=$definitionId&api-version=5.1" -Headers $headers -ErrorAction SilentlyContinue
            if ($runs.StatusCode -ne 200) {
                write-Error "Can't get successful response from DevOps api: $($runs.StatusCode)" >> $LogFile
            }

            $runs = $runs | ConvertFrom-Json
            write "Found $($runs.count) runs total" >> $LogFile

            # Find most recent from $branch branch
            $branchRef = "refs/heads/" + $branch

            #$runs = $runs.value | ?{ $_.status -eq "completed" -and ($_.result -eq "succeeded" -or $_.result -eq "partiallySucceeded") -and $_.sourceBranch -eq $branchRef } | sort -Descending startTime
            # TODO: ignoring SDL failures
            $runs = $runs.value | ?{ $_.status -eq "completed" -and $_.sourceBranch -eq $branchRef } | sort -Descending startTime

            write "Found $($runs.count) completed successful runs for branch '$branch' runs" >> $LogFile

            if ($runs.count -eq 0) {
                write "##[warning]Can't find last successful Apim-Ap run for '$branch' branch" >> $LogFile
                exit 1
            }

            $run = $runs[0]
            $buildId = $run.id
            write "Using run $buildId started at $($run.startTime): https://dev.azure.com/msazure/one/_build/results?buildId=$buildId" >> $LogFile

            $drop = iwr -UseBasicParsing "$($runs[0].url)/artifacts?artifactName=drop_build_main&api-version=5.1" -Headers $headers | ConvertFrom-Json
            $downloadUrl = $drop.resource.downloadUrl

            if ($definition.buildDetails -eq "true")
            {
                # get Builds_details.txt
                $buildDetailsFile = "Build_details.txt"
                $downloadFile = "format=file&subPath=%2F"+$buildDetailsFile
                $targetUrl = $downloadUrl -replace "format=zip", $downloadFile

                $outFile = $($cloudbuilddir + $definitionId + "_" + $buildDetailsFile)
                try
                {
                    New-Item -ItemType Directory -Force -Path $cloudbuilddir  >> $LogFile
                    GetArtifact -Name $buildDetailsFile -Uri $targetUrl -OutFile $outFile -Headers $headers
                }
                catch [Exception]
                {
                    write $_.Exception | format-list -force >> $LogFile
                    exit 1
                }

                #read Build details
                Get-Content $outFile >> $LogFile

                $buildVersion = Get-Content $outFile -TotalCount 1

                if ($buildVersion -notmatch $buildNumber)
                {
                    write "For build definition $definitionId Build_details build verion $buildVersion doesn't contain todays build number $buildNumber." >> $LogFile
                    return
                }

                $agentfileName = (Select-String -Path $outFile -Pattern "Microsoft-ASR_UA").Line
                if ($agentfileName -notmatch $BuildDate)
                {
                    write "For build definition $definitionId file name $agentFileName doesn't contain todays build date $BuildDate." >> $LogFile
                    #return
                }

                $buildDetailsFilePath = $outFile
            }

            $targets = $definition.targets
            ForEach ($t in $targets)
            {
                #if ($definition.buildDetails -ne $t.buildDetails)
                #{
                #    write "Skipping $($t.name) from download for $definitionId" >> $LogFile
                #    continue
                #}

                if ($t.buildDetails -eq "true")
                {
                    $fileName = (Select-String -Path $buildDetailsFilePath -Pattern $t.name).Line
                    if ([string]::IsNullOrEmpty($fileName))
                    {
                        write "$($t.name) is not found in $buildDetailsFilePath" >> $LogFile
                        continue
                    }
                }
                else
                {
                    $fileName = $t.name
                    write "download $($t.name) without build definition for defintionId $definitionId" >> $LogFile
                }

                $downloadFile = "format=file&subPath=%2F"
                if ($t.containsKey("inDir"))
                {
                    $filePathArr = $($t.inDir).Split("\")
                    foreach ($fileCurDir in $filePathArr)
                    {
                        $downloadFile = $downloadFile + "$fileCurDir" + "%2F"
                    }
                }
                $downloadFile = $downloadFile + $fileName

                $targetUrl = $downloadUrl -replace "format=zip", $downloadFile
                $outFile = $TargetPath + $t.outDir + $fileName
                try
                {
                  GetArtifact -Name $fileName -Uri $targetUrl -OutFile $outFile -Headers $headers
                  write "Downloaded $fileName from $targetUrl to $outFile" >> $LogFile
                  $outFileName = [System.IO.Path]::GetFileName($outFile)
                  if ($fileName -match "pushinstallclient")
                  {
                      if ($fileName -match "Windows")
                      {
                        $fileBaseName = [System.IO.Path]::GetFileNameWithoutExtension($outFileName)
                        $fileExtName = [System.IO.Path]::GetExtension($outFileName)
                      }
                      else
                      {
                        write "Copy push install client" >> $LogFile
                        Copy-Item "$outFile" -Destination "$PushInstallClientsDir"
                        $fileBaseName = [System.IO.Path]::GetFileNameWithoutExtension($outFileName)
                        $fileExtName = [System.IO.Path]::GetExtension($fileBaseName) + [System.IO.Path]::GetExtension($outFileName)
                        $fileBaseName = [System.IO.Path]::GetFileNameWithoutExtension($fileBaseName)
                      }
                      write "Renaming push install client to ${fileBaseName}_${version}${fileExtName}" >> $LogFile
                      Rename-Item -Path "$outFile" -NewName "${fileBaseName}_${version}${fileExtName}"
                  }
                  elseif ($fileName -match "indskflt_ct")
                  {
                    write "Renaming indskflt_ct to ${outFileName}_$($definition.name)" >> $LogFile
                    Rename-Item -Path "$outFile" -NewName "${outFileName}_$($definition.name)"
                  }
                }
                catch [Exception]
                {
                  write $_.Exception | format-list -force >> $LogFile
                  if ( -not $t.ContainsKey("skipFailure") -or -not $($t.SkipFailure))
                  {
                    exit 1
                  }
                }
            }

            write "Successfully downloaded the artifacts for definition id: $definitionId, name : $($definition.name)" >> $LogFile
        }
        catch [Exception] {
            write $_.Exception | format-list -force >> $LogFile
        }
    }
}


Function GetArtifact($Name, $Uri, $OutFile, [hashtable]$Headers, [int]$Retries = 3, [int]$SecondsDelay = 10)
{
    $retryCount = 1
    $completed = $false


    Write "Request to download artifact:$Name , url: $Uri , outfile: $OutFile, MaxRetries: $Retries" >> $LogFile

    while ( -not $completed ) {
        Write "Attempt to download artifact:$Name, RetryCount:$retryCount" >> $LogFile
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
                write "Request to download artifact:$Name , url: $Uri failed on the $retryCount attempt." >> $LogFile
                throw
            }
            else
            {
                write "Request to download artifact:$Name , url: $Uri failed. Retrying in $SecondsDelay seconds." >> $LogFile
                Start-Sleep $SecondsDelay
                $retrycount++
            }
        }
    }

    write "Downloaded $Name. OK." >> $LogFile
}


##### MAIN ####

CreateLogFile
CreateBuildDir
DownloadBuilds
Move-Item $LogFile $cloudbuilddir -ErrorAction SilentlyContinue
exit 0
