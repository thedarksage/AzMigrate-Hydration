@REM
@REM This sample script to issue tags for Pervasive database
@REM
@REM Usage:
@REM
@REM C:\Program Files\InMage Systems\consistency\Pervasive_consistency.bat <PervasivePath> <PervasiveDB> <PervasiveDrive>
@REM
@REM Where
@REM      Pervasive Path:
@REM       	Pervasive Binary Path
@REM
@REM      Pervasive DB Name:
@REM      	Pervasive database Name
@REM
@REM      Pervasive Drive Name:
@REM      	Pervasive Drive which is being Protected
@REM
@REM
@REM eg: 
@REM
@REM C:\Program Files\InMage Systems\consistency\Pervasive_consistency.bat E:\PVSW\Bin inmage F:
@REM


@REM
@REM Get the Pervasive path and DB Name
@REM

set PERVASIVE_PATH=%1
set PERVASIVE_DB=%2
set PERVASIVE_DRIVE=%~d3

@REM
@REM set path to include path for sqlutil - PERVASIVE_PATH and path for VACP
@REM

set path=%path%;%~dp0\..;%~dp0;%PERVASIVE_PATH%;

set CURRDATE=%TEMP%\CURRDATE.TMP
set CURRTIME=%TEMP%\CURRTIME.TMP

DATE /T > %CURRDATE%
TIME /T > %CURRTIME%

Set PARSEARG="eol=; tokens=1,2,3,4* delims=/, "
For /F %PARSEARG% %%i in (%CURRDATE%) Do SET DD=%%k%
For /F %PARSEARG% %%i in (%CURRDATE%) Do SET MM=%%j%
For /F %PARSEARG% %%i in (%CURRDATE%) Do SET YYYY=%%l%

Set PARSEARG="eol=; tokens=1,2,3* delims=:, "
For /F %PARSEARG% %%i in (%CURRTIME%) Do Set HHMM=%%i%%j%%k

Set tag=Perv_%PERVASIVE_DB%_%YYYY%_%MM%_%DD%_%HHMM%_tag

  VER |find /i "Windows 95" >NUL
  IF NOT ERRORLEVEL 1 GOTO ERROR_EXIT

  VER |find /i "Windows 98" >NUL
  IF NOT ERRORLEVEL 1 GOTO ERROR_EXIT

  VER |find /i "Windows Millennium" >NUL
  IF NOT ERRORLEVEL 1 GOTO ERROR_EXIT
  
  VER | find "NT" > nul
  IF %errorlevel% EQU 0 GOTO ERROR_EXIT

  VER | find "XP" > nul
  IF %errorlevel% EQU 0 GOTO ERROR_EXIT

  VER | find "2000" > nul
  IF %errorlevel% EQU 0 GOTO w2k

goto w2k3

:ERROR_EXIT

REM Report error and exit

ECHO "Application Consistecy is not supported on this platform"
ECHO "It is supported on Windows 2003 and Windows 2000 only"
GOTO FAIL_EXIT


:w2k
sqlutil -startbu %PERVASIVE_DB%
sync %PERVASIVE_DRIVE%
vacp2k -v %PERVASIVE_DRIVE%: -t %tag%
sqlutil -endbu %PERVASIVE_DB%
IF %errorlevel% EQU 0 GOTO SUCCESS_EXIT
GOTO FAIL_EXIT

:w2k3
sqlutil -startbu %PERVASIVE_DB%
vacp -v %PERVASIVE_DRIVE%: -t %tag%
sqlutil -endbu %PERVASIVE_DB%
IF %errorlevel% EQU 0 GOTO SUCCESS_EXIT
GOTO FAIL_EXIT

:FAIL_EXIT
ECHO "Exiting abnormally......."
exit 1


:SUCCESS_EXIT
ECHO "Successfully created application consistency tags"
ECHO "Existing gracefully...."
exit 0
