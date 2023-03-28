@Echo off
SETLOCAL
set PATH=%PATH%;%~dp0\..;%~dp0;

IF (%1) == () GOTO USAGE_PRINT
IF /i (%1) == (Failover) GOTO FS_FAILOVER
IF /i (%1) == (Failback) GOTO FS_FAILBACK
IF /i (%1) == (FastFailback) GOTO FS_FASTFAILBACK  

:USAGE_PRINT
echo USAGE:
echo	FileServer-SecondaryServer.bat "<Operation>"
echo	OPERATION can be any of the following
echo		Failover
echo		Failback
echo		Fastfailback
echo Example: FileServer-PrimaryServer.bat Failback
GOTO USAGE_EXIT

:USAGE_FAILOVER_PRINT
echo FAILOVER USAGE:
echo	FileServer-SecondaryServer.bat Failover "<Primary Server Host Name>"  "<Secondary Server Host Name>"
echo		- First parameter must be Failover.
echo		- Second parameter must be Primary Server Host Name. 
echo		- Third parameter must be Secondary Server Host Name 
echo Example:
echo		FileServer-SecondaryServer.bat  Failover  Primary_Server  Secondary-Server  
GOTO  USAGE_EXIT

:USAGE_FAILBACK_PRINT
echo FAILBACK USAGE:
echo	FileServer-SecondaryServer.bat Failback "<Primary Server host Name>" "<Secondary Server Host Name>"  "<List of Volume>" "<List of ServerDevices(LUN IDs)>" "<Vacp/PS Server IP>" "<CS Server IP>"
echo		- First parameter must be Failback. 
echo		- Second parameter must be Primary Server host name.
echo		- Third parameter must be Secondary Server host name
echo		- Fourth parameter must be list of protected volumes for which tag should be generated.
echo		Use double quotes for specifying list of volumes separated by semicolon.
echo		- Fifth parameter must be server device names. Server devices are the LUN IDs(Specify the LUN IDs for the respective list of volumes) in the Server.
echo		Use double quotes for specifying list of server device name separated by semicolon.
echo		- Sixth parameter must be PS Server IP or Vacp Server IP
echo		- Seventh parameter must be CS server IP
echo Example: 
echo		FileServer-SecondaryServer.bat  Failback  Primary_Server  Secondary_Server  "E:;F:\MNT1;G:"  "/dev/mapper/2000b0802a9002158;/dev/mapper/2000b0802a9002545"  10.0.25.26  10.0.80.26
GOTO  USAGE_EXIT

:USAGE_FASTFAILBACK_PRINT
echo FASTFAILBACK USAGE:
echo	FileServer-SecondaryServer.bat Fastfailback "<Primary Server Host Name>"  "<Secondary Server Host Name>"
echo		- First parameter must be Fastfailback 
echo		- Second parameter must be Primary Server Host Name.
echo		- Third parameter must be Secondary Server Host Name. 
echo Example:
echo        FileServer-SecondaryServer.bat Fastfailback Primary_Server  Secondary_Server
GOTO  USAGE_EXIT

:FS_FAILOVER

SET PRIMARY_SERVER_NAME=%2
SET SECONDARY_SERVER_NAME=%3

IF (%2) == () GOTO USAGE_FAILOVER_PRINT
IF (%3) == () GOTO USAGE_FAILOVER_PRINT
IF (%4) == () GOTO START_FS_FAILOVER

:START_FS_FAILOVER

echo ***********Starting Fileserver Failover ***********

REM - ROLLBACK all the volumes to a consistency point
echo Importing registry to the secondary server
@echo on
regedit /s "..\Application Data\replshares\shares.reg"
@echo off

echo Deleting Secondary server NetBIOS name
@echo on
WinOp.exe NETBIOS -delete %SECONDARY_SERVER_NAME%
@echo off

echo Stopping lanmanserver and Computer Browser service
REM net stop dfs
@echo on
net stop browser
net stop lanmanserver
@echo off

echo Removing Primary Server HOST SPN entries from Primary Server Account at AD
@echo on
WinOP.exe SPN -deleteHost %PRIMARY_SERVER_NAME%:%PRIMARY_SERVER_NAME%
@echo off

echo Adding Primary Server HOST SPN entries to Secondary Server account at AD
@echo on
WinOp.exe SPN -addHost %SECONDARY_SERVER_NAME%:%PRIMARY_SERVER_NAME%
@echo off

echo Starting DNS failover
@echo on
dns.exe -failover -s %PRIMARY_SERVER_NAME% -t %SECONDARY_SERVER_NAME% -fabric
@echo off

echo Starting Lanmanserver and Computer Browser service
REM net start dfs
@echo on
net start lanmanserver
net start browser
@echo off

echo Adding Primary Server netbios name to Secondary Server
@echo on
WinOp.exe NETBIOS -add %PRIMARY_SERVER_NAME%
@echo off

echo **********Fileserver failover completed***********

GOTO USAGE_EXIT


:FS_FAILBACK

SET PRIMARY_SERVER_NAME=%2
SET SECONDARY_SERVER_NAME=%3
SET VOLUMES=%4
SET SERVERDEVICE=%5
SET SERVERIP=%6
SET CSIP=%7

IF (%2) == () GOTO USAGE_FAILBACK_PRINT
IF (%3) == () GOTO USAGE_FAILBACK_PRINT
IF (%4) == () GOTO USAGE_FAILBACK_PRINT
IF (%5) == () GOTO USAGE_FAILBACK_PRINT
IF (%6) == () GOTO USAGE_FAILBACK_PRINT
IF (%7) == () GOTO USAGE_FAILBACK_PRINT
IF (%8) == () GOTO START_FS_FAILBACK

:START_FS_FAILBACK

echo "***********Starting Fileserver Failback ***********"

REM remove Primary Server NetBIOS name from the Secondary Server
echo Deleting Primary Server netbios name...

@Echo on
WinOp.exe NETBIOS -delete %PRIMARY_SERVER_NAME%
@Echo off

echo .
echo Stopping browser service and lanmanserver...
@echo on
net stop Browser
net stop DFS
net stop lanmanserver
@echo off

echo Generating tag for proctected volumes...
@echo on
vacp.exe -v %VOLUMES% -t BM_FS_%random%%random% -remote -serverdevice %SERVERDEVICE% -serverip %SERVERIP% -csip %CSIP%
@echo off

IF %errorlevel% EQU 0 GOTO SUCCESS_EXIT

echo Starting computer browser and lanmanserver service...
@echo on
net start lanmanserver
net start Browser
net start DFS
@echo off

echo "Exiting abnormally......."
GOTO FAILED_EXIT
 
:SUCCESS_EXIT
echo Successfully created application consistency tags

mkdir "..\Application Data\replshares"
regedit /E "..\Application Data\replshares\shares.reg" HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\lanmanserver\Shares

echo Removing Primary Server HOST SPN entries from Secondary Server Account at AD
@echo on
WinOP.exe SPN -deleteHost  %SECONDARY_SERVER_NAME%:%PRIMARY_SERVER_NAME%
@echo off

echo Adding Primary Server HOST SPN entries to Primary Server Account at AD
@echo on
WinOp.exe SPN -addHost  %PRIMARY_SERVER_NAME%
@echo off

echo "*******Fileserver failback completed********"

GOTO USAGE_EXIT

:FAILED_EXIT
echo Failed to creat application consistency tags

echo "*******Fileserver failback Failed********"

GOTO USAGE_EXIT


:FS_FASTFAILBACK

SET PRIMARY_SERVER_NAME=%2
SET SECONDARY_SERVER_NAME=%3

IF (%2) == () GOTO USAGE_FASTFAILBACK_PRINT
IF (%3) == () GOTO USAGE_FASTFAILBACK_PRINT
IF (%4) == () GOTO START_FS_FASTFAILBACK

:START_FS_FASTFAILBACK

echo ***********Starting Fileserver Fast Failback ***********

echo Deleting Primary Server NetBios from Secondary Server
@echo on
WinOp.exe NETBIOS -delete %PRIMARY_SERVER_NAME%
WinOp.exe NETBIOS -delete %SECONDARY_SERVER_NAME%
@echo off

echo Stopping Computer browser service and lanmanserver service
REM net stop dfs
@echo on
net stop Browser
net stop lanmanserver
@echo off

echo Removing Primary Server HOST SPN entries from Secondary Server account at AD
@echo on
WinOP.exe SPN -deleteHost  %SECONDARY_SERVER_NAME%:%PRIMARY_SERVER_NAME%
@echo off

echo Adding Primary Server HOST SPN entries to Primary Server account at AD
@echo on
WinOp.exe SPN -addHost  %PRIMARY_SERVER_NAME%
@echo off
REM echo Starting Lanmanserver and Computer Browser service
REM net start lanmanserver
REM net start browser
REM net start dfs

echo **********Fileserver Fast Failback is completed************

:USAGE_EXIT
