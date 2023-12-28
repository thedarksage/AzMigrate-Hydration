@echo OFF

set SrcFolder=%1
set BuildFolder=%2

if [%SrcFolder%] == []  ( 
	echo Source Folder is missing.
	echo Usage: %0 SourceFolder BuildFolder
	goto:eof
)

if [%BuildFolder%] == []  (
	echo Build Location is missing.
	echo Usage: %0 SourceFolder BuildFolder
	goto:eof
)

if not exist %SrcFolder% (
     echo Source Folder %SrcFolder% doesn't exist
     goto:eof
)

if not exist %BuildFolder% (
     echo Build Location %BuildFolder% doesn't exist
     goto:eof
)

call "%VS110COMNTOOLS%\..\..\VC\bin\vcvars32.bat"
pushd %1
if exist "%2\VistaRelease" rmdir /q /s "%2\VistaRelease"
mkdir "%2\VistaRelease\x86"
mkdir "%2\VistaRelease\x64"
msbuild WDK8-DiskFlt.vcxproj /p:configuration="Vista Release" /p:platform=x64 /t:Clean > %2\VistaRelease\build.log 2>&1
msbuild WDK8-DiskFlt.vcxproj /p:configuration="Vista Release" /p:platform=win32 /t:Clean >> %2\VistaRelease\build.log 2>&1
msbuild WDK8-DiskFlt.vcxproj /p:configuration="Vista Release" /p:platform=x64  >> %2\VistaRelease\build.log 2>&1
msbuild WDK8-DiskFlt.vcxproj /p:configuration="Vista Release" /p:platform=win32  >> %2\VistaRelease\build.log 2>&1
copy /Y "VistaRelease\*" "%2\VistaRelease\x86\."
copy /Y "x64\VistaRelease\*" "%2\VistaRelease\x64\*"
popd

