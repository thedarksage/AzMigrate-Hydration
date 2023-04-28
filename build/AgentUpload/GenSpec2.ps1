param(
	
	[Parameter(Mandatory=$false)]
    [string]$TargetDirectory = "$pwd"
)

class ASRMobSvcVersionInfo {
	[string]$DistroName
	[string]$FullVersion
	[string]$AgentDownloadLink
	[string]$AgentSHA256Checksum
	[string]$PushclientDownloadLink
	[string]$PushclientSHA256Checksum
	[string]$ReleaseDate
	[string]$ExpiryDate
	[System.Collections.ArrayList]$UpgradeableVersions = @()
	[System.Collections.ArrayList]$SupportedGeos = @()
	[System.Collections.ArrayList]$MinDependencyVersions = @()
}

class ASRMobSvcSpecFile {
	[hashtable]$ReleaseDetails = @{}
}

Function Get-V2ARcmAgentSpecFromDlc {
[CmdletBinding()]
    param (
        [Parameter(Mandatory=$true, Position=1)]
        [string]$uri,
        [Parameter(Mandatory=$true, Position=2)]
        [string]$outfile
    )

    Begin {

    }

    Process {

        Invoke-webrequest -URI $uri -OutFile $TargetDirectory\$outfile
		$latestSpecFile = Get-Content $TargetDirectory\$outfile
    }
}

Function Update-UpgradeableVersions {
[CmdletBinding()]

    param (
		
		[Parameter(Mandatory=$true, Position=2)]
        $OldSpecs,
		
		[Parameter(Mandatory=$true, Position=3)]
        [String]$DistroName
    )

    Begin {

    }

    Process {
		$SpvObj = Get-Content "$TargetDirectory\spv.json" | Out-String | ConvertFrom-Json
		$AgentEntryExist = $False
		foreach ($item in $OldSpecs.ReleaseDetails)
		{
			if ($item.PSobject.Properties.name -match $DistroName) {
				$AgentEntryExist = $True
				foreach ($entry in $item.$DistroName) {
					foreach ($oldVersion in $SpvObj.SupportedPreviousVersions)
					{
						if ($entry.FullVersion -match $oldVersion)
						{
							$entry.UpgradeableVersions += $AgentVersion
							break
						}
					}
				
				    if ([string]::IsNullOrEmpty($entry.ExpiryDate)) {
				        $entry.ExpiryDate = Get-Date (get-date -Day 1 -Hour 12 -Minute 00 -Second 00).AddMonths(10).AddDays(-1) -UFormat "+%Y-%m-%dT%H:%M:%SZ"
				    }
                }
			}
		}
		return $AgentEntryExist
	}
}

Function Get-MobSvcVersionBasicObj {
[CmdletBinding()]

    param (
        [Parameter(Mandatory=$true, Position=1)]
        $AgentReleaseInfo
    )

    Begin {

    }

    Process {
		#Creating a new object
		$verInfo = New-Object ASRMobSvcVersionInfo
		$verInfo.DistroName = $AgentReleaseInfo.DistroName
		$verInfo.FullVersion = $FullVersion
		$verInfo.AgentDownloadLink = $PartLink + "/" + $AgentReleaseInfo.AgentName
		$verInfo.AgentSHA256Checksum = $AgentReleaseInfo.AgentSHA256Checksum
		$verInfo.PushclientDownloadLink = $PartLink + "/" + $AgentReleaseInfo.PushclientName
		$verInfo.PushclientSHA256Checksum = $AgentReleaseInfo.PushclientSHA256Checksum
		$verInfo.SupportedGeos += "SEACAN"
		$verInfo.SupportedGeos += "CCY"
		$verInfo.SupportedGeos += "ECY"
		$verInfo.ReleaseDate = Get-Date -UFormat "+%Y-%m-%dT%H:%M:%SZ"
		$verInfo.ExpiryDate =  ""
		$verInfo = $verinfo | ConvertTo-Json | ConvertFrom-Json
		return $verInfo
	}
}

Function Hotfix-UpdateVersions {
[CmdletBinding()]

    param (
		
		[Parameter(Mandatory=$true, Position=2)]
        $OldSpecs,
		
        [Parameter(Mandatory=$true, Position=1)]
        $AgentReleaseInfo
    )

    Begin {

    }

    Process {
		$DistroName = $AgentReleaseInfo.DistroName
		foreach ($item in $OldSpecs.ReleaseDetails.$DistroName)
		{
			if ($item.FullVersion -match $AgentVersion)
			{
				$item.FullVersion = $FullVersion
				$item.AgentDownloadLink = $PartLink + "/" + $AgentReleaseInfo.AgentName
				$item.AgentSHA256Checksum = $AgentReleaseInfo.AgentSHA256Checksum
				$item.PushclientDownloadLink = $PartLink + "/" + $AgentReleaseInfo.PushclientName
				$item.PushclientSHA256Checksum = $AgentReleaseInfo.PushclientSHA256Checksum
				break
			}
		}
	}
}

Function Add-V2ARcmAgentSpvToSpec {
[CmdletBinding()]

    param (
        [Parameter(Mandatory=$true, Position=1)]
        [string]$oldspec,
        [Parameter(Mandatory=$true, Position=2)]
        [string]$outfile

    )
	Begin {

    }
    Process {
        $OldSpecs = Get-Content "$TargetDirectory\$oldspec" | Out-String | ConvertFrom-Json
		$Stage1Specs = Get-Content "$TargetDirectory\InputToStage2.json" -ErrorAction SilentlyContinue | Out-String | ConvertFrom-Json
		
		$CommonReleaseInfo = $Stage1Specs.CommonReleaseInfo
		$AgentsReleaseInfo = $Stage1Specs.AgentsReleaseInfo
		
		foreach ($AgentReleaseInfo in $AgentsReleaseInfo) {

			$DistroName = $AgentReleaseInfo.DistroName
			if ($CommonReleaseInfo.DeploymentType -eq "GA") {
				$AgentEntryExist = Update-UpgradeableVersions -OldSpecs $OldSpecs -DistroName $DistroName
				
				#Creating a entry for new agent version in GA
				$verInfo = Get-MobSvcVersionBasicObj -AgentReleaseInfo $AgentReleaseInfo
			
				if ($AgentEntryExist) {
					$OldSpecs.ReleaseDetails.$DistroName += $verinfo
				}
				else {
					$AgentList = @()
					$AgentList += $verInfo
					$OldSpecs.ReleaseDetails | Add-member -Name "$DistroName" -value $AgentList -MemberType NoteProperty
				}
			}
			else {
				Hotfix-UpdateVersions -OldSpecs $OldSpecs -AgentReleaseInfo $AgentReleaseInfo
			}
		}
		$OldSpecs | ConvertTo-Json -Depth 5 | Out-File -FilePath $TargetDirectory\$outfile
		"Succesfully generated a new spec $TargetDirectory\$outfile"
	}
}

Function Get-Stage1Spec {
[CmdletBinding()]

    param (

    )
	Begin {

    }
    Process {
        $Stage1Specs = Get-Content "$TargetDirectory\InputToStage2.json" -ErrorAction SilentlyContinue | Out-String | ConvertFrom-Json
		
		$CommonReleaseInfo = $Stage1Specs.CommonReleaseInfo
		$AgentsReleaseInfo = $Stage1Specs.AgentsReleaseInfo

		#Globla paramters
		$Global:FullVersion = $CommonReleaseInfo.FullVersion
		$Global:AgentVersion = ($FullVersion.Split(".",3) | Select -Index 0,1) -join "."
		$Global:PartLink = $CommonReleaseInfo.PartLink
    
    }
}

try {
	Get-Stage1Spec

	Get-V2ARcmAgentSpecFromDlc -uri "aka.ms/latestmobsvcspec" -outfile "ASRMobSvcSpecProd_old.json"
	Add-V2ARcmAgentSpvToSpec -oldspec "ASRMobSvcSpecProd_old.json" -outfile ("ASRMobSvcSpecProd_" + $FullVersion + ".json")


	Get-V2ARcmAgentSpecFromDlc -uri "aka.ms/latestmobsvcspec_seacan" -outfile "ASRMobSvcSpecCan_old.json"
	Add-V2ARcmAgentSpvToSpec -oldspec "ASRMobSvcSpecCan_old.json" -outfile ("ASRMobSvcSpecCan_" + $FullVersion + ".json")
}
catch
{
	Write-Error "ERROR Message:  $_.Exception.Message"
	Write-Error "ERROR:: $Error | ConvertTo-json -Depth 1"

	throw "GenerateSpec file creation failed !!"
}