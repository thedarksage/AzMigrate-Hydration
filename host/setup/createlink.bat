@echo off

if "%1"=="VCON" goto VCON
if "%1"=="UA" goto UA
if "%1"=="CX" goto CX
if "%1"=="" goto End

:VCON
cscript %systemdrive%\Temp\createlink.vbs "VCON" "%USERPROFILE%\Desktop\vContinuum.lnk"  %2 "%systemdrive%\Temp\vConti_sc.ico"
goto End

:UA
cscript %2 "UA" "%USERPROFILE%\Desktop\hostconfigwxcommon.lnk" %3 
goto End

:CX
cscript %2 "CX" "%USERPROFILE%\Desktop\cspsconfigtool.lnk" %3 
goto End

:End
