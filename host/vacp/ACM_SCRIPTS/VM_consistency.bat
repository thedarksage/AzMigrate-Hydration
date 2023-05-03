@ECHO ON
SETLOCAL
REM Set PATH
set PATH=%PATH%;%~dp0\..;%~dp0;

REM Set Volumes
 SET VOLUMES=%1
REM Set Application Name and Tag name 
 SET APPLICATION_BOOKMARK_MSG=%2
 SET CRASH_CONSISTNECY="FALSE"

IF (%1) == ()   GOTO USAGE_PRINT
IF (%2) == ()   SET APPLICATION_BOOKMARK_MSG="BM_VM%random%%random%"
if /i (%2) == (-CrashConsistency) GOTO PROCESS_CRASH_CONSISTENCY_DATA 
if /i (%2) == ("-CrashConsistency") GOTO PROCESS_CRASH_CONSISTENCY_DATA 

GOTO BEGIN

:PROCESS_CRASH_CONSISTENCY_DATA
SET CRASH_CONSISTNECY="TRUE"
set APPLICATION_BOOKMARK_MSG="BM_VM%random%%random%"

:BEGIN
IF /i (%3) == (-CrashConsistency) set CRASH_CONSISTNECY="TRUE"
IF /i (%3) == ("-CrashConsistency") set CRASH_CONSISTNECY="TRUE"

REM Do not modify the below code ....

REM OS Related Checks

  VER |find /i "Windows 95" >NUL
  IF NOT ERRORLEVEL 1 GOTO ERROR_EXIT

  VER |find /i "Windows 98" >NUL
  IF NOT ERRORLEVEL 1 GOTO ERROR_EXIT

  VER |find /i "Windows Millennium" >NUL
  IF NOT ERRORLEVEL 1 GOTO ERROR_EXIT

  VER | find "2000" > nul
  IF %errorlevel% EQU 0 GOTO WIN2K_CMD

  VER | find "NT" > nul
  IF %errorlevel% EQU 0 GOTO ERROR_EXIT

  VER | find "XP" > nul
  IF %errorlevel% EQU 0 GOTO WINXP_CMD

REM If NOTHING matches Assume Windows 2003

  GOTO WIN2K3_CMD


:ERROR_EXIT

REM Report error and exit

 ECHO "Application Consistecy is not supported on this platform"
 ECHO "It is supported on Windows XP, Windows 2003 and above"
 GOTO FAIL_EXIT

REM Create Application Tags on Windows XP
:WINXP_CMD
 set VACP_CMD=vacpxp.exe
 GOTO GENERATE_TAGS

:WIN2K3_CMD
 set VACP_CMD=vacp.exe
 GOTO GENERATE_TAGS

:WIN2K_CMD
 set VACP_CMD=vacp2k.exe
 GOTO WIN2K_CONSISTENCY


:WIN2K_CONSISTENCY
 %VACP_CMD% -v %VOLUMES% -t %APPLICATION_BOOKMARK_MSG% > consistency_tag.txt
 IF %errorlevel% EQU 0 GOTO SUCCESS_EXIT
 type consistency_tag.txt
 ECHO "Exiting abnormally......."
 exit 1


:GENERATE_TAGS
IF /i %CRASH_CONSISTNECY% == "TRUE"  GOTO CRASH_CONSISTENCY

:CONSISTENCY
 %VACP_CMD% -systemlevel -t %APPLICATION_BOOKMARK_MSG% > consistency_tag.txt
 IF %errorlevel% EQU 0 GOTO SUCCESS_EXIT
 type consistency_tag.txt
 ECHO "Exiting abnormally......."
 exit 1

:CRASH_CONSISTENCY
 %VACP_CMD% -x -v %VOLUMES% -t %APPLICATION_BOOKMARK_MSG% > consistency_tag.txt
 IF %errorlevel% EQU 0 GOTO SUCCESS_EXIT
 type consistency_tag.txt
 ECHO "Exiting abnormally......."
 exit 1
 
:SUCCESS_EXIT
 type consistency_tag.txt
 move consistency_tag.txt ../Failover/data
 ECHO "Successfully created consistency tags"
 ECHO "Exiting gracefully...."
 exit 0

:USAGE_PRINT
 ECHO USAGE:
 ECHO This script must be run from production server(original source host).
 ECHO      VM_consistency.bat "<List of Volumes under replication>" <Tag Name> -CrashConsistency
 ECHO   The first argument must be list of protected volumes for which tag should be generated.
 ECHO      Use double quotes for specifying the volume mount point with spaces.
 ECHO   The second argument is the tag name
 ECHO   The third argument is to given only if we want crash consistency.
 ECHO Ex: VM_consistency.bat "C:;E:;C:\Mount Points\MP1;" MYTAG 
 GOTO FAIL_EXIT

:FAIL_EXIT
 exit 1