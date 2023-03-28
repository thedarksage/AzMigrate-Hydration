@echo off
set Configuration=%1
set ESRPExePath=%2
set CurrentDirectoryPath=%3
set "InputFile=%CurrentDirectoryPath%\CAWSignInput.json"
set "OutputFile=%CurrentDirectoryPath%\CAWSignOutput.json"
set "OutputVerboseLog=%CurrentDirectoryPath%\CAWSignOutputVerbose.Log"

if %Configuration% == Release (
goto :RunSigning
) else (
goto :SkipSigning
)

:RunSigning
del /F %InputFile% %OutputFile% %OutputVerboseLog%
robocopy %CurrentDirectoryPath%\ESRPConfigTemplateFiles %CurrentDirectoryPath%\ CAWSignInput.json

setlocal enableextensions disabledelayedexpansion
set "sppsearch=SOURCE_PATH_PLACHOLDER"
set "sppreplace=%CurrentDirectoryPath%\bin\%Configuration%"
set sppreplace=%sppreplace:\=\\%
set "dppsearch=DESTINATION_PATH_PLACHOLDER"
set "dppreplace=%CurrentDirectoryPath%\bin\%Configuration%"
set dppreplace=%dppreplace:\=\\%

for /f "delims=" %%i in ('type "%InputFile%" ^& break ^> "%InputFile%" ') do (
    set "line=%%i"
    setlocal enabledelayedexpansion
    set "line=!line:%sppsearch%=%sppreplace%!"
	set "line=!line:%dppsearch%=%dppreplace%!"
    >>"%InputFile%" echo(!line!
    endlocal
)

call %ESRPExePath% sign -a C:\esrpclientconfig\Auth.json -p C:\esrpclientconfig\Policy.json -i %InputFile% -o %OutputFile% -l verbose -f %OutputVerboseLog%

goto :End

:SkipSigning
echo UnifiedAgentCustomActions.CA.dll signing is skipped.

:End