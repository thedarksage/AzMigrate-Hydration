@echo off

setlocal

set progName=%0

if "" == "%1" goto usage

set iis_vdir=

:parseArgs
if "%1" == "" goto verifyIisArgs
if "%1" == "-d" (if "%iis_vdir%" == "" (set iis_vdir=%2&& shift && goto nextArg) else (echo -d already specified && goto usage))
echo ERROR invalid command line: "%*"
goto usage

:nextArg
shift
goto parseArgs

:verifyIisArgs
if "" == "%iis_vdir%" (echo ERROR: missing -d ^<app dir^> && goto usage)

rem delete default web site

%windir%\system32\inetsrv\appcmd.exe delete site "Default Web Site"
sleep 20
%windir%\system32\inetsrv\appcmd.exe add site /name:"Default Web Site" -physicalPath:%iis_vdir%
netsh advfirewall set currentprofile state off
goto done

:usage
echo.
echo usage: %progName% ^-d ^<app dir^>
echo.
echo   ^-d ^<app dir^>    (required): application directory
echo.
goto done

:done