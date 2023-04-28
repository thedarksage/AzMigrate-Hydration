# Start - Newtonsoft Json - Pick the latest DLL present (Assembly binding redirection)

$AssemblyResolveEvent = [System.ResolveEventHandler] {
    param($senderObj, $eventArgs)

    foreach ($currAssembly in [System.AppDomain]::CurrentDomain.GetAssemblies())
    {
        if ($currAssembly.FullName -eq $eventArgs.Name)
        {
            return $currAssembly
        }
    }

    # Assembly redirecting via code
    if (([System.Reflection.AssemblyName]($eventArgs.Name)).Name -eq "Newtonsoft.Json")
    {
        $LargestAssembly = $null
        $LargestAssemblyVersion = $null

        foreach ($currAssembly in [System.AppDomain]::CurrentDomain.GetAssemblies())
        {
            $currAssemblyName = [System.Reflection.AssemblyName]($currAssembly.FullName)

            if ($currAssemblyName.Name -eq "Newtonsoft.Json" -and $currAssemblyName.Version -gt $LargestAssemblyVersion)
            {
                $LargestAssembly = $currAssembly
                $LargestAssemblyVersion = $currAssemblyName.Version
            }
        }

        return $LargestAssembly
    }

    return $null
}

[System.AppDomain]::CurrentDomain.add_AssemblyResolve($AssemblyResolveEvent)

# End - Newtonsoft Json - Pick the latest DLL present (Assembly binding redirection)

# Start - Load necessary DLLs to invoke the Process Server library

function script:Load-DLLs
{
    param(
        [Parameter(Mandatory=$true)]
        [string]$BasePath,
        [Parameter(Mandatory=$true)]
        [string[]]$DllNames
    )

    foreach ($currDllName in $DllNames) {
        $assembly = [System.Reflection.Assembly]::LoadFile((Join-Path $BasePath $currDllName))
        if ($assembly -eq $null)
        {
            throw "Failed to read assembly from path: $currDllPath"
        }
    }
}

$script:RefDllNames = @(
    "Microsoft.Azure.SiteRecovery.ProcessServer.Core.dll"
)

$script:DepDllNames = @(
    "Newtonsoft.Json.dll",
    "System.ValueTuple.dll"
)

Load-DLLs -BasePath $PSScriptRoot -DllNames $RefDllNames
Load-DLLs -BasePath (Join-Path $PSScriptRoot ProcessServer) -DllNames $DepDllNames

# End - Load necessary DLLs to invoke the Process Server library

function Report-Statistics
{
    param(
        [Parameter(Mandatory=$true)]
        [long]$ProcessorQueueLength,
        [Parameter(Mandatory=$true)]
        [Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]$ProcessorQueueLengthStatus,
        [Parameter(Mandatory=$true)]
        [decimal]$CpuUtilizationPercentage,
        [Parameter(Mandatory=$true)]
        [Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]$CpuUtilizationStatus,
        [Parameter(Mandatory=$true)]
        [long]$MemoryTotal,
        [Parameter(Mandatory=$true)]
        [long]$MemoryUsed,
        [Parameter(Mandatory=$true)]
        [Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]$MemoryStatus,
        [Parameter(Mandatory=$true)]
        [long]$InstallVolTotal,
        [Parameter(Mandatory=$true)]
        [long]$InstallVolUsed,
        [Parameter(Mandatory=$true)]
        [Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]$InstallVolStatus,
        [Parameter(Mandatory=$true)]
        [long]$ThroughputUploadPendingData,
        [Parameter(Mandatory=$true)]
        [long]$ThroughputBytesPerSec,
        [Parameter(Mandatory=$true)]
        [Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]$ThroughputStatus,
        [Parameter(Mandatory=$true)]
        [HashTable]$Services
    )

    # Check the following link for Contracts, HTTP request and response for reporting PS statistics with CS:
    # https://microsoft.sharepoint.com/teams/BackupASRMigratePortalTeam/_layouts/OneNote.aspx?id=%2Fteams%2FBackupASRMigratePortalTeam%2FShared%20Documents%2FASR%20Portal%2FASR%20Portal&wd=target%28Design.one%7CFB6E09DE-7CB5-466F-906D-6BA6792951F3%2FV2A%3A%20PS%20Statics%28Configuration%20Server%20Changes%5C%29%7C89DF747F-9389-4B74-9319-0A915F098240%2F%29

    $stats = New-Object "Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.ProcessServerStatistics"

    $stats.SystemLoad = New-Object `
        "System.ValueTuple[long, Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]" `
        $ProcessorQueueLength,`
        ([Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]$ProcessorQueueLengthStatus)

    $stats.CpuLoad = New-Object `
        "System.ValueTuple[decimal, Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]" `
        $CpuUtilizationPercentage,`
        ([Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]$CpuUtilizationStatus)

    $stats.MemoryUsage = New-Object `
        "System.ValueTuple[long, long, Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]" `
        $MemoryTotal,$MemoryUsed,`
        ([Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]$MemoryStatus)

    $stats.InstallVolumeSpace = New-Object `
        "System.ValueTuple[long, long, Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]" `
        $InstallVolTotal,$InstallVolUsed,`
        ([Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]$InstallVolStatus)

    $stats.Throughput = New-Object `
        "System.ValueTuple[long, long, Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]" `
        $ThroughputUploadPendingData,$ThroughputBytesPerSec,`
        ([Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.StatisticStatus]$ThroughputStatus)

    $stats.Services = New-Object "System.Collections.Generic.List[System.ValueTuple[string,int]]"

    foreach ($currSvc in $Services.Keys)
    {
        $stats.Services.Add((New-Object "System.ValueTuple[string,int]" $currSvc,$Services[$currSvc]))
    }

    $stubs = [Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.CSApiFactory]::GetProcessServerCSApiStubs()

    try
    {
        $stats

        $stubs.ReportStatisticsAsync($stats).Wait()
    }
    catch [AggregateException]
    {
        Write-Error $_.Exception.InnerException.ToString()
    }
    catch [Exception]
    {
        Write-Error $_
    }
}

# NOTE:
# If this script needs to be used as library for loading the methods for testing and not for one time execution,
# 1. Comment the rest of this script
# 2. Place this script under home\svsystems\bin
# 3. LOAD the script
#    PS C:\> . $env:ProgramData\ASR\home\svsystems\bin\PSMock.ps1
# 4. Run the methods as required
#    PS C:\> Report-Statistics ...

# If needed for one time execution of certain methods or in need of scripting,
# 1. Uncomment the rest of this script
# 2. Place this script under home\svsystems\bin
# 3. Edit the invocations, add scripting or modify sample values in the script below.
# 4. RUN the script
#    PS C:\> & $env:ProgramData\ASR\home\svsystems\bin\PSMock.ps1

<#
Report-Statistics `
    -ProcessorQueueLength 40 -ProcessorQueueLengthStatus Warning `
    -CpuUtilizationPercentage 60.45 -CpuUtilizationStatus Healthy `
    -MemoryTotal 5GB -MemoryUsed 2GB -MemoryStatus Healthy `
    -InstallVolTotal 100GB -InstallVolUsed 20.65GB -InstallVolStatus Unknown `
    -ThroughputUploadPendingData 650.56MB -ThroughputBytesPerSec 0.90MB -ThroughputStatus Critical `
    -Services @{
        "ProcessServer" = 1;
        "cxprocessserver" = 0
    }
#>