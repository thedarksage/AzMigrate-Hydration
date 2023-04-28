#Requires -RunAsAdministrator      # Fails the script on not running as Admin

using namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Setup

param(
    [Parameter(Mandatory=$true, ParameterSetName="1stConfigure")]
    [switch]$FirstTimeConfigure,

    [Parameter(Mandatory=$true, ParameterSetName="1stConfigure")]
    [string]$CacheLocation,
	
	[Parameter(Mandatory=$true, ParameterSetName="1stConfigure")]
	[string]$AgentRepository,

    [Parameter(Mandatory=$true, ParameterSetName="ConfigureNetwork")]
    [switch]$ConfigureNetwork,

    [Parameter(Mandatory=$false, ParameterSetName="1stConfigure")]
    [Parameter(Mandatory=$false, ParameterSetName="ConfigureNetwork")]
    [ipaddress]$ReplicationNatIpv4Address,

    [Parameter(Mandatory=$true, ParameterSetName="1stConfigure")]
    [Parameter(Mandatory=$true, ParameterSetName="ConfigureNetwork")]
    [int]$ReplicationDataPort,

    [Parameter(Mandatory=$true, ParameterSetName="1stConfigure")]
    [Parameter(Mandatory=$true, ParameterSetName="ConfigureNetwork")]
    [bool]$ConfigureFirewall,

    [Parameter(Mandatory=$true, ParameterSetName="UpdateWebProxy")]
    [switch]$UpdateWebProxy,

    [Parameter(Mandatory=$true, ParameterSetName="Register")]
    [switch]$Register,
    [Parameter(Mandatory=$true, ParameterSetName="Register")]
    [string]$RcmUri,

    [Parameter(Mandatory=$true, ParameterSetName="Status")]
    [switch]$Status,

    [Parameter(Mandatory=$true, ParameterSetName="Test")]
    [switch]$TestRcmConnection,

    [Parameter(Mandatory=$true, ParameterSetName="RegenPassphrase")]
    [switch]$PassphraseRegenerated,

    [Parameter(Mandatory=$true, ParameterSetName="Unregister")]
    [switch]$Unregister,
    
    [Parameter(Mandatory=$true, ParameterSetName="Unconfigure")]
    [switch]$Unconfigure,

    [Parameter(Mandatory=$false, ParameterSetName="Unconfigure")]
    [switch]$Force,

    [Parameter(Mandatory=$false, ParameterSetName="Register")]
    [string]$caCertThumbprint,

    [Parameter(Mandatory=$true, ParameterSetName="1stConfigure")]
    [Parameter(Mandatory=$false, ParameterSetName="ConfigureNetwork")]
    [Parameter(Mandatory=$true, ParameterSetName="UpdateWebProxy")]
    [Parameter(Mandatory=$false, ParameterSetName="Register")]
    [Parameter(Mandatory=$false, ParameterSetName="Status")]
    [Parameter(Mandatory=$false, ParameterSetName="Test")]
    [Parameter(Mandatory=$false, ParameterSetName="RegenPassphrase")]
    [Parameter(Mandatory=$false, ParameterSetName="Unregister")]
    [Parameter(Mandatory=$false, ParameterSetName="Unconfigure")]
    [string]$ApplianceJsonPath,

    [Parameter(Mandatory=$false, ParameterSetName="1stConfigure")]
    [Parameter(Mandatory=$false, ParameterSetName="ConfigureNetwork")]
    [Parameter(Mandatory=$false, ParameterSetName="UpdateWebProxy")]
    [Parameter(Mandatory=$false, ParameterSetName="Register")]
    [Parameter(Mandatory=$false, ParameterSetName="Status")]
    [Parameter(Mandatory=$false, ParameterSetName="Test")]
    [Parameter(Mandatory=$false, ParameterSetName="RegenPassphrase")]
    [Parameter(Mandatory=$false, ParameterSetName="Unregister")]
    [Parameter(Mandatory=$false, ParameterSetName="Unconfigure")]
    [string]$ClientRequestId,

    [Parameter(Mandatory=$false, ParameterSetName="1stConfigure")]
    [Parameter(Mandatory=$false, ParameterSetName="ConfigureNetwork")]
    [Parameter(Mandatory=$false, ParameterSetName="UpdateWebProxy")]
    [Parameter(Mandatory=$false, ParameterSetName="Register")]
    [Parameter(Mandatory=$false, ParameterSetName="Status")]
    [Parameter(Mandatory=$false, ParameterSetName="Test")]
    [Parameter(Mandatory=$false, ParameterSetName="RegenPassphrase")]
    [Parameter(Mandatory=$false, ParameterSetName="Unregister")]
    [Parameter(Mandatory=$false, ParameterSetName="Unconfigure")]
    [string]$ActivityId
    )

# Assembly redirection is needed for NewtonSoft.Json dll, since multiple versions are
# referred by various other DLLs. Redirection can't be done from the app.config file,
# since PowerShell.exe's config would've already been loaded. So, we programmatically
# supporting the redirection.

#https://stackoverflow.com/a/33302645/3003598
#[System.AppDomain]::CurrentDomain.SetData("APP_CONFIG_FILE", (Join-Path $PSScriptRoot "PS_Configurator.ps1.config"))

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

            if ($currAssemblyName.Name -eq ([System.Reflection.AssemblyName]($eventArgs.Name)).Name -and $currAssemblyName.Version -gt $LargestAssemblyVersion)
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

# Unhandled exception handler for any errors that happen before invoking the DLL methods.
trap
{
    # These contents are from RcmAgentErrors.xml. Not trying to retrieve the
    # output built using PSCore.dll, since the DLL may not be / could not be loaded.
    $ConfiguratorOutput = (New-Object psobject -property @{
        ErrorCode = "ConfiguratorUnexpectedError"
        Message = "The operation failed due to an internal error."
        PossibleCauses = "The operation failed due to an internal error."
        RecommendedAction = "Retry the operation. If the issue persists, contact support."
        ErrorSeverity = "Error"
        ErrorCategory = "ComponentReportedError"
        MessageParameters = @{
            ErrRecord = $_
        }
    }) | ConvertTo-Json -Compress:$false # Pretty print

    [System.Console]::Error.WriteLine($ConfiguratorOutput)

    #Write-Host "Press any key to exit... [TRAP]"
    #Read-Host
    
    [System.AppDomain]::CurrentDomain.remove_AssemblyResolve($AssemblyResolveEvent)

    exit 520300 #ConfiguratorUnexpectedError
}

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
    "ClientLibHelpers.dll",
    "CredentialStore.dll",
    "Microsoft.Identity.Client.dll",
    "Microsoft.IdentityModel.Abstractions.dll",
    "Newtonsoft.Json.dll",
    "RcmClientLib.dll",
    "RcmContract.dll",
    "System.ValueTuple.dll",
    "Microsoft.ServiceBus.dll",
    "Microsoft.ApplicationInsights.dll",
    "Microsoft.AI.ServerTelemetryChannel.dll",
    "RcmAgentCommonLib.dll"
)

function script:Load-DLLsFromInstallation
{
    $Hklm = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, [Microsoft.Win32.RegistryView]::Registry64)
    $AsrPsKeyPath = "Software\Microsoft\Azure Site Recovery Process Server"

    $Key = $Hklm.OpenSubKey($AsrPsKeyPath)
    if ($Key -ne $null)
    {
        $InstallLocation = $Key.GetValue("Install Location")
    }

    if (-not $InstallLocation)
    {
        throw "Unable to find the install location"
    }
    
    Load-DLLs -BasePath (Join-Path $InstallLocation "home\svsystems\bin") -DllNames $RefDllNames
    Load-DLLs -BasePath (Join-Path $InstallLocation "home\svsystems\bin\ProcessServer") -DllNames $DepDllNames
}

# Even though .Net 4.6.2 is installed on the machine and it uses TLS 1.2 by default,
# PowerShell rather uses SSL3. So, forcing any communication via this script to RCM
# using TLS 1.2.
[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12

if($FirstTimeConfigure)
{
    Load-DLLsFromInstallation
    $ConfiguratorOutput = [RcmConfigurator]::FirstTimeConfigure($ReplicationNatIpv4Address, $ReplicationDataPort, $ConfigureFirewall, $CacheLocation, $ApplianceJsonPath, $AgentRepository, $ClientRequestId, $ActivityId)
}
elseif($ConfigureNetwork)
{
    Load-DLLsFromInstallation
    $ConfiguratorOutput = [RcmConfigurator]::ConfigureProcessServerHttpListener($ReplicationNatIpv4Address, $ReplicationDataPort, $ConfigureFirewall, $ApplianceJsonPath, $ClientRequestId, $ActivityId)
}
elseif($UpdateWebProxy)
{
    Load-DLLsFromInstallation
    $ConfiguratorOutput = [RcmConfigurator]::UpdateWebProxy($ApplianceJsonPath, $ClientRequestId, $ActivityId)
}
elseif($PassphraseRegenerated)
{
    Load-DLLsFromInstallation
    $ConfiguratorOutput = [RcmConfigurator]::OnPassphraseRegenerated($ApplianceJsonPath, $ClientRequestId, $ActivityId)
}
elseif($Register)
{
    Load-DLLsFromInstallation
    $ConfiguratorOutput = [RcmConfigurator]::RegisterProcessServer($RcmUri, $ApplianceJsonPath, $ClientRequestId, $ActivityId, $caCertThumbprint)
}
elseif($Unregister)
{
    Load-DLLsFromInstallation
    $ConfiguratorOutput = [RcmConfigurator]::UnregisterProcessServer($ApplianceJsonPath, $ClientRequestId, $ActivityId)
}
elseif($Unconfigure)
{
    Load-DLLsFromInstallation
    $ConfiguratorOutput = [RcmConfigurator]::UnconfigureProcessServer($Force, $ApplianceJsonPath, $ClientRequestId, $ActivityId)
}
elseif($Status)
{
    Load-DLLsFromInstallation
    $ConfiguratorOutput = [RcmConfigurator]::GetProcessServerConfigurationStatus($ApplianceJsonPath, $ClientRequestId, $ActivityId)
}
elseif($TestRcmConnection)
{
    Load-DLLsFromInstallation
    $ConfiguratorOutput = [RcmConfigurator]::TestRcmConnection($ApplianceJsonPath, $ClientRequestId, $ActivityId)
}
else
{
    throw "Script internal error: Unknown switch params configuration"
}

# TODO: CSV hack is to print enums as strings instead of their int values
#$ConfiguratorOutput | ConvertTo-Csv | ConvertFrom-Csv | ConvertTo-Json -Compress:$false # Pretty print
#$ConfiguratorOutput | ConvertTo-Json -Compress:$false # Pretty print
[System.Console]::Error.WriteLine($ConfiguratorOutput)

#Write-Host "Press any key to exit... [EOF]"
#Read-Host

[System.AppDomain]::CurrentDomain.remove_AssemblyResolve($AssemblyResolveEvent)

exit $ConfiguratorOutput.ExitCode
