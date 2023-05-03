@echo on

setlocal EnableDelayedExpansion

rem check that install or uninstall is properly specified
if "%1" == "/iu" (
	set ACTION=adding
	goto argOk
)

if "%1" == "/uu" (
	set ACTION=removing
	goto argOk
)
echo usage iis-install.bat /iu ^| /uu
echo.
echo /iu - install
echo /uu - uninstall
goto done

:argOk
rem if running on 64-bit OS, it is required to use the 64-bit pkgmgr so make sure if this 
rem bat file is being run in a 32-bit process on a 64-bit OS to use SysNative alias for 
rem system32 to get to the 64-bit system32 stuff. but because SysNative is only available to 
rem 32-bit apps running on 64-bit OS, have to check if system32 or SysNative should be used. 
rem the registry entry used below has the actual OS arch while the environment var 
rem %PROCESSOR_ARCHITECTURE% has the current running process arch if they are the same OK 
rem to use system32 if they are diff use SysNative
for /F "tokens=2* delims=	 " %%A in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v PROCESSOR_ARCHITECTURE') do set SYS_ARCH=%%B
if %PROCESSOR_ARCHITECTURE% == %SYS_ARCH% (
	set SYS_DIR=system32
) else (
  set SYS_DIR=SysNative
)

ver | findstr /IL "6.0." > NUL
if %ERRORLEVEL% equ 0 (
	set POWERSHELL=MicrosoftWindowsPowerShell
	goto do_install
)

ver | findstr /IL "6.1." > NUL
if %ERRORLEVEL% equ 0 (
	set POWERSHELL=MicrosoftWindowsPowerShellISE
	goto do_install
)

ver | findstr /IL "6.2." > NUL
if %ERRORLEVEL% equ 0 (
	set POWERSHELL=MicrosoftWindowsPowerShellISE
	goto do_install
)

::Using the DISM tool to install the IIS in Windows 2012-R2
ver | findstr /IL "6.3." > NUL
if %ERRORLEVEL% equ 0 (
	set POWERSHELL=MicrosoftWindowsPowerShellISE
	start /w dism /online /enable-feature /featurename:NetFx3 /featurename:!POWERSHELL! /featurename:WAS-WindowsActivationService /featurename:WAS-ProcessModel /featurename:WAS-NetFxEnvironment /featurename:WAS-ConfigurationAPI	/featurename:IIS-WebServerRole /featurename:IIS-WebServer /featurename:IIS-ManagementService /featurename:IIS-RequestMonitor /featurename:IIS-CommonHttpFeatures /featurename:IIS-StaticContent /featurename:IIS-DefaultDocument /featurename:IIS-DirectoryBrowsing /featurename:IIS-HttpErrors /featurename:IIS-ApplicationDevelopment /featurename:IIS-CGI /featurename:IIS-HealthAndDiagnostics /featurename:IIS-HttpLogging /featurename:IIS-LoggingLibraries /featurename:IIS-Security /featurename:IIS-RequestFiltering /featurename:IIS-Performance /featurename:IIS-HttpCompressionStatic /featurename:IIS-WebServerManagementTools /featurename:IIS-ManagementConsole /featurename:IIS-ManagementScriptingTools /featurename:NetFx4Extended-ASPNET45 /featurename:IIS-NetFxExtensibility45
)

:do_install
rem IIS
echo   %ACTION% Web Server (IIS)
start /w %windir%\%SYS_DIR%\pkgmgr.exe /norestart %1:NetFx3;%POWERSHELL%;WAS-WindowsActivationService;WAS-ProcessModel;WAS-NetFxEnvironment;WAS-ConfigurationAPI;IIS-WebServerRole;IIS-WebServer;IIS-CommonHttpFeatures;IIS-StaticContent;IIS-DefaultDocument;IIS-DirectoryBrowsing;IIS-HttpErrors;IIS-ApplicationDevelopment;IIS-CGI;IIS-HealthAndDiagnostics;IIS-HttpLogging;IIS-LoggingLibraries;IIS-RequestMonitor;IIS-Security;IIS-RequestFiltering;IIS-Performance;IIS-HttpCompressionStatic;IIS-WebServerManagementTools;IIS-ManagementConsole;IIS-ManagementScriptingTools;IIS-ManagementService;NetFx4Extended-ASPNET45;IIS-NetFxExtensibility45


:done
echo done
echo.
