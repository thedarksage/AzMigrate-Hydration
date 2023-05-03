
#Global Params
$LogDirectory = $PSScriptRoot + "\AgentUploadLogs"

Function CreateLogDir() {
    if ( !$(Test-Path -Path $LogDirectory -PathType Container) ) {
        Write-Host "Creating the directory $LogDirectory"
        New-Item -Path $LogDirectory -ItemType Directory -Force >> $null

        if ( !$? ) {
            Write-Error "Error creating log directory $LogDirectory"
            throw "Error creating log directory $LogDirectory"
        }
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
        Write-output $message | out-file $global:LogName -Append
    }
}