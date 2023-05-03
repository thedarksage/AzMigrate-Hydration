# Script to build CustomActions DLL.
Param(
[Parameter(Mandatory=$true)]
    [String]$MsBuildExePath,
[Parameter(Mandatory=$true)]
    [String]$Project,
[Parameter(Mandatory=$true)]
    [String]$Platform,
[Parameter(Mandatory=$true)]
    [String]$LogsFolder
)

try
{
    # Set ProcessStartInfo attributes.
    $pinfo = New-Object System.Diagnostics.ProcessStartInfo
    $pinfo.RedirectStandardError = $true
    $pinfo.RedirectStandardOutput = $true
    $pinfo.UseShellExecute = $false

    $VerboseLogPath = $LogsFolder + "CustomActionsInitial.log"
    $ErrorLogPath = $LogsFolder + "CustomActionsInitial.err"
    $Args = $Project + " /p:Configuration=release /p:Platform=" + $Platform + " /p:Phase=Initial -t:build /v:n /nr:false /flp1:Verbosity=n;LogFile=" + $VerboseLogPath + `
            ";Encoding=UTF-8 /flp2:logfile=" + $ErrorLogPath + ";errorsonly"
    Write-Host ("Args:", $Args)

    Write-Host "Building CustomActions DLL..."
    $pinfo.FileName = $MsBuildExePath
    $pinfo.Arguments = $Args
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $pinfo
    $p.Start() | Out-Null
    $p.WaitForExit()
    Write-Host "Built CustomActions DLL"
    exit 0
} 
catch
{
    Write-Host "Hit below excecption."
    Write-Host $_.Exception | Format-List 
    exit 1
}