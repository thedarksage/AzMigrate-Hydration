#
# AAD App identity to login to Azure subscription
#
#
$global:AgentSpnCertName = "AgentSpnCert"
$global:DrDatapPlaneAppClientId = "7c07fb08-b554-4955-8baf-13d2f772a65b"
$global:TenantId = "72f988bf-86f1-41af-91ab-2d7cd011db47"
$global:SubscriptionId = "41b6b0c9-3e3a-4701-811b-92135df8f9e3"
$global:SecretsKeyVaultName = 'asr-src-secrets-kv'
$global:UserName = "Administrator"
$global:SecretName = 'Default'
$global:credential = 'Default'
$global:LogName = 'CVTLogs.log'

$global:JumpboxWorkingDir = "C:\CVTScripts"
$global:Event = 0
$global:EventDescription = "Default"
$global:EventTaskName = "Default"
$global:EventTaskPath = "Default"

# Enum for LogType
Add-Type -TypeDefinition @"
   public enum LogType
   {
      Error,
      Info,
      Warning
   }
"@

# Enum for LogType
Add-Type -TypeDefinition @"
   public enum JobType
   {
      CVT19DC,
      CVT12R2,
      CVT16DC,
      WIN10PRO,
      WIN2022
   }
"@

<#
.SYNOPSIS
    Write messages to log file
#>
Function LogMessage {
  param(
    [string] $Message,
    [LogType] $LogType = [LogType]::Info,
    [bool] $WriteToLogFile = $false
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

Function SetJumpboxEnvVariables() {

  [CmdletBinding()]
  param (
    [Parameter(Mandatory = $true)]
    [ValidateSet('CVT19DC', 'CVT12R2', 'CVT16DC', 'WIN10PRO', 'WIN2022')]
    [String] $Job,
    [Parameter(Mandatory = $true)]
    [ValidateSet("develop", "release", "custom", IgnoreCase = $true)]
    [String] $Branch
  )

  $global:JumpboxWorkingDir = "C:\CVTScripts\" + $Branch + "\" + $Job 
  
  if ($Branch -eq "release") {  
    if ($Job -eq [JobType]::CVT19DC) {
      $global:Event = 23
      $global:EventDescription = "SS-VCVT-19DC"
      $global:EventTaskName = "SS-VCVT-19DC"
      $global:EventTaskPath = "\CVT\Release\"
    }
    elseif ($Job -eq [JobType]::CVT12R2) {
      $global:Event = 24
      $global:EventDescription = "SS-VCVT-12R2"
      $global:EventTaskName = "SS-VCVT-12R2"
      $global:EventTaskPath = "\CVT\Release\"
    }
    elseif ($Job -eq [JobType]::CVT16DC) {
      $global:Event = 25
      $global:EventDescription = "SS-VCVT-16DC"
      $global:EventTaskName = "SS-VCVT-16DC"
      $global:EventTaskPath = "\CVT\Release\"
    }
    elseif ($Job -eq [JobType]::WIN10PRO) {
      $global:Event = 26
      $global:EventDescription = "SS-VCVT-WIN10PRO"
      $global:EventTaskName = "SS-VCVT-WIN10PRO"
      $global:EventTaskPath = "\CVT\Release\"
    }
    elseif ($Job -eq [JobType]::WIN2022) {
      $global:Event = 27
      $global:EventDescription = "SS-VCVT-WIN2022"
      $global:EventTaskName = "SS-VCVT-WIN2022"
      $global:EventTaskPath = "\CVT\Release\"
    }
    else {
      throw "Invalid Job Argument"
    } 
  }
  elseif ($Branch -eq "develop") {
    if ($Job -eq [JobType]::CVT19DC) {
      $global:Event = 33
      $global:EventDescription = "SS-VCVT-19DC"
      $global:EventTaskName = "SS-VCVT-19DC"
      $global:EventTaskPath = "\CVT\Develop\"
    }
    elseif ($Job -eq [JobType]::CVT12R2) {
      $global:Event = 34
      $global:EventDescription = "SS-VCVT-12R2"
      $global:EventTaskName = "SS-VCVT-12R2"
      $global:EventTaskPath = "\CVT\Develop\"
    }
    elseif ($Job -eq [JobType]::CVT16DC) {
      $global:Event = 35
      $global:EventDescription = "SS-VCVT-16DC"
      $global:EventTaskName = "SS-VCVT-16DC"
      $global:EventTaskPath = "\CVT\Develop\"
    }
    elseif ($Job -eq [JobType]::WIN10PRO) {
      $global:Event = 36
      $global:EventDescription = "SS-VCVT-WIN10PRO"
      $global:EventTaskName = "SS-VCVT-WIN10PRO"
      $global:EventTaskPath = "\CVT\Develop\"
    }
    elseif ($Job -eq [JobType]::WIN2022) {
      $global:Event = 37
      $global:EventDescription = "SS-VCVT-WIN2022"
      $global:EventTaskName = "SS-VCVT-WIN2022"
      $global:EventTaskPath = "\CVT\Develop\"
    }
    else {
      throw "Invalid Job Argument"
    } 
  }
  elseif ($Branch -eq "custom") {
    if ($Job -eq [JobType]::CVT19DC) {
      $global:Event = 43
      $global:EventDescription = "SS-VCVT-19DC"
      $global:EventTaskName = "SS-VCVT-19DC"
      $global:EventTaskPath = "\CVT\Custom\"
    }
    elseif ($Job -eq [JobType]::CVT12R2) {
      $global:Event = 44
      $global:EventDescription = "SS-VCVT-12R2"
      $global:EventTaskName = "SS-VCVT-12R2"
      $global:EventTaskPath = "\CVT\Custom\"
    }
    elseif ($Job -eq [JobType]::CVT16DC) {
      $global:Event = 45
      $global:EventDescription = "SS-VCVT-16DC"
      $global:EventTaskName = "SS-VCVT-16DC"
      $global:EventTaskPath = "\CVT\Custom\"
    }
    elseif ($Job -eq [JobType]::WIN10PRO) {
      $global:Event = 46
      $global:EventDescription = "SS-VCVT-WIN10PRO"
      $global:EventTaskName = "SS-VCVT-WIN10PRO"
      $global:EventTaskPath = "\CVT\Custom\"
    }
    elseif ($Job -eq [JobType]::WIN2022) {
      $global:Event = 47
      $global:EventDescription = "SS-VCVT-WIN2022"
      $global:EventTaskName = "SS-VCVT-WIN2022"
      $global:EventTaskPath = "\CVT\Custom\"
    }
    else {
      throw "Invalid Job Argument"
    } 
  } 
  else {
    throw "Invalid build type"
  }
}

Function LoginToAzureSubscription { 
  try {
    LogMessage -Message ("Logging to Azure Subscription : {0}" -f $global:SubscriptionId) -LogType ([LogType]::Info)

    $retry = 1
    $sleep = 0
    while ($retry -ge 0) {
      Start-Sleep -Seconds $sleep
      $cert = Get-ChildItem -path 'cert:\LocalMachine\My' | Where-Object { $_.Subject.Contains($global:AgentSpnCertName) } | Sort-Object -Property "NotAfter" -Descending | Select-Object -First 1
      #LogMessage -Message ("Cert: {0}" -f ($cert | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
      Start-Sleep -Seconds $sleep
      $Thumbprint = $cert.ThumbPrint
      if (!$cert -or !$Thumbprint) {
        if ($retry -eq 0) {
          LogMessage -Message ("Failed to fetch the thumbprint") -LogType ([LogType]::Error)
          return $false
        }
    
        LogMessage -Message ("Failed to fetch the thumbprint..retrying") -LogType ([LogType]::Info)
        $retry = 0
        $sleep = 60
      }
      else {
        break
      }
    }

    ### Connect to the Azure Account
    LogMessage -Message ("ApplicationId: {0} and TenantId: {1}" -f $global:DrDatapPlaneAppClientId, $global:TenantId) -LogType ([LogType]::Info)
	
	[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12;
    Connect-AzAccount -ApplicationId $global:DrDatapPlaneAppClientId -Tenant $global:TenantId -CertificateThumbprint $Thumbprint  -Subscription $global:SubscriptionId
    if (!$?) {
      $ErrorMessage = ('Unable to login to Azure account using ApplicationId: {0} and TenantId: {1}' -f $global:DrDatapPlaneAppClientId, $global:TenantId)
      LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
      throw $ErrorMessage
    }

    LogMessage -Message ("Login to Azure succeeded...Selecting subscription : {0}" -f $global:SubscriptionId ) -LogType ([LogType]::Info)
    Set-AzContext -SubscriptionId $global:SubscriptionId
    if (!$?) {
      $ErrorMessage = ('Failed to select the subscription {0}' -f $global:SubscriptionId)
      LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
      throw $ErrorMessage
    }
  }
  catch {
    $ErrorMessage = ('Failed to login to Azure Subscription with error : {0}' -f $_.Exception.Message)
    LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
    throw $ErrorMessage
  }

  return $true
}

<#
.SYNOPSIS
    Fetched password of the On-poremises VM from vault
#>
Function GetSecretFromKeyVault {
  param(
    [Parameter(Mandatory = $True)]
    [string] $SecretName
  )

  try {
    LogMessage -Message ("Trying to fetch secret {0} from {1} key-vault" -f $SecretName, $global:SecretsKeyVaultName) -LogType ([LogType]::Info)
    $secretText = Get-AzKeyVaultSecret -VaultName $global:SecretsKeyVaultName -name $SecretName -AsPlainText

    if (!$? -or $null -eq $secretText) {
      LogMessage -Message ("Failed to fetch secret from keyvault") -LogType ([LogType]::Error)
      Throw "Failed to fetch secret from keyvault"
    }
  }
  catch {
    $ErrorMessage = ('Failed to fetch secret from keyvault : {0}' -f $_.Exception.Message)
    LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
    Throw $ErrorMessage
  }

  return $secretText
}

<#
.SYNOPSIS
  Cleanuo previous run remote directory and create a new directory.
#>
Function CleanupRemoteDirectory() {
  [CmdletBinding()]
  param (
    [Parameter(Mandatory = $True)]
    $Session,
    [Parameter(Mandatory = $True)]
    [string] $RemoteDir
  )

  $script = {
    param($RemoteDir)

    if ( $(Test-Path -Path $RemoteDir -PathType Container) ) {
      Write-Host ("[CleanupRemoteDirectory] : Removing the dir : {0}" -f $RemoteDir)
      Remove-Item $RemoteDir -Recurse -Force >> $null
    }

    Write-Host ("[CleanupRemoteDirectory] : Creating the dir : {0}" -f $RemoteDir) 

    New-Item -Path $RemoteDir -ItemType Directory -Force >> $null
    if ( !$? ) {
      Write-Error "[CleanupRemoteDirectory] : Error creating log directory $RemoteDir"
      Write-Error ("[CleanupRemoteDirectory] : Failed to create dir : {0}" -f $RemoteDir)
      throw
    }
  }
  
  Invoke-Command -Session $Session -ScriptBlock $script -ArgumentList $RemoteDir
  LogMessage -Message ("[CleanupRemoteDirectory] : Remote DestinationDir : {0} is created successfully." -f $RemoteDir) -LogType ([LogType]::Info)
}

Function CopyFilesToRemoteSession() {
  param(
    [Parameter(Mandatory = $True)]
    $Session,
    [Parameter(Mandatory = $True)]
    [string] $SourceDir,
    [Parameter(Mandatory = $True)]
    [string] $DestinationDir,
    [Parameter(Mandatory = $False)]
    [string[]] $Filters = @("*.*"),
    [Parameter(Mandatory = $False)]
    [string[]] $Exclude = @("UserLogs", "*.log"),
    [Parameter(Mandatory = $False)]
    [bool] $MaintainStructure = $True
  )

  try {
    LogMessage -Message ("[CopyFilesToRemoteSession] : Entered to CopyFiles method with parameters : `
                          SourceDir : {0}, `
                          DestinationDir : {1}, `
                          Filters : {2}, `
                          Exclude : {3}, `
                          " -f $SourceDir, $DestinationDir, [system.String]::Join(',', $Filters), [system.String]::Join(',', $Exclude)) -LogType ([LogType]::Info)
    
    if (!$SourceDir.EndsWith("\*")) {
      if ($SourceDir.EndsWith("\")) {
        $SourceDir += "*"
      }
      else {
        $SourceDir += "\*"
      }
    }
    
    LogMessage -Message ("[CopyFilesToRemoteSession] : Formatted SourceDir : {0}" -f $SourceDir) -LogType ([LogType]::Info)

    $script = {
      param($DestinationDir)
                        
      if (! $(Test-Path -Path $DestinationDir -PathType Container) ) {
        New-Item -Path $DestinationDir -ItemType Directory -Force >> $null
      }

      if ( !$? ) {
        Write-Error ("[CopyFilesToRemoteSession] : Failed to create dir : {0}" -f $DestinationDir)
        throw
      }
    }                     
    Invoke-Command -Session $Session -ScriptBlock $script -ArgumentList $DestinationDir

    LogMessage -Message ("[CopyFilesToRemoteSession] : stated copying files to remote from LocalSourceDir : {0}, DestinationDir : {1}" -f $SourceDir, $DestinationDir) -LogType ([LogType]::Info)
    $Items = Get-ChildItem $SourceDir -Include $Filters -Exclude $Exclude
    foreach ($item in $Items) {
      LogMessage -Message ("[CopyFilesToRemoteSession] : Copying file : {0} to Remote DestinationDir : {1}" -f $item.FullName, $DestinationDir) -LogType ([LogType]::Info)
      Copy-Item -Path $item.FullName -Destination $DestinationDir -ToSession $Session -Recurse -Container:$MaintainStructure -Force
    }
    
    $ret = Invoke-Command -Session $Session -command { $? }
    if (!$ret) {
      LogMessage -Message ("[CopyFilesToRemoteSession]: Failed to copy the files from {0} to {1} with exit code : {2}" -f $SourceDir, $DestinationDir, $ret) -LogType ([LogType]::Error)
      throw
    }
    else {
      LogMessage -Message ("[CopyFilesToRemoteSession]: Successfully copied the files from {0} to {1}" -f $SourceDir, $DestinationDir) -LogType ([LogType]::Info)
    }
  }
  catch {
    LogMessage -Message ("[CopyFilesToRemoteSession]: Failed with ERROR:: {0}" -f $PSItem.Exception.Message) -LogType ([LogType]::Error)
    throw "Remote CopyFilesToRemoteSession Failed"
  }
}

Function CopyFilesFromRemoteSession() {
  param(
    [Parameter(Mandatory = $True)]
    $Session,
    [Parameter(Mandatory = $True)]
    [string] $SourceDir,
    [Parameter(Mandatory = $True)]
    [string] $DestinationDir,
    [Parameter(Mandatory = $False)]
    [string[]] $Filters = @("*.*"),
    [Parameter(Mandatory = $False)]
    [string[]] $Exclude = @("*.log"),
    [Parameter(Mandatory = $False)]
    [bool] $MaintainStructure = $True
  )

  try {
    LogMessage -Message ("[CopyFilesFromRemoteSession] : Entered to CopyFiles method with parameters : `
                          SourceDir : {0}, `
                          DestinationDir : {1}, `
                          Filters : {2}, `
                          Excludes : {3}, `
                          " -f $SourceDir, $DestinationDir, [system.String]::Join(',', $Filters), [system.String]::Join(',', $Exclude)) -LogType ([LogType]::Info)

    if (!$SourceDir.EndsWith("\*")) {
      if ($SourceDir.EndsWith("\")) {
        $SourceDir += "*"
      }
      else {
        $SourceDir += "\*"
      }
    }
                          
    LogMessage -Message ("[CopyFilesFromRemoteSession] : Formatted SourceDir : {0}" -f $SourceDir) -LogType ([LogType]::Info)
    
    if (! $(Test-Path -Path $DestinationDir -PathType Container) ) {
      New-Item -Path $DestinationDir -ItemType Directory -Force >> $null
    }

    if ( !$? ) {
      Write-Error ("[CopyFilesFromRemoteSession] : Failed to create dir : {0}" -f $DestinationDir)
      throw
    }

    LogMessage -Message ("[CopyFilesFromRemoteSession] : stated copying files to local from remote, RemoteSourceDir: {0}, LocalDestinationDir : {1}" -f $SourceDir, $DestinationDir) -LogType ([LogType]::Info)
    $RemoteFiles = Invoke-Command -Session $Session -command { Get-ChildItem $using:SourceDir -Include $using:Filters -Recurse }
    foreach ($File in $RemoteFiles) {
      LogMessage -Message ("[CopyFilesFromRemoteSession] : Copying file : {0} to local DestinationDir : {1}" -f $File.FullName, $DestinationDir) -LogType ([LogType]::Info)
      Copy-Item -Path $File.FullName -Destination $DestinationDir -FromSession $Session -Recurse -Container:$MaintainStructure -Force
    }

    $ret = $?    
    if (!$ret) {
      LogMessage -Message ("[CopyFilesFromRemoteSession]: Failed to copy the files from {0} to {1} with exit code : {2}" -f $SourceDir, $DestinationDir, $ret) -LogType ([LogType]::Error)
      throw
    }
    else {
      LogMessage -Message ("[CopyFilesFromRemoteSession]: Successfully copied the files from {0} to {1}" -f $SourceDir, $DestinationDir) -LogType ([LogType]::Info)
    }
  }
  catch {
    LogMessage -Message ("[CopyFilesFromRemoteSession]: Failed with ERROR:: {0}" -f $PSItem.Exception.Message) -LogType ([LogType]::Error)
    throw "Remote CopyFilesFromRemoteSession Failed"
  }
}
