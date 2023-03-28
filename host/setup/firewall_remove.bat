@echo off

REM --- Get agent instalaltion directory ---
REG QUERY "HKLM\SOFTWARE\Wow6432Node" > NUL 2>&1

if %errorlevel% equ 0 (

FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "HKLM\SOFTWARE\Wow6432Node\SV Systems\FileReplicationAgent" /v InstallDirectory') DO SET installpath=%%B
)

if %errorlevel% NEQ 0 (

FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "HKLM\SOFTWARE\SV Systems\FileReplicationAgent" /v InstallDirectory') DO SET installpath=%%B
)

REM --- Remove inbound rule for vacp only when Role is not a MasterTarget ---
findstr "^Role=MasterTarget" "%installpath%\Application Data\etc\drscout.conf" > NUL 2>&1
if %errorlevel% NEQ 0 (
	netsh advfirewall firewall delete rule name="InMage_vacp" program="%installpath%\vacp.exe"
)