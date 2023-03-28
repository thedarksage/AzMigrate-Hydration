@echo off

REM --- Get agent instalaltion directory ---
REG QUERY "HKLM\SOFTWARE\Wow6432Node" > NUL 2>&1

if %errorlevel% equ 0 (

FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "HKLM\SOFTWARE\Wow6432Node\SV Systems\FileReplicationAgent" /v InstallDirectory') DO SET installpath=%%B
)

if %errorlevel% NEQ 0 (

FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "HKLM\SOFTWARE\SV Systems\FileReplicationAgent" /v InstallDirectory') DO SET installpath=%%B
)

REM --- Remove inbound rules related to VX ---
For /F "usebackq tokens=1" %%I IN (`type "%installpath%\vxfiles.txt"`) DO (
	netsh advfirewall firewall delete rule name="InMage_%%I" program="%installpath%\%%I.exe" > NUL 2>&1
)

REM --- Remove inbound rules related to FX ---
netsh advfirewall firewall delete rule name="InMage_frsvc" program="%installpath%\filerep\frsvc.exe" > NUL 2>&1
netsh advfirewall firewall delete rule name="InMage_inmsync" program="%installpath%\filerep\inmsync.exe" > NUL 2>&1
netsh advfirewall firewall delete rule name="InMage_unregagent" program="%installpath%\filerep\unregagent.exe" > NUL 2>&1
netsh advfirewall firewall delete rule name="InMage_cxps" program="%installpath%\transport\cxps.exe" > NUL 2>&1
netsh advfirewall firewall delete rule name="InMage_cxpscli" program="%installpath%\transport\cxpscli.exe" > NUL 2>&1

REM --- Add inbound rule for vacp only when Role is not a MasterTarget ---
findstr "^Role=MasterTarget" "%installpath%\Application Data\etc\drscout.conf" > NUL 2>&1
if %errorlevel% NEQ 0 (
	netsh advfirewall firewall add rule name="InMage_vacp" dir=in action=allow program="%installpath%\vacp.exe" enable=yes
)