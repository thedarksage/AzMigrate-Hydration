@echo off

set MYSQL_EXE=%1
set Root_Passwd=%2

%MYSQL_EXE% -u root -p%Root_Passwd% -e "delete from mysql.user where User='' or Host='127.0.0.1' or Host='::1'"

if %errorlevel% NEQ 0 (
	exit /b 1
)

exit /b 0