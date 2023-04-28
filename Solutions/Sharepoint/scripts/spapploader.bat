@echo off 

ECHO.

SET "errorMsg="
SET outputFilePath=OUTPUTFILE
SET keyPath="HKLM\Software\SV Systems\VxAgent"
REG QUERY "HKLM\SOFTWARE\Wow6432Node" > NUL 2>&1
if %errorlevel% EQU 0 (
SET keyPath="HKLM\Software\Wow6432Node\SV Systems\VxAgent"
)
FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY %keyPath% /v InstallDirectory') DO SET installpath=%%B
REG QUERY %keyPath% /v InstallDirectory > NUL 2>&1
if %errorlevel% NEQ 0 (
SET errorMsg=InMage Scout Agent is not installed in this computer. Please Install InMage Scout Agent to perform SharePoint discovery.
GOTO ERROR_EXIT
)
SET outputFilePath="%installpath%\spapploader_output.txt"
SET executablePath="%installpath%"
SET keyPath="HKLM\SOFTWARE\Microsoft\NET Framework Setup\NDP"
reg query %keyPath% /se # > %outputFilePath% 2>&1

For /F "usebackq tokens=1 delims=#" %%I IN (`type %outputFilePath%`) DO (	
	REM ECHO "%%I"
        For /F "tokens=1,6 delims=\" %%A IN ("%%I") DO (
		REM ECHO "%%B"
		if "%%B"=="v3.0" GOTO RUN_SHAREPOINT_DISCOVERY
		if "%%B"=="v3.5" GOTO RUN_SHAREPOINT_DISCOVERY
		if "%%B"=="v4" (
			SET "tempKeyPath=%keyPath%\v4\Full"
			reg query "%tempKeyPath%" > NUL 2>&1			
			if %errorlevel% equ 0 GOTO RUN_SHAREPOINT_DISCOVERY
		)
	)        
)

SET errorMsg=The required version of .NET framework is not installed in this computer. Please install .NET framework 3.0 or higher version to perform SharePoint discovery.
GOTO ERROR_EXIT

:RUN_SHAREPOINT_DISCOVERY
ECHO Discovering..
%executablePath%\spapp.exe --discover
IF %errorlevel% NEQ 0 (
SET errorMsg=Exiting abnormally...
GOTO ERROR_EXIT;
)

:SUCCESS_EXIT
if %outputFilePath% NEQ "OUTPUTFILE" (
	if EXIST %outputFilePath% (
		del %outputFilePath% /F
	)
)
EXIT 0

:ERROR_EXIT
if %outputFilePath% NEQ "OUTPUTFILE" (
	if EXIST %outputFilePath% (
		del %outputFilePath% /F
	)
)
ECHO [ERROR]: %errorMsg%
EXIT 1