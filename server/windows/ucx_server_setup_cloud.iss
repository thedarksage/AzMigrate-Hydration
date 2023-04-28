; Installation script for Caspian CX Server

#include "..\..\host\setup\version.iss"
#include "..\..\host\setup\branding_parameters.iss"


#ifdef DEBUG
#define Configuration "Debug"
#define ConfigurationAppVerName "Debug"
#else
#define Configuration "Release"
#define ConfigurationAppVerName ""
#endif

[Setup]
AppName=Microsoft Azure Site Recovery Configuration/Process Server
AppVerName=Microsoft Azure Site Recovery Configuration/Process Server
AppVersion={#APPVERSION}
AppPublisher=Microsoft Corporation
DefaultDirName={code:GetDefaultDirName}
DefaultGroupName={#COMPANYNAME}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
OutputBaseFilename=ucx_server_setup
OutputDir={#Configuration}
DirExistsWarning=no
DisableStartupPrompt=yes
uninstallable=yes
DisableReadyPage=yes
SetupLogging=yes
ChangesEnvironment=yes
VersionInfoCompany=Microsoft Corporation
VersionInfoCopyright={#COPYRIGHT}
VersionInfoDescription=CX
VersionInfoProductName=Microsoft Azure Site Recovery
VersionInfoVersion={#APPVERSION}
VersionInfoProductVersion={#VERSION}
CloseApplications=force
Compression=lzma2/ultra64
DiskSpanning=yes
DiskSliceSize=1678000000

[LangOptions]
DialogFontName=Times New Roman
DialogFontSize=10
CopyrightFontName=Times New Roman
CopyrightFontSize=10

[Files]
;server\db files
Source: ..\tm\bin\getAllWinNics.pl ; destdir : {sd}; flags: dontcopy
Source: ..\tm\bin\clean_stale_process.pl ; destdir : {sd}; flags: dontcopy
Source: ..\packages\VCRedistributable.0.0.1\vcredist_x86.exe ; destdir : {sd}; flags: dontcopy 
Source: ..\packages\VCRedistributable.0.0.1\vc_redist.x86.exe; destdir : {sd}; flags: dontcopy
Source: Check_Software_Installed.ps1; DestDir: dontcopy; Flags: ignoreversion 
Source: ..\..\host\csgetfingerprint\{#Configuration}\csgetfingerprint.exe; DestDir: dontcopy; Flags: ignoreversion;
Source: ..\db\*.sql; DestDir: C:\Temp; Excludes: cloud_vhd.sql ; Flags: ignoreversion
Source: ..\db\*.php; DestDir: C:\Temp; Flags: ignoreversion
Source: ..\db\*.*; DestDir: {code:Getdircx}\Upgrade; Flags: ignoreversion; Check: UpgradeCheck
Source: mysql-init.txt; DestDir: C:\Temp; Flags: ignoreversion overwritereadonly; Check: (not (PSOnlySelected));
Source: my.ini; DestDir: "{code:GetMySQLDataPath}"; Flags: ignoreversion overwritereadonly; Check: ((not (UpgradeCheck)) and (not (PSOnlySelected))); AfterInstall : ChangeDataPath;
Source: my.ini; DestDir:  C:\Temp; Flags: ignoreversion overwritereadonly; Check: (not (PSOnlySelected))
;Batch scripts in the source path
Source: *.bat; DestDir: C:\Temp\; Flags: ignoreversion
Source: verifydb.bat; DestDir: C:\Temp; Flags: ignoreversion; Afterinstall: Verifydatabase; Check: ((not (UpgradeCheck)) and (not (PSOnlySelected)))
Source: cx_backup.bat; DestDir: {code:Getdircx}\bin; Flags: ignoreversion
Source: ..\..\host\setup\manifest.txt; DestDir: {code:Getdircx}\bin; Flags: ignoreversion
Source: Language_Names.txt; DestDir: dontcopy; Flags: ignoreversion
Source: unzip_Files.vbs; DestDir: dontcopy; Flags: ignoreversion

Source: Site_Secure_Config.ps1; DestDir: C:\Temp; Flags: ignoreversion
Source: Remove_Binding.ps1; DestDir: {code:Getdircx}; Flags: ignoreversion
Source: Uninstall_DRA.ps1; DestDir: {code:Getdircx}; Flags: ignoreversion
Source: PassphraseUpdateHealth.ps1; DestDir: {code:Getdircx}\bin; Flags: ignoreversion

;Packging the below batch scripts to be used while installing UA also on the same box.(CX)
Source: start_services.bat; DestDir: {code:Getdircx}\bin; Flags: ignoreversion
Source: stop_services.bat; DestDir: {code:Getdircx}\bin; Flags: ignoreversion

Source: ..\..\host\setup\removefiles.bat; DestDir : {code:Getdircx}\bin; Flags: ignoreversion
Source: configure.bat; DestDir: C:\Temp; Flags: ignoreversion

Source: configure_upgrade.bat; DestDir: C:\Temp; Flags: ignoreversion; Afterinstall: Upgradedatabase; Check: UpgradeCheck and (not (PSOnlySelected))
Source: Stop_CX_Services.bat; DestDir: {code:Getdircx}\bin; Flags: ignoreversion;
Source: removedb.bat; DestDir: {code:Getdircx}; Flags: ignoreversion;
Source: Unconfigure_IIS.bat; DestDir: {code:Getdircx}; Flags: ignoreversion;
Source: Uninstall_DRA_Invoke.bat; DestDir: {code:Getdircx}; Flags: ignoreversion;
Source: ..\tm\build\p_valid.pl; DestDir: dontcopy;

;perl files and modules and conf files in the source path
Source: rsyncd.conf; DestDir: {code:Getdircx}\etc; Flags: ignoreversion
;tmansvc files
Source: tmansvc\{#Configuration}\tmansvc.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\tmansvc.exe');
;ProcessServerCore files
Source: ..\ProcessServerCore\bin\{#Configuration}\Microsoft.Azure.SiteRecovery.ProcessServer.Core.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\Microsoft.Azure.SiteRecovery.ProcessServer.Core.dll');
;ProcessServer files
Source: ProcessServer\bin\{#Configuration}\ProcessServer.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer.exe');
Source: ProcessServer\bin\{#Configuration}\ProcessServer.exe.config; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer.exe.config');
;ProcessServerMonitor files
Source: ProcessServerMonitor\bin\{#Configuration}\ProcessServerMonitor.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServerMonitor.exe');
Source: ProcessServerMonitor\bin\{#Configuration}\ProcessServerMonitor.exe.config; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServerMonitor.exe.config');
;ProcessServer Dependencies
; NOTE-SanKumar-1905: Creating a subfolder named ProcessServer under bin\ and redirecting assembly resolutions of ProcessServer.exe and ProcessServerMonitor.exe
; via their App.Configs, as the bin\ folder where these EXEs exist contain same DLLs of different versions from DRA build. Also, only the subset of the assemblies
; referred in these projects are deployed, since the rest are only needed in RCM PS.
Source: ..\packages\Newtonsoft.Json.13.0.1\lib\net45\Newtonsoft.Json.dll; DestDir: {code:Getdircx}\bin\ProcessServer; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer\Newtonsoft.Json.dll');
Source: ..\packages\System.ValueTuple.4.5.0\lib\net461\System.ValueTuple.dll; DestDir: {code:Getdircx}\bin\ProcessServer; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer\System.ValueTuple.dll');
Source: ..\packages\ClientLibHelpers.1.0.0.47\lib\net462\ClientLibHelpers.dll; DestDir: {code:Getdircx}\bin\ProcessServer; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer\ClientLibHelpers.dll');
Source: ..\packages\CredentialStore.1.1.0.6\lib\net462\CredentialStore.dll; DestDir: {code:Getdircx}\bin\ProcessServer; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer\CredentialStore.dll');
Source: ..\packages\RcmAgentCommonLib.1.30.8485.9888\lib\net462\RcmAgentCommonLib.dll; DestDir: {code:Getdircx}\bin\ProcessServer; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer\RcmAgentCommonLib.dll');
Source: ..\packages\RcmClientLib.1.37.8450.18164\lib\net462\RcmClientLib.dll; DestDir: {code:Getdircx}\bin\ProcessServer; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer\RcmClientLib.dll');
Source: ..\packages\RcmContract.1.37.8450.18164\lib\net462\RcmContract.dll; DestDir: {code:Getdircx}\bin\ProcessServer; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer\RcmContract.dll');
Source: ..\packages\WindowsAzure.ServiceBus.6.0.2\lib\net462\Microsoft.ServiceBus.dll; DestDir: {code:Getdircx}\bin\ProcessServer; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer\Microsoft.ServiceBus.dll');
Source: ..\packages\Microsoft.ApplicationInsights.2.15.0\lib\net46\Microsoft.ApplicationInsights.dll; DestDir: {code:Getdircx}\bin\ProcessServer; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer\Microsoft.ApplicationInsights.dll');
Source: ..\packages\Microsoft.ApplicationInsights.WindowsServer.TelemetryChannel.2.15.0\lib\net452\Microsoft.AI.ServerTelemetryChannel.dll; DestDir: {code:Getdircx}\bin\ProcessServer; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer\Microsoft.AI.ServerTelemetryChannel.dll');
Source: ..\packages\Microsoft.Identity.Client.4.48.1\lib\net461\Microsoft.Identity.Client.dll; DestDir: {code:Getdircx}\bin\ProcessServer; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer\Microsoft.Identity.Client.dll');
Source: ..\packages\Microsoft.IdentityModel.Abstractions.6.22.0\lib\net461\Microsoft.IdentityModel.Abstractions.dll; DestDir: {code:Getdircx}\bin\ProcessServer; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ProcessServer\Microsoft.IdentityModel.Abstractions.dll');

;snmptrapagent files
Source: SnmpTrapAgent\{#Configuration}\SnmpTrapAgent.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\SnmpTrapAgent.exe');

;InMageDiscovery.exe 
Source: ..\..\host\setup\DRA\InMageDiscovery.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\InMageDiscovery.exe');

; VMware management SDK DLLs
Source: ..\..\host\setup\DRA\AutoMapper.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\AutoMapper.dll');
Source: ..\..\host\setup\DRA\STSService.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\STSService.dll');
Source: ..\..\host\setup\DRA\VMware.Interfaces.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\VMware.Interfaces.dll');
Source: ..\..\host\setup\DRA\VMware.Interfaces.resources.dll; DestDir: {code:Getdircx}\bin\en; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\en\VMware.Interfaces.resources.dll');
Source: ..\..\host\setup\DRA\Vim25Service.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\Vim25Service.dll');
Source: ..\..\host\setup\DRA\VMware.VSphere.Management.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\VMware.VSphere.Management.dll');
Source: ..\..\host\setup\DRA\Polly.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\Polly.dll');

;Packaging LogUploadService.exe along with its dependency binaries for uploading setup logs to telemetry.
Source: ..\..\host\ASRSetup\LogUploadService\bin\x64\{#Configuration}\AccessControl2.S2S.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\AccessControl2.S2S.dll');
Source: ..\..\host\ASRSetup\ASRResources\bin\{#Configuration}\ASRResources.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ASRResources.dll');
Source: ..\..\host\ASRSetup\ASRResources\bin\{#Configuration}\en\ASRResources.resources.dll; DestDir: {code:Getdircx}\bin\en; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\en\ASRResources.resources.dll');
Source: ..\..\host\ASRSetup\SetupFramework\bin\{#Configuration}\ASRSetupFramework.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ASRSetupFramework.dll');
Source: ..\..\host\setup\DRA\AsyncInterface.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\AsyncInterface.dll');
Source: ..\..\host\setup\DRA\CatalogCommon.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\CatalogCommon.dll');
Source: ..\..\host\setup\DRA\CloudCommonInterface.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\CloudCommonInterface.dll');
Source: ..\..\host\setup\DRA\CloudSharedInfra.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\CloudSharedInfra.dll');
Source: ..\..\host\setup\DRA\DRResources.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\DRResources.dll');
;Source: ..\..\host\setup\DRA\DRResources.resources.dll; DestDir: {code:Getdircx}\bin\en; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\en\DRResources.resources.dll');
Source: ..\..\host\setup\DRA\ErrorCodeUtils.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\ErrorCodeUtils.dll');
Source: ..\..\host\setup\DRA\IdMgmtApiClientLib.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\IdMgmtApiClientLib.dll');
Source: ..\..\host\setup\DRA\IdMgmtInterface.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\IdMgmtInterface.dll');
Source: ..\..\host\setup\DRA\IntegrityCheck.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\IntegrityCheck.dll');
Source: ..\..\host\ASRSetup\LogUploadService\bin\x64\{#Configuration}\LogUploadService.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\LogUploadService.exe');
Source: ..\..\host\setup\DRA\Microsoft.ApplicationInsights.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\Microsoft.ApplicationInsights.dll');
Source: ..\..\host\setup\DRA\Microsoft.Identity.Client.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\Microsoft.Identity.Client.dll');
Source: ..\..\host\setup\DRA\Microsoft.IdentityModel.Abstractions.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\Microsoft.IdentityModel.Abstractions.dll');
Source: ..\..\host\setup\DRA\Newtonsoft.Json.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\Newtonsoft.Json.dll');
Source: ..\..\host\setup\DRA\SetupFramework.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\SetupFramework.dll');
Source: ..\..\host\setup\DRA\SrsRestApiClientLib.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\SrsRestApiClientLib.dll');
Source: ..\..\host\setup\DRA\TelemetryInterface.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\TelemetryInterface.dll');
Source: ..\..\host\setup\DRA\EndpointsConfig.xml; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\EndpointsConfig.xml');

Source: ..\..\host\ASRSetup\SetMarsProxy\bin\x64\{#Configuration}\SetMarsProxy.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\SetMarsProxy.exe');

Source: CSAuthModule\bin\x64\{#Configuration}\CSAuthModule.dll; DestDir: {code:Getdircx}\admin\web\BIN; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\admin\web\BIN\CSAuthModule.dll');
Source: RenewCerts\bin\{#Configuration}\RenewCerts.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\RenewCerts.exe');
Source: RenewCerts\bin\{#Configuration}\RenewCerts.exe.config; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\RenewCerts.exe.config');
;server\tm files
;copying all the perl(.pl) and (.sh) files
Source: ..\tm\bin\*; DestDir: {code:Getdircx}\bin; Excludes: \CVS\* ; Flags: ignoreversion recursesubdirs

;copying all the perl modules(.pm) files from tm\pm path to C:\strawberry\perl\site\lib
Source: ..\tm\pm\*; DestDir: {code:Getdircx}\pm; Excludes: \CVS\* ; Flags: ignoreversion recursesubdirs

Source: ..\tm\systemjobd; DestDir: {code:Getdircx}\etc\init.d; Flags: ignoreversion recursesubdirs
Source: ..\tm\bin\systemjobs.pl; DestDir: {code:Getdircx}\bin; Flags: ignoreversion recursesubdirs
Source: ..\tm\pm\SystemJobs.pm; DestDir: {code:Getdircx}\pm; Flags: ignoreversion recursesubdirs
Source: ..\tm\pm\EventManager.pm; DestDir: {code:Getdircx}\pm; Flags: ignoreversion recursesubdirs
Source: ..\tm\pm\Common\Log.pm; DestDir: {code:Getdircx}\pm\Common; Flags: ignoreversion recursesubdirs
Source: ..\tm\pm\InstallationPath.pm; DestDir: {sd}\ProgramData\Microsoft Azure Site Recovery\Scripts; Flags: ignoreversion recursesubdirs

;other tm files
Source: ..\tm\etc\*; DestDir: {code:Getdircx}\etc; Excludes: amethyst.conf.orig; Flags: ignoreversion recursesubdirs; Afterinstall: ReplaceConfFiles;
Source: ..\tm\failover.resource; DestDir: {code:Getdircx}\etc; Flags: ignoreversion
Source: ..\tm\version; DestDir: {code:Getdircx}\etc; Flags: ignoreversion;

Source: RunPerlFiles.bat; DestDir: C:\Temp; Flags: ignoreversion deleteafterinstall; Afterinstall: Run_rrd_tunepl; Check: UpgradeCheck

;server\ files
Source: ..\ha\db_sync_src_pre_script.pl; DestDir: {code:Getdircx}\bin; Flags: ignoreversion
Source: ..\ha\db_sync_tgt_post_script.pl; DestDir: {code:Getdircx}\bin; Flags: ignoreversion
Source: ..\inmtouch\{#Configuration}\inmtouch.exe; DestDir: {code:Getdircx}\bin; DestName: inmtouch.exe; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\inmtouch.exe');
Source: ..\linux\Abhai-1-MIB-V1.mib; DestDir: {code:Getdircx}\etc\mibs; Flags: ignoreversion

;copying all the files and folders under admin\web
Source: ..\..\admin\web\*; DestDir: {code:Getdircx}\admin\web; Excludes: ui,inmages,xml_rpc,trends,cgi-bin,\CVS\*; Flags: ignoreversion restartreplace recursesubdirs

;host files.
Source: ..\..\host\diffdatasort\{#Configuration}\diffdatasort.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\diffdatasort.exe');
Source: ..\..\host\inmmessage\{#Configuration}\inmmessage.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion restartreplace; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\inmmessage.dll');

; cx process server related files.
Source: ..\..\host\cxps\x64\{#Configuration}\cxps.exe; DestDir: {code:Getdircx}\transport; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\transport\cxps.exe');
Source: ..\..\host\cxpscli\{#Configuration}\cxpscli.exe; DestDir: {code:Getdircx}\transport; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\transport\cxpscli.exe');
Source: ..\..\host\cxpslib\cxps.conf; DestDir: {code:Getdircx}\transport; Flags: ignoreversion; Check: not UpgradeCheck
Source: ..\..\host\cxpslib\pem\dh.pem; DestDir: {code:Getdircx}\transport\pem; Flags: ignoreversion
Source: ..\..\host\cxpslib\pem\servercert.pem; DestDir: {code:Getdircx}\transport\pem; Flags: ignoreversion
Source: ..\..\host\cxpsclient\{#Configuration}\cxpsclient.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion restartreplace; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\cxpsclient.exe');

; vCon discovery files
Source: ..\..\Solutions\SolutionsWizard\Scripts\p2v_esx\*; DestDir: {code:Getdircx}\pm\Cloud; Excludes: *.sh,\CVS\* ;Flags: ignoreversion restartreplace recursesubdirs

; Caspian specific files
Source: ..\rpm\asr\etc\*; DestDir: {code:Getdircx}\etc; Flags: ignoreversion restartreplace recursesubdirs
Source: ..\..\host\csgetfingerprint\{#Configuration}\csgetfingerprint.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\csgetfingerprint.exe');
Source: ..\..\host\gencert\{#Configuration}\gencert.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\gencert.exe');
Source: ..\..\host\genpassphrase\{#Configuration}\genpassphrase.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\genpassphrase.exe');
Source: ..\..\host\verifycert\{#Configuration}\verifycert.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\verifycert.exe');
Source: ..\..\host\viewcert\{#Configuration}\viewcert.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\viewcert.exe');
Source: ..\..\host\configtool\{#Configuration}\configtool.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\configtool.dll');
Source: ..\..\host\cspsconfigtool\bin\{#Configuration}\cspsconfigtool.exe; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\cspsconfigtool.exe');
Source: ..\..\host\cspsconfigtool\bin\{#Configuration}\cspsconfigtool.exe.config; DestDir: {code:Getdircx}\bin; Flags: ignoreversion

Source: ..\..\host\csharphttpclient\bin\{#Configuration}\httpclient.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\httpclient.dll');
Source: ..\..\host\InMageAPILibrary\bin\InMageAPILibrary.dll; DestDir: {code:Getdircx}\bin; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\bin\InMageAPILibrary.dll');
Source: ..\..\host\cspsconfigtool\bin\{#Configuration}\en-US\cspsconfigtool.resources.dll; DestDir: {code:Getdircx}\bin\en-US; Flags: ignoreversion

;Push install files
;host
Source: ..\..\host\pushInstallerCli\{#Configuration}\pushClient.exe; DestDir: {code:Getdircx}\pushinstallsvc; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\pushinstallsvc\pushClient.exe');
Source: ..\..\host\PushInstall\CsBasedPushInstall\{#Configuration}\PushInstall.exe; DestDir: {code:Getdircx}\pushinstallsvc; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\pushinstallsvc\PushInstall.exe');

Source: ..\..\host\csharphttpclient\bin\{#Configuration}\httpclient.dll; DestDir: {code:Getdircx}\pushinstallsvc; Flags: ignoreversion;
Source: ..\..\host\InMageAPILibrary\bin\InMageAPILibrary.dll; DestDir: {code:Getdircx}\pushinstallsvc; Flags: ignoreversion;
Source: ..\packages\Newtonsoft.Json.13.0.1\lib\net45\Newtonsoft.Json.dll; DestDir: {code:Getdircx}\pushinstallsvc; Flags: ignoreversion;
Source: ..\..\host\VMwarePushInstall\bin\{#Configuration}\VMwarePushInstall.exe; DestDir: {code:Getdircx}\pushinstallsvc; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\pushinstallsvc\VMwarePushInstall.exe');

Source: ..\..\host\VMwarePushInstall\VMwarePushInstall.conf; DestDir: {code:Getdircx}\pushinstallsvc; Flags: ignoreversion;

; VMware management SDK DLLs
Source: ..\..\host\setup\DRA\AutoMapper.dll; DestDir: {code:Getdircx}\pushinstallsvc; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\pushinstallsvc\AutoMapper.dll');
Source: ..\..\host\setup\DRA\STSService.dll; DestDir: {code:Getdircx}\pushinstallsvc; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\pushinstallsvc\STSService.dll');
Source: ..\..\host\setup\DRA\VMware.Interfaces.dll; DestDir: {code:Getdircx}\pushinstallsvc; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\pushinstallsvc\VMware.Interfaces.dll');
Source: ..\..\host\setup\DRA\VMware.Interfaces.resources.dll; DestDir: {code:Getdircx}\pushinstallsvc\en; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\pushinstallsvc\en\VMware.Interfaces.resources.dll');
Source: ..\..\host\setup\DRA\Vim25Service.dll; DestDir: {code:Getdircx}\pushinstallsvc; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\pushinstallsvc\Vim25Service.dll');
Source: ..\..\host\setup\DRA\Polly.dll; DestDir: {code:Getdircx}\pushinstallsvc; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\pushinstallsvc\Polly.dll');
Source: ..\..\host\setup\DRA\VMware.VSphere.Management.dll; DestDir: {code:Getdircx}\pushinstallsvc; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('{code:Getdircx}\pushinstallsvc\VMware.VSphere.Management.dll');

;build
Source: ..\packages\InmLSA.0.0.3\InmLSA.exe; DestDir: {code:Getdircx}\pushinstallsvc;
Source: Unix_LE_OS_details.sh; Destname: OS_details.sh; DestDir: {code:Getdircx}\pushinstallsvc;

;Packaging Windows and Linux source agents and Linux push clients
#ifndef DEBUG
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_Windows_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.exe; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_RHEL6-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_RHEL7-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_RHEL8-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_OL7-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_OL8-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_SLES12-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_SLES15-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_UBUNTU-16.04-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_UBUNTU-18.04-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_UBUNTU-20.04-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_DEBIAN10-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_DEBIAN11-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;

;Source: ..\..\host\setup\AgentArtifacts\RHEL5-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\RHEL6-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\RHEL7-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\RHEL8-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\OL6-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\OL7-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\OL8-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
;Source: ..\..\host\setup\AgentArtifacts\SLES11-SP3-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
;Source: ..\..\host\setup\AgentArtifacts\SLES11-SP4-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\SLES12-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\SLES15-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\UBUNTU-14.04-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\UBUNTU-16.04-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\UBUNTU-18.04-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\UBUNTU-20.04-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\DEBIAN7-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\DEBIAN8-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\DEBIAN9-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\DEBIAN10-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;
Source: ..\..\host\setup\AgentArtifacts\DEBIAN11-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\pushinstallsvc\repository; Flags: ignoreversion;

Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_Windows_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.exe; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_RHEL6-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_RHEL7-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_RHEL8-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_OL7-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_OL8-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_SLES12-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_SLES15-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_UBUNTU-16.04-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_UBUNTU-18.04-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_UBUNTU-20.04-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_DEBIAN10-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\Microsoft-ASR_UA_{#VERSION}_DEBIAN11-64_{#BUILDPHASE}_{#BUILDDATE}_{#Configuration}.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected

;Source: ..\..\host\setup\AgentArtifacts\RHEL5-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\RHEL6-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\RHEL7-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\RHEL8-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\OL6-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\OL7-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\OL8-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
;Source: ..\..\host\setup\AgentArtifacts\SLES11-SP3-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
;Source: ..\..\host\setup\AgentArtifacts\SLES11-SP4-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\SLES12-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\SLES15-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\UBUNTU-14.04-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\UBUNTU-16.04-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\UBUNTU-18.04-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\UBUNTU-20.04-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\DEBIAN7-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\DEBIAN8-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\DEBIAN9-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\DEBIAN10-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected
Source: ..\..\host\setup\AgentArtifacts\DEBIAN11-64_pushinstallclient.tar.gz; DestDir: {code:Getdircx}\admin\web\sw; Flags: ignoreversion; Check: not PSOnlySelected

#endif

;thirdparty
Source: ..\..\thirdparty\Notices\Third-Party_Notices.rtf; DestDir: {code:Getdircx}; Flags: ignoreversion
Source: ..\..\thirdparty\server\Win32-UTCFileTime-1.50\*; DestDir: C:\Temp\Win32-UTCFileTime-1.50; Excludes: \CVS\*; Flags: ignoreversion  recursesubdirs
Source: ..\..\thirdparty\server\Win32-UTCFileTime-1.50\lib\Win32\UTCFileTime.pm; DestDir: C:\strawberry\perl\site\lib\Win32; Flags: ignoreversion overwritereadonly; Check: UpgradeCheck

Source: ..\..\thirdparty\server\cpan\PHP\*; DestDir: {code:Getdircx}\pm\Cloud\PHP; Excludes: \CVS\* ;Flags: ignoreversion restartreplace recursesubdirs
Source: ..\..\thirdparty\server\cpan\PHP\*; DestDir: {code:Getdircx}\pm\Cloud\vCloud\PHP; Excludes: \CVS\* ;Flags: ignoreversion restartreplace recursesubdirs

Source: ..\..\thirdparty\server\xcache\xcache.ini; DestDir: {code:Getdircx}\etc\php.d; Excludes: \CVS\* ;Flags: ignoreversion restartreplace recursesubdirs
Source: ..\..\thirdparty\server\cpan\*; DestDir: {code:Getdircx}\pm; Excludes: \CVS\*,\Mail\* ;Flags: ignoreversion restartreplace recursesubdirs

Source: installmod1.bat; DestDir: C:\Temp\; Flags: ignoreversion; Afterinstall: InstallPerlModules;
;Pdbs to be packaged for debug build.
#ifdef DEBUG
Source: tmansvc\{#Configuration}\tmansvc.pdb; DestDir: {code:Getdircx}\bin; Flags: ignoreversion
Source: ..\ProcessServerCore\bin\{#Configuration}\Microsoft.Azure.SiteRecovery.ProcessServer.Core.pdb; DestDir: {code:Getdircx}\bin; Flags: ignoreversion
Source: ProcessServer\bin\{#Configuration}\ProcessServer.pdb; DestDir: {code:Getdircx}\bin; Flags: ignoreversion
Source: ProcessServerMonitor\bin\{#Configuration}\ProcessServerMonitor.pdb; DestDir: {code:Getdircx}\bin; Flags: ignoreversion
Source: ..\..\host\cxps\x64\{#Configuration}\cxps.pdb; DestDir: {code:Getdircx}\transport; Flags: ignoreversion
Source: ..\..\host\cxpscli\{#Configuration}\cxpscli.pdb; DestDir: {code:Getdircx}\transport; Flags: ignoreversion
Source: ..\..\host\PushInstall\CsBasedPushInstall\{#Configuration}\pushInstall.pdb; DestDir: {code:Getdircx}\pushinstallsvc
Source: ..\..\host\pushInstallerCli\{#Configuration}\pushInstallerCli.pdb; DestDir: {code:Getdircx}\pushinstallsvc
Source: ..\..\host\csgetfingerprint\{#Configuration}\csgetfingerprint.pdb; DestDir: {code:Getdircx}\bin; Flags: ignoreversion;
Source: ..\..\host\gencert\{#Configuration}\gencert.pdb; DestDir: {code:Getdircx}\bin; Flags: ignoreversion;
Source: ..\..\host\genpassphrase\{#Configuration}\genpassphrase.pdb; DestDir: {code:Getdircx}\bin; Flags: ignoreversion;
Source: ..\..\host\verifycert\{#Configuration}\verifycert.pdb; DestDir: {code:Getdircx}\bin; Flags: ignoreversion;
Source: ..\..\host\viewcert\{#Configuration}\viewcert.pdb; DestDir: {code:Getdircx}\bin; Flags: ignoreversion
Source: CSAuthModule\bin\x64\{#Configuration}\CSAuthModule.pdb; DestDir: {code:Getdircx}\admin\web\BIN; Flags: ignoreversion
Source: ..\..\host\configtool\{#Configuration}\configtool.pdb; DestDir: {code:Getdircx}\bin; Flags: ignoreversion
Source: ..\..\host\cspsconfigtool\bin\{#Configuration}\cspsconfigtool.pdb; DestDir: {code:Getdircx}\bin; Flags: ignoreversion
Source: ..\..\host\csharphttpclient\bin\{#Configuration}\httpclient.pdb; DestDir: {code:Getdircx}\bin; Flags: ignoreversion
Source: ..\..\host\InMageAPILibrary\bin\InMageAPILibrary.pdb; DestDir: {code:Getdircx}\bin; Flags: ignoreversion

#endif
[Dirs]
Name : {code:Getdircx}; Flags: uninsalwaysuninstall; Check: (not (UpgradeCheck))
Name : {code:Getdircx}\var; Flags: uninsalwaysuninstall; Check: (not (UpgradeCheck))
Name : {sd}\ProgramData\Microsoft Azure Site Recovery\; Check: (not (UpgradeCheck))
Name : {sd}\ProgramData\Microsoft Azure Site Recovery\private; Check: (not (UpgradeCheck))
#include "..\..\host\setup\Windows_Services.iss"

[Messages]
SelectDirBrowseLabel=To continue, click Next. If you would like to select a different drive, choose from the list.
SetupWindowTitle=%1
WizardLicense=License Terms
LicenseLabel=
LicenseLabel3=You must accept the license terms before you can install or use the software. If you do not accept the license terms, installation will not proceed. You may print the license terms by clicking the Print button below.
FinishedLabelNoIcons=
ClickFinish=
WizardSelectDir=Select Installation Drive
SelectDirDesc=
SelectDirBrowseLabel=
DiskSpaceMBLabel=

[Registry]

Root: HKLM; Subkey: Software\Cygnus Solutions; Flags: uninsdeletekeyifempty

; Keys for proftpd server
Root: HKLM; Subkey: Software\Cygnus Solutions; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin\mounts v2; ValueType: string; ValueName: cygdrive prefix; ValueData: /cygdrive; Flags: uninsdeletevalue
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin\mounts v2; ValueType: dword; ValueName: cygdrive flags; ValueData: 00000022; Flags: uninsdeletevalue
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin\mounts v2\/; ValueType: string; ValueName: native; ValueData: {code:Getdircx}; Flags: uninsdeletevalue
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin\mounts v2\/; ValueType: dword; ValueName: flags; ValueData: 000000010; Flags: uninsdeletevalue
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin\mounts v2\/etc; ValueType: dword; ValueName: flags; ValueData: 000000010; Flags: uninsdeletevalue
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin\mounts v2\/etc; ValueType: string; ValueName: native; ValueData: {code:Getdircx}\etc
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin\mounts v2\/usr/bin; ValueType: dword; ValueName: flags; ValueData: 000000010; Flags: uninsdeletevalue
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin\mounts v2\/usr/bin; ValueType: string; ValueName: native; ValueData: {code:Getdircx}\bin; Flags: uninsdeletevalue
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin\mounts v2\/usr/lib; ValueType: dword; ValueName: flags; ValueData: 000000010; Flags: uninsdeletevalue
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin\mounts v2\/usr/lib; ValueType: string; ValueName: native; ValueData: {code:Getdircx}\bin; Flags: uninsdeletevalue
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin\mounts v2\/home; ValueType: dword; ValueName: flags; ValueData: 000000010; Flags: uninsdeletevalue
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin\mounts v2\/home; ValueType: string; ValueName: native; ValueData: {code:Getdircx}\..; Flags: uninsdeletevalue
Root: HKLM; Subkey: Software\Cygnus Solutions\Cygwin\Program Options; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: Software\CX; ValueType: string; ValueName: CX; ValueData: CX; Flags: uninsdeletevalue uninsdeletekeyifempty

Root: HKLM; Subkey: System\CurrentControlSet\Services\Eventlog\Application\tmansvc; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: System\CurrentControlSet\Services\Eventlog\Application\tmansvc; ValueType: string; ValueName: EventMessageFile; ValueData: {code:Getdircx}\bin\inmmessage.dll; Flags: uninsdeletevalue
Root: HKLM; Subkey: System\CurrentControlSet\Services\Eventlog\Application\tmansvc; ValueType: dword; ValueName: TypesSupported; ValueData: 7; Flags: uninsdeletevalue

Root: HKLM; Subkey: SYSTEM\CurrentControlSet\Services\Tcpip\Parameters; ValueType: dword; ValueName: Tcp1323Opts; ValueData: 3; Flags: uninsdeletevalue
Root: HKLM; Subkey: SYSTEM\CurrentControlSet\Services\Tcpip\Parameters; ValueType: dword; ValueName: GlobalMaxTcpWindowSize; ValueData: 4000000; Flags: uninsdeletevalue
Root: HKLM; Subkey: SYSTEM\CurrentControlSet\Services\Afd\Parameters; ValueType: dword; ValueName: DefaultReceiveWindow; ValueData: 1048576; Flags: uninsdeletevalue
Root: HKLM; Subkey: SYSTEM\CurrentControlSet\Services\Afd\Parameters; ValueType: dword; ValueName: DefaultSendWindow; ValueData: 1048576; Flags: uninsdeletevalue

Root: HKLM; Subkey: Software\SV Systems; Flags: uninsdeletekeyifempty
;Push Install registry entry
Root: HKLM; Subkey: Software\SV Systems\PushInstaller; Flags: uninsdeletekey; ValueType: String; ValueName: InstallDirectory; ValueData: {code:Getdircx}\pushinstallsvc;
Root: HKLM; Subkey: Software\SV Systems\PushInstaller; ValueType: string; ValueName: PIVersion; ValueData: {#Version}; Flags: uninsdeletevalue; AfterInstall: SetPermissions;

;Creating the CX Sub-Key to track the CX product Details
;CX is addressed as "9" Registry Key
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\9; Flags: uninsdeletekey;
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\9; ValueType: string; ValueName: Product_Name; ValueData: Caspian CX; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\9','Product_Name'); 
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\9; ValueType: string; ValueName: CsIpAddress; ValueData: 127.0.0.1; Flags: uninsdeletevalue; Check: not (PSOnlySelected) and (not UpgradeCheck)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\9; ValueType: string; ValueName: CsPort; ValueData: 443; Flags: uninsdeletevalue; Check: not (PSOnlySelected) and (not UpgradeCheck)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\9; ValueType: string; ValueName: CsProtocol; ValueData: https; Flags: uninsdeletevalue; Check: not (PSOnlySelected) and (not UpgradeCheck)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\9; ValueType: string; ValueName: InstallDirectory; ValueData: {code:Getdircx}; Flags: uninsdeletevalue; Check: (not UpgradeCheck)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\9; ValueType: dword; ValueName: CX_TYPE; ValueData: {code:Get_CX_TYPE}; Flags: uninsdeletevalue;

Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{code:SetEnv}"; Check: not UpgradeCheck; AfterInstall: MyAfterInstall('HKLM','SYSTEM\CurrentControlSet\Control\Session Manager\Environment','Path');
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: expandsz; ValueName: "PERL5LIB"; ValueData: "C:\strawberry\perl\site\lib"; Check: not UpgradeCheck; AfterInstall: MyAfterInstall('HKLM','SYSTEM\CurrentControlSet\Control\Session Manager\Environment','PERL5LIB');
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: expandsz; ValueName: "CYGWIN"; ValueData: "nodosfilewarning"; Check: not UpgradeCheck; AfterInstall: MyAfterInstall('HKLM','SYSTEM\CurrentControlSet\Control\Session Manager\Environment','CYGWIN');
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\SecurityProviders\SCHANNEL\Protocols\SSL 3.0"; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\SecurityProviders\SCHANNEL\Protocols\SSL 3.0"; ValueType: dword; ValueName: "DisabledByDefault"; ValueData: 1; AfterInstall: MyAfterInstall('HKLM','SYSTEM\CurrentControlSet\Control\SecurityProviders\SCHANNEL\Protocols\SSL 3.0','DisabledByDefault');
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\SecurityProviders\SCHANNEL\Protocols\SSL 3.0"; ValueType: dword; ValueName: "Enabled"; ValueData: 0; AfterInstall: MyAfterInstall('HKLM','SYSTEM\CurrentControlSet\Control\SecurityProviders\SCHANNEL\Protocols\SSL 3.0','Enabled');

; ProcessServer Registry Keys
Root: HKLM64; Subkey: Software\Microsoft\Azure Site Recovery Process Server; Flags: uninsdeletekey; Check: IsWin64;
Root: HKLM64; Subkey: Software\Microsoft\Azure Site Recovery Process Server; ValueType: string; ValueName: "CS Mode"; ValueData: LegacyCS; Flags: uninsdeletevalue; Check: IsWin64;

[INI]                    
Filename: {code:Getdircx}\transport\cxps.conf; Section: cxps; Key: remap_full_path_prefix; String: {code:GetRemapPath}; Check: not UpgradeCheck
Filename: {code:Getdircx}\transport\cxps.conf; Section: cxps; Key: cs_use_secure; String: yes; Check: not UpgradeCheck
Filename: {code:Getdircx}\transport\cxps.conf; Section: cxps; Key: ssl_port; String: {code:GetSecurePort}; Check: not UpgradeCheck
#ifdef DEBUG
Filename: {code:Getdircx}\transport\cxps.conf; Section: cxps; Key: monitor_log_enabled; String: yes; Check: not UpgradeCheck
Filename: {code:Getdircx}\transport\cxps.conf; Section: cxps; Key: monitor_log_rotate_count; String: 3; Check: not UpgradeCheck
#endif

Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: max_execution_time; String: 600; Flags: uninsdeleteentry; 
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: default_charset; String: utf-8; Flags: uninsdeleteentry; 
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: log_errors; String: On; Check: not PSOnlySelected;
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: extension_dir; String: C:\thirdparty\php5nts\ext;
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP_MBSTRING; Key: extension; String: C:\thirdparty\php5nts\ext\php_mbstring.dll; 

Filename: {code:Getdircx}\pushinstallsvc\pushinstaller.conf; Section: PushInstaller.transport; Key: Port; String: 443; Flags: createkeyifdoesntexist; Check: not UpgradeCheck; Languages: 
Filename: {code:Getdircx}\pushinstallsvc\pushinstaller.conf; Section: PushInstaller.transport; Key: Hostname; String: {code:GetIPaddress}; Flags: createkeyifdoesntexist; Check: not UpgradeCheck; Languages: 
Filename: {code:Getdircx}\pushinstallsvc\pushinstaller.conf; Section: PushInstaller.transport; Key: Https; String: 0; Flags: createkeyifdoesntexist; Check: not UpgradeCheck; Languages:
Filename: {code:Getdircx}\pushinstallsvc\pushinstaller.conf; Section: PushInstaller.transport; Key: SignatureVerificationChecks; String: 0; Flags: createkeyifdoesntexist; Check: not UpgradeCheck; Languages:
Filename: {code:Getdircx}\pushinstallsvc\pushinstaller.conf; Section: PushInstaller; Key: LogLevel; String: 7; Languages: 
Filename: {code:Getdircx}\pushinstallsvc\pushinstaller.conf; Section: PushInstaller; Key: IsVmWareBasedPushInstallEnabled; String: 1; Languages: 

[Run]
Filename: {code:Getdircx}\Unconfigure_IIS.bat; Flags: shellexec waituntilterminated runhidden; Check: not UpgradeCheck; StatusMsg: "Configuring IIS..."

Filename: {code:Getdircx}\bin\genpassphrase.exe; Flags: runhidden; Check: (not UpgradeCheck) and (not (PassphraseFileExist)) and (not (PSOnlySelected)); StatusMsg: "Generating passphrase..."
Filename: {code:Getdircx}\bin\genpassphrase.exe;Parameters: -k ; Flags: runhidden; Check: (not UpgradeCheck) and (not (EncryptionFileExist)); StatusMsg: "Generating encryption key..."

Filename: {cmd}; Parameters: "/C powershell.exe -File ""{code:Getdircx}\Remove_Binding.ps1"""; Check: (not UpgradeCheck) and (IsWin64 or IsItanium64); Flags: runhidden 64bit waituntilterminated; StatusMsg: "Configuring IIS..."

Filename: {code:Getdircx}\transport\cxps.exe; Parameters:" -c ""{code:Getdircx}\transport\cxps.conf"" install"; Flags: runhidden; Check: (not (UpgradeCheck)); StatusMsg: "Installing cxps service..."
Filename: C:\Temp\configure.bat; Parameters: "{code:ProcessServerConfiguration} ""{code:Getdircx}"""; Flags: shellexec waituntilterminated ; Check: (not (UpgradeCheck)); AfterInstall : RunPushinstallBinary; StatusMsg: " "
; this should be executed for fresh install only
Filename: {code:Getdircx}\bin\tmansvc.exe; Parameters: /service; flags: runhidden; Check: (not (UpgradeCheck)) ; AfterInstall : CheckService('tmansvc'); StatusMsg: "Installing tmansvc service..."
Filename: {code:Getdircx}\bin\ProcessServer.exe; Parameters: /i; flags: runhidden; Check: (not (ServiceInstallationCheck('ProcessServer'))) ; AfterInstall : CheckService('ProcessServer'); StatusMsg: "Installing ProcessServer service..."
Filename: {code:Getdircx}\bin\ProcessServerMonitor.exe; Parameters: /i; flags: runhidden; Check: (not (ServiceInstallationCheck('ProcessServerMonitor'))) ; AfterInstall : CheckService('ProcessServerMonitor'); StatusMsg: "Installing ProcessServerMonitor service..."

;Running Push Install exe
Filename: {code:Getdircx}\pushinstallsvc\InmLSA.exe; Parameters: {code:GetUserNameValue}; Flags: runhidden; Check: (not UpgradeCheck) and (CheckPushInstallService); StatusMsg: "Installing pushinstall service..."

; should not hard code app dir
Filename: C:\Temp\Delete_Default_Site.bat;Parameters:" -d ""{code:Getdircx}\admin\web"""; Flags: shellexec waituntilterminated runhidden; Check: ((not (UpgradeCheck)) and (not (PSOnlySelected))); StatusMsg: "Configuring IIS..."
Filename: C:\Temp\Configuration_IIS.bat; Flags: shellexec waituntilterminated runhidden; Check: ((not (UpgradeCheck)) and (not (PSOnlySelected))); StatusMsg: "Configuring IIS..."
Filename: {code:Getdircx}\bin\gencert.exe; Parameters:" -n cs -i -C ""US"" --ST ""Washington"" -L ""Redmond"" -O ""Microsoft"" --OU ""InMage"" --CN ""Scout"""; Check: (not UpgradeCheck) and ((IsWin64 or IsItanium64) and (not (PSOnlySelected))); Flags: runhidden 64bit waituntilterminated; StatusMsg: "Generating certificates..."
Filename: {code:Getdircx}\bin\gencert.exe; Parameters:" -n ps --dh -C ""US"" --ST ""Washington"" -L ""Redmond"" -O ""Microsoft"" --OU ""InMage"" --CN ""Scout"""; Check: (not UpgradeCheck) and (IsWin64 or IsItanium64); Flags: runhidden 64bit waituntilterminated; StatusMsg: "Generating certificates..."
;Configuring IIS to be accessible over https
Filename: {cmd}; Parameters: "/C ""{code:GetPowershellPath}\powershell.exe"" -c C:\Temp\Site_Secure_Config.ps1 > %systemdrive%\Temp\iis_https.log"; Check: (not UpgradeCheck) and ((IsWin64 or IsItanium64) and (not (PSOnlySelected))); Flags: runhidden 64bit waituntilterminated; AfterInstall: IIS_https_Log; StatusMsg: "Configuring IIS..."
Filename: C:\strawberry\perl\bin\ppm.bat; Parameters: install {code:GetRRDPath}\rrdtool\bindings\perl-shared\RRDs.ppd; Flags: shellexec waituntilterminated runhidden; StatusMsg: " "
Filename: {cmd}; Parameters: "/C ""{code:GetPowershellPath}\powershell.exe"" Set-WebConfiguration //System.webServer/Modules -metadata overrideMode -value Allow -PSPath IIS:/"; Check: (not UpgradeCheck) and ((IsWin64 or IsItanium64) and (not (PSOnlySelected))); Flags: runhidden 64bit waituntilterminated; StatusMsg: "Configuring IIS..."
;Opening port for firewall.
Filename: {cmd}; Parameters: "/C netsh advfirewall firewall add rule name=""Open Port 443"" dir=in action=allow protocol=TCP localport=443"; Check: (not UpgradeCheck) and (IsWin64 or IsItanium64); Flags: runhidden 64bit; StatusMsg: "Configuring firewall ports..."
Filename: {cmd}; Parameters: "/C netsh advfirewall firewall add rule name=""Open Port {code:GetSecurePort}"" dir=in action=allow protocol=TCP localport={code:GetSecurePort}"; Check: (not UpgradeCheck) and (IsWin64 or IsItanium64); Flags: runhidden 64bit; StatusMsg: "Configuring firewall ports..."

; Changing the recovery options of W3SVC service. (Changing the first and second failure options to restart the service.) 
Filename: {cmd}; Parameters: "/C ""{code:GetPowershellPath}\powershell.exe"" {sd}\Windows\System32\sc.exe failure W3SVC reset= 0 actions= restart/60000/restart/60000//"; Check: (not UpgradeCheck) and (IsWin64 or IsItanium64); Flags: runhidden 64bit waituntilterminated; StatusMsg: "Configuring W3SVC service..."

; Install Log upload service.
Filename: {code:Getdircx}\bin\LogUploadService.exe; Parameters: /i; Flags: runhidden; Check: (not ServiceInstallationCheck('LogUpload')); StatusMsg: "Installing Log upload service..."

[UninstallRun]
Filename: {code:Getdircx}\bin\Stop_CX_Services.bat; Flags: shellexec waituntilterminated runhidden
Filename: {code:Getdircx}\bin\tmansvc.exe; Parameters: /unregserver ; flags: runhidden;
Filename: {code:Getdircx}\bin\ProcessServer.exe; Parameters: /u ; flags: runhidden;
Filename: {code:Getdircx}\bin\ProcessServerMonitor.exe; Parameters: /u ; flags: runhidden;
Filename: {code:Getdircx}\bin\LogUploadService.exe; Parameters: /u ; flags: runhidden;
Filename: {code:Getdircx}\pushinstallsvc\PushInstall.exe; Parameters: -u ; flags: runhidden;
Filename: {code:Getdircx}\transport\cxps; Parameters:" -c ""{code:Getdircx}\transport\cxps.conf"" uninstall" ; flags: runhidden;
Filename: {cmd}; Parameters: "/C netsh advfirewall firewall delete rule name=""Open Port {code:GetSecurePort}"" "; Check: (IsWin64 or IsItanium64); Flags: runhidden 64bit;
Filename: {cmd}; Parameters: "/C netsh advfirewall firewall delete rule name=""Open Port 443"" "; Check: (IsWin64 or IsItanium64); Flags: runhidden 64bit;

Filename: {cmd}; Parameters: "/C powershell.exe -File ""{code:Getdircx}\Remove_Binding.ps1"""; Check: (IsWin64 or IsItanium64); Flags: runhidden 64bit waituntilterminated;
Filename: {cmd}; Parameters: "/C %windir%\system32\inetsrv\appcmd.exe delete site ""Default Web Site"""; Check: (IsWin64 or IsItanium64) and (not PSOnlySelected); Flags: runhidden 64bit waituntilterminated;
Filename: {code:Getdircx}\bin\removefiles.bat; Parameters: "UCX" ; flags: runhidden;

[UninstallDelete]
Type: filesandordirs; Name: {code:Getdircx}
Type: filesandordirs; Name: {code:Getdircx}\admin\web
Name: {code:Getdircx}\pushinstallsvc\pushinstaller.conf; Type: filesandordirs;
Name: {code:Getdircx}\pushinstallsvc; Type: filesandordirs;

[Code]

var
	ServerChoicePage: TInputOptionWizardPage;
	CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterServerChoicePage_Https : TInputQueryWizardPage;
	UpgradeSelected,UpgradeCX,RemoveServer,AddServer,Message : AnsiString;
  ResultCode : Integer;
  str1,str2,mcLogFile,GetOSVersion : String;
	Version: TWindowsVersion;
	FORK_MONITOR_PAIRS_Value,Volsync_value : String;	
  CX_TYPE_Value : String;
	CSONLY_CS_IP_Value,CSONLY_CS_Port_Value,PSONLY_PS_IP_Value,PSONLY_PS_CS_IP_Value,PSONLY_PS_CS_PORT_Value, PSONLY_PS_NIC_Value, CSONLY_AZURE_IP_Value : String;
  CSPS_CS_IP_Value,CSPS_CS_Port_Value,CSPS_PS_IP_Value,CSPS_PS_CS_IP_Value,CSPS_PS_CS_PORT_Value : String;
	PSONLY_PS_CS_SECURED_COMMUNICATION, PSONLY_PS_Https_Value : String;
	CSONLY_CS_SECURED_COMM, CSONLY_CS_Https_Value, CS_SECURED_MODE, CSONLY_Https_Port_Value : String;
	CSPS_Https_Port_Value, CSPS_CS_SECURED_COMM, CSPS_PS_CS_SECURED_COMM, CSPS_SECURED_MODE, CSPS_CS_Https_Value : String;
	OldState: Boolean;
  ErrorCode: Integer;
  Installation_StartTime : String;
  RRD_Message, Cygwin_Message, MySQL_Message : String;
  License_Page_Link : TLabel;
	Passphrase_Text, SQlDetailsPage_Note, Disable_Text : TNewStaticText;
	CheckBox: TNewCheckBox;
  RRDTOOL_Input_PATH, Cygwin_Input_PATH, MySQL_Input_PATH, AZUREIP : String;
  ServerMode,MySQLRootPass,MySQLUserPass,MySQLPasswordFile,CSIP,CSPort,PSIP,UserName,DomainName,Password,PassphrasePath, Cred_File, ConnectionType, Language, DataTransferSecurePort : String;
  RichEditViewer: TRichEditViewer;
  cancelclick: Boolean;
  Passphrase_Value, PS_CS_IP_Value, PS_CS_PORT_Value, PSOnly_CSDetailsPagePortString : String;
  LogFileString,PassphraseString : AnsiString;
  SQlDetailsPage : TInputQueryWizardPage;
  ProgressPage: TOutputProgressWizardPage;
  MySQL_License_Page : TWizardPage;
  MySQL_License_Page_AcceptDesc, MySQL_License_Page_Description, MySQL_License_Page_MySQL_Name : TNewStaticText;
  MySQL_License_Page_MySQL_ViewLicense, MySQL_License_Page_MySQL_DownloadLink : TLabel;
  ConnectionStatus: DWORD;
  Azure_Reg_Page : TInputFileWizardPage;
  DeclineButton, AcceptButton: TButton;
  MySQLLogFileString : AnsiString;
  LoopCount : Integer;
  ExitInstall, PassphraseValue : String;
  EnvPage: TWizardPage;
  vmware, nonvmware : TNewRadioButton;
  PageText, CSFinishPageText, CSFinishPageLink, CSFinishPageEndText,EnvPageText : TNewStaticText;
  DriveListText, FreeSpaceText : TNewStaticText;
  DriveSpace: TNewEdit;
  DrivePath : String;
  FreeSpaceinMB, FreeSpaceinGB, TotalSpace: Cardinal;
  Localization_Page : TWizardPage; 
  LanguageName : TNewComboBox;
  Localization_PageText : TNewStaticText;
  Language_Selected, Language_Code, CX_TP_Version : String;
  Language_Name_Entries : TArrayOfString;
  iLineCounter : Integer;
  PSMultipleNicsPage : TWizardPage;
  PSMultipleNicsPage_StaticText_NicSelection  : TNewStaticText;
  PSMultipleNicsPage_CheckListBox: TNewCheckListBox;
	a_strTextfile : TArrayOfString;
	Nic_Section_Name,Nic_Name,IP,IPString,Nic_IP_Count : String;
	Nic_StringList : TStringList;    
  Total_IP_Count: integer;
  rebootReq : String;
  Count : Integer;
  Actual_Install_Path, Install_Path, Install_Dir : String;
  BuildVersion, CurrentBuildVersion, BuildNumber, CurrentBuildNumber, BuildTag, PartnerCode : String;
  StringList : TStringList;
  RenameSuccess, CSMachineIP : String;
  
function SetEnvironmentVariable(lpName: string; lpValue: string): BOOL;
  external 'SetEnvironmentVariableW@kernel32.dll stdcall';
Procedure  ExitProcess(exitCode:integer);
external 'ExitProcess@kernel32.dll stdcall';

const
  SMTO_ABORTIFHUNG = 2;
  WM_WININICHANGE = $001A;
  WM_SETTINGCHANGE = WM_WININICHANGE;

type
  WPARAM = UINT_PTR;
  LPARAM = INT_PTR;
  LRESULT = INT_PTR;

function SendTextMessageTimeout(hWnd: HWND; Msg: UINT;
  wParam: WPARAM; lParam: PAnsiChar; fuFlags: UINT;
  uTimeout: UINT; out lpdwResult: DWORD): LRESULT;
  external 'SendMessageTimeoutA@user32.dll stdcall';   
   
const  
  LOGON32_LOGON_INTERACTIVE = 2;
  LOGON32_LOGON_NETWORK = 3;
  LOGON32_LOGON_BATCH = 4;
  LOGON32_LOGON_SERVICE = 5;
  LOGON32_LOGON_UNLOCK = 7;
  LOGON32_LOGON_NETWORK_CLEARTEXT = 8;
  LOGON32_LOGON_NEW_CREDENTIALS = 9;

  LOGON32_PROVIDER_DEFAULT = 0;
  LOGON32_PROVIDER_WINNT40 = 2;
  LOGON32_PROVIDER_WINNT50 = 3;

  ERROR_SUCCESS = 0;
  ERROR_LOGON_FAILURE = 1326;
  
  function LogonUser(lpszUsername, lpszDomain, lpszPassword: string; dwLogonType, dwLogonProvider: DWORD; var phToken: THandle): BOOL;
  external 'LogonUserW@advapi32.dll stdcall';
    

// Function to delete files
function CustomDeleteFile(FullPathFileName: String) : Boolean;
var
  LoopCount : Integer;
begin
  LoopCount := 1;
  while (LoopCount <= 3) do
  begin
      if Fileexists(FullPathFileName) then
      begin
          if DeleteFile(FullPathFileName) then
          begin
              Log('Successfully deleted the ' + FullPathFileName + ' file.');
              Result := True;
              break;
          end
          else
          begin
              Log('Unable to delete ' + FullPathFileName + ' file.');
              LoopCount := LoopCount + 1;
              Sleep(20000);
              Result := False;
          end;            
      end
      else
      begin
          Log('' + FullPathFileName + ' file doesnot exist.');
          Result := True;
          break;
      end;
  end;
end;

// Function to delete directory.
function CustomDeleteDirectory(DirectoryPath: String): Boolean;
var
  LoopCount : Integer; 
begin
  LoopCount := 1;
  while (LoopCount <= 3) do
  begin
      if DirExists(DirectoryPath) then
      begin
          if DelTree(DirectoryPath, True, True, True) then
          begin
              Log('Successfully deleted the ' + DirectoryPath + ' folder.');
              Result := True;
              break;
          end
          else
          begin
              Log('Unable to delete ' + DirectoryPath + ' folder.');
              LoopCount := LoopCount + 1;
              Sleep(60000);
              Result := False;
          end;            
      end
      else
      begin
          Log('' + DirectoryPath + ' folder doesnot exist.');
          Result := True;
          break;
      end;
  end;
end;

// Function to create directory.
function CustomCreateDirectory(DirectoryPath: String): Boolean;
var
  LoopCount : Integer; 
begin
  LoopCount := 1;
  while (LoopCount <= 3) do
  begin
      if not DirExists(DirectoryPath) then
      begin
          if CreateDir(DirectoryPath) then
          begin
              Log('Successfully created the ' + DirectoryPath + ' folder.');
              Result := True;
              break;
          end
          else
          begin
              Log('Unable to create ' + DirectoryPath + ' folder.');
              LoopCount := LoopCount + 1;
              Sleep(60000);
              Result := False;
          end;            
      end
      else
      begin
          Log('' + DirectoryPath + ' folder already exists.');
          Result := True;
          break;
      end;
  end;
end;

// Function to start a service in loop.
function ServiceStartInLoop(ServiceName: AnsiString): Boolean;
var
  WaitLoopCount, LoopCount : Integer; 
begin
  if IsServiceInstalled(ServiceName) then
  begin
      Log(''+ ServiceName +' service status - '+ ServiceStatusString(ServiceName) +'');
      if not IsServiceRunning(ServiceName) then
      begin
          LoopCount := 1;
          while (LoopCount <= 3) do
          begin
              Log('Starting '+ ServiceName +' service.');
              StartService(ServiceName);
              WaitLoopCount := 1;
              while (WaitLoopCount <= 9) and (ServiceStatusString(ServiceName)<>'SERVICE_RUNNING') do
              begin
                Sleep(20000);
                WaitLoopCount := WaitLoopCount + 1;
              end;
              
              if ServiceStatusString(ServiceName)='SERVICE_RUNNING' then
              begin
                  Log(''+ ServiceName +' service started successfully.');
                  Result := True;
                  break;
              end
              else
              begin
                  Sleep(20000);
                  Log('Unable to start '+ ServiceName +' service.');
                  LoopCount := LoopCount + 1;
                  Result := False;
              end;
          end;
      end
      else
      begin
          Log(''+ ServiceName +' service is already running.');
          Result := True;      
      end;
  end
  else
  begin
      Log(''+ ServiceName +' service is not installed.');
      Result := True;
  end;
end;

// Function to stop a service in loop.
function ServiceStopInLoop(ServiceName: AnsiString): Boolean;
var
  WaitLoopCount, LoopCount : Integer; 
begin
  if IsServiceInstalled(ServiceName) then
  begin
  Log(''+ ServiceName +' service status - '+ ServiceStatusString(ServiceName) +'');
      if IsServiceRunning(ServiceName) then
      begin
          LoopCount := 1;
          while (LoopCount <= 3) do
          begin
              Log('Stopping '+ ServiceName +' service.');
              StopService(ServiceName);
              WaitLoopCount := 1;
              while (WaitLoopCount <= 9) and (ServiceStatusString(ServiceName)<>'SERVICE_STOPPED') do
              begin
                Sleep(20000);
                WaitLoopCount := WaitLoopCount + 1;
              end;
              
              if ServiceStatusString(ServiceName)='SERVICE_STOPPED' then
              begin
                  Log(''+ ServiceName +' service stopped successfully.');
                  Result := True;
                  break;
              end
              else
              begin
                  Sleep(20000);
                  Log('Unable to stop '+ ServiceName +' service.');
                  LoopCount := LoopCount + 1;
                  Result := False;
              end;
          end;
      end
      else
      begin
          Log(''+ ServiceName +' service is not running.');
          Result := True;      
      end;
  end
  else
  begin
      Log(''+ ServiceName +' service is not installed.');
      Result := True;
  end;
end;

function GetDefaultDirName(Param: String) : String;
var dirpath : String;
begin
		dirpath :=  ExpandConstant('{pf}\{#INSTALLPATH}\home\svsystems');
		Result := dirpath;
end;

function Getdircx(Param: String) : String;
var dirpath : String;
begin
    if RegKeyExists(HKLM,'Software\InMage Systems\Installed Products\9') and IsServiceInstalled('tmansvc') then
    begin
        RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\9','InstallDirectory',dirpath);
        Result := dirpath;
    end
    else
    begin
        dirpath :=  ExpandConstant('{app}');
        Result := dirpath;
    end;
end;

function IsItanium64 : Boolean;
begin
	if ProcessorArchitecture = paIA64 then
		Result := True
	else
		Result := False;
end;

function UpgradeCheck : Boolean;
begin
	if UpgradeCX = 'IDYES' then
		Result := True
	else
		Result := False;
end;

Function ExitInstallation(): Boolean; 
var 
    LogFileString : AnsiString;
begin
    Log('In the function Writing ExitInstallation.' + Chr(13));

    CustomDeleteFile(ExpandConstant('{sd}\Temp\PageValues.conf'));

    if not UpgradeCheck then
    begin
        CustomDeleteDirectory(ExpandConstant('{code:Getdircx}'));
        
        if DelTree(ExpandConstant('C:\Temp\*'), False, True, True) then
            Log('Successfully deleted the C:\Temp contents.')
        else
            Log('Unable to delete the C:\Temp contents.');
    end;
  			         
  	FileCopy(ExpandConstant ('{log}'), ExpandConstant ('{sd}\InstallLogFile.log'), FALSE);
    LoadStringFromFile(ExpandConstant ('{sd}\InstallLogFile.log'), LogFileString);    
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_InstallLogFile.log'),'Installation Starts here '+ Installation_StartTime + #13#10 + '*********************************************', True);
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_InstallLogFile.log'),#13#10 + LogFileString , True);
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_InstallLogFile.log'), #13#10 + '*********************************************' + #13#10 + 'Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
    CustomDeleteFile(ExpandConstant('{sd}\InstallLogFile.log'));	
end;

function GetMySQLDataPath(Param: String) : String;
begin
		Result := GetIniString( 'Install_Paths', 'MySQL_Data_Path','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );  
end;

function GetPowershellPath(Param: String) : String;
begin
		  Result := ExpandConstant('{sd}\Windows\System32\WindowsPowerShell\v1.0');
end;

function MachineIPAddress(Param: String) : String;
begin
	ShellExec('', ExpandConstant('{cmd}'),'/C powershell "Get-WmiObject Win32_NetworkAdapterConfiguration -WarningAction SilentlyContinue | ? {$_.IPEnabled} | select -ExpandProperty IPAddress | select -First 1" > '+ExpandConstant('{sd}\Temp\IPAddress.txt'),'', SW_HIDE, ewWaitUntilTerminated,Errorcode);
  	LoadStringFromFile(ExpandConstant('{sd}\Temp\IPAddress.txt'), LogFileString);
	Result := LogFileString;
end;

function MachineDescription(Param: String) : String;
begin
	ShellExec('', ExpandConstant('{cmd}'),'/C powershell "Get-WmiObject Win32_NetworkAdapterConfiguration -WarningAction SilentlyContinue | ? {$_.IPEnabled} | select -ExpandProperty Description | select -First 1" > '+ExpandConstant('{sd}\Temp\NicName.txt'),'', SW_HIDE, ewWaitUntilTerminated,Errorcode);
	LoadStringFromFile(ExpandConstant('{sd}\Temp\NicName.txt'), LogFileString);
	Result := LogFileString;
end;

function IPDescription(IPName: String) : String;
begin
	ShellExec('', ExpandConstant('{cmd}'),'/C powershell "Get-WmiObject Win32_NetworkAdapterConfiguration | ? {$_.IPAddress -Match """'+IPName+'"""} | select -ExpandProperty Description" > '+ExpandConstant('{sd}\Temp\NicName.txt'),'', SW_HIDE, ewWaitUntilTerminated,Errorcode);
	LoadStringFromFile(ExpandConstant('{sd}\Temp\NicName.txt'), LogFileString);
	Result := LogFileString;
end;

procedure MySQLViewLicenseLinkClick(Sender: TObject);
var
  ErrorCode: Integer;
begin
  ShellExec('', 'http://go.microsoft.com/fwlink/?LinkId=524803', '', '', SW_SHOW, ewNoWait,ErrorCode);
end;

procedure FinishPageLinkClick(Sender: TObject);
var
  ErrorCode: Integer;
begin
  ShellExec('', 'http://go.microsoft.com/fwlink/?LinkID=613142', '', '', SW_SHOW, ewNoWait,ErrorCode);
end;

procedure MySQLLinkClick(Sender: TObject);
var
  ErrorCode: Integer;
begin
  ShellExec('', 'http://go.microsoft.com/fwlink/?LinkId=524804', '', '', SW_SHOW, ewNoWait,ErrorCode);
end;

function LanguageCode() : String;
begin
    case Language_Selected of
        'English': 
            begin
                Language_Code := 'en-US';
            end;
        'Portuguese':
            begin
                Language_Code := 'cs-CZ';
            end;
        'German':
            begin
                Language_Code := 'de-DE';
            end;
        'Spanish': 
            begin
                Language_Code := 'es-ES';
            end;
        'French':
            begin
                Language_Code := 'fr-FR';
            end;
        'Hungarian':
            begin
                Language_Code := 'hu-HU';
            end;
        'Italian': 
            begin
                Language_Code := 'it-IT';
            end;
        'Japanese':
            begin
                Language_Code := 'ja-JP';
            end;
        'Korean':
            begin
                Language_Code := 'ko-KR';
            end;
        'Russian': 
            begin
                Language_Code := 'ru-RU';
            end;
        'Polish':
            begin
                Language_Code := 'pl-PL';
            end;
        'Portuguese (Brazil)':
            begin
                Language_Code := 'pt-BR';
            end;
        'Simplified Chinese': 
            begin
                Language_Code := 'zh-CN';
            end;
        'Traditional Chinese':
            begin
                Language_Code := 'zh-Hant';
            end;
    end;
    Result :=  Language_Code;        
end;

Procedure ChangeDataPath();
var
MySQL_Data_Path, MySQL_Path, MyINI_Path , MySQL_Binary : String;
begin
    MySQL_Data_Path := GetIniString( 'Install_Paths', 'MySQL_Data_Path','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
    MySQL_Path := GetIniString( 'Install_Paths', 'MySQL_Path','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
    MyINI_Path := ExpandConstant(MySQL_Data_Path+'my.ini');
    MySQL_Binary := ExpandConstant(MySQL_Path+'bin\mysqld.exe');

    // Stop MySQL service.     
    if not ServiceStopInLoop('MySQL') then
    begin
        ExitInstallation();
        ExitProcess(18);
    end;

    // kill the mysqld process.
    if (IsServiceInstalled('MySQL')) and (ServiceStatusString('MySQL')<>'SERVICE_STOPPED') then
    begin
        if ShellExec('', ExpandConstant('{cmd}'),'/C taskkill /F /IM mysqld.exe /T','', SW_HIDE, ewWaitUntilTerminated,Errorcode) then
            Log('Successfully killed the mysqld process.')
        else
        begin
            Log('Unable to kill the mysqld process. Contact Microsoft support.');
            ExitInstallation();
            ExitProcess(18);
        end;
    end;

    // Delete files.
    CustomDeleteFile(ExpandConstant(MySQL_Data_Path+'data\ib_logfile0'));
    CustomDeleteFile(ExpandConstant(MySQL_Data_Path+'data\ib_logfile1'));
    CustomDeleteFile(ExpandConstant(MySQL_Data_Path+'data\ibdata'));
    CustomDeleteFile(ExpandConstant(MySQL_Data_Path+'data\ibdata1'));
    CustomDeleteFile(ExpandConstant(MySQL_Data_Path+'data\ibdata2'));

    // Update datadir value in my.ini file.    
    if SetIniString( 'mysqld', 'datadir','"'+ExpandConstant(MySQL_Data_Path)+'data"',MyINI_Path) then
        Log('Datadir value is replaced successfully in my.ini file.' + Chr(13))
    else
    begin
        Log('datadir value has not been replaced in my.ini. Aborting installation.'+ Chr(13));
        ExitInstallation();
        ExitProcess(17);
    end;

    // Update basedir value in my.ini file.
    if SetIniString( 'mysqld', 'basedir','"'+ExpandConstant(MySQL_Path)+'"',MyINI_Path) then
        Log('Basedir value is replaced successfully in my.ini file.' + Chr(13))
    else
    begin
        Log('basedir value has not been replaced in my.ini. Aborting installation.'+ Chr(13));
        ExitInstallation();
        ExitProcess(17);
    end;
    
    // Install MySQL.      
    if not (IsServiceInstalled('MySQL')) then
    begin
        if IsWin64 then
        begin
            OldState := EnableFsRedirection(False);
            try
                Exec(ExpandConstant('{cmd}'), '/C ""'+MySQL_Binary+'" --install MySQL --defaults-file="'+MyINI_Path+'""','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
            finally
                EnableFsRedirection(OldState);
            end;
        end;

        if (IsServiceInstalled('MySQL')) then
        begin
            Log('MySQL service is installed after replacing values in my.ini file.' + Chr(13));
        end
        else
        begin
            Log('Unable to install MySQL service. Please retry with install. If issue persists even after retry, contact Microsoft support.' + Chr(13));
            ExitInstallation();
            ExitProcess(18);
        end;
    end;

    // Delete data folder.
    if not CustomDeleteDirectory(ExpandConstant(MySQL_Data_Path+'data')) then
    begin
        ExitInstallation();
        ExitProcess(18);
    end;

    // Create Uploads folder.
    if not CustomCreateDirectory(ExpandConstant(MySQL_Data_Path+'Uploads')) then
    begin
        ExitInstallation();
        ExitProcess(18);
    end;

    if IsWin64 then
    begin
        OldState := EnableFsRedirection(False);
        try
            Exec(ExpandConstant('{cmd}'), '/C ""'+MySQL_Binary+'" --defaults-file="'+MyINI_Path+'" --initialize --datadir="'+MySQL_Data_Path+'data""','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
        finally
            EnableFsRedirection(OldState);
        end;
    end;

    // Start MySQL service.
    if not ServiceStartInLoop('MySQL') then
    begin
        ExitInstallation();
        ExitProcess(18);
    end;
end;

function SetEnv(Param: String) : String;
var
MySQL_Path, Cygwin_Path, RRDTOOL_PATH, RRD_PATH, Path_Backup  : String;
begin
    MySQL_Path := GetIniString( 'Install_Paths', 'MySQL_Path','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
    Cygwin_Path := GetIniString( 'Install_Paths', 'Cygwin_Path','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
    RRDTOOL_PATH := GetIniString( 'Install_Paths', 'RRDTOOL_PATH','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
    RRD_PATH := GetIniString( 'Install_Paths', 'RRD_PATH','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
    Path_Backup := ExpandConstant('{code:Getdircx}\bin')+';'+ExpandConstant(MySQL_Path)+'lib;'+ExpandConstant(MySQL_Path)+'bin;'+ExpandConstant(RRDTOOL_PATH)+'\rrdtool\Release;'+ExpandConstant(RRD_PATH)+';'+ExpandConstant('C:\thirdparty\php5nts')+';'+ExpandConstant('C:\thirdparty\php5nts\ext')+';'+ExpandConstant('{sd}\windows')+';'+ExpandConstant('C:\strawberry\perl\bin')+';'+ExpandConstant('C:\strawberry\c\bin')+';'+ExpandConstant(Cygwin_Path)+'\bin;';
    RegWriteExpandStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\9','Install_Path_Backup',Path_Backup);
    
    Result := Path_Backup;
end;

procedure MyAfterInstall(Root,Subkey,ValueName : String);
begin

if (RegValueExists (HKLM, Subkey, ValueName)) then
    Log('In function MyAfterInstall - values found for  '+ Subkey + '\' + ValueName + Chr(13))
else
    Log('In function MyAfterInstall - values are not found for  '+ Subkey + '\' + ValueName + Chr(13));
end;

function PassphraseFileExist : Boolean;
begin
	if fileexists(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\private\connection.passphrase')) then
		Result := True
	else
		Result := False;
end;

function EncryptionFileExist : Boolean;
begin
	if fileexists(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\private\encryption.key')) then
		Result := True
	else
		Result := False;
end;

function FreshInstallationCheck : Boolean;
begin
	if UpgradeSelected = 'IDYES' then
		Result := False
	else
		Result := True;
end;

Function CheckPushInstallService() : Boolean;
begin
	if (IsServiceInstalled('InMage PushInstall')) and (ServiceStatusString('InMage PushInstall')='SERVICE_STOPPED') then
	   Result := True;
end;

// Function to check if service is installed or not.
function ServiceInstallationCheck(ServiceName: String) : Boolean;
begin
    if (IsServiceInstalled(ServiceName)) then
    begin
        Log('' + ServiceName +' service is installed.');
        Result := True;
    end
    else
    begin
        Log('' + ServiceName +' service is not installed.');
        Result := False;
    end;
end;

function GetUserNameValue(Param: String) : String;
var
LocalUserName,DomainName,UserName,BackwardSlashString : String;
begin
	DomainName:= ExpandConstant(GetEnv('USERDOMAIN'));
		UserName := ExpandConstant( +GetUserNameString);
		BackwardSlashString := '\'
		LocalUserName := DomainName + BackwardSlashString + UserName;
	Result := LocalUserName;
end;

Procedure StopIIS_Service();
var
  	LogFileString : AnsiString;
  	ErrorCode : Integer;
    OldState: Boolean;
begin
      
      //stopping the WAS service(internally stops IIS service)
      if IsWin64 then
      begin
      		        //Turn off redirection, so that cmd.exe from the 64-bit System directory is launched.
      		        OldState := EnableFsRedirection(False);
      		        try
            	         Exec(ExpandConstant('{cmd}'), '/c net stop WAS /Y  > %systemdrive%\StopIIS_Service.log 2>&1 ','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
      		        finally
      			           // Restore the previous redirection state.
      			      EnableFsRedirection(OldState);
      	       	  end;
      end;
      
      //Appending the log generated by net stop command into common installation Log.
    	if FileExists(ExpandConstant('{sd}\StopIIS_Service.log')) then	
	    begin
	       	LoadStringFromFile(ExpandConstant ('{sd}\StopIIS_Service.log'), LogFileString);    
	        Log(#13#10 +'Stop IIS Service log Starts here '+ #13#10 + '*********************************************');
	        Log(#13#10 + LogFileString);
	        Log(#13#10 + '*********************************************' + #13#10 + 'Stop IIS Service log Ends here :' +#13#10 + #13#10);
	        
	    end;
end;

procedure InitialSettings();
var
	j: Cardinal;

begin
	for j := 1 to ParamCount do
	if (CompareText(ParamStr(j),'/ServerMode') = 0) or (CompareText(ParamStr(j),'-ServerMode') = 0) and not (j = ParamCount) then
	begin
		ServerMode := ParamStr(j+1)
	end
	else if (CompareText(ParamStr(j),'/MySQLPasswordFile') = 0) or (CompareText(ParamStr(j),'-MySQLPasswordFile') = 0) and not (j = ParamCount) then
	begin
		MySQLPasswordFile := ParamStr(j+1)
	end
  else if (CompareText(ParamStr(j),'/DataTransferSecurePort') = 0) or (CompareText(ParamStr(j),'-DataTransferSecurePort') = 0) and not (j = ParamCount) then
	begin
		DataTransferSecurePort := ParamStr(j+1)
	end
	else if (CompareText(ParamStr(j),'/CSIP') = 0) or (CompareText(ParamStr(j),'-CSIP') = 0) and not (j = ParamCount) then
	begin
		CSIP := ParamStr(j+1)
	end
	else if (CompareText(ParamStr(j),'/CSPort') = 0) or (CompareText(ParamStr(j),'-CSPort') = 0) and not (j = ParamCount) then
	begin
		CSPort := ParamStr(j+1)
	end
  else if (CompareText(ParamStr(j),'/PSIP') = 0) or (CompareText(ParamStr(j),'-PSIP') = 0) and not (j = ParamCount) then
	begin
		PSIP := ParamStr(j+1)
	end
  else if (CompareText(ParamStr(j),'/AZUREIP') = 0) or (CompareText(ParamStr(j),'-AZUREIP') = 0) and not (j = ParamCount) then
	begin
		AZUREIP := ParamStr(j+1)
	end
	else if (CompareText(ParamStr(j),'/PassphrasePath') = 0) or (CompareText(ParamStr(j),'-PassphrasePath') = 0) and not (j = ParamCount) then
	begin
		PassphrasePath := ParamStr(j+1)
	end
	else if (CompareText(ParamStr(j),'/Language') = 0) or (CompareText(ParamStr(j),'-Language') = 0) and not (j = ParamCount) then
	begin
		Language := ParamStr(j+1)
	end;
end;

procedure IsCurrentAppRunning();
begin
     if (not (CheckForMutexes('UnifiedCXMutexName'))) then
           CreateMutex('UnifiedCXMutexName')            
     else
     begin
          While(CheckForMutexes('UnifiedCXMutexName')) do
          begin
              if WizardSilent then
              begin
                   Log('Another instance of this appliccation is already running! Hence, Aborting..');
                   RegWriteStringValue(HKLM,'Software\SV Systems\VxAgent','InstallExitCode','1');
                   Abort;
              end
              else if SuppressibleMsgBox('The installer is already running! To run installer again, you must first close existing installer process.',mbError, MB_OKCANCEL or MB_DEFBUTTON1, MB_OK) = IDCANCEL then
                	 Abort;  
          end;
          CreateMutex('UnifiedCXMutexName')
     end;   
end;

function GetValue(KeyName: AnsiString) : AnsiString ;
var
	Line, Value : AnsiString;
	iLineCounter, n : Integer;
	a_strTextfile : TArrayOfString;
begin
	if RegValueExists(HKLM,'Software\InMage Systems\Installed Products\9','Product_Name') and IsServiceInstalled('tmansvc') then
	begin
		LoadStringsFromFile(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'), a_strTextfile);

		Value := '';

		for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
		begin
		    if (Pos(KeyName, a_strTextfile[iLineCounter]) > 0) then
			begin
				Line := a_strTextfile[iLineCounter] ;
				n := Pos('=',Line);

				if n > 0 then
				begin
					Value := RemoveQuotes(Trim(Copy(Line,n+1,1000)));
					Result := Value;
				end else begin
					Result := '';
				end;
			end;
		end;
	end;
end;

function GetRRDPath(Param: String) : String;
begin
		if UpgradeCheck then
		begin
		    Result := GetValue('RRDTOOL_PATH');
		end
		else
		    Result := GetIniString( 'Install_Paths', 'RRDTOOL_PATH','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );  
end;

function PSOnlySelected : Boolean;
begin
  if UpgradeCheck then
  begin
      if (GetValue('CX_TYPE') = '2') then
      begin
              Result := True
      end
      else
      begin
          Result := False;
      end;
  end
  else
  begin
      if WizardSilent then
      begin
          InitialSettings;
          if (ServerMode = 'PS') or (ServerMode = 'ps') or (ServerMode = 'Ps')  then
          begin
              Result := True
          end
          else
          begin
              Result := False;
          end;
      end
      else if (ServerChoicePage.SelectedValueIndex = 1) then
      		Result := True
      	else
      		Result := False;
  end;
end;

function ProcessServerConfiguration(Param : String): String;
begin
  if WizardSilent then
  begin
      InitialSettings;
      if (ServerMode = 'PS') or (ServerMode = 'ps') or (ServerMode = 'Ps')  then
      begin
          Result := 'Process_server';
          Log('User has given the option ''/ServerMode PS''  in silent installation.')
      end
      else
      begin
          Result := 'Not_process_server';
      end;
  end
  else if (ServerChoicePage.SelectedValueIndex = 1) then begin
  	   	Result := 'Process_server';
  end else
	begin
	   Result := 'Not_process_server';
	end ;
end;

function AddOrReplaceLine(FileName, KeyString, NewString : AnsiString): Boolean;
var
	iLineCounter : Integer;
	a_strTextfile : TArrayOfString;
	ReplacedText : Boolean;
begin
	{ Load text file into an array of strings }
	LoadStringsFromFile(FileName, a_strTextfile);

	{ Look for the key AnsiString in the file and if found, replace that line with new AnsiString }
	for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do begin
		if (Pos(KeyString, a_strTextfile[iLineCounter]) = 1) then begin
			a_strTextfile[iLineCounter] := NewString;
			ReplacedText := True;
		end;
	end;

	if ReplacedText = True then begin
		SaveStringsToFile(FileName, a_strTextfile, False)
		Result := True;
	end else begin
		SaveStringToFile(FileName, #13#10 + NewString + #13#10, True);
		Result := True;
	end;
end;

function VerifyPreviousVersion : Boolean;
var
	a_strTextfile : TArrayOfString;
	iLineCounter : Integer;
begin
	Result := False;
	if RegKeyExists(HKLM,'Software\InMage Systems\Installed Products\9') and IsServiceInstalled('tmansvc') then
  begin
      if RegValueExists(HKLM,'Software\InMage Systems\Installed Products\9','Build_Version') then
      begin
          RegQueryStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\9','Build_Version',BuildVersion);
          StringChangeEx(BuildVersion,'.','',True);
          CurrentBuildVersion := '{#APPVERSION}';
          StringChangeEx(CurrentBuildVersion,'.','',True);
          if (StrToInt(CurrentBuildVersion) > StrToInt(BuildVersion)) then
          begin
              RegQueryStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\9','Build_Tag',BuildTag);
              Log('Already CX - ' + BuildTag + ' is installed' + Chr(13));
              Result := True;
          end
          else
              Result := False;
      end
      else                                     
      begin   
          RegQueryStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\9','Version',BuildVersion);
          RegQueryStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\9','Build_Number',BuildNumber);
          RegQueryStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\9','Partner_Code',PartnerCode);

          StringList := TStringList.Create;
          StringChangeEx(BuildVersion,'.',',',FALSE);
          StringList.CommaText :=BuildVersion;
          BuildVersion := (StringList.Strings[0])+(StringList.Strings[1])+BuildNumber+PartnerCode;

          CurrentBuildVersion := '{#APPVERSION}';
          StringChangeEx(CurrentBuildVersion,'.','',True);

          if (StrToInt(CurrentBuildVersion) > StrToInt(BuildVersion)) then
          begin
              RegQueryStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\9','Build_Tag',BuildTag);
              Log('Already CX - ' + BuildTag + ' is installed' + Chr(13));
              Result := True;
          end
          else
              Result := False;
      end;
  end;
end;

// Function to validate same or higher version of build is installed.
function VerifySameOrHigherVersion : Boolean;
begin
  Result := False;
	if RegKeyExists(HKLM,'Software\InMage Systems\Installed Products\9') and IsServiceInstalled('tmansvc') then
  begin
      if RegValueExists(HKLM,'Software\InMage Systems\Installed Products\9','Build_Version') then
      begin
          RegQueryStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\9','Build_Version',BuildVersion);
          Log('Installed build version - ' + BuildVersion + '' + Chr(13));
          StringChangeEx(BuildVersion,'.','',True);
          CurrentBuildVersion := '{#APPVERSION}';
          Log('Current build version - ' + CurrentBuildVersion + '' + Chr(13));
          RegQueryStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\9','Build_Tag',BuildTag);
          Log('CX - ' + BuildTag + ' is installed' + Chr(13));
          StringChangeEx(CurrentBuildVersion,'.','',True);
          if (StrToInt(CurrentBuildVersion) <= StrToInt(BuildVersion)) then
          begin
              Result := True;
          end;
      end;
  end;
end;

Procedure IIS_https_Log();
var
    IISLogFileString  : AnsiString;
begin
          	if FileExists(ExpandConstant('{sd}\Temp\iis_https.log')) then	
      	    begin
      	       	LoadStringFromFile(ExpandConstant ('{sd}\Temp\iis_https.log'), IISLogFileString);    
      	        Log(#13#10 +'IIS https configuration log Starts here '+ #13#10 + '*********************************************');
      	        Log(#13#10 + IISLogFileString);
      	        Log(#13#10 + '*********************************************' + #13#10 + 'IIS https configuration log Ends here :' +#13#10 + #13#10);
      	        CustomDeleteFile(ExpandConstant('{sd}\Temp\iis_https.log'));
      	    end;
end;

Function IsAgentInstalled : Boolean;
begin
    if RegKeyExists(HKLM, 'Software\InMage Systems\Installed Products\5') and IsServiceInstalled('svagents') then
    begin   
        Log('Microsoft Azure Site Recovery Mobility Service/Master Target Server is already installed on this machine.');
		Result := True;
    end
    else 
    begin
        Log('Microsoft Azure Site Recovery Mobility Service/Master Target Server is not installed on this machine.');
		Result := False;
    end;
end;

Procedure CheckService(Service: String);
begin
      if IsServiceRunning(Service) then
         Log('The service is running: '+ Service)else
		 Log('The service is not running: '+ Service)
end;

// Stop CX services.
function StopCXServices : Boolean;
begin
  Result := False;
  Log('Stopping CS services.');

  // Always stop ProcessServerMonitor service first to avoid generating alerts if other services are stopped during install/upgrade/uninstall.
  if ServiceStopInLoop('ProcessServerMonitor') then
  begin
	if ServiceStopInLoop('tmansvc') then
		begin
		if ServiceStopInLoop('ProcessServer') then
			begin
			if ServiceStopInLoop('cxprocessserver') then
				begin
				if ServiceStopInLoop('InMage PushInstall') then
					begin
					if ServiceStopInLoop('LogUpload') then
						begin
							Result := True;
						end;
					end;
				end;
			end;
		end;
  end; 
end;

// Start CX services.
Procedure StartCXServices();
begin
    ServiceStartInLoop('W3SVC');
    ServiceStartInLoop('cxprocessserver');
	ServiceStartInLoop('tmansvc');
	ServiceStartInLoop('ProcessServer');
	ServiceStartInLoop('InMage PushInstall');
    ServiceStartInLoop('LogUpload');
	// Always start ProcessServerMonitor service first to avoid generating alerts if other services are stopped first during install/upgrade/uninstall.
	ServiceStartInLoop('ProcessServerMonitor');
end;

function ValidateInput(InputStr, TypeOfValue: String) : Boolean;
var
	IPDotCount, DotPosition, Octet : Integer;
	InputStrCopy, DotPositionStr, OctetStr : String;
	IPFirstValue : TStringList;
begin
	InputStrCopy := InputStr + '.';
	if TypeOfValue = 'Port' then begin
		if (StrToIntDef(InputStr,1) = 1) or (InputStr = '') then begin
			Result := False;
		end else begin
			Result := True;
		end;
	end else if TypeOfValue = 'IP' then begin
		IPFirstValue := TStringList.Create;
		IPDotCount := StringChangeEx(InputStr,'.',',',False);
		IPFirstValue.CommaText :=InputStr;
		if IPDotCount <> 3 then begin
			Result := False;
		end else begin
			while Length(InputStrCopy) <> 0 do begin
				DotPosition := Pos('.',InputStrCopy);
				if DotPosition <> 0 then begin
					OctetStr := Copy(InputStrCopy,1,DotPosition-1);
					DotPositionStr := IntToStr(DotPosition);
					InputStrCopy := Copy(InputStrCopy,DotPosition + 1,1000);
				end else begin
					OctetStr := InputStrCopy;
					InputStrCopy := '';
				end;

				Octet := StrToIntDef(OctetStr,1000);
				if (OctetStr = '') or (Octet > 255) or (Octet = 1000) then begin
					Result := False;
					Break;
				end else begin
					Result := True;
				end;
			end;
		end;
		if not (InputStr = '') then
		begin
  		if (StrToInt(IPFirstValue.Strings[0]) = 0) then 
    	begin
    			Result := False;
    	end;
  	end;
	end else begin
		SuppressibleMsgBox('Invalid value for TypeOfValue. Please enter a correct value.',mbError, MB_OK, MB_OK);
		Result := False;
	end;

end;

function FileReplaceString(const FileName, SearchString, ReplaceString: string):boolean;
var
  MyFile : TStrings;
  MyText : string;
begin
  MyFile := TStringList.Create;
  try
    result := true;
    try
      MyFile.LoadFromFile(FileName);
      MyText := MyFile.Text;

      if StringChangeEx(MyText, SearchString, ReplaceString, True) > 0 then
      begin;
        MyFile.Text := MyText;
        MyFile.SaveToFile(FileName);
      end;
    except
      result := false;
    end;
  finally
    MyFile.Free;
  end;
end;

procedure Verifydatabase ();
var
  ErrorCode: Integer;
  MySQL_Root_Password, MySQL_User_Password, MySQL_Path, MySQL_Data_Path, MySQL_Binary, MySQLInitFile, MySQL_Install_Binary, MyINI_Path, MySQLD_Binary : String;
begin
	if not PSOnlySelected then
	begin
    	MySQL_Root_Password := GetIniString('MPWD', 'MySQL_Root_Password','', ExpandConstant('{sd}\Temp\PageValues.conf'));  
    	MySQL_User_Password := GetIniString('MPWD', 'MySQL_User_Password','', ExpandConstant('{sd}\Temp\PageValues.conf'));
      MySQL_Data_Path := GetIniString( 'Install_Paths', 'MySQL_Data_Path','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
    	MySQL_Path := GetIniString( 'Install_Paths', 'MySQL_Path','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
      MyINI_Path := ExpandConstant(MySQL_Data_Path+'my.ini');
      MySQLD_Binary := ExpandConstant(MySQL_Path+'bin\mysqld.exe');
      MySQL_Install_Binary := ExpandConstant(MySQL_Path+'bin\mysql.exe');
      MySQLInitFile := ExpandConstant('C:\Temp\mysql-init.txt');

    	if FileReplaceString((ExpandConstant('C:\Temp\create_user.sql')), 'user-defined', MySQL_User_Password) then
          Log('user-defined value has been replaced in create_user.sql'+ Chr(13))
       else
       begin  
          Log('user-defined value has not been replaced in create_user.sql. Aborting installation.'+ Chr(13));
          ExitInstallation();
          ExitProcess(20);
       end;
       if FileReplaceString(MySQLInitFile, 'user-defined', MySQL_Root_Password) then
          Log('user-defined value has been replaced in '+ MySQLInitFile)
       else
       begin  
          Log('user-defined value has not been replaced in '+MySQLInitFile+'. Aborting installation.'+ Chr(13));
          ExitInstallation();
          ExitProcess(20);
       end;
      Actual_Install_Path := ExpandConstant('{code:Getdircx}');
      StringChangeEx(Actual_Install_Path,'\home\svsystems','',True);
      Install_Path := Actual_Install_Path;
      Install_Dir := Actual_Install_Path;
      StringChangeEx(Install_Path,'\','/',True);
      StringChangeEx(Install_Dir,'\','\\',True);
      StringChangeEx(MySQLInitFile,'\','\\',True);

      if FileReplaceString((ExpandConstant('C:\Temp\svsdb.sql')), 'CX_INSTALL_PATH_FORWARDSLASH', Install_Path) then
          Log('Install-Path value has been replaced in svsdb.sql'+ Chr(13))
      else
      begin  
          Log('Install-Path value has not been replaced in svsdb.sql. Aborting installation.'+ Chr(13));
          ExitInstallation();
          ExitProcess(21);
      end;

      if FileReplaceString((ExpandConstant('C:\Temp\svsdb.sql')), 'CX_INSTALL_PATH_BACKSLASH', Install_Dir) then
          Log('Install-Dir value has been replaced in svsdb.sql'+ Chr(13))
      else
      begin  
          Log('IInstall-Dir value has not been replaced in svsdb.sql. Aborting installation.'+ Chr(13));
          ExitInstallation();
          ExitProcess(21);
      end; 
      
      // Stop MySQL service.
      if not ServiceStopInLoop('MySQL') then
      begin
          ExitInstallation();
          ExitProcess(22);
      end;

      ShellExec('', ExpandConstant('{cmd}'),'/C ""'+MySQLD_Binary+'" --defaults-file="'+MyINI_Path+'" --datadir="'+MySQL_Data_Path+'data"" --init-file='+MySQLInitFile,'', SW_HIDE, ewNoWait,ResultCode);
      Sleep(10000);

      // Kill mysqld process.
      if ShellExec('', ExpandConstant('{cmd}'),'/C taskkill /F /IM mysqld.exe /T','', SW_HIDE, ewWaitUntilTerminated,Errorcode) then
          Log('Successfully killed the mysqld process.')
      else
      begin
          Log('Unable to kill the mysqld process.');
          ExitInstallation();
          ExitProcess(22);
      end;

      // Start MySQL service.
      if not ServiceStartInLoop('MySQL') then
      begin
          ExitInstallation();
          ExitProcess(22);
      end;
        
      Log('Invoking verifydb.bat script.');
      if ShellExec('', ExpandConstant('C:\Temp\verifydb.bat'),MySQL_Root_Password+' '+MySQL_User_Password+' "'+MySQL_Install_Binary+'" "'+ExpandConstant('{code:Getdircx}')+'"', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode) then
      begin
          if not (ErrorCode = 0) then
          begin
              ShellExec('', ExpandConstant('{cmd}'),'/C "'+MySQL_Install_Binary+'" -u root -p'+MySQL_Root_Password+' --execute="set global general_log=0"','', SW_HIDE, ewWaitUntilTerminated,Errorcode);
              
              // Remove svsdb1 database and svsystems user.
              if ShellExec('', ExpandConstant('C:\Temp\removedb.bat'),MySQL_Root_Password+' "'+MySQL_Install_Binary+'"', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode) then
                  Log('Removed svsdb1 database and svsystems user')
              else
                  Log('Failed to remove svsdb1 database and svsystems user');
              
              // Copy sql.log file to SystemDrive as MySQL_Database.log.
              if FileCopy(ExpandConstant ('{code:Getdircx}\sql.log'), ExpandConstant ('C:\MySQL_Database.log'), FALSE) then
                  Log('sql.log file copied to SystemDrive as MySQL_Database.log')
              else
                  Log('Failed to copy sql.log file to SystemDrive as MySQL_Database.log');
              
              if (ErrorCode = 1) then
              begin
                  SuppressibleMsgBox('Could not create svsystems user. Aborting installation.', mbError, MB_OK, MB_OK);
                  ExitInstallation();
                  ExitProcess(25);
              end
              else if (ErrorCode = 2) then
              begin
                  SuppressibleMsgBox('Could not create svsdb1 database. Aborting installation.', mbError, MB_OK, MB_OK);
                  ExitInstallation();
                  ExitProcess(26);
              end; 
          end
          else
              Log('Successfully created MySQL database.');
      end;
  end;
end;

procedure Run_rrd_tunepl ();
var
  ErrorCode: Integer;
begin
	ShellExec('', ExpandConstant('{sd}\Temp\RunPerlFiles.bat'),'"'+ExpandConstant('{code:Getdircx}')+'"', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode);
	if FileExists(ExpandConstant('{sd}\Temp\rrdtunefileoutput.txt')) then	
	begin
	   	LoadStringFromFile(ExpandConstant ('{sd}\Temp\rrdtunefileoutput.txt'), LogFileString);    
	    Log(#13#10 +'rrd tune log Starts here '+ #13#10 + '*********************************************');
	    Log(#13#10 + LogFileString);
	    Log(#13#10 + '*********************************************' + #13#10 + 'rrd tune log ends here :' +#13#10 + #13#10);
	    CustomDeleteFile(ExpandConstant('{sd}\Temp\rrdtunefileoutput.txt'));
	end;
end;

function Validate_MySQL : Boolean;
var
    MySQL_Path, MySQL_Version, MySQL_Data_Path : String;
begin
	RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','Location',MySQL_Path);
	RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','DataLocation',MySQL_Data_Path);
	RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','Version',MySQL_Version);
  
  Result := True;
  if Fileexists(ExpandConstant(MySQL_Path+'bin\mysql.exe')) then
  begin
      if (Pos('5.7.20',MySQL_Version) > 0) then   
      begin 
          SetIniString( 'Install_Paths', 'MySQL_Path',MySQL_Path, ExpandConstant('{sd}\Temp\Install_Paths.conf') );
          SetIniString( 'Install_Paths', 'MySQL_Data_Path',MySQL_Data_Path, ExpandConstant('{sd}\Temp\Install_Paths.conf') );
      end
      else
      begin
          Result := False;
      end;
  end
  else
  begin
      Result := False;
  end;
end;

Procedure SaveInstallLog();
var
    LogFileString : AnsiString;
begin
    FileCopy(ExpandConstant ('{log}'), ExpandConstant ('{sd}\InstallLogFile.log'), FALSE);
    LoadStringFromFile(ExpandConstant ('{sd}\InstallLogFile.log'), LogFileString);    
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_InstallLogFile.log'),'Installation Starts here '+ Installation_StartTime + #13#10 + '*********************************************', True);
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_InstallLogFile.log'),#13#10 + LogFileString , True);
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_InstallLogFile.log'), #13#10 + '*********************************************' + #13#10 + 'Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
    CustomDeleteFile(ExpandConstant('{sd}\InstallLogFile.log'));
end;

function InstallMySQL : Boolean;
begin
      if not Validate_MySQL then
      begin
          if Fileexists(ExpandConstant('{sd}\Temp\ASRSetup\mysql-installer-community-5.7.20.0.msi')) then
          begin
              if not (Fileexists(ExpandConstant('{sd}\ProgramData\MySQL\MySQL Installer for Windows\Product Cache\mysql-5.7.20-win32.msi'))) then
              begin
                  Log('mysql-installer-community-5.7.20.0.msi file exists, proceeding ahead with installation.');
                  ShellExec('', ExpandConstant('msiexec.exe'),' /i '+ExpandConstant('{sd}\Temp\ASRSetup\mysql-installer-community-5.7.20.0.msi')+' /qn /l* C:\MySQL_Installer.log','', SW_SHOW, ewWaitUntilTerminated, ResultCode);
                  if FileExists(ExpandConstant('C:\MySQL_Installer.log')) then	
                  begin
                      LoadStringFromFile(ExpandConstant ('C:\MySQL_Installer.log'), MySQLLogFileString);    
                      Log(#13#10 +'MySQL community installer log Starts here '+ #13#10 + '*********************************************');
                      Log(#13#10 + MySQLLogFileString);
                      Log(#13#10 + '*********************************************' + #13#10 + 'MySQL community installer log Ends here :' +#13#10 + #13#10);
                      CustomDeleteFile(ExpandConstant('C:\MySQL_Installer.log'));
                  end;
              end;

              if (Fileexists(ExpandConstant('{sd}\ProgramData\MySQL\MySQL Installer for Windows\Product Cache\mysql-5.7.20-win32.msi'))) then
              begin
                  Log(ExpandConstant('{sd}\ProgramData\MySQL\MySQL Installer for Windows\Product Cache\mysql-5.7.20-win32.msi') + ' file exists, proceeding ahead with installation.');
                  ShellExec('', ExpandConstant('msiexec.exe'),' /i "'+ExpandConstant('{sd}\ProgramData\MySQL\MySQL Installer for Windows\Product Cache\mysql-5.7.20-win32.msi')+'" /qn /l* C:\MySQL_Install.log','', SW_SHOW, ewWaitUntilTerminated, ResultCode);
                  if FileExists(ExpandConstant('C:\MySQL_Install.log')) then	
                  begin
                      LoadStringFromFile(ExpandConstant ('C:\MySQL_Install.log'), MySQLLogFileString);    
                      Log(#13#10 +'MySQL-5.7.20 installation log Starts here '+ #13#10 + '*********************************************');
                      Log(#13#10 + MySQLLogFileString);
                      Log(#13#10 + '*********************************************' + #13#10 + 'MySQL-5.7.20 installation log Ends here :' +#13#10 + #13#10);
                      CustomDeleteFile(ExpandConstant('C:\MySQL_Install.log'));
                  end;
              end
              else
              begin
                  Log(ExpandConstant('{sd}\ProgramData\MySQL\MySQL Installer for Windows\Product Cache\mysql-5.7.20-win32.msi') + ' file does not exists, aborting the installation.');
                  Result := False;
              end;

              if Validate_MySQL then
              begin
                  Log('MySQL 5.7.20 is installed successfully');
                  Result := True;
              end
              else
              begin
                  Log('MySQL 5.7.20 is not installed. Aborting.');
                  Result := false;
              end;
          end
          else
          begin
              Log('mysql-installer-community-5.7.20.0.msi setup file does not exist.');
              Result := False;
          end;
      end
      else
      begin
          Log('The prerequisite MySQL 5.7.20 version is installed already, proceeding with installation.');
          Result := True;
      end;
end;

function IsMySQL55Installed : Boolean;
var
    MySQL55Path, MySQL55Version : String;
begin
	RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.5','Location',MySQL55Path);
	RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.5','Version',MySQL55Version);
  
  Result := True;
  if Fileexists(ExpandConstant(MySQL55Path+'bin\mysql.exe')) then
  begin
      if (Pos('5.5.37',MySQL55Version) > 0) then   
      begin 
          Result := True;
      end
      else
      begin
          Result := False;
      end;
  end
  else
  begin
      Result := False;
  end;
end;

// Stop CX and Agent services.
function StopCXandAgentServices : Boolean;
begin
    Result := True;

    Log('Stopping the services. This may take some time. Please wait...');

	// Always stop ProcessServerMonitor service first to avoid generating alerts if other services are stopped first during install/upgrade/uninstall.
	if not ServiceStopInLoop('ProcessServerMonitor') then
    begin
        Result := False;
        Exit;
    end;

    if GetValue('CX_TYPE') = '3' then
    begin
        if not ServiceStopInLoop('W3SVC') then
        begin
            Result := False;
            Exit;
        end;
    end;

    if not ServiceStopInLoop('tmansvc') then
    begin
        Result := False;
        Exit;
    end;
	
	if not ServiceStopInLoop('ProcessServer') then
    begin
        Result := False;
        Exit;
    end;

    ExtractTemporaryFile('clean_stale_process.pl');
    Exec(ExpandConstant('C:\strawberry\perl\bin\perl.exe'),ExpandConstant('{tmp}\clean_stale_process.pl'), '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    
    if not ServiceStopInLoop('svagents') then
    begin
        Result := False;
        Exit;
    end;
     
    if not ServiceStopInLoop('InMage Scout Application Service') then
    begin
        Result := False;
        Exit;
    end;

    if not ServiceStopInLoop('InMage PushInstall') then
    begin
        Result := False;
        Exit;
    end;

    if GetValue('CX_TYPE') = '3' then
    begin
        if not ServiceStopInLoop('dra') then
        begin
            Result := False;
            Exit;
        end;
    end;

    if not ServiceStopInLoop('cxprocessserver') then
    begin
        Result := False;
        Exit;
    end;
end;

// Start CX and Agent services.
function StartCXandAgentServices : Boolean;
begin
    Result := True;

    if GetValue('CX_TYPE') = '3' then
    begin
        if not ServiceStartInLoop('W3SVC') then
        begin
            Result := False;
        end;
    end;

    if not ServiceStartInLoop('tmansvc') then
    begin
        Result := False;
    end;
	
	if not ServiceStartInLoop('ProcessServer') then
    begin
        Result := False;
    end;

    if not ServiceStartInLoop('svagents') then
    begin
        Result := False;
    end;

    if not ServiceStartInLoop('InMage Scout Application Service') then
    begin
        Result := False;
    end;

    if not ServiceStartInLoop('InMage PushInstall') then
    begin
        Result := False;
    end;

    if GetValue('CX_TYPE') = '3' then
    begin
        if not ServiceStartInLoop('dra') then
        begin
            Result := False;
        end;
    end;

    if not ServiceStartInLoop('cxprocessserver') then
    begin
        Result := False;
    end;

	// Always start ProcessServerMonitor service first to avoid generating alerts if other services are stopped first during install/upgrade/uninstall.
	if not ServiceStartInLoop('ProcessServerMonitor') then
    begin
        Result := False;
    end;
end;

procedure UninstallMySQL57();
var
  ErrorCode, ReturnCode: Integer;
  MySQL57Path, MySQL57Data_Path : String;
begin
    RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','Location',MySQL57Path);
    RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','DataLocation',MySQL57Data_Path);

    if not ServiceStopInLoop('MySQL') then       
    begin
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(48);
    end;
    
    ShellExec('', ExpandConstant('msiexec.exe'),' /x "'+ExpandConstant('{sd}\ProgramData\MySQL\MySQL Installer for Windows\Product Cache\mysql-5.7.20-win32.msi"')+' /qn /norestart /l* C:\ProgramData\ASRSetupLogs\MySQL57_Uninstall.log','', SW_SHOW, ewWaitUntilTerminated, ErrorCode);
    
    if (ErrorCode = 0) then
    begin
        Log('MySQL 5.7.20 uninstalled successfully.');
    end
    else
    begin
        SuppressibleMsgBox('Unable to uninstall MySQL 5.7.20. Aborting the installation.',mbCriticalError, MB_OK, MB_OK);
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(56);
    end;
    
    CustomDeleteDirectory(MySQL57Path);
    CustomDeleteDirectory(MySQL57Data_Path);
end;

Procedure RestoreMySQLDB();
var
MySQL_Data_Path, MySQL_Path, MyINI_Path , MySQL_Binary, MySQL_Root_Password, MySQL_User_Password, MySQL_Install_Binary, PathString, Path_Backup, PathRestoreBackup : String;
begin
      MySQL_Root_Password := GetValue('DB_ROOT_PASSWD');  
      MySQL_User_Password := GetValue('DB_PASSWD');

      if Validate_MySQL then
      begin
          UninstallMySQL57;
      end;

      RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\InMage Systems\Installed Products\9', 'RollbackToPreviousState', 'YES');
      if FileCopy(ExpandConstant ('C:\thirdparty\php5nts\php.ini'), ExpandConstant ('{sd}\Temp\php_latest.ini'), FALSE) then
        Log('Successfully copied php.ini to '+ExpandConstant('{sd}\Temp\php_latest.ini')+' from C:\thirdparty\php5nts')
      else
      begin
        Log('Unable to copy php.ini to '+ExpandConstant('{sd}\Temp\php_latest.ini')+' from C:\thirdparty\php5nts');
        SaveInstallLog();
        ExitProcess(57);
      end;

      CustomDeleteDirectory(ExpandConstant('C:\thirdparty\php5nts'));
		
      if IsWin64 then
      begin
          //Turn off redirection, so that cmd.exe from the 64-bit System directory is launched.
          OldState := EnableFsRedirection(False);
          try
              Exec(ExpandConstant('{cmd}'), '/C cscript "'+ExpandConstant('C:\Temp\unzip_Files.vbs')+'" "'+ExpandConstant('C:\Temp\php-5.4.27-nts-Win32-VC9-x86.zip')+'" "'+ExpandConstant('C:\thirdparty\php5nts')+'" ','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
          finally
              // Restore the previous redirection state.
          EnableFsRedirection(OldState);
          end;
      end;

      if FileCopy(ExpandConstant ('{sd}\Temp\php.ini'), ExpandConstant ('C:\thirdparty\php5nts\php.ini'), FALSE) then
        Log('Successfully copied php.ini to C:\thirdparty\php5nts from '+ExpandConstant('{sd}\Temp'))
      else
      begin
        Log('Unable to copy php.ini to C:\thirdparty\php5nts from '+ExpandConstant('{sd}\Temp'));
        SaveInstallLog();
        ExitProcess(57);
      end;

      Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('C:\thirdparty\php5nts')+'" /grant "IUSR:(OI)(CI)F"','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
      if (not(Errorcode=0)) then
      begin
          Log('Unable to change permissions of C:\thirdparty\php5nts directory. Aborting installation.');
          SaveInstallLog();
          ExitProcess(57);
      end
      else
          Log('Permissions changes completed successfully on C:\thirdparty\php5nts directory.');

      if Fileexists(ExpandConstant('{sd}\Temp\ASRSetup\mysql-5.5.37-win32.msi')) then
      begin
          Log('mysql-5.5.37-win32.msi file exists, proceeding ahead with installation.');
          ShellExec('', ExpandConstant('msiexec.exe'),' /i '+ExpandConstant('{sd}\Temp\ASRSetup\mysql-5.5.37-win32.msi')+' /qn /l* C:\MySQL_Install.log','', SW_SHOW, ewWaitUntilTerminated, ResultCode);
          if FileExists(ExpandConstant('C:\MySQL_Install.log')) then	
          begin
              LoadStringFromFile(ExpandConstant ('C:\MySQL_Install.log'), MySQLLogFileString);    
              Log(#13#10 +'MySQL installation log Starts here '+ #13#10 + '*********************************************');
              Log(#13#10 + MySQLLogFileString);
              Log(#13#10 + '*********************************************' + #13#10 + 'MySQL installation log Ends here :' +#13#10 + #13#10);
              CustomDeleteFile(ExpandConstant('C:\MySQL_Install.log'));
          end;
          if IsMySQL55Installed then
          begin
              Log('MySQL 5.5.37 is installed successfully');
          end
          else
          begin
              Log('MySQL 5.5.37 is not installed. Aborting.');
              SaveInstallLog();
              ExitProcess(57);
          end;
      end
      else
      begin
          Log('mysql-5.5.37-win32.msi setup file does not exist.');
          SaveInstallLog();
          ExitProcess(58);
      end;

      RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.5','Location',MySQL_Path);
      RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.5','DataLocation',MySQL_Data_Path);
      MyINI_Path := ExpandConstant(MySQL_Path+'my.ini');
      MySQL_Binary := ExpandConstant(MySQL_Path+'bin\mysqld.exe');
      MySQL_Install_Binary := ExpandConstant(MySQL_Path+'bin\mysql.exe');

      if FileCopy(ExpandConstant ('{code:Getdircx}\my_initial.ini'), MyINI_Path , FALSE) then
          Log('my.ini file copied to '+ MyINI_Path)
      else
      begin
          Log('my.ini file failed to copy to '+MyINI_Path);
          SaveInstallLog();
          ExitProcess(48);
      end;
          
      if SetIniString( 'mysqld', 'datadir','"'+ExpandConstant(MySQL_Data_Path)+'data"',MyINI_Path) then
          Log('Datadir value is replaced successfully in my.ini file.' + Chr(13))
      else
      begin
          Log('datadir value has not been replaced in my.ini. Aborting installation.'+ Chr(13));
          SaveInstallLog();
          ExitProcess(48);
      end;
      if SetIniString( 'mysqld', 'basedir','"'+ExpandConstant(MySQL_Path)+'"',MyINI_Path) then
          Log('Basedir value is replaced successfully in my.ini file.' + Chr(13))
      else
      begin
          Log('basedir value has not been replaced in my.ini. Aborting installation.'+ Chr(13));
          SaveInstallLog();
          ExitProcess(48);
      end;
          
      if not (IsServiceInstalled('MySQL')) then
      begin
          if IsWin64 then
          begin
              OldState := EnableFsRedirection(False);
              try
                  Exec(ExpandConstant('{cmd}'), '/C "'+MySQL_Binary+'" --install','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
              finally
                  EnableFsRedirection(OldState);
              end;
          end;

          if (IsServiceInstalled('MySQL')) then
          begin
              if ServiceStartInLoop('MySQL') then
              begin
                  Log('Unable to start MySQL service after replacing values in my.ini file. Please retry with install.' + Chr(13));
                  SaveInstallLog();
                  ExitProcess(48);
              end;
          end
          else
          begin
              Log('Unable to install MySQL service. Please retry with install. If issue persists even after retry, contact Microsoft support.' + Chr(13));
              SaveInstallLog();
              ExitProcess(48);
          end;
      end;

    	Sleep(5000);	   
      
      LoopCount := 0;
      while (LoopCount <= 3) do
      begin
          Log('Invoking Reset_Passwd.bat script.');
          if ShellExec('', ExpandConstant('C:\Temp\Reset_Passwd.bat'),'"'+ExpandConstant(MySQL_Path)+'" '+MySQL_Root_Password+' "'+ExpandConstant('{code:Getdircx}')+'"', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode) then
          begin
              if not (ErrorCode = 0) then
              begin
                  if (ErrorCode = 1) then
                  begin
                    SuppressibleMsgBox('Cannot log into MySQL with the default root password or with the user provided root password. Aborting the installation.', mbError, MB_OK, MB_OK);
                    ShellExec('', ExpandConstant('{cmd}'),'/C "'+ExpandConstant(MySQL_Path)+'"bin\mysql.exe -u root -p'+MySQL_Root_Password+' --execute="set global general_log=0"','', SW_HIDE, ewWaitUntilTerminated,Errorcode);
                    if FileCopy(ExpandConstant ('{code:Getdircx}\sql.log'), ExpandConstant ('C:\MySQL_Database.log'), FALSE) then
                        Log('sql.log file copied to SystemDrive as MySQL_Database.log');
                    SaveInstallLog();
                    ExitProcess(59);
                  end
                  else if (ErrorCode = 2) then
                  begin
                      Log('Could not reset MySQL root password. Re-trying.');
                      ExitInstall := 'Yes';
                  end
              end
              else
              begin
                  Log('Successfully reset the MySQL root password.');
                  ExitInstall := 'No';
                  break;
              end;
          end;
          sleep(10000);                                    
          LoopCount := LoopCount + 1;                                
      end;

      if (ExitInstall = 'Yes') then
      begin
          SuppressibleMsgBox('Could not reset MySQL root password. Aborting installation.', mbError, MB_OK, MB_OK);
          ShellExec('', ExpandConstant('{cmd}'),'/C "'+ExpandConstant(MySQL_Path)+'"bin\mysql.exe -u root -p'+MySQL_Root_Password+' --execute="set global general_log=0"','', SW_HIDE, ewWaitUntilTerminated,Errorcode);
          if FileCopy(ExpandConstant ('{code:Getdircx}\sql.log'), ExpandConstant ('C:\MySQL_Database.log'), FALSE) then
              Log('sql.log file copied to SystemDrive as MySQL_Database.log');
          SaveInstallLog();
          ExitProcess(60);
      end;

      ShellExec('', ExpandConstant('{cmd}'),'/C ""'+MySQL_Install_Binary+'" -u root -p'+MySQL_Root_Password+' < "c:\Temp\create_user.sql""','', SW_HIDE, ewWaitUntilTerminated,ErrorCode);
      if (ErrorCode = 0) then
      begin
          Log('Create svssystems user successful.');
      end
      else
      begin
          SuppressibleMsgBox('Could not create svsystems user. Aborting the installation.',mbCriticalError, MB_OK, MB_OK);
          SaveInstallLog();
          ExitProcess(61);
      end;

      ShellExec('', ExpandConstant('{cmd}'),'/C ""'+MySQL_Install_Binary+'" -u root -p'+MySQL_Root_Password+' svsdb1 < "'+ExpandConstant ('{code:Getdircx}')+'\upgrade_db.sql""','', SW_HIDE, ewWaitUntilTerminated,ErrorCode);
      if (ErrorCode = 0) then
      begin
          Log('Restore database successful.');
      end
      else
      begin
          SuppressibleMsgBox('Could not restore database. Aborting the installation.',mbCriticalError, MB_OK, MB_OK);
          SaveInstallLog();
          ExitProcess(61);
      end;

      if (Length(MySQL_Path) > 0) and (MySQL_Path[Length(MySQL_Path)] = '\') then
      begin
        while (Length(MySQL_Path) > 0) and (MySQL_Path[Length(MySQL_Path)] = '\') do
          Delete(MySQL_Path, Length(MySQL_Path), 1);
      end;
      
      if (Length(MySQL_Data_Path) > 0) and (MySQL_Data_Path[Length(MySQL_Data_Path)] = '\') then
      begin
        while (Length(MySQL_Data_Path) > 0) and (MySQL_Data_Path[Length(MySQL_Data_Path)] = '\') do
          Delete(MySQL_Data_Path, Length(MySQL_Data_Path), 1);
      end;
      
      if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'MYSQL_PATH','MYSQL_PATH = "' + MySQL_Path + '"') then
          Log('Successfully added MYSQL_PATH to amethyst.conf file.')
      else
      begin
          Log('Unable to add MYSQL_PATH to amethyst.conf file. Aborting installation.');
          SaveInstallLog();
          ExitProcess(62);
      end;
      if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'MYSQL_DATA_PATH','MYSQL_DATA_PATH = "' + MySQL_Data_Path + '"') then
          Log('Successfully added MYSQL_DATA_PATH to amethyst.conf file.')
      else
      begin
          Log('Unable to add MYSQL_DATA_PATH to amethyst.conf file. Aborting installation.');
          SaveInstallLog();
          ExitProcess(62);
      end;

      RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\9','PathRestoreBackup',PathRestoreBackup);
      RegWriteExpandStringValue(HKEY_LOCAL_MACHINE,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment','Path',PathRestoreBackup);
      RegWriteExpandStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\9','Install_Path_Backup',PathRestoreBackup);

      if StartCXandAgentServices then
      begin
          Log('Services started successful.');
      end
      else
      begin
          SuppressibleMsgBox('Unable to start services. Aborting the installation.',mbCriticalError, MB_OK, MB_OK);
          SaveInstallLog();
          ExitProcess(48);
      end;
end;

procedure UninstallMySQLAndRestoreDB();
var
  ErrorCode, ReturnCode: Integer;
  MySQL_Root_Password, MySQL_User_Password, MySQL_Path, MySQL_Data_Path, MyINI_Path, MySQL_Binary, MySQLCheck_Binary, MySQLDump_Binary, MySQLInitFile, MySQL55Path, MySQL55Data_Path, MySQL_Install_Binary, Path_Backup, PathString, MySQL55RegPath, MySQLRegPath, RollbackState : String;
begin
    MySQL_Root_Password := GetValue('DB_ROOT_PASSWD');  
    MySQL_User_Password := GetValue('DB_PASSWD');
    RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.5','Location',MySQL55Path);
    RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.5','DataLocation',MySQL55Data_Path);
    MySQLCheck_Binary := ExpandConstant(MySQL55Path+'bin\mysqlcheck.exe');
    MySQLDump_Binary := ExpandConstant(MySQL55Path+'bin\mysqldump.exe');
    MySQLInitFile := ExpandConstant('C:\Temp\mysql-init.txt');

    RegQueryStringValue(HKLM,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment','Path',PathString);
    RegWriteExpandStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\9','PathRestoreBackup',PathString);

    if not StopCXandAgentServices then
    begin
         StartCXandAgentServices;
         SaveInstallLog();
         ExitProcess(48);
    end;

    RegQueryStringValue(HKLM,'SOFTWARE\InMage Systems\Installed Products\9','RollbackToPreviousState',RollbackState);
    if RollbackState = 'YES' then
    begin
        CustomDeleteDirectory(ExpandConstant('C:\thirdparty\php5nts')); 
      
        if IsWin64 then
        begin
            //Turn off redirection, so that cmd.exe from the 64-bit System directory is launched.
            OldState := EnableFsRedirection(False);
            try
                Exec(ExpandConstant('{cmd}'), '/C cscript "'+ExpandConstant('C:\Temp\unzip_Files.vbs')+'" "'+ExpandConstant('C:\Temp\php-7.1.10-nts-Win32-VC14-x86.zip')+'" "'+ExpandConstant('C:\thirdparty\php5nts')+'" ','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
            finally
                // Restore the previous redirection state.
            EnableFsRedirection(OldState);
            end;
        end;

        if FileCopy(ExpandConstant ('{sd}\Temp\php_latest.ini'), ExpandConstant ('C:\thirdparty\php5nts\php.ini'), FALSE) then
          Log('Successfully copied php_latest.ini to C:\thirdparty\php5nts from '+ExpandConstant('{sd}\Temp'))
        else
        begin
          Log('Unable to copy php_latest.ini to C:\thirdparty\php5nts from '+ExpandConstant('{sd}\Temp'));
          SaveInstallLog();
          ExitProcess(57);
        end;
    end;

    if not ServiceStartInLoop('MySQL') then
    begin
        SaveInstallLog();
        ExitProcess(48);
    end;

    if ShellExec('', ExpandConstant('{cmd}'),'/C "'+MySQLCheck_Binary+'" -u root -p'+MySQL_Root_Password+' svsdb1 > C:\DBHealth.log','', SW_HIDE, ewWaitUntilTerminated,ErrorCode) then
    begin
        if FileExists(ExpandConstant('C:\DBHealth.log')) then	
        begin
            LoadStringFromFile(ExpandConstant ('C:\DBHealth.log'), LogFileString);    
            Log(#13#10 +'DB health log Starts here '+ #13#10 + '*********************************************');
            Log(#13#10 + LogFileString);
            Log(#13#10 + '*********************************************' + #13#10 + 'DB health log Ends here :' +#13#10 + #13#10);
        end;
        if (ErrorCode = 0) then
        begin
            Log('Datatabase server health is fine.');
        end
        else
        begin
            StartCXandAgentServices;
            SuppressibleMsgBox('Database server health is not good. Aborting the installation.',mbCriticalError, MB_OK, MB_OK);
            SaveInstallLog();
            ExitProcess(49);
        end;
    end;

    if ShellExec('', ExpandConstant('{cmd}'),'/C ""'+MySQLDump_Binary+'" -u root -p'+MySQL_Root_Password+' svsdb1 > "'+ExpandConstant ('{code:Getdircx}')+'\upgrade_db.sql""','', SW_HIDE, ewWaitUntilTerminated,ReturnCode) then
    begin
        if (ReturnCode = 0) then
        begin
            Log('Datatabase backup is successful.');
        end
        else
        begin
            StartCXandAgentServices;
            SuppressibleMsgBox('Unable to take database backup. Aborting the installation.',mbCriticalError, MB_OK, MB_OK);
            SaveInstallLog();
            ExitProcess(50);
        end;
    end;

    if not ServiceStopInLoop('MySQL') then
    begin
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(48);
    end;
    
    ShellExec('', ExpandConstant('msiexec.exe'),' /x '+ExpandConstant('{sd}\Temp\ASRSetup\mysql-5.5.37-win32.msi')+' /qn /norestart /l* C:\ProgramData\ASRSetupLogs\MySQL_Uninstall.log','', SW_SHOW, ewWaitUntilTerminated, ErrorCode);
    
    if (ErrorCode = 0) then
    begin
        Log('MySQL 5.5.37 uninstalled successfully.');
    end
    else
    begin
        SuppressibleMsgBox('Unable to uninstall MySQL 5.5.37. Aborting the installation.',mbCriticalError, MB_OK, MB_OK);
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(51);
    end;
    
    if not CustomDeleteDirectory(MySQL55Path) then
    begin
        SuppressibleMsgBox('Unable to delete '+MySQL55Path+'. Aborting the installation.',mbCriticalError, MB_OK, MB_OK);
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(51);
    end
    else
        Log('Delete '+MySQL55Path+' is successful.');
    
    if not CustomDeleteDirectory(MySQL55Data_Path) then
    begin
        SuppressibleMsgBox('Unable to delete '+MySQL55Data_Path+'. Aborting the installation.',mbCriticalError, MB_OK, MB_OK);
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(51);
    end
    else
        Log('Delete '+MySQL55Data_Path+' is successful.');

    if not InstallMySQL then
    begin
        SuppressibleMsgBox('Unable to install MySQL. Aborting the installation.',mbCriticalError, MB_OK, MB_OK);
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(32);
    end;

    RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','Location',MySQL_Path);
    RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','DataLocation',MySQL_Data_Path);
    MyINI_Path := ExpandConstant(MySQL_Data_Path+'my.ini');
    MySQL_Binary := ExpandConstant(MySQL_Path+'bin\mysqld.exe');
    MySQL_Install_Binary := ExpandConstant(MySQL_Path+'bin\mysql.exe');

    if FileCopy(ExpandConstant('C:\Temp\my.ini'), ExpandConstant(MySQL_Data_Path+'my.ini'), FALSE) then
        Log('my.ini file copied to '+ExpandConstant(MySQL_Data_Path)+' path.')
    else
    begin
        Log('my.ini file failed to copy to'+ExpandConstant(MySQL_Data_Path)+' path.');
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(52);
    end;

    if SetIniString( 'mysqld', 'datadir','"'+ExpandConstant(MySQL_Data_Path)+'data"',MyINI_Path) then
        Log('Datadir value is replaced successfully in my.ini file.' + Chr(13))
    else
    begin
        Log('datadir value has not been replaced in my.ini. Aborting installation.'+ Chr(13));
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(52);
    end;
    if SetIniString( 'mysqld', 'basedir','"'+ExpandConstant(MySQL_Path)+'"',MyINI_Path) then
        Log('Basedir value is replaced successfully in my.ini file.' + Chr(13))
    else
    begin
        Log('basedir value has not been replaced in my.ini. Aborting installation.'+ Chr(13));
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(52);
    end;

    if IsWin64 then
    begin
        OldState := EnableFsRedirection(False);
        try
            Exec(ExpandConstant('{cmd}'), '/C ""'+MySQL_Binary+'" --install MySQL --defaults-file="'+MyINI_Path+'""','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
        finally
            EnableFsRedirection(OldState);
        end;
    end;
    
    if (IsServiceInstalled('MySQL')) then
    begin
        Log('MySQL service is installed after replacing values in my.ini file.' + Chr(13));
    end
    else
    begin
        Log('Unable to install MySQL service. Please retry with install. If issue persists even after retry, contact Microsoft support.' + Chr(13));
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(48);
    end;

    if not CustomDeleteDirectory(ExpandConstant(MySQL_Data_Path+'data')) then
    begin
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(48);
    end;

    if not CustomCreateDirectory(ExpandConstant(MySQL_Data_Path+'Uploads')) then
    begin
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(48);
    end;

    if IsWin64 then
    begin
        OldState := EnableFsRedirection(False);
        try
            Exec(ExpandConstant('{cmd}'), '/C ""'+MySQL_Binary+'" --defaults-file="'+MyINI_Path+'" --initialize --datadir="'+MySQL_Data_Path+'data""','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
        finally
            EnableFsRedirection(OldState);
        end;
    end;

    if not ServiceStartInLoop('MySQL') then
    begin
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(48);
    end;

    if FileReplaceString((ExpandConstant('C:\Temp\create_user.sql')), 'user-defined', MySQL_User_Password) then
      Log('user-defined value has been replaced in create_user.sql'+ Chr(13))
    else
    begin  
      Log('user-defined value has not been replaced in create_user.sql. Aborting installation.'+ Chr(13));
      RestoreMySQLDB;
      StartCXandAgentServices;
      SaveInstallLog();
      ExitProcess(48);
    end;
    if FileReplaceString(MySQLInitFile, 'user-defined', MySQL_Root_Password) then
      Log('user-defined value has been replaced in '+ MySQLInitFile)
    else
    begin  
      Log('user-defined value has not been replaced in '+MySQLInitFile+'. Aborting installation.'+ Chr(13));
      RestoreMySQLDB;
      StartCXandAgentServices;
      SaveInstallLog();
      ExitProcess(48);
    end;

    StringChangeEx(MySQLInitFile,'\','\\',True);

    // Stop MySQL service.
    if not ServiceStopInLoop('MySQL') then
    begin
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(48);
    end;

    ShellExec('', ExpandConstant('{cmd}'),'/C ""'+MySQL_Binary+'" --defaults-file="'+MyINI_Path+'" --datadir="'+MySQL_Data_Path+'data"" --init-file='+MySQLInitFile,'', SW_HIDE, ewNoWait,ResultCode);
    Sleep(10000);
    
    if ShellExec('', ExpandConstant('{cmd}'),'/C taskkill /F /IM mysqld.exe /T','', SW_HIDE, ewWaitUntilTerminated,Errorcode) then
        Log('Successfully killed the mysqld process.')
    else
    begin
        Log('Unable to kill the mysqld process.');
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(48);
    end;

    if not ServiceStartInLoop('MySQL') then
    begin
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(48);
    end;

    ShellExec('', ExpandConstant('{cmd}'),'/C ""'+MySQL_Install_Binary+'" -u root -p'+MySQL_Root_Password+' < "c:\Temp\create_user.sql""','', SW_HIDE, ewWaitUntilTerminated,ErrorCode);
    if (ErrorCode = 0) then
    begin
        Log('Create svssystems user successful.');
    end
    else
    begin
        SuppressibleMsgBox('Could not create svsystems user. Aborting the installation.',mbCriticalError, MB_OK, MB_OK);
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(53);
    end;

    ShellExec('', ExpandConstant('{cmd}'),'/C ""'+MySQL_Install_Binary+'" -u root -p'+MySQL_Root_Password+' svsdb1 < "'+ExpandConstant ('{code:Getdircx}')+'\upgrade_db.sql""','', SW_HIDE, ewWaitUntilTerminated,ErrorCode);
    if (ErrorCode = 0) then
    begin
        Log('Restore database successful.');
    end
    else
    begin
        SuppressibleMsgBox('Could not restore database. Aborting the installation.',mbCriticalError, MB_OK, MB_OK);
        RestoreMySQLDB;
        StartCXandAgentServices;
        SaveInstallLog();
        ExitProcess(54);
    end;

    RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\9','Install_Path_Backup',Path_Backup);
    MySQL55RegPath := ExpandConstant(MySQL55Path)+'lib;'+ExpandConstant(MySQL55Path)+'bin;';
    MySQLRegPath := ExpandConstant(MySQL_Path)+'lib;'+ExpandConstant(MySQL_Path)+'bin;';
    StringChangeEx(PathString,MySQL55RegPath,MySQLRegPath,True);
    StringChangeEx(Path_Backup,MySQL55RegPath,MySQLRegPath,True);
    RegWriteExpandStringValue(HKEY_LOCAL_MACHINE,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment','Path',PathString);
    RegWriteExpandStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\9','Install_Path_Backup',Path_Backup);

    if StartCXandAgentServices then
    begin
        Log('Services started successful.');
    end
    else
    begin
        SuppressibleMsgBox('Unable to start services. Aborting the installation.',mbCriticalError, MB_OK, MB_OK);
        SaveInstallLog();
        ExitProcess(55);
    end;
end;

procedure RefreshEnvironment;
var
  S: AnsiString;
  MsgResult: DWORD;
begin
  S := 'Environment';
  SendTextMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
    PAnsiChar(S), SMTO_ABORTIFHUNG, 5000, MsgResult);
end;

procedure Upgradedatabase ();
var
  ErrorCode, DbUpgradeErrorCode : Integer;
  MySQL_Root_Password, MySQL_User_Password, MySQL_Path, MySQL_Data_Path, MyINI_Path, MySQL57Path, MySQ57Data_Path, MySQL55_Path, MyINI55_Path, DbUpgraded : String;
begin
    MySQL_Root_Password := GetValue('DB_ROOT_PASSWD');  
    MySQL_User_Password := GetValue('DB_PASSWD');

    MySQL_Path := GetIniString( 'Install_Paths', 'MySQL_Path','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
    MyINI_Path := ExpandConstant(MySQL_Path+'my.ini');

    RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.5','Location',MySQL55_Path);
    MyINI55_Path := ExpandConstant(MySQL55_Path+'my.ini');

    if IsMySQL55Installed then
    begin
        if FileCopy(MyINI55_Path, ExpandConstant ('{code:Getdircx}\my_initial.ini'), FALSE) then
            Log('my.ini file copied to '+ExpandConstant('{code:Getdircx}'))
        else
        begin
            Log('my.ini file failed to copy to'+ExpandConstant('{code:Getdircx}'));
            SaveInstallLog();
            ExitProcess(48);
        end;
        UninstallMySQLAndRestoreDB;
        RefreshEnvironment;
    end;

    FileCopy(ExpandConstant ('{code:Getdircx}\etc\amethyst.conf'), ExpandConstant ('{code:Getdircx}\etc\amethyst_upgrade_initial.conf'), FALSE);
    if FileCopy(ExpandConstant ('{code:Getdircx}\backup\amethyst.conf'), ExpandConstant ('{code:Getdircx}\etc\amethyst.conf'), FALSE) then
        Log('Successfully copied amethyst.conf from backup to "'+ExpandConstant('{code:Getdircx}\etc\amethyst.conf')+'" path');

    RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','Location',MySQL57Path);
    RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','DataLocation',MySQ57Data_Path);

    if (Length(MySQL57Path) > 0) and (MySQL57Path[Length(MySQL57Path)] = '\') then
    begin
      while (Length(MySQL57Path) > 0) and (MySQL57Path[Length(MySQL57Path)] = '\') do
        Delete(MySQL57Path, Length(MySQL57Path), 1);
    end;

    if (Length(MySQ57Data_Path) > 0) and (MySQ57Data_Path[Length(MySQ57Data_Path)] = '\') then
    begin
      while (Length(MySQ57Data_Path) > 0) and (MySQ57Data_Path[Length(MySQ57Data_Path)] = '\') do
        Delete(MySQ57Data_Path, Length(MySQ57Data_Path), 1);
    end;

    if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'MYSQL_PATH','MYSQL_PATH = "' + MySQL57Path + '"') then
        Log('Successfully added MYSQL_PATH to amethyst.conf file.')
    else
    begin
        Log('Unable to add MYSQL_PATH to amethyst.conf file. Aborting installation.');
        SaveInstallLog();
        ExitProcess(63);
    end;
    if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'MYSQL_DATA_PATH','MYSQL_DATA_PATH = "' + MySQ57Data_Path + '"') then
        Log('Successfully added MYSQL_DATA_PATH to amethyst.conf file.')
    else
    begin
        Log('Unable to add MYSQL_DATA_PATH to amethyst.conf file. Aborting installation.');
        SaveInstallLog();
        ExitProcess(63);
    end;

    if FileReplaceString((ExpandConstant('C:\Temp\create_user.sql')), 'user-defined', MySQL_User_Password) then
        Log('user-defined value has been replaced in create_user.sql'+ Chr(13))
    else
    begin
        Log('user-defined value has not been replaced in create_user.sql. Aborting installation.'+ Chr(13));
        CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\Upgrade'));
        DelTree(ExpandConstant('C:\Temp\*'), False, True, True);
        SaveInstallLog();
        ExitProcess(27);
    end;
     
    Actual_Install_Path := ExpandConstant('{code:Getdircx}');
    StringChangeEx(Actual_Install_Path,'\home\svsystems','',True);
    Install_Path := Actual_Install_Path;
    Install_Dir := Actual_Install_Path;
    StringChangeEx(Install_Path,'\','/',True);
    StringChangeEx(Install_Dir,'\','\\',True);
    if FileReplaceString((ExpandConstant('{code:Getdircx}\Upgrade\svsdb.sql')), 'CX_INSTALL_PATH_FORWARDSLASH', Install_Path) then
        Log('CX_INSTALL_PATH_FORWARDSLASH value has been replaced in svsdb.sql'+ Chr(13))
     else
     begin  
        Log('CX_INSTALL_PATH_FORWARDSLASH value has not been replaced in svsdb.sql. Aborting the installation.'+ Chr(13));
        CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\Upgrade'));
        DelTree(ExpandConstant('C:\Temp\*'), False, True, True);
        SaveInstallLog();
        ExitProcess(28);
     end;
    if FileReplaceString((ExpandConstant('{code:Getdircx}\Upgrade\svsdb.sql')), 'CX_INSTALL_PATH_BACKSLASH', Install_Dir) then
        Log('CX_INSTALL_PATH_BACKSLASH value has been replaced in svsdb.sql'+ Chr(13))
     else
     begin  
        Log('CX_INSTALL_PATH_BACKSLASH value has not been replaced in svsdb.sql. Aborting the installation.'+ Chr(13));
        CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\Upgrade'));
        DelTree(ExpandConstant('C:\Temp\*'), False, True, True);
        SaveInstallLog();
        ExitProcess(28);
     end;

    if not ServiceStopInLoop('W3SVC') then
    begin
       CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\Upgrade'));
       DelTree(ExpandConstant('C:\Temp\*'), False, True, True);
       SaveInstallLog();
       ExitProcess(30);
    end;

    RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','Location',MySQL57Path);
    SetEnvironmentVariable('PATH', ExpandConstant('{code:Getdircx}\bin')+';'+ExpandConstant(MySQL57Path)+'lib;'+ExpandConstant(MySQL57Path)+'bin;');

    // Invoke configure_upgrade.bat script.
    LoopCount := 1;
    while (LoopCount <= 3) do
    begin
        Log('Invoking configure_upgrade.bat script.');
        ShellExec('', ExpandConstant('C:\Temp\configure_upgrade.bat'),MySQL_Root_Password+' '+MySQL_User_Password+' "'+ExpandConstant(MySQL57Path)+'" "'+ExpandConstant('{code:Getdircx}')+'"', '', SW_HIDE, ewWaitUntilTerminated, DbUpgradeErrorCode);
        Log('DbUpgradeErrorCode  - ('+IntToStr(DbUpgradeErrorCode)+')');
        if (DbUpgradeErrorCode = 0) then
        begin
            Log('Successfully upgraded the MySQL database.');
            DbUpgraded := 'Yes';
            break;
        end
        else
        begin
            Log('Failed to upgrade the MySQL database.');
            DbUpgraded := 'No';  
            LoopCount := LoopCount + 1;
            Sleep(20000);              
        end;
    end;

    if FileExists(ExpandConstant('C:\Temp\Db_Upgrade.log')) then	
    begin
        LoadStringFromFile(ExpandConstant ('C:\Temp\Db_Upgrade.log'), LogFileString);    
        Log(#13#10 +'DB Upgrade log Starts here '+ #13#10 + '*********************************************');
        Log(#13#10 + LogFileString);
        Log(#13#10 + '*********************************************' + #13#10 + 'DB Upgrade log Ends here :' +#13#10 + #13#10);    
    end
    else
        Log('C:\Temp\Db_Upgrade.log does not exist.');

    if (DbUpgraded = 'No') then
    begin
        if Fileexists(ExpandConstant('{code:Getdircx}\Upgrade\upgrade_error.log')) then
        begin
            Log(ExpandConstant('{code:Getdircx}\Upgrade\upgrade_error.log')+' exists.');
            if FileCopy(ExpandConstant ('{code:Getdircx}\Upgrade\upgrade_error.log'), ExpandConstant ('{code:Getdircx}\upgrade_error.log'), FALSE) then
                Log(ExpandConstant('{code:Getdircx}\Upgrade\upgrade_error.log')+' file copied to '+ExpandConstant ('{code:Getdircx}\upgrade_error.log'))
            else
                Log(ExpandConstant('{code:Getdircx}\Upgrade\upgrade_error.log')+' not copied as '+ExpandConstant ('{code:Getdircx}\upgrade_error.log'));
        end
        else
            Log(ExpandConstant('{code:Getdircx}\Upgrade\upgrade_error.log')+' does not exist.');

        CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\Upgrade'));
        DelTree(ExpandConstant('C:\Temp\*'), False, True, True);
        CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\backup'));
        ServiceStartInLoop('cxprocessserver');
        ServiceStartInLoop('tmansvc');
		ServiceStartInLoop('ProcessServer');
        SaveInstallLog();
        ExitProcess(30);
    end;
end;

procedure ReplaceConfFiles ();
var
  ErrorCode: Integer;
  MySQL_Root_Password, MySQL_User_Password, MySQL_Path, MySQL57Path, MySQ57Data_Path : String;
begin
  FileCopy(ExpandConstant ('{code:Getdircx}\etc\amethyst.conf'), ExpandConstant ('{code:Getdircx}\etc\amethyst_upgrade_initial.conf'), FALSE);
  if FileCopy(ExpandConstant ('{code:Getdircx}\backup\amethyst.conf'), ExpandConstant ('{code:Getdircx}\etc\amethyst.conf'), FALSE) then
      Log('Successfully copied amethyst.conf from backup to "'+ExpandConstant('{code:Getdircx}\etc\amethyst.conf')+'" path');

  RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','Location',MySQL57Path);
  RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','DataLocation',MySQ57Data_Path);
  
  if (Length(MySQL57Path) > 0) and (MySQL57Path[Length(MySQL57Path)] = '\') then
  begin
    while (Length(MySQL57Path) > 0) and (MySQL57Path[Length(MySQL57Path)] = '\') do
      Delete(MySQL57Path, Length(MySQL57Path), 1);
  end;
  
  if (Length(MySQ57Data_Path) > 0) and (MySQ57Data_Path[Length(MySQ57Data_Path)] = '\') then
  begin
    while (Length(MySQ57Data_Path) > 0) and (MySQ57Data_Path[Length(MySQ57Data_Path)] = '\') do
      Delete(MySQ57Data_Path, Length(MySQ57Data_Path), 1);
  end;
  
  if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'MYSQL_PATH','MYSQL_PATH = "' + MySQL57Path + '"') then
      Log('Successfully added MYSQL_PATH to amethyst.conf file.')
  else
  begin
      Log('Unable to add MYSQL_PATH to amethyst.conf file. Aborting installation.');
      SaveInstallLog();
      ExitProcess(63);
  end;
  if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'MYSQL_DATA_PATH','MYSQL_DATA_PATH = "' + MySQ57Data_Path + '"') then
      Log('Successfully added MYSQL_DATA_PATH to amethyst.conf file.')
  else
  begin
      Log('Unable to add MYSQL_DATA_PATH to amethyst.conf file. Aborting installation.');
      SaveInstallLog();
      ExitProcess(63);
  end;
end;

function ReplaceLine(strFilename, strFind, strNewLine: AnsiString): Boolean;
var
	iLineCounter : Integer;
	a_strTextfile : TArrayOfString;
begin
	{ Load textfile into AnsiString array }
	LoadStringsFromFile(strFilename, a_strTextfile);

	for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do begin
		{ Overwrite textline when text searched for is part of it }
		if (Pos(strFind, a_strTextfile[iLineCounter]) > 0) then
			a_strTextfile[iLineCounter] := strNewLine;
    end;

	SaveStringsToFile(strFilename, a_strTextfile, False);
	Result := True;
end;

procedure DeleteEmptyLines(const FileName: string);
var
  I: Integer;
  Lines: TStringList;
begin
  // create an instance of a string list
  Lines := TStringList.Create;
  try
    // load a file specified by the input parameter
    Lines.LoadFromFile(FileName);
    // iterate line by line from bottom to top (because of reindexing)
    for I := Lines.Count - 1 downto 0 do
    begin
      // check if the currently iterated line is empty (after trimming
      // the text) and if so, delete it
      if Trim(Lines[I]) = '' then
        Lines.Delete(I);
    end;
    // save the cleaned-up string list back to the file
    Lines.SaveToFile(FileName);
  finally
    // free the string list object instance
    Lines.Free;
  end;
end;

function GetOrGenerateGUID(Param: AnsiString) : AnsiString;
var
	GUID_STRING : String;
	ConfigPathname : String;
begin
	// If 4.0 or higher FX is already installed, then SV Systems\HostId exists. Pick up the GUID value from this entry
	// instead of generating it afresh.
	if RegValueExists(HKLM,'Software\SV Systems\FileReplicationAgent','FxAgent') then
	begin
		// SuppressibleMsgBox('Fx is already installed on this box!', mbInformation, MB_OK);
		// Get GUID
		RegQueryStringValue(HKEY_LOCAL_MACHINE, 'Software\SV Systems', 'HostId', GUID_STRING);
		Result := GUID_STRING;
	end
	else if RegQueryStringValue(HKLM, 'Software\SV Systems\VxAgent', 'ConfigPathname', ConfigPathname) then
		Result := GetIniString('vxagent', 'HostId', GetHostId(Param), ConfigPathname)
	else
	  Result := GetHostId(Param);
end;

function IsSilentOptionSelected(SilentOption: String) : Boolean;
var
	j: Cardinal;
begin
	for j := 1 to ParamCount do
  begin
   	  if (CompareText(ParamStr(j),'/'+SilentOption) = 0) or (CompareText(ParamStr(j),'-'+SilentOption) = 0) then
      begin
            Result := True;
            Break;
      end
      else
      begin
           Result := False;
      end;
 end;
end;

function IsPSSelected : Boolean;
begin
	if WizardSilent then
  begin
      InitialSettings;
      if (ServerMode = 'PS') or (ServerMode = 'ps') or (ServerMode = 'Ps') or (ServerMode = 'BOTH') or (ServerMode = 'both') or (ServerMode = 'Both') or not IsSilentOptionSelected('ServerMode')  then
      begin
          Result := True
      end
      else
      begin
          Result := False;
      end;
  end
  else 
  begin
      if (ServerChoicePage.SelectedValueIndex = 1) or (ServerChoicePage.SelectedValueIndex = 2)then
    		Result := True
    	else
    		Result := False;
  end;
end;

procedure PSIPValidation();
begin
    if IsSilentOptionSelected('PSIP') then
    begin
        Log('User has passed ''/PSIP'' option in silent installation.');
        if (not (ValidateInput(PSIP,'IP'))) then
        begin
            Log('Please pass valid PSIP Address. Aborting installation.');
            SaveInstallLog();
            ExitProcess(33);
        end
        else
        begin
            SetIniString( 'PSONLY', 'PS_IP_Value',PSIP, ExpandConstant('{sd}\Temp\PageValues.conf') );
            SetIniString( 'PSONLY', 'PS_NIC_Value',IPDescription(PSIP), ExpandConstant('{sd}\Temp\PageValues.conf') );
        end;
    end
    else
    begin
        Log('User has not passed ''/PSIP'' option in silent installation.');
        SetIniString( 'PSONLY', 'PS_IP_Value',CSMachineIP, ExpandConstant('{sd}\Temp\PageValues.conf') );
        SetIniString( 'PSONLY', 'PS_NIC_Value',MachineDescription('0'), ExpandConstant('{sd}\Temp\PageValues.conf') );
    end;
end;

procedure AzureIPValidation();
begin
    if IsSilentOptionSelected('AZUREIP') then
    begin
        Log('User has passed ''/AZUREIP'' option in silent installation.');
        if (not (ValidateInput(AZUREIP,'IP'))) then
        begin
            Log('Please pass valid AZUREIP Address. Aborting installation.');
            SaveInstallLog();
            ExitProcess(68);
        end
        else
        begin
            SetIniString( 'CSONLY', 'AZURE_IP_Value',AZUREIP, ExpandConstant('{sd}\Temp\PageValues.conf') );
        end;
    end
    else
    begin
        Log('User has not passed ''/AZUREIP'' option in silent installation.');
        SetIniString( 'CSONLY', 'AZURE_IP_Value',CSMachineIP, ExpandConstant('{sd}\Temp\PageValues.conf') );
    end;
end;

procedure DataTransferSecurePortValidation();
begin
    if IsSilentOptionSelected('DataTransferSecurePort') then
    begin
        InitialSettings;
        Log('User has passed ''/DataTransferSecurePort'' option in silent installation.');
        if (not (ValidateInput(DataTransferSecurePort,'Port'))) then
        begin
            Log('User has passed invalid Port: '+ DataTransferSecurePort +'');
            Log('Please pass valid Port. Aborting installation.');
            SaveInstallLog();
            ExitProcess(34);
        end
        else
        begin
            Log('User has passed valid Port.');
            SetIniString( 'DataTransfer', 'SecurePort',DataTransferSecurePort, ExpandConstant('{sd}\Temp\PageValues.conf') );
        end;
    end
    else
    begin
        Log('User has not passed ''/DataTransferSecurePort'' option in silent installation.');
        SetIniString( 'DataTransfer', 'SecurePort','9443', ExpandConstant('{sd}\Temp\PageValues.conf') );
    end;
end;

function GetSecurePort(Param: String) : String;
begin
    if WizardSilent then
    begin
        Result := GetIniString( 'DataTransfer', 'SecurePort','', ExpandConstant('{sd}\Temp\PageValues.conf') );  
    end
    else
    begin
        Result := '9443';
    end;
end;

function GetRemapPath(Param: String) : String;
begin
      Result := '/home/svsystems "'+ExpandConstant('{code:Getdircx}')+'"';
end;
				
function SilentOptionsValidation(): Boolean;
var
RootErrorCode, UserErrorCode : Integer;
MySQLRootPasswd, MySQLUserPasswd : String;
begin
	Result:=True;
  if (not (UpgradeCheck)) then
  begin
      if WizardSilent then
    	begin
      InitialSettings;
      
      if (ServerMode = 'PS') or (ServerMode = 'ps') or (ServerMode = 'Ps') then
      begin
          Result := True;
          Log('User has passed ''/ServerMode PS'' option in silent installation.');
          SetIniString( 'CX_Type', 'Type','2', ExpandConstant('{sd}\Temp\PageValues.conf') );
          PSIPValidation;
          
    	if IsSilentOptionSelected('CSIP') then
    	begin
    		Log('User has passed ''/CSIP'' option in silent installation.');
    		if (not (ValidateInput(CSIP,'IP'))) then
              begin
                  Log('Please pass valid CSIP Address. Aborting installation.');
                  SaveInstallLog();
                  ExitProcess(35);
              end
    		else
    			SetIniString( 'PSONLY', 'PS_CS_IP_Value',CSIP, ExpandConstant('{sd}\Temp\PageValues.conf') ); 
    	end
    	else if not IsSilentOptionSelected('CSIP') then
          begin
              Log('User has not passed ''/CSIP'' option in silent installation. Aborting...');
              SaveInstallLog();
              ExitProcess(35);
          end;
              SetIniString( 'PSONLY', 'PS_CS_SECURED_COMMUNICATION','1', ExpandConstant('{sd}\Temp\PageValues.conf') );
              SetIniString( 'PSONLY', 'Https','1', ExpandConstant('{sd}\Temp\PageValues.conf') );
        
    		if IsSilentOptionSelected('CSPort') then
        begin
            Log('User has passed ''/CSPort'' option in silent installation.');
            SetIniString( 'PSONLY', 'PS_CS_PORT_Value',CSPort, ExpandConstant('{sd}\Temp\PageValues.conf') );
            Result := True;   
        end
        else
        begin
            CSPort := '443';
            Log('User has not passed ''/CSPort'' option in silent installation. Hence taking ''443'' as default port.');
            SetIniString( 'PSONLY', 'PS_CS_PORT_Value',CSPort, ExpandConstant('{sd}\Temp\PageValues.conf') );
        end;
        DataTransferSecurePortValidation;  
      end
      else if (ServerMode = 'CS') or (ServerMode = 'cs') or (ServerMode = 'Cs') or not IsSilentOptionSelected('ServerMode') then
      begin
          Result := True;
          if (ServerMode = 'CS') or (ServerMode = 'cs') or (ServerMode = 'Cs') then
              Log('User has passed ''/ServerMode CS'' option in silent installation.')
          else if not IsSilentOptionSelected('ServerMode') then
              Log('User has not passed ''/ServerMode'' option in silent installation. Hence taking both CS as default.');
          SetIniString( 'CX_Type', 'Type','3', ExpandConstant('{sd}\Temp\PageValues.conf') );
          SetIniString( 'CSONLY', 'CS_IP_Value',CSMachineIP, ExpandConstant('{sd}\Temp\PageValues.conf') );          
          PSIPValidation;
          AzureIPValidation;
          SetIniString( 'CSONLY', 'CS_SECURED_COMMUNICATION','1', ExpandConstant('{sd}\Temp\PageValues.conf') );
          SetIniString( 'CSONLY', 'Https','1', ExpandConstant('{sd}\Temp\PageValues.conf') );
          SetIniString( 'CSONLY', 'CS_Https_Port_Value','443', ExpandConstant('{sd}\Temp\PageValues.conf') );
          SetIniString( 'CSONLY', 'CS_SECURED_MODE','Https', ExpandConstant('{sd}\Temp\PageValues.conf') );
         
          if not InstallMySQL then
          begin
              SaveInstallLog();
              ExitProcess(32);
          end;

          if not IsSilentOptionSelected('MySQLPasswordFile') then
          begin
              Log('User has not passed ''/MySQLPasswordFile'' option in silent installation. Aborting the installation.'); 
              SaveInstallLog();
              ExitProcess(36);
          end
          else
          begin
              
              Result := True;
          end;
          DataTransferSecurePortValidation;

          MySQLRootPass := GetIniString( 'MySQLCredentials', 'MySQLRootPassword','', ExpandConstant(MySQLPasswordFile) );
          MySQLUserPass := GetIniString( 'MySQLCredentials', 'MySQLUserPassword','', ExpandConstant(MySQLPasswordFile) );
          SetIniString( 'MPWD', 'MySQL_Root_Password',MySQLRootPass, ExpandConstant('{sd}\Temp\PageValues.conf') );
          SetIniString( 'MPWD', 'MySQL_User_Password',MySQLUserPass, ExpandConstant('{sd}\Temp\PageValues.conf') );
          StringChangeEx(MySQLRootPass,'"', '\"',True);
          StringChangeEx(MySQLUserPass,'"', '\"',True);
          
          if not IsSilentOptionSelected('Language') then
          begin
              Log('User has not passed ''/Language'' option in silent installation. Considering en-US as the default language code.');
              Language_Code := 'en-US';   
          end
          else
          begin
              InitialSettings;
              if not ((Language = 'English') or (Language = 'Portuguese') or (Language = 'German') or (Language = 'Spanish') or (Language = 'French') or (Language = 'Hungarian') or (Language = 'Italian') or (Language = 'Japanese') or (Language = 'Korean') or (Language = 'Russian') or (Language = 'Polish') or (Language = 'Portuguese (Brazil)') or (Language = 'Simplified Chinese') or (Language = 'Traditional Chinese') or (Language = 'english') or (Language = 'portuguese') or (Language = 'german') or (Language = 'spanish') or (Language = 'french') or (Language = 'hungarian') or (Language = 'italian') or (Language = 'japanese') or (Language = 'korean') or (Language = 'russian') or (Language = 'polish') or (Language = 'portuguese (brazil)') or (Language = 'cimplified chinese') or (Language = 'traditional chinese')) then
              begin
                  Log('User has not passed correct value for ''/Language'' option in silent installation.');
              end
              else
              begin
                  Log('In silent installation, user has passed /Language '+ Language +' as an argument.');
                  Language_Selected := Language;
                  
           				// Invoking LanguageCode function to get the corresponding language code of user selected language.
                  LanguageCode;
                  Log('The corresponding language code in silent installation is '+ Language_Code +'');
                  Result := True;
              end;
          end;
  
      end;
      end;
  end;
end;

// Install Microsoft Visual C++ 2013 Redistributable (X86) package.
procedure InstallVCRedistributable2013();
var
  VC2013ResultCode: Integer;
begin
    if IsSilentOptionSelected('SkipMSVC2013Installation') then
    begin
        Log('Skipping Microsoft Visual C++ 2013 Redistributable (X86) package installation.');
    end
    else
    begin
        ExtractTemporaryFile('vcredist_x86.exe');
        ShellExec('', ExpandConstant('{cmd}'),'/C ' + ExpandConstant('{tmp}\vcredist_x86.exe') + ' /q /norestart','', SW_HIDE, ewWaitUntilTerminated, VC2013ResultCode);
        Log('Microsoft Visual C++ 2013 Redistributable (X86) installation return code: '+IntToStr(VC2013ResultCode));
        if (VC2013ResultCode = 0) or (VC2013ResultCode = 1638) then
        begin
          Log('Successfully installed Microsoft Visual C++ 2013 Redistributable (X86).');
        end
        else
        begin
          Log('Microsoft Visual C++ 2013 Redistributable (X86) installation failed.');
          SaveInstallLog();
          ExitProcess(45);
        end;
    end;
end;

// Install Microsoft Visual C++ 2015 Redistributable (X86) package.
procedure InstallVCRedistributable2015();
var
  VC2015ResultCode: Integer;
begin
    if IsSilentOptionSelected('SkipMSVC2015Installation') then
    begin
        Log('Skipping Microsoft Visual C++ 2015 Redistributable (X86) package installation.');
    end
    else
    begin
        ExtractTemporaryFile('vc_redist.x86.exe');
        ShellExec('', ExpandConstant('{cmd}'),'/C ' + ExpandConstant('{tmp}\vc_redist.x86.exe') + ' /q /norestart','', SW_HIDE, ewWaitUntilTerminated, VC2015ResultCode);
        Log('Microsoft Visual C++ 2015 Redistributable (X86) installation return code: '+IntToStr(VC2015ResultCode));
        if (VC2015ResultCode = 0) or (VC2015ResultCode = 1638) then
        begin
          Log('Successfully installed Microsoft Visual C++ 2015 Redistributable (X86).');
        end
        else
        begin
          Log('Microsoft Visual C++ 2015 Redistributable (X86) installation failed.');
          SaveInstallLog();
          ExitProcess(46);
        end;
    end;
end;

procedure RemoveAuthenticatedUsersPermissions();
begin
	Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('C:\ProgramData\ASR\home\svsystems')+'" /remove "Authenticated Users" /T /C /L /Q','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
	if (not(Errorcode=0)) then
	begin
		Log('Unable to remove authenticated users permissions on svsystems directory. Aborting installation.');
		ExitInstallation();
		ExitProcess(13);
	end
	else
		Log('Permissions removal for authenticated users completed successfully on svsystems directory.');

	Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('C:\ProgramData\ASR\home\svsystems\var')+'" /remove "Authenticated Users" /T /C /L /Q','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
	if (not(Errorcode=0)) then
	begin
	  Log('Unable to remove authenticated permissions on svsystems/var directory. Aborting installation.');
	  ExitInstallation();
	  ExitProcess(13);
	end
	else
	  Log('Permissions removal for authenticated users completed successfully on svsystems var directory.');
end;

function InitializeSetup(): Boolean;
var
    FileName: AnsiString;
    sPrevPath, MySQL_Data_Path  : String;
    Path: String;
    FreeSpaceinMB, TotalSpace, MinSpace, FreeSpaceinGB: Cardinal;
begin
	Result:=True;
  
 	sPrevPath := '';
  InitialSettings;

  // If PSIP argument is passed, use the PSIP as CSMachineIP 
  // else fetch the IP from the machine.
  if IsSilentOptionSelected('PSIP') then
  begin
      Log('Using PSIP as CSMachineIP.');
      CSMachineIP := PSIP;
  end
  else
  begin
      Log('Fetching CSMachineIP from the machine.');
      CSMachineIP := Trim(MachineIPAddress('0'));
  end;

  if (not (ValidateInput(CSMachineIP,'IP'))) then
  begin
      Log('IP validation has failed. Aborting installation.');
      SaveInstallLog();
      ExitProcess(33);
  end;
	
  //Captures the StartTime of the installation in a globally declared variable.
  Installation_StartTime := GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':');

  IsCurrentAppRunning;
  
  // Blocking interactive installation.
  if not WizardSilent then
  begin
       SuppressibleMsgBox('Interactive installation is not supported. Please try with silent options.', mbCriticalError, MB_OK, MB_OK);
       Abort;
  end;

  // Abort installation if same or higher version of build is installed.
  if VerifySameOrHigherVersion then
	begin
      SuppressibleMsgBox('Upgrade is not supported as same or higher version of the software is already installed on the server.', mbCriticalError, MB_OK, MB_OK);
      SaveInstallLog();
      ExitProcess(64);
  end;

  CustomDeleteFile(ExpandConstant('{sd}\Temp\PageValues.conf'));
  CustomDeleteFile(ExpandConstant('{sd}\Temp\Install_Paths.conf'));
  CustomDeleteFile(ExpandConstant('{sd}\Temp\nicdetails.conf'));
  CustomDeleteFile(ExpandConstant('{sd}\Temp\IPAddress.txt'));
  CustomDeleteFile(ExpandConstant('{sd}\Temp\NicName.txt'));
	
	InstallVCRedistributable2013();
  InstallVCRedistributable2015();
  
	if  RegQueryStringValue( HKLM,'Software\InMage Systems\Installed Products\9','Build_Version',sPrevpath)then
	begin
      if VerifyPreviousVersion then
      begin
          RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\10','CX_TP_Version',CX_TP_Version);
          if not (Pos(ExpandConstant('{#VERSION}'), CX_TP_Version)> 0) then
          begin
              FileName := ExpandConstant('{src}\Microsoft-ASR')+'_CX_TP_'+ExpandConstant('{#PRODUCTVERSION}')+'_Windows_'+ExpandConstant('{#BUILDPHASE}')+'_'+ExpandConstant('{#BUILDDATE}')+'_'+ExpandConstant('{#Configuration}')+'.exe';
              If (Not FileExists(FileName)) Then
              begin
                SuppressibleMsgBox('Please upgrade ''Microsoft Azure Site Recovery Configuration/Process Sever Dependencies'' before proceeding with ''Microsoft Azure Site Recovery Configuration/Process Server'' upgrade.', mbCriticalError, MB_OK, MB_OK);
                SaveInstallLog();
                ExitProcess(38);
              end
              else
              begin
                Exec(FileName,'','',SW_HIDE,ewWaitUntilTerminated, ResultCode);
                RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\10','CX_TP_Version',CX_TP_Version);
                if not (Pos(ExpandConstant('{#VERSION}'), CX_TP_Version)> 0) then
                begin
                  SuppressibleMsgBox('Please upgrade ''Microsoft Azure Site Recovery Configuration/Process Sever Dependencies'' before proceeding with ''Microsoft Azure Site Recovery Configuration/Process Server'' upgrade.', mbCriticalError, MB_OK, MB_OK);
                  SaveInstallLog();
                  ExitProcess(38);
                end;
              end;
          end;

          if GetValue('CX_TYPE') = '3' then
          begin
              if WizardSilent then
              begin
                  if IsSilentOptionSelected('upgrade') then
                  begin
                     UpgradeCX := 'IDYES'
                     Log('Configuration/Process Server is already existing in this system. Upgrading to '+ExpandConstant('{#BUILDDATE}')+' version.');
                     Log('Stopping Configuration Server services.');
                     if not StopCXServices then
                     begin
                          StartCXServices;
                          SaveInstallLog();
                          ExitProcess(19);
                     end;
                     
                     Log('Sleeping for 2 minutes after stopping CX services.');
                     sleep(120000)
                  end
                  else
                  begin
                      Log('Configuration/Process Server is already existing in this system. User did not pass ''/upgrade'' option.');
                      Log('The options that can be passed are' #13 '/verysilent /suppressmsgboxes /upgrade');
                      SaveInstallLog();
                      ExitProcess(39);
                  end;
              end;
          end
          else
          begin
              if WizardSilent then
              begin
                  if IsSilentOptionSelected('upgrade') then
                  begin
                     UpgradeCX := 'IDYES'
                     Log('Process Server is already existing in this system. Upgrading to '+ExpandConstant('{#BUILDDATE}')+' version.');
                     Log('Stopping Process Server services.');
                     if not StopCXServices then
                     begin
                          StartCXServices;
                          SaveInstallLog();
                          ExitProcess(19);
                     end;
                  end
                  else
                  begin
                      Log('Process Server is already existing in this system. User did not pass ''/upgrade'' option.');
                      Log('The options that can be passed are' #13 '/verysilent /suppressmsgboxes /upgrade');
                      SaveInstallLog();
                      ExitProcess(39);
                  end;
              end;
          end;
      end;
	end
	else if not ((Fileexists(ExpandConstant('C:\strawberry\perl\bin\perl.exe'))) and (Fileexists(ExpandConstant('C:\thirdparty\php5nts\php.exe'))) and (RegKeyExists(HKLM, 'Software\InMage Systems\Installed Products\10'))) then 
	begin
      FileName := ExpandConstant('{src}\Microsoft-ASR')+'_CX_TP_'+ExpandConstant('{#PRODUCTVERSION}')+'_Windows_'+ExpandConstant('{#BUILDPHASE}')+'_'+ExpandConstant('{#BUILDDATE}')+'_'+ExpandConstant('{#Configuration}')+'.exe';
    
      If (Not FileExists(FileName)) Then
      begin
        SuppressibleMsgBox('Please install ''Microsoft Azure Site Recovery Configuration/Process Sever Dependencies'' before proceeding with ''Microsoft Azure Site Recovery Configuration/Process Server'' installation.', mbCriticalError, MB_OK, MB_OK);
        SaveInstallLog();
        ExitProcess(40);
      end
      else
      begin
        Exec(FileName,'','',SW_HIDE,ewWaitUntilTerminated, ResultCode);
        if not ((Fileexists(ExpandConstant('C:\strawberry\perl\bin\perl.exe'))) and (Fileexists(ExpandConstant('C:\thirdparty\php5nts\php.exe'))) and (RegKeyExists(HKLM, 'Software\InMage Systems\Installed Products\10'))) then
        begin
          SuppressibleMsgBox('Please install ''Microsoft Azure Site Recovery Configuration/Process Sever Dependencies'' before proceeding with ''Microsoft Azure Site Recovery Configuration/Process Server'' installation.', mbCriticalError, MB_OK, MB_OK);
          SaveInstallLog();
          ExitProcess(40);
        end;
      end;
	end;

	if WizardSilent then
	begin
    	if not (UpgradeCheck) then
    	begin
        	InitialSettings;
        	if SilentOptionsValidation then
        	   Result := True
        	else
        	   Result := False;
      end;
  end;

  // Check for MySQL during fresh installation.
  RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','DataLocation',MySQL_Data_Path);
  if not (UpgradeCheck) then
  begin
      // Stop MySQL service and remove remnant references of svsdb1 folder.
      if DirExists(ExpandConstant(MySQL_Data_Path+'data\svsdb1')) then
      begin
          if ServiceStopInLoop('MySQL') then
          begin
              if not CustomDeleteDirectory(ExpandConstant(MySQL_Data_Path+'data\svsdb1')) then
              begin
                  Log('Traces of MySQL database of previous installation is existing in this system. Please remove svsdb1 database manually and try the installation again.');
                  SaveInstallLog();
                  ExitProcess(41);
              end;
          end
          else
          begin
              Log('Traces of MySQL database of previous installation is existing in this system. Please remove svsdb1 database manually and try the installation again.');
              SaveInstallLog();
              ExitProcess(42);
          end;
      end;

      // Remove MySQL folder from system if it exits before fresh installation.
      if not Validate_MySQL then
      begin
          CustomDeleteDirectory(ExpandConstant('{sd}\ProgramData\MySQL'));
      end;
  end;

  // Update PageValues.conf during fresh installation.
  if not (UpgradeCheck) then
  begin
      if RegKeyExists(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Gallery Image\CS') then
      begin
          SetIniString( 'CX_Type', 'Type','3', ExpandConstant('{sd}\Temp\PageValues.conf') );
          SetIniString( 'CSONLY', 'CS_IP_Value',CSMachineIP, ExpandConstant('{sd}\Temp\PageValues.conf') );
          SetIniString( 'CSONLY', 'CS_SECURED_COMMUNICATION','1', ExpandConstant('{sd}\Temp\PageValues.conf') );
          SetIniString( 'CSONLY', 'Https','1', ExpandConstant('{sd}\Temp\PageValues.conf') );
          SetIniString( 'CSONLY', 'CS_Https_Port_Value','443', ExpandConstant('{sd}\Temp\PageValues.conf') );
          SetIniString( 'CSONLY', 'CS_SECURED_MODE','Https', ExpandConstant('{sd}\Temp\PageValues.conf') );
      end;
      SetIniString( 'Install_Paths', 'Cygwin_Path',ExpandConstant('C:\thirdparty\Cygwin'), ExpandConstant('{sd}\Temp\Install_Paths.conf') );
      SetIniString( 'Install_Paths', 'RRDTOOL_PATH',ExpandConstant('C:\thirdparty\rrdtool-1.2.15-win32-perl58'), ExpandConstant('{sd}\Temp\Install_Paths.conf') );
      SetIniString( 'Install_Paths', 'RRD_PATH',ExpandConstant('C:\thirdparty\rrdtool-1.2.15-win32-perl58\rrdtool\bindings\perl-shared'), ExpandConstant('{sd}\Temp\Install_Paths.conf') );
  end;

  // Set permissions on rrdtool folder.
  Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('C:\thirdparty\rrdtool-1.2.15-win32-perl58\rrdtool\Release\')+'*" /grant everyone:(F)','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
  if (not(Errorcode=0)) then
  begin
      Log('Unable to change permissions of "'+ExpandConstant('C:\thirdparty\rrdtool-1.2.15-win32-perl58\rrdtool\Release\*')+'" directory. Aborting installation.');
      SaveInstallLog();
      ExitProcess(13);
  end
  else
      Log('Permissions changes completed successfully on "'+ExpandConstant('C:\thirdparty\rrdtool-1.2.15-win32-perl58\rrdtool\Release\*')+'" directory.');

  if UpgradeCheck then
	begin
		//Delete unused directories
		CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\system'));
		CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\system\var'));
		CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\admin\web\SourceMBR'));
		CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\var\cli'));
		CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\rr\backup'));
		CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\rr\restore'));

		//Remove authenticated users permissions on svsystems and var folders before initialize setup.
		RemoveAuthenticatedUsersPermissions();
      CustomCreateDirectory(ExpandConstant('{code:Getdircx}\backup'));
      
      if (GetValue('CX_TYPE') = '3') then
      begin
         CustomCreateDirectory(ExpandConstant('{code:Getdircx}\var\cs_db_backup'));
      end;
	   
      CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\transport\data\tstdata'));
      CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\pm\Mail'));
      FileCopy(ExpandConstant ('{code:Getdircx}\etc\amethyst.conf'), ExpandConstant ('{code:Getdircx}\backup\amethyst.conf'), FALSE);
      FileCopy(ExpandConstant ('{code:Getdircx}\etc\version'), ExpandConstant ('{code:Getdircx}\backup\version'), FALSE);
      FileCopy(ExpandConstant ('{code:Getdircx}\transport\cxps.conf'), ExpandConstant ('{code:Getdircx}\backup\cxps.conf'), FALSE);
      ExtractTemporaryFile('clean_stale_process.pl');
      Exec(ExpandConstant('C:\strawberry\perl\bin\perl.exe'),ExpandConstant('{tmp}\clean_stale_process.pl'), '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
      sleep(120000);
	end;
end;

function GetIPaddress(Param: String) : String;
begin
    if IniKeyExists('CX_Type', 'Type', ExpandConstant('{sd}\Temp\PageValues.conf')) then
    begin
          CX_TYPE_Value := GetIniString( 'CX_Type', 'Type','', ExpandConstant('{sd}\Temp\PageValues.conf'));
          case (StrToInt(CX_TYPE_Value)) of
              3: 
              begin
                    Result := GetIniString( 'CSONLY', 'CS_IP_Value','', ExpandConstant('{sd}\Temp\PageValues.conf'));
              end;
              2:
              begin
                    Result := GetIniString( 'PSONLY', 'PS_CS_IP_Value','', ExpandConstant('{sd}\Temp\PageValues.conf'));
              end;  
          end;           
    end;
end;

procedure RunPushinstallBinary();
var
HostGuid, PushHostid : AnsiString;
ErrorCode : Integer;
begin
    if (not (UpgradeCheck)) then
    begin
        HostGuid := GetOrGenerateGUID(HOSTGUID);
        PushHostid := ExpandConstant('{code:Getdircx}\pushinstallsvc\pushinstaller.conf');
                
        AddOrReplaceLine(PushHostid,'Hostid','Hostid = "' + HostGuid + '"');
        AddOrReplaceLine(PushHostid,'InstallDirectory','InstallDirectory = "' + ExpandConstant('{code:Getdircx}') + '\pushinstallsvc"');
        Log('Invoking PushInstall binary.')
        Exec(ExpandConstant('{code:Getdircx}\pushinstallsvc\PushInstall.exe'), '-i -u ".\LocalSystem" -p ""','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
        if (ErrorCode=0) then
            Log('PushInstall binary executed successfully.')
        else
        begin
            Log('PushInstall binary execution failed.');
            ExitInstallation();
            ExitProcess(43);
        end;
    end;
end;

Function RollBackInstallation(): Boolean; 
var 
    ErrorCode : Integer;
begin
    Log('Rolling back the install changes.');

    if not UpgradeCheck then
    begin
        Exec(ExpandConstant('{cmd}'), '/C "' + ExpandConstant('{code:Getdircx}\unins000.exe') + '" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
        if (ErrorCode = 0) then
            Log('Roll back completed successfully.')
        else
        begin
            Log('Unable to roll back install changes.');
            SaveInstallLog();
            ExitProcess(12);
        end;
    end;

    if UpgradeCheck then
    begin
        Log('Upgrade has failed.');
    end;

    SaveInstallLog();
end;

Procedure SetPermissions();
begin
    Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('{code:Getdircx}\bin')+'" /inheritance:r /grant:r "BUILTIN\Administrators:(OI)(CI)F" /grant:r "NT AUTHORITY\SYSTEM:(OI)(CI)F" /grant:r "NT AUTHORITY\IUSR:(OI)(CI)F"','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
    if (not(Errorcode=0)) then
    begin
        Log('Unable to change permissions of "'+ExpandConstant('{code:Getdircx}\bin')+'" directory. Aborting installation.');
        RollBackInstallation();
        ExitProcess(13);
    end
    else
        Log('Permissions changes completed successfully on "'+ExpandConstant('{code:Getdircx}\bin')+'" directory.');

    Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('{code:Getdircx}\transport')+'" /inheritance:r /grant:r "BUILTIN\Administrators:(OI)(CI)F" /grant:r "NT AUTHORITY\SYSTEM:(OI)(CI)F" /grant:r "NT AUTHORITY\IUSR:(OI)(CI)F"','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
    if (not(Errorcode=0)) then
    begin
        Log('Unable to change permissions of "'+ExpandConstant('{code:Getdircx}\transport')+'" directory. Aborting installation.');
        RollBackInstallation();
        ExitProcess(13);
    end
    else
        Log('Permissions changes completed successfully on "'+ExpandConstant('{code:Getdircx}\transport')+'" directory.');

    Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('{code:Getdircx}\admin\web\BIN')+'" /inheritance:r /grant:r "BUILTIN\Administrators:(OI)(CI)F" /grant:r "NT AUTHORITY\SYSTEM:(OI)(CI)F" /grant:r "NT AUTHORITY\IUSR:(OI)(CI)F" /grant:r "BUILTIN\Users:(OI)(CI)RX"','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
    if (not(Errorcode=0)) then
    begin
        Log('Unable to change permissions of "'+ExpandConstant('{code:Getdircx}\admin\web\BIN')+'" directory. Aborting installation.');
        RollBackInstallation();
        ExitProcess(13);
    end
    else
        Log('Permissions changes completed successfully on "'+ExpandConstant('{code:Getdircx}\admin\web\BIN')+'" directory.');

    if not UpgradeCheck then
    begin
      Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('{code:Getdircx}')+'" /grant "IUSR:(OI)(CI)F"','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
      if (not(Errorcode=0)) then
      begin
          Log('Unable to change permissions of install directory. Aborting installation.');
          ExitInstallation();
          ExitProcess(13);
      end
      else
          Log('Permissions changes completed successfully on install directory.');
			  
			Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('C:\Temp')+'" /grant "IUSR:(OI)(CI)F"','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
      if (not(Errorcode=0)) then
      begin
          Log('Unable to change permissions of C:\Temp directory. Aborting installation.');
          ExitInstallation();
          ExitProcess(13);
      end
      else
          Log('Permissions changes completed successfully on C:\Temp directory.');
    end;
			  
    Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('C:\thirdparty\php5nts')+'" /grant "IUSR:(OI)(CI)F"','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
    if (not(Errorcode=0)) then
    begin
        Log('Unable to change permissions of C:\thirdparty\php5nts directory. Aborting installation.');
        SaveInstallLog();
        ExitProcess(13);
    end
    else
        Log('Permissions changes completed successfully on C:\thirdparty\php5nts directory.');
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
 param, actual_path, DBHost, HostGuid, HTTPPort,PushHostid,InstallPath,INSDATE,INSTIME : AnsiString;
	ResultCode, I, Posnum : Integer;
	Names: TArrayOfString;
	OldRegistryValue,WEB_ROOT_PATH,NewRegistryValue,MySQL_Root_Password, MySQL_User_Password, CustomPath, path_change, MySQL_Install_Binary : String;
	
	MySQL_Path, MySQL_Data_Path, Cygwin_Path, RRDTOOL_PATH, RRD_PATH, DB_ROOT_PASSWD : String;
	  Errorcode: Integer;
begin
	if CurStep = ssInstall then
	begin
        if (UpgradeCheck) then
        begin	
            CurrentBuildVersion := '{#APPVERSION}';

            if (DirExists(ExpandConstant('{code:Getdircx}\Release_')+CurrentBuildVersion)) then
            begin
                Log('The directory ' + ExpandConstant('{code:Getdircx}\Release_')+CurrentBuildVersion+' already exists. Deleting contents.');
                if DelTree(ExpandConstant('{code:Getdircx}\Release_')+CurrentBuildVersion+'\*', False, True, True) then
                  Log('Successfully deleted '+ ExpandConstant('{code:Getdircx}\Release_')+CurrentBuildVersion+ ' contents.')
                else
                  Log('Unable to delete ' + ExpandConstant('{code:Getdircx}\Release_')+CurrentBuildVersion+ ' contents.')
            end
            else 
            begin
                CustomCreateDirectory(ExpandConstant('{code:Getdircx}\Release_')+CurrentBuildVersion);
            end;

            if RenameFile(ExpandConstant('{code:Getdircx}\patch.log'), ExpandConstant('{code:Getdircx}\Release_'+CurrentBuildVersion+'\patch.log.old')) then
               Log('Successfully renamed patch.log to patch.log.old.')
            else
               Log('Unable to rename patch.log to patch.log.old.'); 
            if FileCopy(ExpandConstant('{code:Getdircx}\unins000.dat'), ExpandConstant('{code:Getdircx}\Release_'+CurrentBuildVersion+'\backup_unins000.dat'), FALSE) then
               Log('Successfully copied unins000.dat to backup_unins000.dat.')
            else
                Log('Unable to copy unins000.dat to backup_unins000.dat.');
            if RenameFile(ExpandConstant('{code:Getdircx}\backup_updates'), ExpandConstant('{code:Getdircx}\backup_updates_old')) then
               Log('Successfully renamed backup_updates to backup_updates_old.')
            else
                Log('Unable to rename backup_updates to backup_updates_old.');

            if DelTree(ExpandConstant('{code:Getdircx}\pushinstallsvc\repository'), False, True, True) then
                  Log('Successfully deleted all contents from pushinstallsvc\repository path.')
            else
                  Log('Unable to delete all contents from pushinstallsvc\repository path.');

            if not PSOnlySelected then
            begin
                if DelTree(ExpandConstant('{code:Getdircx}\admin\web\sw'), False, True, True) then
                      Log('Successfully deleted all contents from admin\web\sw path.')
                else
                      Log('Unable to delete all contents from admin\web\sw path.');
            end;
        end;
	end;

	if CurStep = ssPostInstall then 
	begin
      		if (not (UpgradeCheck())) then
      		begin
          		MySQL_Path := GetIniString( 'Install_Paths', 'MySQL_Path','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
          		MySQL_Data_Path := GetIniString( 'Install_Paths', 'MySQL_Data_Path','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
          		Cygwin_Path := GetIniString( 'Install_Paths', 'Cygwin_Path','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
          		RRDTOOL_PATH := GetIniString( 'Install_Paths', 'RRDTOOL_PATH','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
          		RRD_PATH := GetIniString( 'Install_Paths', 'RRD_PATH','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
          		
          		if (Length(MySQL_Path) > 0) and (MySQL_Path[Length(MySQL_Path)] = '\') then
              begin
                while (Length(MySQL_Path) > 0) and (MySQL_Path[Length(MySQL_Path)] = '\') do
                  Delete(MySQL_Path, Length(MySQL_Path), 1);
              end;
              
              if (Length(MySQL_Data_Path) > 0) and (MySQL_Data_Path[Length(MySQL_Data_Path)] = '\') then
              begin
                while (Length(MySQL_Data_Path) > 0) and (MySQL_Data_Path[Length(MySQL_Data_Path)] = '\') do
                  Delete(MySQL_Data_Path, Length(MySQL_Data_Path), 1);
              end;
          
          		if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'MYSQL_PATH','MYSQL_PATH = "' + MySQL_Path + '"') then
                  Log('Successfully added MYSQL_PATH to amethyst.conf file.')
              else
              begin
                  Log('Unable to add MYSQL_PATH to amethyst.conf file. Aborting installation.');
                  RollBackInstallation();
                  ExitProcess(14);
              end;
          		if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'MYSQL_DATA_PATH','MYSQL_DATA_PATH = "' + MySQL_Data_Path + '"') then
                  Log('Successfully added MYSQL_DATA_PATH to amethyst.conf file.')
              else
              begin
                  Log('Unable to add MYSQL_DATA_PATH to amethyst.conf file. Aborting installation.');
                  RollBackInstallation();
                  ExitProcess(14);
              end;
          		if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'CYGWIN_PATH','CYGWIN_PATH = "' + Cygwin_Path + '"') then
                  Log('Successfully added CYGWIN_PATH to amethyst.conf file.')
              else
              begin
                  Log('Unable to add CYGWIN_PATH to amethyst.conf file. Aborting installation.');
                  RollBackInstallation();
                  ExitProcess(14);
              end;
          		if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'RRDTOOL_PATH','RRDTOOL_PATH = "' + RRDTOOL_PATH + '"') then
                  Log('Successfully added RRDTOOL_PATH to amethyst.conf file.')
              else
              begin
                  Log('Unable to add RRDTOOL_PATH to amethyst.conf file. Aborting installation.');
                  RollBackInstallation();
                  ExitProcess(14);
              end;
          		if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'RRD_PATH','RRD_PATH = "' + RRD_PATH + '"') then
                  Log('Successfully added RRD_PATH to amethyst.conf file.')
              else
              begin
                  Log('Unable to add RRD_PATH to amethyst.conf file. Aborting installation.');
                  RollBackInstallation();
                  ExitProcess(14);
              end;
          		if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'PHP_PATH','PHP_PATH = "'+ExpandConstant('C:\thirdparty\php5nts')+'"') then
                  Log('Successfully added PHP_PATH to amethyst.conf file.')
              else
              begin
                  Log('Unable to add PHP_PATH to amethyst.conf file. Aborting installation.');
                  RollBackInstallation();
                  ExitProcess(14);
              end;
          		
              if not PSOnlySelected then
              begin
              		if RegQueryMultiStringValue(HKEY_LOCAL_MACHINE,'System\CurrentControlSet\Services\tmansvc','DependOnService',OldRegistryValue) then 
              		begin
              			StringChange(OldRegistryValue,#0,'SEP');
              			NewRegistryValue := OldRegistryValue + 'MySQL' + 'SEP';
              			StringChange(NewRegistryValue,'SEP',#0);
              			RegWriteMultiStringValue(HKEY_LOCAL_MACHINE,'System\CurrentControlSet\Services\tmansvc','DependOnService',NewRegistryValue);
              		end;
              end;
      
          		{ Add the valuename TcpWindowSize and valuedata as dword 1048576 for CX performance improvements }
          		if RegGetSubkeyNames(HKEY_LOCAL_MACHINE, 'SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\Interfaces', Names) then 
          		begin
          		    	for I := 0 to GetArrayLength(Names)-1 do 
          		    	begin
          			         RegWriteDWordValue(HKEY_LOCAL_MACHINE, 'SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\Interfaces\' + Names[I], 'TcpWindowSize', 1048576);
          			    end;
          		end;
    
          		{ Fix for bug 5722. Fix by Samyuktha. Reviewed by Chaitanya. }
          		if FileExists(ExpandConstant('{code:Getdircx}\etc\amethyst.conf')) then
          		begin
          			   Web_Root_Path := ExpandConstant('{code:Getdircx}\admin\web');
          			   Volsync_value := '12';
                   FORK_MONITOR_PAIRS_Value := '0'
          			   if ReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'WEB_ROOT', 'WEB_ROOT         = "'+ Web_Root_Path +'"') then
                      Log('Successfully added WEB_ROOT to amethyst.conf file.')
                    else
                    begin
                        Log('Unable to add WEB_ROOT to amethyst.conf file. Aborting installation.');
                        RollBackInstallation();
                        ExitProcess(14);
                    end;
          			   if ReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'MAX_TMID', 'MAX_TMID         = "'+ Volsync_value +'"') then
                      Log('Successfully added MAX_TMID to amethyst.conf file.')
                    else
                    begin
                        Log('Unable to add MAX_TMID to amethyst.conf file. Aborting installation.');
                        RollBackInstallation();
                        ExitProcess(14);
                    end;
          			   if ReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'FORK_MONITOR_PAIRS', 'FORK_MONITOR_PAIRS         = "'+ FORK_MONITOR_PAIRS_Value +'"') then
                      Log('Successfully added FORK_MONITOR_PAIRS to amethyst.conf file.')
                    else
                    begin
                        Log('Unable to add FORK_MONITOR_PAIRS to amethyst.conf file. Aborting installation.');
                        RollBackInstallation();
                        ExitProcess(14);
                    end;
          	  end;
          end;
      		if FileExists(ExpandConstant('{code:Getdircx}\etc\version')) then 
      		begin
          			INSDATE := GetDateTimeString ('dd/mmm/yyyy', '_', #0);
          			INSTIME := GetDateTimeString ('hh:nn:ss', #0, '_');
          			Log('Adding INSTALLATION_DATE=' + INSDATE + ' and INSTALLATION_TIME=' + INSTIME + ' in version file.');
          			SaveStringToFile(ExpandConstant('{code:Getdircx}\etc\version'), #13#10 + 'INSTALLATION_DATE=' + INSDATE + #13#10 + 'INSTALLATION_TIME=' + INSTIME + #13#10 , True);
      	  end;

      		if (not (UpgradeCheck())) then 
      		begin
                PushHostid := ExpandConstant('{code:Getdircx}\pushinstallsvc\pushinstaller.conf');
                  HTTPPort := GetValue('HTTP_PORT');
     				
       				    if IniKeyExists('CX_Type', 'Type', ExpandConstant('{sd}\Temp\PageValues.conf')) then
                  begin
                        CX_TYPE_Value := GetIniString( 'CX_Type', 'Type','', ExpandConstant('{sd}\Temp\PageValues.conf'));

                        case (StrToInt(CX_TYPE_Value)) of
             			      3: 
                        begin
                              CSONLY_CS_IP_Value := GetIniString( 'CSONLY', 'CS_IP_Value','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                              CSONLY_CS_SECURED_COMM := GetIniString( 'CSONLY', 'CS_SECURED_COMMUNICATION','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                              CSONLY_CS_Https_Value := GetIniString( 'CSONLY', 'Https','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                              CS_SECURED_MODE := GetIniString( 'CSONLY', 'CS_SECURED_MODE','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                              CSONLY_Https_Port_Value := GetIniString( 'CSONLY', 'CS_Https_Port_Value','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                              CSONLY_AZURE_IP_Value := GetIniString( 'CSONLY', 'AZURE_IP_Value','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                              PSONLY_PS_IP_Value      := GetIniString( 'PSONLY', 'PS_IP_Value','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                              PSONLY_PS_NIC_Value      := GetIniString( 'PSONLY', 'PS_NIC_Value','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                              HTTPPort       := CSONLY_Https_Port_Value;
                              DBHost         := 'localhost';
                              if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'CX_TYPE','CX_TYPE = "' + CX_TYPE_Value + '"') then
                                  Log('Successfully added CX_TYPE to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add CX_TYPE to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
                              if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'CS_IP','CS_IP = "' + CSONLY_CS_IP_Value + '"') then
                                 Log('Successfully added CS_IP to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add CS_IP to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
                              if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'HTTP_PORT','HTTP_PORT = "' + HTTPPort + '"') then
                                 Log('Successfully added HTTP_PORT to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add HTTP_PORT to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
                              if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'AZURE_IP_Value','IP_ADDRESS_FOR_AZURE_COMPONENTS = "' + CSONLY_AZURE_IP_Value + '"') then
                                 Log('Successfully added IP_ADDRESS_FOR_AZURE_COMPONENTS to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add IP_ADDRESS_FOR_AZURE_COMPONENTS to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
                         			if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'DB_HOST','DB_HOST = "' + DBHost + '"') then
                                 Log('Successfully added DB_HOST to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add DB_HOST to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
                         			if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'CS_SECURED_COMMUNICATION','CS_SECURED_COMMUNICATION = "' + CSONLY_CS_SECURED_COMM + '"') then
                                  Log('Successfully added CS_SECURED_COMMUNICATION to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add CS_SECURED_COMMUNICATION to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
             			      		  if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'CS_PORT','CS_PORT = "' + CSONLY_Https_Port_Value + '"') then
                                 Log('Successfully added CS_PORT to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add CS_PORT to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
             			      		  
             			      		  if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'BPM_NTW_DEVICE','BPM_NTW_DEVICE = "' + PSONLY_PS_NIC_Value + '"') then
                                 Log('Successfully added BPM_NTW_DEVICE to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add BPM_NTW_DEVICE to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
             			      		  if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'PS_IP','PS_IP = "' + PSONLY_PS_IP_Value + '"') then
                                 Log('Successfully added PS_IP to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add PS_IP to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
                              if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'PS_CS_IP','PS_CS_IP = "' + CSONLY_CS_IP_Value + '"') then
                                 Log('Successfully added PS_CS_IP to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add PS_CS_IP to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
                              if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'PS_CS_PORT','PS_CS_PORT = "' + CSONLY_Https_Port_Value + '"') then
                                 Log('Successfully added PS_CS_PORT to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add PS_CS_PORT to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
                              if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'PS_CS_SECURED_COMMUNICATION','PS_CS_SECURED_COMMUNICATION = "' + CSONLY_CS_SECURED_COMM + '"') then
                                 Log('Successfully added PS_CS_SECURED_COMMUNICATION to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add PS_CS_SECURED_COMMUNICATION to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
                              if AddOrReplaceLine(PushHostid,'Https','Https = "' + CSONLY_CS_Https_Value + '"') then
                                 Log('Successfully added Https to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add Https to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
                              if AddOrReplaceLine(PushHostid,'Port','Port = "' + CSONLY_Https_Port_Value + '"') then
                                 Log('Successfully added Port to amethyst.conf file.')
                              else
                              begin
                                  Log('Unable to add Port to amethyst.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;

                              if SetIniString( 'cxps', 'cs_ip_address', CSONLY_CS_IP_Value, ExpandConstant('{code:Getdircx}\transport\cxps.conf') ) then
                                  Log('The cs_ip_address -> '+ CSONLY_CS_IP_Value + ' is successfully updated in the cxps.conf file.')
                              else
                              begin
                                  Log('Unable to add cs_ip_address to cxps.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
                              if SetIniString( 'cxps', 'cs_ssl_port', CSONLY_Https_Port_Value, ExpandConstant('{code:Getdircx}\transport\cxps.conf') ) then
                                  Log('The cs_ssl_port -> '+ CSONLY_Https_Port_Value + ' is successfully updated in the cxps.conf file.')
                              else
                              begin
                                  Log('Unable to add cs_ssl_port to cxps.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;

             			      end;
             			      2:
             			      begin
                         			 PSONLY_PS_IP_Value      := GetIniString( 'PSONLY', 'PS_IP_Value','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                         			 PSONLY_PS_NIC_Value      := GetIniString( 'PSONLY', 'PS_NIC_Value','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                         			 PSONLY_PS_CS_IP_Value   := GetIniString( 'PSONLY', 'PS_CS_IP_Value','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                         			 PSONLY_PS_CS_PORT_Value := GetIniString( 'PSONLY', 'PS_CS_PORT_Value','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                         			 PSONLY_PS_CS_SECURED_COMMUNICATION := GetIniString( 'PSONLY', 'PS_CS_SECURED_COMMUNICATION','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                         			 PSONLY_PS_Https_Value := GetIniString( 'PSONLY', 'Https','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                         		   DBHost                  := PSONLY_PS_CS_IP_Value
                        			 if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'CX_TYPE','CX_TYPE = "' + CX_TYPE_Value + '"') then
                                    Log('Successfully added CX_TYPE to amethyst.conf file.')
                               else
                               begin
                                    Log('Unable to add CX_TYPE to amethyst.conf file. Aborting installation.');
                                    RollBackInstallation();
                                    ExitProcess(14);
                               end;
                               if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'PS_IP','PS_IP = "' + PSONLY_PS_IP_Value + '"') then
                                    Log('Successfully added PS_IP to amethyst.conf file.')
                               else
                               begin
                                    Log('Unable to add PS_IP to amethyst.conf file. Aborting installation.');
                                    RollBackInstallation();
                                    ExitProcess(14);
                               end;
                               if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'BPM_NTW_DEVICE','BPM_NTW_DEVICE = "' + PSONLY_PS_NIC_Value + '"') then
                                    Log('Successfully added BPM_NTW_DEVICE to amethyst.conf file.')
                               else
                               begin
                                    Log('Unable to add BPM_NTW_DEVICE to amethyst.conf file. Aborting installation.');
                                    RollBackInstallation();
                                    ExitProcess(14);
                               end;
                               if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'PS_CS_IP','PS_CS_IP = "' + PSONLY_PS_CS_IP_Value + '"') then
                                    Log('Successfully added PS_CS_IP to amethyst.conf file.')
                               else
                               begin
                                    Log('Unable to add PS_CS_IP to amethyst.conf file. Aborting installation.');
                                    RollBackInstallation();
                                    ExitProcess(14);
                               end;
                               if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'PS_CS_PORT','PS_CS_PORT = "' + PSONLY_PS_CS_PORT_Value + '"') then
                                    Log('Successfully added PS_CS_PORT to amethyst.conf file.')
                               else
                               begin
                                    Log('Unable to add PS_CS_PORT to amethyst.conf file. Aborting installation.');
                                    RollBackInstallation();
                                    ExitProcess(14);
                               end;
                               if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'PS_CS_SECURED_COMMUNICATION','PS_CS_SECURED_COMMUNICATION = "' + PSONLY_PS_CS_SECURED_COMMUNICATION + '"') then
                                    Log('Successfully added PS_CS_SECURED_COMMUNICATION to amethyst.conf file.')
                               else
                               begin
                                    Log('Unable to add PS_CS_SECURED_COMMUNICATION to amethyst.conf file. Aborting installation.');
                                    RollBackInstallation();
                                    ExitProcess(14);
                               end;
                           		 if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'DB_HOST','DB_HOST = "' + DBHost + '"') then
                                    Log('Successfully added DB_HOST to amethyst.conf file.')
                               else
                               begin
                                    Log('Unable to add DB_HOST to amethyst.conf file. Aborting installation.');
                                    RollBackInstallation();
                                    ExitProcess(14);
                               end;
                           		 if AddOrReplaceLine(PushHostid,'Https','Https = "' + PSONLY_PS_Https_Value + '"') then
                                    Log('Successfully added Https to amethyst.conf file.')
                               else
                               begin
                                    Log('Unable to add Https to amethyst.conf file. Aborting installation.');
                                    RollBackInstallation();
                                    ExitProcess(14);
                               end;
                           		 if AddOrReplaceLine(PushHostid,'Port','Port = "' + PSONLY_PS_CS_PORT_Value + '"') then
                                    Log('Successfully added Port to amethyst.conf file.')
                               else
                               begin
                                    Log('Unable to add Port to amethyst.conf file. Aborting installation.');
                                    RollBackInstallation();
                                    ExitProcess(14);
                               end;
                           		 
                              if SetIniString( 'cxps', 'cs_ip_address', PSONLY_PS_CS_IP_Value, ExpandConstant('{code:Getdircx}\transport\cxps.conf') )then
                                  Log('The cs_ip_address -> '+ PSONLY_PS_CS_IP_Value + ' is successfully updated in the cxps.conf file.')
                              else
                              begin
                                  Log('Unable to add cs_ip_address to cxps.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;
                              if SetIniString( 'cxps', 'cs_ssl_port', PSONLY_PS_CS_PORT_Value, ExpandConstant('{code:Getdircx}\transport\cxps.conf') )then
                                  Log('The cs_ssl_port -> '+ PSONLY_PS_CS_PORT_Value + ' is successfully updated in the cxps.conf file.')
                              else
                              begin
                                  Log('Unable to add cs_ssl_port to cxps.conf file. Aborting installation.');
                                  RollBackInstallation();
                                  ExitProcess(14);
                              end;  
                           		 
                        end;            
                  end;
                  actual_path := ExpandConstant('{code:Getdircx}');
      			      
      			        if Fileexists(ExpandConstant('{code:Getdircx}\pushinstallsvc\pushinstaller.conf')) then
      			        begin
      			          Log('Fetching HostGuid from pushinstaller.conf file in case of Process server alone installation.');
      			          HostGuid := GetIniString( 'PushInstaller', 'Hostid','', ExpandConstant('{code:Getdircx}\pushinstallsvc\pushinstaller.conf') );
      			       
                  end;
                  
                  if SetIniString( 'cxps', 'id', HostGuid, ExpandConstant('{code:Getdircx}\transport\cxps.conf') ) then
                      Log('The id is successfully updated in the cxps.conf file.')
                  else
                  begin
                      Log('Unable to add id to cxps.conf file. Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(14);
                  end;
                  if SetIniString( 'cxps', 'install_dir', ExpandConstant('{code:Getdircx}\transport'), ExpandConstant('{code:Getdircx}\transport\cxps.conf') ) then
                      Log('The install_dir is successfully updated in the cxps.conf file.')
                  else
                  begin
                      Log('Unable to add install_dir to cxps.conf file. Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(14);
                  end;
                  if SetIniString( 'cxps', 'allowed_dirs', ExpandConstant('{code:Getdircx}'), ExpandConstant('{code:Getdircx}\transport\cxps.conf') ) then
                      Log('The allowed_dirs is successfully updated in the cxps.conf file.')
                  else
                  begin
                      Log('Unable to add allowed_dirs to cxps.conf file. Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(14);
                  end;

            			if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'HOST_GUID','HOST_GUID = "' + HostGuid + '"') then
                      Log('Successfully added HOST_GUID to amethyst.conf file.')
                  else
                  begin
                      Log('Unable to add HOST_GUID to amethyst.conf file. Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(14);
                  end;
            			if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'CXPS_XFER','CXPS_XFER = 1') then
                      Log('Successfully added CXPS_XFER to amethyst.conf file.')
                  else
                  begin
                      Log('Unable to add CXPS_XFER to amethyst.conf file. Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(14);
                  end;
            			if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'XFER_LOG_FILE','XFER_LOG_FILE = '+ExpandConstant('{code:Getdircx}\transport\log\cxps.xfer.log')) then
                      Log('Successfully added XFER_LOG_FILE to amethyst.conf file.')
                  else
                  begin
                      Log('Unable to add XFER_LOG_FILE to amethyst.conf file. Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(14);
                  end;
            			if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'ACTUAL_INSTALLATION_DIR ','ACTUAL_INSTALLATION_DIR = "' + actual_path + '"') then
                      Log('Successfully added ACTUAL_INSTALLATION_DIR to amethyst.conf file.')
                  else
                  begin
                      Log('Unable to add ACTUAL_INSTALLATION_DIR to amethyst.conf file. Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(14);
                  end;
                  if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'INSTALLATION_DIR ','INSTALLATION_DIR = "' + actual_path + '"') then
                      Log('Successfully added INSTALLATION_DIR to amethyst.conf file.')
                  else
                  begin
                      Log('Unable to add INSTALLATION_DIR to amethyst.conf file. Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(14);
                  end;
            			if AddOrReplaceLine(PushHostid,'Hostid','Hostid = "' + HostGuid + '"') then
                      Log('Successfully added Hostid to amethyst.conf file.')
                  else
                  begin
                      Log('Unable to add Hostid to amethyst.conf file. Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(14);
                  end;
                  
                  MySQL_Root_Password := GetIniString('MPWD', 'MySQL_Root_Password','', ExpandConstant('{sd}\Temp\PageValues.conf'));  
	                MySQL_User_Password := GetIniString('MPWD', 'MySQL_User_Password','', ExpandConstant('{sd}\Temp\PageValues.conf'));
                 	if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'DB_PASSWD ','DB_PASSWD = "' + MySQL_User_Password + '"') then
                      Log('Successfully added DB_PASSWD to amethyst.conf file.')
                  else
                  begin
                      Log('Unable to add DB_PASSWD to amethyst.conf file. Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(14);
                  end;
                 	if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'DB_ROOT_PASSWD ','DB_ROOT_PASSWD = "' + MySQL_Root_Password + '"') then
                      Log('Successfully added DB_ROOT_PASSWD to amethyst.conf file.')
                  else
                  begin
                      Log('Unable to add DB_ROOT_PASSWD to amethyst.conf file. Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(14);
                  end;
                  
                  if not CustomCreateDirectory(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')) then
                  begin
                      RollBackInstallation();
                      ExitProcess(16); 
                  end;

                  if not CustomCreateDirectory(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\Config')) then
                  begin
                      RollBackInstallation();
                      ExitProcess(16);
                  end;

                  if SaveStringToFile(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\Config\App.conf'),'' , False) then
                      Log('Successfully created ''ProgramData\Microsoft Azure Site Recovery\Config\App.conf'' file.')
                  else
                  begin
                      Log('Unable to create ''ProgramData\Microsoft Azure Site Recovery\Config\App.conf'' file. Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(16);
                  end;
                  path_change :=  actual_path;
                  StringChangeEx(path_change,'\home\svsystems','',True);
                  ShellExec('', ExpandConstant('{cmd}'),'/C mklink /J "'+ExpandConstant('{sd}\ProgramData\ASR')+'" "'+path_change + '"','', SW_HIDE, ewWaitUntilTerminated,Errorcode);
                  if (Errorcode = 0) then
                  begin
                      Log('Junction created for "'+ExpandConstant('{sd}\ProgramData\ASR')+'" "'+path_change + '"');
                  end
                  else
                  begin
                      Log('Unable to create Junction for "'+ExpandConstant('{sd}\ProgramData\ASR')+'" "'+path_change + '". Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(15);
                  end;
                  if AddOrReplaceLine(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\Config\App.conf'),'INSTALLATION_PATH ','INSTALLATION_PATH = "' + ExpandConstant('{sd}\ProgramData\ASR') + '"') then
                      Log('INSTALLATION_PATH value has been added in app.conf file.'+ Chr(13))
                  else
                  begin
                      Log('Unable to add INSTALLATION_PATH value in app.conf file. Aborting installation.');
                      RollBackInstallation();
                      ExitProcess(16);
                  end;
      		end;
          DB_ROOT_PASSWD := GetValue('DB_ROOT_PASSWD');
          MySQL_Path := GetIniString( 'Install_Paths', 'MySQL_Path','', ExpandConstant('{sd}\Temp\Install_Paths.conf') );
          MySQL_Install_Binary := ExpandConstant(MySQL_Path+'bin\mysql.exe');
		      
          // Start cxprocessserver service.                     
          ServiceStartInLoop('cxprocessserver');

          // Start MySQL service.
          if ServiceStopInLoop('MySQL') then
          begin
              if not ServiceStartInLoop('MySQL') then
              begin
                  CustomDeleteFile(ExpandConstant(MySQL_Data_Path+'\ib_logfile*'));
                  ServiceStartInLoop('MySQL');
              end;
          end;

      		sleep(10000);
        	
          // Start InMage PushInstall service.
          ServiceStartInLoop('InMage PushInstall');

          // Start W3SVC service.
          if ServiceStopInLoop('W3SVC') then
          begin
              ServiceStartInLoop('W3SVC');
          end;

          // Start tmansvc service.
          if ServiceStopInLoop('tmansvc') then
          begin
              ServiceStartInLoop('tmansvc');
          end;
		  
		  // Start ProcessServer service.
          if ServiceStopInLoop('ProcessServer') then
          begin
              ServiceStartInLoop('ProcessServer');
          end;
          
          // Start LogUpload service.
          ServiceStartInLoop('LogUpload');

		  // Always start ProcessServerMonitor service first to avoid generating alerts if other services are stopped first during install/upgrade/uninstall.
          if ServiceStopInLoop('ProcessServerMonitor') then
          begin
              ServiceStartInLoop('ProcessServerMonitor');
          end;
      
       		//If only PS is installed stopping MYSQL service and setting it maual.
       		if (GetValue('CX_TYPE') = '2') then
       		begin
           		ServiceStopInLoop('MySQL');
           		if ShellExec('', ExpandConstant('{cmd}'),'/C sc config "MySQL" start= demand','', SW_HIDE, ewWaitUntilTerminated,Errorcode) then
           		begin
           		   Log('Successfully set the MySQL service as manual'); 
           		end;
       		end;


       	   if not PSOnlySelected then
       	   begin  
              if not UpgradeCheck then
              begin
                  RRDTOOL_PATH := GetValue('RRDTOOL_PATH');
                  RRD_PATH := GetValue('RRD_PATH');
                  SetEnvironmentVariable('PATH', ExpandConstant('{code:Getdircx}\bin')+';'+ExpandConstant('C:\strawberry\perl\bin')+';'+ExpandConstant('C:\strawberry\c\bin')+';'+ExpandConstant(RRDTOOL_PATH+';')+ExpandConstant(RRD_PATH+';'));
                  Log('Invoking updateAdminPassword.pl script.');
                  ShellExec('',ExpandConstant('C:\strawberry\perl\bin\perl.exe'),'"'+ExpandConstant('{code:Getdircx}\bin\updateAdminPassword.pl')+'" -p "Password~1"', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode);
                  if (Errorcode=2) then
                  begin
                      SuppressibleMsgBox('Configuration Server user interface password could not be updated in database.',mbError, MB_OK, MB_OK);
                      CustomDeleteFile(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\private\CSUIPassword'));
                  end
                  else if (Errorcode=5) then
                  begin
                      SuppressibleMsgBox('Configuration Server user interface password could not be updated in database.',mbError, MB_OK, MB_OK);
                      CustomDeleteFile(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\private\CSUIPassword'));
                  end
                  else 
                  begin
                      Log('CS UI Password updated successfully in database..');
                      CustomDeleteFile(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\private\CSUIPassword'));
                  end;
              end;
           end;

          Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('{code:Getdircx}\etc')+'" /inheritance:r /grant:r "BUILTIN\Administrators:(OI)(CI)F" /grant:r "NT AUTHORITY\SYSTEM:(OI)(CI)F" /grant:r "NT AUTHORITY\IUSR:(OI)(CI)RX"','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
          if (not(Errorcode=0)) then
          begin
              Log('Unable to change permissions of "'+ExpandConstant('{code:Getdircx}\etc')+'" directory. Aborting installation.');
              RollBackInstallation();
              ExitProcess(13);
          end
          else
              Log('Permissions changes completed successfully on "'+ExpandConstant('{code:Getdircx}\etc')+'" directory.');
          
          Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\private')+'" /inheritance:r /grant:r "BUILTIN\Administrators:(OI)(CI)F" /grant:r "NT AUTHORITY\SYSTEM:(OI)(CI)F" /grant:r "NT AUTHORITY\IUSR:(OI)(CI)RX"','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
          if (not(Errorcode=0)) then
          begin
              Log('Unable to change permissions of Microsoft Azure Site Recovery\private directory. Aborting installation.');
              RollBackInstallation();
              ExitProcess(13);
          end
          else
              Log('Permissions changes completed successfully on Microsoft Azure Site Recovery\private directory.');
              
          Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('{code:Getdircx}\pushinstallsvc')+'" /inheritance:r /grant:r "BUILTIN\Administrators:(OI)(CI)F" /grant:r "NT AUTHORITY\SYSTEM:(OI)(CI)F" /grant:r "NT AUTHORITY\IUSR:(OI)(CI)RX"','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
          if (not(Errorcode=0)) then
          begin
              Log('Unable to change permissions of pushinstallsvc directory. Aborting installation.');
              RollBackInstallation();
              ExitProcess(13);
          end
          else
              Log('Permissions changes completed successfully on pushinstallsvc directory.');
         
        end; 
        if not PSOnlySelected then
        begin
            Count := 0;
            while (Count <= 3) do
            begin
                Log('Executing '+ExpandConstant('{code:Getdircx}\bin\send_updates.pl')+' file');
                Exec(ExpandConstant('C:\strawberry\perl\bin\perl.exe'),'"'+ExpandConstant('{code:Getdircx}\bin\send_updates.pl')+'" -filename "'+ExpandConstant('{code:Getdircx}\bin\manifest.txt')+'"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);  
                if (ResultCode = 0) then
                begin
                    Log('Executed "'+ExpandConstant('{code:Getdircx}\bin\send_updates.pl')+'" file successfully.');
                    break;
                end
                else
                begin
                    Sleep(20000);
                    Count := Count + 1;
                    Log('Unable to execute '+ExpandConstant('{code:Getdircx}\bin\send_updates.pl')+' file. Retry count : ('+IntToStr(Count)+')');
                end;
            end;
        end;
        if UpgradeCheck then 
        begin
            if (GetValue('IP_ADDRESS_FOR_AZURE_COMPONENTS') <> '') then
            begin 
                CSONLY_AZURE_IP_Value := GetValue('IP_ADDRESS_FOR_AZURE_COMPONENTS');
            end
            else
               begin
                CSONLY_AZURE_IP_Value := GetValue('CS_IP');
               end;

            if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'IS_APPINSIGHTS_LOGGING_ENABLED','IS_APPINSIGHTS_LOGGING_ENABLED = "1"') then
                Log('Successfully added IS_APPINSIGHTS_LOGGING_ENABLED parameter to amethyst.conf file.')
            else
            begin
                Log('Unable to add IS_APPINSIGHTS_LOGGING_ENABLED parameter to amethyst.conf file. Aborting installation.');
                RollBackInstallation();
                ExitProcess(14);
            end;

            if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'IS_MDS_LOGGING_ENABLED','IS_MDS_LOGGING_ENABLED = "1"') then
                Log('Successfully added IS_MDS_LOGGING_ENABLED parameter to amethyst.conf file.')
            else
            begin
                Log('Unable to add IS_MDS_LOGGING_ENABLED parameter to amethyst.conf file. Aborting installation.');
                RollBackInstallation();
                ExitProcess(14);
            end;

            if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'EVENT_THRESHOLD_TIME','EVENT_THRESHOLD_TIME = "7200"') then
                Log('Successfully added EVENT_THRESHOLD_TIME to amethyst.conf file.')
            else
            begin
                Log('Unable to add EVENT_THRESHOLD_TIME to amethyst.conf file. Aborting installation.');
                RollBackInstallation();
                ExitProcess(14);
            end;
            if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'PUSH_INSTALL_JOB_TIMEOUT','PUSH_INSTALL_JOB_TIMEOUT = "5400"') then
                Log('Successfully added PUSH_INSTALL_JOB_TIMEOUT to amethyst.conf file.')
            else
            begin
                Log('Unable to add PUSH_INSTALL_JOB_TIMEOUT to amethyst.conf file. Aborting installation.');
                RollBackInstallation();
                ExitProcess(14);
            end;
            if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'STOP_PROTECTION_JOB_TIMEOUT','STOP_PROTECTION_JOB_TIMEOUT = "1800"') then
                Log('Successfully added STOP_PROTECTION_JOB_TIMEOUT to amethyst.conf file.')
            else
            begin
                Log('Unable to add STOP_PROTECTION_JOB_TIMEOUT to amethyst.conf file. Aborting installation.');
                RollBackInstallation();
                ExitProcess(14);
            end;
            if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'DISCOVERY_JOB_TIMEOUT','DISCOVERY_JOB_TIMEOUT = "7200"') then
                Log('Successfully added DISCOVERY_JOB_TIMEOUT to amethyst.conf file.')
            else
            begin
                Log('Unable to add DISCOVERY_JOB_TIMEOUT to amethyst.conf file. Aborting installation.');
                RollBackInstallation();
                ExitProcess(14);
            end;
            if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'DB_TRANSACTION_RETRY_LIMIT','DB_TRANSACTION_RETRY_LIMIT = "15"') then
                Log('Successfully added DB_TRANSACTION_RETRY_LIMIT to amethyst.conf file.')
            else
            begin
                Log('Unable to add DB_TRANSACTION_RETRY_LIMIT to amethyst.conf file. Aborting installation.');
                RollBackInstallation();
                ExitProcess(14);
            end;
            if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'DB_TRANSACTION_RETRY_SLEEP','DB_TRANSACTION_RETRY_SLEEP = "1"') then
                Log('Successfully added DB_TRANSACTION_RETRY_SLEEP to amethyst.conf file.')
            else
            begin
                Log('Unable to add DB_TRANSACTION_RETRY_SLEEP to amethyst.conf file. Aborting installation.');
                RollBackInstallation();
                ExitProcess(14);
            end;
            if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'IR_DR_STUCK_WARNING_THRESHOLD','IR_DR_STUCK_WARNING_THRESHOLD = "1800"') then
                Log('Successfully added IR_DR_STUCK_WARNING_THRESHOLD to amethyst.conf file.')
            else
            begin
                Log('Unable to add IR_DR_STUCK_WARNING_THRESHOLD to amethyst.conf file. Aborting installation.');
                RollBackInstallation();
                ExitProcess(14);
            end;
            if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'IR_DR_STUCK_CRITICAL_THRESHOLD','IR_DR_STUCK_CRITICAL_THRESHOLD = "3600"') then
                Log('Successfully added IR_DR_STUCK_CRITICAL_THRESHOLD to amethyst.conf file.')
            else
            begin
                Log('Unable to add IR_DR_STUCK_CRITICAL_THRESHOLD to amethyst.conf file. Aborting installation.');
                RollBackInstallation();
                ExitProcess(14);
            end;
            if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'MAX_PHP_CONNECTION_RETRY_LIMIT','MAX_PHP_CONNECTION_RETRY_LIMIT = "5"') then
                Log('Successfully added MAX_PHP_CONNECTION_RETRY_LIMIT to amethyst.conf file.')
            else
            begin
                Log('Unable to add MAX_PHP_CONNECTION_RETRY_LIMIT to amethyst.conf file. Aborting installation.');
                RollBackInstallation();
                ExitProcess(14);
            end;
            if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'IP_ADDRESS_FOR_AZURE_COMPONENTS','IP_ADDRESS_FOR_AZURE_COMPONENTS = "' + CSONLY_AZURE_IP_Value + '"') then
               Log('Successfully added IP_ADDRESS_FOR_AZURE_COMPONENTS to amethyst.conf file.')
            else
            begin
                Log('Unable to add AZURE_IP_Value to amethyst.conf file. Aborting installation.');
                RollBackInstallation();
                ExitProcess(14);
            end;
			if AddOrReplaceLine(ExpandConstant('{code:Getdircx}\etc\amethyst.conf'),'DRA_ALLOW_LOCALHOST_COMMUNICATION','DRA_ALLOW_LOCALHOST_COMMUNICATION = "1"') then
                Log('Successfully added DRA_ALLOW_LOCALHOST_COMMUNICATION parameter to amethyst.conf file.')
            else
            begin
                Log('Unable to add DRA_ALLOW_LOCALHOST_COMMUNICATION parameter to amethyst.conf file. Aborting installation.');
                RollBackInstallation();
                ExitProcess(14);
            end;
            StartCXServices;
        end; 
           
        ShellExec('', ExpandConstant('{cmd}'),'/C "'+ExpandConstant(MySQL_Path)+'"bin\mysql.exe -u root -p'+DB_ROOT_PASSWD+' --execute="set global general_log=0"','', SW_HIDE, ewWaitUntilTerminated,Errorcode);
        if (GetValue('CX_TYPE') = '3') then
        begin
            if IsWin64 then
            begin
                Log('Writing language code '+ Language_Code +' to HKLM\Software\Microsoft\Azure Site Recovery\FabricAdapterLocale registry.');
                RegWriteStringValue(HKLM64,'Software\Microsoft\Azure Site Recovery','FabricAdapterLocale',''+ Language_Code +'');
            end;
        end;

      if RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\InMage Systems\Installed Products\9', 'Version', '{#VERSION}') then
          Log('''Version'' registry value updated under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.')
      else
      begin
          Log('Unable to update ''Version'' registry value under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.');
          RollBackInstallation();
          ExitProcess(11);
      end;
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\InMage Systems\Installed Products\9', 'Product_Version', '{#PRODUCTVERSION}') then
          Log('''Product_Version'' registry value updated under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.')
      else
      begin
          Log('Unable to update ''Product_Version'' registry value under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.');
          RollBackInstallation();
          ExitProcess(11);
      end;
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\InMage Systems\Installed Products\9', 'Partner_Code', '{#PARTNERCODE}') then
          Log('''Partner_Code'' registry value updated under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.')
      else
      begin
          Log('Unable to update ''Partner_Code'' registry value under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.');
          RollBackInstallation();
          ExitProcess(11);
      end;
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\InMage Systems\Installed Products\9', 'Build_Tag', '{#BUILDTAG}') then
          Log('''Build_Tag'' registry value updated under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.')
      else
      begin
          Log('Unable to update ''Build_Tag'' registry value under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.');
          RollBackInstallation();
          ExitProcess(11);
      end;
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\InMage Systems\Installed Products\9', 'Build_Phase', '{#BUILDPHASE}') then
          Log('''Build_Phase'' registry value updated under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.')
      else
      begin
          Log('Unable to update ''Build_Phase'' registry value under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.');
          RollBackInstallation();
          ExitProcess(11);
      end;
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\InMage Systems\Installed Products\9', 'Build_Number', '{#BUILDNUMBER}') then
          Log('''Build_Number'' registry value updated under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.')
      else
      begin
          Log('Unable to update ''Build_Number'' registry value under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.');
          RollBackInstallation();
          ExitProcess(11);
      end;
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\InMage Systems\Installed Products\9', 'Build_Version', '{#APPVERSION}') then
          Log('''Build_Version'' registry value updated under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.')
      else
      begin
          Log('Unable to update ''Build_Version'' registry value under ''HKLM\SOFTWARE\InMage Systems\Installed Products\9'' registry key.');
          RollBackInstallation();
          ExitProcess(11);
      end;

      CustomDeleteFile(ExpandConstant('{sd}\Temp\PageValues.conf'));
      CustomDeleteFile(ExpandConstant('{sd}\Temp\Install_Paths.conf'));
      CustomDeleteFile(ExpandConstant('{sd}\Temp\nicdetails.conf'));
      CustomDeleteFile(ExpandConstant('{sd}\Temp\IPAddress.txt'));
      CustomDeleteFile(ExpandConstant('{sd}\Temp\NicName.txt'));
      CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\Upgrade'));

        if UpgradeCheck then
        begin
            if DelTree(ExpandConstant('C:\Temp\*'), False, True, True) then
                Log('Successfully deleted the C:\Temp contents.')
            else
                Log('Unable to delete the C:\Temp contents.');
        end
        else
        begin
            if IsWin64 then
            begin
              if not RegKeyExists(HKLM64,'SOFTWARE\Microsoft\AzureSiteRecoveryOVF') then
              begin
                  if DelTree(ExpandConstant('C:\Temp\ASRSetup\*'), False, True, True) then
                      Log('Successfully deleted the C:\Temp\ASRSetup contents.')
                  else
                      Log('Unable to delete the C:\Temp\ASRSetup contents.');
              end;
            end;

            CustomDeleteFile(ExpandConstant('{sd}\Temp\php-5.4.27-nts-Win32-VC9-x86.zip'));
            CustomDeleteFile(ExpandConstant('{sd}\Temp\php-7.1.10-nts-Win32-VC14-x86.zip'));
        end;

        if ShellExec('', ExpandConstant('{cmd}'),'/C schtasks /delete /tn \MySQL\Installer\ManifestUpdate /f','', SW_HIDE, ewWaitUntilTerminated,Errorcode) then
          Log('Successfully deleted ManifestUpdate task.')
        else
        begin
            Log('Unable to delete ManifestUpdate task.');
        end;
          
        Log('Installation completed successfully.');
    end;

    // ssDone.
    if curstep = ssDone then
	  begin
        CustomDeleteFile(ExpandConstant('{code:Getdircx}\pushinstallsvc\InmLSA.exe'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\PSAPIHandler.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\resync_now.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\updatePS.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\register_fr_agent.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\sharepoint_discovery.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\Axiom_Log_Collector.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\cleanup_cluster_database.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\clsDatabase.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\fx_compatability_update.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\get_file_replication_config.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\FXHandler.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\get_deleted_fr_jobs.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\get_fx_exit_code.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\get_OpenSSH_public_key.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\get_replication_pair_info.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\get_SECSH_public_key.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\get_virtual_server_name.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\mail_partial_protection.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\node_status.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\OnFRServiceStop.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\standby_cx_details.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\update_file_replication_status.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\update_fx_daemon_job_status.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\update_hba_info.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\update_OpenSSH_public_key.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\update_permission_state.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\update_SecSH_public_key.php'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\ScoutAPI\RequestSamples\GetRollbackStatus.xml'));
		CustomDeleteFile(ExpandConstant('{code:Getdircx}\admin\web\download_file.php'));
        if not UpgradeCheck then
        begin 
            if ShellExec('', ExpandConstant('{cmd}'),'/C sc failure "LogUpload" actions= restart/300000/restart/300000// reset= 0','', SW_HIDE, ewWaitUntilTerminated,Errorcode) then
                Log('Successfully set the recovery options for LogUpload service.')
            else
                Log('Unable to set the recovery options for LogUpload service.');
        end;

		if ShellExec('', ExpandConstant('{cmd}'), '/C sc failure "ProcessServer" actions= restart/300000/// reset= 0', '', SW_HIDE, ewWaitUntilTerminated,Errorcode) then
			Log('Successfully set the recovery options for ProcessServer service.')
		else
			Log('Unable to set the recovery options for ProcessServer service.');

		if ShellExec('', ExpandConstant('{cmd}'), '/C sc failure "ProcessServerMonitor" actions= restart/120000/// reset= 0', '',  SW_HIDE, ewWaitUntilTerminated,Errorcode) then
			Log('Successfully set the recovery options for ProcessServerMonitor service.')
		else
			Log('Unable to set the recovery options for ProcessServerMonitor service.');

        // Adding Winmgmt dependency to PushInstall service during upgrade.
        if UpgradeCheck then
        begin 
            Log('Adding Winmgmt dependency to PushInstall service');    
            if ShellExec('', ExpandConstant('{cmd}'),'/C sc  config "InMage PushInstall" depend= winmgmt','', SW_HIDE, ewWaitUntilTerminated,Errorcode) then
            begin
                Log('Successfully set the Winmgmt dependency to PushInstall service.'); 
            end
            else
            begin   
                Log('Unable to set the Winmgmt dependency to PushInstall service.'); 
                SaveInstallLog();
                ExitProcess(65);
            end;
         
            // Stop Pushinstall service.
            if not ServiceStopInLoop('InMage PushInstall') then
            begin
                SaveInstallLog();
                ExitProcess(66);
            end;

            // Start Pushinstall service.
            if not ServiceStartInLoop('InMage PushInstall') then
            begin
                SaveInstallLog();
                ExitProcess(67);
            end;
        end;
    end;
end;

procedure DeinitializeSetup();
var
LogFileString : AnsiString;
begin
	  CustomDeleteDirectory(ExpandConstant('C:\bin'));
    CustomDeleteDirectory(ExpandConstant('C:\site'));
    CustomDeleteDirectory(ExpandConstant('C:\Temp\Win32-UTCFileTime-1.50'));
    StartCXServices;
  	
    FileCopy(ExpandConstant ('{log}'), ExpandConstant ('{sd}\InstallLogFile.log'), FALSE);
    LoadStringFromFile(ExpandConstant ('{sd}\InstallLogFile.log'), LogFileString);    
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_InstallLogFile.log'),'Installation Starts here '+ Installation_StartTime + #13#10 + '*********************************************', True);
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_InstallLogFile.log'),#13#10 + LogFileString , True);
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_InstallLogFile.log'), #13#10 + '*********************************************' + #13#10 + 'Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
    CustomDeleteFile(ExpandConstant('{sd}\InstallLogFile.log'));
  
end;

function InitializeUninstall(): Boolean;
begin
      mcLogFile := ExpandConstant('{sd}\ProgramData\ASRSetupLogs\CX_UnInstallLogFile.log');
    	SaveStringToFile(mcLogFile, Chr(13), True);
      SaveStringToFile(mcLogFile, 'Uninstallation starts here : ' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10 + '*********************************************', True);
  	  SaveStringToFile(mcLogFile, Chr(13), True);
	    SaveStringToFile(mcLogFile, 'In function InitializeUninstall' + Chr(13), True);
	    Result := True;	    
end;

procedure DeinitializeUninstall();
begin
	SaveStringToFile(mcLogFile, 'In function DeinitializeUninstall' + Chr(13), True);
	SaveStringToFile(mcLogFile, Chr(13), True);
		
  SaveStringToFile(mcLogFile, #13#10 + '*********************************************' + #13#10 + 'Un-Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
	SaveStringToFile(mcLogFile, Chr(13), True);
end;


procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
	ResultCode,RX_ResultCode,DRA_ResultCode : Integer;
	Names: TArrayOfString;
	I: Integer;
	DB_ROOT_PASSWD, MySQL_Path, MySQL_Data_Path, MySQL_Install_Binary : String;
	ResetPathString, Path_Backup : String;

begin
	if CurUninstallStep=usUninstall then 
	begin
        if IsAgentInstalled then
        begin
             SuppressibleMsgBox('Azure Site Recovery configuration server/process server couldn''t be uninstalled. You''ll need to remove other software in this order before you can uninstall the server or service:'+#13+#13+'1. Microsoft Azure Site Recovery Mobility Service / Master Target Server', mbError, MB_OK, MB_OK);
             SaveStringToFile(mcLogFile, 'Azure Site Recovery configuration server/process server couldn''t be uninstalled. You''ll need to remove other software in this order before you can uninstall the server or service:'+#13+#13+'1. Microsoft Azure Site Recovery Mobility Service / Master Target Server' + Chr(13), True);
             Abort;
        end;

        //Uninstalling DRA(Microsoft Azure Site Recovery Provider)
         if not (GetValue('CX_TYPE') = '2') then
          begin
            ShellExec('', ExpandConstant('{code:Getdircx}\Uninstall_DRA_Invoke.bat'),'', '', SW_HIDE, ewWaitUntilTerminated, DRA_ResultCode);
            if DRA_ResultCode <> 0 then
    				  SaveStringToFile(mcLogFile, 'Microsoft Azure Site Recovery Provider could not be successfully uninstalled.' + Chr(13), True) 
            else
    	        SaveStringToFile(mcLogFile, 'Microsoft Azure Site Recovery Provider successfully uninstalled' + Chr(13), True); 
          end;

        // Unregister CS with RX
          SaveStringToFile(mcLogFile, 'Invoking unregisterRx.pl script.' + Chr(13), True)
    		  Exec(ExpandConstant('C:\strawberry\perl\bin\perl.exe'),'"'+ExpandConstant('{code:Getdircx}\bin\unregisterRx.pl')+'"', '', SW_HIDE, ewWaitUntilTerminated, RX_ResultCode);
    			if RX_ResultCode <> 0 then
    				  SaveStringToFile(mcLogFile, 'CS could not be successfully unregistered from the RX.' + Chr(13), True) 
    	    else
    	        SaveStringToFile(mcLogFile, 'CS successfully unregistered from the RX.' + Chr(13), True); 

    		// Unregister PS with CS    
        SaveStringToFile(mcLogFile, 'Invoking unregisterps.pl script.' + Chr(13), True)
        SetEnvironmentVariable('PATH', ExpandConstant('C:\strawberry\perl\bin')+';'+ExpandConstant('C:\strawberry\c\bin')+';'+ExpandConstant('C:\thirdparty\php5nts')+';'+ExpandConstant('C:\thirdparty\php5nts\ext')+';');
        ShellExec('',ExpandConstant('C:\strawberry\perl\bin\perl.exe'),'"'+ExpandConstant('{code:Getdircx}\bin\unregisterps.pl')+'"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
        if ResultCode <> 0 then
            SuppressibleMsgBox('Process Server could not be successfully unregistered from the Configuration Server.',mbInformation, MB_OK,3)
        else
            SaveStringToFile(mcLogFile, 'Process Server is successfully unregistered from the Configuration Server.' + Chr(13), True); 

        if not (GetValue('CX_TYPE') = '2') then
    		begin
    		    RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','Location',MySQL_Path);
            MySQL_Install_Binary := ExpandConstant(MySQL_Path+'bin\mysql.exe');
        		DB_ROOT_PASSWD := GetValue('DB_ROOT_PASSWD');
        		SaveStringToFile(mcLogFile, 'Invoking removedb.bat script.' + Chr(13), True);
            if ShellExec('', ExpandConstant('{code:Getdircx}\removedb.bat'),DB_ROOT_PASSWD+' "'+ExpandConstant(MySQL_Install_Binary)+'"', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode) then
                SaveStringToFile(mcLogFile, 'Removed svsdb1 database and svsystems user.' + Chr(13), True);
                
            ShellExec('', ExpandConstant('{cmd}'),'/C "'+ExpandConstant(MySQL_Path)+'"bin\mysql.exe -u root -p'+DB_ROOT_PASSWD+' --execute="set global general_log=0"','', SW_HIDE, ewWaitUntilTerminated,Errorcode);
            ShellExec('', ExpandConstant('{code:Getdircx}\Unconfigure_IIS.bat'),'', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode);
        end;
        ShellExec('',ExpandConstant('C:\strawberry\perl\bin\perl.exe'),'"'+ExpandConstant('{code:Getdircx}\bin\clean_stale_process.pl')+'"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
        RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\9','Install_Path_Backup',Path_Backup);
        RegQueryStringValue(HKLM,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment','Path',ResetPathString);
        StringChangeEx(ResetPathString, Path_Backup, ';', True);
        RegWriteExpandStringValue(HKEY_LOCAL_MACHINE,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment','Path',ResetPathString);
        
        if RegDeleteValue(HKLM,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment','PERL5LIB') then
		          SaveStringToFile(mcLogFile,'The PERL5LIB registry entry is deleted successfuly from ''SYSTEM\CurrentControlSet\Control\Session Manager\Environment'' key' +  Chr(13), True);
		    if RegDeleteValue(HKLM,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment','CYGWIN') then
		          SaveStringToFile(mcLogFile,'The CYGWIN registry entry is deleted successfuly from ''SYSTEM\CurrentControlSet\Control\Session Manager\Environment'' key' +  Chr(13), True);
        
    		{ Remove the valuename TcpWindowSize and its valuedata }
    		if RegGetSubkeyNames(HKEY_LOCAL_MACHINE, 'SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\Interfaces', Names) then 
    		begin
        			for I := 0 to GetArrayLength(Names)-1 do 
        			begin
        				    RegDeleteValue(HKEY_LOCAL_MACHINE, 'SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\Interfaces\' + Names[I], 'TcpWindowSize');
        			end;
    		end;

		  // Stopping ProcessserverMonitor service first to avoid generating false alerts/events when other services are stopped during install/upgrade/uninstall.
		  SaveStringToFile(mcLogFile, 'Stopping ProcessServerMonitor service.' + Chr(13), True)
				if ServiceStopInLoop('ProcessServerMonitor') then
          begin
              SaveStringToFile(mcLogFile, 'ProcessServerMonitor service stopped successfully.' + Chr(13), True);
          end
          else
          begin
              SaveStringToFile(mcLogFile, 'Failed to stop ProcessServerMonitor service.' + Chr(13), True);
          end;

    			SaveStringToFile(mcLogFile, 'Stopping tmansvc service.' + Chr(13), True)
    			if ServiceStopInLoop('tmansvc') then
          begin
              SaveStringToFile(mcLogFile, 'tmansvc service stopped successfully.' + Chr(13), True);
                  ShellExec('',ExpandConstant('C:\strawberry\perl\bin\perl.exe'),'"'+ExpandConstant('{code:Getdircx}\bin\clean_stale_process.pl')+'"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
            		  if ResultCode <> 0 then
            		      SaveStringToFile(mcLogFile, 'Failed to execute clean_stale_process.pl script.' + Chr(13), True)
            		  else
            		      SaveStringToFile(mcLogFile, 'clean_stale_process.pl script executed successfully.' + Chr(13), True);    
          end
          else
          begin
              SaveStringToFile(mcLogFile, 'Failed to stop tmansvc service. Not invoking clean_stale_process.pl script.' + Chr(13), True);
          end;
		  
		  SaveStringToFile(mcLogFile, 'Stopping ProcessServer service.' + Chr(13), True)
    			if ServiceStopInLoop('ProcessServer') then
          begin
              SaveStringToFile(mcLogFile, 'ProcessServer service stopped successfully.' + Chr(13), True);  
          end
          else
          begin
              SaveStringToFile(mcLogFile, 'Failed to stop ProcessServer service.' + Chr(13), True);
          end;

          SaveStringToFile(mcLogFile, 'Stopping LogUpload service.' + Chr(13), True)
    			if ServiceStopInLoop('LogUpload') then
          begin
              SaveStringToFile(mcLogFile, 'LogUpload service stopped successfully.' + Chr(13), True);
          end
          else
          begin
              SaveStringToFile(mcLogFile, 'Failed to stop LogUpload service.' + Chr(13), True);
          end;

          if DirExists(ExpandConstant('{code:Getdircx}\Upgrade')) then
          begin
          		if CustomDeleteDirectory(ExpandConstant('{code:Getdircx}\Upgrade')) then
                  SaveStringToFile(mcLogFile, 'Successfully Deleted the '+ExpandConstant('{code:Getdircx}\Upgrade')+' directory'  +  Chr(13), True)
              else
                  SaveStringToFile(mcLogFile, 'Unable to Delete the '+ExpandConstant('{code:Getdircx}\Upgrade')+' directory. Please cleanup manually.'  +  Chr(13), True);
          end;
          
          if not (GetValue('CX_TYPE') = '2') then
    		  begin
              if Fileexists(ExpandConstant('C:\thirdparty\php5nts\php.ini')) then
              begin
                  SaveStringToFile(mcLogFile, 'Updating the value log_errors=Off in php.ini file.'  +  Chr(13), True);
                  SetIniString( 'PHP', 'log_errors','Off', ExpandConstant('C:\thirdparty\php5nts\php.ini') );
                  SetIniString( 'PHP', 'error_log','"'+ExpandConstant('{code:Getdircx}\var\php_error.log')+'"', ExpandConstant('C:\thirdparty\php5nts\php.ini') );
              end;
          end;

          ShellExec('', ExpandConstant('{cmd}'),'/C fsutil reparsepoint query '+ExpandConstant('{sd}\ProgramData\ASR'),'', SW_HIDE, ewWaitUntilTerminated,Errorcode );
          if (Errorcode = 0) then
          begin
              ShellExec('', ExpandConstant('{cmd}'),'/C fsutil reparsepoint delete '+ExpandConstant('{sd}\ProgramData\ASR'),'', SW_HIDE, ewWaitUntilTerminated,Errorcode );
              if (Errorcode = 0) then
              begin
                  SaveStringToFile(mcLogFile, 'Successfully deleted the junction directory  '+ExpandConstant('{sd}\ProgramData\ASR')+''  +  Chr(13), True);
              end
              else
              begin
                  SaveStringToFile(mcLogFile, 'Unable to delete the junction directory  '+ExpandConstant('{sd}\ProgramData\ASR')+''  +  Chr(13), True);
              end;
          end;
          
          if CustomDeleteDirectory(ExpandConstant('{sd}\ProgramData\ASR')) then
          begin
              SaveStringToFile(mcLogFile, 'Successfully deleted the '+ExpandConstant('{sd}\ProgramData\ASR')+' path.'  +  Chr(13), True);
          end
          else
          begin
              SaveStringToFile(mcLogFile, 'Unable to delete the '+ExpandConstant('{sd}\ProgramData\ASR')+' path.'  +  Chr(13), True);
          end;
	end;
	if CurUninstallStep=usDone then
  Begin
	    if not IsAgentInstalled then
      begin
          if DirExists(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')) then
          begin
            if CustomDeleteDirectory(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')) then
                SaveStringToFile(mcLogFile, 'Successfully deleted the '+ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')+' path.'  +  Chr(13), True)
            else
                SaveStringToFile(mcLogFile, 'Unable to delete the '+ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')+' path.'  +  Chr(13), True)
            end;
      end;

  		if not (GetValue('CX_TYPE') = '2') then
  		begin 
      		RegQueryStringValue(HKLM,'SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.7','DataLocation',MySQL_Data_Path);
      		if DirExists(ExpandConstant(MySQL_Data_Path+'data\svsdb1')) then
      		begin
          		if CustomDeleteDirectory(ExpandConstant(MySQL_Data_Path+'data\svsdb1')) then
                  SaveStringToFile(mcLogFile, 'Successfully Deleted the svsdb1 database.'  +  Chr(13), True)
              else
                  SaveStringToFile(mcLogFile, 'Unable to Delete the svsdb1 database. Please cleanup manually.'  +  Chr(13), True);
          end;
      end;
      if CustomDeleteDirectory(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\Config')) then
          SaveStringToFile(mcLogFile, 'Successfully deleted the Config directory from ''ProgramData\Microsoft Azure Site Recovery'' path.'  +  Chr(13), True)
      else
          SaveStringToFile(mcLogFile, 'Unable to delete the Config directory from ''ProgramData\Microsoft Azure Site Recovery'' path.'  +  Chr(13), True);

      CustomDeleteDirectory(ExpandConstant('{code:Getdircx}'));

      if RegDeleteKeyIncludingSubkeys(HKLM,'SOFTWARE\InMage Systems\Installed Products\9') then 
          SaveStringToFile(mcLogFile, 'Deleted ''HKLM\SOFTWARE\Wow6432Node\InMage Systems\Installed Products\9'' registry key successfully.' + Chr(13), True) 
      else 
          SaveStringToFile(mcLogFile, 'Unable to Delete ''HKLM\SOFTWARE\Wow6432Node\InMage Systems\Installed Products\9'' registry key. Please delete it manually.' + Chr(13), True);
      
      if RegDeleteKeyIncludingSubkeys(HKLM,'Software\SV Systems') then 
          SaveStringToFile(mcLogFile, 'Deleted ''HKLM\Software\SV Systems'' registry key successfully.' + Chr(13), True) 
      else 
          SaveStringToFile(mcLogFile, 'Unable to Delete ''HKLM\Software\SV Systems'' registry key. Please delete it manually.' + Chr(13), True);

      // Kill stale perl process.
      if ShellExec('', ExpandConstant('{cmd}'),'/C taskkill /F /IM perl.exe /T','', SW_HIDE, ewWaitUntilTerminated,Errorcode) then
          SaveStringToFile(mcLogFile, 'Successfully killed the stale perl process.' + Chr(13), True)
      else
          SaveStringToFile(mcLogFile, 'Unable to kill the stale perl process.' + Chr(13), True);

      DeleteFile(ExpandConstant('{userdesktop}\Cspsconfigtool.lnk'));
      DeleteFile(ExpandConstant('{commondesktop}\Cspsconfigtool.lnk'));
	end;	
end;

function NeedRestart(): Boolean;
begin
		if not UpgradeCheck then
		begin
    		if PSOnlySelected then
    		begin
            Result := True;
    		end
    		else
    			Result := False;
    end;
end;

function Get_CX_TYPE(Param: String) : String;
begin
    if not UpgradeCheck then
		begin
        Result := GetIniString( 'CX_Type', 'Type','', ExpandConstant('{sd}\Temp\PageValues.conf'));
    end
    else
    begin
        if (GetValue('CX_TYPE') = '3') then
        begin
            Result := '3';
        end
        else if (GetValue('CX_TYPE') = '2') then
        begin
            Result := '2';
        end;
    end;
end;     

//Install Perl modules
procedure InstallPerlModules();
var
  ErrorCode: Integer;
  LogFileString : AnsiString;
begin
    Log('Invoking installmod1.bat script.');
    if ShellExec('', ExpandConstant('C:\Temp\installmod1.bat'),'', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode) then
    begin
        if FileExists(ExpandConstant('C:\thirdparty\perlmoduleinstall.log')) then	
        begin
            LoadStringFromFile(ExpandConstant ('C:\thirdparty\perlmoduleinstall.log'), LogFileString);    
            Log(#13#10 +'Perl modules install log Starts here '+ #13#10 + '*********************************************');
            Log(#13#10 + LogFileString);
            Log(#13#10 + '*********************************************' + #13#10 + 'Perl modules install log ends here :' +#13#10 + #13#10);
        end;

        if not (ErrorCode = 0) then
        begin
            if (ErrorCode = 1) then
            begin
                SuppressibleMsgBox('Unable to change directory to perl module. Aborting installation.', mbError, MB_OK, MB_OK);
                SaveInstallLog();
            end
            else if (ErrorCode = 2) then
            begin
                SuppressibleMsgBox('Unable to generate makefile for perl module. Aborting installation.', mbError, MB_OK, MB_OK);
                SaveInstallLog();
            end
            else if (ErrorCode = 3) then
            begin
                SuppressibleMsgBox('Unable to install perl module. Aborting installation.', mbError, MB_OK, MB_OK);
                SaveInstallLog();
            end;
        end
        else
            Log('Successfully installed perl modules.');
    end;
end;

(* 
 Moving the old binary files during upgrade, 
If unable to move binary files showing message and aborting the installation.
*)
procedure RenameBinaryFiles(BinaryName: String);
var
    WaitLoopCount : Integer;
    BinaryFilePath, BinaryFileName : String;
begin	  
   if UpgradeCheck then
   begin
      BinaryFilePath := ExtractFileDir(BinaryName);
      BinaryFileName := ExtractFileName(BinaryName);

     if FileExists(ExpandConstant(BinaryName)) then
     begin
         WaitLoopCount := 1;
         while (WaitLoopCount <= 4) do
         begin
             CurrentBuildVersion := '{#APPVERSION}';
             CustomDeleteFile(ExpandConstant('{code:Getdircx}\Release_'+CurrentBuildVersion+'\'+BinaryFileName));
				
             if RenameFile(ExpandConstant(BinaryName), ExpandConstant('{code:Getdircx}\Release_'+CurrentBuildVersion+'\'+BinaryFileName)) then
             begin
               Log('Successfully moved the binary from '+ ExpandConstant(BinaryName)+ ' to ' + ExpandConstant('{code:Getdircx}\Release_'+CurrentBuildVersion+'\'+BinaryFileName)+' before copying the new file.');
               SaveStringToFile(ExpandConstant('{code:Getdircx}\Release_'+CurrentBuildVersion+'\Backup_BinaryFile.log'),#13#10+ExpandConstant(BinaryName), True);
               SetIniString( 'Rename', 'RenameSuccessValue','YES', ExpandConstant('{tmp}\RenameSuccess.conf') );
               break;
             end;
             SetIniString( 'Rename', 'RenameSuccessValue','NO', ExpandConstant('{tmp}\RenameSuccess.conf') );
             Sleep(15000);
             WaitLoopCount := WaitLoopCount + 1;
             Log('Unable to rename the file '+ ExpandConstant(BinaryName)+', Retrying('+IntToStr(WaitLoopCount)+')');
         end;
         RenameSuccess := GetIniString( 'Rename', 'RenameSuccessValue','', ExpandConstant('{tmp}\RenameSuccess.conf') );
         if RenameSuccess = 'NO' then
         begin
            SuppressibleMsgBox('Setup was unable to replace ' + ExpandConstant(BinaryName) + ' as it is in use. Stop the application that is using '+ExpandConstant(BinaryName)+' and re-try the installation.',mbError, MB_OK, MB_OK);
            StartCXServices;
            SaveInstallLog();
            ExitProcess(44);
         end;                 	
     end;
  end;   
end;
