@echo off

del /Q %systemdrive%\Temp\LatestvSphereexist.txt
del /Q %systemdrive%\Temp\unsupportvSphere.txt

Perl -MVMware::VIRuntime -e print
if %errorlevel% equ 0 goto CheckvSphereVersion
if %errorlevel% NEQ 0 exit

:CheckvSphereVersion
Perl -MVMware::VILib -e "print $Util::script_version" 2>NULL | findstr "^5.1"
if %errorlevel% equ 0 echo "5.1" >  %systemdrive%\Temp\LatestvSphereexist.txt
if %errorlevel% NEQ 0 goto CheckvSphereVersion5.5
goto End
:CheckvSphereVersion5.5
Perl -MVMware::VILib -e "print $Util::script_version" 2>NULL | findstr "^5.5"
if %errorlevel% equ 0 echo "5.5" >  %systemdrive%\Temp\LatestvSphereexist.txt
if %errorlevel% NEQ 0 echo "vSphere perl is not supported " >  %systemdrive%\Temp\unsupportvSphere.txt

:End