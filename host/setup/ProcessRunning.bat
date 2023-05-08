@echo off

set ProcessName=%1

tasklist /FI "IMAGENAME eq %ProcessName%" 2>NUL | find /I /N "%ProcessName%">NUL
if %errorlevel% EQU 0   exit /B 1
if %errorlevel% NEQ 0   exit /B 2
