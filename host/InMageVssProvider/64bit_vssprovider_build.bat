@echo off
SETLOCAL ENABLEDELAYEDEXPANSION
Set MSSDK=
Set VCENVCMD=
Set VCENVCMDRUNX64=
Set VCENVCMDRunIA64=
@for /f "usebackq tokens=3,*" %%f in (`reg query "HKLM\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v6.1" /v InstallationFolder`) do set MSSDK=%%f %%g
@if "%1" == "RELEASE" (
	Set VCENVCMD=!MSSDK!Bin\SetEnv.Cmd
	Set VCENVCMDRUNX64="!VCENVCMD!" /2003 /x64 /Release
	@call !VCENVCMDRUNX64!
	@nmake /f MakefileX64 clean_release
	@nmake /f MakefileX64 release

	REM Set VCENVCMDRUNIA64="!VCENVCMD!" /2003 /ia64 /Release
	REM @call !VCENVCMDRUNIA64!
	REM REM @call "%MSSDK%Bin\SetEnv.Cmd /ia64 /Release
	REM @nmake /f MakefileIA64 clean_release
	REM @nmake /f MakefileIA64 release

) 

@if "%1" == "DEBUG" (
	Set VCENVCMD=!MSSDK!Bin\SetEnv.Cmd
	Set VCENVCMDRUNX64="!VCENVCMD!" /2003 /x64 /Debug
	@call !VCENVCMDRUNX64!
	@nmake /f MakefileX64 clean_debug
	@nmake /f MakefileX64 Debug
	
	REM Set VCENVCMDRUNIA64="!VCENVCMD!" /2003 /ia64 /Debug
	REM @call !VCENVCMDRUNIA64!
	REM @nmake /f MakefileIA64 clean_debug
	REM @nmake /f MakefileIA64 Debug
)
@if "%1" == "CLEAN" (
	
	@nmake /f MakefileX64 clean_release	
	@nmake /f MakefileX64 clean_debug
	@nmake /f MakefileIA64 clean_release	
	@nmake /f MakefileIA64 clean_debug
	

)

@if "%1" == "CLEAN_DEBUG" (
	
	@nmake /f MakefileX64 clean_debug
	@nmake /f MakefileIA64 clean_debug

)
@if "%1" == "CLEAN_RELEASE" (
	
	@nmake /f MakefileX64 clean_release
	@nmake /f MakefileIA64 clean_release
	

)
ENDLOCAL