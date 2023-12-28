#Powershell script to copy builds to staging server.

#Declaring the date and time variables.
$Date = Get-Date -Format dd_MMM_yyyy
$Time = Get-Date -Format hh_mm_tt

#Declaring the config and product variables and reading the values from perl module
$Config=$args[0]
$Product=$args[1]

#Declaring the variables for directory structure in InMStagingSvr and fort.
$Build_Machine="InMStagingSvr"
$Branch=$args[2]
$Branch_Dest=$args[3]
$ProdVersion=$args[4]

#Declaring the variables for directory structure and log name.
$FolderPath = "\\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date"
$Log_Name = "Copy_daily_build_"+$Date+"_"+$Time+".txt"
$Log_File = "\\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Log_Name"
$PushInstallBinariesPath = "\\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\PushInstallBinaries"
$PushInstallClientsRcmPath = "\\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\PushInstallClientsRcm"
$DataProtectionBinariesPath = "\\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\DataProtectionBinaries"
$MarsAgentBinariesRcmPath = "\\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\MarsAgentBinariesRcm"
$HostPath = "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host"

#Creating the directory structure.
new-Item -Path $FolderPath -ItemType directory
New-Item -Path $PushInstallBinariesPath -ItemType directory -Force
New-Item -Path $PushInstallClientsRcmPath -ItemType directory -Force
New-Item -Path $DataProtectionBinariesPath -ItemType directory -Force
New-Item -Path $MarsAgentBinariesRcmPath -ItemType directory -Force

#Writing the first message into the log
Write-Output "Copying to InMStagingSvr" >> $Log_File

#Creating the directory structure in InMStagingSvr
foreach ($Path in "","UnifiedAgent_Builds")
{
	foreach ($ConfigType in "release","debug")
	{
		foreach ($SubPath in "logs","symbol_tars")
		{
			New-Item -Path $FolderPath\$Path\$ConfigType -name $SubPath -ItemType directory
		}
	}
}
foreach ($ConfigType in "release","debug")
{
	New-Item -Path $FolderPath\$ConfigType -name Test -ItemType directory
	New-Item -Path $FolderPath\$ConfigType\Test -name CVT -ItemType directory
	New-Item -Path $FolderPath\$ConfigType\Test -name Symbols -ItemType directory
	New-Item -Path $FolderPath\$ConfigType\Test\Symbols -name CVT -ItemType directory
	New-Item -Path $FolderPath\$ConfigType\Test\Symbols -name DITests -ItemType directory
	New-Item -Path $FolderPath\$ConfigType\Test -name DITests -ItemType directory
    New-Item -Path $FolderPath\$ConfigType -name AzureRecoveryTools -ItemType directory
    New-Item -Path $FolderPath\$ConfigType\AzureRecoveryTools -name Symbols -ItemType directory
}

# Copying the build and symbol files to InMStagingSvr
if ($Product -eq "UCX")
{
	Copy-Item "H:\builds\Daily_Builds\$Branch\HOST\$Date\$Config\InMage_CX_9.*.zip" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\symbol_tars -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\setup\DRA" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config -recurse -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\setup\MARS" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config -recurse -passthru >> $Log_File

	# Copying the required RCM MARS binaries
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\server\MarsAgent\bin\$Config\*.exe" $MarsAgentBinariesRcmPath -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\server\MarsAgent\bin\$Config\MarsAgent.exe.config" $MarsAgentBinariesRcmPath -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\server\MarsAgent\bin\$Config\MARSAgent.pdb" $MarsAgentBinariesRcmPath -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\server\MarsAgent\bin\$Config\*.dll" $MarsAgentBinariesRcmPath -passthru >> $Log_File
	Copy-Item "\\$Build_Machine\OneOffRequests\SetMarsProxy\SetMarsProxy.exe" $MarsAgentBinariesRcmPath -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\server\MarsAgent\bin\$Config\en" $MarsAgentBinariesRcmPath -Recurse -passthru >> $Log_File
}

if ($Product -eq "CX_TP")
{
	Copy-Item "H:\builds\Daily_Builds\$Branch\HOST\$Date\$Config\InMage_CX_TP*.zip" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\symbol_tars -passthru >> $Log_File
}

if ($Product -eq "PI")
{
    # Copy Pushinstall related binaries to staging server.
    Copy-Item "$HostPath\PushInstall\RcmBasedPushInstall\$Config\RcmBasedPushInstall.exe" $PushInstallBinariesPath -passthru >> $Log_File
    Copy-Item "$HostPath\VMwarePushInstall\bin\$Config\VMware.Interfaces.dll" $PushInstallBinariesPath -passthru >> $Log_File
    Copy-Item "$HostPath\VMwarePushInstall\bin\$Config\VMware.VSphere.Management.dll" $PushInstallBinariesPath -passthru >> $Log_File
    Copy-Item "$HostPath\VMwarePushInstall\bin\$Config\VMwarePushInstall.exe" $PushInstallBinariesPath -passthru >> $Log_File
    Copy-Item "$HostPath\VMwarePushInstall\VMwarePushInstall.conf" $PushInstallBinariesPath -passthru >> $Log_File
    Copy-Item "$HostPath\VMwarePushInstall\bin\$Config\httpclient.dll" $PushInstallBinariesPath -passthru >> $Log_File
    Copy-Item "$HostPath\VMwarePushInstall\bin\$Config\InMageAPILibrary.dll" $PushInstallBinariesPath -passthru >> $Log_File
    Copy-Item "$HostPath\VMwarePushInstall\bin\$Config\Newtonsoft.Json.dll" $PushInstallBinariesPath -passthru >> $Log_File
    Copy-Item "$HostPath\VMwarePushInstall\bin\$Config\Vim25Service.dll" $PushInstallBinariesPath -passthru >> $Log_File
    Copy-Item "$HostPath\VMwarePushInstall\bin\$Config\CredentialStore.dll" $PushInstallBinariesPath -passthru >> $Log_File
	Copy-Item "$HostPath\VMwarePushInstall\bin\$Config\AutoMapper.dll" $PushInstallBinariesPath -passthru >> $Log_File
    Copy-Item "$HostPath\VMwarePushInstall\bin\$Config\en" $PushInstallBinariesPath -Recurse -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\server\windows\Unix_LE_OS_details.sh" $PushInstallBinariesPath\OS_details.sh -Recurse -passthru >> $Log_File
	Copy-Item "$HostPath\pushInstallerCli\$Config\pushClient.exe" $PushInstallClientsRcmPath\Windows_pushinstallclient_$ProdVersion.exe -passthru >> $Log_File	   
}

if ($Product -eq "ASRUA")
{
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\ASRSetup\UnifiedAgentMSI\x64\bin\Release\UnifiedAgentMSI.wixpdb" "\\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\UnifiedAgent_Builds\$Config\symbol_tars\UnifiedAgentMSI_x64.wixpdb" -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\ASRSetup\UnifiedAgentMSI\x86\bin\Release\UnifiedAgentMSI.wixpdb" "\\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\UnifiedAgent_Builds\$Config\symbol_tars\UnifiedAgentMSI_x86.wixpdb" -passthru >> $Log_File
	Copy-Item "H:\BUILDS\Daily_Builds\$Branch\HOST\$Date\$Config\Microsoft-ASR_UA_*.exe" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\UnifiedAgent_Builds\$Config -passthru >> $Log_File
	Copy-Item "H:\BUILDS\Daily_Builds\$Branch\HOST\$Date\$Config\InMage_ASRUA*.zip" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\UnifiedAgent_Builds\$Config\symbol_tars -passthru -ErrorAction Ignore >> $Log_File
	Copy-Item "H:\builds\Daily_Builds\$Branch\HOST\$Date\$Config\InMage_UA_*.zip" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\UnifiedAgent_Builds\$Config\symbol_tars -passthru >> $Log_File
	
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\tests\CVT\*.ps1" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\Test\CVT -passthru >> $Log_File

	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\tests\$Config\*.dll" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\Test\CVT -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\tests\$Config\*.exe" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\Test\CVT -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\tests\$Config\*.pdb" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\Test\Symbols\CVT -passthru >> $Log_File
	
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\tests\DITests\$Config\*.exe" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\Test\DITests -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\tests\DITests\$Config\*.dll" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\Test\DITests -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\tests\DITests\$Config\*.pdb" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\Test\Symbols\DITests -passthru >> $Log_File

	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\AzureRecoveryUtil\$Config\*.exe" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\AzureRecoveryTools -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\AzureRecoveryUtil\Scripts\win32\StartupScript.ps1" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\AzureRecoveryTools -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\AzureRecoveryUtil\$Config\*.pdb" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\AzureRecoveryTools\Symbols -passthru >> $Log_File
	& { C:\'Program Files (x86)'\7-zip\7z.exe a -tzip "\\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\release\AzureRecoveryTools\AzureRecoveryTools.zip" "\\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\release\AzureRecoveryTools\*" -mx0 -xr!"\\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\release\AzureRecoveryTools\Symbols\" -x!"\\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\release\AzureRecoveryTools\StartupScript.*" -x!"\\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\release\AzureRecoveryTools\*.zip" }
	Copy-Item "$HostPath\DataProtectionSyncRcm\$Config\DataProtectionSyncRcm.exe" $DataProtectionBinariesPath -passthru >> $Log_File
	Copy-Item "$HostPath\DataProtectionSyncRcm\$Config\DataProtectionSyncRcm.pdb" $DataProtectionBinariesPath -passthru >> $Log_File

	# Copy the GQL scripts to staging server
	if ($Branch -eq "develop")
	{
		Remove-Item \\$Build_Machine\V2AGQLShare\scripts\*.ps1 >> $Log_File
		Remove-Item \\$Build_Machine\V2AGQLShare\scripts\AddAccounts.exe >> $Log_File
		Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\tests\ScenarioGQL\V2A\*.ps1" \\$Build_Machine\V2AGQLShare\scripts >> $Log_File
		Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\tests\ScenarioGQL\V2A\V2AGQLTests\AddAccounts\bin\$Config\AddAccounts.exe" \\$Build_Machine\V2AGQLShare\scripts >> $Log_File

        // Copy A2A GQL scripts to share
        Remove-Item \\$Build_Machine\A2AGQLShare\scripts\*.xml >> $Log_File
        Remove-Item \\$Build_Machine\A2AGQLShare\scripts\*.ps1 >> $Log_File
        Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\tests\ScenarioGQL\A2A\*.ps1" \\$Build_Machine\A2AGQLShare\scripts >> $Log_File
        Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\tests\ScenarioGQL\A2A\*.xml" \\$Build_Machine\A2AGQLShare\scripts >> $Log_File
        // Below script cannot be checked into GIT due to SAS token are not allowed. This reporting will be deprecated with CloudTest migration, not fixing this.
        Copy-Item "\\$Build_Machine\A2AGQLShare\TestStatus\TestStatus.ps1" \\$Build_Machine\A2AGQLShare\scripts >> $Log_File
    }
}

if ($Product -eq "ASRSETUP")
{
	Copy-Item "H:\BUILDS\Daily_Builds\$Branch\HOST\$Date\$Config\MicrosoftAzureSiteRecoveryUnifiedSetup.exe" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config -passthru >> $Log_File
	Copy-Item "H:\BUILDS\Daily_Builds\$Branch\HOST\$Date\$Config\InMage_ASRSETUP*.zip" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\symbol_tars -passthru >> $Log_File
    
    if ($Config -eq "release")
    {
        Copy-Item "H:\BUILDS\Daily_Builds\$Branch\HOST\$Date\release\AzureRecoveryTools.exe" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\release\AzureRecoveryTools -passthru >> $Log_File
    }
}

if ($Product -eq "PSMSI")
{
	Copy-Item "H:\BUILDS\Daily_Builds\$Branch\HOST\$Date\$Config\ProcessServer.msi" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config -passthru >> $Log_File
	Copy-Item "H:\BUILDS\Daily_Builds\$Branch\HOST\$Date\$Config\InMage_PSMSI*.zip" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\symbol_tars -passthru >> $Log_File
	Copy-Item "I:\SRC\$Branch\InMage-Azure-SiteRecovery\server\windows\ProcessServerMSI\x64\bin\Release\ProcessServer.wixpdb" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\symbol_tars -passthru >> $Log_File
}

if ($Product -eq "PSGI")
{
	Copy-Item "H:\BUILDS\Daily_Builds\$Branch\HOST\$Date\DeployPS.log" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\ -passthru >> $Log_File
}

# Copying logs to InMStagingSvr 

Copy-Item "H:\builds\Daily_Builds\$Branch\HOST\$Date\$Config\logs\*" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date\$Config\logs -passthru >> $Log_File
Copy-Item "H:\builds\Daily_Builds\$Branch\HOST\$Date\*.txt" \\$Build_Machine\DailyBuilds\Daily_Builds\$Branch_Dest\HOST\$Date -passthru >> $Log_File
