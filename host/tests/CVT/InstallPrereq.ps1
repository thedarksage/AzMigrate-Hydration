# This script installs Az Powershell 6.6.0, if it is not installed

Function Install-Prereq
{
    try
    {
        Write-Host "Start - Install pre-requisites"

        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

        $version = Get-Host | Select-Object Version

        if (($version.Version.Major -le 5) -and ($version.Version.Major -ne 5 -or $version.Version.Minor -lt 1))
        {
            Write-Host ("The minimum required Powershell Version is '5.1'. Current Powershell Version is {0}" -f $version) -ForegroundColor Red
            Write-Host ("Please install the proper version and run the script again.") -ForegroundColor Cyan
            exit 1
        }

        $nugetPackageProvider = Find-PackageProvider -Name Nuget -Force -ErrorAction Stop
        if (!$nugetPackageProvider)
        {
            Throw "Unable to fetch nuget package provider."
        }


        if ($nugetPackageProvider.Version.CompareTo("2.8.5.208") -lt 0)
        {
            Write-Host ("Installing nuget package provider Version '2.8.5.208'. Current Version is {0}" -f $nugetPackageProvider.Version) -ForegroundColor Yellow
            Install-PackageProvider -Name NuGet -RequiredVersion 2.8.5.208 -Force
        }

        Import-PackageProvider NuGet -Force

        Write-Host "Verifying if Az Powershell 6.6.0 is installed or not"
        $AzModule = Get-InstalledModule -Name "Az" -RequiredVersion 6.6.0 -ErrorAction silentlycontinue
        if (!$AzModule)
        {
            Set-PSRepository -Name PSGallery -InstallationPolicy Trusted
            Write-Host "Installing Az Powershell 6.6.0"
            Install-Module -Name Az -RequiredVersion 6.6.0 -AllowClobber -Scope AllUsers -Force
            Get-Module -ListAvailable Az
            Get-InstalledModule -Name Az
        }
        else
        {
            Write-Host "Az Powershell 6.6.0 is already installed. Skipping!!"
        }

        Get-Command Connect-AzAccount
        Get-Command Get-AzRecoveryServicesAsrFabric

        Write-Host "End - Install pre-requisites"
    }
    catch
    {
        Write-Host "Install Prerequisites failed with error:  $_.Exception.Message"
        Write-Host "ERROR:: ".$Error | ConvertTo-json -Depth 1
        Throw
    }
}

Install-Prereq