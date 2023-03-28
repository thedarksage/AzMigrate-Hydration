@echo OFF
set _argcActual=0
set _argcExpected=3

SET USER=%1 
SET PASSWORD=%2 
SET MYSQL_PATH=%3

for %%i in (%*) do set /A _argcActual+=1

if %_argcActual% NEQ %_argcExpected% (

  call :USAGE %0%, "script required 3 arguments please see usgae"

   goto:END
)


%MYSQL_PATH%\mysql -u %USER% -p%PASSWORD% <lock.sql
IF ERRORLEVEL 1 GOTO ERR
echo Issuing consistency Tag.
..\vacp -v all -t MySql_%random%%random%
echo Issuing consistency Tag is done... 
%MYSQL_PATH%\mysql -u %USER% -p%PASSWORD% <unlock.sql
IF ERRORLEVEL 1 GOTO ERR
GOTO CON
:USAGE
echo Usage: %~1 MySQL-UserName MySQL-Password "MySQL-exe-path"
echo script is aborting. 
GOTO END
:ERR
echo Please provide valid arguments.... 
echo script is aborting.
GOTO END
:CON
echo script executed successfully.
:END
