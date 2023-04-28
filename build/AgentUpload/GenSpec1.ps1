param(

	[Parameter(Mandatory=$true)]
    [String] $BuildPath,

    [Parameter(Mandatory=$true)]
    [String] $BuildVersion,

    [Parameter(Mandatory=$true)]
    [String[]] $Distros,
	
	[Parameter(Mandatory=$true)]
	[ValidateSet("GA", "HOTFIX")]
    [string]$DeploymentType,
	
	[Parameter(Mandatory=$false)]
    [String] $BuildDate = "Default",
	
	[Parameter(Mandatory=$false)]
    [string]$TargetDirectory = "$pwd",
	
	[Parameter(Mandatory=$false)]
    [boolean]$SkipCopy = $false
	
)

class ASRMobSvcCommonInfo {
	[string]$PartLink
	[string]$FullVersion
	[string]$DeploymentType
}

class ASRMobSvcVersionInfo {
	[string]$DistroName
	[string]$AgentName
	[string]$AgentSHA256Checksum
	[string]$PushclientName
	[string]$PushclientSHA256Checksum
}

Function Get-FilesToTransfer {
[CmdletBinding()]

    param (
        [Parameter(Mandatory=$true, Position=1)]
        [System.Collections.ArrayList]$FilesList,
		
		[Parameter(Mandatory=$true, Position=2)]
        [String[]] $Distros
    )

    Begin {

    }

    Process {
		$FilesToTransfer = @()
		foreach ($file in $FilesList) {
            if ($Distros.Count -eq 1 -and $Distros[0] -eq "all") {
                $FilesToTransfer += $file
            }
            else {
                foreach($distro in $Distros) {
                    if ($file -match $distro) {
                        Write-host "Found distro " $distro " in file" $file
                        $FilesToTransfer += $file
						break
                    }
                }
            }
        }
		return $FilesToTransfer
    }
}

Function CreateJsonForSecondStep {
[CmdletBinding()]
    param (
        [Parameter(Mandatory=$true, Position=1)]
        [String] $MobSvcFullVersion,

        [Parameter(Mandatory=$true, Position=2)]
        [String] $BitsPath
    )

    Begin {

    }

    Process {
	
		$SpecObj = @{}
		
		$GlobalInfoObj = New-Object ASRMobSvcCommonInfo
		$GlobalInfoObj.PartLink = ""
		$GlobalInfoObj.FullVersion = $MobSvcFullVersion
		$GlobalInfoObj.DeploymentType = $DeploymentType
		
		$SpecObj["CommonReleaseInfo"] = $GlobalInfoObj
		
		$AgentObj = @()
		ls $BitsPath\*pushinstallclient* | foreach {
			$pushClientPath = $_
			$distroName = $pushClientPath.Name.Split("_")[0]
			$agentBitsPath = ls $BitsPath\*Microsoft*$distroName*
			
			#Fill in a single object
			$distroInfo = New-Object ASRMobSvcVersionInfo
			$distroInfo.DistroName = $distroName
			$distroInfo.AgentName = $agentBitsPath.Name
			$distroInfo.AgentSHA256Checksum = (Get-FileHash $agentBitsPath).Hash
			$distroInfo.PushclientName = $pushClientPath.Name
			$distroInfo.PushclientSHA256Checksum = (Get-FileHash $pushClientPath).Hash
			
			$AgentObj += $distroInfo
		}
		$SpecObj["AgentsReleaseInfo"] = $AgentObj
		$SpecObj | ConvertTo-Json | Out-File -FilePath $TargetDirectory\InputToStage2.json
    }
}
Function Get-V2ARcmAgentBuilds {
[CmdletBinding()]

    param (
        [Parameter(Mandatory=$false)]
        [boolean] $SkipCopy,

        [Parameter(Mandatory=$false)]
        [boolean] $ForceClean
    )

    Begin {

    }

    Process {
        try {
            Write-Host "Remote build path : " $BuildPath

            $agentBuildsPath = $BuildPath + "\UnifiedAgent_Builds\release\"
            $pushClientBuildsPath = $BuildPath + "\PushInstallClientsRcm\"

            $MobSvcFullVersion = Get-Content -Path ($agentBuildsPath + "AgentVersion.txt")
            Write-Host " Mobility Service Full Version " $MobSvcFullVersion

            $destDir = $TargetDirectory + "\" + $MobSvcFullVersion + "\"
            Write-Host "DestDir : $destDir"
            if (!$SkipCopy) {
                Remove-Item -Path $destDir -Recurse -Force -ErrorAction SilentlyContinue

                New-Item -Path $destDir -ItemType Directory

                $agentFiles = Get-ChildItem $agentBuildsPath -Filter 'Microsoft-ASR_UA_*' -Name

                Write-Host "agentFiles : $agentFiles"

                $pushClientFiles = Get-ChildItem $pushClientBuildsPath -Filter '*pushinstallclient*'

                Write-Host "pushClientFiles : $pushClientFiles"

                $agentFilesToTransfer = Get-FilesToTransfer -FilesList $agentFiles -Distros $Distros
                $pushClientFilesToTransfer = Get-FilesToTransfer -FilesList $pushClientFiles -Distros $Distros

                "Agent files to transfer : "
                $agentFilesToTransfer
                ""
                "Push client files to transfer : "
                $pushClientFilesToTransfer

                Write-Host "AgentFilesCount - $agentFileseToTransfer.Count, pushclientbinaries count - $pushClientFilesToTransfer.Count"
                if ($agentFilesToTransfer.Count -le 0 -or $pushClientFilesToTransfer.Length -le 0)
                {
                    Throw "Could not find agent builds or push client binaries. agentFilesCount - $agentFileseToTransfer.Count, pushclientbinaries count - $pushClientFilesToTransfer.Count"
                }

                foreach ($agentFileToTransfer in $agentFilesToTransfer) {
                    $targetAgentFile = $agentFileToTransfer -replace "Microsoft-ASR_UA_[0-9]+.[0-9]+.0.0", "Microsoft-ASR_UA_$MobSvcFullVersion"
                    Write-Host $targetAgentFile
                    Start-BitsTransfer -Source ($agentBuildsPath + $agentFileToTransfer) -Destination ($destDir + $targetAgentFile)
                }
                foreach ($pushClientFileToTransfer in $pushClientFilesToTransfer) {
                    $targetpushClientFile = $pushClientFileToTransfer -replace "_[0-9]+.[0-9]+.", "_$MobSvcFullVersion."
                    Write-Host $targetpushClientFile
                    Start-BitsTransfer -Source ($pushClientBuildsPath + $pushClientFileToTransfer) -Destination ($destDir + $targetpushClientFile)
                }
            }

            CreateJsonForSecondStep -MobSvcFullVersion $MobSvcFullVersion -BitsPath $destDir
        }
        catch {
            Write-Host "Failed to generate spec file"
            Throw
        }
    }
}

# GenSpec1.ps1 -BuildPath "\\InmStagingSvr.fareast.corp.microsoft.com\DailyBuilds\Daily_Builds\9.53\HOST\15_Dec_2022" -BuildVersion 9.41 -Distros DEBIAN7-64,DEBIAN8-64,DEBIAN9-64,OL6-64,OL7-64,OL8-64,RHEL6-64,RHEL7-64,RHEL8-64,SLES11-SP3-64,SLES11-SP4-64,SLES12-64,SLES15-64,UBUNTU-14.04-64,UBUNTU-16.04-64,UBUNTU-18.04-64,UBUNTU-20.04-64,Windows -DeploymentType HOTFIX

Get-V2ARcmAgentBuilds -SkipCopy $SkipCopy

