rem @echo off
setlocal

set INSTALLTO=%~dp0

net stop vds /Y
net stop vss /Y
net stop swprv /Y
net stop "Azure Site Recovery VSS Provider" /Y


pushd %INSTALLTO%

cscript > nul 2>&1
if %errorlevel% NEQ 0 exit /B 806

reg.exe delete HKLM\SYSTEM\CurrentControlSet\Services\Eventlog\Application\InMageVSSProvider /f

cscript InMageVssProvider_Register.vbs -unregister "Azure Site Recovery VSS Provider"
set /a unInstallExitCode = %errorlevel%
regsvr32 /s /u InMageVssProvider.dll

exit /B %unInstallExitCode%




