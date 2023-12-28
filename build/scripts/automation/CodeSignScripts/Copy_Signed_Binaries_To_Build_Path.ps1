#Declaring the date and time variables.
$Date = Get-Date -Format dd_MMM_yyyy
$Time = Get-Date -Format hh_mm_tt

$Config=$args[0]
$Product=$args[1]
$SourcePath=$args[2]
$Branch=$args[3]
$Signing=$args[4]
$FolderPath = "I:\Signing\"+$Branch
$Log_Name = "Copy_Signed_Binaries_To_Build_Path"+"_"+$Config+"_"+$Date+"_"+$Time+".txt"
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
	# Define copy back path.
	$UACopyBackPath = "$FolderPath\$Date\$Product\Signed\$Config"

	# Copy unmanaged executables from copyback path to build path.
	Copy-Item "$UACopyBackPath\ClusUtil.exe" "$SourcePath\host\Utilities\Windows\ClusUtil\$Config\ClusUtil.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\MoveSQLDB.exe" "$SourcePath\host\Utilities\Windows\MovingSQLDBs\$Config\MoveSQLDB.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\EsxUtil.exe" "$SourcePath\host\EsxUtil\$Config\EsxUtil.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\EsxUtilWin.exe" "$SourcePath\host\EsxUtilWin\$Config\EsxUtilWin.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\evtcollforw.exe" "$SourcePath\host\EvtCollForw\$Config\evtcollforw.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\cachemgr.exe" "$SourcePath\host\cachemgr\$Config\cachemgr.exe"  -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\cdpmgr.exe" "$SourcePath\host\cdpmgr\$Config\cdpmgr.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\dataprotection.exe" "$SourcePath\host\dataprotection\$Config\dataprotection.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\DataProtectionSyncRcm.exe" "$SourcePath\host\DataProtectionSyncRcm\$Config\DataProtectionSyncRcm.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\s2.exe" "$SourcePath\host\s2\$Config\s2.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\volopt.exe" "$SourcePath\host\volopt\$Config\volopt.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\AppConfiguratorAPITestBed.exe" "$SourcePath\host\AppConfiguratorAPITestBed\$Config\AppConfiguratorAPITestBed.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\unregagent.exe" "$SourcePath\host\unregagent\$Config\unregagent.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\svdcheck.exe" "$SourcePath\host\svdcheck\$Config\svdcheck.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\inmmessage.dll" "$SourcePath\host\inmmessage\$Config\inmmessage.dll" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\svagents.exe" "$SourcePath\host\service\$Configuration2\svagents.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\drvutil.exe" "$SourcePath\host\drivers\Tests\win32\drvutil\$Config\drvutil.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\cdpcli.exe" "$SourcePath\host\cdpcli\$Config\cdpcli.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\cxcli.exe" "$SourcePath\host\cxcli\$Config\cxcli.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\ServiceCtrl.exe" "$SourcePath\host\ServiceCtrl\$Config\ServiceCtrl.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\UnLockDrives.exe" "$SourcePath\host\UnlockDrives\$Config\UnLockDrives.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\vacp.exe" "$SourcePath\host\vacp\$Config\vacp.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\vacp_X64.exe" "$SourcePath\host\vacp\X64\$Config\vacp_X64.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\cxpsclient.exe" "$SourcePath\host\cxpsclient\$Config\cxpsclient.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\ConfiguratorAPITestBed.exe" "$SourcePath\host\config\ConfiguratorAPITestBed\$Config\ConfiguratorAPITestBed.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\AppWizard.exe" "$SourcePath\host\AppWizard\$Config\AppWizard.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\configmergetool.exe" "$SourcePath\host\configmergetool\$Config\configmergetool.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\ScoutTuning.exe" "$SourcePath\host\ScoutTuning\$Config\ScoutTuning.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\inmexec.exe" "$SourcePath\host\inmexec\$Config\inmexec.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\hostconfigwxcommon.exe" "$SourcePath\host\hostconfigwxcommon\$Config\hostconfigwxcommon.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\appservice.exe" "$SourcePath\host\applicationagent\$Config\appservice.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\GetDataPoolSize.exe" "$SourcePath\host\drivers\Tests\win32\GetDataPoolSize\$Config\GetDataPoolSize.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\flush_registry.exe" "$SourcePath\host\Utilities\Windows\flush_registry\$Config\flush_registry.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\frsvc.exe" "$SourcePath\host\frsvc\$Configuration2\frsvc.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\cxps.exe" "$SourcePath\host\cxps\$Config\cxps.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\cxpscli.exe" "$SourcePath\host\cxpscli\$Config\cxpscli.exe" -passthru >> $Log_File	
	Copy-Item "$UACopyBackPath\InMageVssProvider.dll" "$SourcePath\host\InMageVssProvider\$Config\InMageVssProvider.dll" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\InMageVssProvider_X64.dll" "$SourcePath\host\InMageVssProvider\x64\$Config\InMageVssProvider_X64.dll" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\diskgeneric.dll" "$SourcePath\host\drivers\Tests\win32\diskgenericdll\$Config\diskgeneric.dll" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\AzureRecoveryUtil.exe" "$SourcePath\host\AzureRecoveryUtil\$Config\AzureRecoveryUtil.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\AzureRcmCli.exe" "$SourcePath\host\AzureRcmCli\$Config\AzureRcmCli.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\s2-cs.exe" "$SourcePath\host\s2-cs\$Config\s2.exe" -passthru >> $Log_File		
	Copy-Item "$UACopyBackPath\svagents-cs.exe" "$SourcePath\host\service-cs\$Configuration2\svagents.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\AddAccounts.exe" "$SourcePath\host\tests\ScenarioGQL\V2A\V2AGQLTests\AddAccounts\bin\$Config\AddAccounts.exe" -passthru >> $Log_File
	
	# Copy managed executables from copyback path to build path.
	Copy-Item "$UACopyBackPath\ExchangeValidation.exe" "$SourcePath\host\Utilities\Windows\ExchangeValidation\ExchangeValidation\bin\$Config\ExchangeValidation.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\spapp.exe" "$SourcePath\Solutions\Sharepoint\spapp\$Config\spapp.exe" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\httpclient.dll" "$SourcePath\host\csharphttpclient\bin\$Config\httpclient.dll" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\InmCheckAzureServices.exe" "$SourcePath\host\InmCheckAzureServices\bin\$Config\InmCheckAzureServices.exe"  -passthru >> $Log_File
		
	# Copy drivers executables from copyback path to build path.
	Copy-Item "$UACopyBackPath\InDskFlt_vista_x86.sys" $SourcePath\host\drivers\InVolFlt\windows\DiskFlt\x86\Vista"$Config"\InDskFlt.sys  -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\InDskFlt_vista_amd64.sys" $SourcePath\host\drivers\InVolFlt\windows\DiskFlt\x64\Vista"$Config"\InDskFlt.sys  -passthru >> $Log_File	
	Copy-Item "$UACopyBackPath\InDskFlt_win7_amd64.sys" $SourcePath\host\drivers\InVolFlt\windows\DiskFlt\x64\Win7"$Config"\InDskFlt.sys -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\InDskFlt_win8_amd64.sys" $SourcePath\host\drivers\InVolFlt\windows\DiskFlt\x64\Win8"$Config"\InDskFlt.sys -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\InDskFlt_win81_amd64.sys" $SourcePath\host\drivers\InVolFlt\windows\DiskFlt\x64\Win8.1"$Config"\InDskFlt.sys -passthru >> $Log_File	
	
	# Copy VB scripts from copyback path to build path.
	Copy-Item "$UACopyBackPath\ExchangeVerify.vbs" "$SourcePath\host\vacp\ACM_SCRIPTS\ExchangeVerify.vbs" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\oraclefailover.vbs" "$SourcePath\Solutions\Oracle\windows_vx\oraclefailover.vbs" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\InMageVssProvider_Register.vbs" "$SourcePath\host\InMageVssProvider\InstallScripts\InMageVssProvider_Register.vbs" -passthru >> $Log_File
	
	# Copy power shell scripts from copyback path to build path.
	Copy-Item "$UACopyBackPath\wintogo.ps1" "$SourcePath\host\setup\wintogo.ps1" -passthru >> $Log_File	
	Copy-Item "$UACopyBackPath\VerifyBootDiskFiltering.ps1" "$SourcePath\host\setup\VerifyBootDiskFiltering.ps1" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\StartupScript.ps1" "$SourcePath\host\AzureRecoveryUtil\Scripts\win32\StartupScript.ps1" -passthru >> $Log_File
	Copy-Item "$UACopyBackPath\InmSrcLogCollector.ps1" "$SourcePath\host\setup\InmSrcLogCollector.ps1" -passthru >> $Log_File
}

if ($Product -eq "ASRUA")
{
	# Define copy back path.
	$ASRUACopyBackPath = "$FolderPath\$Date\$Product\Signed\$Config"
	
	Copy-Item "$ASRUACopyBackPath\ASRResources.dll" "$SourcePath\host\ASRSetup\ASRResources\bin\$Config\ASRResources.dll" -passthru >> $Log_File
	Copy-Item "$ASRUACopyBackPath\ASRResources.resources.dll" "$SourcePath\host\ASRSetup\ASRResources\bin\$Config\en\ASRResources.resources.dll" -passthru >> $Log_File
	Copy-Item "$ASRUACopyBackPath\ASRSetupFramework.dll" "$SourcePath\host\ASRSetup\SetupFramework\bin\$Config\ASRSetupFramework.dll" -passthru >> $Log_File
	Copy-Item "$ASRUACopyBackPath\UnifiedAgentInstaller.exe" "$SourcePath\host\ASRSetup\UnifiedAgentInstaller\bin\$Config\UnifiedAgentInstaller.exe" -passthru >> $Log_File
	Copy-Item "$ASRUACopyBackPath\Interop.COMAdmin.dll" "$SourcePath\host\ASRSetup\UnifiedAgentInstaller\bin\$Config\Interop.COMAdmin.dll" -passthru >> $Log_File
	Copy-Item "$ASRUACopyBackPath\Interop.WindowsInstaller.dll" "$SourcePath\host\ASRSetup\UnifiedAgentInstaller\bin\$Config\Interop.WindowsInstaller.dll" -passthru >> $Log_File
	Copy-Item "$ASRUACopyBackPath\UnifiedAgentConfigurator.exe" "$SourcePath\host\ASRSetup\UnifiedAgentConfigurator\bin\$Config\UnifiedAgentConfigurator.exe" -passthru >> $Log_File
	Copy-Item "$ASRUACopyBackPath\UnifiedAgent.exe" "$SourcePath\host\ASRSetup\WrapperUnifiedAgent\bin\$Config\UnifiedAgent.exe" -passthru >> $Log_File
	Copy-Item "$ASRUACopyBackPath\StorageLibrary.dll" "$SourcePath\host\ASRSetup\StorageLibrary\bin\$Config\StorageLibrary.dll" -passthru >> $Log_File		
	Copy-Item "$ASRUACopyBackPath\SystemRequirementValidator.dll" "$SourcePath\host\ASRSetup\SystemRequirementValidator\bin\$Config\SystemRequirementValidator.dll" -passthru >> $Log_File
	Copy-Item "$ASRUACopyBackPath\MobilityServiceValidator.exe" "$SourcePath\host\ASRSetup\MobilityServiceValidator\bin\$Config\MobilityServiceValidator.exe" -passthru >> $Log_File
	Copy-Item "$ASRUACopyBackPath\UnifiedAgentUpgrader.exe" "$SourcePath\host\ASRSetup\UnifiedAgentUpgrader\bin\$Config\UnifiedAgentUpgrader.exe" -passthru >> $Log_File
}

if ($Product -eq "ASRUAMSI")
{
	# Define copy back path.
	$ASRUAMSICopyBackPath = "$FolderPath\$Date\$Product\Signed\$Config"	
	
	Copy-Item "$ASRUAMSICopyBackPath\UnifiedAgentMSI_x86.msi" "$SourcePath\host\ASRSetup\UnifiedAgentMSI\x86\bin\$Config\UnifiedAgentMSI.msi" -passthru >> $Log_File
	Copy-Item "$ASRUAMSICopyBackPath\UnifiedAgentMSI_x64.msi" "$SourcePath\host\ASRSetup\UnifiedAgentMSI\x64\bin\$Config\UnifiedAgentMSI.msi" -passthru >> $Log_File
}

if ($Product -eq "ASRSETUP")
{
	# Define copy back path.
	$ASRUSCopyBackPath = "$FolderPath\$Date\$Product\Signed\$Config"		
	
	Copy-Item "$ASRUSCopyBackPath\ASRResources.dll" "$SourcePath\host\ASRSetup\ASRResources\bin\$Config\ASRResources.dll" -passthru >> $Log_File
	Copy-Item "$ASRUSCopyBackPath\ASRResources.resources.dll" "$SourcePath\host\ASRSetup\ASRResources\bin\$Config\en\ASRResources.resources.dll" -passthru >> $Log_File
	Copy-Item "$ASRUSCopyBackPath\ASRSetupFramework.dll" "$SourcePath\host\ASRSetup\SetupFramework\bin\$Config\ASRSetupFramework.dll" -passthru >> $Log_File	
	Copy-Item "$ASRUSCopyBackPath\UnifiedSetup.exe" "$SourcePath\host\ASRSetup\WrapperSetup\bin\x64\$Config\UnifiedSetup.exe" -passthru >> $Log_File
	Copy-Item "$ASRUSCopyBackPath\RemoveUnifiedSetupConfiguration.dll" "$SourcePath\host\ASRSetup\RemoveUnifiedSetupConfiguration\bin\x64\$Config\RemoveUnifiedSetupConfiguration.dll" -passthru >> $Log_File
	Copy-Item "$ASRUSCopyBackPath\LogUploadService.exe" "$SourcePath\host\ASRSetup\LogUploadService\bin\x64\$Config\LogUploadService.exe" -passthru >> $Log_File
}

if ($Product -eq "PI")
{
	# Define copy back path.
	$PICopyBackPath = "$FolderPath\$Date\$Product\Signed\$Config"

	# Copy unmanaged executables from copyback path to build path.
	Copy-Item "$PICopyBackPath\pushClient.exe" "$SourcePath\host\pushInstallerCli\$Config\pushClient.exe" -passthru >> $Log_File
	Copy-Item "$PICopyBackPath\PushInstall.exe" "$SourcePath\host\PushInstall\CsBasedPushInstall\$Config\PushInstall.exe" -passthru >> $Log_File	
	Copy-Item "$PICopyBackPath\RcmBasedPushInstall.exe" "$SourcePath\host\PushInstall\RcmBasedPushInstall\$Config\RcmBasedPushInstall.exe" -passthru >> $Log_File	
	
	# Copy managed executables from copyback path to build path.
	Copy-Item "$PICopyBackPath\VMwarePushInstall.exe" "$SourcePath\host\VMwarePushInstall\bin\$Config\VMwarePushInstall.exe" -passthru >> $Log_File
	
	Copy-Item "$PICopyBackPath\httpclient.dll" "$SourcePath\host\VMwarePushInstall\bin\$Config\httpclient.dll" -passthru >> $Log_File
	Copy-Item "$PICopyBackPath\VMware.VSphere.Management.dll" "$SourcePath\host\VMwarePushInstall\bin\$Config\VMware.VSphere.Management.dll" -passthru >> $Log_File
	Copy-Item "$PICopyBackPath\VMware.Interfaces.dll" "$SourcePath\host\VMwarePushInstall\bin\$Config\VMware.Interfaces.dll" -passthru >> $Log_File
	Copy-Item "$PICopyBackPath\InMageAPILibrary.dll" "$SourcePath\host\VMwarePushInstall\bin\$Config\InMageAPILibrary.dll" -passthru >> $Log_File
	Copy-Item "$PICopyBackPath\VMware.Interfaces.resources.dll" "$SourcePath\host\VMwarePushInstall\bin\$Config\en\VMware.Interfaces.resources.dll" -passthru >> $Log_File
	Copy-Item "$PICopyBackPath\AutoMapper.dll" "$SourcePath\host\VMwarePushInstall\bin\$Config\AutoMapper.dll" -passthru >> $Log_File
	Copy-Item "$PICopyBackPath\Polly.dll" "$SourcePath\host\VMwarePushInstall\bin\$Config\Polly.dll" -passthru >> $Log_File
}

if ($Product -eq "CX")
{
	# Define copy back path.
	$CXCopyBackPath = "$FolderPath\$Date\$Product\Signed\$Config"
	
	# Copy unmanaged executables from copyback path to build path.
	Copy-Item "$CXCopyBackPath\tmansvc.exe" "$SourcePath\server\windows\tmansvc\$Config\tmansvc.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\SnmpTrapAgent.exe" "$SourcePath\server\windows\SnmpTrapAgent\$Config\SnmpTrapAgent.exe" -passthru >> $Log_File		
	Copy-Item "$CXCopyBackPath\inmtouch.exe" "$SourcePath\server\inmtouch\$Config\inmtouch.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\scheduler.exe" "$SourcePath\host\CSScheduler\$Config\scheduler.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\diffdatasort.exe" "$SourcePath\host\diffdatasort\$Config\diffdatasort.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\inmmessage.dll" "$SourcePath\host\inmmessage\$Config\inmmessage.dll" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\cxps.exe" "$SourcePath\host\cxps\x64\$Config\cxps.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\cxpscli.exe" "$SourcePath\host\cxpscli\$Config\cxpscli.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\cxpsclient.exe" "$SourcePath\host\cxpsclient\$Config\cxpsclient.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\pushClient.exe" "$SourcePath\host\pushInstallerCli\$Config\pushClient.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\PushInstall.exe" "$SourcePath\host\PushInstall\CsBasedPushInstall\$Config\PushInstall.exe" -passthru >> $Log_File	
	Copy-Item "$CXCopyBackPath\RcmBasedPushInstall.exe" "$SourcePath\host\PushInstall\RcmBasedPushInstall\$Config\RcmBasedPushInstall.exe" -passthru >> $Log_File	
	Copy-Item "$CXCopyBackPath\csgetfingerprint.exe" "$SourcePath\host\csgetfingerprint\$Config\csgetfingerprint.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\gencert.exe" "$SourcePath\host\gencert\$Config\gencert.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\genpassphrase.exe" "$SourcePath\host\genpassphrase\$Config\genpassphrase.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\verifycert.exe" "$SourcePath\host\verifycert\$Config\verifycert.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\viewcert.exe" "$SourcePath\host\viewcert\$Config\viewcert.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\configtool.dll" "$SourcePath\host\configtool\$Config\configtool.dll" -passthru >> $Log_File
	
	# Copy managed executables from copyback path to build path.
	Copy-Item "$CXCopyBackPath\CSAuthModule.dll" "$SourcePath\server\windows\CSAuthModule\bin\x64\$Config\CSAuthModule.dll" -passthru >> $Log_File	
	Copy-Item "$CXCopyBackPath\cspsconfigtool.exe" "$SourcePath\host\cspsconfigtool\bin\$Config\cspsconfigtool.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\httpclient.dll" "$SourcePath\host\csharphttpclient\bin\$Config\httpclient.dll" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\InMageAPILibrary.dll" "$SourcePath\host\InMageAPILibrary\bin\InMageAPILibrary.dll" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\cspsconfigtool.resources.dll" "$SourcePath\host\cspsconfigtool\bin\$Config\en-US\cspsconfigtool.resources.dll" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\RenewCerts.exe" "$SourcePath\server\windows\RenewCerts\bin\$Config\RenewCerts.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\VMwarePushInstall.exe" "$SourcePath\host\VMwarePushInstall\bin\$Config\VMwarePushInstall.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\Microsoft.Azure.SiteRecovery.ProcessServer.Core.dll" "$SourcePath\server\ProcessServerCore\bin\$Config\Microsoft.Azure.SiteRecovery.ProcessServer.Core.dll" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\ProcessServer.exe" "$SourcePath\server\windows\ProcessServer\bin\$Config\ProcessServer.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\ProcessServerMonitor.exe" "$SourcePath\server\windows\ProcessServerMonitor\bin\$Config\ProcessServerMonitor.exe" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\MarsAgent.exe" "$SourcePath\server\MarsAgent\bin\$Config\MarsAgent.exe" -passthru >> $Log_File
	
	
	# Copy Java scripts from copyback path to build path.
	Copy-Item "$CXCopyBackPath\comfun.js" "$SourcePath\admin\web\ui\js\comfun.js" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\common.js" "$SourcePath\admin\web\ui\js\common.js" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\flash_detection.js" "$SourcePath\admin\web\ui\js\flash_detection.js" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\group_recovery.js" "$SourcePath\admin\web\ui\js\group_recovery.js" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\index-menu.js" "$SourcePath\admin\web\ui\js\index-menu.js" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\nls_menu.js" "$SourcePath\admin\web\ui\js\nls_menu.js" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\pushagentstatus.js" "$SourcePath\admin\web\ui\js\pushagentstatus.js" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\pushagentstatusall.js" "$SourcePath\admin\web\ui\js\pushagentstatusall.js" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\RetentionValidation.js" "$SourcePath\admin\web\ui\js\RetentionValidation.js" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\save_dashboard_settings.js" "$SourcePath\admin\web\ui\js\save_dashboard_settings.js" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\single_recovery.js" "$SourcePath\admin\web\ui\js\single_recovery.js" -passthru >> $Log_File
	
	# Copy VB scripts from copyback path to build path.
	Copy-Item "$CXCopyBackPath\Agentcheck.vbs" "$SourcePath\Solutions\SolutionsWizard\Scripts\p2v_esx\Agentcheck.vbs" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\prereqcheck.vbs" "$SourcePath\Solutions\SolutionsWizard\Scripts\p2v_esx\prereqcheck.vbs" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\unzip_Files.vbs" "$SourcePath\server\windows\unzip_Files.vbs" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\mount.vbs" "$SourcePath\server\windows\mount.vbs" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\unzip.vbs" "$SourcePath\server\windows\unzip.vbs" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\unzip_strawberry.vbs" "$SourcePath\server\windows\unzip_strawberry.vbs" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\delete.vbs" "$SourcePath\server\windows\delete.vbs" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\createlink.vbs" "$SourcePath\host\setup\createlink.vbs" -passthru >> $Log_File		
			
	# Copy power shell scripts from copyback path to build path.
	Copy-Item "$CXCopyBackPath\Site_Secure_Config.ps1" "$SourcePath\server\windows\Site_Secure_Config.ps1" -passthru >> $Log_File	
	Copy-Item "$CXCopyBackPath\Remove_Binding.ps1" "$SourcePath\server\windows\Remove_Binding.ps1" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\Uninstall_DRA.ps1" "$SourcePath\server\windows\Uninstall_DRA.ps1" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\PassphraseUpdateHealth.ps1" "$SourcePath\server\windows\PassphraseUpdateHealth.ps1" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\Create_Shortcut.ps1" "$SourcePath\server\windows\Create_Shortcut.ps1" -passthru >> $Log_File
	Copy-Item "$CXCopyBackPath\Check_Software_Installed.ps1" "$SourcePath\server\windows\Check_Software_Installed.ps1" -passthru >> $Log_File	
	Copy-Item "$CXCopyBackPath\PS_Configurator.ps1" "$SourcePath\server\windows\PS_Configurator.ps1" -passthru >> $Log_File
}
