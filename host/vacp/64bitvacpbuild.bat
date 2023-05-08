@echo off
SETLOCAL ENABLEDELAYEDEXPANSION
Set MSSDK=
Set VCENVCMD=
Set VCENVCMDRUNX64=

@for /f "usebackq tokens=3,*" %%f in (`reg query "HKLM\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v6.1" /v InstallationFolder`) do set MSSDK=%%f %%g
@if "%1" == "RELEASE" (
	Set VCENVCMD=!MSSDK!Bin\SetEnv.Cmd
	Set VCENVCMDRUNX64="!VCENVCMD!" /2003 /x64 /Release
	@call !VCENVCMDRUNX64!
	@nmake /f MakefileX64 clean_release
	@nmake /f MakefileX64 release
) 

@if "%1" == "DEBUG" (
	Set VCENVCMD=!MSSDK!Bin\SetEnv.Cmd
	Set VCENVCMDRUNX64="!VCENVCMD!" /2003 /x64 /Debug
	@call !VCENVCMDRUNX64!
	@nmake /f MakefileX64 clean_debug
	@nmake /f MakefileX64 Debug
)
ENDLOCAL