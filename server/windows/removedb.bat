@echo off

set Root_Passwd=%1
set MYSQL_EXE=%2

%MYSQL_EXE% -u root -p%Root_Passwd% -e "drop database svsdb1"
%MYSQL_EXE% -u root -p%Root_Passwd% -e "delete from mysql.user where user='svsystems'"
%MYSQL_EXE% -u root -p%Root_Passwd% -e "SET PASSWORD FOR 'root'@'localhost' = PASSWORD('')"


