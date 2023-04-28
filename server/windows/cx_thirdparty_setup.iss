; Installation script for Microsoft Azure Site Recovery Configuration/Process Server Dependencies

#include "..\..\host\setup\version.iss"
#include "..\..\host\setup\branding_parameters.iss"



#ifdef DEBUG
#define Configuration "Debug"
#else
#define Configuration "Release"
#endif

[Setup]
AppName=Azure Site Recovery Configuration/Process Server Dependencies
AppVerName=Microsoft Azure Site Recovery Configuration/Process Server Dependencies
AppVersion={#APPVERSION}
AppPublisher=Microsoft Corporation
DefaultDirName=C:\thirdparty
DefaultGroupName={#COMPANYNAME}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
OutputBaseFilename=cx_thirdparty_setup
OutputDir={#Configuration}
DirExistsWarning=no
DisableStartupPrompt=yes
uninstallable=yes
DisableReadyPage=yes

VersionInfoCompany=Microsoft Corporation
VersionInfoCopyright={#COPYRIGHT}
VersionInfoDescription=Microsoft Azure Site Recovery Configuration/Process Server Dependencies
VersionInfoProductName=Microsoft Azure Site Recovery
VersionInfoVersion={#APPVERSION}
VersionInfoProductVersion={#VERSION}

; The following entry disables the Select Destination Location wizard page. In this case, it will always use the default directory name.
DisableDirPage=Yes
; The following entry disables non-root installation
UninstallRestartComputer=yes
setuplogging=yes
ChangesEnvironment=yes
CloseApplications=force
DiskSpanning=yes
[LangOptions]
DialogFontName=Times New Roman
DialogFontSize=10
CopyrightFontName=Times New Roman
CopyrightFontSize=10
[Files]
Source: unzip_Files.vbs; DestDir: C:\Temp; Flags: ignoreversion;
Source: ..\packages\perl.5.8.8.4\strawberry-perl-5.8.8.4.zip; DestDir: C:\Temp; Flags: ignoreversion deleteafterinstall; Afterinstall: ExtractStrawberryPerl;
Source: ..\packages\php.7.1.10\php-7.1.10-nts-Win32-VC14-x86.zip; DestDir: C:\Temp; Flags: ignoreversion; Afterinstall: ExtractPHP;
Source: ..\packages\phpold.5.4.27\php-5.4.27-nts-Win32-VC9-x86.zip; DestDir: C:\Temp; Flags: ignoreversion;
Source: ..\tm\bin\clean_stale_process.pl ; destdir : {sd}; flags: dontcopy

Source: ..\..\thirdparty\server\DBI\*; DestDir: C:\Temp\DBI; Flags: ignoreversion;
Source: ..\..\thirdparty\server\DBI\MSWin32-x86-multi-thread-5.8\*; DestDir: C:\Temp\DBI\MSWin32-x86-multi-thread-5.8; Flags: ignoreversion;
Source: ..\..\thirdparty\server\DBD-mysql\*; DestDir: C:\Temp\DBD-mysql; Flags: ignoreversion;
Source: ..\..\thirdparty\server\DBD-mysql\MSWin32-x86-multi-thread-5.8\*; DestDir: C:\Temp\DBD-mysql\MSWin32-x86-multi-thread-5.8; Flags: ignoreversion;

Source: ..\..\thirdparty\server\Win32-0.39\*; DestDir: C:\Temp\Win32-0.39; Excludes: \CVS\*; Flags: ignoreversion  recursesubdirs;
Source: ..\..\thirdparty\server\Win32-OLE-0.1709\*; DestDir: C:\Temp\Win32-OLE-0.1709; Excludes: \CVS\*; Flags: ignoreversion  recursesubdirs;
Source: ..\..\thirdparty\server\Win32-Process-0.14\*; DestDir: C:\Temp\Win32-Process-0.14; Excludes: \CVS\*; Flags: ignoreversion  recursesubdirs;
Source: ..\..\thirdparty\server\Win32-Service-0.06\*; DestDir: C:\Temp\Win32-Service-0.06; Excludes: \CVS\*; Flags: ignoreversion  recursesubdirs;
Source: ..\packages\rrdtool.1.2.15\rrdtool-1.2.15-win32-perl58.zip; DestDir: C:\Temp\; Flags: ignoreversion; Afterinstall: ExtractRRD;
Source: ..\packages\cygwin.1.7.33\*; DestDir: C:\thirdparty\Cygwin\bin; Excludes: \CVS\*;Flags: ignoreversion;

; NOTE: Don't use "Flags: ignoreversion" on any shared system files
Source: installmod.bat; DestDir: C:\Temp\; Flags: ignoreversion deleteafterinstall; Afterinstall: InstallPerlModules;

[Messages]
SetupWindowTitle=%1
WelcomeLabel1=Welcome to the Microsoft Azure Site Recovery Configuration/Process Server Dependencies Setup Wizard
ReadyLabel1=Setup is now ready to begin installing Microsoft Azure Site Recovery Configuration/Process Server Dependencies on your computer.
FinishedHeadingLabel=Completing the Microsoft Azure Site Recovery Configuration/Process Server Dependencies Setup Wizard
FinishedLabelNoIcons=Setup has finished installing Microsoft Azure Site Recovery Configuration/Process Server Dependencies on your computer.
ClickFinish=Click Finish to exit Setup.
[UninstallDelete]
Name: C:\strawberry\perl; Type: filesandordirs
Name: C:\strawberry; Type: filesandordirs
Name: C:\thirdparty\php5nts; Type: filesandordirs
Name: C:\thirdparty; Type: filesandordirs
Name: C:\site; Type: filesandordirs
Name: C:\bin; Type: filesandordirs
Name: C:\thirdparty\rrdtool-1.2.15-win32-perl58; Type: filesandordirs
Name: C:\Temp; Type: filesandordirs

[Dirs]
Name : {sd}\Temp; Flags: uninsalwaysuninstall
#include "..\..\host\setup\Windows_Services.iss"

[INI]
; php.ini update
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: short_open_tag; String: On; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: output_buffering; String: On; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: max_input_time; String: 160; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: memory_limit; String: 750M; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: display_errors; String: Off; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: log_errors; String: off; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: variables_order; String: EGPCS; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: register_argc_argv; String: On; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: post_max_size; String: 1024M; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: magic_quotes_gpc; String: Off; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: upload_tmp_dir; String: C:\thirdparty\php5nts\uploadtemp; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: upload_max_filesize; String: 1024M; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: session.save_path; String: C:\thirdparty\php5nts\sessiondata; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: session.bug_compat_42; String: 1; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: max_execution_time; String: 600; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: fastcgi.impersonate; String: 1; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: fastcgi.logging; String: 0; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: cgi.fix_pathinfo; String: 1; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: cgi.force_redirect; String: 0; Check: not UpgradeCheck

Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: max_file_uploads; String: 500; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: max_input_vars; String: 4000; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP; Key: error_reporting; String: E_ALL & ~E_NOTICE & ~E_DEPRECATED; Check: not UpgradeCheck

Filename: C:\thirdparty\php5nts\php.ini; Section: PHP_LDAP; Key: extension; String: C:\thirdparty\php5nts\ext\php_ldap.dll; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP_CURL; Key: extension; String: C:\thirdparty\php5nts\ext\php_curl.dll; Check: not UpgradeCheck
Filename: C:\thirdparty\php5nts\php.ini; Section: PHP_MYSQLI; Key: extension; String: C:\thirdparty\php5nts\ext\php_mysqli.dll; Check: not UpgradeCheck 

[Run]
Filename: C:\strawberry\perl\bin\ppm.bat; Parameters: install C:\Temp\DBD-mysql\DBD-mysql.ppd; Flags: shellexec waituntilterminated runhidden;
Filename: C:\strawberry\perl\bin\ppm.bat; Parameters: install C:\Temp\DBI\DBI.ppd; Flags: shellexec waituntilterminated runhidden;


[UninstallRun]
Filename: C:\strawberry\perl\bin\ppm.bat; Parameters: uninstall C:\Temp\DBD-mysql\DBD-mysql.ppd; Flags: shellexec waituntilterminated runhidden
Filename: C:\strawberry\perl\bin\ppm.bat; Parameters: uninstall C:\Temp\DBI\DBI.ppd; Flags: shellexec waituntilterminated runhidden
Filename: C:\Temp\reset_path.bat; Flags: shellexec runhidden
Filename: del C:\Temp\reset_path.bat

[Code]
var
	path1,Message,str,UpgradeCX,FileName : AnsiString;
	PosNum,PosNum1 : Integer;
	OSName,versionname : String;
	ResultCode : Integer;
	Version: TWindowsVersion;
	GetOSVersion,mcLogFile,Installation_StartTime : String;
  RichEditViewer: TRichEditViewer;
  cancelclick: Boolean;
  CUSTOM_BUILD_TYPE, Build_Name: String;
  BuildVersion, CurrentBuildVersion, BuildNumber, BuildTag, CSTPRegistry : String;

Procedure  ExitProcess(exitCode:integer);
external 'ExitProcess@kernel32.dll stdcall';


Procedure SaveInstallLog();
var
    LogFileString : AnsiString;
begin
    FileCopy(ExpandConstant ('{log}'), ExpandConstant ('{sd}\InstallLogFile.log'), FALSE);
    LoadStringFromFile(ExpandConstant ('{sd}\InstallLogFile.log'), LogFileString);    
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_TP_InstallLogFile.log'),'Installation Starts here '+ Installation_StartTime + #13#10 + '*********************************************', True);
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_TP_InstallLogFile.log'),#13#10 + LogFileString , True);
    SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_TP_InstallLogFile.log'), #13#10 + '*********************************************' + #13#10 + 'Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
    DeleteFile(ExpandConstant('{sd}\InstallLogFile.log'));
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

// Function to validate same or higher version of build is installed.
function VerifySameOrHigherVersion : Boolean;
begin
  Result := False;
  CSTPRegistry := 'Software\InMage Systems\Installed Products\10';

	if RegKeyExists(HKLM,CSTPRegistry) then
  begin
      if RegValueExists(HKLM,CSTPRegistry,'Build_Version') then
      begin
          RegQueryStringValue(HKEY_LOCAL_MACHINE,CSTPRegistry,'Build_Version',BuildVersion);
          Log('Installed build version - ' + BuildVersion + '' + Chr(13));
          StringChangeEx(BuildVersion,'.','',True);
          CurrentBuildVersion := '{#APPVERSION}';
          Log('Current build version - ' + CurrentBuildVersion + '' + Chr(13));
          RegQueryStringValue(HKEY_LOCAL_MACHINE,CSTPRegistry,'Build_Tag',BuildTag);
          Log('CX - ' + BuildTag + ' is installed' + Chr(13));
          StringChangeEx(CurrentBuildVersion,'.','',True);
          if (StrToInt(CurrentBuildVersion) <= StrToInt(BuildVersion)) then
          begin
              Result := True;
          end;
      end;
  end;
end;

// Extract PHP.
procedure ExtractPHP();
var
  ErrorCode: Integer;
	OldState: Boolean;
begin
    if not CustomDeleteDirectory(ExpandConstant('C:\thirdparty\php5nts')) then
    begin
        ServiceStopInLoop('W3SVC');
        if not CustomDeleteDirectory(ExpandConstant('C:\thirdparty\php5nts')) then
        begin
            SaveInstallLog();
            ExitProcess(18);
        end;
    end; 
		
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
end;

// Extract Strawberry Perl.
procedure ExtractStrawberryPerl();
var
  ErrorCode: Integer;
	OldState: Boolean;
begin
    CustomDeleteDirectory(ExpandConstant('C:\strawberry')); 

    if IsWin64 then
      begin
          //Turn off redirection, so that cmd.exe from the 64-bit System directory is launched.
          OldState := EnableFsRedirection(False);
          try
               Exec(ExpandConstant('{cmd}'), '/C cscript "'+ExpandConstant('C:\Temp\unzip_Files.vbs')+'" "'+ExpandConstant('C:\Temp\strawberry-perl-5.8.8.4.zip')+'" "'+ExpandConstant('C:\strawberry')+'" ','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
          finally
               // Restore the previous redirection state.
          EnableFsRedirection(OldState);
          end;
      end;
end;

// Extract RRD.
procedure ExtractRRD();
var
  ErrorCode: Integer;
	OldState: Boolean;
begin
    CustomDeleteDirectory(ExpandConstant('C:\thirdparty\rrdtool-1.2.15-win32-perl58'));
    CustomDeleteDirectory(ExpandConstant('C:\thirdparty\Cygwin'));

    if IsWin64 then
      begin
          //Turn off redirection, so that cmd.exe from the 64-bit System directory is launched.
      		OldState := EnableFsRedirection(False);
      		try
              Exec(ExpandConstant('{cmd}'), '/C cscript "'+ExpandConstant('C:\Temp\unzip_Files.vbs')+'" "'+ExpandConstant('C:\Temp\rrdtool-1.2.15-win32-perl58.zip')+'" "'+ExpandConstant('C:\thirdparty\rrdtool-1.2.15-win32-perl58')+'" ','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
      		finally
      		// Restore the previous redirection state.
      		EnableFsRedirection(OldState);
      	  end;
      end;
      DeleteFile(ExpandConstant('C:\Temp\rrdtool-1.2.15-win32-perl58.zip'));
end;


//Install Perl modules
procedure InstallPerlModules();
var
  ErrorCode: Integer;
  LogFileString : AnsiString;
begin
    Log('Invoking installmod.bat script.');
    if ShellExec('', ExpandConstant('C:\Temp\installmod.bat'),'', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode) then
    begin
        if FileExists(ExpandConstant('C:\thirdparty\perlmoduleinstall.log')) then	
        begin
            LoadStringFromFile(ExpandConstant ('C:\thirdparty\perlmoduleinstall.log'), LogFileString);    
            Log(#13#10 +'Perl modules install log Starts here '+ #13#10 + '*********************************************');
            Log(#13#10 + LogFileString);
            Log(#13#10 + '*********************************************' + #13#10 + 'Perl modules install log ends here :' +#13#10 + #13#10);
            DeleteFile(ExpandConstant('C:\thirdparty\perlmoduleinstall.log'));
        end;

        if not (ErrorCode = 0) then
        begin
            if (ErrorCode = 1) then
            begin
                SuppressibleMsgBox('Unable to change directory to perl module.', mbError, MB_OK, MB_OK);
                SaveInstallLog();
            end
            else if (ErrorCode = 2) then
            begin
                SuppressibleMsgBox('Unable to generate makefile for perl module.', mbError, MB_OK, MB_OK);
                SaveInstallLog();
            end
            else if (ErrorCode = 3) then
            begin
                SuppressibleMsgBox('Unable to install perl module.', mbError, MB_OK, MB_OK);
                SaveInstallLog();
            end;
        end
        else
            Log('Successfully installed perl modules.');
    end;
end;


// Upgrade check.
function UpgradeCheck : Boolean;
begin
	if UpgradeCX = 'IDYES' then
		Result := True
	else
		Result := False;
end;

// Verify previous version installation.
function VerifyPreviousVersion : Boolean;
begin
	if Fileexists(ExpandConstant('C:\strawberry\perl\bin\perl.exe')) and Fileexists(ExpandConstant('C:\thirdparty\php5nts\php.exe')) then
	begin
		if RegValueExists(HKLM,'Software\InMage Systems\Installed Products\10','Build_Version') then
    begin
        RegQueryStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\10','Build_Version',BuildVersion);
        Log('Installed build version - ' + BuildVersion + '' + Chr(13));
        StringChangeEx(BuildVersion,'.','',True);
        CurrentBuildVersion := '{#APPVERSION}';
        Log('Current build version - ' + CurrentBuildVersion + '' + Chr(13));
        StringChangeEx(CurrentBuildVersion,'.','',True);
        if (StrToInt(CurrentBuildVersion) > StrToInt(BuildVersion)) then
        begin
            RegQueryStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\10','Build_Tag',BuildTag);
            Log('Already CX_TP - ' + BuildTag + ' is installed' + Chr(13));
            Result := True;
        end
        else
            Result := False;
    end
    else
    begin
        RegQueryStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\10','CX_TP_Version',BuildVersion);
        Log('Installed build version - ' + BuildVersion + '' + Chr(13));
        StringChangeEx(BuildVersion,'.','',True);
        CurrentBuildVersion := '{#APPVERSION}';
        Log('Current build version - ' + CurrentBuildVersion + '' + Chr(13));
        StringChangeEx(CurrentBuildVersion,'.','',True);
        if (StrToInt(CurrentBuildVersion) > StrToInt(BuildVersion)) then
        begin
            RegQueryStringValue(HKEY_LOCAL_MACHINE,'Software\InMage Systems\Installed Products\10','CX_TP_Version',BuildNumber);
            Log('Already CX_TP - ' + BuildNumber + ' is installed' + Chr(13));
            Result := True;
        end
        else
            Result := False;
    end;
	end;
end;

// Verify CS installation.
function IsCSInstalled : Boolean;
var
	a_strTextfile : TArrayOfString;
begin
	if RegKeyExists(HKLM,'Software\InMage Systems\Installed Products\9') and IsServiceInstalled('tmansvc') then
		Result := True
	else
		Result := False;
end;

// Verify Agent installation.
function IsAgentInstalled : Boolean;
begin
    if RegKeyExists(HKLM, 'Software\InMage Systems\Installed Products\5') and IsServiceInstalled('svagents') then
        Result := True
    else 
        Result := False;
end;

// Check if application is already running.
procedure IsCurrentAppRunning();
begin
     if (not (CheckForMutexes('InMage_CX_Depends'))) then
           CreateMutex('InMage_CX_Depends')            
     else
     begin
          While(CheckForMutexes('InMage_CX_Depends')) do
          begin
              if WizardSilent then
              begin
                   Log('Another instance of this appliccation is already running! Hence, Aborting..');
                   Abort;
              end
              else if SuppressibleMsgBox('The installer is already running! To run installer again, you must first close existing installer process.',mbError, MB_OKCANCEL or MB_DEFBUTTON1, MB_OK) = IDCANCEL then
                	 Abort;  
          end;
          CreateMutex('InMage_CX_Depends')
     end;   
end;

function InitializeSetup(): Boolean;
begin
	Result := True;
	
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
      ExitProcess(11);
  end;

	if VerifyPreviousVersion then
	begin
    UpgradeCX := 'IDYES'
	end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if not WizardSilent then
  begin
      if CurPageID = wpWelcome then 
      begin
          WizardForm.NextButton.Caption := 'Install';
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
        Exec(ExpandConstant('{cmd}'), '/C '+ExpandConstant('C:\thirdparty\unins000.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART'),'', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
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

function GetValue(KeyName: AnsiString) : AnsiString ;
var
	Line, Value : AnsiString;
	iLineCounter, n : Integer;
	a_strTextfile : TArrayOfString;
  dirpath : String;
begin
	if RegValueExists(HKLM,'Software\InMage Systems\Installed Products\9','Product_Name') and IsServiceInstalled('tmansvc') then
	begin
		RegQueryStringValue(HKLM,'Software\InMage Systems\Installed Products\9','InstallDirectory',dirpath);
    LoadStringsFromFile(ExpandConstant(dirpath+'\etc\amethyst.conf'), a_strTextfile);

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

// Function to copy php.ini to systemdrive\temp.
function CopyPhpIniToTempDirectory(): Boolean;
var
  LoopCount : Integer; 
begin
  LoopCount := 1;
  while (LoopCount <= 3) do
  begin
      if FileCopy(ExpandConstant ('C:\thirdparty\php5nts\php.ini'), ExpandConstant ('{sd}\Temp\php.ini'), FALSE) then
      begin
          Log('Successfully copied php.ini to '+ExpandConstant('{sd}\Temp')+' from C:\thirdparty\php5nts')
          Result := True;
          break;
      end
      else
      begin
          Log('Unable to copy php.ini to '+ExpandConstant('{sd}\Temp')+' from C:\thirdparty\php5nts');
          LoopCount := LoopCount + 1;
          ServiceStopInLoop('W3SVC');
          Sleep(60000);
          Result := False;
      end;            
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
	ResultCode, ErrorCode, WaitLoopCount : Integer;
begin
  // ssInstall step.
	if CurStep = ssInstall then
	begin
    	if UpgradeCheck then
    	begin
          // Create systemdrive\Temp directory.
          if not CustomCreateDirectory(ExpandConstant('{sd}\Temp')) then
          begin
              RollBackInstallation();
              ExitProcess(13);
          end;

          // Copy php.ini to systemdrive\Temp from C:\thirdparty\php5nts.
          if not CopyPhpIniToTempDirectory() then
          begin
              ServiceStartInLoop('W3SVC');
              RollBackInstallation();
              ExitProcess(14);
          end;

          // Stop svagents service.
          if not ServiceStopInLoop('svagents') then 
          begin
              RollBackInstallation();
              ExitProcess(22);
          end;

          // Stop InMage Scout Application Service.
          if not ServiceStopInLoop('InMage Scout Application Service') then 
          begin
              RollBackInstallation();
              ExitProcess(23);
          end;

          // Stop W3SVC service.
          if not ServiceStopInLoop('W3SVC') then 
          begin
              RollBackInstallation();
              ExitProcess(20);
          end;

          // Stop WAS service.
          if GetValue('CX_TYPE') = '3' then
          begin
              Log('Stopping WAS service.');
              Exec(ExpandConstant('{cmd}'), '/C net stop WAS /Y','', SW_HIDE,ewWaitUntilTerminated, ErrorCode);
              
              WaitLoopCount := 1;
              while (WaitLoopCount <= 9) and (ServiceStatusString('WAS')<>'SERVICE_STOPPED') do
              begin
                Sleep(20000);
                WaitLoopCount := WaitLoopCount + 1;
              end;
              
              if ServiceStatusString('WAS')='SERVICE_STOPPED' then
              begin
                  Log('WAS service stopped successfully.');
              end
              else
              begin
                  Log('Unable to stop WAS service.');
              end;
          end;

          // Stop W3SVC service.
          if not ServiceStopInLoop('W3SVC') then 
          begin
              RollBackInstallation();
              ExitProcess(20);
          end;

          // Always stop ProcessServerMonitor service first to avoid generating alerts if other PS services are stopped during install/upgrade/uninstall.
          if not ServiceStopInLoop('ProcessServerMonitor') then
          begin
              RollBackInstallation();
              ExitProcess(35);
          end;

          if not ServiceStopInLoop('ProcessServer') then
          begin
              RollBackInstallation();
              ExitProcess(36);
          end;

          // Stop cxprocessserver service.
          if not ServiceStopInLoop('cxprocessserver') then 
          begin
              RollBackInstallation();
              ExitProcess(26);
          end;
  
          // Stop tmansvc service.
      		if not ServiceStopInLoop('tmansvc') then 
          begin
              RollBackInstallation();
              ExitProcess(21);
          end;

          // Executing clean_stale_process.pl script.
          ExtractTemporaryFile('clean_stale_process.pl');
          Exec(ExpandConstant('C:\strawberry\perl\bin\perl.exe'),ExpandConstant('{tmp}\clean_stale_process.pl'), '', SW_HIDE, ewWaitUntilTerminated, ResultCode);

          // Stop InMage PushInstall service.
          if not ServiceStopInLoop('InMage PushInstall') then 
          begin
              RollBackInstallation();
              ExitProcess(24);
          end;

          // Stop dra service.
          if GetValue('CX_TYPE') = '3' then
          begin
              if not ServiceStopInLoop('dra') then 
              begin
                  RollBackInstallation();
                  ExitProcess(25);
              end;
          end;
          
          // Stop MySQL service.
          if GetValue('CX_TYPE') = '3' then
          begin
              if not ServiceStopInLoop('MySQL') then 
              begin
                  RollBackInstallation();
                  ExitProcess(27);
              end;
          end;

          Log('Sleeping for 2 minutes after stopping the services.');
          sleep(120000)
    	end;
	end;

  // ssPostInstall step.
	if CurStep = ssPostInstall then 
	begin
      // Create C:\thirdparty\php5nts\sessiondata directory.
      if not CustomCreateDirectory(ExpandConstant('C:\thirdparty\php5nts\sessiondata')) then
      begin
          RollBackInstallation();
          ExitProcess(15);
      end;

      // Create C:\thirdparty\php5nts\uploadtemp directory.
      if not CustomCreateDirectory(ExpandConstant('C:\thirdparty\php5nts\uploadtemp')) then
      begin
          RollBackInstallation();
          ExitProcess(16);
      end;

      if UpgradeCheck then
      begin
          // Copy systemdrive\Temp\php.ini to C:\thirdparty\php5nts\php.ini.
          if FileCopy(ExpandConstant ('{sd}\Temp\php.ini'), ExpandConstant ('C:\thirdparty\php5nts\php.ini'), FALSE) then
            Log('Successfully copied php.ini to C:\thirdparty\php5nts from '+ExpandConstant('{sd}\Temp'))
          else
          begin
            Log('Unable to copy php.ini to C:\thirdparty\php5nts from '+ExpandConstant('{sd}\Temp'));
            RollBackInstallation();
            ExitProcess(17);
          end;

          // Delete PHP_MYSQL section in C:\thirdparty\php5nts\php.ini file.
          DeleteIniSection('PHP_MYSQL', 'C:\thirdparty\php5nts\php.ini');
          
          // Start services.
          if GetValue('CX_TYPE') = '3' then
          begin
              ServiceStartInLoop('W3SVC');
              ServiceStartInLoop('MySQL');
          end;
          ServiceStartInLoop('tmansvc');
          ServiceStartInLoop('ProcessServer');
          ServiceStartInLoop('svagents');
          ServiceStartInLoop('InMage Scout Application Service');
          ServiceStartInLoop('InMage PushInstall');
          
          if GetValue('CX_TYPE') = '3' then
          begin
              ServiceStartInLoop('dra');
          end;
          
          ServiceStartInLoop('cxprocessserver');
          // Always start ProcessServerMonitor service last to avoid generating alerts if other services are stopped first during install/upgrade/uninstall.
          ServiceStartInLoop('ProcessServerMonitor');
      end;
      
      // Update Build_Tag in registry.
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\InMage Systems\Installed Products\10', 'Build_Tag', '{#BUILDTAG}') then
          Log('''Build_Tag'' registry value updated under ''HKLM\SOFTWARE\InMage Systems\Installed Products\10'' registry key.')
      else
      begin
          Log('Unable to update ''Build_Tag'' registry value under ''HKLM\SOFTWARE\InMage Systems\Installed Products\10'' registry key.');
          RollBackInstallation();
          ExitProcess(30);
      end;

      // Update Build_Phase in registry.
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\InMage Systems\Installed Products\10', 'Build_Phase', '{#BUILDPHASE}') then
          Log('''Build_Phase'' registry value updated under ''HKLM\SOFTWARE\InMage Systems\Installed Products\10'' registry key.')
      else
      begin
          Log('Unable to update ''Build_Phase'' registry value under ''HKLM\SOFTWARE\InMage Systems\Installed Products\10'' registry key.');
          RollBackInstallation();
          ExitProcess(31);
      end;

      // Update Build_Number in registry.
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\InMage Systems\Installed Products\10', 'Build_Number', '{#BUILDNUMBER}') then
          Log('''Build_Number'' registry value updated under ''HKLM\SOFTWARE\InMage Systems\Installed Products\10'' registry key.')
      else
      begin
          Log('Unable to update ''Build_Number'' registry value under ''HKLM\SOFTWARE\InMage Systems\Installed Products\10'' registry key.');
          RollBackInstallation();
          ExitProcess(32);
      end;

      // Update CX_TP_Version in registry.
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\InMage Systems\Installed Products\10', 'CX_TP_Version', '{#VERSION}') then
          Log('''CX_TP_Version'' registry value updated under ''HKLM\SOFTWARE\InMage Systems\Installed Products\10'' registry key.')
      else
      begin
          Log('Unable to update ''CX_TP_Version'' registry value under ''HKLM\SOFTWARE\InMage Systems\Installed Products\10'' registry key.');
          RollBackInstallation();
          ExitProcess(33);
      end;

      // Update Build_Version in registry.
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\InMage Systems\Installed Products\10', 'Build_Version', '{#APPVERSION}') then
          Log('''Build_Version'' registry value updated under ''HKLM\SOFTWARE\InMage Systems\Installed Products\10'' registry key.')
      else
      begin
          Log('Unable to update ''Build_Version'' registry value under ''HKLM\SOFTWARE\InMage Systems\Installed Products\10'' registry key.');
          RollBackInstallation();
          ExitProcess(34);
      end;
    
	end;

  // ssDone step.
	if CurStep=ssDone then
	begin
		Log('Installation completed successfully.');
	end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
	if CurUninstallStep=usUninstall then
	begin
		if IsCSInstalled and IsAgentInstalled then
    begin
        SuppressibleMsgBox('Third- party components for Azure Site Recovery configuration server or process server couldn''t be uninstalled. You''ll need to remove other software in this order before you can uninstall the components:'+#13+#13+'1. Microsoft Azure Site Recovery Mobility Service / Master Target Server'+#13+'2. Microsoft Azure Site Recovery Configuration/Process Server.', mbError, MB_OK,MB_OK);
  		  SaveStringToFile(mcLogFile, 'Third- party components for Azure Site Recovery configuration server or process server couldn''t be uninstalled. You''ll need to remove other software in this order before you can uninstall the components:'+#13+#13+'1. Microsoft Azure Site Recovery Mobility Service / Master Target Server'+#13+'2. Microsoft Azure Site Recovery Configuration/Process Server.' + Chr(13), True);
  		  Abort;
    end
    else if IsCSInstalled then
    begin
        SuppressibleMsgBox('Third- party components for Azure Site Recovery configuration server or process server couldn''t be uninstalled. You''ll need to remove other software in this order before you can uninstall the components:'+#13+#13+'1. Microsoft Azure Site Recovery Configuration/Process Server.', mbError, MB_OK,MB_OK);
  		  SaveStringToFile(mcLogFile, 'Third- party components for Azure Site Recovery configuration server or process server couldn''t be uninstalled. You''ll need to remove other software in this order before you can uninstall the components:'+#13+#13+'1. Microsoft Azure Site Recovery Configuration/Process Server.' + Chr(13), True);
  		  Abort;
    end
    else if IsAgentInstalled then
    begin
        SuppressibleMsgBox('Third- party components for Azure Site Recovery configuration server or process server couldn''t be uninstalled. You''ll need to remove other software in this order before you can uninstall the components:'+#13+#13+'1. Microsoft Azure Site Recovery Mobility Service / Master Target Server', mbError, MB_OK,MB_OK);
  		  SaveStringToFile(mcLogFile, 'Third- party components for Azure Site Recovery configuration server or process server couldn''t be uninstalled. You''ll need to remove other software in this order before you can uninstall the components:'+#13+#13+'1. Microsoft Azure Site Recovery Mobility Service / Master Target Server' + Chr(13), True);
  		  Abort;
    end
    else
    begin
        SaveStringToFile(mcLogFile, 'Microsoft Azure Site Recovery Mobility Service / Master Target Server and Microsoft Azure Site Recovery Configuration/Process Server are not installed. Proceeding with uninstallation.' + Chr(13), True);
    end
	end;

  if CurUninstallStep=usDone then
  begin
      if RegDeleteKeyIncludingSubkeys(HKLM,'SOFTWARE\InMage Systems\Installed Products\10') then
          SaveStringToFile(mcLogFile, 'Deleted ''HKLM\SOFTWARE\Wow6432Node\InMage Systems\Installed Products\10'' registry key successfully.' + Chr(13), True)
      else
          SaveStringToFile(mcLogFile, 'Unable to Delete ''HKLM\SOFTWARE\Wow6432Node\InMage Systems\Installed Products\10'' registry key. Please delete it manually.' + Chr(13), True);

      if RegDeleteValue(HKLM,'SOFTWARE\InMage Systems\Installed Products','IIS_Installation_Status') then
          SaveStringToFile(mcLogFile, 'Deleted ''HKLM\SOFTWARE\InMage Systems\Installed Products\IIS_Installation_Status'' registry value successfully.' + Chr(13), True)
      else
          SaveStringToFile(mcLogFile, 'Unable to delete ''HKLM\SOFTWARE\InMage Systems\Installed Products\IIS_Installation_Status'' registry value. Please delete it manually.' + Chr(13), True);
		  
      // Delete MachineIdentifier at the end of installation.
      if RegDeleteValue(HKLM,'SOFTWARE\InMage Systems','MachineIdentifier') then
          SaveStringToFile(mcLogFile, 'Deleted ''HKLM\SOFTWARE\Wow6432Node\InMage Systems\MachineIdentifier'' registry key successfully.' + Chr(13), True)
      else
          SaveStringToFile(mcLogFile, 'Unable to Delete ''HKLM\SOFTWARE\Wow6432Node\InMage Systems\MachineIdentifier'' registry key. Please delete it manually.' + Chr(13), True);
		  
  end;
end;


procedure DeinitializeSetup();
var
Errorcode : Integer;
LogFileString : AnsiString;
Result1 : Boolean;
begin
    	DelTree(ExpandConstant('C:\Temp\DBD-mysql'), True, True, True);
    	DelTree(ExpandConstant('C:\Temp\DBI'), True, True, True);
    	DelTree(ExpandConstant('C:\Temp\Win32-0.39'), True, True, True);
    	DelTree(ExpandConstant('C:\Temp\Win32-OLE-0.1709'), True, True, True);
    	DelTree(ExpandConstant('C:\Temp\Win32-Process-0.14'), True, True, True);
    	DelTree(ExpandConstant('C:\Temp\Win32-Service-0.06'), True, True, True);
    	
    	FileCopy(ExpandConstant ('{log}'), ExpandConstant ('{sd}\InstallLogFile.log'), FALSE);
      LoadStringFromFile(ExpandConstant ('{sd}\InstallLogFile.log'), LogFileString);    
      SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_TP_InstallLogFile.log'),'Installation Starts here '+ Installation_StartTime + #13#10 + '*********************************************', True);
      SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_TP_InstallLogFile.log'),#13#10 + LogFileString , True);
      SaveStringToFile(ExpandConstant ('{sd}\ProgramData\ASRSetupLogs\CX_TP_InstallLogFile.log'), #13#10 + '*********************************************' + #13#10 + 'Installation Ends here :' + GetDateTimeString('dd/mm/yyyy hh:nn:ss', '.', ':')+ #13#10, True);
      DeleteFile(ExpandConstant('{sd}\InstallLogFile.log'));
  
end;

function InitializeUninstall(): Boolean;
begin
      mcLogFile := ExpandConstant('{sd}\ProgramData\ASRSetupLogs\CX_TP_UnInstallLogFile.log');
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
