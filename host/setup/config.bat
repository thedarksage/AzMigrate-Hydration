@echo off

if "%PROCESSOR_ARCHITECTURE%"=="x86" (
set REG_OS=SOFTWARE
) else (
set REG_OS=SOFTWARE\Wow6432Node
)

ver | find "XP" > nul
if %ERRORLEVEL% == 0 (
FOR /F "tokens=2* " %%A IN ('REG QUERY "HKLM\%REG_OS%\SV Systems\vContinuum" /v vSphereCLI_Path') DO SET vmpath=%%B
) else (
FOR /F "tokens=2* delims= " %%A IN ('REG QUERY "HKLM\%REG_OS%\SV Systems\vContinuum" /v vSphereCLI_Path') DO SET vmpath=%%B
)

set PATH=%vmpath%;%systemdrive%\windows;%systemdrive%\windows\system32;%systemdrive%\windows\system32\wbem;%path%

%1
