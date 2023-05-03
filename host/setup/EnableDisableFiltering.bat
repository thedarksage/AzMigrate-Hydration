@echo off 

set RES=0

cd "Install-Directory"

echo Enter EnableDisableFiltering.bat %1 >> Indskfltservice_output.txt
drvutil.exe --gdv >> Indskfltservice_output.txt
drvutil.exe --startf %1 >> Indskfltservice_output.txt

drvutil.exe --startf %1 | findstr /C:"Returned Success from start filtering DeviceIoControl" >> Indskfltservice_output.txt 2>&1
if %errorlevel% NEQ 0 goto Cleanup 
if %errorlevel% EQU 0 goto VerifyPoolAllocation

:VerifyPoolAllocation
TIMEOUT /T 10 /NOBREAK
drvutil.exe --ps %1 | findstr /C:"Driver failed to allocate data pool" >> Indskfltservice_output.txt 2>&1
if %errorlevel% NEQ 0 goto FirstCommand 
if %errorlevel% EQU 0 goto StopFiltering

:FirstCommand
drvutil.exe --ps %1 | findstr /C:"Bytes of Total changes pending = 0" >> Indskfltservice_output.txt 2>&1
if %errorlevel% NEQ 0 goto output
if %errorlevel% EQU 0 goto SecondCommand

:SecondCommand
drvutil.exe --ps %1 | findstr /C:"Bytes of changes written to bitmap = 0" >> Indskfltservice_output.txt 2>&1
if %errorlevel% NEQ 0 goto output
if %errorlevel% EQU 0 goto StopFiltering

:output
echo  ----------------------------------------------------------------------------- >> Indskfltservice_output.txt
echo  Disk filter is tracking the changes for system volume spanning across disk %1 >> Indskfltservice_output.txt
echo  ----------------------------------------------------------------------------- >> Indskfltservice_output.txt
set RES=1
goto StopFiltering

:StopFiltering
echo   ---------------STATS---------------------- >> Indskfltservice_output.txt
drvutil.exe --ps %1 >> Indskfltservice_output.txt
echo   ------------------------------------------ >> Indskfltservice_output.txt
drvutil.exe --stopf %1 >> Indskfltservice_output.txt 2>&1

:Cleanup
if %RES%==1 (
    echo End EnableDisableFiltering.bat >> Indskfltservice_output.txt
    exit 0
) else (
    echo ----------------------------------------------------------------------------- >> Indskfltservice_output.txt
    echo Disk filter is not able to track the changes for system volume spanning across disk %1 >> Indskfltservice_output.txt
    echo ----------------------------------------------------------------------------- >> Indskfltservice_output.txt
	echo End EnableDisableFiltering.bat >> Indskfltservice_output.txt
    exit -1
)