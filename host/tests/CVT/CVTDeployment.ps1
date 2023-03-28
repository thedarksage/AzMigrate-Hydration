<#
.SYNOPSIS
    Powershell script to copy the agent builds, test binaries, install and uninstall agent
#>

# Input parameters
[CmdletBinding(DefaultParameterSetName = 'Uninstall')]
param (
    [Parameter(ParameterSetName = "Install", Mandatory = $true)]
    [bool] $InstallAgent,
    [Parameter(ParameterSetName = "Copy", Mandatory = $true)]
    [Parameter(ParameterSetName = "Install", Mandatory = $true)]
    [Parameter(ParameterSetName = "CopyBinariesCloudTest", Mandatory = $true)]
    [Parameter(ParameterSetName = "InstallAgentOnpremisesCloudTest", Mandatory = $true)]
    [String] $BuildLocation,
    [Parameter(ParameterSetName = "Copy", Mandatory = $true)]
    [Parameter(ParameterSetName = "Install", Mandatory = $true)]
    [Parameter(ParameterSetName = "CopyBinariesCloudTest", Mandatory = $true)]
    [Parameter(ParameterSetName = "InstallAgentOnpremisesCloudTest", Mandatory = $true)]
    [ValidateSet("develop", "release", "custom", IgnoreCase = $true)]
    [String] $BuildType,
    [Parameter(ParameterSetName = "Copy", Mandatory = $true)]
    [Parameter(ParameterSetName = "Install", Mandatory = $true)]
    [Parameter(ParameterSetName = "CopyBinariesCloudTest", Mandatory = $true)]
    [Parameter(ParameterSetName = "InstallAgentOnpremisesCloudTest", Mandatory = $true)]
    [Parameter(ParameterSetName = "InstallAgentAzureCloudTest", Mandatory = $true)]
    [String] $LocalBuildPath,
    [Parameter(ParameterSetName = "Copy", Mandatory = $true)]
    [Parameter(ParameterSetName = "Install", Mandatory = $true)]
    [Parameter(ParameterSetName = "InstallAgentOnpremisesCloudTest", Mandatory = $true)]
    [String] $LocalCVTBinPath,
    [Parameter(ParameterSetName = "Install", Mandatory = $true)]
    [Parameter(ParameterSetName = "InstallAgentOnpremisesCloudTest", Mandatory = $true)]
    [Parameter(ParameterSetName = "InstallAgentAzureCloudTest", Mandatory = $true)]
    [ValidateSet("azure", "vmware", IgnoreCase = $true)]
    [String] $Platform,
    [Parameter(ParameterSetName = "Uninstall", Mandatory = $true)]
    [bool] $UnInstallAgent,
    [Parameter(ParameterSetName = "Copy", Mandatory = $true)]
    [bool] $CopyBinaries,
    [Parameter(ParameterSetName = "CopyBinariesCloudTest", Mandatory = $true)]
    [bool] $CopyBinariesCloudTest,
    [Parameter(ParameterSetName = "InstallAgentOnpremisesCloudTest", Mandatory = $true)]
    [bool] $InstallAgentOnpremisesCloudTest,
    [Parameter(ParameterSetName = "InstallAgentAzureCloudTest", Mandatory = $true)]
    [bool] $InstallAgentAzureCloudTest
)

# Enum for LogType
Add-Type -TypeDefinition @"
   public enum LogType
   {
      Error,
      Info,
      Warning
   }
"@

# Enum for DeploymentStatus
Add-Type -TypeDefinition @"
   public enum DeploymentStatus
   {
      PASSED,
      FAILED_BUILD_NOT_FOUND,
      FAILED_COPYBUILDS,
      FAILED_COPYCVTBINARIES,
      FAILED_UNINSTALL,
      FAILED_INSTALL,
      FAILED_UNKNOWN_ERROR
   }
"@

#
# Include common library files
#
. $PSScriptRoot\CommonFunctions.ps1

# Global Variables
$FareastUser = "FAREAST\IDCDLSLB"
$global:LogDir = $env:SystemDrive + "\TestResults"
$global:LogName = "$LogDir\Deployment_$env:computername.txt"
$global:LocalBuildName = "SourceAgent.exe"
$global:DeploymentStatus = [DeploymentStatus]::PASSED
if ($BuildType) {
    $global:BuildType = $BuildType.ToLower()
}

if ($BuildLocation) {
    $global:BuildPath = $BuildLocation
}
<#
.SYNOPSIS
    Get the latest build location
#>
Function GetBuildPath() {
    try {
        LogMessage -Message ("Copying builds to {0}" -f $env:computername) -LogType ([LogType]::Info)
        LogMessage -Message ("BuildLocation : {0}, BuildType : {1}" -f $BuildLocation, $global:BuildType) -LogType ([LogType]::Info)

        if (!(Test-Path $BuildLocation)) {
            LogMessage -Message ("Build location {0} doesn't exist" -f $BuildLocation) -LogType ([LogType]::Error)
            throw
        }

        if (!($global:BuildType -match "custom")) {
            $buildFolder = $BuildLocation + "\9.*"

            $devBuild = 0
            $relBuild = 0
            $dirs = Get-ChildItem $buildFolder | Where-Object { $_.PSIsContainer }
            $dirs | Foreach-Object {
                $minorBuildNum = $_.BaseName.split(".")[1]
                if ($minorBuildNum -gt $devBuild) {
                    $relBuild = $devBuild
                    $devBuild = $minorBuildNum
                }
                else {
                    if ($minorBuildNum -gt $relBuild) {
                        $relBuild = $minorBuildNum
                    }
                }
            }

            LogMessage -Message ("Dev Build = 9.{0} and Rel Build = 9.{1}" -f $devBuild, $relBuild) -LogType ([LogType]::Info)

            if ($global:BuildType -match "develop") {
                $buildFolder = $BuildLocation + "\9." + $devBuild + "\Host"
            }
            else {
                $buildFolder = $BuildLocation + "\9." + $relBuild + "\Host"
            }

            $buildDate = $null
            $dirs = Get-ChildItem $buildFolder | Where-Object { $_.PSIsContainer }
            $dirs | Foreach-Object {
                $directory = $_.BaseName
                Get-Date $directory.replace("_", "-")
                if (!$?) {
                    LogMessage -Message ("Not a valid build directory {0}...skipping" -f $directory) -LogType ([LogType]::Info)
                    return
                }

                if ($null -eq $buildDate) {
                    $buildDate = $directory
                }
                else {
                    $date1 = Get-Date $buildDate.replace("_", "-")
                    $date2 = Get-Date $directory.replace("_", "-")

                    $diff = New-TimeSpan -Start $date1 -end $date2
                    if ($diff.Days -gt 0) {
                        $buildDate = $directory
                    }
                }
            }

            if ($null -eq $buildDate) {
                LogMessage -Message ("Build location {0} doesn't contain any valid build date directory" -f $buildFolder) -LogType ([LogType]::Error)
                throw
            }
            $buildDate = $buildDate.Trim();
            $buildFolder = $buildFolder + "\" + $buildDate

            $buildFile = $buildFolder + "\UnifiedAgent_Builds\release\Microsoft-ASR_UA_*.exe"

            if ( !(Test-Path $buildFile)) {
                LogMessage -Message ("The build {0} doesn't exist" -f $buildFile) -LogType ([LogType]::Error)
                throw
            }
            $global:BuildPath = $buildFolder
        }
        LogMessage -Message ( "BuildPath : {0} " -f $global:BuildPath) -LogType ([LogType]::Info)
    }
    catch {
        $global:DeploymentStatus = [DeploymentStatus]::FAILED_BUILD_NOT_FOUND
        throw
    }
}

<#
.SYNOPSIS
    Copies Windows unified agent build to the test machine
#>
Function CopyBuild() {
    try {
        if ( $(Test-Path -Path $LocalBuildPath -PathType Container) ) {
            LogMessage -Message ("Removing the dir : {0}" -f $LocalBuildPath) -LogType ([LogType]::Info)
            Remove-Item $LocalBuildPath -Recurse -Force
        }

        LogMessage -Message ("Creating the dir : {0}" -f $LocalBuildPath) -LogType ([LogType]::Info)
        New-Item -Path $LocalBuildPath -ItemType Directory -Force >> $null

        if ( !$? ) {
            Write-Error "Error creating log directory $LocalBuildPath"
            LogMessage -Message ("Failed to create dir : {0}" -f $LocalBuildPath) -LogType ([LogType]::Error)
            throw
        }

        $buildFile = $global:BuildPath + "\UnifiedAgent_Builds\release\Microsoft-ASR_UA_*.exe"
        Copy-Item $buildFile -Destination $LocalBuildPath -ErrorAction stop -force
        $ret = $?
        LogMessage -Message ("Copy file {0} return value $ret" -f $buildFile) -LogType ([LogType]::Info)
        if (!$ret) {
            LogMessage -Message ("Failed to copy the file {0} to {1}" -f $buildFile, $LocalBuildPath) -LogType ([LogType]::Error)
            throw
        }
        else {
            LogMessage -Message ("Successfully copied the file {0} to {1}" -f $buildFile, $LocalBuildPath) -LogType ([LogType]::Info)
        }

        $localBuild = $LocalBuildPath + "\Microsoft-ASR_UA_*.exe"
        $cmd = "rename  $localBuild $global:LocalBuildName"
        cmd.exe /c $cmd
        if ( !$?) {
            LogMessage -Message ("Failed to rename {0} to {1}" -f $localBuild, $global:LocalBuildName) -LogType ([LogType]::Error)
            throw
        }
        else {
            LogMessage -Message ("Successfully renamed {0} to {1}" -f $localBuild, $global:LocalBuildName) -LogType ([LogType]::Info)
        }
    }
    catch {
        $global:DeploymentStatus = [DeploymentStatus]::FAILED_COPYBUILDS
        throw
    }
}

<#
.SYNOPSIS
    Copies test binaries to the test machine
#>
Function CopyTestBinaries() {
    try {
        $CVTBinPath = $global:BuildPath + "\release\Test"
        if (!(Test-Path $CVTBinPath)) {
            LogMessage -Message ("CVT binaries path {0} doesn't exist" -f $BuildLocation) -LogType ([LogType]::Error)
            throw
        }

        if ( $(Test-Path -Path $LocalCVTBinPath -PathType Container) ) {
            LogMessage -Message ("Removing the dir : {0}" -f $LocalCVTBinPath) -LogType ([LogType]::Info)
            Remove-Item $LocalCVTBinPath -Recurse -Force
        }

        LogMessage -Message ("Creating the dir : {0}" -f $LocalCVTBinPath) -LogType ([LogType]::Info)
        New-Item -Path $LocalCVTBinPath -ItemType Directory -Force >> $null
        if ( !$? ) {
            Write-Error "Error creating log directory $LocalCVTBinPath"
            LogMessage -Message ("Failed to create cvt binaries dir : {0}" -f $LocalCVTBinPath) -LogType ([LogType]::Error)
            throw
        }

        Get-ChildItem -Path "$CVTBinPath\*" -Include *.exe, *.dll, *.ps1 -Recurse | Copy-Item -Destination "$LocalCVTBinPath"
        $ret = $?
        if (!$ret) {
            LogMessage -Message ("Failed to copy the files from {0} to {1} with exit code : {2}" -f $CVTBinPath, $LocalBuildPath, $ret) -LogType ([LogType]::Error)
            throw
        }
        else {
            LogMessage -Message ("Successfully copied the files from {0} to {1}" -f $CVTBinPath, $LocalCVTBinPath) -LogType ([LogType]::Info)
        }
    }
    catch {
        $global:DeploymentStatus = [DeploymentStatus]::FAILED_COPYCVTBINARIES
        throw
    }
}

<#
.SYNOPSIS
    Installs mobility agent
#>
Function InstallAgent {
    try {
        $buildFilePath = $LocalBuildPath + '\' + $global:LocalBuildName
        $cmd = $buildFilePath + ' /q /x:' + $LocalBuildPath
        cmd.exe /c $cmd
        if (!$?) {
            LogMessage -Message ("Failed to extract {0} to {1}. CMD : {2}" -f $buildFilePath, $LocalBuildPath, $cmd) -LogType ([LogType]::Error)
            throw
        }
        else {
            LogMessage -Message ("Successfully extracted {0} to {1}" -f $buildFilePath, $LocalBuildPath) -LogType ([LogType]::Info)
        }

        Start-Sleep -s 20

        if ($Platform.Equals("Azure", 5)) {
            $installCmd = "$LocalBuildPath\UNIFIEDAGENT.EXE /Role MS /Silent"
        }
        else {
            $installCmd = "$LocalBuildPath\UnifiedAgentInstaller.exe /Role MS /Platform VmWare /Silent"
        }

        [ScriptBlock] $InstallBlock = {
            if ($Platform.Equals("Azure", 5)) {
                $args = "/Role MS /Silent"
                $installExe = "$LocalBuildPath\UNIFIEDAGENT.EXE"
            }
            else {
                $args = "/Role MS /Platform VmWare /Silent"
                $installExe = "$LocalBuildPath\UnifiedAgentInstaller.exe"
            }

            $process = (Start-Process -Wait -Passthru -FilePath $installExe -ArgumentList $args)
            return $process.ExitCode;
        }

        $ret = (Invoke-Command -ScriptBlock $InstallBlock)

        Switch -regex ($ret) {
            "0|1641" {
                LogMessage -Message ("Successfully installed mobility agent with exit code : {0}" -f $ret) -LogType ([LogType]::Info)
            }Default {
                LogMessage -Message ("Failed to install mobility agent with exit code : {0}. CMD : {1}" -f $ret, $installCmd) -LogType ([LogType]::Error)
                throw
            }
        }
    }
    catch {
        $global:DeploymentStatus = [DeploymentStatus]::FAILED_INSTALL
        throw
    }
}

<#
.SYNOPSIS
    Uninstalls mobility agent
#>
Function UninstallAgent {
    try {
        New-Item -Type Directory -Path "$Env:SystemDrive\ProgramData\ASRSetupLogs\" -ErrorAction SilentlyContinue
        $id = Get-WmiObject -Class Win32_Product | Where-Object -FilterScript { $_.Name -eq "Microsoft Azure Site Recovery Mobility Service/Master Target Server" } | select-object -expand IdentifyingNumber

        if ($id) {
            LogMessage -Message ("Previous ASRUnifiedAgent Identifier Number : {0}" -f $id) -LogType ([LogType]::Info)

            [ScriptBlock] $UninstallBlock = {
                $uninstallLogFile = $Env:SystemDrive + "\ProgramData\ASRSetupLogs\UnifiedAgentMSIUninstall.log"
                $process = (Start-Process -Wait -Passthru -FilePath "MsiExec.exe" -ArgumentList "/qn /x $id MSIRMSHUTDOWN=2 /L+*V $uninstallLogFile")
                return $process.ExitCode;
            }

            $returnCode = (Invoke-Command -ScriptBlock $UninstallBlock)

            Switch -regex ($returnCode) {
                "0|3010|1605" {
                    LogMessage -Message ("Successfully uninstalled mobility agent with error code : {0}" -f $returnCode) -LogType ([LogType]::Info)
                }Default {
                    LogMessage -Message ("Failed to uninstall mobility agent with error code: {0}" -f $returnCode) -LogType ([LogType]::Error)
                    throw
                }
            }

            Start-Sleep -Seconds 60
        }
        else {
            LogMessage -Message ("Mobility Agent is not installed") -LogType ([LogType]::Info)
        }
    }
    catch {
        $global:DeploymentStatus = [DeploymentStatus]::FAILED_UNINSTALL
        throw
    }
}

<#
.SYNOPSIS
    Write messages to log file
#>
Function LogMessage {
    param(
        [string] $Message,
        [LogType] $LogType = [LogType]::Info,
        [bool] $WriteToLogFile = $true
    )

    if ($LogType -eq [LogType]::Error) {
        $logLevel = "ERROR"
        $color = "Red"
    }
    elseif ($LogType -eq [LogType]::Info) {
        $logLevel = "INFO"
        $color = "Green"
    }
    elseif ($LogType -eq [LogType]::Warning) {
        $logLevel = "WARNING"
        $color = "Yellow"
    }
    else {
        $logLevel = "Unknown"
        $color = "Magenta"
    }

    $format = "{0,-7} : {1,-8} : {2}"
    $dateTime = Get-Date -Format "MM-dd-yyyy HH.mm.ss"
    $msg = $format -f $logLevel, $dateTime, $Message
    Write-Host $msg -ForegroundColor $color

    if ($WriteToLogFile) {
        $message = $dateTime + ' : ' + $logLevel + ' : ' + $Message
        Write-output $message | Out-file $global:LogName -Append
    }
}

<#
.SYNOPSIS
    Create Log directory, if not exists.
#>
Function CreateLogDir() {
    <#
    if ( $(Test-Path -Path $global:LogDir -PathType Container)) {
        Write-Host "Removing the directory $global:LogDir"
        Remove-Item $global:LogDir -Recurse -Force
    }
#>
    Write-Host "Creating the directory $global:LogDir"
    New-Item -Path $global:LogDir -ItemType Directory -Force -ErrorAction SilentlyContinue >> $null
}

<#
.SYNOPSIS
    Login to shared path and mount it to Z:
#>
Function LoginToBuildPath() {
    try {
        LoginToAzureSubscription
        $FareastPass = GetSecretFromKeyVault -SecretName "IDCDLSLB"
        & NET USE Z: /D
        & NET USE Z: '\\INMSTAGINGSVR.corp.microsoft.com\DailyBuilds\Daily_Builds' /user:$FareastUser $FareastPass
        LogMessage -Message ("Login to build path succeeded.") -LogType ([LogType]::Info)
    }
    catch {
        LogMessage -Message ("Failed to access the shared path \\INMSTAGINGSVR.corp.microsoft.com") -LogType ([LogType]::Error)
        return $false
    }
    return $true
}

<#
.SYNOPSIS
Main Block
#>
Function Main() {
    try {
        CreateLogDir

        if ($InstallAgent -Or $CopyBinaries -Or $CopyBinariesCloudTest -Or $InstallAgentOnpremisesCloudTest) {
            LoginToBuildPath
            GetBuildPath
        }

        if ($InstallAgent) {
            CopyTestBinaries
            CopyBuild
            InstallAgent
        }
        elseif ($UnInstallAgent) {
            UninstallAgent
        }
        elseif ($CopyBinaries) {
            CopyTestBinaries
        }
        elseif ($CopyBinariesCloudTest) {
            CopyBuild
        }
        elseif ($InstallAgentOnpremisesCloudTest) {
            CopyBuild
            InstallAgent
        } elseif ($InstallAgentAzureCloudTest) {
            InstallAgent
        }
        else {
            LogMessage -Message ("One of the InstallAgent/UninstallAgent/CopyTestBinaries options should be true") -LogType ([LogType]::Error)
            throw
        }
    }
    catch {
        if ($global:DeploymentStatus -eq [DeploymentStatus]::PASSED) {
            $global:DeploymentStatus = [DeploymentStatus]::FAILED_UNKNOWN_ERROR
        }
        LogMessage -Message ("ERROR:: {0}" -f $Error) -LogType ([LogType]::Error)
        LogMessage -Message ("CVT deployment failed with error : {0}" -f $global:DeploymentStatus) -LogType ([LogType]::Error)
    }

    LogMessage -Message ("Exit Code : {0}" -f [int]$global:DeploymentStatus)
    LogMessage -Message ("CVT Deployment Status : {0}" -f $global:DeploymentStatus) -LogType ([LogType]::Info)

    exit [int]$global:DeploymentStatus
}

Main
