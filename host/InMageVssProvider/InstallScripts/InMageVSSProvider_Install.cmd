rem @echo off
setlocal

rem UnInstall existing installation of InMage VSS Provider
call "%~dp0%InMageVSSProvider_Uninstall.cmd"
set/a unInstallExitCode = %errorlevel%
if %unInstallExitCode% NEQ 0 goto EXIT_FROM_UNINSTALL
set INSTALLTO=%~dp0
set INSTALLFROM=%~dp0
pushd %INSTALLTO%

cscript InMageVssProvider_Register.vbs -register "Azure Site Recovery VSS Provider" "%INSTALLTO%InMageVssProvider.dll" "Azure Site Recovery VSS Provider"
set/a exitCode = %errorlevel%

set EVENT_LOG=HKLM\SYSTEM\CurrentControlSet\Services\Eventlog\Application\InMageVSSProvider
reg.exe add %EVENT_LOG% /f
reg.exe add %EVENT_LOG% /f /v CustomSource /t REG_DWORD /d 1
reg.exe add %EVENT_LOG% /f /v EventMessageFile /t REG_EXPAND_SZ /d "%INSTALLTO%InMageVssProvider.dll"
reg.exe add %EVENT_LOG% /f /v TypesSupported /t REG_DWORD /d 7
exit /B %exitCode%
goto DONE

:EXIT_FROM_UNINSTALL
exit /B %unInstallExitCode%
goto DONE

:DONE


