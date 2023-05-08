@echo off 



del /Q Check_Involfltservice.txt

drvutil.exe --startf %systemdrive% > Involfltservice_output.txt 2>&1
TIMEOUT /T 7 /NOBREAK


drvutil.exe --ps %systemdrive% | findstr /C:"Driver failed to allocate data pool" >> Involfltservice_output.txt 2>&1
if %errorlevel% NEQ 0 goto FirstCommand
if %errorlevel% EQU 0 goto StopFiltering

:FirstCommand
drvutil.exe --ps %systemdrive% | findstr /C:"Bytes of Total changes pending = 0" >> Involfltservice_output.txt 2>&1
if %errorlevel% NEQ 0 goto output
if %errorlevel% EQU 0 goto SecondCommand

:SecondCommand
drvutil.exe --ps %systemdrive% | findstr /C:"Bytes of changes written to bitmap = 0" >> Involfltservice_output.txt 2>&1
if %errorlevel% NEQ 0 goto output
if %errorlevel% EQU 0 goto StopFiltering

:output
echo  True > Check_Involfltservice.txt
goto StopFiltering

:StopFiltering
drvutil.exe --stopf %systemdrive% -DeleteBitmap >> Involfltservice_output.txt 2>&1
exit











