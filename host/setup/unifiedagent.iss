; Installation script for Microsoft Azure Site Recovery Mobility Service/Master Target Server

#include "version.iss"
#include "branding_parameters.iss"

#ifdef DEBUG
#define Configuration "Debug"
#define Configuration2 "Debug"
#define ConfigurationAppVerName "Debug"
#define ConfigurationDriverDir "objchk"
#define DefaultLogLevel 7
#define LogLevelValue 7
#define LogOptionValue 1
#else
#define Configuration "Release"
#define Configuration2 "ReleaseMinDependency"
#define ConfigurationAppVerName ""
#define ConfigurationDriverDir "objfre"
#define DefaultLogLevel 3
#define LogLevelValue 3
#define LogOptionValue 0
#endif

[Setup]
AppId=InMage Product
AppName=Microsoft Azure Site Recovery Mobility Service/Master Target Server
AppVerName=Microsoft Azure Site Recovery Mobility Service/Master Target Server
AppVersion={#APPVERSION}
AppPublisher=Microsoft Corporation
DefaultDirName={code:GetDefaultDirName}
DefaultGroupName={#COMPANYNAME}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
OutputBaseFilename={#OSTYPE}_unified_setup
OutputDir={#Configuration}
DirExistsWarning=no
DisableStartupPrompt=yes
setuplogging=yes
uninstallable=yes
DisableReadyPage=yes

VersionInfoCompany=Microsoft Corporation
VersionInfoCopyright={#COPYRIGHT}
VersionInfoDescription=Microsoft Azure Site Recovery Mobility Service/Master Target Server
VersionInfoProductName=Microsoft Azure Site Recovery
VersionInfoVersion={#VERSION}

[LangOptions]
DialogFontName=Times New Roman
DialogFontSize=10
CopyrightFontName=Times New Roman
CopyrightFontSize=10

[Files]

;Installing the VX related files in to the respective locations
source: DeleteBitmapFiles.bat ; DestDir : {code:Getdirvx}; Flags: ignoreversion; 
Source: drscout_upgrade.bat; DestDir: {code:Getdirvx}; Flags: overwritereadonly ignoreversion
Source: rem_files.bat; DestDir: {code:Getdirvx}; Flags: overwritereadonly ignoreversion
Source: reboot.bat; DestDir: {code:Getdirvx}; Flags: overwritereadonly ignoreversion; Check: VolumeAgentInstall and issilent
Source: configureadmin.bat; DestDir: {code:Getdirvx}; Flags: ignoreversion; Check: VolumeAgentInstall or FileAgentInstall
Source: EnableDisableFiltering.bat; DestDir: {code:Getdirvx}; Flags: ignoreversion;
Source: VerifyBootDiskFiltering.ps1; DestDir: {code:Getdirvx}; Flags: ignoreversion;
Source: ..\Utilities\Windows\FindDiskID\{#Configuration}\FindDiskID.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion;
Source: createlink.vbs; DestDir : {code:Getdirvx}; Flags: ignoreversion
Source: createlink.bat; DestDir : {code:Getdirvx}; Flags: ignoreversion
Source: removefiles.bat; DestDir : {code:Getdirvx}; Flags: ignoreversion
source: wintogo.ps1 ; DestDir : {code:Getdirvx}; Flags: ignoreversion;
Source: ..\ApplicationFailover\Scripts\Windows\*.*; DestDir: {code:Getdirvx}; Flags: ignoreversion  overwritereadonly; Check: VolumeAgentInstall
Source: vxfiles.txt; DestDir: {code:Getdirvx}
Source: firewall_remove.bat; DestDir: {code:Getdirvx}
Source: vxfirewall.bat; DestDir: {code:Getdirvx}
Source: ..\..\Server\Windows\Country_Codes.txt; DestDir: dontcopy; Flags: ignoreversion

;Packaging InMageVssProvider scripts
Source: ..\InMageVssProvider\InstallScripts\*; DestDir: {code:Getdirvx}; Flags: ignoreversion

; Packaging the solution scripts
Source: ..\..\Solutions\Oracle\windows_vx\Oracle.bat; DestDir: {code:Getdirvx}; Flags: ignoreversion; Check: VolumeAgentInstall
Source: ..\..\Solutions\Oracle\windows_vx\Oracle.sql; DestDir: {code:Getdirvx}\consistency; Flags: ignoreversion; Check: VolumeAgentInstall
Source: ..\..\Solutions\Oracle\windows_vx\oraclefailover.vbs; DestDir: {code:Getdirvx}\consistency; Flags: ignoreversion; Check: VolumeAgentInstall
Source: ..\vacp\ACM_SCRIPTS\win\winshare_*.bat; DestDir: {code:Getdirfx}\Consistency; Check: FileAgentInstall
Source: ..\vacp\ACM_SCRIPTS\*.bat; DestDir: {code:Getdirvx}\consistency; Flags: ignoreversion; Check: FileAgentInstall
Source: ..\vacp\ACM_SCRIPTS\*; DestDir: {code:Getdirvx}\consistency; Flags: ignoreversion; Check: VolumeAgentInstall
Source: ..\drivers\Tests\win32\GetDataPoolSize\{#Configuration}\GetDataPoolSize.exe; DestDir: {code:Getdirvx}\GetDataPoolSize; Flags: ignoreversion; Check: (FreshInstallationCheck)
Source: DataPoolSize.bat; DestDir: {code:Getdirvx}\GetDataPoolSize; Flags: ignoreversion; Afterinstall: DataPoolSize; Check: (FreshInstallationCheck)

Source: ..\tal\newcert.pem; DestDir: {code:Getdirvx}\transport; Flags: ignoreversion; Check: VolumeAgentInstall
Source: ..\cxpslib\pem\client.pem; DestDir: {code:Getdirvx}\transport; Flags: ignoreversion

Source: ..\inmexec\inmexec_jobspec.txt; DestDir: {code:Getdirvx}\Application Data\etc; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall
Source: ..\..\thirdparty\Notices\Third-Party_Notices.rtf; DestDir: {code:Getdirvx}; Flags: ignoreversion; Check: VolumeAgentInstall

Source: ..\Utilities\Windows\WinOperation\{#Configuration}\WinOp.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: (VolumeAgentInstall) or (FileAgentInstall); BeforeInstall: RenameBinaryFiles('WinOp.exe');
Source: ..\Utilities\Windows\ClusUtil\{#Configuration}\ClusUtil.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('ClusUtil.exe');
Source: ..\Utilities\Windows\MovingSQLDBs\{#Configuration}\MoveSQLDB.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('MoveSQLDB.exe');
Source: ..\EsxUtil\{#Configuration}\EsxUtil.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('EsxUtil.exe');
Source: ..\EsxUtilWin\{#Configuration}\EsxUtilWin.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('EsxUtilWin.exe');
Source: ..\cachemgr\{#Configuration}\cachemgr.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('cachemgr.exe');
Source: ..\cdpmgr\{#Configuration}\cdpmgr.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('cdpmgr.exe');
Source: ..\dataprotection\{#Configuration}\dataprotection.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('dataprotection.exe');
Source: ..\s2\{#Configuration}\s2.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('s2.exe');
Source: ..\volopt\{#Configuration}\volopt.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('volopt.exe');
Source: ..\AppConfiguratorAPITestBed\{#Configuration}\AppConfiguratorAPITestBed.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('AppConfiguratorAPITestBed.exe');
Source: ..\unregagent\{#Configuration}\unregagent.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('unregagent.exe');
Source: ..\svdcheck\{#Configuration}\svdcheck.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('svdcheck.exe');
Source: ..\inmmessage\{#Configuration}\inmmessage.dll; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('inmmessage.dll');
Source: ..\service\{#Configuration2}\svagents.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('svagents.exe');
Source: ..\drivers\Tests\win32\drvutil\{#Configuration}\drvutil.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('drvutil.exe');
Source: ..\dnsfailover\{#Configuration}\dns.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: (VolumeAgentInstall) or (FileAgentInstall); BeforeInstall: RenameBinaryFiles('dns.exe');
Source: ..\cdpcli\{#Configuration}\cdpcli.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('cdpcli.exe');
Source: ..\cxcli\{#Configuration}\cxcli.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('cxcli.exe');
Source: ..\ServiceCtrl\{#Configuration}\ServiceCtrl.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('ServiceCtrl.exe');
Source: ..\UnlockDrives\{#Configuration}\UnLockDrives.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('UnLockDrives.exe');
Source: ..\vacp\{#Configuration}\vacp.exe; DestDir: {code:Getdirvx}; Check: (not (IsWin64)) and (VolumeAgentInstall or FileAgentInstall); Flags: ignoreversion overwritereadonly; MinVersion: 0, 5.02; BeforeInstall: RenameBinaryFiles('vacp.exe');
Source: ..\vacp\X64\{#Configuration}\vacp_X64.exe; DestDir: {code:Getdirvx}; DestName: vacp.exe; Check: IsWin64 and (not IsItanium64) and (VolumeAgentInstall or FileAgentInstall) and (not(CheckWindowsXP64)); Flags: ignoreversion overwritereadonly; BeforeInstall: RenameBinaryFiles('vacp.exe');

Source: ..\ApplicationFailover\{#Configuration}\Application.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: (VolumeAgentInstall) or (FileAgentInstall); BeforeInstall: RenameBinaryFiles('Application.exe');
Source: ..\cxpsclient\{#Configuration}\cxpsclient.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion; BeforeInstall: RenameBinaryFiles('cxpsclient.exe');
Source: ..\config\ConfiguratorAPITestBed\{#Configuration}\ConfiguratorAPITestBed.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('ConfiguratorAPITestBed.exe');
Source: ..\AppWizard\{#Configuration}\AppWizard.exe; DestDir: {code:Getdirvx}; Destname: ApplicationReadinesswizard.exe; Check: VolumeAgentInstall; Flags: ignoreversion overwritereadonly; BeforeInstall: RenameBinaryFiles('ApplicationReadinesswizard.exe');
Source: ..\FailoverCommadUtil\{#Configuration}\FailoverCommandUtil.exe; DestDir: {code:Getdirvx}; Destname: FailoverCommandUtil.exe; Flags: ignoreversion overwritereadonly; BeforeInstall: RenameBinaryFiles('FailoverCommandUtil.exe');

Source: ..\csgetfingerprint\{#Configuration}\csgetfingerprint.exe; DestDir: DontCopy; Flags: ignoreversion overwritereadonly;
Source: ..\csgetfingerprint\{#Configuration}\csgetfingerprint.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly;
Source: ..\gencert\{#Configuration}\gencert.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly;
Source: ..\genpassphrase\{#Configuration}\genpassphrase.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly;
Source: ..\verifycert\{#Configuration}\verifycert.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly;
Source: ..\csharphttpclient\bin\{#Configuration}\httpclient.dll; DestDir: {code:Getdirfx};Check: VolumeAgentInstall; Flags: ignoreversion overwritereadonly;
;Packaging InMageVssProvider dll's.
Source: ..\InMageVssProvider\{#Configuration}\InMageVssProvider.dll; DestDir: {code:Getdirvx}; Check: (not IsWin64); Flags: ignoreversion overwritereadonly; BeforeInstall: RenameBinaryFiles('InMageVssProvider.dll');
Source: ..\InMageVssProvider\X64\{#Configuration}\InMageVssProvider_X64.dll; DestDir: {code:Getdirvx}; DestName: InMageVssProvider.dll; Check: IsWin64 and (not IsItanium64); Flags: ignoreversion overwritereadonly; BeforeInstall: RenameBinaryFiles('InMageVssProvider.dll');
;Source: ..\InMageVssProvider\IA64\{#Configuration}\InMageVssProvider_IA64.dll; DestDir: {code:Getdirvx}; DestName: InMageVssProvider.dll; Check: IsWin2008 and (not IsWin64) and (IsItanium64) and (FileAgentInstall); Flags: ignoreversion overwritereadonly; BeforeInstall: RenameBinaryFiles('InMageVssProvider.dll');

Source: ..\configmergetool\{#Configuration}\configmergetool.exe; DestDir: {code:Getdirvx}; Flags: overwritereadonly ignoreversion; BeforeInstall: RenameBinaryFiles('configmergetool.exe');
Source:..\ScoutTuning\{#Configuration}\ScoutTuning.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion;Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('ScoutTuning.exe'); 
Source: ..\inmexec\{#Configuration}\inmexec.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('inmexec.exe');

; Packaging diskgeneric.dll
Source: ..\drivers\Tests\win32\diskgenericdll\{#Configuration}\diskgeneric.dll; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('diskgeneric.dll');

; Adding the common hostconfig exe so as to use single exe which will take the cx server port and ip when both FX and VX needs to be installed or upgraded.
Source: ..\hostconfigwxcommon\{#Configuration}\hostconfigwxcommon.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion; Check: ((Not FreshFXAgentConfiguration) and (Not FreshVXAgentConfiguration)) or FreshVXAgentConfiguration or ((FreshVXAgentConfiguration) and (fxinstall))  or (VolumeAgentConfiguration or FileAgentConfiguration) or ((FreshFXAgentConfiguration) and (vxinstall)); BeforeInstall: RenameBinaryFiles('hostconfigwxcommon.exe');
Source: ..\..\onlinehelp\help_output\HostAgentConfiguration\hostconfig.chm; DestDir: {code:Getdirvx}; Flags: ignoreversion uninsremovereadonly overwritereadonly; Attribs: readonly; Check: ((Not FreshFXAgentConfiguration) and (Not FreshVXAgentConfiguration)) or FreshVXAgentConfiguration or ((FreshVXAgentConfiguration) and (fxinstall))  or (VolumeAgentConfiguration or FileAgentConfiguration) or ((FreshFXAgentConfiguration) and (vxinstall));
; Application Usability
Source: ..\applicationagent\{#Configuration}\appservice.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('appservice.exe');

; Packaging the solution scripts
;Source: ..\..\Solutions\InMageSQL\bin\{#Configuration}\InMageSQL.dll; DestDir: {code:Getdirvx}; Flags: ignoreversion; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('InMageSQL.dll');

Source: ..\Utilities\Windows\ExchangeValidation\ExchangeValidation\bin\{#Configuration}\ExchangeValidation.exe; DestDir: {code:Getdirvx}\consistency; Flags: ignoreversion overwritereadonly; Check: VolumeAgentInstall; BeforeInstall: RenameBinaryFiles('ExchangeValidation.exe');

;Packaging the flush_registry binary inorder to flush the HKLM\SV SYSTEMS registry hive.
Source: ..\Utilities\Windows\flush_registry\{#Configuration}\flush_registry.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion; Check: VolumeAgentInstall;

; Packaging the scsicmdutil.exe 
Source: ..\Utilities\Windows\scsicmdutil\{#Configuration}\scsicmdutil.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion; Check: VolumeAgentInstall;

; Packaging the MTConfigUpdate.ps1 
Source: ..\..\build\scripts\deployment\Windows\MTConfigUpdate.ps1; DestDir: {code:Getdirvx}; Flags: ignoreversion; Check: VolumeAgentInstall;

; Packaging MARS agent.
Source: ..\setup\MARS\MARSAgentInstaller.exe; DestDir: {code:Getdirvx}; Flags: ignoreversion; Check: VolumeAgentInstall and IsRoleSelected; BeforeInstall: RenameBinaryFiles('MARSAgentInstaller.exe');

; NOTE: Don't use "Flags: ignoreversion" on any shared system files.
;   Used on our files to allow developers to re-install setups with same version number w/o excessive prompting

; Packaging InDskFlt driver
Source: ..\drivers\InVolFlt\windows\DiskFlt\x64\Win7{#Configuration}\InDskFlt.sys; DestDir: {sys}\drivers; Check: IsWin64 and (VolumeAgentInstall) and (not IsRoleSelected); DestName: InDskFlt.sys; Afterinstall: chkdrvfile; Flags: 64bit ignoreversion   replacesameversion; MinVersion: 0, 6.1; OnlyBelowVersion: 0, 6.2; BeforeInstall: RenameDriverFiles('InDskFlt.sys');
Source: ..\drivers\InVolFlt\windows\DiskFlt\x64\Win8{#Configuration}\InDskFlt.sys; DestDir: {sys}\drivers; Check: IsWin64 and (VolumeAgentInstall) and (not IsRoleSelected); DestName: InDskFlt.sys; Afterinstall: chkdrvfile; Flags: 64bit ignoreversion   replacesameversion; MinVersion: 0, 6.2; OnlyBelowVersion: 0, 6.3; BeforeInstall: RenameDriverFiles('InDskFlt.sys');
Source: ..\drivers\InVolFlt\windows\DiskFlt\x64\Win8.1{#Configuration}\InDskFlt.sys; DestDir: {sys}\drivers; Check: IsWin64 and (VolumeAgentInstall) and (not IsRoleSelected); DestName: InDskFlt.sys; Afterinstall: chkdrvfile; Flags: 64bit ignoreversion   replacesameversion; MinVersion: 0, 6.3; BeforeInstall: RenameDriverFiles('InDskFlt.sys');


#ifdef DEBUG
Source: ..\AppConfiguratorAPITestBed\{#Configuration}\AppConfiguratorAPITestBed.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion; Check: VolumeAgentInstall
Source: ..\applicationagent\{#Configuration}\appservice.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion; Check: VolumeAgentInstall
Source: ..\AppWizard\{#Configuration}\AppWizard.pdb; DestDir: {code:Getdirvx}; Destname: ApplicationReadinesswizard.pdb; Flags: ignoreversion
Source: ..\dataprotection\{#Configuration}\dataprotection.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion
Source: ..\s2\{#Configuration2}\s2.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion
Source: ..\service\{#Configuration2}\svagents.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion
Source: ..\cachemgr\{#Configuration}\cachemgr.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion
Source: ..\cdpmgr\{#Configuration}\cdpmgr.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion
Source: ..\unregagent\{#Configuration}\unregagent.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion
Source: ..\cdpcli\{#Configuration}\cdpcli.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion
Source: ..\cxcli\{#Configuration}\cxcli.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion
Source: ..\FailoverCommadUtil\{#Configuration}\FailoverCommandUtil.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion
Source: ..\vacp\{#Configuration}\vacp.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion
Source: ..\vacp\X64\{#Configuration}\vacp_X64.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion

; Packaging InDskFlt driver pdb's
Source: ..\drivers\InVolFlt\windows\DiskFlt\Win32\Win7Debug\InDskFlt.pdb; DestDir: {sys}\drivers; Check: (not IsWin64) and (VolumeAgentInstall); DestName: InDskFlt.pdb; Flags: ignoreversion ; MinVersion: 0, 6.1; OnlyBelowVersion: 0, 6.2;
Source: ..\drivers\InVolFlt\windows\DiskFlt\x64\Win7Debug\InDskFlt.pdb; DestDir: {sys}\drivers; Check: IsWin64 and (VolumeAgentInstall); DestName: InDskFlt.pdb; Flags: 64bit ignoreversion ; MinVersion: 0, 6.1; OnlyBelowVersion: 0, 6.2;
Source: ..\drivers\InVolFlt\windows\DiskFlt\Win32\Win8Debug\InDskFlt.pdb; DestDir: {sys}\drivers; Check: (not IsWin64) and (VolumeAgentInstall); DestName: InDskFlt.pdb; Flags: ignoreversion ; MinVersion: 0, 6.2; OnlyBelowVersion: 0, 6.3;
Source: ..\drivers\InVolFlt\windows\DiskFlt\x64\Win8Debug\InDskFlt.pdb; DestDir: {sys}\drivers; Check: IsWin64 and (VolumeAgentInstall); DestName: InDskFlt.pdb; Flags: 64bit ignoreversion ; MinVersion: 0, 6.2; OnlyBelowVersion: 0, 6.3;
Source: ..\drivers\InVolFlt\windows\DiskFlt\Win32\Win8.1Debug\InDskFlt.pdb; DestDir: {sys}\drivers; Check: (not IsWin64) and (VolumeAgentInstall); DestName: InDskFlt.pdb; Flags: ignoreversion ; MinVersion: 0, 6.3;
Source: ..\drivers\InVolFlt\windows\DiskFlt\x64\Win8.1Debug\InDskFlt.pdb; DestDir: {sys}\drivers; Check: IsWin64 and (VolumeAgentInstall); DestName: InDskFlt.pdb; Flags: 64bit ignoreversion ; MinVersion: 0, 6.3;

Source: ..\drivers\Tests\win32\diskgenericdll\{#Configuration}\diskgeneric.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion
Source: ..\cxpsclient\{#Configuration}\cxpsclient.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion
Source: ..\cxps\{#Configuration}\cxps.pdb; DestDir: {code:Getdirvx}\transport; Check: FileAgentInstall; Flags: ignoreversion
Source: ..\cxpscli\{#Configuration}\cxpscli.pdb; DestDir: {code:Getdirvx}\transport; Check: FileAgentInstall; Flags: ignoreversion
Source: ..\inmsync\{#Configuration}\inmsync.pdb; DestDir: {code:Getdirfx}\FileRep; Check: FileAgentInstall; Flags: ignoreversion;

Source: ..\csgetfingerprint\{#Configuration}\csgetfingerprint.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly;
Source: ..\gencert\{#Configuration}\gencert.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly;
Source: ..\genpassphrase\{#Configuration}\genpassphrase.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly;
Source: ..\verifycert\{#Configuration}\verifycert.pdb; DestDir: {code:Getdirvx}; Flags: ignoreversion overwritereadonly;
Source: ..\csharphttpclient\bin\{#Configuration}\httpclient.pdb; DestDir: {code:Getdirfx};Check: VolumeAgentInstall; Flags: ignoreversion overwritereadonly;
#endif

; Installing the FX files in to the respective locations

Source: ..\..\server\windows\cx_backup_withfx.bat; DestDir: {code:Getdirfx}\FileRep; Check: FileAgentInstall

; NOTE: Don't use "Flags: ignoreversion" on any shared system files
; Our main files
Source: ..\frsvc\{#Configuration2}\frsvc.exe; DestDir: {code:Getdirfx}\FileRep; Flags: ignoreversion; Check: FileAgentInstall; BeforeInstall: RenameFileRepBinaryFiles('frsvc.exe');

; Used only for Fresh FX Installation and if Only FX is installed in the setup and we are trying to upgrade FX alone.
Source: ..\hostconfigwxcommon\{#Configuration}\hostconfigwxcommon.exe; DestDir: {code:Getdirfx}\FileRep; Flags: ignoreversion; Check: (FreshFXAgentConfiguration);
Source: ..\..\onlinehelp\help_output\HostAgentConfiguration\hostconfig.chm; DestDir: {code:Getdirvx}\FileRep; Flags: ignoreversion uninsremovereadonly overwritereadonly; Attribs: readonly; Check: (FreshFXAgentConfiguration);
Source: ..\unregagent\{#Configuration}\unregagent.exe; DestDir: {code:Getdirfx}\FileRep; Flags: ignoreversion; Check: FileAgentInstall; BeforeInstall: RenameFileRepBinaryFiles('unregagent.exe');
Source: ..\inmmessage\{#Configuration}\inmmessage.dll; DestDir: {code:Getdirfx}\FileRep; Flags: ignoreversion; Check: FileAgentInstall; BeforeInstall: RenameFileRepBinaryFiles('inmmessage.dll');
Source: ..\..\Solutions\Sharepoint\spapp\{#Configuration}\spapp.exe; DestDir: {code:Getdirfx}; Flags: ignoreversion; Check: VolumeAgentInstall; BeforeInstall: RenameFileRepBinaryFiles('spapp.exe');
Source: ..\..\Solutions\Sharepoint\spapp\{#Configuration}\spapp.exe.config; DestDir: {code:Getdirfx}; Flags: ignoreversion; Check: VolumeAgentInstall;
Source: ..\..\Solutions\Sharepoint\scripts\spapploader.bat; DestDir: {code:Getdirfx}; Flags: ignoreversion; Check: VolumeAgentInstall;
Source: ..\..\Solutions\DB2\db2_consistency.bat; DestDir: {code:Getdirfx}; Flags: ignoreversion; Check: VolumeAgentInstall;
Source: ..\inmsync\{#Configuration}\inmsync.exe; DestDir: {code:Getdirfx}\FileRep; Check: FileAgentInstall; Flags: ignoreversion;

Source: ..\cxps\{#Configuration}\cxps.exe; DestDir: {code:Getdirvx}\transport; Check: FileAgentInstall; Flags: ignoreversion
Source: ..\cxpscli\{#Configuration}\cxpscli.exe; DestDir: {code:Getdirvx}\transport; Check: FileAgentInstall; Flags: ignoreversion
Source: ..\cxpslib\cxps.conf; DestDir: {code:Getdirvx}\transport; Flags: ignoreversion; Check: (FreshInstallationCheck);
Source: ..\cxpslib\pem\dh.pem; DestDir: {code:Getdirvx}\transport\pem; Check: FileAgentInstall; Flags: ignoreversion
Source: ..\cxpslib\pem\servercert.pem; DestDir: {code:Getdirvx}\transport\pem; Check: FileAgentInstall; Flags: ignoreversion


; Packaging .conf files.
Source: drscout_temp.conf; DestDir: {code:Getdirvx}\Application Data\etc; Flags: overwritereadonly ignoreversion; Check: VolumeAgentInstall
Source: drscout.conf; DestDir: {code:Getdirvx}\Application Data\etc; DestName: drscout.conf.new; Flags: overwritereadonly ignoreversion; Check: (vxinstall) and VolumeAgentInstall
Source: drscout.conf; DestDir: {code:Getdirvx}\Application Data\etc; DestName: drscout.conf; Flags: overwritereadonly ignoreversion; Check: (not vxinstall) and VolumeAgentInstall

#ifdef DEBUG
Source: ..\frsvc\{#Configuration2}\frsvc.pdb; DestDir: {code:Getdirfx}\FileRep
Source: ..\hostconfigwxcommon\{#Configuration}\hostconfigwxcommon.pdb; DestDir: {code:Getdirfx}\FileRep; Flags: ignoreversion
Source: ..\unregagent\{#Configuration}\unregagent.pdb; DestDir: {code:Getdirfx}\FileRep
#endif

[Icons]
; Creating this icon only for FX Fresh Installation case and FX Upgrade case.
Name: {group}{#PRODUCT}\Unified Agent\Agent Configuration; Filename: {code:Getdirfx}\FileRep\hostconfigwxcommon.exe; Check: (FreshFXAgentConfiguration) or (FileAgentConfiguration)
Name: {group}{#PRODUCT}\Unified Agent\Uninstall Agent; Filename: {uninstallexe}
; Creating this icon if both FX and VX are installed Freshly or Upgraded.
Name: {group}{#PRODUCT}\Unified Agent\Agent Configuration; Filename: {code:Getdirvx}\hostconfigwxcommon.exe; Check: FreshVXAgentConfiguration or (VolumeAgentInstall and FileAgentInstall) or hostconfigiconcommon  or ((FreshVXAgentConfiguration) and (fxinstall))  or ((FreshFXAgentConfiguration) and (vxinstall)) or ((vxinstall) and (fxinstall))

; Creating this icon for running Application Wizard
Name: {group}{#PRODUCT}\Unified Agent\Application Readiness wizard; Filename: {code:Getdirvx}\ApplicationReadinesswizard.exe; Check: VolumeAgentInstall



; Delete previously created icons for VX and FX
[InstallDelete]
Type: filesandordirs; Name: {group}{#PRODUCT}\File Replication
Type: filesandordirs; Name: {group}{#PRODUCT}\Volume Replication
Type: files; Name: {code:GetVXInstallDir}\unins*.dat
Type: files; Name: {code:GetVXInstallDir}\unins*.exe

[Dirs]
; This is VX related
Name: {code:Getdirvx}; Flags: uninsalwaysuninstall
Name: {code:Getdirvx}\Application Data; Flags: uninsalwaysuninstall
Name: {code:Getdirvx}\Application Data\etc; Flags: uninsalwaysuninstall
; Default source and replication directory for FX failover jobs
Name: {code:Getdirvx}\Failover; Flags: uninsalwaysuninstall
Name: {code:Getdirvx}\Failover\Data; Flags: uninsalwaysuninstall

;This is FX related.
Name: {code:Getdirfx}\Application Data; Flags: uninsalwaysuninstall
Name: {code:Getdirfx}\Application Data\replshares; Flags: uninsalwaysuninstall
;This is for ICAT related.
Name: {code:Getdirfx}\ICAT; Flags: uninsalwaysuninstall
Name : {sd}\ProgramData\Microsoft Azure Site Recovery;
Name : {sd}\ProgramData\Microsoft Azure Site Recovery\private;

#include "Windows_Services.iss"
#include "os_check.iss"

[Registry]
; Writing the registry values for VX.
; Keys shared by the sentinel and outpost agent
Root: HKLM; Subkey: Software\SV Systems; Flags: uninsdeletekeyifempty; Check: VolumeAgentInstall

; Key for VX agent install location
Root: HKLM; Subkey: Software\SV Systems\VxAgent; Flags: uninsdeletekey; Check: VolumeAgentInstall
Root: HKLM; Subkey: Software\SV Systems\VxAgent; ValueType: string; ValueName: InstallDirectory; ValueData: {code:Getdirvx}; Flags: uninsdeletevalue; Check: VolumeAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\VxAgent','InstallDirectory')
Root: HKLM; Subkey: Software\SV Systems\VxAgent; ValueType: string; ValueName: ConfigPathname; ValueData: {code:Getdirvx}\Application Data\etc\drscout.conf; Flags: uninsdeletevalue; Check: VolumeAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\VxAgent','ConfigPathname')
Root: HKLM; Subkey: Software\SV Systems\VxAgent; ValueType: string; ValueName: VxAgent; ValueData: VxAgent; Flags: uninsdeletevalue; Check: VolumeAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\VxAgent','VxAgent')
Root: HKLM; Subkey: Software\SV Systems\VxAgent; ValueType: string; ValueName: PROD_VERSION; ValueData: {#PRODUCTVERSION}; Flags: uninsdeletevalue; Check: VolumeAgentInstall

; Keys for InDskFlt driver
Root: HKLM; Subkey: System\CurrentControlSet\Services\InDskFlt; Flags: uninsdeletekey; Check: (VolumeAgentInstall) and (not IsRoleSelected)
Root: HKLM; Subkey: System\CurrentControlSet\Services\InDskFlt; ValueType: string; ValueName: DisplayName; ValueData: Disk Upper Class Filter Driver; Check: (VolumeAgentInstall) and (not IsRoleSelected); AfterInstall: MyAfterInstall('HKLM','System\CurrentControlSet\Services\InDskFlt','DisplayName')
Root: HKLM; Subkey: System\CurrentControlSet\Services\InDskFlt; ValueType: dword; ValueName: ErrorControl; ValueData: 1; Flags: uninsdeletekey; Check: (VolumeAgentInstall) and (not IsRoleSelected); AfterInstall: MyAfterInstall('HKLM','System\CurrentControlSet\Services\InDskFlt','ErrorControl')
Root: HKLM; Subkey: System\CurrentControlSet\Services\InDskFlt; ValueType: string; ValueName: Group; ValueData: PnP Filter; Flags: uninsdeletekey; Check: (VolumeAgentInstall) and (not IsRoleSelected); AfterInstall: MyAfterInstall('HKLM','System\CurrentControlSet\Services\InDskFlt','Group')
Root: HKLM; Subkey: System\CurrentControlSet\Services\InDskFlt; ValueType: dword; ValueName: Start; ValueData: 0; Flags: uninsdeletekey; Check: (VolumeAgentInstall) and (not IsRoleSelected); AfterInstall: MyAfterInstall('HKLM','System\CurrentControlSet\Services\InDskFlt','Start')
Root: HKLM; Subkey: System\CurrentControlSet\Services\InDskFlt; ValueType: dword; ValueName: Type; ValueData: 1; Flags: uninsdeletekey; Check: (VolumeAgentInstall) and (not IsRoleSelected); AfterInstall: MyAfterInstall('HKLM','System\CurrentControlSet\Services\InDskFlt','Type')
Root: HKLM; Subkey: System\CurrentControlSet\Services\InDskFlt; ValueType: expandsz; ValueName: ImagePath; ValueData: system32\DRIVERS\indskflt.sys; Flags: uninsdeletekey; Check: (VolumeAgentInstall) and (not IsRoleSelected); AfterInstall: MyAfterInstall('HKLM','System\CurrentControlSet\Services\InDskFlt','ImagePath')

Root: HKLM; Subkey: System\CurrentControlSet\Services\InDskFlt\Parameters; Flags: uninsdeletekey; Check: (VolumeAgentInstall) and (not IsRoleSelected)
Root: HKLM; Subkey: System\CurrentControlSet\Services\InDskFlt\Parameters; ValueType: string; ValueName: DefaultLogDirectory; ValueData: {code:Getdirvx}\Application Data; Flags: uninsdeletekey; Check: (VolumeAgentInstall) and (not IsRoleSelected); AfterInstall: MyAfterInstall('HKLM','System\CurrentControlSet\Services\InDskFlt\Parameters','DefaultLogDirectory')


; Keys for system event log error text used by the driver, don't remove on uninstall, as the event viewer can't lookup error text then
Root: HKLM; Subkey: System\CurrentControlSet\Services\EventLog\System\InDskFlt; Flags: uninsdeletekey; Check: (VolumeAgentInstall) and (not IsRoleSelected)
Root: HKLM; Subkey: System\CurrentControlSet\Services\EventLog\System\InDskFlt; ValueType: expandsz; ValueName: EventMessageFile; ValueData: %SystemRoot%\System32\drivers\InDskFlt.sys; Check: (VolumeAgentInstall) and (not IsRoleSelected); AfterInstall: MyAfterInstall('HKLM','System\CurrentControlSet\Services\EventLog\System\InDskFlt','EventMessageFile')
Root: HKLM; Subkey: System\CurrentControlSet\Services\EventLog\System\InDskFlt; ValueType: dword; ValueName: TypesSupported; ValueData: 7; Check: (VolumeAgentInstall) and (not IsRoleSelected); AfterInstall: MyAfterInstall('HKLM','System\CurrentControlSet\Services\EventLog\System\InDskFlt','TypesSupported')
Root: HKLM; Subkey: System\CurrentControlSet\Control\Class\{{4d36e967-e325-11ce-bfc1-08002be10318}; ValueType: multisz; ValueName: UpperFilters; ValueData: {code:GetUpperfilterkey}{break}{olddata}; AfterInstall : StartInDskFltService; Check: (VolumeAgentInstall) and (not IsRoleSelected)
Root: HKLM; Subkey: System\CurrentControlSet\Services\Eventlog\Application\svagents; Flags: uninsdeletekeyifempty; Check: VolumeAgentInstall
Root: HKLM; Subkey: System\CurrentControlSet\Services\Eventlog\Application\svagents; ValueType: string; ValueName: EventMessageFile; ValueData: {code:Getdirvx}\inmmessage.dll; Flags: uninsdeletevalue; Check: VolumeAgentInstall; AfterInstall: MyAfterInstall('HKLM','System\CurrentControlSet\Services\Eventlog\Application\svagents','EventMessageFile')
Root: HKLM; Subkey: System\CurrentControlSet\Services\Eventlog\Application\svagents; ValueType: dword; ValueName: TypesSupported; ValueData: 7; Flags: uninsdeletevalue; Check: VolumeAgentInstall; AfterInstall: MyAfterInstall('HKLM','System\CurrentControlSet\Services\Eventlog\Application\svagents','TypesSupported')

; Writing the registry values for FX.
Root: HKLM; Subkey: Software\SV Systems; Flags: uninsdeletekeyifempty; Check: FileAgentInstall
Root: HKLM; Subkey: Software\SV Systems; ValueType: string; ValueName: ServerName; ValueData: 192.168.80.201; Flags: uninsdeletevalue createvalueifdoesntexist; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems','ServerName')
Root: HKLM; Subkey: Software\SV Systems; ValueType: dword; ValueName: ServerHttpPort; ValueData: 80; Flags: uninsdeletevalue createvalueifdoesntexist; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems','ServerHttpPort')
Root: HKLM; Subkey: Software\SV Systems; ValueType: string; ValueName: HostId; ValueData: {code:GetOrGenerateGUID}; Flags: createvalueifdoesntexist uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems','HostId')
Root: HKLM; Subkey: Software\SV Systems; ValueType: string; ValueName: ThrottleBootStrap; ValueData: /throttle_bootstrap.php; Flags: uninsdeletevalue createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems','ThrottleBootStrap')
Root: HKLM; Subkey: Software\SV Systems; ValueType: string; ValueName: ThrottleOutpost; ValueData: /throttle_outpost.php; Flags: uninsdeletevalue createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems','ThrottleOutpost')
Root: HKLM; Subkey: Software\SV Systems; ValueType: string; ValueName: ThrottleSentinel; ValueData: /throttle_sentinel.php; Flags: uninsdeletevalue createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems','ThrottleSentinel')
Root: HKLM; Subkey: Software\SV Systems; ValueType: string; ValueName: ThrottleAgent; ValueData: /throttle_agent.php; Flags: uninsdeletevalue createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems','ThrottleAgent')
Root: HKLM; Subkey: Software\SV Systems; ValueType: dword; ValueName: DebugLogLevel; ValueData: {#DefaultLogLevel}; Flags: uninsdeletevalue createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems','DebugLogLevel')

Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; Flags: uninsdeletekey; ValueType: dword; ValueName: EnableDeleteOptions; ValueData: 3255; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','EnableDeleteOptions')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: dword; ValueName: EnableFrLogFileDeletion; ValueData: 0; Flags: createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','EnableFrLogFileDeletion')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: dword; ValueName: EnableEncryption; ValueData: 0; Flags: createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','EnableEncryption')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: FxAgent; ValueData: FxAgent; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','FxAgent')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: dword; ValueName: AvailableDrives; ValueData: 0; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','AvailableDrives')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: CacheDirectory; ValueData: {code:Getdirfx}\Application Data; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','CacheDirectory'); Flags: 
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: ClearReturnHomeURL; ValueData: /clear_return_home.php; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','ClearReturnHomeURL')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: ClearSnapshotURL; ValueData: /clear_snapshot_drive.php; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','ClearSnapshotURL')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: ConfiguredHostname; ValueData: ; Flags: uninsdeletevalue  createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','ConfiguredHostname')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: ConfiguredIP; ValueData: ; Flags: uninsdeletevalue createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','ConfiguredIP')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: PROD_VERSION; ValueData: {#PRODUCTVERSION}; Flags: uninsdeletevalue; Check: FileAgentInstall
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: FilenamePrefix; ValueData: completed_*; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','FilenamePrefix')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: GetFileReplicationConfigurationURL; ValueData: /get_file_replication_config.php; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','GetFileReplicationConfigurationURL')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: GetProtectedDrivesURL; ValueData: /get_outpost_agent_protected_drives.php; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','GetProtectedDrivesURL')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: GetResyncDrivesURL; ValueData: /get_resync_drives.php; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','GetResyncDrivesURL')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: GetReturnHomeDrivesURL; ValueData: /get_return_home_drives.php; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','GetReturnHomeDrivesURL')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: dword; ValueName: InitialSyncComplete; ValueData: 0; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','InitialSyncComplete')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: dword; ValueName: MaxFrLogPayload; ValueData: 0; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','MaxFrLogPayload')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: dword; ValueName: ProtectedDrives; ValueData: 0; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','ProtectedDrives')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: dword; ValueName: RegisterHostIntervel; ValueData: 600; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','RegisterHostIntervel')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: RegisterURL; ValueData: /register_fr_agent.php; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','RegisterURL')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: ResyncFilenamePrefix; ValueData: resync_*; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','ResyncFilenamePrefix')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: ResyncSourceDirectoryPath; ValueData: /home/svsystems; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','ResyncSourceDirectoryPath')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: InmsyncExe; ValueData: {code:Getdirfx}\FileRep\inmsync.exe; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','InmsyncExe')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: ShellCommand; ValueData: "cmd /c "; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','ShellCommand')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: SnapshotURL; ValueData: /get_snapshot_drive.php; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','SnapshotURL')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: SourceDirectoryPrefix; ValueData: /home/svsystems; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','SourceDirectoryPrefix')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: dword; ValueName: SplitMirrorDrives; ValueData: 0; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','SplitMirrorDrives')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: UnregisterURL; ValueData: /unregister_fr_agent.php; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','UnregisterURL')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: UpdateFileReplicationStatusURL; ValueData: /update_file_replication_status.php; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','UpdateFileReplicationStatusURL')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: UpdateStatusURL; ValueData: /update_status.php; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','UpdateStatusURL')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: dword; ValueName: UseConfiguredHostname; ValueData: 0; Flags: uninsdeletevalue createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','UseConfiguredHostname')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: dword; ValueName: UseConfiguredIP; ValueData: 0; Flags: uninsdeletevalue createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','UseConfiguredIP')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: dword; ValueName: VolPak; ValueData: 0; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','VolPak')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: dword; ValueName: MaxResyncThreads; ValueData: 8; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','MaxResyncThreads')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: dword; ValueName: MaxOutpostThreads; ValueData: 4; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','MaxOutpostThreads')
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: UpdatePermissionStateUrl; ValueData: /update_permission_state.php; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','UpdatePermissionStateUrl')

Root: HKLM; Subkey: System\CurrentControlSet\Services\Eventlog\Application\frsvc; Flags: uninsdeletekeyifempty; Check: FileAgentInstall
Root: HKLM; Subkey: System\CurrentControlSet\Services\Eventlog\Application\frsvc; ValueType: string; ValueName: EventMessageFile; ValueData: {code:Getdirfx}\FileRep\inmmessage.dll; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','System\CurrentControlSet\Services\Eventlog\Application\frsvc','EventMessageFile')
Root: HKLM; Subkey: System\CurrentControlSet\Services\Eventlog\Application\frsvc; ValueType: dword; ValueName: TypesSupported; ValueData: 7; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','System\CurrentControlSet\Services\Eventlog\Application\frsvc','TypesSupported')

; Keys for Transport parameters
Root: HKLM; Subkey: Software\SV Systems\Transport; Flags: uninsdeletekey; Check: FileAgentInstall
Root: HKLM; Subkey: Software\SV Systems\Transport; ValueType: string; ValueName: FTPHost; ValueData: 10.1.1.134; Flags: createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\Transport','FTPHost')
Root: HKLM; Subkey: Software\SV Systems\Transport; ValueType: string; ValueName: FTPUser; ValueData: svs; Flags: createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\Transport','FTPUser')
Root: HKLM; Subkey: Software\SV Systems\Transport; ValueType: string; ValueName: FTPKey; ValueData: svs; Flags: createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\Transport','FTPKey')
Root: HKLM; Subkey: Software\SV Systems\Transport; ValueType: dword; ValueName: FTPPort; ValueData: 21; Flags: createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\Transport','FTPPort')
Root: HKLM; Subkey: Software\SV Systems\Transport; ValueType: string; ValueName: FTPInfoURL; ValueData: /get_ftp_config.php; Flags: createvalueifdoesntexist; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\Transport','FTPInfoURL')

; Key for FX agent install location
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: InstallDirectory; ValueData: {code:Getdirfx}; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','InstallDirectory')
; Key for FX installed version
Root: HKLM; Subkey: Software\SV Systems\FileReplicationAgent; ValueType: string; ValueName: FxVersion; ValueData: {#PRODUCTVERSION}_{#BUILDPHASE}; Flags: uninsdeletevalue; Check: FileAgentInstall; AfterInstall: MyAfterInstall('HKLM','Software\SV Systems\FileReplicationAgent','FxVersion')

;Creating the InMage Systems Hive for tracking the  products that are installed and the product details..
Root: HKLM; Subkey: Software\InMage Systems; Flags: uninsdeletekeyifempty; check:((FileAgentInstall)or (VolumeAgentInstall))
Root: HKLM; Subkey: Software\InMage Systems\Installed Products; Flags: uninsdeletekeyifempty; check: ((FileAgentInstall) or (VolumeAgentInstall))

;Creating the FX Sub-Key to track the FX product Details
;FX is addressed as "4" Registry Key
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\4; Flags: uninsdeletekey; check:(FileAgentInstall) 
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\4; ValueType: string; ValueName: Product_Name; ValueData: FX; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\4','Product_Name'); check:(FileAgentInstall)  
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\4; ValueType: string; ValueName: Version; ValueData: {#VERSION}; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\4','Version');   check:(FileAgentInstall)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\4; ValueType: string; ValueName: Product_Version; ValueData: {#PRODUCTVERSION}; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\4','Product_Version');   check:(FileAgentInstall)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\4; ValueType: string; ValueName: Partner_Code; ValueData: {#PARTNERCODE}; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\4','Partner_Code');   check:(FileAgentInstall)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\4; ValueType: string; ValueName: Build_Tag; ValueData: {#BUILDTAG}; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\4','Build_Tag');  check:(FileAgentInstall)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\4; ValueType: string; ValueName: Build_Phase; ValueData: {#BUILDPHASE}; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\4','Build_Phase');   check:(FileAgentInstall)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\4; ValueType: string; ValueName: Build_Number; ValueData: {#BUILDNUMBER}; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\4','Build_Number');   check:(FileAgentInstall)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\4; ValueType: string; ValueName: InstallDirectory; ValueData: {code:Getdirfx}; Flags: uninsdeletevalue;AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\4','InstallDirectory');  check:(FileAgentInstall)

;Creating the VX Sub-Key to track the VX product Details
;VX is addressed as "5" Registry Key
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\5; Flags: uninsdeletekey; check:(VolumeAgentInstall)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\5; ValueType: string; ValueName: Product_Name; ValueData: VX; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\5','Product_Name'); check:(VolumeAgentInstall)  
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\5; ValueType: string; ValueName: Version; ValueData: {#VERSION}; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\5','Version');  check:(VolumeAgentInstall)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\5; ValueType: string; ValueName: Product_Version; ValueData: {#PRODUCTVERSION}; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\5','Product_Version');   check:(VolumeAgentInstall)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\5; ValueType: string; ValueName: Partner_Code; ValueData: {#PARTNERCODE}; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\5','Partner_Code'); check:(VolumeAgentInstall)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\5; ValueType: string; ValueName: Build_Tag; ValueData: {#BUILDTAG}; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\5','Build_Tag'); check:(VolumeAgentInstall)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\5; ValueType: string; ValueName: Build_Phase; ValueData: {#BUILDPHASE}; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\5','Build_Phase');   check:(VolumeAgentInstall)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\5; ValueType: string; ValueName: Build_Number; ValueData: {#BUILDNUMBER}; Flags: uninsdeletevalue; AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\5','Build_Number'); check:(VolumeAgentInstall)
Root: HKLM; Subkey: Software\InMage Systems\Installed Products\5; ValueType: string; ValueName: InstallDirectory; ValueData: {code:Getdirvx}; Flags: uninsdeletevalue;AfterInstall: MyAfterInstall('HKLM','Software\InMage Systems\Installed Products\5','InstallDirectory'); check:(VolumeAgentInstall)


[INI]
; This is only for VX.
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent.transport; Key: SslClientFile; String: {code:Getdirvx}\transport\client.pem; Flags: createkeyifdoesntexist; Languages:  
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent.transport; Key: SslCertificatePathname; String: {code:Getdirvx}\transport\newcert.pem; Flags: createkeyifdoesntexist; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent.transport; Key: Hostname; String: {code:Getip}; Flags: uninsdeleteentry; Check: (not vxinstall) and VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent.transport; Key: Port; String: {code:Getport}; Flags: uninsdeleteentry; Check: (not vxinstall) and VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: LogPathname; String: {code:Getdirvx}\; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: CacheDirectory; String: {code:Getdirvx}\Application Data; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: VsnapConfigPathname; String: {code:Getdirvx}\Application Data\etc\vsnap; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: VsnapPendingPathname; String: {code:Getdirvx}\Application Data\etc\pendingvsnap; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages:  
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: OffloadSyncPathname; String: {code:Getdirvx}\dataprotection.exe; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: FastSyncExePathname; String: {code:Getdirvx}\dataprotection.exe; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: DiffTargetCacheDirectoryPrefix; String: {code:Getdirvx}\Application Data; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: DiffSourceExePathname; String: {code:Getdirvx}\s2.exe; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: DataProtectionExePathname; String: {code:Getdirvx}\dataprotection.exe; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: DiffTargetExePathname; String: {code:Getdirvx}\dataprotection.exe; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: InstallDirectory; String: {code:Getdirvx}; Flags: createkeyifdoesntexist uninsdeleteentry; Check: VolumeAgentInstall
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: Version; String: {#VERSION}; Flags: uninsdeleteentry; Check: VolumeAgentInstall
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: PROD_VERSION; String: {#PRODUCTVERSION}_{#BUILDPHASE}; Flags: uninsdeleteentry; Check: VolumeAgentInstall
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: TargetChecksumsDir; String: {code:Getdirvx}\cksums; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: CdpcliExePathname; String: {code:Getdirvx}\cdpcli.exe; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: CacheMgrExePathname; String: {code:Getdirvx}\cachemgr.exe; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: CdpMgrExePathname; String: {code:Getdirvx}\cdpmgr.exe; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages:  
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: LogLevel; String: {#LogLevelValue}; Flags: uninsdeleteentry; Check: FreshVolumeAgentInstall; Languages:  
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: HostId; String: {code:GetOrGenerateGUID}; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall; Languages: 
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: MaxFastSyncApplyThreads; String: {code:MaxFastSyncThreads}; Flags: uninsdeleteentry; Check: (VolumeAgentInstall) and (IsRoleSelected)
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: RegisterHostInterval; String: {code:Registerhost}; Flags: uninsdeleteentry; Check: (VolumeAgentInstall) and (IsRoleSelected)
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: NWThreadsPerReplication; String: {code:NWThreads}; Flags: uninsdeleteentry; Check: (VolumeAgentInstall) and (IsRoleSelected)
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: IOThreadsPerReplication; String: {code:IOThreads}; Flags: uninsdeleteentry; Check: (VolumeAgentInstall) and (IsRoleSelected)
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: VolpackDriverAvailable; String: 0; Flags: uninsdeleteentry; Check: (VolumeAgentInstall)
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: VsnapDriverAvailable; String: 0; Flags: uninsdeleteentry; Check: (VolumeAgentInstall)
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: FilterDriverAvailable; String: 0; Flags: uninsdeleteentry; Check: (VolumeAgentInstall) and (IsRoleSelected)
  
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: appagent; Key: ApplicationSettingsCachePath; String: {code:Getdirvx}\Application Data\etc\AppAgentCache.dat; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall
Filename: {code:Getdirvx}\Application Data\etc\drscout.conf; Section: appagent; Key: ApplicationSchedulerCachePath; String: {code:Getdirvx}\Application Data\etc\SchedulerCache.dat; Flags: createkeyifdoesntexist; Check: VolumeAgentInstall

Filename: {code:Getdirvx}\transport\cxps.conf; Section: cxps; Key: install_dir; String: {code:Getdirvx}\transport; Check: (FreshInstallationCheck) and (not IsCXInstalled)
Filename: {code:Getdirvx}\transport\cxps.conf; Section: cxps; Key: cfs_mode; String: yes; Check: IsRoleSelected and FreshInstallationCheck and (not IsCXInstalled);

[Messages]
SetupWindowTitle=%1
WizardLicense=License Terms
LicenseLabel=
LicenseLabel3=You must accept the license terms before you can install or use the software. If you do not accept the license terms, installation will not proceed. You may print the license terms by clicking the Print button below.
FinishedRestartLabel=To complete the installation of [name], Please restart your computer. Would you like to restart now?
ConfirmUninstall=Would you like to uninstall the Microsoft Azure Site Recovery Mobility Service/Master Target Server?
DiskSpaceMBLabel=

[Run]
; Executing the exe's for VX.

Filename: {code:Getdirvx}\drscout_upgrade.bat; Flags: runhidden waituntilterminated runascurrentuser; check: not (FreshInstallationCheck)

Filename: {code:Getdirfx}\configureadmin.bat; Parameters: """{code:Getdirfx}\FileRep\hostconfigwxcommon.exe""  ""{code:Getdirvx}\hostconfigwxcommon.exe"""; Flags: shellexec runhidden runascurrentuser waituntilterminated

Filename: {code:Getdirvx}\ScoutTuning.exe; Parameters:"-version ""{#VERSION}"" -role ""vconmt""" ; Flags: runhidden waituntilterminated runascurrentuser; check: IsRoleSelected

Filename: {code:Getdirvx}\gencert.exe; Parameters: -n ps --dh; Flags: runhidden waituntilterminated; Check: FreshInstallationCheck and (not IsCXInstalled)

; This is for VX Upgrade,fresh VX installation.
Filename: {code:Getdirvx}\ServiceCtrl.exe; Parameters: TrkWks stop force; Flags: runhidden ; Check: VolumeAgentInstall

; This is only for VX Upgrade, fresh VX installation.
Filename: {code:Getdirvx}\svagents.exe; Parameters: /service; Flags: runhidden ; Check: (not vxinstall) and VolumeAgentInstall; BeforeInstall : Replace_Runtime_Install_Path

Filename: {code:Getdirvx}\appservice.exe; Parameters: /service; Flags: runhidden  ; Check: (not appserviceinstall) and VolumeAgentInstall

; This is for FX Upgrade, Fresh FX Installation
Filename: {code:Getdirfx}\FileRep\frsvc.exe; Parameters: /service; Flags: runhidden ; Check: (not fxinstall) and FileAgentInstall


Filename: {code:Getdirvx}\transport\cxps.exe; Parameters: "-c ""{code:Getdirvx}\transport\cxps.conf"" install"; Flags: runhidden; Check: IsRoleSelected and FreshInstallationCheck and (not IsCXInstalled); BeforeInstall : SetcxpsGUID

; Executing the exe's for FX.This is only for Fresh FX installation Only.
Filename: {code:Getdirfx}\FileRep\hostconfigwxcommon.exe;Parameters: /enablehttps; Check: (NoReinstallCheck) and (FreshFXAgentConfiguration) and (Not vxinstall); Flags: skipifsilent; BeforeInstall: StartCXServices

; This is used Only for both option  (FX + VX).
Filename: {code:Getdirvx}\hostconfigwxcommon.exe;Parameters: /enablehttps; Check: (((NoReinstallCheck) and (FreshFXAgentConfiguration) and (FreshVXAgentConfiguration)) or ((VolumeAgentConfiguration) and (Not FileAgentConfiguration)) or ((FileAgentConfiguration) and (Not VolumeAgentConfiguration))  or FreshVXAgentConfiguration or ((FreshVXAgentConfiguration) and (fxinstall))  or ((FreshFXAgentConfiguration) and (vxinstall)) or  (hostconfigiconcommon)); Flags: skipifsilent; BeforeInstall: StartCXServicesAndReplaceLineInConfFile

; This is for enabling firewall for FX and VX binaries
Filename: {code:Getdirvx}\vxfirewall.bat; Flags: shellexec runhidden waituntilterminated runascurrentuser
Filename: {code:Getdirfx}\fxfirewall.bat; Flags: shellexec runhidden waituntilterminated runascurrentuser

Filename: {code:Getdirvx}\createlink.bat; Parameters: "UA ""{code:Getdirvx}\createlink.vbs"" ""{code:Getdirvx}\hostconfigwxcommon.exe"""; Description: Create a desktop icon for Hostconfig wizard; Flags: shellexec postinstall nowait runhidden runascurrentuser; Check: ShortcutNotExist 

[UninstallRun]
Filename: {code:Getdirvx}\InMageVSSProvider_Uninstall.cmd; Flags: shellexec runhidden waituntilterminated runascurrentuser
Filename: {app}\rem_files.bat; Parameters: "{code:Getdirvx}"" ""{code:Getdirfx}"
Filename: {code:Getdirvx}\removefiles.bat; Parameters: "UA" ; flags: runhidden;

[UninstallDelete]
; This is for VX.
Name: {code:GetVXInstallDir}\InMage*.VolumeLog; Type: files
Name: {code:GetVXInstallDir}\UAInstallLogFile.log; Type: files
Name: {code:GetVXInstallDir}\bin; Type: filesandordirs
Name: {code:GetVXInstallDir}\Consistency; Type: filesandordirs
Name: {code:GetVXInstallDir}\FileRep; Type: filesandordirs
Name: {code:GetVXInstallDir}\transport; Type: filesandordirs
Name: {code:GetVXInstallDir}\Application Data\etc\drscout.conf; Type: filesandordirs
Name: {code:GetVXInstallDir}\GetDataPoolSize; Type: filesandordirs
Name: {code:GetVXInstallDir}\Backup_Release; Type: filesandordirs
Name: {code:GetVXInstallDir}\backup_updates; Type: filesandordirs

; This is for FX.
Type: files; Name: {code:GetFXInstallDir}\Application Data\inmage_job_*.log
Type: files; Name: {code:GetFXInstallDir}\Application Data\daemon.conf
Type: files; Name: {group}{#PRODUCT}\Unified Agent\*
Type: dirifempty; Name: {group}{#PRODUCT}\Unified Agent
Type: dirifempty; Name: {group}{#PRODUCT}

[Code]
// Declaring the global variables we are using in this script

var
	Installation_StartTime: String;
	FxInstalled,VxInstalled : String;
	FreshInstallation : String;
	FreshInstallationFX,ErrorMsg1,ErrorMsg2 : String;
	FreshInstallationVX : String;
	FxVersionString,ConfigPathnamevx : String;
	VxVersionString : String;
	FreshFX : String;
	FreshVX : String;
	FreshVXAdd : String;
	ResultCode1,ResultCode2 : Integer ;
	a,E,H,AW,restart,setappagent : String;
	ReinstallSelected : String;
	OkToCopyLog : Boolean;
	chkdrv,DrvStatus,role,Uninstall,UpdateConfig,HostID : String;
	RoleType, HeapValueChangeRestartValue : String;
	RolePage: TWizardPage;
	Agent,Target : TNewRadioButton;
  AgentText,TargetText,RolePageText : TNewStaticText;
  RichEditViewer: TRichEditViewer;
  cancelclick: Boolean;
  ProgressPage, ProgressPageEnd: TOutputProgressWizardPage;
  Wait: Integer;
  MaxProgress: Integer;
  RenameSuccess : String;
  SilentUninstallString, CommunicationMode : String;
  CS_IP_Value, CS_Port_Value, vConInstall, PassphrasePath,PassphraseValue,CommMode,Comm_Number : String;
  LogFileString : AnsiString;
  ErrorCode : Integer;
  InstallDir, containerRegistrationStatus, Driver_Reboot, Driver_Reboot_Machine : String;
  
//This windows Api is used to exit from the process with a proper exit code.
Procedure  ExitProcess(exitCode:integer);
external 'ExitProcess@kernel32.dll stdcall';
function GetFileAttributes(lpFileName: string): DWORD;
 external 'GetFileAttributesW@kernel32.dll stdcall';
function SetFileAttributes(lpFileName: string; dwFileAttributes: DWORD): BOOL;
external 'SetFileAttributesW@kernel32.dll stdcall';

(* FreshFXAgentConfiguration --- Used to verfiy if it is FX Fresh installation or not
 1. The function will return true if the Fresh FX is installed.
 2. THe function will return false if the Fresh FX is not installed.
*)

function FreshFXAgentConfiguration : Boolean;
begin
	if FreshFX = 'IDYES' then
		Result := True
	else
		Result := False;
end;

(* FreshVXAgentConfiguration --- Used to verfiy if it is VX Fresh installation or not
 1. The function will return true if the Fresh VX is installed.
 2. THe function will return false if the Fresh VX is not installed.
*)

function FreshVXAgentConfiguration : Boolean;
begin
	if FreshVX = 'IDYES' then
		Result := True
	else
		Result := False;
end;

procedure chkdrvfile();
begin
	chkdrv := 'IDYES' ;
end;

(* FileAgentConfiguration --- Used to verfiy if FX is already installed (Upgrade case)
 1. The function will return true if the setup is having FX installed init
 2. THe function will return false if the setup is not having FX installed init
*)

function FileAgentConfiguration : Boolean;
begin
	if FxInstalled = 'IDYES' then
		Result := True
	else
		Result := False;
end;

(* VolumeAgentConfiguration --- Used to verfiy if VX is already installed (Upgrade case)
 1. The function will return true if the setup is having VX installed init
 2. THe function will return false if the setup is not having VX installed init
*)

function VolumeAgentConfiguration : Boolean;
begin
	if VxInstalled = 'IDYES' then
		Result := True
	else
		Result := False;
end;

function ShortcutNotExist : Boolean;
begin
    if FileExists(ExpandConstant('{userdesktop}\hostconfigwxcommon.lnk')) or FileExists(ExpandConstant('{commondesktop}\hostconfigwxcommon.lnk')) then
    begin
        Result := False;
    end
    else
        Result := True; 
end;

procedure DataPoolSize();
var
  ErrorCode: Integer;
  GetDataPoolSizeFile : String;
  LogFileString : AnsiString;
begin
	GetDataPoolSizeFile := ExpandConstant('{code:Getdirvx}\GetDataPoolSize\GetDataPoolSize.exe')
	if FileExists(GetDataPoolSizeFile) then
	begin
	    Log('Invoking DataPoolSize.bat script.');
	    ShellExec('', ExpandConstant('{code:Getdirvx}\GetDataPoolSize\DataPoolSize.bat'),ExpandConstant('{code:Getdirvx}\GetDataPoolSize'), '', SW_HIDE, ewWaitUntilTerminated, ErrorCode)
			
			if FileExists(ExpandConstant('{code:Getdirvx}\GetDataPoolSize\DatapoolsizeBinaryOutput.txt')) then	
		  begin
			  LoadStringFromFile(ExpandConstant ('{code:Getdirvx}\GetDataPoolSize\DatapoolsizeBinaryOutput.txt'), LogFileString);
			  Log(#13#10 +'DataPoolsize log Starts here '+ #13#10 + '*********************************************')
		    Log(#13#10 + LogFileString);
			  Log(#13#10 + '*********************************************' + #13#10 + 'DataPoolsize log Ends here ' +#13#10 + #13#10);
		  end;
	end
	else
	   	 Log('The '+ GetDataPoolSizeFile + ' does not exist.' + Chr(13));	
	
end;

procedure ChangeReadOnlyAttribute(const FileName : String);
var
 FileAttribute : DWord;
begin
  FileAttribute := GetFileAttributes(FileName);
  if (FileAttribute and 1) = 1  then          
  begin
    FileAttribute := FileAttribute -1;
    SetFileAttributes(FileName,FileAttribute);
  end;
end;

procedure SetcxpsGUID();
begin
    if SetIniString( 'cxps', 'id', ExpandConstant('{code:GetOrGenerateGUID}'), ExpandConstant('{code:Getdirvx}\transport\cxps.conf') ) then
    begin
        Log('The GUID is successfully updated in the cxps.conf file.');
    end
    else
    begin
        Log('Unable to update GUID in the cxps.conf file.');
    end;
end;
(* FreshInstallationCheck --- Verifying if the agent is already installed or not
1. If the agent is already installed, the output from FreshInstallationCheck is false
2. If the agent is not installed, the output from FreshInstallationCheck is true *)

function FreshInstallationCheck : Boolean;
begin
	if FreshInstallation = 'IDYES' then
		Result := True
	else
		Result := False;
end;

function FileAgentInstall : Boolean;
begin
	if ((FreshInstallation = 'IDYES') and (FreshInstallationFX = 'IDYES')) or (FreshInstallationFX = 'IDYES') or FileAgentConfiguration() then
	begin
	  // This is FX Upgrade, FreshInstallation with FX.
		Result := True
	end
	else
		Result := False;
end;

function VolumeAgentInstall : Boolean;
begin
	if ((FreshInstallation = 'IDYES') and (FreshInstallationVX = 'IDYES')) or (FreshInstallationVX = 'IDYES') or VolumeAgentConfiguration()  then
	  // This is VX Upgrade, Fresh Installation with VX.
		Result := True
	else
		Result := False;
end;


// This for fresh Installation of VX.
function FreshVolumeAgentInstall : Boolean;
begin
	if ((FreshInstallation = 'IDYES') and (FreshInstallationVX = 'IDYES')) or (FreshInstallationVX = 'IDYES') then
		Result := True
	else
		Result := False;
end;


(* fxinstall --- This is used to detect if FX is installed or not
If frsvc is installed, it means FX agent is installed in the setup *)

function fxinstall : Boolean;
begin
	if (IsServiceInstalled('frsvc')) then
	begin
	  Log('frsvc service is already installed.' + Chr(13));
		Result := True
	end
	else
	  Log('frsvc service is not installed.' + Chr(13));
		Result := False;
end;

(* vxinstall --- This is used to detect if VX is installed or not
If svagents is installed, it means VX agent is installed in the setup *)

function vxinstall : Boolean;
begin
	if (IsServiceInstalled('svagents')) then
	begin
	  Log('svagents service is already installed.' + Chr(13));
		Result := True
	end
	else
	  Log('svagents service is not installed.' + Chr(13));
		Result := False;
end;

(* vxinstall --- This is used to detect if VX is installed or not
If svagents is installed, it means VX agent is installed in the setup *)

function appserviceinstall : Boolean;
begin
	if (IsServiceInstalled('InMage Scout Application Service')) then
	begin
		   Log('InMage Scout Application service is already installed.' + Chr(13));
		   Result := True;
	end
	else
	begin
	      Log('InMage Scout Application service is not installed.' + Chr(13));
	      Result := False;
	end;
end;

(* GetFXInstallDir --- This is used to get the Installation directory for FX
We are finding the FX Installation directory from the registry keys *)

function GetFXInstallDir(Param: String) : String;
var
	InstallDir : String;
begin
	RegQueryStringValue(HKLM,'Software\SV Systems\FileReplicationAgent','InstallDirectory',InstallDir);
	Result := InstallDir;
end;

(* GetVXInstallDir --- This is used to get the Installation directory for VX
We are finding the VX Installation directory from the registry keys *)

function GetVXInstallDir(Param: String) : String;
var
	InstallDir : String;
begin
	RegQueryStringValue(HKLM,'Software\SV Systems\VxAgent','InstallDirectory',InstallDir);
	Result := InstallDir;
end;

(* Getdirfx --- Used to find if FX is installed or not. Otherwise get the Installation dir by expanding {app}
1. If FX is already installed, call the function GetFXInstallDir and get the FX Installated directory
2. Else if VX is already installed, call the function GetVXInstallDir and get the VX Installed directory
3. Else which means that FX or VX is not installed in the setup. In this case expand the {app} variable and get the Installation path*)

function Getdirfx(Param: String) : String;
var dirpath : String;
begin
	if FileAgentConfiguration then
	begin
		dirpath := GetFXInstallDir('0') ;
		Result := dirpath;
	end
	else if VolumeAgentConfiguration then
	begin
		dirpath := GetVXInstallDir('0') ;
		Result := dirpath;
	end
	else
	begin
		dirpath :=  ExpandConstant('{app}');
		Result := dirpath;
	end;
end;

(* Getdirvx --- Used to find if VX is installed or not. Otherwise get the Installation dir by expanding {app}
1. If VX is already installed, call the function GetVXInstallDir and get the VX Installated directory
2. Else if FX is already installed, call the function GetFXInstallDir and get the FX Installed directory
3. Else which means that FX or VX is not installed in the setup. In this case expand the {app} variable and get the Installation path*)

function Getdirvx(Param: String) : String;
var dirpath : String;
begin
	if VolumeAgentConfiguration then
	begin
		dirpath := GetVXInstallDir('0') ;
		Result := dirpath;
	end
	else if FileAgentConfiguration then
	begin
		dirpath := GetFXInstallDir('0') ;
		Result := dirpath;
	end
	else
	begin
		dirpath :=  ExpandConstant('{app}');
		Result := dirpath;
	end;
end;

function Getdircx(Param: String) : String;
var dirpath : String;
begin
		RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\9','InstallDirectory',dirpath);
		Result := dirpath;
end;

function GetDefaultDirName(Param: String) : String;
var dirpath : String;
begin
	if (RegValueExists(HKLM,'Software\InMage Systems\Installed Products\5','InstallDirectory') and (IsServiceInstalled('svagents'))) then
	begin
		dirpath := GetVXInstallDir('0') ;
		Result := dirpath;
	end
	else if (RegValueExists(HKLM,'Software\InMage Systems\Installed Products\4','InstallDirectory') and (IsServiceInstalled('frsvc'))) then
	begin
		dirpath := GetFXInstallDir('0') ;
		Result := dirpath;
	end
	else
	begin
		dirpath :=  ExpandConstant('{pf}\{#INSTALLPATH}');
		Result := dirpath;
	end;
end;

function GetReadyMemoInstallDir(Param: String) : String;
var 
InstallDir : String;
begin
	if (RegValueExists(HKLM,'Software\InMage Systems\Installed Products\5','InstallDirectory')) then
	begin
		RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\5','InstallDirectory',InstallDir);
		Result := InstallDir;
	end
	else if (RegValueExists(HKLM,'Software\InMage Systems\Installed Products\4','InstallDirectory')) then
	begin
		RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\4','InstallDirectory',InstallDir);
		Result := InstallDir;
	end;
end;

(* Getip --- This is used to add the default ip address to the drscout.conf file
This will check if FX is installed or not. If FX is installed, it will take the FX IP from the registry and uses this for VX
If FX is not installed in the setup, it will take the default IP 0.0.0.0 *)

function Getip(Param: String) : String;
var vxip,fxip : String;
  begin
	RegQueryStringValue(HKLM,'Software\SV Systems','ServerName',fxip);
	if (IsServiceInstalled('frsvc')) or  (IsServiceInstalled('svagents')) then
	begin
		vxip := fxip ;
		Result := vxip;
	end
	else
	begin
		vxip := '0.0.0.0';
		Result := vxip;
	end;
end;

function Getport(Param: String) : String;
var fxport,vxport : Cardinal;
port : Integer;
  begin
	port := 80;
	RegQueryDWordValue(HKLM,'Software\SV Systems','ServerHttpPort',fxport);
	if (IsServiceInstalled('frsvc')) or  (IsServiceInstalled('svagents')) then
	begin
		vxport := fxport;
		Result := IntToStr(vxport);
	end
	else
	begin
		Result := IntToStr(port);
	end;
end;


(* hostconfigiconcommon --- Used for creating the icons in the icons section
1. This function is used to create the icon only for Fresh Installation of both the FX and VX
2. Upgradation of both the FX and VX *)

function hostconfigiconcommon : Boolean;
begin
	if  ((FreshInstallationFX = 'IDYES') and (FreshInstallationVX = 'IDYES')) then
	begin
		Result := True
	end
	else
		Result := False;
end;

(* StartCXServices --- This will check if CX is installed in the setup or not
	If CX is installed and the time shot service is in stopped stated, it will start
		all the CX related services such as IIS, Mysql*)

procedure StartCXServices;
var
	ResultCode: Integer;
begin
	if IsServiceInstalled('tmansvc') then
	begin
		Log('Starting the CX services.' + Chr(13))
		StartService('W3SVC');
		StartService('MySQL');
		Log('Invoking the start_services.bat script.' + Chr(13))
    Exec(ExpandConstant('{code:Getdircx}\bin\start_services.bat'), '', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
	end;
end;

function CustomStartService(ServiceName: String): Boolean;
var
  WaitLoopCount: Integer;
begin
      StartService(ServiceName);
    	WaitLoopCount := 1;
    	while (WaitLoopCount <= 9) and (ServiceStatusString(ServiceName)<>'SERVICE_RUNNING') do
    	begin
    		Sleep(20000);
    		WaitLoopCount := WaitLoopCount + 1;
    	end;
    	if ServiceStatusString(ServiceName)='SERVICE_RUNNING' then
    	Result := True
    	else
    	Result := False;
end;

procedure InitialSettings();
var
	j: Cardinal;

begin
	for j := 1 to ParamCount do
	if (CompareText(ParamStr(j),'/ip') = 0) or (CompareText(ParamStr(j),'-ip') = 0) and not (j = ParamCount) then
  begin
    CS_IP_Value := ParamStr(j+1)  
  end
  else if (CompareText(ParamStr(j),'/port') = 0) or (CompareText(ParamStr(j),'-port') = 0) and not (j = ParamCount) then
  begin
    CS_Port_Value := ParamStr(j+1)   
  end
  else if (CompareText(ParamStr(j),'/passphrasepath') = 0) or (CompareText(ParamStr(j),'-passphrasepath') = 0) and not (j = ParamCount) then
  begin
    PassphrasePath := ParamStr(j+1)   
  end
  else if (CompareText(ParamStr(j),'/CommunicationMode') = 0) or (CompareText(ParamStr(j),'-CommunicationMode') = 0) and not (j = ParamCount) then
  begin
    CommunicationMode := ParamStr(j+1)   
  end
	else if (CompareText(ParamStr(j),'/a') = 0) or (CompareText(ParamStr(j),'-a') = 0) and not (j = ParamCount) then
	begin
		a := ParamStr(j+1)
	end
	else if (CompareText(ParamStr(j),'/E') = 0) or (CompareText(ParamStr(j),'-E') = 0) and not (j = ParamCount) then
	begin
		E := ParamStr(j+1)
	end
	else if (CompareText(ParamStr(j),'/AW') = 0) or (CompareText(ParamStr(j),'-AW') = 0) and not (j = ParamCount) then
	begin
		//Msgbox('This is in Use command line settings upgrade',MbConfirmation, MB_OK);
		AW := ParamStr(j+1)
	end
	else if (CompareText(ParamStr(j),'/H') = 0) or (CompareText(ParamStr(j),'-H') = 0) and not (j = ParamCount) then
	begin
		H := ParamStr(j+1)
	end
	else if (CompareText(ParamStr(j),'/restart') = 0) or (CompareText(ParamStr(j),'-restart') = 0) and not (j = ParamCount) then
	begin
		restart := ParamStr(j+1)
	end
	else if (CompareText(ParamStr(j),'/role') = 0) or (CompareText(ParamStr(j),'-role') = 0) and not (j = ParamCount) then
	begin
		role := ParamStr(j+1)
	end
	else if (CompareText(ParamStr(j),'/Uninstall') = 0) or (CompareText(ParamStr(j),'-Uninstall') = 0) and not (j = ParamCount) then
	begin
		Uninstall := ParamStr(j+1)
	end
	else if (CompareText(ParamStr(j),'/UpdateConfig') = 0) or (CompareText(ParamStr(j),'-UpdateConfig') = 0) and not (j = ParamCount) then
	begin
		UpdateConfig := ParamStr(j+1)
	end
	else if (CompareText(ParamStr(j),'/vConInstall') = 0) or (CompareText(ParamStr(j),'-vConInstall') = 0) and not (j = ParamCount) then
	begin
		vConInstall := ParamStr(j+1)
	end;
end;

function RoleSelectedInSilentInstall : Boolean;
var
	j: Cardinal;
begin
	for j := 1 to ParamCount do
  begin
   	  if (CompareText(ParamStr(j),'/role') = 0) then
      begin
            Result := True;
            Break;
      end
      else
           Result := False;
 end;
end;

function GetValue(KeyName: AnsiString) : AnsiString ;
var
	Line, Value : AnsiString;
	iLineCounter, n : Integer;
	a_strTextfile : TArrayOfString;
	ConfFile : String;
	
begin
		ConfFile := GetVXInstallDir('0') + '\Application Data\etc\drscout.conf';
		LoadStringsFromFile(ConfFile, a_strTextfile);

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

function GetValuefromAmethyst(KeyName: AnsiString) : AnsiString ;
var
	Line, Value : AnsiString;
	iLineCounter, n : Integer;
	a_strTextfile : TArrayOfString;
begin
	if RegValueExists(HKLM,'Software\InMage Systems\Installed Products\9','Version') and IsServiceInstalled('tmansvc') then
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

function IsRoleSelected : Boolean;
begin
	if (FreshInstallation = 'IDYES') then
	begin
    	if WizardSilent then
    	begin
        		InitialSettings;
        		if RoleSelectedInSilentInstall then
        		begin
               		if (role = 'vconmt')  then
              		begin
              		      SetIniString( 'vxagent', 'Role','MasterTarget', ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') )
              		      Log('User has given the option ''/role''  in silent installation. Hence, running the ScoutTuning.exe from Run section.')
              		      Result := True;
              		end
              		else 
              		begin
              		      Log('User has passed wrong parameter for ''/role'' option in silent installation. Hence, not running the ScoutTuning.exe from Run section.')
              		      Result := False;
              		end;
            end
            else 
            begin
                  SetIniString( 'vxagent', 'Role','Agent', ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') )
                  Log('User has not passed ''/role'' option in silent installation. Hence, not running the ScoutTuning.exe from Run section.')
                  Result := False;
            end;       
    	end
    	else
    	begin
        		RoleType := GetIniString( 'vxagent', 'Role','', ExpandConstant('{tmp}\Role.conf') );
        		if RoleType = 'MasterTarget' then
        		begin   
        		      SetIniString( 'vxagent', 'Role','MasterTarget', ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') )
        		      Log('User has selected the role as ''Master Target'' in Role of the Agent page. Hence, running the ScoutTuning.exe from Run section.');
        		      Result := True;
        		end
        	  else if RoleType = 'Agent' then
        		begin
        		      SetIniString( 'vxagent', 'Role','Agent', ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') )
        		      Log('User has not selected the role as ''Master Target'' in Role of the Agent page. Hence, not running the ScoutTuning.exe from Run section.');
        		      Result := False;
        		end;
    	end;
	end
	else
	begin
	     if (GetValue('Role') = 'MasterTarget') then
	     begin
	         Result := True;
	     end
	     else
	     begin
	         Result := False;
	     end;
	end
end;

Function ExitInstallationWithStartServices(): Boolean; 
var 
    LogFileString : AnsiString;
begin
  Log('In the function Writing ExitInstallationWithStartServices.' + Chr(13));
 
    		if (IsServiceInstalled('svagents')) and (ServiceStatusString('svagents')='SERVICE_STOPPED') then
	      begin
	             if CustomStartService('svagents') then
        		       Log('svagents service is started succesfully in ExitInstallationWithStartServices function.' + Chr(13))
        		   else
        		       Log('svagents service is not started in ExitInstallationWithStartServices function.' + Chr(13));
	      end;
    		if (IsRoleSelected) then
            begin 
        		if (IsServiceInstalled('frsvc')) and (ServiceStatusString('frsvc')='SERVICE_STOPPED') then
    	      begin
    	             if CustomStartService('frsvc') then
            		       Log('frsvc service is started succesfully in ExitInstallationWithStartServices function.' + Chr(13))
            		   else
            		       Log('frsvc service is not started in ExitInstallationWithStartServices function.' + Chr(13));
    	      end;
	      end;
    		if (IsServiceInstalled('InMage Scout Application Service')) and (ServiceStatusString('InMage Scout Application Service')='SERVICE_STOPPED') then
		    begin
		            if CustomStartService('InMage Scout Application Service') then
        		       Log('InMage Scout Application service is started successfully in ExitInstallationWithStartServices function.' + Chr(13))
        		    else
        		       Log('InMage Scout Application service is not started in ExitInstallationWithStartServices function.' + Chr(13));
		    end;

      	if IsServiceInstalled('sshd') then
        begin
      		    
      		    if CustomStartService('sshd') then
      		       Log('sshd service is started succesfully in ExitInstallationWithStartServices function.' + Chr(13))
      		    else
      		       Log('sshd service is not started in ExitInstallationWithStartServices function.' + Chr(13));
        end;
 
        StartCXServices;
	//Creating the installation log in the system drive,this log gets appended every time the user runs the
	//setup.
	FileCopy(ExpandConstant ('{log}'), ExpandConstant ('{sd}\InstallLogFile.log'), FALSE);
  LoadStringFromFile(ExpandConstant ('{sd}\InstallLogFile.log'), LogFileString);    
  SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),'Installation Starts here '+ Installation_StartTime + #13#10 + '*********************************************', True);
  SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),#13#10 + LogFileString , True);
  SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'), #13#10 + '*********************************************' + #13#10 + 'Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
  DeleteFile(ExpandConstant('{sd}\InstallLogFile.log'));	
end;

(* CustomServiceStop --- Used for stopping the services
1. Check if the service is running or not
	--	If the service is running, stop the service
	--	If the service is in stopping state, display the message and quit
	--	If the service is already stopped, it will not do anything *)

function CustomServiceStop(ServiceName: String): Boolean;
var
  WaitLoopCount,ResultCode: Integer;
begin
	Result := True;
	if IsServiceRunning(ServiceName) then
	begin
		Log('The ' + ServiceName + ' Service will be stopped now. Please be patient as this may take a while.' + Chr(13));
		StopService(ServiceName);
		WaitLoopCount := 1;
		while (WaitLoopCount <= 18) and (ServiceStatusString(ServiceName)='SERVICE_STOP_PENDING') do
		begin
			Sleep(20000);
			WaitLoopCount := WaitLoopCount + 1;
		end;

		case ServiceName of
		'svagents':
			if ServiceStatusString(ServiceName) <> 'SERVICE_STOPPED' then
			begin
				SuppressibleMsgBox('Unable to stop Volume Replication Service within 6 minutes. Aborting the installation.',mbError, MB_OK, MB_OK);
				Log('Installation aborted with exit code 53.');
				ExitInstallationWithStartServices();
				ExitProcess(53);
			end;

		'InMage Scout Application Service':
			if ServiceStatusString(ServiceName) <> 'SERVICE_STOPPED' then
			begin
				SuppressibleMsgBox('Unable to stop Application Agent Service within 6 minutes. Aborting the installation.',mbError, MB_OK, MB_OK);
        Log('Installation aborted with exit code 54.');
				ExitInstallationWithStartServices();
				ExitProcess(54);
			end;

		'frsvc':
			if ServiceStatusString(ServiceName) <> 'SERVICE_STOPPED' then
			begin
				SuppressibleMsgBox('Unable to stop File Replication Service within 6 minutes. Aborting the installation.',mbError, MB_OK, MB_OK);
				Log('Installation aborted with exit code 52.');
				ExitInstallationWithStartServices();
				ExitProcess(52);
			end;
		
		'cxprocessserver':
		  if ServiceStatusString(ServiceName) <> 'SERVICE_STOPPED' then
			begin
				SuppressibleMsgBox('Unable to stop cxprocessserver Service within 6 minutes. Aborting the installation.',mbError, MB_OK, MB_OK);
				Log('Installation aborted with exit code 55.');
				ExitInstallationWithStartServices();
				ExitProcess(55);
			end;
		
		end;
	end;
end;

function UninstallStopServices(ServiceName: String): Boolean;
var
  WaitLoopCount,ResultCode: Integer;
begin
	Result := True;
	if IsServiceRunning(ServiceName) then
	begin
		Log('The ' + ServiceName + ' Service will be stopped now. Please be patient as this may take a while.' + Chr(13));
		StopService(ServiceName);
		WaitLoopCount := 1;
		while (WaitLoopCount <= 18) and (ServiceStatusString(ServiceName)='SERVICE_STOP_PENDING') do
		begin
			Sleep(20000);
			WaitLoopCount := WaitLoopCount + 1;
		end;

		case ServiceName of
		'svagents':
			if ServiceStatusString(ServiceName) <> 'SERVICE_STOPPED' then
			begin
    		SuppressibleMsgBox('Unable to stop Volume Replication Service within 6 minutes. Aborting the uninstallation.',mbError, MB_OK, MB_OK);
    		Log('Uninstallation aborted with exit code 53.');
				ExitInstallationWithStartServices();
				ExitProcess(53);
			end;

		'InMage Scout Application Service':
			if ServiceStatusString(ServiceName) <> 'SERVICE_STOPPED' then
			begin
        SuppressibleMsgBox('Unable to stop Application Agent Service within 6 minutes. Aborting the uninstallation.',mbError, MB_OK, MB_OK);
        Log('Uninstallation aborted with exit code 54.');
				ExitInstallationWithStartServices();
				ExitProcess(54);
			end;

		'frsvc':
			if ServiceStatusString(ServiceName) <> 'SERVICE_STOPPED' then
			begin
    		SuppressibleMsgBox('Unable to stop File Replication Service within 6 minutes. Aborting the uninstallation.',mbError, MB_OK, MB_OK);
    		Log('Uninstallation aborted with exit code 52.');
				ExitInstallationWithStartServices();
				ExitProcess(52);
			end;
			
		'cxprocessserver':
		  if ServiceStatusString(ServiceName) <> 'SERVICE_STOPPED' then
			begin
				SuppressibleMsgBox('Unable to stop cxprocessserver Service within 6 minutes. Aborting the uninstallation.',mbError, MB_OK, MB_OK);
				Log('Uninstallation aborted with exit code 55.');
				ExitInstallationWithStartServices();
				ExitProcess(55);
			end;
		
		end;
	end;
end;

function VerifyExistingVersion : Boolean;
var
	a_strTextfile : TArrayOfString;
	iLineCounter : Integer;
	ConfFile,FxVersionString : String;
begin
	ConfFile := GetVXInstallDir('0') + '\Application Data\etc\drscout.conf';
	RegQueryStringValue(HKLM,'Software\SV Systems\FileReplicationAgent','FxVersion',FxVersionString);
	LoadStringsFromFile(ConfFile, a_strTextfile);
	Result := False;
	for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
	begin
		if (Pos('Version=8.2', a_strTextfile[iLineCounter]) > 0) or (Pos('Version=8.3', a_strTextfile[iLineCounter]) > 0) then
		begin
			Log('Already VX - ' + a_strTextfile[iLineCounter] + ' is installed' + Chr(13));
			Result := True;
			Break;
		end;
	end;
	if (Pos('8.2',FxVersionString) > 0) or (Pos('8.3',FxVersionString) > 0) then
	begin
    	Log('Already FX - ' + FxVersionString + ' is installed' + Chr(13));
    	Result := True;
	end;
end;

function UnsupportVersion : Boolean;
var
	a_strTextfile : TArrayOfString;
	iLineCounter : Integer;
	ConfFile,VxVersionString,FxVersionString : String;
begin
	ConfFile := GetVXInstallDir('0') + '\Application Data\etc\drscout.conf';
	RegQueryStringValue(HKLM,'Software\SV Systems','VxVersion',VxVersionString);
	RegQueryStringValue(HKLM,'Software\SV Systems\FileReplicationAgent','FxVersion',FxVersionString);
	LoadStringsFromFile(ConfFile, a_strTextfile);
	Result := False;
	for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
	begin
		if ((Pos('Version=8.0', a_strTextfile[iLineCounter]) > 0) or (Pos('Version=8.0.1', a_strTextfile[iLineCounter]) > 0) or (Pos('Version=8.1', a_strTextfile[iLineCounter]) > 0)) then
		begin
			Log('Unsupported VX - ' + a_strTextfile[iLineCounter] + ' is installed' + Chr(13));
			Result := True;
			Break;
		end;
	end;
	if ((Pos('8.0',FxVersionString) > 0) or (Pos('8.0.1',FxVersionString) > 0) or (Pos('8.1',FxVersionString) > 0))then
	begin
		Log('Unsupported FX - ' + FxVersionString + ' is installed' + Chr(13));
		Result := True;
	end;
end;

(* Verify if Latest FX or VX is already installed in the setup. *)

function VerifyNewVersion : Boolean;
var
     FxVersionString,VxVersionString: String;
begin
	if RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\5','Version',VxVersionString) then
	begin
      	if (Pos(ExpandConstant('{#VERSION}'), VxVersionString)> 0) then
		    begin
	         		Log('The latest VX - ' + VxVersionString + ' version  is installed' + Chr(13));
			        Result := True;
        end;
  end
  else if RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\4','Version',FxVersionString) then
	begin
      	if (Pos(ExpandConstant('{#VERSION}'), FxVersionString)> 0) then
		    begin
	         		Log('The latest FX - ' + FxVersionString + ' version is installed' + Chr(13));
			        Result := True;
        end;
  end
  else
      Result := False;
end;

Function CheckHigherVersion(): Boolean;
var
      ExistingVersion,CurrentVersion : String;
begin
			CurrentVersion := '{#VERSION}';
     	if RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\4','Version',ExistingVersion) then
     	begin             	
            StringChangeEx(CurrentVersion,'.','',FALSE);
            StringChangeEx(ExistingVersion,'.','',FALSE);
            if ((StrToInt(CurrentVersion)) < (StrToInt(ExistingVersion))) then
                  Result := True
            else
                  Result := False;
      end
      else if RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\5','Version',ExistingVersion) then
      begin             	
            StringChangeEx(CurrentVersion,'.','',FALSE);
            StringChangeEx(ExistingVersion,'.','',FALSE);
            if ((StrToInt(CurrentVersion)) < (StrToInt(ExistingVersion))) then
                  Result := True
            else
                  Result := False;
      end
      else
            Result := False;
end;

procedure MyAfterInstall(Root,Subkey,ValueName : String);
begin
      if (RegValueExists (HKLM, Subkey, ValueName)) then
          Log('In function MyAfterInstall - values found for  '+ Subkey + '\' + ValueName + Chr(13))
      else
          Log('In function MyAfterInstall - values are not found for  '+ Subkey + '\' + ValueName + Chr(13));
end;

Procedure DefaultHelpForm();
var
  Form: TSetupForm;
  OKButton: TNewButton;
  mLabel:TLabel;
begin
   Form := CreateCustomForm();
  try
    Form.ClientWidth := ScaleX(590);
    Form.ClientHeight := ScaleY(490);
    Form.Caption := 'Microsoft Azure Site Recovery Mobility Service/Master Target Server HELP';
    Form.Center
    mLabel:=TLabel.Create(Form);
    mLabel.Caption:='COMMAND LINE PARAMETERS';
    mLabel.AutoSize:=True;
    mLabel.Alignment:=taCenter;
  
    OKButton := TNewButton.Create(Form);
    OKButton.Parent := Form;
    OKButton.Width := ScaleX(80);
    OKButton.Height := ScaleY(30);
    OKButton.Left := ScaleX(250)
    OKButton.Top := ScaleY(430)
    OKButton.Caption := 'OK';
    OKButton.ModalResult := mrOk;

    RichEditViewer :=TRichEditViewer.Create(Form);
    RichEditViewer.Width :=550;
    RichEditViewer.Height :=450;
    RichEditViewer.Top := 20;
    RichEditViewer.Left :=20;
    RichEditViewer.Alignment:=taLeftJustify;
    RichEditViewer.Parent := Form;
    RichEditViewer.WordWrap :=True;
    RichEditViewer.ScrollBars := ssBoth;
    RichEditViewer.UseRichEdit := True;
    RichEditViewer.Font.Size:=9;
    RichEditViewer.RTFText := 'Command Line Parameters'+#13#10+#13#10+
    '/? : Opens Help'+#13#10+
    '/help : Opens Help'+#13#10+
    '/verysilent : In very silent installation mode the installation progress window will not be displayed.'+#13#10+
    '/suppressmsgboxes : Suppressess the message boxes(Effective only with /verysilent switch). '+#13#10+
    '/dir= : Absolute installaton path(Effective only with /verysilent switch).'+#13#10+
    '/restart : Restarts the System(Effective only with /verysilent switch).'+#13#10+ 
    '/ip : CS IP address' +#13#10+ '/port : CS Port' +#13#10+ 
    '/PassphrasePath <Passphrase file path> : CS Passphrase file path.'+#13#10+
    '/CommunicationMode(Http, Https) : Communication Mode' +#13#10+
    '/nathostname : NAT Hostname' +#13#10+ '/natip : NAT IP address' +#13#10+ 
    '/user : Run frsvc as this user' +#13#10+ 
    '/password : Authenticate user with this password' +#13#10+
    '/cachedir : Where to store the catched files' +#13#10+ 
    '/usesysvolumes : Agent should register system & cache volumes as available' +#13#10+ 
    '/noremotelogging : Agent should not send error logs to the CX' +#13#10+ 
    '/loglevel : 0-7 Log nothing (0) or everything (7)' +#13#10+ 
    ' /AW : ApplicationWizard(Y/y)' +#13#10+ '/a upgrade(u)' +#13#10+ 
    '/setappagent : Sets the Appservice startup type to manual(manual)' +#13#10+ 
    '/role : vconmt' +#13#10;
    
    RichEditViewer.ReadOnly := True;
    Form.ActiveControl := OKButton;
    cancelclick:=True;
    if Form.ShowModal() = mrOk then
    begin
      Log('Default Help page is displayed succesfully');
      Abort;
    end;
  finally
    Form.Free();
  end;
end;

Procedure ExistingVersionHelpForm();
var
  Form: TSetupForm;
  OKButton: TNewButton;
  mLabel:TLabel;
begin
   Form := CreateCustomForm();
  try
    Form.ClientWidth := ScaleX(560);
    Form.ClientHeight := ScaleY(460);
    Form.Caption := 'Microsoft Azure Site Recovery Mobility Service/Master Target Server HELP';
    Form.Center
    mLabel:=TLabel.Create(Form);
    mLabel.Caption:='COMMAND LINE PARAMETERS';
    mLabel.AutoSize:=True;
    mLabel.Alignment:=taCenter;
  
    OKButton := TNewButton.Create(Form);
    OKButton.Parent := Form;
    OKButton.Width := ScaleX(80);
    OKButton.Height := ScaleY(30);
    OKButton.Left := ScaleX(220)
    OKButton.Top := ScaleY(415)
    OKButton.Caption := 'OK';
    OKButton.ModalResult := mrOk;

    RichEditViewer :=TRichEditViewer.Create(Form);
    RichEditViewer.Width :=550;
    RichEditViewer.Height :=430;
    RichEditViewer.Top := 20;
    RichEditViewer.Left :=20;
    RichEditViewer.Alignment:=taLeftJustify;
    RichEditViewer.Parent := Form;
    RichEditViewer.WordWrap :=True;
    RichEditViewer.ScrollBars := ssBoth;
    RichEditViewer.UseRichEdit := True;
    RichEditViewer.Font.Size:=9;
    RichEditViewer.RTFText := 'Command Line Parameters'+#13#10+#13#10+
    '/? : Opens Help'+#13#10+
    '/help : Opens Help'+#13#10+
    '/verysilent : In very silent installation mode the installation progress window will not be displayed.'+#13#10+
    '/suppressmsgboxes : Suppressess the message boxes(Effective only with /verysilent switch). '+#13#10+
    '/dir= : Absolute installaton path(Effective only with /verysilent switch).'+#13#10+
    '/restart : Restarts the System(Effective only with /verysilent switch).'+#13#10+
    '/ip : CS IP address' +#13#10+ '/port : CS Port' +#13#10+ 
    '/CommunicationMode(Http, Https) : Communication Mode' +#13#10+
    '/nathostname : NAT Hostname' +#13#10+ '/natip : NAT IP address' +#13#10+ 
    '/user : Run frsvc as this user' +#13#10+ 
    '/password : Authenticate user with this password' +#13#10+
    '/cachedir : Where to store the catched files' +#13#10+ 
    '/usesysvolumes : Agent should register system & cache volumes as available' +#13#10+ 
    '/noremotelogging : Agent should not send error logs to the CX' +#13#10+ 
    '/loglevel : 0-7 Log nothing (0) or everything (7)' +#13#10+ 
    ' /AW : ApplicationWizard(Y/y)' +#13#10+ '/a upgrade(u)' +#13#10+ 
    '/setappagent : Sets the Appservice startup type to manual(manual)' +#13#10+ 
    '/role : vconmt' +#13#10+ '/Uninstall : Uninstallation of UA (Y/y)'  +#13#10+ 
    ' /UpdateConfig : Change CX IP and Port (Y/y)'+#13#10;
    
    RichEditViewer.ReadOnly := True;
    Form.ActiveControl := OKButton;
    cancelclick:=True;
    if Form.ShowModal() = mrOk then
    begin
      Log('ExistingVersion Help page is displayed succesfully');
      Abort;
    end;
  finally
    Form.Free();
  end;
end;

Procedure UpgradeHelpForm();
var
  Form: TSetupForm;
  OKButton: TNewButton;
  mLabel:TLabel;
begin
   Form := CreateCustomForm();
  try
    Form.ClientWidth := ScaleX(590);
    Form.ClientHeight := ScaleY(250);
    Form.Caption := 'Microsoft Azure Site Recovery Mobility Service/Master Target Server HELP';
    Form.Center
    mLabel:=TLabel.Create(Form);
    mLabel.Caption:='COMMAND LINE PARAMETERS';
    mLabel.AutoSize:=True;
    mLabel.Alignment:=taCenter;
   
    OKButton := TNewButton.Create(Form);
    OKButton.Parent := Form;
    OKButton.Width := ScaleX(80);
    OKButton.Height := ScaleY(30);
    OKButton.Left := ScaleX(230)
    OKButton.Top := ScaleY(215)
    OKButton.Caption := 'OK';
    OKButton.ModalResult := mrOk;

    RichEditViewer :=TRichEditViewer.Create(Form);
    RichEditViewer.Width :=550;
    RichEditViewer.Height :=190;
    RichEditViewer.Top := 20;
    RichEditViewer.Left :=20;
    RichEditViewer.Alignment:=taLeftJustify;
    RichEditViewer.Parent := Form;
    RichEditViewer.WordWrap :=True;
    RichEditViewer.ScrollBars := ssBoth;
    RichEditViewer.UseRichEdit := True;
    RichEditViewer.Font.Size:=9;
    RichEditViewer.RTFText := 'Command Line Parameters'+#13#10+#13#10+
    '/? : Opens Help'+#13#10+
    '/help : Opens Help'+#13#10+
    '/verysilent : In very silent installation mode the installation progress window will not be displayed.'+#13#10+
    '/suppressmsgboxes : Suppressess the message boxes(Effective only with /verysilent switch). '+#13#10+
    '[ /a <upgrade(u)>]' #13 '[ (/restart Y)/(/norestart) ]'+#13#10;
    
    RichEditViewer.ReadOnly := True;
    Form.ActiveControl := OKButton;
    cancelclick:=True;
    if Form.ShowModal() = mrOk then
    begin
      Log('Upgrade Help page is displayed succesfully');
      Abort;
    end;
  finally
    Form.Free();
  end;
end;

Procedure UninstallHelpForm();
var
  Form: TSetupForm;
  OKButton: TNewButton;
  mLabel:TLabel;
begin
   Form := CreateCustomForm();
  try
    Form.ClientWidth := ScaleX(610);
    Form.ClientHeight := ScaleY(190);
    Form.Caption := 'Microsoft Azure Site Recovery Mobility Service/Master Target Server Uninstall HELP';
    Form.Center
    mLabel:=TLabel.Create(Form);
    mLabel.Caption:='COMMAND LINE PARAMETERS';
    mLabel.AutoSize:=True;
    mLabel.Alignment:=taCenter;
  
    OKButton := TNewButton.Create(Form);
    OKButton.Parent := Form;
    OKButton.Width := ScaleX(80);
    OKButton.Height := ScaleY(30);
    OKButton.Left := ScaleX(230)
    OKButton.Top := ScaleY(150)
    OKButton.Caption := 'OK';
    OKButton.ModalResult := mrOk;
   
    RichEditViewer :=TRichEditViewer.Create(Form);
    RichEditViewer.Width :=570;
    RichEditViewer.Height :=120;
    RichEditViewer.Top := 20;
    RichEditViewer.Left :=20;
    RichEditViewer.Alignment:=taLeftJustify;
    RichEditViewer.Parent := Form;
    RichEditViewer.WordWrap :=True;
    RichEditViewer.ScrollBars := ssBoth;
    RichEditViewer.UseRichEdit := True;
    RichEditViewer.Font.Size:=9;
    RichEditViewer.RTFText := 'Command Line Parameters'+#13#10+#13#10+
    '/? : Opens Help'+#13#10+
    '/help : Opens Help'+#13#10+
    '/verysilent : In very silent Uninstallation mode the Uninstallation progress window will not be displayed.'+#13#10+
    '/suppressmsgboxes : Suppressess the message boxes(Effective only with /verysilent switch). '+#13#10+
    '/restart : Restarts the System(Effective only with /verysilent switch).'+#13#10;
    RichEditViewer.ReadOnly := True;
    Form.ActiveControl := OKButton;
    cancelclick:=True;
    if Form.ShowModal() = mrOk then
    begin
      Log('Uninstall Help page is displayed succesfully');
      Abort;
    end;
  finally
    Form.Free();
  end;
end;

(* DisplayHelpPage --- This is used to parse the silent install options/command line parameters:
  If the user gives "/help" or "/HELP" Option then it returns true else it Aborts the installation.*)
  
procedure DisplayHelpPage();
var
	j: Cardinal;
begin
	for j := 1 to ParamCount do
  if ((CompareText(ParamStr(j),'/HELP') = 0) or (CompareText(ParamStr(j),'/?') = 0)) then
  begin
  		if (VerifyExistingVersion) then
  		    UpgradeHelpForm
  		else if (VerifyNewVersion) then
  		    ExistingVersionHelpForm
  		else
 		  		DefaultHelpForm;
  		Abort;
  end;  		
end;



(* DisplayUninstallHelp --- This is used to parse the silent install options/command line parameters:
  If the user gives "/help" or "/HELP" Option then it returns true else it Aborts the installation.*)
  
procedure DisplayUninstallHelp();
var
	j: Cardinal;
begin
	for j := 1 to ParamCount do
  if ((CompareText(ParamStr(j),'/HELP') = 0) or (CompareText(ParamStr(j),'/?') = 0)) then
  begin
		  		UninstallHelpForm;
      		Abort;
  end;  		
end;

function NoReinstallCheck : Boolean;
begin
	if ReinstallSelected = 'IDYES' then
		Result := False
	else
		Result := True;
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

procedure UseCommandLineSettings();
var
	i: Cardinal;
	user, password: String;
begin
  for i := 1 to ParamCount do
    if (CompareText(ParamStr(i),'/ip') = 0) or (CompareText(ParamStr(i),'-ip') = 0) and not (i = ParamCount) then
    begin
      if SetIniString( 'vxagent.transport', 'Hostname', ParamStr(i+1), ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
          Log('The ip address -> '+ ParamStr(i+1) + ' is successfully updated in the drscout.conf file.');
      if RegWriteStringValue( HKLM, 'Software\SV Systems', 'ServerName', ParamStr(i+1) ) then
          Log('The ip address -> '+ ParamStr(i+1) + ' is successfully updated in ServerName registry entry under ''HKLM\Software\SV Systems'' registry key.');
      InitialSettings;
      if (role = 'vconmt')  then
      begin
      if SetIniString( 'cxps', 'cs_ip_address', ParamStr(i+1), ExpandConstant('{code:Getdirvx}\transport\cxps.conf') ) then
          Log('The cs_ip_address -> '+ ParamStr(i+1) + ' is successfully updated in the cxps.conf file.');
      end;
    end
    else if (CompareText(ParamStr(i),'/port') = 0) or (CompareText(ParamStr(i),'-port') = 0) and not (i = ParamCount) then
    begin
      if  SetIniString( 'vxagent.transport', 'Port', ParamStr(i+1), ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
          Log('The Port value -> '+ ParamStr(i+1) + ' is successfully updated in the drscout.conf file.');
      if  RegWriteDWordValue( HKLM, 'Software\SV Systems', 'ServerHttpPort', StrToIntDef(ParamStr(i+1),80)) then
          Log('The Port value -> '+ ParamStr(i+1) + ' is successfully updated in ServerHttpPort registry entry under ''HKLM\Software\SV Systems'' registry key.')
    end
    else if (CompareText(ParamStr(i),'/nathostname')=0) or (CompareText(ParamStr(i),'-nathostname')=0) and not (i = ParamCount) then
    begin
        if SetIniString( 'vxagent', 'ConfiguredHostname', ParamStr(i+1), ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
           Log('The nathostname value -> '+ ParamStr(i+1) + ' is successfully updated in the drscout.conf file as ConfiguredHostname.');
        if SetIniString( 'vxagent', 'UseConfiguredHostname', '1', ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
           Log('The UseConfiguredHostname value -> 1 is successfully updated in the drscout.conf file.');
        if RegWriteStringValue( HKLM, 'Software\SV Systems\FileReplicationAgent', 'ConfiguredHostname', ParamStr(i+1) ) then
           Log('The nathostname value -> '+ ParamStr(i+1) + ' is successfully updated in ConfiguredHostname registry entry under ''HKLM\Software\SV Systems\FileReplicationAgent'' registry key.');
        if RegWriteDWordValue(  HKLM, 'Software\SV Systems\FileReplicationAgent', 'UseConfiguredHostname', 1) then
           Log('The registry entry UseConfiguredHostname under ''HKLM\Software\SV Systems\FileReplicationAgent'' registry key is updated with the value 1.') 
    end
    else if (CompareText(ParamStr(i), '/natip')=0) or (CompareText(ParamStr(i),'-natip')=0) and not (i = ParamCount) then
    begin
        if SetIniString( 'vxagent', 'ConfiguredIpAddress', ParamStr(i+1), ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
           Log('The natip value -> '+ ParamStr(i+1) + ' is successfully updated in the drscout.conf file as ConfiguredIpAddress.'); 
        if SetIniString( 'vxagent', 'UseConfiguredIpAddress', '1', ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
           Log('The UseConfiguredIpAddress value -> 1 is successfully updated in the drscout.conf file.');          
        if RegWriteStringValue( HKLM, 'Software\SV Systems\FileReplicationAgent', 'ConfiguredIP', ParamStr(i+1) ) then
           Log('The natip value -> '+ ParamStr(i+1) + ' is successfully updated in ConfiguredIP registry entry under ''HKLM\Software\SV Systems\FileReplicationAgent'' registry key.');
        if RegWriteDWordValue(  HKLM, 'Software\SV Systems\FileReplicationAgent', 'UseConfiguredIP', 1) then
           Log('The registry entry UseConfiguredIP under ''HKLM\Software\SV Systems\FileReplicationAgent'' registry key is updated with the value 1.') 
    end
    else if (CompareText(ParamStr(i),'/cachedir')=0) or (CompareText(ParamStr(i),'-cachedir')=0) and not (i = ParamCount) then
    begin
          if SetIniString( 'vxagent', 'CacheDirectory', ParamStr(i+1), ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
             Log('The cachedir value -> '+ ParamStr(i+1) + ' is successfully updated in the drscout.conf file as CacheDirectory.');
          if SetIniString( 'vxagent', 'DiffTargetCacheDirectoryPrefix', ParamStr(i+1), ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
             Log('The cachedir value -> '+ ParamStr(i+1) + ' is successfully updated in the drscout.conf file as DiffTargetCacheDirectoryPrefix.');
          if RegWriteStringValue( HKLM, 'Software\SV Systems\FileReplicationAgent', 'CacheDirectory', ParamStr(i+1) ) then
             Log('The cachedir value -> '+ ParamStr(i+1) + ' is successfully updated in CacheDirectory registry entry under ''Software\SV Systems\FileReplicationAgent'' registry key.')
    end          
    else if (CompareText(ParamStr(i),'/usesysvolumes')=0) or (CompareText(ParamStr(i),'-usesysvolumes')=0) then
    begin
          if  SetIniString( 'vxagent', 'RegisterSystemDrive', '1', ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
              Log('The RegisterSystemDrive value is successfully updated in the drscout.conf file as 1, as the user passed ''/usesysvolumes'' parameter.')             
    end              
    else if (CompareText(ParamStr(i),'/noremotelogging')=0) or (CompareText(ParamStr(i),'-noremotelogging')=0) then
    begin
          if SetIniString( 'vxagent', 'RemoteLog', '0', ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
              Log('As user passed ''/noremotelogging'', updating the RemoteLog value to 0 in the drscout.conf file')
    end
    else if (CompareText(ParamStr(i),'/loglevel')=0) or (CompareText(ParamStr(i),'-loglevel')=0) and not (i = ParamCount) then
    begin
          if SetIniString( 'vxagent', 'LogLevel', ParamStr(i+1), ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
             Log('As user passed ''/loglevel'', updating the LogLevel value to '+ ParamStr(i+1) + ' in the drscout.conf file');
          if RegWriteDWordValue(  HKLM, 'Software\SV Systems', 'DebugLogLevel', StrtoInt(ParamStr(i+1))) then
           Log('The registry entry DebugLogLevel under ''HKLM\Software\SV Systems'' registry key is updated with the value '+ ParamStr(i+1) + '.'); 
    end
    else if (CompareText(ParamStr(i),'/user') = 0) or (CompareText(ParamStr(i),'-user') = 0) and not (i = ParamCount) then
		begin
		      Log('Assingning the value '+ ParamStr(i+1) + ' as the username to start the frsvc service.')
		      user := ParamStr(i+1)
		end		      
    else if (CompareText(ParamStr(i),'/password') = 0) or (CompareText(ParamStr(i),'-password') = 0) and not (i = ParamCount) then
		begin
		      Log('Assingning the value '+ ParamStr(i+1) + ' as the password to start the frsvc service.')
  		    password := ParamStr(i+1)
		end
		else if (CompareText(ParamStr(i),'/CommunicationMode')=0) or (CompareText(ParamStr(i),'-CommunicationMode')=0) and not (i = ParamCount) then
    begin
        Log('Assingning the value '+ ParamStr(i+1) + ' as the CommunicationMode.')
        CommunicationMode := ParamStr(i+1);
        if (CommunicationMode = 'Http') or (CommunicationMode = 'http') then
        begin
            if SetIniString( 'vxagent.transport', 'Https', '0', ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
               Log('The Https value -> ''0'' is successfully updated in the drscout.conf file.');
            
            if RegWriteDWordValue( HKLM, 'Software\SV Systems', 'Https', 0) then
               Log('The Https value -> ''0'' is successfully updated in Https registry entry under ''HKLM\Software\SV Systems'' registry key.'); 
        end
        else if (CommunicationMode = 'Https') or (CommunicationMode = 'https')then
        begin
            if SetIniString( 'vxagent.transport', 'Https', '1', ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
               Log('The Https value -> ''1'' is successfully updated in the drscout.conf file.');
            
            if RegWriteDWordValue( HKLM, 'Software\SV Systems', 'Https', 1) then
               Log('The Https value -> ''1'' is successfully updated in Https registry entry under ''HKLM\Software\SV Systems'' registry key.'); 
        end;
    end;
	  
    if not ((user = '') and (password = '')) then
		begin
		      if ChangeServiceUser( 'frsvc', user, password ) then
		         Log('Successfully changed the username to '+ user + ' and password to '+ password+' to start the FX service.')
    end;
    InitialSettings;
    if (role = 'vconmt')  then
    begin
    InitialSettings;
    if (CommunicationMode = 'Http') or (CommunicationMode = 'http') then
    begin 
        if SetIniString( 'cxps', 'cs_port', CS_Port_Value, ExpandConstant('{code:Getdirvx}\transport\cxps.conf') ) then
          Log('The cs_port -> '+ CS_Port_Value + ' is successfully updated in the cxps.conf file.');
        if SetIniString( 'cxps', 'cs_use_secure', 'no', ExpandConstant('{code:Getdirvx}\transport\cxps.conf') ) then
          Log('The cs_use_secure -> no is successfully updated in the cxps.conf file.');
    end;
    if (CommunicationMode = 'Https') or (CommunicationMode = 'https') or not IsSilentOptionSelected('CommunicationMode') then
    begin 
        if SetIniString( 'cxps', 'cs_ssl_port', CS_Port_Value, ExpandConstant('{code:Getdirvx}\transport\cxps.conf') ) then
          Log('The cs_ssl_port -> '+ CS_Port_Value + ' is successfully updated in the cxps.conf file.');
        if SetIniString( 'cxps', 'cs_use_secure', 'yes', ExpandConstant('{code:Getdirvx}\transport\cxps.conf') ) then
          Log('The cs_use_secure -> yes is successfully updated in the cxps.conf file.');
    end;
    end;		      
end;


//This procedure is used to validate the "setappagent" parameter and its argument.
//This parameter is used to set the appagent service to manual.
procedure Check_SetAppAgent_Command();
var
	i: Cardinal;
begin
    for i := 1 to ParamCount do
    begin
          if (CompareText(ParamStr(i),'/setappagent') = 0) or (CompareText(ParamStr(i),'-setappagent') = 0) and not (i = ParamCount) then
      	  begin
      		      Log('Assingning the value '+ ParamStr(i+1) + ' to the setappagent parameter to start the appservice.')
      		      setappagent := ParamStr(i+1);
      		      Break;
      	  end;
    end;		      
end;

// Procedure to invoke cdpcli --registermt command.
procedure Cdpcli_RegisterMT();
var
CdpcliPath : String;
ResultCode : Integer;
begin
    Log(#13#10 + 'Invoking cdpcli with --registermt as an argument. ' + #13#10 +  ExpandConstant('{code:Getdirvx}\cdpcli.exe') + ' --registermt' + #13#10);
    Exec(ExpandConstant('{code:Getdirvx}\cdpcli.exe'),'--registermt','',SW_HIDE,ewWaitUntilTerminated, ResultCode);
    if (ResultCode=0) then
    begin
        Log('cdpcli --registermt command executed successfully.');
        if RegWriteStringValue(HKLM,'Software\InMage Systems\Installed Products\9','Cdpcli_RegisterMT_Status','Succeeded') then
            Log('''Cdpcli_RegisterMT_Status'' success entry is updated in ''HKLM\Software\InMage Systems\Installed Products\9'' registry')
        else
            Log('Unable to update ''Cdpcli_RegisterMT_Status'' success entry in ''HKLM\Software\InMage Systems\Installed Products\9'' registry');
    end
    else
    begin
        Log('Failed to execute cdpcli --registermt command.');
        if RegWriteStringValue(HKLM,'Software\InMage Systems\Installed Products\9','Cdpcli_RegisterMT_Status','Failed') then
            Log('''Cdpcli_RegisterMT_Status'' failure entry is updated in ''HKLM\Software\InMage Systems\Installed Products\9'' registry')
        else
            Log('Unable to update ''Cdpcli_RegisterMT_Status'' failure entry in ''HKLM\Software\InMage Systems\Installed Products\9'' registry');
    end;
end;


function CustomStopService(ServiceName: String): Boolean;
var
  WaitLoopCount: Integer;
begin
      StopService(ServiceName);
    	WaitLoopCount := 1;
    	while (WaitLoopCount <= 9) and (ServiceStatusString(ServiceName)<>'SERVICE_STOPPED') do
    	begin
    		Sleep(20000);
    		WaitLoopCount := WaitLoopCount + 1;
    	end;
    	if ServiceStatusString(ServiceName)='SERVICE_STOPPED' then
    	Result := True
    	else
    	Result := False;
end;

Function ExitInstallationWithoutStartServices(): Boolean; 
var 
    LogFileString : AnsiString;
begin
  Log('In the function Writing ExitInstallationWithoutStartServices.' + Chr(13));
	//Creating the installation log in the system drive,this log gets appended every time the user runs the
	//setup.
	FileCopy(ExpandConstant ('{log}'), ExpandConstant ('{sd}\InstallLogFile.log'), FALSE);
  LoadStringFromFile(ExpandConstant ('{sd}\InstallLogFile.log'), LogFileString);    
  SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),'Installation Starts here '+ Installation_StartTime + #13#10 + '*********************************************', True);
  SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),#13#10 + LogFileString , True);
  SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'), #13#10 + '*********************************************' + #13#10 + 'Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
  DeleteFile(ExpandConstant('{sd}\InstallLogFile.log'));	  
end;

Function ExitFromUninstallProcess(): Boolean;
begin
    	SaveStringToFile(mcLogFile, 'In the function Writing ExitFromUninstallProcess.' + Chr(13), True);
    	SaveStringToFile(mcLogFile, Chr(13), True);
    	if (IsServiceInstalled('svagents')) and (ServiceStatusString('svagents')='SERVICE_STOPPED') then
    		StartService('svagents');
    
    	if (IsServiceInstalled('InMage Scout Application Service')) and (ServiceStatusString('InMage Scout Application Service')='SERVICE_STOPPED') then
    		StartService('InMage Scout Application Service');
      if (GetValue('Role') = 'MasterTarget') then
      begin    
        	if (IsServiceInstalled('frsvc')) and (ServiceStatusString('frsvc')='SERVICE_STOPPED') then
        		StartService('frsvc');
      end;
    		
      SaveStringToFile(mcLogFile, #13#10 + '*********************************************' + #13#10 + 'Un-Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
    	SaveStringToFile(mcLogFile, Chr(13), True);
end;

Function RollBackProgress() : Boolean;
Begin
Result := True;
    ProgressPage.show();
    ProgressPage.setProgress(ProgressPage.ProgressBar.Position+100, ProgressPage.ProgressBar.Max);
    ProgressPage.SetText('Rolling back the changes..', '');
    Wait := 20;
    MaxProgress := ProgressPage.ProgressBar.Position - Wait;    
    while (Wait > 8)  do
    begin
         ProgressPage.setProgress(ProgressPage.ProgressBar.Position-5, ProgressPage.ProgressBar.Max);
         Sleep(1000);
         Wait := Wait - 1;
    end;  
end;

Function RollBackProgressEnd() : Boolean;
Begin
Result := True;
    ProgressPageEnd.show();
    ProgressPageEnd.setProgress(ProgressPageEnd.ProgressBar.Position+40, ProgressPageEnd.ProgressBar.Max);
    ProgressPageEnd.SetText('Rolling back the changes..', '');
    Sleep(15000);
    Wait := 20;
    MaxProgress := ProgressPageEnd.ProgressBar.Position - Wait;
    while (Wait > 2)  do
    begin
         ProgressPageEnd.setProgress(ProgressPageEnd.ProgressBar.Position-2, ProgressPageEnd.ProgressBar.Max);
         Sleep(1000);
         Wait := Wait - 1;
    end; 
end;

//The below procedure is used to roll back the binaries/dll's/drivers when upgrade is failed.
procedure RollBackBinaries();
var
    iLineCounter : Integer; 
    a_strTextfile : TArrayOfString; 
    OldState: Boolean;
begin      
    RollBackProgress;
           
     LoadStringsFromFile(ExpandConstant ('{code:Getdirvx}\Backup_Release\Backup_BinaryFile.log'), a_strTextfile);
     for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
     begin
         if FileExists(ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter])) then
         begin
             if FileCopy(ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter]),ExpandConstant('{code:Getdirvx}\'+a_strTextfile[iLineCounter]),False) then
             Log('Successfully moved the binary from '+ ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter])+ ' to ' + ExpandConstant('{code:Getdirvx}\'+a_strTextfile[iLineCounter])+' during roll back.')
             else
             Log('Failed to move the binary from '+ ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter])+ ' to ' + ExpandConstant('{code:Getdirvx}\'+a_strTextfile[iLineCounter])+' during roll back.');
         end;
     end; 
     if FileExists(ExpandConstant('{code:Getdirvx}\Backup_Release\Backup_BinaryFile.log')) then
     DeleteFile(ExpandConstant('{code:Getdirvx}\Backup_Release\Backup_BinaryFile.log'));
     
     LoadStringsFromFile(ExpandConstant ('{code:Getdirvx}\Backup_Release\Backup_BinaryFile_consistency.log'), a_strTextfile);
     for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
     begin
         if FileExists(ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter])) then
         begin
             if FileCopy(ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter]),ExpandConstant('{code:Getdirvx}\consistency\'+a_strTextfile[iLineCounter]),False) then
             Log('Successfully moved the binary from '+ ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter])+ ' to ' + ExpandConstant('{code:Getdirvx}\consistency\'+a_strTextfile[iLineCounter])+' during roll back.')
             else
             Log('Failed to move the binary from '+ ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter])+ ' to ' + ExpandConstant('{code:Getdirvx}\consistency\'+a_strTextfile[iLineCounter])+' during roll back.');
         end;
     end; 
     
     LoadStringsFromFile(ExpandConstant ('{code:Getdirvx}\Backup_Release\FileRep\Backup_BinaryFile_FileRep.log'), a_strTextfile);
     for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
     begin
         if FileExists(ExpandConstant('{code:Getdirvx}\Backup_Release\FileRep\'+a_strTextfile[iLineCounter])) then
         begin
             if FileCopy(ExpandConstant('{code:Getdirvx}\Backup_Release\FileRep\'+a_strTextfile[iLineCounter]),ExpandConstant('{code:Getdirvx}\FileRep\'+a_strTextfile[iLineCounter]),False) then
             Log('Successfully moved the binary from '+ ExpandConstant('{code:Getdirvx}\Backup_Release\FileRep\'+a_strTextfile[iLineCounter])+ ' to ' + ExpandConstant('{code:Getdirvx}\FileRep\'+a_strTextfile[iLineCounter])+' during roll back.')
             else
             Log('Failed to move the binary from '+ ExpandConstant('{code:Getdirvx}\Backup_Release\FileRep\'+a_strTextfile[iLineCounter])+ ' to ' + ExpandConstant('{code:Getdirvx}\FileRep\'+a_strTextfile[iLineCounter])+' during roll back.');
         end;
     end; 
     if FileExists(ExpandConstant('{code:Getdirvx}\Backup_Release\FileRep\Backup_BinaryFile_FileRep.log')) then
     DeleteFile(ExpandConstant('{code:Getdirvx}\Backup_Release\FileRep\Backup_BinaryFile_FileRep.log'));
     
     if  (ProcessorArchitecture = paIA64) or (ProcessorArchitecture = paX64)then
      begin
          OldState := EnableFsRedirection(False);
          try
              LoadStringsFromFile(ExpandConstant ('{code:Getdirvx}\Backup_Release\Backup_DriverFile.log'), a_strTextfile);
        			for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
        			begin
        				if FileExists(ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter])) then
        				begin
        					if FileCopy(ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter]), ExpandConstant('{sys}\drivers\'+a_strTextfile[iLineCounter]),False) then
        					Log('Successfully moved the binary from '+ ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter])+ ' to ' + ExpandConstant('{sys}\drivers\'+a_strTextfile[iLineCounter])+' during roll back.')
        					else
        					Log('Failed to move the binary from '+ ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter])+ ' to ' + ExpandConstant('{sys}\drivers\'+a_strTextfile[iLineCounter])+' during roll back.');
        				end;
        			end; 
        			if FileExists(ExpandConstant('{code:Getdirvx}\Backup_Release\Backup_DriverFile.log')) then
        			DeleteFile(ExpandConstant('{code:Getdirvx}\Backup_Release\Backup_DriverFile.log'));
        			
        			LoadStringsFromFile(ExpandConstant ('{code:Getdirvx}\Backup_Release\Backup_DriverFilesyswow.log'), a_strTextfile);
        			for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
        			begin
        				if FileExists(ExpandConstant('{code:Getdirvx}\Backup_Release\syswow64\'+a_strTextfile[iLineCounter])) then
        				begin
        					if FileCopy(ExpandConstant('{code:Getdirvx}\Backup_Release\syswow64\'+a_strTextfile[iLineCounter]), ExpandConstant('{syswow64}\drivers\'+a_strTextfile[iLineCounter]),False) then                                    
        					Log('Successfully moved the binary from '+ ExpandConstant('{code:Getdirvx}\Backup_Release\syswow64\'+a_strTextfile[iLineCounter])+ ' to ' + ExpandConstant('{syswow64}\drivers\'+a_strTextfile[iLineCounter])+' during roll back.')
        					else
        					Log('Failed to move the binary from '+ ExpandConstant('{code:Getdirvx}\Backup_Release\syswow64\'+a_strTextfile[iLineCounter])+ ' to ' + ExpandConstant('{syswow64}\drivers\'+a_strTextfile[iLineCounter])+' during roll back.');
        				end;
        			end;
        			if FileExists(ExpandConstant('{code:Getdirvx}\Backup_Release\Backup_DriverFilesyswow.log')) then
        			DeleteFile(ExpandConstant('{code:Getdirvx}\Backup_Release\Backup_DriverFilesyswow.log'));
          finally
          EnableFsRedirection(OldState);         
      end;
      end
      else
      begin
              LoadStringsFromFile(ExpandConstant ('{code:Getdirvx}\Backup_Release\Backup_DriverFile.log'), a_strTextfile);
        			for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
        			begin
        				if FileExists(ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter])) then
        				begin
        					if FileCopy(ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter]), ExpandConstant('{sys}\drivers\'+a_strTextfile[iLineCounter]),False) then
        					Log('Successfully moved the binary from '+ ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter])+ ' to ' + ExpandConstant('{sys}\drivers\'+a_strTextfile[iLineCounter])+' during roll back.')
        					else
        					Log('Failed to move the binary from '+ ExpandConstant('{code:Getdirvx}\Backup_Release\'+a_strTextfile[iLineCounter])+ ' to ' + ExpandConstant('{sys}\drivers\'+a_strTextfile[iLineCounter])+' during roll back.');
        				end;
        			end; 
        			if FileExists(ExpandConstant('{code:Getdirvx}\Backup_Release\Backup_DriverFile.log')) then
        			DeleteFile(ExpandConstant('{code:Getdirvx}\Backup_Release\Backup_DriverFile.log'));	       
      end;
            
    
		if DirExists(ExpandConstant('{code:Getdirvx}\Backup_Release')) then
		begin
			if DelTree(ExpandConstant('{code:Getdirvx}\Backup_Release'), True, True, True) then
			Log('Successfully Deleted the Backup_Release folder from the installation path.');
		end;  
		if RenameFile(ExpandConstant('{code:Getdirvx}\patch.log.old'), ExpandConstant('{code:Getdirvx}\patch.log')) then
		     Log('Successfully renamed patch.log.old to patch.log.');
		if RenameFile(ExpandConstant('{code:Getdirvx}\Backup_Release_old'), ExpandConstant('{code:Getdirvx}\Backup_Release')) then
			   Log('Successfully renamed Backup_Release_old to Backup_Release.');
		if RenameFile(ExpandConstant('{code:Getdirvx}\backup_updates_old'), ExpandConstant('{code:Getdirvx}\backup_updates')) then
			   Log('Successfully renamed backup_updates_old to backup_updates.');
		
		if FileExists(ExpandConstant('{code:Getdirvx}\unins000.dat')) then
		begin
		   if DeleteFile(ExpandConstant('{code:Getdirvx}\unins000.dat')) then
		      Log('Successfully deleted file unins000.dat');
		end;
		if FileCopy(ExpandConstant('{code:Getdirvx}\backup_unins000.dat'), ExpandConstant('{code:Getdirvx}\unins000.dat'), FALSE) then 
		Log('Successfully renamed file backup_unins000.dat to unins000.dat');
			   
		RollBackProgressEnd;
		Log('Installation aborted with exit code 61.');
    ExitInstallationWithStartServices();
		ExitProcess(61);
end;     

(* Moving the old driver files during upgrade, 
If unable to move driver files showing message and aborting the installation so that rollback happens smoothly.*)

procedure RenameDriverFiles(DriverName: String);
var
    OldState: Boolean;
    DriverfilePath, DriverfilePathSyswow: String;
    WaitLoopCount : Integer;
begin
   if VerifyExistingVersion then
   begin
   DriverfilePath := ExpandConstant('{sys}\drivers\'+DriverName);
   DriverfilePathSyswow := ExpandConstant('{syswow64}\drivers\'+DriverName);
	 if CreateDir(ExpandConstant('{code:Getdirvx}\Backup_Release')) then
	 Log('Successfully created '+ ExpandConstant('{code:Getdirvx}\Backup_Release'));
	 
      if  (ProcessorArchitecture = paIA64) or (ProcessorArchitecture = paX64)then
      begin
          OldState := EnableFsRedirection(False);
          try
                  if FileExists(ExpandConstant('{sys}\drivers\'+DriverName)) then
                  begin
                        WaitLoopCount := 1;
                        while (WaitLoopCount <= 4) do
                        begin
                            if RenameFile(ExpandConstant('{sys}\drivers\'+DriverName), ExpandConstant('{code:Getdirvx}\Backup_Release\'+DriverName)) then
                            begin
                               Log('Successfully moved the driver from '+ ExpandConstant('{sys}\drivers\'+DriverName)+ ' to ' + ExpandConstant('{code:Getdirvx}\Backup_Release\'+DriverName)+' before copying the new file.');
                               SaveStringToFile(ExpandConstant('{code:Getdirvx}\Backup_Release\Backup_DriverFile.log'),#13#10+DriverName, True);
                               SetIniString( 'Rename', 'RenameSuccessValue','YES', ExpandConstant('{tmp}\RenameSuccess.conf') );
                               break;
                            end;
                            SetIniString( 'Rename', 'RenameSuccessValue','NO', ExpandConstant('{tmp}\RenameSuccess.conf') );
                            Sleep(15000);
                            WaitLoopCount := WaitLoopCount + 1;
                            Log('Unable to rename the file '+ DriverName+', Retrying('+IntToStr(WaitLoopCount)+')');
                        end;
                   end;
                   RenameSuccess := GetIniString( 'Rename', 'RenameSuccessValue','', ExpandConstant('{tmp}\RenameSuccess.conf') );
                   if RenameSuccess = 'NO' then
                   begin
                        SuppressibleMsgBox('Could not replace ' + ExpandConstant('{sys}\drivers\'+DriverName) + ' as it is in use. Please make '+DriverName+' free and try again the installation.',mbError, MB_OK, MB_OK);
                        RollBackBinaries     
                   end;
                    
                  
                  if FileExists(ExpandConstant('{syswow64}\drivers\'+DriverName)) then
                  begin
                        if CreateDir(ExpandConstant('{code:Getdirvx}\Backup_Release\syswow64')) then
	                         Log('Successfully created '+ ExpandConstant('{code:Getdirvx}\Backup_Release\syswow64'));
                        WaitLoopCount := 1;
                        while (WaitLoopCount <= 4) do
                        begin
                            if RenameFile(ExpandConstant('{syswow64}\drivers\'+DriverName), ExpandConstant('{code:Getdirvx}\Backup_Release\syswow64\'+DriverName)) then
                            begin
                               Log('Successfully moved the driver from '+ ExpandConstant('{syswow64}\drivers\'+DriverName)+ ' to ' + ExpandConstant('{code:Getdirvx}\Backup_Release\syswow64\'+DriverName)+' before copying the new file.');
                               SaveStringToFile(ExpandConstant('{code:Getdirvx}\Backup_Release\Backup_DriverFilesyswow.log'),#13#10+DriverName, True);
                               SetIniString( 'Rename', 'RenameSuccessValue','YES', ExpandConstant('{tmp}\RenameSuccess.conf') );
                               break;
                            end;
                            SetIniString( 'Rename', 'RenameSuccessValue','NO', ExpandConstant('{tmp}\RenameSuccess.conf') );
                            Sleep(15000);
                            WaitLoopCount := WaitLoopCount + 1;
                            Log('Unable to rename the file '+ DriverName+', Retrying('+IntToStr(WaitLoopCount)+')');
                        end;
                   end;
                   RenameSuccess := GetIniString( 'Rename', 'RenameSuccessValue','', ExpandConstant('{tmp}\RenameSuccess.conf') );
                   if RenameSuccess = 'NO' then
                   begin
                        SuppressibleMsgBox('Could not replace ' + ExpandConstant('{syswow64}\drivers\'+DriverName) + ' as it is in use. Figure out the software that is using '+DriverName+' with process explorer utility. Stop the service of corresponding software and re-try the installation.',mbError, MB_OK, MB_OK);
                        RollBackBinaries      
                   end;
                  
          finally
                  EnableFsRedirection(OldState);         
      end;
      end
      else
      begin
                  if FileExists(ExpandConstant('{sys}\drivers\'+DriverName)) then
                  begin
                        WaitLoopCount := 1;
                        while (WaitLoopCount <= 4) do
                        begin
                            if RenameFile(ExpandConstant('{sys}\drivers\'+DriverName), ExpandConstant('{code:Getdirvx}\Backup_Release\'+DriverName)) then
                            begin
                               Log('Successfully moved the driver from '+ ExpandConstant('{sys}\drivers\'+DriverName)+ ' to ' + ExpandConstant('{code:Getdirvx}\Backup_Release\'+DriverName)+' before copying the new file.');
                               SaveStringToFile(ExpandConstant('{code:Getdirvx}\Backup_Release\Backup_DriverFile.log'),#13#10+DriverName, True);
                               SetIniString( 'Rename', 'RenameSuccessValue','YES', ExpandConstant('{tmp}\RenameSuccess.conf') );
                               break;
                            end;
                            SetIniString( 'Rename', 'RenameSuccessValue','NO', ExpandConstant('{tmp}\RenameSuccess.conf') );
                            Sleep(15000);
                            WaitLoopCount := WaitLoopCount + 1;
                            Log('Unable to rename the file '+ DriverName+', Retrying('+IntToStr(WaitLoopCount)+')');
                        end;
                   end;
                   RenameSuccess := GetIniString( 'Rename', 'RenameSuccessValue','', ExpandConstant('{tmp}\RenameSuccess.conf') );
                   if RenameSuccess = 'NO' then
                   begin
                        SuppressibleMsgBox('Could not replace ' + ExpandConstant('{sys}\drivers\'+DriverName) + ' as it is in use. Figure out the software that is using '+DriverName+' with process explorer utility. Stop the service of corresponding software and re-try the installation.',mbError, MB_OK, MB_OK);
                        RollBackBinaries     
                   end;
      end;   
   end;
end;

(* 
 Moving the old binary files during upgrade, 
If unable to move binary files showing message and aborting the installation so that rollback happens smoothly.
*)

procedure RenameBinaryFiles(BinaryName: String);
var
    WaitLoopCount : Integer;
begin	  
   if VerifyExistingVersion then
   begin
   if (DirExists(ExpandConstant('{code:Getdirvx}\Backup_Release'))) then
	     Log('The directory ' + ExpandConstant('{code:Getdirvx}\Backup_Release')+' already exists.')
	 else if CreateDir(ExpandConstant('{code:Getdirvx}\Backup_Release')) then
	     Log('Successfully created '+ ExpandConstant('{code:Getdirvx}\Backup_Release'));

     if FileExists(ExpandConstant('{code:Getdirvx}\'+BinaryName)) then
     begin
         WaitLoopCount := 1;
         while (WaitLoopCount <= 4) do
         begin
             if RenameFile(ExpandConstant('{code:Getdirvx}\'+BinaryName), ExpandConstant('{code:Getdirvx}\Backup_Release\'+BinaryName)) then
             begin
               Log('Successfully moved the binary from '+ ExpandConstant('{code:Getdirvx}\'+BinaryName)+ ' to ' + ExpandConstant('{code:Getdirvx}\Backup_Release\'+BinaryName)+' before copying the new file.');
               SaveStringToFile(ExpandConstant('{code:Getdirvx}\Backup_Release\Backup_BinaryFile.log'),#13#10+BinaryName, True);
               SetIniString( 'Rename', 'RenameSuccessValue','YES', ExpandConstant('{tmp}\RenameSuccess.conf') );
               break;
             end;
             SetIniString( 'Rename', 'RenameSuccessValue','NO', ExpandConstant('{tmp}\RenameSuccess.conf') );
             Sleep(15000);
             WaitLoopCount := WaitLoopCount + 1;
             Log('Unable to rename the file '+ BinaryName+', Retrying('+IntToStr(WaitLoopCount)+')');
         end;
         RenameSuccess := GetIniString( 'Rename', 'RenameSuccessValue','', ExpandConstant('{tmp}\RenameSuccess.conf') );
         if RenameSuccess = 'NO' then
         begin
            SuppressibleMsgBox('Could not replace ' + ExpandConstant('{code:Getdirvx}\'+BinaryName) + ' as it is in use. Figure out the software that is using '+BinaryName+' with process explorer utility. Stop the service of corresponding software and re-try the installation.',mbError, MB_OK, MB_OK);
            RollBackBinaries
         end;                 	
     end;
     
     if FileExists(ExpandConstant('{code:Getdirvx}\consistency\'+BinaryName)) then
     begin
         WaitLoopCount := 1;
         while (WaitLoopCount <= 4) do
         begin
             if RenameFile(ExpandConstant('{code:Getdirvx}\consistency\'+BinaryName), ExpandConstant('{code:Getdirvx}\Backup_Release\'+BinaryName)) then
             begin
               Log('Successfully moved the binary from '+ ExpandConstant('{code:Getdirvx}\consistency\'+BinaryName)+ ' to ' + ExpandConstant('{code:Getdirvx}\Backup_Release\'+BinaryName)+' before copying the new file.');
               SaveStringToFile(ExpandConstant('{code:Getdirvx}\Backup_Release\Backup_BinaryFile_consistency.log'),#13#10+BinaryName, True);
               SetIniString( 'Rename', 'RenameSuccessValue','YES', ExpandConstant('{tmp}\RenameSuccess.conf') );
               break;
             end;
             SetIniString( 'Rename', 'RenameSuccessValue','NO', ExpandConstant('{tmp}\RenameSuccess.conf') );
             Sleep(15000);
             WaitLoopCount := WaitLoopCount + 1;
             Log('Unable to rename the file '+ BinaryName+', Retrying('+IntToStr(WaitLoopCount)+')');
         end;
         RenameSuccess := GetIniString( 'Rename', 'RenameSuccessValue','', ExpandConstant('{tmp}\RenameSuccess.conf') );
         if RenameSuccess = 'NO' then
         begin
            SuppressibleMsgBox('Could not replace ' + ExpandConstant('{code:Getdirvx}\consistency\'+BinaryName) + ' as it is in use. Figure out the software that is using '+BinaryName+' with process explorer utility. Stop the service of corresponding software and re-try the installation.',mbError, MB_OK, MB_OK);
            RollBackBinaries
         end;                 	
     end;
     
  end;   
end;

(* 
 Moving the old binary files during upgrade, 
If unable to move binary files showing message and aborting the installation so that rollback happens smoothly.
*)

procedure RenameFileRepBinaryFiles(BinaryName: String);
var
    WaitLoopCount : Integer;
begin	  
   if VerifyExistingVersion then
   begin
	 if (DirExists(ExpandConstant('{code:Getdirvx}\Backup_Release\FileRep'))) then
	     Log('The directory ' + ExpandConstant('{code:Getdirvx}\Backup_Release\FileRep')+' already exists.')
	 else if CreateDir(ExpandConstant('{code:Getdirvx}\Backup_Release\FileRep')) then
	     Log('Successfully created '+ ExpandConstant('{code:Getdirvx}\Backup_Release\FileRep'));
     
     if FileExists(ExpandConstant('{code:Getdirvx}\FileRep\'+BinaryName)) then
     begin
         WaitLoopCount := 1;
         while (WaitLoopCount <= 4) do
         begin
             if RenameFile(ExpandConstant('{code:Getdirvx}\FileRep\'+BinaryName), ExpandConstant('{code:Getdirvx}\Backup_Release\FileRep\'+BinaryName)) then
             begin
               Log('Successfully moved the binary from '+ ExpandConstant('{code:Getdirvx}\FileRep\'+BinaryName)+ ' to ' + ExpandConstant('{code:Getdirvx}\Backup_Release\FileRep\'+BinaryName)+' before copying the new file.');
               SaveStringToFile(ExpandConstant('{code:Getdirvx}\Backup_Release\FileRep\Backup_BinaryFile_FileRep.log'),#13#10+BinaryName, True);
               SetIniString( 'Rename', 'RenameSuccessValue','YES', ExpandConstant('{tmp}\RenameSuccess.conf') );
               break;
             end;
             SetIniString( 'Rename', 'RenameSuccessValue','NO', ExpandConstant('{tmp}\RenameSuccess.conf') );
             Sleep(15000);
             WaitLoopCount := WaitLoopCount + 1;
             Log('Unable to rename the file '+ BinaryName+', Retrying('+IntToStr(WaitLoopCount)+')');
         end;
         RenameSuccess := GetIniString( 'Rename', 'RenameSuccessValue','', ExpandConstant('{tmp}\RenameSuccess.conf') );
         if RenameSuccess = 'NO' then
         begin
            SuppressibleMsgBox('Could not replace ' + ExpandConstant('{code:Getdirvx}\FileRep\'+BinaryName) + ' as it is in use. Figure out the software that is using '+BinaryName+' with process explorer utility. Stop the service of corresponding software and re-try the installation.',mbError, MB_OK, MB_OK);
            RollBackBinaries
         end;                 	
     end;
  end;   
end;

function Registerhost(Param: String) : String;
begin
    	if WizardSilent then
    	begin
    		InitialSettings;
       	if RoleSelectedInSilentInstall then
       	begin
        		Log('User has given the option ''/role''  in silent installation.')
        		if (role = 'vconmt')  then
        		begin
        		     Log('User has given the option ''/role vconmt''  in silent installation.')
        		     Result := '180'
        		end
        		else 
        		begin
        		      Log('User has passed wrong parameter for ''/role'' option in silent installation.')
    		      		Log('Installation aborted with exit code 46.');
    		      		ExitInstallationWithStartServices();
          				ExitProcess(46);
        		end;
        end;      
    	end
    	else
    	begin
        		RoleType := GetIniString( 'vxagent', 'Role','', ExpandConstant('{tmp}\Role.conf') ); 
            if RoleType = 'MasterTarget' then 
        		begin
        		      Log('User has selected the role as ''Master Target'' in Role of the Agent page.')
        		      Result := '180';
        		end;
    	end;
end;

function IOThreads(Param: String) : String;
begin
	if WizardSilent then
	begin
		InitialSettings;
   	if RoleSelectedInSilentInstall then
   	begin
    		    Log('User has given the option ''/role''  in silent installation.')
        		if (role = 'vconmt')  then
        		begin
        		     Log('User has given the option ''/role vconmt''  in silent installation.')
        		     Result := '1'
        		end
        		else 
        		begin
        		      Log('User has passed wrong parameter for ''/role'' option in silent installation.')
        		      Log('Installation aborted with exit code 46.');
    		      	  ExitInstallationWithStartServices();
          				ExitProcess(46);
        		end;
    end;
	end
	else
	begin
        		RoleType := GetIniString( 'vxagent', 'Role','', ExpandConstant('{tmp}\Role.conf') ); 
            if RoleType = 'MasterTarget' then 
        		begin
        		      Log('User has selected the Role as ''Master Target'' in Role of the Agent page.')
        		      Result := '1';
        		end;
        		
	end;
end;

function NWThreads(Param: String) : String;
begin
	if WizardSilent then
	begin
		InitialSettings;
   	if RoleSelectedInSilentInstall then
   	begin
    		    Log('User has given the option ''/role''  in silent installation.')
        		if (role = 'vconmt')  then
        		begin
        		     Log('User has given the option ''/role vconmt''  in silent installation.')
        		     Result := '1'
        		end
        		else 
        		begin
        		      Log('User has passed wrong parameter for ''/role'' option in silent installation.')
        		      Log('Installation aborted with exit code 46.');
    		      	  ExitInstallationWithStartServices();
          				ExitProcess(46);
        		end;
    end;
	end
	else
	begin
            RoleType := GetIniString( 'vxagent', 'Role','', ExpandConstant('{tmp}\Role.conf') ); 
            if RoleType = 'MasterTarget' then 
        		begin
        		      Log('User has selected the role as ''Master Target'' in Role of the Agent page.')
        		      Result := '1';
        		end;
  end;
        		
end;

function MaxFastSyncThreads(Param: String) : String;
begin
	if WizardSilent then
	begin
		InitialSettings;
   	if RoleSelectedInSilentInstall then
   	begin
    		    Log('User has given the option ''/role''  in silent installation.')
        		if (role = 'vconmt')  then
        		begin
        		     Log('User has given the option ''/role vconmt''  in silent installation.')
        		     Result := '2'
        		end
        		else 
        		begin
        		      Log('User has passed wrong parameter for ''/role'' option in silent installation.')
        		      Log('Installation aborted with exit code 46.');
    		      	  ExitInstallationWithStartServices();
          				ExitProcess(46);
        		end;
    end;
	end
	else
	begin
		        RoleType := GetIniString( 'vxagent', 'Role','', ExpandConstant('{tmp}\Role.conf') ); 
            if RoleType = 'MasterTarget' then 
        		begin
        		      Log('User has selected the role as ''Master Target'' in Role of the Agent page.')
        		      Result := '2';
        		end;
        		
	end;
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

function PassphraseValidation : Boolean;
var
Count: Integer;
begin
	if WizardSilent then
	begin
    		InitialSettings;
    		if IsSilentOptionSelected('PassphrasePath') then
        begin
            if FileExists(ExpandConstant(PassphrasePath)) then	
    		    begin
    			  	  LoadStringFromFile(ExpandConstant (PassphrasePath), LogFileString);    
    			      PassphraseValue := LogFileString;
    			      if PassphraseValue = '' then 
                begin
                    SuppressibleMsgBox('The provided Passphrase file is empty. Aborting the installation.',mbError, MB_OK, MB_OK);
    			          DeleteFile(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\private\connection.passphrase'));
    			          Log('Installation aborted with exit code 14.');
    			          ExitInstallationWithoutStartServices();
                    ExitProcess(14);
                end;
                if not DirExists(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\private')) then
                begin
                    if not DirExists(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')) then
                    begin
                        if not CreateDir(ExpandConstant ('{sd}\ProgramData\Microsoft Azure Site Recovery')) then
                        begin
                              SuppressibleMsgBox('Unable to create '+ExpandConstant ('{sd}\ProgramData\Microsoft Azure Site Recovery')+ ' directory.' ,mbError, MB_OK, MB_OK);
                              Log('Installation aborted with exit code 49.');
                    			    ExitInstallationWithoutStartServices();
                              ExitProcess(49);
                        end
                        else
                            Log('Successfully created {sd}\ProgramData\Microsoft Azure Site Recovery directory.');
                    end;
                    if not CreateDir(ExpandConstant ('{sd}\ProgramData\Microsoft Azure Site Recovery\private')) then
                    begin
                          SuppressibleMsgBox('Unable to create '+ExpandConstant ('{sd}\ProgramData\Microsoft Azure Site Recovery\private')+ ' directory.' ,mbError, MB_OK, MB_OK);
                          Log('Installation aborted with exit code 49.');
                			    ExitInstallationWithoutStartServices();
                          ExitProcess(49);
                    end
                    else
                        Log('Successfully created {sd}\ProgramData\Microsoft Azure Site Recovery\private directory.');
                end
                else
                			Log(ExpandConstant ('{sd}\ProgramData\Microsoft Azure Site Recovery\private')+ ' directory already exists.');    			              
              	SaveStringToFile(ExpandConstant ('{sd}\ProgramData\Microsoft Azure Site Recovery\private\connection.passphrase'),'' , False);
              	DeleteEmptyLines(ExpandConstant ('{sd}\ProgramData\Microsoft Azure Site Recovery\private\connection.passphrase'));
                SaveStringToFile(ExpandConstant ('{sd}\ProgramData\Microsoft Azure Site Recovery\private\connection.passphrase'),PassphraseValue , False);
              
                if VerifyExistingVersion and (IsServiceInstalled('frsvc') or IsServiceInstalled('svagents')) then
                begin
                    if IsServiceInstalled('svagents') then
                        RegQueryStringValue(HKLM,'Software\SV Systems\VxAgent','InstallDirectory',InstallDir);
                    if IsServiceInstalled('frsvc') then
                        RegQueryStringValue(HKLM,'Software\SV Systems\FileReplicationAgent','InstallDirectory',InstallDir);
                    CS_IP_Value := GetIniString( 'vxagent.transport', 'Hostname','', ExpandConstant(InstallDir+'\Application Data\etc\drscout.conf') );
                    CS_PORT_Value := GetIniString( 'vxagent.transport', 'Port','', ExpandConstant(InstallDir+'\Application Data\etc\drscout.conf') );
                    Comm_Number := GetIniString( 'vxagent.transport', 'https','', ExpandConstant(InstallDir+'\Application Data\etc\drscout.conf'));
                    if Comm_Number = '1' then
                        CommMode := 'https'
                    else
                        CommMode := 'http';
                end
                else
                begin
                    InitialSettings;
                    if (CommunicationMode = 'Http') or (CommunicationMode = 'http') then
                    begin 
                        CommMode := 'http';
                    end;
                    if (CommunicationMode = 'Https') or (CommunicationMode = 'https') or not IsSilentOptionSelected('CommunicationMode') then
                    begin 
                        CommMode := 'https';
                    end;
                end;
                    
                ExtractTemporaryFile('csgetfingerprint.exe');
                
                // Invoking csgetfingerprint.exe for 3 times inorder to establish a connection with Configuration Server, if it fails for the 1st or 2nd try with connection issues.
                // Aborting the installation if it fails for the 3rd try.
                Count := 0;
                while (Count <= 3) do
                begin
                    if (CommMode = 'http') then
                    begin
    					          Log('Invoking csgetfingerprint.exe with IP '+CS_IP_Value+' and Port '+CS_PORT_Value+' in case of http mode.');
                        ShellExec('', ExpandConstant('{tmp}\csgetfingerprint.exe'),' -i '+CS_IP_Value+' -p '+CS_PORT_Value+' -n -v -r', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode); 
                    end; 
            
                    if (CommMode = 'https') then
                    begin 
    					          Log('Invoking csgetfingerprint.exe with IP '+CS_IP_Value+' and Port '+CS_PORT_Value+' in case of https mode.');
                        ShellExec('', ExpandConstant('{tmp}\csgetfingerprint.exe'),' -i '+CS_IP_Value+' -p '+CS_PORT_Value +' -r', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode);            
        			      end;
        			      if FileExists(ExpandConstant('{tmp}\csgetfingerprint.log')) then	
                    begin
                       	LoadStringFromFile(ExpandConstant ('{tmp}\csgetfingerprint.log'), LogFileString);    
                        Log(#13#10 +'csgetfingerprint log Starts here '+ #13#10 + '*********************************************');
                        Log(#13#10 + LogFileString);
                        Log(#13#10 + '*********************************************' + #13#10 + 'csgetfingerprint log Ends here :' +#13#10 + #13#10);
                    end;
        			      if (not(Errorcode=0)) then
        			      begin
        			           Sleep(30000);
                         Count := Count + 1;
                         Log('Either the passphrase is invalid or unable to establish a connection with Configuration server. Retry count : ('+IntToStr(Count)+')');
        			           if (Count = 4) then
        			           begin
            			           SuppressibleMsgBox('Invalid Configuration Server IP or Port or  Connection Passphrase provided. Aborting…',mbError, MB_OK, MB_OK);
            			           DeleteFile(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\private\connection.passphrase'));
            			           Log('Installation aborted with exit code 14.');
            			           ExitInstallationWithoutStartServices();
                             ExitProcess(14);
                         end;
        			      end
        			      else 
        			      begin
                        if not Fileexists(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\private\connection.passphrase')) then
                        begin                                                  
                            Log('Passphrase file is not available at '+ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\private')+'. Aborting.');
                            Count := 4;
                            ExitInstallationWithoutStartServices();
  						              ExitProcess(14);
                        end
                        else
                        begin
                             Log('Proceeding ahead with the installation as user entered a valid passphrase value.');
              			         Count := 4;
              			         Result := True;
                        end;
        			      end;
        			  end;
    		    end
    		    else
            begin
                Log('User has passed wrong ''/PassphrasePath'' option in silent installation. Aborting...');
                Log('Installation aborted with exit code 48.');
                ExitInstallationWithoutStartServices();
                ExitProcess(48);
            end;
        end
        else
        begin
            Log('User has not passed ''/PassphrasePath'' option in silent installation. Aborting...');
            Log('Installation aborted with exit code 47.');
            ExitInstallationWithoutStartServices();
            ExitProcess(47);
        end;
    end;
end;

function AbortIfWrongRoleSelected : Boolean;
begin
	if (FreshInstallation = 'IDYES') then
	begin
	if WizardSilent then
	begin
    		if RoleSelectedInSilentInstall then
    		begin
           		InitialSettings;
           		if (role = 'vconmt')  then
         		      Result := True
           		else 
          		begin
          		      Log('User has passed wrong parameter for ''/role'' option in silent installation. Hence, Aborting.');
          		      Log('Installation aborted with exit code 46.');
    		      	    ExitInstallationWithoutStartServices();
                    ExitProcess(46);
          		end;
        end
        else 
              Result := True;
  end;
  end;
end;



function issilent : Boolean;
begin
	if WizardSilent then
	begin
		InitialSettings;
		if (restart = 'Y') or (restart = 'y') then
			Result := True
		else
			Result := False;
	end
	else
		Result := False;
end;

function Getoptions(Param: String) : String;
begin
	if WizardSilent then
	begin
		InitialSettings;
		if (AW = 'Y') or (AW = 'y') then
			Result := ' --appname sql --appname exchange --appname fileserver --appname bes '
		else
			Result := '';
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

procedure StartInDskFltService ;
var
  ErrorCode, Returncode: Integer;
  DrvutilFile : String;  
  LogFileString : AnsiString;
begin
	if FreshInstallation = 'IDYES' then
	begin
		DrvutilFile := ExpandConstant('{code:Getdirvx}\drvutil.exe');
		ShellExec('', ExpandConstant('{cmd}'),'/C sc create InDskFlt start= boot type= kernel group= "PnP Filter" binpath= '+ExpandConstant('{sys}\drivers\InDskFlt.sys'),'', SW_HIDE, ewWaitUntilTerminated,Errorcode);
    if (IsServiceInstalled('InDskFlt')) then
		begin
			Log('InDskFlt service is installed.' + Chr(13));
			Log('Starting the InDskFlt service.' + Chr(13));
			if CustomStartService('InDskFlt') then
			begin
				Log('InDskFlt service is started successfully.' + Chr(13));
        SetIniString( 'Reboot', 'Driver_Reboot','no', ExpandConstant('{tmp}\Driver_Reboot.conf')); 
			end
			else
			begin
				Log('Unable to start InDskFlt service.' + Chr(13));
        SetIniString( 'Reboot', 'Driver_Reboot','yes', ExpandConstant('{tmp}\Driver_Reboot.conf') );
			end;
			
      if FileReplaceString(ExpandConstant('{code:Getdirvx}\VerifyBootDiskFiltering.ps1'), 'Install-Directory', ExpandConstant('{code:Getdirvx}')) then
          Log('Install-Directory value has been replaced in VerifyBootDiskFiltering.ps1'+ Chr(13))
      else  
          Log('Install-Directory value has not been replaced in VerifyBootDiskFiltering.ps1'+ Chr(13));

      if FileReplaceString(ExpandConstant('{code:Getdirvx}\EnableDisableFiltering.bat'), 'Install-Directory', ExpandConstant('{code:Getdirvx}')) then
          Log('Install-Directory value has been replaced in EnableDisableFiltering.bat'+ Chr(13))
      else  
          Log('Install-Directory value has not been replaced in EnableDisableFiltering.bat'+ Chr(13));

			if (FileExists(DrvutilFile)) then
			begin
				Log('Invoking VerifyBootDiskFiltering.ps1 script.');
        ShellExec('', ExpandConstant('{cmd}'),'/C PowerShell Set-ExecutionPolicy Unrestricted','', SW_HIDE, ewWaitUntilTerminated,Returncode);
        ShellExec('', ExpandConstant('{cmd}'),'/C PowerShell -NonInteractive -NoProfile -Command "& '''+ExpandConstant('{code:Getdirvx}\VerifyBootDiskFiltering.ps1')+'''  ; exit $LastExitCode"','', SW_HIDE, ewWaitUntilTerminated,Errorcode);
        if (Errorcode = 0) then
        begin
            Log('VerifyBootDiskFiltering returned success.');
            SetIniString( 'Reboot', 'Driver_Reboot_Machine','no', ExpandConstant('{tmp}\Driver_Reboot.conf') );
        end
        else
        begin
            Log('VerifyBootDiskFiltering returned failure.');
            SetIniString( 'Reboot', 'Driver_Reboot_Machine','yes', ExpandConstant('{tmp}\Driver_Reboot.conf') );
        end;

				if FileExists(ExpandConstant('{code:Getdirvx}\Indskfltservice_output.txt')) then	
				begin
						LoadStringFromFile(ExpandConstant ('{code:Getdirvx}\Indskfltservice_output.txt'), LogFileString);    
						Log('Indskflt service output Start here '+ #13#10 + '*********************************************')
						Log(#13#10 + LogFileString);
						Log(#13#10 + '*********************************************' + #13#10 + 'Indskflt service output End here :' +#13#10);
				end;
			end
			else
			begin
				Log('drvutil.exe file is not available.');
			end;
		end
		else
		begin
			Log('Unable to install InDskFlt service.' + Chr(13));
      SetIniString( 'Reboot', 'Driver_Reboot','yes', ExpandConstant('{tmp}\Driver_Reboot.conf') );
		end;
	end;
end;

procedure HeapValueChange();
var
  SharedSectionStringList : TStringList;
  DotPosition : Integer;
  Windows_Key : String;
  InputStr, OctetStr, SharedSection, HeapValue: String;	
  Errorcode: Integer;
  OldState: Boolean;

begin
    HeapValue := '4096';
    RoleType := GetIniString( 'vxagent', 'Role','', ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ); 
    if RoleType = 'MasterTarget' then 
    begin
        if IsWin64 then
        begin
            //Turn off redirection, so that cmd.exe from the 64-bit System directory is launched.
            OldState := EnableFsRedirection(False);
            try
                Exec(ExpandConstant('{cmd}'), '/C fsutil.exe behavior set disable8dot3 1 ','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
            finally
                // Restore the previous redirection state.
                EnableFsRedirection(OldState);
            end;
        end
        else
            Exec(ExpandConstant('{cmd}'), '/C fsutil.exe behavior set disable8dot3 1 ','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
        if (Errorcode=0) then
            Log('Disable 8dot3 name creation on all volumes on the system is successful.'+ #13#10) 
        else   
            Log('Disable 8dot3 name creation on all volumes on the system is not successful.'+ #13#10);
             
        //Log('User has selected the role as ''Master Target'' Hence, Changing the registry ''HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\SubSystems\Windows''')
        
        if RegQueryStringValue(HKLM,'SYSTEM\CurrentControlSet\Control\Session Manager\SubSystems','Windows',Windows_Key) then
        begin
            Log('Existing value in registry ''HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\SubSystems'' for Windows is'+ Windows_Key)
            InputStr := Windows_Key;
            while Length(InputStr) <> 0 do 
            begin
                DotPosition := Pos(' ',InputStr);
                if DotPosition <> 0 then 
                begin
                    OctetStr := Copy(InputStr,1,DotPosition-1);
                    InputStr := Copy(InputStr,DotPosition + 1,1000);
                    					  
                    DotPosition := Pos('SharedSection',OctetStr);
                    if DotPosition <> 0 then 
                    begin
                        SharedSection := OctetStr;
                        SharedSectionStringList := TStringList.Create;
                        StringChangeEx(SharedSection,',',',',FALSE);
                        SharedSectionStringList.CommaText :=SharedSection;
                        if ((StrToInt(SharedSectionStringList.Strings[2])) < (StrToInt(HeapValue))) then
                        begin
                            SharedSection := (SharedSectionStringList.Strings[0])+','+(SharedSectionStringList.Strings[1])+','+(HeapValue); 
                            StringChangeEx(Windows_Key,OctetStr,SharedSection,True);
                            RegWriteStringValue(HKLM,'SYSTEM\CurrentControlSet\Control\Session Manager\SubSystems','Windows',Windows_Key);
                            Log('Modified value in registry ''HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\SubSystems'' for Windows is'+ Windows_Key)
                            SetIniString( 'heap', 'HeapValueChangeRestart','YES', ExpandConstant('{tmp}\Heap.conf') );
                        end;
                    end;
                end
                else 
                begin
                    OctetStr := InputStr;
                    InputStr := '';
                end;
            end;
        end;
    end;
end;

(* NeedRestart --- Prompt the user for reboot option with default set to Yes
If we are installing UA with VX as the agent type or upgrading the VX, we prompt the user for reboot *)

function NeedRestart(): Boolean;
begin
	HeapValueChange
	Result := False;
	HeapValueChangeRestartValue := GetIniString( 'heap', 'HeapValueChangeRestart','', ExpandConstant('{tmp}\Heap.conf') );
  Driver_Reboot := GetIniString( 'Reboot', 'Driver_Reboot','', ExpandConstant('{tmp}\Driver_Reboot.conf') );
  Driver_Reboot_Machine := GetIniString( 'Reboot', 'Driver_Reboot_Machine','', ExpandConstant('{tmp}\Driver_Reboot.conf') );
  if ((FreshInstallation = 'IDYES') and (FreshInstallationVX = 'IDYES')) or (FreshInstallationVX = 'IDYES') then
	begin
	    if WizardSilent then
		  begin
	        InitialSettings;
			    if (restart = 'Y') or (restart = 'y') then
				      Result := True
		      else if HeapValueChangeRestartValue = 'YES' then 
              Result := True
          else if (not IsRoleSelected) then
              Log('Though reboot is required at the end of Mobility service installation, silent installer will not reboot the system.');
              Result := False;
		  end
      // Restart when heapvalue is changed.
      else if HeapValueChangeRestartValue = 'YES' then 
          Result := True
      // Restart when Role is Agent/Mobility Service.
      else if Driver_Reboot = 'yes' then
          Result := True
      else if Driver_Reboot_Machine = 'yes' then
          Result := True
      //Default - no reboot is required.
		  else
			    Result := False;
	end;
end;

function IsItanium64 : Boolean;
begin
	if ProcessorArchitecture = paIA64 then
		Result := True
	else
		Result := False;
end;

function Verifywin2k : Boolean;
var
	Versionstring : String;
begin
	RegQueryStringValue(HKLM,'Software\Microsoft\Windows NT\CurrentVersion','ProductName',Versionstring);
	if (Pos('Windows 2000',Versionstring) > 0) then
	begin
    		Log('Your machine operating system does not support the installer.' + #13#10);
    		SuppressibleMsgBox('Executable you are trying to install is not suitable for this platform.',mbError, MB_OK, MB_OK);
    		Log('Installation aborted with exit code 42.');
        ExitInstallationWithoutStartServices();
        ExitProcess(42);       		      
	end
	else
		    Result := True;
end;

function VerifyCluster : Boolean;
begin
      Result := True;
	    if IsServiceInstalled('ClusSvc') then
	    begin
        	 if not IsServiceInstalled('MSDTC') then
        	 begin
          			Log('MSDTC service is not installed on this Cluster Node.' + #13#10);
          			SuppressibleMsgBox('MSDTC service is not installed on this Cluster Node.', mbError, MB_OK, MB_OK);
          			Log('Installation aborted with exit code 45.');
    		      	ExitInstallationWithoutStartServices();
                ExitProcess(45);        		      
        	 end
        	 else
        	 begin
        	       Log('MSDTC service is installed for Cluster Nodes.' + #13#10);
        	       Result := True;
        	 end;
     end
     else
     begin
           Log('ClusSvc service is not installed.' + #13#10);
           Result := True;
     end;
end;


function VsnapCheck : Boolean;
var
 CdpcliPath : String;
begin
      CdpcliPath := GetVXInstallDir('0') + '\cdpcli.exe';
	    Log(#13#10 + 'Executing the fisrt command for unmounting the vsnaps:' + #13#10 +  CdpcliPath + ' --vsnap --op=unmountall --softremoval ' + #13#10);
	    Exec(CdpcliPath,' --vsnap --op=unmountall --softremoval','',SW_HIDE,ewWaitUntilTerminated, ResultCode1);
      if (ResultCode1=0) then
      begin
		        Log(#13#10 + 'Executing the second command for unmounting the vsnaps:' + #13#10 + CdpcliPath + ' --virtualvolume --op=unmountall --softremoval --checkfortarget=no' + #13#10);
        		Exec(CdpcliPath,' --virtualvolume --op=unmountall --softremoval --checkfortarget=no','',SW_HIDE,ewWaitUntilTerminated, ResultCode2);
        		if (ResultCode2=0) then
		        begin
			             Log(#13#10 + 'Vsnaps are un-mounted successfully' + #13#10);
               		 Result := True;
            end
        		else
		        begin
            			ErrorMsg2 := IntToStr(ResultCode2);
            			Log(#13#10 + 'Vsnaps are not un-mounted properly because of the following error ' + ErrorMsg2 +  #13#10);
            			SuppressibleMsgBox('Vsnaps are not un-mounted properly. Aborting installation as this pre-upgrade step couldn’t be completed.', mbInformation, MB_OK, MB_OK);
            			Log('Installation aborted with exit code 50.');
    		      		ExitInstallationWithStartServices();
          				ExitProcess(50);
        	  end;
      end
    	else
    	begin
        		ErrorMsg1 := IntToStr(ResultCode1);
        		Log(#13#10 + 'Vsnaps are not un-mounted properly because of the following error ' + ErrorMsg1 +  #13#10);
        		SuppressibleMsgBox('Vsnaps are not un-mounted properly. Aborting installation as this pre-upgrade step couldn’t be completed.', mbInformation, MB_OK, MB_OK);
        		Log('Installation aborted with exit code 50.');
    		    ExitInstallationWithStartServices();
    				ExitProcess(50);
    	end;
end;

(* Change Drscout value during upgrade from (5.5.GA or 5.5.SP1) to current version
->if CDPFreeSpaceTrigger is less than 1073741824, set it to 1073741824, if its more or equal to 1073741824 then leave it as it is.
->if FastSyncHashCompareDataSize key exists, then set it to 65536, no matter whatever is the previous value.
*)

procedure ChangeDrScoutValue();
var
	CDPFreeSpaceValue : String ;
	CDPFreeSpaceTrigger : Longint ;
begin
      if IniKeyExists( 'vxagent', 'CDPFreeSpaceTrigger', ExpandConstant('{code:GetVXInstallDir}\Application Data\etc\drscout.conf') ) then
	    begin
		        CDPFreeSpaceValue := GetIniString( 'vxagent', 'CDPFreeSpaceTrigger','', ExpandConstant('{code:GetVXInstallDir}\Application Data\etc\drscout.conf') );
		        CDPFreeSpaceTrigger := StrToInt(CDPFreeSpaceValue);
		        If (CDPFreeSpaceTrigger < 1073741824) then
        		begin
			             Log('Changing the CDPFreeSpaceTrigger value to 1073741824 in drscout.conf file.' + #13#10);
               		 SetIniInt( 'vxagent', 'CDPFreeSpaceTrigger',1073741824, ExpandConstant('{code:GetVXInstallDir}\Application Data\etc\drscout.conf'));
        		end;
	    end;
    	if IniKeyExists( 'vxagent', 'FastSyncHashCompareDataSize', ExpandConstant('{code:GetVXInstallDir}\Application Data\etc\drscout.conf') ) then
    	begin
        		Log('Changing the FastSyncHashCompareDataSize value to 65536 in drscout.conf file.' + #13#10);
        		SetIniInt( 'vxagent', 'FastSyncHashCompareDataSize',65536, ExpandConstant('{code:GetVXInstallDir}\Application Data\etc\drscout.conf') );
    	end;
end;

(* Removes all "." chars from the end of directory path
*)

procedure OnDirEditChange(Sender: TObject);
var
  S: string;
begin
  S := WizardDirValue;
  if (Length(S) > 0) and (S[Length(S)] = '.') then
  begin
    MsgBox('Last char(s) of the entered target folder is "."' + #13#10 +
      'All "." chars from the end will be deleted!', mbInformation, MB_OK);
    while (Length(S) > 0) and (S[Length(S)] = '.') do
      Delete(S, Length(S), 1);
    WizardForm.DirEdit.Text := S;
  end;
end;



(* InitializeWizard function is called after InitializeSetup function (If this function exists).
1. If the Installation is silent, it will not not dispalying the Input option page. The input is taken from the option we are passing to this exe
2. If the Installation is not silent (Interactive), we are displaying the Input option page which will prompt the user for
	a. File Replication Agent Installation or
	b. Volume Replication Agent Installation or
	c. Both the File and Volume Replication Agent Installation
*)

procedure InitializeWizard;
begin
  	 WizardForm.DirEdit.OnChange := @OnDirEditChange;
  	if WizardSilent then
	     InitialSettings;

    if FreshInstallation = 'IDYES' then 
    begin
      		FreshInstallationFX := 'IDYES';
          FreshInstallationVX := 'IDYES';
	  
	     RolePage := CreateCustomPage(WpLicense, 'Role of the Agent.', '');
   
       RolePageText := TNewStaticText.Create(RolePage);
       RolePageText.Caption := 'Select the installation type'
       RolePageText.AutoSize := True;
       RolePageText.Parent := RolePage.Surface;
       
       Agent := TNewRadioButton.Create(RolePage);
       Agent.Caption := 'Mobility Service';
       Agent.Top := ScaleY(30);
       Agent.Checked := True;
       Agent.Parent := RolePage.Surface;
       
       AgentText := TNewStaticText.Create(RolePage);
       AgentText.Top := ScaleY(60);
       AgentText.Caption := '     Install Mobility Service on source machines that you want to protect.'
       AgentText.AutoSize := True;
       AgentText.Parent := RolePage.Surface;
       
       Target := TNewRadioButton.Create(RolePage);
       Target.Caption := 'Master Target';
       Target.Top := ScaleY(100);
       Target.Parent := RolePage.Surface;
       
       TargetText := TNewStaticText.Create(RolePage);
       TargetText.Top := ScaleY(130);
       TargetText.Caption := '     Install Master Target Server on machines that will act as a target for replicated' +#13#10 '     data from your protected machines.'
       TargetText.AutoSize := True;
       TargetText.Parent := RolePage.Surface;
    end;
    ProgressPage := CreateOutputProgressPage('Setup is rolling back the changes....','');
   	ProgressPageEnd := CreateOutputProgressPage('Setup is rolling back the changes....','');
end;

// Validating HostName length. If hostname length is greater than 15 characters, aborting the installation.

function ValidateHostName : Boolean;
var
HostName_Input,HostName_Output : String;
ExecStdout : AnsiString;
Errorcode: Integer;
begin  
    HostName_Input := ExpandConstant('{tmp}\HostName.txt');
    Exec(ExpandConstant('{cmd}'), '/C hostname > "'+HostName_Input+'"','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
    LoadStringFromFile(HostName_Input, ExecStdout)
    HostName_Output := Trim(ExecStdout);
    
    if( Length(HostName_Output) > 15) then
    Begin
       SuppressibleMsgBox('Hostname has more than 15 chracters. Master Target server can not be installeed on this system.',mbError, MB_OK, MB_OK);
       Result := True;
    end
    else
        Result := False;
end;

function ValidateHostName_Silent : Boolean;
begin
    if (FreshInstallation = 'IDYES') then
    begin
    if WizardSilent then
	  begin
    		if RoleSelectedInSilentInstall then
    		begin
           		InitialSettings;
           		if (role = 'vconmt')  then
           		begin
         		      if ValidateHostName then
         		      begin
         		          Log('Installation aborted with exit code 44.');
         		          ExitInstallationWithoutStartServices();
                      ExitProcess(44);
                  end;
         		  end
           		else 
          		begin
          		   Result := True;
          		end;
        end
        else 
              Result := True;
    end;
    end;
end;

function RestartRequired : Boolean;
var
	PendingFileRenameOperations : String;
	UpdateExeVolatile : Cardinal;
begin
      if RegValueExists(HKLM, 'SYSTEM\CurrentControlSet\Control\Session Manager', 'PendingFileRenameOperations') then
      begin 
          RegQueryMultiStringValue(HKLM,'SYSTEM\CurrentControlSet\Control\Session Manager','PendingFileRenameOperations',PendingFileRenameOperations);
          if not (PendingFileRenameOperations = '') then
          begin
              Log('PendingFileRenameOperations value: '+PendingFileRenameOperations);
              SuppressibleMsgBox('A system restart from a previous installation or update is pending. Restart your system and re-try the installation again.' , mbError, MB_OK, MB_OK);
              Log('Installation aborted with exit code 62.');
              ExitInstallationWithoutStartServices();
              ExitProcess(62);
          end
          else
          begin
              Log('''HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\PendingFileRenameOperations'' registry key is having empty value. Proceeding with installation.');
              Result := True;
          end;
      end
      else
      begin
          Log('PendingFileRenameOperations registry key is not available under ''HKLM\SYSTEM\CurrentControlSet\Control\Session Manager'' registry hive. Proceeding with installation.');
          Result := True;
      end;
      
      if RegValueExists(HKLM, 'SOFTWARE\Microsoft\Updates', 'UpdateExeVolatile') then
      begin     
          RegQueryDWordValue(HKLM,'SOFTWARE\Microsoft\Updates','UpdateExeVolatile',UpdateExeVolatile);
          if not (IntToStr(UpdateExeVolatile) = '0') then
          begin
              Log('UpdateExeVolatile value: '+IntToStr(UpdateExeVolatile));
              SuppressibleMsgBox('A system restart from a previous installation or update is pending. Restart your system and re-try the installation again.' , mbError, MB_OK, MB_OK);
              Log('Installation aborted with exit code 62.');
              ExitInstallationWithoutStartServices();
              ExitProcess(62);
          end
          else
          begin
              Log('''SOFTWARE\Microsoft\Updates\UpdateExeVolatile'' registry key is having ''0'' value. Proceeding with installation.');
              Result := True;
          end;
      end
      else
      begin
          Log('UpdateExeVolatile registry key is not available under ''SOFTWARE\Microsoft\Updates'' registry hive. Proceeding with installation.');
          Result := True;
      end;
      
      if not (RegKeyExists(HKLM, 'Software\InMage Systems\Installed Products\5') and IsServiceInstalled('svagents')) then
      begin 
          if ((IsServiceInstalled('involflt')) and (not (RegKeyExists(HKEY_LOCAL_MACHINE,'System\CurrentControlSet\Services\involflt')))) then
          begin
              Log('Found the old driver.');
              SuppressibleMsgBox('A system restart from a previous installation or update is pending. Restart your system and re-try the installation again.' , mbError, MB_OK, MB_OK);
              Log('Installation aborted with exit code 62.');
              ExitInstallationWithoutStartServices();
              ExitProcess(62);
          end
          else
          begin
              Log('Did not find old driver. Proceeding with installation.');
              Result := True;
          end;
      end
      else
      begin
          Log('Already agent is installed. Proceeding with upgrade.');
          Result := True;
      end;
end;

function IsCXInstalled : Boolean;
begin
    if RegKeyExists(HKLM, 'Software\InMage Systems\Installed Products\9') and IsServiceInstalled('tmansvc') then
    begin
        Log('Configuration/Process Server is already installed on this machine.');
        Result := True;
    end
    else
    begin
        Log('Configuration/Process Server is not installed on this machine.');
        Result := False;
    end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
	Result := True;
  if FreshInstallation = 'IDYES' then
  begin
		if not WizardSilent then
    begin
        if CurPageID = RolePage.ID then
        begin
            if Agent.Checked = True then
            begin	
                SetIniString( 'vxagent', 'Role','Agent', ExpandConstant('{tmp}\Role.conf') );
                RestartRequired
            end
            else if Target.Checked = True then
            begin	
                SetIniString( 'vxagent', 'Role','MasterTarget', ExpandConstant('{tmp}\Role.conf') );
                if ValidateHostName then
                begin
                    Log('Installation aborted with exit code 44.');
                    ExitInstallationWithStartServices();
                    ExitProcess(44);
                end;
                if not IsCXInstalled then
                begin
                    RestartRequired;
                end;
            end;
        end;
    end;
  end;
		if CurPageID = wpSelectDir then    
    begin
      OnDirEditChange(nil);
    end;
end;



(* This function is called when both the FX and VX are already installed in the setup and
when both are the latest versions. In this case we give proper message and will not upgrade the agents
*)

function Fx_Vx_Available : Boolean;
begin
	if WizardSilent then
	begin
    		InitialSettings;
    		if a = 'u' then
    		begin
        			Log('Aborting installation as latest version of Unified Agent is existing in this machine.' + Chr(13));
              Result := False;
    		end
    		else if ((Uninstall = 'Y') or (Uninstall = 'y')) then
    			     Result:=True
    		else if ((UpdateConfig = 'Y') or (UpdateConfig = 'y')) then
    		begin
        			SuppressibleMsgBox('Stopping the services. It may take a while, please wait.', mbInformation, MB_OK, MB_OK);
        			CustomServiceStop('frsvc');
        			CustomServiceStop('svagents');
        			CustomServiceStop('InMage Scout Application Service');
        			CustomServiceStop('cxprocessserver');
        			Result:=True;
    		end
    		else
    		begin
        			Log('Aborting installation as latest version of Unified Agent is existing in this machine.' + Chr(13));
              Result := False;
    		end;
	end
	else
	begin
    		Log('Aborting installation as latest version of Unified Agent is existing in this machine.' + Chr(13));
    		Msgbox('Aborting installation as latest version of Unified Agent is existing in this machine.',MbConfirmation, MB_OK);
        Result := False;
	end;
end;

(* This function is called when VX is already installed in the setup
This will add the FX agent
*)

function Vx_Available : Boolean;
begin
	if WizardSilent then
	begin
		      InitialSettings;
      		if a = 'u' then
      		begin
          			Log('Adding FX..' + Chr(13));
          			FreshInstallationFX := 'IDYES';
          			FreshFX := 'IDYES';
          			ReinstallSelected := 'IDYES';
      		end
      		else if ((Uninstall = 'Y') or (Uninstall = 'y')) then
      			   Result:=True
      		else if ((UpdateConfig = 'Y') or (UpdateConfig = 'y')) then
      		begin
          			SuppressibleMsgBox('Stopping the services. It may take a while, please wait.', mbInformation, MB_OK, MB_OK);
          			CustomServiceStop('svagents');
          			CustomServiceStop('InMage Scout Application Service');
          			Result:=True;
      		end
      		else
      		begin
          			Log('The options that can be passed are' #13 '[ /a <upgrade(u)>]');
                Result := False;              
      		end;
	end
	else if SuppressibleMsgBox('The Volume Replication agent is existing in this machine.' #13 'Would you like to install the File Replication agent also?', mbConfirmation, MB_YESNO or MB_DEFBUTTON2,IDYES) = IDYES then
	begin
  		Log('Installing FX...' + Chr(13));
			FreshInstallationFX := 'IDYES';
			FreshFX := 'IDYES';
			ReinstallSelected := 'IDYES';
	end
	else
		Abort;
end;

(* This function is called when FX is already installed in the setup
This will add the VX agent
*)

function Fx_Available : Boolean;
begin
	if WizardSilent then
	begin
    		InitialSettings;
    		if a = 'u' then
    		begin
        			Log('Installing VX., ...' + Chr(13));
        			FreshInstallationVX := 'IDYES';
        			FreshVX := 'IDYES';
        			FreshVXAdd := 'IDYES';
        			ReinstallSelected := 'IDYES';
    		end
    		else if ((Uninstall = 'Y') or (Uninstall = 'y')) then
    			   Result:=True
    		else if ((UpdateConfig = 'Y') or (UpdateConfig = 'y')) then
    		begin
        			SuppressibleMsgBox('Stopping the services. It may take a while, please wait.', mbInformation, MB_OK, MB_OK);
        			CustomServiceStop('frsvc');
        			Result:=True;
    		end
    		else
    		begin
        			Log('The options that can be passed are' #13 '[ /a <upgrade(u)>]');
              Result := False;
    		end;
	end
	else if SuppressibleMsgBox('The File Replication agent is existing in this machine.' #13 'Would you like to install Volume Replication agent also?', mbConfirmation, MB_YESNO or MB_DEFBUTTON2,IDYES) = IDYES then
	begin
		Log('Adding VX., ...' + Chr(13));
		FreshInstallationVX := 'IDYES';
		FreshVX := 'IDYES';
		FreshVXAdd := 'IDYES';
		ReinstallSelected := 'IDYES';
	end
	else
		Abort;
end;


function DeleteIORE(): Boolean;
var
	InstallDir,StartMenuName : String;
begin
    if RegQueryStringValue(HKLM,'Software\SV Systems\VxAgent','InstallDirectory',InstallDir) then
    begin
            if DirExists(InstallDir) then
            begin
                DelTree(InstallDir+'\InMageObjectRecovery', True, True, True);
                if  RegKeyExists(HKLM,'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\InMage Product_is1') then		
    					  begin
    					       if RegQueryStringValue(HKLM,'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\InMage Product_is1','Inno Setup: Icon Group',StartMenuName) then
    					       begin
    					           		if DeleteFile(ExpandConstant('{commonprograms}')+'\'+StartMenuName+'\Unified Agent\InMageObjectRecovery.lnk') then
    					           		    Log('Deleted the InMageObjectRecovery shortcut from startmenu.');
    					       end;
                end;
           end;
     end;
end;

procedure IsCurrentAppRunning();
begin
     if (not (CheckForMutexes('UnifiedAgentMutexName'))) then
           CreateMutex('UnifiedAgentMutexName')            
     else
     begin
          While(CheckForMutexes('UnifiedAgentMutexName')) do
          begin
              if WizardSilent then
              begin
                   Log('Another instance of this appliccation is already running! Hence, Aborting..');
                   Log('Installation aborted with exit code 41.');
                   ExitInstallationWithoutStartServices();
                   ExitProcess(41);
              end
              else if SuppressibleMsgBox('An instance of installer is already running. To run installer again, you must first close existing installer process.',mbError, MB_OKCANCEL or MB_DEFBUTTON1, MB_OK) = IDCANCEL then
                   Log('Installation aborted with exit code 41.');
                   ExitProcess(41); 
          end;
          CreateMutex('UnifiedAgentMutexName')
     end;   
end;

(*Procedure VssProvider_UnInstall_DuringUpgrade_Log();
var
	LogFileString : AnsiString;
begin
    	if FileExists(ExpandConstant('{code:Getdirvx}\InMageVSSProvider_UnInstall_DuringUpgrade.log')) then	
	    begin
	       	LoadStringFromFile(ExpandConstant ('{code:Getdirvx}\InMageVSSProvider_UnInstall_DuringUpgrade.log'), LogFileString);    
	        Log(#13#10 +'InMageVSSProvider_UnInstall_DuringUpgrade log Start here '+ #13#10 + '*********************************************');
	        Log(#13#10 + LogFileString);
	        Log(#13#10 + '*********************************************' + #13#10 + 'InMageVSSProvider_UnInstall_DuringUpgrade log End here :' +#13#10 + #13#10);
	    end;
end;


function UninstallVssProviderDuringUpgrade() : Boolean;
var
    VSSProvider_UnInstall_DuringUpgrade : String;
    ErrorCode : Integer;
    OldState: Boolean;
begin
      VSSProvider_UnInstall_DuringUpgrade := ExpandConstant ('{code:Getdirvx}\InMageVSSProvider_Uninstall.cmd');
      if FileExists(VSSProvider_UnInstall_DuringUpgrade) then
      begin
            if IsWin64 then
      	    begin
      		        //Turn off redirection, so that cmd.exe from the 64-bit System directory is launched.
      		        OldState := EnableFsRedirection(False);
      		        try
            	         Exec(ExpandConstant('{cmd}'), '/c "' + VSSProvider_UnInstall_DuringUpgrade + '"  > %systemdrive%\InMageVSSProvider_UnInstall_DuringUpgrade.log','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
      		        finally
      			           // Restore the previous redirection state.
      			      EnableFsRedirection(OldState);
      	       	  end;
      	    end
      	    else
          	      Exec(ExpandConstant('{cmd}'), '/c "' + VSSProvider_UnInstall_DuringUpgrade + '"  > %systemdrive%\InMageVSSProvider_UnInstall_DuringUpgrade.log','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
             
             
             RenameFile(ExpandConstant('{sd}\InMageVSSProvider_UnInstall_DuringUpgrade.log'),ExpandConstant('{code:Getdirvx}\InMageVSSProvider_UnInstall_DuringUpgrade.log'));
      	     VssProvider_UnInstall_DuringUpgrade_Log;
     end;
end;
*)
//Checking whether the O.S is "windows-XP-64 bit.
function CheckWindowsXP64 : Boolean;
var
  Version: TWindowsVersion;
	OSName : String;
begin
    GetWindowsVersionEx(Version);

    if RegValueExists(HKLM, 'SOFTWARE\Microsoft\Windows NT\CurrentVersion', 'ProductName') then
    begin
        RegQueryStringValue(HKLM,'SOFTWARE\Microsoft\Windows NT\CurrentVersion','ProductName',OSName);
        if ( (pos('XP',OSName)>0) and ( (IsWin64) or (IsItanium64) ) ) then
        begin    
        		if ( (Version.NTPlatform) and (Version.Major = 5) and (Version.Minor = 1) and (Version.Build = 2600) ) or ( (Version.NTPlatform) and (Version.Major = 5) and (Version.Minor = 2) and (Version.Build = 3790) )then
                  Result := True
            else
            			Result := False;
        end;
     end;
end;


//Checking whether the O.S is "windows-2003(Without any Service Packs)

function CheckWindows2003 : Boolean;
var
   	Version: TWindowsVersion;
begin
    GetWindowsVersionEx(Version);

    if (Version.NTPlatform) and (Version.Major = 5) and (Version.Minor = 2) and (Version.Build = 3790) and (not (Version.ServicePackMajor > 1)) then
      	Result := True
    else
    		Result := False;
end;

//Appending the flush_registry binaries output to Main Log

Procedure Reg_flush_log();
var
	LogFileString : AnsiString;
begin
    	if FileExists(ExpandConstant('{code:Getdirvx}\reg_flush.log')) then	
	    begin
	       	LoadStringFromFile(ExpandConstant ('{code:Getdirvx}\reg_flush.log'), LogFileString);    
	        Log(#13#10 +'Registry flush log Starts here '+ #13#10 + '*********************************************');
	        Log(#13#10 + LogFileString);
	        Log(#13#10 + '*********************************************' + #13#10 + 'Registry flush log ends here :' +#13#10 + #13#10);
	    end;
end;

//Initially checking the O.S is WIN2K3-32 Bit(Without Service-Packs)
//then Executing the flush_registry.exe

Function Run_reg_flush() : Boolean;
var
	OSName,reg_flush_path : String;
	ErrorCode : Integer;
begin
    Result := True;	
    reg_flush_path := ExpandConstant('{code:Getdirvx}\flush_registry.exe');
    if RegValueExists(HKLM, 'SOFTWARE\Microsoft\Windows NT\CurrentVersion', 'ProductName') then
    begin
        RegQueryStringValue(HKLM,'SOFTWARE\Microsoft\Windows NT\CurrentVersion','ProductName',OSName);
        if ( (pos('2003',OSName)>0) and (CheckWindows2003) and (not(IsWin64)) ) then
        begin
	          Exec(ExpandConstant('{cmd}'), '/c "' + reg_flush_path + '"  > %systemdrive%\reg_flush.log','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
	          if FileExists(ExpandConstant('{code:Getdirvx}\reg_flush.log')) then
           	begin
            		DeleteFile(ExpandConstant('{code:Getdirvx}\reg_flush.log')); 
           	end;
	          if fileexists(ExpandConstant('{sd}\reg_flush.log')) then
	          begin
    	          RenameFile(ExpandConstant('{sd}\reg_flush.log'),ExpandConstant('{code:Getdirvx}\reg_flush.log'));
                Reg_flush_log;   
            end; 
        end;
    end; 
end;

procedure NonLatestVersionExitStatus();
begin
    FileCopy(ExpandConstant ('{log}'), ExpandConstant ('{sd}\InstallLogFile.log'), FALSE);
    LoadStringFromFile(ExpandConstant ('{sd}\InstallLogFile.log'), LogFileString);    
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),'Installation Starts here '+ Installation_StartTime + #13#10 + '*********************************************', True);
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),#13#10 + LogFileString , True);
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'), #13#10 + '*********************************************' + #13#10 + 'Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
    DeleteFile(ExpandConstant('{sd}\InstallLogFile.log'));
end;

procedure LatestVersionExitStatus();
var
CSIP, CSPORT : String;
begin
    InitialSettings;
    CSIP := GetIniString( 'vxagent.transport', 'Hostname','', ExpandConstant(GetVXInstallDir('0') +'\Application Data\etc\drscout.conf') );
    CSPORT := GetIniString( 'vxagent.transport', 'port','', ExpandConstant(GetVXInstallDir('0') +'\Application Data\etc\drscout.conf') );
    if (CSIP = CS_IP_Value) and (CSPORT = CS_Port_Value) then
    begin
        FileCopy(ExpandConstant ('{log}'), ExpandConstant ('{sd}\InstallLogFile.log'), FALSE);
        LoadStringFromFile(ExpandConstant ('{sd}\InstallLogFile.log'), LogFileString);    
        SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),'Installation Starts here '+ Installation_StartTime + #13#10 + '*********************************************', True);
        SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),#13#10 + LogFileString , True);
        SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),'Same version of the agent is already installed in the setup and pointed to same Configuration Server IP and Port.', True);
        SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),'Installation aborted with exit code 12.', True);
        SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'), #13#10 + '*********************************************' + #13#10 + 'Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
        DeleteFile(ExpandConstant('{sd}\InstallLogFile.log'));	
        ExitProcess(12);
    end
    else
    begin
        FileCopy(ExpandConstant ('{log}'), ExpandConstant ('{sd}\InstallLogFile.log'), FALSE);
        LoadStringFromFile(ExpandConstant ('{sd}\InstallLogFile.log'), LogFileString);    
        SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),'Installation Starts here '+ Installation_StartTime + #13#10 + '*********************************************', True);
        SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),#13#10 + LogFileString , True);
        SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),'Same version of the agent is already installed in the setup and pointed to different Configuration Server IP and Port.', True);
        SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),'Installation aborted with exit code 13.', True);
        SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'), #13#10 + '*********************************************' + #13#10 + 'Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
        DeleteFile(ExpandConstant('{sd}\InstallLogFile.log'));	
        ExitProcess(13);
    end;
end;

(* The function InitializeSetup will be called as the first step of the Installation if this function exists
1. sshd service will be available if openssh is already installed in the setup. If the service exists, we are stopping the service
*)

function InitializeSetup(): Boolean;
var
	PrevInstallPath : String;
	versionfx : String;
	ResultCode : Integer;
	FXStopStatus : Boolean;
begin

	 Installation_StartTime := GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')

	Log('In function InitializeSetup' + Chr(13));
	Result := True;

  if not DirExists(ExpandConstant('{sd}\ProgramData\ASRSetupLogs')) then
  begin
      Log('Creating ASRSetupLogs directory.');
      if not CreateDir(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs')) then
          Log('Unable to create ASRSetupLogs directory.')
      else
          Log('Successfully created ASRSetupLogs directory.');
  end
  else
      Log('ASRSetupLogs directory is already available.');

  InitialSettings;
  // Checks whether the current application is running or not.
  IsCurrentAppRunning;  
  DisplayHelpPage
 	// Checks whether the O.S is supported or not and abort if its an unsupported O.S
	OsCheck

	// Checks whether it is a Win2k operating system or not and Abort if it is Win2k
	Verifywin2k
  // Checks whether it is Itanium processor or not and Abort if it is Itanium
  if IsItanium64 then
  begin
        SuppressibleMsgBox('Agent installation is not supported on Itanium based systems.',mbError, MB_OK, MB_OK);
    		Log('Installation aborted with exit code 43.');
        ExitInstallationWithoutStartServices();
        ExitProcess(43);       		      
  end;	
	AbortIfWrongRoleSelected
  
  ValidateHostName_Silent		
	// Checks whether the higher version is already installed or not and aborts if it is installed.
  if CheckHigherVersion then
  begin
        SuppressibleMsgBox('Aborting installation as a higher version of the Microsoft Azure Site Recovery Mobility Service/Master Target Server is existing in this machine.' , mbError, MB_OK, MB_OK);
        Log('Installation aborted with exit code 11.');
        NonLatestVersionExitStatus;
        ExitProcess(11);
  end;
	
	// Verify whether it is a Cluster machine and MSDTC is installed on cluster machine,if not it aborts.
	VerifyCluster
	

	if IsServiceRunning('sshd') then
	begin
	       Log('SSHD Service is in running state')
      	 if CustomStopService('sshd') then
          	 Log('SSHD Service stopped successfully in InitializeSetup function.')
         else
             Log('Unable to stop SSHD Service in InitializeSetup function..')
  end;

  // Aborting the installation if involflt references are found.
	if ((IsServiceInstalled('involflt')) and (not (RegKeyExists(HKEY_LOCAL_MACHINE,'System\CurrentControlSet\Services\involflt')))) then
  begin
      SuppressibleMsgBox('Found references of involflt driver. Please reboot the system to remove the remnant references and re-try the installation.', mbError, MB_OK, MB_OK);
      Log('Installation aborted with exit code 65.');
      NonLatestVersionExitStatus;
      ExitProcess(65);
  end;
  
  // 	Aborting the installation if InDskFlt references are found.	
  if ((IsServiceInstalled('InDskFlt')) and (not (RegKeyExists(HKEY_LOCAL_MACHINE,'System\CurrentControlSet\Services\InDskFlt')))) then
  begin
      SuppressibleMsgBox('Found references of InDskFlt driver. Please reboot the system to remove the remnant references and re-try the installation.', mbError, MB_OK, MB_OK);
      Log('Installation aborted with exit code 66.');
      NonLatestVersionExitStatus;
      ExitProcess(66);   
  end;

  if ((not IsServiceInstalled('InDskFlt')) and (RegKeyExists(HKEY_LOCAL_MACHINE,'System\CurrentControlSet\Services\InDskFlt'))) then
  begin
      Log('Found references of InDskFlt registry from services key. deleting them.');
      if RegDeleteKeyIncludingSubkeys(HKLM,'System\CurrentControlSet\Services\InDskFlt') then
          Log('The InDskFlt registry entry is deleted successfuly from services key.')
      else
          Log('Unable to delete InDskFlt registry entry from services key.');
  end
  else
      Log('Not found references of InDskFlt registry from services key.');

	if VerifyNewVersion() then
	begin
		if (RegQueryStringValue(HKLM, 'Software\SV Systems\FileReplicationAgent', 'InstallDirectory', PrevInstallPath) and IsServiceInstalled('frsvc')) and (RegValueExists(HKLM,'Software\SV Systems\VxAgent','VxAgent') and (IsServiceInstalled('svagents'))) then
		begin
			if not Fx_Vx_Available then
			begin
			     LatestVersionExitStatus;
			end;
		end
		else if (RegValueExists(HKLM,'Software\SV Systems\VxAgent','VxAgent') and (IsServiceInstalled('svagents'))) then
		begin
			if not Vx_Available then
			begin
			     LatestVersionExitStatus;
			end;
		end
		else if(RegQueryStringValue(HKLM, 'Software\SV Systems\FileReplicationAgent', 'InstallDirectory', PrevInstallPath) and IsServiceInstalled('frsvc')) then
		begin
			if not Fx_Available then
			begin
			     LatestVersionExitStatus;
			end;
		end
		else
		begin
           LatestVersionExitStatus;
		end;
	end;
	  //CS Passphrase validation
  if not (IsServiceInstalled('svagents') and IsServiceInstalled('frsvc')) then
  begin
      if (not IsCXInstalled) then
      begin
          if DirExists(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')) then
          begin
            	Log(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')+' directory is available. Deleting it.');
            	if DelTree(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery'), True, True, True) then
            	begin
            	     Log('Successfully deleted the '+ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')+' path.');
            	     Result := True;
              end
            	else
            	begin      	     
            	     SuppressibleMsgBox('Found pre-existing "'+ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')+'" folder and installer is unable to remove it. Please remove it manually and re-try the installation.',mbError, MB_OK, MB_OK);
            	     Log('Installation aborted with exit code 15.');
            	     ExitInstallationWithoutStartServices();
                   ExitProcess(15);
              end;
          end
          else
          begin
              Log(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')+' directory is not available.');
          end;
      end
      else
      begin
          Log('Not removing the '+ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')+' path as Configuration/Process Server is already installed on this machine.'); 
      end;
        
      if (not IsCXInstalled) then
          PassphraseValidation;
	end;
  if WizardSilent then
  begin
      InitialSettings;
      if RoleSelectedInSilentInstall then
      begin
        if (role = 'vconmt')  then
        begin
          if not IsCXInstalled then
          begin
            RestartRequired;
          end;
        end;
      end
      else
          RestartRequired;
  end;
  
	if VerifyExistingVersion then
	begin
		DrvStatus := 'IDYES';
		if (RegQueryStringValue(HKLM, 'Software\SV Systems\FileReplicationAgent', 'InstallDirectory', PrevInstallPath) and IsServiceInstalled('frsvc')) and (RegValueExists(HKLM,'Software\SV Systems\VxAgent','VxAgent') and (IsServiceInstalled('svagents'))) then
		begin
			RegQueryStringValue(HKLM, 'Software\SV Systems\FileReplicationAgent', 'FxVersion', versionfx);
			if WizardSilent then
			begin
    				InitialSettings;
    				if a = 'u' then
    				begin
            					if IsServiceInstalled('tmansvc') then
            					begin
            						Exec(ExpandConstant('{code:Getdircx}\bin\stop_services.bat'), '', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
            						Sleep(10000);
            					end;
            					Log('upgrading the agents ' + Chr(13));
            					FxInstalled := 'IDYES';
            					VxInstalled := 'IDYES';
            					ReinstallSelected := 'IDYES';
            					SuppressibleMsgBox('Stopping the services. It may take a while, please wait.', mbInformation, MB_OK, MB_OK);
            					Result := CustomServiceStop('svagents');
            					Result := CustomServiceStop('InMage Scout Application Service');
            					FXStopStatus := CustomServiceStop('frsvc');
            					Result := CustomServiceStop('cxprocessserver');
            					//UninstallVssProviderDuringUpgrade;
            						if not FXStopStatus then
            						begin
            							Log('FX Services are not stopped during upgrade' + Chr(13));
            							StartCXServices;
            							Result := False;
            						end;
            
            						 vsnapcheck;
                         DeleteIORE;
            						 ChangeDrScoutValue;
            end
    				else
    				begin
          					Log('The options that can be passed are' #13 '[ /a <upgrade(u)>]' + Chr(13));
          					Log('Installation aborted with exit code 51.');
                    NonLatestVersionExitStatus;
                    ExitProcess(51);
    				end;
			end
			else if SuppressibleMsgBox('Microsoft Azure Site Recovery Mobility Service/Master Target Server is already installed  on this Server. Would you like to upgrade?', mbConfirmation, MB_YESNO or MB_DEFBUTTON2,IDYES) = IDYES then
			begin
			  Log('upgrading the agents ' + Chr(13));
				if IsServiceInstalled('tmansvc') then
				begin
					if MsgBox('Configuration Server is existing in this machine. The Configuration Server services need to be stopped to continue.',mbConfirmation,MB_YESNO) = IDYES then
					begin
						Exec(ExpandConstant('{code:Getdircx}\bin\stop_services.bat'), '', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
						Sleep(10000);
						ReinstallSelected := 'IDYES';
						FXStopStatus := CustomServiceStop('frsvc');

						if not FXStopStatus then
						begin
              Log('FX Services are not stopped during upgrade.Aborting the installation' + Chr(13));
							StartCXServices;
							Result := False;
						end;
					end
					else
					Result := False;
				end
				else
				begin
					// Upgrade
					FxInstalled := 'IDYES';
					VxInstalled := 'IDYES';
					ReinstallSelected := 'IDYES';
					Log('upgraded the agents' + Chr(13));
					Result := CustomServiceStop('svagents');
					Result := CustomServiceStop('frsvc');
					Result := CustomServiceStop('InMage Scout Application Service');
					Result := CustomServiceStop('cxprocessserver');
					//UninstallVssProviderDuringUpgrade;

					vsnapcheck;
          DeleteIORE;
					ChangeDrScoutValue;
				end;
			end
			else
				Result := False;
		end
		else if(RegQueryStringValue(HKLM, 'Software\SV Systems\FileReplicationAgent', 'InstallDirectory', PrevInstallPath) and IsServiceInstalled('frsvc')) then
		begin
			RegQueryStringValue(HKLM, 'Software\SV Systems\FileReplicationAgent', 'FxVersion', versionfx);
		// Upgrade
			if WizardSilent then
			begin
				InitialSettings;
				if a = 'ufx' then
				begin
					Log('upgrading the fx agents and adding vx' + Chr(13));
					if IsServiceInstalled('tmansvc') then
					begin
						//Msgbox('This is silent install',MbConfirmation, MB_OK);
						Exec(ExpandConstant('{code:Getdircx}\bin\stop_services.bat'), '', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
						Sleep(10000);
					end;
					FxInstalled := 'IDYES';
					ReinstallSelected := 'IDYES';
					SuppressibleMsgBox('Stopping the services. It may take a while, please wait.', mbInformation, MB_OK, MB_OK);
					FXStopStatus := CustomServiceStop('frsvc');
					//UninstallVssProviderDuringUpgrade;
					if not FXStopStatus then
					begin
						Log('FX Services are not stopped during upgrade.Aborting the installation' + Chr(13));
						StartCXServices;
						Result := False;
					end;
				end
				else if a = 'u' then
				begin
          Log('upgrading the fx agents and adding vx' + Chr(13));
					if IsServiceInstalled('tmansvc') then
					begin
						//Msgbox('This is silent install',MbConfirmation, MB_OK);
						Exec(ExpandConstant('{code:Getdircx}\bin\stop_services.bat'), '', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
						Sleep(10000);
					end;
					FxInstalled := 'IDYES';
					ReinstallSelected := 'IDYES';
					FreshInstallationVX := 'IDYES';
					FreshVX := 'IDYES';
					FreshVXAdd := 'IDYES';
					SuppressibleMsgBox('Stopping the services. It may take a while, please wait.', mbInformation, MB_OK, MB_OK);
					FXStopStatus := CustomServiceStop('frsvc');
					//UninstallVssProviderDuringUpgrade;
					if not FXStopStatus then
					begin
						Log('FX Services are not stopped during upgrade.Aborting the installation' + Chr(13));
						StartCXServices;
						Result := False;
					end;
				end
				else
				begin
						Log('The options that can be passed are' #13 '[ /a <upgrade(u)>]' + Chr(13));
					  Log('Installation aborted with exit code 51.');
            NonLatestVersionExitStatus;
            ExitProcess(51);
				end;
			end
			else if SuppressibleMsgBox('The File Replication Agent is existing in this machine. Would you like to upgrade it to higher version and also install the Volume Replication Agent?', mbConfirmation, MB_YESNO or MB_DEFBUTTON2,IDYES) = IDYES then
			begin
					Log('upgrading the agents' + Chr(13));
				if IsServiceInstalled('tmansvc') then
				begin
					if MsgBox('Configuration Server is existing in this machine. The Configuration Server services need to be stopped to continue.',mbConfirmation,MB_YESNO) = IDYES then
					begin
						Exec(ExpandConstant('{code:Getdircx}\bin\stop_services.bat'), '', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
						Sleep(10000);
						ReinstallSelected := 'IDYES';
						SuppressibleMsgBox('Stopping the services. It may take a while, please wait.', mbInformation, MB_OK, MB_OK);
						FXStopStatus := CustomServiceStop('frsvc');
						//UninstallVssProviderDuringUpgrade;

						if not FXStopStatus then
						begin
								Log('FX Services are not stopped during upgrade.Aborting the installation' + Chr(13));
							StartCXServices;
							Result := False;
						end;
					end
					else
						Result := False;
				end
				else
				begin
					FxInstalled := 'IDYES';
					ReinstallSelected := 'IDYES';
					SuppressibleMsgBox('Stopping the services. It may take a while, please wait.', mbInformation, MB_OK, MB_OK);
					Result := CustomServiceStop('frsvc');
					//UninstallVssProviderDuringUpgrade;
					FreshInstallationVX := 'IDYES';
					FreshVX := 'IDYES';
					FreshVXAdd := 'IDYES';
				end;
			end
			else
				Result := False;
		end
		// Upgrade
		// Check if VX is installed or not. If VX is installed, stop the VX services.
		else if (RegValueExists(HKLM,'Software\SV Systems\VxAgent','VxAgent') and (IsServiceInstalled('svagents'))) then
		begin
			if WizardSilent then
			begin
				InitialSettings;
				if a = 'uvx' then
				begin
					Log('upgrading the vx agents and adding fx' + Chr(13));
					VxInstalled := 'IDYES';
					ReinstallSelected := 'IDYES';
					SuppressibleMsgBox('Stopping the services. It may take a while, please wait.', mbInformation, MB_OK, MB_OK);
					Result := CustomServiceStop('svagents');
					Result := CustomServiceStop('InMage Scout Application Service');
					//UninstallVssProviderDuringUpgrade;
					if (Result) then
						Log('VX Services are stopped during upgrade' + Chr(13))
					else
						Log('VX Services are not stopped during upgrade' + Chr(13));
				end
				else if a = 'u' then
				begin
						Log('upgrading the vx agents and adding fx' + Chr(13));
					VxInstalled := 'IDYES';
					ReinstallSelected := 'IDYES';
					FreshInstallationFX := 'IDYES';
					FreshFX := 'IDYES';
					SuppressibleMsgBox('Stopping the services. It may take a while, please wait.', mbInformation, MB_OK, MB_OK);
					Result := CustomServiceStop('svagents');
					Result := CustomServiceStop('InMage Scout Application Service');
					//UninstallVssProviderDuringUpgrade;
					if (Result) then
							Log('VX Services are stopped during upgrade' + Chr(13))
					else
							Log('VX Services are not stopped during upgrade' + Chr(13));

					vsnapcheck;
          DeleteIORE;
					ChangeDrScoutValue;

				end
				else
				begin
						Log('The options that can be passed are' #13 '[ /a <upgrade(u)>]');
					  Log('Installation aborted with exit code 51.');
            NonLatestVersionExitStatus;
            ExitProcess(51);
				end;
			end
			else if SuppressibleMsgBox('The Volume Replication Agent is existing in this machine. Would you like to upgrade it to higher version and also install the File Replication Agent?', mbConfirmation, MB_YESNO,IDYES) = IDYES then
			begin
				VxInstalled := 'IDYES';
				ReinstallSelected := 'IDYES';
				SuppressibleMsgBox('Stopping the services. It may take a while, please wait.', mbInformation, MB_OK, MB_OK);
				Result := CustomServiceStop('svagents');
				Result := CustomServiceStop('InMage Scout Application Service');
				//UninstallVssProviderDuringUpgrade;

				vsnapcheck;
        DeleteIORE;
				ChangeDrScoutValue;

				FreshInstallationFX := 'IDYES';
				FreshFX := 'IDYES';
			end
			else
				Result:=False;
		end
		else
			Result := False;
	end
	else if UnsupportVersion then
	begin
		if (RegValueExists(HKLM,'Software\SV Systems\VxAgent','VxAgent'))  and (RegValueExists(HKLM,'Software\SV Systems\FileReplicationAgent','PROD_VERSION')) then
		begin
			RegQueryStringValue(HKLM,'Software\SV Systems\FileReplicationAgent','PROD_VERSION',FxVersionString);
			if WizardSilent then
			begin
  					Log('Found ' + FxVersionString + ' Agents version. Upgrade is not supported from this version. Aborting…' + Chr(13))
    				Log('Installation aborted with exit code 11.');
            NonLatestVersionExitStatus;
            ExitProcess(11);
			end
			else
			begin
				    MsgBox('Found ' + FxVersionString + ' Agents version. Upgrade is not supported from this version. Aborting…', mbInformation, MB_OK);
				    Log('Installation aborted with exit code 11.');
            NonLatestVersionExitStatus;
            ExitProcess(11);
			end;
		end
		else if RegValueExists(HKLM,'Software\SV Systems\VxAgent','PROD_VERSION') then
		begin
			RegQueryStringValue(HKLM,'Software\SV Systems\VxAgent','PROD_VERSION',VxVersionString);
			if WizardSilent then
			begin
  					Log('Found ' + VxVersionString + ' Agents version. Upgrade is not supported from this version. Aborting…' + Chr(13))
  				  Log('Installation aborted with exit code 11.');
            NonLatestVersionExitStatus;
            ExitProcess(11);
			end
			else
			begin
    				MsgBox('Found ' + VxVersionString + ' Agents version. Upgrade is not supported from this version. Aborting…', mbInformation, MB_OK);
    				Log('Installation aborted with exit code 11.');
            NonLatestVersionExitStatus;
            ExitProcess(11);
			end;
		end
		else  if RegValueExists(HKLM,'Software\SV Systems\FileReplicationAgent','PROD_VERSION') then
		begin
			RegQueryStringValue(HKLM,'Software\SV Systems\FileReplicationAgent','PROD_VERSION',FxVersionString);
			if WizardSilent then
			begin
					Log('Found ' + FxVersionString + ' Agents version. Upgrade is not supported from this version. Aborting…' + Chr(13));
				  Log('Installation aborted with exit code 11.');
          NonLatestVersionExitStatus;
          ExitProcess(11);
			end
			else
			begin
				    MsgBox('Found ' + FxVersionString + ' Agents version. Upgrade is not supported from this version. Aborting…', mbInformation, MB_OK);
				    Log('Installation aborted with exit code 11.');
            NonLatestVersionExitStatus;
            ExitProcess(11);
			end;
		end
		else
			Result := False;
	end
	else
	begin
		// Fresh install
		FreshInstallation := 'IDYES'
		if RegValueExists(HKLM, 'Software\SV Systems\VxAgent', 'ConfigPathname') then
		begin
		      if RegDeleteValue(HKLM,'Software\SV Systems\VxAgent','ConfigPathname') then
		          Log('The ConfigPathname registry entry is deleted successfuly from ''Software\SV Systems\VxAgent'' key' +  Chr(13));
		end;
		if RegValueExists(HKLM, 'Software\SV Systems', 'HostId') then
		begin
		      if RegDeleteValue(HKLM,'Software\SV Systems','HostId') then
		          Log('The HostId registry entry is deleted successfuly from ''HKLM\Software\SV Systems'' key' +  Chr(13));
		end;
		if IsCXInstalled then
		begin
			if WizardSilent then
			begin
				Log('A Scout CX Server installation is found on this system. Stopping the CX services..' + Chr(13));
				Exec(ExpandConstant('{code:Getdircx}\bin\stop_services.bat'), '', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
				Sleep(10000);
			end
			else if MsgBox('Configuration Server is existing in this machine. The Configuration Server services need to be stopped to continue.',mbConfirmation,MB_YESNO) = IDYES then
			begin
				Exec(ExpandConstant('{code:Getdircx}\bin\stop_services.bat'), '', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
				Sleep(10000);
			end
			else
				Result := False;
		end;
	end;
end;

function ReplaceLine(strFilename, strFind, strNewLine: String): Boolean;
var
	iLineCounter : Integer;
	a_strTextfile : TArrayOfString;
begin
	{ Load textfile into String array }
	LoadStringsFromFile(strFilename, a_strTextfile);

	for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do begin
		{ Overwrite textline when text searched for is part of it }
			StringChange((a_strTextfile[iLineCounter]),strFind, strNewLine);
    end;

	if SaveStringsToFile(strFilename, a_strTextfile, False) then
	   Result := True
	else
	   Result := False;
end;

procedure Replace_Runtime_Install_Path ;
var
FilePath,ConfFile : String;

begin
    	FilePath := ExpandConstant('{code:GetVXInstallDir}');
	    ConfFile := GetVXInstallDir('0') + '\Application Data\etc\drscout.conf';
      // Replacing the Run Time Install Path in drscout.conf file.
      if ReplaceLine(ConfFile,'RUNTIME_INSTALL_PATH',FilePath) then
           Log('Succesfully Replaced the "RUNTIME_INSTALL_PATH" with the installation path in the drscout.conf file.' + Chr(13))
      else
           Log('Failed to Replace the "RUNTIME_INSTALL_PATH" with the installation path in the drscout.conf file.' + Chr(13)); 	
end;

Procedure VssProviderLog();
var
	LogFileString : AnsiString;
begin
		if FileExists(ExpandConstant('{code:Getdirvx}\InMageVSSProvider_Install.log')) then	
	    begin
	  		LoadStringFromFile(ExpandConstant ('{code:Getdirvx}\InMageVSSProvider_Install.log'), LogFileString);    
	      Log(#13#10 +'InMageVSSProvider logs Start here '+ #13#10 + '*********************************************');
	      Log(#13#10 + LogFileString);
	      Log(#13#10 + '*********************************************' + #13#10 + 'InMageVSSProvider logs End here :' +#13#10 + #13#10);
	    end;
end;

procedure StartCXServicesAndReplaceLineInConfFile ;
var
	ResultCode: Integer;
	CacheDirectoryPath,ConfFile : String;
begin
    	CacheDirectoryPath := ExpandConstant('{code:GetVXInstallDir}\Application Data');
    	ConfFile := GetVXInstallDir('0') + '\Application Data\etc\drscout.conf';
    	if IsServiceInstalled('tmansvc') then
    	begin
    		Log('Starting the CX services.' + Chr(13));
    		StartService('W3SVC');
    		StartService('MySQL');
    		Exec(ExpandConstant('{code:Getdircx}\bin\start_services.bat'), '', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    	end;
    	
     	if IniKeyExists('vxagent','CacheDirectory',ConfFile) then
		      SetIniString('vxagent','CacheDirectory',CacheDirectoryPath,ConfFile)   	
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
		if (Pos(KeyString, a_strTextfile[iLineCounter]) > 0) then begin
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

function GetOrGenerateGUID(Param: String) : String;
var
	ConfigPathname,GUID_STRING,GUIDEntry,GUIDFile,getpart,ConfFile : String;
	iLineCounter,n : Integer;
	a_strTextfile : TArrayOfString;
	Result1 : Boolean;
begin
	if RegValueExists(HKLM,'Software\InMage Systems\Installed Products\9','Version') and IsServiceInstalled('tmansvc') then
	begin
		GUIDFile := ExpandConstant('{code:Getdircx}\etc\amethyst.conf');
		LoadStringsFromFile(GUIDFile, a_strTextfile);
		Result1 := False ;
		for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
		begin
			if (Pos('HOST_GUID', a_strTextfile[iLineCounter]) > 0) then
			begin
				SaveStringToFile(mcLogFile, 'HOST_GUID Value exists in Amethyst.conf ' + Chr(13), True);
				Result1 := True;
				GUIDEntry := a_strTextfile[iLineCounter] ;
				n := -1;
				while n <> 0 do
				begin
					n := Pos('=',GUIDEntry);  // Separator is ;
					if n = 0 then
						getpart := GUIDEntry
					else
						getpart := Copy(GUIDEntry,1,n - 1);
						// Add getpart to the array, separator-char is left out
						// Keep rest of string for further processing
					GUIDEntry := Copy(GUIDEntry,n+1,MaxInt);
				end;
					ConfFile := Trim(GUIDEntry);
					Result :=  RemoveQuotes(ConfFile) ;
					Log('The value of HOST_GUID is taken from CX ' + Result + Chr(13));
			end
		end
	end
	else if RegQueryStringValue(HKLM, 'Software\SV Systems\VxAgent', 'ConfigPathname', ConfigPathname) then
	begin
		if FileExists(ConfigPathname) then
		begin
		  Log('Getting the value of HOST_GUID from VX Agent  ' + Result + Chr(13));
		  Result := GetIniString('vxagent', 'HostId', HostID, ConfigPathname);
		end;
	end
	else if RegValueExists(HKLM,'Software\SV Systems\FileReplicationAgent','FxAgent') then
	begin
    if RegValueExists(HKLM,'Software\SV Systems','HostId') then
    begin
      RegQueryStringValue(HKLM, 'Software\SV Systems', 'HostId', GUID_STRING);
  		Result := GUID_STRING;
  		Log('Getting the value of HOST_GUID from FX Agent  ' + Result + Chr(13));
		end;
	end
	else
	begin
		GUID_STRING := GetHostId(Param);
		Result := GUID_STRING;
		Log('Generated the value of HOST_GUID:  ' + Result + Chr(13));
	end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if not WizardSilent then
  begin
      if CurPageID = wpSelectDir then 
      begin
          WizardForm.NextButton.Caption := 'Install';
      end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var StartMenuPath,str,FilePath,ConfFile,OldConfFile,NewConfFile,DataPoolDir,CdpexplorerBrandingConfFile, VSSProvider_Install, INSTALL_PATH : String;
HostGuid : String;
ErrorCode, Resultcode : Integer;
OldState: Boolean;
begin
	StartMenuPath := ExpandConstant('{group}');
	FilePath := ExpandConstant('{code:GetVXInstallDir}');
	ConfFile := GetVXInstallDir('0') + '\Application Data\etc\drscout.conf';
	OldConfFile := GetVXInstallDir('0') + '\Application Data\etc\drscout.conf.old';
	NewConfFile := GetVXInstallDir('0') + '\Application Data\etc\drscout.conf.new';

    if CurStep = ssInstall then
    begin
      if RenameFile(FilePath + '\patch.log', FilePath + '\patch.log.old') then
         Log('Successfully renamed patch.log to patch.log.old.');
      if FileCopy(FilePath + '\unins000.dat', FilePath + '\backup_unins000.dat', FALSE) then
         Log('Successfully copied unins000.dat to backup_unins000.dat.');
			if RenameFile(FilePath + '\Backup_Release', FilePath + '\Backup_Release_old') then
			   Log('Successfully renamed Backup_Release to Backup_Release_old.');
			if RenameFile(FilePath + '\backup_updates', FilePath + '\backup_updates_old') then
			   Log('Successfully renamed backup_updates to backup_updates_old.');
			
 	  // Fresh Installation directory.
		if FreshInstallation = 'IDYES' then
		begin
			Log('Installing UA ' + Chr(13));
			FreshInstallationFX := 'IDYES';
			FreshInstallationVX := 'IDYES';
		end
		else
		begin
			FileCopy(ConfFile,NewConfFile,False);
		    Log('Accessing the HostID value from the drscout.conf file.' + Chr(13));
		    HostID := GetIniString( 'vxagent', 'HostId','', ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') );
		    if DeleteFile(FilePath+'\exfailover.exe') then
		        Log('Successfully deleted exfailover.exe in upgrade.')
        else
            Log('Unable to delete exfailover.exe in upgrade.');
		end;
		if WizardSilent then
		begin
			InitialSettings;
				if ((Uninstall = 'Y') or (Uninstall = 'y')) then
				begin
					Exec(ExpandConstant('{code:Getdirvx}\unins000.exe'),'/VERYSILENT /SUPPRESSMSGBOXES','',SW_HIDE,ewWaitUntilTerminated, ErrorCode);
					Abort;
				end
				else if ((UpdateConfig = 'Y') or (UpdateConfig = 'y')) then
				begin
					UseCommandLineSettings;
					if (GetValue('Role') = 'MasterTarget') then
          begin
					   StartService('frsvc');
					end;
					
					StartService('InMage Scout Application Service');
					Abort;
				end;
		end;
 	end;

   if CurStep = ssPostInstall then
   begin
    Run_reg_flush	
    	if FileExists(ExpandConstant('{code:Getdirvx}\vacps.exe')) then
			begin
          ChangeReadOnlyAttribute(ExpandConstant('{code:Getdirvx}\vacps.exe'))
          if DeleteFile(ExpandConstant('{code:Getdirvx}\vacps.exe')) then
          begin
              Log('Successfully deleted vacps.exe from install path.')
          end
          else 
              Log('Unable to delete vacps.exe from install path.');
          
      end;
   	if FileExists(ExpandConstant('{code:Getdirvx}\backup_unins000.dat')) then
   	begin
    		if DeleteFile(ExpandConstant('{code:Getdirvx}\backup_unins000.dat')) then
       	   Log('Successfully deleted file backup_unins000.dat in sspostinstall step.');
   	end;
   	// Remove uninstall entries of VX/FX from Add/Remove Programs after installation of UA
	if RegKeyExists(HKLM,'Software\Microsoft\Windows\CurrentVersion\Uninstall\File Replication Agent_is1') then
	begin
			Log('Deleting the FX registry entry "File Replication Agent_is1"  from "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\File Replication Agent_is1" path ' + Chr(13));
		RegDeleteKeyIncludingSubkeys(HKLM,'Software\Microsoft\Windows\CurrentVersion\Uninstall\File Replication Agent_is1');
	end;

	if RegKeyExists(HKLM,'Software\Microsoft\Windows\CurrentVersion\Uninstall\Volume Agent_is1') then
	begin
  	Log('Deleting the VX registry entry "Volume Agent_is1"  from "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\File Replication Agent_is1" path ' + Chr(13));
		RegDeleteKeyIncludingSubkeys(HKLM,'Software\Microsoft\Windows\CurrentVersion\Uninstall\Volume Agent_is1');
	end;

    if DirExists(ExpandConstant('{code:Getdirvx}\Backup_Release_old')) then
		begin
			if DelTree(ExpandConstant('{code:Getdirvx}\Backup_Release_old'), True, True, True) then
			SaveStringToFile(mcLogFile, 'Successfully Deleted the Backup_Release_old folder from the installation path. '  +  Chr(13), True);
		end;
		
		if DirExists(ExpandConstant('{code:Getdirvx}\backup_updates_old')) then
		begin
			if DelTree(ExpandConstant('{code:Getdirvx}\backup_updates_old'), True, True, True) then
			SaveStringToFile(mcLogFile, 'Successfully Deleted the backup_updates_old folder from the installation path. '  +  Chr(13), True);
		end;
    
    if FileExists(StartMenuPath + '\Agent Configuration.lnk') then
		DeleteFile(StartMenuPath + '\Agent Configuration.lnk');

	if FileExists(StartMenuPath + '\vContinuum\Agent Configuration.lnk') then
        DeleteFile(StartMenuPath + '\vContinuum\Agent Configuration.lnk');

    if FileExists(StartMenuPath + '\Agent Documentation.lnk') then
		DeleteFile(StartMenuPath + '\Agent Documentation.lnk');
   	
	if IniKeyExists('vxagent','Version',ConfFile) then
		SetIniString('vxagent','Version','{#VERSION}', ConfFile);
  if IniKeyExists('vxagent','PROD_VERSION',ConfFile) then
  	SetIniString('vxagent','PROD_VERSION','{#PRODUCTVERSION}', ConfFile);
	
	VSSProvider_Install := ExpandConstant ('{code:Getdirvx}\InMageVSSProvider_Install.cmd');

	if IsWin64 then
	begin
		OldState := EnableFsRedirection(False);
		try
        Exec(ExpandConstant('{cmd}'), '/c "' + VSSProvider_Install + '"  > %systemdrive%\InMageVSSProvider_Install.log','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
		finally
        EnableFsRedirection(OldState);
		end;
	end
	else
		Exec(ExpandConstant('{cmd}'), '/c "' + VSSProvider_Install + '"  > %systemdrive%\InMageVSSProvider_Install.log','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
  RenameFile(ExpandConstant('{sd}\InMageVSSProvider_Install.log'),ExpandConstant('{code:Getdirvx}\InMageVSSProvider_Install.log'));
	INSTALL_PATH := ExpandConstant ('{code:GetVXInstallDir}');
	VssProviderLog;

  // MARS agent installation.
  if IsRoleSelected then
  begin
      if ((GetValuefromAmethyst('CX_TYPE') = '2') or (GetValuefromAmethyst('CX_TYPE') = '3')) then
      begin
          Log('Installing the MARS agent.');
          ShellExec('', ExpandConstant('{cmd}'),'/C "'+ExpandConstant('{code:Getdirvx}\MARSAgentInstaller.exe')+'" /q /nu','', SW_HIDE, ewWaitUntilTerminated,Errorcode);
          if (Errorcode=0) then
          begin
              Log('MARS agent installation is completed successfully.');
              if RegWriteStringValue(HKLM,'Software\InMage Systems\Installed Products\9','MARS_Install_Status','Succeeded') then
                  Log('''MARS_Install_Status'' success entry is updated in ''HKLM\Software\InMage Systems\Installed Products\9'' registry')
              else
                  Log('Unable to update ''MARS_Install_Status'' success entry in ''HKLM\Software\InMage Systems\Installed Products\9'' registry');
          end
          else
          begin
              SuppressibleMsgBox('Failed to install MARS agent.',mbError, MB_OK, MB_OK);
              if RegWriteStringValue(HKLM,'Software\InMage Systems\Installed Products\9','MARS_Install_Status','Failed') then
                  Log('''MARS_Install_Status'' failure entry is updated in ''HKLM\Software\InMage Systems\Installed Products\9'' registry')
              else
                  Log('Unable to update ''MARS_Install_Status'' failure entry in ''HKLM\Software\InMage Systems\Installed Products\9'' registry');
          end;
      end;
  end;

	if IsServiceInstalled('Azure Site Recovery VSS Provider') then
	begin
		if CustomStartService('Azure Site Recovery VSS Provider') then
		    Log('Azure Site Recovery VSS Provider service started successfully.')
    else
        Log('Unable to start Azure Site Recovery VSS Provider service.');
  end
	else
		SuppressibleMsgBox('Azure Site Recovery VSS Provider installation is failed. Please install it manually by running the command - ' + INSTALL_PATH + '\' + 'InMageVSSProvider_Install.cmd', mbInformation, MB_OK, MB_OK);

    if not NoReinstallCheck() then
    begin
     			Log('Setting the "UpdatedCX" value to "0" in drscout.conf file' + Chr(13));
      		SetIniInt('vxagent.upgrade','UpdatedCX',0,ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf'));
     			Log('Setting the "UpgradePHP" value to "/compatibility_update.php" in drscout.conf file' + Chr(13));
      		SetIniString('vxagent.upgrade','UpgradePHP','/compatibility_update.php',ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf'));
      		if  RegWriteDWordValue( HKLM, 'SOFTWARE\SV Systems\FileReplicationAgent', 'IsFxUpgraded', 1) then
              Log('The value -> ''1'' is successfully updated in IsFxUpgraded registry entry under ''HKLM\Software\SV Systems\FileReplicationAgent'' registry key.')
    end;
    if  (VolumeAgentConfiguration) then
		begin
		      SetIniString('vxagent','VirtualVolumeCompression','0',ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf'))
		      Log('Changing the value of ''VirtualVolumeCompression'' key from ''1'' to ''0'' in vxagent section during upgrade.');
		end;
          

    if FileExists(StartMenuPath + '\Agent Configuration.lnk') then
		DeleteFile(StartMenuPath + '\Agent Configuration.lnk');

	if FileExists(StartMenuPath + '\FX Agent Configuration.lnk') then
		DeleteFile(StartMenuPath + '\FX Agent Configuration.lnk');

    if FileExists(StartMenuPath + '\Agent Documentation.lnk') then
		DeleteFile(StartMenuPath + '\Agent Documentation.lnk');

    if WizardSilent then
    begin
          if FreshInstallationCheck then
		      begin
              if SetIniString( 'vxagent.transport', 'Https', '0', ExpandConstant('{code:Getdirvx}\Application Data\etc\drscout.conf') ) then
                   Log('The Https value -> ''0'' is successfully updated in the drscout.conf file.');
                
              if RegWriteDWordValue( HKLM, 'Software\SV Systems', 'Https', 0) then
                   Log('The Https value -> ''0'' is successfully updated in Https registry entry under ''HKLM\Software\SV Systems'' registry key.');
    		      
    		          UseCommandLineSettings;
		      end;
		end;

      if (IsRoleSelected) then
      begin  
         if (IsServiceInstalled('frsvc')) then
         begin
             if CustomStartService('frsvc') then
             Log('frsvc service is started succesfully during post installation.' + Chr(13))
             else
             Log('frsvc service is not started during post installation.' + Chr(13));
    		 end;
      end;

    if IsServiceInstalled('sshd') then
    begin
		    if CustomStartService('sshd') then
		       Log('sshd service is started succesfully in post installation.' + Chr(13))
		    else
		       Log('sshd service is not started in post installation.' + Chr(13));
    end;
		 
    if (IsServiceInstalled('svagents')) and (ServiceStatusString('svagents')='SERVICE_STOPPED') then
    begin
         if CustomStartService('svagents') then
         Log('svagents service is started succesfully in post installation.' + Chr(13))
         else
         Log('svagents service is not started in post installation.' + Chr(13));
    end
    else if (IsServiceInstalled('svagents')) and (ServiceStatusString('svagents')='SERVICE_RUNNING') then   
    begin   
         CustomStopService('svagents');   
         if StartService('svagents') then   
             Log('svagents service is re-started successfully.' + Chr(13));   
    end;
  	 	
		if (not IsCXInstalled) then
    begin
        if (IsServiceInstalled('cxprocessserver')) and (ServiceStatusString('cxprocessserver')='SERVICE_STOPPED') then
        begin
              if CustomStartService('cxprocessserver') then
              Log('cxprocessserver service is started succesfully in post installation.' + Chr(13))
              else
              Log('cxprocessserver service is not started in post installation.' + Chr(13));
        end;
    end;
		if IsWin2008 then
	  begin
    		//Sets the service ShellHWDetection to manual
        if CustomStopService('ShellHWDetection') then
       	begin
       	     Log('ShellHWDetection service is stopped successfully in post installation.' + Chr(13));
       	     ShellExec('', ExpandConstant('{cmd}'),'/C sc config "ShellHWDetection" start= demand','', SW_HIDE, ewWaitUntilTerminated,Errorcode);
       	end
       	else
       	     Log('ShellHWDetection service is not stopped successfully in post installation.' + Chr(13));
	  end;
    if not(IsRoleSelected) then
    begin
        if IsServiceRunning('frsvc') then
        begin
            if CustomServiceStop('frsvc') then
                Log('Stopped frsvc service successfully.')
            else
                Log('Unable to stop frsvc service.');
        end
        else
            Log('frsvc service is not running.');
        if ShellExec('', ExpandConstant('{cmd}'),'/C sc config "frsvc" start= demand','', SW_SHOW, ewWaitUntilTerminated,Errorcode) then
            Log('frsvc service is set to manual.')
        else
            Log('Unable to set frsvc service to manual.');
    end;
		
		if WizardSilent then
	  begin
      		Check_SetAppAgent_Command;
      		if (setappagent = 'manual') then
      		begin
        			if CustomStopService('InMage Scout Application Service') then
        			   Log('InMage Scout Application service is stopped successfully, when user has selected the manual option in silent installation.' + Chr(13))
        			else
        			   Log('InMage Scout Application service is notstopped successfully, when user has selected the manual option in silent installation.' + Chr(13));
        			if ShellExec('', ExpandConstant('{cmd}'),'/C sc config "InMage Scout Application Service" start= demand','', SW_SHOW, ewWaitUntilTerminated,Errorcode) then
        			   Log('InMage Scout Application service is set to manual')
      		end
      		else
      		begin
      		      if (IsServiceInstalled('InMage Scout Application Service')) then
      		      begin
          		      if CustomStartService('InMage Scout Application Service') then
            		       Log('InMage Scout Application service is started successfully,when user has not selected the manual option in silent installation.' + Chr(13))
            		    else
            		       Log('InMage Scout Application service is not started,when user has not selected the manual option in silent installation.' + Chr(13));
        		    end;
      		end; 
  	end
  	else
  	begin
	      if (IsServiceInstalled('InMage Scout Application Service')) and (ServiceStatusString('InMage Scout Application Service')='SERVICE_STOPPED') then
		    begin
		          if CustomStartService('InMage Scout Application Service') then
      		       Log('InMage Scout Application service is started successfully in interactive installation.' + Chr(13))
      		    else
      		       Log('InMage Scout Application service is not started in interactive installation.' + Chr(13));
		    end;   
	  end;

    StartCXServices;
    // Bug - 15804   
    if (IsServiceInstalled('InMage Scout Application Service')) and (ServiceStatusString('InMage Scout Application Service')='SERVICE_RUNNING') then   
    begin   
        CustomStopService('InMage Scout Application Service');   
        if StartService('InMage Scout Application Service') then   
        Log('InMage Scout Application service is re-started successfully.' + Chr(13));   
    end; 
    if (not IsCXInstalled) then
    begin
        if (IsServiceInstalled('cxprocessserver')) and (ServiceStatusString('cxprocessserver')='SERVICE_RUNNING') then   
        begin   
            Log('cxprocessserver service is running, re-starting the cxprocessserver service.');
            if CustomStopService('cxprocessserver') then
            begin
                Log('Stopped cxprocessserver service successfully.');
                if StartService('cxprocessserver') then   
                    Log('cxprocessserver service is re-started successfully.' + Chr(13))
                else
                    Log('Unable to re-start cxprocessserver service.');
            end
            else
            begin
                Log('Unable to stop cxprocessserver service.');
            end;
        end
        else
        begin
            Log('cxprocessserver service is not running.');
        end;
    end;

    Log('Verifying the MARS Agent service status.')
    if IsServiceRunning('obengine') then
    begin
        if RegWriteStringValue(HKLM,'Software\InMage Systems\Installed Products\9','MARS_Service_Status','Succeeded') then
            Log('''MARS_Service_Status'' entry is updated in ''HKLM\Software\InMage Systems\Installed Products\9'' registry')
        else
            Log('Unable to update ''MARS_Service_Status'' entry in ''HKLM\Software\InMage Systems\Installed Products\9'' registry');

        Log('MARS Agent service is running.');
    end
    else
    begin
        if RegWriteStringValue(HKLM,'Software\InMage Systems\Installed Products\9','MARS_Service_Status','Failed') then
            Log('''MARS_Service_Status'' entry is updated in ''HKLM\Software\InMage Systems\Installed Products\9'' registry')
        else
            Log('Unable to update ''MARS_Service_Status'' entry in ''HKLM\Software\InMage Systems\Installed Products\9'' registry');

        Log('MARS Agent service is not running.');
    end;


    Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('{code:Getdirvx}')+'" /inheritance:r /grant:r "BUILTIN\Administrators:(OI)(CI)F" /grant:r "NT AUTHORITY\SYSTEM:(OI)(CI)F"','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
    if (not(Errorcode=0)) then
        Log('Permissions changes are failed with error on install directory.')
    else
        Log('Permissions changes completed successfully on install directory.');
    Exec(ExpandConstant('{cmd}'), '/C icacls "'+ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery\private')+'" /inheritance:r /grant:r "BUILTIN\Administrators:(OI)(CI)F" /grant:r "NT AUTHORITY\SYSTEM:(OI)(CI)F"','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
    if (not(Errorcode=0)) then
        Log('Permissions changes are failed with error on {sd}\ProgramData\Microsoft Azure Site Recovery\private directory.')
    else
        Log('Permissions changes completed successfully on {sd}\ProgramData\Microsoft Azure Site Recovery\private directory.');
	end;
	
	if CurStep = ssDone then
	begin
    if IsRoleSelected then
    begin
        // Invoking cdpcli with --registermt as an argument.
        if (GetValuefromAmethyst('CX_TYPE') = '3') then
        begin
            RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\9','Container_Registration_Status',containerRegistrationStatus);
            if (containerRegistrationStatus = 'Succeeded') then
            begin
                Cdpcli_RegisterMT;
            end
            else
            begin
                Log('As the container registartion failed, not invoking cdpcli with --registermt as an argument.');
            end;
        end
        else if (GetValuefromAmethyst('CX_TYPE') = '2') then
        begin
            Cdpcli_RegisterMT;
        end;
    end;

		FileCopy(ExpandConstant ('{code:Getdirvx}\Application Data\etc\drscout.conf'), ExpandConstant ('{code:Getdirvx}\Application Data\etc\drscout.conf.initialbackup'), FALSE);
  
		if FileExists(ExpandConstant('{code:Getdirvx}\GetDataPoolSize\DataPoolSize.bat')) then
		begin
		  if DeleteFile(ExpandConstant('{code:Getdirvx}\GetDataPoolSize\DataPoolSize.bat')) then
			  Log('Successfully deleted the DataPoolSize.bat file from GetDataPoolSize folder at end of installation.' + Chr(13));		
		end;

		OkToCopyLog := True;
		str := ExpandConstant ('{code:Getdirvx}\reboot.bat');
		if ((FileExists(str)) and (issilent)) then
		begin
			SaveStringToFile(mcLogFile, 'Rebooting the system in silent installation.' + Chr(13), True);
			if IsWin64 then
			begin
				// Turn off redirection, so that cmd.exe from the 64-bit System
				// directory is launched.
				OldState := EnableFsRedirection(False);
				try
					Exec(ExpandConstant('{cmd}'), '/c "' + str + '" ','', SW_SHOW,ewNoWait, ErrorCode);
				finally
					// Restore the previous redirection state.
					EnableFsRedirection(OldState);
				end;
			end
			else
				ShellExec('',str,'', '', SW_SHOW, ewNoWait, ErrorCode);
		end;
		deletefile(OldConfFile);
		deletefile(NewConfFile);
		Log('Installation completed successfully.');
    if WizardSilent then
    begin
        Driver_Reboot := GetIniString( 'Reboot', 'Driver_Reboot','', ExpandConstant('{tmp}\Driver_Reboot.conf') );
        Driver_Reboot_Machine := GetIniString( 'Reboot', 'Driver_Reboot_Machine','', ExpandConstant('{tmp}\Driver_Reboot.conf') );
        if (Driver_Reboot = 'yes') or (Driver_Reboot_Machine = 'yes') then
        begin
            Log('Installation returned exit code 3010 in silent install as reboot is required.');
            ExitInstallationWithoutStartServices();
            ExitProcess(3010);
        end;
    end;
	end;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  { Skip pages that shouldn't be shown }
	if (PageID = wpSelectDir) and (not NoReinstallCheck) then
		Result := True
	else
		Result := False;
end;

function InitializeUninstall(): Boolean;
begin
      mcLogFile := ExpandConstant('{sd}\ProgramData\ASRSetupLogs\UA_UnInstallLogFile.log');
    	SaveStringToFile(mcLogFile, Chr(13), True);
      SaveStringToFile(mcLogFile, 'Uninstallation starts here : ' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10 + '*********************************************', True);
  	  SaveStringToFile(mcLogFile, Chr(13), True);
	    SaveStringToFile(mcLogFile, 'In function InitializeUninstall' + Chr(13), True);
	    DisplayUninstallHelp;
	    Result := True;
	    if RegKeyExists(HKLM, 'Software\InMage Systems\Installed Products\6')and IsServiceInstalled('svagents') then
      begin 
         if UninstallSilent then
         begin
           SaveStringToFile(mcLogFile,'Aborting uninstallation of Microsoft Azure Mobility Service/Master Target Server. Please uninstall vContinuum first and try uninstalling Master Target Server.'+ Chr(13),True);
           SaveStringToFile(mcLogFile,'Uninstallation aborted with exit code 1.'+ Chr(13),True);
           ExitFromUninstallProcess();
           ExitProcess(1);
         end
         else 
         begin
           SuppressibleMsgBox('Aborting uninstallation of Microsoft Azure Mobility Service/Master Target Server. Please uninstall vContinuum first and try uninstalling Master Target Server.',mbError, MB_OK, MB_OK);
           SaveStringToFile(mcLogFile,'The uninstallation can not proceed as vContinuum presents in this setup. Please uninstall vContinuum first and then re-try uninstallation of the Microsoft Azure Site Recovery Mobility Service/Master Target Server.'+ Chr(13),True);
           SaveStringToFile(mcLogFile,'Uninstallation aborted with exit code 1.'+ Chr(13),True);
           ExitFromUninstallProcess();
           ExitProcess(1);
         end;
      end;
end;

procedure DeinitializeSetup();
var 
Errorcode : Integer;
LogFileString : AnsiString;
begin
  Log('In procedure DeInitializeSetup' + Chr(13));
  if CheckHigherVersion then
     Abort;	
	//Creating the installation log in the system drive,this log gets appended every time the user runs the
	//setup.
	FileCopy(ExpandConstant ('{log}'), ExpandConstant ('{sd}\InstallLogFile.log'), FALSE);
  LoadStringFromFile(ExpandConstant ('{sd}\InstallLogFile.log'), LogFileString);    
  SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),'Installation Starts here '+ Installation_StartTime + #13#10 + '*********************************************', True);
  SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'),#13#10 + LogFileString , True);
  SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\UA_InstallLogFile.log'), #13#10 + '*********************************************' + #13#10 + 'Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
  DeleteFile(ExpandConstant('{sd}\InstallLogFile.log'));	
end;

procedure DeinitializeUninstall();
begin
	SaveStringToFile(mcLogFile, 'In function DeinitializeUninstall' + Chr(13), True);
	SaveStringToFile(mcLogFile, Chr(13), True);
	if (IsServiceInstalled('svagents')) and (ServiceStatusString('svagents')='SERVICE_STOPPED') then
		StartService('svagents');

	if (IsServiceInstalled('InMage Scout Application Service')) and (ServiceStatusString('InMage Scout Application Service')='SERVICE_STOPPED') then
		StartService('InMage Scout Application Service');
  if (GetValue('Role') = 'MasterTarget') then
  begin
    	if (IsServiceInstalled('frsvc')) and (ServiceStatusString('frsvc')='SERVICE_STOPPED') then
    		StartService('frsvc');
  end;	
  SaveStringToFile(mcLogFile, #13#10 + '*********************************************' + #13#10 + 'Un-Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
	SaveStringToFile(mcLogFile, Chr(13), True);
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
	MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
var
	S: String;
begin
	if not NoReinstallCheck then
	begin
		S := 'Upgradation will take place in the same location where';
		S := S + NewLine + 'the previous agent is installed.' + NewLine + NewLine;
		if  VolumeAgentInstall() then
		    S := S + 'For VX' +  '-' + ExpandConstant('{code:GetReadyMemoInstallDir}');
		if FileAgentInstall() then
		    S :=  S + NewLine + 'For FX' + '-' + ExpandConstant('{code:GetReadyMemoInstallDir}');
		Result := S;
	end
	else
	begin
		Result := MemoDirInfo;
	end;
end;

function UnregisterConfirmed(): Boolean;
begin
	if SuppressibleMsgBox('Would you like to unregister File Replication Agent from Configuration Server?', mbConfirmation, MB_YESNO,IDYES)=IDYES then
		Result := True
	else
		Result := False;
end;

procedure DeleteBitmapFiles();
var
Errorlevel : Integer;
	DriverLog : TArrayOfString;
	DriverLogFile,DriverLogLines : String ;
	iLineCounter : Integer;
begin
    if FileExists(ExpandConstant('{code:Getdirvx}\DeleteBitmapFiles.bat')) then
    begin
       SaveStringToFile(mcLogFile,'Invoking DeleteBitmapFiles.bat script.'+ Chr(13),True);
       ShellExec('', ExpandConstant('{code:Getdirvx}\DeleteBitmapFiles.bat'),'', '', SW_HIDE, ewWaitUntilTerminated, Errorlevel)
        if (Errorlevel=1) then 
            SaveStringToFile(mcLogFile,'Deleted Bitmap files successfully.'+ Chr(13),True)
        else if (Errorlevel=2) then
            SaveStringToFile(mcLogFile,'Failed to delete the bitmap files.'+ Chr(13),True);
    end;
   DriverLogFile := ExpandConstant('{code:Getdirvx}\DeletBitmapFiles.log');
   if FileExists(DriverLogFile) then
   begin
       LoadStringsFromFile(DriverLogFile, DriverLog);
	     DriverLogLines := '';
        for iLineCounter := 0 to GetArrayLength(DriverLog)-1 do
        DriverLogLines := DriverLogLines + DriverLog[iLineCounter] + #13#10;
    	  Log('drvutil commands starts here' + #13#10 + '*********************************************'+ #13#10 );
        Log(DriverLogLines);
        Log(#13#10 + '*********************************************' + #13#10 + 'drvutil commands Ends here' + #13#10);
   end;
end;


procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
	ResultCode, PosNum, ErrorCode: Integer;
	RegValueString,str,InstallDir: String;
	OldState: Boolean;
begin
	if CurUninstallStep=usUninstall then
	begin
	   SaveStringToFile(mcLogFile, 'Stopping the svagents,frsvc,InMage Scout Application Service and cxprocessserver services.' +  Chr(13), True);

     if (IsServiceRunning('svagents')) then
     begin
         // Using taskkill command to stop svagents service.
         SaveStringToFile(mcLogFile, 'Invoking taskkill command to stop svagents service.' +  Chr(13), True);
         ShellExec('', ExpandConstant('{cmd}'),'/C taskkill /im svagents.exe /f /t','', SW_HIDE, ewWaitUntilTerminated,Errorcode);
         if Errorcode=0 then
         begin
             SaveStringToFile(mcLogFile, 'Stopped the Volume Replication service successfully.' +  Chr(13), True);
         end
         else
         begin
             SuppressibleMsgBox('Unable to stop Volume Replication Service. Aborting the uninstallation.',mbError, MB_OK, MB_OK);
             Log('Uninstallation aborted with exit code 53.');
             ExitInstallationWithStartServices();
             ExitProcess(53);
         end;
     end
     else
        SaveStringToFile(mcLogFile, 'Volume Replication service is not running. Proceeding with uninstallation' +  Chr(13), True);

     UninstallStopServices('frsvc');
     UninstallStopServices('InMage Scout Application Service');
     if (not IsCXInstalled) then
     begin
        UninstallStopServices('cxprocessserver');
     end;

    // Uninstall MARS agent.
    if FileExists(ExpandConstant('{code:Getdirvx}\MARSAgentInstaller.exe')) then
    begin
        SaveStringToFile(mcLogFile, 'Uninstalling the MARS agent.' +  Chr(13), True);
        ShellExec('', ExpandConstant('{cmd}'),'/C "'+ExpandConstant('{code:Getdirvx}\MARSAgentInstaller.exe')+'" /q /d','', SW_HIDE, ewWaitUntilTerminated,Errorcode);
        if (Errorcode=0) then
        begin
            SaveStringToFile(mcLogFile, 'Uninstallation of MARS agent is completed successfully.' +  Chr(13), True);
        end
        else
        begin
            SaveStringToFile(mcLogFile, 'Failed to uninstall MARS agent.' +  Chr(13), True);
        end;
    end;

		DeleteBitmapFiles;
		SaveStringToFile(mcLogFile, 'Invoking firewall_remove.bat script.' +  Chr(13), True);
		ShellExec('',ExpandConstant('{code:Getdirvx}\firewall_remove.bat'),'','',SW_HIDE,ewWaitUntilTerminated, ResultCode);
		
		SaveStringToFile(mcLogFile, 'Invoking ServiceCtrl.exe.' +  Chr(13), True);
		Exec(ExpandConstant('{code:Getdirvx}\ServiceCtrl.exe'),'TrkWks restore','',SW_HIDE,ewWaitUntilTerminated, ResultCode);
		if ResultCode=0 then
			SaveStringToFile(mcLogFile, 'uninstalled ServiceCtrl.exe successfully ' +  Chr(13), True)
		else
			SaveStringToFile(mcLogFile, 'uninstallation of ServiceCtrl.exe failed ' +  Chr(13), True);
		
		SaveStringToFile(mcLogFile, 'Invoking cdpcli.exe.' +  Chr(13), True);
		Exec(ExpandConstant('{code:Getdirvx}\cdpcli.exe'),'--unhideall','',SW_HIDE,ewWaitUntilTerminated, ResultCode);
		if ResultCode=0 then
			SaveStringToFile(mcLogFile, 'uninstalled UnLockDrives.exe successfully ' +  Chr(13), True)
		else
			SaveStringToFile(mcLogFile, 'uninstallation of UnLockDrives.exe failed ' +  Chr(13), True);
		Exec(ExpandConstant('{code:Getdirvx}\cdpcli.exe'),'--virtualvolume --op=unmountall --checkfortarget=no','',SW_HIDE,ewWaitUntilTerminated, ResultCode);
		if ResultCode=0 then
			SaveStringToFile(mcLogFile, 'uninstalled all virtual volumes successfully ' +  Chr(13), True)
		else
			SaveStringToFile(mcLogFile, 'uninstallation of all virtual volumes failed ' +  Chr(13), True);
			
		SaveStringToFile(mcLogFile, 'Invoking appservice.exe.' +  Chr(13), True);
		Exec(ExpandConstant('{code:Getdirvx}\appservice.exe'),'-r','',SW_HIDE,ewWaitUntilTerminated, ResultCode);
		if ResultCode=0 then
			SaveStringToFile(mcLogFile, 'uninstalled appservice successfully ' +  Chr(13), True)
		else
			SaveStringToFile(mcLogFile, 'uninstallation of appservice failed ' +  Chr(13), True);
    
    if not IsCXInstalled then
    begin
        SaveStringToFile(mcLogFile, 'Invoking cxps.exe.' +  Chr(13), True);
        Exec(ExpandConstant('{code:Getdirvx}\transport\cxps.exe'),'uninstall','',SW_HIDE,ewWaitUntilTerminated, ResultCode);
        if ResultCode=0 then
          SaveStringToFile(mcLogFile, 'uninstalled cxps.exe successfully ' +  Chr(13), True)
        else
          SaveStringToFile(mcLogFile, 'uninstallation of cxps.exe failed ' +  Chr(13), True);
    end;
    SaveStringToFile(mcLogFile, 'Invoking svagents.exe.' +  Chr(13), True);
		Exec(ExpandConstant('{code:Getdirvx}\svagents.exe'),'/UnregServer','',SW_HIDE,ewWaitUntilTerminated, ResultCode);
		if ResultCode=0 then
			SaveStringToFile(mcLogFile, 'uninstalled svagents.exe successfully ' +  Chr(13), True)
		else
			SaveStringToFile(mcLogFile, 'uninstallation of svagents.exe failed ' +  Chr(13), True);


		// if UnregisterConfirmed then
		SaveStringToFile(mcLogFile, 'Invoking unregagent.exe.' +  Chr(13), True);
		Exec(ExpandConstant('{code:Getdirvx}\unregagent.exe'),'Vx Y','',SW_HIDE,ewWaitUntilTerminated, ResultCode);
		if ResultCode=0 then
			SaveStringToFile(mcLogFile, 'uninstalled unregagent.exe for vx successfully ' +  Chr(13), True)
		else
			SaveStringToFile(mcLogFile, 'uninstallation of unregagent.exe  for vx failed ' +  Chr(13), True);
			
		SaveStringToFile(mcLogFile, 'Invoking frsvc.exe.' +  Chr(13), True);
		Exec(ExpandConstant('{code:Getdirfx}\FileRep\frsvc.exe'),'/UnregServer','',SW_HIDE,ewWaitUntilTerminated, ResultCode);
		if ResultCode=0 then
			SaveStringToFile(mcLogFile, 'uninstalled frsvc.exe successfully ' +  Chr(13), True)
		else
			SaveStringToFile(mcLogFile, 'uninstallation of frsvc.exe  for fx failed ' +  Chr(13), True);

			//if UnregisterConfirmed then
		SaveStringToFile(mcLogFile, 'Invoking unregagent.exe.' +  Chr(13), True);
		Exec(ExpandConstant('{code:Getdirfx}\FileRep\unregagent.exe'),'filereplication Y','',SW_HIDE,ewWaitUntilTerminated, ResultCode);
		if ResultCode=0 then
			SaveStringToFile(mcLogFile, 'uninstalled unregagent.exe for fx successfully ' +  Chr(13), True)
		else
			SaveStringToFile(mcLogFile, 'uninstallation of  unregagent.exe for fx failed ' +  Chr(13), True);

		if RegValueExists(HKLM,'System\CurrentControlSet\Control\Class\{4d36e967-e325-11ce-bfc1-08002be10318}','UpperFilters') then
		begin
			RegQueryMultiStringValue(HKLM,'System\CurrentControlSet\Control\Class\{4d36e967-e325-11ce-bfc1-08002be10318}','UpperFilters',RegValueString);
			str := 'InDskFlt' + #0;
			PosNum := Pos(str,RegValueString);
			if PosNum<>0 then
			begin
				SaveStringToFile(mcLogFile, 'Deleting the InDskFlt String from upperfilterkey registry entry.' +  Chr(13), True);
				Delete(RegValueString,PosNum,9);
				RegWriteMultiStringValue(HKLM,'System\CurrentControlSet\Control\Class\{4d36e967-e325-11ce-bfc1-08002be10318}','UpperFilters',RegValueString );
			end;
		end;
		if RegKeyExists(HKLM,'System\CurrentControlSet\Services\InDskFlt') then
		begin
          if RegDeleteKeyIncludingSubkeys(HKLM,'System\CurrentControlSet\Services\InDskFlt') then
              SaveStringToFile(mcLogFile,'The InDskFlt registry entry is deleted successfuly from services key.' +  Chr(13), True)
          else
              SaveStringToFile(mcLogFile,'Unable to delete InDskFlt registry entry from services key.' +  Chr(13), True);
		end;
		
		if RegKeyExists(HKLM,'System\CurrentControlSet\Services\frsvc') then
		begin
		  	 if RegDeleteKeyIncludingSubkeys(HKLM,'System\CurrentControlSet\Services\frsvc') then
            SaveStringToFile(mcLogFile,'The frsvc registry entry is deleted successfuly from services key.' +  Chr(13), True);
		end;
		
		if RegKeyExists(HKLM,'System\CurrentControlSet\Services\svagents') then
		begin
		      if RegDeleteKeyIncludingSubkeys(HKLM,'System\CurrentControlSet\Services\svagents') then
		          SaveStringToFile(mcLogFile,'The svagents registry entry is deleted successfuly from services key.' +  Chr(13), True);
		end;
		
		if RegValueExists(HKLM, 'Software\SV Systems', 'DlmChecksum') then
		begin
		      if RegDeleteValue(HKLM,'Software\SV Systems','DlmChecksum') then
		          SaveStringToFile(mcLogFile,'The DlmChecksum registry entry is deleted successfuly from ''HKLM\Software\SV Systems'' key' +  Chr(13), True);
		end;
		
		if RegValueExists(HKLM, 'Software\SV Systems', 'DlmMbrFile') then
		begin
		      if RegDeleteValue(HKLM,'Software\SV Systems','DlmMbrFile') then
		          SaveStringToFile(mcLogFile,'The DlmMbrFile registry entry is deleted successfuly from ''HKLM\Software\SV Systems'' key' +  Chr(13), True);
		end;
		
		if RegValueExists(HKLM, 'Software\SV Systems', 'PartitionFile') then
		begin
		      if RegDeleteValue(HKLM,'Software\SV Systems','PartitionFile') then
		          SaveStringToFile(mcLogFile,'The PartitionFile registry entry is deleted successfuly from ''HKLM\Software\SV Systems'' key' +  Chr(13), True);
		end;
		
		if RegValueExists(HKLM, 'Software\SV Systems', 'Https') then
		begin
		      if RegDeleteValue(HKLM,'Software\SV Systems','Https') then
		          SaveStringToFile(mcLogFile,'The Https registry entry is deleted successfuly from ''HKLM\Software\SV Systems'' key' +  Chr(13), True);
		end;
		if RegValueExists(HKLM, 'Software\SV Systems\VxAgent', 'ConfigPathname') then
		begin
		      if RegDeleteValue(HKLM,'Software\SV Systems\VxAgent','ConfigPathname') then
		          SaveStringToFile(mcLogFile,'The ConfigPathname registry entry is deleted successfuly from ''Software\SV Systems\VxAgent'' key' +  Chr(13), True);
		end;
		if RegValueExists(HKLM, 'Software\SV Systems', 'HostId') then
		begin
		      if RegDeleteValue(HKLM,'Software\SV Systems','HostId') then
		          SaveStringToFile(mcLogFile,'The HostId registry entry is deleted successfuly from ''HKLM\Software\SV Systems'' key' +  Chr(13), True);
		end;
		
		if RegKeyExists(HKLM,'Software\SV Systems') then
		begin
		      if RegDeleteKeyIfEmpty(HKLM,'Software\SV Systems') then
		          SaveStringToFile(mcLogFile,'The SV Systems registry entry is deleted successfuly from ''HKLM\Software'' key' +  Chr(13), True);
		end;
		
    if RegKeyExists(HKLM,'SAM\SV Systems') then
		begin
		      if RegDeleteKeyIncludingSubkeys(HKLM,'SAM\SV Systems') then
		          SaveStringToFile(mcLogFile,'The SV Systems registry entry is deleted successfuly from ''HKLM\SAM'' key' +  Chr(13), True);
		end;

		if RegKeyExists(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\4') then
		begin
   			if RegDeleteKeyIncludingSubkeys(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\4') then
           SaveStringToFile(mcLogFile, 'Successfully Deleted the ''HKLM\Software\InMage Systems\Installed Products\4'' registry key.' +  Chr(13), True);
		end;
    
    if RegKeyExists(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\5') then
		begin
         if RegDeleteKeyIncludingSubkeys(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\5') then
            SaveStringToFile(mcLogFile, 'Successfully Deleted the ''HKLM\Software\InMage Systems\Installed Products\5'' registry key.' +  Chr(13), True);
		end;  
          
    
    if RegKeyExists(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products') then
		begin
		     if RegDeleteKeyIfEmpty(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products') then
		        SaveStringToFile(mcLogFile, 'Successfully Deleted the ''HKLM\Software\InMage Systems\Installed Products'' registry key.' +  Chr(13), True);
		end;
		
		if RegKeyExists(HKEY_LOCAL_MACHINE,'Software\InMage Systems') then
		begin
			   if RegDeleteKeyIfEmpty(HKEY_LOCAL_MACHINE,'Software\InMage Systems') then
			      SaveStringToFile(mcLogFile, 'Successfully Deleted the ''HKLM\Software\InMage Systems'' registry key.' +  Chr(13), True);
		end;
		
		DelTree(ExpandConstant('{code:Getdirvx}/Application Data'),False, True, True);
		
		if DirExists(ExpandConstant('{code:Getdirvx}\GetDataPoolSize')) then
    begin
    	   if DelTree(ExpandConstant('{code:Getdirvx}\GetDataPoolSize'), True, True, True) then
            SaveStringToFile(mcLogFile, 'Successfully Deleted the GetDataPoolSize directory from the installation path. ' +  Chr(13), True);    	   
    end;
 
		if DirExists(ExpandConstant('{code:Getdirvx}\Backup_Release')) then
		begin
			if DelTree(ExpandConstant('{code:Getdirvx}\Backup_Release'), True, True, True) then
			SaveStringToFile(mcLogFile, 'Successfully Deleted the Backup_Release folder from the installation path. '  +  Chr(13), True);
		end;
		
		if DirExists(ExpandConstant('{code:Getdirvx}\backup_updates')) then
		begin
			if DelTree(ExpandConstant('{code:Getdirvx}\backup_updates'), True, True, True) then
			SaveStringToFile(mcLogFile, 'Successfully Deleted the backup_updates folder from the installation path. '  +  Chr(13), True);
		end;
		if FileExists(ExpandConstant('{code:Getdirvx}\backup_unins000.dat')) then
		begin
		   if DeleteFile(ExpandConstant('{code:Getdirvx}\backup_unins000.dat')) then
		      SaveStringToFile(mcLogFile, 'Successfully deleted the file backup_unins000.dat.'  +  Chr(13), True);
		end;
		
		if not ShortcutNotExist then
		begin
		   if DeleteFile(ExpandConstant('{userdesktop}\hostconfigwxcommon.lnk')) then
		       SaveStringToFile(mcLogFile, 'Successfully deleted the hostconfigwxcommon.lnk desktop shortcut.'  +  Chr(13), True)
		   else
		       SaveStringToFile(mcLogFile, 'Unable to delete the hostconfigwxcommon.lnk desktop shortcut.'  +  Chr(13), True);
		end;
		
	end;
	if CurUninstallStep=usDone then
  Begin
	    if DirExists(ExpandConstant('{code:Getdirvx}')) then
  		begin
  			if DelTree(ExpandConstant('{code:Getdirvx}'), True, True, True) then
  			   SaveStringToFile(mcLogFile, 'Successfully deleted the '+ExpandConstant('{code:Getdirvx}')+' path.'  +  Chr(13), True)
  			else
  			   SaveStringToFile(mcLogFile, 'Unable to delete the '+ExpandConstant('{code:Getdirvx}')+' path. Please delete it manually.'  +  Chr(13), True)
  		end;
  		if not IsCXInstalled then
      begin
          if DirExists(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')) then
          begin
            if DelTree(ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery'), True, True, True) then
                 SaveStringToFile(mcLogFile, 'Successfully deleted the '+ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')+' path.' +  Chr(13), True)
            else
                 SaveStringToFile(mcLogFile, 'Unable to delete the '+ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')+' path. Please delete it manually.'  +  Chr(13), True)
          end;
      end
	else
	begin
			SaveStringToFile(mcLogFile, 'Not deleting the '+ExpandConstant('{sd}\ProgramData\Microsoft Azure Site Recovery')+' path as Configuration/Process Server is installed on this machine.'  +  Chr(13), True)
	end;
      Exec(ExpandConstant('{cmd}'), '/C mountvol %SystemDrive%\SRV /d','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
      DelTree(ExpandConstant('{sd}\SRV'), True, True, True);  		
	end;	
	if CurUninstallStep=usPostUninstall then
  Begin
      SuppressibleMsgBox('To complete the uninstallation of agent, reboot of machine is required. Optionally you could do it later point of time.', mbInformation, MB_OK, MB_OK);
      SaveStringToFile(mcLogFile, 'To complete the uninstallation of agent, reboot of machine is required. Optionally you could do it later point of time.' + Chr(13), True);
	end;
	
end;

function GetUpperfilterkey(Param: String) : String;
var
	PosNum: Integer;
	RegValueString: String;
begin
	if chkdrv = 'IDYES' then
	begin
		if RegValueExists(HKLM,'System\CurrentControlSet\Control\Class\{4d36e967-e325-11ce-bfc1-08002be10318}','UpperFilters') then
		begin
			RegQueryMultiStringValue(HKLM,'System\CurrentControlSet\Control\Class\{4d36e967-e325-11ce-bfc1-08002be10318}','UpperFilters',RegValueString);
			PosNum := Pos('InDskFlt',RegValueString);
			if PosNum=0 then
				Result := copy('InDskFlt',0,8);
		end
		else
			Result := 'InDskFlt';
	end
	else
	begin
		Log('Aborting installation as operating system of your machine is not supporting the installer.' + Chr(13));
		SuppressibleMsgBox('Aborting installation as operating system of your machine is not supporting the installer.',mbError, MB_OK, MB_OK);
    Log('Installation aborted with exit code 42.');
    ExitInstallationWithStartServices();
		ExitProcess(42);
	end;
end;
