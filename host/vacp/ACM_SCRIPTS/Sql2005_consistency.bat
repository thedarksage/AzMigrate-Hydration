@ECHO ON
SETLOCAL
REM Set PATH
set PATH=%PATH%;%~dp0\..;%~dp0;
REM Set Application Name and Tag name 
set APPLICATION_NAME="Sql2005"
set APPLICATION_BOOKMARK_MSG="BM_SQL2K5%random%%random%"

if "%1" == "" (goto NORMAL_PROCESSING)
..\winop.exe cluster -checkactivenode %1

if %errorlevel% EQU 0  goto NORMAL_PROCESSING
if %errorlevel% EQU 1   goto PASSIVE_NODE
if %errorlevel% EQU -1  goto NOT_A_CLUSTER
if %errorlevel% EQU -2  goto INVALID_USAGE



:NORMAL_PROCESSING


REM Do not modify the below code ....

REM OS Related Checks

  VER |find /i "Windows 95" >NUL
  IF NOT ERRORLEVEL 1 GOTO ERROR_EXIT

  VER |find /i "Windows 98" >NUL
  IF NOT ERRORLEVEL 1 GOTO ERROR_EXIT

  VER |find /i "Windows Millennium" >NUL
  IF NOT ERRORLEVEL 1 GOTO ERROR_EXIT

  VER | find "2000" > nul
  IF %errorlevel% EQU 0 GOTO ERROR_EXIT

  VER | find "NT" > nul
  IF %errorlevel% EQU 0 GOTO ERROR_EXIT

  VER | find "XP" > nul
  IF %errorlevel% EQU 0 GOTO WINXP_CMD

REM If NOTHING matches Assume Windows 2003

  GOTO WIN2K3_CMD


:ERROR_EXIT

REM Report error and exit

 ECHO "Application Consistecy is not supported on this platform"
 ECHO "It is supported on Windows 2003 and Windows XP only"
 GOTO FAIL_EXIT

REM Create Application Tags on Windows XP
:WINXP_CMD
 set VACP_CMD=vacpxp.exe
 GOTO GENERATE_TAGS

:WIN2K3_CMD
 set VACP_CMD=vacp.exe
 GOTO GENERATE_TAGS



:GENERATE_TAGS

 %VACP_CMD% -a %APPLICATION_NAME% -t %APPLICATION_BOOKMARK_MSG% > consistency_tag.txt
 IF %errorlevel% EQU 0 GOTO SUCCESS_EXIT
  type consistency_tag.txt
  ECHO "Exiting abnormally......."
 exit 1
 
 
 :SUCCESS_EXIT
 type consistency_tag.txt
 move consistency_tag.txt ../Failover/data
 ECHO "Successfully created application consistency tags"
 ECHO "Existing gracefully...."
 exit 0

:PASSIVE_NODE
echo
echo This node is a Passive Node of the Cluster.
exit 0

:NOT_A_CLUSTER
echo
echo No Cluster available with the given virtual server name.
exit 1

:INVALID_USAGE
echo
echo Invalid Usage of this script. Check the parameter passed to this script.
exit 1
