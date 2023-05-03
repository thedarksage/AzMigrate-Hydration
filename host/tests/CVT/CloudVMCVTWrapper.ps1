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
  [String] $Branch,
  [Parameter(Mandatory = $true)]
  [int] $TimeoutInMins
)

Enable-PSRemoting -Force
Set-Item wsman:\localhost\Client\TrustedHosts -value '*' -Force
Get-Item wsman:\localhost\Client\TrustedHosts

#
# Include common library files
#
. $PSScriptRoot\CommonFunctions.ps1

$global:UserName = "azureuser"
$global:WaitTimeBeforeCopyInSecs = 300
$global:CloudVMScriptStatus = 0

Function CleanPreviousRun() {
  $Session = New-PSSession -ComputerName $IPAddress -Credential $global:jumpbox_credential  
  try {
    LogMessage -Message ("[CleanPreviousRun] : Cleanup remote WorkingDirectory started" ) -LogType ([LogType]::Info)
    
    #$global:JumpboxWorkingDir = Invoke-Command -Session $Session -command { $env:WorkingDirectory }
    LogMessage -Message ("[CleanPreviousRun] : Remote WorkingDirectory {0}" -f $global:JumpboxWorkingDir) -LogType ([LogType]::Info)
    
    CleanupRemoteDirectory -Session $Session -RemoteDir $global:JumpboxWorkingDir
    
    LogMessage -Message ("[CleanPreviousRun] : Cleanup Remote WorkingDirectory {0} Succeeded" -f $global:JumpboxWorkingDir) -LogType ([LogType]::Info)
  }
  catch {
    LogMessage -Message ("[CleanPreviousRun]: Failed with ERROR:: {0}" -f $PSItem.Exception.Message) -LogType ([LogType]::Error)
    Remove-PSSession -Session $session
    throw
  }
  Remove-PSSession -Session $session
}

Function CopyTestBinariesToRemote() {
  $Session = New-PSSession -ComputerName $IPAddress -Credential $global:jumpbox_credential
  try {
    LogMessage -Message ("[CopyTestBinariesToRemote] : copying files to remote started" ) -LogType ([LogType]::Info)
    
    #$global:JumpboxWorkingDir = Invoke-Command -Session $Session -command { $env:WorkingDirectory }
    $LocalSourceDir = "$env:WorkingDirectory\"

    CopyFilesToRemoteSession -Session $Session -SourceDir $LocalSourceDir -DestinationDir $global:JumpboxWorkingDir
    
    LogMessage -Message ("[CopyTestBinariesToRemote] : successfully copied files to remote LocalSourceDir : {0}, DestinationDir : {1}" -f $LocalSourceDir, $global:JumpboxWorkingDir) -LogType ([LogType]::Info)
  }
  catch {
    Remove-PSSession -Session $session
    throw
  }
  Remove-PSSession -Session $Session
}

Function TriggerEvent() {
  $Session = New-PSSession -ComputerName $IPAddress -Credential $global:jumpbox_credential
  try {
    LogMessage -Message ("[TriggerEvent] : Triggering Event for IP : {0}" -f $IPAddress) -LogType ([LogType]::Info)

    Invoke-Command -Session $Session -Command { eventcreate /l APPLICATION /so "CVTTrigger" /t INFORMATION /id $using:Event /d $using:EventDescription }

  }
  catch {
    LogMessage -Message ("[TriggerEvent]: Failed with ERROR:: {0}" -f $PSItem.Exception.Message) -LogType ([LogType]::Error)
    Remove-PSSession -Session $Session
    throw "TriggerEvent Failed"
  }
  Remove-PSSession -Session $Session
}

Function WaitEventToComplete() {
  $Session = New-PSSession -ComputerName $IPAddress -Credential $global:jumpbox_credential
  try {
    LogMessage -Message ("[WaitEventToComplete] : Checking Event Status for IP : {0}" -f $IPAddress) -LogType ([LogType]::Info)

    Invoke-Command -Session $Session -Command { eventcreate /l APPLICATION /so "CVTTrigger" /t INFORMATION /id $using:Event /d $using:EventDescription }

  }
  catch {
    LogMessage -Message ("[TriggerEvent]: Failed with ERROR:: {0}" -f $PSItem.Exception.Message) -LogType ([LogType]::Error)
    Remove-PSSession -Session $Session
    throw "TriggerEvent Failed"
  }
  Remove-PSSession -Session $Session
}

Function WaitForResults() {
  $TimeForEachRunInMin = 10
  $TimeForEachRunInSec = $TimeForEachRunInMin * 60
  $TotalRetries = $TimeoutInMins / $TimeForEachRunInMin
  
  $Session = New-PSSession -ComputerName $IPAddress -Credential $global:jumpbox_credential

  try {
    LogMessage -Message ("[WaitForResults] : Started waiting for tests to complete") -LogType ([LogType]::Info)
    $retries = 1
    $completed = $false
    do {
      LogMessage -Message ("[WaitForResults] : Waiting for the next {0} seconds.. Retry count : {1}" -f $TimeForEachRunInSec, $retries) -LogType ([LogType]::Info)
      Start-Sleep -s $TimeForEachRunInSec

      #$global:JumpboxWorkingDir = Invoke-Command -Session $Session -command { $env:WorkingDirectory }
      $RemoteTestResultsDir = "$global:JumpboxWorkingDir\TestResults"
      LogMessage -Message ("[WaitForResults] : Checking RemoteTestResultsDir  : {0}" -f $RemoteTestResultsDir) -LogType ([LogType]::Info)
      
      $script = {
        param($RemoteTestResultsDir)
        return (Test-Path -Path "$RemoteTestResultsDir")
      }
      
      $output = Invoke-Command -Session $session -ScriptBlock $script -ArgumentList $RemoteTestResultsDir

      $event_status_script = {
        param($TaskName, $TaskPath)
        $isrunning = (Get-ScheduledTask | Where TaskName -eq $TaskName | Where TaskPath -eq $TaskPath).State
        return ($isrunning -eq 'Running')
      }
      
      $iseventrunning =  Invoke-Command -Session $session -ScriptBlock $event_status_script -ArgumentList $global:EventTaskName, $global:EventTaskPath

      if ($output) {
        LogMessage -Message ("[WaitForResults] : TestResults found on remote : {0}" -f $IPAddress) -LogType ([LogType]::Info)
        $completed = $true
        break
      } 
      elseif ($iseventrunning -eq $False) {
        LogMessage -Message ("[WaitForResults] : Is Event running on remote : {0}" -f $iseventrunning) -LogType ([LogType]::Info)

        $event_error_script = {
          param($TaskName, $TaskPath)
          $event_error = (Get-ScheduledTask | Where TaskName -eq $TaskName | Where TaskPath -eq $TaskPath | Get-ScheduledTaskInfo).LastTaskResult
          return $event_error
        }

        $eventerror = Invoke-Command -Session $session -ScriptBlock $event_error_script -ArgumentList $global:EventTaskName, $global:EventTaskPath
        
        if($eventerror -ne 0) {
          LogMessage -Message ("[WaitForResults] : Event exited on remote with error : {0}" -f $eventerror) -LogType ([LogType]::Error)
          throw "Trigger Failed with error : $eventerror"
        } else {
          LogMessage -Message ("[WaitForResults] : TestResults found on remote : {0}" -f $IPAddress) -LogType ([LogType]::Info)
          $completed = $true
          break
        }
      }
      else {
        LogMessage -Message ("[WaitForResults] : TestResults not found on remote : {0}.... Waiting for results." -f $IPAddress) -LogType ([LogType]::Info)
      }
      
      $retries++;
    } while ($retries -le $TotalRetries)
  }
  catch {
    LogMessage -Message ("[WaitForResults]: Failed with ERROR:: {0}" -f $PSItem.Exception.Message) -LogType ([LogType]::Error)
    Remove-PSSession -Session $Session  
    throw
  }
  Remove-PSSession -Session $Session
  return $completed
}

Function CopyTestsResultsFromRemote() {
  $Session = New-PSSession -ComputerName $IPAddress -Credential $global:jumpbox_credential
  try {
    LogMessage -Message ("[CopyTestsResultsFromRemote] : Started copying test results file from machine {0}" -f $IPAddress) -LogType ([LogType]::Info)
    
    Start-Sleep -s $global:WaitTimeBeforeCopyInSecs

    #$global:JumpboxWorkingDir = Invoke-Command -Session $Session -command { $env:WorkingDirectory }
    $RemoteTestResultsDir = "$global:JumpboxWorkingDir\TestResults\*"
    $LocalTestResultDir = "$env:WorkingDirectory\TestResults"
    $Filters = @("*.trx")
    
    CopyFilesFromRemoteSession -Session $Session -SourceDir $RemoteTestResultsDir -DestinationDir $LocalTestResultDir -Filters $Filters
    
    LogMessage -Message ("[CopyTestsResultsFromRemote] : successfully copied files to local DestinationDir : {0}, from remote SourceDir : {1}" -f $LocalTestResultDir, $RemoteTestResultsDir) -LogType ([LogType]::Info)
  }
  catch {
    Remove-PSSession -Session $Session
    throw
  }
  Remove-PSSession -Session $Session
}

Function CopyJumpboxLogsFromRemote() {
  $Session = New-PSSession -ComputerName $IPAddress -Credential $global:jumpbox_credential
  try {
    LogMessage -Message ("[CopyJumpboxLogsFromRemote] : Started copying jumpbox logs from machine {0}" -f $IPAddress) -LogType ([LogType]::Info)
    
    $RemoteLogDir = "$global:JumpboxWorkingDir"
    $LocalLogDir = "$env:LoggingDirectory"
    $Filters = @("JumpboxCVTWrapper.log")
    
    CopyFilesFromRemoteSession -Session $Session -SourceDir $RemoteLogDir -DestinationDir $LocalLogDir -Filters $Filters
    
    LogMessage -Message ("[CopyJumpboxLogsFromRemote] : successfully copied files to local DestinationDir : {0}, from remote SourceDir : {1}" -f $LocalLogDir, $RemoteLogDir) -LogType ([LogType]::Info)
  }
  catch {
    Remove-PSSession -Session $Session
    throw
  }
  Remove-PSSession -Session $Session
}

<#
.SYNOPSIS
    Creating remote credential object to ps remoting
#>
Function CreateCredentialJumpbox() {
  LoginToAzureSubscription
  $Password = GetSecretFromKeyVault -SecretName "JumpboxCVTPassword"
  $secpasswd = ConvertTo-SecureString $Password -AsPlainText -Force
  $global:jumpbox_credential = New-Object System.Management.Automation.PSCredential($global:UserName, $secpasswd)
}

Function EndPSS { Get-PSSession | Remove-PSSession }
  
Function Main() {
  Start-Transcript -Path "$env:LoggingDirectory\CloudVMCVTWrapper.log" -IncludeInvocationHeader
  
  CreateCredentialJumpbox

  try {
    CleanPreviousRun
    
    CopyTestBinariesToRemote
    
    TriggerEvent
    
    $result = WaitForResults
    if (!$result) {
      throw "CVT Test failed for ip : $IPAddress within timelimit of CloudVMCVTWrapper.ps1"
    }

    CopyTestsResultsFromRemote
  }
  catch {
    LogMessage -Message ("[CloudVMCVTWrapper]: Failed with ERROR:: {0} `n StackTrace : {1}" -f $PSItem.Exception.Message, $PSItem.ScriptStackTrace) -LogType ([LogType]::Error)
    $global:CloudVMScriptStatus = 1
  }
  CopyJumpboxLogsFromRemote
  Stop-Transcript
  EndPSS
}

SetJumpboxEnvVariables -Job $Job -Branch $Branch
Main
    
return $global:CloudVMScriptStatus