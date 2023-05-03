@echo off

setlocal

rem Extracting php zip file
cscript C:\Temp\unzip.vbs

rem create needed directories as php will not create them
mkdir c:\thirdparty\php5nts\sessiondata
mkdir c:\thirdparty\php5nts\uploadtemp