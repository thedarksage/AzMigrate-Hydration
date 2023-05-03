@echo off

if "%PROCESSOR_ARCHITECTURE%"=="x86" (
set REG_OS=SOFTWARE
) else (
set REG_OS=SOFTWARE\Wow6432Node
)

ver | find "XP" > nul
if %ERRORLEVEL% == 0 (
FOR /F "tokens=3* " %%A IN ('REG QUERY "HKLM\%REG_OS%\ActiveState\ActivePerl\820"') DO SET vSpherepath=%%B
) else (
FOR /F "tokens=2* delims= " %%A IN ('REG QUERY "HKLM\%REG_OS%\ActiveState\ActivePerl\820"') DO SET vSpherepath=%%B
)

REG ADD "HKLM\%REG_OS%\SV Systems\vContinuum" /v vSphereCLI_Path /t REG_MULTI_SZ /d "%vSpherepath%site\bin;%vSpherepath%bin;"

