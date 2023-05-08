@ECHO OFF
REM MoveLogFile.cmd
REM Parameters:
REM  First  : Source log file path
REM  Second : Target log file path
REM  Third  : Delay time in seconds
REM  Fourth : '/Delete' to delete the log file and this batch script (!)
IF %1.==. GOTO :Usage
IF NOT EXIST %1. (
	ECHO Error: Source log file '%1' not found.
	GOTO :End
)
IF %2.==. GOTO :Usage
IF NOT %3.==. PING -n %3 127.0.0.1
ECHO.>>%2
ECHO ***********************   New installation starts here   ***********************>>%2
ECHO.>>%2
TYPE %1>>%2
IF %4.==/Delete. (
	DEL /F/Q %1
	IF EXIST %1 ECHO Error deleting '%1'.
	DEL /F/Q "%0"
)
GOTO :End
:Usage
ECHO Log file mover - Appends a source log file to
ECHO  a target log file.
ECHO.
ECHO  MoveLogFile.cmd source target delay [/Delete]
ECHO   source  = Full path name of source log file
ECHO   target  = Full path name of target log file
ECHO   delay   = Delay time in seconds
ECHO   /Delete = Removes source file and this batch script (!)
ECHO.
:End

