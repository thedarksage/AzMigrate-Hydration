@echo on

rem This script will discover SQL server configuration

SETLOCAL
REM Set PATH
set PATH=%PATH%;%~dp0\..;%~dp0;

Application -discover -app Exchange
exit 0