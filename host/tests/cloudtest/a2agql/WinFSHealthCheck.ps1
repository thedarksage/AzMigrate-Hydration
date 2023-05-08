
Param(
    [Parameter(Mandatory=$True)]
    [string]
    $LogDirectory,

    [Parameter(Mandatory=$True)]
    [string]
    $LogName,

    [Parameter(Mandatory=$True)]
    [string]
    $ProviderName,

    [Parameter(Mandatory=$True)]
    [string]
    $EventId,

    [Parameter(Mandatory=$True)]
    [string]
    $StartTime,

    [Parameter(Mandatory=$True)]
    [string]
    $EndTime
)

$Error.Clear()

try {
    $events = Get-WinEvent -LogName $LogName -ErrorAction SilentlyContinue `
                | Where-Object {$_.ProviderName -eq $ProviderName -and $_.Id -eq $EventId -and $_.TimeCreated -ge $StartTime -and $_.TimeCreated -le $EndTime} `
                | Select-Object TimeCreated, ID, ProviderName, LevelDisplayName, Message

    if($null -ne $events) {
        $events
        Write-Host "Health Check Failed. Observed File System Corruption!!"
        $events | Format-Table TimeCreated,Id,LevelDisplayName,ProviderName,Message -wrap | Out-File ($LogDirectory + '\Eventlog.txt')
        exit 1
    }
    else {
        Write-Host "Health Check Succeeded. No filesystem corruption events found!!"
        "Health Check Succeeded. No filesystem corruption events found!!" | Out-File ($LogDirectory + '\EventLog.txt')
        exit 0
    }
}
catch {
    Write-Error "Issue encountered while running the file system health check on the VM."
    Write-Error "ERROR Message:  $_.Exception.Message"
    Write-Error "ERROR:: $Error | ConvertTo-json -Depth 1"
    Throw
}