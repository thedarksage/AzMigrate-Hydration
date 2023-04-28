@echo off
set Configuration=%1
set ESRPExePath=%2
set CurrentDirectoryPath=%3
set "InputFile=%CurrentDirectoryPath%\CASignInput.json"
set "OutputFile=%CurrentDirectoryPath%\CASignOutput.json"
set "OutputVerboseLog=%CurrentDirectoryPath%\CASignOutputVerbose.Log"

if %Configuration% == Release (
goto :RunSigning
) else (
goto :SkipSigning
)

:RunSigning
del /F %InputFile% %OutputFile% %OutputVerboseLog%
robocopy %CurrentDirectoryPath%\ESRPConfigTemplateFiles %CurrentDirectoryPath%\ CASignInput.json

setlocal enableextensions disabledelayedexpansion
set "sppsearch=SOURCE_PATH_PLACHOLDER"
set "sppreplace=%CurrentDirectoryPath%\obj\x86\%Configuration%"
set sppreplace=%sppreplace:\=\\%
set "dppsearch=DESTINATION_PATH_PLACHOLDER"
set "dppreplace=%CurrentDirectoryPath%\obj\x86\%Configuration%"
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
echo ProcessServerCustomAction.dll signing is skipped.

:End