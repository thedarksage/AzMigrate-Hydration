<#
.SYNOPSIS
    Powershell script to run the remote CVT tests
#>

# Input parameters
param (
  [Parameter(Mandatory = $true)]
  [String] $IPAddress,
  [Parameter(Mandatory = $true)]
  [ValidateSet('CVT19DC', 'CVT12R2', 'CVT16DC', 'WIN10PRO', 'WIN2022')]
  [String] $Job,
  [Parameter(Mandatory = $true)]
  [ValidateSet("develop", "release", "custom", IgnoreCase = $true)]
  [String] $Branch
)

Enable-PSRemoting -SkipNetworkProfileCheck -Force
Set-Item wsman:\localhost\Client\TrustedHosts -value '*' -Force
Get-Item wsman:\localhost\Client\TrustedHosts

#
# Include common library files
#
. $PSScriptRoot\CommonFunctions.ps1

# Global Variables
$global:RemotePSPath = "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe"
$global:RemoteRebootCmdPath = "RebootDITests.cmd"
$global:RemoteDICmdPath = "DataIntegrityTests.cmd"
$global:RemoteBasicMBRCmdPath = "BasicMBRVolPNPTests.cmd"
$global:RemoteDynamicPNPCmdPath = "DynamicVolPNPTests.cmd"
$global:ScriptStatus = 0

<#
.SYNOPSIS
    Remotely runs the reboot across DI on the On-premise VM
#>
Function RunRemoteRebootAcrossDI() {
  $done = $false
  $runs = 0
  
  while ( ($done -ne $true) -and ($runs -le 2)) {  
    try {
      LogMessage -Message ("RunRemoteRebootAcrossDI run number : {0}" -f $runs) -LogType ([LogType]::Info)

      $Session = New-PSSession -ComputerName $IPAddress -Credential $global:cvt_credential   

      Invoke-Command -Session $Session -command { Set-Location $env:WorkingDirectory }
      Invoke-Command -Session $Session -command { cmd /c $using:RemoteRebootCmdPath }
      $remotelastexitcode = Invoke-Command -Session $Session -command { $lastexitcode } 
      LogMessage -Message ("Return code received from the remote call {0}" -f $remotelastexitcode) -LogType ([LogType]::Info)
      
      if ($remotelastexitcode -eq 0) {
        $done = $true
      }
      elseif ($remotelastexitcode -eq 3010) {
        $runs = $runs + 1
      }
      
      if ($done -ne $true) {
        Start-Sleep -s 60
        LogMessage -Message ("Rebooting Remote VM") -LogType ([LogType]::Info)
        Invoke-Command -Session $Session -ScriptBlock { Restart-Computer -Force } 
        Start-Sleep -s 600
      }
    }
    catch {
      LogMessage -Message ("[RunRemoteRebootAcrossDI] :  run number : {0} Failed with ERROR:: {1}" -f $runs, $PSItem.Exception.Message) -LogType ([LogType]::Error)
    }
    finally {
      Remove-PSSession -Session $Session
    }
  }
}

<#
.SYNOPSIS
    Remotely runs the DataIntegrityTest on the On-premise VM
#>
Function RunRemoteDataIntegrityTests() {
  try {
    $Session = New-PSSession -ComputerName $IPAddress -Credential $global:cvt_credential

    LogMessage -Message ("[RunDataIntegrityTests]:  Starting the test" ) -LogType ([LogType]::Info)

    Invoke-Command -Session $Session -command { Set-Location $env:WorkingDirectory }
    Invoke-Command -Session $Session -command { cmd /c $using:RemoteDICmdPath }
    $remotelastexitcode = Invoke-Command -Session $Session -command { $lastexitcode } 
    LogMessage -Message ("[RunDataIntegrityTests]: Return code received from the remote call {0}" -f $remotelastexitcode) -LogType ([LogType]::Info)
    
    Start-Sleep -s 10
  }
  catch {
    LogMessage -Message ("[RunDataIntegrityTests]: Failed with ERROR:: {0}" -f $PSItem.Exception.Message) -LogType ([LogType]::Error)
  }
  Finally {
    Remove-PSSession -Session $Session
  }
}

<#
.SYNOPSIS
    Remotely runs the BasicMBRVolPNPTests on the On-premise VM
#>
Function RunRemoteBasicMBRVolPNPTests() {
  try {
    $Session = New-PSSession -ComputerName $IPAddress -Credential $global:cvt_credential

    LogMessage -Message ("[RunRemoteBasicMBRVolPNPTests]:  Starting the test" ) -LogType ([LogType]::Info)
    
    Invoke-Command -Session $Session -command { Set-Location $env:WorkingDirectory }
    Invoke-Command -Session $Session -command { cmd /c $using:RemoteBasicMBRCmdPath }
    $remotelastexitcode = Invoke-Command -Session $Session -command { $lastexitcode } 
    LogMessage -Message ("[RunRemoteBasicMBRVolPNPTests]: Return code received from the remote call {0}" -f $remotelastexitcode) -LogType ([LogType]::Info)
    
    Start-Sleep -s 10
  }
  catch {
    LogMessage -Message ("[RunRemoteBasicMBRVolPNPTests]: Failed with ERROR:: {0}" -f $PSItem.Exception.Message) -LogType ([LogType]::Error)
  }
  Finally {
    Remove-PSSession -Session $Session
  }
}

<#
.SYNOPSIS
    Remotely runs the DynamicVolPNPTests on the On-premise VM
#>
Function RunRemoteDynamicVolPNPTests() {
  try {
    $Session = New-PSSession -ComputerName $IPAddress -Credential $global:cvt_credential

    LogMessage -Message ("[RunRemoteDynamicVolPNPTests]:  Starting the test") -LogType ([LogType]::Info)

    Invoke-Command -Session $Session -command { Set-Location $env:WorkingDirectory }
    Invoke-Command -Session $Session -command { cmd /c $using:RemoteDynamicPNPCmdPath }
    $remotelastexitcode = Invoke-Command -Session $Session -command { $lastexitcode } 
    LogMessage -Message ("[RunRemoteDynamicVolPNPTests]: Return code received from the remote call {0}" -f $remotelastexitcode) -LogType ([LogType]::Info)
    
    Start-Sleep -s 10
  }
  catch {
    LogMessage -Message ("[RunRemoteDynamicVolPNPTests]: Failed with ERROR:: {0}" -f $PSItem.Exception.Message) -LogType ([LogType]::Error)
  }
  Finally {
    Remove-PSSession -Session $Session
  }
}

<#
.SYNOPSIS
    Copy the trx files to local from remote machine
#>
Function CopyResults() {
  
  [CmdletBinding()]
  Param(
    [Parameter(Mandatory = $false)]
    [int] $RetryCount = 3
  )
  
  $done = $false
  $run = 1
  
  $Session = New-PSSession -ComputerName $IPAddress -Credential $global:cvt_credential 

  while ( ($done -ne $true) -and ($run -le $RetryCount)) {
    try {
      LogMessage -Message ("Started copying test results from remote. Attempt number {0}" -f $run) -LogType ([LogType]::Info)
      
      $RemoteWorkingDir = Invoke-Command -Session $Session -command { $env:WorkingDirectory }
      $RemoteTestResultsDir = "$RemoteWorkingDir\TestResults\*"
      $LocalTestResultDir = "$global:JumpboxWorkingDir\TestResults"

      $Filters = @("*.trx")
    
      CopyFilesFromRemoteSession -Session $Session -SourceDir $RemoteTestResultsDir -DestinationDir $LocalTestResultDir -Filters $Filters
      
      $done = $true
      
    }
    catch {
      LogMessage -Message ("CopyResults run number : {0} Failed with ERROR:: {1}" -f $run, $PSItem.Exception.Message) -LogType ([LogType]::Error)
    }
    finally {
      $run = $run + 1
    }
  }
  Remove-PSSession -Session $Session
}

Function RunRemoteCVTDeployment() {
  try {
    LogMessage -Message ("[RunRemoteCVTDeployment] : Started Remote CVT Deployment") -LogType ([LogType]::Info)
    
    LogMessage -Message ("[RunRemoteCVTDeployment] : Started remote archive and cleanup of files") -LogType ([LogType]::Info)
    #Archive and Delete Older Logs 
    ClearPreviousRun
    #Copy Test Binaries to remote
    CopyBinaries
    LogMessage -Message ("[RunRemoteCVTDeployment] : Completed archive and cleanup of files") -LogType ([LogType]::Info)

    LogMessage -Message ("[RunRemoteCVTDeployment] : Started remote uninstall agent") -LogType ([LogType]::Info)
    UnInstallRemoteAgent
    LogMessage -Message ("[RunRemoteCVTDeployment] : Completed remote uninstall agent") -LogType ([LogType]::Info)

    LogMessage -Message ("[RunRemoteCVTDeployment] : Started remote install agent") -LogType ([LogType]::Info)
    InstallRemoteAgent
    LogMessage -Message ("[RunRemoteCVTDeployment] : Completed remote install agent") -LogType ([LogType]::Info)
    
    LogMessage -Message ("Completed Remote CVT Deployment") -LogType ([LogType]::Info)
  }
  catch {
    LogMessage -Message ("RunRemoteCVTDeployment deployment failed on remote machine : {0} Failed with ERROR:: {1}" -f $IPAddress, $PSItem.Exception.Message) -LogType ([LogType]::Error)
    throw "RunRemoteCVTDeployment"
  }
}

Function UnInstallRemoteAgent() {
  $Session = New-PSSession -ComputerName $IPAddress -Credential $global:cvt_credential
  try {
    $TrueFlag = "`$true"
    LogMessage -Message ("Started Remote Uninstalling Agent") -LogType ([LogType]::Info)
    
    Invoke-Command -Session $Session -command { Set-Location $env:WorkingDirectory }
    Invoke-Command -Session $Session -command { C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe .\CVTDeployment.ps1 -UnInstallAgent $using:TrueFlag }
    $remotelastexitcode = Invoke-Command -Session $Session -command { $lastexitcode } 

    if ($remotelastexitcode -ne 0) {
      throw "Remote Uninstallation failed with exitcode $remotelastexitcode" 
    }

    Start-Sleep -s 10
    LogMessage -Message ("Rebooting Remote VM") -LogType ([LogType]::Info)
    Invoke-Command -Session $Session -ScriptBlock { Restart-Computer -Force } 
    Start-Sleep -s 600
  }
  catch {
    LogMessage -Message ("[UnInstallRemoteAgent] : deployment failed on remote machine : {0} Failed with ERROR:: {1}" -f $IPAddress, $PSItem.Exception.Message) -LogType ([LogType]::Error)
    Remove-PSSession -Session $Session
    throw
  }
  finally {
    Remove-PSSession -Session $Session
  }
  Remove-PSSession -Session $Session
}

Function InstallRemoteAgent() {
  $TotalInstallAgentRetries = 3
  
  try {
    $retries = 1
    $completed = $false
    $remotelastexitcode = 0

    while($retries -le $TotalInstallAgentRetries) {
      $TrueFlag = "`$true"
      LogMessage -Message ("[InstallRemoteAgent] : Started Remote Installing Agent, retry number : {0}" -f $retries) -LogType ([LogType]::Info)

      #$BuildLocation = "Z:\9.48\HOST\30_Jan_2022"
      $BuildLocation = "Z:"
      $LocalBuildPath = "UnifiedAgentInstaller"
      #$LocalCVTBinPath = "RemoteDownload"
      $Platform = "vmware"

      $Session = New-PSSession -ComputerName $IPAddress -Credential $global:cvt_credential

      Invoke-Command -Session $Session -command { Set-Location $env:WorkingDirectory }
      Invoke-Command -Session $Session -command { C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe .\CVTDeployment.ps1 -InstallAgentOnpremisesCloudTest $using:TrueFlag -BuildLocation $using:BuildLocation -BuildType $using:Branch -LocalBuildPath $env:WorkingDirectory\$using:LocalBuildPath -LocalCVTBinPath $env:WorkingDirectory -Platform $using:Platform }
      $remotelastexitcode = Invoke-Command -Session $Session -command { $lastexitcode } 

      if($remotelastexitcode -eq 0) {
        $completed = $True
        LogMessage -Message ("[InstallRemoteAgent] : Completed Installing Agent, retry number : {0}" -f $retries) -LogType ([LogType]::Info)
      } 
      
      $retries++
    } 

    if (!$completed) {
      throw "Remote InstallRemoteAgent failed with exitcode $remotelastexitcode" 
    }
  }
  catch {
    LogMessage -Message ("[InstallRemoteAgent] : deployment failed on remote machine : {0} Failed with ERROR:: {1}" -f $IPAddress, $PSItem.Exception.Message) -LogType ([LogType]::Error)
    Remove-PSSession -Session $Session
    throw
  }
  finally {
    Remove-PSSession -Session $Session
  }
}

Function CopyBinaries {
  try {
    LogMessage -Message ("[CopyBinaries] : copying files to remote started" ) -LogType ([LogType]::Info)
    
    $Session = New-PSSession -ComputerName $IPAddress -Credential $global:cvt_credential
    
    $RemoteWorkingDir = Invoke-Command -Session $Session -command { $env:WorkingDirectory }
    $LocalSourceDir = "$global:JumpboxWorkingDir\"

    CopyFilesToRemoteSession -Session $Session -SourceDir $LocalSourceDir -DestinationDir $RemoteWorkingDir

    LogMessage -Message ("[CopyBinaries] : successfully copied files to remote LocalSourceDir : {0}, DestinationDir : {1}" -f $LocalSourceDir, $RemoteWorkingDir) -LogType ([LogType]::Info)
  }
  catch {
    Remove-PSSession -Session $Session
    throw
  }
  finally {
    Remove-PSSession -Session $Session
  }
}

Function ClearPreviousRun() {
  try {
    LogMessage -Message ("[ClearPreviousRun] : Started Remote logs/results cleanup and CVT Deployment") -LogType ([LogType]::Info)

    $Session = New-PSSession -ComputerName $IPAddress -Credential $global:cvt_credential
    
    $RemoteWorkingDir = Invoke-Command -Session $Session -command { $env:WorkingDirectory }
    $RemoteLoggingDir = Invoke-Command -Session $Session -command { $env:LoggingDirectory }
  
    CleanupRemoteDirectory -Session $Session -RemoteDir $RemoteWorkingDir
    CleanupRemoteDirectory -Session $Session -RemoteDir $RemoteLoggingDir
  }
  catch {
    Remove-PSSession -Session $Session
    throw
  }
  finally {
    Remove-PSSession -Session $Session
  }
}

Function RemoteArchiveLogs() {
  try {
    LogMessage -Message ("[RemoteArchiveLogs] : Started Remote archive logs") -LogType ([LogType]::Info)

    $Session = New-PSSession -ComputerName $IPAddress -Credential $global:cvt_credential

    $RemoteWorkingDir = Invoke-Command -Session $Session -command { $env:WorkingDirectory }
    $RemoteLoggingDir = Invoke-Command -Session $Session -command { $env:LoggingDirectory }

    Invoke-Command -Session $Session -ScriptBlock ${Function:ArchiveOlderLogs} -ArgumentList $RemoteWorkingDir, $RemoteLoggingDir, $Branch

    LogMessage -Message ("[RemoteArchiveLogs] : Completed Remote archive logs") -LogType ([LogType]::Info)
  }
  catch {
    LogMessage -Message ("[RemoteArchiveLogs] : remote archive logs failed for branch : {0} Failed with ERROR:: {1}" -f $Branch, $PSItem.Exception.Message) -LogType ([LogType]::Error)
  }
  finally {
    Remove-PSSession -Session $Session
  }
}

Function ArchiveOlderLogs() {
  [CmdletBinding()]
  param (
    [Parameter(Mandatory = $true)]
    [String]
    $RemoteWorkingDir,
    [Parameter(Mandatory = $true)]
    [String]
    $RemoteLoggingDir,
    [Parameter(Mandatory = $true)]
    [String]
    $BranchParam
  )

  $TodayDate = Get-Date -Format "MM-dd-yyyy"
  $RootDirectory = (Get-PSDrive C).Root
  $ArchivedLogsDir = $RootDirectory + "ArchivedLogs" + "\" + $BranchParam
  
  if ( !(Test-Path -Path $ArchivedLogsDir) ) {
    New-Item -Path $ArchivedLogsDir -ItemType Directory -Force -ErrorAction SilentlyContinue >> $null
  }
  
  if (Test-Path -Path $RemoteLoggingDir) {
    if ( !(Test-Path -Path $ArchivedLogsDir\$TodayDate) ) {
      New-Item -Path $ArchivedLogsDir\$TodayDate -ItemType Directory -Force -ErrorAction SilentlyContinue >> $null
    }
    Copy-Item -Path $RemoteLoggingDir -Destination "$ArchivedLogsDir\$TodayDate" -Force -Recurse
  }
  
  if (Test-Path -Path $RemoteWorkingDir\TestResults) {
    if ( !(Test-Path -Path $ArchivedLogsDir\$TodayDate) ) {
      New-Item -Path $ArchivedLogsDir\$TodayDate -ItemType Directory -Force -ErrorAction SilentlyContinue >> $null
    }
    Copy-Item -Path $RemoteWorkingDir\TestResults -Destination "$ArchivedLogsDir\$TodayDate" -Force -Recurse
  }
  
  Get-ChildItem $ArchivedLogsDir -Directory | Where-Object CreationTime -lt  (Get-Date).AddDays(-5) | Remove-Item -Force -Recurse
}

<#
.SYNOPSIS
    Creating remote credential object to ps remoting
#>
Function CreateCVTCredential() {
  LoginToAzureSubscription
  $Password = GetSecretFromKeyVault -SecretName "OnPremiseCVTMachinePassword"
  $secpasswd = ConvertTo-SecureString $Password -AsPlainText -Force
  $global:cvt_credential = New-Object System.Management.Automation.PSCredential($global:UserName, $secpasswd)
}

Function EndPSS { Get-PSSession | Remove-PSSession }

Function Main() {
  Start-Transcript -Path "$PSScriptRoot\JumpboxCVTWrapper.log" -IncludeInvocationHeader

  try {
    # Responsible to create credential object required to fetch the On-premises CVT Password
    CreateCVTCredential

    RunRemoteCVTDeployment

    # Start remote DataIntegrityTest test 
    RunRemoteDataIntegrityTests
    
    # Start remote BasicMBRVolPNP test 
    RunRemoteBasicMBRVolPNPTests
    
    # Start remote DynamicVolPNP test 
    RunRemoteDynamicVolPNPTests
    
    # Start remote RebootAcrossDI test 
    RunRemoteRebootAcrossDI

    # Copy all the results (.trx) files from On-premises CVT to local(jumpbox)
    CopyResults
  }
  catch {
    LogMessage -Message ("On-premise CVT failed on remote machine : {0} Failed with ERROR:: {1}" -f $IPAddress, $Error) -LogType ([LogType]::Error)
    $global:ScriptStatus = 1
  }
  Stop-Transcript

  #Copy logs to archive
  RemoteArchiveLogs 

  EndPSS
}

SetJumpboxEnvVariables -Job $Job -Branch $Branch
Main
exit $global:ScriptStatus