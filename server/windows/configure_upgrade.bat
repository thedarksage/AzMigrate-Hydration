@echo off

set Root_Passwd=%1
set User_Passwd=%2
set MYSQL_DIR=%3
set INSTALL_DIR=%4

%MYSQL_DIR%bin\mysqldump.exe --add-drop-table -u root -p%Root_Passwd% svsdb1 > %INSTALL_DIR%\var\cs_db_backup\9.55_db_backup.sql
if %errorlevel% NEQ 0 (
    echo "Unable to take backup of svsdb1." >> C:\Temp\Db_Upgrade.log
    echo "Starting W3SVC Service" >> C:\Temp\Db_Upgrade.log
	net start W3SVC
	EXIT /B 3
)

if exist %INSTALL_DIR%\var\cs_db_backup\9.55_db_backup.sql (
    echo "Database backup file is created successfully." >> C:\Temp\Db_Upgrade.log
) else (
    echo "Unable to create database backup file." >> C:\Temp\Db_Upgrade.log
	echo "Starting W3SVC Service" >> C:\Temp\Db_Upgrade.log
	net start W3SVC
    EXIT /B 2
)

cd /d %INSTALL_DIR%\Upgrade
C:\thirdparty\php5nts\php.exe svsdb_upgrade.php >> C:\Temp\Db_Upgrade.log

echo "svsdb_upgrade.php return code: %errorlevel%" >> C:\Temp\Db_Upgrade.log
if %errorlevel% NEQ 0 (
	echo "Unable to execute svsdb_upgrade.php." >> C:\Temp\Db_Upgrade.log
	echo "Dropping the svsdb1 database and restoring the svsdb1 database with the backup." >> C:\Temp\Db_Upgrade.log
	echo drop database svsdb1 | %MYSQL_DIR%bin\mysql.exe -u root -p%Root_Passwd%
	%MYSQL_DIR%bin\mysql.exe -u root -p%Root_Passwd% < "c:\Temp\create_user.sql"
	%MYSQL_DIR%bin\mysql.exe -u svsystems -p%User_Passwd% svsdb1 < %INSTALL_DIR%\var\cs_db_backup\9.55_db_backup.sql
	net start W3SVC
	exit /B 1
)

exit /B 0

