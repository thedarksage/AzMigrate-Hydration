@echo off

REG QUERY "HKLM\SOFTWARE\Wow6432Node" > NUL 2>&1

if %errorlevel% equ 0 (

FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "HKLM\SOFTWARE\Wow6432Node\SV Systems\FileReplicationAgent" /v InstallDirectory') DO SET fxpath=%%B
)

if %errorlevel% NEQ 0 (

FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "HKLM\SOFTWARE\SV Systems\FileReplicationAgent" /v InstallDirectory') DO SET fxpath=%%B
)


For /F "usebackq tokens=1" %%I IN (`type vcontinuumfiles.txt`) DO (
	netsh firewall add allowedprogram program="%fxpath%\vContinuum\%%I.exe" name="InMage_%%I" ENABLE
)