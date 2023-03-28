<#
.SYNOPSIS
    Powershell script to down,oad and copy the unifies agent using the jumpbox
#>

# Input parameters
param (
  [Parameter(Mandatory = $false)]
  [String] $IPAddress = $env:JB_IPAddress,
  [Parameter(Mandatory = $false)]
  [ValidateSet('CVT19DC', 'CVT12R2', 'CVT16DC', 'WIN10PRO', 'WIN2022')]
  [String] $Job = $env:Job,
  [Parameter(Mandatory = $false)]
  [ValidateSet("develop", "release", "custom", IgnoreCase = $true)]
  [String] $Branch = $env:Branch
)

Enable-PSRemoting -SkipNetworkProfileCheck -Force
Set-Item wsman:\localhost\Client\TrustedHosts -value '*' -Force
Get-Item wsman:\localhost\Client\TrustedHosts

#
# Include common library files
#
. $PSScriptRoot\CommonFunctions.ps1

$global:UserName = "azureuser"
$global:AzJumpboxWorkingDir = "F:\AzureWinCVT\" + $Branch + "\" + $Job 
$global:RemoteUnifiedAgentDir = $global:AzJumpboxWorkingDir + "\UnifiedAgentInstaller"
$global:CloudVMScriptStatus = 0

Function ClearUnifiedPrevioudBuild() {
  $Session = New-PSSession -ComputerName $IPAddress -Credential $global:jumpbox_credential  
  try {
    LogMessage -Message ("[CleanupRemoteDirectory] : Cleanup remote WorkingDirectory started" ) -LogType ([LogType]::Info)
      
    LogMessage -Message ("[CleanupRemoteDirectory] : Remote WorkingDirectory {0}" -f $global:AzJumpboxWorkingDir) -LogType ([LogType]::Info)
      
    CleanupRemoteDirectory -Session $Session -RemoteDir $global:AzJumpboxWorkingDir
      
    LogMessage -Message ("[CleanupRemoteDirectory] : Cleanup Remote WorkingDirectory {0} Succeeded" -f $global:AzJumpboxWorkingDir) -LogType ([LogType]::Info)
  }
  catch {
    LogMessage -Message ("[CleanupRemoteDirectory]: Failed with ERROR:: {0}" -f $PSItem.Exception.Message) -LogType ([LogType]::Error)
    Remove-PSSession -Session $session
    throw
  }
  Remove-PSSession -Session $session
}

Function CopyCVTScriptToRemote() {
  $Session = New-PSSession -ComputerName $IPAddress -Credential $global:jumpbox_credential
  try {
    LogMessage -Message ("[CopyScriptToRemote] : copying files to remote started" ) -LogType ([LogType]::Info)
      
    $LocalSourceDir = "$env:WorkingDirectory"
  
    $Filters = @("CVTDeployment.ps1", "CommonFunctions.ps1")

    CopyFilesToRemoteSession -Session $Session -SourceDir $LocalSourceDir -DestinationDir $global:AzJumpboxWorkingDir -Filters $Filters
      
    LogMessage -Message ("[CopyScriptToRemote] : successfully copied files to remote LocalSourceDir : {0}, DestinationDir : {1}" -f $LocalSourceDir, $global:AzJumpboxWorkingDir) -LogType ([LogType]::Info)
  }
  catch {
    Remove-PSSession -Session $session
    throw
  }
  Remove-PSSession -Session $Session
}

Function DownloadUnifiedAgent() {
  try {
    $TrueFlag = "`$true"
    LogMessage -Message ("[DownloadUnifiedAgent] : Started Remote UnifiedAgent download.") -LogType ([LogType]::Info)

    #$BuildLocation = "Z:\9.48\HOST\30_Jan_2022"
    $BuildLocation = "Z:"

    $Session = New-PSSession -ComputerName $IPAddress -Credential $global:jumpbox_credential

    Invoke-Command -Session $Session -command { Set-Location $using:AzJumpboxWorkingDir }
    Invoke-Command -Session $Session -command { C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe .\CVTDeployment.ps1 -CopyBinariesCloudTest $using:TrueFlag -BuildLocation $using:BuildLocation -BuildType $using:Branch -LocalBuildPath $using:RemoteUnifiedAgentDir }
    $remotelastexitcode = Invoke-Command -Session $Session -command { $lastexitcode } 

    if ($remotelastexitcode -ne 0) {
      throw "Remote DownloadUnifiedAgent failed with exitcode $remotelastexitcode" 
    }
  }
  catch {
    LogMessage -Message ("[DownloadUnifiedAgent] : deployment failed on remote machine : {0} Failed with ERROR:: {1}" -f $IPAddress, $PSItem.Exception.Message) -LogType ([LogType]::Error)
    Remove-PSSession -Session $Session
    throw
  }
  finally {
    Remove-PSSession -Session $Session
  }
}

Function CopyAgentFromRemote() {
  $Session = New-PSSession -ComputerName $IPAddress -Credential $global:jumpbox_credential
  try {
    LogMessage -Message ("[CopyAgentFromRemote] : Started copying unified agnet from machine {0}" -f $IPAddress) -LogType ([LogType]::Info)

    $LocalAgentDir = "$env:WorkingDirectory\UnifiedAgentInstaller"
    $Filters = @("*.exe")
    
    CopyFilesFromRemoteSession -Session $Session -SourceDir $global:RemoteUnifiedAgentDir -DestinationDir $LocalAgentDir -Filters $Filters
    
    LogMessage -Message ("[CopyAgentFromRemote] : successfully copied unified agent to local DestinationDir : {0}, from remote SourceDir : {1}" -f $LocalAgentDir, $global:RemoteUnifiedAgentDir) -LogType ([LogType]::Info)
  }
  catch {
    Remove-PSSession -Session $Session
    throw
  }
  Remove-PSSession -Session $Session
}
  
Function EndPSS { Get-PSSession | Remove-PSSession }

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

Function Main() {
  Start-Transcript -Path "$env:LoggingDirectory\AzureCloudUnifiedAgent.log" -IncludeInvocationHeader

  LogMessage -Message ("[AzureCloudUnifiedAgent.ps1] : Started with parameters : `
    IPAddress : {0}, `
    Job : {1}, `
    Branch : {2} `
    " -f $IPAddress, $Job, $Branch) -LogType ([LogType]::Info)
  
  CreateCredentialJumpbox

  try {
    ClearUnifiedPrevioudBuild
    
    CopyCVTScriptToRemote
    
    DownloadUnifiedAgent
    
    CopyAgentFromRemote
  }
  catch {
    LogMessage -Message ("[AzureCloudUnifiedAgent]: Failed with ERROR:: {0} `n StackTrace : {1}" -f $PSItem.Exception.Message, $PSItem.ScriptStackTrace) -LogType ([LogType]::Error)
    $global:CloudVMScriptStatus = 1
  }

  Stop-Transcript
  EndPSS
}

Main