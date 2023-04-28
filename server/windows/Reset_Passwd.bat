@echo off

set MYSQL_DIR=%1
set Root_Passwd=%2
set INSTALL_DIR=%3
%MYSQL_DIR%bin\mysql.exe -u root --execute="set global general_log=1"

if %errorlevel% EQU 0 (
	goto RESETPASSWORD
)

%MYSQL_DIR%bin\mysql.exe -u root -p%Root_Passwd% --execute="set global general_log=1"

if %errorlevel% NEQ 0 (	
	echo "Cannot log into MySQL with the default root password or with the user provided root password" >> %INSTALL_DIR%\sql.log
	echo %Root_Passwd% >> %INSTALL_DIR%\sql.log
	exit /b 1
)
else (
	exit /b 0
)


:RESETPASSWORD
%MYSQL_DIR%bin\mysqladmin.exe -u root password %Root_Passwd%

if %errorlevel% NEQ 0 (
	echo "Could not reset MySQL root password" >> %INSTALL_DIR%\sql.log
	echo %Root_Passwd% >> %INSTALL_DIR%\sql.log
	exit /b 2
)
exit /b 0