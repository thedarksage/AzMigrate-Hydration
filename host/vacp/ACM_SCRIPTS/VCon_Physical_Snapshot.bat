@echo on

rem This Will collect all the required files by running first command and next it will take the snapshot on running the next command.
rem It will run first "EsxUtilWin -role physnapshot" to get the required files,
rem Next it will run "EsxUtil -physnapshot" to take the physical snapshot.

SETLOCAL
REM Set PATH
set PATH=%PATH%;%~dp0\..;%~dp0;

SET DIRECTORY_PATH=%1
SET CX_PATH=%2
SET PLAN_ID=%3
SET PLAN_OP=%4

IF (%1) == () (
  ECHO "Running command EsxUtilWin without any directory details.."
  EsxUtilWin -role physnapshot > EsxUtilWin_FxJob.txt
  GOTO LEVEL1
)

IF (%4) == () (
  ECHO "Running command EsxUtilWin without Operation entry."
  EsxUtilWin -role physnapshot -d %DIRECTORY_PATH% -cxpath %CX_PATH% -planid %PLAN_ID% > EsxUtilWin_FxJob.txt
) ELSE (
  ECHO "Running command EsxUtilWin with All the Entries."
  EsxUtilWin -role physnapshot -d %DIRECTORY_PATH% -cxpath %CX_PATH% -planid %PLAN_ID% -op %PLAN_OP% > EsxUtilWin_FxJob.txt
)

:LEVEL1
IF %errorlevel% EQU 0 GOTO SNAPSHOT_CMD
type EsxUtilWin_FxJob.txt
move EsxUtilWin_FxJob.txt ../Failover/data
ECHO "Physical Snapshot operation Failed while running the EsxUtilWin command."
ECHO "Exiting abnormally......."
exit 1
 
:SNAPSHOT_CMD
 type EsxUtilWin_FxJob.txt
 move EsxUtilWin_FxJob.txt ../Failover/data

 IF (%1) == () (
   ECHO "Running command EsxUtil without any directory details.."
   EsxUtil -physnapshot > EsxUtil_FxJob.txt
   GOTO LEVEL2
 )
 
 IF (%4) == () (
   ECHO "Running command EsxUtil without Operation entry."
   EsxUtil -physnapshot -d %DIRECTORY_PATH% -cxpath %CX_PATH% -planid %PLAN_ID% > EsxUtilWin_FxJob.txt
 ) ELSE (
   ECHO "Running command EsxUtil with All the Entries."
   EsxUtil -physnapshot -d %DIRECTORY_PATH% -cxpath %CX_PATH% -planid %PLAN_ID% -op %PLAN_OP% > EsxUtilWin_FxJob.txt
 )
 
:LEVEL2
 IF %errorlevel% EQU 0 GOTO SUCCESS_EXIT
 type EsxUtil_FxJob.txt
 move EsxUtil_FxJob.txt ../Failover/data
 ECHO "Physical Snapshot operation Failed while running the EsxUtil command."
 ECHO "Exiting abnormally......."
 exit 1
 
:SUCCESS_EXIT
 type EsxUtil_FxJob.txt
 move EsxUtil_FxJob.txt ../Failover/data
 ECHO "Successfully completed the physical snapshot operation"
 ECHO "Exiting gracefully...."
 exit 0