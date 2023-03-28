<#
#.SYNOPSIS
#
# This script updates the test.runsettings file used for getting/setting variables used while running CVT 
# The naming convention in this script is aligned with "create vm" script, ensure both updateconfig.ps1 and CreateNewVM.ps1 scripts are modified accordingly
# .\UpdateConfig.ps1 -TestBinRoot '[TestBinRoot]' -SubscriptionName '[SubscriptionName]' -SubscriptionId '[SubscriptionId]' -PrimaryLocation '[PrimaryLocation]' -RecoveryLocation '[RecoveryLocation]' -VaultRGLocation '[VaultRGLocation]' -OSType '[OSType]' -VMNamePrefix '[VMNamePrefix]' -OSName '[OSName]'
# Replace brackets with appropriate values
#>


Param(
    [Parameter(Mandatory=$True)]
    [string]
    $LoggingDirectory,

    [Parameter(Mandatory=$True)]
    [string]
    $WorkingDirectory,
    
    [Parameter(Mandatory=$True)]
    [string]
    $SettingsFileDirectory
)

# Include common library files
. $PSScriptRoot\CommonFunctions.ps1

$OpName = "CreateConfig"    
    
try
{    
    LogMessage -Message ("Update config Started") -LogType ([LogType]::Info)

    LogMessage -Message ("[UpdateConfig] : Entered to CopyFiles method with parameters : `
        LoggingDirectory : {0} `
        WorkingDirectory : {1} `
        SettingsFileDirectory : {2} `
        " -f $LoggingDirectory, $WorkingDirectory, $SettingsFileDirectory) -LogType ([LogType]::Info)


    $ConfigFile = $SettingsFileDirectory + "\test.runsettings"

    #sleep 120
    Write-Host "Config File: $ConfigFile"

    $xmlDoc = [System.Xml.XmlDocument](Get-Content $ConfigFile);
    
    $LoggingDirectoryNode = $xmlDoc.SelectSingleNode("//*[@name='LoggingDirectory']")
    $WorkingDirectoryNode = $xmlDoc.SelectSingleNode("//*[@name='WorkingDirectory']")
    
    $LoggingDirectoryNode.SetAttribute("value", $LoggingDirectory)
    $WorkingDirectoryNode.SetAttribute("value", $WorkingDirectory)
    
    Write-Host "Saving $ConfigFile"
    $xmlDoc.Save($ConfigFile)

    LogMessage -Message ("Update config Passed/Completed") -LogType ([LogType]::Info)
}
catch
{
    LogMessage -Message ('Exception caught - {0}' -f $_.Exception.Message) -LogType ([LogType]::Error)
    LogMessage -Message ('ERROR : @line : {0}' -f $_.InvocationInfo.ScriptLineNumber) -LogType ([LogType]::Error)
    LogMessage -Message ('Failed item - {0}' -f $_.Exception.ItemName) -LogType ([LogType]::Error)

    $ErrorMessage = ('{0} operation failed' -f $OpName)

    LogMessage -Message $ErrorMessage -LogType ([LogType]::Error)
    
    throw $ErrorMessage
}