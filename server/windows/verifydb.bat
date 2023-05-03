@echo off

set Root_Passwd=%1
set User_Passwd=%2
set MYSQL_EXE=%3
set INSTALL_DIR=%4

%MYSQL_EXE% -u root -p%Root_Passwd% < "c:\Temp\create_user.sql"

if %errorlevel% NEQ 0 (
	echo "Could not create svsystems user" >> %INSTALL_DIR%\sql.log
	exit /b 1
)

%MYSQL_EXE% -u svsystems -p%User_Passwd% svsdb1 < "c:\Temp\svsdb.sql"

if %errorlevel% NEQ 0 (
	echo "Could not create svsdb1 database" >> %INSTALL_DIR%\sql.log
	exit /b 2
)

exit /b 0