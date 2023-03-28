@echo off
SETLOCAL
set PATH=%PATH%;%~dp0\..;%~dp0;

IF (%1) == () GOTO USAGE_PRINT
IF /i (%1) == (Failover) GOTO FS_FAILOVER
IF /i (%1) == (Failback) GOTO FS_FAILBACK 
IF /i (%1) == (FastFailback) GOTO FS_FASTFAILBACK 

:USAGE_PRINT
echo USAGE:
echo 	FileServer-PrimaryServer.bat "<Operation>"
echo OPERATION can be any of the following
echo		Failover
echo		Failback
echo		Fastfailback
echo Example: FileServer-PrimaryServer.bat Failback
GOTO USAGE_EXIT

:USAGE_FAILOVER_PRINT
echo FAILOVER USAGE:
echo	FileServer-PrimaryServer.bat Failover "<Primary Server Name>" "<List of Volumes>" "<List of ServerDevices(LUN IDs>" "<Vacp Server IP>" "<CS Server IP>"
echo		- First parameter must be Failover 
echo		- Second parameter must be Primary Server Host Name
echo		- Third parameter must be list of protected volumes for which tag should be generated. 
echo		Use double quotes for specifying list of volumes separated by semicolon.
echo		- Fourth parameter must be server device names. Server devices are the LUN IDs(specify the LUN IDs for the respective list of volumes) in the Server.
echo		Use double quotes for specifying list of server device name separated by semicolon.
echo		- Fifth parameter must be Vacp Server IP(The Server IP where vacp Server exists) or PS Serevr IP.
echo		- sixth parameter must be CS server IP
echo Example:
echo		FileServer-PrimaryServer.bat  Failover  Primary_Server  "E:;F:\MNT1;G:"  "/dev/mapper/2000b0802a9002158;/dev/mapper/2000b0802a9002545"  10.0.25.26  10.0.1.10
GOTO USAGE_EXIT

:USAGE_FAILBACK_PRINT
echo FAILBACK USAGE:
echo	FileServer-PrimaryServer.bat Failback  "<Primary Server Host Name>"  "<Primary Server IP address>"
echo		- First parameter must be Failback 
echo		- Second parameter must be Primary Server host name.
echo		- Third parameter must be Primary Server IP address. 
echo Example:
echo		FileServer-PrimaryServer.bat  Failback  Primary_Server  10.0.135.1
GOTO  USAGE_EXIT

:USAGE_FASTFAILBACK_PRINT
echo FASTFAILBACK USAGE:
echo	FileServer-PrimaryServer.bat Fastfailback "<Primary Server Host Name>"  "{<Primary Server IP Address>"
echo		- First parameter must be Fastfailback
echo		- Second parameter must be Primary Server Host Name.
echo		- Third parameter must be Primary Server IP Address. 
echo Example:
echo		FileServer-PrimaryServer.bat  Fastfailback  Primary_Server  10.0.135.1
GOTO  USAGE_EXIT


:FS_FAILOVER

SET PRIMARY_SERVER_NAME=%2
SET VOLUMES=%3
SET SERVERDEVICE=%4
SET SERVERIP=%5
SET CSIP=%6

IF (%2) == () GOTO USAGE_FAILOVER_PRINT
IF (%3) == () GOTO USAGE_FAILOVER_PRINT
IF (%4) == () GOTO USAGE_FAILOVER_PRINT
IF (%5) == () GOTO USAGE_FAILOVER_PRINT
IF (%6) == () GOTO USAGE_FAILOVER_PRINT
IF (%7) == () GOTO START_FS_FAILOVER

:START_FS_FAILOVER

echo "***********Starting Fileserver failover ***********"

REM remove NetBIOS name from the Primary Server

echo Deleting the netbios name...
@echo on
WinOp.exe NETBIOS -delete %PRIMARY_SERVER_NAME%
@echo off

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

echo "*******Fileserver failover completed********"

GOTO USAGE_EXIT

:FAILED_EXIT
echo Failed to creat application consistency tags

echo "*******Fileserver failover Failed********"

GOTO USAGE_EXIT


:FS_FAILBACK

SET PRIMARY_SERVER_NAME=%2
SET IP_ADDRESS=%3

IF (%2) == () GOTO USAGE_FAILBACK_PRINT
IF (%3) == () GOTO USAGE_FAILBACK_PRINT
IF (%4) == () GOTO START_FS_FAILBACK

:START_FS_FAILBACK

echo ***********Starting Fileserver Failback***********

REM - ROLLBACK all the volumes to a consistency point

echo Importing registry to the target
@echo on
regedit /s "..\Application Data\replshares\shares.reg"
@echo off

echo Starting lanmanserver and Computer Browser service...
REM net start dfs
@echo on
net start lanmanserver
net start browser
@echo off

echo Starting DNS failback...
@echo on
dns.exe -failback -host %PRIMARY_SERVER_NAME% -ip %IP_ADDRESS% -fabric
@echo off

echo ***********Fileserver Failback is completed**********

GOTO USAGE_EXIT


:FS_FASTFAILBACK

SET PRIMARY_SERVER_NAME=%2
SET IP_ADDRESS=%3

IF (%2) == () GOTO USAGE_FASTFAILBACK_PRINT
IF (%3) == () GOTO USAGE_FASTFAILBACK_PRINT
IF (%4) == () GOTO START_FS_FASTFAILBACK

:START_FS_FASTFAILBACK

echo ************ Starting Fileserver fast failback***********

echo Starting lanmanserver and Computer Browser service...
REM net start dfs
@echo on
net start lanmanserver
net start browser
@echo off

echo Starting DNS failback...
@echo on
dns.exe -failback -host %PRIMARY_SERVER_NAME% -ip %IP_ADDRESS% -fabric
@echo off

echo ***********Fileserver fast failback is completed**********

:USAGE_EXIT
