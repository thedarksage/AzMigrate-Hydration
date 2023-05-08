@echo on

rem This script will validate the success or failure of consistency validation of MSExchange based on the ".status" file. This will generate flushing conistency tag only if found Successful status" file.

rem unqid is the id which should be same as the id specified in the "exchange_consistency_validation.bat" file on the target host.


SETLOCAL
REM Set PATH
set PATH=%PATH%;%~dp0\..;%~dp0;

REM Delete any existing old SeqNum.bat file
del /F SeqNum.bat
ren SequenceNum.status SeqNum.bat
REM Extract the Sequence number from the SeqNum.bat into unqid variable
set /p unqid=<SeqNum.bat

set CURRDATE=%TEMP%\CURRDATE.TMP
set CURRTIME=%TEMP%\CURRTIME.TMP

DATE /T > %CURRDATE%
TIME /T > %CURRTIME%

Set PARSEARG="eol=; tokens=1,2,3,4* delims=/, "
For /F %PARSEARG% %%i in (%CURRDATE%) Do SET YYYY=%%l%
For /F %PARSEARG% %%i in (%CURRDATE%) Do SET MM=%%k%
For /F %PARSEARG% %%i in (%CURRDATE%) Do SET DD=%%j%

Set PARSEARG="eol=; tokens=1,2,3* delims=:, "
For /F %PARSEARG% %%i in (%CURRTIME%) Do Set HHMM=%%i%%j%%k


IF EXIST "%~dp0%unqid%_Failed.status" goto end


IF EXIST "%~dp0%unqid%_Success.status" goto vacp1


IF NOT EXIST "%~dp0%unqid%_Success.status" goto end

:vacp1
vacp -f -a Exchange

IF EXIST  "%~dp0%unqid%_Success.status" goto ren1

:ren1
del  "%~dp0%unqid%_Success.status"
exit 0

:end
vacp -a Exchange
del  "%~dp0%unqid%_Failed.status"
exit 1