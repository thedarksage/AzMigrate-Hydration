@echo off

rem configures IIS to use windows CX
rem requires application directory (directory where CX admin/web is installed
rem optionally can specify IP address (default is ANY) and port (default is 80)
 
setlocal

set progName=%0

if "" == "%1" goto usage

set iis_vdir=
set iis_ip=
set iis_port=

:parseArgs
if "%1" == "" goto verifyIisArgs
if "%1" == "-d" (if "%iis_vdir%" == "" (set iis_vdir=%2&& shift && goto nextArg) else (echo -d already specified && goto usage))
if "%1" == "-i" (if "%iis_ip%" == "" (set iis_ip=%2&& shift && goto nextArg) else (echo -i already specified && goto usage))
if "%1" == "-p" (if "%iis_port%" == "" (set iis_port=%2&& shift && goto nextArg) else (echo -p already specified && goto usage))
echo ERROR invalid command line: "%*"
goto usage

:nextArg
shift
goto parseArgs

:verifyIisArgs
if "" == "%iis_vdir%" (echo ERROR: missing -d ^<app dir^> && goto usage)

rem set defauls if not provided
if "" == "%iis_ip%" set iis_ip=*
if "" == "%iis_port%" set iis_port=80

:: update IIS configuration
:: note when setting vdir the "/" is needed but not when setting site
:: Assigning * value for IP value under the bindings section in IIS.
%windir%\system32\inetsrv\appcmd.exe set site "Default Web Site" -bindings:http/*:%iis_port%:

rem disabling the firewall
netsh advfirewall set currentprofile state off
goto done

:usage
echo.
echo usage: %progName% ^-d ^<app dir^> [^-i ^<ip address^>] [^-p ^<port^>]
echo.
echo   ^-d ^<app dir^>    (required): application directory
echo   ^-i ^<ip address^> (optional): ip address to use. Default * (ANY)
echo   ^-p ^<port^>       (optional): port to listen on. Default 80
echo.
goto done

:done



