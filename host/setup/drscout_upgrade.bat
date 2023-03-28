@echo off

if "%PROCESSOR_ARCHITECTURE%"=="x86" (
set REG_OS=SOFTWARE
) else (
set REG_OS=SOFTWARE\Wow6432Node
)

ver | find "XP" > nul
if %ERRORLEVEL% == 0 (
FOR /F "tokens=2* " %%A IN ('REG QUERY "HKLM\%REG_OS%\SV Systems\VxAgent" /v InstallDirectory') DO SET vxpath=%%B
) else (
FOR /F "tokens=2* delims= " %%A IN ('REG QUERY "HKLM\%REG_OS%\SV Systems\VxAgent" /v InstallDirectory') DO SET vxpath=%%B
)


"%vxpath%\configmergetool.exe" --oldfilepath "%vxpath%\Application Data\etc\drscout.conf" --newfilepath "%vxpath%\Application Data\etc\drscout.conf.new" --operation merge

"%vxpath%\configmergetool.exe" --oldfilepath "%vxpath%\Application Data\etc\drscout.conf"  --overwritefilepath "%vxpath%\Application Data\etc\drscout_temp.conf" --operation merge

