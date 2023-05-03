@echo off

REG QUERY "HKLM\SOFTWARE\Wow6432Node" > NUL 2>&1

if %errorlevel% equ 0 (

FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "HKLM\SOFTWARE\Wow6432Node\SV Systems\FileReplicationAgent" /v InstallDirectory') DO SET fxpath=%%B
)

if %errorlevel% NEQ 0 (

FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "HKLM\SOFTWARE\SV Systems\FileReplicationAgent" /v InstallDirectory') DO SET fxpath=%%B
)


sc query svagents
if %errorlevel% equ 0 (
For /F "usebackq tokens=1" %%I IN (`type vxfiles.txt`) DO (
	netsh firewall add allowedprogram "%fxpath%\%%I.exe" "InMage_%%I" ENABLE
)
)



sc query frsvc
if %errorlevel% equ 0 (
For /F "usebackq tokens=1" %%I IN (`type fxfiles.txt`) DO (
	netsh firewall add allowedprogram "%fxpath%\Filerep\%%I.exe" "InMage_%%I" ENABLE
)
)
