#Declaring the date and time variables.
$Date = Get-Date -Format dd_MMM_yyyy
$Time = Get-Date -Format hh_mm_tt

$Config=$args[0]
$Product=$args[1]
$SourcePath=$args[2]
$Branch=$args[3]
$Signing=$args[4]
$FolderPath = "I:\Signing\"+$Branch
$Log_Name = "Copy_Unsinged_Binaries_to_Submission_Path"+"_"+$Config+"_"+$Date+"_"+$Time+".txt"
$Log_File = $FolderPath+"\"+$Date+"\"+$Product+"\"+$Log_Name

Write-Host $Config
Write-Host $Product
Write-Host $SourcePath
Write-Host $Branch
Write-Host $Signing

if ($Config -eq "Release")
{
	$Configuration2 = "ReleaseMinDependency"
	$DriverDir = "objfre"
}
if ($Config -eq "Debug")
{
	$Configuration2 = "Debug"
	$DriverDir = "objchk"
}

if ($Product -eq "UA")
{
	# Remove existing contents and create directories afresh to keep unsigned executables
	Remove-Item $FolderPath\$Date\$Product\Unsigned\$Config\* -recurse -Force
	new-Item -Path $FolderPath\$Date\$Product\Unsigned\$Config -ItemType directory

	# Remove existing contents and create directories afresh to keep signed executables
	Remove-Item $FolderPath\$Date\$Product\Signed\\$Config\* -recurse -Force
	new-Item -Path $FolderPath\$Date\$Product\Signed\\$Config -ItemType directory
	
	# Define target path for binaries and files.
	$UATargetPath = "$FolderPath\$Date\$Product\Unsigned\$Config"
	
	# Copy unmanaged executables to target path.
	Copy-Item "$SourcePath\host\Utilities\Windows\ClusUtil\$Config\ClusUtil.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\Utilities\Windows\MovingSQLDBs\$Config\MoveSQLDB.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\EsxUtil\$Config\EsxUtil.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\EsxUtilWin\$Config\EsxUtilWin.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\EvtCollForw\$Config\evtcollforw.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\cachemgr\$Config\cachemgr.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\cdpmgr\$Config\cdpmgr.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\dataprotection\$Config\dataprotection.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\DataProtectionSyncRcm\$Config\DataProtectionSyncRcm.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\s2\$Config\s2.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\volopt\$Config\volopt.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\AppConfiguratorAPITestBed\$Config\AppConfiguratorAPITestBed.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\unregagent\$Config\unregagent.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\svdcheck\$Config\svdcheck.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\inmmessage\$Config\inmmessage.dll" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\service\$Configuration2\svagents.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\drivers\Tests\win32\drvutil\$Config\drvutil.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\cdpcli\$Config\cdpcli.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\cxcli\$Config\cxcli.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ServiceCtrl\$Config\ServiceCtrl.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\UnlockDrives\$Config\UnLockDrives.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\vacp\$Config\vacp.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\vacp\X64\$Config\vacp_X64.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\cxpsclient\$Config\cxpsclient.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\config\ConfiguratorAPITestBed\$Config\ConfiguratorAPITestBed.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\AppWizard\$Config\AppWizard.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\configmergetool\$Config\configmergetool.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ScoutTuning\$Config\ScoutTuning.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\inmexec\$Config\inmexec.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\hostconfigwxcommon\$Config\hostconfigwxcommon.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\applicationagent\$Config\appservice.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\drivers\Tests\win32\GetDataPoolSize\$Config\GetDataPoolSize.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\Utilities\Windows\flush_registry\$Config\flush_registry.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\frsvc\$Configuration2\frsvc.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\cxps\$Config\cxps.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\cxpscli\$Config\cxpscli.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\InMageVssProvider\$Config\InMageVssProvider.dll" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\InMageVssProvider\x64\$Config\InMageVssProvider_X64.dll" "$UATargetPath" -passthru >> $Log_File
    Copy-Item "$SourcePath\host\drivers\Tests\win32\diskgenericdll\$Config\diskgeneric.dll" "$UATargetPath" -passthru >> $Log_File			
	Copy-Item "$SourcePath\host\AzureRecoveryUtil\$Config\AzureRecoveryUtil.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\AzureRcmCli\$Config\AzureRcmCli.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\s2-cs\$Config\s2.exe" "$UATargetPath\s2-cs.exe" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\service-cs\$Configuration2\svagents.exe" "$UATargetPath\svagents-cs.exe" -passthru >> $Log_File	
	Copy-Item "$SourcePath\host\tests\ScenarioGQL\V2A\V2AGQLTests\AddAccounts\bin\$Config\AddAccounts.exe" "$UATargetPath" -passthru >> $Log_File

	# Copy managed executables to target path.
	Copy-Item "$SourcePath\host\Utilities\Windows\ExchangeValidation\ExchangeValidation\bin\$Config\ExchangeValidation.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\Solutions\Sharepoint\spapp\$Config\spapp.exe" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\csharphttpclient\bin\$Config\httpclient.dll" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\InmCheckAzureServices\bin\$Config\InmCheckAzureServices.exe" "$UATargetPath" -passthru >> $Log_File
		
	# Copy drivers to target path regardless of configuration.
	Copy-Item $SourcePath\host\drivers\InVolFlt\windows\DiskFlt\x86\Vista"$Config"\InDskFlt.sys "$UATargetPath\InDskFlt_vista_x86.sys" -passthru >> $Log_File
	Copy-Item $SourcePath\host\drivers\InVolFlt\windows\DiskFlt\x64\Vista"$Config"\InDskFlt.sys "$UATargetPath\InDskFlt_vista_amd64.sys" -passthru >> $Log_File
	Copy-Item $SourcePath\host\drivers\InVolFlt\windows\DiskFlt\x64\Win7"$Config"\InDskFlt.sys "$UATargetPath\InDskFlt_win7_amd64.sys" -passthru >> $Log_File
	Copy-Item $SourcePath\host\drivers\InVolFlt\windows\DiskFlt\x64\Win8"$Config"\InDskFlt.sys "$UATargetPath\InDskFlt_win8_amd64.sys" -passthru >> $Log_File
	Copy-Item $SourcePath\host\drivers\InVolFlt\windows\DiskFlt\x64\Win8.1"$Config"\InDskFlt.sys "$UATargetPath\InDskFlt_win81_amd64.sys" -passthru >> $Log_File
	
	# Copy VB scripts to target path.
	Copy-Item "$SourcePath\host\vacp\ACM_SCRIPTS\ExchangeVerify.vbs" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\Solutions\Oracle\windows_vx\oraclefailover.vbs" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\InMageVssProvider\InstallScripts\InMageVssProvider_Register.vbs" "$UATargetPath" -passthru >> $Log_File
	
	# Copy power shell scripts to target path.
	Copy-Item "$SourcePath\host\setup\wintogo.ps1" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\setup\VerifyBootDiskFiltering.ps1" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\AzureRecoveryUtil\Scripts\win32\StartupScript.ps1" "$UATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\setup\InmSrcLogCollector.ps1" "$UATargetPath" -passthru >> $Log_File	
}

if ($Product -eq "ASRUA")
{
	# Remove existing contents and create directories afresh to keep unsigned executables	
	Remove-Item $FolderPath\$Date\$Product\Unsigned\$Config\* -recurse -Force	
	new-Item -Path $FolderPath\$Date\$Product\Unsigned\$Config -ItemType directory
    
	# Remove existing contents and create directories afresh to keep signed executables
	Remove-Item $FolderPath\$Date\$Product\Signed\$Config\* -recurse -Force
	new-Item -Path $FolderPath\$Date\$Product\Signed\$Config -ItemType directory
    
	# Define target path for executables and files.
	$ASRUATargetPath = "$FolderPath\$Date\$Product\Unsigned\$Config"
  
	# Copy managed executables to target path.
	Copy-Item "$SourcePath\host\ASRSetup\ASRResources\bin\$Config\ASRResources.dll" "$ASRUATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\ASRResources\bin\$Config\en\ASRResources.resources.dll" "$ASRUATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\SetupFramework\bin\$Config\ASRSetupFramework.dll" "$ASRUATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\UnifiedAgentInstaller\bin\$Config\UnifiedAgentInstaller.exe" "$ASRUATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\UnifiedAgentInstaller\bin\$Config\Interop.COMAdmin.dll" "$ASRUATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\UnifiedAgentInstaller\bin\$Config\Interop.WindowsInstaller.dll" "$ASRUATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\UnifiedAgentConfigurator\bin\$Config\UnifiedAgentConfigurator.exe" "$ASRUATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\WrapperUnifiedAgent\bin\$Config\UnifiedAgent.exe" "$ASRUATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\StorageLibrary\bin\$Config\StorageLibrary.dll" "$ASRUATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\SystemRequirementValidator\bin\$Config\SystemRequirementValidator.dll" "$ASRUATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\MobilityServiceValidator\bin\$Config\MobilityServiceValidator.exe" "$ASRUATargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\UnifiedAgentUpgrader\bin\$Config\UnifiedAgentUpgrader.exe" "$ASRUATargetPath" -passthru >> $Log_File
}

if ($Product -eq "ASRUAMSI")
{
	# Remove existing contents and create directories afresh to keep unsigned executables	
	Remove-Item $FolderPath\$Date\$Product\Unsigned\$Config\* -recurse -Force	
	new-Item -Path $FolderPath\$Date\$Product\Unsigned\$Config -ItemType directory
    
	# Remove existing contents and create directories afresh to keep signed executables
	Remove-Item $FolderPath\$Date\$Product\Signed\$Config\* -recurse -Force
	new-Item -Path $FolderPath\$Date\$Product\Signed\$Config -ItemType directory
    
	# Define target path for managed and unmanaged executables.
	$ASRUAMSITargetPath = "$FolderPath\$Date\$Product\Unsigned\$Config"
 
	# Copy unmanaged executables to target path.
	Copy-Item "$SourcePath\host\ASRSetup\UnifiedAgentMSI\x86\bin\$Config\UnifiedAgentMSI.msi" "$ASRUAMSITargetPath\UnifiedAgentMSI_x86.msi" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\UnifiedAgentMSI\x64\bin\$Config\UnifiedAgentMSI.msi" "$ASRUAMSITargetPath\UnifiedAgentMSI_x64.msi" -passthru >> $Log_File
}

if ($Product -eq "ASRSETUP")
{
	# Remove existing contents and create directories afresh to keep unsigned executables	
	Remove-Item $FolderPath\$Date\$Product\Unsigned\$Config\* -recurse -Force	
	new-Item -Path $FolderPath\$Date\$Product\Unsigned\$Config -ItemType directory

	# Remove existing contents and create directories afresh to keep signed executables	
	Remove-Item $FolderPath\$Date\$Product\Signed\$Config\* -recurse -Force	
	new-Item -Path $FolderPath\$Date\$Product\Signed\$Config -ItemType directory
	
	# Define target path for executables and files
	$ASRUSTargetPath = "$FolderPath\$Date\$Product\Unsigned\$Config"
		
	# Copy unmanaged executables to target path.
	Copy-Item "$SourcePath\host\ASRSetup\ASRResources\bin\$Config\ASRResources.dll" "$ASRUSTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\ASRResources\bin\$Config\en\ASRResources.resources.dll" "$ASRUSTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\SetupFramework\bin\$Config\ASRSetupFramework.dll" "$ASRUSTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\WrapperSetup\bin\x64\$Config\UnifiedSetup.exe" "$ASRUSTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\RemoveUnifiedSetupConfiguration\bin\x64\$Config\RemoveUnifiedSetupConfiguration.dll" "$ASRUSTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\ASRSetup\LogUploadService\bin\x64\$Config\LogUploadService.exe" "$ASRUSTargetPath" -passthru >> $Log_File
}

if ($Product -eq "PI")
{
	# Remove existing contents and create directories afresh
	Remove-Item $FolderPath\$Date\$Product\Unsigned\$Config\* -recurse -Force
	new-Item -Path $FolderPath\$Date\$Product\Unsigned\$Config -ItemType directory
	
	# Remove existing contents and create directories afresh to keep signed executables
	Remove-Item $FolderPath\$Date\$Product\Signed\$Config\* -recurse -Force
	new-Item -Path $FolderPath\$Date\$Product\Signed\$Config -ItemType directory
		
	# Define target path for executables and files.
	$PITargetPath = "$FolderPath\$Date\$Product\Unsigned\$Config"	

	# Copy managed executables to target path.
	Copy-Item "$SourcePath\host\pushInstallerCli\$Config\pushClient.exe" "$PITargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\PushInstall\CsBasedPushInstall\$Config\PushInstall.exe" "$PITargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\PushInstall\RcmBasedPushInstall\$Config\RcmBasedPushInstall.exe" "$PITargetPath" -passthru >> $Log_File
	
	# Copy managed executables to target path.
	Copy-Item "$SourcePath\host\VMwarePushInstall\bin\$Config\VMwarePushInstall.exe" "$PITargetPath" -passthru >> $Log_File
	
	Copy-Item "$SourcePath\host\VMwarePushInstall\bin\$Config\httpclient.dll" "$PITargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\VMwarePushInstall\bin\$Config\VMware.VSphere.Management.dll" "$PITargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\VMwarePushInstall\bin\$Config\VMware.Interfaces.dll" "$PITargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\VMwarePushInstall\bin\$Config\InMageAPILibrary.dll" "$PITargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\VMwarePushInstall\bin\$Config\en\VMware.Interfaces.resources.dll" "$PITargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\packages\AutoMapper.6.2.1\lib\net45\AutoMapper.dll" "$PITargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\packages\Polly-Signed.5.8.0\lib\net45\Polly.dll" "$PITargetPath" -passthru >> $Log_File
}

if ($Product -eq "CX")
{
	# Remove existing contents and create directories afresh
	Remove-Item $FolderPath\$Date\$Product\Unsigned\$Config\* -recurse -Force
	new-Item -Path $FolderPath\$Date\$Product\Unsigned\$Config -ItemType directory
	
	# Remove existing contents and create directories afresh to keep signed executables
	Remove-Item $FolderPath\$Date\$Product\Signed\$Config\* -recurse -Force
	new-Item -Path $FolderPath\$Date\$Product\Signed\$Config -ItemType directory
				
	# Define target path for managed and unmanaged executables.
	$CXTargetPath = "$FolderPath\$Date\$Product\Unsigned\$Config"
	
	# Copy unmanaged executables to target path.
	Copy-Item "$SourcePath\server\windows\tmansvc\$Config\tmansvc.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\windows\SnmpTrapAgent\$Config\SnmpTrapAgent.exe" "$CXTargetPath" -passthru >> $Log_File		
	Copy-Item "$SourcePath\server\inmtouch\$Config\inmtouch.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\CSScheduler\$Config\scheduler.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\diffdatasort\$Config\diffdatasort.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\inmmessage\$Config\inmmessage.dll" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\cxps\x64\$Config\cxps.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\cxpscli\$Config\cxpscli.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\cxpsclient\$Config\cxpsclient.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\pushInstallerCli\$Config\pushClient.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\PushInstall\CsBasedPushInstall\$Config\PushInstall.exe" "$CXTargetPath" -passthru >> $Log_File	
	Copy-Item "$SourcePath\host\PushInstall\RcmBasedPushInstall\$Config\RcmBasedPushInstall.exe" "$CXTargetPath" -passthru >> $Log_File	
	Copy-Item "$SourcePath\host\csgetfingerprint\$Config\csgetfingerprint.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\gencert\$Config\gencert.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\genpassphrase\$Config\genpassphrase.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\verifycert\$Config\verifycert.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\viewcert\$Config\viewcert.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\configtool\$Config\configtool.dll" "$CXTargetPath" -passthru >> $Log_File
	
	# Copy managed executables to target path.
	Copy-Item "$SourcePath\server\windows\CSAuthModule\bin\x64\$Config\CSAuthModule.dll" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\cspsconfigtool\bin\$Config\cspsconfigtool.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\csharphttpclient\bin\$Config\httpclient.dll" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\InMageAPILibrary\bin\InMageAPILibrary.dll" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\cspsconfigtool\bin\$Config\en-US\cspsconfigtool.resources.dll" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\windows\RenewCerts\bin\$Config\RenewCerts.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\VMwarePushInstall\bin\$Config\VMwarePushInstall.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\ProcessServerCore\bin\$Config\Microsoft.Azure.SiteRecovery.ProcessServer.Core.dll" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\windows\ProcessServer\bin\$Config\ProcessServer.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\windows\ProcessServerMonitor\bin\$Config\ProcessServerMonitor.exe" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\MarsAgent\bin\$Config\MarsAgent.exe" "$CXTargetPath" -passthru >> $Log_File
	
	# Copy Java scripts to target path.
	Copy-Item "$SourcePath\admin\web\ui\js\comfun.js" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\admin\web\ui\js\common.js" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\admin\web\ui\js\flash_detection.js" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\admin\web\ui\js\group_recovery.js" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\admin\web\ui\js\index-menu.js" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\admin\web\ui\js\nls_menu.js" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\admin\web\ui\js\pushagentstatus.js" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\admin\web\ui\js\pushagentstatusall.js" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\admin\web\ui\js\RetentionValidation.js" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\admin\web\ui\js\save_dashboard_settings.js" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\admin\web\ui\js\single_recovery.js" "$CXTargetPath" -passthru >> $Log_File
	
	# Copy VB scripts to target path.
	Copy-Item "$SourcePath\Solutions\SolutionsWizard\Scripts\p2v_esx\Agentcheck.vbs" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\Solutions\SolutionsWizard\Scripts\p2v_esx\prereqcheck.vbs" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\windows\unzip_Files.vbs" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\windows\mount.vbs" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\windows\unzip.vbs" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\windows\unzip_strawberry.vbs" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\windows\delete.vbs" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\host\setup\createlink.vbs" "$CXTargetPath" -passthru >> $Log_File
			
	# Copy power shell scripts to target path.
	Copy-Item "$SourcePath\server\windows\Site_Secure_Config.ps1" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\windows\Remove_Binding.ps1" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\windows\Uninstall_DRA.ps1" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\windows\PassphraseUpdateHealth.ps1" "$CXTargetPath" -passthru >> $Log_File	
	Copy-Item "$SourcePath\server\windows\Create_Shortcut.ps1" "$CXTargetPath" -passthru >> $Log_File
	Copy-Item "$SourcePath\server\windows\Check_Software_Installed.ps1" "$CXTargetPath" -passthru >> $Log_File	
	Copy-Item "$SourcePath\server\windows\PS_Configurator.ps1" "$CXTargetPath" -passthru >> $Log_File
}
