@echo off

if "%1"=="VCON" goto VCON
if "%1"=="UCX" goto UCX
if "%1"=="UA" goto UA
if "%1"=="" goto End

:VCON
del "%USERPROFILE%\Desktop\vContinuum.lnk"
del %systemdrive%\Temp\reset_cxpath.bat
goto End

:UCX
del "%USERPROFILE%\Desktop\cspsconfigtool.lnk"
goto End

:UA
del "%USERPROFILE%\Desktop\hostconfigwxcommon.lnk"
goto End

:End
