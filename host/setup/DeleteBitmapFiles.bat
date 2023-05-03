@echo off 

REG QUERY "HKLM\SOFTWARE\Wow6432Node" > NUL 2>&1

if %errorlevel% equ 0 (

FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "HKLM\SOFTWARE\Wow6432Node\SV Systems\VxAgent" /v InstallDirectory') DO SET vxpath=%%B
)

if %errorlevel% NEQ 0 (

FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "HKLM\SOFTWARE\SV Systems\VxAgent" /v InstallDirectory') DO SET vxpath=%%B
)

"%vxpath%\drvutil.exe" --stopf -StopAll > "%vxpath%\DeletBitmapFiles.log"
if %errorlevel% EQU 0       exit /B 0
if %errorlevel% NEQ 0       exit /B 1
