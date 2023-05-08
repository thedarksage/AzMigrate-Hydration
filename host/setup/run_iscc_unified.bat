@echo off
rem Invokes the InnoSetup compiler to build the specified setup script
rem Usage: run_iscc <scriptname.iss>

rem if %1 equ unifiedagent.iss call C:\build_scripts\perl\7_Feb_2007\sign_drivers.bat %2

for /f "usebackq tokens=2*" %%f in (`..\..\build\reg query HKLM\SOFTWARE\Classes\InnoSetupScriptFile\shell\Compile\command\`) do set ISCC_PATHNAME=%%f %%g
set ISCC_PATHNAME=%ISCC_PATHNAME:/cc "%1"=%
set ISCC_PATHNAME=%ISCC_PATHNAME:Compil32=iscc%
echo about to execute %ISCC_PATHNAME%
%ISCC_PATHNAME% %1 %2 %3 %4 %5 %6 %7 %8 %9

