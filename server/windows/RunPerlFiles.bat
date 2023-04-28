@echo off
set INSTALL_DIR=%1
c:\strawberry\perl\bin\perl.exe %INSTALL_DIR%\bin\rrd_tune.pl >> %systemdrive%\Temp\rrdtunefileoutput.txt