@echo off
SETLOCAL

set hostName=

cd ..\Failover\ConsistencyValidation\

REM Delete any existing old files
IF Exist SeqNum.bat del /F SeqNum.bat
IF Exist SequenceNum.status (ren SequenceNum.status SeqNum.bat) ELSE (goto STATUSFILENOTFOUND)

REM Extract the Sequence number from the SeqNum.bat into uniqueId variable
echo.
echo Reading XML file...
set /p uniqueId=<SeqNum.bat

if exist XML_%uniqueId%.xml goto CHECKSOURCE

if not exist XML_%uniqueId%.xml goto ERROR


:CHECKSOURCE
FOR /F "tokens=3 delims=><" %%j in (
'type XML_%uniqueId%.xml ^| find "<IsSourceCluster>"'
) do (
if /I %%j==TRUE (goto DELETE)
)


:CHECKHOSTNAME
echo Checking for Host name match...
echo.
FOR /F "tokens=3 delims=><" %%h in (
'type XML_%uniqueId%.xml ^| find "<Host>"'
) do (
if /I %computername%==%%h (
echo Local Host Name       : %computername%
echo Host Name in XML File : %%h
echo Host Name match	      : Found
echo.
echo Continue to flush Exchange logs	
) ELSE (
set hostName=%%h
goto HOSTMATCHNOTFOUND
)
)


:DELETE
echo.
echo Started deleting log files...
echo.
echo Please Wait while the log files are deleted...
FOR /F "tokens=3 delims=><" %%a in (
'type XML_%uniqueId%.xml ^| find "<FilePath>"'
) do (
IF exist "%%a" (
DEL "%%a"
IF %errorlevel%==0 (echo Deleted File - %%a >> LogsFlushed_%uniqueId%.txt ) ELSE (
echo Delete Failed - %%a >> LogsNotFlushed_%uniqueId%.txt
) 
) ELSE (
echo %%a does not exist >> LogsNotFlushed_%uniqueId%.txt
) 
)
goto ENDGRACEFULLY


:ERROR
echo "XML_%uniqueId%.xml" File does not exist
exit /B 1


:HOSTMATCHNOTFOUND
echo Local Host Name       : %computername%
echo Host Name in XML File : %hostName%
echo Host Name match	   : NotFound
echo Unable to continue Exchange Log flush Job
exit /B 2


:STATUSFILENOTFOUND
echo.
echo SequenceNum.status file not found
exit /B 3


:ENDGRACEFULLY
echo.
echo Log Flush Successful. Please verify the results in detailed report "LogsFlushed_%uniqueId%.txt,LogsNotFlushed_%uniqueId%.txt"
echo.
echo **********End of Exchange LogFlush**********
exit /B 0