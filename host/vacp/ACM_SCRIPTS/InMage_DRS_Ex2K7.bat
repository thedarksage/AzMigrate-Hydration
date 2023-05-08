@ECHO ON
SETLOCAL
REM Set PATH
set PATH=%PATH%;%~dp0\..;%~dp0;

SET ACTION=%1
IF (%1) == () GOTo USAGE_PRINT
IF /i (%1) == (START) GOTO START_SERVICE
IF /i (%1) == (STOP) GOTO STOP_SERVICE

:USAGE_PRINT
echo USAGE:
echo     InMage_DRS_Ex2K7.bat <START|STOP>
echo The Script takes one parameter. It must be either 
echo START or STOP. Depending on the parameter Exchange services 
echo will be started or stopped.
echo
echo Ex: InMage_DRS_Ex2K7.bat START
GOTO  USAGE_EXIT

:START_SERVICE
net start MSExchangeSA
net start MSExchangeIS

REM "***Add additional services here****"

ECHO "****Successfully started the services*****"
GOTO EXIT


:STOP_SERVICE
REM "***ADD additional services here****"
net stop MSExchangeIS
net stop MSExchangeSA

ECHO "****Successfully stopped the services*****"
:EXIT