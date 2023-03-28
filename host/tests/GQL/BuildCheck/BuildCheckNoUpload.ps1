. $PSScriptRoot/TestStatus.ps1

Function CheckStatus()
{
    param (
        [Parameter(Mandatory=$true)][string] $Path,
        [Parameter(Mandatory=$true)][string] $SubDir,
        [Parameter(Mandatory=$true)][string] $File    
    )

    $File2 = $File
    $File = $Path + "/" + $SubDir + "/" + $File
    Write-Host "Checking $File"
 
    $found=0
    if (Test-Path -Path $File) {
        if ($File2 -eq "DeployPS.log") {
            foreach($line in Get-Content $File) {
                echo $line | grep -q "Successfully validated TestImage"
                if ($?) {
                    $found=1
                    break
                }
            }

            if ($found -eq "0" ) {
                #TestStatusLog $Logger $Key "FAILED"
                Write-Host "$Key + FAILED"
                return
            }
        }
#        TestStatusLog $Logger $Key "PASSED"
        Write-Host "$Key + PASSED"
    } else {
#        TestStatusLog $Logger $Key "FAILED"
        Write-Host "$Key + FAILED"
    }
}

$Date = Get-Date -Format "dd_MMM_yyyy"
$Branches = @("develop", "release")

foreach ($Branch in $Branches) {
    Write-Host "Checkin $Branch"
    $Version = $Branch

    $Path = "/home/builds/Daily_Builds/"
       
    if ($Branch -eq "develop") {
        $Cmd = "ls " + $Path +" | grep -e ^[9] | sort -rn | sed -n 1p"
        $Version = Invoke-Expression -Command $Cmd
    } elseif ($Branch -eq "release") {
        $Cmd = "ls " + $Path +" | grep -e ^[9] | sort -rn | sed -n 2p"
        $Version = ls $Path | grep -e ^[9] | sort -rn | sed -n 2p
    }

    Write-Host "$Branch = $Version"

    $Path = $Path + $Version + "/HOST/" + $Date 
    
    if (Test-Path -Path $Path) {
        Write-Host "Checking release branch"
    } else {
        Write-Host "Skipping release branch as no build"
        continue
    }

#   $Logger = TestStatusGetContext $Version Build $Date
#   TestStatusInit $Logger 

    foreach ($Key in $Scenarios["BUILD"]) {
        $File = ""
        $SubDir = ""
        switch ($Key) {
            "Branch" {Write-Host "Skip $Key"; break}
            
            "Logs" {Write-Host "Skip $Key"; break}
            
            "PS.msi" {$SubDir = "release"; $File = "ProcessServer.msi"; break}
            
            "UnifiedSetup" {$SubDir = "release"; 
                            $File = "MicrosoftAzureSiteRecoveryUnifiedSetup.exe"; 
                            break}

            "PSGImage" {$SubDir = "."; $File = "DeployPS.log"; break}

            $Key {$SubDir = "UnifiedAgent_Builds/release";
                  $File = "Microsoft-ASR_UA_*" + $Key + "*";
                  break}
        }

        if ( $File) {
            CheckStatus $Path $SubDir $File
        }
    }
}

$NewKernelPath = "/home/builds/Daily_Builds/LinuxNewKernels/"

$NewKernelPath += "NewKernels-" + $Date

Write-Host $NewKernelPath

if (Test-Path -Path $NewKernelPath) {
    Write-Host "Checking Linux new kernels"

    $NewKernelInfo = Get-Content $NewKernelPath -ErrorAction SilentlyContinue | Out-String | ConvertFrom-Json
    $NewKernels=$NewKernelInfo.NewKernels

    #   $Logger = TestStatusGetContext NewKernels Build $Date
    #   TestStatusInit $Logger 
    
    foreach ($Key in $Scenarios["BUILD"]) {
        $Value=""

        switch ($Key) {
            "Branch" {Write-Host "Skip $Key"; break}

            "Windows" {Write-Host "Skip $Key"; break}
            
            "Logs" {Write-Host "Skip $Key"; break}
            
            "PS.msi" {Write-Host "Skip $Key"; break}
            
            "UnifiedSetup" {Write-Host "Skip $Key"; break}

            "PSGImage" {Write-Host "Skip $Key"; break}

            $Key {
                    foreach ( $NewKernel in $NewKernels ) {
                        $Distro = $NewKernel.Distro
                        if ($Key -eq $Distro) {
                            if ([string]::IsNullOrEmpty($Value)) {
                                $Value += $NewKernel.Date + " : " + $NewKernel.Kernel
                            }
                            else{
                                $Value += ", " + $NewKernel.Date + " : " + $NewKernel.Kernel
                            }
                        }
                    }
                    break}

        }

        if ([string]::IsNullOrEmpty($Value)) {
            Write-Host $Key  "NA"
            # TestStatusLog $Logger $Key "NA"

        }
        else {
            Write-Host $Key $Value
            # TestStatusLog $Logger $Key $Value

        }
    }
} else {
    Write-Host "Skipping Linux new kernel check"
}
